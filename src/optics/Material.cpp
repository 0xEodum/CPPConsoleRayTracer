#include "optics/Material.hpp"

#include <cmath>

#include "core/Spectrum.hpp"

namespace crt::optics {

float Material::ior(float wavelengthNm) const { return spectrum::cauchyIor(cauchyA, cauchyB, wavelengthNm); }

const std::vector<Material>& materialCatalogue() {
    static const std::vector<Material> catalogue = [] {
        std::vector<Material> m;
        auto add = [&m](Material mat) { m.push_back(std::move(mat)); };

        add({"matte", "Matte white", MaterialKind::Diffuse, Rgb(0.8f), 0, 0, 0, {205, 205, 200}});
        add({"matte-red", "Matte red", MaterialKind::Diffuse, {0.80f, 0.10f, 0.08f}, 0, 0, 0, {215, 70, 60}});
        add({"matte-green", "Matte green", MaterialKind::Diffuse, {0.10f, 0.70f, 0.14f}, 0, 0, 0, {70, 190, 80}});
        add({"matte-blue", "Matte blue", MaterialKind::Diffuse, {0.08f, 0.18f, 0.80f}, 0, 0, 0, {70, 110, 225}});
        add({"mirror", "Mirror", MaterialKind::Mirror, Rgb(0.96f), 0, 0, 0, {235, 240, 255}});
        add({"gold", "Gold mirror", MaterialKind::Mirror, {1.0f, 0.78f, 0.34f}, 0, 0, 0, {240, 200, 90}});
        add({"brushed", "Brushed metal", MaterialKind::Metal, Rgb(0.9f), 0.12f, 0, 0, {170, 180, 195}});
        add({"glass", "Glass (BK7)", MaterialKind::Dielectric, Rgb(1.0f), 0, 1.5046f, 0.00420f, {150, 210, 240}});
        add({"water", "Water", MaterialKind::Dielectric, Rgb(1.0f), 0, 1.3240f, 0.00309f, {90, 150, 240}});
        add({"diamond", "Diamond", MaterialKind::Dielectric, Rgb(1.0f), 0, 2.3818f, 0.01210f, {225, 235, 255}});
        // Deliberately exaggerated dispersion so the spectrum fans out visibly even at terminal
        // resolution; the spiritual successor of the original "Prism" material.
        add({"prism", "Prism crystal", MaterialKind::Dielectric, Rgb(1.0f), 0, 1.5500f, 0.03000f, {200, 170, 255}});
        add({"absorber", "Absorber", MaterialKind::Absorber, Rgb(0.0f), 0, 0, 0, {110, 110, 115}});
        return m;
    }();
    return catalogue;
}

int findMaterial(std::string_view id) {
    const auto& all = materialCatalogue();
    for (std::size_t i = 0; i < all.size(); ++i) {
        if (all[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

const Material& material(int index) {
    const auto& all = materialCatalogue();
    if (index < 0 || static_cast<std::size_t>(index) >= all.size()) return all.front();
    return all[static_cast<std::size_t>(index)];
}

float fresnelDielectric(float cosI, float eta, float& cosT) {
    const float sin2T = eta * eta * (1.0f - cosI * cosI);
    if (sin2T >= 1.0f) {
        cosT = 0.0f;
        return 1.0f;
    }
    cosT = std::sqrt(1.0f - sin2T);
    const float rs = (eta * cosI - cosT) / (eta * cosI + cosT);
    const float rp = (eta * cosT - cosI) / (eta * cosT + cosI);
    return 0.5f * (rs * rs + rp * rp);
}

Interaction interact(const Material& m, Vec2 dir, Vec2 normal, float lambda, Pcg32& rng) {
    Interaction out;
    const bool frontFace = dot(dir, normal) < 0.0f;
    const Vec2 n = frontFace ? normal : -normal;  // normal on the side the light arrives from

    switch (m.kind) {
        case MaterialKind::Absorber:
            return out;

        case MaterialKind::Diffuse: {
            // 2D Lambertian: outgoing flux ~ cos(theta)  =>  theta = asin(2u - 1).
            const float theta = std::asin(2.0f * rng.uniform() - 1.0f);
            out.direction = n * std::cos(theta) + perpendicular(n) * std::sin(theta);
            out.weight = spectrum::reflectance(m.albedo, lambda);
            out.diffuse = true;
            out.alive = out.weight > 0.0f;
            return out;
        }

        case MaterialKind::Mirror:
        case MaterialKind::Metal: {
            Vec2 r = reflect(dir, n);
            if (m.kind == MaterialKind::Metal && m.roughness > 0.0f) {
                r = rotate(r, rng.normal() * m.roughness);
                if (dot(r, n) <= 0.0f) r = reflect(r, n);  // fold samples that went below the surface
            }
            out.direction = normalize(r);
            out.weight = spectrum::reflectance(m.albedo, lambda);
            out.alive = out.weight > 0.0f;
            return out;
        }

        case MaterialKind::Dielectric: {
            const float ior = m.ior(lambda);
            const float eta = frontFace ? 1.0f / ior : ior;
            const float cosI = -dot(dir, n);
            float cosT = 0.0f;
            const float reflectance = fresnelDielectric(cosI, eta, cosT);
            if (rng.uniform() < reflectance) {
                out.direction = reflect(dir, n);
            } else {
                out.direction = normalize(dir * eta + n * (eta * cosI - cosT));
                out.transmitted = true;
            }
            out.weight = 1.0f;
            out.alive = true;
            return out;
        }
    }
    return out;
}

}  // namespace crt::optics
