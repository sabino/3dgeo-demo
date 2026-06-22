# 3Dgeo

Minimal Neo Geo/ngdevkit project that draws a controllable cube using generated shade tiles:

- light shade: `░`
- medium shade: `▒`
- dark shade: `▓`
- full block: `█`

The default renderer uses 8x8 FIX tiles in S-ROM. The cube treats each FIX cell as a tiny dithered pixel: the tile selects the coverage pattern, while the upper FIX palette bits select the lighting level.

Cube geometry is computed live on the 68000. The runtime rotates the cube vertices, projects them, rasterizes faces into a small shade-cell framebuffer, and dirty-blits changed cells during vblank. There are no baked cube frames.

Two renderer modes are available:

- `RENDERER=fix`: 40x28 FIX-cell framebuffer, default.
- `RENDERER=sprite`: 20x14 native 16x16 sprite-cell framebuffer using C-ROM shade tiles at tile 256 and a sticky sprite chain.
- `RENDERER=sprite40`: 40x28 sprite-cell framebuffer using half-size sprite shrink, richer shade levels, and directional edge tiles.

Controls:

- left/right: yaw
- up/down: pitch
- A/B: roll

The visible FIX area used here is 40x28 cells. The full FIX map is 40x32 cells, with the top and bottom two rows hidden on NTSC displays.

`cube.txt` is kept as the 40x20 static reference art from the first pass.

## Build

```sh
make
make RENDERER=sprite
make RENDERER=sprite40
```

## Run

```sh
make gngeo
make gngeo-sprite
make gngeo-sprite40
```

This project follows the ngdevkit example convention of building a cart zip plus GnGeo hash data and running it with `ngdevkit-gngeo`.
