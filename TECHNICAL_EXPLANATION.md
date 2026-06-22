# 3Dgeo Technical Explanation

This project is a Neo Geo/ngdevkit experiment for rendering a live 3D cube through tile-like framebuffers. The main idea is to treat Neo Geo display primitives as coarse pixels:

- FIX mode uses 8x8 FIX tiles in S-ROM as framebuffer cells.
- Sprite mode uses native 16x16 C-ROM sprite tiles as framebuffer cells.
- Sprite40 mode uses 16x16 C-ROM sprite tiles shrunk to roughly 8x8 display cells, giving a 40x28 sprite-backed framebuffer.

The cube is not a baked animation. The 68000 code rotates cube vertices every frame when input changes, projects them to screen-space cells, rasterizes the faces, and then dirty-blits only changed cells to Neo Geo VRAM.

## Build Layout

The project has one source file and generated graphics assets:

- `src/main.c`: 68000 runtime, renderer, input, projection, rasterization, and blitters.
- `tools/make_fix_tiles.py`: generates FIX tile GIF input for `tiletool.py`.
- `tools/make_sprite_tiles.py`: generates C-ROM sprite tile GIF input for `tiletool.py`.
- `Makefile`: builds cart ROMs and emulator hash files for all renderer modes.
- `build/captures/`: screenshots captured from GnGeo while rotating and zooming the cube.

The Makefile supports three renderer variants:

```sh
make                  # RENDERER=fix, default
make RENDERER=sprite  # coarse 20x14 native sprite cells
make RENDERER=sprite40
```

Run targets:

```sh
make gngeo
make gngeo-sprite
make gngeo-sprite40
```

Each renderer gets its own build directory:

- `build/fix`
- `build/sprite`
- `build/sprite40`

That separation prevents stale object files when switching compile-time renderer flags.

## Controls

The current GnGeo mapping in the Makefile is:

```make
A=K122,B=K120,C=K97,D=K115,START=K49,COIN=K51,UP=K82,DOWN=K81,LEFT=K80,RIGHT=K79,MENU=K27
```

So the effective keyboard controls are:

- Arrow keys: yaw and pitch.
- `Z` / `X`: roll.
- `A` / `S`: zoom in and zoom out.
- `Esc`: emulator menu.

In Neo Geo button terms:

- Left/right: yaw.
- Up/down: pitch.
- A/B: roll.
- C/D: zoom in/out.

## Geometry Pipeline

The cube is defined as eight 3D vertices and six quad faces:

```c
static const Vec3 cube_vertices[8] = { ... };
static const Face cube_faces[6] = { ... };
```

The renderer uses fixed-point integer math with `Q = 8`, meaning `ONE == 256`.

Rotation is performed by `rotate_vec()`:

1. Rotate around X for pitch.
2. Rotate around Y for yaw.
3. Rotate around Z for roll.

Angles are 6-bit indices into a 64-entry sine table:

```c
#define ANGLE_MASK 63
static const s16 sin_lut[64] = { ... };
```

This avoids floating point and keeps the runtime appropriate for the 68000.

Projection is done in `project_vertex()`:

```c
sx = FRAME_W / 2 + (x * PROJECT_SCALE_X) / (z + camera_distance)
sy = FRAME_H / 2 - (y * PROJECT_SCALE_Y) / (z + camera_distance)
```

`camera_distance` is runtime-adjustable through C/D. Smaller values zoom in; larger values zoom out.

## Face Sorting And Rasterization

Each frame, `render_cube()`:

1. Rotates and projects all eight vertices.
2. Rotates the six face normals.
3. Computes average face depth.
4. Painter-sorts the faces from back to front.
5. Clears the logical framebuffer to paper.
6. Fills each face into the framebuffer.
7. For non-sprite40 modes, draws edge overlays.

The face fill path checks cell centers against each projected quad using edge-cross tests:

```c
static u8 point_in_quad(...)
```

This is simple and readable. It is not the fastest possible polygon filler; a scanline fill would be faster, especially if this grows beyond a cube.

## Logical Framebuffer

The logical framebuffer is:

```c
static u16 frame_words[FRAME_H][FRAME_W];
static u16 prev_words[FRAME_H][FRAME_W];
```

Each cell stores one Neo Geo tile word:

```c
palette << 12 | tile
```

The dirty blitter compares `frame_words` to `prev_words` and writes only changed cells.

This is important because VRAM writes are the real cost. Full 40x28 refresh means 1120 cells. Dirty blitting is usually much less when the cube is not changing, and bounded when it is rotating.

## FIX Renderer

`RENDERER=fix` uses:

- `FRAME_W = 40`
- `FRAME_H = 28`
- FIX map rows 2 through 29

The full FIX map is 40x32, but top and bottom rows are hidden on normal NTSC display framing. The project therefore treats 40x28 as the visible working area.

FIX cells are written with:

```c
ADDR_FIXMAP + (col << 5) + row
```

FIX is useful for simple full-screen tile experiments, but it draws above sprites. That caused an early white-screen issue in sprite mode: the sprite cube was behind an opaque white FIX map. Sprite modes now clear FIX with a transparent tile.

## Sprite Renderer

`RENDERER=sprite` uses:

- `FRAME_W = 20`
- `FRAME_H = 14`
- Full-size 16x16 sprite tiles
- One vertical sprite column per framebuffer column
- Sticky sprites for the columns after the first

This is the fastest coarse sprite-backed test. It uses fewer cells than FIX mode, so fewer VRAM writes.

## Sprite40 Renderer

`RENDERER=sprite40` uses:

- `FRAME_W = 40`
- `FRAME_H = 28`
- Sprite shrink value `SPRITE_ZOOM = 0x077f`
- C-ROM tiles at tile 256+
- One sprite column per framebuffer column

The goal is to approximate the FIX renderer's 40x28 resolution while using sprites instead of FIX.

The Neo Geo sprite system is a good fit because the hardware is built around many vertical sprite strips. The project initializes one strip per framebuffer column, then updates SCB1 tile/palette entries for cells that changed.

Sprite control block usage:

- SCB1: tile number and palette attributes for each vertical tile.
- SCB2: shrink/zoom.
- SCB3: Y position, sticky bit, and sprite height.
- SCB4: X position.

The first column has position and height. Later columns are sticky, so they follow the previous column.

## Tile Atlas

The generated C-ROM sprite atlas starts at tile 256. The first 256 C-ROM tiles are avoided because ngdevkit examples and Neo Geo BIOS conventions reserve early tiles for BIOS/eyecatcher behavior.

The current logical atlas includes:

- Clear tile.
- 9 dither shade tiles.
- Paper tile.
- Horizontal, vertical, diagonal, shallow, and steep edge masks.

Sprite40 currently uses solid shade tile `TILE_SHADE_9` with palette changes for lighting. Earlier line-mask experiments produced a broken wireframe look, so the current sprite40 mode intentionally avoids edge overlays and lets face contrast define the cube.

## Lighting

Face lighting is based on a rotated face normal and a fixed light vector:

```c
Vec3 light = {-104, -150, -186};
intensity = dot(normal, light) >> Q;
```

In `sprite40`, the lighting path uses a 12-step grayscale palette ramp:

```c
PAL_FACE_0  = 0x0222
...
PAL_FACE_11 = 0x0eee
```

The renderer maps lighting intensity to a palette level:

```c
level = (intensity + 170) / 32
```

Then it uses:

```c
palette = PAL_FACE_0 + level
tile = TILE_SHADE_9
```

This is hardware-friendly: the tile shape stays constant while the palette determines brightness.

Sprite40 also adds a cheap per-cell gradient within each face:

```c
gradient = ((FRAME_H / 2 - y) * 4) - ((x - FRAME_W / 2) * 2)
```

This gives the faces more visual variation than a single flat shade per face. It is still stylized and banded because the display resolution is only 40x28 cells, but it is closer to the reference cube than the wireframe version.

## Zoom

Zoom is implemented as camera distance, not sprite hardware zoom:

```c
#define CAMERA_DISTANCE_DEFAULT 700
#define CAMERA_DISTANCE_MIN 360
#define CAMERA_DISTANCE_MAX 1200
#define CAMERA_DISTANCE_STEP 24
```

Button C decreases camera distance, zooming in. Button D increases it, zooming out.

This preserves the sprite cell layout and avoids reconfiguring SCB2 every frame. It also matches how a real 3D camera would behave.

## Performance Notes

Current performance is good because:

- Angles use a small sine lookup table.
- Rotation is fixed-point integer math.
- The cube has only 8 vertices and 6 faces.
- Renderer writes only dirty cells.
- Sprite columns are initialized once; per-frame updates mostly touch SCB1 cell entries.
- Sprite40 keeps SCB2/SCB3/SCB4 stable while changing tile/palette cells.

Main costs:

- Per-cell point-in-quad tests.
- Dirty comparisons over the full logical framebuffer.
- VRAM writes for changed cells during rotation.

Potential next optimizations:

- Replace point-in-quad fill with scanline polygon fill.
- Track bounding boxes per face to reduce framebuffer comparisons.
- Store cell tile and palette in separate arrays if palette-only updates become common.
- Precompute rotation matrices or use smaller angle tables if more objects appear.
- Add a frame budget counter or visible debug timing mode.

## Visual Roadmap

The current result is a solid low-resolution shaded cube. The next visual upgrades should be tile-atlas driven:

1. More face masks:
   - 16 or 32 coverage levels.
   - Ordered dithers with multiple phases.
   - Sub-cell edge masks that preserve face fill instead of replacing it.

2. Better edge treatment:
   - Do not use wireframe lines globally.
   - Add only silhouette/crease darkening masks where face contrast is insufficient.
   - Use corner and junction masks to avoid broken line fragments.

3. Better lighting:
   - Separate directional gradients per face.
   - Palette ramps for highlight, midtone, and shadow.
   - Optional shadow plane or contact shadow under the cube.

4. Better resolution illusion:
   - Use many pre-generated masks for partial cell coverage.
   - Approximate sub-pixel polygon edges by choosing the closest mask tile.
   - This is closer to a software framebuffer while staying Neo Geo-native.

## Captures

The latest captured frames are in `build/captures/`:

- `01-initial.png`
- `02-yaw-right.png`
- `03-yaw-pitch.png`
- `04-zoom-in.png`
- `05-zoom-out.png`
- `sprite40-rotation-contact.png`

These were captured from the emulator window without moving or resizing the window.

## Git History

The repository history documents the renderer evolution:

1. Initial ngdevkit FIX shade demo.
2. Static FIX shade matrix.
3. Palette-based FIX lighting.
4. Precomputed cube rotations.
5. Live 3D cube rotation into a FIX framebuffer.
6. Coarse sprite framebuffer.
7. Sprite visibility fix for FIX-over-sprite ordering.
8. Higher-detail sprite40 renderer.
9. Edgeless solid-face sprite40 rendering.
10. Zoom controls and smoother lighting.

The zip package includes `.git`, so the full commit history is available after extraction.
