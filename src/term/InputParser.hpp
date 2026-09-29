#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "term/Event.hpp"

namespace crt::term {

/// Incremental decoder for VT/xterm input byte streams: keys, CSI/SS3 escape sequences,
/// UTF-8 text and SGR (1006) mouse reports.
///
/// A lone ESC byte is ambiguous (Escape key vs. the start of a sequence that has not fully
/// arrived yet); it stays pending until parse() is called with flushEscape = true, which the
/// terminal does after a short quiet period.
class InputParser {
public:
    void feed(std::string_view bytes) { buffer_.append(bytes.data(), bytes.size()); }

    /// Decodes as many complete events as possible and appends them to `out`.
    void parse(std::vector<Event>& out, bool flushEscape = false);

    bool hasPending() const { return !buffer_.empty(); }

private:
    enum class Result { Emitted, Incomplete, Skip };

    Result parseOne(std::size_t& consumed, Event& event, bool flushEscape) const;
    Result parseCsi(std::size_t& consumed, Event& event) const;
    Result parseSs3(std::size_t& consumed, Event& event) const;
    Result parsePlain(std::size_t pos, std::size_t& consumed, Event& event) const;

    std::string buffer_;
};

}  // namespace crt::term
