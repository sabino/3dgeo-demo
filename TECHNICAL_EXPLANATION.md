# 3Dgeo Technical Explanation

3Dgeo is a Neo Geo/ngdevkit experiment that renders live 3D objects through a
sprite-backed microframebuffer. The ROM still uses normal Neo Geo layers:
sprites draw the 3D framebuffer and the FIX layer draws text. The unusual part
is that the sprite layer is arranged as a dense grid of zoomed tiles so the
68000 can update it like a very small software framebuffer.

The design favors predictable hardware cost over visual precision. It uses a
small logical resolution, fixed-point math, flat shaded triangles, and a compact
model format generated at build time.

## Hardware Layer Use

Sprites:

- Draw the full 3D image.
- Provide the 80x56 logical framebuffer.
- Use 160 hardware sprite strips: 80 columns times 2 vertical chunks.
- Use C-ROM tiles as cell patterns.
- Use sprite palette attributes as the grayscale shade selector.

FIX:

- Draws menu and control text.
- Uses a generated 5x7 uppercase font in the S-ROM.
- Sits above sprites, so it is intentionally limited to a small overlay.

Palettes:

- `PAL_PAPER` is the normal white background.
- `PAL_FACE_0` through `PAL_FACE_11` are grayscale face levels.
- `PAL_EDGE` is the dark line color.
- `PAL_SHADOW_LIGHT` is used by the dark-background light-control scene.
- `PAL_SHADOW_DARK` is used for contact shadows.

Sound:

- The cart uses ngdevkit's null sound driver. Audio is out of scope.

## Logical Framebuffer

The renderer uses:

- Logical width: 80 cells.
- Logical height: 56 cells.
- Visible cell size: 4x4 pixels.
- Approximate visible framebuffer size: 320x224 pixels.
- Source sprite tile size: 16x16 pixels.
- Sprite zoom: configured so each 16x16 tile displays as a 4x4 block.

Each framebuffer column is represented by two vertical sprite strips. The split
keeps every strip at 28 tiles high, below the Neo Geo 32-tile-per-sprite
height limit:

```text
80 columns x 2 chunks = 160 sprite strips
56 rows / 2 chunks = 28 cells per strip
```

This layout also keeps the sprite count under the visible hardware budget and
avoids building one hardware sprite per logical pixel.

## Tile Atlas

`tools/make_sprite_tiles.py` generates the C-ROM atlas used by the sprite
framebuffer. Runtime code uses tile number plus palette to describe a cell:

```c
word = (palette << 12) | tile;
```

The C-ROM atlas reserves the first 256 sprite tiles and places framebuffer
patterns after that. The runtime adds `SPRITE_START_TILE` before writing the
tile index to sprite VRAM.

The atlas contains:

- Clear and paper tiles.
- Ordered-dither shade tiles.
- Solid shade tiles used for filled 3D cells.
- Simple edge tiles retained for line experiments.
- Coverage-mask tiles retained from earlier masked-cell renderer tests.

The current hot path primarily uses solid shade cells. That is faster and more
stable than trying to update fine coverage patterns for every cell.

## Build-Time Model Pipeline

The ROM does not parse text OBJ files. OBJ is a source format for the build.

`tools/obj_to_model.py` reads every model listed by the Makefile and emits
`build/generated/model_data.h`. The generated header contains compact C arrays
for the Neo Geo runtime:

- Fixed-point vertices.
- Triangulated faces.
- Per-triangle normals.
- Per-edge normals.
- A model table with names and counts.

The converter supports:

- `v` vertex records.
- `f` face records.
- Positive and negative OBJ indices.
- Face index forms with optional texture and normal fields.
- Fan triangulation for quads and polygons.
- Centered bounding-box normalization to a fixed radius.

The converter intentionally ignores:

- Materials.
- Textures.
- OBJ vertex normals.
- Groups and smoothing flags.
- Runtime streaming or loading from memory card.

Current hard limits come from `u8` runtime indices:

- 255 vertices per model.
- 255 triangles per model.
- 255 perimeter edges per model.

The bundled ball and donut are intentionally low-poly. Dense curved models look
better but can become too slow for the target frame rate.

## Primitive Assets

`tools/make_primitive_objs.py` generates readable primitive OBJ files:

- `ball.obj`
- `cone.obj`
- `cylinder.obj`
- `donut.obj`

The hand-maintained source assets are:

- `cube.obj`
- `pyramid.obj`
- `tetrahedron.obj`

The torus/donut is non-convex, so the converter keeps its original face normal
orientation instead of applying the convex outward-normal correction used for
the other bundled models.

## Runtime Render Pipeline

Each active frame follows this order:

1. Read Neo Geo controller input.
2. Update model rotation, zoom, or light position.
3. Rotate model vertices with fixed-point sine/cosine lookup tables.
4. Project rotated vertices into the 80x56 framebuffer.
5. Rotate triangle normals.
6. Compute average triangle depth.
7. Sort triangles from far to near.
8. Clear the logical framebuffer.
9. Draw a contact shadow or light-scene cast shadow.
10. Fill visible triangles as horizontal spans.
11. Draw optional model edges.
12. Draw picker markers or the light marker when needed.
13. Wait for vblank.
14. Upload the logical framebuffer to sprite VRAM.

There are no baked animation frames. The model is transformed every frame on
the 68000.

## Math Model

Angles use 64 steps per revolution:

```text
0..63 -> 360 degrees
```

Sine and cosine are Q8 fixed-point values:

```text
256 == 1.0
```

Rotation is applied in pitch, yaw, roll order. The same rotation function is
used for vertices and normals.

Projection is perspective-like but deliberately cheap:

```text
screen_x = center_x + x * scale_x / (z + camera_distance)
screen_y = center_y - y * scale_y / (z + camera_distance)
```

The denominator is clamped so geometry near the camera does not explode.

## Triangle Filling

The rasterizer is span-based:

1. Find the triangle's screen-space vertical bounds.
2. Walk each edge and record left/right x limits for each framebuffer row.
3. Clamp spans to the 80x56 target.
4. Fill each row with one shade word.

This intentionally avoids sub-cell coverage, barycentric interpolation, and a
Z-buffer. The renderer trades precision for speed and simplicity.

## Lighting

Lighting is a stylized point-light cue:

```text
intensity = dot(face_normal, light_position - face_center)
```

The intensity is scaled into 12 grayscale palette levels. A small screen-space
row bias is added so large flat faces get more visual shape even at the coarse
cell resolution.

The `CUBE LIGHT` scene switches to a darker background, exposes the light
position through the controller, and draws a white marker whose radius changes
with projected depth. This mode is meant to make the shading model easier to
inspect.

## Edges

Edges are optional per model:

- Cube, pyramid, tetrahedron, cone, and cylinder keep dark edge lines for
  readability.
- Ball and donut skip edge drawing because dense edge sets make them noisy and
  slower.

Edges are generated from original OBJ face perimeters, not from a mesh
deduplication pass. That is simple and predictable, but it can draw duplicate
lines when neighboring faces share an edge.

## Sprite Upload

The logical framebuffer lives in `frame_words[FRAME_H][FRAME_W]`. When it is
dirty, the ROM writes every cell into sprite VRAM:

- SCB1 receives tile index and palette attribute words for each strip.
- SCB2 is initialized once with strip position, height, and zoom.
- Each frame upload touches 80 columns times 56 cells.

The strip positions are static, so per-frame work is mostly tile and attribute
updates. This is the core reason the sprite microframebuffer path is faster
than the earlier experiments that tried to use the FIX layer for the whole
image.

## Input Details

The ROM reads:

- D-pad and A/B/C/D from `REG_P1CNT`.
- COIN/select from `REG_STATUS_B`.

START is intentionally not used by the ROM. In this ngdevkit setup, the
constants for `CNT_UP` and `CNT_START1` both use bit 0, but they apply to
different hardware input bytes. Earlier builds treated them as interchangeable,
which made UP behave like a mode reset. The current ROM uses COIN to return to
the picker and leaves START alone.

## Performance Notes

The fast path comes from a few constraints:

- Low logical resolution.
- One word per framebuffer cell.
- Static sprite strip layout.
- Fixed-point rotation and projection.
- One shade per triangle span row.
- Small model counts.
- No runtime OBJ parser.
- No per-pixel depth buffer.

The expensive parts are:

- Rotating all vertices and normals.
- Sorting all triangles.
- Filling spans for every visible face.
- Uploading the whole dirty framebuffer to VRAM.

The ball and donut were reduced in complexity because curved meshes cost more
triangles and more spans. They are still recognizable, but they are intentionally
coarse to keep interaction responsive.

## Known Limitations

- No Z-buffer.
- No clipping against the near plane.
- No material or texture support.
- No runtime `.obj` loading.
- No sub-cell antialiasing.
- No true perspective-correct interpolation.
- No hidden-edge merge pass.
- No audio.
- No formal hardware benchmark pass yet.

## Public Release Notes

This repository should be treated as an experiment and reference prototype. It
is useful for studying a Neo Geo sprite microframebuffer, build-time OBJ import,
and fixed-point 3D rendering under severe constraints. It should not be treated
as a drop-in engine for a finished commercial-style Neo Geo game without more
profiling, cleanup, and hardware testing.
