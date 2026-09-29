#pragma once

#include <memory>
#include <vector>

#include "rt/Geometry.hpp"

namespace crt::rt {

/// A piece of geometry the path tracer can intersect. Primitives with an emissive material can
/// also be sampled directly as area lights (next event estimation).
class Primitive {
public:
    explicit Primitive(int material) : material_(material) {}
    virtual ~Primitive() = default;

    /// Fills hit.t, hit.normal and hit.shadingNormal on success (point/material are set by the scene).
    virtual bool intersect(const Ray& ray, float tMin, float tMax, Hit& hit) const = 0;
    virtual Aabb bounds() const = 0;
    /// Unbounded primitives (infinite planes) cannot live in the BVH.
    virtual bool isBounded() const { return true; }

    virtual float area() const = 0;
    /// Uniformly distributed point on the surface for (u, v) in [0,1)^2.
    virtual Vec3 samplePoint(float u, float v, Vec3& normal) const = 0;

    /// Light sampling as seen from `from`: picks a point on the surface, returns false if it cannot
    /// contribute (back-facing / degenerate). Outputs the unit direction, distance and the pdf of
    /// the sample with respect to solid angle. The default samples the area uniformly.
    virtual bool sampleDirection(const Vec3& from, float u, float v, Vec3& direction, float& distance,
                                 float& pdfSolidAngle) const;

    int material() const { return material_; }

private:
    int material_;
};

class Sphere final : public Primitive {
public:
    Sphere(const Vec3& center, float radius, int material) : Primitive(material), center_(center), radius_(radius) {}

    bool intersect(const Ray& ray, float tMin, float tMax, Hit& hit) const override;
    Aabb bounds() const override;
    float area() const override { return 4.0f * kPi * radius_ * radius_; }
    Vec3 samplePoint(float u, float v, Vec3& normal) const override;
    /// Samples the cone of directions subtended by the sphere (far lower variance than area sampling).
    bool sampleDirection(const Vec3& from, float u, float v, Vec3& direction, float& distance,
                         float& pdfSolidAngle) const override;

private:
    Vec3 center_;
    float radius_;
};

/// Parallelogram spanned by edges `u` and `v` from `corner`; the normal is cross(u, v).
class Quad final : public Primitive {
public:
    Quad(const Vec3& corner, const Vec3& u, const Vec3& v, int material);

    bool intersect(const Ray& ray, float tMin, float tMax, Hit& hit) const override;
    Aabb bounds() const override;
    float area() const override { return area_; }
    Vec3 samplePoint(float u, float v, Vec3& normal) const override;

private:
    Vec3 corner_, u_, v_, normal_, w_;
    float offset_;
    float area_;
};

class Triangle final : public Primitive {
public:
    Triangle(const Vec3& a, const Vec3& b, const Vec3& c, int material);
    /// Smooth-shaded triangle with per-vertex normals.
    Triangle(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& na, const Vec3& nb, const Vec3& nc, int material);

    bool intersect(const Ray& ray, float tMin, float tMax, Hit& hit) const override;
    Aabb bounds() const override;
    float area() const override;
    Vec3 samplePoint(float u, float v, Vec3& normal) const override;

private:
    Vec3 a_, e1_, e2_, normal_;
    Vec3 na_, nb_, nc_;
    bool smooth_ = false;
};

/// Infinite plane through `point` with the given normal.
class Plane final : public Primitive {
public:
    Plane(const Vec3& point, const Vec3& normal, int material)
        : Primitive(material), normal_(normalize(normal)), offset_(dot(normal_, point)) {}

    bool intersect(const Ray& ray, float tMin, float tMax, Hit& hit) const override;
    Aabb bounds() const override { return {}; }
    bool isBounded() const override { return false; }
    float area() const override { return 0.0f; }
    Vec3 samplePoint(float, float, Vec3& normal) const override {
        normal = normal_;
        return normal_ * offset_;
    }

private:
    Vec3 normal_;
    float offset_;
};

using PrimitiveList = std::vector<std::unique_ptr<Primitive>>;

/// Appends the six faces of a box (rotated by `yaw` radians around the vertical axis).
void addBox(PrimitiveList& out, const Vec3& center, const Vec3& size, float yaw, int material);

/// Appends a smooth torus lying in the XZ plane, rotated by `tilt` radians around the X axis.
void addTorus(PrimitiveList& out, const Vec3& center, float majorRadius, float minorRadius, float tilt,
              int material, int segments = 48, int sides = 24);

}  // namespace crt::rt
