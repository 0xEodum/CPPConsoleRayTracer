#pragma once
#include "../materials/Material.cpp"

class Absorber : public Material {
public:
    Absorber() : Material(0.0f, 1.0f, L'▓', L"Absorber") {
        setColor(Color(163, 163, 163));
    }

protected:
    float getSpectralResponse(float wavelength) const override {
        return 0.0f;
    }

    ReflectionResult calculateReflection(
        const Vector2D& incomingDir,
        const Vector2D& normal,
        const IntensityInfo& intensity,
        const Spectrum& incidentSpectrum,
        const Vector2D& hitPoint
    ) const override {
        return ReflectionResult();
    }

    Color getBorderColor() const {
        return Color(163, 163, 163);
    }

};