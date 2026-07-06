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
#include <iostream>
#include <string>

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

    std::cout << "[INFO] registry: " << registry.instrumentCount()
              << " instruments, " << registry.strategyCount() << " strategies\n";
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  main  –  construct the compile-time–configured source and run
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    // Low-latency logging → ./log/tradeengine.<n>.txt
    nanolog::initialize(nanolog::GuaranteedLogger(), "./log/", "tradeengine", 8);

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
            mde::feed::AeronIpcSource(std::string(mde::kAeronChannel),
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
