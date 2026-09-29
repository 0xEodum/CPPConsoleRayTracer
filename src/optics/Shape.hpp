#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/Vec2.hpp"

namespace crt::optics {

struct Ray2 {
    Vec2 origin;
    Vec2 direction;  ///< unit length

    Vec2 at(float t) const { return origin + direction * t; }
};

/// Named scalar parameter of an object; used for serialisation and the property panel.
struct Param {
    std::string name;
    float value = 0.0f;
};

/// Base class of all scene geometry.
///
/// Every shape is defined in its own local frame and placed in the world with a rigid transform
/// (position + rotation). The public queries are non-virtual template methods that move the query
/// into the local frame and delegate to the *Local hooks, so concrete shapes only deal with
/// simple canonical geometry.
class Shape {
public:
    virtual ~Shape() = default;

    virtual std::unique_ptr<Shape> clone() const = 0;
    /// Identifier used in scene files: "box", "circle", ...
    virtual std::string_view type() const = 0;
    virtual std::string_view displayName() const = 0;

    /// Uniformly scales the shape's dimensions (clamped to sane limits).
    virtual void scale(float factor) = 0;
    virtual std::vector<Param> params() const = 0;
    /// Returns false if the parameter is unknown for this shape type.
    virtual bool setParam(std::string_view name, float value) = 0;

    /// Solid shapes enclose an area (light can travel inside them); thin ones are curves.
    virtual bool isSolid() const { return true; }

    /// Nearest intersection with t in (tMin, tMax). The returned normal is the geometric outward
    /// normal (for thin shapes: the one facing the incoming ray).
    bool intersect(const Ray2& ray, float tMin, float tMax, float& t, Vec2& normal) const;

    /// Signed distance (negative inside) for solids, unsigned distance for thin shapes.
    /// May be a conservative approximation outside of polygons; exact enough for picking/outlines.
    float distance(Vec2 worldPoint) const;

    bool contains(Vec2 worldPoint) const { return isSolid() && distance(worldPoint) <= 0.0f; }

    /// Conservative world-space bounds.
    Box2 bounds() const;

    Vec2 position() const { return position_; }
    void setPosition(Vec2 p) { position_ = p; }
    /// Rotation in radians; the y axis points down (screen convention).
    float angle() const { return angle_; }
    void setAngle(float radians);
    /// Index into materialCatalogue().
    int material() const { return material_; }
    void setMaterial(int index) { material_ = index; }

protected:
    virtual bool intersectLocal(Vec2 o, Vec2 d, float tMin, float tMax, float& t, Vec2& n) const = 0;
    virtual float distanceLocal(Vec2 p) const = 0;
    virtual float boundingRadius() const = 0;

private:
    Vec2 toLocal(Vec2 world) const;
    Vec2 directionToLocal(Vec2 d) const { return {d.x * cos_ + d.y * sin_, -d.x * sin_ + d.y * cos_}; }
    Vec2 directionToWorld(Vec2 d) const { return {d.x * cos_ - d.y * sin_, d.x * sin_ + d.y * cos_}; }

    Vec2 position_;
    float angle_ = 0.0f;
    float cos_ = 1.0f;  // cached rotation, the hot path must not call trig functions
    float sin_ = 0.0f;
    int material_ = 0;
};

class CircleShape final : public Shape {
public:
    explicit CircleShape(float radius = 8.0f) : radius_(radius) {}

    std::unique_ptr<Shape> clone() const override { return std::make_unique<CircleShape>(*this); }
    std::string_view type() const override { return "circle"; }
    std::string_view displayName() const override { return "Circle"; }
    void scale(float factor) override;
    std::vector<Param> params() const override { return {{"r", radius_}}; }
    bool setParam(std::string_view name, float value) override;

    float radius() const { return radius_; }

protected:
    bool intersectLocal(Vec2 o, Vec2 d, float tMin, float tMax, float& t, Vec2& n) const override;
    float distanceLocal(Vec2 p) const override { return length(p) - radius_; }
    float boundingRadius() const override { return radius_; }

private:
    float radius_;
};

/// Convex polygon described by its vertices (counter-clockwise in a y-up sense is not required;
/// winding is normalised on construction).
class PolygonShape : public Shape {
public:
    bool setParam(std::string_view name, float value) override;

protected:
    void setVertices(std::vector<Vec2> vertices);
    bool intersectLocal(Vec2 o, Vec2 d, float tMin, float tMax, float& t, Vec2& n) const override;
    float distanceLocal(Vec2 p) const override;
    float boundingRadius() const override { return boundingRadius_; }
    virtual void rebuild() = 0;
    virtual bool setDimension(std::string_view name, float value) = 0;

private:
    struct Edge {
        Vec2 normal;   // outward
        float offset;  // dot(normal, p) <= offset inside
    };
    std::vector<Edge> edges_;
    float boundingRadius_ = 0.0f;
};

class BoxShape final : public PolygonShape {
public:
    BoxShape(float width = 16.0f, float height = 10.0f);

    std::unique_ptr<Shape> clone() const override { return std::make_unique<BoxShape>(*this); }
    std::string_view type() const override { return "box"; }
    std::string_view displayName() const override { return "Box"; }
    void scale(float factor) override;
    std::vector<Param> params() const override { return {{"w", width_}, {"h", height_}}; }

protected:
    void rebuild() override;
    bool setDimension(std::string_view name, float value) override;

private:
    float width_;
    float height_;
};

/// Equilateral triangular prism (cross-section), the classic dispersion demo.
class PrismShape final : public PolygonShape {
public:
    explicit PrismShape(float side = 18.0f);

    std::unique_ptr<Shape> clone() const override { return std::make_unique<PrismShape>(*this); }
    std::string_view type() const override { return "prism"; }
    std::string_view displayName() const override { return "Prism"; }
    void scale(float factor) override;
    std::vector<Param> params() const override { return {{"side", side_}}; }

protected:
    void rebuild() override;
    bool setDimension(std::string_view name, float value) override;

private:
    float side_;
};

/// Symmetric biconvex lens: the intersection of two discs of radius R whose centres are placed
/// so that the lens is `thickness` thick along its optical (local x) axis.
class LensShape final : public Shape {
public:
    LensShape(float radius = 24.0f, float thickness = 8.0f);

    std::unique_ptr<Shape> clone() const override { return std::make_unique<LensShape>(*this); }
    std::string_view type() const override { return "lens"; }
    std::string_view displayName() const override { return "Lens"; }
    void scale(float factor) override;
    std::vector<Param> params() const override { return {{"r", radius_}, {"t", thickness_}}; }
    bool setParam(std::string_view name, float value) override;

protected:
    bool intersectLocal(Vec2 o, Vec2 d, float tMin, float tMax, float& t, Vec2& n) const override;
    float distanceLocal(Vec2 p) const override;
    float boundingRadius() const override;

private:
    void sanitise();
    float centerOffset() const { return radius_ - 0.5f * thickness_; }

    float radius_;
    float thickness_;
};

/// Thin straight wall (two-sided), e.g. a flat mirror or a screen.
class WallShape final : public Shape {
public:
    explicit WallShape(float length = 24.0f) : length_(length) {}

    std::unique_ptr<Shape> clone() const override { return std::make_unique<WallShape>(*this); }
    std::string_view type() const override { return "wall"; }
    std::string_view displayName() const override { return "Wall"; }
    void scale(float factor) override;
    std::vector<Param> params() const override { return {{"len", length_}}; }
    bool setParam(std::string_view name, float value) override;
    bool isSolid() const override { return false; }

protected:
    bool intersectLocal(Vec2 o, Vec2 d, float tMin, float tMax, float& t, Vec2& n) const override;
    float distanceLocal(Vec2 p) const override;
    float boundingRadius() const override { return 0.5f * length_; }

private:
    float length_;
};

/// Thin circular arc (two-sided): a curved mirror that focuses parallel light.
/// The arc's midpoint sits at the local origin and its concave side faces local +x.
class ArcShape final : public Shape {
public:
    ArcShape(float radius = 40.0f, float spanDegrees = 70.0f);

    std::unique_ptr<Shape> clone() const override { return std::make_unique<ArcShape>(*this); }
    std::string_view type() const override { return "arc"; }
    std::string_view displayName() const override { return "Arc mirror"; }
    void scale(float factor) override;
    std::vector<Param> params() const override;
    bool setParam(std::string_view name, float value) override;
    bool isSolid() const override { return false; }

protected:
    bool intersectLocal(Vec2 o, Vec2 d, float tMin, float tMax, float& t, Vec2& n) const override;
    float distanceLocal(Vec2 p) const override;
    float boundingRadius() const override;

private:
    bool withinSpan(Vec2 localFromCenter) const;

    float radius_;
    float span_;  // radians
};

/// Factory: creates a default-sized shape from its type identifier (nullptr if unknown).
std::unique_ptr<Shape> makeShape(std::string_view type);

/// All shape type identifiers, in tool order.
const std::vector<std::string_view>& shapeTypes();

}  // namespace crt::optics
