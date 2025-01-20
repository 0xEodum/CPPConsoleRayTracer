#pragma once
#include "../core/Color.cpp"
#include "../utils/Logger.hpp"
class Spectrum {

public:
    static constexpr int SPECTRUM_SAMPLES = 8;
    bool isMonochromatic;

private:
    float wavelengths[SPECTRUM_SAMPLES];
    float intensities[SPECTRUM_SAMPLES];
    

public:
    Spectrum(float wavelength, float intensity = 1.0f, bool monochromatic = false)
        : isMonochromatic(monochromatic) {
        for (int i = 0; i < SPECTRUM_SAMPLES; ++i) {
            wavelengths[i] = 380.0f + (780.0f - 380.0f) * i / (SPECTRUM_SAMPLES - 1);
            float diff = wavelengths[i] - wavelength;

            if (monochromatic) {
                float sigma = 100.0f;
                intensities[i] = intensity * exp(-(diff * diff) / (2.0f * sigma * sigma));

                if (std::abs(diff) < (780.0f - 380.0f) / SPECTRUM_SAMPLES) {
                    intensities[i] = intensity;
                }
            }
            else {
                intensities[i] = intensity * exp(-(diff * diff) / 2000.0f);
            }
        }

        if (monochromatic) {
            float maxIntensity = 0;
            for (int i = 0; i < SPECTRUM_SAMPLES; ++i) {
                if (intensities[i] > maxIntensity) {
                    maxIntensity = intensities[i];
                }
            }
            
            if (maxIntensity > 0) {
                for (int i = 0; i < SPECTRUM_SAMPLES; ++i) {
                    intensities[i] *= intensity / maxIntensity;
                }
            }
        }
    }

    void setMonochromatic(bool value) {
        isMonochromatic = value;
    }

    static Spectrum White(float intensity = 1.0f) {
        Spectrum spectrum(0);
        for (int i = 0; i < SPECTRUM_SAMPLES; ++i) {
            spectrum.wavelengths[i] = 380.0f + (780.0f - 380.0f) * i / (SPECTRUM_SAMPLES - 1);
            spectrum.intensities[i] = intensity;
        }
        spectrum.isMonochromatic = false;
        return spectrum;
    }

    Color toRGB() const {
        float r = 0, g = 0, b = 0;
        float maxIntensity = 0;

        for (int i = 0; i < SPECTRUM_SAMPLES; ++i) {
            if (intensities[i] > maxIntensity) {
                maxIntensity = intensities[i];
            }
        }

        if (maxIntensity <= 0) {
            return Color(0, 0, 0);
        }

        bool remainsWhite = !isMonochromatic;
        if (remainsWhite) {
            float firstRatio = intensities[0] / maxIntensity;
            for (int i = 1; i < SPECTRUM_SAMPLES; ++i) {
                float ratio = intensities[i] / maxIntensity;
                if (std::abs(ratio - firstRatio) > 0.1f) {
                    remainsWhite = false;
                    break;
                }
            }
        }

        if (isMonochromatic) {
            float wavelength = 0;
            float totalIntensity = 0;

            for (int i = 0; i < SPECTRUM_SAMPLES; ++i) {
                wavelength += wavelengths[i] * intensities[i];
                totalIntensity += intensities[i];
            }

            if (totalIntensity > 0) {
                wavelength /= totalIntensity;

                if (wavelength >= 620 && wavelength <= 750) {
                    r = 1.0f;
                }
                else if (wavelength >= 495 && wavelength < 620) {
                    g = 1.0f;
                }
                else if (wavelength >= 380 && wavelength < 495) {
                    b = 1.0f;
                }
            }
        }
        else {
            for (int i = 0; i < SPECTRUM_SAMPLES; ++i) {
                float wavelength = wavelengths[i];
                float normalizedIntensity = intensities[i] / maxIntensity;

                if (!remainsWhite) {
                    normalizedIntensity *= 1.5f;
                }

                if (wavelength >= 600) {
                    r += normalizedIntensity;
                }
                else if (wavelength >= 490 && wavelength < 600) {
                    g += normalizedIntensity;
                }
                else {
                    b += normalizedIntensity;
                }
            }

            float samplesWeight = 3.0f;
            r = std::min<float>(1.0f, r / samplesWeight);
            g = std::min<float>(1.0f, g / samplesWeight);
            b = std::min<float>(1.0f, b / samplesWeight);
        }

        float brightness = isMonochromatic ?
            std::min<float>(1.0f, maxIntensity * 2.0f) :
            remainsWhite ? maxIntensity : maxIntensity * 1.5f;

        r *= brightness;
        g *= brightness;
        b *= brightness;

        if (remainsWhite) {
            float maxComponent = std::max(std::max(r, g), b);
            r = g = b = maxComponent;
        }

        return Color::fromNormalized(r, g, b);
    }

    Spectrum operator*(float factor) const {
        Spectrum result(*this);
        for (int i = 0; i < SPECTRUM_SAMPLES; ++i) {
            result.intensities[i] *= factor;
        }
        result.isMonochromatic = this->isMonochromatic;
        return result;
    }

    float getIntensityAt(float wavelength) const {
        for (int i = 0; i < SPECTRUM_SAMPLES - 1; ++i) {
            if (wavelength >= wavelengths[i] && wavelength <= wavelengths[i + 1]) {
                float t = (wavelength - wavelengths[i]) / (wavelengths[i + 1] - wavelengths[i]);
                return intensities[i] * (1 - t) + intensities[i + 1] * t;
            }
        }
        return 0.0f;
    }

    float getWavelength(int index) const {
        if (index >= 0 && index < SPECTRUM_SAMPLES) {
            return wavelengths[index];
        }
        return 0.0f;
    }

    float getIntensity(int index) const {
        if (index >= 0 && index < SPECTRUM_SAMPLES) {
            return intensities[index];
        }
        return 0.0f;
    }


    void modifyIntensity(int index, float factor) {
        if (index >= 0 && index < SPECTRUM_SAMPLES) {
            intensities[index] *= factor;
        }
    }

};