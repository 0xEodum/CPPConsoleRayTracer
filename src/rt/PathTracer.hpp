#pragma once

#include <cstdint>

#include "core/Image.hpp"
#include "core/ThreadPool.hpp"
#include "rt/Scene.hpp"

namespace crt::rt {

/// Unidirectional Monte Carlo path tracer with next event estimation.
///
/// Every call to renderPass() adds samples to a running per-pixel sum, so the image refines
/// progressively while the camera stands still. Sampling is deterministic: the random stream of
/// every sample is derived from (pixel, sample index), independently of thread scheduling.
class PathTracer {
public:
    struct Settings {
        int maxDepth = 8;
        float clampLuminance = 10.0f;  ///< firefly suppression per sample (0 = off)
        /// Caustic paths (diffuse -> specular -> small light) cannot be sampled by next event
        /// estimation and produce fireflies. Their contribution is clamped to this luminance.
        float causticClamp = 1.5f;
    };

    void configure(int width, int height, float pixelAspect);
    void reset();

    void renderPass(const Scene& scene, const Camera& camera, ThreadPool& pool, int samplesPerPixel = 1);

    /// Average radiance per pixel.
    void resolve(ImageF& out) const;

    /// Radiance arriving along one ray (exposed for tests and tools).
    Rgb trace(const Scene& scene, Ray ray, Pcg32& rng) const;

    int samples() const { return samples_; }
    std::uint64_t pathsTraced() const { return pathsTraced_; }
    int width() const { return width_; }
    int height() const { return height_; }
    float pixelAspect() const { return pixelAspect_; }
    Settings& settings() { return settings_; }
    const Settings& settings() const { return settings_; }

private:
    Rgb sampleDirectLight(const Scene& scene, const Vec3& point, const Vec3& normal, Pcg32& rng) const;

    Settings settings_;
    int width_ = 0;
    int height_ = 0;
    float pixelAspect_ = 1.0f;
    ImageF sum_;
    int samples_ = 0;
    std::uint64_t pathsTraced_ = 0;
};

/// Offline render helper.
ImageF renderImage(const Scene& scene, const Camera& camera, int width, int height, int samples, ThreadPool& pool,
                   int maxDepth = 8);

}  // namespace crt::rt
