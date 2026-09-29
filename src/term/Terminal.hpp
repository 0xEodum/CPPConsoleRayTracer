#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "term/Event.hpp"

namespace crt::term {

struct TerminalSize {
    int columns = 80;
    int rows = 24;
};

/// Platform abstraction over an interactive text terminal.
///
/// Construction switches the terminal into "application" mode (raw input, alternate screen,
/// hidden cursor, mouse reporting); destruction restores everything (RAII). Implementations
/// also restore the terminal if the process is interrupted by a signal / console close event.
class Terminal {
public:
    virtual ~Terminal() = default;

    /// Creates the implementation for the current platform. Throws std::runtime_error when
    /// stdin/stdout are not attached to an interactive terminal.
    static std::unique_ptr<Terminal> create();

    virtual TerminalSize size() const = 0;

    /// Writes raw UTF-8 bytes (text and escape sequences) to the screen.
    virtual void write(std::string_view bytes) = 0;

    /// Waits up to `timeoutMs` for input and appends all available events to `out`.
    /// Returns immediately if events are already pending.
    virtual void poll(std::vector<Event>& out, int timeoutMs) = 0;
};

}  // namespace crt::term
