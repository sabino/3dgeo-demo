#!/usr/bin/env python3
"""Copy reviewed native captures, unchanged, into the documentation gallery."""
# SPDX-License-Identifier: Apache-2.0
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from PIL import Image

NAMES = ("cube", "pyramid", "tetrahedron", "ball", "donut", "cone", "cylinder", "light")
ROOT = Path(__file__).resolve().parents[1]


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build/viewer"))
    args = parser.parse_args()
    build = ROOT / args.build_dir
    log = (build / "gallery.log").read_text()
    if "GALLERY PASS: eight native model/lighting screenshots" not in log:
        raise SystemExit("Capture replay did not pass; refusing to publish")
    captures = {}
    for name in NAMES:
        path = build / "gallery" / f"{name}.png"
        with Image.open(path) as capture:
            if capture.format != "PNG" or capture.size != (320, 224):
                raise SystemExit(f"Not a native 320x224 PNG: {path}")
            capture.verify()
        captures[path.name] = path.read_bytes()
    inputs = [ROOT / "src/main.c", ROOT / "src/viewer_core.h",
              ROOT / "tools/make_sprite_tiles.py", ROOT / "tools/make_fix_tiles.py",
              ROOT / "tools/obj_to_model.py", ROOT / "tools/gallery_probe.lua",
              *sorted((ROOT / "assets/models").glob("*.obj"))]
    manifest = {
        "description": "Unmodified MAME native screen snapshots; controller replay, no game RAM writes.",
        "dimensions": [320, 224],
        "checkout_commit_at_capture": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "emulator": subprocess.check_output(["mame", "-version"], text=True).strip(),
        "compiler": subprocess.check_output(
            ["m68k-neogeo-elf-gcc", "--version"], text=True).splitlines()[0],
        "rom_sha256": sha256(build / "rom/3dgeo_demo.zip"),
        "elf_sha256": sha256(build / "3dgeo.elf"),
        "source_sha256": {str(path.relative_to(ROOT)): sha256(path) for path in inputs},
        "captures": [line for line in log.splitlines() if line.startswith("CAPTURE ")],
        "image_sha256": {name: hashlib.sha256(data).hexdigest() for name, data in captures.items()},
    }
    out = ROOT / "docs/images"
    out.mkdir(parents=True, exist_ok=True)
    for name, data in captures.items():
        (out / name).write_bytes(data)
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print("Published eight unchanged PNGs and capture provenance to docs/images/")


if __name__ == "__main__":
    main()
