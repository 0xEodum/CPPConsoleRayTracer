#pragma once

#include <utility>

#include "core/Vec3.hpp"

namespace crt::rt {

class Primitive;

struct Ray {
    Vec3 origin;
    Vec3 direction;  ///< unit length

    Vec3 at(float t) const { return origin + direction * t; }
};

struct Hit {
    float t = kInfinity;
    Vec3 point;
    Vec3 normal;         ///< geometric normal, unit, pointing out of the surface
    Vec3 shadingNormal;  ///< interpolated normal (equal to `normal` for flat primitives)
    int material = 0;
    const Primitive* primitive = nullptr;
};

struct Aabb {
    Vec3 lo{kInfinity, kInfinity, kInfinity};
    Vec3 hi{-kInfinity, -kInfinity, -kInfinity};

    void expand(const Vec3& p) {
        lo = min(lo, p);
        hi = max(hi, p);
    }
    void expand(const Aabb& b) {
        lo = min(lo, b.lo);
        hi = max(hi, b.hi);
    }
    Vec3 center() const { return (lo + hi) * 0.5f; }
    Vec3 extent() const { return hi - lo; }
    bool valid() const { return lo.x <= hi.x && lo.y <= hi.y && lo.z <= hi.z; }

    float surfaceArea() const {
        if (!valid()) return 0.0f;
        const Vec3 e = extent();
        return 2.0f * (e.x * e.y + e.y * e.z + e.z * e.x);
    }

    /// Slab test. `invDir` = 1 / ray direction (infinities are fine thanks to IEEE semantics).
    bool intersect(const Vec3& origin, const Vec3& invDir, float tMin, float tMax) const {
        for (int axis = 0; axis < 3; ++axis) {
            float t0 = (lo[axis] - origin[axis]) * invDir[axis];
            float t1 = (hi[axis] - origin[axis]) * invDir[axis];
            if (invDir[axis] < 0.0f) std::swap(t0, t1);
            tMin = t0 > tMin ? t0 : tMin;
            tMax = t1 < tMax ? t1 : tMax;
            if (tMax < tMin) return false;
        }
        return true;
    }
};

}  // namespace crt::rt
