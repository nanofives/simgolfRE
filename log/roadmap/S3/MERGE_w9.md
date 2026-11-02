# S3 wave 9 merge request (lead cmvu18cc81fcpp715vy6fd2dv, 2026-10-31)

Base: **2291a342 + MERGE_w8** (the wave 8 stack, being committed by I21) + the two wave 8 fix layers for I21's suite. Steps: S3.28
(candidate 2, lowerEdgeCorner's black-light pass, U-1952 / U-1955 / U-1956) and S3.29 (candidate 1, U-1309, a FINDING: rule behind a
switch, default off). Both accepted after the lead re-checked their numbers on disk (STATUS.md log, re/roadmap/S3_steps.tsv). Nothing is
staged; the lead does not commit.

## 1. How to apply

```
~/bin/py -3.12 log/roadmap/S3/w9_assemble.py --dst <tree> [--commit 2291a342]
```

w9_assemble.py runs, each through its own `--root` form, every anchor exactly once (refusal stops it):

| # | layer | assembler | port files |
|---|---|---|---|
| 1 | wave 8 stack (MERGE_w8 section 1) | w8_assemble.py | see MERGE_w8.md |
| 2 | I21 fix 1 | w8_fix_i21.py | test/test_gfx_pixel.py, test/test_gfx_reports.py |
| 3 | I21 fix 2 | w8_fix_i21b.py | include/portsim/sim/gl_wgl.h, src/sim/gl_wgl.cpp, test/test_gl_wgl.py, test/test_gl_wgl_selfcheck.py |
| 4 | S3.28 | 28_assemble.py --only-28 | include/portsim/sim/terrain_render.h, src/sim/terrain_render.cpp, terrain_live.cpp, gfx_terrain.cpp, test/lib32.py, test/test_gl_wgl_selfcheck.py, test/test_terrain_s3_28.py (new) |
| 5 | S3.29 | 29_assemble.py --only-29 | include/portsim/sim/gl_raster.h, src/sim/gl_raster.cpp, test/test_gl_raster.py |

On a tree that already holds I21's commit (wave 8 + both fix layers), apply only rows 4 and 5. S3.28 and S3.29 share no file;
test_gl_wgl_selfcheck.py takes edits from rows 3 and 4 and both apply. Then append `w9_uncertain.md`'s rows (11; U-1952 and
U-1955..U-1957 REPLACE the rows I21 adds from 27_files.txt) and insert the CLAUDE.md paragraphs of section 4. Tracked log files:
`files_w9.txt` (166 paths under log/roadmap/S3 plus re/roadmap/S3_steps.tsv; probe scenes, recordings, images and trees are local).

## 2. Gates, recomputed by the lead on port/build/S3_W9 (w9_gates.sh + w9_gates2.sh, logs log/roadmap/S3/w9_out/, summary.txt)

Every run one w8_lock.py hold, --jobs 1; segments / boot / census under the machine test slot.

| gate | result |
|---|---|
| assembly | all five rows applied, no anchor refused |
| build x64 / Win32 | rc 0 / rc 0 |
| port tests x64 | test_static 6, test_gl_raster 278, test_terrain_s3_28 107, test_gl_wgl 9, test_gl_wgl_selfcheck 17 (+7 app tests skipped), test_terrain_render 522, test_terrain_live 432, test_gfx 25, test_gfx_pixel 17, test_gfx_reports 24 (+12 deselected), test_framegate_split 17, test_gfx_s3_26 9: all pass |
| port tests Win32 | test_terrain_s3_28 4 (+103 x64-only skipped: no sg_gl / sg_raster op on the runner), test_gl_wgl_selfcheck 17 (+7), test_gfx 25 |
| segments x64 / Win32 | 205 / 205 each, 0 rows / digests differ from the wave 8 stack (w8_out/segs_*.tsv) |
| boot x64 / Win32 | 66 PASS / 2 KNOWN / 2 NO_FRAMES each, 0 rows differ from wave 8, Win32 = x64 |
| census RAW (default GlRaster), 698 rows | 126 PASS / 562 FAIL / 0 / 10 against wave 8's 123 / 565 / 0 / 10; 0 PASS lost, 0 rows worse; differing px 6,203 -> 5,962 |
| census SPLIT (G18), 698 rows | 685 PASS / 3 FAIL / 0 / 10 against wave 8's 677 / 11 / 0 / 10; 0 PASS lost |

Exactly 8 rows move, all cov_r_s2_screens (S3.28): course 69 -> 4, radial_hover 68 -> 3, rep_comments 24 -> 4, rep_financial 21 -> 1,
rep_shortcuts 21 -> 1 (split PASS, raw keeps 1..4 px of GlRaster terrain residue), rep_histograph 20 -> 0, rep_report1 20 -> 0,
rep_roster 11 -> 0 (PASS raw and split). S3.29's rule is off by default, so it moves no row (its own gates: section 3).

The 3 split FAIL rows left: cov_k3_camera y_inj3 (847 px, the explained frame divergence U-1834), cov_a_build brush_465 (3 px, the line
end cap U-1826), cov_a_keys k_dlg_R (1 px, a one-level line value U-1829).

Not run by the lead: the full port suites, chains, live equivalence, app replay and the app tests (integrator; the app pacing files need
a quiet machine).

## 3. The steps

**S3.28** (opus, 28_notes.md / 28_merge.md): renderTileAtFiveOffsets (Terrain.dll 0x10038900) + enableBlackLight1 (0x10003830) as
terrain pass kind 5, handed to the backend from LiveTerrain::lower_edge_corner before the state call; the recorder carries the angle
(U-1956). Oracle: recording s328_s2b on instance s3_w8 SAME as det_b at 32 checkpoints; 12 organic + 20 direct passes at 0 / 90 / 180 /
-90, all 32 streams equal the original's line for line; host GL equals the original's pixels on all 32, GlRaster within G2 (2 px in
total). Controls RED: pass skipped (the 65 px back), renderer draws nothing, second offset -10, 90 / 180 neighbour sets swapped; light 1
ambient 0.2 RED on streams and host GL, blind in GlRaster (light 0 only; U-1957: GlRaster needs no GL_LIGHT1 while its colours are 0).
Open: U-1960 (the fault order with a pivot outside the grid; no recording has one). Process: one chained hold broke the one-file rule.

**S3.29** (opus, 29_notes.md / 29_merge.md), a FINDING: the generic GL's textured interpolation (float32 plane walk from the lowest
vertex, rows from the largest y, row start restarted at the middle vertex, float32 final stage) is in both pixel loops behind
`#tune 32 1`, DEFAULT OFF because it is exact on 9 probes and not on 4 (U-1965, 1-2 float32 ulps of u / v). With the rule on: census raw
123 -> 131 PASS, 0 lost / 0 worse, split unchanged; stream one-level 2,296 -> 1,938, chains 1,605 -> 1,387, all within G2. Default path:
505 recorded passes hash-equal to the wave 8 build, stream / chain G2 equal to S3.21's numbers, segments 205 and boot 66/2/2 on x64 and
Win32 with 0 rows differing. Controls RED (rule off, double final stage, anchor variants, no restart, workerctl); `#fastpath 4` blind with
the rule on (walked pixels never take the reciprocal path). Finding U-1968: most of the remaining terrain residue is NOT interpolation:
per-vertex lighting (whole tile bands), coverage ownership when an edge passes 2e-6..2.4e-3 px from a pixel centre, and blended pixels.
Open: U-1966 (all-NEAREST textures, not used by the terrain), U-1967 (the float blend form), U-1969.

## 4. CLAUDE.md paragraphs (in this order, after the last wave 8 paragraph)

1. S3.28: 28_merge.md section 4.
2. S3.29: 29_merge.md "CLAUDE.md (suggested paragraph)".
3. The wave 9 note:
```
S3 wave 9 (2026-10-31, merge log/roadmap/S3/MERGE_w9.md, stack w9_assemble.py): on 2291a342 + wave 8 + the two I21 fix layers
(w8_fix_i21.py: the pixel family's body count and the hire RESIDUAL after S3.26; w8_fix_i21b.py: test_gl_wgl's sandbox_00 pin 125 -> 9 px
and the self-check's GlRaster control, which turns S3.21's rules off with glWgl.rasterCheck(1) since the default GlRaster now passes the
check) S3.28 and S3.29 give census raw 126 / 562 / 0 / 10 and split 685 / 3 / 0 / 10 (0 PASS lost, 0 rows worse); the 3 split FAIL rows
left are cov_k3_camera y_inj3 (U-1834, explained), cov_a_build brush_465 (U-1826) and cov_a_keys k_dlg_R (U-1829). w8_lock.py: a
`--priority` step that made the last hold yields one turn (alternation), one pytest file per hold, and `run` starts its command with
CreateProcess (wrap ~/bin/py in `bash -c`; test-slot the same: `test-slot bash -c '...'`).
```

## 5. Local only (never commit)

Probe scenes port/corpus/gl/probe/s329_*, S3.28's recordings (s328_s2 / s328_s2b under port/corpus/gl and log/c3/alt/refs), 28_img/ /
29 images, *.tsv.d census frame dirs, the trees port/build/S3_28*, S3_29, S3_W8, S3_W9 (S3_26v / S3_26m hold junctions: never rm -rf).
