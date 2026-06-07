#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  adapter/tse/tseadapter.h  –  TSE Arrowhead FLEX MBO → normalised mde:: events
//
//  Header-only CRTP adapter, templated on its Handler.  No virtual dispatch.
//  Order-book events are also replayed into a per-instrument fastorderbook.
//
//  Supported tags: T O K A E C D R L (see tse_market_data.h for framing).
// ─────────────────────────────────────────────────────────────────────────────
#include "base.h"
#include "orderbook.h"
#include "tsemarketdata.h"
#include "NanoLog.hpp"

#include <unordered_map>
#include <string>
#include <cstring>

namespace mde {

template<MarketDataHandler Handler>
class TseAdapter : public AdapterBase<TseAdapter<Handler>, Handler> {
    using Base = AdapterBase<TseAdapter<Handler>, Handler>;

public:
    explicit TseAdapter(Handler& handler) : Base(handler) {}

    Exchange    exchangeID() const { return Exchange::TSE; }
    const char* name()       const { return "TSE-Arrowhead-MBO"; }

    bool processPacketImpl(const uint8_t* data, uint16_t len) {
        if (len < sizeof(TsePktHeader)) return false;

        TsePktHeader hdr;
        hdr.multicastGroupNum = data[0];
        hdr.numReboots        = data[1];
        hdr.seqNumber         = be32(data + 2);
        memcpy(hdr.issueCode, data + 6, 12);
        hdr.updateNumber      = be32(data + 18);
        hdr.packetNumber      = data[22];
        hdr.totalPackets      = data[23];
        hdr.utilityFlag       = data[24];
        hdr.messageCount      = data[25];

        // Multicast routing maintenance message: 1-byte payload, no tags.
        if (hdr.messageCount == 0 && len == sizeof(TsePktHeader) + 1) return true;

        const uint8_t* cursor    = data + sizeof(TsePktHeader);
        const uint8_t* packetEnd = data + len;
        uint8_t        remaining = hdr.messageCount;
        while (remaining > 0 && cursor < packetEnd) {
            uint8_t tagLen = *cursor++;
            if (cursor + tagLen > packetEnd) break;
            if (tagLen == 0) { --remaining; continue; }
            switch (static_cast<char>(cursor[0])) {
            case 'T': onTagT(hdr, cursor, tagLen); break;
            case 'O': onTagO(hdr, cursor, tagLen); break;
            case 'K': onTagK(hdr, cursor, tagLen); break;
            case 'A': onTagA(hdr, cursor, tagLen); break;
            case 'E': onTagE(hdr, cursor, tagLen); break;
            case 'C': onTagC(hdr, cursor, tagLen); break;
            case 'D': onTagD(hdr, cursor, tagLen); break;
            case 'R': onTagR(hdr, cursor, tagLen); break;
            case 'L': onTagL(hdr, cursor, tagLen); break;
            default: break; // II, BP, MG – not decoded here
            }
            cursor += tagLen;
            --remaining;
        }
        return true;
    }

    void resetImpl() {
        issues_.clear();
        lastMatch_.clear();
        currentTimeSec_ = 0;
        books_.clear();
    }

private:
    // ── Big-endian byte readers ──────────────────────────────────────────────
    static uint32_t be32(const uint8_t* p) {
        return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16)
             | (uint32_t(p[2]) <<  8) |  uint32_t(p[3]);
    }
    static uint64_t be64(const uint8_t* p) {
        return (uint64_t(p[0]) << 56) | (uint64_t(p[1]) << 48)
             | (uint64_t(p[2]) << 40) | (uint64_t(p[3]) << 32)
             | (uint64_t(p[4]) << 24) | (uint64_t(p[5]) << 16)
             | (uint64_t(p[6]) <<  8) |  uint64_t(p[7]);
    }
    static uint64_t be48(const uint8_t* p) {
        return (uint64_t(p[0]) << 40) | (uint64_t(p[1]) << 32)
             | (uint64_t(p[2]) << 24) | (uint64_t(p[3]) << 16)
             | (uint64_t(p[4]) <<  8) |  uint64_t(p[5]);
    }

    // TSE Bn(Price): uint64, last 4 digits are implicit decimal.
    Price tsePrice(uint64_t raw) const {
        if (raw == 0xFFFFFFFFFFFFFFFFULL) return Price::market();
        if (raw == 0)                     return Price::null();
        Price p;
        p.raw      = static_cast<int64_t>(raw);
        p.decimals = 4;
        return p;
    }

    InstrumentKey makeKey(const std::string& issueCode) const {
        InstrumentKey k;
        k.exchange = Exchange::TSE;
        k.symbol   = issueCode;
        try { k.nativeID = static_cast<uint32_t>(std::stoul(issueCode)); }
        catch (...) { k.nativeID = 0; }
        return k;
    }

    SessionState mapMarketStatus(uint8_t ms, const char* sf) const {
        if (sf[0] == 'A' && sf[1] == '0') return SessionState::TRADING_HALT;
        if (sf[0] == 'A' && sf[1] == '1') return SessionState::TRADING;
        if (sf[0] == 'B' && sf[1] == '0') return SessionState::AUCTION_OPEN;
        if (sf[0] == 'C' && sf[1] == '0') return SessionState::SUSPENDED;
        if (sf[0] == 'C' && sf[1] == '1') return SessionState::TRADING;
        if (sf[0] == 'D' && sf[1] == '0') return SessionState::TRADING_HALT;
        switch (ms) {
        case 11: case 21: return SessionState::ORDER_ACCEPTANCE;
        case 12: case 22: return SessionState::TRADING;
        case 23:          return SessionState::AUCTION_CLOSE;
        case 19:          return SessionState::END_OF_SESSION;
        case 29:          return SessionState::END_OF_DAY;
        default:          return SessionState::UNKNOWN;
        }
    }

    TseIssueCache& getOrCreateIssue(const std::string& issueCode) {
        auto it = issues_.find(issueCode);
        if (it != issues_.end()) return it->second;
        TseIssueCache c;
        c.issueCode     = issueCode;
        c.priceDecimals = 4;
        InstrumentDef d;
        d.key           = makeKey(issueCode);
        d.kind          = InstrumentKind::EQUITY;  // default; II tag refines this
        d.currency      = "JPY";
        d.priceDecimals = 4;
        d.tseIssueCode  = d.key.nativeID;
        d.receivedAt    = nsNow();
        this->emitInstrument(d);
        return issues_.emplace(issueCode, c).first->second;
    }

    static NsTimestamp exchTime(uint32_t timeSec, const uint8_t* tag) {
        return int64_t(timeSec) * 1'000'000'000LL + int64_t(be32(tag + 1)) * 1'000LL;
    }

    // ── Tag handlers ─────────────────────────────────────────────────────────
    void onTagT(const TsePktHeader&, const uint8_t* tag, uint8_t l) {
        if (l < 5) return;
        currentTimeSec_ = be32(tag + 1);
    }

    void onTagO(const TsePktHeader& hdr, const uint8_t* tag, uint8_t l) {
        if (l < 18) return;
        std::string issueCode(hdr.issueCode, strnlen(hdr.issueCode, 12));
        getOrCreateIssue(issueCode);
        uint8_t     ms = tag[5];
        const char* sf = reinterpret_cast<const char*>(tag + 6);
        MarketStateEvent ev;
        ev.exchange     = Exchange::TSE;
        ev.instrument   = makeKey(issueCode);
        ev.state        = mapMarketStatus(ms, sf);
        ev.stateDetail  = "O:" + std::to_string(ms) + "/" + std::string(sf, 2);
        ev.isEndOfDay   = (ms == 29);
        ev.exchangeTime = exchTime(currentTimeSec_, tag);
        ev.receivedAt   = nsNow();
        this->emitMarketState(ev);
    }

    void onTagK(const TsePktHeader& hdr, const uint8_t* tag, uint8_t l) {
        if (l < 46) return;
        std::string issueCode(hdr.issueCode, strnlen(hdr.issueCode, 12));
        getOrCreateIssue(issueCode);
        uint32_t matchID = be32(tag + 26);
        uint64_t lp      = be64(tag + 18);
        char     tside   = static_cast<char>(tag[5]);
        lastMatch_[issueCode] = {matchID, lp};
        TradeEvent ev;
        ev.exchange     = Exchange::TSE;
        ev.instrument   = makeKey(issueCode);
        ev.kind         = (tside == ' ') ? TradeKind::AUCTION : TradeKind::NORMAL;
        ev.tradeID      = matchID;
        ev.price        = tsePrice(lp);
        ev.quantity     = be48(tag + 6);
        ev.aggressor    = (tside == 'B') ? Side::BID : Side::ASK;
        ev.bestBid      = tsePrice(be64(tag + 38));
        ev.bestAsk      = tsePrice(be64(tag + 30));
        ev.isPrintable  = true;
        ev.exchangeTime = exchTime(currentTimeSec_, tag);
        ev.receivedAt   = nsNow();
        this->emitTrade(ev);
    }

    void onTagA(const TsePktHeader& hdr, const uint8_t* tag, uint8_t l) {
        if (l < 26) return;
        std::string issueCode(hdr.issueCode, strnlen(hdr.issueCode, 12));
        getOrCreateIssue(issueCode);
        uint8_t modFlag = tag[25];
        char    side    = static_cast<char>(tag[9]);
        OrderEvent ev;
        ev.exchange      = Exchange::TSE;
        ev.instrument    = makeKey(issueCode);
        ev.kind          = (modFlag == 1) ? OrderEventKind::MODIFY : OrderEventKind::ADD;
        ev.orderID       = be32(tag + 5);
        ev.side          = (side == 'B') ? Side::BID : Side::ASK;
        ev.price         = tsePrice(be64(tag + 16));
        ev.isMarketOrder = ev.price.isMarket;
        ev.quantity      = be48(tag + 10);
        ev.bookPosition  = 0;
        ev.priorityChange= (modFlag == 0);
        ev.orderCondition= tag[24];
        ev.exchangeTime  = exchTime(currentTimeSec_, tag);
        ev.receivedAt    = nsNow();
        this->emitOrderEvent(ev);
        books_.apply(ev);
    }

    void onTagE(const TsePktHeader& hdr, const uint8_t* tag, uint8_t l) {
        if (l < 20) return;
        std::string issueCode(hdr.issueCode, strnlen(hdr.issueCode, 12));
        uint32_t matchID = be32(tag + 16);
        char     side    = static_cast<char>(tag[9]);
        Price px;
        auto kit = lastMatch_.find(issueCode);
        if (kit != lastMatch_.end() && kit->second.matchID == matchID)
            px = tsePrice(kit->second.lastPrice);
        TradeEvent ev;
        ev.exchange     = Exchange::TSE;
        ev.instrument   = makeKey(issueCode);
        ev.kind         = TradeKind::NORMAL;
        ev.tradeID      = matchID;
        ev.orderID      = be32(tag + 5);
        ev.aggressor    = (side == 'B') ? Side::BID : Side::ASK;
        ev.price        = px;
        ev.quantity     = be48(tag + 10);
        ev.isPrintable  = true;
        ev.exchangeTime = exchTime(currentTimeSec_, tag);
        ev.receivedAt   = nsNow();
        this->emitTrade(ev);
    }

    void onTagC(const TsePktHeader& hdr, const uint8_t* tag, uint8_t l) {
        if (l < 29) return;
        std::string issueCode(hdr.issueCode, strnlen(hdr.issueCode, 12));
        char side = static_cast<char>(tag[9]);
        TradeEvent ev;
        ev.exchange     = Exchange::TSE;
        ev.instrument   = makeKey(issueCode);
        ev.kind         = TradeKind::AUCTION;
        ev.tradeID      = be32(tag + 16);
        ev.orderID      = be32(tag + 5);
        ev.aggressor    = (side == 'B') ? Side::BID : Side::ASK;
        ev.price        = tsePrice(be64(tag + 20));
        ev.quantity     = be48(tag + 10);
        ev.isPrintable  = true;
        ev.exchangeTime = exchTime(currentTimeSec_, tag);
        ev.receivedAt   = nsNow();
        this->emitTrade(ev);
    }

    void onTagD(const TsePktHeader& hdr, const uint8_t* tag, uint8_t l) {
        if (l < 11) return;
        std::string issueCode(hdr.issueCode, strnlen(hdr.issueCode, 12));
        char side = static_cast<char>(tag[9]);
        OrderEvent ev;
        ev.exchange      = Exchange::TSE;
        ev.instrument    = makeKey(issueCode);
        ev.kind          = OrderEventKind::DELETE;
        ev.orderID       = be32(tag + 5);
        ev.side          = (side == 'B') ? Side::BID : Side::ASK;
        ev.quantity      = 0;
        ev.priorityChange= (tag[10] == 0);
        ev.exchangeTime  = exchTime(currentTimeSec_, tag);
        ev.receivedAt    = nsNow();
        this->emitOrderEvent(ev);
        books_.apply(ev);
    }

    void onTagR(const TsePktHeader&, const uint8_t* tag, uint8_t l) {
        if (l < 2) return;
        uint8_t flag = tag[1]; // 1 = start, 2 = end
        ControlEvent ev;
        ev.exchange   = Exchange::TSE;
        ev.kind       = (flag == 1) ? ControlKind::RESET_START : ControlKind::RESET_END;
        ev.receivedAt = nsNow();
        if (flag == 1) resetImpl(); // clear all A-tag data on reset start
        this->emitControl(ev);
    }

    void onTagL(const TsePktHeader&, const uint8_t* tag, uint8_t l) {
        if (l < 3) return;
        uint8_t testMode     = tag[1]; // 1 = prod, 2 = test
        uint8_t startEndFlag = tag[2]; // 0 = health, 1 = start, 2 = end
        ControlEvent ev;
        ev.exchange     = Exchange::TSE;
        ev.isProduction = (testMode == 1);
        switch (startEndFlag) {
        case 1:  ev.kind = ControlKind::COMM_START; break;
        case 2:  ev.kind = ControlKind::COMM_END;   break;
        default: ev.kind = ControlKind::HEARTBEAT;  break;
        }
        ev.receivedAt = nsNow();
        this->emitControl(ev);
    }

    // ── Internal state ───────────────────────────────────────────────────────
    std::unordered_map<std::string, TseIssueCache> issues_;
    uint32_t currentTimeSec_{0};
    struct MatchCtx { uint32_t matchID{}; uint64_t lastPrice{}; };
    std::unordered_map<std::string, MatchCtx> lastMatch_;
    OrderBookMap<> books_;
};

} // namespace mde
