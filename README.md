# 3Dgeo

Neo Geo/ngdevkit project that draws a controllable 3D cube through a sprite-backed microframebuffer.

The renderer is based on the Doom64KB Neo Geo approach: an 80x56 logical framebuffer where each logical cell is a 16x16 C-ROM tile shrunk by the Neo Geo sprite hardware to a 4x4 visible pixel. The framebuffer uses 160 sprite strips total: 80 columns split into two 28-cell vertical chunks, keeping the display under both the per-scanline sprite budget and the 32-tile-per-sprite height limit.

The cube is computed live on the 68000. The runtime rotates vertices and normals, projects the cube, fills visible faces with a lightweight edge-walked span rasterizer, applies a cheap virtual point-light cue, draws a contact shadow, and adds thin face edges for readability. There are no baked cube frames.

Controls:

- left/right: yaw
- up/down: pitch
- A/B: roll
- C/D: zoom in/out

## Build

```sh
make
```

## Run

```sh
make gngeo
```

This project follows the ngdevkit example convention of building a cart zip plus GnGeo hash data and running it with `ngdevkit-gngeo`.
