#include "TestFramework.hpp"
#include "rt/PathTracer.hpp"
#include "rt/Presets.hpp"

using namespace crt;
using namespace crt::rt;

namespace {

Environment uniformSky(float radiance) {
    Environment env;
    env.zenith = env.horizon = env.ground = Rgb(radiance);
    return env;
}

/// Averages the radiance of many camera rays aimed at the centre of the image.
Rgb averageCenter(const Scene& scene, const Camera& camera, int samples, int maxDepth = 64) {
    PathTracer tracer;
    tracer.settings().maxDepth = maxDepth;
    tracer.settings().clampLuminance = 0.0f;
    Rgb sum;
    for (int i = 0; i < samples; ++i) {
        Pcg32 rng(static_cast<std::uint64_t>(i) * 7919u + 1u);
        const Ray ray = camera.generateRay(16.0f + rng.uniform(), 16.0f + rng.uniform(), 32, 32, 1.0f, rng);
        sum += tracer.trace(scene, ray, rng);
    }
    return sum / static_cast<float>(samples);
}

}  // namespace

TEST_CASE("rt: sphere, quad and triangle intersection") {
    Sphere sphere({0, 0, -5}, 1.0f, 0);
    Hit hit;
    REQUIRE(sphere.intersect({{0, 0, 0}, {0, 0, -1}}, 1e-4f, 1e9f, hit));
    CHECK_NEAR(hit.t, 4.0, 1e-4);
    CHECK_NEAR(hit.normal.z, 1.0, 1e-5);
    CHECK(!sphere.intersect({{0, 2, 0}, {0, 0, -1}}, 1e-4f, 1e9f, hit));
    REQUIRE(sphere.intersect({{0, 0, -5}, {1, 0, 0}}, 1e-4f, 1e9f, hit));  // from inside
    CHECK_NEAR(hit.t, 1.0, 1e-4);

    Quad quad({-1, -1, -3}, {2, 0, 0}, {0, 2, 0}, 0);
    REQUIRE(quad.intersect({{0.5f, 0.5f, 0}, {0, 0, -1}}, 1e-4f, 1e9f, hit));
    CHECK_NEAR(hit.t, 3.0, 1e-4);
    CHECK(!quad.intersect({{1.5f, 0, 0}, {0, 0, -1}}, 1e-4f, 1e9f, hit));
    CHECK_NEAR(quad.area(), 4.0, 1e-5);

    Triangle tri({0, 0, -2}, {1, 0, -2}, {0, 1, -2}, 0);
    REQUIRE(tri.intersect({{0.2f, 0.2f, 0}, {0, 0, -1}}, 1e-4f, 1e9f, hit));
    CHECK_NEAR(hit.t, 2.0, 1e-4);
    CHECK(!tri.intersect({{0.8f, 0.8f, 0}, {0, 0, -1}}, 1e-4f, 1e9f, hit));
    CHECK_NEAR(tri.area(), 0.5, 1e-5);
}

TEST_CASE("rt: area sampling stays on the surface") {
    Sphere sphere({1, 2, 3}, 2.0f, 0);
    Quad quad({0, 0, 0}, {3, 0, 0}, {0, 0, 2}, 0);
    Pcg32 rng(5);
    for (int i = 0; i < 200; ++i) {
        Vec3 n;
        const Vec3 p = sphere.samplePoint(rng.uniform(), rng.uniform(), n);
        CHECK_NEAR(length(p - Vec3{1, 2, 3}), 2.0, 1e-4);
        CHECK_NEAR(dot(n, normalize(p - Vec3{1, 2, 3})), 1.0, 1e-4);
        const Vec3 q = quad.samplePoint(rng.uniform(), rng.uniform(), n);
        CHECK(q.x >= 0.0f && q.x <= 3.0f && q.z >= 0.0f && q.z <= 2.0f && q.y == 0.0f);

        // Cone sampling returns directions that really hit the sphere.
        Vec3 dir;
        float dist = 0.0f, pdf = 0.0f;
        REQUIRE(sphere.sampleDirection({1, 2, 10}, rng.uniform(), rng.uniform(), dir, dist, pdf));
        Hit hit;
        REQUIRE(sphere.intersect({{1, 2, 10}, dir}, 1e-4f, 1e9f, hit));
        CHECK_NEAR(hit.t, dist, 1e-3);
        CHECK(pdf > 0.0f);
    }
}

TEST_CASE("rt: BVH agrees with brute force") {
    Scene scene;
    const int m = scene.addMaterial(Material::diffuse(Rgb(0.5f)));
    Pcg32 rng(11);
    std::vector<const Primitive*> all;
    for (int i = 0; i < 300; ++i) {
        const Vec3 c{rng.uniform(-10, 10), rng.uniform(-10, 10), rng.uniform(-10, 10)};
        if (i % 3 == 0) scene.add(std::make_unique<Sphere>(c, rng.uniform(0.1f, 1.0f), m));
        else if (i % 3 == 1) scene.add(std::make_unique<Triangle>(c, c + Vec3{1, 0, 0}, c + Vec3{0, 1, 0.3f}, m));
        else scene.add(std::make_unique<Quad>(c, Vec3{0.8f, 0, 0}, Vec3{0, 0, 0.8f}, m));
    }
    for (const auto& p : scene.primitives()) all.push_back(p.get());
    scene.finalize();
    CHECK(scene.bvh().nodeCount() > 1);

    int agreements = 0;
    for (int i = 0; i < 2000; ++i) {
        const Vec3 origin{rng.uniform(-15, 15), rng.uniform(-15, 15), rng.uniform(-15, 15)};
        const Vec3 dir = normalize(Vec3{rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1)});
        const Ray ray{origin, dir};
        Hit bvhHit;
        const bool a = scene.intersect(ray, kInfinity, bvhHit);
        Hit bruteHit;
        bool b = false;
        float closest = kInfinity;
        for (const Primitive* p : all) {
            Hit h;
            if (p->intersect(ray, 1e-4f, closest, h)) {
                closest = h.t;
                bruteHit = h;
                b = true;
            }
        }
        CHECK(a == b);
        if (a && b) {
            CHECK_NEAR(bvhHit.t, bruteHit.t, 1e-4);
            CHECK(scene.occluded(ray, bvhHit.t + 1.0f));
            CHECK(!scene.occluded(ray, bvhHit.t * 0.5f) || bvhHit.t * 0.5f < 1e-3f);
        }
        agreements += a == b;
    }
    CHECK(agreements == 2000);
}

TEST_CASE("rt: camera basis and look-at") {
    Camera cam;
    cam.position = {0, 0, 0};
    cam.lookAt({3, 4, -5});
    const Vec3 f = cam.forward(), r = cam.right(), u = cam.up();
    CHECK_NEAR(length(f), 1.0, 1e-5);
    CHECK_NEAR(dot(f, r), 0.0, 1e-5);
    CHECK_NEAR(dot(f, u), 0.0, 1e-5);
    CHECK_NEAR(dot(f, normalize(Vec3{3, 4, -5})), 1.0, 1e-5);
    CHECK(u.y > 0.0f);
    Pcg32 rng;
    const Ray center = cam.generateRay(50, 50, 100, 100, 1.0f, rng);
    CHECK_NEAR(dot(center.direction, f), 1.0, 1e-5);
}

TEST_CASE("rt: white furnace - diffuse sphere reflects albedo times sky") {
    // A convex object in a uniform environment of radiance 1: every path escapes after one
    // bounce, so the expected radiance is exactly the albedo.
    Scene scene;
    scene.environment = uniformSky(1.0f);
    scene.add(std::make_unique<Sphere>(Vec3{0, 0, -3}, 1.0f, scene.addMaterial(Material::diffuse(Rgb(0.5f)))));
    scene.finalize();
    Camera cam;
    cam.position = {0, 0, 0};
    cam.verticalFov = 5.0f;
    const Rgb l = averageCenter(scene, cam, 4000);
    CHECK_NEAR(l.r, 0.5, 0.02);
    CHECK_NEAR(l.g, 0.5, 0.02);
}

TEST_CASE("rt: white furnace - glass neither creates nor destroys energy") {
    Scene scene;
    scene.environment = uniformSky(1.0f);
    scene.add(std::make_unique<Sphere>(Vec3{0, 0, -3}, 1.0f, scene.addMaterial(Material::glass(1.5f))));
    scene.add(std::make_unique<Sphere>(Vec3{0.3f, 0, -6}, 1.0f, scene.addMaterial(Material::metal(Rgb(1.0f)))));
    scene.finalize();
    Camera cam;
    cam.position = {0, 0, 0};
    cam.verticalFov = 10.0f;
    const Rgb l = averageCenter(scene, cam, 3000);
    CHECK_NEAR(l.r, 1.0, 0.02);
    CHECK_NEAR(l.b, 1.0, 0.02);
}

TEST_CASE("rt: next event estimation matches the analytic irradiance") {
    // A small emitting sphere above a large diffuse floor: the radiance of the floor point below it
    // is albedo/pi * E, with E = L * pi * (r/d)^2 for a spherical source (exact for a sphere).
    Scene scene;
    const float radiance = 10.0f, r = 0.5f, d = 4.0f, albedo = 0.8f;
    scene.add(std::make_unique<Quad>(Vec3{-50, 0, -50}, Vec3{0, 0, 100}, Vec3{100, 0, 0},
                                     scene.addMaterial(Material::diffuse(Rgb(albedo)))));
    scene.add(std::make_unique<Sphere>(Vec3{0, d, 0}, r, scene.addMaterial(Material::light(Rgb(radiance)))));
    scene.finalize();
    Camera cam;
    cam.position = {0, 1.0f, 0.01f};
    cam.pitch = -kHalfPi + 1e-3f;
    cam.verticalFov = 0.5f;
    const Rgb l = averageCenter(scene, cam, 4000, 2);
    const float expected = albedo * kInvPi * radiance * kPi * (r * r) / (d * d);
    CHECK_NEAR(l.g / expected, 1.0, 0.05);
}

TEST_CASE("rt: presets build and render") {
    ThreadPool pool(2);
    for (const auto& p : presets()) {
        Scene scene;
        REQUIRE(makePreset(p.id, scene));
        CHECK(scene.primitiveCount() > 0);
        const ImageF img = renderImage(scene, scene.camera, 24, 16, 2, pool, 4);
        double sum = 0.0;
        bool finite = true;
        for (std::size_t i = 0; i < img.size(); ++i) {
            sum += img.data()[i].luminance();
            finite = finite && std::isfinite(img.data()[i].r);
        }
        CHECK(finite);
        CHECK(sum > 0.0);
    }
}
