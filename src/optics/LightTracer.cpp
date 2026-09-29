#include "optics/LightTracer.hpp"

#include <algorithm>
#include <cmath>

#include "core/Spectrum.hpp"
#include "optics/Material.hpp"

namespace crt::optics {

ViewTransform ViewTransform::fit(const Box2& world, int width, int height, float pixelAspect) {
    const Vec2 size = world.size();
    ViewTransform v;
    const float physicalScale = std::min(static_cast<float>(width) / size.x,
                                         static_cast<float>(height) * pixelAspect / size.y);
    v.sx = physicalScale;
    v.sy = physicalScale / pixelAspect;
    v.offset = {(static_cast<float>(width) - size.x * v.sx) * 0.5f - world.lo.x * v.sx,
                (static_cast<float>(height) - size.y * v.sy) * 0.5f - world.lo.y * v.sy};
    return v;
}

namespace {

constexpr float kEpsilon = 1e-3f;
constexpr float kRouletteThreshold = 0.25f;

/// Distance along the ray to the boundary of the world box (0 if the origin is outside).
float exitDistance(const Box2& box, const Ray2& ray) {
    if (!box.contains(ray.origin)) return 0.0f;
    float t = kInfinity;
    if (ray.direction.x > 0.0f) t = std::min(t, (box.hi.x - ray.origin.x) / ray.direction.x);
    if (ray.direction.x < 0.0f) t = std::min(t, (box.lo.x - ray.origin.x) / ray.direction.x);
    if (ray.direction.y > 0.0f) t = std::min(t, (box.hi.y - ray.origin.y) / ray.direction.y);
    if (ray.direction.y < 0.0f) t = std::min(t, (box.lo.y - ray.origin.y) / ray.direction.y);
    return std::max(t, 0.0f);
}

}  // namespace

struct LightTracer::PassContext {
    const Scene* scene;
    Box2 bounds;
    std::vector<float> lightCdf;  // cumulative, normalised light power
    float energyPerRay;           // total power: each ray carries the whole budget, divided at resolve
    float density;                // pixels per world area: converts length*energy into per-pixel fluence
};

void LightTracer::configure(int width, int height, const ViewTransform& view) {
    if (width == width_ && height == height_ && view == view_) return;
    width_ = width;
    height_ = height;
    view_ = view;
    for (auto& buffer : workerBuffers_) buffer.resize(width_ + 2, height_ + 2);  // 1px guard border
    reset();
}

void LightTracer::reset() {
    for (auto& buffer : workerBuffers_) buffer.fill({});
    raysTraced_ = 0;
    passes_ = 0;
}

void LightTracer::trace(const Scene& scene, ThreadPool& pool, std::size_t rayCount) {
    if (width_ <= 0 || height_ <= 0 || rayCount == 0) return;
    if (workerBuffers_.size() != pool.size()) {
        workerBuffers_.assign(pool.size(), ImageF(width_ + 2, height_ + 2));
        raysTraced_ = 0;
        passes_ = 0;
    }

    PassContext ctx;
    ctx.scene = &scene;
    ctx.bounds = scene.bounds();
    ctx.energyPerRay = scene.totalPower();
    ctx.density = view_.sx * view_.sy;
    if (ctx.energyPerRay <= 0.0f) {
        raysTraced_ += rayCount;
        ++passes_;
        return;
    }
    float running = 0.0f;
    for (const auto& light : scene.lights()) {
        running += std::max(0.0f, light->power) / ctx.energyPerRay;
        ctx.lightCdf.push_back(running);
    }

    const std::size_t jobs = std::clamp<std::size_t>(rayCount / 512, 1, 256);
    const std::uint64_t passSeed = hash64(seed_++);
    pool.parallelFor(jobs, [&](std::size_t job, std::size_t worker) {
        Pcg32 rng(hashCombine(passSeed, job), job);
        const std::size_t begin = rayCount * job / jobs;
        const std::size_t end = rayCount * (job + 1) / jobs;
        for (std::size_t i = begin; i < end; ++i) traceRay(ctx, rng, workerBuffers_[worker]);
    });

    raysTraced_ += rayCount;
    ++passes_;
}

void LightTracer::traceRay(const PassContext& ctx, Pcg32& rng, ImageF& target) const {
    const auto& lights = ctx.scene->lights();
    const float pick = rng.uniform();
    std::size_t index = 0;
    while (index + 1 < ctx.lightCdf.size() && pick >= ctx.lightCdf[index]) ++index;
    const Light& light = *lights[index];

    Ray2 ray = light.emit(rng);
    const float lambda = lightColor(light.color).spectrum.sample(rng.uniform());
    const Rgb radiance = spectrum::wavelengthToRgb(lambda) * (ctx.energyPerRay * ctx.density);
    float throughput = 1.0f;

    for (int bounce = 0; bounce <= settings_.maxBounces; ++bounce) {
        const float tExit = exitDistance(ctx.bounds, ray);
        if (tExit <= 0.0f) break;

        SurfaceHit hit;
        const bool hitSomething = ctx.scene->intersect(ray, kEpsilon, tExit, hit);
        const float tEnd = hitSomething ? hit.t : tExit;
        depositSegment(target, ray.origin, ray.at(tEnd), tEnd, radiance * throughput);
        if (!hitSomething) break;

        const Interaction it = interact(material(hit.shape->material()), ray.direction, hit.normal, lambda, rng);
        if (it.diffuse && settings_.surfaceGlow > 0.0f) {
            // A lit matte surface scatters light towards the viewer as well: show it as a thin
            // bright rim one pixel long, proportional to the reflected energy.
            const float glowLength = settings_.surfaceGlow / view_.sx;
            splat(target, view_.toPixel(hit.point), radiance * (throughput * it.weight * glowLength));
        }
        if (!it.alive) break;

        throughput *= it.weight;
        if (throughput < kRouletteThreshold) {  // Russian roulette keeps the estimator unbiased
            if (rng.uniform() * kRouletteThreshold >= throughput) break;
            throughput = kRouletteThreshold;
        }

        const float side = dot(it.direction, hit.normal) > 0.0f ? 1.0f : -1.0f;
        ray.origin = hit.point + hit.normal * (side * kEpsilon);
        ray.direction = it.direction;
    }
}

void LightTracer::depositSegment(ImageF& target, Vec2 a, Vec2 b, float worldLength, const Rgb& value) const {
    if (!(worldLength > 0.0f)) return;
    Vec2 pa = view_.toPixel(a);
    Vec2 pb = view_.toPixel(b);
    const float dx = pb.x - pa.x;
    const float dy = pb.y - pa.y;
    const bool xMajor = std::abs(dx) >= std::abs(dy);
    const float major = xMajor ? std::abs(dx) : std::abs(dy);
    if (major < 1e-6f) {
        splat(target, (pa + pb) * 0.5f, value * worldLength);
        return;
    }

    // Walk the major axis one pixel centre at a time and distribute each step between the two
    // nearest pixels on the minor axis (Wu-style anti-aliasing). Every step covers the same world
    // length, so brightness does not depend on the line's slope. The target has a one pixel
    // border on every side, which removes all per-pixel bounds checks on the minor axis.
    if (xMajor ? pa.x > pb.x : pa.y > pb.y) std::swap(pa, pb);
    const float startMajor = xMajor ? pa.x : pa.y;
    const float endMajor = xMajor ? pb.x : pb.y;
    const float slope = xMajor ? (pb.y - pa.y) / (pb.x - pa.x) : (pb.x - pa.x) / (pb.y - pa.y);
    const int limitMajor = xMajor ? width_ : height_;
    const float limitMinor = static_cast<float>(xMajor ? height_ : width_);

    const int first = std::max(static_cast<int>(std::ceil(startMajor - 0.5f)), 0);
    const int last = std::min(static_cast<int>(std::floor(endMajor - 0.5f)), limitMajor - 1);
    if (last < first) {
        if (std::ceil(startMajor - 0.5f) > std::floor(endMajor - 0.5f)) splat(target, (pa + pb) * 0.5f, value * worldLength);
        return;
    }

    const Rgb step = value * (worldLength / major);
    const std::ptrdiff_t stride = target.width();
    const std::ptrdiff_t majorStride = xMajor ? 1 : stride;
    const std::ptrdiff_t minorStride = xMajor ? stride : 1;
    Rgb* const base = target.data() + stride + 1;  // pixel (0,0) inside the padded buffer

    float minor = (xMajor ? pa.y : pa.x) + (static_cast<float>(first) + 0.5f - startMajor) * slope - 0.5f;
    for (int i = first; i <= last; ++i, minor += slope) {
        if (!(minor >= -1.0f && minor < limitMinor)) continue;
        const int j = static_cast<int>(minor + 1.0f) - 1;  // floor(minor), valid because minor >= -1
        const float f = minor - static_cast<float>(j);
        Rgb* p = base + i * majorStride + j * minorStride;
        *p += step * (1.0f - f);
        *(p + minorStride) += step * f;
    }
}

void LightTracer::splat(ImageF& target, Vec2 pixel, const Rgb& value) const {
    const float fx = pixel.x - 0.5f;
    const float fy = pixel.y - 0.5f;
    if (!(fx >= -1.0f && fy >= -1.0f && fx < static_cast<float>(width_) && fy < static_cast<float>(height_))) return;
    const int x = static_cast<int>(fx + 1.0f) - 1;
    const int y = static_cast<int>(fy + 1.0f) - 1;
    const float tx = fx - static_cast<float>(x);
    const float ty = fy - static_cast<float>(y);
    const std::ptrdiff_t stride = target.width();
    Rgb* p = target.data() + (y + 1) * stride + (x + 1);
    p[0] += value * ((1 - tx) * (1 - ty));
    p[1] += value * (tx * (1 - ty));
    p[stride] += value * ((1 - tx) * ty);
    p[stride + 1] += value * (tx * ty);
}

void LightTracer::resolve(ImageF& out) const {
    out.resize(width_, height_);
    if (raysTraced_ == 0 || workerBuffers_.empty()) return;
    const float scale = 1.0f / static_cast<float>(raysTraced_);
    for (int y = 0; y < height_; ++y) {
        Rgb* dst = out.row(y);
        for (int x = 0; x < width_; ++x) dst[x] = {};
        for (const auto& buffer : workerBuffers_) {
            const Rgb* src = buffer.row(y + 1) + 1;
            for (int x = 0; x < width_; ++x) dst[x] += src[x];
        }
        for (int x = 0; x < width_; ++x) dst[x] *= scale;
    }
}

ImageF renderScene(const Scene& scene, int width, int height, std::size_t rayCount, ThreadPool& pool,
                   float pixelAspect) {
    LightTracer tracer;
    tracer.configure(width, height, ViewTransform::fit(scene.bounds(), width, height, pixelAspect));
    constexpr std::size_t kBatch = 1u << 20;
    for (std::size_t done = 0; done < rayCount; done += kBatch) tracer.trace(scene, pool, std::min(kBatch, rayCount - done));
    ImageF out;
    tracer.resolve(out);
    return out;
}

}  // namespace crt::optics
