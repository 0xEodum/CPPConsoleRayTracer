#pragma once

#include <cstdint>

#include "core/Math.hpp"
#include "core/Vec2.hpp"

namespace crt {

/// PCG32 random number generator (O'Neill 2014): small state, fast, statistically solid.
/// Each worker thread / pixel owns its own instance, so there is no shared state to contend on.
class Pcg32 {
public:
    explicit Pcg32(std::uint64_t seed = 0x853c49e6748fea9bULL, std::uint64_t stream = 0xda3e39cb94b95bdbULL) {
        seedWith(seed, stream);
    }

    void seedWith(std::uint64_t seed, std::uint64_t stream = 0xda3e39cb94b95bdbULL) {
        state_ = 0;
        inc_ = (stream << 1u) | 1u;
        nextU32();
        state_ += seed;
        nextU32();
    }

    std::uint32_t nextU32() {
        const std::uint64_t old = state_;
        state_ = old * 6364136223846793005ULL + inc_;
        const auto xorShifted = static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
        const auto rot = static_cast<std::uint32_t>(old >> 59u);
        return (xorShifted >> rot) | (xorShifted << ((32u - rot) & 31u));
    }

    /// Uniform float in [0, 1).
    float uniform() { return static_cast<float>(nextU32() >> 8) * 0x1p-24f; }
    float uniform(float lo, float hi) { return lo + (hi - lo) * uniform(); }

    /// Uniform integer in [0, bound).
    std::uint32_t below(std::uint32_t bound) {
        return static_cast<std::uint32_t>((static_cast<std::uint64_t>(nextU32()) * bound) >> 32u);
    }

    /// Standard normal sample (Box-Muller).
    float normal() {
        const float u1 = 1.0f - uniform();  // (0, 1]
        const float u2 = uniform();
        return std::sqrt(-2.0f * std::log(u1)) * std::cos(kTwoPi * u2);
    }

private:
    std::uint64_t state_ = 0;
    std::uint64_t inc_ = 0;
};

/// Uniformly distributed point on the unit disk (concentric mapping, Shirley & Chiu).
inline Vec2 sampleUnitDisk(float u, float v) {
    const float a = 2.0f * u - 1.0f;
    const float b = 2.0f * v - 1.0f;
    if (a == 0.0f && b == 0.0f) return {};
    float r;
    float phi;
    if (a * a > b * b) {
        r = a;
        phi = (kPi / 4.0f) * (b / a);
    } else {
        r = b;
        phi = kHalfPi - (kPi / 4.0f) * (a / b);
    }
    return {r * std::cos(phi), r * std::sin(phi)};
}

}  // namespace crt
