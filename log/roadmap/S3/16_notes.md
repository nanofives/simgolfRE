# S3.16 notes: bottom_panel residue and 2D pixels beyond one level in the view

Worker: S3.16 (claude2 opus). Base: port/build/S3_16_base = `12_assemble.py --dst S3_16_base --commit ee6efc1d` (the tree
I15 gates). Mirror with the fixes: port/build/S3_16 (x64 build; `sh log/roadmap/S3/12_build.sh S3_16 x64`). Edits:
16_edits.py (40 anchored drawing edits in dialogs.cpp, shots.cpp, tick_leaves.inc, tick_ball.inc, commands_panels.inc, plus
the new file 16_new/port/test/test_gfx_info_panel.py), 16_snapshot_edits.py (5 anchored edits of the gate harness: gfx.py,
framegate.py; the lead's MERGE_w7 section), 16_assemble.py (`--only drawing|snapshot`, `--root <tree>`), merge text
16_merge.md, U-rows 16_uncertain.tsv (U-1820..U-1825). Heavy runs took the wave 7 token (W7_HEAVY_RULE.md) each time.

## 1. What the I14 residue is

Tools (all light, one scenario at a time): `16_diff.py` (clusters of differing px, one-level / beyond), `16_classify.py`
(split by the scene family's TEST-ONLY mask run `--negctl gfx.family.scene=7`), `16_terrain_cmp.py` (the port's terrain-only
root against S3.2's recordings log/p5/terrain/s32_*), `16_qcmp.py` (the port's own renderSpriteQueue results around a
checkpoint against the original's queue corpus), `16_snapsweep.py` (the port's back buffer at a list of clock ordinals),
`16_report.py` (framegate TSV rows against I14's census), `16_crop.py` / `16_sheet.py` (images).

- bottom_panel 26 / 29 / 45 / 50 px (one level): TERRAIN, not a 2D body. On course_season sandbox, cov_k1_river r00 and
  cov_a_build2 buildings every bottom_panel px is outside the scene mask, and the same pixels differ between the port's
  terrain-only root and the original's (s32_course_season sandbox exit: 46 bottom_panel px one level, 0 beyond). They gather
  in vertical bands x 656..688 and 784..800 (and x 0..16) over the whole frame height. G2 holds; left as U-1821 (U-1392's
  residue). No edit.
- view beyond one level: five causes found and fixed (section 2), one harness cause (section 3), the rest in U-1824.

### 1b. Terrain split (S3.15's log/roadmap/S3/15_terrsplit.py, unedited; log 16_terrsplit.log; mirror with every fix)

T = the terrain exit already differs and nothing 2D drew there, TB = the terrain exit differs and a 2D body drew over it, B = the
terrain exits are equal (made after updateTerrainRender: 2D). Per pattern, px left after S3.16's fixes:

| row | bottom_panel T / TB / B | view T / TB / B | 2D-made (B) that is left |
|---|---|---|---|
| cov_l_camera step1 | 0 / 0 / 0 | 0 / 0 / 0 | none (PASS) |
| cov_a_build2 buildings (29/29 pattern) | 29 / 0 / 0 | 43 / 4 / 0 | none |
| course_season sandbox (45/45 pattern) | 45 / 0 / 0 | 46 / 5 / 0 | none |
| championship tour_0180 | 13 / 2 / 0 | 64 / 5 / 1 | 1 view px (the arc's end) |
| cov_a_golfer panel_1 | 0 / 0 / 0 | 37 / 5 / 4 | 4 view px |
| cov_a_play end | 0 / 0 / 0 | 38 / 5 / 0 | none |
| golfer_stories season_end | 0 / 0 / 0 | 38 / 6 / 289 | 289 (U-1824 ellipse) |
| cov_k3_camera y_inj3 | 0 / 0 / 1 | 90 / 2 / 846 | 846 (U-1824 red line) + 1 bp |
| cov_k3_camera2 y_inj4 | 0 / 0 / 0 | 77 / 0 / 182 | 182 view (fountain) + 232 toolbar (S3.15's region) |
| cov_b_inject zoom_box | 0 / 0 / 0 | 0 / 0 / 27 | 27 view + 4 hud_meters (U-1824) |

So the bottom_panel patterns 26 / 29 / 45 / 50 have no 2D-made px (all T); the 2D-made view px left are the U-1824 rows.
(cov_k1_paths has no S3.2 recording; its cart row is section 3's.)

## 2. Port fixes (drawing only: no GameState byte is written by any of them)

1. tick_leaves.inc drawLabels / drawMoneyLabels (c3dv.cpp DrawLabels_re / DrawMoneyLabels_re): the setDrawTarget(text
   object, g_font51b360, 0, 0, 0) + drawCenteredText2(g_textBuffer, sx, sy - 4, colour) calls were replaced by `eax = 0`, so
   the floating text labels and money popups never reached the backend. Colour: g_labelQueueA[k] / 0x80007d08 for an amount
   <= 0, else 0x800023e8. Both are drops; the drop returns 0 as before. cov_l_camera step1..9 227 -> PASS, cov_a_build2
   buildings 975 -> 0 beyond, cov_k2_build bulldoze 511 -> 0. Controls drawLabels 2 (one pixel lower) RED 903 beyond,
   drawMoneyLabels 2 (sign colours swapped) RED 173.
2. shots.cpp narrateShot (c3fr.cpp NarrateShot_re): the shadow line (setLinePoint(sx, sy + 1) / lineTo(sx, sy + 1, 0x80000000,
   2, 7)) and the flight arc (lineTo(sx, sy, 0x80007fff, 2, 7)) of the SimFoto frame were dropped. championship tour_0180 297
   -> 1 beyond. Control narrateShot 3 (arc one pixel lower) RED 207.
3. dialogs.cpp draw_golfer_info_panel (c3fr.cpp GolferPanel_re): the whole panel was undrawn. Added in the original's order
   through SkDraw.gx (gfx only): Surface_blit2 + Surface_stretchBlit of the panel (not story) or Image_draw + the story band,
   the portrait frame and thought_bubble(g, -1, 0xac, -2, draw), the name / status / traits / opinion / likes lines, the five
   meters (fillWindowRect bands), the scorecard (par colours), the story page (Widget_fillRect, title, drawText4049d0 lines
   per redo of the fit loop, buttons, partner bubble, tooltip drawPanelBackdrop) or the thoughts page (buttons, tooltip, up
   to five drawTextSurface lines with the Text_put > 0x180 choice measured by TextMetrics with g_font51b360). Patch script
   16_patch_panel.py. cov_a_golfer panel_0..3 / tee / shift_tee / button_3 / strip, cov_a_keys2 C_overwrite, cov_a_keys3
   panel, cov_r_bio panel: about 100,000 px beyond -> 0 (cov_r_bio exit stays UNSUPPORTED on the bio EditBox, S3.18).
   Controls drawGolferInfoPanel 6 (name line one lower) RED 1,070, 7 (meters one lower) RED 800. test_dialogs 27 passed.
   No body is registered (gfx.check unchanged): the panel calls the existing bodies (Surface_blit2, Surface_stretchBlit,
   Image_draw, Surface_drawScaled, Win_fillRect, setDrawTarget, drawCenteredText / 2, drawText4049d0, drawTextSurface,
   fillWindowRect, Widget_fillRect, drawPanelBackdrop, composeGolferSprite).
   CALL ORACLE (new port/test/test_gfx_info_panel.py, SG_EMU_ORACLE=1): the ORIGINAL drawGolferInfoPanel under Unicorn on
   test_dialogs.py's panel cases (record byte +0x18 = 0: the skill dialog's pass is S2.10's), every drawing callee a recorder,
   drawThoughtBubble real; the port with a backend logs every SkDraw call (TEST-ONLY dialogs.recordDraw / dialogs.drawLog):
   232 of 300 cases ran in the original (68 fault on varied records), all 232 equal, 21,247 calls (drawCenteredText 8,019,
   setDrawTarget 7,312, fillWindowRect 2,320, Surface_stretchBlit 1,389, drawText4049d0 866 = the story page, drawCenteredText2
   464, Surface_drawScaled 321, drawTextSurface 183 = the thoughts page, Surface_blit2 121, Widget_fillRect 118, Image_draw 111,
   drawPanelBackdrop 23 = the tooltips). Default: golden digest 299d474290a0d26a over 120 cases, controls 6 and 7 RED against
   it (16_info_panel_oracle.log, 16_info_panel_default.log: 3 passed + the oracle skipped).
4. tick_ball.inc drawFlyingBall: composeGolferSprite(0x99) is a registered function (its look-index write-back), so its
   palette layers never reached the backend and the preview golfer's addSprite(.., tag | 0x200) drew with the previous
   golfer's palette. Now also gfx_drop("composeGolferSprite", 0x99) with a backend. cov_a_play end / n_yes 134 / 107 -> 0.
   Control drawFlyingBall 2 (skipped) RED 134.
5. commands_panels.inc help_window (c3em.cpp ClickSlotPanel_re / ClickAmenityPanel_re / ClickBuildTool / PickBuilding):
   drawWindow(0xc8, 0x154, 0x80007fff, -2), Window::refreshFull(root, 0, 0), pumpAndRedraw(0). The port dropped the refresh
   (the window never reached the back buffer during the wait) and clickSlotPanel / clickAmenityPanel passed the ticker's
   position (0xf0, -8). cov_r_palette_hot undo_help 46,200 -> 0. Control clickSlotPanel 3 (refresh skipped) RED 46,200.
   tick_leaves.inc canAfford: the same refresh after drawWindow(0xc8, 0x96, ..) (c3em.cpp CanAfford_re); no pinned
   checkpoint waits there (U-1823, control canAfford 2 unjudged).

## 3. Harness fix: the checkpoint's clock call (U-1820)

framegate.py and gfx.checkpoint_ordinal took the status.tgt-th timeGetTime call as the checkpoint's clock call.
TRACE EVIDENCE (16_poll_evidence.txt, all 7 rows): the checkpoint's POLL record has the same position as the frame's FRAME
record (the frame head), and the clock call at that ordinal is jgld's QueryPerformanceCounter (return address 0x8106a414,
kind 2); status.tgt's call is H1's g_frameMs timeGetTime (return address 0x41e641), 10 to 86 QPC calls later, after the frame
body and its present:
  cov_k1_paths cart: POLL index 457 pos 64805 = FRAME pos 64805; QPC 64805..64814 (10); status.tgt 46359 = ordinal 64815 at 0x41e641
  cov_k1_river r20: POLL index 151 pos 30599 = FRAME pos 30599; QPC 30599..30608 (10); status.tgt 15453 = ordinal 30609 at 0x41e641
  cov_k1_routing end: POLL 67463 = FRAME 67463, tgt ordinal 67473; cov_a_keys3 season: 389471 / 389557 (86 QPC); cov_l_bridge
  obj5: 42364 / 42374; cov_a_flags zdemo: 103331 / 103397 (66 QPC); cov_a_flags ztut: 68382 / 68392.
So the original's checkpoint is taken at the frame head, before that frame's body draws (its back buffer is the previous
frame's present), which is what the reference shows. ROWS WHOSE SNAPSHOT MOVES (a scan of every census scenario's POLL records
against status.tgt): exactly these 7, all FAIL in I14 (cov_a_flags ztut 89 / zdemo 2,068, cov_a_keys3 season 1,588,
cov_k1_paths cart 1,421, cov_k1_river r20 2,010, cov_k1_routing end 1,394, cov_l_bridge obj5 2,648 beyond px in I14).
ANCHORS (16_snapshot_edits.py): gfx.py `import json` / `import pathlib` (adds `import os`), the two lines `tr = TF.read(...)` /
`k = 0` of checkpoint_ordinal and its `raise ValueError(... tgt call ...)` / `return ordinal`; framegate.py run_scenario's
`k += 1` / `summ = json.loads(...)` (the POLL table) and `o = tgt[st["tgt"]][0]` / `meta[c] = dict(classify(tr, o), ...)`.
None touches the argument parser (main()). The trace's
POLL record holds where the poll really rode: for 7 census rows (the only ones of the 64 scenarios whose POLL lies before status.tgt's call) it is a jgld Palette::update QPC call at the start of a frame
body that makes no timeGetTime read before H1's wait, and the tgt-th call is H1's g_frameMs read of that frame (offset 0 in
all 7; every other H1WAIT row is at offset >= 1 and has at most 2 px beyond). Measured on cov_k1_paths cart with
16_snapsweep.py: the port's back buffer at ordinals 64790..64810 equals the reference in every pixel (0 beyond), from 64815
(the old P) on it holds the next sprite frame (1,421 beyond); 16_qcmp.py: the original's queue capture at the scene ordinal
equals the reference and the port's own capture there, the old snapshot equals the port's next render. Now the POLL's
position is used when it lies before the tgt call (SG_GFX_CHECKPOINT_AT=tgt restores the old mapping: control RED, view 43
one-level -> 1,385 beyond). Fixed rows: cov_k1_paths cart 1,385 -> 0, cov_k1_river r20 1,973 -> 1, cov_k1_routing end 1,372
-> 0, cov_a_keys3 season 1,559 -> 0, cov_l_bridge obj5 2,648 -> 0, cov_a_flags zdemo 750 -> 2 / ztut 53 -> 0. A frame-head
snapshot trigger in tick.cpp was tried first and changed nothing on 226 rows; it was reverted.

## 4. Numbers (light framegate runs per scenario on the mirror, x64; I14 census as the reference)

Rows re-run: 31 scenarios (16_out/f2..f9_*.tsv, 226 rows in the f7 set alone), 0 rows lose PASS, 0 rows with more differing
px than in I14, rows gaining PASS include cov_l_camera step1..9. bottom_panel: unchanged (terrain, U-1821). The harness change
moves the clock call of exactly 7 census checkpoints (a scan of all 64 scenarios' POLL records against status.tgt), all of
them FAIL rows before; the port edits only add drawing calls made with a backend attached, so the segment and boot gates
(no backend) cannot see them, but they are not re-run yet (section 6).
Win32 (port/build/S3_16/port/build/Win32, SG_BACKEND=win32, 16_out/w32_*.tsv): cov_a_golfer panel_1, cov_l_camera step1,
cov_r_palette_hot undo_help, cov_k1_paths cart, championship tour_0180, cov_a_play end, cov_a_build2 buildings: every region's
px equal to x64.
Port test files (x64, one process per file, 16_tests.log / 16_tests_b.log): test_static 6, test_dialogs 27 + 7 skipped,
test_commands 83 + 44 skipped, test_shots 33 + 1, test_gfx 25, test_framegate 32, test_gfx_eoi 5, test_gfx_hud 34 + 7,
test_gfx_s2_10 13, test_gfx_scene 118, test_gfx_text 32 + 20, test_terrain_overlay 12 + 6, test_gfx_dialogs 32,
test_gfx_dialogs2 26 + 1, test_gfx_reports 24 (12 deselected), test_tick 81 (169 deselected): all passed.
Controls (16_out/ctl/c1..c8.tsv): 8 / 8 RED (section 2 and 3); canAfford 2 unjudged (U-1823).

## 5. Open (U-1824)

golfer_stories season_end 289 (translucent ellipse), cov_k3_camera y_inj3 759 (red line to the amenities panel),
cov_k3_camera2 y_inj4 144 (fountain frame), cov_b_inject 15, cov_r_s2_screens course 11..21 (screen-edge triangle),
cov_r_s2_screens hire 1,394 (S2.8's animated employees).

## 6. Gates (after I15, 2026-10-29 22:32-23:5x, wave 7 tokens; runner 16_gates.sh, log 16_gates.log / 16_gates2.log)

- Win32 rebuild of the mirror (16_build_w32.log, 0 errors).
- CENSUS with the snapshot fix (`framegate --all --jobs 1`, 16_census.tsv, 698 rows): PASS 109 / FAIL 528 / UNSUPPORTED 51 /
  NOT_RUN 10 against I14's 100 / 537 / 51 / 10. Against I14 (16_census_cmp.py, 16_census_cmp.log): 0 rows lose PASS, 0 rows with
  more differing px, 38 rows with fewer, PASS gained cov_l_camera step1..9.
- CENSUS without the fix (`SG_GFX_CHECKPOINT_AT=tgt`, 16_census_tgt.tsv): the six scenarios holding the 7 checkpoints whose POLL
  lies before status.tgt's call (every other checkpoint has the same clock ordinal in both modes), 110 rows. Exactly 7 rows differ
  between the two snapshot positions, all FAIL in both, all better with the fix (total px / beyond one level):
  cov_a_flags zdemo 3,070 / 2,068 -> 112 / 2; ztut 231 / 89 -> 107 / 1; cov_a_keys3 season 2,475 / 1,588 -> 101 / 0; cov_k1_paths
  cart 2,133 / 1,421 -> 102 / 0; cov_k1_river r20 4,058 / 2,010 -> 270 / 1; cov_k1_routing end 2,057 / 1,394 -> 102 / 0;
  cov_l_bridge obj5 3,065 / 2,648 -> 72 / 0.
- Segments x64 205 / 205 PASS and Win32 205 / 205 PASS (16_segs_x64.tsv, 16_segs_win32.tsv; the Win32 run was going when the lead
  stopped the queue and finished). Boot x64 / Win32 were dropped on the lead's order: S3.9 runs segments / boot on the combined wave 7 tree against plain 37e58e96.
