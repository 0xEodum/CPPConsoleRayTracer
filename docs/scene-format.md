# 2D scene file format

2D scenes are plain UTF-8 text files, one object per line. They are written by `Ctrl+S` / `F5`
in the editor and read by `Ctrl+O` / `F9`, `raytracer --scene FILE` and
`raytracer render2d --scene FILE`. See the [`scenes/`](../scenes) directory for examples.

```text
# comments start with '#', blank lines are ignored
scene name="Periscope" width=160 height=100 exposure=-3.5

shape wall pos=50,20 angle=45 material=mirror len=16
shape prism pos=72,50 angle=0 material=prism side=34
light laser pos=8,20 angle=0 power=0.6 color=green
light spot pos=8,20 angle=0 power=0.5 color=warm spread=10
```

Every line is a keyword followed by `key=value` pairs. Values containing spaces go in double
quotes. Coordinates are world units: the world is `width` × `height` (default 160 × 100) with
the origin in the top-left corner and **y pointing down**. Angles are in degrees, measured
clockwise on screen from the +x axis. The world border absorbs light.

## `scene`

| key | meaning |
|---|---|
| `name` | title shown in the UI |
| `width`, `height` | world size |
| `exposure` | exposure compensation in EV stops, applied on top of automatic exposure |

## `shape TYPE`

Common keys: `pos=x,y`, `angle`, `material`.

| type | size keys | description |
|---|---|---|
| `box` | `w`, `h` | rectangle |
| `circle` | `r` | disc |
| `prism` | `side` | equilateral triangle, apex up at angle 0 |
| `lens` | `r` (surface radius), `t` (thickness) | symmetric biconvex lens, optical axis along +x |
| `wall` | `len` | thin two-sided segment along +x |
| `arc` | `r`, `span` (degrees) | thin circular mirror; its concave side faces +x |

Materials: `matte`, `matte-red`, `matte-green`, `matte-blue`, `mirror`, `gold`, `brushed`,
`glass`, `water`, `diamond`, `prism`, `absorber`.

## `light TYPE`

Common keys: `pos=x,y`, `angle`, `power`, `color`.

| type | extra keys | description |
|---|---|---|
| `point` | — | emits in all directions |
| `spot` | `spread` (degrees) | cone with soft edges |
| `laser` | — | single infinitely thin beam |
| `beam` | `width` | collimated bundle of parallel rays |

Colours: `white` (equal-energy), `warm` (3000 K blackbody), `daylight` (6500 K), and the
monochromatic `red` (650 nm), `orange` (610), `yellow` (589, sodium), `green` (532),
`cyan` (495), `blue` (460), `violet` (410).

Errors are reported with the line number, e.g. `line 4: unknown material 'unobtainium'`.
