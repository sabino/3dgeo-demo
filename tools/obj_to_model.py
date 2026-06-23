#!/usr/bin/env python3
"""Convert a small Wavefront OBJ mesh into fixed Neo Geo renderer data."""

from __future__ import annotations

import argparse
import math
from pathlib import Path


MODEL_RADIUS = 120
NORMAL_ONE = 256
MAX_VERTICES = 255
MAX_TRIS = 255
MAX_EDGES = 255


class ConvertedModel:
    def __init__(
        self,
        source: Path,
        vertices: list[tuple[int, int, int]],
        tris: list[tuple[int, int, int, tuple[int, int, int]]],
        edges: list[tuple[int, int, tuple[int, int, int]]],
    ) -> None:
        self.source = source
        self.name = source.stem
        self.symbol = make_symbol(source.stem)
        self.vertices = vertices
        self.tris = tris
        self.edges = edges


def make_symbol(name: str) -> str:
    out = []
    for ch in name.lower():
        if ch.isalnum():
            out.append(ch)
        else:
            out.append("_")
    symbol = "".join(out).strip("_")
    return symbol or "model"


def parse_index(token: str, vertex_count: int) -> int:
    head = token.split("/", 1)[0]
    if not head:
        raise ValueError(f"invalid empty OBJ face index in {token!r}")

    value = int(head)
    if value > 0:
        index = value - 1
    else:
        index = vertex_count + value

    if index < 0 or index >= vertex_count:
        raise ValueError(f"OBJ face index {value} is out of range")
    return index


def load_obj(path: Path) -> tuple[list[tuple[float, float, float]], list[list[int]]]:
    vertices: list[tuple[float, float, float]] = []
    faces: list[list[int]] = []

    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue

        parts = line.split()
        command = parts[0]

        if command == "v":
            if len(parts) < 4:
                raise ValueError(f"{path}:{lineno}: vertex requires x y z")
            vertices.append((float(parts[1]), float(parts[2]), float(parts[3])))
        elif command == "f":
            if len(parts) < 4:
                raise ValueError(f"{path}:{lineno}: face requires at least 3 vertices")
            faces.append([parse_index(token, len(vertices)) for token in parts[1:]])

    if not vertices:
        raise ValueError(f"{path}: no vertices found")
    if not faces:
        raise ValueError(f"{path}: no faces found")
    if len(vertices) > MAX_VERTICES:
        raise ValueError(f"{path}: {len(vertices)} vertices exceeds limit {MAX_VERTICES}")

    return vertices, faces


def normalize_vertices(vertices: list[tuple[float, float, float]]) -> list[tuple[int, int, int]]:
    xs = [v[0] for v in vertices]
    ys = [v[1] for v in vertices]
    zs = [v[2] for v in vertices]
    center = (
        (min(xs) + max(xs)) * 0.5,
        (min(ys) + max(ys)) * 0.5,
        (min(zs) + max(zs)) * 0.5,
    )
    max_dim = max(max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs))
    if max_dim <= 0.0:
        raise ValueError("model has no measurable size")

    scale = (MODEL_RADIUS * 2.0) / max_dim
    out = []
    for x, y, z in vertices:
        out.append((
            int(round((x - center[0]) * scale)),
            int(round((y - center[1]) * scale)),
            int(round((z - center[2]) * scale)),
        ))
    return out


def vector_sub(a: tuple[int, int, int], b: tuple[int, int, int]) -> tuple[float, float, float]:
    return (float(a[0] - b[0]), float(a[1] - b[1]), float(a[2] - b[2]))


def cross(a: tuple[float, float, float], b: tuple[float, float, float]) -> tuple[float, float, float]:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def make_normal(
    vertices: list[tuple[int, int, int]], a: int, b: int, c: int, convex_normal_fix: bool
) -> tuple[int, int, int]:
    va = vertices[a]
    vb = vertices[b]
    vc = vertices[c]
    ab = vector_sub(vb, va)
    ac = vector_sub(vc, va)
    normal = cross(ab, ac)
    center = (
        (va[0] + vb[0] + vc[0]) / 3.0,
        (va[1] + vb[1] + vc[1]) / 3.0,
        (va[2] + vb[2] + vc[2]) / 3.0,
    )

    if convex_normal_fix and normal[0] * center[0] + normal[1] * center[1] + normal[2] * center[2] < 0.0:
        normal = (-normal[0], -normal[1], -normal[2])

    length = math.sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2])
    if length <= 0.0:
        raise ValueError("degenerate triangle has no normal")

    return (
        int(round(normal[0] * NORMAL_ONE / length)),
        int(round(normal[1] * NORMAL_ONE / length)),
        int(round(normal[2] * NORMAL_ONE / length)),
    )


def triangulate(
    vertices: list[tuple[int, int, int]], faces: list[list[int]], convex_normal_fix: bool
) -> tuple[
    list[tuple[int, int, int, tuple[int, int, int]]],
    list[tuple[int, int, tuple[int, int, int]]],
]:
    tris = []
    edges = []
    for face in faces:
        face_normal = make_normal(vertices, face[0], face[1], face[2], convex_normal_fix)
        for i in range(1, len(face) - 1):
            a = face[0]
            b = face[i]
            c = face[i + 1]
            tris.append((a, b, c, make_normal(vertices, a, b, c, convex_normal_fix)))
        for i, a in enumerate(face):
            edges.append((a, face[(i + 1) % len(face)], face_normal))

    if len(tris) > MAX_TRIS:
        raise ValueError(f"{len(tris)} triangles exceeds limit {MAX_TRIS}")
    if len(edges) > MAX_EDGES:
        raise ValueError(f"{len(edges)} edges exceeds limit {MAX_EDGES}")
    return tris, edges


def convert_model(source: Path) -> ConvertedModel:
    raw_vertices, faces = load_obj(source)
    vertices = normalize_vertices(raw_vertices)
    convex_normal_fix = source.stem != "donut"
    tris, edges = triangulate(vertices, faces, convex_normal_fix)
    return ConvertedModel(source, vertices, tris, edges)


def append_model_arrays(lines: list[str], model: ConvertedModel) -> None:
    prefix = model.symbol

    lines.extend([
        f"static const ModelVertex {prefix}_vertices[] = {{",
    ])

    for x, y, z in model.vertices:
        lines.append(f"    {{ {x:4d}, {y:4d}, {z:4d} }},")

    lines.extend([
        "};",
        "",
        f"static const ModelTri {prefix}_tris[] = {{",
    ])

    for a, b, c, normal in model.tris:
        nx, ny, nz = normal
        lines.append(f"    {{ {a:3d}, {b:3d}, {c:3d}, {{ {nx:4d}, {ny:4d}, {nz:4d} }} }},")

    lines.extend([
        "};",
        "",
        f"static const ModelEdge {prefix}_edges[] = {{",
    ])

    for a, b, normal in model.edges:
        nx, ny, nz = normal
        lines.append(f"    {{ {a:3d}, {b:3d}, {{ {nx:4d}, {ny:4d}, {nz:4d} }} }},")

    lines.extend([
        "};",
        "",
    ])


def emit_header(output: Path, models: list[ConvertedModel]) -> None:
    if not models:
        raise ValueError("at least one OBJ model is required")

    max_vertices = max(len(model.vertices) for model in models)
    max_tris = max(len(model.tris) for model in models)
    max_edges = max(len(model.edges) for model in models)

    lines = [
        "/* Generated by tools/obj_to_model.py. Do not edit by hand. */",
        "#ifndef MODEL_DATA_H",
        "#define MODEL_DATA_H",
        "",
        f"#define MODEL_COUNT {len(models)}",
        f"#define MODEL_MAX_VERTEX_COUNT {max_vertices}",
        f"#define MODEL_MAX_TRI_COUNT {max_tris}",
        f"#define MODEL_MAX_EDGE_COUNT {max_edges}",
        "",
    ]

    for model in models:
        append_model_arrays(lines, model)

    lines.extend([
        "static const ModelData models[MODEL_COUNT] = {",
    ])

    for model in models:
        prefix = model.symbol
        lines.append(
            f"    {{ \"{model.name}\", {prefix}_vertices, {len(model.vertices)}, "
            f"{prefix}_tris, {len(model.tris)}, {prefix}_edges, {len(model.edges)} }},"
        )

    lines.extend([
        "};",
        "",
        "#endif",
        "",
    ])

    content = "\n".join(lines)
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists() and output.read_text(encoding="utf-8") == content:
        return
    output.write_text(content, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", nargs="+", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    emit_header(args.output, [convert_model(path) for path in args.input])


if __name__ == "__main__":
    main()
