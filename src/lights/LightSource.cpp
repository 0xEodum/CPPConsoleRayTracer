#pragma once
#define _USE_MATH_DEFINES
#include <windows.h>
#include <algorithm>
#include <memory>
#include <cmath>
#include <math.h>
#include <vector>
#include "../core/Color.cpp"
#include "../core/Spectrum.cpp"
#include "../console/ConsoleBuffer.cpp"
#include "../core/Vector2D.cpp"

class LightSource {
protected:
    Vector2D position;
    float intensity;
    int raysCount;
    float radius;
    wchar_t symbol;
    Spectrum spectrum;

public:
    struct Ray {
        Vector2D direction;
        Spectrum spectrum;

        Ray(const Vector2D& dir, const Spectrum& s)
            : direction(dir), spectrum(s) {
        }

        Ray(float dx, float dy, const Spectrum& s)
            : direction(Vector2D(dx, dy)), spectrum(s) {
        }
    };

    LightSource(SHORT x, SHORT y, float intensity = 1.0f, int rays = 72)
        : position(Vector2D::fromConsoleCoords(x, y))
        , intensity(intensity)
        , raysCount(rays)
        , radius(50.0f)
        , symbol(L'•')
        , spectrum(Spectrum::White(intensity))
    {
    }

    virtual ~LightSource() = default;

    virtual void generateRays(std::vector<Ray>& rays) const {
        rays.clear();
        rays.reserve(raysCount);

        for (int i = 0; i < raysCount; ++i) {
            float angle = (2.0f * M_PI * i) / raysCount;
            rays.emplace_back(
                Vector2D::fromAngle(angle),
                spectrum
            );
        }
    }

    void draw(ConsoleBuffer& buffer) const {
        Color color = spectrum.toRGB();
        Color brightColor = Color::fromNormalized(
            std::min<float>(1.0f, color.getNormalizedR() * 1.5f),
            std::min<float>(1.0f, color.getNormalizedG() * 1.5f),
            std::min<float>(1.0f, color.getNormalizedB() * 1.5f)
        );

        SHORT x, y;
        position.toConsoleCoords(x, y);
        buffer.setPixel(x, y, brightColor, symbol);
    }

    float calculateAttenuation(const Vector2D& point) const {
        float distance = (point - position).length();
        if (distance > radius) return 0.0f;
        return intensity * (1.0f - (distance / radius));
    }

    float calculateAttenuation(float distance) const {
        if (distance > radius) return 0.0f;
        return intensity * (1.0f - (distance / radius));
    }

    void setSpectrum(const Spectrum& newSpectrum) {
        spectrum = newSpectrum;
    }

    void setWavelength(float wavelength, bool monochromatic = false) {
        spectrum = Spectrum(wavelength, intensity, monochromatic);
    }

    const Spectrum& getSpectrum() const { return spectrum; }

    void setColor(const Color& color) {
        float r = color.getNormalizedR();
        float g = color.getNormalizedG();
        float b = color.getNormalizedB();

        spectrum = Spectrum::White(intensity);
        if (r > g && r > b) {
            spectrum = Spectrum(700.0f, intensity * r);
        }
        else if (g > r && g > b) {
            spectrum = Spectrum(530.0f, intensity * g);
        }
        else if (b > r && b > g) {
            spectrum = Spectrum(470.0f, intensity * b);
        }
    }

    const Vector2D& getPosition() const { return position; }
    void setPosition(const Vector2D& newPosition) { position = newPosition; }

    SHORT getX() const {
        SHORT x, y;
        position.toConsoleCoords(x, y);
        return x;
    }

    SHORT getY() const {
        SHORT x, y;
        position.toConsoleCoords(x, y);
        return y;
    }

    void setPosition(SHORT newX, SHORT newY) {
        position = Vector2D::fromConsoleCoords(newX, newY);
    }

    float getIntensity() const { return intensity; }
    float getRadius() const { return radius; }
    int getRaysCount() const { return raysCount; }

    void setIntensity(float value) {
        intensity = std::max<float>(0.0f, std::min<float>(1.0f, value));
        spectrum = spectrum * (intensity / getIntensity());
    }

    void setRadius(float value) { radius = std::max<float>(1.0f, value); }
    void setRaysCount(int count) { raysCount = std::max<int>(1, count); }
    void setSymbol(wchar_t newSymbol) { symbol = newSymbol; }

    virtual std::unique_ptr<LightSource> clone() const {
        return std::make_unique<LightSource>(*this);
    }
};