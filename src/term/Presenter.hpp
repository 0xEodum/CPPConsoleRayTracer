#pragma once

#include <string>

#include "term/Canvas.hpp"

namespace crt::term {

enum class ColorMode {
    TrueColor,   ///< 24-bit SGR colours (Windows Terminal, modern conhost, most Unix terminals)
    Palette256,  ///< xterm 256-colour palette for older terminals
};

/// Turns successive Canvas frames into minimal terminal output.
///
/// Only cells that differ from what is already on screen are emitted; colours within
/// `tolerance` (per channel) of the displayed ones count as unchanged, which keeps the byte
/// stream small while a progressive render slowly converges. SGR state and the cursor position
/// are tracked so redundant escape codes are never sent.
class Presenter {
public:
    explicit Presenter(ColorMode mode = ColorMode::TrueColor) : mode_(mode) {}

    /// Escape sequences that bring the screen from the previous frame to `frame`.
    std::string render(const Canvas& frame);

    /// Forces the next render() to redraw every cell (after resize or external output).
    void invalidate() { valid_ = false; }

    void setTolerance(int tolerance) { tolerance_ = tolerance; }
    void setColorMode(ColorMode mode) {
        mode_ = mode;
        invalidate();
    }
    ColorMode colorMode() const { return mode_; }

    /// Closest xterm-256 palette index.
    static int paletteIndex(Rgb8 c);

private:
    bool same(const Cell& a, const Cell& b) const;
    void appendColor(std::string& out, Rgb8 fg, Rgb8 bg, bool setFg, bool setBg) const;

    ColorMode mode_;
    int tolerance_ = 0;
    Canvas screen_;  ///< what we believe is currently displayed
    bool valid_ = false;
};

}  // namespace crt::term
