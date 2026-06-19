# 3Dgeo

Minimal Neo Geo/ngdevkit project that fills the FIX layer with generated shade tiles in a repeating light-to-dark sequence:

- light shade: `░`
- medium shade: `▒`
- dark shade: `▓`
- full block: `█`

The shade glyphs are represented as four 8x8 FIX tiles in S-ROM. For now each tile is black ink on a white tile background, repeated across the full 40x32 FIX map.

## Build

```sh
make
```

## Run

```sh
make gngeo
```

This project follows the ngdevkit example convention of building a cart zip plus GnGeo hash data and running it with `ngdevkit-gngeo`.
