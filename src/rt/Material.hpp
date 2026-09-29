#pragma once

#include <cmath>
#include <string>

#include "core/Color.hpp"
#include "core/Vec3.hpp"

namespace crt::rt {

enum class MaterialKind {
    Diffuse,     ///< Lambertian
    Glossy,      ///< Lambertian base under a clear dielectric coat (plastic, lacquer)
    Metal,       ///< Specular reflector with optional roughness
    Dielectric,  ///< Glass / water: Fresnel reflection + refraction
    Emissive,    ///< Area light
};

struct Material {
    std::string name;
    MaterialKind kind = MaterialKind::Diffuse;
    Rgb albedo{0.8f};
    Rgb emission{0.0f};
    float roughness = 0.0f;  ///< metal / glossy coat: 0 = perfect mirror
    float ior = 1.5f;        ///< dielectric / glossy coat

    // Optional procedural checkerboard in the XZ plane.
    bool checker = false;
    Rgb albedo2{0.2f};
    float checkerSize = 1.0f;

    Rgb albedoAt(const Vec3& p) const {
        if (!checker) return albedo;
        const auto cx = static_cast<long>(std::floor(p.x / checkerSize));
        const auto cz = static_cast<long>(std::floor(p.z / checkerSize));
        return ((cx + cz) & 1) == 0 ? albedo : albedo2;
    }

    static Material diffuse(Rgb albedo) { return make(MaterialKind::Diffuse, albedo); }
    static Material glossy(Rgb albedo, float roughness = 0.0f) {
        Material m = make(MaterialKind::Glossy, albedo);
        m.roughness = roughness;
        return m;
    }
    static Material metal(Rgb albedo, float roughness = 0.0f) {
        Material m = make(MaterialKind::Metal, albedo);
        m.roughness = roughness;
        return m;
    }
    static Material glass(float ior = 1.5f, Rgb tint = Rgb(1.0f)) {
        Material m = make(MaterialKind::Dielectric, tint);
        m.ior = ior;
        return m;
    }
    static Material light(Rgb emission) {
        Material m = make(MaterialKind::Emissive, Rgb(0.0f));
        m.emission = emission;
        return m;
    }
    static Material checkerboard(Rgb a, Rgb b, float size) {
        Material m = make(MaterialKind::Diffuse, a);
        m.checker = true;
        m.albedo2 = b;
        m.checkerSize = size;
        return m;
    }

private:
    static Material make(MaterialKind kind, Rgb albedo) {
        Material m;
        m.kind = kind;
        m.albedo = albedo;
        return m;
    }
};

}  // namespace crt::rt
