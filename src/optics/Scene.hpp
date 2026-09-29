#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "optics/Light.hpp"
#include "optics/Shape.hpp"

namespace crt::optics {

/// Handle to an object inside a Scene (a shape or a light).
struct Selection {
    enum class Kind { None, Shape, Light };

    Kind kind = Kind::None;
    std::size_t index = 0;

    static Selection none() { return {}; }
    static Selection shape(std::size_t i) { return {Kind::Shape, i}; }
    static Selection light(std::size_t i) { return {Kind::Light, i}; }

    bool isShape() const { return kind == Kind::Shape; }
    bool isLight() const { return kind == Kind::Light; }
    explicit operator bool() const { return kind != Kind::None; }
    bool operator==(const Selection& o) const { return kind == o.kind && (kind == Kind::None || index == o.index); }
    bool operator!=(const Selection& o) const { return !(*this == o); }
};

struct SurfaceHit {
    float t = kInfinity;
    Vec2 point;
    Vec2 normal;  ///< geometric outward normal
    const Shape* shape = nullptr;
};

/// The 2D optics scene: a rectangular world (closed by absorbing walls) containing shapes and
/// lights. Value semantics: copying a Scene deep-copies all objects, which is what the editor's
/// undo history relies on.
class Scene {
public:
    static constexpr float kDefaultWidth = 160.0f;
    static constexpr float kDefaultHeight = 100.0f;

    Scene() = default;
    Scene(const Scene& other);
    Scene& operator=(const Scene& other);
    Scene(Scene&&) noexcept = default;
    Scene& operator=(Scene&&) noexcept = default;
    ~Scene() = default;

    const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    /// Exposure compensation in EV stops, applied on top of automatic exposure.
    float exposure() const { return exposure_; }
    void setExposure(float ev) { exposure_ = ev; }

    Box2 bounds() const { return {{0.0f, 0.0f}, {width_, height_}}; }
    float width() const { return width_; }
    float height() const { return height_; }
    Vec2 clampToBounds(Vec2 p) const;

    const std::vector<std::unique_ptr<Shape>>& shapes() const { return shapes_; }
    const std::vector<std::unique_ptr<Light>>& lights() const { return lights_; }

    Shape* shape(Selection s);
    const Shape* shape(Selection s) const;
    Light* light(Selection s);
    const Light* light(Selection s) const;
    bool isValid(Selection s) const;

    Selection add(std::unique_ptr<Shape> shape);
    Selection add(std::unique_ptr<Light> light);
    void remove(Selection s);
    Selection duplicate(Selection s, Vec2 offset);
    void clear();

    /// Topmost object under `p`: lights first (within `tolerance`), then shapes, newest first.
    Selection pick(Vec2 p, float tolerance) const;

    /// Nearest shape intersection along the ray.
    bool intersect(const Ray2& ray, float tMin, float tMax, SurfaceHit& hit) const;

    float totalPower() const;

    /// Text serialisation (see docs/scene-format.md).
    std::string serialize() const;
    static bool parse(std::string_view text, Scene& out, std::string& error);
    bool saveToFile(const std::string& path, std::string& error) const;
    static bool loadFromFile(const std::string& path, Scene& out, std::string& error);

private:
    std::string name_ = "Untitled";
    float exposure_ = 0.0f;
    float width_ = kDefaultWidth;
    float height_ = kDefaultHeight;
    std::vector<std::unique_ptr<Shape>> shapes_;
    std::vector<std::unique_ptr<Light>> lights_;
};

}  // namespace crt::optics
