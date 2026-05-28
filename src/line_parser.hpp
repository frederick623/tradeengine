#pragma once

#include <cstdlib>
#include <string>
#include <string_view>

#include "constants.hpp"

namespace tradeengine {

// Pipe-delimited tokenizer over a contiguous buffer.
class LineParser {
public:
    LineParser() = default;
    LineParser(std::string_view msg, char delim = '|')
        : m_msg(msg), m_pos(0), m_delim(delim) {}

    void reset(std::string_view msg, char delim = '|') {
        m_msg = msg; m_pos = 0; m_delim = delim;
    }

    bool eof() const { return m_pos >= m_msg.size(); }

    // Returns next token as a view into the underlying buffer (no allocation).
    // Returns false if no more tokens.
    bool next(std::string_view& out) {
        if (m_pos > m_msg.size()) return false;
        if (m_pos == m_msg.size()) { m_pos = m_msg.size() + 1; out = {}; return true; }
        const auto p = m_msg.find(m_delim, m_pos);
        if (p == std::string_view::npos) {
            out = m_msg.substr(m_pos);
            m_pos = m_msg.size() + 1;
        } else {
            out = m_msg.substr(m_pos, p - m_pos);
            m_pos = p + 1;
        }
        return true;
    }

    // Convenience: parse two-token (attr-id, value) at once.
    bool next_pair(int& attr, std::string_view& value) {
        std::string_view a;
        if (!next(a) || a.empty()) return false;
        if (!next(value)) return false;
        attr = std::atoi(std::string(a).c_str());
        return true;
    }

private:
    std::string_view m_msg;
    std::size_t      m_pos{0};
    char             m_delim{'|'};
};

inline double sv_to_double(std::string_view s) {
    if (s.empty()) return kDoubleNull;
    return std::strtod(std::string(s).c_str(), nullptr);
}

inline int32_t sv_to_int(std::string_view s) {
    if (s.empty()) return kInt32Null;
    return static_cast<int32_t>(std::strtol(std::string(s).c_str(), nullptr, 10));
}

inline int64_t sv_to_int64(std::string_view s) {
    if (s.empty()) return kInt64Null;
    return std::strtoll(std::string(s).c_str(), nullptr, 10);
}

inline char sv_to_char(std::string_view s) {
    return s.empty() ? char{0} : s.front();
}

}
