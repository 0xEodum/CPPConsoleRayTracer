#include "rt/Scene.hpp"

#include <cmath>

namespace crt::rt {

namespace {
constexpr float kRayEpsilon = 1e-4f;
}

Rgb Environment::radiance(const Vec3& d, bool includeSun) const {
    Rgb sky;
    if (d.y >= 0.0f) {
        const float t = std::sqrt(d.y);
        sky = horizon * (1.0f - t) + zenith * t;
    } else {
        const float t = std::min(1.0f, -d.y * 4.0f);
        sky = horizon * (1.0f - t) + ground * t;
    }
    if (includeSun && hasSun && dot(d, sunDirection) >= sunCosThreshold()) sky += sunRadiance;
    return sky;
}

int Scene::addMaterial(Material m) {
    materials_.push_back(std::move(m));
    return static_cast<int>(materials_.size() - 1);
}

void Scene::add(std::unique_ptr<Primitive> primitive) { primitives_.push_back(std::move(primitive)); }

void Scene::finalize() {
    std::vector<const Primitive*> bounded;
    unbounded_.clear();
    emitters_.clear();
    for (const auto& p : primitives_) {
        (p->isBounded() ? bounded : unbounded_).push_back(p.get());
        const Material& m = material(p->material());
        if (m.kind == MaterialKind::Emissive && p->isBounded() && p->area() > 0.0f) emitters_.push_back(p.get());
    }
    bvh_.build(bounded);
}

bool Scene::intersect(const Ray& ray, float tMax, Hit& hit) const {
    bool found = bvh_.intersect(ray, kRayEpsilon, tMax, hit);
    if (found) tMax = hit.t;
    for (const Primitive* p : unbounded_) {
        if (p->intersect(ray, kRayEpsilon, tMax, hit)) {
            tMax = hit.t;
            hit.primitive = p;
            found = true;
        }
    }
    if (found) {
        hit.point = ray.at(hit.t);
        hit.material = hit.primitive->material();
    }
    return found;
}

bool Scene::occluded(const Ray& ray, float tMax) const {
    if (bvh_.occluded(ray, kRayEpsilon, tMax)) return true;
    Hit scratch;
    for (const Primitive* p : unbounded_) {
        if (p->intersect(ray, kRayEpsilon, tMax, scratch)) return true;
    }
    return false;
}

}  // namespace crt::rt
