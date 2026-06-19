#!/usr/bin/env python3
"""Generate precomputed FIX cell transition diffs for a controllable cube."""

from __future__ import annotations

import math
import sys
from pathlib import Path


FIX_COLS = 40
FIX_VISIBLE_ROWS = 28
FIX_VISIBLE_ROW_OFFSET = 2
CELL_COUNT = FIX_COLS * FIX_VISIBLE_ROWS
YAW_STEPS = 16
PITCH_STEPS = 16
FRAME_COUNT = YAW_STEPS * PITCH_STEPS
START_YAW = 0
START_PITCH = PITCH_STEPS // 2
START_FRAME = START_PITCH * YAW_STEPS + START_YAW

DIR_LEFT = 0
DIR_RIGHT = 1
DIR_UP = 2
DIR_DOWN = 3
DIRECTIONS = (DIR_LEFT, DIR_RIGHT, DIR_UP, DIR_DOWN)

CUBE_RADIUS = 120.0
CAMERA_DISTANCE = 700.0
PROJECT_SCALE_X = 46.0
PROJECT_SCALE_Y = 38.0

TILE_BLANK = 0
TILE_LIGHT = 1
TILE_MEDIUM = 2

PAL_PAPER = 0
PAL_FACE_LIGHT = 1
PAL_FACE_MID = 2
PAL_FACE_DARK = 3
PAL_EDGE = 4


def fix_word(palette: int, tile: int) -> int:
    return (palette << 12) | tile


BLANK_WORD = fix_word(PAL_PAPER, TILE_BLANK)

VERTICES = [
    (-CUBE_RADIUS, -CUBE_RADIUS, -CUBE_RADIUS),
    (CUBE_RADIUS, -CUBE_RADIUS, -CUBE_RADIUS),
    (CUBE_RADIUS, CUBE_RADIUS, -CUBE_RADIUS),
    (-CUBE_RADIUS, CUBE_RADIUS, -CUBE_RADIUS),
    (-CUBE_RADIUS, -CUBE_RADIUS, CUBE_RADIUS),
    (CUBE_RADIUS, -CUBE_RADIUS, CUBE_RADIUS),
    (CUBE_RADIUS, CUBE_RADIUS, CUBE_RADIUS),
    (-CUBE_RADIUS, CUBE_RADIUS, CUBE_RADIUS),
]

FACES = [
    ((0, 1, 2, 3), (0.0, 0.0, -1.0)),
    ((4, 5, 6, 7), (0.0, 0.0, 1.0)),
    ((0, 1, 5, 4), (0.0, -1.0, 0.0)),
    ((3, 2, 6, 7), (0.0, 1.0, 0.0)),
    ((0, 3, 7, 4), (-1.0, 0.0, 0.0)),
    ((1, 2, 6, 5), (1.0, 0.0, 0.0)),
]

LIGHT = (-0.40625, -0.5859375, -0.7265625)


def rotate_vec(point: tuple[float, float, float], angle_x: float, angle_y: float) -> tuple[float, float, float]:
    x, y, z = point
    sx = math.sin(angle_x)
    cx = math.cos(angle_x)
    sy = math.sin(angle_y)
    cy = math.cos(angle_y)

    x1 = x * cy + z * sy
    z1 = -x * sy + z * cy
    y1 = y

    return (
        x1,
        y1 * cx - z1 * sx,
        y1 * sx + z1 * cx,
    )


def project(point: tuple[float, float, float]) -> tuple[float, float, float, int, int]:
    x, y, z = point
    denom = max(z + CAMERA_DISTANCE, 160.0)
    sx = round(FIX_COLS / 2 + (x * PROJECT_SCALE_X) / denom)
    sy = round(FIX_VISIBLE_ROWS / 2 - (y * PROJECT_SCALE_Y) / denom)
    return x, y, z, sx, sy


def palette_for_normal(normal: tuple[float, float, float]) -> int:
    intensity = normal[0] * LIGHT[0] + normal[1] * LIGHT[1] + normal[2] * LIGHT[2]
    if intensity > 105.0 / 256.0:
        return PAL_FACE_LIGHT
    if intensity > 25.0 / 256.0:
        return PAL_FACE_MID
    return PAL_FACE_DARK


def edge_cross(ax: int, ay: int, bx: int, by: int, px: int, py: int) -> int:
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax)


def point_in_quad(verts: list[tuple[float, float, float, int, int]], face: tuple[int, ...], x: int, y: int) -> bool:
    first = 0
    for i, vi in enumerate(face):
        a = verts[vi]
        b = verts[face[(i + 1) & 3]]
        c = edge_cross(a[3], a[4], b[3], b[4], x, y)
        if c == 0:
            continue
        if first == 0:
            first = c
        elif (first < 0 < c) or (first > 0 > c):
            return False
    return True


def fill_face(frame: list[list[int]], verts: list[tuple[float, float, float, int, int]], face: tuple[int, ...], word: int) -> None:
    xs = [verts[vi][3] for vi in face]
    ys = [verts[vi][4] for vi in face]
    min_x = max(min(xs), 0)
    max_x = min(max(xs), FIX_COLS - 1)
    min_y = max(min(ys), 0)
    max_y = min(max(ys), FIX_VISIBLE_ROWS - 1)

    for y in range(min_y, max_y + 1):
        row = frame[y]
        for x in range(min_x, max_x + 1):
            if point_in_quad(verts, face, x, y):
                row[x] = word


def draw_line(frame: list[list[int]], x0: int, y0: int, x1: int, y1: int, word: int) -> None:
    dx = abs(x1 - x0)
    sx = 1 if x0 < x1 else -1
    dy = -abs(y1 - y0)
    sy = 1 if y0 < y1 else -1
    err = dx + dy

    while True:
        if 0 <= x0 < FIX_COLS and 0 <= y0 < FIX_VISIBLE_ROWS:
            frame[y0][x0] = word
        if x0 == x1 and y0 == y1:
            break
        e2 = err * 2
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


def render_frame(yaw_index: int, pitch_index: int) -> list[int]:
    frame = [[BLANK_WORD for _ in range(FIX_COLS)] for _ in range(FIX_VISIBLE_ROWS)]
    yaw = (math.tau * yaw_index) / YAW_STEPS
    pitch = (math.tau * pitch_index) / PITCH_STEPS

    verts = [project(rotate_vec(v, pitch, yaw)) for v in VERTICES]
    normals = [rotate_vec(normal, pitch, yaw) for _, normal in FACES]
    order = []
    for face_index, (face, _) in enumerate(FACES):
        depth = sum(verts[vi][2] for vi in face) / 4.0
        order.append((depth, face_index))
    order.sort(reverse=True)

    for _, face_index in order:
        face, _ = FACES[face_index]
        fill_face(frame, verts, face, fix_word(palette_for_normal(normals[face_index]), TILE_MEDIUM))

    edge_word = fix_word(PAL_EDGE, TILE_LIGHT)
    for face_index, (face, _) in enumerate(FACES):
        if normals[face_index][2] < 0.25:
            for i, vi in enumerate(face):
                a = verts[vi]
                b = verts[face[(i + 1) & 3]]
                draw_line(frame, a[3], a[4], b[3], b[4], edge_word)

    return [frame[y][x] for y in range(FIX_VISIBLE_ROWS) for x in range(FIX_COLS)]


def changed_cells(a: list[int], b: list[int]) -> int:
    return sum(1 for av, bv in zip(a, b) if av != bv)


def metric(frames: list[list[int]], pitch: int, yaw: int) -> list[int]:
    return frames[pitch * YAW_STEPS + yaw]


def max_adjacent_changes(frames: list[list[int]]) -> tuple[int, int, int, int]:
    initial = changed_cells([BLANK_WORD] * CELL_COUNT, frames[START_FRAME])
    yaw = 0
    pitch = 0
    for pitch_index in range(PITCH_STEPS):
        for yaw_index in range(YAW_STEPS):
            yaw = max(yaw, changed_cells(
                metric(frames, pitch_index, yaw_index),
                metric(frames, pitch_index, (yaw_index + 1) % YAW_STEPS),
            ))
    for pitch_index in range(PITCH_STEPS):
        for yaw_index in range(YAW_STEPS):
            pitch = max(pitch, changed_cells(
                metric(frames, pitch_index, yaw_index),
                metric(frames, (pitch_index + 1) % PITCH_STEPS, yaw_index),
            ))
    return initial, yaw, pitch, max(yaw, pitch)


def updates_between(a: list[int], b: list[int]) -> list[int]:
    updates: list[int] = []
    for cell, (av, bv) in enumerate(zip(a, b)):
        if av != bv:
            x = cell % FIX_COLS
            y = cell // FIX_COLS
            updates.append((x << 5) + y + FIX_VISIBLE_ROW_OFFSET)
            updates.append(bv)
    return updates


def target_for_direction(yaw: int, pitch: int, direction: int) -> tuple[int, int]:
    if direction == DIR_LEFT:
        return (yaw - 1) % YAW_STEPS, pitch
    if direction == DIR_RIGHT:
        return (yaw + 1) % YAW_STEPS, pitch
    if direction == DIR_UP:
        return yaw, (pitch - 1) % PITCH_STEPS
    if direction == DIR_DOWN:
        return yaw, (pitch + 1) % PITCH_STEPS
    return yaw, pitch


def build_transitions(frames: list[list[int]]) -> tuple[list[list[int]], list[list[int]], list[int]]:
    offsets = [[0 for _ in DIRECTIONS] for _ in range(FRAME_COUNT)]
    counts = [[0 for _ in DIRECTIONS] for _ in range(FRAME_COUNT)]
    updates: list[int] = []

    for pitch in range(PITCH_STEPS):
        for yaw in range(YAW_STEPS):
            frame_index = pitch * YAW_STEPS + yaw
            source = frames[frame_index]
            for direction in DIRECTIONS:
                target_yaw, target_pitch = target_for_direction(yaw, pitch, direction)
                target_index = target_pitch * YAW_STEPS + target_yaw
                transition_updates = updates_between(source, frames[target_index])
                offsets[frame_index][direction] = len(updates) // 2
                counts[frame_index][direction] = len(transition_updates) // 2
                updates.extend(transition_updates)

    return offsets, counts, updates


def c_u16_array(name: str, values: list[int], per_line: int = 8) -> list[str]:
    lines = [f"static const u16 {name}[{len(values)}] = {{"]
    for i in range(0, len(values), per_line):
        chunk = values[i : i + per_line]
        lines.append("    " + ", ".join(f"0x{value:04x}" for value in chunk) + ",")
    lines.append("};")
    return lines


def write_u32_table(lines: list[str], name: str, values: list[list[int]]) -> None:
    lines.append(f"static const u32 {name}[CUBE_FRAME_COUNT][CUBE_DIRECTION_COUNT] = {{")
    for row in values:
        lines.append("    {" + ", ".join(f"{value}UL" for value in row) + "},")
    lines.append("};")


def write_u16_table(lines: list[str], name: str, values: list[list[int]]) -> None:
    lines.append(f"static const u16 {name}[CUBE_FRAME_COUNT][CUBE_DIRECTION_COUNT] = {{")
    for row in values:
        lines.append("    {" + ", ".join(f"0x{value:04x}" for value in row) + "},")
    lines.append("};")


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} OUT.h", file=sys.stderr)
        return 2

    out = Path(sys.argv[1])
    out.parent.mkdir(parents=True, exist_ok=True)

    frames = [
        render_frame(yaw_index, pitch_index)
        for pitch_index in range(PITCH_STEPS)
        for yaw_index in range(YAW_STEPS)
    ]
    initial, yaw_changes, pitch_changes, step_changes = max_adjacent_changes(frames)
    start_updates = updates_between([BLANK_WORD] * CELL_COUNT, frames[START_FRAME])
    offsets, counts, updates = build_transitions(frames)

    lines = [
        "/* Generated by tools/make_cube_frames.py. */",
        "#ifndef CUBE_FRAMES_H",
        "#define CUBE_FRAMES_H",
        "",
        f"#define CUBE_YAW_STEPS {YAW_STEPS}",
        f"#define CUBE_PITCH_STEPS {PITCH_STEPS}",
        f"#define CUBE_FRAME_COUNT {FRAME_COUNT}",
        "#define CUBE_DIRECTION_COUNT 4",
        f"#define CUBE_START_YAW {START_YAW}",
        f"#define CUBE_START_PITCH {START_PITCH}",
        f"#define CUBE_START_FRAME {START_FRAME}",
        f"#define CUBE_START_UPDATE_COUNT {len(start_updates) // 2}",
        f"#define CUBE_STEP_UPDATE_PAIRS {len(updates) // 2}",
        f"#define CUBE_MAX_INITIAL_UPDATES {initial}",
        f"#define CUBE_MAX_YAW_STEP_UPDATES {yaw_changes}",
        f"#define CUBE_MAX_PITCH_STEP_UPDATES {pitch_changes}",
        f"#define CUBE_MAX_STEP_UPDATES {step_changes}",
        "#define CUBE_DIR_LEFT 0",
        "#define CUBE_DIR_RIGHT 1",
        "#define CUBE_DIR_UP 2",
        "#define CUBE_DIR_DOWN 3",
        "",
    ]

    lines.extend(c_u16_array("cube_start_updates", start_updates))
    lines.append("")
    write_u32_table(lines, "cube_step_offsets", offsets)
    lines.append("")
    write_u16_table(lines, "cube_step_counts", counts)
    lines.append("")
    lines.extend(c_u16_array("cube_step_updates", updates))

    lines.extend(["", "#endif"])
    out.write_text("\n".join(lines) + "\n", encoding="ascii")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
