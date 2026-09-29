#include "optics/Shape.hpp"

#include <algorithm>
#include <cmath>

namespace crt::optics {
namespace {

constexpr float kMinSize = 1.0f;
constexpr float kMaxSize = 400.0f;

float clampSize(float v) { return std::clamp(v, kMinSize, kMaxSize); }

/// Parametric interval [tIn, tOut] where a line lies inside a convex region, with the outward
/// normals of the boundary at both ends. Intersections of convex regions are convex, so
/// intersecting intervals is all CSG we need for lenses.
struct Interval {
    float tIn = -kInfinity;
    float tOut = kInfinity;
    Vec2 nIn;
    Vec2 nOut;
};

bool diskInterval(Vec2 center, float radius, Vec2 o, Vec2 d, Interval& out) {
    const Vec2 oc = o - center;
    const float b = dot(oc, d);
    const float c = lengthSquared(oc) - radius * radius;
    const float disc = b * b - c;
    if (disc < 0.0f) return false;
    const float s = std::sqrt(disc);
    out.tIn = -b - s;
    out.tOut = -b + s;
    out.nIn = (o + d * out.tIn - center) / radius;
    out.nOut = (o + d * out.tOut - center) / radius;
    return true;
}

Interval intersectIntervals(const Interval& a, const Interval& b) {
    Interval r;
    if (a.tIn > b.tIn) { r.tIn = a.tIn; r.nIn = a.nIn; } else { r.tIn = b.tIn; r.nIn = b.nIn; }
    if (a.tOut < b.tOut) { r.tOut = a.tOut; r.nOut = a.nOut; } else { r.tOut = b.tOut; r.nOut = b.nOut; }
    return r;
}

bool firstHit(const Interval& iv, float tMin, float tMax, float& t, Vec2& n) {
    if (iv.tIn > iv.tOut) return false;
    if (iv.tIn > tMin && iv.tIn < tMax) {
        t = iv.tIn;
        n = iv.nIn;
        return true;
    }
    if (iv.tOut > tMin && iv.tOut < tMax) {
        t = iv.tOut;
        n = iv.nOut;
        return true;
    }
    return false;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Shape
// ---------------------------------------------------------------------------------------------

void Shape::setAngle(float radians) {
    angle_ = wrapAngle(radians);
    cos_ = std::cos(angle_);
    sin_ = std::sin(angle_);
}

Vec2 Shape::toLocal(Vec2 world) const { return directionToLocal(world - position_); }

bool Shape::intersect(const Ray2& ray, float tMin, float tMax, float& t, Vec2& normal) const {
    // Early out against the bounding circle before doing any shape-specific work.
    const float r = boundingRadius();
    const Vec2 oc = ray.origin - position_;
    const float b = dot(oc, ray.direction);
    const float c = lengthSquared(oc) - r * r;
    if ((c > 0.0f && b > 0.0f) || b * b - c < 0.0f) return false;

    Vec2 n;
    if (!intersectLocal(toLocal(ray.origin), directionToLocal(ray.direction), tMin, tMax, t, n)) return false;
    normal = directionToWorld(n);
    return true;
}

float Shape::distance(Vec2 worldPoint) const { return distanceLocal(toLocal(worldPoint)); }

Box2 Shape::bounds() const {
    const float r = boundingRadius();
    return {{position_.x - r, position_.y - r}, {position_.x + r, position_.y + r}};
}

// ---------------------------------------------------------------------------------------------
// Circle
// ---------------------------------------------------------------------------------------------

void CircleShape::scale(float factor) { radius_ = clampSize(radius_ * factor); }

bool CircleShape::setParam(std::string_view name, float value) {
    if (name != "r") return false;
    radius_ = clampSize(value);
    return true;
}

bool CircleShape::intersectLocal(Vec2 o, Vec2 d, float tMin, float tMax, float& t, Vec2& n) const {
    Interval iv;
    return diskInterval({}, radius_, o, d, iv) && firstHit(iv, tMin, tMax, t, n);
}

// ---------------------------------------------------------------------------------------------
// Convex polygons
// ---------------------------------------------------------------------------------------------

bool PolygonShape::setParam(std::string_view name, float value) {
    if (!setDimension(name, value)) return false;
    rebuild();
    return true;
}

void PolygonShape::setVertices(std::vector<Vec2> vertices) {
    Vec2 centroid;
    for (Vec2 v : vertices) centroid += v;
    centroid = centroid / static_cast<float>(vertices.size());

    edges_.clear();
    boundingRadius_ = 0.0f;
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const Vec2 a = vertices[i];
        const Vec2 b = vertices[(i + 1) % vertices.size()];
        Vec2 n = normalize(perpendicular(b - a));
        if (dot(n, a - centroid) < 0.0f) n = -n;
        edges_.push_back({n, dot(n, a)});
        boundingRadius_ = std::max(boundingRadius_, length(a));
    }
}

bool PolygonShape::intersectLocal(Vec2 o, Vec2 d, float tMin, float tMax, float& t, Vec2& n) const {
    // Cyrus-Beck clipping of the line against every edge half-plane.
    Interval iv;
    for (const Edge& e : edges_) {
        const float denom = dot(e.normal, d);
        const float dist = e.offset - dot(e.normal, o);  // > 0 when o is inside this half-plane
        if (std::abs(denom) < 1e-9f) {
            if (dist < 0.0f) return false;
            continue;
        }
        const float te = dist / denom;
        if (denom < 0.0f) {
            if (te > iv.tIn) { iv.tIn = te; iv.nIn = e.normal; }
        } else {
            if (te < iv.tOut) { iv.tOut = te; iv.nOut = e.normal; }
        }
    }
    return firstHit(iv, tMin, tMax, t, n);
}

float PolygonShape::distanceLocal(Vec2 p) const {
    float d = -kInfinity;
    for (const Edge& e : edges_) d = std::max(d, dot(e.normal, p) - e.offset);
    return d;
}

BoxShape::BoxShape(float width, float height) : width_(clampSize(width)), height_(clampSize(height)) { rebuild(); }

void BoxShape::scale(float factor) {
    width_ = clampSize(width_ * factor);
    height_ = clampSize(height_ * factor);
    rebuild();
}

void BoxShape::rebuild() {
    const float hw = 0.5f * width_, hh = 0.5f * height_;
    setVertices({{-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}});
}

bool BoxShape::setDimension(std::string_view name, float value) {
    if (name == "w") width_ = clampSize(value);
    else if (name == "h") height_ = clampSize(value);
    else return false;
    return true;
}

PrismShape::PrismShape(float side) : side_(clampSize(side)) { rebuild(); }

void PrismShape::scale(float factor) {
    side_ = clampSize(side_ * factor);
    rebuild();
}

void PrismShape::rebuild() {
    const float circumradius = side_ / std::sqrt(3.0f);
    std::vector<Vec2> v;
    for (float deg : {-90.0f, 30.0f, 150.0f}) v.push_back(fromAngle(radians(deg)) * circumradius);  // apex up
    setVertices(std::move(v));
}

bool PrismShape::setDimension(std::string_view name, float value) {
    if (name != "side") return false;
    side_ = clampSize(value);
    return true;
}

// ---------------------------------------------------------------------------------------------
// Lens
// ---------------------------------------------------------------------------------------------

LensShape::LensShape(float radius, float thickness) : radius_(radius), thickness_(thickness) { sanitise(); }

void LensShape::sanitise() {
    radius_ = std::clamp(radius_, 2.0f, kMaxSize);
    thickness_ = std::clamp(thickness_, 0.5f, 2.0f * radius_);
}

void LensShape::scale(float factor) {
    radius_ *= factor;
    thickness_ *= factor;
    sanitise();
}

bool LensShape::setParam(std::string_view name, float value) {
    if (name == "r") radius_ = value;
    else if (name == "t") thickness_ = value;
    else return false;
    sanitise();
    return true;
}

bool LensShape::intersectLocal(Vec2 o, Vec2 d, float tMin, float tMax, float& t, Vec2& n) const {
    const float c = centerOffset();
    Interval a, b;
    if (!diskInterval({c, 0.0f}, radius_, o, d, a) || !diskInterval({-c, 0.0f}, radius_, o, d, b)) return false;
    return firstHit(intersectIntervals(a, b), tMin, tMax, t, n);
}

float LensShape::distanceLocal(Vec2 p) const {
    const float c = centerOffset();
    return std::max(length(p - Vec2{c, 0.0f}), length(p - Vec2{-c, 0.0f})) - radius_;
}

float LensShape::boundingRadius() const {
    const float c = centerOffset();
    return std::max(0.5f * thickness_, std::sqrt(std::max(0.0f, radius_ * radius_ - c * c)));
}

// ---------------------------------------------------------------------------------------------
// Wall
// ---------------------------------------------------------------------------------------------

void WallShape::scale(float factor) { length_ = clampSize(length_ * factor); }

bool WallShape::setParam(std::string_view name, float value) {
    if (name != "len") return false;
    length_ = clampSize(value);
    return true;
}

bool WallShape::intersectLocal(Vec2 o, Vec2 d, float tMin, float tMax, float& t, Vec2& n) const {
    if (std::abs(d.y) < 1e-9f) return false;
    const float th = -o.y / d.y;
    if (th <= tMin || th >= tMax) return false;
    if (std::abs(o.x + d.x * th) > 0.5f * length_) return false;
    t = th;
    n = {0.0f, d.y > 0.0f ? -1.0f : 1.0f};
    return true;
}

float WallShape::distanceLocal(Vec2 p) const {
    const float half = 0.5f * length_;
    return length(p - Vec2{std::clamp(p.x, -half, half), 0.0f});
}

// ---------------------------------------------------------------------------------------------
// Arc mirror
// ---------------------------------------------------------------------------------------------

ArcShape::ArcShape(float radius, float spanDegrees)
    : radius_(clampSize(radius)), span_(radians(std::clamp(spanDegrees, 10.0f, 300.0f))) {}

void ArcShape::scale(float factor) { radius_ = clampSize(radius_ * factor); }

std::vector<Param> ArcShape::params() const { return {{"r", radius_}, {"span", degrees(span_)}}; }

bool ArcShape::setParam(std::string_view name, float value) {
    if (name == "r") radius_ = clampSize(value);
    else if (name == "span") span_ = radians(std::clamp(value, 10.0f, 300.0f));
    else return false;
    return true;
}

bool ArcShape::withinSpan(Vec2 v) const { return std::abs(wrapAngle(angleOf(v) - kPi)) <= 0.5f * span_; }

bool ArcShape::intersectLocal(Vec2 o, Vec2 d, float tMin, float tMax, float& t, Vec2& n) const {
    const Vec2 center{radius_, 0.0f};
    Interval iv;
    if (!diskInterval(center, radius_, o, d, iv)) return false;
    for (float root : {iv.tIn, iv.tOut}) {
        if (root <= tMin || root >= tMax) continue;
        const Vec2 v = o + d * root - center;
        if (!withinSpan(v)) continue;
        t = root;
        n = v / radius_;
        if (dot(n, d) > 0.0f) n = -n;
        return true;
    }
    return false;
}

float ArcShape::distanceLocal(Vec2 p) const {
    const Vec2 center{radius_, 0.0f};
    const Vec2 v = p - center;
    if (withinSpan(v)) return std::abs(length(v) - radius_);
    const Vec2 e1 = center + fromAngle(kPi + 0.5f * span_) * radius_;
    const Vec2 e2 = center + fromAngle(kPi - 0.5f * span_) * radius_;
    return std::min(length(p - e1), length(p - e2));
}

float ArcShape::boundingRadius() const { return 2.0f * radius_ * std::sin(0.25f * span_) + 0.01f; }

// ---------------------------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------------------------

const std::vector<std::string_view>& shapeTypes() {
    static const std::vector<std::string_view> types = {"box", "circle", "prism", "lens", "wall", "arc"};
    return types;
}

std::unique_ptr<Shape> makeShape(std::string_view type) {
    if (type == "box") return std::make_unique<BoxShape>();
    if (type == "circle") return std::make_unique<CircleShape>();
    if (type == "prism") return std::make_unique<PrismShape>();
    if (type == "lens") return std::make_unique<LensShape>();
    if (type == "wall") return std::make_unique<WallShape>();
    if (type == "arc") return std::make_unique<ArcShape>();
    return nullptr;
}

}  // namespace crt::optics
