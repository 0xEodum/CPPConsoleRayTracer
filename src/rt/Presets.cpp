#include "rt/Presets.hpp"

#include <functional>

namespace crt::rt {
namespace {

void daylight(Environment& env, const Vec3& sunDirection) {
    env.zenith = {0.12f, 0.28f, 0.75f};
    env.horizon = {0.55f, 0.65f, 0.82f};
    env.ground = {0.25f, 0.23f, 0.21f};
    env.hasSun = true;
    env.sunDirection = normalize(sunDirection);
    env.sunAngularRadius = radians(1.0f);
    env.sunRadiance = Rgb{1.0f, 0.93f, 0.82f} * 3200.0f;
}

void showcase(Scene& s) {
    daylight(s.environment, {0.6f, 0.7f, 0.4f});
    const int floor = s.addMaterial(Material::checkerboard({0.8f, 0.8f, 0.78f}, {0.18f, 0.2f, 0.24f}, 1.0f));
    const int glass = s.addMaterial(Material::glass(1.5f));
    const int gold = s.addMaterial(Material::metal({1.0f, 0.77f, 0.34f}, 0.04f));
    const int red = s.addMaterial(Material::glossy({0.75f, 0.08f, 0.06f}, 0.02f));
    const int blue = s.addMaterial(Material::diffuse({0.1f, 0.25f, 0.8f}));
    const int mirror = s.addMaterial(Material::metal(Rgb(0.95f)));
    const int copper = s.addMaterial(Material::metal({0.95f, 0.55f, 0.4f}, 0.12f));
    const int water = s.addMaterial(Material::glass(1.33f, {0.8f, 0.95f, 1.0f}));

    s.add(std::make_unique<Plane>(Vec3{0, 0, 0}, Vec3{0, 1, 0}, floor));
    s.add(std::make_unique<Sphere>(Vec3{0.0f, 1.0f, 0.0f}, 1.0f, glass));
    s.add(std::make_unique<Sphere>(Vec3{-2.3f, 0.8f, -0.6f}, 0.8f, gold));
    s.add(std::make_unique<Sphere>(Vec3{2.3f, 0.8f, -0.4f}, 0.8f, red));
    s.add(std::make_unique<Sphere>(Vec3{1.1f, 0.35f, 1.5f}, 0.35f, blue));
    s.add(std::make_unique<Sphere>(Vec3{-1.2f, 0.4f, 1.6f}, 0.4f, mirror));
    s.add(std::make_unique<Sphere>(Vec3{0.1f, 0.25f, 2.3f}, 0.25f, water));
    addTorus(s.primitives(), {-1.3f, 0.95f, -2.6f}, 0.7f, 0.25f, radians(80.0f), copper);
    addBox(s.primitives(), {3.4f, 0.5f, -2.4f}, {1.0f, 1.0f, 1.0f}, radians(30.0f), blue);

    s.camera.position = {0.0f, 1.7f, 6.5f};
    s.camera.verticalFov = 40.0f;
    s.camera.lookAt({0.0f, 0.8f, 0.0f});
}

void cornell(Scene& s) {
    s.environment = Environment{};
    const int white = s.addMaterial(Material::diffuse({0.73f, 0.73f, 0.73f}));
    const int red = s.addMaterial(Material::diffuse({0.65f, 0.05f, 0.05f}));
    const int green = s.addMaterial(Material::diffuse({0.12f, 0.45f, 0.15f}));
    const int light = s.addMaterial(Material::light(Rgb{1.0f, 0.9f, 0.75f} * 18.0f));
    const int glass = s.addMaterial(Material::glass(1.5f));
    const int metal = s.addMaterial(Material::metal(Rgb(0.9f), 0.03f));

    auto quad = [&s](Vec3 corner, Vec3 u, Vec3 v, int m) { s.add(std::make_unique<Quad>(corner, u, v, m)); };
    quad({-1, 0, -1}, {0, 0, 2}, {2, 0, 0}, white);   // floor
    quad({-1, 2, -1}, {2, 0, 0}, {0, 0, 2}, white);   // ceiling
    quad({-1, 0, -1}, {2, 0, 0}, {0, 2, 0}, white);   // back
    quad({-1, 0, -1}, {0, 2, 0}, {0, 0, 2}, red);     // left
    quad({1, 0, -1}, {0, 0, 2}, {0, 2, 0}, green);    // right
    quad({-0.25f, 1.995f, -0.25f}, {0.5f, 0, 0}, {0, 0, 0.4f}, light);  // faces down

    addBox(s.primitives(), {-0.38f, 0.6f, -0.35f}, {0.55f, 1.2f, 0.55f}, radians(18.0f), white);
    addBox(s.primitives(), {0.45f, 0.15f, 0.35f}, {0.5f, 0.3f, 0.5f}, radians(-15.0f), metal);
    s.add(std::make_unique<Sphere>(Vec3{0.45f, 0.58f, 0.35f}, 0.28f, glass));

    s.camera.position = {0.0f, 1.0f, 3.6f};
    s.camera.verticalFov = 38.0f;
    s.camera.lookAt({0.0f, 1.0f, 0.0f});
}

void randomSpheres(Scene& s) {
    daylight(s.environment, {0.3f, 0.9f, 0.5f});
    s.environment.sunRadiance = Rgb{1.0f, 0.95f, 0.88f} * 1800.0f;
    const int ground = s.addMaterial(Material::diffuse({0.5f, 0.5f, 0.5f}));
    s.add(std::make_unique<Plane>(Vec3{0, 0, 0}, Vec3{0, 1, 0}, ground));

    Pcg32 rng(2024);
    const int glass = s.addMaterial(Material::glass(1.5f));
    for (int a = -11; a < 11; ++a) {
        for (int b = -11; b < 11; ++b) {
            const float choose = rng.uniform();
            const Vec3 center{static_cast<float>(a) + 0.9f * rng.uniform(), 0.2f, static_cast<float>(b) + 0.9f * rng.uniform()};
            if (length(center - Vec3{4.0f, 0.2f, 0.0f}) <= 0.9f) continue;
            int m;
            if (choose < 0.7f) {
                m = s.addMaterial(Material::diffuse({rng.uniform() * rng.uniform(), rng.uniform() * rng.uniform(),
                                                     rng.uniform() * rng.uniform()}));
            } else if (choose < 0.9f) {
                m = s.addMaterial(Material::metal({rng.uniform(0.5f, 1.0f), rng.uniform(0.5f, 1.0f), rng.uniform(0.5f, 1.0f)},
                                                  rng.uniform(0.0f, 0.3f)));
            } else {
                m = glass;
            }
            s.add(std::make_unique<Sphere>(center, 0.2f, m));
        }
    }
    s.add(std::make_unique<Sphere>(Vec3{0, 1, 0}, 1.0f, glass));
    s.add(std::make_unique<Sphere>(Vec3{-4, 1, 0}, 1.0f, s.addMaterial(Material::diffuse({0.4f, 0.2f, 0.1f}))));
    s.add(std::make_unique<Sphere>(Vec3{4, 1, 0}, 1.0f, s.addMaterial(Material::metal({0.7f, 0.6f, 0.5f}))));

    s.camera.position = {13.0f, 2.0f, 3.0f};
    s.camera.verticalFov = 22.0f;
    s.camera.lookAt({0.0f, 0.0f, 0.0f});
    s.camera.aperture = 0.1f;
    s.camera.focusDistance = 10.0f;
}

void neon(Scene& s) {
    Environment& env = s.environment;
    env.zenith = {0.002f, 0.003f, 0.01f};
    env.horizon = {0.01f, 0.008f, 0.02f};
    env.ground = {0.0f, 0.0f, 0.0f};
    const int floor = s.addMaterial(Material::glossy({0.04f, 0.04f, 0.05f}, 0.03f));
    const int white = s.addMaterial(Material::diffuse(Rgb(0.8f)));
    const int chrome = s.addMaterial(Material::metal(Rgb(0.9f), 0.0f));
    const int glass = s.addMaterial(Material::glass(1.5f));
    const int pink = s.addMaterial(Material::light(Rgb{1.0f, 0.1f, 0.6f} * 12.0f));
    const int cyan = s.addMaterial(Material::light(Rgb{0.1f, 0.8f, 1.0f} * 12.0f));
    const int amber = s.addMaterial(Material::light(Rgb{1.0f, 0.55f, 0.1f} * 12.0f));

    s.add(std::make_unique<Plane>(Vec3{0, 0, 0}, Vec3{0, 1, 0}, floor));
    s.add(std::make_unique<Sphere>(Vec3{-2.0f, 0.5f, -1.0f}, 0.5f, pink));
    s.add(std::make_unique<Sphere>(Vec3{2.2f, 0.4f, -0.5f}, 0.4f, cyan));
    s.add(std::make_unique<Sphere>(Vec3{0.2f, 2.6f, -2.5f}, 0.3f, amber));
    s.add(std::make_unique<Sphere>(Vec3{0.0f, 0.9f, 0.0f}, 0.9f, chrome));
    s.add(std::make_unique<Sphere>(Vec3{1.2f, 0.45f, 1.3f}, 0.45f, glass));
    addTorus(s.primitives(), {-1.0f, 0.3f, 1.4f}, 0.5f, 0.2f, 0.0f, white);
    addBox(s.primitives(), {0.0f, 1.0f, -3.5f}, {6.0f, 2.0f, 0.2f}, 0.0f, white);

    s.camera.position = {0.0f, 1.2f, 5.5f};
    s.camera.verticalFov = 42.0f;
    s.camera.lookAt({0.0f, 0.7f, 0.0f});
    s.exposure = 0.5f;
}

struct Entry {
    PresetInfo info;
    std::function<void(Scene&)> build;
};

const std::vector<Entry>& entries() {
    static const std::vector<Entry> list = {
        {{"showcase", "Showcase", "Glass, gold, glossy paint and a copper torus in the sun"}, showcase},
        {{"cornell", "Cornell box", "The classic global illumination test scene"}, cornell},
        {{"random", "Random spheres", "The cover of 'Ray Tracing in One Weekend' (~480 objects, BVH)"}, randomSpheres},
        {{"neon", "Neon night", "Glowing emitters reflected in a lacquered floor"}, neon},
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
        if (e.info.id != id) continue;
        Scene scene;
        scene.name = e.info.title;
        scene.description = e.info.description;
        e.build(scene);
        scene.finalize();
        out = std::move(scene);
        return true;
    }
    return false;
}

}  // namespace crt::rt
