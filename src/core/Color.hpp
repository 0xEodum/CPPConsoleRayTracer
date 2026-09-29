#pragma once

#include <cstdint>

#include "core/Math.hpp"

namespace crt {

/// Linear-light RGB color with float precision (HDR, may exceed 1 and, transiently, go negative).
struct Rgb {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;

    constexpr Rgb() = default;
    constexpr Rgb(float r_, float g_, float b_) : r(r_), g(g_), b(b_) {}
    constexpr explicit Rgb(float v) : r(v), g(v), b(v) {}

    constexpr Rgb operator+(const Rgb& o) const { return {r + o.r, g + o.g, b + o.b}; }
    constexpr Rgb operator-(const Rgb& o) const { return {r - o.r, g - o.g, b - o.b}; }
    constexpr Rgb operator*(const Rgb& o) const { return {r * o.r, g * o.g, b * o.b}; }
    constexpr Rgb operator*(float s) const { return {r * s, g * s, b * s}; }
    constexpr Rgb operator/(float s) const { return {r / s, g / s, b / s}; }
    constexpr Rgb& operator+=(const Rgb& o) { r += o.r; g += o.g; b += o.b; return *this; }
    constexpr Rgb& operator*=(const Rgb& o) { r *= o.r; g *= o.g; b *= o.b; return *this; }
    constexpr Rgb& operator*=(float s) { r *= s; g *= s; b *= s; return *this; }

    constexpr float maxComponent() const { return r > g ? (r > b ? r : b) : (g > b ? g : b); }
    constexpr float average() const { return (r + g + b) * (1.0f / 3.0f); }
    /// Rec.709 relative luminance.
    constexpr float luminance() const { return 0.2126f * r + 0.7152f * g + 0.0722f * b; }
    constexpr bool isBlack() const { return r <= 0.0f && g <= 0.0f && b <= 0.0f; }
};

constexpr Rgb operator*(float s, const Rgb& c) { return c * s; }

/// Display-ready 8-bit sRGB color.
struct Rgb8 {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    constexpr bool operator==(const Rgb8& o) const { return r == o.r && g == o.g && b == o.b; }
    constexpr bool operator!=(const Rgb8& o) const { return !(*this == o); }
};

inline float srgbEncode(float linear) {
    linear = saturate(linear);
    return linear <= 0.0031308f ? 12.92f * linear : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
}

inline float srgbDecode(float encoded) {
    encoded = saturate(encoded);
    return encoded <= 0.04045f ? encoded / 12.92f : std::pow((encoded + 0.055f) / 1.055f, 2.4f);
}

inline std::uint8_t toByte(float v01) { return static_cast<std::uint8_t>(saturate(v01) * 255.0f + 0.5f); }

/// Converts linear RGB in [0,1] to 8-bit sRGB (with the proper transfer curve).
inline Rgb8 toSrgb8(const Rgb& c) {
    return {toByte(srgbEncode(c.r)), toByte(srgbEncode(c.g)), toByte(srgbEncode(c.b))};
}

/// Converts 8-bit sRGB back to linear RGB.
inline Rgb fromSrgb8(Rgb8 c) {
    return {srgbDecode(c.r / 255.0f), srgbDecode(c.g / 255.0f), srgbDecode(c.b / 255.0f)};
}

/// Linear blend in display space; used for UI overlays where exact photometry does not matter.
inline Rgb8 mix(Rgb8 a, Rgb8 b, float t) {
    t = saturate(t);
    auto ch = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(x + (y - x) * t + 0.5f);
    };
    return {ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b)};
}

}  // namespace crt
