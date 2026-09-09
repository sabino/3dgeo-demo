# Building on the demo/template

Start here if you want to change a mesh, lighting, or the display technique.
This is a small standalone experiment, not a packaged engine with a stable API.
The [rendering walkthrough](../TECHNICAL_EXPLANATION.md) explains the math and
hardware layout; the [gallery](SCREENSHOTS.md) shows actual native output.

## 1. Get a known-good baseline

For a native build, install ngdevkit and its toolchain, GNU Make, `pkg-config`,
Python 3, and Pillow. `m68k-neogeo-elf-gcc`, `tiletool.py`, and `romtool.py` must
be on `PATH`. A host C compiler is needed for sanitizer tests. MAME, Xvfb, and
xauth enable native replay; `ngdevkit-gngeo` enables the interactive runner.

```sh
make -j2 cart
make test
make native-test
make docs-check
make gngeo
```

The generated cartridge is `build/viewer/rom/3dgeo_demo.zip`.
`gngeo_data.zip` and `neogeo.xml` in the same directory describe it to the
emulators; they are not extra game ROMs. The test/run targets copy the required
open-source ngdevkit BIOS locally. No BIOS is included in release bundles.

If SDK discovery fails, use `make NGDEVKIT_PREFIX=/absolute/sdk/prefix cart`.
Do not add `-mshort` to optimize integer arithmetic: the runtime/SDK interfaces
must agree on their integer ABI.

The [publishing guide](PUBLISHING.md) provides the Docker-based CI build for a
clean checkout. That container already includes the build and replay tools.
It is useful when you do not want to install the SDK directly.

### Try a downloaded CI build

Open a passing run in [GitHub Actions](https://github.com/sabino/3dgeo-demo/actions/workflows/build.yml)
and download the `3dgeo-demo` artifact. Unpack the artifact, verify
`SHA256SUMS`, then unpack its `3dgeo-demo.zip` bundle. The cartridge is the
**inner** `rom/3dgeo_demo.zip`; do not unpack that cartridge for the emulator.

Use the included `rom/gngeo_data.zip` with ngdevkit-gngeo, and provide a compatible
BIOS separately. The source archive corresponds to the same commit. Building
from source and using `make gngeo` is the simplest supported local launch path.

## 2. Add or replace a mesh

Save a small OBJ as `assets/models/custom.obj`. Start with a convex solid using
vertices and simple polygon faces. Keep coordinates readable; the importer
centers the bounding box and scales its largest dimension to 240 units.

To try a custom mesh without changing the default list:

```sh
make BUILDDIR=build/custom \
  MODEL_OBJS='assets/models/cube.obj assets/models/pyramid.obj assets/models/custom.obj' cart
make BUILDDIR=build/custom \
  MODEL_OBJS='assets/models/cube.obj assets/models/pyramid.obj assets/models/custom.obj' gngeo
```

Use a **fresh build directory when changing `MODEL_OBJS`**. Make does not track
a variable's value as a dependency; reusing an existing generated header can
leave a removed or reordered model in the ROM. Keep cube first: the light mode
renders model index zero. Default regression expectations also assume cube and
pyramid are the first two entries. The eight-scene gallery replay is specifically
for the default model set, not this shortened custom list.

For a permanent addition, update `MODEL_OBJS` in the [Makefile](../Makefile).
The generated header lives under `build/.../generated/model_data.h`; do not
hand-edit it. The four generated primitives come from
[make_primitive_objs.py](../tools/make_primitive_objs.py), so edit their generator
when changing their topology. Cube, pyramid, and tetrahedron have hand-authored
OBJ sources.

Importer boundaries:

- At most 255 vertices, 255 triangles, and 255 perimeter edge records per mesh.
  Edge records are not deduplicated, so this limit may arrive first.
- Quads and polygons use fan triangulation. Triangulate concave faces yourself.
- Materials, UVs, supplied normals, and smoothing groups are ignored.
- Convex outward-normal repair is not a general solution for concave solids.
  The bundled mesh named `donut` has a hard-coded exception; renaming it changes
  that behavior. Inspect normals before using other non-convex models.

## 3. Change the look cheaply

The following entry points are in [src/main.c](../src/main.c):

| Change | Entry point | Tradeoff |
| --- | --- | --- |
| Face brightness ramp | `init_palettes()` | Changes tone without more triangles |
| Lighting response | `light_for_tri_position()`, `face_level_for_intensity()` | Per-face work; not physically based |
| Default framing | `camera_distance`, `PROJECT_SCALE_X/Y` | Changes apparent size, not detail |
| Outlines | Edge pass in `render_model()` | Extra line walks; hidden edges can leak |
| Contact shadow | `draw_contact_shadow()` | Screen-space cue, not geometry |
| Light shadow / marker | `draw_light_cast_shadow()`, `draw_light_marker()` | No depth occlusion |

Keep coplanar faces on the same plane-based intensity calculation. Reintroducing
separately rounded triangle-centroid lighting can bring back diagonal seams.
Changing a palette is inexpensive, but it cannot repair visibility ordering or
increase the geometry's 80×56 sampling resolution.

The atlas generator is [make_sprite_tiles.py](../tools/make_sprite_tiles.py).
The current face pass uses its solid pattern; many line/mask patterns are unused.
Changing the atlas requires rebuilding cartridge graphics. The runtime changes
tile references and palette attributes, not pixels in graphics ROM.

## 4. Understand the display budget before increasing quality

Do not change `FRAME_W` or `FRAME_H` in isolation. The current 80×56 buffer is
coupled to four sprite bands, 16-source-tile vertical shrink behavior, and the
320×224 visible area. A wider buffer consumes more sprites per scanline; a taller
one changes source rows and display windows. The text layer is independent.

A dirty upload always converts 4,480 RAM cells into 8,960 VRAM data words.
More polygons add transform, sort, fill, and outline work on top of that fixed
display cost. More palettes do not make those operations faster.

Before attempting a larger scene, consider explicit profiling and proper
near-plane clipping. A depth buffer would solve some visibility problems but
adds memory and per-cell work. Dirty-region uploads or incremental presentation
would need careful synchronization; neither is implemented here.

## 5. Debug the right layer

| Symptom | First check |
| --- | --- |
| Host capture wrong too | Projection, face order, span bounds, normals, lighting |
| Host capture correct, MAME stretched or missing rows | Sprite shrink, band heights, active-strip overlap, SCB addresses |
| Diagonal shade seam on a flat face | Imported normals and plane offsets, not triangle centroids |
| Dark edges showing through faces | Final edge pass has no depth test |
| Non-convex surfaces overwrite each other | Mean-Z painter sorting is not a depth buffer |
| Extreme distortion near the camera | Denominator clamp is not triangle clipping |
| Light marker disappears | It can legitimately move outside the viewport |
| Short input appears ignored | Input is sampled between renders; hold longer while measuring |
| Missing SDK/BIOS or no boot | `NGDEVKIT_PREFIX`, generated hash files, emulator ROM path and test log |

`make test` runs the host-side renderer with ASan/UBSan and writes fixed-pose PPM
captures under `build/viewer/captures`. It cannot validate sprite hardware.
`make native-test` exercises actual controller inputs in MAME, checks timing and
mode changes, and writes five PNGs plus `native-test.log`.
`make screenshots` exercises all eight scenes and writes the documentation
capture candidates. None of these substitutes for physical-hardware testing.

After a renderer/model change, run the tests, inspect the native output, and
refresh the reviewed gallery as described in [SCREENSHOTS.md](SCREENSHOTS.md).
`make docs-check` deliberately flags gallery source hashes that no longer match.

## 6. Contribute or publish a derivative

Keep generated builds and BIOS files out of Git. Commit source, tests, explanatory
documentation, and reviewed gallery images together. Use a new `BUILDDIR` or a
fresh checkout for clean rebuilds; the Makefile has no `clean` target.

Follow [PUBLISHING.md](PUBLISHING.md) for CI artifacts and draft releases.
For commercial use and attribution, see [LICENSING.md](LICENSING.md) and
[NOTICE](../NOTICE). The [history guide](HISTORY.md) explains which experiments
were preserved and where the standalone project begins.
