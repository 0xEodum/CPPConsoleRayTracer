#pragma once
#include "../materials/Material.cpp"

class Mirror : public Material {
public:
    Mirror() : Material(1.0f, 0.0f, L'█', L"Mirror") {
        setSpectralRange(380.0f, 780.0f);
        setColor(Color::White());
    }

protected:
    float getSpectralResponse(float wavelength) const override {
        if (wavelength >= spectralProps.minWavelength &&
            wavelength <= spectralProps.maxWavelength) {
            return 1.0f;
        }
        return 0.0f;
    }

    ReflectionResult calculateReflection(
        const Vector2D& incomingDir,
        const Vector2D& normal,
        const IntensityInfo& intensity,
        const Spectrum& incidentSpectrum,
        const Vector2D& hitPoint
    ) const override {
        ReflectionResult result;

        float dotProduct = incomingDir.dot(normal);
        Vector2D reflectedDir = incomingDir - normal * (2.0f * dotProduct);

        float reflectionFactor = 0.98f;

        result.reflectedRays.emplace_back(
            reflectedDir,
            intensity.incidentIntensity * reflectionFactor,
            incidentSpectrum * reflectionFactor
        );

        return result;
    }
};