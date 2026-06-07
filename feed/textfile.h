#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  feed/text_file_source.h  –  Replay UDP payloads from a hex text file
//
//  Each line of the file is one complete UDP payload (i.e. one exchange packet
//  starting at the packet header) encoded as hexadecimal.  Spaces, tabs and
//  colons between bytes are ignored; everything after a '#' is treated as a
//  comment and blank lines are skipped.  Example:
//
//      # SeriesDefBase + AddOrder
//      3c 00 02 00 e9 03 00 00 ...
//
//  The source pulls bytes off disk and invokes onPacket(data, len) per line,
//  matching the signature the adapters expect via processPacket().
// ─────────────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "dispatch.h"

namespace mde::feed {

class TextFileSource {
public:
    explicit TextFileSource(std::string path) : path_(std::move(path)) {}

    // Reader thread parses the file; onPacket runs on the consumer thread.
    template<class OnPacket>
    void run(OnPacket&& onPacket) {
        pumpThreaded(
            [this](auto&& sink) { this->readLoop(std::forward<decltype(sink)>(sink)); },
            std::forward<OnPacket>(onPacket));
    }

private:
    // Blocking hex-decode loop; hands each decoded payload to sink(data, len).
    template<class OnPacket>
    void readLoop(OnPacket&& onPacket) {
        std::ifstream in(path_);
        if (!in) {
            std::cerr << "[text] cannot open " << path_ << "\n";
            return;
        }
        std::string          line;
        std::vector<uint8_t> buf;
        size_t               lineNo = 0, pkts = 0;
        while (std::getline(in, line)) {
            ++lineNo;
            auto hash = line.find('#');
            if (hash != std::string::npos) line.erase(hash);
            buf.clear();
            if (!decodeHex(line, buf)) {
                std::cerr << "[text] line " << lineNo
                          << ": dangling hex nibble, skipped\n";
                continue;
            }
            if (buf.empty()) continue;
            if (buf.size() > 0xFFFF) {
                std::cerr << "[text] line " << lineNo
                          << ": payload > 65535 bytes, skipped\n";
                continue;
            }
            onPacket(buf.data(), static_cast<uint16_t>(buf.size()));
            ++pkts;
        }
        std::cout << "[text] replayed " << pkts << " packet(s) from "
                  << path_ << "\n";
    }

private:
    static int hexVal(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }
    // Collect hex digits, ignoring all other characters; returns false on a
    // trailing odd nibble.
    static bool decodeHex(const std::string& s, std::vector<uint8_t>& out) {
        int hi = -1;
        for (char c : s) {
            int v = hexVal(c);
            if (v < 0) continue;
            if (hi < 0) hi = v;
            else { out.push_back(static_cast<uint8_t>((hi << 4) | v)); hi = -1; }
        }
        return hi < 0;
    }

    std::string path_;
};

} // namespace mde::feed
