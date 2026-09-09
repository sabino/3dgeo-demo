#!/usr/bin/env python3
"""Package an explicit allowlist, never the ROM directory (which has test BIOSes)."""
# SPDX-License-Identifier: Apache-2.0
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile
from PIL import Image


def main():
    root = Path(__file__).resolve().parents[1]
    build = root / "build/ci"
    out = build / "dist"
    out.mkdir(exist_ok=False)
    revision = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=root, text=True
    ).strip()
    files = {
        "rom/3dgeo_demo.zip": build / "rom/3dgeo_demo.zip",
        "rom/gngeo_data.zip": build / "rom/gngeo_data.zip",
        "rom/neogeo.xml": build / "rom/neogeo.xml",
        "README.md": root / "README.md",
        "LICENSE": root / "LICENSE",
        "NOTICE": root / "NOTICE",
        "TECHNICAL_EXPLANATION.md": root / "TECHNICAL_EXPLANATION.md",
        "docs/HISTORY.md": root / "docs/HISTORY.md",
        "docs/PUBLISHING.md": root / "docs/PUBLISHING.md",
        "docs/LICENSING.md": root / "docs/LICENSING.md",
        "toolchain.txt": build / "toolchain.txt",
    }
    for package in ("ngdevkit", "ngdevkit-toolchain", "ngdevkit-gngeo"):
        files[f"licenses/{package}-copyright.txt"] = Path(f"/usr/share/doc/{package}/copyright")
    for license_name in ("GPL-2", "GPL-3", "LGPL-3"):
        files[f"licenses/{license_name}.txt"] = Path(f"/usr/share/common-licenses/{license_name}")
    # A passing controller replay must also leave usable visual evidence.
    for name in ("cube", "cube-rotated", "pyramid", "light-before", "light-after"):
        with Image.open(build / f"native-captures/{name}.png") as capture:
            assert capture.size == (320, 224), f"Unexpected capture dimensions: {name}"
            capture.verify()
    # Validate the cartridge itself, not just the outer archive's filenames.
    expected = {f"3dgeo_demo-{part}.{part}" for part in ("p1", "s1", "c1", "c2", "m1", "v1")}
    with zipfile.ZipFile(files["rom/3dgeo_demo.zip"]) as cart:
        assert set(cart.namelist()) == expected, "Unexpected cartridge contents"
        assert cart.testzip() is None, "Corrupt cartridge"

    archive = out / "3dgeo-demo.zip"
    manifest = {"commit": revision, "sha256": {}}
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as bundle:
        for name, path in files.items():
            data = path.read_bytes()
            manifest["sha256"][name] = hashlib.sha256(data).hexdigest()
            bundle.writestr(name, data)
        bundle.writestr("manifest.json", json.dumps(manifest, indent=2) + "\n")
    source = out / "3dgeo-demo-source.tar.gz"
    subprocess.run([
        "git", "archive", "--format=tar.gz", "--prefix=3dgeo-demo/",
        f"--output={source}", revision,
    ], cwd=root, check=True)
    sums = "".join(
        f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n"
        for path in (archive, source)
    )
    (out / "SHA256SUMS").write_text(sums)
    print(f"Packaged {revision}: {archive.relative_to(root)} (no BIOS archives)")


if __name__ == "__main__":
    main()
