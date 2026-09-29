#include "term/Presenter.hpp"

#include <charconv>
#include <cstdlib>

#include "term/Utf8.hpp"

namespace crt::term {
namespace {

void appendInt(std::string& out, int v) {
    char buf[12];
    const auto res = std::to_chars(buf, buf + sizeof(buf), v);
    out.append(buf, res.ptr);
}

void appendRgb(std::string& out, Rgb8 c) {
    appendInt(out, c.r);
    out += ';';
    appendInt(out, c.g);
    out += ';';
    appendInt(out, c.b);
}

bool close(Rgb8 a, Rgb8 b, int tolerance) {
    return std::abs(a.r - b.r) <= tolerance && std::abs(a.g - b.g) <= tolerance && std::abs(a.b - b.b) <= tolerance;
}

int cubeIndex(int v) { return v < 48 ? 0 : (v < 115 ? 1 : (v - 35) / 40); }

}  // namespace

int Presenter::paletteIndex(Rgb8 c) {
    static constexpr int levels[6] = {0, 95, 135, 175, 215, 255};
    const int r = cubeIndex(c.r), g = cubeIndex(c.g), b = cubeIndex(c.b);
    const int cubeDist = square(levels[r] - c.r) + square(levels[g] - c.g) + square(levels[b] - c.b);
    const int average = (c.r + c.g + c.b) / 3;
    const int grayStep = average > 238 ? 23 : (average < 8 ? 0 : (average - 8) / 10);
    const int gray = 8 + grayStep * 10;
    const int grayDist = square(gray - c.r) + square(gray - c.g) + square(gray - c.b);
    return grayDist < cubeDist ? 232 + grayStep : 16 + 36 * r + 6 * g + b;
}

bool Presenter::same(const Cell& a, const Cell& b) const {
    if (a.ch != b.ch || a.bold != b.bold || !close(a.bg, b.bg, tolerance_)) return false;
    return a.ch == U' ' || close(a.fg, b.fg, tolerance_);  // a blank's foreground is invisible
}

void Presenter::appendColor(std::string& out, Rgb8 fg, Rgb8 bg, bool setFg, bool setBg) const {
    if (!setFg && !setBg) return;
    out += "\x1b[";
    if (setFg) {
        if (mode_ == ColorMode::TrueColor) {
            out += "38;2;";
            appendRgb(out, fg);
        } else {
            out += "38;5;";
            appendInt(out, paletteIndex(fg));
        }
    }
    if (setBg) {
        if (setFg) out += ';';
        if (mode_ == ColorMode::TrueColor) {
            out += "48;2;";
            appendRgb(out, bg);
        } else {
            out += "48;5;";
            appendInt(out, paletteIndex(bg));
        }
    }
    out += 'm';
}

std::string Presenter::render(const Canvas& frame) {
    const bool full = !valid_ || screen_.width() != frame.width() || screen_.height() != frame.height();
    std::string out;
    if (full) {
        screen_ = frame;
        out.reserve(static_cast<std::size_t>(frame.width()) * static_cast<std::size_t>(frame.height()) * 24);
        out += "\x1b[0m\x1b[2J";
    }

    int cursorX = -1, cursorY = -1;
    bool haveColors = false;
    bool bold = false;
    Rgb8 fg, bg;
    bool started = false;

    for (int y = 0; y < frame.height(); ++y) {
        for (int x = 0; x < frame.width(); ++x) {
            const Cell& c = frame.at(x, y);
            Cell& shown = screen_.at(x, y);
            if (!full && same(c, shown)) continue;

            if (!started) {
                out += "\x1b[?2026h\x1b[0m";  // begin synchronised update (ignored where unsupported)
                started = true;
            }
            if (cursorX != x || cursorY != y) {
                out += "\x1b[";
                appendInt(out, y + 1);
                out += ';';
                appendInt(out, x + 1);
                out += 'H';
            }
            if (c.bold != bold) {
                out += c.bold ? "\x1b[1m" : "\x1b[22m";
                bold = c.bold;
            }
            const bool needFg = c.ch != U' ';
            const bool setFg = needFg && (!haveColors || fg != c.fg);
            const bool setBg = !haveColors || bg != c.bg;
            if (!haveColors) {
                // First cell of the frame: establish both colours so later comparisons are valid.
                appendColor(out, c.fg, c.bg, true, true);
                haveColors = true;
                fg = c.fg;
                bg = c.bg;
            } else {
                appendColor(out, c.fg, c.bg, setFg, setBg);
                if (setFg) fg = c.fg;
                if (setBg) bg = c.bg;
            }
            appendUtf8(out, c.ch);
            cursorX = x + 1;
            cursorY = y;
            shown = c;
        }
    }
    if (started) out += "\x1b[0m\x1b[?2026l";
    valid_ = true;
    return out;
}

}  // namespace crt::term
