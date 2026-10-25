# S2.9 notes: dialogs over the course (showPopup, textInputBox / saveGameDialog, the System menu, the SGA grading page)

Status: DONE. Every judged pixel of the 14 dialog rectangles of the 10 pinned checkpoints equals the original (0 failing px);
held pixels (decided by the course under the dialog) are listed. Every regression gate unchanged. All work is in the mirror `port/build/S2_9_head` (a robocopy of `port/build/S2_4_head` without its build
dirs and junctions, HEAD_COMMIT c6d7d78b; junctions log/, original/, port/corpus/, re/replay, re/versions, shim/build,
tests/fixtures, tests/golden re-created with PowerShell New-Item -ItemType Junction). x64 build `port/build/S2_9` through
log/roadmap/S2/build.py; Win32 in the mirror's own `port/build/Win32` (built on demand by SG_BACKEND=win32). Merge:
log/roadmap/S2/9_merge.md (2 new files + log/roadmap/S2/9_shared.patch, made and checked by 9_tools/make_patch.py).

## What the original does (specifications; instruction addresses are in the hook sources)

- showPopup (c3em.cpp ShowPopup_re): setDrawTarget(root, g_font519928) once; the box is (widest Text_put - 1) | 0xf + 0x31 wide
  and (lines * 3 + 3) * 8 high around x; each pass draws drawPanelS2(left, y, w, h) for an even y (an odd y: two fills, the
  inner one in the colour read from the root surface at (x, y) before the loop), the close button for a check list (flag4 == 0),
  then per line ended by 0x0a: a caption with drawCenteredText2 (0x80007ff0, + 0xf with flag4), an option with its text at
  left + 0x24 (drawTextSurface at lineY + 7 for the selected one, drawText4049d0 at lineY + 8 for the others; white when its bit
  is in the mask, teal 0x80000210 otherwise, grey 0x80006318 when disabled) and its bullet (g_surfaces5678b8 cell 0 / 2 over
  the mask g_sprites56a7b8 cell 0, Surface_stretchBlit at (left + 0xc, lineY + 4)); lineY += 0x12, + 6 after the first option;
  Window::refreshFull; then the wait and the pump. The System menu IS a showPopup (c3fb.cpp OptionsMenuBuild_re: systemPopup =
  showPopup(0xfa, 0x154, 1, 1, 0)).
- drawPanelS2 (c3da.cpp DrawPanelS2_re): the display scale (1, 1, 1), 16-rounding, fillWindowRect(x + 4, y + 4, w - 8, h - 8,
  0x80004e79), the eight border pieces from g_sprites5612e8 cells 30.. over g_sprites58ccb8 cells 30.. (Surface_stretchBlit,
  the mask blend). Those cells come from loadInterfaceArt(1)'s InfoButtons / InfoButtons_A loads (c3fv.cpp after kIa17 /
  kIa18: Grid at x 0xc8 + 0x11 c, y 0x11 r, 16 x 16, trim 0), the bullets from TransPopups / TransPopups_A (after kIa20 /
  kIa23).
- textInputBox (c3el.cpp TextInputBox_re): per pass setDrawTarget(g_font51b360), Win_fillRect frames (x - 1, y - 1) white,
  (x + 1, y + 1) black, (x, y) 0x800035b3, all (width + 0x20) x 0x30 with width = maxLen * 12; the title at (x + 4, y + 6)
  white; the white field (x + 0x10, y + 0x17, width, 0x16); setDrawTarget(g_font519928); the text at (x + 0x14, y + 0x1c)
  black; the caret Widget_fillRect(root surface, cx, y + 0x1a, cx, y + 0x2a, 0x80006000) with cx = x + Text_put(head) + 0x14
  (= jgld Surface::drawLine -> line16 -> vline16, y1..y2 inclusive); after the cut loop the underscore fillWindowRect(w + x +
  0x15, y + 0x29, 8, 2, 0x80004210) and the refresh.
- saveGameDialog (c3fa.cpp SaveGameDialog_re): drawPanelS2(0x36, 0x50, 0x274, 0x50), setDrawTarget(g_font51b360),
  drawCenteredText2(f_strpool_4c3f74 `SAVE GAME: edit name ...`, 0x180, 0x58, white), then textInputBox(.., 0x40, 0x66, 0x30).
- showCourseReport modes 1 / 2 (c3fr.cpp ShowCourseReport_re): setScale(1, 1, 1); Surface_drawSprite(g_spriteRef_821040 =
  black.pcx, every byte 0xf7, root, 0, 0, alpha 0x3e800000 = 0.25, no mask, flag 1); Widget_setQuad black; the SGA background
  g_spriteRef_821c60 over its mask (+0x2c) at (0x23, 0x1a); the page's texts through Widget_draw (left at x), drawStringAt
  (x - (w >> 1)) and drawStringAt2 (x - w) with the font swap (c3dq.cpp), stars g_surfaces59b050 cell 17 every 0xe from x 0x12f,
  the red `- not acceptable -` mark, the score / grade / prize lines, the OK button g_spriteRef_820f40 at (0x2bd, 0x18e).
  Surface_drawSprite = the sprite's slot +0x94 = Sprite::drawAlpha (re/match/jgld_blitY17.cpp, read from jgld.dll's vtable):
  0.25 on the RGB555 display is draw16_10043430 (re/match/jgld_blit16.cpp): 1/4 palette word + 3/4 destination computed with
  the 565 field masks on <<4 words and stored WITHOUT the final >> 4 (the original's arithmetic, kept).

## What changed (mirror paths; details and the table of shared edits in 9_merge.md)

- port/src/sim/gfx_dialogs.cpp (new): family `dialogs`, 13 bodies: loadInterfaceArt.dialogArt (also the icon row
  g_surfaces59b050 from kIa6's last load, cell 17 the star), drawPanelS2, drawTextSurface, Widget_fillRect (axis-aligned lines),
  showPopup.readColour / .oddBox / .oddRepaint, Surface_drawSprite (drawAlpha's 0.25 band), Widget_draw, drawStringAt,
  drawStringAt2, dialogs.begin / dialogs.end (markers; the held-pixel probe).
- commands_popup.inc: draw_popup_pass and the box drawing of text_input_box (only with a backend attached; a line's 0x0a and
  the caret's cut byte are put back, so the GameState equals a run without a backend).
- saveload.cpp: saveGameDialog's panel and heading; reports.cpp: struct Page / ReportDraw, the grading page; menu.cpp: the
  dialogArt drop after loadInterfaceArt.images; gfx_families.inc: SG_GFX_FAMILY(dialogs).
- port/test/test_gfx_dialogs.py (new, 32 tests; `~/bin/py -3.12 -m port.test.test_gfx_dialogs <tsv>` prints the gate table).

## The rectangles and the held pixels

Rectangles (test_gfx_dialogs.py CASES), measured on the reference frames with log/roadmap/S2/9_tools/comps.py: the bounding box
of each connected region (>= 2,000 px) of the panel fill colour 0x4e79 (RGB (156, 156, 206)) grown by 5 px (the border starts 4 px
outside the fill and covers its outermost pixel); the grading page's from its panel colour RGB (148, 148, 198) grown by 5. A
partly covered dialog (exit_save's retirement popup under the SAVE GAME panel) gets the visible part's box.

Held pixels (decided by what lies under the dialog, the course until S3): the family's probe (negctl mode >= 0x10000 of
gfx.family.dialogs, a measurement) fills the root surface when a dialog stack starts (depth 0 and some drawing call since the
last dialog ended; Window::refreshFull does not count: mainLoop refreshes between the System menu's answer and the retirement
popup it opens). A pixel is judged when the run without a probe and the three probe runs (black, white, a pseudo-random pattern)
agree; with two constant colours only, k_dlg_J's (682, 431) agreed by coincidence (below).

## Gate (logs in log/roadmap/S2/9_*)

Per rectangle (log/roadmap/S2/9_rects.tsv; px = rectangle size, judged = pixels the dialogs decide, fail = judged pixels that
differ from the reference, held = not judged, held_differ = held pixels that differ):

| scenario / checkpoint | rectangle | px | judged | fail | held | held_differ |
|---|---|---|---|---|---|---|
| cov_r_quit exit_prompt (retirement popup) | (240, 100, 559, 243) | 46,080 | 46,061 | 0 | 19 | 15 |
| cov_r_quit exit_prompt (System menu) | (82, 337, 417, 559) | 74,928 | 74,909 | 0 | 19 | 12 |
| cov_r_quit exit_save (SAVE GAME panel + text box) | (49, 80, 687, 159) | 51,120 | 51,088 | 0 | 32 | 31 |
| cov_r_quit exit_save (retirement popup, visible part) | (240, 155, 559, 243) | 28,480 | 28,471 | 0 | 9 | 6 |
| cov_r_quit exit_save (System menu) | (82, 337, 417, 559) | 74,928 | 74,909 | 0 | 19 | 11 |
| cov_a_keys k_dlg_S | (49, 80, 687, 159) | 51,120 | 51,088 | 0 | 32 | 30 |
| cov_a_keys k_dlg_J (grading page) | (34, 25, 766, 444) | 307,860 | 279,281 | 0 | 28,579 | 28,561 |
| cov_a_keys k_C | (192, 137, 607, 263) | 52,832 | 52,813 | 0 | 19 | 15 |
| cov_a_keys2 C_prompt | (192, 137, 607, 263) | 52,832 | 52,813 | 0 | 19 | 17 |
| cov_a_champ n_prompt | (256, 137, 543, 263) | 36,576 | 36,557 | 0 | 19 | 16 |
| cov_a_play n_prompt | (272, 137, 527, 263) | 32,512 | 32,493 | 0 | 19 | 14 |
| cov_a_build2 remove_prompt1 | (248, 97, 551, 223) | 38,608 | 38,589 | 0 | 19 | 15 |
| cov_k3_career prompt / prompt2 | (66, 147, 433, 273) | 46,736 | 46,717 | 0 | 19 | 16 |

k_dlg_J's row counts (682, 431) as held (HELD_EXTRA, U-1280); before that exception the probes left it as the page's one `fail`
(279,282 judged, 1 fail, 28,578 held). Held pixels of the popups and boxes are the border pieces' blended corners and edges; of the page, the
drawAlpha pass and its transparent parts.

| gate | result | log |
|---|---|---|
| test_gfx_dialogs x64 | 32 passed (6 scenarios x 4 runs each, 7 frame controls, 11 unit tests) | 9_x64_test_gfx_dialogs.log (9_test_dialogs_x64.log is an earlier run: 31 + the handled-bodies test before its fix) |
| test_gfx_dialogs Win32 (SG_BACKEND=win32) | 32 passed | 9_test_dialogs_w32.log |
| census, 64 scenarios / 609 rows, against S2.4's 4_census_x64.tsv | same 427, better 146, worse 36. PASS 50 -> 69 and NOT_RUN 56 -> 32 come from cov_a_quit / cov_a_quit_title, whose traces S2.5 added after S2.4's census; 5 of their rows are the `worse` NOT_RUN -> UNSUPPORTED ones. The other 31 worse rows are UNSUPPORTED before and after with the same first unsupported drawing call, 90-100 % of the frame wrong, grown by 1 .. 8,816 px: dialog pixels left in the root surface, which the port does not redraw in-game (the in-game frame is unsupported). Gated rows (whole frame): exit_prompt 479,590 -> 358,618, exit_save 479,793 -> 326,929, k_dlg_S 478,612 -> 427,913, k_dlg_J 428,403 -> 200,635, k_C 479,357 -> 426,636, C_prompt 479,695 -> 426,882, champ n_prompt 479,421 -> 442,750, play n_prompt 479,566 -> 447,020, remove_prompt1 479,725 -> 441,139, prompt / prompt2 479,530 -> 432,817 (the rest of each frame is the course and HUD) | 9_census_x64.tsv / .log, 9_census_compare.log, 9_census_worse.log |
| segments | 205 / 205 PASS x64 and Win32; compare with log/p5/segs_*_loc.tsv: 0 differ | 9_segs_*.log, 9_segs_*_compare.log |
| boot | S2.4's 67 rows (4_boot_gate.tsv): 65 equal; cov_a_keys and golfer_stories FAILed at record 0 (`missing: text font file` in gditext.createFont, before any drawing call) in the 3-job run under heavy machine load and PASS on a re-run (9_boot_rerun.tsv), so all 67 equal; extras: cov_a_quit_title NO_FRAMES, cov_r_s2_screens FAIL (S2.6's new recording, U-1102) | 9_boot_gate.tsv, 9_boot_compare.log, 9_boot_rerun.tsv |
| test_gfx 25, test_gfx_credits 47, test_gfx_routing 35, test_commands 83 / 44 skipped, test_tick 250, test_sve_roundtrip 15, test_chareditor 19 / 4 skipped, port test_static 6 (x64) | as at S2.4 | 9_x64_test_*.log |

## Controls (family `dialogs`, 11 modes, all RED; 0 blind)

On the frames (test_frames_negctl_red: the checkpoint's rectangles differ from the reference in more pixels than without the
control): 1 drawPanelS2's fill 0x4e7a (k_C), 2 drawTextSurface without its shadow (exit_prompt: the System menu's hovered
option), 3 the caret one pixel left (k_dlg_S), 4 no right-edge border pieces (k_C), 5 the bullets cut one row lower (k_C), 6 no
16-rounding (k_C), 8 drawStringAt centred at x - w (k_dlg_J), 10 Widget_draw one pixel right (k_dlg_J). In unit tests (no pinned
frame judges them): 3 again (Widget_fillRect's line), 7 drawAlpha's word shifted back by 4 (the page's drawAlpha pixels are all
held), 9 the odd-y box's inner fill one row taller (no pinned popup has an odd y), 11 drawStringAt2 left-aligned (the pinned page's
score is -999, so it draws no `/100` line). Unit tests also check: vline16 / hline16 with inclusive ends and a point drawing
nothing, drawAlpha's 0.25 blend word by word against draw16_10043430's arithmetic (with and without the flag; 0xff always
skipped), another alpha reported, the probe filling only at a new stack. The probe modes (>= 0x10000) are a measurement.

## Reproduce (mirror root port/build/S2_9_head; the DLL port/build/S2_9/Release/portsim_c.dll)

```
~/bin/py -3.12 <abs>/log/roadmap/S2/build.py <abs>/port/build/S2_9_head <abs>/port/build/S2_9 --log log/roadmap/S2/9_build.log
~/bin/py -3.12 port/test/build.py --arch Win32                                       # the mirror's port/build/Win32 (DLL + runner)
env SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 -m pytest port/test/test_gfx_dialogs.py -q -p no:cacheprovider
env SG_BACKEND=win32 ~/bin/py -3.12 -m pytest port/test/test_gfx_dialogs.py -q -p no:cacheprovider
env SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 -m port.test.test_gfx_dialogs <abs>/log/roadmap/S2/9_rects.tsv      # the table above
~/bin/py -3.12 <abs>/log/roadmap/S2/9_tools/comps.py                                 # the rectangles, from the reference frames
env SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 -m port.test.framegate --all --jobs 2 --no-probe --out <abs>/log/roadmap/S2/9_census_x64.tsv
~/bin/py -3.12 log/roadmap/S2/3_compare_fg.py log/roadmap/S2/4_census_x64.tsv log/roadmap/S2/9_census_x64.tsv
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --dll <dll> --jobs 3 --out <tsv>   (and --backend win32)
~/bin/py -3.12 -m port.test.integrator compare log/p5/segs_x64_loc.tsv <tsv>                   (win32: segs_win32_loc.tsv)
env SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out <abs>/log/roadmap/S2/9_boot_gate.tsv; ~/bin/py -3.12 log/roadmap/S2/9_boot_compare.py
~/bin/py -3.12 log/roadmap/S2/9_tools/make_patch.py                                  # 9_shared.patch, checked with git apply
```
A probe run by hand: `GX.render(scenario, [checkpoint], lib, negctl=0x10000 | colour (or 0x20000 | seed), negctl_name=
"gfx.family.dialogs")`; its gfx.log lists `dialogs.begin (probe): a new stack after <calls>` at every stack break.

Run hygiene: the first Win32 segment run found no Win32 build in the mirror and its three workers each started a cmake configure
of port/build/Win32 at once (every segment ERROR, a broken configure). The run, its orphaned multiprocessing workers (parent_pid
in their command line) and the cmake processes on the mirror were stopped by PID, the directory removed, `port/test/build.py
--arch Win32` run once, then the segment run repeated. In a fresh mirror build the Win32 DLL before a Win32 segment run.

## Open

- U-1280: whether the original draws the grading page twice at k_dlg_J (the port does); (682, 431) held by name.
- U-1281: held pixels are not judged until S3 draws the course (popups 9..32 per rectangle, the page 28,579).
- U-1282: no pinned checkpoint for the Preferences menu's check boxes (Surface_drawScaled), an odd-y popup, the report's lit OK
  button and its mode-0 table.
- U-1283: cov_a_champ / cov_k3_career are NOT_RUN in framegate (U-1120's -11 at the boot's end) though their snapshots exist;
  test_gfx_dialogs judges those snapshots.
- Census `worse` rows: dialog pixels left in frames the port does not redraw in-game (S3).
- Cleanup after acceptance: the mirror port/build/S2_9_head (with its port/build/Win32) and port/build/S2_9.

## CLAUDE.md paragraph (after the S2.4 paragraph)

```
Roadmap S2.9, dialogs over the course (2026-10-24, notes log/roadmap/S2/9_notes.md, U-1280..U-1283): showPopup (every popup and
the System menu, which is a showPopup), textInputBox with saveGameDialog's panel, and showCourseReport's association page are
drawn by the backend. The course under a dialog is S3's, so each dialog is judged in its own rectangle, measured on the
reference frame (the region of the fill colour 0x4e79 grown by 5 px; the page's panel colour), on the pixels the dialogs decide:
family `dialogs`' probe (negctl modes >= 0x10000) fills the root surface when a dialog stack starts, and a pixel is judged when
three probe backgrounds (black, white, a pseudo-random pattern) and the plain run agree. All 14 rectangles of the 10 pinned
checkpoints (cov_r_quit exit_prompt / exit_save, cov_a_keys k_dlg_S / k_dlg_J / k_C, cov_a_keys2 C_prompt, cov_a_champ /
cov_a_play n_prompt, cov_a_build2 remove_prompt1, cov_k3_career prompt / prompt2) have 0 failing pixels, x64 and Win32; held:
9..32 px per popup (blended border corners), 28,579 on the page (its drawAlpha pass and transparent parts). Family `dialogs`
(gfx_dialogs.cpp, 13 bodies): the dialog art loadInterfaceArt cuts (popup border and masks, option bullets, the icon row),
drawPanelS2, drawTextSurface, Widget_fillRect (jgld drawLine's axis-aligned vline16 / hline16, ends inclusive),
Surface_drawSprite (Sprite::drawAlpha's 0.25 band = draw16_10043430: 1/4 palette + 3/4 destination on 565 field masks, stored
without the final >> 4, kept), Widget_draw / drawStringAt (x - (w >> 1)) / drawStringAt2 (x - w) with the font swap, showPopup's
odd-y box (the colour read from the root under it), dialogs.begin / dialogs.end. The screens' calls are in commands_popup.inc,
saveload.cpp and reports.cpp (texts built in g_textBuffer, which is put back with g_textBuf). Gates: census 609 rows against
S2.4, same 427 / better 146 / worse 36 (5 = cov_a_quit rows S2.5 made runnable, 31 = dialog pixels left in unsupported in-game
frames, first unsupported call unchanged); segments 205 / 205 x64 + Win32, 0 digests changed; boot equal to S2.4. Controls: 11
modes RED (8 on the frames, 4 in unit tests), 0 blind.
```
