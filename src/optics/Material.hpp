#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/Color.hpp"
#include "core/Random.hpp"
#include "core/Vec2.hpp"

namespace crt::optics {

enum class MaterialKind {
    Diffuse,     ///< Lambertian scatterer (the old "Matte")
    Mirror,      ///< Perfect specular reflector, optionally tinted (spectral reflectance)
    Metal,       ///< Rough specular reflector
    Dielectric,  ///< Refractive medium: Snell's law, Fresnel, Cauchy dispersion
    Absorber,    ///< Black body: terminates light
};

/// Immutable optical description of a surface. Instances live in a global catalogue and shapes
/// refer to them by index (flyweight), which also makes scene files trivially serialisable.
struct Material {
    std::string id;     ///< stable identifier used in scene files ("glass", "mirror", ...)
    std::string label;  ///< human-readable name for the UI
    MaterialKind kind = MaterialKind::Diffuse;
    Rgb albedo{0.8f};         ///< reflectance for diffuse / mirror / metal
    float roughness = 0.0f;   ///< metal: standard deviation of the reflected angle (radians)
    float cauchyA = 1.5f;     ///< dielectric: n(lambda) = A + B / lambda^2
    float cauchyB = 0.0042f;  ///< dielectric: B in um^2
    Rgb8 tint{200, 200, 200}; ///< colour used when drawing the object outline / fill

    float ior(float wavelengthNm) const;
};

const std::vector<Material>& materialCatalogue();
/// Index of the material with the given id, or -1.
int findMaterial(std::string_view id);
const Material& material(int index);

/// Result of light hitting a surface.
struct Interaction {
    bool alive = false;     ///< false if the light was absorbed
    Vec2 direction;         ///< new propagation direction (unit)
    float weight = 1.0f;    ///< throughput multiplier for this wavelength
    bool transmitted = false;
    bool diffuse = false;   ///< scattered diffusely (drives the "lit surface" glow)
};

/// Samples the interaction of a ray of wavelength `lambda` travelling along `dir` with a surface
/// whose outward unit normal is `normal`.
Interaction interact(const Material& m, Vec2 dir, Vec2 normal, float lambda, Pcg32& rng);

/// Unpolarised Fresnel reflectance for a dielectric interface.
/// cosI: cosine of the incident angle (>= 0), eta: n_incident / n_transmitted.
/// Returns 1 on total internal reflection; otherwise also outputs cosT.
float fresnelDielectric(float cosI, float eta, float& cosT);

}  // namespace crt::optics
