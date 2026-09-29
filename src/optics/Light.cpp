#include "optics/Light.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace crt::optics {

using spectrum::EmissionSpectrum;

const std::vector<LightColor>& lightColorCatalogue() {
    static const std::vector<LightColor> catalogue = {
        {"white", "White", EmissionSpectrum::uniform()},
        {"warm", "Warm 3000K", EmissionSpectrum::blackbody(3000.0f)},
        {"daylight", "Daylight 6500K", EmissionSpectrum::blackbody(6500.0f)},
        {"red", "Red 650nm", EmissionSpectrum::monochromatic(650.0f)},
        {"orange", "Orange 610nm", EmissionSpectrum::monochromatic(610.0f)},
        {"yellow", "Sodium 589nm", EmissionSpectrum::monochromatic(589.0f)},
        {"green", "Green 532nm", EmissionSpectrum::monochromatic(532.0f)},
        {"cyan", "Cyan 495nm", EmissionSpectrum::monochromatic(495.0f)},
        {"blue", "Blue 460nm", EmissionSpectrum::monochromatic(460.0f)},
        {"violet", "Violet 410nm", EmissionSpectrum::monochromatic(410.0f)},
    };
    return catalogue;
}

int findLightColor(std::string_view id) {
    const auto& all = lightColorCatalogue();
    for (std::size_t i = 0; i < all.size(); ++i) {
        if (all[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

const LightColor& lightColor(int index) {
    const auto& all = lightColorCatalogue();
    if (index < 0 || static_cast<std::size_t>(index) >= all.size()) return all.front();
    return all[static_cast<std::size_t>(index)];
}

std::string_view Light::glyph() const { return arrowGlyph(angle); }

std::string_view arrowGlyph(float angle) {
    // Screen coordinates: +x right, +y down; so angle +90deg points down.
    static constexpr std::array<std::string_view, 8> arrows = {"→", "↘", "↓", "↙", "←", "↖", "↑", "↗"};
    const float sector = wrapAngle(angle) / (kPi / 4.0f);
    const int index = (static_cast<int>(std::lround(sector)) % 8 + 8) % 8;
    return arrows[static_cast<std::size_t>(index)];
}

Ray2 PointLight::emit(Pcg32& rng) const { return {position, fromAngle(rng.uniform() * kTwoPi)}; }

Ray2 SpotLight::emit(Pcg32& rng) const {
    // Smooth-edged cone: the sum of two uniforms gives a triangular (soft) angular profile.
    const float offset = (rng.uniform() + rng.uniform() - 1.0f) * spread;
    return {position, fromAngle(angle + offset)};
}

void SpotLight::scale(float factor) { spread = std::clamp(spread * factor, radians(2.0f), radians(180.0f)); }

std::vector<Param> SpotLight::params() const { return {{"spread", degrees(spread)}}; }

bool SpotLight::setParam(std::string_view name, float value) {
    if (name != "spread") return false;
    spread = radians(std::clamp(value, 2.0f, 180.0f));
    return true;
}

Ray2 LaserLight::emit(Pcg32& /*rng*/) const { return {position, fromAngle(angle)}; }

Ray2 BeamLight::emit(Pcg32& rng) const {
    const Vec2 dir = fromAngle(angle);
    const Vec2 across = perpendicular(dir) * ((rng.uniform() - 0.5f) * width);
    return {position + across, dir};
}

void BeamLight::scale(float factor) { width = std::clamp(width * factor, 0.5f, 200.0f); }

std::vector<Param> BeamLight::params() const { return {{"width", width}}; }

bool BeamLight::setParam(std::string_view name, float value) {
    if (name != "width") return false;
    width = std::clamp(value, 0.5f, 200.0f);
    return true;
}

const std::vector<std::string_view>& lightTypes() {
    static const std::vector<std::string_view> types = {"point", "spot", "laser", "beam"};
    return types;
}

std::unique_ptr<Light> makeLight(std::string_view type) {
    if (type == "point") return std::make_unique<PointLight>();
    if (type == "spot") return std::make_unique<SpotLight>();
    if (type == "laser") {
        auto laser = std::make_unique<LaserLight>();
        laser->color = findLightColor("red");
        return laser;
    }
    if (type == "beam") return std::make_unique<BeamLight>();
    return nullptr;
}

}  // namespace crt::optics
