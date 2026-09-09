#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0

"""Generate clear plus FIX framebuffer shade/line tiles."""

from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image


TILE_SIZE = 8
TILE_COUNT = 128

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

FONT_5X7 = {
    " ": ("00000", "00000", "00000", "00000", "00000", "00000", "00000"),
    "/": ("00001", "00010", "00010", "00100", "01000", "01000", "10000"),
    "0": ("01110", "10001", "10011", "10101", "11001", "10001", "01110"),
    "1": ("00100", "01100", "00100", "00100", "00100", "00100", "01110"),
    "2": ("01110", "10001", "00001", "00010", "00100", "01000", "11111"),
    "3": ("11110", "00001", "00001", "01110", "00001", "00001", "11110"),
    "4": ("00010", "00110", "01010", "10010", "11111", "00010", "00010"),
    "5": ("11111", "10000", "10000", "11110", "00001", "00001", "11110"),
    "6": ("01110", "10000", "10000", "11110", "10001", "10001", "01110"),
    "7": ("11111", "00001", "00010", "00100", "01000", "01000", "01000"),
    "8": ("01110", "10001", "10001", "01110", "10001", "10001", "01110"),
    "9": ("01110", "10001", "10001", "01111", "00001", "00001", "01110"),
    ":": ("00000", "00100", "00100", "00000", "00100", "00100", "00000"),
    "A": ("01110", "10001", "10001", "11111", "10001", "10001", "10001"),
    "B": ("11110", "10001", "10001", "11110", "10001", "10001", "11110"),
    "C": ("01110", "10001", "10000", "10000", "10000", "10001", "01110"),
    "D": ("11110", "10001", "10001", "10001", "10001", "10001", "11110"),
    "E": ("11111", "10000", "10000", "11110", "10000", "10000", "11111"),
    "F": ("11111", "10000", "10000", "11110", "10000", "10000", "10000"),
    "G": ("01110", "10001", "10000", "10111", "10001", "10001", "01110"),
    "H": ("10001", "10001", "10001", "11111", "10001", "10001", "10001"),
    "I": ("01110", "00100", "00100", "00100", "00100", "00100", "01110"),
    "J": ("00111", "00010", "00010", "00010", "00010", "10010", "01100"),
    "K": ("10001", "10010", "10100", "11000", "10100", "10010", "10001"),
    "L": ("10000", "10000", "10000", "10000", "10000", "10000", "11111"),
    "M": ("10001", "11011", "10101", "10101", "10001", "10001", "10001"),
    "N": ("10001", "11001", "10101", "10011", "10001", "10001", "10001"),
    "O": ("01110", "10001", "10001", "10001", "10001", "10001", "01110"),
    "P": ("11110", "10001", "10001", "11110", "10000", "10000", "10000"),
    "Q": ("01110", "10001", "10001", "10001", "10101", "10010", "01101"),
    "R": ("11110", "10001", "10001", "11110", "10100", "10010", "10001"),
    "S": ("01111", "10000", "10000", "01110", "00001", "00001", "11110"),
    "T": ("11111", "00100", "00100", "00100", "00100", "00100", "00100"),
    "U": ("10001", "10001", "10001", "10001", "10001", "10001", "01110"),
    "V": ("10001", "10001", "10001", "10001", "10001", "01010", "00100"),
    "W": ("10001", "10001", "10001", "10101", "10101", "10101", "01010"),
    "X": ("10001", "10001", "01010", "00100", "01010", "10001", "10001"),
    "Y": ("10001", "10001", "01010", "00100", "00100", "00100", "00100"),
    "Z": ("11111", "00001", "00010", "00100", "01000", "10000", "11111"),
}

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
    width = 0
    if tile == EDGE_H:
        return near(y, mid, width)
    if tile == EDGE_V:
        return near(x, mid, width)
    if tile == EDGE_DIAG_DOWN:
        return near(y, x, width)
    if tile == EDGE_DIAG_UP:
        return near(y, TILE_SIZE - 1 - x, width)
    if tile == EDGE_SHALLOW_DOWN:
        return near((y - mid) * 2, x - mid, 1)
    if tile == EDGE_SHALLOW_UP:
        return near((y - mid) * 2, mid - x, 1)
    if tile == EDGE_STEEP_DOWN:
        return near(x - mid, (y - mid) * 2, 1)
    if tile == EDGE_STEEP_UP:
        return near(x - mid, (mid - y) * 2, 1)
    return False


def pixel_on(tile: int, x: int, y: int) -> bool:
    if tile < SHADE_FIRST or tile > SHADE_LAST:
        return False
    if tile == SHADE_LAST:
        return True

    threshold = (tile * 64) // SHADE_LAST
    return BAYER_8X8[y][x] < threshold


def font_on(tile: int, x: int, y: int) -> bool:
    if tile < 32 or tile >= TILE_COUNT:
        return False

    glyph = FONT_5X7.get(chr(tile))
    if glyph is None or x < 1 or x > 5 or y == 0:
        return False

    return glyph[y - 1][x - 1] == "1"


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} OUT.gif", file=sys.stderr)
        return 2

    out = Path(sys.argv[1])
    out.parent.mkdir(parents=True, exist_ok=True)

    image = Image.new("P", (TILE_COUNT * TILE_SIZE, TILE_SIZE), 0)
    image.putpalette([
        0, 0, 0,
        255, 255, 255,
    ] + [255, 255, 255] * 254)

    for tile in range(TILE_COUNT):
        ox = tile * TILE_SIZE
        for y in range(TILE_SIZE):
            for x in range(TILE_SIZE):
                if tile == CLEAR:
                    pixel = 0
                elif tile == PAPER:
                    pixel = 1
                elif EDGE_H <= tile <= EDGE_STEEP_UP:
                    pixel = 0 if edge_on(tile, x, y) else 1
                elif tile >= 32:
                    pixel = 1 if font_on(tile, x, y) else 0
                else:
                    pixel = 0 if pixel_on(tile, x, y) else 1
                image.putpixel((ox + x, y), pixel)

    image.save(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
