# Build, CI, and publishing

## Automated checks

`.github/workflows/build.yml` runs on pushes, pull requests (including forks),
and manual dispatch. The `ROM and regression tests` job uses Ubuntu 24.04 to:

1. Install ngdevkit from its [official PPA](https://github.com/dciabrin/ngdevkit#installing-pre-built-binary-packages).
2. Build all cartridge assets from a clean checkout.
3. Run the actual C renderer under ASan/UBSan on the host.
4. Replay controls in headless MAME, checking rotation timing and light movement.
5. Upload a ROM bundle, matching source snapshot, SHA-256 checksums, and diagnostics.

The ROM bundle contains `rom/3dgeo_demo.zip`, GnGeo/MAME game definitions,
documentation, license, exact revision, package versions, and file hashes.
It deliberately excludes `aes.zip` and `neogeo.zip`: CI uses ngdevkit's
open-source test BIOS, but users supply their own emulator/BIOS installation.
The source archive is made from the exact Git revision that was built.

Actions are pinned to full commit IDs and the Ubuntu base to an image digest.
Dependabot proposes updates monthly. The PPA supplies rolling nightly packages,
so the complete toolchain is **not immutable**; `toolchain.txt` records what each
build used. This is not a claim of bit-for-bit reproducible ZIP output.

CI has read-only repository permissions, does not retain checkout credentials,
and requires no custom secrets. Only the separate tag-release job gets write
permission. It downloads artifacts without checking out or executing game code.
There is no `pull_request_target` workflow.

## Reproduce CI locally

Commit changes first, then clone into a fresh directory; `ci/run.sh` refuses a
dirty checkout or an existing `build/ci` directory. On Linux with Docker:

```sh
git clone /path/to/3dgeo-demo /tmp/3dgeo-demo-ci
cd /tmp/3dgeo-demo-ci
docker build --tag 3dgeo-ci --file ci/Dockerfile .
docker run --rm --user "$(id -u):$(id -g)" --volume "$PWD:/workspace" 3dgeo-ci
```

Results appear in `build/ci/dist/`; native screenshots are in
`build/ci/native-captures/`. The native replay is a smoke test, not proof of
hardware accuracy or exhaustive rendering correctness. Review captures and
test in GnGeo before releasing. Physical Neo Geo hardware is still unverified.

## First GitHub publication (manual)

No GitHub repository is created by these scripts. When the name and visibility
are agreed, create an **empty** repository (no generated README or license),
add its URL as `origin`, and push the current branch plus the historical tag.
For example, with a chosen public repository name:

```sh
gh repo create sabino/3dgeo-demo --public --source=. --remote=origin
git push -u origin HEAD
git push origin legacy-demo-baseline
```

These are instructions, not actions already performed. The entire original
35-commit history is preserved; see [HISTORY.md](HISTORY.md). Do not force-push
or squash it when publishing. Review historical content as well as HEAD before
making it public. Generated files, BIOS archives, and local configuration must
remain untracked. No badge points to a nonexistent GitHub repository.

After the first successful hosted run, configure a branch ruleset requiring
`ROM and regression tests` before merges, and enable GitHub secret scanning /
push protection where available. Repository settings cannot be applied until
the GitHub repository exists.

## Release a tested version

```sh
git tag -a v0.1.0 -m "First standalone demo release"
git push origin v0.1.0
```

Pushing a `v*` tag builds and tests that revision, then creates a **draft** GitHub
release containing the verified bundle, matching source, and checksums. Review
the generated notes, licenses, screenshots, and ROM before clicking Publish.
Ordinary branches, pull requests, and `legacy-demo-baseline` never create releases.
If rerunning an already successful tag workflow, delete its draft first or use
a new version tag; the workflow intentionally does not overwrite release assets.

The demo is GPL-3.0-or-later. Keep its license and matching source available
alongside binaries. ngdevkit's runtime/nullsound components come from the SDK;
retain upstream licensing information and review redistribution requirements
before publishing a release. Do not distribute proprietary BIOS or game ROMs.
