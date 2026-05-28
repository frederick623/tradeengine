#pragma once

#include <cstdint>

namespace tradeengine {

// Generic null-sentinel values used for unset numeric fields.
inline constexpr double  kDoubleNull = -999999999999.0;
inline constexpr int32_t kInt32Null  = INT32_MIN;
inline constexpr int64_t kInt64Null  = INT64_MIN + 1;

// Common sizing constants shared across market-data structures.
inline constexpr std::size_t kPriceQueueSize = 10;
inline constexpr std::size_t kMaxTickerSize  = 5;

}
