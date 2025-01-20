#pragma once
#include <memory>
#include <string>
#include <vector>
#include "../core/Color.cpp"
#include "../core/Spectrum.cpp"
#include "../core/Vector2D.cpp"

class Material {
public:
    struct SSSPoint {
        Vector2D position;
        wchar_t symbol;
        float intensity;
        Color color;

        SSSPoint(const Vector2D& pos, wchar_t symbol, float intensity, const Color& color)
            : position(pos), symbol(symbol), intensity(intensity), color(color) {
        }

        SSSPoint(SHORT x, SHORT y, wchar_t symbol, float intensity, const Color& color)
            : position(Vector2D::fromConsoleCoords(x, y))
            , symbol(symbol)
            , intensity(intensity)
            , color(color) {
        }
    };

    struct ReflectionResult {
        struct Ray {
            Vector2D direction;
            float intensity;
            Spectrum spectrum;

            Ray(const Vector2D& dir, float i, const Spectrum& s)
                : direction(dir), intensity(i), spectrum(s) {
            }

            Ray(float dx, float dy, float i, const Spectrum& s)
                : direction(Vector2D(dx, dy)), intensity(i), spectrum(s) {
            }
        };

        std::vector<Ray> reflectedRays;
        std::vector<SSSPoint> sssPoints;
    };

    struct IntensityInfo {
        float incidentIntensity;
        float attenuatedIntensity;

        IntensityInfo(float incident, float attenuated)
            : incidentIntensity(incident), attenuatedIntensity(attenuated) {
        }
    };

protected:
    float reflectivity;
    float absorption;
    wchar_t fillSymbol;
    std::wstring name;
    Color color;

    struct SpectralProperties {
        float minWavelength;
        float maxWavelength;
        float peakWavelength;

        SpectralProperties(float min = 380.0f, float max = 780.0f, float peak = 550.0f)
            : minWavelength(min), maxWavelength(max), peakWavelength(peak) {
        }
    } spectralProps;

    float getHashedFloat(const Vector2D& pos, const Vector2D& dir) const {
        unsigned int hash = static_cast<unsigned int>(
            (pos.getX() * 12.9898f +
                pos.getY() * 78.233f +
                dir.getX() * 37.719f +
                dir.getY() * 63.137f) * 43758.5453f
            );
        return (hash % 1000) / 1000.0f;
    }

    float getHashedFloat(float x, float y, float dx, float dy) const {
        return getHashedFloat(Vector2D(x, y), Vector2D(dx, dy));
    }

    virtual float getSpectralResponse(float wavelength) const {
        if (wavelength < spectralProps.minWavelength ||
            wavelength > spectralProps.maxWavelength) {
            return 0.0f;
        }

        float diff = wavelength - spectralProps.peakWavelength;
        return exp(-(diff * diff) / 10000.0f);
    }

public:
    Material(float refl, float abs, wchar_t symbol, std::wstring name)
        : reflectivity(refl)
        , absorption(abs)
        , fillSymbol(symbol)
        , name(name)
        , color(255, 255, 255) {
    }

    virtual ~Material() = default;

    virtual ReflectionResult calculateReflection(
        const Vector2D& incomingDir,
        const Vector2D& normal,
        const IntensityInfo& intensity,
        const Spectrum& incidentSpectrum,
        const Vector2D& hitPoint
    ) const = 0;

    
    void setColor(const Color& newColor) {
        color = newColor;
        if (color.getNormalizedR() > color.getNormalizedG() &&
            color.getNormalizedR() > color.getNormalizedB()) {
            spectralProps.peakWavelength = 700.0f;
        }
        else if (color.getNormalizedG() > color.getNormalizedR() &&
            color.getNormalizedG() > color.getNormalizedB()) {
            spectralProps.peakWavelength = 530.0f;
        }
        else if (color.getNormalizedB() > color.getNormalizedR() &&
            color.getNormalizedB() > color.getNormalizedG()) {
            spectralProps.peakWavelength = 470.0f;
        }
    }

    void setSpectralRange(float minWavelength, float maxWavelength) {
        spectralProps.minWavelength = minWavelength;
        spectralProps.maxWavelength = maxWavelength;
    }

    void setPeakWavelength(float wavelength) {
        spectralProps.peakWavelength = wavelength;
    }

    const Color& getColor() const { return color; }
    float getReflectivity() const { return reflectivity; }
    float getAbsorption() const { return absorption; }
    wchar_t getFillSymbol() const { return fillSymbol; }
    const std::wstring& getName() const { return name; }
};