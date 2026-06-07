// ─────────────────────────────────────────────────────────────────────────────
//  main.cpp  –  HKEX OMD-D feed handler
//
//  Drives the HKEX adapter from one of three interchangeable packet sources:
//
//    udp  <addr> <port> [iface]   live UDP / multicast (Boost.Asio)
//    file <path>                  hex text replay  (one packet per line)
//    pcap <path> [udp-port]       pcap / pcapng capture replay
//
//  Every source yields raw UDP payloads; each is fed verbatim into
//  HkexAdapter::processPacket().  The adapter emits normalised mde:: events to
//  a FanoutHandler that broadcasts to the InstrumentRegistry and a logger.
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <iostream>
#include <string>

#include "marketdata.h"
#include "adapter/hkex/hkexadapter.h"
#include "registry.h"
#include "logginghandler.h"
#include "feed/textfile.h"
#include "feed/pcap.h"
#include "feed/udp.h"
#include "NanoLog.hpp"

// Build the HKEX adapter + downstream handlers and pump the given source.
template<class Source>
static int runFeed(Source&& source) {
    LoggingHandler          logger;
    mde::InstrumentRegistry registry;
    mde::FanoutHandler      fanout(registry, logger);
    mde::HkexAdapter        hkex(fanout);

    source.run([&](const uint8_t* data, uint16_t len) {
        hkex.processPacket(data, len);
    });

    std::cout << "[INFO] registry: " << registry.instrumentCount()
              << " instruments, " << registry.strategyCount() << " strategies\n";
    return 0;
}

static int usage(const char* prog) {
    std::cout << "Usage:\n"
              << "  " << prog << " udp  <addr> <port> [iface]   live UDP/multicast\n"
              << "  " << prog << " file <path>                  hex text replay\n"
              << "  " << prog << " pcap <path> [udp-port]       pcap replay\n";
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    // Low-latency logging → ./log/tradeengine.<n>.txt
    nanolog::initialize(nanolog::GuaranteedLogger(), "./log/", "tradeengine", 8);

    if (argc < 3) return usage(argv[0]);
    const std::string mode = argv[1];

    if (mode == "udp") {
        if (argc < 4) return usage(argv[0]);
        const std::string addr  = argv[2];
        const uint16_t    port  = static_cast<uint16_t>(std::stoi(argv[3]));
        const std::string iface = (argc > 4) ? argv[4] : "";
        return runFeed(mde::feed::UdpSource(addr, port, iface));
    }
    if (mode == "file") {
        return runFeed(mde::feed::TextFileSource(argv[2]));
    }
    if (mode == "pcap") {
        const uint16_t portFilter =
            (argc > 3) ? static_cast<uint16_t>(std::stoi(argv[3])) : 0;
        return runFeed(mde::feed::PcapSource(argv[2], portFilter));
    }
    return usage(argv[0]);
}
