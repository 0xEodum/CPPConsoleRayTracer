#pragma once
#include "../lights/LightSource.cpp"

class PointLight : public LightSource {
public:
    PointLight(SHORT x, SHORT y, float intensity = 1.0f, int rays = 15)
        : LightSource(x, y, intensity, rays)
    {
        symbol = L'•';
    }

    std::unique_ptr<LightSource> clone() const override {
        return std::make_unique<PointLight>(*this);
    }
};