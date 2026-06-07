#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  feed/dispatch.h  –  Reader/consumer thread bridge for the packet sources
//
//  At high message rates the cost of decoding a packet (onPacket) must not stall
//  the socket / file reader, or datagrams get dropped by the kernel.  pumpThreaded
//  splits the two concerns across threads, bridged by a lock-free MPMC queue:
//
//      reader thread  ── push ──▶  MPMCSimpleQueue  ── wait_pop_bulk ──▶  consumer
//      (recv / read)                                                     (onPacket)
//
//  Because every source hands onPacket a pointer into a *reused* buffer, each
//  packet is copied into an owned queue slot before the reader advances.  Slots
//  are fixed-size inline buffers (no per-packet heap allocation on the hot path);
//  payloads larger than kMaxPacketBytes are clamped (UDP market-data datagrams
//  sit well inside one MTU, so this is not hit in practice — raise it if needed).
// ─────────────────────────────────────────────────────────────────────────────
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <thread>

#include "mpmc.hpp"

namespace mde::feed {

// Max bytes copied per packet across the reader→consumer queue.
inline constexpr size_t kMaxPacketBytes = 2048;
// Bridge queue depth (must be a power of two for MPMCSimpleQueue).
inline constexpr size_t kQueueDepth = 8192;
// Max packets drained per consumer wake-up.
inline constexpr size_t kDrainBatch = 64;

struct Packet {
    uint16_t                             len = 0;
    std::array<uint8_t, kMaxPacketBytes> data{};
};

// Run a source's read loop on a dedicated reader thread and dispatch every
// packet to onPacket on a separate consumer thread.
//
//   readLoop(sink)   – the source's blocking read loop.  It must invoke
//                      sink(const uint8_t*, uint16_t) once per packet and return
//                      when the source is exhausted / stopped.
//   onPacket(d, len) – downstream callback, runs on the consumer thread.
//
// Blocks (joining both threads) until the reader finishes and the queue drains,
// mirroring the original single-threaded run() semantics.
template<class ReadLoop, class OnPacket>
void pumpThreaded(ReadLoop&& readLoop, OnPacket&& onPacket) {
    using Queue = MPMCSimpleQueue<Packet, kQueueDepth>;
    auto queue = std::make_unique<Queue>();   // heap: slots are large

    // Consumer: drain batches and dispatch until the queue is closed and empty.
    std::thread consumer([&] {
        auto batch = std::make_unique<std::array<Packet, kDrainBatch>>();
        for (;;) {
            size_t n = queue->wait_pop_bulk(batch->data(), batch->size());
            if (n == 0) break;                // closed and fully drained
            for (size_t i = 0; i < n; ++i)
                onPacket((*batch)[i].data.data(), (*batch)[i].len);
        }
    });

    // Reader: runs on its own thread; the calling thread waits on join() below.
    std::thread reader([&] {
        readLoop([&](const uint8_t* d, uint16_t len) {
            Packet p;
            if (len > kMaxPacketBytes) len = static_cast<uint16_t>(kMaxPacketBytes);
            std::memcpy(p.data.data(), d, len);
            p.len = len;
            queue->push(p);
        });
        queue->close();                       // no more packets — wake consumer
    });

    reader.join();
    consumer.join();
}

} // namespace mde::feed
