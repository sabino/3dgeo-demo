#!/usr/bin/env python3
"""Generate reserved C-ROM tiles plus clear/light/medium/dark/full/paper sprites."""

from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image


TILE_SIZE = 16
START_TILE = 256
TILE_COUNT = START_TILE + 6


def shade_on(tile: int, x: int, y: int) -> bool:
    shade = tile - START_TILE
    if shade <= 0:
        return False
    if shade == 1:
        return (x + y) % 4 == 0
    if shade == 2:
        return (x + y) % 2 == 0
    if shade == 3:
        return (x + y) % 4 != 0
    return True


def pixel_index(tile: int, x: int, y: int) -> int:
    shade = tile - START_TILE
    if shade <= 0:
        return 0
    if shade == 5:
        return 2
    return 1 if shade_on(tile, x, y) else 2


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
