#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  adapter/tse/tse_market_data.h  –  TSE-specific reference-data caches + wire
//
//  Wire format (Arrowhead FLEX MBO): big-endian binary.  26-byte packet header
//  followed by tagged messages.  Each tag: 1-byte length prefix + tag data
//  (first byte = ASCII tag type char).  The packed wire structs below document
//  the layout; the adapter reads fields via explicit big-endian helpers.
// ─────────────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <string>

namespace mde {

#pragma pack(push, 1)

struct TsePktHeader {       // 26 bytes
    uint8_t  multicastGroupNum;
    uint8_t  numReboots;
    uint32_t seqNumber;
    char     issueCode[12]; // ASCII, left-aligned for MBO
    uint32_t updateNumber;
    uint8_t  packetNumber;
    uint8_t  totalPackets;
    uint8_t  utilityFlag;   // 1 = more packets for same transaction
    uint8_t  messageCount;
};
static_assert(sizeof(TsePktHeader) == 26, "TsePktHeader must be 26 bytes");

#pragma pack(pop)

// ─────────────────────────────────────────────────────────────────────────────
//  TSE Issue cache (populated from II/BP tags in the full FLEX spec; for now a
//  minimal entry is synthesised from the packet-header IssueCode).
// ─────────────────────────────────────────────────────────────────────────────
struct TseIssueCache {
    std::string issueCode;        // 12-char left-aligned from packet header
    std::string name;             // from II tag when available
    uint8_t     priceDecimals{4}; // TSE Bn(Price) always has 4 implicit decimals
};

} // namespace mde
