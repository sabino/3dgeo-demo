# Demo history

This repository preserves the original 35-commit model-demo history. The
milestones below summarize the progression from the first shade-matrix
experiment to the original demo baseline (`9b166b0`, also tagged
`legacy-demo-baseline` during the standalone extraction):

- The initial commits established FIX-layer shading, then moved to a
  framebuffer-like sprite renderer with cube pitch, palette lighting, and
  masked face coverage.
- Subsequent renderer work added solid shaded faces, removed unnecessary edge
  lines, improved zoom/detail, and introduced the sprite microframebuffer
  path.
- Performance commits reduced dense primitive complexity, sped up face
  shading and framebuffer blits, and preserved non-convex donut normals.
- Build-time OBJ import and the bundled cube, pyramid, tetrahedron, ball,
  donut, cone, and cylinder made the demo a reusable model template.
- UI commits added model picking, Neo Geo control names, status-register
  START/COIN handling, and a controls overlay with improved picker contrast.
- Lighting commits added the cube-light scene and refined its controls.

Useful checkout points for studying the original experiments:

- `60d2381`: first shade demo.
- `5999c55`: live FIX framebuffer rendering.
- `be098cc`: first sprite framebuffer.
- `0e07f8b`: sprite microframebuffer introduction.
- `fbc768d`: consolidation around the microframebuffer path.
- `dd8e77c`: build-time OBJ import.
- `22cdb44`: interactive cube-light scene.
- `46ccfe7`: primitive model collection.
- `9b166b0`: original documentation baseline, before this extraction.

No commits were squashed, cherry-picked away, or rewritten. The full progression
is useful historical evidence, including abandoned FIX paths and intermediate
fixes; older code remains discoverable through Git without cluttering the live
template. Historical GPL-3.0-or-later notices and original authorship are
preserved in those commits. The owner subsequently relicensed the current
project-owned code under Apache-2.0 with a readable NOTICE attribution; see
[LICENSING.md](LICENSING.md). Historical grants are not revoked.

The current work focuses on consistent face lighting and time-based rotation
at the native vblank cadence: at 60 Hz, manual rotation is one angle unit per tick
(84.375 degrees/second), zoom changes by 6 per tick (360/second), and light
movement is 6 units/tick in X/Y or 8 in Z. Native visual checks also uncovered
overlapping sprite windows and L0 last-line repetition. The framebuffer now
uses four non-overlapping bands of 16/16/16/8 source tiles rather than two of 28,
and removes undefined signed shifts found by sanitizers. Geometric cast shadows,
near-plane clipping, and a broader renderer-module split are not implemented in
this pass. No voxel-game code or uncommitted experiments were imported.
