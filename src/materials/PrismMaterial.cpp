#pragma once
#include "../materials/Material.cpp"
#define _USE_MATH_DEFINES
#include <cmath>
#include <math.h>
#include "../utils/Logger.hpp"

class PrismMaterial : public Material {
private:
    static constexpr float REFRACTION_ANGLE = 0.3f;
    static constexpr float COLOR_SPREAD = 0.1f;

    void adjustRefractedAngle(
        const Vector2D& incomingDir,
        const Vector2D& normal,
        float& baseAngle
    ) const {
        bool isTopEntry = normal.getY() < 0;
        bool isBottomEntry = normal.getY() > 0;
        bool isLeftEntry = normal.getX() < 0;
        bool isRightEntry = normal.getX() > 0;

        float incomingAngle = incomingDir.getAngle();

        if (isTopEntry) {
            baseAngle = M_PI / 4 + REFRACTION_ANGLE * (incomingDir.getX() > 0 ? 1 : -1);
        }
        else if (isBottomEntry) {
            baseAngle = -M_PI / 4 + REFRACTION_ANGLE * (incomingDir.getX() > 0 ? -1 : 1);
        }
        else if (isLeftEntry) {
            baseAngle = incomingAngle + REFRACTION_ANGLE * (incomingDir.getY() > 0 ? 1 : -1);
        }
        else if (isRightEntry) {
            baseAngle = incomingAngle - REFRACTION_ANGLE * (incomingDir.getY() > 0 ? 1 : -1);
        }
    }

public:
    PrismMaterial() : Material(0.1f, 0.1f, L' ', L"Prism") {
        setColor(Color(200, 200, 255));
    }

    ReflectionResult calculateReflection(
        const Vector2D& incomingDir,
        const Vector2D& normal,
        const IntensityInfo& intensity,
        const Spectrum& incidentSpectrum,
        const Vector2D& hitPoint
    ) const override {
        ReflectionResult result;

        if (intensity.attenuatedIntensity < 0.35f) return result;

        float baseRefractedAngle = incomingDir.getAngle();
        adjustRefractedAngle(incomingDir, normal, baseRefractedAngle);

        if (incidentSpectrum.isMonochromatic) {
            Vector2D refractedDir = Vector2D::fromAngle(baseRefractedAngle);

            result.reflectedRays.emplace_back(
                refractedDir,
                intensity.attenuatedIntensity * 0.95f,
                incidentSpectrum
            );
        }
        else {
            const float wavelengths[] = { 650.0f, 530.0f, 450.0f };
            const float spreads[] = { 0.0f, COLOR_SPREAD, COLOR_SPREAD * 2.0f };

            for (int i = 0; i < 3; ++i) {
                float colorAngle = baseRefractedAngle + spreads[i];
                Vector2D refractedDir = Vector2D::fromAngle(colorAngle);

                Spectrum colorSpectrum(
                    wavelengths[i],
                    intensity.attenuatedIntensity,
                    true
                );

                result.reflectedRays.emplace_back(
                    refractedDir,
                    intensity.attenuatedIntensity * 0.95f,
                    colorSpectrum
                );
            }
        }

        return result;
    }

    float getSpectralResponse(float wavelength) const override {
        return 0.95f;
    }
};