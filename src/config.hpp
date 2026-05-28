#pragma once

#include <cstdint>
#include <string_view>

namespace tradeengine::config {

enum class IoMode { File, Socket };

inline constexpr IoMode kIoMode = IoMode::File;

inline constexpr std::string_view kFilePath =
    "data/ddsdumplite.txt";

inline constexpr std::string_view kSocketHost = "127.0.0.1";
inline constexpr std::uint16_t   kSocketPort  = 9922;

inline constexpr std::size_t kReadBufferBytes = 64 * 1024;

inline constexpr std::string_view kLogDir      = "./";
inline constexpr std::string_view kLogFileRoot = "log";
inline constexpr std::uint32_t    kLogRollMb   = 32;

inline constexpr bool kAllData = true;

}
