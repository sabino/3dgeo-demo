# 3Dgeo Technical Notes

3Dgeo now uses one renderer: a Doom64KB-style Neo Geo sprite microframebuffer.

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

Each frame does:

1. Rotate eight cube vertices and six face normals with fixed-point sine/cosine tables.
2. Project vertices into the 80x56 logical framebuffer.
3. Sort faces by average depth.
4. Clear the logical framebuffer to white.
5. Draw a cheap projected contact shadow.
6. Fill visible faces with an edge-walked span rasterizer.
7. Shade each face from a virtual point-light cue plus a small screen-space row bias.
8. Draw thin visible face edges for readability.
9. Upload the 80x56 framebuffer as 160 vertical sprite strips.

The hot path avoids sub-cell coverage sampling. Each polygon edge computes one fixed-point x step, then face rows are filled as simple spans. This is intentionally closer to Doom64KB's simple framebuffer writes than the older high-detail masked-cell experiment.

## Controls

- left/right: yaw
- up/down: pitch
- A/B: roll
- C/D: zoom in/out

## Build And Run

```sh
make
make gngeo
```
