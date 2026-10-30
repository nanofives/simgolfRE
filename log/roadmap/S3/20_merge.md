# S3.20 merge note: the host-OpenGL startup self-check, opt-in (decision G13, final)

Decision G13 (final, Mariano, 2026-10-30): GlRaster stays the default terrain renderer; the host's OpenGL is opt-in only (`--gl auto`:
the self-check, then the host GL or the GlRaster fallback; `--gl host` / `wgl`: the host GL without the check); no banded host GL.

Base: the S3 wave 7 combined tree = `w7_assemble.py` on 37e58e96 (S3.17, S3.18, S3.19, S3.16, S3.15 and S3.9's 9_edits.py), byte-equal
to S3.9's port/build/S3_9 (`w7_assemble.py --verify port/build/S3_20_base port/build/S3_9`: 393 files, 0 differ). Apply:

```
~/bin/py -3.12 log/roadmap/S3/20_assemble.py --root <tree> --only-20     # a tree that already holds wave 7
~/bin/py -3.12 log/roadmap/S3/20_assemble.py --dst <name>                # a fresh one (runs w7_assemble.py first)
```

Every edit is anchored (old text exactly once, LF; the assembler keeps each target file's own line ends; test_gl_wgl.py goes as one
whole-file edit because a hunk's anchor repeats once an earlier hunk is applied); the two new files must not exist. Verified:
`20_assemble.py --dst S3_20_chk` equals the mirror port/build/S3_20 in every file of port/ (diff -rq --strip-trailing-cr).
Regenerate with `20_make_edits.py` (base port/build/S3_20_base), the reference with `20_make_ref.py`. The band measurement
(log/roadmap/S3/20_band/, band.patch) is NOT in 20_edits.py.

## 1. Own files (20_edits.py)

- port/include/portsim/sim/gl_wgl.h, port/src/sim/gl_wgl.cpp: `gl::SelfCheck`, `gl::wgl_self_check(ctx, variant, perturb)`,
  `gl::raster_self_check(ctx)`, `gl::sha256_hex`; harness entries glWgl.selfCheck / glWgl.rasterCheck
- NEW port/src/sim/gl_wgl_ref.inc (generated: the reference pass and the original's pinned SHA-256; no game file byte)
- port/include/portsim/sim/terrain_live.h, port/src/sim/terrain_live.cpp: `LiveTerrain::choose_renderer(raster|auto|host|wgl)`,
  `renderer()`; SG_GL_BACKEND accepts raster / auto / host / wgl (unset = GlRaster); harness entry terrainLive.renderer(mode)
  (writes `$wgl/renderer.txt`). The library default stays GlRaster.
- port/src/sim/gfx_terrain.cpp: header (controls 13 / 14, the G13 note) and the SG_TERRAIN_PASS_LOG line of the host-GL branch
- port/test/framegate.py: `--gl raster|auto|host` (default raster), run_scenario(gl=...), columns `renderer`, `renderer_reason`
- port/test/test_gl_wgl.py: the default rows are GlRaster's; `auto` and `host` equal S3.15's `--host-gl` rows
- NEW port/test/test_gl_wgl_selfcheck.py

## 2. S7 files (the app; small, listed as the lead asked)

- port/app/main.cpp: option `--gl raster|auto|host|wgl`. Without it, SG_GL_BACKEND (same values) chooses, else GlRaster.
  TerrainGuard::attach(o, ctx, rec) calls `LiveTerrain::choose_renderer` before the first Terrain call and prints
  `simgolf_port: terrain renderer <raster|wgl> (<default|--gl|SG_GL_BACKEND> <mode>): <reason> [self-check <n> ms]` to stderr. It
  writes `terrain_renderer.json` (source, mode, backend, reason, checked, pass, got, want, ms) into the record directory. The three
  attach call sites pass `rec`, and the head comment documents the option.
- port/test/test_app_ingame.py and test_app_live.py:
  - the library replays that rebuild the app's frames use the renderer the session recorded (terrain_renderer.json);
  - judge_live_trace / framegate_frames take a `gl` argument (default raster);
  - test_app_gl_auto_end_to_end is the opt-in path end to end: `--gl auto` app frames equal framegate `--gl auto` frames in every
    pixel at every sandbox_basic checkpoint, and every in-game frame equals the original's.

Note for the integrator: the main working tree holds uncommitted S7 / S9 edits of port/app/main.cpp and host.cpp (audio). These
anchors are on the wave 7 tree (37e58e96's main.cpp); if the merge base carries those edits, run 20_assemble.py --dry first.

## 3. No shared file

Module gl_wgl is already in modules.inc (S3.15). No CMake change (opengl32 is still loaded at run time).

## 4. UNCERTAINTIES.md (append; rows in 20_uncertain.tsv)

- U-1815: the reference covers one zoom, one texture set and no wall vertices.
- U-1816: the check's startup cost on other PCs.
- U-1817: the reference's recorded run-time geometry under the no-game-bytes rule (the lead's call).
- U-1818: the host GL's frame time against GlRaster's, measured; the reason G13 keeps GlRaster the default.
- U-1819: a hang inside opengl32 with several contexts, seen in the band measurement.
- U-1810 stays open.

## 5. CLAUDE.md paragraph (after the S3.15 paragraph)

```
Roadmap S3.20, the host-OpenGL startup self-check (2026-10-30, decision G13 final, notes log/roadmap/S3/20_notes.md, merge 20_merge.md,
U-1815..U-1819): GlRaster stays the default terrain renderer; the host's OpenGL (S3.15's gl::WglSink) is opt-in. In the app,
`--gl auto` (or SG_GL_BACKEND=auto) runs a startup self-check and uses the host GL only if it passes, else GlRaster; `--gl host` (= wgl)
takes the host GL without the check. framegate has the same `--gl` (default raster) and columns renderer / renderer_reason. The app logs
its choice (stderr `simgolf_port: terrain renderer ...`, terrain_renderer.json). The check (gl_wgl.h wgl_self_check,
LiveTerrain::choose_renderer) runs the port's own GL setup from the user's install into a WglSink the way the live path does, draws the
19 triangles of the original's recorded pass c7/z2_r1 that reach a 32 x 32 rectangle (port/src/sim/gl_wgl_ref.inc, generated by
log/roadmap/S3/20_make_ref.py) and compares SHA-256 of the rectangle with the hash of the ORIGINAL's pixels there. Host GL equals the
original in 1,024 / 1,024 px; GlRaster differs in 45 (U-1392). Fallbacks, with the reason logged: wrong pinned hash (control 13),
perturbed reference (14), 565 DIB (8), no 16-bit generic pixel format (12). Census: default (GlRaster) 111 PASS / 577 FAIL / 10 NOT_RUN
of 698, equal to S3.9's row for row; opt-in auto 638 / 50 / 10, 0 PASS lost. Why not the default (U-1818): a full host-GL pass takes
33..43 ms against GlRaster's 6..12 ms with workers; app terrain frames median 45 / max 95 ms against 19 / 40. A banded host GL
(20_band/, measurement only) gave the same pixels and median 21 / max 46 ms, but hung inside opengl32 twice (U-1819).
```
