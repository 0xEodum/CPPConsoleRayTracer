#include "rt/PathTracer.hpp"

#include <cmath>

namespace crt::rt {
namespace {

constexpr float kShadowEpsilon = 1e-3f;
constexpr float kOffset = 1e-4f;

Vec3 toWorld(const Vec3& local, const Vec3& n) {
    Vec3 t, b;
    orthonormalBasis(n, t, b);
    return t * local.x + b * local.y + n * local.z;
}

Vec3 sampleCosineHemisphere(const Vec3& n, Pcg32& rng) {
    const Vec2 d = sampleUnitDisk(rng.uniform(), rng.uniform());
    const float z = std::sqrt(std::max(0.0f, 1.0f - d.x * d.x - d.y * d.y));
    return toWorld({d.x, d.y, z}, n);
}

Vec3 sampleUnitSphere(Pcg32& rng) {
    const float z = 1.0f - 2.0f * rng.uniform();
    const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    const float phi = kTwoPi * rng.uniform();
    const float radius = std::cbrt(rng.uniform());
    return Vec3{r * std::cos(phi), r * std::sin(phi), z} * radius;
}

/// Unpolarised Fresnel reflectance; eta = n_incident / n_transmitted. Returns 1 on TIR.
float fresnel(float cosI, float eta, float& cosT) {
    const float sin2T = eta * eta * std::max(0.0f, 1.0f - cosI * cosI);
    if (sin2T >= 1.0f) {
        cosT = 0.0f;
        return 1.0f;
    }
    cosT = std::sqrt(1.0f - sin2T);
    const float rs = (eta * cosI - cosT) / (eta * cosI + cosT);
    const float rp = (eta * cosT - cosI) / (eta * cosT + cosI);
    return 0.5f * (rs * rs + rp * rp);
}

bool isFinite(const Rgb& c) { return std::isfinite(c.r) && std::isfinite(c.g) && std::isfinite(c.b); }

Rgb clampedLuminance(Rgb c, float maxLuminance) {
    const float lum = c.luminance();
    if (maxLuminance > 0.0f && lum > maxLuminance) c *= maxLuminance / lum;
    return c;
}

}  // namespace

void PathTracer::configure(int width, int height, float pixelAspect) {
    if (width == width_ && height == height_ && pixelAspect == pixelAspect_) return;
    width_ = width;
    height_ = height;
    pixelAspect_ = pixelAspect;
    sum_.resize(width, height);
    reset();
}

void PathTracer::reset() {
    sum_.fill({});
    samples_ = 0;
}

void PathTracer::renderPass(const Scene& scene, const Camera& camera, ThreadPool& pool, int samplesPerPixel) {
    if (width_ <= 0 || height_ <= 0 || samplesPerPixel <= 0) return;
    const int firstSample = samples_;
    pool.parallelFor(static_cast<std::size_t>(height_), [&](std::size_t row, std::size_t) {
        const int y = static_cast<int>(row);
        Rgb* out = sum_.row(y);
        for (int x = 0; x < width_; ++x) {
            const std::uint64_t pixelSeed = hashCombine(static_cast<std::uint64_t>(y) * 65537u + static_cast<std::uint64_t>(x), 0x5eed);
            for (int s = 0; s < samplesPerPixel; ++s) {
                Pcg32 rng(hashCombine(pixelSeed, static_cast<std::uint64_t>(firstSample + s)));
                const Ray ray = camera.generateRay(static_cast<float>(x) + rng.uniform(), static_cast<float>(y) + rng.uniform(),
                                                   width_, height_, pixelAspect_, rng);
                Rgb radiance = trace(scene, ray, rng);
                if (!isFinite(radiance)) continue;
                const float lum = radiance.luminance();
                if (settings_.clampLuminance > 0.0f && lum > settings_.clampLuminance) radiance *= settings_.clampLuminance / lum;
                out[x] += radiance;
            }
        }
    });
    samples_ += samplesPerPixel;
    pathsTraced_ += static_cast<std::uint64_t>(width_) * static_cast<std::uint64_t>(height_) *
                    static_cast<std::uint64_t>(samplesPerPixel);
}

void PathTracer::resolve(ImageF& out) const {
    out.resize(width_, height_);
    if (samples_ == 0) return;
    const float scale = 1.0f / static_cast<float>(samples_);
    for (std::size_t i = 0; i < out.size(); ++i) out.data()[i] = sum_.data()[i] * scale;
}

Rgb PathTracer::sampleDirectLight(const Scene& scene, const Vec3& point, const Vec3& normal, Pcg32& rng) const {
    // Returns the cosine-weighted incident radiance divided by pi, i.e. the direct lighting of
    // a white Lambertian surface. The caller multiplies by the albedo.
    Rgb result;
    const Environment& env = scene.environment;
    if (env.hasSun) {
        const float cosMax = env.sunCosThreshold();
        const float cosTheta = 1.0f - rng.uniform() * (1.0f - cosMax);
        const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
        const float phi = kTwoPi * rng.uniform();
        const Vec3 wi = toWorld({sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta}, env.sunDirection);
        const float cosSurface = dot(normal, wi);
        if (cosSurface > 0.0f && !scene.occluded({point, wi}, kInfinity)) {
            result += env.sunRadiance * (cosSurface * env.sunSolidAngle() * kInvPi);
        }
    }

    const auto& emitters = scene.emitters();
    if (!emitters.empty()) {
        const std::size_t index = rng.below(static_cast<std::uint32_t>(emitters.size()));
        const Primitive& light = *emitters[index];
        Vec3 wi;
        float dist = 0.0f, pdfSolidAngle = 0.0f;
        if (light.sampleDirection(point, rng.uniform(), rng.uniform(), wi, dist, pdfSolidAngle)) {
            const float cosSurface = dot(normal, wi);
            if (cosSurface > 0.0f && pdfSolidAngle > 0.0f && !scene.occluded({point, wi}, dist - kShadowEpsilon)) {
                const Rgb& le = scene.material(light.material()).emission;
                result += le * (cosSurface * kInvPi * static_cast<float>(emitters.size()) / pdfSolidAngle);
            }
        }
    }
    return result;
}

Rgb PathTracer::trace(const Scene& scene, Ray ray, Pcg32& rng) const {
    Rgb radiance;
    Rgb throughput(1.0f);
    bool countEmission = true;  // camera rays and specular bounces see emitters directly
    bool afterDiffuse = false;  // emitters reached now would be a caustic path

    for (int depth = 0; depth < settings_.maxDepth; ++depth) {
        Hit hit;
        if (!scene.intersect(ray, kInfinity, hit)) {
            const Rgb sky = throughput * scene.environment.radiance(ray.direction, countEmission);
            radiance += afterDiffuse && countEmission ? clampedLuminance(sky, settings_.causticClamp) : sky;
            break;
        }

        const Material& m = scene.material(hit.material);
        const bool frontFace = dot(ray.direction, hit.normal) < 0.0f;
        const Vec3 ng = frontFace ? hit.normal : -hit.normal;
        Vec3 n = frontFace ? hit.shadingNormal : -hit.shadingNormal;
        if (dot(n, ng) <= 0.0f) n = ng;  // guard against extreme shading-normal interpolation

        if (m.kind == MaterialKind::Emissive) {
            if (countEmission && frontFace) {
                const Rgb emitted = throughput * m.emission;
                radiance += afterDiffuse ? clampedLuminance(emitted, settings_.causticClamp) : emitted;
            }
            break;
        }

        Vec3 next;
        bool diffuse = false;
        switch (m.kind) {
            case MaterialKind::Glossy: {
                float cosT;
                const float f = fresnel(std::max(0.0f, -dot(ray.direction, n)), 1.0f / m.ior, cosT);
                if (rng.uniform() < f) {  // clear-coat reflection
                    next = normalize(reflect(ray.direction, n) + sampleUnitSphere(rng) * m.roughness);
                    if (dot(next, ng) <= 0.0f) return radiance;
                    countEmission = true;
                    break;
                }
                diffuse = true;
                break;
            }
            case MaterialKind::Diffuse:
                diffuse = true;
                break;
            case MaterialKind::Metal:
                next = normalize(reflect(ray.direction, n) + sampleUnitSphere(rng) * m.roughness);
                if (dot(next, ng) <= 0.0f) return radiance;
                throughput *= m.albedo;
                countEmission = true;
                break;
            case MaterialKind::Dielectric: {
                const float eta = frontFace ? 1.0f / m.ior : m.ior;
                const float cosI = std::min(1.0f, -dot(ray.direction, n));
                float cosT;
                const float f = fresnel(cosI, eta, cosT);
                if (rng.uniform() < f) {
                    next = reflect(ray.direction, n);
                } else {
                    next = normalize(ray.direction * eta + n * (eta * cosI - cosT));
                    throughput *= m.albedo;
                }
                countEmission = true;
                break;
            }
            case MaterialKind::Emissive:
                break;
        }

        if (diffuse) {
            const Rgb albedo = m.albedoAt(hit.point);
            const Vec3 origin = hit.point + ng * kOffset;
            radiance += throughput * albedo * sampleDirectLight(scene, origin, n, rng);
            next = sampleCosineHemisphere(n, rng);
            if (dot(next, ng) <= 0.0f) break;
            throughput *= albedo;
            countEmission = false;
            afterDiffuse = true;
        }

        // Russian roulette once the path has had a chance to pick up indirect light.
        if (depth >= 3) {
            const float survive = std::min(0.95f, throughput.maxComponent());
            if (rng.uniform() >= survive) break;
            throughput *= 1.0f / survive;
        }

        ray.origin = hit.point + ng * (dot(next, ng) > 0.0f ? kOffset : -kOffset);
        ray.direction = next;
    }
    return radiance;
}

ImageF renderImage(const Scene& scene, const Camera& camera, int width, int height, int samples, ThreadPool& pool,
                   int maxDepth) {
    PathTracer tracer;
    tracer.settings().maxDepth = maxDepth;
    tracer.configure(width, height, 1.0f);
    for (int s = 0; s < samples; s += 4) tracer.renderPass(scene, camera, pool, std::min(4, samples - s));
    ImageF out;
    tracer.resolve(out);
    return out;
}

}  // namespace crt::rt
