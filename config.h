#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  config.h  –  compile-time exchange, feed-mode and connection parameters
//
//  Edit the defaults below, or override any knob on the compiler command line
//  without touching this file:
//
//  Exchange / feed mode
//    -DMDE_EXCHANGE_TSE             TSE adapter (default: HKEX)
//    -DMDE_FEED_UDP                 live UDP / multicast
//    -DMDE_FEED_PCAP                pcap / pcapng replay
//    -DMDE_FEED_AERON               Aeron IPC stream source
//    (no flag)                      hex text-file replay  ← default
//
//  Connection parameters (feed-mode specific; unused modes are ignored)
//    -DMDE_PCAP_PATH=\"file.pcap\"
//    -DMDE_PCAP_PORT_FILTER=50001
//    -DMDE_FILE_PATH=\"packets.txt\"
//    -DMDE_UDP_ADDR=\"239.1.2.3\"
//    -DMDE_UDP_PORT=50000
//    -DMDE_UDP_IFACE=\"eth0\"
//    -DMDE_AERON_CHANNEL=\"aeron:ipc\"
//    -DMDE_AERON_STREAM_ID=1001
//
//  Usage in main.cpp:
//    mde::AdapterFor_t<kExchange, decltype(fanout)> adapter(fanout);
//    adapter.processPacket(data, len);
// ─────────────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <string_view>
#include "marketdata.h"

namespace mde {

// ── Feed mode ─────────────────────────────────────────────────────────────────
enum class FeedMode : uint8_t {
    UDP      = 0,   ///< live UDP / multicast (Boost.Asio)
    PCAP     = 1,   ///< pcap / pcapng capture replay
    TEXTFILE = 2,   ///< hex text-file replay (one packet per line)
    AERON    = 3,   ///< Aeron IPC stream source
};

// ── Active exchange ───────────────────────────────────────────────────────────
#if defined(MDE_EXCHANGE_TSE)
inline constexpr Exchange kExchange = Exchange::TSE;
#else
inline constexpr Exchange kExchange = Exchange::HKEX;   // default
#endif

// ── Active feed mode ──────────────────────────────────────────────────────────
#if defined(MDE_FEED_UDP)
inline constexpr FeedMode kFeedMode = FeedMode::UDP;
#elif defined(MDE_FEED_PCAP)
inline constexpr FeedMode kFeedMode = FeedMode::PCAP;
#elif defined(MDE_FEED_AERON)
inline constexpr FeedMode kFeedMode = FeedMode::AERON;
#else
inline constexpr FeedMode kFeedMode = FeedMode::TEXTFILE;   // default
#endif

// ── PCAP parameters ───────────────────────────────────────────────────────────
#ifndef MDE_PCAP_PATH
#  define MDE_PCAP_PATH ""
#endif
#ifndef MDE_PCAP_PORT_FILTER
#  define MDE_PCAP_PORT_FILTER 0
#endif
inline constexpr std::string_view kPcapPath       = MDE_PCAP_PATH;
inline constexpr uint16_t         kPcapPortFilter = MDE_PCAP_PORT_FILTER;

// ── Text-file parameters ──────────────────────────────────────────────────────
#ifndef MDE_FILE_PATH
#  define MDE_FILE_PATH ""
#endif
inline constexpr std::string_view kFilePath = MDE_FILE_PATH;

// ── UDP parameters ────────────────────────────────────────────────────────────
#ifndef MDE_UDP_ADDR
#  define MDE_UDP_ADDR ""
#endif
#ifndef MDE_UDP_PORT
#  define MDE_UDP_PORT 0
#endif
#ifndef MDE_UDP_IFACE
#  define MDE_UDP_IFACE ""
#endif
inline constexpr std::string_view kUdpAddr  = MDE_UDP_ADDR;
inline constexpr uint16_t         kUdpPort  = MDE_UDP_PORT;
inline constexpr std::string_view kUdpIface = MDE_UDP_IFACE;

// ── Aeron IPC parameters ───────────────────────────────────────────────────────
#ifndef MDE_AERON_CHANNEL
#  define MDE_AERON_CHANNEL "aeron:ipc"
#endif
#ifndef MDE_AERON_STREAM_ID
#  define MDE_AERON_STREAM_ID 1001
#endif
inline constexpr std::string_view kAeronChannel  = MDE_AERON_CHANNEL;
inline constexpr int32_t          kAeronStreamId = MDE_AERON_STREAM_ID;

// ── Exchange → Adapter trait ──────────────────────────────────────────────────
//  Primary template – intentionally left undefined.
//  Specialisations are injected by each adapter header:
//
//    template<class H>
//    struct AdapterFor<Exchange::HKEX, H> { using type = HkexAdapter<H>; };
//
template<Exchange E, class Handler>
struct AdapterFor;

/// Convenience alias: AdapterFor_t<E, H>  ≡  typename AdapterFor<E, H>::type
template<Exchange E, class Handler>
using AdapterFor_t = typename AdapterFor<E, Handler>::type;

} // namespace mde

// ── Inject specialisations from each adapter header ───────────────────────────
//  These headers include config.h themselves (for the primary template above),
//  which is a no-op here thanks to #pragma once.  The specialisations must come
//  after the primary template so this is the right place to pull them in.
#include "adapter/hkex/hkexadapter.h"
#include "adapter/tse/tseadapter.h"
