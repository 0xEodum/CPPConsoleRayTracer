#include "term/Canvas.hpp"

#include <algorithm>

#include "term/Utf8.hpp"

namespace crt::term {

void Canvas::resize(int width, int height) {
    width_ = std::max(0, width);
    height_ = std::max(0, height);
    cells_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), Cell{});
}

void Canvas::clear(Rgb8 bg) {
    Cell blank;
    blank.bg = bg;
    std::fill(cells_.begin(), cells_.end(), blank);
}

int Canvas::text(int x, int y, std::string_view utf8, Rgb8 fg, int maxWidth, bool bold) {
    if (y < 0 || y >= height_) return 0;
    int written = 0;
    for (std::size_t i = 0; i < utf8.size() && written < maxWidth;) {
        const char32_t cp = decodeUtf8(utf8, i);
        const int cx = x + written;
        if (cx >= width_) break;
        if (cx >= 0) {
            Cell& c = at(cx, y);
            c.ch = cp;
            c.fg = fg;
            c.bold = bold;
        }
        ++written;
    }
    return written;
}

int Canvas::text(int x, int y, std::string_view utf8, Rgb8 fg, Rgb8 bg, int maxWidth, bool bold) {
    const int n = text(x, y, utf8, fg, maxWidth, bold);
    for (int i = 0; i < n; ++i) {
        if (contains(x + i, y)) at(x + i, y).bg = bg;
    }
    return n;
}

void Canvas::fill(const Rect& r, Rgb8 bg, char32_t ch, Rgb8 fg) {
    const int x0 = std::max(0, r.x), y0 = std::max(0, r.y);
    const int x1 = std::min(width_, r.right()), y1 = std::min(height_, r.bottom());
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            Cell& c = at(x, y);
            c.ch = ch;
            c.fg = fg;
            c.bg = bg;
            c.bold = false;
        }
    }
}

void Canvas::horizontalLine(int x, int y, int width, Rgb8 fg, char32_t ch) {
    for (int i = 0; i < width; ++i) {
        if (!contains(x + i, y)) continue;
        Cell& c = at(x + i, y);
        c.ch = ch;
        c.fg = fg;
    }
}

void Canvas::frame(const Rect& r, Rgb8 fg, Rgb8 bg, std::string_view title, Rgb8 titleColor) {
    if (r.width < 2 || r.height < 2) return;
    fill(r, bg);
    auto put = [&](int x, int y, char32_t ch) {
        if (!contains(x, y)) return;
        Cell& c = at(x, y);
        c.ch = ch;
        c.fg = fg;
    };
    for (int x = r.x + 1; x < r.right() - 1; ++x) {
        put(x, r.y, U'─');
        put(x, r.bottom() - 1, U'─');
    }
    for (int y = r.y + 1; y < r.bottom() - 1; ++y) {
        put(r.x, y, U'│');
        put(r.right() - 1, y, U'│');
    }
    put(r.x, r.y, U'┌');
    put(r.right() - 1, r.y, U'┐');
    put(r.x, r.bottom() - 1, U'└');
    put(r.right() - 1, r.bottom() - 1, U'┘');
    if (!title.empty() && r.width > 6) {
        put(r.x + 1, r.y, U' ');
        const int n = text(r.x + 2, r.y, title, titleColor, r.width - 5, true);
        put(r.x + 2 + n, r.y, U' ');
    }
}

void Canvas::blitImage(const Image8& image, int x0, int y0, ImageStyle style) {
    if (style == ImageStyle::HalfBlock) {
        const int rows = (image.height() + 1) / 2;
        for (int cy = 0; cy < rows; ++cy) {
            for (int cx = 0; cx < image.width(); ++cx) {
                if (!contains(x0 + cx, y0 + cy)) continue;
                const Rgb8 top = image.at(cx, 2 * cy);
                const Rgb8 bottom = 2 * cy + 1 < image.height() ? image.at(cx, 2 * cy + 1) : top;
                Cell& c = at(x0 + cx, y0 + cy);
                c.bold = false;
                c.bg = bottom;
                if (top == bottom) {  // fewer bytes on the wire: a blank with a background suffices
                    c.ch = U' ';
                    c.fg = top;
                } else {
                    c.ch = U'▀';
                    c.fg = top;
                }
            }
        }
        return;
    }

    static constexpr char kRamp[] = " .:-=+*#%@";
    constexpr int kRampSize = sizeof(kRamp) - 1;
    for (int cy = 0; cy < image.height(); ++cy) {
        for (int cx = 0; cx < image.width(); ++cx) {
            if (!contains(x0 + cx, y0 + cy)) continue;
            const Rgb8 p = image.at(cx, cy);
            const int peak = std::max({p.r, p.g, p.b});
            const float luma = (0.2126f * p.r + 0.7152f * p.g + 0.0722f * p.b) / 255.0f;
            const int index = std::clamp(static_cast<int>(luma * (kRampSize - 1) + 0.5f), 0, kRampSize - 1);
            Cell& c = at(x0 + cx, y0 + cy);
            c.ch = static_cast<char32_t>(kRamp[index]);
            c.bg = {0, 0, 0};
            c.bold = false;
            // Glyph density carries brightness; the colour carries hue at full intensity.
            if (peak > 0) {
                const float s = 255.0f / static_cast<float>(peak);
                c.fg = {static_cast<std::uint8_t>(std::min(255.0f, p.r * s)), static_cast<std::uint8_t>(std::min(255.0f, p.g * s)),
                        static_cast<std::uint8_t>(std::min(255.0f, p.b * s))};
            } else {
                c.fg = {0, 0, 0};
            }
        }
    }
}

}  // namespace crt::term
