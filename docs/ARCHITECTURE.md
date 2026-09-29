# Architecture

The code base is split into layers with a strict dependency direction:

```text
            app  (modes, shell, CLI)
           /   \
         ui    optics      rt
          \      |         |
           term  |         |
             \   |        /
               core  ────
```

`core` knows nothing about the other layers, `optics` and `rt` are independent rendering engines
that know nothing about terminals, and only `app` ties everything together. All engine code is
built into the `crt_engine` static library, which both the executable and the unit tests link.

## core

| File | Purpose |
|---|---|
| `Vec2`, `Vec3`, `Math` | value types with `constexpr` operators; no virtual functions, no heap |
| `Color` | linear HDR `Rgb` and display `Rgb8`, sRGB transfer functions |
| `Spectrum` | CIE 1931 colour matching (Wyman et al. 2013 fit) → linear sRGB, normalised so an equal-energy spectrum is white; spectral reflectance from RGB albedo; Cauchy dispersion; Planck's law; importance-sampled `EmissionSpectrum` |
| `Random` | PCG32 generator (one instance per thread / pixel — no shared state) |
| `Image` | `Image<Pixel>` container, ACES/Reinhard tone mapping, automatic exposure, a dependency-free PNG encoder (adaptive scanline filters + LZ77 + fixed-Huffman deflate) |
| `ThreadPool` | persistent workers with `parallelFor(count, job(index, worker))`; dynamic scheduling via an atomic counter; the caller participates as worker 0 |

## optics — the 2D light tracer

* **Shapes** (`Shape.hpp`) use the *template method* pattern: the public `intersect`/`distance`
  are non-virtual, transform the query into the shape's local frame (with cached sine/cosine) and
  call the `*Local` hooks. Convex shapes are expressed as *parametric intervals*
  `[tIn, tOut]`; intersecting two intervals is the CSG used for lenses. Every shape also provides
  a signed distance used for picking and outline drawing.
* **Materials** are immutable records in a catalogue and shapes refer to them by index
  (*flyweight*). This also makes scene files trivially serialisable. `interact()` samples the
  scattering event (Lambert, mirror, rough metal, Fresnel dielectric, absorber).
* **Lights** are polymorphic emitters (`emit(rng) -> Ray2`); their colour is an emission spectrum
  from a second catalogue.
* **LightTracer** traces rays *from the lights* ("forward" light tracing). Each path segment is
  rasterised into a per-worker HDR buffer: the major axis is walked one pixel centre at a time and
  the energy is split between the two nearest pixels on the minor axis, with a one-pixel guard band
  that removes bounds checks from the inner loop. Each step deposits `energy × worldLength / pixelArea`,
  which makes the image independent of the line slope and of the output resolution (unit-tested).
  Russian roulette keeps long paths unbiased; accumulation continues until the scene changes.
* **Scene** has value semantics (deep copy), which the editor's snapshot-based undo relies on
  (*memento*), and a line-based text format (see [scene-format.md](scene-format.md)).

## rt — the 3D path tracer

* `Primitive` subclasses: `Sphere`, `Quad`, `Triangle` (optionally smooth-shaded), `Plane`
  (unbounded, kept outside the BVH). Emissive primitives can be sampled as lights;
  spheres override the default area sampling with solid-angle (cone) sampling.
* `Bvh`: binned surface-area-heuristic build (16 bins/axis), flattened depth-first node array,
  iterative front-to-back traversal and a separate early-exit `occluded()` for shadow rays.
* `PathTracer`: unidirectional path tracing with next event estimation for the sun and emitters,
  emission counted only where NEE cannot account for it (camera rays and specular bounces),
  clamped caustic paths, Russian roulette after three bounces. Every sample's random stream is
  derived from `(pixel, sample index)`, so results are deterministic regardless of threading.
* Physics is verified by *white furnace* tests (a diffuse sphere reflects exactly its albedo,
  glass neither creates nor destroys energy) and by comparing NEE with the analytic irradiance
  of a spherical light.

## term — terminal abstraction

* `Terminal` is an abstract interface with a factory; `TerminalPosix.cpp` (termios, `poll`,
  `SIGWINCH`, async-signal-safe restoration on crashes) and `TerminalWindows.cpp` (Win32
  console API: `ReadConsoleInputW`, VT output, console control handler) implement it. RAII puts
  the terminal back into its original state in every exit path.
* `InputParser` is a pure, unit-tested decoder for VT/xterm input: UTF-8, CSI/SS3 keys with
  modifiers, SGR (1006) and legacy X10 mouse reports, and the ambiguous lone `ESC`.
* `Canvas` is an off-screen grid of cells; images are mapped either to half blocks
  (`▀` = two pixels per cell) or to an ASCII density ramp.
* `Presenter` diffs the new canvas against what is on screen and emits only changed cells,
  tracking cursor position and SGR state and treating colours within a tolerance as unchanged.
  Output is wrapped in synchronised-update markers to avoid tearing.

## app — application shell

* `App` owns the terminal, presenter, thread pool and the two `Mode`s and runs the frame loop:
  poll input → let the active mode render for a time budget → present. The budget and the
  presentation rate adapt to user activity (30 fps while interacting, ~8 fps while converging),
  and the loop sleeps in `poll` once the image has converged.
* `Mode` is the *state* pattern: `OpticsMode` (the 2D editor) and `PathTraceMode` (the 3D viewer)
  receive events, do their rendering work, draw into the canvas and describe their own help.
  They talk back to the shell only through the narrow `AppServices` interface.
* Long-running PNG screenshots are incremental jobs advanced inside `update()`, so the UI stays
  responsive while they render.
* `Cli` parses the command line and runs headless commands (`render2d`, `render3d`, `bench`,
  `list`) without touching the terminal.
