#pragma once

#include "core/Math.hpp"

namespace crt {

/// 3D vector for the path tracer.
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    constexpr float operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }

    constexpr Vec3 operator-() const { return {-x, -y, -z}; }
    constexpr Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator*(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
    constexpr Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    constexpr Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    constexpr Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    constexpr Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
};

constexpr Vec3 operator*(float s, const Vec3& v) { return v * s; }

constexpr float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
constexpr float lengthSquared(const Vec3& v) { return dot(v, v); }
inline float length(const Vec3& v) { return std::sqrt(lengthSquared(v)); }

inline Vec3 normalize(const Vec3& v) {
    const float len = length(v);
    return len > 0.0f ? v / len : Vec3{};
}

constexpr Vec3 reflect(const Vec3& d, const Vec3& n) { return d - n * (2.0f * dot(d, n)); }

constexpr Vec3 min(const Vec3& a, const Vec3& b) {
    return {a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.z < b.z ? a.z : b.z};
}
constexpr Vec3 max(const Vec3& a, const Vec3& b) {
    return {a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y, a.z > b.z ? a.z : b.z};
}

/// Builds an orthonormal basis (t, b) around unit vector n (Duff et al. 2017, branchless).
inline void orthonormalBasis(const Vec3& n, Vec3& t, Vec3& b) {
    const float sign = std::copysign(1.0f, n.z);
    const float a = -1.0f / (sign + n.z);
    const float c = n.x * n.y * a;
    t = {1.0f + sign * n.x * n.x * a, sign * c, -sign * n.x};
    b = {c, sign + n.y * n.y * a, -n.y};
}

}  // namespace crt
