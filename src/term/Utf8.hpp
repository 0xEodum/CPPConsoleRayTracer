#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace crt::term {

inline void appendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xc0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3f));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xe0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (cp & 0x3f));
    } else {
        out += static_cast<char>(0xf0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (cp & 0x3f));
    }
}

/// Number of bytes of the UTF-8 sequence starting with `lead` (1 for invalid lead bytes).
inline std::size_t utf8SequenceLength(unsigned char lead) {
    if (lead < 0x80) return 1;
    if ((lead >> 5) == 0x6) return 2;
    if ((lead >> 4) == 0xe) return 3;
    if ((lead >> 3) == 0x1e) return 4;
    return 1;
}

/// Decodes the code point at `pos` and advances `pos`. Invalid input yields U+FFFD.
inline char32_t decodeUtf8(std::string_view s, std::size_t& pos) {
    const auto lead = static_cast<unsigned char>(s[pos]);
    const std::size_t len = utf8SequenceLength(lead);
    if (len == 1 || pos + len > s.size()) {
        ++pos;
        return lead < 0x80 ? lead : 0xfffd;
    }
    char32_t cp = lead & (0x7f >> len);
    for (std::size_t i = 1; i < len; ++i) {
        const auto c = static_cast<unsigned char>(s[pos + i]);
        if ((c & 0xc0) != 0x80) {
            ++pos;
            return 0xfffd;
        }
        cp = (cp << 6) | (c & 0x3f);
    }
    pos += len;
    return cp;
}

inline std::size_t utf8Length(std::string_view s) {
    std::size_t n = 0;
    for (std::size_t i = 0; i < s.size();) {
        decodeUtf8(s, i);
        ++n;
    }
    return n;
}

}  // namespace crt::term
