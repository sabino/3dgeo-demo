#!/usr/bin/env python3
"""Generate reserved C-ROM tiles plus sprite framebuffer shade/line tiles."""

from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image


TILE_SIZE = 16
START_TILE = 256
ATLAS_TILE_COUNT = 19
TILE_COUNT = START_TILE + ATLAS_TILE_COUNT

CLEAR = 0
SHADE_FIRST = 1
SHADE_LAST = 9
PAPER = 10
EDGE_H = 11
EDGE_V = 12
EDGE_DIAG_DOWN = 13
EDGE_DIAG_UP = 14
EDGE_SHALLOW_DOWN = 15
EDGE_SHALLOW_UP = 16
EDGE_STEEP_DOWN = 17
EDGE_STEEP_UP = 18

BAYER_8X8 = (
    (0, 48, 12, 60, 3, 51, 15, 63),
    (32, 16, 44, 28, 35, 19, 47, 31),
    (8, 56, 4, 52, 11, 59, 7, 55),
    (40, 24, 36, 20, 43, 27, 39, 23),
    (2, 50, 14, 62, 1, 49, 13, 61),
    (34, 18, 46, 30, 33, 17, 45, 29),
    (10, 58, 6, 54, 9, 57, 5, 53),
    (42, 26, 38, 22, 41, 25, 37, 21),
)


def near(value: int, target: int, width: int) -> bool:
    return abs(value - target) <= width


def edge_on(tile: int, x: int, y: int) -> bool:
    mid = TILE_SIZE // 2
    width = 1
    if tile == EDGE_H:
        return near(y, mid, width)
    if tile == EDGE_V:
        return near(x, mid, width)
    if tile == EDGE_DIAG_DOWN:
        return near(y, x, width)
    if tile == EDGE_DIAG_UP:
        return near(y, TILE_SIZE - 1 - x, width)
    if tile == EDGE_SHALLOW_DOWN:
        return near((y - mid) * 2, x - mid, width * 2)
    if tile == EDGE_SHALLOW_UP:
        return near((y - mid) * 2, mid - x, width * 2)
    if tile == EDGE_STEEP_DOWN:
        return near(x - mid, (y - mid) * 2, width * 2)
    if tile == EDGE_STEEP_UP:
        return near(x - mid, (mid - y) * 2, width * 2)
    return False


def shade_on(tile: int, x: int, y: int) -> bool:
    if tile < SHADE_FIRST or tile > SHADE_LAST:
        return False
    if tile == SHADE_LAST:
        return True

    threshold = (tile * 64) // SHADE_LAST
    return BAYER_8X8[y & 7][x & 7] < threshold


def pixel_index(tile: int, x: int, y: int) -> int:
    atlas_tile = tile - START_TILE
    if atlas_tile == CLEAR:
        return 0
    if atlas_tile == PAPER:
        return 2
    if EDGE_H <= atlas_tile <= EDGE_STEEP_UP:
        return 1 if edge_on(atlas_tile, x, y) else 2
    return 1 if shade_on(atlas_tile, x, y) else 2


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} OUT.gif", file=sys.stderr)
        return 2

    out = Path(sys.argv[1])
    out.parent.mkdir(parents=True, exist_ok=True)

    image = Image.new("P", (TILE_COUNT * TILE_SIZE, TILE_SIZE), 0)
    image.putpalette([
        0, 0, 0,
        0, 0, 0,
        255, 255, 255,
    ] + [0, 0, 0] * 253)

    for tile in range(TILE_COUNT):
        ox = tile * TILE_SIZE
        for y in range(TILE_SIZE):
            for x in range(TILE_SIZE):
                image.putpixel((ox + x, y), pixel_index(tile, x, y))

    image.save(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
