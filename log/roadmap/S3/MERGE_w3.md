# S3 merge, wave 3 (S3.2: the terrain drawn into the frame)

Apply after S2 wave 1 (gfx_body.h / gfx_families.inc) and S3 wave 1. files_w3.txt lists this wave's files; nothing is staged.
Notes: log/roadmap/S3/2_notes.md. The shared-file edits are log/roadmap/S3/2_merge.md section 1 (exact text and order; the
script log/roadmap/S3/2_merge_edits.py applies them with exact-match checks, as it did in S3.2's mirror port/build/S3_2_root):

1. 2_merge.md 1.1 port/src/sim/gfx_families.inc: the `terrain` family line (body file port/src/sim/gfx_terrain.cpp).
2. 2_merge.md 1.2 port/src/sim/gfx_body.h: the per-snapshot terrain flag in class Gfx (S0 recommendation 2).
3. 2_merge.md 1.3 port/src/sim/gfx.cpp: four places (flag cleared by a full-screen 2D write, copied at refreshFull,
   `snapshot_terrain\t<k>\t<0|1>` in gfx.log).
4. 2_merge.md 1.4 port/test/framegate.py: LiveTerrain on both backends (S0 recommendation 5) and the `terrain_drawn` column.
5. 2_merge.md 1.6 UNCERTAINTIES.md: append the rows of log/roadmap/S3/2_uncertain.tsv (U-1310 ..).
6. Nothing for modules.inc (2_merge.md 1.7).

2_merge.md 1.5 (the census rule for the terrain verdict) is not a shared-file edit: it is how S3.9 ranks the census
(log/roadmap/S3/2_census.py), and stays with S3.

## CLAUDE.md paragraph (append after the S3 wave 2 paragraph)

```
Roadmap S3 wave 3, the terrain in the frame (2026-10-24/25, step S3.2, notes log/roadmap/S3/2_notes.md, U-1310..): the
original's GL draws into the ROOT surface (initTerrain gives Terrain::initSystem the DC of the jgld surface at
[g_rootWindow + 0x278]; that DIB's memory row 0 is the picture's top row, so GlRaster's buffer and gfx.root are the same
array, no flip). updateTerrainRender clears the root through Window::paint(g_rootWindow, 0x80000000) (clear16 with colour 0),
the passes draw over it, Surface_blitThunk copies root -> the exe's back surface g_image51b068, an unmoved camera copies the
back surface whole onto the root and local-renders changed tiles, and renderTerrainStrip draws the strip and copies the shifted
old picture back. Body family `terrain` (port/src/sim/gfx_terrain.cpp: terrain.windowPaint, terrain.pass, terrain.blit) runs
LiveTerrain's passes through S3.1's terrain_render_entry into a GlRaster over gfx.root; the GL setup is the port's own
(terrain_gl_setup). gfx.log carries a per-snapshot terrain flag (snapshot_terrain) and framegate a `terrain_drawn` column;
LiveTerrain attaches on Win32 too (runner32 / lib32 ops). EVIDENCE: the original's root surface recorded at the return of every
updateTerrainRender (re/tools/terrain_calls.py --pass-frames / --frames-lite, instance s3_2, every hooked run SAME as its
reference): per census checkpoint the port's root at the last pass before the checkpoint meets G2 over the pixels the original
drew on all 527 terrain checkpoints (log/roadmap/S3/2_terrain_verdicts.tsv; S0's `terrain not drawn` 520 -> 0), four themes,
x64 = Win32 terrain pixels; controls (wrong zoom, wrong theme texture set, pass skipped, strip shift, Window::paint not clearing)
RED. Segments 205 / 205 and boot 58 / 2 / 1 unchanged (the backend writes no GameState byte). Frida spawns hung 5 times on the
instance (U-0683 pattern): kill the own PID and retry. Open: U-1310 (rasteriser fixed at 800 x 600), U-1311 (the back surface's
depth inferred from results), U-1312 (pathUpdateRender never runs organically), U-1313 (one organic strip direction framed),
U-1315 (two title rows keep the terrain flag at 1).
```

## Gate (expected numbers on a build holding S2 wave 1 + S3 waves 1-3; S3.2's mirror numbers in 2_notes.md section 3)

- port/test/test_terrain_live.py: x64 252 passed, 4 skipped (log/roadmap/S3/2_pytest_terrain_live_x64.log); Win32 141 passed,
  3 skipped (no runner op for sg_ts_negctl; 2_pytest_terrain_live_w32.log).
- test_gl_raster, test_terrain_state, test_terrain_gl_setup, gfx tests, port static as in 2_pytest_*.log.
- segments x64 205 / 205 and Win32 205 / 205, 0 differ from log/roadmap/merges/S6-1 (2_segs_*_compare.log).
- boot: HEAD's 61 scenarios 58 / 2 / 1, 0 rows differ from S0 (2_boot_compare.log).
- terrain verdict: `~/bin/py -3.12 log/roadmap/S3/2_census.py terrain` -> 527 / 527 PASS (needs the recordings, local data).

## Local data (gitignored)

The per-pass root recordings under log/p5/gl / port/corpus (2_notes.md lists them), instances\s3_2.
