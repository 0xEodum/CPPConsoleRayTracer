#include "core/Spectrum.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace crt::spectrum {
namespace {

constexpr int kLutSize = static_cast<int>(kMaxWavelength - kMinWavelength) + 1;  // samples at 1 nm steps
constexpr int kCdfBins = kLutSize - 1;                                               // 1 nm wide bins

float piecewiseGaussian(float x, float mu, float sigmaLow, float sigmaHigh) {
    const float t = (x - mu) / (x < mu ? sigmaLow : sigmaHigh);
    return std::exp(-0.5f * t * t);
}

struct Tables {
    std::array<Rgb, kLutSize> rgb{};      // normalised spectral colour
    std::array<Rgb, kLutSize> weights{};  // clamped, sum-normalised channel weights for reflectance

    Tables() {
        Rgb sum;
        for (int i = 0; i < kLutSize; ++i) {
            rgb[i] = xyzToLinearSrgb(cieXyz(kMinWavelength + static_cast<float>(i)));
            sum += rgb[i];
        }
        const Rgb mean = sum / static_cast<float>(kLutSize);
        for (int i = 0; i < kLutSize; ++i) {
            rgb[i] = {rgb[i].r / mean.r, rgb[i].g / mean.g, rgb[i].b / mean.b};
            const Rgb c{std::max(rgb[i].r, 0.0f), std::max(rgb[i].g, 0.0f), std::max(rgb[i].b, 0.0f)};
            const float total = c.r + c.g + c.b;
            weights[i] = total > 0.0f ? c / total : Rgb(1.0f / 3.0f);
        }
    }
};

const Tables& tables() {
    static const Tables instance;  // thread-safe lazy initialisation (C++11 magic statics)
    return instance;
}

template <class T, std::size_t N>
T sampleLut(const std::array<T, N>& lut, float wavelengthNm) {
    const float x = std::clamp(wavelengthNm - kMinWavelength, 0.0f, static_cast<float>(N - 1));
    const auto i = std::min(static_cast<std::size_t>(x), N - 2);
    const float t = x - static_cast<float>(i);
    return lut[i] * (1.0f - t) + lut[i + 1] * t;
}

Rgb8 normalisedSwatch(Rgb c) {
    c = {std::max(c.r, 0.0f), std::max(c.g, 0.0f), std::max(c.b, 0.0f)};
    const float m = c.maxComponent();
    return m > 0.0f ? toSrgb8(c / m) : Rgb8{};
}

}  // namespace

Vec3 cieXyz(float l) {
    const float x = 1.056f * piecewiseGaussian(l, 599.8f, 37.9f, 31.0f) +
                    0.362f * piecewiseGaussian(l, 442.0f, 16.0f, 26.7f) -
                    0.065f * piecewiseGaussian(l, 501.1f, 20.4f, 26.2f);
    const float y = 0.821f * piecewiseGaussian(l, 568.8f, 46.9f, 40.5f) +
                    0.286f * piecewiseGaussian(l, 530.9f, 16.3f, 31.1f);
    const float z = 1.217f * piecewiseGaussian(l, 437.0f, 11.8f, 36.0f) +
                    0.681f * piecewiseGaussian(l, 459.0f, 26.0f, 13.8f);
    return {x, y, z};
}

Rgb xyzToLinearSrgb(const Vec3& c) {
    return {3.2404542f * c.x - 1.5371385f * c.y - 0.4985314f * c.z,
            -0.9692660f * c.x + 1.8760108f * c.y + 0.0415560f * c.z,
            0.0556434f * c.x - 0.2040259f * c.y + 1.0572252f * c.z};
}

Rgb wavelengthToRgb(float wavelengthNm) { return sampleLut(tables().rgb, wavelengthNm); }

Rgb8 wavelengthSwatch(float wavelengthNm) { return normalisedSwatch(wavelengthToRgb(wavelengthNm)); }

float reflectance(const Rgb& albedo, float wavelengthNm) {
    const Rgb w = sampleLut(tables().weights, wavelengthNm);
    return albedo.r * w.r + albedo.g * w.g + albedo.b * w.b;
}

float blackbody(float wavelengthNm, float temperatureK) {
    constexpr double h = 6.62607015e-34;
    constexpr double c = 2.99792458e8;
    constexpr double k = 1.380649e-23;
    const double l = static_cast<double>(wavelengthNm) * 1e-9;
    const double value = (2.0 * h * c * c) / (std::pow(l, 5.0) * (std::exp(h * c / (l * k * temperatureK)) - 1.0));
    return static_cast<float>(value * 1e-13);  // keep numbers in a comfortable float range
}

EmissionSpectrum::EmissionSpectrum(Kind kind, float parameter) : kind_(kind), parameter_(parameter) {}

EmissionSpectrum EmissionSpectrum::uniform() { return {Kind::Uniform, 0.0f}; }

EmissionSpectrum EmissionSpectrum::monochromatic(float wavelengthNm) {
    return {Kind::Monochromatic, std::clamp(wavelengthNm, kMinWavelength, kMaxWavelength)};
}

EmissionSpectrum EmissionSpectrum::blackbody(float temperatureK) {
    EmissionSpectrum s(Kind::Blackbody, temperatureK);
    s.cdf_.resize(kCdfBins);
    float total = 0.0f;
    for (int i = 0; i < kCdfBins; ++i) {
        total += spectrum::blackbody(kMinWavelength + static_cast<float>(i) + 0.5f, temperatureK);
        s.cdf_[i] = total;
    }
    for (float& v : s.cdf_) v /= total;
    return s;
}

float EmissionSpectrum::sample(float u) const {
    switch (kind_) {
        case Kind::Uniform:
            return lerp(kMinWavelength, kMaxWavelength, u);
        case Kind::Monochromatic:
            return parameter_;
        case Kind::Blackbody: {
            const auto it = std::lower_bound(cdf_.begin(), cdf_.end(), u);
            const auto i = static_cast<std::size_t>(std::min<std::ptrdiff_t>(it - cdf_.begin(), kCdfBins - 1));
            const float lo = i == 0 ? 0.0f : cdf_[i - 1];
            const float width = cdf_[i] - lo;
            const float t = width > 0.0f ? saturate((u - lo) / width) : 0.5f;
            return kMinWavelength + static_cast<float>(i) + t;
        }
    }
    return kMinWavelength;
}

Rgb8 EmissionSpectrum::swatch() const {
    switch (kind_) {
        case Kind::Uniform:
            return {255, 255, 255};
        case Kind::Monochromatic:
            return wavelengthSwatch(parameter_);
        case Kind::Blackbody: {
            Rgb sum;
            for (int i = 0; i < 64; ++i) sum += wavelengthToRgb(sample((static_cast<float>(i) + 0.5f) / 64.0f));
            return normalisedSwatch(sum);
        }
    }
    return {};
}

}  // namespace crt::spectrum
