#include "optics/Overlay.hpp"

#include <algorithm>
#include <cmath>

#include "optics/Material.hpp"

namespace crt::optics {
namespace {

Rgb8 scaled(Rgb8 c, float s) {
    return {static_cast<std::uint8_t>(c.r * s), static_cast<std::uint8_t>(c.g * s), static_cast<std::uint8_t>(c.b * s)};
}

/// Opaque materials hide whatever is inside them; transparent ones are only tinted.
Rgb8 fillColor(const Material& m) {
    switch (m.kind) {
        case MaterialKind::Absorber: return {34, 34, 38};
        case MaterialKind::Diffuse: return scaled(m.tint, 0.32f);
        case MaterialKind::Mirror:
        case MaterialKind::Metal: return scaled(m.tint, 0.42f);
        case MaterialKind::Dielectric: return m.tint;
    }
    return m.tint;
}

}  // namespace

void drawOverlay(Image8& image, const Scene& scene, const ViewTransform& view, const OverlayOptions& options) {
    const Box2 world = scene.bounds();
    const float pixelSize = view.pixelSize();
    const float outline = 0.75f * pixelSize;

    struct Item {
        const Shape* shape;
        Box2 bounds;
        Rgb8 fill;
        float alpha;
        Rgb8 edge;
        float edgeAlpha;
    };
    std::vector<Item> items;
    items.reserve(scene.shapes().size() + 1);
    for (std::size_t i = 0; i < scene.shapes().size(); ++i) {
        const Shape& s = *scene.shapes()[i];
        const Material& m = material(s.material());
        Rgb8 edge = m.tint;
        if (options.selected == Selection::shape(i)) edge = kSelectionColor;
        else if (options.hovered == Selection::shape(i)) edge = mix(edge, {255, 255, 255}, 0.6f);
        const float alpha = m.kind == MaterialKind::Dielectric ? 0.12f : 1.0f;
        items.push_back({&s, s.bounds().inflated(2.0f * pixelSize), fillColor(m), alpha * options.fillStrength, edge, 0.9f});
    }
    if (options.preview) {
        const Material& m = material(options.preview->material());
        items.push_back({options.preview, options.preview->bounds().inflated(2.0f * pixelSize), m.tint, 0.0f, m.tint, 0.45f});
    }

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const Vec2 p = view.toWorld({static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f});
            Rgb8& px = image.at(x, y);
            if (!world.contains(p)) {
                px = options.background;
                continue;
            }
            for (const Item& item : items) {
                if (!item.bounds.contains(p)) continue;
                const float d = item.shape->distance(p);
                if (item.shape->isSolid() && d < -outline && item.alpha > 0.0f) {
                    px = mix(px, item.fill, item.alpha);
                } else if (std::abs(d) <= outline) {
                    px = mix(px, item.edge, item.edgeAlpha);
                }
            }
        }
    }

    if (!options.drawLights) return;
    const float radius = std::max(1.5f, 1.3f * view.sx);
    for (std::size_t i = 0; i < scene.lights().size(); ++i) {
        const Light& light = *scene.lights()[i];
        const Vec2 c = view.toPixel(light.position);
        const bool selected = options.selected == Selection::light(i);
        const Rgb8 color = lightColor(light.color).spectrum.swatch();
        const int r = static_cast<int>(std::ceil(radius + 1.0f));
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                const int x = static_cast<int>(c.x) + dx;
                const int y = static_cast<int>(c.y) + dy;
                if (!image.contains(x, y)) continue;
                const float d = std::hypot(static_cast<float>(x) + 0.5f - c.x, static_cast<float>(y) + 0.5f - c.y);
                if (d <= radius) image.at(x, y) = color;
                else if (selected && d <= radius + 1.0f) image.at(x, y) = kSelectionColor;
            }
        }
    }
}

}  // namespace crt::optics
