#pragma once

#include "core/Math.hpp"

namespace crt {

/// 2D vector used by the optics simulation. Plain aggregate: cheap to copy, constexpr-friendly.
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

    constexpr Vec2 operator-() const { return {-x, -y}; }
    constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vec2 operator/(float s) const { return {x / s, y / s}; }
    constexpr Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    constexpr Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
    constexpr Vec2& operator*=(float s) { x *= s; y *= s; return *this; }

    constexpr bool operator==(Vec2 o) const { return x == o.x && y == o.y; }
    constexpr bool operator!=(Vec2 o) const { return !(*this == o); }
};

constexpr Vec2 operator*(float s, Vec2 v) { return v * s; }

constexpr float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
/// z-component of the 3D cross product; positive when b is counter-clockwise from a.
constexpr float cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
constexpr float lengthSquared(Vec2 v) { return dot(v, v); }
inline float length(Vec2 v) { return std::sqrt(lengthSquared(v)); }
inline float distance(Vec2 a, Vec2 b) { return length(a - b); }

inline Vec2 normalize(Vec2 v) {
    const float len = length(v);
    return len > 0.0f ? v / len : Vec2{};
}

/// Rotates by +90 degrees.
constexpr Vec2 perpendicular(Vec2 v) { return {-v.y, v.x}; }

inline Vec2 rotate(Vec2 v, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

inline Vec2 fromAngle(float angle) { return {std::cos(angle), std::sin(angle)}; }
inline float angleOf(Vec2 v) { return std::atan2(v.y, v.x); }

/// Mirror reflection of direction d about a unit normal n.
constexpr Vec2 reflect(Vec2 d, Vec2 n) { return d - n * (2.0f * dot(d, n)); }

constexpr Vec2 min(Vec2 a, Vec2 b) { return {a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y}; }
constexpr Vec2 max(Vec2 a, Vec2 b) { return {a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y}; }

/// Axis-aligned bounding box in 2D.
struct Box2 {
    Vec2 lo{kInfinity, kInfinity};
    Vec2 hi{-kInfinity, -kInfinity};

    constexpr void expand(Vec2 p) { lo = min(lo, p); hi = max(hi, p); }
    constexpr bool contains(Vec2 p) const {
        return p.x >= lo.x && p.x <= hi.x && p.y >= lo.y && p.y <= hi.y;
    }
    constexpr Box2 inflated(float r) const { return {{lo.x - r, lo.y - r}, {hi.x + r, hi.y + r}}; }
    constexpr Vec2 size() const { return hi - lo; }
    constexpr Vec2 center() const { return (lo + hi) * 0.5f; }
};

}  // namespace crt
