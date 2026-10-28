# S3.7 notes: marked-up HUD text, the ticker window, the hover tips (U-1069, U-1072, U-1353)

Worker session cmvnprucy848bo1156wzjx7p7 (claude2), lead cmvmns8sn3fsbo115g4dgm1nf. Step row S3.7 of re/roadmap/S3_steps.tsv.

## 1. Build and run roots

`log/roadmap/S3/7_mirror.py [--w32] [--no-build] [--base] [--own-only] [--src <root>] [--resnap]` makes a source root under
port/build (default S3_7_src) from:
- the I6 integrator's port/ (port/build/I6 = HEAD b3d61e41 + S6 wave 2 + S3 waves 2 / 3 + S2 wave 5), frozen once into
  port/build/S3_7_src/_i6port (port tree sha256 75ee7cef..., the same snapshot S3.6b took);
- S3.6 / S3.6b's six files (gfx_hud.*, gfx_pixel.*, tests) and S3.5's three (gfx_scene.*, test), frozen from the working tree
  into S3_7_src/_s36 (sha256 in S3_7_src/I6_SNAPSHOT: S3.6b was still editing gfx_hud.cpp), their family lines, 6_edits.py,
  6b_edits.py and 5_merge_edits.py (EDITS without the families line, EDITS_AFTER_S2W5, EDITS_HUD);
- unless --base, this step's files and log/roadmap/S3/7_edits.py.
Builds: port/build/S3_head2/port/build/S3_7 (x64), S3_7_w32 (Win32 + runner), S3_7_base (--base). Both builds compile without
warnings. LESSON: a run reads its root's port/test and port/corpus junction, so a rebuild (which recreates the root) must never
target a root a run is using: the base census ran from S3_7_srcbase, the gate runs from S3_7_run (each made with --src), and
the DLLs a long run uses are copies (port/build/S3_7_dll/d1..d3). A first base census lost 35 scenarios when the dev rebuild
removed its corpus junction (rerun with --resume after deleting those rows).

## 2. What was there and what was missing (census, base build)

`framegate --all --jobs 2 --no-probe` on the base build (log/roadmap/S3/7_base/census.tsv.d, 698 rows): this step's unsupported
reasons over the 600 in-game rows: showInfoCursorPanel's tool tip 383 (a run's list is cumulative: rows where it fired before),
Text_put 332, drawAmenitiesPanel's dwell tip 95, the action-panel tip 23, the detail / view panel tips 21 / 14,
drawStringRightAt 14, drawCenteredText2 markup 14, the slot tip 5, the building row 3 (`log/roadmap/S3/7_reasons.py`). Not
reported at all: THE TICKER. tickMessage's drawWindow call goes through commands.cpp's draw_window_effect, which handed nothing
to the backend, so the in-game message window (and every help window) was missing from the frames: championship tour_0060
hud_message 1,586 px, the tutorial pages, the help windows (cov_k1_label, cov_b_title) 70,000 to 83,000 px of the view.

## 3. The text family (port/src/sim/gfx_text.cpp, gfx_text.h)

Specification and evidence per part are in the file's comments (each cites its hook source). In short:
- THE ROOT TEXT OBJECT is the Image at g_rootWindow + 0x274; Image_ctor gives it markup ON (+0x40 = f_cfg_4e4210, 1 in the
  user's exe: read at 0x00474b7c), the style colours 1..3 and every shadow -1, offsets 2. So every frame text is marked up.
- drawRichText (scanMarkupText / skipToken / scanToken, `{ } [ ] $ = ^`, doubled braces, link and dropdown tokens, link
  targets, the persistent style words), drawTextRun (style fonts and colours, the baseline, the shadow run, tab stops of 30,
  '_' as a space, the run palette), drawMarkupRun / Text_put widths; the core's draw_text_string hands markup / tab / palette
  texts to it (merge edit 2), so every family's text calls get the markup.
- drawWindow (`drawWindow.frame`, the whole function: golfer portrait and backdrop, picture windows, the slide-in, drawFrameR3 /
  drawFrameS2 / drawWindowFrame, the centring by h) with drawWindowFrame and drawMessageBuffer (per-character Text_put widths,
  the break rules, including the original's redraw of the rest of the buffer when a break index lies behind the line start);
  commands.cpp's draw_window_effect now hands it over (merge edit 4).
- the hover tips: showInfoCursorPanel's tool tip (transcribed from the instructions, the function is C2:
  log/roadmap/S3/7_asm_showInfoCursorPanel.txt) and slot tip, drawTerrainToolbar's, drawGolferActionPanel's, the employee
  panels', drawAmenitiesPanel's dwell tip and building row (merge edit 5 in S3.6's gfx_hud.cpp).
- drawStringRightAt, Widget_setQuad70 / 74 / 78, Text_put as a measuring drop (merge edit 7: the frame code gets the width).
- Not drawn, settled or reported: the image cursor and the widget tooltips (U-1360, which settles U-1069's cursor and tooltip
  parts), the drawMapLine bars (S3.8, U-1367), dropdown combos (U-1366), the link-count message box (U-1363).

Run-palette entries (U-1072, U-1361): drawMessageBuffer takes drawTextSurface's shadow only for H = Widget_value - 4 above 16;
in every segment start image g_font51b360 has +8 = -1, +0xc = 19 (H 15) and g_fontRef_519fd8 +0xc = 13 (H 10), so drawWindow's
and the tips' texts never draw a shadow; g_font519948 (+0xc 31, H 27: the boot screens S2.7 judged) does.

## 4. Oracles and controls

port/test/test_gfx_text.py:
- TEXT CALLS (opt-in SG_EMU_ORACLE=1): the ORIGINAL drawText4049d0 / drawTextSurface / drawCenteredText / drawCenteredText2 /
  drawMessageBuffer / Text_put under port/test/cmdemu.py down to a fake jgld surface (vtable slots record setFont, the colour
  calls, the draws, end), fake fonts with a scripted width rule, a fake run palette, display and surface size; the port's
  record mode (gfx_text.test op 1) logs the same calls. 400 random cases (every delimiter, doubled braces, link / dropdown
  tokens and targets, tabs, underscores, unclosed styles, palette and direct colours, style fonts, both line heights):
  391 compared, ALL EQUAL; 9 not compared because the original stops in MessageBoxA (the link count passed 0x28, U-1363).
- WINDOW CALLS (opt-in): the ORIGINAL drawWindow with its drawing callees recorded (drawFrameR3 / drawFrameS2 / blitPanel /
  Surface_stretchBlit / Surface_drawScaled / composeGolferSprite / Image_draw / Win_fillRect, the display's setScale),
  drawWindowFrame, drawThoughtBubble, typeBit7Clear, thoughtFlag and drawMessageBuffer real; 300 cases (sel -1 with modes
  -3..3, golfers 0..40, pictures -2..-0x16, five positions families, 0..6 wrapped lines): 300 EQUAL (playSound, mode 0's
  sound, is the layout's and not compared).
- TIP CALLS (opt-in): the ORIGINAL showInfoCursorPanel / drawTerrainToolbar / drawGolferActionPanel / drawEmployeeViewPanel /
  drawEmployeeDetailPanel / drawAmenitiesPanel with the dwell counter past ten and the cursor on a grid point whose hit (the
  port's registered hit tests) is a tip's; the tip part of the call log (the prefix before the panel background for the tool
  tip, the trailing tip calls otherwise, the backdrops for the building row) against the port's tip in record mode: 60 cases
  per kind, all EQUAL (distinct hits: tool 15, slot 2, toolbar 6, action 6, view 18, detail 14, amenities 10, row 14).
  lookupThemeText (file input) is stubbed empty in both arms; the theme text is judged by the frame below.
- GOLDEN (default suite): sha256 of the port's logs of the oracles' cases (120 text, 120 window, 30 per tip kind), pinned after
  the oracles were GREEN.
- FRAMES (default suite): the opaque two-fill measure (S3.6's) in the rectangle the family draws: championship tour_0060 (the
  ticker with Gary), cov_a_flags zdemo (the tool tip with its theme text), cov_r_tutorial p01 / p10 (tutorial windows) / p15
  (the ticker with no golfer), cov_k1_label label (a help window): every opaque pixel equal.
- CONTROLS (g_negctl_gfx_text, sg_negctl_set "gfx.family.text"): 1 `{` keeps the style, 2 no literal brace for `{{`, 3 tabs
  ignored, 4 shadow one pixel lower, 6 golfer window one pixel right, 7 drawWindowFrame's strip one pixel lower, 8 tool tip
  frame one pixel right, 9 drawMessageBuffer breaks 8 px later, 10 the backdrop tips one pixel lower, 11 the amenities value
  line one pixel lower, 5 Text_put's width to the frame code 8 more: 1-4 and 6-11 RED on the emulator oracles and on the
  goldens; on the frames 5, 6, 7, 8, 9 RED (5 on cov_a_flags end: the frame code places a golfer's thought box, portrait and
  purple text box, by Text_put's width: with the width the box equals the reference (603 px differ in its bbox, all under the
  missing drawMapLine bar of the label below it, against 9,170 without the family), with the control 3,792; 7_ctl5.py,
  7_ctl5.log). Blind on the frames, measured: 4 (no shadowed message text in a pinned frame, section 3); 1, 2, 3, 10, 11 were
  not run on frames (no pinned frame of this step's shows that markup or that tip).

[results of the runs: section 6]

## 5. The census with the family (d2 build, log/roadmap/S3/7_d2/census.tsv.d; `7_cmp.py` against the base: 7_cmp_base_d2.log)

698 rows: status UNSUPPORTED -> FAIL 21 (no unsupported reason left, pixels only), nothing else changed status. Beyond-one-level
px per region against the base: hud_message better 56 / worse 1 / same 543, hud_meters 57 / 1 / 542, hud_title 44 / 0, view
91 / 1, bottom_panel 3 / 0. The one worse row, cov_r_s2_screens skill_popup, is U-1368 (S2's skillPointDialog draws nothing, so
the ticker the frame before left shows). No in-game row reports any of this step's reasons any more (`7_reasons.py`). Of the 599
in-game rows 37 keep beyond-one-level px in hud_message, each looked at: S2's golfer info panel over the strip (cov_a_golfer,
cov_a_keys2 / keys3, cov_r_bio, stories_fixture: about 556 px each), S2's full-screen screens (7,800), and the thought labels'
drawMapLine bars (S3.8: cov_k3_champ aim*, cov_a_flags, cov_b_inject end, cov_k2_simfoto right, cov_k1_river r20,
cov_b_holes18 holes: the text equal, the dark bar missing; log/roadmap/S3/7_img/d2_*.png).

## 6. Gates (final build d3 = port/build/S3_7_dll/d3, Win32 w3; runs from port/build/S3_7_run)

- TEXT GATE (`7_textgate.py`, the opaque two-fill measure over EVERY pinned scenario, --no-live; log/roadmap/S3/7_gate/,
  textgate.tsv, 7_gate.log): 599 in-game rows. Bad opaque px in the three text regions (hud_message, ticker 0xc0,0,0x1f0,0x98,
  tipband 0xc0,0x170,0x280,0x80) are attributed by `7_attrib.py` (7_attrib.log): a pixel is this step's when the text family
  changed it (base census image against the family's). Family-changed bad px: 9 rows; hud_message 1,473 px, all of them
  another step's: cov_r_s2_screens editor 1,467 (the character editor, S2's screen, draws nothing in the port, so the ticker
  the frame before left shows where the original has the editor: U-1368), cov_r_tutorial p16 4 and stories_fixture loaded 2
  (a thought label's missing drawMapLine bar over a golfer sprite, S3.8 / S3.5); ticker 15,840 (editor 15,437; cov_r_tutorial
  p11 / p19 / p20 403: the thought labels' missing bars over the tutorial window, S3.8); tipband 88 (cov_r_tutorial p23 71 and
  cov_r_course end 16: thought labels' bars over a tip / sprite, S3.8; cov_a_flags zdemo 1: shading over a scene sprite's
  pixel, S3.5). Crops: log/roadmap/S3/7_img/g_*.png. The bad px the family did not change (hud_message 1,874, ticker 258,023,
  tipband 175,848) are other steps' content in the wide rectangles (S2's screens and panels, S3.5's sprites). So no in-game
  checkpoint has a bad opaque pixel of this step's drawing.
- Census (raw framegate, terrain live; d2 = the family without its controls and the two late fixes of section 4's oracle):
  section 5; no in-game row reports a reason of this step's.
- port/test (one file per process, x64, d3): test_gfx_text 30 passed + 20 skipped, then 32 with the cov_a_flags end frame and
  control 5 added (its frame tests 12 / 12: 7_pytest_frames2.log) (the opt-in oracles; with SG_EMU_ORACLE=1
  all 20 pass: 7_oracle.log, 7_oracle_neg.log), test_static 6, test_gfx 25, test_gfx_hud 34 + 7 skipped (S3.6b's file with
  merge edit 5) (7_pytest_x64.log, 7_pytest_static.log, 7_pytest_gfx.log, 7_pytest_others_x64.log); test_gfx_boot_screens 32,
  test_gfx_credits 47, test_gfx_pixel 17, test_gfx_eoi 5, test_gfx_scene 114, test_gditext 22 + 1 skipped, test_commands 83 + 44
  skipped, test_gfx_dialogs 31 + 1 failed, test_gfx_routing 34 + 1 failed, test_gfx_reports 33 + 3 failed (7_pytest_others2_x64.log,
  7_reports_fails.log). The five failures (dialogs / reports test_draw_sprite_other_alpha_is_unsupported, routing
  test_image_draw_at_another_scale_is_unsupported, reports test_frames_report_the_unmodelled_sprites and
  test_frame_controls_red_all[8]) fail the same way on the BASE build without this step (7_base_fails.log): they expect
  `unsupported` for cases S3.5's merge edits 8 / 10 now draw (the scene family), so they belong to S3.5's merge (the integrator
  updates S2's tests or S3.5's notes say how). Win32 (SG_BACKEND=win32, runner from S3_7_w32): test_gfx_text 30 passed + 20
  skipped before the end frame was added, the goldens equal on both backends (7_pytest_w32.log).
- Segments (`python -m port.test.integrator segments`, x64 --dll d3, Win32 --backend win32 --dll32 / --runner32 w3): 205 / 205
  PASS on both, 0 status / digest differences from the committed tables (log/roadmap/merges/S2S3-5/segs_*.tsv), x64 = Win32
  (7_segs_*.tsv, 7_segs_compare.log).
- Boot (`python -m port.test.boot --all --jobs 3`, d3): HEAD's 61 at 58 PASS / 2 KNOWN / 1 NO_FRAMES, 0 of the committed tree's
  70 rows differ (7_boot_x64.tsv, 7_boot_compare.log via 6b_boot_compare.py).

Reproduce:

    ~/bin/py -3.12 log/roadmap/S3/7_mirror.py --w32                        # source root S3_7_src, builds S3_7 / S3_7_w32
    ~/bin/py -3.12 log/roadmap/S3/7_mirror.py --no-build --src S3_7_run     # a run root (never rebuild a root a run reads)
    cd port/build/S3_7_run
    SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 -m pytest port/test/test_gfx_text.py [-k ...]   (SG_EMU_ORACLE=1 for the oracles)
    SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 ../../../log/roadmap/S3/7_textgate.py ../../../log/roadmap/S3/7_gate --jobs 2
    ~/bin/py -3.12 ../../../log/roadmap/S3/7_attrib.py ../../../log/roadmap/S3/7_gate ../../../log/roadmap/S3/7_base/img ../../../log/roadmap/S3/7_d2/img

## 7. Files

log/roadmap/S3/7_files.txt.

## 8. Open

U-1360 .. U-1368 (7_uncertain.tsv). For S3.8: the bars of the tips and thought labels (drawMapLine) are called by name by the
text family once a body exists; Text_put's width reaches the frame code with a backend (merge edit 7), which places them.

## 9. CLAUDE.md paragraph

Roadmap S3.7, the marked-up HUD text, the ticker and the hover tips (2026-10-25, notes log/roadmap/S3/7_notes.md, U-1360..U-1368):
body family `text` (port/src/sim/gfx_text.cpp). The root text object (the Image at g_rootWindow + 0x274) has markup ON from
Image_ctor (f_cfg_4e4210 = 1), so every frame text is drawRichText's: the family ports the scanners, drawTextRun (style fonts and
colours, baseline, shadow run, tab stops of 30, '_', the run palette's entry 1) and drawMarkupRun / Text_put, and the core's
draw_text_string hands it every text with a markup character, a tab or a palette colour. drawWindow is the family's whole body
(golfer portrait, picture windows, the slide-in, drawFrameR3 / S2 / drawWindowFrame, the centring) with drawMessageBuffer; the
TICKER had never reached the backend (commands.cpp draw_window_effect handed nothing over) and now does. The HUD panels' hover tips
(showInfoCursorPanel's tool tip, transcribed from the instructions, with its theme text, and slot tip; drawTerrainToolbar's,
drawGolferActionPanel's, the employee panels', drawAmenitiesPanel's tip and building row) are drawn through S3.6's gfx_hud.cpp;
their drawMapLine bars are S3.8's. Text_put became a measuring drop (gfx_take_value: with a backend the frame code gets the width
that places the thought labels' bars). EVIDENCE: an emulator oracle down to a fake jgld surface (setFont / colours / draws / end)
over the ORIGINAL text functions (391 random marked-up cases), drawWindow (300) and the six panels' tips (60 per kind), all equal;
goldens in the default suite; the opaque two-fill frame measure over every pinned scenario: no in-game checkpoint keeps a bad
pixel the family drew (the 9 rows with any are other steps' content over or under it). The image cursor exists only over an EditBox
and the widget tooltips only through Tooltip::show: neither is on a pinned frame (U-1360, settles U-1069's cursor / tooltip parts);
drawWindow's and the tips' texts never take drawTextSurface's shadow (g_font51b360 H 15, g_fontRef_519fd8 H 10), U-1072 resolved.
Controls: 10 RED on the oracles, 5 on frames.
