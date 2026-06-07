#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  marketdata.h  –  Exchange-agnostic normalised domain model
//
//  Philosophy
//  ──────────
//  Every exchange adapter translates its wire messages into these structs.
//  The rest of the trade engine only ever sees this layer – it has zero
//  knowledge of HKEX message numbers, Arrowhead tag letters, or byte offsets.
//
//  Naming convention
//  ─────────────────
//   Instrument  – one tradeable thing (option series, future, equity …)
//   Strategy    – a multi-leg synthetic (combo / spread / straddle …)
//   Side        – BID / ASK unified enum
//   OrderEvent  – add / modify / delete on the full order book
//   Trade       – a matched fill
//   MarketState – session / trading-status lifecycle event
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <concepts>

namespace mde {   // Market Data Engine

// ─────────────────────────────────────────────────────────────────────────────
//  Enums
// ─────────────────────────────────────────────────────────────────────────────

enum class Exchange : uint8_t { HKEX = 1, TSE = 2 };

enum class Side : uint8_t { BID = 0, ASK = 1 };

enum class InstrumentKind : uint8_t {
    EQUITY          = 1,
    FUTURE          = 2,
    OPTION_CALL     = 3,
    OPTION_PUT      = 4,
    STRATEGY        = 5,   // multi-leg combo; details in StrategyDef
    UNKNOWN         = 0,
};

enum class SessionState : uint8_t {
    UNKNOWN               = 0,
    ORDER_ACCEPTANCE      = 1,   // pre-open, accepting orders
    TRADING               = 2,   // continuous / Zaraba
    AUCTION_OPEN          = 3,   // Itayose / pre-open matching
    AUCTION_CLOSE         = 4,   // closing auction / pre-closing
    TRADING_HALT          = 5,
    SUSPENDED             = 6,
    END_OF_SESSION        = 7,
    END_OF_DAY            = 8,
};

enum class OrderEventKind : uint8_t {
    ADD    = 0,
    MODIFY = 1,
    DELETE = 2,
    CLEAR  = 3,   // all orders on book wiped (e.g. after auction)
};

enum class TradeKind : uint8_t {
    NORMAL          = 0,   // continuous / Zaraba match
    AUCTION         = 1,   // Itayose / opening-closing cross
    TRADE_AMENDMENT = 2,   // cancellation or rectification
};

enum class StrategyLegSide : uint8_t {
    AS_DEFINED = 0,   // same direction as strategy side
    OPPOSITE   = 1,
};

// ─────────────────────────────────────────────────────────────────────────────
//  Timestamp
//  We store nanoseconds since Unix epoch throughout.
// ─────────────────────────────────────────────────────────────────────────────
using NsTimestamp = int64_t;   // nanos since 1970-01-01 00:00:00 UTC

inline NsTimestamp nsNow() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Price
//  We keep prices as int64 scaled integers throughout to avoid FP.
//  `decimals` says how many of those digits are fractional.
//  e.g. HKEX stores price as raw int32 with DecimalInPremium=2 → divide by 100
//       Arrowhead stores price as uint64, last 4 digits = decimals
// ─────────────────────────────────────────────────────────────────────────────
struct Price {
    int64_t  raw{0};      // scaled integer
    uint8_t  decimals{0}; // number of decimal places in raw
    bool     isNull{false};
    bool     isMarket{false}; // market-order placeholder

    static Price null()   { Price p; p.isNull   = true; return p; }
    static Price market() { Price p; p.isMarket  = true; return p; }

    double toDouble() const {
        if (isNull || isMarket) return 0.0;
        double d = static_cast<double>(raw);
        for (int i = 0; i < decimals; ++i) d /= 10.0;
        return d;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  InstrumentKey  – the minimal set of fields that uniquely identifies an
//  instrument across all exchanges.  Used as map key everywhere.
// ─────────────────────────────────────────────────────────────────────────────
struct InstrumentKey {
    Exchange    exchange{};
    std::string symbol;           // exchange-native symbol / ticker
    uint32_t    nativeID{0};      // exchange native numeric id (orderbookID / issueCode)

    bool operator==(const InstrumentKey& o) const {
        return exchange == o.exchange && nativeID == o.nativeID && symbol == o.symbol;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  InstrumentDef  – static reference data for ONE tradeable instrument
//
//  HKEX source: SeriesDefBase (303) + SeriesDefExt (304) + CommodityDef (301)
//               + ClassDef (302)
//  TSE  source: II tag (Issue Information) + BP tag (Base Price Information)
//               (These are in the fuller FLEX spec; we carry what's available
//                from the MBO spec pages provided.)
// ─────────────────────────────────────────────────────────────────────────────
struct InstrumentDef {
    InstrumentKey   key;
    InstrumentKind  kind{InstrumentKind::UNKNOWN};

    // Human-readable description
    std::string     name;                // commodity name / issue name
    std::string     currency;            // ISO-4217 e.g. "HKD", "JPY"
    std::string     isinCode;

    // Option / future specifics (zero/empty if not applicable)
    std::string     underlyingCode;      // underlying equity / index code
    std::string     expirationDate;      // YYYY-MM-DD
    Price           strikePrice;
    bool            effectiveTomorrow{false};

    // Lot / tick
    int64_t         contractSize{1};     // shares per lot
    Price           tickSize;            // minimum price movement
    uint8_t         priceDecimals{0};    // decimal places in all prices for this instrument

    // Status
    bool            isTradable{true};
    bool            isSuspended{false};

    // Exchange-specific raw numeric ids (for fast lookup by adapters)
    uint32_t        hkexOrderbookID{0};
    uint16_t        hkexCommodityCode{0};
    uint32_t        tseIssueCode{0};

    NsTimestamp     receivedAt{0};
};

// ─────────────────────────────────────────────────────────────────────────────
//  StrategyLeg  – one leg of a multi-leg strategy
// ─────────────────────────────────────────────────────────────────────────────
struct StrategyLeg {
    InstrumentKey   instrument;
    StrategyLegSide legSide{StrategyLegSide::AS_DEFINED};
    int32_t         ratio{1};     // relative number of contracts (signed OK for ratio)
};

// ─────────────────────────────────────────────────────────────────────────────
//  StrategyDef  – definition of a multi-leg strategy / combo
//
//  HKEX source: CombinationDef (305) cross-referenced with SeriesDefBase (303)
//  TSE  source: strategies are represented as regular Issue Codes in the MBO
//               feed (no separate combo definition tag exists in this spec
//               section); leg decomposition requires the fuller FLEX spec.
// ─────────────────────────────────────────────────────────────────────────────
struct StrategyDef {
    InstrumentKey           key;     // the strategy instrument itself
    std::vector<StrategyLeg> legs;
    std::string             priceMethod; // "NetPrice" / "NetValue" / ""
    NsTimestamp             receivedAt{0};
};

// ─────────────────────────────────────────────────────────────────────────────
//  MarketStateEvent  – session / trading status update
//
//  HKEX source: MarketStatus (320), SeriesStatus (321), CommodityStatus (322)
//  TSE  source: O tag (Trading Status)
// ─────────────────────────────────────────────────────────────────────────────
struct MarketStateEvent {
    Exchange        exchange{};
    InstrumentKey   instrument;       // empty key = market-wide event
    SessionState    state{SessionState::UNKNOWN};
    std::string     stateDetail;      // raw code for logging / debugging
    uint8_t         priority{0};      // HKEX priority field (0 for TSE)
    bool            isEndOfDay{false};
    NsTimestamp     exchangeTime{0};
    NsTimestamp     receivedAt{0};
};

// ─────────────────────────────────────────────────────────────────────────────
//  OrderEvent  – one order book change (add / modify / delete / clear)
//
//  HKEX source: AddOrder (330), ModifyOrder (331), DeleteOrder (332),
//               OrderbookClear (335)
//  TSE  source: A tag (Add Order), D tag (Order Delete)
//               [TSE has no explicit modify – it's represented as D+A pair]
// ─────────────────────────────────────────────────────────────────────────────
struct OrderEvent {
    Exchange        exchange{};
    InstrumentKey   instrument;
    OrderEventKind  kind{OrderEventKind::ADD};

    uint64_t        orderID{0};       // exchange-assigned order identifier
    Side            side{Side::BID};
    Price           price;
    uint64_t        quantity{0};      // remaining quantity (0 on DELETE)
    uint32_t        bookPosition{0};  // rank in book (HKEX); 0 if not provided (TSE)

    // Modify-specific
    bool            priorityChange{true};  // false = qty reduce, priority kept (TSE ModFlag=1)

    // Condition / type flags
    uint8_t         orderCondition{0}; // TSE: 0=normal,2=on-open,4=on-close,6=funari
    bool            isMarketOrder{false};

    NsTimestamp     exchangeTime{0};   // microsecond precision from exchange
    NsTimestamp     receivedAt{0};
};

// ─────────────────────────────────────────────────────────────────────────────
//  TradeEvent  – a matched execution or amendment
//
//  HKEX source: Trade (350), TradeAmendment (356)
//  TSE  source: K tag (Execution Summary) + E tag (Order Executed / Zaraba)
//               or C tag (Order Executed with Price / Itayose)
// ─────────────────────────────────────────────────────────────────────────────
struct TradeEvent {
    Exchange        exchange{};
    InstrumentKey   instrument;
    TradeKind       kind{TradeKind::NORMAL};

    uint64_t        tradeID{0};        // exchange match/trade ID
    uint64_t        orderID{0};        // resting order that was hit (0 if N/A)
    Side            aggressor{Side::ASK}; // which side triggered the fill
    Price           price;
    uint64_t        quantity{0};

    // Amendment-specific (kind == TRADE_AMENDMENT)
    uint8_t         amendState{0};     // HKEX: 1=deleted-giveup,2=rectified,3=deleted

    // Summary snapshot (from K tag or Trade Statistics)
    Price           bestBid;
    Price           bestAsk;

    // Printable / counted in volume
    bool            isPrintable{true};

    NsTimestamp     exchangeTime{0};
    NsTimestamp     receivedAt{0};
};

// ─────────────────────────────────────────────────────────────────────────────
//  ControlEvent  – feed lifecycle signals
//
//  HKEX source: SequenceReset (100), DRSignal (105), Heartbeat
//  TSE  source: R tag (Reset), L tag (Communication Control)
// ─────────────────────────────────────────────────────────────────────────────
enum class ControlKind : uint8_t {
    HEARTBEAT      = 0,
    SEQUENCE_RESET = 1,   // clear all cached data, re-sync
    DR_IN_PROGRESS = 2,   // disaster recovery started
    DR_COMPLETED   = 3,   // disaster recovery done, can rebuild from snapshot
    COMM_START     = 4,
    COMM_END       = 5,
    RESET_START    = 6,   // TSE R tag start – clear A tags
    RESET_END      = 7,   // TSE R tag end
};

struct ControlEvent {
    Exchange    exchange{};
    ControlKind kind{ControlKind::HEARTBEAT};
    uint32_t    channelID{0};
    uint32_t    seqNum{0};     // last known sequence number
    bool        isProduction{true};
    NsTimestamp receivedAt{0};
};

// ─────────────────────────────────────────────────────────────────────────────
//  Normalised callbacks – compile-time (template) handler model.
//
//  Adapters are templated on a Handler type and invoke these six methods on it.
//  There is no virtual dispatch: the concrete handler is fixed at build time.
//
//  HandlerDefaults provides no-op defaults so a handler only has to define the
//  events it cares about (inherit and override by hiding the relevant method).
// ─────────────────────────────────────────────────────────────────────────────
struct HandlerDefaults {
    void onControl      (const ControlEvent&)    {}
    void onInstrumentDef(const InstrumentDef&)   {}
    void onStrategyDef  (const StrategyDef&)     {}
    void onMarketState  (const MarketStateEvent&){}
    void onOrderEvent   (const OrderEvent&)      {}
    void onTrade        (const TradeEvent&)      {}
};

// A Handler must be callable with all six normalised events.
template<class H>
concept MarketDataHandler = requires(H h,
                                     const ControlEvent& c,
                                     const InstrumentDef& i,
                                     const StrategyDef& s,
                                     const MarketStateEvent& m,
                                     const OrderEvent& o,
                                     const TradeEvent& t) {
    h.onControl(c);
    h.onInstrumentDef(i);
    h.onStrategyDef(s);
    h.onMarketState(m);
    h.onOrderEvent(o);
    h.onTrade(t);
};

} // namespace mde
