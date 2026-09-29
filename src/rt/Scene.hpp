#pragma once

#include <string>
#include <vector>

#include "rt/Bvh.hpp"
#include "rt/Camera.hpp"
#include "rt/Material.hpp"

namespace crt::rt {

/// Distant illumination: a sky gradient plus an optional sun disc.
struct Environment {
    Rgb zenith{0.0f};
    Rgb horizon{0.0f};
    Rgb ground{0.0f};

    bool hasSun = false;
    Vec3 sunDirection{0.0f, 1.0f, 0.0f};  ///< unit vector pointing towards the sun
    Rgb sunRadiance{0.0f};
    float sunAngularRadius = radians(0.8f);

    /// Sky radiance in direction `d` (unit). The sun disc is included only if requested: after a
    /// diffuse bounce it is accounted for by explicit sun sampling instead.
    Rgb radiance(const Vec3& d, bool includeSun) const;

    float sunCosThreshold() const { return std::cos(sunAngularRadius); }
    float sunSolidAngle() const { return kTwoPi * (1.0f - sunCosThreshold()); }
};

/// Everything the path tracer needs: materials, geometry (with an acceleration structure),
/// emitters for light sampling, the environment and a suggested starting camera.
class Scene {
public:
    std::string name;
    std::string description;
    Environment environment;
    Camera camera;
    float exposure = 0.0f;  ///< EV offset suggested for this scene

    int addMaterial(Material m);
    const Material& material(int index) const { return materials_[static_cast<std::size_t>(index)]; }
    std::size_t materialCount() const { return materials_.size(); }

    void add(std::unique_ptr<Primitive> primitive);
    PrimitiveList& primitives() { return primitives_; }

    /// Builds the BVH and the emitter list. Must be called after the last add().
    void finalize();

    bool intersect(const Ray& ray, float tMax, Hit& hit) const;
    bool occluded(const Ray& ray, float tMax) const;

    const std::vector<const Primitive*>& emitters() const { return emitters_; }
    std::size_t primitiveCount() const { return primitives_.size(); }
    const Bvh& bvh() const { return bvh_; }

private:
    std::vector<Material> materials_;
    PrimitiveList primitives_;
    std::vector<const Primitive*> unbounded_;
    std::vector<const Primitive*> emitters_;
    Bvh bvh_;
};

}  // namespace crt::rt
