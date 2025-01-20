#pragma once
#include "../lights/LightSource.cpp"

class LaserLight : public LightSource {
private:
    Vector2D target;
    bool isTargetSet;

public:
    LaserLight(SHORT x, SHORT y)
        : LightSource(x, y, 1.0f, 1)
        , target(Vector2D::fromConsoleCoords(x, y))
        , isTargetSet(false)
    {
        symbol = L'↗';
        radius = 300.0f;
        setWavelength(635.0f, true);
    }

    void generateRays(std::vector<Ray>& rays) const override {
        rays.clear();

        if (!isTargetSet) {
            rays.emplace_back(Vector2D(1.0f, 0.0f), spectrum);
            return;
        }

        Vector2D direction = (target - position).normalize();
        rays.emplace_back(direction, spectrum);
    }

    void setTarget(SHORT x, SHORT y) {
        target = Vector2D::fromConsoleCoords(x, y);
        isTargetSet = true;

        Vector2D direction = (target - position).normalize();
        if (std::abs(direction.getX()) > std::abs(direction.getY())) {
            symbol = direction.getX() > 0 ? L'→' : L'←';
        }
        else {
            symbol = direction.getY() > 0 ? L'↓' : L'↑';
        }
    }

    bool hasTarget() const { return isTargetSet; }

    const Vector2D& getTarget() const { return target; }

    float getTargetX() const {
        SHORT x, y;
        target.toConsoleCoords(x, y);
        return static_cast<float>(x);
    }

    float getTargetY() const {
        SHORT x, y;
        target.toConsoleCoords(x, y);
        return static_cast<float>(y);
    }

    std::unique_ptr<LightSource> clone() const override {
        return std::make_unique<LaserLight>(*this);
    }
};