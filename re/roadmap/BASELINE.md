# Regression baseline at HEAD c6d7d78b + S1 + S8 (I4, 2026-10-24)

This file now describes **HEAD + S1 + S8**: commit `c6d7d78b86a66ec60335f81e14391f071a540d8f` (which holds S6 and S0) plus the files of
`log/roadmap/S1/files.txt` and `log/roadmap/S8/files.txt` and the shared-file edits of their merge (`log/roadmap/merges/S1S8-4.md`). Measured by
integrator I4 in the mirror `port/build/I4_S1S8`: git archive of HEAD, the junctions of `log/roadmap/S0/export.md` plus `shim/build`,
`re/replay` and `extracted` (tests/test_movie_inventory.py reads the install media there), every S1 / S8 file copied from the shared tree, the
merge edits applied by `log/roadmap/merges/S1S8-4/apply_shared.py` and `gate_edits.py`. All commands run from the mirror root. Builds in the
mirror: `port/build/x64` and `port/build/Win32` (logs `log/roadmap/merges/S1S8-4/build/`). Python `~/bin/py -3.12`. Heavy runs one at a
time, free commit charge checked before each (23-38 GB). Raw logs in `log/roadmap/merges/S1S8-4/`.

The previous baselines (HEAD alone by S0.1, HEAD + S6 by I1) are in git history; their numbers are the last column below and their logs stay in
`log/roadmap/S0/` and `log/roadmap/merges/S6-1/`.

## Summary

| Gate | HEAD + S1 + S8 | HEAD + S6 (I1) | Same? |
|---|---|---|---|
| repo static (`-m static`) | 614 passed, 2 skipped, 46 deselected | 600 / 2 | +14 S1 / S8 static tests, all pass |
| port suite x64, one file per process | 52 files, all PASS; test_boot 92 passed | 44 files; test_boot 88 + 2 failed S1 scenarios | yes for HEAD's files; +7 S1 / S8 files |
| port suite Win32 backend | 41 files: 33 PASS, the same 8 files fail the same 20 tests (U-1100, U-1101) | 27 PASS, 8 fail 20 | yes (identical failing test names) |
| segment gate x64 | 205 / 205 PASS, 0 digests differ from `log/roadmap/merges/S6-1/segs_x64.tsv` | 205 / 205 | yes |
| segment gate Win32 | 205 / 205 PASS, 0 digests differ from `S6-1/segs_win32.tsv` and from the x64 run | 205 / 205 | yes |
| boot gate `--all` x64 | 67 rows: HEAD's 61 at 58 PASS, 2 KNOWN, 1 NO_FRAMES (0 rows differ); S1's 5 and S8's 1 PASS | 61 at 58 / 2 / 1; S1 rows 4 PASS + 2 FAIL | yes for HEAD's 61; section rows now all PASS |
| boot gate `--all` Win32 | the same 67 rows, 0 differ from x64 | (not run by I1) | new record |
| chains `integrator chain --all` | 342 / 342 PASS, every row equal to I1's | 342 / 342 | yes |
| compatibility switches `integrator defects` | 12 switches; sinScaled 49, prepareShot.hazardCellTransposed 13, the other 10 0 | same | yes |
| ledger (`integrator ledger`, fresh inputs; written to re/port/divergences.tsv) | 25 rows, 1 without a cause (showRoutingMapScreen.exitRemapReadsFrame) | 20 rows, 2 without a cause | chooseShotTarget.strictFlagSticky now has evidence (U-1012 resolved); +5 S1 scenarios without segments |
| S1 / S8 gates | see section 6 | new | new |
| test_tick (includes the slow COMMAND gate) | 250 passed | 250 | yes |
| test_sve_roundtrip | 15 passed | 15 | yes |
| P5.3 gates | gl_raster 139, terrain_state 500, terrain_render 40, terrain_gl_setup 6, gfx 17, static 6, terrain_live 144 | same | yes |

## 1. Repo static suite

```
~/bin/py -3.12 -m pytest -m static -q -p no:cacheprovider
```
Result: **614 passed, 2 skipped, 46 deselected** (`log/roadmap/merges/S1S8-4/suite/static_rerun.txt`). The 14 new tests are
tests/test_trace_format_s1.py (7) and tests/test_movie_inventory.py (7). The first run in the mirror read 1 failed (test_movie_inventory: the
mirror had no `extracted` junction, so `install_media` read `no`); with the junction the file passes 7 / 7 and the rerun after the gate edits
read 614 / 2.

## 2. Port suite, x64, one file per process

```
sh log/roadmap/merges/S1S8-4/suite_by_file.sh   # a copy of log/roadmap/S0/1_suite_by_file.sh pointed at port/build/I4_S1S8
```
OUT `log/roadmap/merges/S1S8-4/suite/<file>.txt`, run log `suite_x64.log`, 52 files. Changes from HEAD + S6:

| File | HEAD + S1 + S8 | HEAD + S6 | Note |
|---|---|---|---|
| test_boot | 92 passed | 88 passed, 2 failed | +1 NEGCTL cov_r_theme_cancel (showThemePackSelect 2, RED), cov_r_bio and cov_r_flic_input now pass, +cov_r_s8_play |
| test_chareditor | 19 passed, 4 skipped | 19 / 4 | S1.5's version |
| test_chareditor_bio | 28 passed, 8 skipped | new file | S1.5 |
| test_s1_3_scenarios | 8 passed | new file | S1.3 |
| test_s1_4_scenarios | 9 passed | new file | S1.4; the two flic tests run by default now |
| test_shots_strict | 22 passed, 25 skipped | new file | S1.2 |
| test_sve_header_readers | 8 passed | new file | S1.1 |
| test_s8_2_scenarios | 14 passed | new file | S8.2 (hook test follows the widened glob, merge gate edit 1) |
| test_video | 32 passed, 24 skipped | new file | S8.3 (detail text follows item 4, merge gate edit 2) |
| test_vectors | 298 passed, 436 skipped | 298 / 398 | +38 skips, all `no vector file`: the 19 names S1.5 and S8 register |
| test_win32 | 179 passed, 216 skipped | 179 / 197 | +19 skipped, the same names |
| every other file of I1's table | I1's count | | |

## 3. Port suite, Win32 backend

```
SG_BACKEND=win32 STATIC=0 OUT=log/roadmap/merges/S1S8-4/suite_win32 FILES="<I1's list + test_chareditor_bio test_s1_3_scenarios test_s1_4_scenarios test_shots_strict test_video>" sh log/roadmap/merges/S1S8-4/suite_by_file.sh
~/bin/py -3.12 log/roadmap/merges/S1S8-4/win32_compare.py
```
41 files: **33 PASS** (I1's 27 plus framegate, chareditor_bio 28, s1_3 8, s1_4 9, shots_strict 22, video 32). **The same 8 files fail the same 20
tests** (browser 1, course_menu 1, save_dialogs 1, saveload 6, holestats 2, integrator 4, semantic_input 4, tick 1): sorted `FAILED` lines identical
to `log/roadmap/merges/S6-1/suite_win32/` (`win32_compare.log`).

## 4. Segment gate

```
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --jobs 8 --out log/roadmap/merges/S1S8-4/segs_x64.tsv
~/bin/py -3.12 -m port.test.integrator segments --backend win32 --dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe --jobs 8 --out log/roadmap/merges/S1S8-4/segs_win32.tsv
~/bin/py -3.12 -m port.test.integrator compare <a> <b>     # S6-1 x64 vs new x64, S6-1 win32 vs new win32, new x64 vs new win32
```
x64 **205 / 205 PASS** (66.7 s), Win32 **205 / 205 PASS** (89.8 s); 0 of 205 rows differ in all three comparisons (`segs_compare.log`).

## 5. Boot gate

```
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out log/roadmap/merges/S1S8-4/boot_x64.tsv
SG_BACKEND=win32 ~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out log/roadmap/merges/S1S8-4/boot_win32.tsv
~/bin/py -3.12 log/roadmap/merges/S1S8-4/boot_compare.py <tsv>   # against S6-1/boot_gate.tsv over log/p5/boot_gate_loc.tsv + the 6 section scenarios
```
67 rows on each backend (`{"PASS": 64, "KNOWN": 2, "NO_FRAMES": 1}`). Over HEAD's 61: **58 PASS, 2 KNOWN (cov_a_golfer, cov_b_holes18, U-1062),
1 NO_FRAMES (cov_b_location)**, 0 rows differ from I1. Section scenarios, all PASS with every record: cov_r_bio 687, cov_r_flic_input 75,
cov_r_tutorial 1,195, cov_r_theme_cancel 123, cov_r_palette_hot 565 (S1), cov_r_s8_play 216 (S8). x64 and Win32 rows identical.

## 6. S1 / S8 gates and the other gates

From the x64 suite: test_tick 250, test_sve_roundtrip 15, test_gl_raster 139, test_terrain_state 500, test_terrain_render 40,
test_terrain_gl_setup 6, test_gfx 17, test_static (port) 6, test_terrain_live 144, test_commands 83 / 44. Section static files:
tests/test_trace_format_s1.py 7, tests/test_trace_format.py 15, tests/test_movie_inventory.py 7, port/test/test_s8_2_scenarios.py 14
(`section_static.log`). Opt-in oracles (`SG_EMU_ORACLE=1`, `log/roadmap/merges/S1S8-4/oracle/`): test_shots_strict 46 passed (1
regeneration-only skip), test_chareditor_bio 36, test_chareditor 23, test_video 56 on x64 and 56 on Win32.

## 7. Switches, chains and the ledger

```
~/bin/py -3.12 -m port.test.integrator chain --backend x64 --all --jobs 6 --out port/build/I4_ledger/chain_x64.tsv
~/bin/py -3.12 -m port.test.integrator defects --backend x64 --jobs 6 --out port/build/I4_ledger/defects_x64.tsv
~/bin/py -3.12 -m port.test.integrator ledger --segments <segs_x64.tsv> --chain <chain_x64_merged.tsv> --defects <defects_x64.tsv> --out re/port/divergences.tsv
```
(The ledger refuses inputs outside ROOT and the mirror's `log` is a junction, so its inputs sit in `port/build/I4_ledger/` inside the mirror;
copies of every input and output are in `log/roadmap/merges/S1S8-4/`.) Chains **342 / 342 PASS**, every row equal to I1's; one worker
(course_season_6y@season_3000) returned an empty exception under load and was re-run with its scenario's other 9 chains, all PASS. Defects: the
12 switches change the same segments as at I1. Ledger: **25 rows, 1 without a cause** (showRoutingMapScreen.exitRemapReadsFrame, HEAD's
switch from 38g that no test is named for); chooseShotTarget.strictFlagSticky reads `evidence` (port/test/test_shots_strict.py); the 10
scenarios without segments are the 5 earlier ones plus S1's five (U-1172, U-1193). re/port/divergences.tsv is this ledger.

## What a later run must hold

- repo static 614 passed / 2 skipped; every file of section 2 PASS with its count;
- segments 205 / 205 PASS on both backends with 0 digests differing from `log/roadmap/merges/S1S8-4/segs_x64.tsv` and `segs_win32.tsv`
  (identical to I1's and S0's);
- boot gate: HEAD's 61 scenarios 58 / 2 / 1, and cov_r_bio, cov_r_flic_input, cov_r_tutorial, cov_r_theme_cancel, cov_r_palette_hot,
  cov_r_s8_play PASS with all records, on x64 and Win32;
- the 8 Win32 files of section 3 keep their 20 failures until U-1100 / U-1101 are fixed (a different count is a change);
- chains 342 / 342; defects as section 7; the ledger over fresh inputs has 1 row without a cause.
