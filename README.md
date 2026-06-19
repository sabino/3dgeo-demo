# 3Dgeo

Minimal Neo Geo/ngdevkit project that draws a static cube on the FIX layer using generated shade tiles:

- light shade: `░`
- medium shade: `▒`
- dark shade: `▓`
- full block: `█`

The shade glyphs are represented as 8x8 FIX tiles in S-ROM. For now the render is black ink on a white tile background, using a readable 40x28 visible source-art matrix in `src/main.c`.

`cube.txt` is the 40x20 reference art. The runtime matrix pads it to the Neo Geo visible FIX area of 40x28 cells.

## Build

```sh
make
```

## Run

```sh
make gngeo
```

This project follows the ngdevkit example convention of building a cart zip plus GnGeo hash data and running it with `ngdevkit-gngeo`.
