#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  adapter/hkex/omdd_messages.h  –  HKEX OMD-D wire structs (little-endian)
//
//  NOTE: This header was reconstructed from the field accesses in the HKEX
//  adapter and the synthetic packets in the self-test.  The packet header,
//  message header, SeriesDefBase (303) and AddOrder (330) byte layouts are
//  pinned by the self-test and locked with static_assert.  The remaining
//  message layouts are inferred and may need to be reconciled against the
//  official OMD-D specification before live use.
// ─────────────────────────────────────────────────────────────────────────────
#include <cstdint>

namespace mde {

#pragma pack(push, 1)

// ── Framing ──────────────────────────────────────────────────────────────────
struct PktHeader {          // 16 bytes
    uint16_t pktSize;
    uint8_t  msgCount;
    uint8_t  compressionMode;
    uint32_t seqNum;
    uint64_t sendTime;
};
static_assert(sizeof(PktHeader) == 16, "PktHeader must be 16 bytes");

struct MsgHeader {          // 4 bytes – prefixes every message body
    uint16_t msgSize;
    uint16_t msgType;
};
static_assert(sizeof(MsgHeader) == 4, "MsgHeader must be 4 bytes");

// ── Message type ids ─────────────────────────────────────────────────────────
enum MsgType : uint16_t {
    MSG_SEQUENCE_RESET  = 100,
    MSG_DR_SIGNAL       = 105,
    MSG_REFRESH_COMPLETE= 203,
    MSG_COMMODITY_DEF   = 301,
    MSG_CLASS_DEF       = 302,
    MSG_SERIES_DEF_BASE = 303,
    MSG_SERIES_DEF_EXT  = 304,
    MSG_COMBINATION_DEF = 305,
    MSG_MARKET_STATUS   = 320,
    MSG_SERIES_STATUS   = 321,
    MSG_ADD_ORDER       = 330,
    MSG_MODIFY_ORDER    = 331,
    MSG_DELETE_ORDER    = 332,
    MSG_ORDERBOOK_CLEAR = 335,
    MSG_TRADE           = 350,
    MSG_TRADE_AMENDMENT = 356,
};

// ── Control ──────────────────────────────────────────────────────────────────
struct SequenceReset { MsgHeader h; uint32_t newSeqNo; };
struct DRSignal      { MsgHeader h; uint8_t  drStatus; };

// ── Reference data ───────────────────────────────────────────────────────────
struct CommodityDef {
    MsgHeader h;
    uint16_t  commodityCode;
    char      commodityName[100];
    char      baseCurrency[3];
    char      isinCode[12];
    char      underlyingCode[32];
    uint16_t  decimalInUnderlyingPrice;
};

struct ClassDef {
    MsgHeader h;
    uint8_t   country;
    uint8_t   market;
    uint8_t   instrumentGroup;
    uint16_t  commodityCode;
    uint8_t   modifier;
    uint16_t  decimalInPremium;
    uint16_t  decimalInStrikePrice;
    int32_t   tickStepSize;
    char      instrumentClassID[12];
    char      instrumentClassName[100];
    char      baseCurrency[3];
    uint8_t   tradable;
};

struct SeriesDefBase {      // msg 303 – 60 bytes (locked by self-test)
    uint16_t msgSize;
    uint16_t msgType;
    uint32_t orderbookID;
    char     symbol[32];
    uint8_t  financialProduct;
    uint16_t numberOfDecimalsPrice;
    uint8_t  numberOfLegs;
    int32_t  strikePrice;
    char     expirationDate[8];
    uint16_t decimalInStrikePrice;
    uint8_t  putOrCall;
    uint8_t  filler;
};
static_assert(sizeof(SeriesDefBase) == 60, "SeriesDefBase must be 60 bytes");

struct SeriesDefExt {
    MsgHeader h;
    uint32_t  orderBookID;
    char      symbol[32];
    uint16_t  commodityCode;
    uint8_t   market;
    uint8_t   instrumentGroup;
    uint8_t   country;
    uint8_t   modifier;
    uint16_t  expirationDate;   // bit-packed
    int64_t   contractSize;
    uint8_t   seriesStatus;
    uint8_t   effectiveTomorrow;
};

struct CombinationDef {
    MsgHeader h;
    uint32_t  comboOrderbookID;
    uint32_t  legOrderbookID;
    char      legSide[1];       // 'B'=as-defined, 'C'=opposite
    int32_t   legRatio;
};

// ── Status ───────────────────────────────────────────────────────────────────
struct MarketStatus {
    MsgHeader h;
    uint16_t  stateLevel;
    uint16_t  state;
    uint8_t   priority;
    uint32_t  orderbookID;
};

struct SeriesStatus {
    MsgHeader h;
    uint32_t  orderbookID;
    uint8_t   seriesStatus;
    uint8_t   suspensionIndicator;
};

// ── Order book ───────────────────────────────────────────────────────────────
struct AddOrder {           // msg 330 – 32 bytes (locked by self-test)
    uint16_t msgSize;
    uint16_t msgType;
    uint32_t orderbookID;
    uint64_t orderID;
    int32_t  price;
    uint32_t quantity;
    uint8_t  side;          // 0 = BID, 1 = ASK
    uint8_t  lotType;
    uint16_t orderType;
    uint32_t orderBookPosition;
};
static_assert(sizeof(AddOrder) == 32, "AddOrder must be 32 bytes");

struct ModifyOrder {        // same shape as AddOrder
    uint16_t msgSize;
    uint16_t msgType;
    uint32_t orderbookID;
    uint64_t orderID;
    int32_t  price;
    uint32_t quantity;
    uint8_t  side;
    uint8_t  lotType;
    uint16_t orderType;
    uint32_t orderBookPosition;
};
static_assert(sizeof(ModifyOrder) == 32, "ModifyOrder must be 32 bytes");

struct DeleteOrder {
    MsgHeader h;
    uint32_t  orderbookID;
    uint64_t  orderID;
    uint8_t   side;
};

struct OrderbookClear {
    MsgHeader h;
    uint32_t  orderbookID;
};

// ── Trade ────────────────────────────────────────────────────────────────────
struct Trade {
    MsgHeader h;
    uint32_t  orderbookID;
    uint64_t  tradeID;
    uint64_t  orderID;
    int32_t   price;
    uint32_t  quantity;
    uint8_t   dealType;     // bit0 = printable, bit1 = auction
    uint8_t   side;         // 2 = BID aggressor, else ASK
    uint64_t  tradeTime;
};

struct TradeAmendment {
    MsgHeader h;
    uint64_t  tradeID;
    int32_t   price;
    uint32_t  quantity;
    uint8_t   tradeState;
    uint64_t  tradeTime;
};

#pragma pack(pop)

} // namespace mde
