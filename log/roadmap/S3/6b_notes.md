# S3.6b notes: S3.6 rebased onto HEAD + I6's waves, and the two employee panels drawn

Worker session cmvnl56h87pi9o115n0mcwqrj (claude2, spawned for claude3, routed by the folder flag), lead
cmvmns8sn3fsbo115g4dgm1nf. Step row S3.6b of re/roadmap/S3_steps.tsv; S3.6's notes are 6_notes.md, its merge 6_merge.md.

## 1. Rebase

Base: the integrator I6's mirror port/build/I6 (git archive of HEAD b3d61e41 = S2 waves 1-4 + S3 wave 1, with S6 wave 2,
S3 waves 2 / 3 and S2 wave 5 applied; I6 had not committed when this step ran). Its port/ is frozen once into
port/build/S3_6b_src/_i6port (sha256 in S3_6b_src/I6_SNAPSHOT) because I6 may still edit its tree.

    python log/roadmap/S3/6b_mirror.py [--w32] [--no-build] [--base] [--recopy] [--own-only]

copies that port/ to port/build/S3_6b_src/port (every other top-level entry a junction to I6's), the six own files, the
two family lines (hud, pixel after routing) and 6_edits.py's EDITS (all anchors matched once), then builds
port/build/S3_head2/port/build/S3_6b (x64) and S3_6b_w32 (Win32). `--own-only` recopies only the own files (a running test
keeps its tree). Both builds compile without warnings. Python here is the full path of python.exe: `~/bin/py` is a sh
script PowerShell cannot run (a first call hung and was stopped).

What the committed tree changed for S3.6:
- S2 wave 5's `reports` family registers Widget_fillRect: 6_merge.md section 2 is done in gfx_pixel.cpp (the pixel family
  now registers Widget_writePixel only; control 2 left with the body; test_gfx_pixel.py already switched to reports mode 5).
- S3.2's terrain is drawn into the root every frame: the HUD opaque measure counted it (16 of 23 hud tests failed on the
  rebased build before the change, 6b_pytest_hud_pre.log). The gate copy log/roadmap/S3/6b_hudgate.py and the tests' fill
  runs take framegate --no-live (U-1383).
- S3.4's FLIC decoder cuts every sheet cell into the backend (about 47,000 SpriteRefs of sandbox_basic), so the cut
  geometry control 3 (`the art loads do nothing`) compared more than 1,000 equal records and read GREEN; it now reads RED
  when an interface-art SpriteRef of the plain run is missing (test_cut_geometry_negctl_red).

## 2. The employee panels (U-1359)

gfx_hud.cpp b_view_panel (drawEmployeeViewPanel.draw, c3fb.cpp DrawEmployeeViewPanel_re) and b_detail_panel
(drawEmployeeDetailPanel.draw, c3fc.cpp DrawEmployeeDetailPanel_re); each body's comment is its specification. Points that
matter:
- commands_layout.inc hands the drawing over BEFORE its own writes. Values the original reads after its own update are
  recomputed: the detail panel's scroll step (drop by 2 while positive and the previous count below 7), this frame's list
  (the original rebuilds g_employeeDetailList before it reads the selected record), the count for the full image.
- Texts: buildGolferName / appendWalkerName through g_textBuffer with only that buffer saved and put back (buffer_text);
  appendDate / appendNumber (they also store g_year) through S3.6's scratch_text (whole GameState saved); the `__itoa` numbers
  are formatted directly (the original writes them into g_panelNumberText, which the port's layout does not).
- Reported, not drawn: the detail panel's employee figures (addSprite, S3.5's) and the selected employee's circle (drawCircle,
  25 lineTo segments, S3.8's); both are called with the original's arguments when the tree holds a body (call_or_report;
  drawCircle with g_zoomScale = g_ui_5a34e4[1] around the call) (U-1382). The dwell tips are tooltips (S3.7), counted with
  S3.6's InfoPanelState.
- Controls (g_negctl_gfx_hud): 11 the golfer list's names one pixel right, 12 its scroll bar 7 px tall, 13 the detail panel's
  list-slot images one pixel lower, 14 its text block one pixel lower, 15 its no-selection buttons take the selection
  branch's images.

## 3. Evidence for the panels

No pinned checkpoint frame holds the golfer list or a selected employee (U-1380, U-1381): the five scenarios that reach
drawEmployeeViewPanel carried its reason only because a run's unsupported list is cumulative; their reference bottom panels
(log/roadmap/S3/6b_sheet_refs.py) show the action panel, scoreboard, palettes, the empty detail panel or S2 screens.

- EMULATOR CALL ORACLE (port/test/test_gfx_hud.py test_panel_calls_equal_the_original, opt-in SG_EMU_ORACLE=1): the ORIGINAL
  function under Unicorn (port/test/cmdemu.py) on segment start images of cov_b_holes18, cov_a_play and ui_tour with the
  panels' inputs varied (zoom, scroll, golfer status / partner / mood / colour words / pips, crowds and empty courses, partner
  pairs, employee records, a full staff, the selection, the hire trigger, the cursor over every button and slot, the dwell
  counter), every drawing callee a recorder with its own `ret N` (Surface_stretchBlit, Image_draw, Surface_drawScaled,
  setDrawTarget, drawText4049d0, drawCenteredText, fillWindowRect, addSprite, drawCircle with the zoom word, drawMapLine /
  drawPanelBackdrop as `tooltip`), the hit tests, clamp, cellAt and the text builders real, hireEmployee / playSound stubbed;
  the port's bodies log the same calls (TEST-ONLY gfx_hud.recordPanels). Result (log/roadmap/S3/6b_emu_stats.log): view
  240 / 240 equal (3..498 calls per case; drawText in 175, scroll bar in 215, tooltip 3), jcc sites 38 of 41 both sides;
  detail 239 / 239 equal (1 case faults in the original: pickGolferName reads an unmapped name pointer of a varied record;
  text block 169, addSprite 202, drawCircle 170), 37 of 39 sides. The sides not taken are argued in U-1384.
- Controls on the oracle (test_panel_calls_negctl_red): 11 differs in 175 / 240 cases, 12 in 215, 13 in 201 / 240, 14 in 169,
  15 RED; 11 / 12 change no detail case and 13 / 14 no view case (each is the other panel's).
- Default suite: test_panel_calls_golden pins sha256 digests of the port's logs of the first 60 cases per panel (pinned after
  the oracle was GREEN; SG_EMU_ORACLE=update prints new ones); controls 11..15 change them (test_panel_calls_golden_negctl_red).
- test_employee_panels_leave_the_gamestate_unchanged: both bodies on cov_b_holes18 / cov_a_play start images (an employee
  selected) leave every GameState byte as found.
- FRAME: ui_tour `people` (the detail panel, no selection, no employee) bottom panel 0 of 59,956 opaque px differ; control 15
  RED on it (test_hud_regions_negctl_red).

## 4. Gates (rebased build, final)

Commands run from port/build/S3_6b_src (python = the full path of python.exe; SG_PORTSIM_DLL a copy of the x64 build).

- port/test, one file per process (`log/roadmap/S3/6b_run.py [--w32] --log <log> <files>`): x64 test_gfx_hud 34 passed + 7
  skipped (the opt-in emulator tests), test_gfx_pixel 17, test_static 6, test_gfx 25 (6b_pytest_x64.log); Win32 test_gfx_hud
  34 + 7 skipped, test_gfx_pixel 17, test_gfx 25 (6b_pytest_w32.log). Opt-in, x64: SG_EMU_ORACLE=1 test_panel_calls_equal x2
  and test_panel_calls_negctl_red x5 passed (log/roadmap/S3/6b_emu1.log; 6b_emu_stats.log for the numbers of section 3).
- Segments (`python -m port.test.integrator segments --dll <dll> --jobs 4`, Win32 `--backend win32 --dll32 --runner32`):
  205 / 205 PASS on both, 0 status / digest differences from the committed tree's tables (log/roadmap/merges/S2S3-5/
  segs_*.tsv) and x64 = Win32 (6b_segs_compare.log).
- Boot (`python -m port.test.boot --all --jobs 3`): HEAD's 61 at 58 PASS / 2 KNOWN / 1 NO_FRAMES, 0 of the committed
  tree's 70 rows differ (log/roadmap/S3/6b_boot_compare.py, 6b_boot_compare.log).
- cov_r_flic_input: both in-game rows (hud, end) now run (boot 75 / 75) with no unsupported reason; framegate FAIL on pixels
  only (view 107,516 px beyond one level: terrain under G2 plus the scene S3.5 owns), HUD regions 0 bad opaque px
  (6b_flic_input.tsv, 6b_gate).
- HUD opaque gate (6b_hudgate.py, --no-live) over S3.6's 28 gate scenarios, cov_r_flic_input and the five panel scenarios
  (6b_gate/, 6b_g2/; `6b_gate_compare.py` against 6_gate / 6_gate2, 6b_gate_compare.log): 368 in-game rows, 368 clean (S3.6:
  321 of 338), 310 rows equal to S3.6's, 28 changed, none worse in any region; bad opaque px fell from 40,582 / 53,233 /
  253,096 / 425,779 (title, meters, toolbar, bottom panel) to 39,486 / 50,369 / 126,782 / 185,639, mostly S2 wave 5's dialogs
  and report screens now drawn (cov_a_keys k_dlg_J, cov_a_career career_editor, cov_b_shots* results). ui_tour / ui_tour_v
  people: bottom panel 1,053 -> 0. The residual groups are S3.6's attributions (6_notes.md section 4: S2 screens, S3.8 tip
  bars and grid lines, S3.5 hole flag and amenity items).

## 5. Open

U-1380 (no pinned golfer-list frame), U-1381 (no pinned selected-employee frame), U-1382 (addSprite / drawCircle to S3.5 /
S3.8), U-1383 (the HUD measure leaves the terrain out), U-1384 (argued oracle sides). S3.6's U-1350..U-1358 unchanged;
U-1359 resolved (6b_merge.md section 2).

## 6. Files

log/roadmap/S3/6b_files.txt. Builds (not committed): port/build/S3_6b_src (source root), port/build/S3_head2/port/build/
S3_6b, S3_6b_w32, S3_6b_run, S3_6b_g1..g3 (DLL copies the gate runs loaded).
