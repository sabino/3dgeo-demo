# 3Dgeo

Minimal Neo Geo/ngdevkit project that draws a controllable cube using generated shade tiles:

- light shade: `░`
- medium shade: `▒`
- dark shade: `▓`
- full block: `█`

The default renderer uses 8x8 FIX tiles in S-ROM. The cube treats each FIX cell as a tiny dithered pixel: the tile selects the coverage pattern, while the upper FIX palette bits select the lighting level.

Cube geometry is computed live on the 68000. The runtime rotates the cube vertices, projects them, rasterizes faces into a small shade-cell framebuffer, and dirty-blits changed cells during vblank. There are no baked cube frames.

The preferred `sprite40` renderer now uses a masked-cell rasterizer: each framebuffer cell is sampled at four sub-cell points, silhouette cells choose one of fifteen generated 2x2 coverage masks, and fully covered cells stay solid. This keeps the cube sprite-backed and low-bandwidth while making the outline read less like large square blocks. `sprite40` also adds a sprite-only soft contact shadow, face-local light gradients, and a default top/side view closer to the reference cube image.

Four renderer modes are available:

- `RENDERER=fix`: 40x28 FIX-cell framebuffer, default.
- `RENDERER=sprite`: 20x14 native 16x16 sprite-cell framebuffer using C-ROM shade tiles at tile 256 and a sticky sprite chain.
- `RENDERER=sprite40`: 40x28 sprite-cell framebuffer using half-size sprite shrink, richer shade levels, sub-cell coverage masks, and a subtle contact shadow.
- `RENDERER=microfb`: 80x56 sprite microframebuffer inspired by Doom64KB's Neo Geo backend. Each logical cell is a 16x16 C-ROM tile shrunk to 4x4 pixels; each column is split into two 28-cell sprite strips so the full framebuffer stays under the per-scanline sprite budget and the 32-tile-per-sprite height limit. This mode uses a lightweight span rasterizer with a virtual point-light cue, contact shadow, and thin face edges instead of the heavier sub-cell coverage path used by `sprite40`.

Controls:

- left/right: yaw
- up/down: pitch
- A/B: roll
- C/D: zoom in/out

The visible FIX area used here is 40x28 cells. The full FIX map is 40x32 cells, with the top and bottom two rows hidden on NTSC displays.

`cube.txt` is kept as the 40x20 static reference art from the first pass.

## Build

```sh
make
make RENDERER=sprite
make RENDERER=sprite40
make RENDERER=microfb
```

On machines without GnGeo/ngdevkit installed, preview captures can still be regenerated from a Python mirror of the sprite40 renderer:

```sh
make captures-preview
```

These PNGs are for visual review only; final ROM verification should still use GnGeo or hardware.

## Run

```sh
make gngeo
make gngeo-sprite
make gngeo-sprite40
make gngeo-microfb
```

This project follows the ngdevkit example convention of building a cart zip plus GnGeo hash data and running it with `ngdevkit-gngeo`.
