#include "TestFramework.hpp"
#include "core/Spectrum.hpp"
#include "optics/LightTracer.hpp"
#include "optics/Material.hpp"
#include "optics/Presets.hpp"
#include "optics/Scene.hpp"

using namespace crt;
using namespace crt::optics;

namespace {

bool hitShape(const Shape& s, Vec2 origin, Vec2 dir, float& t, Vec2& n) {
    return s.intersect({origin, normalize(dir)}, 1e-4f, 1e6f, t, n);
}

}  // namespace

TEST_CASE("optics: Fresnel reflectance of glass") {
    float cosT = 0.0f;
    CHECK_NEAR(fresnelDielectric(1.0f, 1.0f / 1.5f, cosT), 0.04, 1e-4);  // normal incidence
    CHECK_NEAR(cosT, 1.0, 1e-6);
    // Brewster angle: p-polarised reflectance vanishes, total is rs^2 / 2.
    const float brewster = std::atan(1.5f);
    const float r = fresnelDielectric(std::cos(brewster), 1.0f / 1.5f, cosT);
    CHECK(r > 0.03f && r < 0.08f);
    // Grazing incidence reflects (almost) everything.
    CHECK(fresnelDielectric(0.001f, 1.0f / 1.5f, cosT) > 0.99f);
    // Leaving glass beyond the critical angle (41.8 deg) is total internal reflection.
    CHECK(fresnelDielectric(std::cos(radians(45.0f)), 1.5f, cosT) == 1.0f);
    CHECK(fresnelDielectric(std::cos(radians(30.0f)), 1.5f, cosT) < 1.0f);
}

TEST_CASE("optics: refraction obeys Snell's law") {
    Material glass = material(findMaterial("glass"));
    glass.cauchyB = 0.0f;  // no dispersion: n = A exactly
    Pcg32 rng(1);
    const float incidence = radians(35.0f);
    const Vec2 dir{std::sin(incidence), std::cos(incidence)};  // travelling down (+y)
    const Vec2 normal{0.0f, -1.0f};                             // surface facing up
    int refracted = 0;
    for (int i = 0; i < 200; ++i) {
        const Interaction it = interact(glass, dir, normal, 550.0f, rng);
        CHECK(it.alive);
        if (!it.transmitted) continue;
        ++refracted;
        const float sinT = it.direction.x;
        CHECK_NEAR(std::sin(incidence), glass.cauchyA * sinT, 1e-4);
        CHECK(it.direction.y > 0.0f);
    }
    CHECK(refracted > 150);  // ~96% transmission
}

TEST_CASE("optics: diffuse and absorbing surfaces") {
    Pcg32 rng(9);
    const Material& matte = material(findMaterial("matte"));
    for (int i = 0; i < 500; ++i) {
        const Interaction it = interact(matte, {0.0f, 1.0f}, {0.0f, -1.0f}, 550.0f, rng);
        CHECK(it.alive && it.diffuse);
        CHECK(it.direction.y <= 0.0f);  // back into the upper half-plane
        CHECK_NEAR(it.weight, 0.8, 1e-4);
    }
    CHECK(!interact(material(findMaterial("absorber")), {0.0f, 1.0f}, {0.0f, -1.0f}, 550.0f, rng).alive);
    const Interaction mirror = interact(material(findMaterial("mirror")), normalize(Vec2{1.0f, 1.0f}), {0.0f, -1.0f}, 550.0f, rng);
    CHECK_NEAR(mirror.direction.x, std::sqrt(0.5), 1e-5);
    CHECK_NEAR(mirror.direction.y, -std::sqrt(0.5), 1e-5);
}

TEST_CASE("optics: circle, box and prism intersections") {
    CircleShape circle(5.0f);
    circle.setPosition({10.0f, 0.0f});
    float t;
    Vec2 n;
    REQUIRE(hitShape(circle, {0.0f, 0.0f}, {1.0f, 0.0f}, t, n));
    CHECK_NEAR(t, 5.0, 1e-4);
    CHECK_NEAR(n.x, -1.0, 1e-5);
    REQUIRE(hitShape(circle, {10.0f, 0.0f}, {0.0f, 1.0f}, t, n));  // from inside
    CHECK_NEAR(t, 5.0, 1e-4);
    CHECK_NEAR(n.y, 1.0, 1e-5);
    CHECK(!hitShape(circle, {0.0f, 6.0f}, {1.0f, 0.0f}, t, n));
    CHECK(circle.contains({12.0f, 1.0f}));
    CHECK(!circle.contains({16.0f, 0.0f}));

    BoxShape box(4.0f, 2.0f);
    box.setPosition({0.0f, 0.0f});
    box.setAngle(radians(90.0f));  // now 2 wide, 4 tall
    REQUIRE(hitShape(box, {-10.0f, 0.0f}, {1.0f, 0.0f}, t, n));
    CHECK_NEAR(t, 9.0, 1e-4);
    CHECK_NEAR(n.x, -1.0, 1e-5);
    REQUIRE(hitShape(box, {0.0f, -10.0f}, {0.0f, 1.0f}, t, n));
    CHECK_NEAR(t, 8.0, 1e-4);
    CHECK(box.distance({0.0f, 0.0f}) < 0.0f);
    CHECK_NEAR(box.distance({3.0f, 0.0f}), 2.0, 1e-4);

    PrismShape prism(10.0f);
    const float inradius = 10.0f / (2.0f * std::sqrt(3.0f));
    REQUIRE(hitShape(prism, {0.0f, 20.0f}, {0.0f, -1.0f}, t, n));  // hits the flat bottom edge
    CHECK_NEAR(t, 20.0f - inradius, 1e-3);
    CHECK_NEAR(n.y, 1.0, 1e-5);
}

TEST_CASE("optics: lens, wall and arc intersections") {
    LensShape lens(20.0f, 6.0f);
    float t;
    Vec2 n;
    REQUIRE(hitShape(lens, {-30.0f, 0.0f}, {1.0f, 0.0f}, t, n));
    CHECK_NEAR(t, 27.0, 1e-3);  // thickness 6 => surface at x = -3
    CHECK_NEAR(n.x, -1.0, 1e-4);
    REQUIRE(hitShape(lens, {0.0f, 0.0f}, {1.0f, 0.0f}, t, n));  // exit through the far side
    CHECK_NEAR(t, 3.0, 1e-3);
    const float halfHeight = std::sqrt(20.0f * 20.0f - 17.0f * 17.0f);
    CHECK(!hitShape(lens, {-30.0f, halfHeight + 0.5f}, {1.0f, 0.0f}, t, n));

    WallShape wall(10.0f);
    wall.setPosition({0.0f, 5.0f});
    REQUIRE(hitShape(wall, {2.0f, 0.0f}, {0.0f, 1.0f}, t, n));
    CHECK_NEAR(t, 5.0, 1e-4);
    CHECK_NEAR(n.y, -1.0, 1e-5);  // faces the incoming ray
    CHECK(!hitShape(wall, {6.0f, 0.0f}, {0.0f, 1.0f}, t, n));
    CHECK(!wall.isSolid());
    CHECK_NEAR(wall.distance({0.0f, 7.0f}), 2.0, 1e-4);

    ArcShape arc(10.0f, 90.0f);  // midpoint at the origin, concave side facing +x
    REQUIRE(hitShape(arc, {5.0f, 0.0f}, {-1.0f, 0.0f}, t, n));
    CHECK_NEAR(t, 5.0, 1e-4);
    CHECK_NEAR(n.x, 1.0, 1e-4);
    CHECK(!hitShape(arc, {30.0f, 9.0f}, {-1.0f, 0.0f}, t, n));  // circle is hit, but outside the 90 degree span
}

TEST_CASE("optics: picking prefers lights and topmost shapes") {
    Scene scene;
    auto a = std::make_unique<CircleShape>(10.0f);
    a->setPosition({50.0f, 50.0f});
    auto b = std::make_unique<BoxShape>(6.0f, 6.0f);
    b->setPosition({52.0f, 50.0f});
    scene.add(std::move(a));
    const Selection topmost = scene.add(std::move(b));
    auto light = makeLight("point");
    light->position = {45.0f, 50.0f};
    const Selection lightSel = scene.add(std::move(light));

    CHECK(scene.pick({52.0f, 50.0f}, 1.0f) == topmost);
    CHECK(scene.pick({45.2f, 50.0f}, 1.0f) == lightSel);
    CHECK(scene.pick({43.0f, 50.0f}, 1.0f) == Selection::shape(0));
    CHECK(!scene.pick({5.0f, 5.0f}, 1.0f));

    Scene copy = scene;  // deep copy
    copy.shape(topmost)->setPosition({0.0f, 0.0f});
    CHECK(scene.shape(topmost)->position().x == 52.0f);
    scene.remove(Selection::shape(0));
    CHECK(scene.shapes().size() == 1);
}

TEST_CASE("optics: scene files round-trip") {
    for (const auto& preset : presets()) {
        Scene scene;
        REQUIRE(makePreset(preset.id, scene));
        const std::string text = scene.serialize();
        Scene parsed;
        std::string error;
        REQUIRE(Scene::parse(text, parsed, error));
        CHECK(parsed.serialize() == text);
        CHECK(parsed.shapes().size() == scene.shapes().size());
        CHECK(parsed.lights().size() == scene.lights().size());
    }
}

TEST_CASE("optics: scene parser reports errors with line numbers") {
    Scene scene;
    std::string error;
    CHECK(!Scene::parse("scene name=x\nshape blob pos=1,2\n", scene, error));
    CHECK(error.find("line 2") != std::string::npos);
    CHECK(!Scene::parse("light point pos=1\n", scene, error));
    CHECK(!Scene::parse("shape box material=unobtainium\n", scene, error));
    CHECK(error.find("unobtainium") != std::string::npos);
    CHECK(Scene::parse("# comment only\n\nshape circle pos=10,10 r=3 # trailing\n", scene, error));
    CHECK(scene.shapes().size() == 1);
}

TEST_CASE("optics: ray splatting conserves energy for any slope") {
    // A monochromatic laser through an empty world deposits power x path length, whatever the angle.
    for (float angleDeg : {0.0f, 17.0f, 45.0f, 63.0f, 90.0f, 200.0f}) {
        Scene scene;
        auto laser = makeLight("laser");
        laser->position = {80.0f, 50.0f};
        laser->angle = radians(angleDeg);
        laser->color = findLightColor("green");
        laser->power = 1.0f;
        scene.add(std::move(laser));

        LightTracer tracer;
        tracer.configure(160, 100, ViewTransform::fit(scene.bounds(), 160, 100, 1.0f));
        ThreadPool pool(2);
        tracer.trace(scene, pool, 64);
        ImageF image;
        tracer.resolve(image);
        double sum = 0.0;
        for (std::size_t i = 0; i < image.size(); ++i) sum += image.data()[i].g;

        // Distance from the laser to the world border along the beam.
        const Vec2 d = fromAngle(radians(angleDeg));
        float length = kInfinity;
        if (d.x > 1e-6f) length = std::min(length, (160.0f - 80.0f) / d.x);
        if (d.x < -1e-6f) length = std::min(length, -80.0f / d.x);
        if (d.y > 1e-6f) length = std::min(length, (100.0f - 50.0f) / d.y);
        if (d.y < -1e-6f) length = std::min(length, -50.0f / d.y);
        const double expected = length * spectrum::wavelengthToRgb(532.0f).g;
        CHECK_NEAR(sum / expected, 1.0, 0.03);  // within a pixel of the ends
    }
}

TEST_CASE("optics: light tracer is deterministic across thread counts in total energy") {
    Scene scene;
    REQUIRE(makePreset("classic", scene));
    auto total = [&](std::size_t threads) {
        LightTracer tracer;
        tracer.configure(80, 50, ViewTransform::fit(scene.bounds(), 80, 50, 1.0f));
        ThreadPool pool(threads);
        tracer.trace(scene, pool, 20000);
        ImageF image;
        tracer.resolve(image);
        double sum = 0.0;
        for (std::size_t i = 0; i < image.size(); ++i) sum += image.data()[i].luminance();
        return sum;
    };
    CHECK_NEAR(total(1) / total(4), 1.0, 1e-4);
}
