#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  adapter/hkex/hkexadapter.h  –  HKEX OMD-D → normalised mde:: events
//
//  Header-only CRTP adapter, templated on its Handler.  No virtual dispatch.
//  Order-book events are also replayed into a per-instrument fastorderbook.
// ─────────────────────────────────────────────────────────────────────────────
#include "base.h"
#include "orderbook.h"
#include "omddmessages.h"
#include "hkexmarketdata.h"
#include "NanoLog.hpp"
#include "config.h"

#include <unordered_map>
#include <string>
#include <vector>
#include <cstring>

namespace mde {

template<MarketDataHandler Handler>
class HkexAdapter : public AdapterBase<HkexAdapter<Handler>, Handler> {
    using Base = AdapterBase<HkexAdapter<Handler>, Handler>;

public:
    explicit HkexAdapter(Handler& handler) : Base(handler) {}

    Exchange    exchangeID() const { return Exchange::HKEX; }
    const char* name()       const { return "HKEX-OMD-D"; }

    // Look up an instrument by orderbookID in any registry that provides
    // getByInstrumentKey().  The adapter owns the orderbookID → symbol mapping,
    // so this is the correct place for the lookup rather than the registry.
    template<class Registry>
    const InstrumentDef* lookup(uint32_t orderbookID, const Registry& reg) const {
        return reg.getByInstrumentKey(makeKey(orderbookID));
    }

    // ── CRTP entry points ──────────────────────────────────────────────────
    bool processPacketImpl(const uint8_t* data, uint16_t len) {
        if (len < sizeof(PktHeader)) return false;
        const auto* pkt = reinterpret_cast<const PktHeader*>(data);
        if (pkt->pktSize > len) return false;

        if (pkt->msgCount == 0) {                       // heartbeat
            ControlEvent ev;
            ev.exchange   = Exchange::HKEX;
            ev.kind       = ControlKind::HEARTBEAT;
            ev.seqNum     = pkt->seqNum;
            ev.receivedAt = nsNow();
            this->emitControl(ev);
            return true;
        }
        if (pkt->compressionMode != 0) {
            LOG_WARN << "[HKEX] compressed packet – decompression not implemented";
            return false;
        }

        const uint8_t* cursor    = data + sizeof(PktHeader);
        const uint8_t* packetEnd = data + pkt->pktSize;
        uint8_t        remaining = pkt->msgCount;
        while (remaining > 0 && cursor + sizeof(MsgHeader) <= packetEnd) {
            const auto* mh = reinterpret_cast<const MsgHeader*>(cursor);
            if (mh->msgSize < sizeof(MsgHeader) || cursor + mh->msgSize > packetEnd) break;
            dispatch(mh->msgType, cursor, mh->msgSize);
            cursor += mh->msgSize;
            --remaining;
        }
        return true;
    }

    void resetImpl() {
        commodities_.clear();
        classes_.clear();
        series_.clear();
        symbolToOBID_.clear();
        comboLegs_.clear();
        books_.clear();
    }

private:
    // ── Helpers ─────────────────────────────────────────────────────────────
    static std::string fixedStr(const char* src, size_t n) {
        return std::string(src, strnlen(src, n));
    }
    static uint64_t classKey(uint8_t country, uint8_t market,
                             uint8_t instrGroup, uint16_t commodityCode) {
        return (uint64_t(country) << 40) | (uint64_t(market) << 32)
             | (uint64_t(instrGroup) << 16) | uint64_t(commodityCode);
    }
    static InstrumentKind mapProduct(uint8_t fp, uint8_t putOrCall) {
        switch (fp) {
        case 3:  return InstrumentKind::FUTURE;
        case 1:  return (putOrCall == 1) ? InstrumentKind::OPTION_CALL
                                         : InstrumentKind::OPTION_PUT;
        case 11: return InstrumentKind::STRATEGY;
        default: return InstrumentKind::UNKNOWN;
        }
    }
    static SessionState mapHkexState(uint16_t stateLevel, uint16_t state) {
        if (stateLevel == 99) return SessionState::END_OF_DAY;
        switch (state) {
        case 0:  return SessionState::UNKNOWN;
        case 2:  return SessionState::ORDER_ACCEPTANCE;
        case 3:  return SessionState::AUCTION_OPEN;
        case 5:  return SessionState::TRADING;
        case 6:  return SessionState::AUCTION_CLOSE;
        case 7:  return SessionState::END_OF_SESSION;
        case 8:  return SessionState::TRADING_HALT;
        case 9:  return SessionState::SUSPENDED;
        case 28: return SessionState::TRADING_HALT;
        default: return SessionState::UNKNOWN;
        }
    }

    InstrumentKey makeKey(uint32_t orderbookID) const {
        InstrumentKey k;
        k.exchange = Exchange::HKEX;
        k.nativeID = orderbookID;
        auto it = series_.find(orderbookID);
        if (it != series_.end()) k.symbol = it->second.symbol;
        return k;
    }
    Price makePrice(int32_t raw, uint16_t decimals) const {
        static constexpr int32_t NULL_INT32 = static_cast<int32_t>(0x80000000);
        if (raw == NULL_INT32) return Price::null();
        Price p;
        p.raw      = raw;
        p.decimals = static_cast<uint8_t>(decimals);
        return p;
    }

    // ── Wire-level dispatch ──────────────────────────────────────────────────
    void dispatch(uint16_t type, const uint8_t* body, uint16_t len) {
        switch (static_cast<MsgType>(type)) {
        case MSG_SEQUENCE_RESET: {
            const auto* m = reinterpret_cast<const SequenceReset*>(body);
            resetImpl();
            ControlEvent ev;
            ev.exchange   = Exchange::HKEX;
            ev.kind       = ControlKind::SEQUENCE_RESET;
            ev.seqNum     = m->newSeqNo;
            ev.receivedAt = nsNow();
            this->emitControl(ev);
            break;
        }
        case MSG_DR_SIGNAL: {
            const auto* m = reinterpret_cast<const DRSignal*>(body);
            ControlEvent ev;
            ev.exchange   = Exchange::HKEX;
            ev.kind       = (m->drStatus == 1) ? ControlKind::DR_IN_PROGRESS
                                               : ControlKind::DR_COMPLETED;
            ev.receivedAt = nsNow();
            this->emitControl(ev);
            break;
        }
        case MSG_REFRESH_COMPLETE: break;

        case MSG_COMMODITY_DEF:   onCommodityDef  (body, len); break;
        case MSG_CLASS_DEF:       onClassDef      (body, len); break;
        case MSG_SERIES_DEF_BASE: onSeriesDefBase (body, len); break;
        case MSG_SERIES_DEF_EXT:  onSeriesDefExt  (body, len); break;
        case MSG_COMBINATION_DEF: onCombinationDef(body, len); break;

        case MSG_MARKET_STATUS:   onMarketStatus  (body, len); break;
        case MSG_SERIES_STATUS:   onSeriesStatus  (body, len); break;

        case MSG_ADD_ORDER:       onAddOrder      (body, len); break;
        case MSG_MODIFY_ORDER:    onModifyOrder   (body, len); break;
        case MSG_DELETE_ORDER:    onDeleteOrder   (body, len); break;
        case MSG_ORDERBOOK_CLEAR: onOrderbookClear(body, len); break;

        case MSG_TRADE:           onTrade         (body, len); break;
        case MSG_TRADE_AMENDMENT: onTradeAmendment(body, len); break;
        default: break;
        }
    }

    // ── Reference data handlers ──────────────────────────────────────────────
    void onCommodityDef(const uint8_t* b, uint16_t l) {
        if (l < sizeof(CommodityDef)) return;
        const auto& m = *reinterpret_cast<const CommodityDef*>(b);
        auto& c = commodities_[m.commodityCode];
        c.code                     = m.commodityCode;
        c.name                     = fixedStr(m.commodityName, sizeof(m.commodityName));
        c.currency                 = fixedStr(m.baseCurrency,  sizeof(m.baseCurrency));
        c.isinCode                 = fixedStr(m.isinCode,      sizeof(m.isinCode));
        c.underlyingCode           = fixedStr(m.underlyingCode,sizeof(m.underlyingCode));
        c.decimalInUnderlyingPrice = m.decimalInUnderlyingPrice;
    }

    void onClassDef(const uint8_t* b, uint16_t l) {
        if (l < sizeof(ClassDef)) return;
        const auto& m = *reinterpret_cast<const ClassDef*>(b);
        uint64_t k = classKey(m.country, m.market, m.instrumentGroup, m.commodityCode);
        auto& c = classes_[k];
        c.country             = m.country;
        c.market              = m.market;
        c.instrGroup          = m.instrumentGroup;
        c.modifier            = m.modifier;
        c.commodityCode       = m.commodityCode;
        c.decimalInPremium    = m.decimalInPremium;
        c.decimalInStrikePrice= m.decimalInStrikePrice;
        c.tickStepSize        = m.tickStepSize;
        c.instrumentClassID   = fixedStr(m.instrumentClassID,   sizeof(m.instrumentClassID));
        c.instrumentClassName = fixedStr(m.instrumentClassName, sizeof(m.instrumentClassName));
        c.currency            = fixedStr(m.baseCurrency,        sizeof(m.baseCurrency));
        c.tradable            = (m.tradable == 1);
    }

    void onSeriesDefBase(const uint8_t* b, uint16_t l) {
        if (l < sizeof(SeriesDefBase)) return;
        const auto& m = *reinterpret_cast<const SeriesDefBase*>(b);
        auto& s = series_[m.orderbookID];
        s.orderbookID      = m.orderbookID;
        s.symbol           = fixedStr(m.symbol, sizeof(m.symbol));
        s.financialProduct = m.financialProduct;
        s.priceDecimals    = m.numberOfDecimalsPrice;
        s.numberOfLegs     = m.numberOfLegs;
        s.strikePrice      = m.strikePrice;
        s.expirationDateStr= fixedStr(m.expirationDate, sizeof(m.expirationDate));
        s.putOrCall        = m.putOrCall;
        s.has303           = true;
        symbolToOBID_[s.symbol] = m.orderbookID;
        tryEmitInstrument(m.orderbookID);
    }

    void onSeriesDefExt(const uint8_t* b, uint16_t l) {
        if (l < sizeof(SeriesDefExt)) return;
        const auto& m = *reinterpret_cast<const SeriesDefExt*>(b);
        uint32_t obid = m.orderBookID;
        if (obid == 0) {
            std::string sym = fixedStr(m.symbol, sizeof(m.symbol));
            auto it = symbolToOBID_.find(sym);
            if (it != symbolToOBID_.end()) obid = it->second;
            else return;
        }
        auto& s = series_[obid];
        s.orderbookID = obid;
        if (s.symbol.empty()) s.symbol = fixedStr(m.symbol, sizeof(m.symbol));
        s.commodityCode        = m.commodityCode;
        s.market               = m.market;
        s.instrumentGroup      = m.instrumentGroup;
        s.country              = m.country;
        s.modifier             = m.modifier;
        s.expirationDatePacked = m.expirationDate;
        s.contractSize         = m.contractSize;
        s.seriesStatus         = m.seriesStatus;
        s.effectiveTomorrow    = (m.effectiveTomorrow == 1);
        s.has304               = true;
        symbolToOBID_[s.symbol] = obid;
        tryEmitInstrument(obid);
    }

    void onCombinationDef(const uint8_t* b, uint16_t l) {
        if (l < sizeof(CombinationDef)) return;
        const auto& m = *reinterpret_cast<const CombinationDef*>(b);
        HkexComboLeg leg;
        leg.legOrderbookID = m.legOrderbookID;
        leg.legSide        = m.legSide[0];
        leg.ratio          = m.legRatio;
        comboLegs_[m.comboOrderbookID].push_back(leg);

        if (series_.find(m.comboOrderbookID) == series_.end()) return;
        StrategyDef sd;
        sd.key        = makeKey(m.comboOrderbookID);
        sd.receivedAt = nsNow();
        for (auto& rawLeg : comboLegs_[m.comboOrderbookID]) {
            StrategyLeg leg2;
            leg2.instrument = makeKey(rawLeg.legOrderbookID);
            leg2.legSide    = (rawLeg.legSide == 'B') ? StrategyLegSide::AS_DEFINED
                                                      : StrategyLegSide::OPPOSITE;
            leg2.ratio      = rawLeg.ratio;
            sd.legs.push_back(leg2);
        }
        this->emitStrategy(sd);
    }

    void tryEmitInstrument(uint32_t orderbookID) {
        auto it = series_.find(orderbookID);
        if (it == series_.end()) return;
        const auto& s = it->second;
        if (!s.has303 && !s.has304) return;
        this->emitInstrument(buildInstrumentDef(s));
    }

    InstrumentDef buildInstrumentDef(const HkexSeriesCache& s) const {
        InstrumentDef d;
        d.key.exchange      = Exchange::HKEX;
        d.key.nativeID      = s.orderbookID;
        d.key.symbol        = s.symbol;
        d.kind              = mapProduct(s.financialProduct, s.putOrCall);
        d.expirationDate    = s.expirationDateStr;
        d.priceDecimals     = static_cast<uint8_t>(s.priceDecimals);
        d.isTradable        = (s.seriesStatus != 2 && s.seriesStatus != 5);
        d.isSuspended       = (s.seriesStatus == 2);
        d.effectiveTomorrow = s.effectiveTomorrow;
        d.contractSize      = s.contractSize;
        d.numericCode       = static_cast<uint32_t>(s.commodityCode);
        d.receivedAt        = nsNow();

        uint16_t strikeDec = 0;
        uint64_t ck = classKey(s.country, s.market, s.instrumentGroup, s.commodityCode);
        auto cit = classes_.find(ck);
        if (cit != classes_.end()) {
            strikeDec  = cit->second.decimalInStrikePrice;
            d.currency = cit->second.currency;
            d.name     = cit->second.instrumentClassName;
            d.tickSize = makePrice(cit->second.tickStepSize, cit->second.decimalInPremium);
        }
        d.strikePrice = makePrice(s.strikePrice, strikeDec);

        auto cmit = commodities_.find(s.commodityCode);
        if (cmit != commodities_.end()) {
            if (d.currency.empty()) d.currency = cmit->second.currency;
            if (d.name.empty())     d.name     = cmit->second.name;
            d.underlyingCode = cmit->second.underlyingCode;
            d.isinCode       = cmit->second.isinCode;
        }
        return d;
    }

    // ── Status handlers ──────────────────────────────────────────────────────
    void onMarketStatus(const uint8_t* b, uint16_t l) {
        if (l < sizeof(MarketStatus)) return;
        const auto& m = *reinterpret_cast<const MarketStatus*>(b);
        MarketStateEvent ev;
        ev.exchange    = Exchange::HKEX;
        ev.state       = mapHkexState(m.stateLevel, m.state);
        ev.stateDetail = std::to_string(m.stateLevel) + "/" + std::to_string(m.state);
        ev.priority    = m.priority;
        ev.isEndOfDay  = (m.stateLevel == 99);
        ev.receivedAt  = nsNow();
        if (m.stateLevel == 4 && m.orderbookID != 0)
            ev.instrument = makeKey(m.orderbookID);
        this->emitMarketState(ev);
    }

    void onSeriesStatus(const uint8_t* b, uint16_t l) {
        if (l < sizeof(SeriesStatus)) return;
        const auto& m = *reinterpret_cast<const SeriesStatus*>(b);
        auto it = series_.find(m.orderbookID);
        if (it != series_.end()) it->second.seriesStatus = m.seriesStatus;
        MarketStateEvent ev;
        ev.exchange   = Exchange::HKEX;
        ev.instrument = makeKey(m.orderbookID);
        ev.state      = (m.suspensionIndicator == 1 || m.seriesStatus == 2)
                            ? SessionState::SUSPENDED : SessionState::TRADING;
        ev.stateDetail= "SeriesStatus/" + std::to_string(m.seriesStatus);
        ev.receivedAt = nsNow();
        this->emitMarketState(ev);
    }

    // ── Order book handlers ──────────────────────────────────────────────────
    void onAddOrder(const uint8_t* b, uint16_t l) {
        if (l < sizeof(AddOrder)) return;
        const auto& m = *reinterpret_cast<const AddOrder*>(b);
        uint16_t dec = 0;
        auto it = series_.find(m.orderbookID);
        if (it != series_.end()) dec = it->second.priceDecimals;
        OrderEvent ev;
        ev.exchange      = Exchange::HKEX;
        ev.instrument    = makeKey(m.orderbookID);
        ev.kind          = OrderEventKind::ADD;
        ev.orderID       = m.orderID;
        ev.side          = (m.side == 0) ? Side::BID : Side::ASK;
        ev.price         = makePrice(m.price, dec);
        ev.quantity      = m.quantity;
        ev.bookPosition  = m.orderBookPosition;
        ev.priorityChange= true;
        ev.receivedAt    = nsNow();
        this->emitOrderEvent(ev);
        books_.apply(ev);
    }

    void onModifyOrder(const uint8_t* b, uint16_t l) {
        if (l < sizeof(ModifyOrder)) return;
        const auto& m = *reinterpret_cast<const ModifyOrder*>(b);
        uint16_t dec = 0;
        auto it = series_.find(m.orderbookID);
        if (it != series_.end()) dec = it->second.priceDecimals;
        OrderEvent ev;
        ev.exchange      = Exchange::HKEX;
        ev.instrument    = makeKey(m.orderbookID);
        ev.kind          = OrderEventKind::MODIFY;
        ev.orderID       = m.orderID;
        ev.side          = (m.side == 0) ? Side::BID : Side::ASK;
        ev.price         = makePrice(m.price, dec);
        ev.quantity      = m.quantity;
        ev.bookPosition  = m.orderBookPosition;
        ev.priorityChange= true;
        ev.receivedAt    = nsNow();
        this->emitOrderEvent(ev);
        books_.apply(ev);
    }

    void onDeleteOrder(const uint8_t* b, uint16_t l) {
        if (l < sizeof(DeleteOrder)) return;
        const auto& m = *reinterpret_cast<const DeleteOrder*>(b);
        OrderEvent ev;
        ev.exchange   = Exchange::HKEX;
        ev.instrument = makeKey(m.orderbookID);
        ev.kind       = OrderEventKind::DELETE;
        ev.orderID    = m.orderID;
        ev.side       = (m.side == 0) ? Side::BID : Side::ASK;
        ev.quantity   = 0;
        ev.receivedAt = nsNow();
        this->emitOrderEvent(ev);
        books_.apply(ev);
    }

    void onOrderbookClear(const uint8_t* b, uint16_t l) {
        if (l < sizeof(OrderbookClear)) return;
        const auto& m = *reinterpret_cast<const OrderbookClear*>(b);
        OrderEvent ev;
        ev.exchange   = Exchange::HKEX;
        ev.instrument = makeKey(m.orderbookID);
        ev.kind       = OrderEventKind::CLEAR;
        ev.receivedAt = nsNow();
        this->emitOrderEvent(ev);
        books_.apply(ev);
    }

    // ── Trade handlers ───────────────────────────────────────────────────────
    void onTrade(const uint8_t* b, uint16_t l) {
        if (l < sizeof(Trade)) return;
        const auto& m = *reinterpret_cast<const Trade*>(b);
        uint16_t dec = 0;
        auto it = series_.find(m.orderbookID);
        if (it != series_.end()) dec = it->second.priceDecimals;
        TradeEvent ev;
        ev.exchange     = Exchange::HKEX;
        ev.instrument   = makeKey(m.orderbookID);
        ev.kind         = (m.dealType & 0x02) ? TradeKind::AUCTION : TradeKind::NORMAL;
        ev.tradeID      = m.tradeID;
        ev.orderID      = m.orderID;
        ev.price        = makePrice(m.price, dec);
        ev.quantity     = m.quantity;
        ev.isPrintable  = (m.dealType & 0x01) != 0;
        ev.aggressor    = (m.side == 2) ? Side::BID : Side::ASK;
        ev.receivedAt   = nsNow();
        ev.exchangeTime = static_cast<NsTimestamp>(m.tradeTime);
        this->emitTrade(ev);
    }

    void onTradeAmendment(const uint8_t* b, uint16_t l) {
        if (l < sizeof(TradeAmendment)) return;
        const auto& m = *reinterpret_cast<const TradeAmendment*>(b);
        TradeEvent ev;
        ev.exchange    = Exchange::HKEX;
        ev.kind        = TradeKind::TRADE_AMENDMENT;
        ev.tradeID     = m.tradeID;
        ev.price       = makePrice(m.price, 0);
        ev.quantity    = m.quantity;
        ev.amendState  = m.tradeState;
        ev.receivedAt  = nsNow();
        ev.exchangeTime= static_cast<NsTimestamp>(m.tradeTime);
        this->emitTrade(ev);
    }

    // ── Internal caches ─────────────────────────────────────────────────────
    std::unordered_map<uint16_t, HkexCommodityCache>       commodities_;
    std::unordered_map<uint64_t, HkexClassCache>           classes_;
    std::unordered_map<uint32_t, HkexSeriesCache>          series_;
    std::unordered_map<std::string, uint32_t>              symbolToOBID_;
    std::unordered_map<uint32_t, std::vector<HkexComboLeg>> comboLegs_;
    OrderBookMap<>                                         books_;
};

// ── AdapterFor trait specialisation ──────────────────────────────────────────
template<class Handler>
struct AdapterFor<Exchange::HKEX, Handler> { using type = HkexAdapter<Handler>; };

} // namespace mde
