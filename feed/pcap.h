#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  feed/pcap_source.h  –  Replay UDP payloads captured in a pcap/pcapng file
//
//  Opens a capture file with libpcap, walks each frame down the link / IPv4 /
//  UDP stack and hands the UDP payload (one exchange packet) to onPacket().
//  An optional destination-port filter restricts replay to a single channel.
//
//  Supported link types: Ethernet (incl. 802.1Q VLAN tags), BSD loopback
//  (DLT_NULL / DLT_LOOP) and raw IP (DLT_RAW).  IPv4 / UDP only.
// ─────────────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>

#include <pcap.h>

#include "dispatch.h"

namespace mde::feed {

class PcapSource {
public:
    explicit PcapSource(std::string path, uint16_t udpPortFilter = 0)
        : path_(std::move(path)), portFilter_(udpPortFilter) {}

    // Reader thread walks the capture; onPacket runs on the consumer thread.
    template<class OnPacket>
    void run(OnPacket&& onPacket) {
        pumpThreaded(
            [this](auto&& sink) { this->readLoop(std::forward<decltype(sink)>(sink)); },
            std::forward<OnPacket>(onPacket));
    }

private:
    // Blocking capture-walk loop; hands each UDP payload to sink(data, len).
    template<class OnPacket>
    void readLoop(OnPacket&& onPacket) {
        char errbuf[PCAP_ERRBUF_SIZE] = {0};
        pcap_t* pc = pcap_open_offline(path_.c_str(), errbuf);
        if (!pc) {
            std::cerr << "[pcap] open failed: " << errbuf << "\n";
            return;
        }
        const int dlt = pcap_datalink(pc);

        pcap_pkthdr*   hdr  = nullptr;
        const uint8_t* data = nullptr;
        size_t         pkts = 0;
        int            rc;
        while ((rc = pcap_next_ex(pc, &hdr, &data)) == 1) {
            const uint8_t* end = data + hdr->caplen;
            uint16_t       ether = 0;
            const uint8_t* l3 = linkPayload(dlt, data, end, ether);
            if (!l3 || ether != 0x0800) continue;   // IPv4 only

            uint16_t       len = 0;
            const uint8_t* pl  = udpPayload(l3, end, len);
            if (!pl) continue;
            onPacket(pl, len);
            ++pkts;
        }
        pcap_close(pc);
        std::cout << "[pcap] replayed " << pkts << " UDP payload(s) from "
                  << path_ << "\n";
    }

private:
    // Strip the link-layer header; return start of L3 and set ethertype.
    static const uint8_t* linkPayload(int dlt, const uint8_t* p,
                                      const uint8_t* end, uint16_t& ether) {
        switch (dlt) {
        case DLT_EN10MB: {
            if (p + 14 > end) return nullptr;
            ether = (uint16_t(p[12]) << 8) | p[13];
            p += 14;
            while (ether == 0x8100 || ether == 0x88A8) {   // VLAN tag(s)
                if (p + 4 > end) return nullptr;
                ether = (uint16_t(p[2]) << 8) | p[3];
                p += 4;
            }
            return p;
        }
        case DLT_NULL: {
            if (p + 4 > end) return nullptr;
            uint32_t fam;                                   // host-endian family
            __builtin_memcpy(&fam, p, 4);
            ether = (fam == 2u) ? 0x0800 : 0;               // AF_INET
            return p + 4;
        }
#ifdef DLT_LOOP
        case DLT_LOOP: {
            if (p + 4 > end) return nullptr;
            uint32_t fam = (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16)
                         | (uint32_t(p[2]) << 8)  |  uint32_t(p[3]);
            ether = (fam == 2u) ? 0x0800 : 0;
            return p + 4;
        }
#endif
#ifdef DLT_RAW
        case DLT_RAW:
            ether = ((p[0] >> 4) == 4) ? 0x0800 : 0;
            return p;
#endif
        default:
            return nullptr;
        }
    }

    // Parse IPv4 + UDP; return payload pointer and length (dest-port filtered).
    const uint8_t* udpPayload(const uint8_t* ip, const uint8_t* end,
                              uint16_t& outLen) const {
        if (ip + 20 > end || (ip[0] >> 4) != 4) return nullptr;
        const uint8_t ihl = static_cast<uint8_t>((ip[0] & 0x0F) * 4);
        if (ihl < 20 || ip + ihl > end || ip[9] != 17) return nullptr; // UDP=17
        const uint8_t* udp = ip + ihl;
        if (udp + 8 > end) return nullptr;
        const uint16_t dport = (uint16_t(udp[2]) << 8) | udp[3];
        if (portFilter_ && dport != portFilter_) return nullptr;

        const uint16_t ulen    = (uint16_t(udp[4]) << 8) | udp[5];
        const uint8_t* payload = udp + 8;
        size_t         avail   = static_cast<size_t>(end - payload);
        size_t         plen    = (ulen >= 8) ? static_cast<size_t>(ulen - 8) : avail;
        if (plen > avail)   plen = avail;
        if (plen == 0)      return nullptr;
        if (plen > 0xFFFF)  plen = 0xFFFF;
        outLen = static_cast<uint16_t>(plen);
        return payload;
    }

    std::string path_;
    uint16_t    portFilter_;
};

} // namespace mde::feed
