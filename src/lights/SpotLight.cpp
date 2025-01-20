#pragma once
#include "../lights/LightSource.cpp"

class SpotLight : public LightSource {
private:
    Vector2D direction;
    float angle;
    float falloff;

public:
    SpotLight(SHORT x, SHORT y, float direction = 0.0f, float angle = M_PI / 4,
        float intensity = 1.0f, int rays = 36)
        : LightSource(x, y, intensity, rays)
        , direction(Vector2D::fromAngle(direction))
        , angle(angle)
        , falloff(0.5f)
    {
        symbol = L'◈';
    }

    void generateRays(std::vector<Ray>& rays) const override {
        rays.clear();
        rays.reserve(raysCount);

        float baseAngle = direction.getAngle();
        float halfAngle = angle / 2;
        float angleStep = angle / (raysCount - 1);

        for (int i = 0; i < raysCount; ++i) {
            float currentAngle = baseAngle - halfAngle + angleStep * i;
            float angleDiff = std::abs(currentAngle - baseAngle);
            float rayIntensity = 1.0f - (angleDiff / halfAngle) * falloff;

            rays.emplace_back(
                Vector2D::fromAngle(currentAngle),
                spectrum * rayIntensity
            );
        }
    }

    const Vector2D& getDirection() const { return direction; }
    void setDirection(const Vector2D& newDirection) {
        direction = newDirection.normalize();
    }

    float getDirectionAngle() const { return direction.getAngle(); }
    void setDirectionAngle(float value) {
        direction = Vector2D::fromAngle(value);
    }

    float getAngle() const { return angle; }
    float getFalloff() const { return falloff; }

    void setAngle(float value) {
        angle = std::max<float>(0.1f, std::min<float>(2 * M_PI, value));
    }

    void setFalloff(float value) {
        falloff = std::max<float>(0.0f, std::min<float>(1.0f, value));
    }

    std::unique_ptr<LightSource> clone() const override {
        return std::make_unique<SpotLight>(*this);
    }
};