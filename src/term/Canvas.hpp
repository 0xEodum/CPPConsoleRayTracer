#pragma once

#include <string_view>
#include <vector>

#include "core/Image.hpp"

namespace crt::term {

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool contains(int px, int py) const { return px >= x && py >= y && px < x + width && py < y + height; }
    int right() const { return x + width; }
    int bottom() const { return y + height; }
};

/// One character cell of the terminal.
struct Cell {
    char32_t ch = U' ';
    Rgb8 fg{200, 200, 200};
    Rgb8 bg{0, 0, 0};
    bool bold = false;

    bool operator==(const Cell& o) const { return ch == o.ch && fg == o.fg && bg == o.bg && bold == o.bold; }
    bool operator!=(const Cell& o) const { return !(*this == o); }
};

/// How images are mapped onto character cells.
enum class ImageStyle {
    HalfBlock,  ///< '▀' with fg = upper pixel, bg = lower pixel: 2 square-ish pixels per cell
    Ascii,      ///< density ramp " .:-=+*#%@" coloured by the pixel: 1 pixel per cell (retro look)
};

/// Pixel aspect ratio (height / width) that an image rendered for the given style must use.
inline float pixelAspectFor(ImageStyle style) { return style == ImageStyle::HalfBlock ? 1.0f : 2.0f; }
/// Number of image rows that fit into `cellRows` terminal rows.
inline int imageRowsFor(ImageStyle style, int cellRows) { return style == ImageStyle::HalfBlock ? cellRows * 2 : cellRows; }

/// An off-screen grid of cells that UI code draws into; the Presenter turns it into terminal output.
class Canvas {
public:
    Canvas() = default;
    Canvas(int width, int height) { resize(width, height); }

    void resize(int width, int height);
    void clear(Rgb8 bg = {0, 0, 0});

    int width() const { return width_; }
    int height() const { return height_; }
    bool contains(int x, int y) const { return x >= 0 && y >= 0 && x < width_ && y < height_; }

    Cell& at(int x, int y) { return cells_[static_cast<std::size_t>(y * width_ + x)]; }
    const Cell& at(int x, int y) const { return cells_[static_cast<std::size_t>(y * width_ + x)]; }
    void set(int x, int y, const Cell& cell) {
        if (contains(x, y)) at(x, y) = cell;
    }

    /// Writes UTF-8 text; keeps the existing background. Returns the number of cells written.
    int text(int x, int y, std::string_view utf8, Rgb8 fg, int maxWidth = 1 << 30, bool bold = false);
    /// Same, but also sets the background colour.
    int text(int x, int y, std::string_view utf8, Rgb8 fg, Rgb8 bg, int maxWidth = 1 << 30, bool bold = false);

    void fill(const Rect& r, Rgb8 bg, char32_t ch = U' ', Rgb8 fg = {200, 200, 200});
    /// Box-drawing frame with an optional title in the top border.
    void frame(const Rect& r, Rgb8 fg, Rgb8 bg, std::string_view title = {}, Rgb8 titleColor = {255, 255, 255});
    void horizontalLine(int x, int y, int width, Rgb8 fg, char32_t ch = U'─');

    void blitImage(const Image8& image, int x, int y, ImageStyle style);

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<Cell> cells_;
};

}  // namespace crt::term
