// ─────────────────────────────────────────────────────────────────────────────
//  main.cpp  –  exchange feed handler
//
//  The exchange adapter, packet source and all connection parameters are
//  selected at compile time via config.h.  Override without editing files, e.g.:
//
//    cmake … -DCMAKE_CXX_FLAGS="\
//      -DMDE_EXCHANGE_TSE \
//      -DMDE_FEED_PCAP \
//      -DMDE_PCAP_PATH=\"/data/capture.pcap\" \
//      -DMDE_PCAP_PORT_FILTER=50001"
//
//  See config.h for the full list of knobs.
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <ctime>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <atomic>

#include "config.h"
#include "registry.h"
#include "logginghandler.h"
#include "thirdparty/xdpio/aeron.h"
#include "feed/textfile.h"
#ifdef TRADEENGINE_HAVE_PCAP
#include "feed/pcap.h"
#endif
#ifdef TRADEENGINE_HAVE_BOOST_ASIO
#include "feed/udp.h"
#endif
#include "NanoLog.hpp"

using mde::kExchange;
using mde::kFeedMode;
using mde::FeedMode;
using mde::Exchange;

namespace {

constexpr std::string_view kLogDirectory = "./log/";
constexpr std::string_view kLogSourcePrefix = "tradeengine_";
constexpr auto kLogRenamePollInterval = std::chrono::seconds(1);

std::tm localTime(std::time_t timestamp) {
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &timestamp);
#else
    localtime_r(&timestamp, &tm);
#endif
    return tm;
}

std::string formatProgramStart(std::chrono::system_clock::time_point startedAt) {
    const auto time = std::chrono::system_clock::to_time_t(startedAt);
    const auto tm = localTime(time);

    std::ostringstream out;
    out << std::put_time(&tm, "%Y%m%d%H%M%S");
    return out.str();
}

std::string formatLogIndex(uint32_t index) {
    std::ostringstream out;
    out << std::setw(3) << std::setfill('0') << index;
    return out.str();
}

class LogFileRenamer {
public:
    LogFileRenamer(std::filesystem::path logDirectory,
                   std::string sourceBaseName,
                   std::string targetTimestamp)
        : logDirectory_(std::move(logDirectory))
        , sourceBaseName_(std::move(sourceBaseName))
        , targetTimestamp_(std::move(targetTimestamp))
        , worker_([this] { run(); }) {}

    ~LogFileRenamer() {
        stop_.store(true, std::memory_order_relaxed);
        if (worker_.joinable()) {
            worker_.join();
        }
        renameReadyFiles();
    }

private:
    void run() {
        while (!stop_.load(std::memory_order_relaxed)) {
            renameReadyFiles();
            std::this_thread::sleep_for(kLogRenamePollInterval);
        }
    }

    void renameReadyFiles() const {
        std::error_code ec;
        if (!std::filesystem::exists(logDirectory_, ec)) {
            return;
        }

        const std::string prefix = sourceBaseName_ + ".";

        for (const auto& entry : std::filesystem::directory_iterator(logDirectory_, ec)) {
            if (ec || !entry.is_regular_file(ec)) {
                ec.clear();
                continue;
            }

            const std::string fileName = entry.path().filename().string();
            if (!fileName.starts_with(prefix) || !fileName.ends_with(".txt")) {
                continue;
            }

            const auto indexStart = prefix.size();
            const auto indexLength = fileName.size() - indexStart - 4;
            if (indexLength == 0) {
                continue;
            }

            uint32_t fileIndex = 0;
            const auto* begin = fileName.data() + static_cast<std::ptrdiff_t>(indexStart);
            const auto* end = begin + static_cast<std::ptrdiff_t>(indexLength);
            const auto parseResult = std::from_chars(begin, end, fileIndex);
            if (parseResult.ec != std::errc{} || parseResult.ptr != end) {
                continue;
            }

            const auto targetPath =
                logDirectory_ / (targetTimestamp_ + "_" + formatLogIndex(fileIndex) + ".txt");

            if (entry.path() == targetPath || std::filesystem::exists(targetPath, ec)) {
                ec.clear();
                continue;
            }

            std::filesystem::rename(entry.path(), targetPath, ec);
            ec.clear();
        }
    }

    std::filesystem::path logDirectory_;
    std::string sourceBaseName_;
    std::string targetTimestamp_;
    std::atomic<bool> stop_{false};
    std::thread worker_;
};

}  // namespace

// Build the compile-time–selected adapter + downstream handlers and pump source.
template<Exchange Exch, class Source>
static int runFeed(Source&& source) {
    LoggingHandler          logger;
    mde::InstrumentRegistry registry;
    auto                    fanout = mde::FanoutHandler(registry, logger);

    mde::AdapterFor_t<Exch, decltype(fanout)> adapter(fanout);

    source.run([&](const uint8_t* data, uint16_t len) {
        adapter.processPacket(data, len);
    });

    LOG_INFO << "[BOOK] dumping "
             << static_cast<uint64_t>(adapter.liveOrderBookCount())
             << " orderbooks before shutdown";
    adapter.dumpOrderBooks(logger);

    std::cout << "[INFO] registry: " << registry.instrumentCount()
              << " instruments, " << registry.strategyCount() << " strategies\n";
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  main  –  construct the compile-time–configured source and run
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    // Bounded async logging: prevents unbounded memory growth when INFO volume
    // exceeds disk write throughput.
    const auto programStartedAt = std::chrono::system_clock::now();
    const auto logTimestamp = formatProgramStart(programStartedAt);
    const auto logSourceBaseName = std::string(kLogSourcePrefix) + logTimestamp;
    std::filesystem::create_directories(std::filesystem::path(kLogDirectory));
    nanolog::initialize(nanolog::NonGuaranteedLogger(64),
                        std::string(kLogDirectory),
                        logSourceBaseName,
                        8);
    LogFileRenamer logFileRenamer(std::filesystem::path(kLogDirectory),
                                  logSourceBaseName,
                                  logTimestamp);

    if constexpr (kFeedMode == FeedMode::TEXTFILE) {
        return runFeed<kExchange>(
            mde::feed::TextFileSource(std::string(mde::kFilePath)));
    }
    else if constexpr (kFeedMode == FeedMode::PCAP) {
#ifdef TRADEENGINE_HAVE_PCAP
        return runFeed<kExchange>(
            mde::feed::PcapSource(std::string(mde::kPcapPath),
                                  mde::kPcapPortFilter));
#else
        std::cerr << "[ERROR] PCAP mode requires libpcap (not found at build time)\n";
        std::cerr << "[ERROR] Rebuild with -DMDE_FEED_TEXTFILE or install libpcap\n";
        return 1;
#endif
    }
    else if constexpr (kFeedMode == FeedMode::AERON) {
        return runFeed<kExchange>(
            AeronIpcSource(std::string(mde::kAeronChannel),
                                      mde::kAeronStreamId));
    }
    else if constexpr (kFeedMode == FeedMode::UDP) {
#ifdef TRADEENGINE_HAVE_BOOST_ASIO
        return runFeed<kExchange>(
            mde::feed::UdpSource(std::string(mde::kUdpAddr),
                                 mde::kUdpPort,
                                 std::string(mde::kUdpIface)));
#else
        std::cerr << "[ERROR] UDP mode requires Boost.Asio (not found at build time)\n";
        std::cerr << "[ERROR] Rebuild with -DMDE_FEED_TEXTFILE or install Boost\n";
        return 1;
#endif
    }
}
