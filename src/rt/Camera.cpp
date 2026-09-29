#include "rt/Camera.hpp"

#include <cmath>

namespace crt::rt {

Vec3 Camera::forward() const {
    const float cp = std::cos(pitch);
    return {-std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
}

Vec3 Camera::right() const { return {std::cos(yaw), 0.0f, -std::sin(yaw)}; }

Vec3 Camera::up() const { return cross(right(), forward()); }

void Camera::lookAt(const Vec3& target) {
    const Vec3 d = normalize(target - position);
    pitch = std::asin(std::clamp(d.y, -1.0f, 1.0f));
    yaw = std::atan2(-d.x, -d.z);
    focusDistance = length(target - position);
}

void Camera::move(const Vec3& localDelta) {
    const Vec3 flatForward = normalize(Vec3{forward().x, 0.0f, forward().z});
    position += right() * localDelta.x + Vec3{0.0f, localDelta.y, 0.0f} + flatForward * localDelta.z;
}

Ray Camera::generateRay(float px, float py, int width, int height, float pixelAspect, Pcg32& rng) const {
    const float halfHeight = std::tan(radians(verticalFov) * 0.5f);
    const float halfWidth = halfHeight * static_cast<float>(width) / (static_cast<float>(height) * pixelAspect);
    const float ndcX = 2.0f * px / static_cast<float>(width) - 1.0f;
    const float ndcY = 1.0f - 2.0f * py / static_cast<float>(height);

    const Vec3 f = forward(), r = right(), u = cross(r, f);
    Vec3 dir = normalize(f + r * (ndcX * halfWidth) + u * (ndcY * halfHeight));
    if (aperture <= 0.0f) return {position, dir};

    // Thin lens: all rays through the same pixel converge on the plane of focus.
    const Vec3 focusPoint = position + dir * (focusDistance / dot(dir, f));
    const Vec2 lens = sampleUnitDisk(rng.uniform(), rng.uniform()) * (0.5f * aperture);
    const Vec3 origin = position + r * lens.x + u * lens.y;
    return {origin, normalize(focusPoint - origin)};
}

}  // namespace crt::rt
