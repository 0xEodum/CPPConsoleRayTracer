#pragma once

#include <vector>

#include "core/Color.hpp"
#include "core/Vec3.hpp"

namespace crt::spectrum {

inline constexpr float kMinWavelength = 380.0f;  // nm
inline constexpr float kMaxWavelength = 780.0f;  // nm

/// CIE 1931 2-degree colour matching functions, multi-lobe analytic fit
/// (Wyman, Sloan & Shirley, "Simple Analytic Approximations to the CIE XYZ CMFs", JCGT 2013).
Vec3 cieXyz(float wavelengthNm);

Rgb xyzToLinearSrgb(const Vec3& xyz);

/// Linear sRGB contribution of a single wavelength. Normalised so that a ray whose wavelength is
/// drawn uniformly from [kMinWavelength, kMaxWavelength] has an expected colour of exactly (1,1,1):
/// an equal-energy spectrum therefore renders as neutral white. Components can be negative for
/// saturated spectral colours outside the sRGB gamut; accumulate first, clamp at display time.
Rgb wavelengthToRgb(float wavelengthNm);

/// Clamped, max-normalised colour of a wavelength for UI swatches.
Rgb8 wavelengthSwatch(float wavelengthNm);

/// Spectral reflectance of a surface described by an RGB albedo (simple, energy-bounded upsampling).
float reflectance(const Rgb& albedo, float wavelengthNm);

/// Cauchy's empirical dispersion law n(lambda) = A + B / lambda^2 with lambda in micrometres.
inline float cauchyIor(float a, float bMicron2, float wavelengthNm) {
    const float um = wavelengthNm * 1e-3f;
    return a + bMicron2 / (um * um);
}

/// Planck's law (arbitrary scale).
float blackbody(float wavelengthNm, float temperatureK);

/// Emission spectrum of a light source that can be importance-sampled one wavelength at a time.
class EmissionSpectrum {
public:
    enum class Kind { Uniform, Monochromatic, Blackbody };

    static EmissionSpectrum uniform();
    static EmissionSpectrum monochromatic(float wavelengthNm);
    static EmissionSpectrum blackbody(float temperatureK);

    Kind kind() const { return kind_; }
    /// Wavelength for monochromatic spectra, temperature for blackbody ones.
    float parameter() const { return parameter_; }

    /// Draws a wavelength distributed proportionally to the emitted power.
    float sample(float u) const;

    /// Average colour of the emission (for UI and for quick previews).
    Rgb8 swatch() const;

private:
    EmissionSpectrum(Kind kind, float parameter);

    Kind kind_;
    float parameter_;
    std::vector<float> cdf_;  // only used by tabulated spectra
};

}  // namespace crt::spectrum
