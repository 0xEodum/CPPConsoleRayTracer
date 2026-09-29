#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace crt {

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTwoPi = 2.0f * kPi;
inline constexpr float kHalfPi = 0.5f * kPi;
inline constexpr float kInvPi = 1.0f / kPi;
inline constexpr float kInfinity = 1e30f;

constexpr float radians(float degrees) { return degrees * (kPi / 180.0f); }
constexpr float degrees(float radians) { return radians * (180.0f / kPi); }

constexpr float saturate(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
constexpr float lerp(float a, float b, float t) { return a + (b - a) * t; }

template <class T>
constexpr T square(T v) { return v * v; }

/// Wraps an angle to the range (-pi, pi].
inline float wrapAngle(float a) {
    a = std::fmod(a + kPi, kTwoPi);
    if (a <= 0.0f) a += kTwoPi;
    return a - kPi;
}

/// Cheap integer hash (Thomas Wang / Murmur finalizer style), useful for seeding RNGs.
constexpr std::uint64_t hash64(std::uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

constexpr std::uint64_t hashCombine(std::uint64_t a, std::uint64_t b) {
    return hash64(a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2)));
}

}  // namespace crt
