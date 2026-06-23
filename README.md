# 3Dgeo

Neo Geo/ngdevkit project that draws a controllable OBJ-imported 3D model through a sprite-backed microframebuffer.

The renderer is based on the Doom64KB Neo Geo approach: an 80x56 logical framebuffer where each logical cell is a 16x16 C-ROM tile shrunk by the Neo Geo sprite hardware to a 4x4 visible pixel. The framebuffer uses 160 sprite strips total: 80 columns split into two 28-cell vertical chunks, keeping the display under both the per-scanline sprite budget and the 32-tile-per-sprite height limit.

The model is computed live on the 68000. The runtime rotates vertices and normals, projects the model, fills visible triangles with a lightweight edge-walked span rasterizer, applies a cheap virtual point-light cue, draws a contact shadow, and adds thin triangle edges for readability. There are no baked animation frames.

OBJ files are converted at build time into compact C arrays. The Neo Geo runtime does not parse OBJ text.

Controls:

- left/right: yaw
- up/down: pitch
- A/B: roll
- C/D: zoom in/out

## Build

```sh
make
```

The default model is `assets/models/cube.obj`. To build another imported model:

```sh
make MODEL=pyramid
make MODEL=tetrahedron
```

## Run

```sh
make gngeo
```

You can also launch another model directly:

```sh
make MODEL=pyramid gngeo
```

## OBJ Import

The import tool is `tools/obj_to_model.py`. It currently supports:

- `v` vertices
- `f` faces
- OBJ face index forms such as `1`, `1/2`, `1/2/3`, and `1//3`
- negative face indices
- quad or polygon triangulation by fan
- centered bounding-box normalization
- precomputed fixed-point triangle normals

Current first-pass limits are 255 vertices and 255 triangles, using `u8` runtime indices.

This project follows the ngdevkit example convention of building a cart zip plus GnGeo hash data and running it with `ngdevkit-gngeo`.
