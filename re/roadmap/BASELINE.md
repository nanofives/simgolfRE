# Regression baseline at HEAD 10e9d507 + S4 (I14, 2026-10-29)

This file now describes **HEAD + S4**: the S4 audio engine (sound.dll's engine ported behind an AudioSink, `log/roadmap/S4/MERGE.md`,
files `log/roadmap/S4/files.txt`, shared edits `2b_apply_shared.py`) on 6e6df6e7, whose port/ equals e179bd24's (= I13's state below).
Measured by I14 in the mirror `port/build/I14` (`log/roadmap/merges/I14.md`, raw logs in `log/roadmap/merges/I14/`). Suites by decision
G11: the mirror equals the S4 lead's gated tree port/build/S4r2 byte for byte, so the suite numbers are the lead's.

| Gate | HEAD + S4 (I14) |
|---|---|
| repo static | 630 passed, 2 skipped (I13 614 / 2; +16 S4 static tests) |
| port suite x64 | 95 files (I13's 82 + 13 audio): 94 PASS, test_fttext 1 or 2 of its U-1393 host GDI tests fail (lead 1, I14's partial re-run 2) |
| port suite Win32 | I13's 66-file list: 20 failed ids, all among I10's 22 known |
| test_app_* x64 | 6 / 6 PASS |
| segments x64 / Win32 / freetype | 205 / 205 each |
| boot x64 / Win32 / freetype | 70 rows 66 / 2 KNOWN / 2 NO_FRAMES each |
| chains / ledger | 342 / 342; ledger 29 rows, 1 without a cause |
| frame census / terrain | 698 rows PASS 100, FAIL 537, UNSUPPORTED 51, NOT_RUN 10 / 527 PASS, 75 NOT_MEASURED |
| S7 live equivalence / app replay x64 + Win32 | 68 PASS + 2 NO_FRAMES; hole 340, report 205, saveload 266 |
| S4 own gate | oracle 104 / 104 GREEN 0 fallbacks, exe 3 / 3, streams 205 / 205 (4,964 requests), device 0 fallbacks, goldens 183 / 183 equal, replay 0 fails both backends, controls all RED |

Reference TSVs for later runs: `log/roadmap/merges/I14/{segs,boot,equiv}_{x64,win32}.tsv`, `census.tsv`, `terrain_verdicts.tsv`, `chain_x64.tsv`.

## Before S4 (I13, HEAD d66aa755 + S3, 2026-10-28)


This file now describes **HEAD d66aa755 + S3**: commit `d66aa755` (= 919dbdd5 + S7 waves 1-3, the I10 state below) plus S3's
resubmission c, merged as one request (`log/roadmap/S3/14_merge.md` + `14_files.txt`: MERGE_w5b, i.e. S3 waves 2 / 4 / 5 with S3.5,
S3.7, S3.8, S3.11, S3.13, plus S3.14's seven edits), applied with `log/roadmap/S3/14_assemble.py --root .` and the UNCERTAINTIES rows
U-1393..U-1398 (`log/roadmap/merges/I13.md`). Measured by I13 in the mirror `port/build/I13` (git archive d66aa755 + I6's junctions +
`14_assemble.py --dst I13 --commit d66aa755` + `log/roadmap/merges/I13/unc_rows.py`; builds with SG_FREETYPE_ROOT). Raw logs in
`log/roadmap/merges/I13/`. Every row not named in the table below is equal to I10's. Pass `-m ""` to pytest for these counts.

| Gate | HEAD d66aa755 + S3 (I13) |
|---|---|
| repo static | 614 passed, 2 skipped, 46 deselected |
| test_app_* x64, one file per process | 6 / 6: ingame 12 + 1 xfail, input 16, live 13, present 7, replay 14, shell 18 (= I10); the app starts at Normal class (decision G10, `SG_APP_PRIORITY=inherit` = the old class) |
| port suite x64 | 82 files (I10's 75 + test_flic, test_defects_flic, test_gfx_hud, test_gfx_pixel, test_gfx_scene, test_gfx_text, test_terrain_overlay): 81 PASS, test_fttext 2 failed (U-1393, host); count changes against I10 are S3's own (test_gfx_s2_10 13, test_gl_raster 177, test_menu 24 + 47 skipped, test_terrain_live 398, test_vectors 298 + 538 skipped, test_win32 179 + 267 skipped); per-file counts in `log/roadmap/merges/I13/suite_compare_x64.log` |
| port suite Win32 | 66 files (I7's 59 + the 7 S3 files above): the 22 known failing ids of `log/roadmap/merges/I10/win32_failed_ids.txt` (BASELINE's 20 + test_fttext's 2) and nothing else; all 7 S3 files PASS; test_live 39 (alone: in the suite run the background shell's 2-hour limit killed test_equivalence_all's workers) |
| segments x64 / Win32 | 205 / 205 each, 0 rows (digests included) differ from I10's or between backends |
| boot x64 / Win32 | 70 rows 66 / 2 KNOWN / 2 NO_FRAMES each, 0 rows differ from I10's (HEAD's 61 at 58 / 2 / 1) |
| S7 live equivalence / app replay x64 + Win32 | 68 PASS + 2 NO_FRAMES; hole 340, report 205, saveload 266; 0 rows differ from I10's |
| gfx.check | 0 duplicates, 131 bodies + 10 not_drawing |
| frame census | 698 rows: **PASS 100**, FAIL 537, UNSUPPORTED 51, NOT_RUN 10; 0 of I10's 88 PASS rows lost, 12 gained, the 600 UNSUPPORTED rows now drawn and judged (537 FAIL with fewer differing px, 51 still UNSUPPORTED) |
| S3.2 terrain verdict | 527 PASS, 75 NOT_MEASURED (= I10) |
| chains / defects / ledger, freetype segments / boot | not re-run by I13 (S3.14's edits cannot reach them); I12 on MERGE_w5b at d66aa755: chains 342 / 342, 15 switches (I10's 14 + flic.treeAliasNames, 0 segments changed), ledger 1 row without a cause, freetype 205 / 205 and 66 / 2 / 2 |

Reference TSVs for later runs: `log/roadmap/merges/I13/{segs,boot,equiv,app_replay}_{x64,win32}.tsv`, `census.tsv`, `terrain_verdicts.tsv`.

## Before S3 (I10, HEAD 919dbdd5 + S7 waves 1-3, 2026-10-27)

That state described **HEAD + S7**: commit `919dbdd5` (= `78ffb06d`, the I7 state below, plus the user's test-tier commit: pytest.ini
addopts `-m "not slow and not ghidra"`, which touches no port file) plus S7 waves 1-3 merged as one request (`log/roadmap/S7/9_merge.md`,
S7.9: the live core `live`, sg_live_* C ABI and runner op 40, the Windows app port/app with freetype.dll beside it, the gfx refresh hook,
HeadlessTerrain release per context) and the shared-file edits of `9_apply.py` (`log/roadmap/merges/I10.md`). Measured by I10 in the mirror
`port/build/I10` (git archive 78ffb06d + the S0 junctions + shim/build, re/replay, extracted + `9_apply.py port/build/I10 --files`; builds
with SG_FREETYPE_ROOT); 9_apply.py on 919dbdd5 gives the same bytes except 919dbdd5's own four files (I10.md section 4). Every number below
that I10 re-measured is equal to the I7 / I7b one unless the I10 column or note says otherwise; the I7 text stays as the record of how the
78ffb06d numbers were taken. Raw I10 logs in `log/roadmap/merges/I10/`. Under 919dbdd5's pytest.ini a plain `pytest port/test/<file>`
deselects slow tests: pass `-m ""` for the counts below.

Host change found by I10 (not a code change): since the 2026-10-27 05:24 restart the GDI provider's FontRef metric words differ from the
stored originals for three fonts, so test_fttext's two GDI metric tests fail on x64 and Win32 in every mirror, the 78ffb06d control included
(I10.md section 3). Widths are unchanged.

## Before S7 (I7, 2026-10-26)

The state before S7 described **HEAD + S2 waves 6 / 7 + S5**: commit `bbc9072f` (which holds S0, S6 waves 1 / 2, S1, S8, S2 waves 1-5 and
S3 waves 1 / 3) plus S2 wave 6 (`log/roadmap/S2/MERGE_wave6.md` -> `10_merge.md`, S2.10: buy land, syncTiles, the character editor's and
skill dialog's drawing), S2 wave 7 (`MERGE_wave7.md` -> `11_merge.md`, S2.11: tournament results, bulletin board, SimFoto / message ball),
S5 wave 1 (`log/roadmap/S5/MERGE.md`: modules textlab / fttext / fttext_raster, the OPTIONAL FreeType CMake block, SG_TEXT_METRICS=gdi|
freetype|none in the harness) and the shared-file edits of their merge (`log/roadmap/merges/I7.md`). **S7 waves 1 / 2 (the live core and
the Windows app) are not in it**: integrator I7 refused them (I7.md). Measured by I7 in the mirror `port/build/I7b`: git archive of HEAD,
the junctions of `log/roadmap/S0/export.md` plus `shim/build`, `re/replay` and `extracted`, the section files copied by
`log/roadmap/merges/I7/mirror.py --dst I7b --drop s7w1 s7w2` (the S2 waves' new files from the step mirrors port/build/S2_10_head and
S2_11w6_head, S5's from the shared tree), the edits applied by `apply_shared.py port/build/I7b --drop s7w1 s7w2`. Builds in the mirror:
`port/build/x64` and `port/build/Win32` configured with `SG_FREETYPE_ROOT=C:\Users\maria\vcpkg\installed` (FreeType 2.14.3 linked,
freetype.dll beside the DLLs; 0 warnings). Every gate was first run on `port/build/I7` (all five waves, S7 included); the ones S7's removal
can change were re-run on I7b (section 2). Python `~/bin/py -3.12`. Heavy runs one at a time, free commit charge checked before each.
Raw logs in `log/roadmap/merges/I7/` (I7) and `log/roadmap/merges/I7/b/` (I7b).

The previous baselines (S0.1, I1, I4, I5, I6) are in git history; their logs stay in `log/roadmap/S0/` and `log/roadmap/merges/`.

## Summary

| Gate | HEAD + S7 (I10) |
|---|---|
| repo static | 614 passed, 2 skipped, 46 deselected (= I7) |
| port suite x64 | 75 files (68 + S7's test_live and six test_app_* files): 74 PASS with S7.9's counts; test_fttext 2 failed (host, above); test_live 39 (incl. test_equivalence_all), test_app_live 13 (load-sensitive: 2 of 3 runs failed on wall-clock checks, I10.md section 2), test_app_ingame 12 + 1 xfail (U-1640), test_app_present 7, test_app_shell 18, test_app_input 16, test_app_replay 14, test_win32 179 + 258 skipped |
| port suite Win32 | 59 files (I7's 58 + test_live): 50 PASS; the same 20 failing test ids in the same 8 files + test_fttext's 2 (host); test_live 39 incl. test_equivalence_all |
| segments x64 / Win32 | 205 / 205 each, 0 rows (digests included) differ from I7b's or between backends |
| boot x64 / Win32 | 70 rows 66 / 2 KNOWN / 2 NO_FRAMES each, 0 rows differ from I7b's or between backends |
| S7 live equivalence (`port.test.live equiv --all`) x64 / Win32 | 68 PASS, 2 NO_FRAMES each, = I7's rows, x64 = Win32 |
| S7 app replay gate x64 / Win32 | hole 340 / 340, report 205 / 205, saveload 266 / 266 PASS, = I7's rows |
| chains / defects / ledger | 342 / 342 (= I7 row for row); 14 switches (= I7); ledger rows = re/port/divergences.tsv |
| gfx.check | 0 duplicates, 101 rows (91 bodies + 10 not_drawing) |
| frame census / terrain verdict | 698 rows PASS 88 / UNSUPPORTED 600 / NOT_RUN 10, = I7b row for row; terrain 527 PASS / 75 NOT_MEASURED |
| not re-run by I10 | SG_TEXT_METRICS=freetype segments / boot, S5 widths / labels (S7 does not touch the text providers) |

The I7 summary (before S7):

| Gate | HEAD + S2w6 + S2w7 + S5 | HEAD (I6) | Same? |
|---|---|---|---|
| repo static (`-m static`) | 614 passed, 2 skipped, 46 deselected (I7 and I7b) | 614 / 2 | yes |
| port suite x64, one file per process | 68 files, 68 PASS; test_boot 95 passed (cov_r_s2_screens now PASSES) | 62 files, 61 PASS (test_boot 94 + 1 failed) | +6 files, test_boot's failure gone |
| port suite Win32 backend | 58 files: 50 PASS, the same 8 files fail the same 20 tests (U-1100, U-1101) | 52 files: 44 PASS, 8 fail 20 | yes (identical failing test names) |
| segment gate x64 / Win32 | 205 / 205 PASS each, 0 digests differ from `log/roadmap/merges/I6/segs_*.tsv` or between the backends | 205 / 205 | yes |
| segment gate, SG_TEXT_METRICS=freetype | 205 / 205 x64 and Win32, 0 digests differ from the GDI runs | (no provider choice) | new |
| boot gate `--all` x64 / Win32 | 70 rows: 66 PASS / 2 KNOWN / 2 NO_FRAMES / 0 FAIL, x64 = Win32; HEAD's 61 at 58 / 2 / 1; cov_r_s2_screens PASS 1,268 / 1,268 | 65 / 2 / 2 / 1 (cov_r_s2_screens stops at 1,156) | +1 PASS |
| boot gate, SG_TEXT_METRICS=freetype | the same 70 rows on x64 and Win32 | | new |
| chains `integrator chain --all` | 342 / 342 PASS, every row equal to I5's / I6's (measured on I7) | 342 / 342 | yes |
| compatibility switches / profiles | 14 switches, the same segments changed (sinScaled 49, hazardCellTransposed 13); `profile fixed: 8 member(s) of 14 switches` | same | yes |
| ledger | 28 rows, 1 without a cause (showRoutingMapScreen.exitRemapReadsFrame), rows identical to I6's | same | yes |
| gfx bodies `gfx.check()` | 0 duplicate names, 101 bodies | 84 | +17 (S2.10 / S2.11 families) |
| frame census (`framegate --all`, ranked by S0's 3_rank.py) | 698 rows / 70 scenarios: PASS 88, UNSUPPORTED 600, NOT_RUN 10, FAIL 0; 0 PASS rows lost | PASS 87, UNSUPPORTED 598, NOT_RUN 13 | +1 PASS |
| S3 terrain verdict (`log/roadmap/S3/2_census.py terrain`) | 527 PASS, 0 FAIL, 75 NOT_MEASURED | 527 / 0 / 75 | yes |
| S5 widths / labels | section 6 | | new |

## 1. Repo static suite

```
~/bin/py -3.12 -m pytest -m static -q -p no:cacheprovider        # from the mirror root
```
**614 passed, 2 skipped, 46 deselected** on I7 (`static.txt`) and on I7b (`b/g_static.log`).

## 2. Port suite, x64, one file per process

```
STATIC=0 OUT=log/roadmap/merges/I7/suite sh log/roadmap/merges/I7/suite_by_file.sh       # I7 (74 files), via gates.sh step 1
sh log/roadmap/merges/I7/gates_b.sh                                                       # I7b re-run (17 x64 files, 5 Win32)
```
The full run was on I7 (`suite/`, 74 files: 73 PASS, the one failure is S7's test_live, I7.md); the 17 files S7's removal can change (they
run frames, list modules or runner ops) were re-run on I7b (`b/suite/`), all PASS with the same counts. Without S7's six files: **68 files,
68 PASS**.

| File | now | I6 | Note |
|---|---|---|---|
| test_boot | 95 passed | 94 / 1 failed | cov_r_s2_screens runs to its end (S2.10's showBuyLandScreen, U-1251) |
| test_buyland | 13 passed, 3 skipped | new file | S2.10 (skips: opt-in SG_EMU_ORACLE) |
| test_gfx_s2_10 | 4 passed, 9 skipped | new file | S2.10; the 9 need S3.5 / S3.6's `scene` / `hud` families, not merged |
| test_gfx_dialogs2 | 26 passed, 1 skipped | new file | S2.11; the skip is championship tournament_end, which has a snapshot since U-1120 and is not judged yet (U-1218), as 11_merge.md says |
| test_fttext | 21 passed, 1 skipped | new file | S5; the skip needs `SG_PORTSIM_DLL_NOFT` (a -DPORTSIM_FREETYPE=OFF build): 22 passed with it (section 6) |
| test_fttext_raster | 9 passed | new file | S5 (ClearType host) |
| test_labels | 14 passed | new file | S5 |
| test_gditext | 22 passed, 1 skipped | 22 / 1 | S5's split of gditext |
| test_vectors | 298 passed, 520 skipped (I7) | 298 / 470 | +50 skips, `no vector file` for the new registry names |
| test_win32 | 179 passed, 257 skipped (I7b) | 179 / 233 | +24 skips, the same names |
| every other file of I6's table | I6's count | | test_tick 250, test_terrain_live 397, test_gfx_reports 36, test_gfx_dialogs 32, test_commands 83 / 44, test_s2_6_scenario 4, test_static 6, ... |

## 3. Port suite, Win32 backend

```
SG_BACKEND=win32 STATIC=0 OUT=log/roadmap/merges/I7/suite_win32 FILES="$(cat log/roadmap/merges/I7/win32_files.txt)" sh log/roadmap/merges/I7/suite_by_file.sh
```
I6's 52 files + test_buyland, test_gfx_s2_10, test_gfx_dialogs2, test_fttext, test_fttext_raster, test_labels (+ S7's test_live on I7):
**58 files, 50 PASS; the same 8 files fail the same 20 tests** (browser 1, course_menu 1, save_dialogs 1, saveload 6, holestats 2,
integrator 4, semantic_input 4, tick 1). The new files: buyland 13 + 3 skipped, gfx_s2_10 4 + 9, dialogs2 26 + 1, fttext 21 + 1, fttext_raster
9, labels 14. test_tick read 3 failures in the I7 pass (two test_negctl_reads_red_on_segments cases whose pre-control run read FAIL); its
re-run alone and the I7b run read the known 1 (`suite_win32/test_tick.rerun.txt`, `b/suite_win32/test_tick.txt`).

## 4. Segment gate

```
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --jobs 4 --out <tsv>
~/bin/py -3.12 -m port.test.integrator segments --backend win32 --dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe --jobs 4 --out <tsv>
SG_TEXT_METRICS=freetype <the same>
~/bin/py -3.12 -m port.test.integrator compare <a> <b>
```
x64 **205 / 205**, Win32 **205 / 205**, 0 of 205 rows differ from I6 on either backend or between them, on I7 and I7b; with
SG_TEXT_METRICS=freetype 205 / 205 on x64 and Win32, 0 rows differ from the GDI runs (`segs_compare.log`, `segs_ft_compare.log`,
`b/compare.log`).

## 5. Boot gate

```
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out <tsv>            # SG_BACKEND=win32 / SG_TEXT_METRICS=freetype for the others
~/bin/py -3.12 log/roadmap/merges/I7/boot_compare.py <tsv> [<other tsv>]   # against log/roadmap/merges/I6/boot_x64.tsv
```
70 rows on each backend, `{"PASS": 66, "KNOWN": 2, "NO_FRAMES": 2}`. The one row that differs from I6: **cov_r_s2_screens FAIL 1,156 ->
PASS 1,268 / 1,268** (S2.10 ports showBuyLandScreen, U-1251). HEAD's 61 at **58 PASS, 2 KNOWN (cov_a_golfer, cov_b_holes18, U-1062), 1
NO_FRAMES (cov_b_location)**. x64 = Win32, and the SG_TEXT_METRICS=freetype runs equal the GDI ones row for row (x64 and Win32), on I7
and I7b.

## 6. Section gates

S2 wave 6 (S2.10): test_buyland 13 / 3 skipped, test_gfx_s2_10 4 / 9 skipped (x64 and Win32), segments and boot as above with
cov_r_s2_screens PASS 1,268 / 1,268. S2 wave 7 (S2.11): test_gfx_dialogs2 26 + 1 skipped x64 and Win32 (U-1218's tournament_end skip),
test_gfx_reports 36 (control 8 without hole_stats), test_gfx_dialogs 32; census +1 PASS (bulletin). gfx.check 0 duplicates, 101 bodies.

S5 (FreeType linked; run on I7, the provider paths do not touch S7's code):
- `port.test.widths_check`: GDI and FreeType on x64, FreeType on Win32 all read textWidth 11,473 / 11,481 rows (printable 11,459), Text_put
  9,093 / 9,101 (9,079), drawMarkupRun 10,790 / 10,798 (10,776); the differing rows are the lone 0x0a rows (U-0960), as S5 measured.
- `port.test.labels`: corpus 6,540 entries / 15,960 jobs; compare gdi 0 jobs / 0 px, every checkpoint 0 px; compare freetype 15,618 /
  15,960 jobs, 8,736,593 px, digest 5a336eba875e1d94..., x64 = Win32 (deterministic twice per backend).
- `3_gfx_ft.py --provider fttext` (SG_TEXT_METRICS=freetype): menu 1,735 / refused 15,347 / site_hover 13,964 / buttons 13,600 / tip
  13,714 / back 1,735 px, all inside label boxes, 0 outside.
- test_gditext 22 + 1, test_fttext 22 with `SG_PORTSIM_DLL_NOFT`, test_labels 14, test_fttext_raster 9, port test_static 6.
- GDI-only builds: configuring without SG_FREETYPE_ROOT prints `portsim: no FreeType (module fttext throws MissingResource)` and
  `-DPORTSIM_FREETYPE=OFF` builds x64 and Win32 (port/build/noft, noft32 in the I7 mirror); on that DLL test_fttext's 19 FreeType tests skip
  and test_gditext / test_static pass. GDI stays the default (decision G5).

## 7. Switches, chains and the ledger

```
~/bin/py -3.12 -m port.test.integrator chain --backend x64 --all --jobs 4 --out i7/ledger/chain_x64.tsv
~/bin/py -3.12 -m port.test.integrator defects --backend x64 --jobs 4 --out i7/ledger/defects_x64.tsv
~/bin/py -3.12 -m port.test.integrator ledger --segments i7/ledger/segs_x64.tsv --chain i7/ledger/chain_x64.tsv --defects i7/ledger/defects_x64.tsv --out i7/ledger/divergences.tsv
```
(measured on I7; S7 is inert in trace runs, its removal leaves every segment and boot row equal.) Chains **342 / 342 PASS**, every row equal
to I5's / I6's. Defects: 14 switches changing the same segments as at I6. Ledger: **28 rows, 1 without a cause**
(showRoutingMapScreen.exitRemapReadsFrame), rows identical to I6's, so re/port/divergences.tsv is unchanged. Profiles: 8 of 14.

## 8. Frame census

```
~/bin/py -3.12 -m port.test.framegate --all --jobs 2 --out log/roadmap/merges/I7/b/census.tsv --resume
~/bin/py -3.12 i7/roadmap/S0/3_rank.py log/roadmap/merges/I7/b/census.tsv log/roadmap/merges/I7/b/census_out   # S0's 3_rank.py copied into the mirror
~/bin/py -3.12 log/roadmap/merges/I7/census_compare.py log/roadmap/merges/I7/b/census_out/frame_census.tsv - log/roadmap/merges/I7/b/census.tsv
~/bin/py -3.12 log/roadmap/S3/2_census.py terrain --jobs 3 --out log/roadmap/merges/I7/b/terrain_verdicts.tsv          # from the mirror root
```
re/roadmap/frame_census.tsv and frame_causes.tsv are this census (698 rows, 70 scenarios; the I7 and I7b censuses are equal row for row in
status and px). Status: **PASS 88** (I6 87, S0 45), UNSUPPORTED 600, NOT_RUN 10, FAIL 0. Over I6's rows: UNSUPPORTED -> PASS 1
(cov_r_s2_screens bulletin, S2.11), NOT_RUN -> UNSUPPORTED 3 (cov_r_s2_screens buy_land 221 px, end, land_popup: the boot now runs past
record 1,156), 0 PASS rows lost; over S0's 609 rows PASS 73, 0 lost. NOT_RUN 10 = golfer_events (no trace). Ranked by S3's rule
(`b/census_ranked_s3rule.tsv`): Widget_writePixel 443, PASS 88, terrain not measured 74, Surface_stretchBlit 70, Surface_drawScaled2 13,
no trace 10. Terrain verdict: **527 PASS, 0 FAIL**, 75 NOT_MEASURED.

## What a later run must hold

(HEAD d66aa755 + S3, I13) The I10 list below, with these numbers in place of its: the x64 suite's 82 files with I13's counts
(`log/roadmap/merges/I13/suite/`, `suite_app/`; test_fttext's 2 on this host); the Win32 suite's 66 files with only the 22 known ids;
the census at PASS 100 with no PASS row of `log/roadmap/merges/I13/census.tsv` lost, terrain 527 PASS; gfx.check 131 bodies, 0
duplicates. The app files' pacing gates measure the host: run them one file per process at low load, and restore
log/roadmap/S7/8_*facts.json to HEAD's content after every app run (U-1398).

(HEAD + S7, I10) Everything below, plus: the x64 suite's 75 files with I10's counts (S7's files as in the summary; test_fttext's two GDI
metric tests fail on this host until the host change of I10.md section 3 is settled, in every mirror); the Win32 suite's 20 known failures
(+ test_fttext's 2 on this host) and test_live 39 there; live equivalence 68 PASS + 2 NO_FRAMES on x64 and Win32 with the rows of
`log/roadmap/merges/I10/equiv_x64.tsv` / `equiv_win32.tsv`; the app replay gate 340 / 205 / 266 PASS on both backends; the reference TSVs
`log/roadmap/merges/I10/{segs,boot}_{x64,win32}.tsv` (equal to I7b's). test_app_live checks wall-clock pacing and live captures: re-run it
alone on a quiet machine before counting a failure.

- repo static 614 passed / 2 skipped; every file of section 2 PASS with its count (test_boot 95 passed);
- segments 205 / 205 PASS on both backends with 0 digests differing from `log/roadmap/merges/I7/b/segs_x64.tsv` and `segs_win32.tsv`
  (identical to I6's, I5's, I4's, I1's and S0's), also with SG_TEXT_METRICS=freetype;
- boot gate: the 70 rows of `log/roadmap/merges/I7/b/boot_x64.tsv` (66 / 2 / 2 / 0; HEAD's 61 at 58 / 2 / 1), on x64 and Win32;
- the 8 Win32 files of section 3 keep their 20 failures until U-1100 / U-1101 are fixed (a different count is a change);
- chains 342 / 342; defects as section 7; profiles fixed 8 of 14; the ledger over fresh inputs has 1 row without a cause;
- the census: no PASS row of re/roadmap/frame_census.tsv lost (88); S3.2's terrain verdict 527 PASS;
- S5: the widths / labels numbers of section 6 when built with FreeType; the port builds and its suite runs without FreeType.
