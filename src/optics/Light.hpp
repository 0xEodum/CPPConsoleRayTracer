#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/Random.hpp"
#include "core/Spectrum.hpp"
#include "optics/Shape.hpp"

namespace crt::optics {

/// A named emission spectrum preset ("white", "red laser", ...).
struct LightColor {
    std::string id;
    std::string label;
    spectrum::EmissionSpectrum spectrum;
};

const std::vector<LightColor>& lightColorCatalogue();
int findLightColor(std::string_view id);
const LightColor& lightColor(int index);

/// Base class for light sources. Lights emit rays; each ray carries one wavelength drawn from
/// the light's emission spectrum, which is what makes dispersion "just work".
class Light {
public:
    virtual ~Light() = default;

    virtual std::unique_ptr<Light> clone() const = 0;
    virtual std::string_view type() const = 0;
    virtual std::string_view displayName() const = 0;
    /// Terminal glyph used to draw the light (UTF-8, single column).
    virtual std::string_view glyph() const;

    /// Samples an emitted ray (origin + unit direction).
    virtual Ray2 emit(Pcg32& rng) const = 0;

    /// Resizes the light's shape parameter (cone spread, beam width) if it has one.
    virtual void scale(float /*factor*/) {}
    virtual std::vector<Param> params() const { return {}; }
    virtual bool setParam(std::string_view /*name*/, float /*value*/) { return false; }
    /// Whether direction matters (false for omnidirectional lights).
    virtual bool isDirectional() const { return true; }

    Vec2 position;
    float angle = 0.0f;  ///< emission direction, radians
    float power = 1.0f;
    int color = 0;       ///< index into lightColorCatalogue()
};

class PointLight final : public Light {
public:
    std::unique_ptr<Light> clone() const override { return std::make_unique<PointLight>(*this); }
    std::string_view type() const override { return "point"; }
    std::string_view displayName() const override { return "Point light"; }
    std::string_view glyph() const override { return "●"; }
    Ray2 emit(Pcg32& rng) const override;
    bool isDirectional() const override { return false; }
};

class SpotLight final : public Light {
public:
    std::unique_ptr<Light> clone() const override { return std::make_unique<SpotLight>(*this); }
    std::string_view type() const override { return "spot"; }
    std::string_view displayName() const override { return "Spotlight"; }
    std::string_view glyph() const override { return "◈"; }
    Ray2 emit(Pcg32& rng) const override;
    void scale(float factor) override;
    std::vector<Param> params() const override;
    bool setParam(std::string_view name, float value) override;

    float spread = radians(40.0f);  ///< full cone angle
};

class LaserLight final : public Light {
public:
    std::unique_ptr<Light> clone() const override { return std::make_unique<LaserLight>(*this); }
    std::string_view type() const override { return "laser"; }
    std::string_view displayName() const override { return "Laser"; }
    Ray2 emit(Pcg32& rng) const override;
};

/// Collimated beam of parallel rays (a "flashlight" with a lens), ideal for lens experiments.
class BeamLight final : public Light {
public:
    std::unique_ptr<Light> clone() const override { return std::make_unique<BeamLight>(*this); }
    std::string_view type() const override { return "beam"; }
    std::string_view displayName() const override { return "Beam"; }
    std::string_view glyph() const override { return "≡"; }
    Ray2 emit(Pcg32& rng) const override;
    void scale(float factor) override;
    std::vector<Param> params() const override;
    bool setParam(std::string_view name, float value) override;

    float width = 8.0f;
};

std::unique_ptr<Light> makeLight(std::string_view type);
const std::vector<std::string_view>& lightTypes();

/// Arrow glyph pointing along `angle` (8 directions, screen coordinates with y down).
std::string_view arrowGlyph(float angle);

}  // namespace crt::optics
