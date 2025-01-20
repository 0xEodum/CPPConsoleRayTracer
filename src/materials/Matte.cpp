#pragma once
#define _USE_MATH_DEFINES
#include "../materials/Material.cpp"
#include <math.h>
#include <cmath>
#include <vector>
#include "../lights/LightSource.cpp"

class Matte : public Material {
private:
    static constexpr int MAX_SSS_DISTANCE = 2;
    static constexpr float INTENSITY_THRESHOLD = 0.25f;
    static constexpr float DISTANCE_1_PROBABILITY = 0.8f;
    static constexpr float DISTANCE_2_PROBABILITY = 0.3f;

    static constexpr int BASE_REFLECTION_RAYS = 3;
    static constexpr int MAX_REFLECTION_RAYS = 7;
    static constexpr float GRAZING_ANGLE_THRESHOLD = 0.2f;
    static constexpr float REFLECTION_INTENSITY_THRESHOLD = 0.2f;

public:
    Matte() : Material(0.6f, 0.4f, L'░', L"Matte") {}

protected:
    ReflectionResult calculateReflection(
        const Vector2D& incomingDir,
        const Vector2D& normal,
        const IntensityInfo& intensity,
        const Spectrum& incidentSpectrum,
        const Vector2D& hitPoint
    ) const override {
        ReflectionResult result;
        float dotProduct = incomingDir.dot(normal);

        float spectrumCoherence = calculateSpectrumCoherence(incidentSpectrum);
        float reflectionFactor = reflectivity * (0.4f + spectrumCoherence * 0.6f);
        float actualIntensityAtHitPoint = intensity.attenuatedIntensity * reflectionFactor;

        if (actualIntensityAtHitPoint >= REFLECTION_INTENSITY_THRESHOLD) {
            float baseRayIntensity = intensity.incidentIntensity * reflectionFactor;
            int raysCount = getReflectionRaysCount(dotProduct);

            for (int i = 0; i < raysCount; ++i) {
                float randomValue = getHashedFloat(hitPoint, incomingDir * static_cast<float>(i + 1));
                float angle = (randomValue - 0.5f) * M_PI * (1.0f - std::abs(dotProduct));

                Vector2D reflectedDir = normal.rotate(angle);

                result.reflectedRays.emplace_back(
                    reflectedDir,
                    baseRayIntensity,
                    incidentSpectrum * reflectionFactor
                );
            }
        }

        if (intensity.attenuatedIntensity >= INTENSITY_THRESHOLD) {
            generateSSSPoints(result.sssPoints,
                hitPoint,
                normal,
                intensity.attenuatedIntensity,
                incidentSpectrum,
                spectrumCoherence
            );
        }

        return result;
    }

private:
    int getReflectionRaysCount(float dotProduct) const {
        float grazingFactor = 1.0f - std::abs(dotProduct);
        if (grazingFactor > GRAZING_ANGLE_THRESHOLD) {
            return std::min<float>(
                MAX_REFLECTION_RAYS,
                BASE_REFLECTION_RAYS + static_cast<int>((grazingFactor - GRAZING_ANGLE_THRESHOLD) * 10)
            );
        }
        return BASE_REFLECTION_RAYS;
    }

    float calculateSpectrumCoherence(const Spectrum& spectrum) const {
        float maxIntensity = 0;
        float totalIntensity = 0;

        for (int i = 0; i < Spectrum::SPECTRUM_SAMPLES; ++i) {
            float intensity = spectrum.getIntensity(i);
            maxIntensity = std::max<float>(maxIntensity, intensity);
            totalIntensity += intensity;
        }

        if (totalIntensity <= 0) return 0;
        return maxIntensity / (totalIntensity / Spectrum::SPECTRUM_SAMPLES);
    }

    float calculateSSSIntensityFactor(float coherence, float intensity) const {
        float baseFactor = 0.3f + (std::min<float>(coherence, 1.0f) * 0.5f);
        float intensityFactor = 0.8f + (0.2f * (1.0f - std::min<float>(intensity, 1.0f)));
        return baseFactor * intensityFactor;
    }

    void generateSSSPoints(
        std::vector<SSSPoint>& points,
        const Vector2D& hitPoint,
        const Vector2D& normal,
        float intensity,
        const Spectrum& spectrum,
        float coherence
    ) const {
        Color baseColor = spectrum.toRGB();
        float sssIntensityFactor = calculateSSSIntensityFactor(coherence, intensity);
        float firstLayerIntensity = intensity * sssIntensityFactor;
        float secondLayerIntensity = firstLayerIntensity * 0.8f;

        Vector2D scatterDir;
        if (std::abs(normal.getX()) > std::abs(normal.getY())) {
            scatterDir = Vector2D(normal.getX() > 0 ? 1.0f : -1.0f, 0.0f);
        }
        else {
            scatterDir = Vector2D(0.0f, normal.getY() > 0 ? 1.0f : -1.0f);
        }

        int width = static_cast<int>(5 * (1.0f - coherence * 0.4f));
        int height = static_cast<int>(5 * (1.0f - coherence * 0.4f));
        if (std::abs(normal.getX()) > std::abs(normal.getY())) {
            width = 1;
        }
        else {
            height = 1;
        }

        generateSSSLayer(points,
            hitPoint,
            scatterDir,
            width, height,
            firstLayerIntensity,
            baseColor,
            DISTANCE_1_PROBABILITY,
            1
        );

        generateSSSLayer(points,
            hitPoint,
            scatterDir,
            std::max<float>(1, width - 2),
            std::max<float>(1, height - 2),
            secondLayerIntensity,
            baseColor,
            DISTANCE_2_PROBABILITY,
            2
        );
    }

    void generateSSSLayer(
        std::vector<SSSPoint>& points,
        const Vector2D& center,
        const Vector2D& scatterDir,
        int width, int height,
        float layerIntensity,
        const Color& baseColor,
        float probability,
        int distance
    ) const {
        for (int i = -width; i <= width; i++) {
            for (int j = -height; j <= height; j++) {
                Vector2D offset(static_cast<float>(i), static_cast<float>(j));
                float random = getHashedFloat(center + offset, scatterDir * static_cast<float>(distance));

                if (random < probability) {
                    wchar_t symbol = getSymbolForIntensity(layerIntensity);
                    Vector2D pointPos = center + offset + scatterDir * static_cast<float>(distance);

                    points.emplace_back(
                        pointPos,
                        symbol,
                        layerIntensity,
                        baseColor * layerIntensity
                    );
                }
            }
        }
    }

    wchar_t getSymbolForIntensity(float intensity) const {
        if (intensity > 0.6f) return L'o';
        if (intensity > 0.5f) return L'*';
        if (intensity > 0.3f) return L'+';
        return L'·';
    }
};