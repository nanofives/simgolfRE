# S3.1b merge (U-1300)

Files owned by S3.1b go in as they are (log/roadmap/S3/1b_files.txt). The edits below touch files S3.1b does not own.

## 1. port/test/test_terrain_render.py (S3.1's file): the g6 passes are no longer expected to fail

All 28 g6 draw-list passes now meet the criterion (they XPASS in log/roadmap/S3/1b_pytest_test_terrain_render.log). Replace

```
# g6 is a constructed state (60 % of the tiles made type 7, the border column not hidden: control 12's recording). There GlRaster
# misses the criterion on 16 of 28 passes, by the same pixels when it replays the ORIGINAL's recorded stream (the port's stream is
# equal line for line), so it is the rasteriser's error on that state, not the renderer's: U-1300, log/roadmap/S3/1_notes.md.
RASTER_OPEN = {"g6"}
```

with

```
# g6 is a constructed state (60 % of the tiles made type 7, the border column not hidden: control 12's recording). GlRaster missed
# the criterion on 16 of its 28 passes until the generic GL's specular table and tie rule were modelled (U-1300 resolved,
# log/roadmap/S3/1b_notes.md); no pass is expected to fail now.
RASTER_OPEN: set = set()
```

## 2. UNCERTAINTIES.md

- Append the three rows of log/roadmap/S3/1b_uncertain.tsv (U-1390 .. U-1392).
- U-1300 (from log/roadmap/S3/1_uncertain.tsv): status `resolved`, with this sentence appended to its description:
  `Resolved by roadmap S3.1b: the miss was the generic GL's specular table (U-1390; the untextured type-7 tiles' constant lit colour sat on a quantisation threshold) and its tie rule for centres exactly on a horizontal edge (U-1391; i005's 145 pixels); all 28 g6 passes now meet G2 from the original's stream and from the port's (log/roadmap/S3/1b_notes.md).`
- U-1076: append `S3.1b: the specular table (U-1390) removes most of the one-level noise this row describes (c7 mean 0.46 % -> 0.06 %, t3 0.49 % -> 0.05 %); the per-triangle residual must be re-measured with it (U-1392).`

## 3. re/PHASE5_TERRAIN_PLAN.md, section 6: a new item after 18

```
19. DONE, the specular table and the tie rule (roadmap S3.1b, U-1300 / U-1390 / U-1391; 2026-10-24; notes log/roadmap/S3/1b_notes.md).
    Recording g6 (60 % type-7 tiles) missed the criterion on 16 of 28 passes with the ORIGINAL's own stream. Split by block category
    against glreplay.exe (log/roadmap/S3/1b_probe.py): the untextured tiles (bound texture 0, drawn with the lit colour) had 6.25 %
    one-level pixels, all on one constant colour sitting 0.0034 (8-bit units) above a quantisation threshold. Probes of lit
    untextured quads (glprobe.py untex) showed ambient and diffuse exact and the specular term a step function: the generic GL reads
    (n.h)^shininess from a 256-entry table over [t, 1], t = 0.0005^(1 / shininess), at the nearest entry (0 of 4,923 probe quads
    outside their intervals; exact at shininess 1 .. 128; gl_raster.cpp spec_lookup, tune 27 = 1 restores the pow). It was also most
    of the one-level noise everywhere: c7 0.46 -> 0.06 %, t3 0.49 -> 0.05 %, g2 .. g5 0.37 .. 0.49 -> 0.03 .. 0.04 %. g6/i005's
    145 pixels more than one level off were path bands whose horizontal bottom edge rounds to y = 344.5 exactly: centres on a
    horizontal edge belong to edges pointing to +x, not -x (tie probe 48 coverage differences against 288 for the old rule; tune 10 = 2
    restores it); 10 are left. Result: all 271 recorded-stream replays and all 237 draw-list chains meet G2, no pass worse.
```
