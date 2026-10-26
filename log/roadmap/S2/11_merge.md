# S2.11 merge request: dialogs over the course, part 2 (tournament results, bulletin board, SimFoto / message ball)

Base: the S2.13 integration mirror `port/build/S2_13_head` = HEAD 2c05f02a + S2 waves 1-5 (MERGE.md .. MERGE_wave5.md: wave 5 =
13_merge.md). Everything was built and judged in the mirror `port/build/S2_11_head` (a robocopy of S2_13_head without its build
dirs and junctions, junctions re-created as log/roadmap/S0/export.md: log, original, port/corpus, re/replay, re/versions,
shim/build, tests/fixtures, tests/golden). Apply AFTER wave 5. Files: log/roadmap/S2/11_files.txt.

## 0. Base the patch targets (regenerated 2026-10-25 for wave 6)

11_shared.patch targets **b3d61e41 + wave 5 + wave 6**: 13_shared.patch with 13_merge.md's new files (port/test/test_gfx_reports.py among
them), then 10_shared.patch with 10_merge.md's new files. It was regenerated against port/build/S2_10_head (S2_13_head + S2.10) with
the combined mirror port/build/S2_11w6_head: S2_10_head plus this step's edits, 3-way merged with `git merge-file` (11 of 12 files clean;
gfx_families.inc, where both steps append, resolved as wave 6's `buyland`, `chareditor` lines, then `results`, `simfoto`, `bulletin`).
Hunks that moved: gfx_families.inc only; dialogs.cpp, gfx.cpp and tick.cpp (also touched by wave 6) merged without conflict.
Checks (11_patch_check.log, 11_patch_check_b3d61e41.log): on S2_10_head's copies `git apply --check` rc 0 and every result equal to the
combined mirror; on b3d61e41's copies (test_gfx_reports.py from wave 5's new files) with 13_shared.patch and 10_shared.patch applied
through --include, `git apply --check` rc 0 and 11 of 12 results equal to the combined mirror; tick.cpp differs only by S3.3's U-1120
hunks in b3d61e41 (the boot's last-frame NeedInput catch and gfx_end_of_input), away from this patch's one tick.cpp hunk.
Tests on a build of the combined tree (port/build/S2_11w6, x64; 11w6_suite.log): test_static 6, test_gfx_s2_10 4 + 9 skipped (S3 families absent), test_gfx_reports 36, test_gfx_dialogs2 27, test_gfx_dialogs 32, all pass. Copy the new files of section 1 from
port/build/S2_11w6_head (identical to S2_11_head's).
With U-1120 fixed, championship tournament_end can have a snapshot: test_gfx_dialogs2.py skips it as not judged yet (U-1218).

## 1. New files (copy from the mirror, `port/build/S2_11w6_head/<path>` -> `<path>`)

| file | what |
|---|---|
| port/src/sim/gfx_results.cpp | family `results`: drawStringRightAt, results.begin (marker + probe); negctl 1, 2 |
| port/src/sim/gfx_simfoto.cpp | family `simfoto`: loadInterfaceArt.ballArt, drawWindow.golfer, simfoto.begin (marker + probe); gfxs::probe_fill (used by results); negctl 1-4 |
| port/src/sim/gfx_bulletin.cpp | family `bulletin`: loadInterfaceArt.bulletinArt, showBulletinBoard.board / .event / .photo / .fixed; negctl 1-4 |
| port/test/test_gfx_dialogs2.py | the gate (17 rectangles of 13 checkpoints), 9 frame controls, the ball-cut control, handled-bodies tests |

Copy them with item 2 (gfx_families.inc names the three families; a gfx_<f>.cpp missing from it fails test_static).

## 2. Shared-file edits: log/roadmap/S2/11_shared.patch

One patch (LF), `git apply log/roadmap/S2/11_shared.patch` from the repo root after wave 5. Made and checked by
`~/bin/py -3.12 log/roadmap/S2/11_tools/make_patch.py <scratch> --wave6` (diff of port/build/S2_11w6_head against port/build/S2_10_head; `git apply
--check` and an apply on S2_13_head's copies give the mirror's files exactly: 11_patch_check.log). Hunks only in this step's functions:

| file | change |
|---|---|
| port/src/sim/gfx_families.inc | `SG_GFX_FAMILY(results)`, `(simfoto)`, `(bulletin)` after wave 6's buyland / chareditor |
| port/src/sim/gfx.cpp | b_draw_window: sel != -1 hands the call to `drawWindow.golfer` (5 slots) instead of reporting it; draw_message_buffer: a white line with H > 0x10 is drawTextSurface's (bottom clamp, shadow in run-palette entry 1 one pixel down, U-1230) instead of a reported plain line; two comments |
| port/src/sim/commands.cpp | window_height keeps the unclamped height in `cm::t_window_h_raw` (thread_local, no GameState); new `draw_window_gfx` (the drawWindow.frame drop, a backend only) and `draw_window_gfx_saved` (the same with the window-height pass run and g_textBuffer / g_textBuf / g_ui_821c50 / g_value587ea0 put back); registry rows `commands.windowHeightRaw`, `commands.drawWindowGfx` (internal) |
| port/src/sim/commands_layout.inc | tick_message: `draw_window_gfx(ctx, 0xf0, -8, v)` after draw_window_effect |
| port/src/sim/tick_leaves.inc | drawWindow: the drawWindow.frame drop gets a fifth slot (the unclamped height through commands.windowHeightRaw) |
| port/src/sim/shots.cpp | narrateShot: with a backend attached (only) the ball (`commands.drawWindowGfx(0x96, y, -2)`, y as the original) and the SimFoto frame (simfoto.begin, four fillWindowRect, four Widget_fillRect, the caption, the date by drawTextSurface, Window::refreshFull); include portsim/sim/gfx.h; a Foreign row |
| port/src/sim/dialogs.cpp | show_bulletin_board: the board's drawing calls (showBulletinBoard.board, per event setDrawTarget / .event / drawText4049d0 / .photo, then .fixed and Window::refreshFull) through gfxr::ScreenDraw; include gfx_reports.h |
| port/src/sim/menu.cpp | after loadInterfaceArt.dialogArt: the drops loadInterfaceArt.ballArt and loadInterfaceArt.bulletinArt |
| port/src/sim/tick.cpp | declarations of gfx_sprite_cut (gfx_reports.cpp) and results_control (gfx_results.cpp) in namespace portsim |
| port/src/sim/tick_panels.inc | showTournamentResults: SprW / SprH (the backend's cut size with a backend, else the GameState word as before) for the row steps, the close button's y and its hover box; the results.begin marker before the scorecard header |
| port/src/sim/tick_frame.inc | P5_simfoto: the simfoto.begin marker (a backend only) before the first fillWindowRect |
| port/test/test_gfx_reports.py | S2.8's control 8 (Surface::callBounds leaves the clip narrowed) no longer lists hole_stats: its reading there was 8 one-level blended border pixels of the hole-stats panel over the roster screen left in the root; with the bulletin board drawn, the root under the panel is the board and the control changes no judged pixel there (11_ctl8.log); still RED on its other 5 checkpoints |

Without a backend attached every new call is a no-op, so the GameState of every run without one is unchanged; with one, nothing
the backend does writes the GameState (narrateShot's window-height pass is put back, the row steps change only drawing positions:
11_notes.md section 2). Overlaps to know about when S3 merges: roadmap S3.6's working-tree gfx_hud.cpp also registers
`drawFrameR3` / `drawFrameS2` / `Surface_drawScaled` / `loadInterfaceArt.images` (none of them is registered by this step; the head of
the message ball is held for it, U-1216).

## 3. UNCERTAINTIES.md

Append the 5 rows of log/roadmap/S2/11_uncertain.tsv (U-1215..U-1219).

## 4. CLAUDE.md paragraph (after the S2.13 paragraph)

(see log/roadmap/S2/11_notes.md, section "CLAUDE.md paragraph")

## 5. Gate commands after the merge (repo root, port/build/x64)

```
~/bin/py -3.12 -m pytest port/test/test_gfx_dialogs2.py -q -p no:cacheprovider
env SG_BACKEND=win32 ~/bin/py -3.12 -m pytest port/test/test_gfx_dialogs2.py -q -p no:cacheprovider
~/bin/py -3.12 -m port.test.test_gfx_dialogs2 log/roadmap/S2/11_rects.tsv        # the per-rectangle table
```
