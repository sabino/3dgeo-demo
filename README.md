# 3Dgeo

Minimal Neo Geo/ngdevkit project that draws a controllable cube on the FIX layer using generated shade tiles:

- light shade: `░`
- medium shade: `▒`
- dark shade: `▓`
- full block: `█`

The shade glyphs are represented as 8x8 FIX tiles in S-ROM. The cube treats each FIX cell as a tiny dithered pixel: the tile selects the coverage pattern, while the upper FIX palette bits select the lighting level.

Cube geometry is precomputed at build time into a 32 yaw x 8 pitch angle table. Runtime input only selects the next angle and applies a precomputed FIX-map transition, so no polygon projection or rasterization runs on the Neo Geo CPU.

Controls:

- left/right: rotate yaw
- up/down: rotate pitch

The visible FIX area used here is 40x28 cells. The full FIX map is 40x32 cells, with the top and bottom two rows hidden on NTSC displays.

`cube.txt` is kept as the 40x20 static reference art from the first pass.

## Build

```sh
make
```

## Run

```sh
make gngeo
```

This project follows the ngdevkit example convention of building a cart zip plus GnGeo hash data and running it with `ngdevkit-gngeo`.
