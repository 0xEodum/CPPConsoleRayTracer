#include "rt/Primitive.hpp"

#include <cmath>

namespace crt::rt {

bool Primitive::sampleDirection(const Vec3& from, float u, float v, Vec3& direction, float& distance,
                                float& pdfSolidAngle) const {
    Vec3 lightNormal;
    const Vec3 q = samplePoint(u, v, lightNormal);
    const Vec3 d = q - from;
    const float dist2 = lengthSquared(d);
    if (dist2 <= 1e-12f) return false;
    distance = std::sqrt(dist2);
    direction = d / distance;
    const float cosLight = -dot(lightNormal, direction);
    if (cosLight <= 1e-6f) return false;
    pdfSolidAngle = dist2 / (cosLight * area());
    return true;
}

// ---------------------------------------------------------------------------------------------
// Sphere
// ---------------------------------------------------------------------------------------------

bool Sphere::intersect(const Ray& ray, float tMin, float tMax, Hit& hit) const {
    const Vec3 oc = ray.origin - center_;
    const float b = dot(oc, ray.direction);
    const float c = lengthSquared(oc) - radius_ * radius_;
    const float disc = b * b - c;
    if (disc < 0.0f) return false;
    const float s = std::sqrt(disc);
    float t = -b - s;
    if (t <= tMin || t >= tMax) {
        t = -b + s;
        if (t <= tMin || t >= tMax) return false;
    }
    hit.t = t;
    hit.normal = (ray.at(t) - center_) / radius_;
    hit.shadingNormal = hit.normal;
    return true;
}

Aabb Sphere::bounds() const {
    const Vec3 r{radius_, radius_, radius_};
    return {center_ - r, center_ + r};
}

Vec3 Sphere::samplePoint(float u, float v, Vec3& normal) const {
    const float z = 1.0f - 2.0f * u;
    const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    const float phi = kTwoPi * v;
    normal = {r * std::cos(phi), r * std::sin(phi), z};
    return center_ + normal * radius_;
}

bool Sphere::sampleDirection(const Vec3& from, float u, float v, Vec3& direction, float& distance,
                             float& pdfSolidAngle) const {
    const Vec3 toCenter = center_ - from;
    const float dist2 = lengthSquared(toCenter);
    if (dist2 <= radius_ * radius_) return Primitive::sampleDirection(from, u, v, direction, distance, pdfSolidAngle);

    const float centerDistance = std::sqrt(dist2);
    const Vec3 axis = toCenter / centerDistance;
    const float sinMax2 = radius_ * radius_ / dist2;
    const float cosMax = std::sqrt(std::max(0.0f, 1.0f - sinMax2));
    const float cosTheta = 1.0f - u * (1.0f - cosMax);
    const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
    const float phi = kTwoPi * v;
    Vec3 t, b;
    orthonormalBasis(axis, t, b);
    direction = t * (sinTheta * std::cos(phi)) + b * (sinTheta * std::sin(phi)) + axis * cosTheta;

    // Distance to the near intersection with the sphere along the sampled direction.
    const float proj = centerDistance * cosTheta;
    const float disc = radius_ * radius_ - (dist2 - proj * proj);
    distance = proj - std::sqrt(std::max(0.0f, disc));
    pdfSolidAngle = 1.0f / (kTwoPi * (1.0f - cosMax));
    return distance > 0.0f;
}

// ---------------------------------------------------------------------------------------------
// Quad
// ---------------------------------------------------------------------------------------------

Quad::Quad(const Vec3& corner, const Vec3& u, const Vec3& v, int material)
    : Primitive(material), corner_(corner), u_(u), v_(v) {
    const Vec3 n = cross(u, v);
    area_ = length(n);
    normal_ = n / area_;
    w_ = n / dot(n, n);
    offset_ = dot(normal_, corner);
}

bool Quad::intersect(const Ray& ray, float tMin, float tMax, Hit& hit) const {
    const float denom = dot(normal_, ray.direction);
    if (std::abs(denom) < 1e-9f) return false;
    const float t = (offset_ - dot(normal_, ray.origin)) / denom;
    if (t <= tMin || t >= tMax) return false;
    const Vec3 p = ray.at(t) - corner_;
    const float alpha = dot(w_, cross(p, v_));
    const float beta = dot(w_, cross(u_, p));
    if (alpha < 0.0f || alpha > 1.0f || beta < 0.0f || beta > 1.0f) return false;
    hit.t = t;
    hit.normal = normal_;
    hit.shadingNormal = normal_;
    return true;
}

Aabb Quad::bounds() const {
    Aabb b;
    b.expand(corner_);
    b.expand(corner_ + u_);
    b.expand(corner_ + v_);
    b.expand(corner_ + u_ + v_);
    const Vec3 pad{1e-4f, 1e-4f, 1e-4f};  // flat quads would otherwise have zero thickness
    return {b.lo - pad, b.hi + pad};
}

Vec3 Quad::samplePoint(float u, float v, Vec3& normal) const {
    normal = normal_;
    return corner_ + u_ * u + v_ * v;
}

// ---------------------------------------------------------------------------------------------
// Triangle
// ---------------------------------------------------------------------------------------------

Triangle::Triangle(const Vec3& a, const Vec3& b, const Vec3& c, int material)
    : Primitive(material), a_(a), e1_(b - a), e2_(c - a) {
    normal_ = normalize(cross(e1_, e2_));
    na_ = nb_ = nc_ = normal_;
}

Triangle::Triangle(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& na, const Vec3& nb, const Vec3& nc,
                   int material)
    : Triangle(a, b, c, material) {
    na_ = normalize(na);
    nb_ = normalize(nb);
    nc_ = normalize(nc);
    smooth_ = true;
    if (dot(normal_, na_ + nb_ + nc_) < 0.0f) normal_ = -normal_;  // agree with the shading normals
}

bool Triangle::intersect(const Ray& ray, float tMin, float tMax, Hit& hit) const {
    // Moeller-Trumbore.
    const Vec3 p = cross(ray.direction, e2_);
    const float det = dot(e1_, p);
    if (std::abs(det) < 1e-12f) return false;
    const float inv = 1.0f / det;
    const Vec3 s = ray.origin - a_;
    const float u = dot(s, p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    const Vec3 q = cross(s, e1_);
    const float v = dot(ray.direction, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    const float t = dot(e2_, q) * inv;
    if (t <= tMin || t >= tMax) return false;
    hit.t = t;
    hit.normal = normal_;
    hit.shadingNormal = smooth_ ? normalize(na_ * (1.0f - u - v) + nb_ * u + nc_ * v) : normal_;
    return true;
}

Aabb Triangle::bounds() const {
    Aabb b;
    b.expand(a_);
    b.expand(a_ + e1_);
    b.expand(a_ + e2_);
    const Vec3 pad{1e-4f, 1e-4f, 1e-4f};
    return {b.lo - pad, b.hi + pad};
}

float Triangle::area() const { return 0.5f * length(cross(e1_, e2_)); }

Vec3 Triangle::samplePoint(float u, float v, Vec3& normal) const {
    const float su = std::sqrt(u);
    normal = normal_;
    return a_ + e1_ * (su * (1.0f - v)) + e2_ * (su * v);
}

// ---------------------------------------------------------------------------------------------
// Plane
// ---------------------------------------------------------------------------------------------

bool Plane::intersect(const Ray& ray, float tMin, float tMax, Hit& hit) const {
    const float denom = dot(normal_, ray.direction);
    if (std::abs(denom) < 1e-9f) return false;
    const float t = (offset_ - dot(normal_, ray.origin)) / denom;
    if (t <= tMin || t >= tMax) return false;
    hit.t = t;
    hit.normal = normal_;
    hit.shadingNormal = normal_;
    return true;
}

// ---------------------------------------------------------------------------------------------
// Compound helpers
// ---------------------------------------------------------------------------------------------

void addBox(PrimitiveList& out, const Vec3& center, const Vec3& size, float yaw, int material) {
    const float c = std::cos(yaw), s = std::sin(yaw);
    auto rotate = [c, s](const Vec3& p) { return Vec3{p.x * c + p.z * s, p.y, -p.x * s + p.z * c}; };
    const Vec3 h = size * 0.5f;
    struct Face {
        Vec3 corner, u, v;
    };
    const Face faces[6] = {
        {{h.x, -h.y, -h.z}, {0, size.y, 0}, {0, 0, size.z}},  {{-h.x, -h.y, -h.z}, {0, size.y, 0}, {0, 0, size.z}},
        {{-h.x, h.y, -h.z}, {size.x, 0, 0}, {0, 0, size.z}},  {{-h.x, -h.y, -h.z}, {size.x, 0, 0}, {0, 0, size.z}},
        {{-h.x, -h.y, h.z}, {size.x, 0, 0}, {0, size.y, 0}},  {{-h.x, -h.y, -h.z}, {size.x, 0, 0}, {0, size.y, 0}},
    };
    for (const Face& f : faces) {
        Vec3 u = f.u, v = f.v;
        if (dot(cross(u, v), f.corner + (u + v) * 0.5f) < 0.0f) std::swap(u, v);  // make the normal point outwards
        out.push_back(std::make_unique<Quad>(center + rotate(f.corner), rotate(u), rotate(v), material));
    }
}

void addTorus(PrimitiveList& out, const Vec3& center, float majorRadius, float minorRadius, float tilt, int material,
              int segments, int sides) {
    const float ct = std::cos(tilt), st = std::sin(tilt);
    auto rotate = [ct, st](const Vec3& p) { return Vec3{p.x, p.y * ct - p.z * st, p.y * st + p.z * ct}; };
    auto vertex = [&](int i, int j, Vec3& normal) {
        const float theta = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
        const float phi = kTwoPi * static_cast<float>(j) / static_cast<float>(sides);
        const Vec3 n{std::cos(phi) * std::cos(theta), std::sin(phi), std::cos(phi) * std::sin(theta)};
        const Vec3 ring{majorRadius * std::cos(theta), 0.0f, majorRadius * std::sin(theta)};
        normal = rotate(n);
        return center + rotate(ring + n * minorRadius);
    };
    for (int i = 0; i < segments; ++i) {
        for (int j = 0; j < sides; ++j) {
            Vec3 n00, n10, n01, n11;
            const Vec3 p00 = vertex(i, j, n00), p10 = vertex(i + 1, j, n10);
            const Vec3 p01 = vertex(i, j + 1, n01), p11 = vertex(i + 1, j + 1, n11);
            out.push_back(std::make_unique<Triangle>(p00, p10, p11, n00, n10, n11, material));
            out.push_back(std::make_unique<Triangle>(p00, p11, p01, n00, n11, n01, material));
        }
    }
}

}  // namespace crt::rt
