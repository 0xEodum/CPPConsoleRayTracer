#pragma once

#include "core/Random.hpp"
#include "rt/Geometry.hpp"

namespace crt::rt {

/// First-person pinhole / thin-lens camera. Y is up; yaw 0 looks down -Z; positive pitch looks up.
struct Camera {
    Vec3 position{0.0f, 1.0f, 4.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
    float verticalFov = 45.0f;    ///< degrees
    float aperture = 0.0f;        ///< lens diameter; 0 = everything in focus
    float focusDistance = 4.0f;

    Vec3 forward() const;
    Vec3 right() const;
    Vec3 up() const;

    void lookAt(const Vec3& target);
    void move(const Vec3& localDelta);  ///< x = right, y = world up, z = forward

    /// Primary ray through continuous pixel coordinates (px, py) of a width x height image whose
    /// pixels are `pixelAspect` times taller than wide.
    Ray generateRay(float px, float py, int width, int height, float pixelAspect, Pcg32& rng) const;

    bool operator==(const Camera& o) const {
        return position.x == o.position.x && position.y == o.position.y && position.z == o.position.z && yaw == o.yaw &&
               pitch == o.pitch && verticalFov == o.verticalFov && aperture == o.aperture &&
               focusDistance == o.focusDistance;
    }
    bool operator!=(const Camera& o) const { return !(*this == o); }
};

}  // namespace crt::rt
