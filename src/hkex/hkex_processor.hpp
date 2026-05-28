#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "NanoLog.hpp"
#include "../config.hpp"
#include "../md_processor.hpp"
#include "constants.hpp"

namespace tradeengine::hkex {

constexpr std::size_t kMaxNewsListSize = 50;
// ---------------------------------------------------------------------------
// HkexProcessor<Handler>  –  CRTP derived processor for the HKEX DDS feed.
//
// Inherits the shared market-data stores from MdProcessor<Derived> and
// implements HKEX-specific line routing and OMS attribute parsing.
//
// Usage:
//   HkexProcessor<MyHandler> proc{ MyHandler{} };
//   proc.process(line);          // dispatched via CRTP
//   proc.handler().someQuery();  // access the strategy handler
// ---------------------------------------------------------------------------
template<MarketDataHandler Handler>
class HkexProcessor : public MdProcessor<HkexProcessor<Handler>> {
    using Base = MdProcessor<HkexProcessor<Handler>>;

public:
    explicit HkexProcessor(Handler handler) : m_handler(std::move(handler)) {}

    // Access the strategy handler (e.g. to read state after processing).
    Handler&       handler()       { return m_handler; }
    const Handler& handler() const { return m_handler; }

    // Called by MdProcessor::process() via CRTP dispatch.
    void do_process(std::string_view line);

private:
    // HKEX-specific routing and parsing helpers.
    void process_listsetup(std::string_view upChain, std::string_view body);
    void process_stock_ticker(std::string_view symbol, std::string_view body);
    void process_index_ticker(std::string_view symbol, std::string_view body);
    void process_stock(std::string_view symbol, std::string_view body, bool isImage);
    void process_index(std::string_view symbol, std::string_view body, bool isImage);
    void process_exch_status(std::string_view body);
    void process_market_turnover(std::string_view symbol, std::string_view body, bool isImage);
    void process_odd_lot(std::string_view symbol, std::string_view body);
    void process_news(std::string_view symbol, std::string_view body);
    void process_news_headline(std::string_view body, News& news);
    void process_news_line(std::string_view body, News& news);
    void process_spread(std::string_view symbol, std::string_view body);

    // Returns true when the symbol represents a stock (starts with a digit).
    static bool is_stock(std::string_view sym) {
        return !sym.empty() && std::isdigit(static_cast<unsigned char>(sym.front()));
    }

    Handler m_handler;
};

// ---------------------------------------------------------------------------
// process_stock
// ---------------------------------------------------------------------------
template<MarketDataHandler Handler>
void HkexProcessor<Handler>::process_stock(std::string_view symbol, std::string_view body, bool isImage) {
    auto& info = this->m_stocks[std::string(symbol)];
    if (info.Symbol.empty()) {
        info.Symbol = symbol;
        info.SecDef.Symbol = symbol;
        info.Price.Symbol  = symbol;
    }
    if (info.NeedFilter) return;

    auto& sd = info.SecDef;
    auto& pd = info.Price;
    bool secUpd = false, prcUpd = false, resetImg = true;
    std::string linked;

    this->m_parser.reset(body);
    int attr; std::string_view val;
    while (this->m_parser.next_pair(attr, val)) {
        switch (attr) {
            case OMS_SYMBOL: break;
            case OMS_STATUS:        pd.Status = sv_to_int(val);        prcUpd = true; resetImg = false; break;
            case OMS_ACCOUNT:       sd.ShortName = val;                secUpd = true; resetImg = false; break;
            case OMS_PRODTYPE:      sd.ProductType = sv_to_int(val);   secUpd = true; resetImg = false; break;
            case OMS_CURRENCY:      sd.Currency = val;                 secUpd = true; resetImg = false; break;
            case OMS_LOTSIZE:       sd.LotSize = sv_to_int(val);       secUpd = true; resetImg = false; break;
            case OMS_FIRSTROW:      sd.FreeText = val;                 secUpd = true; resetImg = false; break;
            case OMS_FIRSTROW+1:    sd.FreeText1 = val;                secUpd = true; resetImg = false; break;
            case OMS_STRIKE_PRC:    sd.StrikePrice = sv_to_double(val);secUpd = true; resetImg = false; break;
            case OMS_PUTCALL:       sd.PutCall = sv_to_char(val);      secUpd = true; resetImg = false; break;
            case OMS_MATURITY:      sd.MaturityDate = val;             secUpd = true; resetImg = false; break;
            case OMS_ISIN:          sd.ISINCode = val;                 secUpd = true; resetImg = false; break;
            case OMS_PREV_CLOSE:    sd.PrevClose = sv_to_double(val);  secUpd = true; resetImg = false; break;
            case OMS_CHI_NAME:      sd.ChiName = val;                  secUpd = true; resetImg = false; break;
            case OMS_SPREAD:        sd.SpreadTableCode = val;          secUpd = true; resetImg = false; break;
            case OMS_EXCH_MARKET:   sd.Market = val;                   secUpd = true; resetImg = false; break;
            case OMS_EXCHANGE:      sd.Exchange = val;                 secUpd = true; resetImg = false; break;
            case OMS_SERIESID:      sd.Series = val;                   secUpd = true; resetImg = false; break;
            case OMS_UmmdsYMBOL:    sd.Underlyer = val;                secUpd = true; resetImg = false; break;
            case OMS_OPEN_PRICE:    sd.OpenPrice = sv_to_double(val);  secUpd = true; resetImg = false; break;
            case OMS_CONTRACT_SIZE: sd.ContractSize = sv_to_double(val);secUpd = true; resetImg = false; break;
            case OMS_LEG_COUNT:     sd.LegCount = sv_to_int(val);      secUpd = true; resetImg = false; break;
            case OMS_LEG_RATIO:     sd.LegRatio = val;                 secUpd = true; resetImg = false; break;
            case OMS_LEG_SYMBOLS:   sd.LegSymbols = val;               secUpd = true; resetImg = false; break;
            case OMS_CCASS:         sd.CCASSFlag = sv_to_char(val);    secUpd = true; resetImg = false; break;
            case OMS_SHORTSELL:     sd.ShortSellFlag = sv_to_char(val);secUpd = true; resetImg = false; break;
            case OMS_CONVERSION_RATIO:     sd.ConversionRatio = sv_to_double(val); secUpd = true; resetImg = false; break;
            case OMS_BOND_ACCRUEDINTEREST: sd.AccruedInterest = sv_to_double(val); secUpd = true; resetImg = false; break;
            case OMS_DUMMY_STOCK:   sd.DummySecurityFlag = sv_to_char(val); secUpd = true; resetImg = false; break;
            case OMS_STAMPDUTY:     sd.StampDutyFlag = sv_to_char(val);secUpd = true; resetImg = false; break;
            case OMS_GB_NAME:       sd.GBName = val;                   secUpd = true; resetImg = false; break;
            case OMS_LP_BROKERS:    sd.LPBrokerList = val;             secUpd = true; resetImg = false; break;
            case OMS_LISTING_DATE:  sd.ListingDate = val;              secUpd = true; resetImg = false; break;
            case OMS_DELISTING_DATE:sd.DelistingDate = val;            secUpd = true; resetImg = false; break;
            case OMS_EFN_FLAG:      sd.EFNFlag = sv_to_char(val);      secUpd = true; resetImg = false; break;
            case OMS_COUPON_RATE:   sd.CouponRate = sv_to_double(val); secUpd = true; resetImg = false; break;
            case OMS_BASKET_WARRANT_STYLE: sd.WarrantStyle = sv_to_char(val); secUpd = true; resetImg = false; break;
            case OMS_VCM_FLAG:      sd.VCMFlag = sv_to_char(val);      secUpd = true; resetImg = false; break;
            case OMS_CAS_FLAG:      sd.CASFlag = sv_to_char(val);      secUpd = true; resetImg = false; break;
            case OMS_LINKED_STOCK:  if (config::kAllData || !isImage) linked = val; resetImg = false; break;
            case OMS_OPEN_IN:       pd.OpenInterest = sv_to_int(val);  prcUpd = true; resetImg = false; break;
            case OMS_TIME:          pd.Time = val;                                    resetImg = false; break;
            case OMS_BID_SIZE:      pd.BidSize[0] = sv_to_int64(val);  prcUpd = true; resetImg = false; break;
            case OMS_BID_SIZE_2:  case OMS_BID_SIZE_3:  case OMS_BID_SIZE_4:  case OMS_BID_SIZE_5:
                pd.BidSize[attr - OMS_BID_SIZE_2 + 1] = sv_to_int64(val);  prcUpd = true; resetImg = false; break;
            case OMS_BID_SIZE_6:  case OMS_BID_SIZE_7:  case OMS_BID_SIZE_8:  case OMS_BID_SIZE_9:  case OMS_BID_SIZE_10:
                pd.BidSize[attr - OMS_BID_SIZE_6 + 5] = sv_to_int64(val);  prcUpd = true; resetImg = false; break;
            case OMS_OFFER_SIZE:    pd.AskSize[0] = sv_to_int64(val);  prcUpd = true; resetImg = false; break;
            case OMS_ASK_SIZE_2:  case OMS_ASK_SIZE_3:  case OMS_ASK_SIZE_4:  case OMS_ASK_SIZE_5:
                pd.AskSize[attr - OMS_ASK_SIZE_2 + 1] = sv_to_int64(val);  prcUpd = true; resetImg = false; break;
            case OMS_ASK_SIZE_6:  case OMS_ASK_SIZE_7:  case OMS_ASK_SIZE_8:  case OMS_ASK_SIZE_9:  case OMS_ASK_SIZE_10:
                pd.AskSize[attr - OMS_ASK_SIZE_6 + 5] = sv_to_int64(val);  prcUpd = true; resetImg = false; break;
            case OMS_BID:           pd.BidPrice[0] = sv_to_double(val); prcUpd = true; resetImg = false; break;
            case OMS_OFFER:         pd.AskPrice[0] = sv_to_double(val); prcUpd = true; resetImg = false; break;
            case OMS_BID_PRICE_2: case OMS_BID_PRICE_3: case OMS_BID_PRICE_4: case OMS_BID_PRICE_5:
                pd.BidPrice[attr - OMS_BID_PRICE_2 + 1] = sv_to_double(val); prcUpd = true; resetImg = false; break;
            case OMS_BID_PRICE_6: case OMS_BID_PRICE_7: case OMS_BID_PRICE_8: case OMS_BID_PRICE_9: case OMS_BID_PRICE_10:
                pd.BidPrice[attr - OMS_BID_PRICE_6 + 5] = sv_to_double(val); prcUpd = true; resetImg = false; break;
            case OMS_ASK_PRICE_2: case OMS_ASK_PRICE_3: case OMS_ASK_PRICE_4: case OMS_ASK_PRICE_5:
                pd.AskPrice[attr - OMS_ASK_PRICE_2 + 1] = sv_to_double(val); prcUpd = true; resetImg = false; break;
            case OMS_ASK_PRICE_6: case OMS_ASK_PRICE_7: case OMS_ASK_PRICE_8: case OMS_ASK_PRICE_9: case OMS_ASK_PRICE_10:
                pd.AskPrice[attr - OMS_ASK_PRICE_6 + 5] = sv_to_double(val); prcUpd = true; resetImg = false; break;
            case OMS_BID_QUEUE_1: case OMS_BID_QUEUE_2: case OMS_BID_QUEUE_3: case OMS_BID_QUEUE_4: case OMS_BID_QUEUE_5:
                pd.BidQueue[attr - OMS_BID_QUEUE_1]     = sv_to_int(val);    prcUpd = true; resetImg = false; break;
            case OMS_BID_QUEUE_6: case OMS_BID_QUEUE_7: case OMS_BID_QUEUE_8: case OMS_BID_QUEUE_9: case OMS_BID_QUEUE_10:
                pd.BidQueue[attr - OMS_BID_QUEUE_6 + 5] = sv_to_int(val);    prcUpd = true; resetImg = false; break;
            case OMS_ASK_QUEUE_1: case OMS_ASK_QUEUE_2: case OMS_ASK_QUEUE_3: case OMS_ASK_QUEUE_4: case OMS_ASK_QUEUE_5:
                pd.AskQueue[attr - OMS_ASK_QUEUE_1]     = sv_to_int(val);    prcUpd = true; resetImg = false; break;
            case OMS_ASK_QUEUE_6: case OMS_ASK_QUEUE_7: case OMS_ASK_QUEUE_8: case OMS_ASK_QUEUE_9: case OMS_ASK_QUEUE_10:
                pd.AskQueue[attr - OMS_ASK_QUEUE_6 + 5] = sv_to_int(val);    prcUpd = true; resetImg = false; break;
            case MM_BID_QUANTITY:   pd.BidQuoteQty = sv_to_int(val);   prcUpd = true; resetImg = false; break;
            case MM_ASK_QUANTITY:   pd.AskQuoteQty = sv_to_int(val);   prcUpd = true; resetImg = false; break;
            case OMS_BID_IDS:       pd.BidBrokerQueue = val;           prcUpd = true; resetImg = false; break;
            case OMS_ASK_IDS:       pd.AskBrokerQueue = val;           prcUpd = true; resetImg = false; break;
            case OMS_YIELD:         pd.Yield    = sv_to_double(val);   prcUpd = true; resetImg = false; break;
            case OMS_L_PRICE:       pd.Last     = sv_to_double(val);   prcUpd = true; resetImg = false; break;
            case OMS_HIGH:          pd.High     = sv_to_double(val);   prcUpd = true; resetImg = false; break;
            case OMS_LOW:           pd.Low      = sv_to_double(val);   prcUpd = true; resetImg = false; break;
            case OMS_VOLUME:        pd.Volume   = sv_to_int64(val);    prcUpd = true; resetImg = false; break;
            case OMS_TURNOVER:      pd.Turnover = sv_to_double(val);   prcUpd = true; resetImg = false; break;
            case OMS_CLOSING_TRADES:pd.NumOfClosingTrades = sv_to_int(val); prcUpd = true; resetImg = false; break;
            case OMS_VWAP:          pd.VWAP = sv_to_double(val);       prcUpd = true; resetImg = false; break;
            case OMS_VCM_START_TIME:pd.VCMCoolOffStartTime = val;      prcUpd = true; resetImg = false; break;
            case OMS_VCM_END_TIME:  pd.VCMCoolOffEndTime = val;        prcUpd = true; resetImg = false; break;
            case OMS_VCM_REF_PRICE: pd.VCMReferencePrice  = sv_to_double(val); prcUpd = true; resetImg = false; break;
            case OMS_VCM_LOWER_PRICE:pd.VCMLowerPrice     = sv_to_double(val); prcUpd = true; resetImg = false; break;
            case OMS_VCM_UPPER_PRICE:pd.VCMUpperPrice     = sv_to_double(val); prcUpd = true; resetImg = false; break;
            case OMS_CAS_REF_PRICE: pd.CASReferencePrice  = sv_to_double(val); prcUpd = true; resetImg = false; break;
            case OMS_CAS_LOWER_PRICE:pd.CASLowerPrice     = sv_to_double(val); prcUpd = true; resetImg = false; break;
            case OMS_CAS_UPPER_PRICE:pd.CASUpperPrice     = sv_to_double(val); prcUpd = true; resetImg = false; break;
            case OMS_ORDER_IMBAL_SIDE:pd.OrderImbalDirect = sv_to_int(val);    prcUpd = true; resetImg = false; break;
            case OMS_ORDER_IMBAL_QTY: pd.OrderImbalQty   = sv_to_int64(val);   prcUpd = true; resetImg = false; break;
            case OMS_IEP:            pd.IEP = sv_to_double(val);       prcUpd = true; resetImg = false; break;
            case OMS_IEV:            pd.IEV = sv_to_int64(val);        prcUpd = true; resetImg = false; break;
            default: resetImg = false; break;
        }
    }

    {
        const bool fire = config::kAllData ? (secUpd || (isImage && resetImg)) : (isImage ? resetImg : secUpd);
        if (fire) m_handler.onSecurityDef(sd);
    }
    if (prcUpd || (isImage && resetImg)) m_handler.onPriceData(pd);

    if (!linked.empty()) {
        LinkedStock ls; ls.Symbol = symbol;
        LineParser lp(linked, ' ');
        std::string_view t;
        while (lp.next(t)) if (!t.empty()) ls.Linked.emplace_back(t);
        this->m_linkedStocks[ls.Symbol] = ls;
        m_handler.onLinkedStock(ls);
    }
}

// ---------------------------------------------------------------------------
// process_index
// ---------------------------------------------------------------------------
template<MarketDataHandler Handler>
void HkexProcessor<Handler>::process_index(std::string_view symbol, std::string_view body, bool isImage) {
    auto& idx = this->m_indexes[std::string(symbol)];
    if (idx.Symbol.empty()) idx.Symbol = symbol;
    bool updated = false, resetImg = true;
    this->m_parser.reset(body);
    int attr; std::string_view val;
    while (this->m_parser.next_pair(attr, val)) {
        switch (attr) {
            case OMS_SYMBOL:              break;
            case OMS_INDEX_STATUS:        idx.Status = sv_to_char(val);          updated = true; resetImg = false; break;
            case OMS_TIME:                idx.Time = val;                         updated = true; resetImg = false; break;
            case OMS_L_PRICE:             idx.Last = sv_to_double(val);          updated = true; resetImg = false; break;
            case OMS_NETCHANGE:           idx.NetChgPrevDay = sv_to_double(val); updated = true; resetImg = false; break;
            case OMS_HIGH:                idx.High = sv_to_double(val);          updated = true; resetImg = false; break;
            case OMS_LOW:                 idx.Low  = sv_to_double(val);          updated = true; resetImg = false; break;
            case OMS_INDEX_EAV:           idx.EASValue = sv_to_double(val);      updated = true; resetImg = false; break;
            case OMS_TURNOVER:            idx.Turnover = sv_to_double(val);      updated = true; resetImg = false; break;
            case OMS_OPEN_PRICE:          idx.Open = sv_to_double(val);          updated = true; resetImg = false; break;
            case OMS_CLOSE_SETTLEMENT:    idx.Close = sv_to_double(val);         updated = true; resetImg = false; break;
            case OMS_PREV_CLOSE:          idx.PrevClose = sv_to_double(val);     updated = true; resetImg = false; break;
            case OMS_VOLUME:              idx.Volume = sv_to_int64(val);         updated = true; resetImg = false; break;
            case OMS_INDEX_NETCHANGE_PCT: idx.NetChgPrevDayPct = sv_to_double(val); updated = true; resetImg = false; break;
            case OMS_INDEX_EXCEPTION_FLAG:idx.ExceptionFlag = sv_to_char(val);   updated = true; resetImg = false; break;
            default: resetImg = false; break;
        }
    }
    if (updated || (isImage && resetImg)) m_handler.onIndexData(idx);
}

// ---------------------------------------------------------------------------
// process_odd_lot
// ---------------------------------------------------------------------------
template<MarketDataHandler Handler>
void HkexProcessor<Handler>::process_odd_lot(std::string_view symbol, std::string_view body) {
    OddLotOrder ord{};
    if (symbol.size() > 7) ord.Symbol = symbol.substr(7); else return;
    this->m_parser.reset(body);
    int attr; std::string_view val;
    while (this->m_parser.next_pair(attr, val)) {
        if (attr == OMS_TIME) { ord.Time = val; continue; }
        if (attr < 200) continue;
        if (val.empty() || (val.front() != 'A' && val.front() != 'B')) continue;
        LineParser sp(val, ' ');
        std::string_view tok;
        int i = 0;
        OddLotEntry e{};
        while (sp.next(tok)) {
            switch (i % 5) {
                case 0: e.Side     = sv_to_char(tok);   break;
                case 1: e.Price    = sv_to_double(tok); break;
                case 2: e.Quantity = sv_to_int64(tok);  break;
                case 3: e.OrderID  = sv_to_int64(tok);  break;
                case 4: e.BrokerID = sv_to_int64(tok); ord.Entries.push_back(e); break;
            }
            ++i;
        }
    }
    this->m_oddLots[ord.Symbol + ord.Time] = ord;
    m_handler.onOddLot(ord);
}

// ---------------------------------------------------------------------------
// process_news  /  process_news_headline  /  process_news_line
// ---------------------------------------------------------------------------
template<MarketDataHandler Handler>
void HkexProcessor<Handler>::process_news(std::string_view symbol, std::string_view body) {
    News news{};
    const auto len = symbol.size();
    bool isHeadLine = false;
    if (len > 15 && symbol.substr(0, 8) == "NEWSHEAD") {
        news.NewsType = symbol.substr(12, 3); news.NewsID = symbol.substr(15); isHeadLine = true;
    } else if (len > 11) {
        news.NewsType = symbol.substr(8, 3);  news.NewsID = symbol.substr(11);
    } else return;
    if (isHeadLine) process_news_headline(body, news);
    else            process_news_line(body, news);
}

template<MarketDataHandler Handler>
void HkexProcessor<Handler>::process_news_headline(std::string_view body, News& news) {
    std::string cxl = "1"; char isLast = 'N';
    this->m_parser.reset(body);
    int attr; std::string_view val;
    while (this->m_parser.next_pair(attr, val)) {
        switch (attr) {
            case OMS_STATUS:    cxl = val;                 break;
            case OMS_FREE_TEXT: news.HeadLine = val;       break;
            case OMS_TIME:      news.Time = val;           break;
            case OMS_USER_REF:  isLast = sv_to_char(val); break;
            default: break;
        }
    }
    news.IsLast = isLast; news.CancelFlag = cxl;
    if (this->m_news.size() >= kMaxNewsListSize) {
        bool last = false;
        while (!this->m_news.empty() && !last) { last = this->m_news.front().IsLast == 'Y'; this->m_news.pop_front(); }
    }
    this->m_news.push_back(news);
    m_handler.onNews(news);
}

template<MarketDataHandler Handler>
void HkexProcessor<Handler>::process_news_line(std::string_view body, News& news) {
    std::string cxl = "1"; char isLast = 'N';
    std::vector<std::string> lines;
    this->m_parser.reset(body);
    int attr; std::string_view val;
    while (this->m_parser.next_pair(attr, val)) {
        switch (attr) {
            case OMS_SOURCE_TIME: case 6666: case 8888: case 9999: break;
            case OMS_STATUS:   cxl = val;                          break;
            case OMS_TIME:     news.Time = val;                    break;
            case OMS_USER_REF: isLast = sv_to_char(val);           break;
            default:
                if (attr >= 200 && !val.empty() && val.front() != ' ') lines.emplace_back(val);
                break;
        }
    }
    for (auto it = lines.begin(); it != lines.end(); ++it) {
        news.Body = *it;
        news.IsLast = (std::next(it) == lines.end()) ? isLast : 'N';
        news.CancelFlag = cxl;
        if (this->m_news.size() >= kMaxNewsListSize) {
            bool last = false;
            while (!this->m_news.empty() && !last) { last = this->m_news.front().IsLast == 'Y'; this->m_news.pop_front(); }
        }
        this->m_news.push_back(news);
        m_handler.onNews(news);
    }
}

// ---------------------------------------------------------------------------
// process_spread
// ---------------------------------------------------------------------------
template<MarketDataHandler Handler>
void HkexProcessor<Handler>::process_spread(std::string_view symbol, std::string_view body) {
    SpreadTable st{};
    st.Code = symbol;
    this->m_parser.reset(body);
    int attr; std::string_view val;
    while (this->m_parser.next_pair(attr, val)) {
        if (attr != OMS_SPREAD) continue;
        LineParser sp(val, ',');
        std::string_view tok;
        int i = 0;
        SpreadTier tier{};
        while (sp.next(tok)) {
            if (i % 2 == 0) tier.PriceRange = sv_to_double(tok);
            else { tier.Tick = sv_to_double(tok); st.Tiers.push_back(tier); }
            ++i;
        }
    }
    this->m_spreads[st.Code] = st;
    m_handler.onSpread(st);
}

} // namespace tradeengine::hkex

namespace tradeengine::hkex {

namespace detail {
inline bool starts_with(std::string_view s, std::string_view p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}
} // namespace detail

// ---------------------------------------------------------------------------
// do_process  (CRTP entry point – called by MdProcessor::process())
// ---------------------------------------------------------------------------
template<MarketDataHandler Handler>
void HkexProcessor<Handler>::do_process(std::string_view line) {
    if (line.empty()) return;
    LineParser head(line);
    std::string_view tag;
    if (!head.next(tag) || tag.empty()) return;

    const bool isImage     = tag == kTagImage;
    const bool isUpdate    = tag == kTagUpdate;
    const bool isListSetup = tag == kTagListSetup;
    if (!isImage && !isUpdate && !isListSetup) return;

    std::string_view symbol;
    if (!head.next(symbol) || symbol.empty()) return;

    const std::size_t prefix = tag.size() + 1 + symbol.size() + 1;
    std::string_view body = prefix <= line.size() ? line.substr(prefix) : std::string_view{};

    if (isListSetup)                                    { process_listsetup(symbol, body); return; }
    if (detail::starts_with(symbol, "HEARTBEAT"))       return;
    if (detail::starts_with(symbol, kTickerPrefix)) {
        auto sym = symbol.substr(kTickerPrefix.size());
        if (is_stock(sym)) process_stock_ticker(sym, body);
        else               process_index_ticker(sym, body);
        return;
    }
    if (is_stock(symbol))                                                                         { process_stock(symbol, body, isImage); return; }
    if (detail::starts_with(symbol, kPfxExch))                                                   { process_exch_status(body); return; }
    if (detail::starts_with(symbol, kPfxTurn))                                                   { process_market_turnover(symbol, body, isImage); return; }
    if (detail::starts_with(symbol, kPfxOdd))                                                    { process_odd_lot(symbol, body); return; }
    if (detail::starts_with(symbol, kPfxNews))                                                   { process_news(symbol, body); return; }
    if (detail::starts_with(symbol, kPfxSpread) && (isUpdate || (isImage && config::kAllData)))  { process_spread(symbol, body); return; }
    if (auto it = this->m_indexes.find(std::string(symbol)); it != this->m_indexes.end())        { process_index(symbol, body, isImage); return; }
}

// ---------------------------------------------------------------------------
// process_listsetup
// ---------------------------------------------------------------------------
template<MarketDataHandler Handler>
void HkexProcessor<Handler>::process_listsetup(std::string_view upChain, std::string_view body) {
    auto& chain = this->m_chains[std::string(upChain)];
    chain.UpChain = upChain;
    this->m_parser.reset(body);
    std::string_view tok;
    while (this->m_parser.next(tok)) {
        if (tok.empty() || tok.size() >= 32) continue;
        chain.DownChain.emplace(tok);
    }
    if (!chain.DownChain.empty()) m_handler.onChain(chain);
}

// ---------------------------------------------------------------------------
// process_stock_ticker
// ---------------------------------------------------------------------------
template<MarketDataHandler Handler>
void HkexProcessor<Handler>::process_stock_ticker(std::string_view symbol, std::string_view body) {
    auto& info = this->m_stockTickers[std::string(symbol)];
    if (info.Symbol.empty()) info.Symbol = symbol;
    StockTick tick{};
    this->m_parser.reset(body);
    int attr; std::string_view val;
    while (this->m_parser.next_pair(attr, val)) {
        switch (attr) {
            case OMS_L_PRICE:   tick.Price     = sv_to_double(val); break;
            case OMS_QUANTITY:  tick.Volume    = sv_to_int64(val);  break;
            case OMS_TRAN_NO:   tick.Seq       = sv_to_int64(val);  break;
            case OMS_TIME:      tick.TradeTime = val;               break;
            case OMS_TRADETYPE: tick.TradeType = sv_to_char(val);   break;
            default: break;
        }
    }
    info.push(std::move(tick));
    m_handler.onStockTicker(info);
}

// ---------------------------------------------------------------------------
// process_index_ticker
// ---------------------------------------------------------------------------
template<MarketDataHandler Handler>
void HkexProcessor<Handler>::process_index_ticker(std::string_view symbol, std::string_view body) {
    auto& info = this->m_indexTickers[std::string(symbol)];
    if (info.Symbol.empty()) info.Symbol = symbol;
    IndexTick tick{};
    this->m_parser.reset(body);
    int attr; std::string_view val;
    while (this->m_parser.next_pair(attr, val)) {
        switch (attr) {
            case OMS_L_PRICE:   tick.Price          = sv_to_double(val); break;
            case OMS_QUANTITY:  tick.Volume         = sv_to_int64(val);  break;
            case OMS_TRAN_NO:   tick.Seq            = sv_to_int64(val);  break;
            case OMS_SIDE:      tick.Side           = sv_to_int(val);    break;
            case OMS_FREE_TEXT: tick.DealInfo       = sv_to_int(val);    break;
            case OMS_PRODTYPE:  tick.DealType       = sv_to_int(val);    break;
            case OMS_INSTRUCT:  tick.TradeCondition = sv_to_int(val);    break;
            case OMS_TIME:      tick.TradeTime      = val;               break;
            case OMS_SYSTEMREF: tick.ComboGroupID   = sv_to_int64(val);  break;
            case OMS_USER_REF:  tick.UserRef        = sv_to_int(val);    break;
            default: break;
        }
    }
    info.push(std::move(tick));
    m_handler.onIndexTicker(info);
}

// ---------------------------------------------------------------------------
// process_exch_status
// ---------------------------------------------------------------------------
template<MarketDataHandler Handler>
void HkexProcessor<Handler>::process_exch_status(std::string_view body) {
    ExchStatus st{};
    this->m_parser.reset(body);
    int attr; std::string_view val;
    while (this->m_parser.next_pair(attr, val)) {
        switch (attr) {
            case OMS_EXCH_MARKET: st.Market   = val;            break;
            case OMS_STATUS:      st.Status   = sv_to_int(val); break;
            case OMS_EXCHANGE:    st.Exchange = val;            break;
            default: break;
        }
    }
    this->m_exchanges[st.Exchange + st.Market] = st;
    m_handler.onExchStatus(st);
}

// ---------------------------------------------------------------------------
// process_market_turnover
// ---------------------------------------------------------------------------
template<MarketDataHandler Handler>
void HkexProcessor<Handler>::process_market_turnover(std::string_view symbol, std::string_view body, bool isImage) {
    MarketTurnover mt{};
    const auto len = symbol.size();
    if      (len >= 15) { mt.Market = symbol.substr(8, 4); mt.Currency = symbol.substr(12, 3); }
    else if (len >= 14) { mt.Market = symbol.substr(8, 3); mt.Currency = symbol.substr(11, 3); }
    else if (len >= 12) { mt.Market = symbol.substr(8, 4); }
    else if (len >= 11) { mt.Market = symbol.substr(8, 3); }
    else return;

    bool updated = false, reset = true;
    this->m_parser.reset(body);
    int attr; std::string_view val;
    while (this->m_parser.next_pair(attr, val)) {
        if      (attr == OMS_TURNOVER) { mt.Turnover = sv_to_double(val); updated = true; reset = false; }
        else if (attr != OMS_SYMBOL)   { reset = false; }
    }
    if (updated || (isImage && reset)) {
        this->m_turnovers[mt.Market + mt.Currency] = mt;
        m_handler.onTurnover(mt);
    }
}

} // namespace tradeengine::hkex
