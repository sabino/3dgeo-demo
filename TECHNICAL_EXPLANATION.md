# 3Dgeo Technical Notes

3Dgeo now uses one renderer: a Doom64KB-style Neo Geo sprite microframebuffer fed by build-time OBJ model data.

## Framebuffer Layout

- Logical resolution: 80x56 cells.
- Visible cell size: 4x4 pixels.
- Hardware representation: each logical cell is one 16x16 C-ROM tile shrunk by sprite zoom.
- Sprite layout: 80 columns split into two vertical chunks.
- Sprite count: 160 display strips, starting at sprite 1.
- Chunk height: 28 source tiles, staying under the Neo Geo 32-tile-per-sprite limit.

The FIX layer is cleared and kept out of the way. The cube is rendered entirely through sprites.

## Tile And Palette Model

The sprite asset generator keeps the same shade tile set:

- paper/clear tiles
- solid shade tiles
- simple edge/shade support tiles
- coverage-mask tiles retained in the asset atlas for compatibility

Runtime cell words use the Neo Geo sprite tile index for coverage/pattern and the sprite palette attribute for grayscale light level.

## Render Loop

OBJ models are converted before compilation by `tools/obj_to_model.py`. The converter parses vertices and faces, normalizes the bounding box around the origin, triangulates faces, and emits `build/generated/model_data.h` with fixed-point vertices, normals, original perimeter edges, and a model table. The 68000 runtime receives compact arrays only; it does not parse OBJ text.

Each frame does:

1. Rotate imported model vertices and triangle normals with fixed-point sine/cosine tables.
2. Project vertices into the 80x56 logical framebuffer.
3. Sort triangles by average depth.
4. Clear the logical framebuffer to white.
5. Draw a cheap projected contact shadow.
6. Fill visible triangles with an edge-walked span rasterizer.
7. Shade each triangle from a virtual point-light cue plus a small screen-space row bias.
8. Draw thin visible triangle edges for readability.
9. Upload the 80x56 framebuffer as 160 vertical sprite strips.

The hot path avoids sub-cell coverage sampling. Each polygon edge computes one fixed-point x step, then face rows are filled as simple spans. This is intentionally closer to Doom64KB's simple framebuffer writes than the older high-detail masked-cell experiment.

## OBJ Import Limits

- Bundled models: `assets/models/cube.obj`, `assets/models/pyramid.obj`, and `assets/models/tetrahedron.obj`.
- The ROM starts in a picker: left/right selects cube, pyramid, tetrahedron, or the BALL light-control demo. Neo Geo A enters the selected mode, and COIN returns to the picker.
- The BALL option keeps the cube model spinning, switches the D-pad to move the light source, draws the light as a white marker in the microframebuffer, and uses Neo Geo A/B for light depth.
- Runtime indices are `u8`, so the converter caps imports at 255 vertices and 255 triangles.
- Materials, texture coordinates, and OBJ normals are ignored for now.
- Normals are generated from triangle geometry and stored in Q8 fixed point.

## Controls

- GnGeo keyboard mapping: arrow keys are the Neo Geo D-pad, `Z`/`X`/`A`/`S` are Neo Geo A/B/C/D, `1` is START, and `3` is COIN.
- Input is read from two hardware bytes: D-pad/buttons come from `REG_P1CNT`, while only the COIN/select bit is masked from `REG_STATUS_B`. START is not used by the ROM because ngdevkit's `CNT_UP` and `CNT_START1` constants both use bit 0 for different input bytes.
- Picker: left/right selects the option, Neo Geo A enters it.
- Cube/pyramid/tetrahedron: D-pad rotates yaw/pitch, Neo Geo A/B rolls, and Neo Geo C/D zooms.
- BALL: D-pad moves the white light on screen, Neo Geo A/B moves the light in depth, Neo Geo C/D zooms, and COIN returns to the picker.

## Build And Run

```sh
make
make gngeo
```
