#include "ui/Widgets.hpp"

#include <algorithm>
#include <cstdio>

#include "term/Utf8.hpp"

namespace crt::ui {

using term::Canvas;
using term::Rect;

namespace {
int textWidth(std::string_view s) { return static_cast<int>(term::utf8Length(s)); }
}  // namespace

// ---------------------------------------------------------------------------------------------
// PanelWriter
// ---------------------------------------------------------------------------------------------

void PanelWriter::heading(std::string_view title) {
    if (full()) return;
    canvas_.horizontalLine(area_.x, y_, area_.width, theme::kBorder);
    canvas_.text(area_.x + 1, y_, " ", theme::kTitle);
    const int n = canvas_.text(area_.x + 2, y_, title, theme::kTitle, area_.width - 4, true);
    canvas_.text(area_.x + 2 + n, y_, " ", theme::kTitle);
    ++y_;
}

void PanelWriter::text(std::string_view line, Rgb8 color, bool bold) {
    if (full()) return;
    canvas_.text(area_.x + 1, y_, line, color, area_.width - 2, bold);
    ++y_;
}

void PanelWriter::keyValue(std::string_view key, std::string_view value, Rgb8 valueColor) {
    if (full()) return;
    const int n = canvas_.text(area_.x + 1, y_, key, theme::kDim, area_.width - 2);
    canvas_.text(area_.x + 1 + n + 1, y_, value, valueColor, area_.width - 3 - n);
    ++y_;
}

void PanelWriter::item(std::string_view label, bool selected, Rgb8 swatch, bool hasSwatch, std::string_view prefix) {
    if (full()) return;
    if (selected) canvas_.fill({area_.x, y_, area_.width, 1}, theme::kHighlight);
    int x = area_.x + 1;
    canvas_.text(x, y_, selected ? "▶" : " ", theme::kAccent);
    x += 2;
    if (!prefix.empty()) x += canvas_.text(x, y_, prefix, theme::kDim) + 1;
    if (hasSwatch) {
        canvas_.text(x, y_, "■", swatch);
        x += 2;
    }
    canvas_.text(x, y_, label, selected ? theme::kAccent : theme::kText, area_.right() - x - 1, selected);
    ++y_;
}

void PanelWriter::wrapped(std::string_view paragraph, Rgb8 color) {
    for (const auto& line : wrapText(paragraph, area_.width - 2)) text(line, color);
}

// ---------------------------------------------------------------------------------------------
// Free functions
// ---------------------------------------------------------------------------------------------

void drawStatusBar(Canvas& canvas, int row, std::string_view left, std::string_view right) {
    if (row < 0 || row >= canvas.height()) return;
    canvas.fill({0, row, canvas.width(), 1}, theme::kStatusBar);
    const int rightWidth = textWidth(right);
    const int rightX = std::max(0, canvas.width() - rightWidth - 1);
    canvas.text(1, row, left, theme::kText, std::max(0, rightX - 2));
    canvas.text(rightX, row, right, theme::kDim);
}

void drawToast(Canvas& canvas, int row, std::string_view message) {
    const int w = std::min(canvas.width() - 2, textWidth(message) + 4);
    if (w <= 4 || row < 0 || row >= canvas.height()) return;
    const int x = (canvas.width() - w) / 2;
    canvas.fill({x, row, w, 1}, theme::kAccent);
    canvas.text(x + 2, row, message, {20, 20, 20}, w - 4, true);
}

void drawMessageBox(Canvas& canvas, std::string_view title, const std::vector<std::string>& lines) {
    int w = textWidth(title) + 6;
    for (const auto& l : lines) w = std::max(w, textWidth(l) + 4);
    w = std::min(w, canvas.width());
    const int h = std::min(static_cast<int>(lines.size()) + 2, canvas.height());
    const Rect r{(canvas.width() - w) / 2, (canvas.height() - h) / 2, w, h};
    canvas.frame(r, theme::kBorder, theme::kPanel, title, theme::kTitle);
    for (std::size_t i = 0; i < lines.size() && static_cast<int>(i) < h - 2; ++i) {
        canvas.text(r.x + 2, r.y + 1 + static_cast<int>(i), lines[i], theme::kText, w - 4);
    }
}

void drawHelpOverlay(Canvas& canvas, std::string_view title, const std::vector<HelpSection>& sections) {
    int keyWidth = 0;
    int lineCount = 0;
    for (const auto& s : sections) {
        lineCount += 1 + static_cast<int>(s.entries.size()) + 1;
        for (const auto& e : s.entries) keyWidth = std::max(keyWidth, textWidth(e.keys));
    }
    const int w = std::min(canvas.width() - 2, 78);
    const int h = std::min(canvas.height() - 2, lineCount + 3);
    if (w < 20 || h < 5) return;
    const Rect r{(canvas.width() - w) / 2, (canvas.height() - h) / 2, w, h};
    canvas.frame(r, theme::kAccent, theme::kPanel, title, theme::kAccent);

    int y = r.y + 1;
    const int bottom = r.bottom() - 2;
    for (const auto& s : sections) {
        if (y >= bottom) break;
        canvas.text(r.x + 2, y++, s.title, theme::kTitle, w - 4, true);
        for (const auto& e : s.entries) {
            if (y >= bottom) break;
            canvas.text(r.x + 3, y, e.keys, theme::kAccent, keyWidth);
            canvas.text(r.x + 5 + keyWidth, y, e.description, theme::kText, w - keyWidth - 7);
            ++y;
        }
        ++y;
    }
    canvas.text(r.x + 2, r.bottom() - 2, "F1 / h / Esc - close help", theme::kDim, w - 4);
}

std::vector<std::string> wrapText(std::string_view text, int width) {
    std::vector<std::string> lines;
    if (width <= 0) return lines;
    std::string current;
    int currentWidth = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && text[i] == ' ') ++i;
        std::size_t j = i;
        while (j < text.size() && text[j] != ' ') ++j;
        if (j == i) break;
        const std::string_view word = text.substr(i, j - i);
        const int ww = textWidth(word);
        if (currentWidth > 0 && currentWidth + 1 + ww > width) {
            lines.push_back(current);
            current.clear();
            currentWidth = 0;
        }
        if (currentWidth > 0) {
            current += ' ';
            ++currentWidth;
        }
        current.append(word.data(), word.size());
        currentWidth += ww;
        i = j;
    }
    if (!current.empty()) lines.push_back(current);
    return lines;
}

std::string formatFixed(double value, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, value);
    return buf;
}

std::string formatCount(double value) {
    if (value >= 1e9) return formatFixed(value / 1e9, 2) + "G";
    if (value >= 1e6) return formatFixed(value / 1e6, 2) + "M";
    if (value >= 1e3) return formatFixed(value / 1e3, 1) + "k";
    return formatFixed(value, 0);
}

}  // namespace crt::ui
