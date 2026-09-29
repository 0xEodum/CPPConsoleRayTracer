#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "term/Canvas.hpp"

namespace crt::ui {

/// Colour palette shared by all screens.
namespace theme {
inline constexpr Rgb8 kBackground{12, 12, 16};
inline constexpr Rgb8 kPanel{20, 21, 28};
inline constexpr Rgb8 kBorder{70, 74, 96};
inline constexpr Rgb8 kText{214, 216, 224};
inline constexpr Rgb8 kDim{128, 132, 150};
inline constexpr Rgb8 kAccent{255, 205, 60};
inline constexpr Rgb8 kTitle{120, 200, 255};
inline constexpr Rgb8 kGood{120, 220, 140};
inline constexpr Rgb8 kWarn{255, 120, 100};
inline constexpr Rgb8 kStatusBar{34, 36, 48};
inline constexpr Rgb8 kHighlight{52, 56, 76};
}  // namespace theme

struct HelpEntry {
    std::string keys;
    std::string description;
};

struct HelpSection {
    std::string title;
    std::vector<HelpEntry> entries;
};

/// Sequential writer for a side panel: keeps track of the current line and clips to the area.
class PanelWriter {
public:
    PanelWriter(term::Canvas& canvas, const term::Rect& area) : canvas_(canvas), area_(area), y_(area.y) {}

    bool full() const { return y_ >= area_.bottom(); }
    int remaining() const { return area_.bottom() - y_; }
    int width() const { return area_.width; }

    void heading(std::string_view title);
    void text(std::string_view line, Rgb8 color = theme::kText, bool bold = false);
    void keyValue(std::string_view key, std::string_view value, Rgb8 valueColor = theme::kText);
    /// A list item with an optional colour swatch; `selected` items are highlighted.
    void item(std::string_view label, bool selected, Rgb8 swatch, bool hasSwatch, std::string_view prefix = {});
    void wrapped(std::string_view paragraph, Rgb8 color = theme::kDim);
    void skip(int lines = 1) { y_ += lines; }

private:
    term::Canvas& canvas_;
    term::Rect area_;
    int y_;
};

void drawStatusBar(term::Canvas& canvas, int row, std::string_view left, std::string_view right);
void drawToast(term::Canvas& canvas, int row, std::string_view message);
void drawHelpOverlay(term::Canvas& canvas, std::string_view title, const std::vector<HelpSection>& sections);
/// Centered message box (e.g. "terminal too small").
void drawMessageBox(term::Canvas& canvas, std::string_view title, const std::vector<std::string>& lines);

std::vector<std::string> wrapText(std::string_view text, int width);
std::string formatFixed(double value, int decimals);
/// 1234567 -> "1.23M"
std::string formatCount(double value);

}  // namespace crt::ui
