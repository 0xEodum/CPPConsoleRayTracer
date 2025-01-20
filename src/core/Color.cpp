#pragma once
#include <windows.h>
#include <string>
#include <cmath>

class Color {
private:
    BYTE r, g, b;

public:
    Color(BYTE r = 0, BYTE g = 0, BYTE b = 0) : r(r), g(g), b(b) {}

    static Color fromNormalized(float r, float g, float b) {
        return Color(
            static_cast<BYTE>(r * 255),
            static_cast<BYTE>(g * 255),
            static_cast<BYTE>(b * 255)
        );
    }

    float getNormalizedR() const { return r / 255.0f; }
    float getNormalizedG() const { return g / 255.0f; }
    float getNormalizedB() const { return b / 255.0f; }

    Color operator*(float intensity) const {
        return Color(
            static_cast<BYTE>(std::min<float>(255.0f, r * intensity)),
            static_cast<BYTE>(std::min<float>(255.0f, g * intensity)),
            static_cast<BYTE>(std::min<float>(255.0f, b * intensity))
        );
    }

    Color operator+(const Color& other) const {
        return Color(
            static_cast<BYTE>(std::min<float>(255, int(r) + int(other.r))),
            static_cast<BYTE>(std::min<float>(255, int(g) + int(other.g))),
            static_cast<BYTE>(std::min<float>(255, int(b) + int(other.b)))
        );
    }

    std::wstring getColorCode(bool isBackground = false) const {
        wchar_t buffer[32];
        swprintf(buffer, sizeof(buffer) / sizeof(wchar_t),
            L"\x1b[%d;2;%d;%d;%dm",
            isBackground ? 48 : 38, r, g, b);
        return std::wstring(buffer);
    }

    static Color White() { return Color(255, 255, 255); }
    static Color Black() { return Color(0, 0, 0); }
    static Color Red() { return Color(255, 0, 0); }
    static Color Green() { return Color(0, 255, 0); }
    static Color Blue() { return Color(0, 0, 255); }

    Color darker(float factor = 0.7f) const {
        return *this * factor;
    }

    BYTE getR() const { return r; }
    BYTE getG() const { return g; }
    BYTE getB() const { return b; }
};