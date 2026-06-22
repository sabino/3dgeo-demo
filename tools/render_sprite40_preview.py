#!/usr/bin/env python3
"""Render sprite40 preview captures without requiring GnGeo.

This mirrors the fixed-point cube math and cell-level sprite40 rasterizer in
src/main.c closely enough to review the generated masks, palette dithering, and
contact shadow on machines that do not have the Neo Geo toolchain installed.
It is not a hardware/emulator capture replacement; use make gngeo-sprite40 for
final verification on a configured ngdevkit/GnGeo system.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

from PIL import Image


FRAME_W = 40
FRAME_H = 28
PROJECT_SCALE_X = 46
PROJECT_SCALE_Y = 38
Q = 8
ONE = 1 << Q
CUBE_RADIUS = 120
CAMERA_DISTANCE_DEFAULT = 760
ANGLE_MASK = 63
FACE_LEVELS = 12
CELL_PIXELS = 24

TILE_CLEAR = 0
TILE_SHADE_1 = 1
TILE_SHADE_9 = 9
TILE_PAPER = 10
TILE_MASK_1 = 19

PAL_PAPER = 0
PAL_FACE_0 = 1
PAL_EDGE = 13
PAL_SHADOW_LIGHT = 14
PAL_SHADOW_DARK = 15

SIN_LUT = (
    0, 25, 50, 74, 98, 121, 142, 162,
    181, 198, 213, 226, 237, 245, 251, 255,
    256, 255, 251, 245, 237, 226, 213, 198,
    181, 162, 142, 121, 98, 74, 50, 25,
    0, -25, -50, -74, -98, -121, -142, -162,
    -181, -198, -213, -226, -237, -245, -251, -255,
    -256, -255, -251, -245, -237, -226, -213, -198,
    -181, -162, -142, -121, -98, -74, -50, -25,
)

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

PALETTE_GRAY = {
    PAL_PAPER: 255,
    PAL_FACE_0 + 0: 0x22,
    PAL_FACE_0 + 1: 0x33,
    PAL_FACE_0 + 2: 0x44,
    PAL_FACE_0 + 3: 0x55,
    PAL_FACE_0 + 4: 0x66,
    PAL_FACE_0 + 5: 0x77,
    PAL_FACE_0 + 6: 0x88,
    PAL_FACE_0 + 7: 0x99,
    PAL_FACE_0 + 8: 0xAA,
    PAL_FACE_0 + 9: 0xBB,
    PAL_FACE_0 + 10: 0xCC,
    PAL_FACE_0 + 11: 0xEE,
    PAL_EDGE: 0x11,
    PAL_SHADOW_LIGHT: 0xEE,
    PAL_SHADOW_DARK: 0xDD,
}

FACE_ADJACENCY = (
    (2, 5, 3, 4),
    (2, 5, 3, 4),
    (0, 5, 1, 4),
    (0, 5, 1, 4),
    (0, 3, 1, 2),
    (0, 3, 1, 2),
)

CUBE_VERTICES = (
    (-CUBE_RADIUS, -CUBE_RADIUS, -CUBE_RADIUS),
    ( CUBE_RADIUS, -CUBE_RADIUS, -CUBE_RADIUS),
    ( CUBE_RADIUS,  CUBE_RADIUS, -CUBE_RADIUS),
    (-CUBE_RADIUS,  CUBE_RADIUS, -CUBE_RADIUS),
    (-CUBE_RADIUS, -CUBE_RADIUS,  CUBE_RADIUS),
    ( CUBE_RADIUS, -CUBE_RADIUS,  CUBE_RADIUS),
    ( CUBE_RADIUS,  CUBE_RADIUS,  CUBE_RADIUS),
    (-CUBE_RADIUS,  CUBE_RADIUS,  CUBE_RADIUS),
)

CUBE_FACES = (
    ((0, 1, 2, 3), (0, 0, -ONE)),
    ((4, 5, 6, 7), (0, 0, ONE)),
    ((0, 1, 5, 4), (0, -ONE, 0)),
    ((3, 2, 6, 7), (0, ONE, 0)),
    ((0, 3, 7, 4), (-ONE, 0, 0)),
    ((1, 2, 6, 5), (ONE, 0, 0)),
)


@dataclass
class ProjectedVertex:
    x: int
    y: int
    z: int
    sx: int
    sy: int


@dataclass
class FaceShade:
    base: int
    origin_x: int
    origin_y: int
    ux: int
    uy: int
    vx: int
    vy: int
    inv_u_len: int
    inv_v_len: int
    g0: int
    g1: int
    g2: int
    g3: int
    g_center: int


def qmul(a: int, b: int) -> int:
    return (a * b) >> Q


def sine(angle: int) -> int:
    return SIN_LUT[angle & ANGLE_MASK]


def cosine(angle: int) -> int:
    return SIN_LUT[(angle + 16) & ANGLE_MASK]


def rotate_vec(p: tuple[int, int, int], pitch: int, yaw: int, roll: int) -> tuple[int, int, int]:
    sx = sine(pitch)
    cx = cosine(pitch)
    sy = sine(yaw)
    cy = cosine(yaw)
    sz = sine(roll)
    cz = cosine(roll)

    x, y, z = p
    x1 = x
    y1 = qmul(y, cx) - qmul(z, sx)
    z1 = qmul(y, sx) + qmul(z, cx)

    x2 = qmul(x1, cy) + qmul(z1, sy)
    y2 = y1
    z2 = -qmul(x1, sy) + qmul(z1, cy)

    return (qmul(x2, cz) - qmul(y2, sz), qmul(x2, sz) + qmul(y2, cz), z2)


def project_vertex(p: tuple[int, int, int], camera_distance: int) -> ProjectedVertex:
    x, y, z = p
    denom = z + camera_distance
    if denom < 160:
        denom = 160
    sx = FRAME_W // 2 + (x * PROJECT_SCALE_X) // denom
    sy = FRAME_H // 2 - (y * PROJECT_SCALE_Y) // denom
    return ProjectedVertex(x, y, z, sx, sy)


def fix_word(palette: int, tile: int) -> int:
    return (palette << 12) | tile


def word_palette(word: int) -> int:
    return word >> 12


def word_tile(word: int) -> int:
    return word & 0x0FFF


def light_for_normal(normal: tuple[int, int, int]) -> int:
    light = (-150, 70, -150)
    return sum(a * b for a, b in zip(normal, light)) >> Q


def face_depth(verts: list[ProjectedVertex], face: tuple[int, int, int, int]) -> int:
    return sum(verts[i].z for i in face) >> 2


def edge_cross_q4(a: ProjectedVertex, b: ProjectedVertex, px4: int, py4: int) -> int:
    ax4 = a.sx << 2
    ay4 = a.sy << 2
    bx4 = b.sx << 2
    by4 = b.sy << 2
    return (bx4 - ax4) * (py4 - ay4) - (by4 - ay4) * (px4 - ax4)


def point_in_quad_q4(verts: list[ProjectedVertex], face: tuple[int, int, int, int], px4: int, py4: int) -> bool:
    first = 0
    for i in range(4):
        a = verts[face[i]]
        b = verts[face[(i + 1) & 3]]
        c = edge_cross_q4(a, b, px4, py4)
        if c == 0:
            continue
        if first == 0:
            first = c
        elif (first < 0 < c) or (first > 0 > c):
            return False
    return True


def coverage_mask_in_quad(verts: list[ProjectedVertex], face: tuple[int, int, int, int], x: int, y: int) -> int:
    mask = 0
    for bit, (ox, oy) in enumerate(((1, 1), (3, 1), (1, 3), (3, 3))):
        if point_in_quad_q4(verts, face, (x << 2) + ox, (y << 2) + oy):
            mask |= 1 << bit
    return mask


def near_visible_neighbor_edge(
    verts: list[ProjectedVertex],
    face: tuple[int, int, int, int],
    face_id: int,
    normals: list[tuple[int, int, int]],
    x: int,
    y: int,
) -> bool:
    px4 = (x << 2) + 2
    py4 = (y << 2) + 2
    for i in range(4):
        neighbor = FACE_ADJACENCY[face_id][i]
        if normals[neighbor][2] >= 0:
            continue
        a = verts[face[i]]
        b = verts[face[(i + 1) & 3]]
        cross = abs(edge_cross_q4(a, b, px4, py4))
        dx = b.sx - a.sx
        dy = b.sy - a.sy
        length = abs(dx) + abs(dy)
        if cross <= (length + 1) * 7:
            return True
    return False


def screen_light_bias(x: int, y: int) -> int:
    return ((FRAME_H // 2 - y) * 5) - ((x - FRAME_W // 2) * 2)


def clamp_q8(value: int) -> int:
    return min(ONE, max(0, value))


def prepare_face_shade(verts: list[ProjectedVertex], face: tuple[int, int, int, int], normal: tuple[int, int, int]) -> FaceShade:
    v0, v1, v2, v3 = (verts[i] for i in face)
    ux = v1.sx - v0.sx
    uy = v1.sy - v0.sy
    vx = v3.sx - v0.sx
    vy = v3.sy - v0.sy
    u_len = ux * ux + uy * uy
    v_len = vx * vx + vy * vy
    g0 = screen_light_bias(v0.sx, v0.sy)
    g1 = screen_light_bias(v1.sx, v1.sy)
    g2 = screen_light_bias(v2.sx, v2.sy)
    g3 = screen_light_bias(v3.sx, v3.sy)
    return FaceShade(
        base=light_for_normal(normal),
        origin_x=v0.sx,
        origin_y=v0.sy,
        ux=ux,
        uy=uy,
        vx=vx,
        vy=vy,
        inv_u_len=4096 // u_len if u_len else 0,
        inv_v_len=4096 // v_len if v_len else 0,
        g0=g0,
        g1=g1,
        g2=g2,
        g3=g3,
        g_center=(g0 + g1 + g2 + g3) >> 2,
    )


def face_local_gradient(shade: FaceShade, x: int, y: int) -> int:
    px = x - shade.origin_x
    py = y - shade.origin_y
    u = 0
    v = 0
    if shade.inv_u_len:
        dot_u = px * shade.ux + py * shade.uy
        u = clamp_q8((dot_u * shade.inv_u_len) >> 4)
    if shade.inv_v_len:
        dot_v = px * shade.vx + py * shade.vy
        v = clamp_q8((dot_v * shade.inv_v_len) >> 4)
    gu0 = shade.g0 + (((shade.g1 - shade.g0) * u) >> Q)
    gu1 = shade.g3 + (((shade.g2 - shade.g3) * u) >> Q)
    g = gu0 + (((gu1 - gu0) * v) >> Q)
    return g - shade.g_center


def face_level_for_intensity(intensity: int) -> int:
    scaled = ((intensity + 130) * 16) // 36
    scaled = min((FACE_LEVELS - 1) * 16, max(0, scaled))
    scaled = min((FACE_LEVELS - 1) * 16, scaled + 8)
    return scaled >> 4


def face_word_for_intensity_mask(intensity: int, mask: int) -> int:
    level = face_level_for_intensity(intensity)
    tile = TILE_SHADE_9
    if mask != 0x0F:
        tile = TILE_MASK_1 + mask - 1
    return fix_word(PAL_FACE_0 + level, tile)


def draw_contact_shadow(frame: list[list[int]], verts: list[ProjectedVertex]) -> None:
    min_x = min(v.sx for v in verts)
    max_x = max(v.sx for v in verts)
    min_y = min(v.sy for v in verts)
    max_y = max(v.sy for v in verts)
    cx = (min_x + max_x) >> 1
    cy = max_y + 2
    rx = ((max_x - min_x) >> 1) + 2
    ry = ((max_y - min_y) // 9) + 1
    rx = min(14, max(5, rx))
    ry = min(4, max(2, ry))
    x0 = max(0, cx - rx)
    x1 = min(FRAME_W - 1, cx + rx)
    y0 = max(0, cy - ry)
    y1 = min(FRAME_H - 1, cy + ry)
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            dx = x - cx
            dy = y - cy
            d = (dx * dx * 16) // (rx * rx) + (dy * dy * 16) // (ry * ry)
            if d <= 16:
                if d < 7:
                    frame[y][x] = fix_word(PAL_SHADOW_DARK, TILE_SHADE_1 + 1)
                else:
                    frame[y][x] = fix_word(PAL_SHADOW_LIGHT, TILE_SHADE_1)


def render_cube(pitch: int, yaw: int, roll: int = 0, camera_distance: int = CAMERA_DISTANCE_DEFAULT) -> list[list[int]]:
    verts = [project_vertex(rotate_vec(v, pitch, yaw, roll), camera_distance) for v in CUBE_VERTICES]
    normals = [rotate_vec(normal, pitch, yaw, roll) for _face, normal in CUBE_FACES]
    order = sorted(range(6), key=lambda i: face_depth(verts, CUBE_FACES[i][0]), reverse=True)
    frame = [[fix_word(PAL_PAPER, TILE_PAPER) for _ in range(FRAME_W)] for _ in range(FRAME_H)]
    draw_contact_shadow(frame, verts)
    for face_id in order:
        face, _normal = CUBE_FACES[face_id]
        if normals[face_id][2] >= 0:
            continue
        shade = prepare_face_shade(verts, face, normals[face_id])
        min_x = max(0, min(verts[i].sx for i in face))
        max_x = min(FRAME_W - 1, max(verts[i].sx for i in face))
        min_y = max(0, min(verts[i].sy for i in face))
        max_y = min(FRAME_H - 1, max(verts[i].sy for i in face))
        for y in range(min_y, max_y + 1):
            for x in range(min_x, max_x + 1):
                mask = coverage_mask_in_quad(verts, face, x, y)
                if not mask:
                    continue
                gradient = face_local_gradient(shade, x, y)
                if mask != 0x0F and near_visible_neighbor_edge(verts, face, face_id, normals, x, y):
                    mask = 0x0F
                frame[y][x] = face_word_for_intensity_mask(shade.base + gradient, mask)
    return frame


def shade_on(tile: int, px: int, py: int) -> bool:
    if tile == TILE_SHADE_9:
        return True
    if TILE_SHADE_1 <= tile <= TILE_SHADE_9:
        threshold = (tile * 64) // TILE_SHADE_9
        return BAYER_8X8[py & 7][px & 7] < threshold
    return False


def mask_on(tile: int, px: int, py: int) -> bool:
    mask = tile - TILE_MASK_1 + 1
    bit = (1 if px >= 8 else 0) | (2 if py >= 8 else 0)
    return (mask & (1 << bit)) != 0


def tile_pixel_on(tile: int, px: int, py: int) -> bool:
    if tile == TILE_CLEAR:
        return False
    if tile == TILE_PAPER:
        return False
    if TILE_MASK_1 <= tile < TILE_MASK_1 + 15:
        return mask_on(tile, px, py)
    return shade_on(tile, px, py)


def frame_to_image(frame: list[list[int]]) -> Image.Image:
    image = Image.new("RGB", (FRAME_W * CELL_PIXELS, FRAME_H * CELL_PIXELS), (255, 255, 255))
    pixels = image.load()
    for cy, row in enumerate(frame):
        for cx, word in enumerate(row):
            pal = word_palette(word)
            tile = word_tile(word)
            gray = PALETTE_GRAY.get(pal, 255)
            for py in range(CELL_PIXELS):
                ty = (py * 16) // CELL_PIXELS
                for px in range(CELL_PIXELS):
                    tx = (px * 16) // CELL_PIXELS
                    if tile_pixel_on(tile, tx, ty):
                        pixels[cx * CELL_PIXELS + px, cy * CELL_PIXELS + py] = (gray, gray, gray)
    return image


def make_contact_sheet(images: Iterable[Image.Image]) -> Image.Image:
    thumbs = []
    for image in images:
        thumb = image.resize((image.width // 3, image.height // 3), Image.Resampling.NEAREST)
        thumbs.append(thumb)
    gap = 8
    sheet = Image.new("RGB", (sum(i.width for i in thumbs) + gap * (len(thumbs) - 1), thumbs[0].height), (0, 0, 0))
    x = 0
    for image in thumbs:
        sheet.paste(image, (x, 0))
        x += image.width + gap
    return sheet


def main() -> int:
    out_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("build/captures")
    out_dir.mkdir(parents=True, exist_ok=True)
    captures = [
        ("01-initial.png", 54, 56, 0, 760),
        ("02-yaw-right.png", 54, 4, 0, 760),
        ("03-yaw-pitch.png", 50, 4, 0, 760),
        ("04-zoom-in.png", 54, 56, 0, 440),
        ("05-zoom-out.png", 54, 56, 0, 1100),
    ]
    images = []
    for filename, pitch, yaw, roll, camera in captures:
        image = frame_to_image(render_cube(pitch, yaw, roll, camera))
        image.save(out_dir / filename)
        images.append(image)
    make_contact_sheet(images).save(out_dir / "sprite40-rotation-contact.png")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
