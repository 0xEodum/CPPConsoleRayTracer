#pragma once

#include <cstdint>
#include <vector>

#include "core/Image.hpp"
#include "core/ThreadPool.hpp"
#include "optics/Scene.hpp"

namespace crt::optics {

/// Maps world coordinates onto a pixel grid, preserving the world's aspect ratio (letterboxed).
struct ViewTransform {
    Vec2 offset;      ///< pixel position of the world origin
    float sx = 1.0f;  ///< pixels per world unit along x
    float sy = 1.0f;  ///< pixels per world unit along y

    /// pixelAspect = physical height / width of one pixel (1 for half-block cells, 2 for ASCII).
    static ViewTransform fit(const Box2& world, int width, int height, float pixelAspect);

    Vec2 toPixel(Vec2 w) const { return {offset.x + w.x * sx, offset.y + w.y * sy}; }
    Vec2 toWorld(Vec2 p) const { return {(p.x - offset.x) / sx, (p.y - offset.y) / sy}; }
    /// Size of one pixel in world units (the larger of both axes).
    float pixelSize() const { return std::max(1.0f / sx, 1.0f / sy); }

    bool operator==(const ViewTransform& o) const { return offset == o.offset && sx == o.sx && sy == o.sy; }
    bool operator!=(const ViewTransform& o) const { return !(*this == o); }
};

/// Forward light tracer for the 2D scene.
///
/// Rays are emitted from the lights (one wavelength each), bounced through the scene and every
/// segment of their path is splatted into an HDR image with anti-aliased line rasterisation.
/// The image therefore shows light "in flight", as if the plane were filled with a thin haze.
/// Brightness falls off as 1/r purely from ray density: no artificial attenuation is needed.
/// Results accumulate across calls to trace() until reset(), so a static scene converges.
class LightTracer {
public:
    struct Settings {
        int maxBounces = 32;
        float surfaceGlow = 1.0f;  ///< how strongly diffusely lit surfaces light up
    };

    /// Sets the output resolution and mapping. Resets the accumulation only if something changed.
    void configure(int width, int height, const ViewTransform& view);
    void reset();

    /// Traces `rayCount` more light paths and adds them to the accumulation.
    void trace(const Scene& scene, ThreadPool& pool, std::size_t rayCount);

    /// Writes the average fluence image (before exposure / tone mapping) into `out`.
    void resolve(ImageF& out) const;

    std::uint64_t raysTraced() const { return raysTraced_; }
    int passes() const { return passes_; }
    int width() const { return width_; }
    int height() const { return height_; }
    const ViewTransform& view() const { return view_; }
    Settings& settings() { return settings_; }

private:
    struct PassContext;
    void traceRay(const PassContext& ctx, Pcg32& rng, ImageF& target) const;
    void depositSegment(ImageF& target, Vec2 a, Vec2 b, float worldLength, const Rgb& value) const;
    void splat(ImageF& target, Vec2 pixel, const Rgb& value) const;

    Settings settings_;
    int width_ = 0;
    int height_ = 0;
    ViewTransform view_;
    std::vector<ImageF> workerBuffers_;
    std::uint64_t raysTraced_ = 0;
    std::uint64_t seed_ = 1;
    int passes_ = 0;
};

/// Convenience for offline rendering: traces `rayCount` rays into a fresh width x height image.
ImageF renderScene(const Scene& scene, int width, int height, std::size_t rayCount, ThreadPool& pool,
                   float pixelAspect = 1.0f);

}  // namespace crt::optics
