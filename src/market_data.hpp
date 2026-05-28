#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <list>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "constants.hpp"

namespace tradeengine {

struct SecurityDef {
    std::string Symbol;
    std::string Market;
    std::string ISINCode;
    int         ProductType{kInt32Null};
    std::string SpreadTableCode;
    std::string ShortName;
    std::string Currency;
    std::string ChiName;
    std::string GBName;
    int         LotSize{kInt32Null};
    double      PrevClose{kDoubleNull};
    char        VCMFlag{0};
    char        ShortSellFlag{0};
    char        CASFlag{0};
    char        CCASSFlag{0};
    char        DummySecurityFlag{0};
    char        StampDutyFlag{0};
    std::string ListingDate;
    std::string DelistingDate;
    std::string FreeText;
    std::string FreeText1;
    char        EFNFlag{0};
    double      AccruedInterest{kDoubleNull};
    double      CouponRate{kDoubleNull};
    double      ConversionRatio{kDoubleNull};
    double      StrikePrice{kDoubleNull};
    std::string MaturityDate;
    char        PutCall{0};
    char        WarrantStyle{0};
    std::string LPBrokerList;
    std::string Exchange;
    std::string Series;
    std::string Underlyer;
    double      OpenPrice{kDoubleNull};
    double      ContractSize{kDoubleNull};
    int         LegCount{kInt32Null};
    std::string LegRatio;
    std::string LegSymbols;
};

struct PriceData {
    std::string Symbol;
    int         Status{kInt32Null};
    std::string Time;
    double      Yield{kDoubleNull};
    double      Last{kDoubleNull};
    double      High{kDoubleNull};
    double      Low{kDoubleNull};
    std::array<int32_t,  kPriceQueueSize> BidQueue{};
    std::array<int32_t,  kPriceQueueSize> AskQueue{};
    std::array<int64_t,  kPriceQueueSize> BidSize{};
    std::array<int64_t,  kPriceQueueSize> AskSize{};
    std::array<double,   kPriceQueueSize> BidPrice{};
    std::array<double,   kPriceQueueSize> AskPrice{};
    int64_t     Volume{kInt64Null};
    double      Turnover{kDoubleNull};
    std::string BidBrokerQueue;
    std::string AskBrokerQueue;
    int         NumOfClosingTrades{kInt32Null};
    double      VWAP{kDoubleNull};
    std::string VCMCoolOffStartTime;
    std::string VCMCoolOffEndTime;
    double      VCMReferencePrice{kDoubleNull};
    double      VCMLowerPrice{kDoubleNull};
    double      VCMUpperPrice{kDoubleNull};
    double      CASReferencePrice{kDoubleNull};
    double      CASLowerPrice{kDoubleNull};
    double      CASUpperPrice{kDoubleNull};
    int         OpenInterest{kInt32Null};
    int         BidQuoteQty{kInt32Null};
    int         AskQuoteQty{kInt32Null};
    int         OrderImbalDirect{kInt32Null};
    int64_t     OrderImbalQty{kInt64Null};
    double      IEP{kDoubleNull};
    int64_t     IEV{kInt64Null};
};

struct StockInfo {
    std::string Symbol;
    bool        NeedFilter{false};
    SecurityDef SecDef;
    PriceData   Price;
};

struct StockTick {
    std::string TradeTime;
    char        TradeType{0};
    int64_t     Volume{0};
    double      Price{0};
    int64_t     Seq{0};
};

struct IndexTick {
    double      Price{0};
    int64_t     Volume{0};
    int64_t     Seq{0};
    long        ComboGroupID{0};
    int         Side{0};
    int         DealType{0};
    int         TradeCondition{0};
    int         DealInfo{0};
    int         UserRef{0};
    std::string TradeTime;
};

template <typename T>
struct TickerInfo {
    std::string Symbol;
    std::array<T, kMaxTickerSize> Ticker{};
    void push(T v) {
        for (std::size_t i = kMaxTickerSize - 1; i > 0; --i) Ticker[i] = Ticker[i - 1];
        Ticker[0] = std::move(v);
    }
};

struct IndexInfo {
    std::string Symbol;
    char        Status{0};
    std::string Time;
    double      Last{kDoubleNull};
    double      NetChgPrevDay{kDoubleNull};
    double      High{kDoubleNull};
    double      Low{kDoubleNull};
    double      EASValue{kDoubleNull};
    double      Turnover{kDoubleNull};
    double      Open{kDoubleNull};
    double      Close{kDoubleNull};
    double      PrevClose{kDoubleNull};
    int64_t     Volume{kInt64Null};
    double      NetChgPrevDayPct{kDoubleNull};
    char        ExceptionFlag{0};
};

struct ExchStatus {
    std::string Exchange;
    std::string Market;
    int         Status{kInt32Null};
};

struct OddLotEntry {
    char    Side{0};
    double  Price{kDoubleNull};
    int64_t Quantity{0};
    int64_t OrderID{0};
    int64_t BrokerID{0};
};

struct OddLotOrder {
    std::string Symbol;
    std::string Time;
    std::vector<OddLotEntry> Entries;
};

struct MarketTurnover {
    std::string Market;
    std::string Currency;
    double      Turnover{kDoubleNull};
};

struct SpreadTier {
    double PriceRange{0};
    double Tick{0};
};

struct SpreadTable {
    std::string Code;
    std::vector<SpreadTier> Tiers;
};

struct News {
    std::string NewsType;
    std::string NewsID;
    std::string HeadLine;
    std::string Time;
    std::string Body;
    char        IsLast{0};
    std::string CancelFlag;
};

struct LinkedStock {
    std::string Symbol;
    std::vector<std::string> Linked;
};

struct Chain {
    std::string UpChain;
    std::set<std::string> DownChain;
};

using StockMap         = std::unordered_map<std::string, StockInfo>;
using IndexMap         = std::unordered_map<std::string, IndexInfo>;
using StockTickerMap   = std::unordered_map<std::string, TickerInfo<StockTick>>;
using IndexTickerMap   = std::unordered_map<std::string, TickerInfo<IndexTick>>;
using ExchStatusMap    = std::unordered_map<std::string, ExchStatus>;
using OddLotMap        = std::unordered_map<std::string, OddLotOrder>;
using TurnoverMap      = std::unordered_map<std::string, MarketTurnover>;
using SpreadMap        = std::unordered_map<std::string, SpreadTable>;
using LinkedStockMap   = std::unordered_map<std::string, LinkedStock>;
using ChainMap         = std::unordered_map<std::string, Chain>;
using NewsList         = std::list<News>;

// Default no-op base: override only the events you care about.
struct MarketDataHandlerBase {
    void onSecurityDef(const SecurityDef&)           {}
    void onPriceData(const PriceData&)               {}
    void onStockTicker(const TickerInfo<StockTick>&) {}
    void onIndexTicker(const TickerInfo<IndexTick>&) {}
    void onIndexData(const IndexInfo&)               {}
    void onExchStatus(const ExchStatus&)             {}
    void onOddLot(const OddLotOrder&)                {}
    void onTurnover(const MarketTurnover&)           {}
    void onSpread(const SpreadTable&)                {}
    void onNews(const News&)                         {}
    void onLinkedStock(const LinkedStock&)           {}
    void onChain(const Chain&)                       {}
};

// Concept: any type that exposes the full handler surface.
template<typename T>
concept MarketDataHandler =
    requires(T& h,
             const SecurityDef&           sd,
             const PriceData&             pd,
             const TickerInfo<StockTick>& st,
             const TickerInfo<IndexTick>& it,
             const IndexInfo&             ii,
             const ExchStatus&            es,
             const OddLotOrder&           ol,
             const MarketTurnover&        mt,
             const SpreadTable&           sp,
             const News&                  nw,
             const LinkedStock&           ls,
             const Chain&                 ch)
    {
        h.onSecurityDef(sd);
        h.onPriceData(pd);
        h.onStockTicker(st);
        h.onIndexTicker(it);
        h.onIndexData(ii);
        h.onExchStatus(es);
        h.onOddLot(ol);
        h.onTurnover(mt);
        h.onSpread(sp);
        h.onNews(nw);
        h.onLinkedStock(ls);
        h.onChain(ch);
    };

}

