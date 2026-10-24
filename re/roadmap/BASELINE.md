# Regression baseline at HEAD 4bfefc2a + S6 (I1, 2026-10-24)

This file now describes **HEAD + S6**: commit `4bfefc2a4efcd5ba712c4d676dbb4be46a4e34c6` plus section S6's files
(`log/roadmap/S6/files.txt`) and the shared-file edits of its merge (`log/roadmap/merges/S6-1.md`). Measured by integrator I1 in the mirror
`port/build/I1_S6`: git archive of HEAD, the junctions of `log/roadmap/S0/export.md` plus `shim/build` and `re/replay` (S0.1's two extra
junctions), every S6 file copied from the shared tree, the merge edits applied by `log/roadmap/merges/S6-1/apply_shared.py`. All commands
run from the mirror root, so Python's ROOT is the mirror and no S1 / S4 working-tree edit is in the code under test. Builds in the mirror:
`port/build/x64` and `port/build/Win32` (logs `log/roadmap/merges/S6-1/build/`). Python `~/bin/py -3.12`. Heavy runs strictly one at a time,
free commit charge checked before each (30-40 GB). Raw logs in `log/roadmap/merges/S6-1/` (paths relative to the shared repo root; the
mirror's `log` is a junction to it).

The previous baseline (HEAD alone, S0.1) is kept in git history; its numbers are the "HEAD (S0)" column below and its logs stay in
`log/roadmap/S0/`.

## Summary

| Gate | HEAD + S6 | HEAD (S0) | Same? |
|---|---|---|---|
| repo static (`-m static`) | 600 passed, 2 skipped, 46 deselected | 600 / 2 | yes |
| port suite x64, one file per process | 44 files: 43 PASS; test_boot 88 passed + 2 failed S1 scenarios (section 2) | 41 / 41 PASS | yes for HEAD's files; +3 S6 files (57 tests) PASS |
| port suite Win32 backend, portlib files | 35 HEAD + S6 files: 27 PASS, 8 fail the same 20 tests (U-1100, U-1101) | 25 PASS, 8 fail 20 | yes (identical failing test names) |
| segment gate x64 | 205 / 205 PASS, 0 digests differ from `log/roadmap/S0/1_segs_x64.tsv` | 205 / 205 | yes |
| segment gate Win32 | 205 / 205 PASS, 0 digests differ from `log/roadmap/S0/1_segs_win32.tsv` and from the x64 run | 205 / 205 | yes |
| boot gate `--all` | 61 HEAD scenarios: 58 PASS, 2 KNOWN, 1 NO_FRAMES, 0 rows differ (6 extra rows are S1's, U-1102) | 58 / 2 / 1 | yes |
| chains `integrator chain --all` | 342 / 342 PASS, no break | (not run by S0; 335 / 335 at 38j) | new record |
| compatibility switches `integrator defects` | 12 switches; segments changed: sinScaled 49, prepareShot.hazardCellTransposed 13, the other 10 0 | (not run by S0) | new record |
| ledger (`integrator ledger`, fresh inputs) | 20 rows, 2 without a cause (chooseShotTarget.strictFlagSticky U-1012, showRoutingMapScreen.exitRemapReadsFrame) | 1 without a cause in the committed `re/port/divergences.tsv` (before the routing switch existed) | see section 7 |
| S6 gate | test_profiles 19, test_defects_golfers 25, test_defects_gen 13 (x64 and Win32), `profiles validate` 6 members | new | new |
| test_tick (includes the slow COMMAND gate) | 250 passed | 250 | yes |
| test_sve_roundtrip | 15 passed | 15 | yes |
| P5.3 gates | gl_raster 139, terrain_state 500, terrain_render 40, terrain_gl_setup 6, gfx 17, static 6, terrain_live 144 | same | yes |

## 1. Repo static suite

```
~/bin/py -3.12 -m pytest -m static -q -p no:cacheprovider
```
Result: **600 passed, 2 skipped, 46 deselected in 471.36 s** (`log/roadmap/merges/S6-1/static_rerun.txt`). Same two skips as S0
(`tests/test_patches.py:33`, `tests/test_failure_tools.py:55`). The first run in the mirror (`suite/static.txt`) read 1 failed:
`tests/test_classify.py::test_real_tracker_state` raised ValueError in `scripts/re_classify.py` norm_addr, because three of S6's U-rows
(U-1595..U-1597) carried a function name in UNCERTAINTIES.md's address column. The merge moved those names into the description (address
cell empty) and the rerun passed; see `log/roadmap/merges/S6-1.md`.

## 2. Port suite, x64, one file per process

```
sh log/roadmap/merges/S6-1/suite_by_file.sh   # a copy of log/roadmap/S0/1_suite_by_file.sh pointed at port/build/I1_S6
```
OUT `log/roadmap/merges/S6-1/suite/<file>.txt`, run log `log/roadmap/merges/S6-1/suite_x64.log`, 44 files. Every HEAD file has S0's count
except test_boot:

| File | HEAD + S6 | HEAD (S0) | Note |
|---|---|---|---|
| test_boot | 88 passed, 2 failed (978 s) | 88 passed | the 2 failures are S1 recordings in the shared corpus, not HEAD scenarios (U-1102): cov_r_bio (`sg_run_boot returned -11` after 579 of 687 records) and cov_r_flic_input (record 1, g_cachedCameraTileX). Both fail identically on the HEAD export (`log/roadmap/merges/S6-1/boot_s1_at_head.log`) |
| test_defects_gen | 13 passed (1 s) | new file | S6 |
| test_defects_golfers | 25 passed (339 s) | new file | S6 |
| test_profiles | 19 passed (15 s) | new file | S6 |
| test_tick | 250 passed (715 s) | 250 (478 s) | time only |
| test_win32 | 179 passed, 197 skipped (533 s) | same (360 s) | time only |
| every other file of S0's table | S0's count | | bridge 25, browser 9/26, chareditor 19/4, commands 83/44, core 54, course 69/1, course_menu 7/24, dialogs 27/7, economy 19, events 50, gditext 22/1, gfx 17, gl_raster 139, golfers 60, holestats 10/3, integrator 15, loaders 14/50, membership 6/1, menu 23/44, reports 14/2, routing 11/3, save_dialogs 6/26, saveload 26/37, segment_files 3, segments 210, semantic_input 11, shortcuts 5/1, shots 33/1, shotstyle 4/1, static 6, sve_roundtrip 15, terrain 75/62, terrain_gen 14/101, terrain_gl_setup 6, terrain_live 144, terrain_render 40, terrain_state 500, vectors 298/398 |

The suite ran before the evidence-map edit of `port/test/integrator.py` (section 7); test_integrator was re-run after it: 15 passed
(`test_integrator_after_evidence.txt`).

## 3. Port suite, Win32 backend

```
SG_BACKEND=win32 STATIC=0 OUT=log/roadmap/merges/S6-1/suite_win32 FILES="<S0's list + test_defects_gen test_defects_golfers test_profiles>" sh log/roadmap/merges/S6-1/suite_by_file.sh
```
Run log `log/roadmap/merges/S6-1/suite_win32.log` (36 entries). test_framegate is S0.2's file, not HEAD's, and is absent from the mirror (its
row reads `no tests ran`). **27 files PASS with the x64 counts** (S0's 25 minus framegate, plus defects_gen 13, defects_golfers 25,
profiles 19). **The same 8 files fail the same 20 tests as in S0's table** (browser 1, course_menu 1, save_dialogs 1, saveload 6, holestats 2,
integrator 4, semantic_input 4, tick 1): the sorted `FAILED` lines of all per-file outputs are identical to `log/roadmap/S0/1_suite_win32/`.

## 4. Segment gate

```
~/bin/py -3.12 -m port.test.integrator segments --backend x64   --jobs 8 --out log/roadmap/merges/S6-1/segs_x64.tsv
~/bin/py -3.12 -m port.test.integrator segments --backend win32 --jobs 8 --out log/roadmap/merges/S6-1/segs_win32.tsv
~/bin/py -3.12 -m port.test.integrator compare log/roadmap/S0/1_segs_x64.tsv   log/roadmap/merges/S6-1/segs_x64.tsv    # 205 rows in both, 0 differ
~/bin/py -3.12 -m port.test.integrator compare log/roadmap/S0/1_segs_win32.tsv log/roadmap/merges/S6-1/segs_win32.tsv  # 205 rows in both, 0 differ
~/bin/py -3.12 -m port.test.integrator compare log/roadmap/merges/S6-1/segs_x64.tsv log/roadmap/merges/S6-1/segs_win32.tsv  # 205 rows in both, 0 differ
```
x64 **205 / 205 PASS** (wall 65.3 s), Win32 **205 / 205 PASS** (92.5 s); 0 digests changed (`segs_compare.log`).

## 5. Boot gate

```
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out log/roadmap/merges/S6-1/boot_gate.tsv
~/bin/py -3.12 log/roadmap/merges/S6-1/boot_compare.py   # against log/roadmap/S0/1_boot_gate.tsv, over the 61 scenarios of log/p5/boot_gate_loc.tsv
```
67 scenarios ran (`{"PASS": 62, "KNOWN": 2, "NO_FRAMES": 1, "FAIL": 2}`). Over HEAD's 61: **58 PASS, 2 KNOWN (cov_a_golfer, cov_b_holes18,
U-1062), 1 NO_FRAMES (cov_b_location)**, 0 rows differ from S0 in status, record count or expected count (`boot_compare.log`). The 6 others are
S1's (U-1102): cov_r_palette_hot, cov_r_s8_play, cov_r_theme_cancel, cov_r_tutorial PASS; cov_r_bio and cov_r_flic_input FAIL, the same on
the HEAD export.

## 6. Other gates

All x64, one file per process, from the suite run of section 2: test_tick 250 (715 s), test_sve_roundtrip 15 (31 s), test_gl_raster 139 (52 s),
test_terrain_state 500 (268 s), test_terrain_render 40 (47 s), test_terrain_gl_setup 6 (50 s), test_gfx 17 (35 s), test_static (port) 6 (1 s),
test_terrain_live 144 (113 s). S6's own gate: test_golfers 60, test_events 50, test_terrain_gen 14 / 101 skipped (the `original` profile leaves
them unchanged), `~/bin/py -3.12 -m port.test.profiles validate`: `fixed` = 6 members of 12 switches, no error
(`profiles_validate.log`). The optional C ABI `sg_profile_set` / `sg_profile_get` was checked with `log/roadmap/merges/S6-1/abi_check.py`
(12 checks, including unknown switch / profile refused with nothing changed): all OK (`abi_check.log`, x64).

## 7. Switches, chains and the ledger

```
~/bin/py -3.12 -m port.test.integrator defects --backend x64 --jobs 8 --out log/roadmap/merges/S6-1/defects_x64.tsv
~/bin/py -3.12 -m port.test.integrator chain --backend x64 --all --jobs 6 --out port/build/I1_ledger/chain_x64_fresh.tsv
~/bin/py -3.12 -m port.test.integrator ledger --segments <segs_x64.tsv> --chain <chain_x64_fresh.tsv> --defects <defects_x64.tsv> --out <ledger>
```
(The ledger resolves its inputs and refuses one outside ROOT, and the mirror's `log` is a junction, so its inputs were copied to
`port/build/I1_ledger/` inside the mirror; copies of every input and output are in `log/roadmap/merges/S6-1/`.)

Defects (baseline 205 PASS): sinScaled.readPastTable 49 segments change, prepareShot.hazardCellTransposed 13, the other ten 0, among them S6's
six (placeHabitat.clusterLoopBound, spawnObject.cellSearchBound, loadGolfPro.divideAndRejectGuard, placeAnimal.zeroFrameCount,
replaceInText.tailLimit, generateRandomCourse.searchBound), as their class requires (no effect on a run the original completes). Before the
evidence-map edit the ledger listed those six as `no evidence`; with S6's state tests named in `port/test/integrator.py` they read `evidence`.
Chains: **342 / 342 PASS, no break** (wall 474 s). The stale `log/p38/38f/chain_x64.tsv` (2026-10-20 03:36, before 38j) still holds 7 FAIL
chains whose cause the ledger used to take from segment rows that now PASS: with it the ledger shows 9 rows without a cause; with fresh
chains **20 rows, 2 without a cause** (chooseShotTarget.strictFlagSticky, U-1012; showRoutingMapScreen.exitRemapReadsFrame, a HEAD switch
from 38g that no test is named for). `re/port/divergences.tsv` was not rewritten (MERGE.md item 2).

## What a later run must hold

- repo static 600 passed / 2 skipped; every file of section 2 PASS with its count (test_boot: HEAD's 88 parametrizations pass; S1's
  recordings are judged by S1);
- segments 205 / 205 PASS on both backends with 0 digests differing from `log/roadmap/merges/S6-1/segs_x64.tsv` and `segs_win32.tsv`
  (identical to S0's);
- boot gate: the 61 HEAD scenarios 58 / 2 / 1;
- the 8 Win32 files of section 3 keep their 20 failures until U-1100 / U-1101 are fixed (a different count is a change);
- chains 342 / 342; defects: S6's six switches change 0 segments; the ledger over fresh inputs has 2 rows without a cause.
