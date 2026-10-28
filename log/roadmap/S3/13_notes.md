# S3.13 notes: the I9 refusal fixed (S3 waves 2 + 4 + 5 resubmission rebased on 78ffb06d)

Integrator I9 refused S3.11's resubmission (MERGE_w5.md) on HEAD 78ffb06d (log/roadmap/merges/I9.md): two bodies meet between S3.7 and
S2.11 (drawStringRightAt registered twice; drawWindow split between S3.7's text family and S2.11's simfoto hand-over), test_gfx_s2_10's
skill dialog 8 px, and the ledger's flic.treeAliasNames row without evidence. This step makes ONE resubmission on 78ffb06d and measures it
there. Nothing in the shared tree was edited, staged or committed; every change is an anchored edit in log/roadmap/S3/13_edits.py.

## 1. The tree

`~/bin/py -3.12 log/roadmap/S3/13_assemble.py [--recopy]` = 11_assemble.py's steps (unchanged) on `git archive 78ffb06d` into
port/build/S3_13, then step s313: 13_edits.py (36 edits in 9 files, each old text exactly once) and the CLAUDE.md paragraph of
MERGE_w5b.md section 6. Before any fix the assembled tree was identical to I9's mirror port/build/I9 in port/src, port/include, port/test,
re/port, UNCERTAINTIES.md and CLAUDE.md (diff -rq, line ends ignored), so I9's numbers are this step's baseline.
13_edits.py is generated from the fixed mirror by 13_make_edits.py (base port/build/I9, each hunk grown until its old text is unique,
then checked: applying the edits to the base gives the mirror byte for byte). Builds: port/build/S3_13/port/build/x64 and Win32
(port/test/build.py with SG_FREETYPE_ROOT, as I9; 0 warnings, 0 errors: 13_build_x64_2.log, 13_build_w32.log).

## 2. What changed and why

I9 found three places where S3.7 (text family) and S2.10 / S2.11 (committed in 78ffb06d) implement the same drawing, plus a ledger row
without evidence. 13_edits.py's 36 edits (table and reasons in MERGE_w5b.md section 2) resolve them with one body per name and one
drawing call per original call:

- **drawStringRightAt: one body, the text family's (S3.7).** It is the original's pair (c3df.cpp DrawString_re = drawRichText(text,
  x - drawMarkupRun(text, n), y, n)); S2.11's `results` body measured with plain glyph widths. gfx_results.cpp lost its body and
  control 1 (6 edits); the control moved to `gfx.family.text` mode 15. Its one caller, showTournamentResults (tick_panels.inc), draws
  on the root after Widget_setQuad(root, 0x80001284, -1, 2, 2), which prim_text hands to the text family.
- **drawWindow: one body, the text family's `drawWindow.frame`, with S2.11's probe.** S3.7's body already draws everything S2.11's
  golfer arm drew and the head (composeGolferSprite + Surface_drawScaled). S2.11's held-pixel probe is a measurement of the dialogs2
  gate, so gfx_simfoto.cpp keeps it as the exported helpers `gfxs::window_probe` / `gfxs::head_probe` and the text body calls them for
  sel >= 0, where S2.11's body ran them (9 edits in gfx_simfoto.cpp, 10 in gfx_text.cpp). `drawWindow.golfer` and simfoto controls
  2..4 are gone; they are `gfx.family.text` modes 12 / 13 / 14. gfx.cpp's dead core `b_draw_window` was removed (1 edit).
- **Two duplicate draws removed.** Since S3.7, commands.cpp's draw_window_effect DRAWS the window. S2.11 had added a second
  `draw_window_gfx(ctx, 0xf0, -8, v)` after it in tickMessage (commands_layout.inc), so the ticker was drawn twice: the cause of I9's
  test_gfx_text frame failures (tour_0060, cov_r_tutorial p01 / p10 / p15, cov_k1_label) and of the dialogs2 ticker rectangles. S2.10
  had added a `drawWindow.frame` drop after `draw_window_effect(ctx, -2)` in showCharacterIntro (dialogs.cpp), which drew the intro
  window at (0xc8, 0x172) and again at draw_window_effect's default ticker position (0xf0, -8), under the skill dialog's title: the 8 px
  of test_gfx_s2_10's skill_points. showCharacterIntro now calls `draw_window_effect(ctx, -2, 0xc8, 0x172)` (1 edit), and the second
  ticker draw is gone (1 edit). commands.cpp: one comment edit (draw_window_gfx is used only by narrateShot's draw_window_gfx_saved).
- **Tests.** test_gfx_dialogs2.py (6 edits): docstring, test_bodies_handled no longer wants an `unsupported drawThoughtBubble: the
  head` line (the head is drawn now), FRAME_NEGCTL renamed to the text family's modes 12..15.
- **Ledger.** port/test/integrator.py's switch-evidence map gained `flic.treeAliasNames` ->
  test_defects_flic.py::test_fault_probe_on_and_off, test_tropical_world_loads_equal_the_alias_recording_off (state) (1 edit).

Skill dialog, framegate on cov_r_s2_screens skill_points / skill_popup (log/roadmap/S3/13_skill/): before the fix (skill.tsv) 5,828 px
each (hud_meters 252, hud_message 1,765, toolbar 1, view 3,810); after it (fix/skill.tsv) 68 px each (view 67, all one level, toolbar 1).
test_gfx_s2_10's test_skill_dialog_is_zero_px counts only the pixels inside SKILL_RECTS: 8 in I9, 0 here (section 3: x64 and Win32 suite file test_gfx_s2_10 PASS 13 / 13).

## 3. Gates (port/build/S3_13, x64 and Win32)

Logs: 13_gates.log (one line per step) and 13_g_<step>.log; comparisons by 13_compare.py in 13_*_compare*.log. I9 = log/roadmap/merges/I9
(the same tree before S3.13's edits), I7b = HEAD 78ffb06d's own record (re/roadmap/BASELINE.md after I7).

| Gate | port/build/S3_13 | I9 | baseline (I7b) |
|---|---|---|---|
| repo static | 614 passed, 2 skipped | 614 / 2 | 614 / 2 |
| port suite x64 | 75 files, 75 PASS (13_g_suite_x64.log files 1-30, 13_g_suite_x64_b.log files 31-75 numbered [N/45] = file 30 + N); vs I9 65 SAME, 10 CHANGED, all now passing: gfx, boot_screens, credits, routing, reports, pixel (1 failed each in I9), dialogs2 (6), s2_10 (1), gfx_text (5), gfx_hud (1, under load in I9); 13_suite_compare_x64.log | 66 / 75 | 68 / 68 |
| port suite Win32 | all 75 files (I7b / I9 ran 58 / 65): 61 PASS, 14 FAIL (13_g_suite_win32.log, 13_suite_compare_win32.log). The 8 known files fail the same 20 tests with the same names as I7b (browser 1, course_menu 1, save_dialogs 1, saveload 6, holestats 2, integrator 4, semantic_input 4, tick 1 = test_run_frames_reports_why_it_stopped; I9's 20 tick failures were load). All 9 of I9's clash files PASS (gfx, boot_screens, credits, routing, reports, pixel, dialogs2 26 + 1 skipped, s2_10 13, gfx_text 32 + 20). Of the 10 files outside the Win32 list, 5 PASS (s8_2_scenarios, segment_files, static, sve_header_readers, win32) and 5 fail on the Win32 harness itself: gl_raster 177, terrain_gl_setup 6, terrain_render 522 errors, terrain_state 500 errors (they call lib.dll.sg_raster_new / sg_gl_setup / sg_gl_set_globals; Lib32 has no in-process dll) and boot 1 (test_quit_writes_the_originals_files reads run_detail, which segments.py _detail returns empty on the Win32 runner). test_fttext 2 failed, alone again 2 failed: host GDI metric words, U-1393 (the same two tests fail on x64 and in I9's mirror at those times and pass at others) | 48 / 65 | 50 / 58, 20 known failures in 8 files |
| segments x64 / Win32 / freetype x64 | 205 / 205 each, 0 rows differ from I9 or I7b, x64 = Win32 | 205 / 205 | 205 / 205 |
| boot x64 / Win32 / freetype x64 | 70 rows 66 PASS / 2 KNOWN / 2 NO_FRAMES each, 0 rows differ from I9; HEAD's 61 at 58 / 2 / 1 | same | same |
| chains | 342 / 342 PASS, 0 rows differ from I9 | 342 / 342 | 342 / 342 |
| defects / ledger | 15 switches, 0 rows differ from I9; ledger 30 lines, the only change: `flic.treeAliasNames` now `evidence` (test_defects_flic.py) instead of `no evidence`; 1 row without a cause (showRoutingMapScreen.exitRemapReadsFrame, as at I7) | 30 lines, 2 without a cause | 29 lines, 1 |
| gfx.check | 0 duplicates, 131 bodies, x64 and Win32; Surface_drawSprite / Widget_fillRect `reports`, drawStringRightAt / drawWindow.frame `text`, no drawWindow.golfer | 1 duplicate, 132 bodies | 0 / 101 |
| frame census | 698 rows: PASS 100, FAIL 537, UNSUPPORTED 51, NOT_RUN 10 = I9's counts, 0 PASS lost or gained; per row vs I9 629 same, 59 better, 0 worse, 10 unjudged (the 59 lost the doubled ticker: e.g. cov_b_title career 75,663 -> 180 px, championship tour_0060 36,063 -> 94, cov_r_tutorial p01 30,089 -> 226) | PASS 100 | PASS 88 |
| S3.2 terrain verdict | 527 PASS, 75 NOT_MEASURED, 0 rows differ from I9 | 527 | 527 |
| test_gfx_text frames (alone, x64) | test_text_frames_equal_the_original 12 passed (13_gfx_text_frames_x64.log) | 5 failed | - |

## 4. Controls

S3.13 adds no new control. It moves four and keeps the rest of MERGE_w5.md's controls unchanged:

| control now | was | read by | result |
|---|---|---|---|
| gfx.family.text 15 (drawStringRightAt ends one pixel right) | results 1 | test_gfx_dialogs2 test_frames_negctl_red, cov_b_shots results | RED |
| gfx.family.text 12 (message text one pixel lower) | simfoto 2 | same, ui_tour menus | RED |
| gfx.family.text 13 (ball frame one pixel left) | simfoto 3 | same, ui_tour menus | RED |
| gfx.family.text 14 (text offset for a window under 0x60 ignored) | simfoto 4 | same, championship tour_0180 | RED (I9: BLIND, 35,312 = 35,312 px, because S2.11's golfer body no longer ran) |

The kept S2.11 controls read RED in the same file: results 2, bulletin 1..4, simfoto 1 (test_ball_cut_negctl_red). The four test_gfx_s2_10
controls (chareditor 1..3, buyland 1..3, gfx.attach 7: 7 cases) read RED on x64 (suite file 34 PASS, 13 passed). test_gfx_dialogs2 passes
26 / 1 skipped on x64 (the skip is tournament_end, U-1218), so all nine of its frame controls and the ball-cut control are RED. In the two
files the drawWindow clash had broken: 17 RED / 17, 0 blind. Every other control of S3 waves 2 / 4 / 5, S3.7 and S3.8 is read by its own test file in the suites below.

## 5. Reproduce

```
~/bin/py -3.12 log/roadmap/S3/13_assemble.py [--recopy]                     # port/build/S3_13 (13_assemble.log)
~/bin/py -3.12 log/roadmap/S3/13_make_edits.py                              # only when the mirror's fixes change: rewrites 13_edits.py
(cd port/build/S3_13; SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed' ~/bin/py -3.12 port/test/build.py --arch x64; ... --arch Win32)
nohup sh log/roadmap/S3/13_gates_all.sh > log/roadmap/S3/13_gates.log 2>&1 &  # I9's 14 gate steps (13_gates.sh), short ones first
~/bin/py -3.12 log/roadmap/S3/13_compare.py suite x64|win32 | boot x64|win32|ft_x64 | segs | ledger | census
(cd port/build/S3_13; ~/bin/py -3.12 ../../../log/roadmap/S3/13_gfx_check.py x64 win32)
~/bin/py -3.12 log/roadmap/S0/3_rank.py log/roadmap/S3/13_census.tsv log/roadmap/S3/13_census_out
nohup sh log/roadmap/S3/13_suite_rest.sh x64 > log/roadmap/S3/13_g_suite_x64_b.log 2>&1 &     # port suite x64 files 31..75 (FROM=<n> to resume)
nohup sh log/roadmap/S3/13_suite_rest.sh win32 > log/roadmap/S3/13_g_suite_win32.log 2>&1 &   # port suite Win32, all 75 files
~/bin/py -3.12 log/roadmap/S3/13_claude_para.py                             # MERGE_w5b.md section 6 into port/build/S3_13's CLAUDE.md
~/bin/py -3.12 log/roadmap/S3/13_files.py                                   # files_w5b.txt
```
`13_gates.sh` is I9's gates.sh pointed at the mirror; every runner that is not pytest is wrapped in `test-slot` with the real
python.exe (test-slot starts an exe; the `~/bin/py` shim is a shell script and failed with WinError 193).

## 6. Open

- One U-row, U-1393 (13_uncertain.tsv): the host GDI's metric words for the install's Klepto / Manual SSi faces change with
  the host's state; U-1394..U-1397 unused.
- `13_assemble.py --dst S3_13_verify` from a fresh archive was not run (the reboot stopped the first worker before it). What holds it:
  13_make_edits.py's check (13_edits.py applied to port/build/I9 = the mirror byte for byte) and 11_assemble.py's own verified steps.
- The measured tree got its CLAUDE.md paragraph after the gates (13_claude_para.py; no code or test file changed by it).
- U-1393: test_fttext's two GDI metric tests pass or fail with the host (x64, Win32 and I9's tree alike); the Win32 suite run and
  its re-run alone both read 2 failed. Not caused by S3.13 (no text-metric code is in its edits).
- The Win32 harness cannot run 5 x64-only files (gl_raster, terrain_gl_setup, terrain_render, terrain_state: `lib.dll` is None on
  Lib32; test_boot's quit test reads run_detail): they were never in the Win32 list (U-1100's class).
- I9's leftovers that S3.13 does not touch: the 10 NOT_RUN census rows (golfer_events has no trace), census FAIL rows with no
  unsupported call (S3 wave 6 / S3.9), the ledger row without a cause (showRoutingMapScreen.exitRemapReadsFrame, as at I7).
- Process: a background shell's 2-hour limit stopped the x64 suite loop while test_terrain_render ran; the pytest finished on its own
  (522 passed) and files 71-75 were run detached (13_suite_rest.sh with FROM=71, 13_g_suite_x64_c.log).

## 7. Files

log/roadmap/S3/13_files.txt (S3.13's own files) and log/roadmap/S3/files_w5b.txt (the whole resubmission: files_w5.txt + S3.13).
