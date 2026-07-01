# 3Dgeo

3Dgeo is an experimental Neo Geo/ngdevkit renderer that treats the sprite
hardware as a tiny grayscale framebuffer for live 3D objects. It is not a
general game engine and it is not a finished rendering library. The goal is to
explore how far a framebuffer-like 3D pipeline can be pushed on Neo Geo while
still using normal cartridge assets, the 68000 CPU, and the stock ngdevkit
build flow.

The current ROM renders OBJ-derived primitive models through a sprite-backed
80x56 logical framebuffer. Each logical cell is represented by one 16x16 C-ROM
tile shrunk by sprite zoom to a visible 4x4 block. The 68000 rotates vertices
and normals, projects triangles, sorts faces, fills spans, applies simple
point-light shading, draws optional silhouette/detail edges, and uploads the
result to sprite VRAM every frame.

## Status

This is research code for a public Neo Geo experiment:

- The renderer is intentionally small, fixed-point, and approximate.
- OBJ files are imported at build time, not parsed by the ROM at runtime.
- Lighting is a lightweight visual cue, not a physically correct model.
- Visibility uses painter-style triangle sorting and back-face checks, not a
  Z-buffer.
- Dense meshes are expected to be slow on real hardware. The bundled ball and
  donut are deliberately low-poly.
- The code is currently optimized for readability and iteration, not for a
  locked production ABI.

## Features

- ngdevkit cart build with generated `p1`, `s1`, `c1`, `c2`, `m1`, and `v1`
  ROM data.
- Sprite microframebuffer using 160 sprite strips.
- FIX layer text overlay for controls and menu labels.
- Build-time OBJ conversion into compact C arrays.
- In-ROM picker for cube, pyramid, tetrahedron, ball, donut, cone, cylinder,
  and a cube light-control scene.
- Per-face grayscale palette shading.
- Zoom, yaw, pitch, and roll controls.
- Movable light marker in the `CUBE LIGHT` scene.

## Requirements

- ngdevkit installed and available through `pkg-config`.
- `m68k-neogeo-elf-gcc`, `tiletool.py`, and `romtool.py` in `PATH`.
- Python 3.
- Pillow for the asset generation scripts.
- `ngdevkit-gngeo` if you want to use `make gngeo`.

This repository does not include a Neo Geo BIOS, emulator, or ngdevkit itself.

## Build

```sh
make
```

The cart zip and hash files are written under `build/rom/`.

## Run

```sh
make gngeo
```

The Makefile uses this keyboard mapping for GnGeo:

- Neo Geo D-pad: arrow keys
- Neo Geo A/B/C/D: `Z` / `X` / `A` / `S`
- Neo Geo START: `1`
- Neo Geo COIN: `3`
- Emulator menu: `Esc`

## ROM Controls

Picker:

- D-pad left/right: choose a model or `CUBE LIGHT`
- A: enter the selected mode

Model modes:

- D-pad: rotate yaw and pitch
- A/B: roll
- C/D: zoom in and out
- COIN: return to picker

`CUBE LIGHT` mode:

- D-pad: move the light on screen
- A/B: move the light nearer or farther in depth
- C/D: zoom in and out
- COIN: return to picker

## Project Layout

- `src/main.c` - Neo Geo runtime, renderer, input handling, and UI overlay.
- `assets/models/*.obj` - readable source meshes imported at build time.
- `tools/obj_to_model.py` - OBJ-to-C converter for the renderer.
- `tools/make_primitive_objs.py` - regenerates the low-poly primitive OBJ
  assets.
- `tools/make_sprite_tiles.py` - generates the C-ROM tile atlas used by the
  sprite framebuffer.
- `tools/make_fix_tiles.py` - generates the S-ROM/FIX font and tile data.
- `TECHNICAL_EXPLANATION.md` - detailed architecture and pipeline notes.

## OBJ Import

The converter supports a deliberately small subset of Wavefront OBJ:

- `v` vertex records.
- `f` face records.
- Face index forms such as `1`, `1/2`, `1/2/3`, and `1//3`.
- Negative face indices.
- Fan triangulation for quads and polygons.
- Centered bounding-box normalization.
- Generated fixed-point triangle and edge normals.

Runtime indices are currently `u8`, so each model is capped at 255 vertices,
255 triangles, and 255 perimeter edges. Materials, textures, vertex normals,
groups, smoothing flags, and runtime OBJ text parsing are not implemented.

## Notes On The Rendering Approach

The sprite microframebuffer approach is inspired by the general technique used
by fast Neo Geo framebuffer experiments such as Doom64KB: many narrow sprite
columns become a low-resolution writable display. This project uses its own
asset generators and renderer code. No Doom64KB, DoomGeo, or NGRayEx source or
assets are included in this repository.

For the full technical walkthrough, read
[`TECHNICAL_EXPLANATION.md`](TECHNICAL_EXPLANATION.md).

## License

This project is released under the GNU General Public License v3.0 or later
(`GPL-3.0-or-later`). See [`LICENSE`](LICENSE).
