# Regression baseline at HEAD 2c05f02a + S2 + S3 (I5, 2026-10-25)

This file now describes **HEAD + S2 + S3**: commit `2c05f02a` (which holds S0, S6, S1 and S8) plus S2 waves 1-4 and S3 wave 1, i.e. the
paths of `log/roadmap/S2/files.txt`, `files_wave2.txt`, `files_wave3.txt`, `files_wave4.txt` and `log/roadmap/S3/files.txt` and the
shared-file edits of their merge (`log/roadmap/merges/S2S3-5.md`). Measured by integrator I5 in the mirror `port/build/I5_S2S3`: git
archive of HEAD, the junctions of `log/roadmap/S0/export.md` plus `shim/build`, `re/replay` and `extracted`, the section files copied by
`log/roadmap/merges/S2S3-5/mirror.py` (S2 waves 2 / 3 new files from the S2 lead's mirrors port/build/S2_3_head / S2_4_head, every
other one from the shared tree), the edits applied by `apply_shared.py` and `gate_edits.py`. All commands run from the mirror root. Builds
in the mirror: `port/build/x64` and `port/build/Win32` (logs `log/roadmap/merges/S2S3-5/build/`, 0 warnings). Python `~/bin/py -3.12`.
Heavy runs one at a time, free commit charge checked before each (16.9-23.1 GB). Raw logs in `log/roadmap/merges/S2S3-5/`.

The previous baselines (HEAD alone by S0.1, HEAD + S6 by I1, HEAD + S1 + S8 by I4) are in git history; their logs stay in
`log/roadmap/S0/`, `log/roadmap/merges/S6-1/` and `log/roadmap/merges/S1S8-4/`.

## Summary

| Gate | HEAD + S2 + S3 | HEAD + S1 + S8 (I4) | Same? |
|---|---|---|---|
| repo static (`-m static`) | 614 passed, 2 skipped, 46 deselected | 614 / 2 | yes |
| port suite x64, one file per process | 57 files; 56 PASS; test_boot 94 passed + 1 failed (cov_r_s2_screens, U-1253, the new FAIL row MERGE_wave4.md 1.3 states) | 52 files PASS, test_boot 92 | yes for I4's files; +5 S2 / S3 files |
| port suite Win32 backend | 46 files: 38 PASS, the same 8 files fail the same 20 tests (U-1100, U-1101) | 41 files: 33 PASS, 8 fail 20 | yes (identical failing test names) |
| segment gate x64 | 205 / 205 PASS, 0 digests differ from `log/roadmap/merges/S1S8-4/segs_x64.tsv` | 205 / 205 | yes |
| segment gate Win32 | 205 / 205 PASS, 0 digests differ from `S1S8-4/segs_win32.tsv` and from the x64 run | 205 / 205 | yes |
| boot gate `--all` x64 | 70 rows: I4's 67 with 0 rows differing (HEAD's 61 at 58 PASS / 2 KNOWN / 1 NO_FRAMES); S2's cov_a_quit PASS 301 / 301, cov_a_quit_title NO_FRAMES, cov_r_s2_screens FAIL at record 771 (U-1253) | 67 rows | yes for I4's 67; +3 S2 rows as stated |
| boot gate `--all` Win32 | the same 70 rows, 0 differ from x64 | 67 = x64 | yes |
| chains `integrator chain --all` | 342 / 342 PASS, every row equal to I4's | 342 / 342 | yes |
| compatibility switches `integrator defects` | 12 switches; sinScaled 49, prepareShot.hazardCellTransposed 13, the other 10 0 | same | yes |
| ledger (fresh inputs; written to re/port/divergences.tsv) | 26 rows, 1 without a cause (showRoutingMapScreen.exitRemapReadsFrame) | 25 rows, 1 without a cause | +1 row: cov_r_s2_screens has no segment corpus run yet |
| frame census (`framegate --all`, ranked by S0's 3_rank.py) | 698 rows / 70 scenarios: PASS 82, UNSUPPORTED 592, FAIL 4, NOT_RUN 20 (S0's 609 rows: PASS 73, 0 PASS rows lost) | S0: 609 rows, PASS 45 | new record (re/roadmap/frame_census.tsv, frame_causes.tsv) |

## 1. Repo static suite

```
~/bin/py -3.12 -m pytest -m static -q -p no:cacheprovider
```
**614 passed, 2 skipped, 46 deselected** (`log/roadmap/merges/S2S3-5/static.txt`), equal to I4.

## 2. Port suite, x64, one file per process

```
STATIC=0 sh log/roadmap/merges/S2S3-5/suite_by_file.sh      # a copy of S1S8-4/suite_by_file.sh pointed at port/build/I5_S2S3
~/bin/py -3.12 log/roadmap/merges/S2S3-5/suite_compare.py log/roadmap/merges/S2S3-5/suite log/roadmap/merges/S1S8-4/suite
```
OUT `log/roadmap/merges/S2S3-5/suite/<file>.txt`, run log `suite_x64.log`, 57 files. Changes from HEAD + S1 + S8:

| File | HEAD + S2 + S3 | HEAD + S1 + S8 | Note |
|---|---|---|---|
| test_boot | 94 passed, 1 failed | 92 passed | +cov_a_quit, cov_a_quit_title (S2.5 traces); the failure is test_boot_gate[cov_r_s2_screens]: FAIL at record 771, the row MERGE_wave4.md 1.3 says the gate reads as a new FAIL, not a regression (U-1253, no KNOWN entry by the orchestrator's decision) |
| test_gfx | 25 passed | 17 | S2.1 |
| test_gfx_credits | 47 passed | new file | S2.3 |
| test_gfx_routing | 35 passed | new file | S2.4 |
| test_gfx_eoi | 5 passed | new file | S3.3 |
| test_s2_5_traces | 3 passed | new file | S2.5 |
| test_s2_6_scenario | 3 passed, 2 xfailed (U-1253) | new file | S2.6 |
| test_gl_raster | 177 passed | 139 | S3.1b (the mirror needs re/tools/glexact/glreplay.exe, an untracked build output: without it 175 passed / 2 skipped) |
| test_terrain_render | 522 passed | 40 | S3.1 + S3.1b |
| test_s8_2_scenarios | 14 passed | 14 | after merge gate edit 1 (the hook test asserts the widened glob) |
| test_vectors | 298 passed, 454 skipped | 298 / 436 | +18 skips, all `no vector file`: the 9 new registry names gfx.call, gfx.check, gfx.family.<7 families> |
| test_win32 | 179 passed, 225 skipped | 179 / 216 | +9 skipped, the same names |
| every other file of I4's table | I4's count | | test_tick 250, test_sve_roundtrip 15, test_terrain_state 500, test_terrain_live 144, test_terrain_gl_setup 6, test_static 6, test_commands 83 / 44 |

## 3. Port suite, Win32 backend

```
SG_BACKEND=win32 STATIC=0 OUT=log/roadmap/merges/S2S3-5/suite_win32 FILES="<I4's 41 files> port/test/test_gfx_credits.py port/test/test_gfx_routing.py port/test/test_gfx_eoi.py port/test/test_s2_5_traces.py port/test/test_s2_6_scenario.py" sh log/roadmap/merges/S2S3-5/suite_by_file.sh
```
46 files: **38 PASS** (I4's 33 plus gfx_credits 47, gfx_routing 35, gfx_eoi 5, s2_5_traces 3, s2_6_scenario 3 + 2 xfailed; test_gfx 25).
**The same 8 files fail the same 20 tests** (browser 1, course_menu 1, save_dialogs 1, saveload 6, holestats 2, integrator 4, semantic_input 4,
tick 1): sorted `FAILED` lines identical to `S1S8-4/suite_win32/` and `S6-1/suite_win32/` (`win32_compare.log`).

## 4. Segment gate

```
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --jobs 4 --out log/roadmap/merges/S2S3-5/segs_x64.tsv
~/bin/py -3.12 -m port.test.integrator segments --backend win32 --dll32 port/build/Win32/Release/portsim_c.dll --runner32 port/build/Win32/Release/portsim_runner32.exe --jobs 4 --out log/roadmap/merges/S2S3-5/segs_win32.tsv
~/bin/py -3.12 -m port.test.integrator compare <a> <b>
```
x64 **205 / 205 PASS** (203.6 s), Win32 **205 / 205 PASS** (297.1 s); 0 of 205 rows differ against I4 on either backend or between the
backends (`segs_compare.log`).

## 5. Boot gate

```
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out log/roadmap/merges/S2S3-5/boot_x64.tsv
SG_BACKEND=win32 ~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out log/roadmap/merges/S2S3-5/boot_win32.tsv
~/bin/py -3.12 log/roadmap/merges/S2S3-5/boot_compare.py <tsv> [<other tsv>]   # against S1S8-4/boot_x64.tsv
```
70 rows on each backend (`{"PASS": 65, "KNOWN": 2, "NO_FRAMES": 2, "FAIL": 1}`). I4's 67 rows: 0 differ (HEAD's 61 at **58 PASS, 2 KNOWN
(cov_a_golfer, cov_b_holes18, U-1062), 1 NO_FRAMES (cov_b_location)**; S1's 5 and S8's 1 PASS with every record). S2's rows: cov_a_quit PASS
301 / 301, cov_a_quit_title NO_FRAMES (title menu only), cov_r_s2_screens FAIL 1,156 / 1,268 with its first divergence at record 771
(g_cursorTileX / g_cursorTileY, U-1253) and the stop at the unported showBuyLandScreen (U-1251). x64 and Win32 rows identical.

## 6. Section gates

From the suites: test_gfx 25 x64 + Win32, test_gfx_credits 47 x64 + Win32, test_gfx_routing 35 x64 + Win32, test_gfx_eoi 5 x64 + Win32,
port test_static 6, test_s2_5_traces 3, test_s2_6_scenario 3 + 2 xfailed, test_terrain_render 522, test_gl_raster 177, test_terrain_state 500,
test_terrain_live 144 + test_terrain_gl_setup 6. From the census (section 8): cov_r_quit quit_00..03 and credits PASS 0 px (S2.3),
cov_k1_routing routing and cov_a_keys k_dlg_L PASS, k_dlg_R 370 px (S2.4, held), cov_a_quit 10 PASS + 5 in-game UNSUPPORTED and
cov_a_quit_title 9 PASS (S2.5: 19 terrain-free rows), sandbox_basic + cov_b_location + cov_r_quit 16 PASS (S2.1's 11 plus the 5 credits rows),
and the 22 rows of cov_a_champ (11), championship (6) and cov_k3_career (5) all run, none NOT_RUN (S3.3, U-1120).

## 7. Switches, chains and the ledger

```
~/bin/py -3.12 -m port.test.integrator chain --backend x64 --all --jobs 4 --out i5/ledger/chain_x64.tsv
~/bin/py -3.12 -m port.test.integrator defects --backend x64 --jobs 4 --out i5/ledger/defects_x64.tsv
~/bin/py -3.12 -m port.test.integrator ledger --segments i5/ledger/segs_x64.tsv --chain i5/ledger/chain_x64.tsv --defects i5/ledger/defects_x64.tsv --out i5/ledger/divergences.tsv
```
(inputs inside the mirror root, `i5/ledger/`, since the ledger refuses inputs outside ROOT; copies in `log/roadmap/merges/S2S3-5/`.) Chains
**342 / 342 PASS**, every row's status / frames / record / digest equal to I4's (`chain_compare.log`, 997 s). Defects: the 12 switches change the
same segments as at I4 (`defects_compare.log`). Ledger: **26 rows, 1 without a cause** (showRoutingMapScreen.exitRemapReadsFrame); the new row is
`scenario-no-segment cov_r_s2_screens` (no segment corpus run yet). re/port/divergences.tsv is this ledger.

## 8. Frame census

```
~/bin/py -3.12 -m port.test.framegate --all --jobs 2 --out log/roadmap/merges/S2S3-5/census.tsv --resume
~/bin/py -3.12 i5/roadmap/S0/3_rank.py log/roadmap/merges/S2S3-5/census.tsv log/roadmap/merges/S2S3-5/census_out   # S0's 3_rank.py copied into the mirror
~/bin/py -3.12 log/roadmap/merges/S2S3-5/census_compare.py re/roadmap/frame_census.tsv <S0's>
```
re/roadmap/frame_census.tsv and frame_causes.tsv are this census (698 rows, 70 scenarios; S0's rerun step 3_rerun_noend.py is no longer needed,
S3.3). Status: **PASS 82** (S0 45), UNSUPPORTED 592, FAIL 4, NOT_RUN 20. Over S0's 609 rows: PASS 73, UNSUPPORTED 526, NOT_RUN 10 (golfer_events,
no trace); transitions NOT_RUN -> PASS 21, NOT_RUN -> UNSUPPORTED 8, UNSUPPORTED -> PASS 7, 0 PASS rows lost. The 89 rows new since S0 (cov_r_bio,
cov_r_flic_input, cov_r_palette_hot, cov_r_theme_cancel, cov_r_tutorial, cov_r_s2_screens) are 9 PASS, 66 UNSUPPORTED, 10 NOT_RUN (cov_r_s2_screens
past record 771) and 4 FAIL (cov_r_s2_screens loading_t200 / loading_t300 / theme_packs / difficulty: whole-frame differences with no unsupported
call, screens the port does not draw yet). Terrain verdict (G2 on the view area): 589 FAIL, 3 PASS. Primary causes: terrain not drawn 589. First
unsupported drawing call (2d_all): Widget_writePixel 453 (S0: 316), Surface_stretchBlit `a SpriteRef the backend has no sprite for` 126,
Surface_drawScaled2 13; S0's Surface_blit3 (168) and Image_draw SpriteRef (39) causes no longer occur as a first unsupported call.

## What a later run must hold

- repo static 614 passed / 2 skipped; every file of section 2 PASS with its count, test_boot 94 passed with cov_r_s2_screens its one failure until
  U-1253 / U-1251 are settled;
- segments 205 / 205 PASS on both backends with 0 digests differing from `log/roadmap/merges/S2S3-5/segs_x64.tsv` and `segs_win32.tsv`
  (identical to I4's, I1's and S0's);
- boot gate: I4's 67 rows unchanged (HEAD's 61 at 58 / 2 / 1), cov_a_quit PASS 301, cov_a_quit_title NO_FRAMES, cov_r_s2_screens FAIL at record 771,
  on x64 and Win32;
- the 8 Win32 files of section 3 keep their 20 failures until U-1100 / U-1101 are fixed (a different count is a change);
- chains 342 / 342; defects as section 7; the ledger over fresh inputs has 1 row without a cause;
- the census: no PASS row of re/roadmap/frame_census.tsv lost.
