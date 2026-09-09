#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

if [[ -n $(git status --porcelain --untracked-files=all) ]]; then
    echo 'CI requires a clean checkout, including untracked files.' >&2
    exit 1
fi
if [[ -e build/ci ]]; then
    echo 'build/ci already exists; use a fresh checkout for CI.' >&2
    exit 1
fi

mkdir -p build/ci
{
    git rev-parse HEAD
    m68k-neogeo-elf-gcc --version
    cc --version
    python3 --version
    mame -version
    if command -v dpkg-query >/dev/null; then
        dpkg-query -W
    fi
} > build/ci/toolchain.txt

make docs-check
make -j2 BUILDDIR=build/ci cart test
make BUILDDIR=build/ci native-test
make BUILDDIR=build/ci screenshots
git diff --exit-code
python3 ci/package.py
echo 'CI PASS: docs, cartridge, sanitizers, native replay, gallery, and release package'
