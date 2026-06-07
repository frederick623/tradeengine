#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  adapter/hkex/hkex_market_data.h  –  HKEX-specific reference-data caches
//
//  Internal product cache used by HkexAdapter while assembling normalised
//  InstrumentDef / StrategyDef events.  Kept private to the HKEX adapter – the
//  engine never sees raw HKEX types.
// ─────────────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <string>
#include <vector>

namespace mde {

struct HkexCommodityCache {
    uint16_t    code{};
    std::string name;
    std::string currency;
    std::string isinCode;
    std::string underlyingCode;
    uint16_t    decimalInUnderlyingPrice{};
};

struct HkexClassCache {
    uint8_t  country{}, market{}, instrGroup{}, modifier{};
    uint16_t commodityCode{};
    uint16_t decimalInPremium{};
    uint16_t decimalInStrikePrice{};
    int32_t  tickStepSize{};
    std::string instrumentClassID;
    std::string instrumentClassName;
    std::string currency;
    bool     tradable{true};
};

struct HkexSeriesCache {
    uint32_t    orderbookID{};
    std::string symbol;
    uint8_t     financialProduct{};
    uint16_t    priceDecimals{};
    uint8_t     numberOfLegs{};
    int32_t     strikePrice{};          // from 303
    uint16_t    expirationDatePacked{}; // from 304 (bit-packed)
    std::string expirationDateStr;      // YYYYMMDD from 303
    uint8_t     putOrCall{};
    uint16_t    commodityCode{};
    uint8_t     market{};
    uint8_t     instrumentGroup{};
    uint8_t     country{};
    uint8_t     modifier{};
    int64_t     contractSize{1};
    uint8_t     seriesStatus{};
    bool        effectiveTomorrow{false};
    bool        has303{false};
    bool        has304{false};
};

struct HkexComboLeg {
    uint32_t legOrderbookID{};
    char     legSide{};   // 'B'=as-defined, 'C'=opposite
    int32_t  ratio{};
};

} // namespace mde
