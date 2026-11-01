# S3 wave 8 merge request (lead cmvu18cc81fcpp715vy6fd2dv, 2026-10-31)

Base: **2291a342** (I20 merged). Steps: S3.21 (W8.2, two parts), S3.22 (W8.3 + W8.6), S3.23 (W8.1), S3.24 (W8.4a), S3.25 (W8.4b,
recordings only), S3.26 (W8.4c). All accepted after the lead re-checked their numbers on disk (STATUS.md log, re/roadmap/S3_steps.tsv).
Nothing is staged; the lead does not commit.

## 1. How to apply

```
~/bin/py -3.12 log/roadmap/S3/w8_assemble.py --dst <tree> [--commit 2291a342] [--no-21b]
```

w8_assemble.py makes `w7_assemble.py --plain` of the base, then runs each step's own assembler with `--root <tree>` in this order (every
anchor exactly once; an assembler refuses 0 or 2+ matches and the script stops there):

| # | step | assembler | port files |
|---|---|---|---|
| 1 | W8.6 (S3.22) | 22_assemble.py --only-22 | src/sim/gfx_chareditor.cpp |
| 2 | S3.21 part 1 | 21_assemble.py | src/sim/gl_raster.cpp, include/portsim/sim/gl_raster.h, test/test_gl_raster.py |
| 3 | S3.21 part 2 (optional) | 21b_assemble.py | src/sim/gl_raster.cpp, test/test_gl_raster.py |
| 4 | S3.24 | 24_assemble.py --only-24 | src/sim/terrain_overlay.cpp, include/portsim/sim/terrain_overlay.h, test/test_terrain_overlay.py |
| 5 | S3.23 | 23_assemble.py --only-23 | test/framegate.py, test/test_framegate_split.py (new) |
| 6 | S3.26 | 26_assemble.py --only-26 | src/sim/gfx_pixel.cpp, gfx.cpp, tick.cpp, tick_frame.inc, commands_screens.inc; test/gfx.py, framegate.py, test_gfx.py, test_framegate.py, test_app_ingame.py, test_gfx_s3_26.py (new) |

Applied on 2291a342 by the lead (port/build/S3_W8): all six layers applied with no refusal. No file of the list changed between
f2dff003 (the steps' base) and 2291a342: I20 touched port/app/*, audio_exe.*, app_replay.py, integrator.py and two new tests only.
No modules.inc / gfx_families.inc / CMakeLists change. Then copy the tree's port/ files of the table into the repo, append
`w8_uncertain.md`'s rows to UNCERTAINTIES.md and insert the CLAUDE.md paragraphs of section 4 after the S3.20 paragraph, in the table's
order. Tracked log files: `files_w8.txt` (353 paths, all under log/roadmap/S3 plus re/roadmap/S3_steps.tsv).

## 2. Gates

Per step (each on its own tree off f2dff003, heavy runs one at a time):

| gate | S3.21 p1 | S3.21b | W8.6 | S3.24 | S3.23 | S3.26 |
|---|---|---|---|---|---|---|
| default census (GlRaster), 698 rows | 122 / 566 / 0 / 10, 0 PASS lost, 0 worse | not run (default path unchanged: `#tune 30` = 0) | not run | 111 / 577 / 0 / 10 equal in status, 29 rows fewer px | raw = f2dff003 on 698 / 698 rows | 111 / 577 / 0 / 10, 0 worse, 10 fewer |
| host-GL / split | - | - | - | 29 / 31 group 1 rows PASS (x64 = Win32) | split 638 / 50 / 10 / 0 = census_auto row for row | 91 / 100 rows PASS; 9 FAIL = groups 2 + 5 |
| segments x64 / Win32 | 205 / 205 both | - | 205 x64 | 205 / 205 both | - (test-only change) | 205 / 205 both, 0 digests differ from S3.20 |
| boot x64 / Win32 | 66/2/2 both | - | 66/2/2 x64 | 66/2/2 both | - | 66/2/2 both |
| unit tests | test_gl_raster 248, terrain_render 522, static 6 | test_gl_raster 250 | test_gfx 25 x64 + Win32, gfx_s3_19 | test_terrain_overlay 33 x64 + Win32, frame tests 6, static 6 | test_framegate_split 17 x64 + Win32, static 6 | static 6, gfx 25, gfx_s3_26 9, framegate 32, commands 83, tick 81 |
| controls | 2 / 2 RED | 1 RED (stepping off) | gfx.check 137 | 7 / 7 RED | 3 / 3 RED | pixel 2, 3, hire 3, poll16 RED |
| gfx.check | - | - | 0 dup / 137 | - | - | 0 dup / 138 |

Stack check on 2291a342 (port/build/S3_W8, `bash log/roadmap/S3/w8_gates.sh` + `w8_census.sh`, logs log/roadmap/S3/w8_out/): section 6. The full census, chains, live equivalence, app replay and the app tests are the integrator's (the app pacing files need a quiet
machine, S3.14).

Merged-tree census (port/build/S3_W8, 698 rows, section 6): raw 123 / 565 / 0 / 10 (was 111 / 577 / 0 / 10), split 677 / 11 / 0 / 10
(was 638 / 50 / 0 / 10).

## 3. Decisions taken and open items

1. **Group 5, U-1834: EXPLAINED FRAME DIVERGENCE (orchestrator, 2026-10-31, option a).** One row: cov_k3_camera y_inj3, 847 px. The
   original's worldToScreen(0xc7ff, 0xd800) reads the cell past the grids, byte 0 of the Surface_ctor vtable word stored in
   g_surfaces5439e0 (0xd8 = -40, so sy + 215), and draws the cursor diamond's connector to (431, 496). The port keeps its regular
   diamond: copying the byte would put an exe .rdata value into the port's state, and no simulation read of those bytes is measured.
   Re-open only if a simulation read of those objects shows up. U-1834's row in w8_uncertain.md carries this resolution.
2. **Group 2, U-1952: open, outside MERGE_w8.** Terrain, not 2D (the port's Terrain calls and all 32 states equal the original's; the
   original's pass draws a near-black dither over the map's right edge that the port's draw list does not hold). Game slot granted
   (orchestrator, 2026-10-31): S3.27 records the GL stream of the full render at Terrain call seq 19931 on instance s3_w8; no port code;
   a fix is wave 9.
3. **Wave 9 candidate 1: U-1309, GlRaster's textured interpolation** (WAVE8.md section 4: value about 638 PASS on the default renderer
   if exact; 8-10 h, one opus worker, no game run). Deferred by the orchestrator.
4. Open U rows: U-1305..U-1309 (S3.21), U-1826..U-1829 (S3.24), U-1950..U-1952 (S3.26).

## 4. CLAUDE.md paragraphs (in this order, after the S3.20 paragraph)

1. S3.21 part 1: 21_merge.md "CLAUDE.md (suggested paragraph)", plus this sentence for part 2: `S3.21b (21b_edits.py, opt-in #tune 30):
   the 16.16 fixed-point colour stepping is exact on untextured triangles (ramp 705 -> 0 px, Gouraud 481 -> 90) and changes nothing on the
   textured terrain, so it stays off; U-1309 is the textured (GL_MODULATE) interpolation.` and: `Both part 1 rules are on by default
   (orchestrator, 2026-10-31: exact stage models, not fitted); SG_RASTER_TUNE=28=1,29=1 restores the old renderer row for row.`
2. S3.24: 24_merge.md "CLAUDE.md (after the S3.20 paragraph)".
3. S3.23 (text below).
4. S3.26: 26_merge.md section 4, with its last U-1834 clause (`storing one needs an exe .rdata address: the lead's decision`)
   replaced by `kept as an explained frame divergence on cov_k3_camera y_inj3 (orchestrator, 2026-10-31)`.
5. The lead's process paragraph (text below), ending with: `Merged-tree census on 2291a342: raw 123 / 565 / 0 / 10 (0 PASS lost, 576
   rows fewer px, 71,871 -> 6,203), split 677 / 11 / 0 / 10; the 11 are group 5 (explained), group 2 (U-1952, 8 rows), U-1826, U-1829.`

S3.23:
```
Roadmap S3.23, the census split verdict (2026-10-31, decision G18, notes log/roadmap/S3/23_notes.md, edits 23_edits.py): framegate.py
--split runs each scenario a second time with the host-GL terrain (S3.15's WglSink, bit-exact against the original's terrain). A
differing pixel is terrain-origin when it differs under GlRaster and equals the reference under host GL, on rows with terrain on
screen; every other differing pixel is 2D. Split PASS = 2D px 0 in every region and terrain-origin px within G2 over the view area (the
census's G2 denominator, U-1121). Raw columns stay unchanged (raw = f2dff003 on 698 / 698 rows); a host without a 16-bit generic pixel
format reads UNJUDGED, never PASS. Census 70 scenarios: split 638 / 50 / 10 / 0 = the host-GL census row for row; every split FAIL is
2D only; the largest terrain-origin count is 270 px (8 beyond), inside G2. Test seam SG_FRAMEGATE_PLANT=<both|raster>:<region>:<n>
(controls: a 2D pixel planted in hud_title stays FAIL; a terrain-only difference beyond G2 fails as terrain-origin).
port/test/test_framegate_split.py 17 x64 + Win32.
```

Lead process:
```
S3 wave 8 heavy-run lock (2026-10-31): log/roadmap/S3/w8_lock.py take | release | run <step> -- <cmd> | status enforces one S3
heavy run at a time (a build, a pytest file, a census batch or a game boot): `take` writes w8_wait_<step> (epoch, its pid), blocks
until that file is the oldest live wait file, the lock dir w8_heavy.lock is free and >= 16 GB commit charge is free; wait files of a dead
take pid are removed; `release` refuses another step's lock; `run` always releases; `--priority` (lead / orchestrator only) ranks first,
used for I20's quiet window (w8_i20_window.py). A hand-written lock seen by a waiter is logged to w8_breaches.log. It replaced a written
oldest-first rule that workers broke four times in one night (a helper that checked only the lock dir, an unlocked finish step).
S3.22 (W8.3) classified the 50 host-GL FAIL rows into seven cause groups (22_causes.tsv, images local) and removed the unused body
showCharacterEditor.bio (W8.6, gfx.check 137 bodies); S3.25 (W8.4b) recorded the original for groups 2-7 on instance s3_w8 (8 runs SAME
as det_b, port/corpus/w8rec/ and log/p5/terrain/s325_* local, 25_notes.md).
```

## 5. Local only (never commit)

Game frames and images: 22_img/, 26_img/, *.tsv.d/ census frame dirs, 24_data/*.npz and *.bin, 21_glfeed/*.f32, 21_glfeed/*.exe/.obj.
Recordings: port/corpus/w8rec/, port/corpus/terrain/pin_cov_r_s2_screens/, log/p5/terrain/s325_cov_r_s2_screens/, log/c3/alt/refs/*/s325_*.
Trees: port/build/S3_21*, S3_22, S3_23, S3_24*, S3_26*, S3_W8. port/build/S3_26v and S3_26m hold junctions to original/, log/ and the
corpus: never `rm -rf` them (remove the junctions first). Instance instances/s3_w8 is kept for a group 2 recording or W8.5.
Lock files w8_heavy.lock/ and w8_wait_* are runtime state.

## 6. Stack check on 2291a342 (port/build/S3_W8), recomputed by the lead

Every run one w8_lock.py hold, --jobs 1 (w8_out_gates.log, w8_out_census.log, w8_out/census_summary.txt).

| gate | result |
|---|---|
| assembly (w8_assemble.py) | all six layers applied, no anchor refused |
| build x64 / Win32 | rc 0 / rc 0 |
| port tests x64 | test_static 6, test_gl_raster 250, test_terrain_overlay 33 (+6 opt-in skipped), test_framegate 32, test_framegate_split 17, test_gfx 25, test_gfx_s3_26 9, test_gfx_s3_19 15 (+6 opt-in skipped): all pass |
| port tests Win32 | test_terrain_overlay 33, test_framegate_split 17, test_gfx 25, test_gfx_s3_26 9: all pass (test_gl_raster is an x64-only harness: glraster.py binds the x64 DLL, never in the Win32 suite list) |
| segments x64 / Win32 | 205 / 205 PASS each, 0 digests differ from S3.20's segs_x64 / segs_win32 tables |
| boot x64 | 66 PASS / 2 KNOWN / 2 NO_FRAMES, 0 rows differ from 22_boot_x64.tsv |
| census RAW (default GlRaster), 698 rows | 123 PASS / 565 FAIL / 0 UNSUPPORTED / 10 NOT_RUN against f2dff003's 111 / 577 / 0 / 10: 0 PASS lost, 0 rows with more differing px, 576 rows with fewer, 12 new PASS; differing px 71,871 -> 6,203 |
| census SPLIT (G18), 698 rows | 677 PASS / 11 FAIL / 0 UNJUDGED / 10 NOT_RUN against S3.23's 638 / 50 / 10 on f2dff003: 0 PASS lost, 39 new PASS; 0 PASS lost against the host-GL census census_auto; largest terrain-origin count on any row 27 px (7 beyond one level), inside G2 |

The 11 split FAIL rows (all differ under host GL too, so the split counts them as 2D):
- cov_k3_camera y_inj3, 847 px: group 5, the explained frame divergence (U-1834, section 3.1);
- cov_r_s2_screens course, radial_hover (65), rep_comments / financial / histograph / report1 / shortcuts (20), rep_roster (11): group 2,
  the original's terrain pass draws a dark primitive the port's draw list lacks (U-1952, S3.27 recording);
- cov_a_build brush_465, 3 px (the line end cap at the second vertex, U-1826) and cov_a_keys k_dlg_R, 1 px (a one-level line value, U-1829).

Not run by the lead: the full port suites file by file, chains, live equivalence, app replay and the app tests (integrator; the app pacing
files need a quiet machine, S3.14), Win32 boot (each step ran it on its own tree: S3.21, S3.24, S3.26 66/2/2 equal), Win32 census.
