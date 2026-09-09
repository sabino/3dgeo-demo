#!/usr/bin/env python3
"""Check local Markdown links and the committed native screenshot provenance."""
# SPDX-License-Identifier: Apache-2.0
import hashlib
import json
from pathlib import Path
import re
from urllib.parse import unquote, urlsplit

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
NAMES = {f"{name}.png" for name in
         ("cube", "pyramid", "tetrahedron", "ball", "donut", "cone", "cylinder", "light")}


def check(root=ROOT):
    errors = []
    documents = [root / "README.md", root / "TECHNICAL_EXPLANATION.md",
                 *sorted((root / "docs").glob("*.md"))]
    for document in documents:
        body = re.sub(r"```.*?```", "", document.read_text(), flags=re.S)
        for target in re.findall(r"!?\[[^\]]*\]\(([^)]+)\)", body):
            url = urlsplit(target.strip("<>"))
            if url.scheme or url.netloc or not url.path:
                continue
            if not (document.parent / unquote(url.path)).is_file():
                errors.append(f"{document.relative_to(root)}: missing link target {target}")
    manifest = json.loads((root / "docs/images/manifest.json").read_text())
    if set(manifest["image_sha256"]) != NAMES:
        errors.append("Gallery must contain all seven models and the light scene")
    for name, expected in manifest["image_sha256"].items():
        path = root / "docs/images" / name
        with Image.open(path) as capture:
            if capture.format != "PNG" or capture.size != (320, 224):
                errors.append(f"Not a native 320x224 PNG: {name}")
            capture.verify()
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            errors.append(f"Screenshot checksum mismatch: {name}")
    for name, expected in manifest["source_sha256"].items():
        path = root / name
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            errors.append(f"Gallery source changed: {name}; recapture, review, and publish screenshots")
    if errors:
        raise SystemExit("\n".join(errors))
    print(f"DOCS PASS: {len(documents)} documents, local links, eight native PNGs, source/image checksums")


if __name__ == "__main__":
    check()
