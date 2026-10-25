# Regression baseline at HEAD b3d61e41 + S6 wave 2 + S3 wave 3 + S2 wave 5 (I6, 2026-10-25)

This file now describes **HEAD + S2 + S3 + S6 wave 2**: commit `b3d61e41` (which holds S0, S6 wave 1, S1, S8, S2 waves 1-4 and S3 wave 1)
plus S6 wave 2 (`log/roadmap/S6/MERGE.md`, `files.txt`), S3 wave 3 (`log/roadmap/S3/MERGE_w3.md`, `files_w3.txt`) and S2 wave 5
(`log/roadmap/S2/MERGE_wave5.md` -> `13_merge.md`, `files_wave5.txt`) and the shared-file edits of their merge (`log/roadmap/merges/I6.md`).
**S3 wave 2 (the flic module and the tropical-tree switch) is not in it**: integrator I6 refused it (I6.md). Measured by I6 in the mirror
`port/build/I6b`: git archive of HEAD, the junctions of `log/roadmap/S0/export.md` plus `shim/build`, `re/replay` and `extracted`, the
section files copied by `log/roadmap/merges/I6/mirror.py --dst I6b --drop s3w2` (S2 wave 5's seven new files from the S2.13 mirror
port/build/S2_13_head, every other one from the shared tree), the edits applied by `apply_shared.py port/build/I6b --drop s3w2`. All
commands run from the mirror root. Builds in the mirror: `port/build/x64` and `port/build/Win32` (logs `log/roadmap/merges/I6/build_I6b/`,
0 warnings). Python `~/bin/py -3.12`. Heavy runs one at a time, free commit charge checked before each (18.7-29.4 GB). Raw logs in
`log/roadmap/merges/I6/`. The first mirror `port/build/I6` (all four waves) gave the x64 suite and repo static of section 1 / 2 and the
two failures that refused S3 wave 2.

The previous baselines (HEAD alone by S0.1, HEAD + S6 by I1, HEAD + S1 + S8 by I4, HEAD + S2 + S3 by I5) are in git history; their logs
stay in `log/roadmap/S0/`, `log/roadmap/merges/S6-1/`, `S1S8-4/` and `S2S3-5/`.

## Summary

| Gate | HEAD + S6w2 + S3w3 + S2w5 | HEAD + S2 + S3 (I5) | Same? |
|---|---|---|---|
| repo static (`-m static`) | 614 passed, 2 skipped, 46 deselected (I6 and I6b) | 614 / 2 | yes |
| port suite x64, one file per process | 62 files; 61 PASS; test_boot 94 passed + 1 failed (cov_r_s2_screens: no divergence through record 1,156, stop at the unported showBuyLandScreen, U-1251) | 57 files; 56 PASS; test_boot 94 + 1 failed (at record 771) | yes for I5's files; +5 files |
| port suite Win32 backend | 52 files: 44 PASS, the same 8 files fail the same 20 tests (U-1100, U-1101) | 46 files: 38 PASS, 8 fail 20 | yes (identical failing test names) |
| segment gate x64 | 205 / 205 PASS, 0 digests differ from `log/roadmap/merges/S2S3-5/segs_x64.tsv` | 205 / 205 | yes |
| segment gate Win32 | 205 / 205 PASS, 0 digests differ from `S2S3-5/segs_win32.tsv` and from the x64 run | 205 / 205 | yes |
| boot gate `--all` x64 | 70 rows, 0 differ from I5 in status / records / expected (HEAD's 61 at 58 PASS / 2 KNOWN / 1 NO_FRAMES; 65 / 2 / 2 / 1 overall); cov_r_s2_screens FAIL 1,156 / 1,268 with no divergent record (I5: first divergence at record 771) | 70 rows | yes; cov_r_s2_screens now equal through 1,156 |
| boot gate `--all` Win32 | the same 70 rows, 0 differ from x64 | = x64 | yes |
| chains `integrator chain --all` | 342 / 342 PASS, every row equal to I5's | 342 / 342 | yes |
| compatibility switches `integrator defects` | 14 switches; sinScaled 49, prepareShot.hazardCellTransposed 13, the other 12 0 (the 2 new: loadGolfPro.negativeWindowBound, generateRandomCourse.placementBound) | 12 switches, same counts | yes (+2 switches, 0 segments each) |
| profiles `port.test.profiles validate` | `profile fixed: 8 member(s) of 14 switches` | 6 of 12 | +2 (S6 wave 2) |
| ledger (fresh inputs; written to re/port/divergences.tsv) | 28 rows, 1 without a cause (showRoutingMapScreen.exitRemapReadsFrame) | 26 rows, 1 without a cause | +2 rows: the two new defect switches |
| gfx bodies `gfx.check()` | 0 duplicate names, 84 bodies; Surface_drawSprite and Widget_fillRect registered once each, by `reports` | (wave 1-4 families) | U-1278 settled |
| frame census (`framegate --all`, ranked by S0's 3_rank.py) | 698 rows / 70 scenarios: PASS 87, UNSUPPORTED 598, NOT_RUN 13, FAIL 0; over I5's 698 rows 0 PASS lost | 698 rows: PASS 82, UNSUPPORTED 592, FAIL 4, NOT_RUN 20 | +5 PASS |
| S3 terrain verdict (`log/roadmap/S3/2_census.py terrain`) | 527 PASS, 0 FAIL, 75 NOT_MEASURED (no recording) | not measured (no terrain in the frame) | new record |

## 1. Repo static suite

```
~/bin/py -3.12 -m pytest -m static -q -p no:cacheprovider
```
**614 passed, 2 skipped, 46 deselected** on I6 (`log/roadmap/merges/I6/static.txt`) and on I6b (`static_I6b.txt`), equal to I5.

## 2. Port suite, x64, one file per process

```
STATIC=0 sh log/roadmap/merges/I6/suite_by_file_I6b.sh      # I6b; suite_by_file.sh is the same runner pointed at port/build/I6
~/bin/py -3.12 log/roadmap/merges/S2S3-5/suite_compare.py log/roadmap/merges/I6/suite_merged log/roadmap/merges/S2S3-5/suite
```
The full run was on I6 (`suite/`, `suite_x64.log`, 64 files); the files S3 wave 2's removal can change were re-run on I6b (`suite_I6b/`:
test_menu, test_terrain_live, test_profiles, test_static, test_defects_gen_w2, test_defects_golfers_w2, test_vectors, test_win32);
`suite_merged/` is the first with the second replacing it, minus S3 wave 2's test_flic / test_defects_flic: **62 files, 61 PASS**.

| File | now | I5 | Note |
|---|---|---|---|
| test_boot | 94 passed, 1 failed | 94 / 1 | the failure is still test_boot_gate[cov_r_s2_screens], now with no divergent record: `sg_run_boot returned -11 ... after 1156 of 1268 records`, missing showBuyLandScreen (U-1251); S2.13 fixed U-1254 (the record-771 cursor tile) |
| test_terrain_live | 397 passed | 144 | S3.2 (incl. test_x64_equals_win32_terrain_pixels) |
| test_gfx_boot_screens | 32 passed | new file | S2.7 |
| test_gfx_reports | 36 passed | new file | S2.8 |
| test_gfx_dialogs | 32 passed | new file | S2.9 |
| test_defects_gen_w2 | 30 passed | new file | S6.8 |
| test_defects_golfers_w2 | 45 passed | new file | S6.7 |
| test_s2_6_scenario | 4 passed | 3 + 2 xfailed | S2.13 replaced the U-1253 xfails |
| test_vectors | 298 passed, 470 skipped | 298 / 454 | +16 skips, `no vector file` for the new registry names (gfx families boot_screens / reports / dialogs / terrain and their entries) |
| test_win32 | 179 passed, 233 skipped | 179 / 225 | +8 skips, the same names |
| every other file of I5's table | I5's count | | test_tick 250, test_menu 23 / 44 skipped, test_profiles 19, test_static 6, test_gfx 25, test_gfx_credits 47, test_gfx_routing 35, test_gfx_eoi 5, test_gl_raster 177, test_terrain_render 522, test_terrain_state 500, test_terrain_gl_setup 6, test_commands 83 / 44, test_defects_golfers 25, test_defects_gen 13 |

## 3. Port suite, Win32 backend

```
SG_BACKEND=win32 STATIC=0 OUT=log/roadmap/merges/I6/suite_win32 FILES="$(cat log/roadmap/merges/I6/win32_files.txt)" sh log/roadmap/merges/I6/suite_by_file_I6b.sh
```
52 files (I5's 46 + test_gfx_boot_screens, test_gfx_reports, test_gfx_dialogs, test_defects_gen_w2, test_defects_golfers_w2,
test_terrain_live): **44 PASS** (boot_screens 29 + 3 skipped, reports 36, dialogs 32, defects_gen_w2 30, defects_golfers_w2 45, terrain_live
394 + 3 skipped (no runner op for sg_ts_negctl), s2_6_scenario 4). **The same 8 files fail the same 20 tests** (browser 1, course_menu 1,
save_dialogs 1, saveload 6, holestats 2, integrator 4, semantic_input 4, tick 1; `win32_compare.log`). One extra failure in the first pass,
test_profiles::test_original_profile_segments_equal_no_profile[course_season/00] (its first of two runs of the same segment returned FAIL
with 0 frames run while sibling sessions ran heavy tests), passed on the re-run (19 / 19, `suite_win32/test_profiles.rerun.txt`); the
Win32 segment gate has course_season/00 PASS.

## 4. Segment gate

```
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --jobs 4 --out log/roadmap/merges/I6/segs_x64.tsv
~/bin/py -3.12 -m port.test.integrator segments --backend win32 --dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe --jobs 4 --out log/roadmap/merges/I6/segs_win32.tsv
~/bin/py -3.12 -m port.test.integrator compare <a> <b>
```
x64 **205 / 205 PASS** (117.8 s), Win32 **205 / 205 PASS** (173.3 s); 0 of 205 rows differ against I5 on either backend or between the
backends (`segs_compare.log`).

## 5. Boot gate

```
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out log/roadmap/merges/I6/boot_x64.tsv
SG_BACKEND=win32 ~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out log/roadmap/merges/I6/boot_win32.tsv
~/bin/py -3.12 log/roadmap/merges/I6/boot_compare.py <tsv> [<other tsv>]   # against S2S3-5/boot_x64.tsv
```
70 rows on each backend (`{"PASS": 65, "KNOWN": 2, "NO_FRAMES": 2, "FAIL": 1}`), 0 rows differ from I5 in status / records / expected.
HEAD's 61 at **58 PASS, 2 KNOWN (cov_a_golfer, cov_b_holes18, U-1062), 1 NO_FRAMES (cov_b_location)**. cov_r_s2_screens: FAIL 1,156 / 1,268,
now with no divergent record (I5: g_cursorTileX / Y at record 771, U-1253 / U-1254, fixed by S2.13), stopping at the unported
showBuyLandScreen (U-1251). x64 and Win32 rows identical.

## 6. Section gates

S6 wave 2: profiles validate 8 of 14; test_profiles 19, test_defects_gen_w2 30, test_defects_gen 13, test_terrain_gen 14 (101 opt-in skipped),
test_golfers 60, test_defects_golfers_w2 45, test_defects_golfers 25, test_core 54, test_tick 250 (S6's long-gate log ended green:
`log/roadmap/S6/lead_gate_wave2_long.log`). S3 wave 3: test_terrain_live 397 x64 / 394 + 3 skipped Win32, test_gl_raster 177, test_terrain_state
500, test_terrain_gl_setup 6, gfx tests, port static 6; terrain verdict 527 PASS (section 8). S2 wave 5: check_on_commit.py HEAD rc 0, `equal
to the mirror: yes` (`s2_check_on_commit.log`, run with core.autocrlf=false: with the user's global autocrlf=true the scratch repository writes
CRLF and the byte comparison reads NO although every file is equal up to line ends); test_gfx 25, credits 47, routing 35, boot_screens 32,
reports 36, dialogs 32, commands 83 / 44 skipped, tick 250, test_reports 14, s2_5 3, s2_6 4 x64; Win32 gfx files pass; gfx.check 0 duplicates.

## 7. Switches, chains and the ledger

```
~/bin/py -3.12 -m port.test.integrator chain --backend x64 --all --jobs 4 --out i6/ledger/chain_x64.tsv
~/bin/py -3.12 -m port.test.integrator defects --backend x64 --jobs 4 --out i6/ledger/defects_x64.tsv
~/bin/py -3.12 -m port.test.integrator ledger --segments i6/ledger/segs_x64.tsv --chain i6/ledger/chain_x64.tsv --defects i6/ledger/defects_x64.tsv --out i6/ledger/divergences.tsv
```
(inputs inside the mirror root, `i6/ledger/`; copies in `log/roadmap/merges/I6/`.) Chains **342 / 342 PASS**, every row equal to I5's
(`chain_compare.log`, 715 s). Defects: 14 switches, the 12 old ones change the same segments as at I5, the 2 new ones none
(`defects_compare.log`). Ledger: **28 rows, 1 without a cause** (showRoutingMapScreen.exitRemapReadsFrame); re/port/divergences.tsv is this ledger.

## 8. Frame census

```
~/bin/py -3.12 -m port.test.framegate --all --jobs 2 --out log/roadmap/merges/I6/census.tsv --resume
~/bin/py -3.12 i6/roadmap/S0/3_rank.py log/roadmap/merges/I6/census.tsv log/roadmap/merges/I6/census_out   # S0's 3_rank.py copied into the mirror
~/bin/py -3.12 log/roadmap/merges/I6/census_compare.py log/roadmap/merges/I6/census_out/frame_census.tsv - log/roadmap/merges/I6/census.tsv
~/bin/py -3.12 log/roadmap/S3/2_census.py terrain --jobs 3 --out log/roadmap/merges/I6/terrain_verdicts.tsv          # from the mirror root
~/bin/py -3.12 log/roadmap/S3/2_census.py rank log/roadmap/merges/I6/census.tsv log/roadmap/merges/I6/terrain_verdicts.tsv --out log/roadmap/merges/I6/census_ranked_s3rule.tsv
```
re/roadmap/frame_census.tsv and frame_causes.tsv are this census (698 rows, 70 scenarios). Status: **PASS 87** (I5 82, S0 45), UNSUPPORTED 598,
NOT_RUN 13, FAIL 0. Over I5's rows: PASS 82 -> 87 (FAIL -> PASS 4: cov_r_s2_screens loading_t200, loading_t300, theme_packs, difficulty;
UNSUPPORTED -> PASS 1: pick_a_pro), NOT_RUN -> UNSUPPORTED 7 (cov_r_s2_screens checkpoints past record 771 now run: cancel_prompt, editor,
end_of_year, practice, pre_land, skill_points, skill_popup), 0 PASS rows lost. Over S0's 609 rows: PASS 73, 0 lost. NOT_RUN 13 = golfer_events
10 (no trace) + cov_r_s2_screens buy_land, end, land_popup (after the boot's stop at record 1,156). `terrain_drawn` (S3.2's column): 598
terrain-yes rows 1, 84 terrain-no rows 0; 2 terrain-no rows 1 (U-1315), 2 terrain-yes rows 0. First unsupported drawing call (2d_all):
Widget_writePixel 453, Surface_stretchBlit `a SpriteRef the backend has no sprite for` 132, Surface_drawScaled2 13. The S0 ranking's terrain
verdict (G2 on the whole view region, which also holds sprites / HUD / overlays) reads FAIL 591, PASS 8, so its primary cause stays `terrain not
drawn` 591; S3.2's own verdict (the port's root surface at the last updateTerrainRender before each checkpoint against the original's, G2 over
the pixels the original drew) is **527 PASS, 0 FAIL** and 75 NOT_MEASURED (cov_r_bio, cov_r_flic_input, cov_r_palette_hot, cov_r_s2_screens,
cov_r_theme_cancel, cov_r_tutorial: no terrain recording); ranked by S3's rule (`census_ranked_s3rule.tsv`, MERGE_w3 1.5 proposes it for S3.9):
primary causes Widget_writePixel 443, terrain not measured 75, Surface_stretchBlit 70, Surface_drawScaled2 13, no trace 10.

## What a later run must hold

- repo static 614 passed / 2 skipped; every file of section 2 PASS with its count, test_boot 94 passed with cov_r_s2_screens its one failure
  (stop at record 1,156, no divergent record) until showBuyLandScreen is ported (U-1251);
- segments 205 / 205 PASS on both backends with 0 digests differing from `log/roadmap/merges/I6/segs_x64.tsv` and `segs_win32.tsv`
  (identical to I5's, I4's, I1's and S0's);
- boot gate: the 70 rows of `log/roadmap/merges/I6/boot_x64.tsv` (HEAD's 61 at 58 / 2 / 1), on x64 and Win32;
- the 8 Win32 files of section 3 keep their 20 failures until U-1100 / U-1101 are fixed (a different count is a change);
- chains 342 / 342; defects as section 7; profiles fixed 8 of 14; the ledger over fresh inputs has 1 row without a cause;
- the census: no PASS row of re/roadmap/frame_census.tsv lost; S3.2's terrain verdict 527 PASS.
