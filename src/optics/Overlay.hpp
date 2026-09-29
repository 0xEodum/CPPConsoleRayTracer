#pragma once

#include "core/Image.hpp"
#include "optics/LightTracer.hpp"
#include "optics/Scene.hpp"

namespace crt::optics {

struct OverlayOptions {
    Selection selected;
    Selection hovered;
    const Shape* preview = nullptr;  ///< ghost of the object about to be placed
    bool drawLights = true;          ///< draw lights as dots (the terminal UI draws glyphs instead)
    float fillStrength = 1.0f;       ///< 0 = outlines only
    Rgb8 background{16, 16, 20};     ///< colour of the letterbox outside the world
};

/// Draws object fills/outlines (and optionally lights) on top of a tone-mapped light image.
void drawOverlay(Image8& image, const Scene& scene, const ViewTransform& view, const OverlayOptions& options);

/// Colour used for UI highlights of the current selection.
inline constexpr Rgb8 kSelectionColor{255, 205, 60};

}  // namespace crt::optics
