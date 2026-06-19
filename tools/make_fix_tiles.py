#!/usr/bin/env python3
"""Generate blank plus light/medium/dark/full 8x8 FIX tiles."""

from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image


TILE_SIZE = 8
TILE_COUNT = 5


def pixel_on(tile: int, x: int, y: int) -> bool:
    if tile == 0:
        return False
    if tile == 1:
        return (x + y) % 4 == 0
    if tile == 2:
        return (x + y) % 2 == 0
    if tile == 3:
        return (x + y) % 4 != 0
    return True


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} OUT.gif", file=sys.stderr)
        return 2

    out = Path(sys.argv[1])
    out.parent.mkdir(parents=True, exist_ok=True)

    image = Image.new("P", (TILE_COUNT * TILE_SIZE, TILE_SIZE), 1)
    image.putpalette([
        0, 0, 0,
        255, 255, 255,
    ] + [255, 255, 255] * 254)

    for tile in range(TILE_COUNT):
        ox = tile * TILE_SIZE
        for y in range(TILE_SIZE):
            for x in range(TILE_SIZE):
                image.putpixel((ox + x, y), 0 if pixel_on(tile, x, y) else 1)

    image.save(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
