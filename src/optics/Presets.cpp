#include "optics/Presets.hpp"

#include <functional>

#include "optics/Material.hpp"

namespace crt::optics {
namespace {

/// Small builder DSL that keeps the preset definitions readable.
class SceneBuilder {
public:
    explicit SceneBuilder(Scene& scene) : scene_(scene) {}

    template <class T>
    T& shape(std::unique_ptr<T> s, Vec2 pos, float angleDeg, std::string_view materialId) {
        T& ref = *s;
        s->setPosition(pos);
        s->setAngle(radians(angleDeg));
        s->setMaterial(findMaterial(materialId));
        scene_.add(std::move(s));
        return ref;
    }

    template <class T>
    T& light(std::unique_ptr<T> l, Vec2 pos, float angleDeg, std::string_view colorId, float power = 1.0f) {
        T& ref = *l;
        l->position = pos;
        l->angle = radians(angleDeg);
        l->color = findLightColor(colorId);
        l->power = power;
        scene_.add(std::move(l));
        return ref;
    }

    void box(Vec2 pos, float w, float h, float angleDeg, std::string_view m) {
        shape(std::make_unique<BoxShape>(w, h), pos, angleDeg, m);
    }
    void wall(Vec2 pos, float len, float angleDeg, std::string_view m) {
        shape(std::make_unique<WallShape>(len), pos, angleDeg, m);
    }

private:
    Scene& scene_;
};

void dispersion(Scene& s) {
    s.setName("Dispersion");
    SceneBuilder b(s);
    // Incoming beam tilted by half the minimum deviation, so it crosses the prism symmetrically.
    auto& beam = b.light(std::make_unique<BeamLight>(), {18, 69}, -28, "white", 1.0f);
    beam.width = 3.0f;
    b.shape(std::make_unique<PrismShape>(34.0f), {72, 50}, 0, "prism");
    b.wall({142, 78}, 44, 90, "matte");
}

void lenses(Scene& s) {
    s.setName("Lenses");
    s.setExposure(-1.0f);
    SceneBuilder b(s);
    auto& beam = b.light(std::make_unique<BeamLight>(), {8, 50}, 0, "white", 1.2f);
    beam.width = 34.0f;
    b.shape(std::make_unique<LensShape>(48.0f, 13.0f), {40, 50}, 0, "glass");
    b.shape(std::make_unique<LensShape>(20.0f, 9.0f), {104, 50}, 0, "glass");
    b.shape(std::make_unique<ArcShape>(60.0f, 60.0f), {152, 50}, 180, "mirror");
}

void mirrors(Scene& s) {
    s.setName("Mirrors & lasers");
    SceneBuilder b(s);
    b.light(std::make_unique<LaserLight>(), {10, 90}, -35, "red", 0.6f);
    b.light(std::make_unique<LaserLight>(), {10, 10}, 25, "green", 0.6f);
    b.light(std::make_unique<LaserLight>(), {150, 92}, -150, "blue", 0.6f);
    b.wall({60, 55}, 22, 60, "mirror");
    b.wall({100, 30}, 22, -30, "mirror");
    b.wall({125, 70}, 26, 20, "gold");
    b.box({80, 80}, 26, 6, -10, "brushed");
    b.shape(std::make_unique<CircleShape>(9.0f), {95, 55}, 0, "diamond");
    auto& spot = b.light(std::make_unique<SpotLight>(), {150, 8}, 150, "daylight", 0.5f);
    spot.spread = radians(30.0f);
    b.box({30, 30}, 12, 12, 45, "matte-blue");
}

void room(Scene& s) {
    s.setName("Room with caustics");
    s.setExposure(-1.5f);
    SceneBuilder b(s);
    b.wall({4, 50}, 92, 90, "matte-red");
    b.wall({156, 50}, 92, 90, "matte-green");
    b.wall({80, 96}, 152, 0, "matte");
    b.wall({80, 4}, 152, 0, "matte");
    b.light(std::make_unique<PointLight>(), {80, 18}, 0, "white", 1.0f);
    b.shape(std::make_unique<CircleShape>(13.0f), {56, 70}, 0, "glass");
    b.shape(std::make_unique<PrismShape>(20.0f), {112, 75}, 0, "diamond");
    b.box({84, 84}, 8, 16, 15, "mirror");
}

void classic(Scene& s) {
    // A tribute to the original 2020 version: the same four materials, three light types.
    s.setName("Classic (v1 tribute)");
    SceneBuilder b(s);
    b.box({50, 30}, 20, 12, 0, "mirror");
    b.box({110, 30}, 20, 12, 0, "absorber");
    b.box({50, 72}, 20, 12, 0, "matte");
    b.box({110, 72}, 20, 12, 0, "prism");
    b.light(std::make_unique<PointLight>(), {80, 50}, 0, "white", 1.0f);
    auto& spot = b.light(std::make_unique<SpotLight>(), {12, 50}, 0, "daylight", 0.6f);
    spot.spread = radians(45.0f);
    b.light(std::make_unique<LaserLight>(), {150, 92}, -140, "red", 0.4f);
}

void fiber(Scene& s) {
    s.setName("Total internal reflection");
    SceneBuilder b(s);
    b.box({86, 48}, 130, 7, -6, "glass");
    b.light(std::make_unique<LaserLight>(), {10, 66}, -25, "green", 0.8f);
    b.light(std::make_unique<LaserLight>(), {10, 36}, 12, "red", 0.8f);
    b.shape(std::make_unique<CircleShape>(10.0f), {60, 82}, 0, "water");
    b.light(std::make_unique<LaserLight>(), {28, 95}, -20, "violet", 0.8f);
}

struct Entry {
    PresetInfo info;
    std::function<void(Scene&)> build;
};

const std::vector<Entry>& entries() {
    static const std::vector<Entry> list = {
        {{"dispersion", "Dispersion", "White light split into a spectrum by a prism"}, dispersion},
        {{"lenses", "Lenses", "Collimated beam focused by lenses and a curved mirror"}, lenses},
        {{"mirrors", "Mirrors & lasers", "Lasers bouncing off flat, gold and brushed mirrors"}, mirrors},
        {{"room", "Room with caustics", "Diffuse coloured walls, glass ball and diamond"}, room},
        {{"fiber", "Total internal reflection", "Light trapped inside a glass slab"}, fiber},
        {{"classic", "Classic (v1 tribute)", "The four original materials and three light types"}, classic},
    };
    return list;
}

}  // namespace

const std::vector<PresetInfo>& presets() {
    static const std::vector<PresetInfo> infos = [] {
        std::vector<PresetInfo> v;
        for (const auto& e : entries()) v.push_back(e.info);
        return v;
    }();
    return infos;
}

bool makePreset(std::string_view id, Scene& out) {
    for (const auto& e : entries()) {
        if (e.info.id == id) {
            Scene scene;
            e.build(scene);
            out = std::move(scene);
            return true;
        }
    }
    return false;
}

}  // namespace crt::optics
