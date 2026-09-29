# Changelog

## 2.0.0 — the rewrite

Version 2 is a ground-up rewrite of the 2020 Windows-console prototype. The original version is
preserved in the git history (commit `3d0cc2f`).

### New
* Cross-platform: Linux, macOS and Windows (MSVC and MinGW), CMake build, CI on all three.
* Physically based spectral 2D optics: Snell's law, Fresnel equations, total internal reflection,
  Cauchy dispersion, Lambertian scattering, blackbody and monochromatic emitters, CIE colour.
* New shapes (circle, prism, lens, wall, arc mirror), new lights (beam), new materials
  (gold, brushed metal, water, diamond, coloured mattes).
* A 3D path tracer: BVH, spheres/quads/meshes, glass, metal, glossy, area lights with next event
  estimation, depth of field, free-flight camera, four demo scenes.
* Editor features: drag & drop, aiming, rotation, resizing, duplicate, undo/redo, scene files,
  demo scenes, PNG screenshots, help overlay, status bar, automatic exposure.
* Headless `render2d` / `render3d` / `bench` / `list` commands.
* Half-block rendering (2 square pixels per character cell) and an ASCII-art mode.
* 41 unit tests, including physics tests (energy conservation, white furnace, analytic
  irradiance) and a deflate decoder that validates the PNG writer.

### Problems in v1 that the rewrite fixes
* **Build structure:** `.cpp` files were `#include`d as headers (with `#pragma once`) *and*
  compiled separately; every class was header-only in disguise. Windows types (`SHORT`, `BYTE`,
  `HANDLE`) leaked into the maths classes.
* **Undefined behaviour:** float→`BYTE` casts without clamping in `Color` (negative or >255
  values); `Matte::getHashedFloat` cast negative floats to `unsigned`.
* **`LightSource::setIntensity`** divided by the *new* intensity, so the spectrum was never
  rescaled — and produced NaNs (division by zero) once the intensity reached 0.
* **Energy creation in `Matte`:** "spectrum coherence" is `max/average` (range 1…8) but was used
  as if it were in [0, 1], giving reflection factors above 1 and negative scattering widths.
* **Wrong normals:** rectangle normals were derived from *rounded* hit coordinates, so rays
  hitting near an edge picked the wrong face or a diagonal "corner" normal.
* **Fake optics:** prism refraction used fixed angles instead of Snell's law, dispersion was three
  hard-coded colours, light attenuation was an arbitrary linear falloff with a per-light radius,
  and the laser range was found by a `dynamic_cast` + position-comparison hack in the renderer.
  `ConsolePrism`, `getSpectralResponse`, `calculateAttenuation` and the logger were never used.
* **`SpotLight`** divided by `raysCount - 1` (division by zero for a single ray).
* **Rendering:** overlapping rays overwrote each other instead of adding up; `Spectrum::toRGB`
  ran for every 0.1-unit step of every ray; every frame re-sent every cell with a colour code
  and a reset code.
* **Input:** holding the left mouse button spawned rectangles on every mouse-move event; `Esc`
  was only noticed after some other input event arrived; the console size and font were hard-coded.
