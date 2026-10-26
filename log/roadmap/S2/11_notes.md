# S2.11 notes: dialogs over the course 2 (tournament results, bulletin board, SimFoto / message ball)

Worker on claude3 (session cmvncb29y6uqlo1153sxbjj5g), lead cmvmns3ax3frbo1152mcm7eg2. Merge request: 11_merge.md (+ 11_shared.patch).
Files: 11_files.txt. Uncertainties: 11_uncertain.tsv (U-1215..U-1219).

Status: DONE. Every judged pixel of the 17 rectangles of the 13 pinned checkpoints that have a snapshot equals the original (0 failing px);
championship tournament_end has no snapshot (U-1120, S3.3's). Segments, boot and census as in section 5.

## 1. The mirror and the builds

`port/build/S2_11_head` = a robocopy of `port/build/S2_13_head` (HEAD 2c05f02a + S2 waves 1-5; the S2 waves are not committed: `git log`
shows 2c05f02a as HEAD) without build dirs and junctions; junctions re-created per log/roadmap/S0/export.md. x64:
`~/bin/py -3.12 log/roadmap/S2/build.py <abs>/port/build/S2_11_head <abs>/port/build/S2_11 --log log/roadmap/S2/11_build.log`
(development builds went to port/build/S2_11d, run from a copy in port/build/S2_11d_run so a running boot never locked the build).
Win32: `~/bin/py -3.12 port/test/build.py --arch Win32` from the mirror root (its port/build/Win32; 11_build_w32.log).

## 2. What the original does and what the port now draws

All three dialogs ran in the port without drawing; the probe log of the base DLL (11_probe_base.log, tool 11_tools/probe.py) and a
drop trace (a temporary SG_GFX_TRACE switch in gfx_drop, removed before the final build) showed where.

- **Tournament results** (tick_panels.inc showTournamentResults, from c3eo.cpp; the scorecard layout of all five pinned checkpoints): every
  drawing call already had a body except drawStringRightAt (the prize line), but the rows never moved: the row step, the close button's y
  and its hover box read the bands' SpriteRef +0x18 / +0x1c (the cut size cropSprite stores; g_spriteRef_821cb8 + 0x58 + 0x1c, + 0x108 +
  0x1c, + 0x160 + 0x1c, + 0x1b8 + 0x1c; g_spriteRef_820f40 + 0x18 / + 0x1c), which the port's GameState never holds (the cuts live in the
  backend). SprW / SprH read them from the backend (gfx_sprite_cut, S2.8's) when one is attached, the GameState word (0) otherwise.
  Simulation: only drawing positions depend on them. The y >= 0x225 break is checked with the 0x12-rank break after each score's
  sweep; rows start at y 0x4d and step 0x1a (a prize row: band 1, height 0x1a) or 0x12 (band 3), so while rank < 0x12, y <= 0x4d + 17 *
  0x1a = 0x207 < 0x225 and the y break never fires before the rank break; the box only picks which button image is drawn.
  New body drawStringRightAt (c3df.cpp DrawString_re: x - run, prim_text's drawText path).
- **Message ball** (drawWindow with f_w_4c2e00[2] = sel >= 0, c3ei.cpp DrawWindow_re): the port's tickMessage is commands_layout.inc's
  (registered; tick_leaves.inc's drawWindow is the registry row others call), which never handed drawWindow to the backend:
  draw_window_gfx now does (x 0xf0, y -8, the slide mode). drawWindow.golfer: ex = x + 0x1c, ey = y + 0x2c, mode -2;
  drawFrameR3(ex - 8, ey - 8, 0x1a0, H + 0x10) (the core's draw_frame_r3), the ball g_sprites5a53c8 cell 29 over g_sprites58b680 cell 29
  (Surface_stretchBlit; cut by loadInterfaceArt.ballArt from TransPopups / TransPopups_A at (0, 0x12c) 0x8c x 0x8c, kIa22 / kIa25), the
  head (held, U-1216), ey + 0x10 (+ (H - h - 0x10) / 2 when the unclamped h < 0x60; h through commands.windowHeightRaw / t_window_h_raw),
  drawMessageBuffer(0x63, ex + 0x64, ey, white): the core's draw_message_buffer, now with drawTextSurface's shadow for a white line with
  H > 0x10 (it reported that case before).
- **SimFoto**: all seven pinned SimFoto checkpoints wait in narrateShot's pumpAndRedraw(0) (framegate context MODAL:pumpAndRedraw), so the
  frame on screen is narrateShot's own (c3fr.cpp: box (minX - 0x28, minY - 0x28) .. (maxX + 0x28, maxY + 0x28), 16-pixel border, black
  line, caption f_strpool_4c4e58, the date by drawTextSurface, refreshFull), not P5_simfoto's. The port's narrateShot (shots.cpp)
  dropped drawWindow and the frame: with a backend it now draws both. drawWindow's simulation part is not in the port's narrateShot (the
  window-height pass writes g_textBuffer's line breaks and g_value587ea0), so commands.drawWindowGfx runs that pass for the drawing and
  puts g_textBuffer, g_textBuf, g_ui_821c50 and g_value587ea0 back (gfxr::ScreenDraw::text).
- **Bulletin board** (dialogs.cpp show_bulletin_board, from c3em.cpp): reached in cov_b_holes18, cov_r_s2_screens, ui_tour, ui_tour_v (7 visits,
  11_bbscan.log, a framegate run with a marker); pinned at cov_r_s2_screens `bulletin` (a full-screen trophy shelf, no course, no dated
  record). Family bulletin: the art cuts of loadInterfaceArt(1) (kIa11..kIa16 with their Row / TableRow calls; cells 0..36 and 38..59, 67..69
  of g_sprites53fd48 / masks g_sprites5a34f8, the TableRow rectangles from f_tab_point16_4c2d38), the board image (loadImage of
  f_strpool_4e3e28 + 0x30 with g_palette5199c8, Surface_fillRegion over 800 x 600), per event the frame (cell 33), the line text
  (drawText4049d0, font g_fontRef_519fd8), the photo (U-1215) and the pin, then the fixed sprites (notes, clubs, ball, button, trophy,
  plates). The original cut cell numbers are from the destination addresses (e.g. 0x5403d0 = g_sprites53fd48 + 0x688 = cell 38).

## 3. The rectangles and the held pixels

The table is 11_rects.tsv from the final x64 DLL. Rectangles (test_gfx_dialogs2.py CASES; measured by 11_tools/rects.py, 11_rects_measure.log):
- SimFoto: the box of the 0x7fff region after a 5 x 5 opening (white label text touching the border had widened ui_tour menus by 2
  columns: the first table had 363 failing pixels there, all in columns 755-756 where the reference is black);
- the ball's frame: the outline colour RGB (206, 206, 231) gives the top / bottom lines; frame = (x0 - 10, top, x1 + 7, bottom + 2), the
  insets measured on tour_0180 (drawWindow(0x96, 0xef, .., -2), H 0x60: frame (170, 275) .. (585, 386), outline x 180..578, rows 275 / 384);
- results strip and bulletin board: the whole frame.
Held: probe modes >= 0x10000 of the families results / simfoto fill the rectangle a dialog covers when it starts (results.begin: the whole
root; simfoto.begin: the SimFoto's outer box; drawWindow.golfer: frame and ball square) and paint the ball's square after the ball (the
head is not drawn: U-1216). Judged = the three probe runs and the plain run agree.

| scenario / checkpoint | rectangle | px | judged | fail | held |
|---|---|---|---|---|---|
| cov_b_shots results | whole frame | 480,000 | 101,719 | 0 | 378,281 |
| cov_b_shots results_r2 | whole frame | 480,000 | 101,719 | 0 | 378,281 |
| cov_b_shots2 results / results_r2 / results_r3 | whole frame | 480,000 | 101,719 each | 0 | 378,281 |
| championship tour_0180 SimFoto | (505, 102, 657, 254) | 23,409 | 9,132 | 0 | 14,277 |
| championship tour_0180 ball | (170, 275, 585, 386) | 46,592 | 2,861 | 0 | 43,731 |
| ui_tour menus SimFoto | (531, 291, 754, 474) | 41,216 | 12,463 | 0 | 28,753 |
| ui_tour menus ball | (260, 26, 675, 168) | 59,488 | 3,393 | 0 | 56,095 |
| cov_k1_animals bought SimFoto / ball | (395, 235, 617, 418) / (260, 26, 675, 168) | 41,032 / 59,488 | 12,404 / 3,374 | 0 / 0 | 28,628 / 56,114 |
| cov_b_inject2 debug_overlay SimFoto / ball | (376, 241, 599, 423) / (260, 26, 675, 168) | 40,992 / 59,488 | 12,415 / 3,377 | 0 / 0 | 28,577 / 56,111 |
| cov_k2_simfoto bottom | (288, 484, 511, 599) | 25,984 | 10,193 | 0 | 15,791 |
| cov_k2_simfoto right | (684, 208, 799, 391) | 21,344 | 8,873 | 0 | 12,471 |
| cov_a_golfer button_2 | (215, 191, 517, 454) | 79,992 | 17,727 | 0 | 62,265 |
| cov_r_s2_screens bulletin | whole frame | 480,000 | 480,000 | 0 | 0 |
| championship tournament_end | NOT_RUN (U-1120, S3.3) | | | | |

The results strip's held pixels are the course under drawAlpha's quarter-black band (Surface_drawSprite of g_spriteRef_821040 over the whole
screen) and its transparent parts; a ball's are its frame's shaded panel (blitPanel's 0xf8 bytes), the blended border and the 0x8c square
(ball + head); a SimFoto's are the photo (the course) and nothing else of the frame.

## 4. Controls

Frame controls (test_frames_negctl_red: more failing pixels in the rectangles than without): results 1 (drawStringRightAt one right), 2 (rows
one pixel further apart), simfoto 2 (text one lower), 3 (frame one left), 4 (the h < 0x60 offset ignored, tour_0180: h 0x4d gives +1),
bulletin 1 (board one lower), 2 (trophy one right), 3 (notes' x without s / 2), 4 (TableRow w / h one entry later). simfoto 1 (the ball cut
one row lower) is judged by test_ball_cut_negctl_red (the square's pixels equal to the reference: more with the right cut). Blind: none.

## 5. Gates (logs in log/roadmap/S2/11_*)

| gate | result | log |
|---|---|---|
| test_gfx_dialogs2 x64 | 27 tests: 26 passed in the suite run (every rectangle 0 failing judged px, 9 frame controls RED, the ball-cut control RED, tournament_end NOT_RUN), test_bodies_handled failed on a test defect (drawWindow.golfer is reached through find_body and has no `handled` count) and passes after the fix | 11_x64_test_gfx_dialogs2.log, 11_x64_test_gfx_dialogs2_fix.log |
| Win32 (SG_BACKEND=win32, one file per process) | test_gfx_dialogs2 27 passed (with the test fix), test_gfx 25, test_gfx_dialogs 32, test_gfx_reports 36 (with the control 8 list change) | 11_w32_suite.log, 11_w32_test_*.log |
| segments | 205 / 205 PASS x64 and Win32; against 13_segs_*.tsv 0 differ (status and digest) | 11_segs_*.tsv |
| boot --all | 70 rows equal to 13_boot_gate.tsv (PASS 65, KNOWN 2, NO_FRAMES 2, FAIL 1 = cov_r_s2_screens at 1156 on showBuyLandScreen, U-1251, S2.10's) | 11_boot_gate.tsv, 11_boot_compare.log |
| census, 698 rows, against 13_census_x64.tsv | PASS 83 -> 84 (cov_r_s2_screens bulletin); no other status changed. The first run had cov_a_golfer NOT_RUN in all rows: sg_run_boot -11 after 0 records in 0.4 s (a game file missing at load while S2.10 / S3 runs shared the machine; the boot gate ran the scenario fine); a re-run alone gives its usual rows (11_census_rerun.tsv, merged into 11_census_x64_final.tsv). 33 rows UNSUPPORTED before and after with more differing pixels (+1 .. +9,586) and the same first unsupported drawing call: dialog pixels (the bulletin board, message balls) the port leaves in the root under frames it cannot draw, the class S2.9 reported | 11_census_x64.tsv, 11_census_x64_final.tsv, 11_census_compare.log, 11_census_worse.log |
| x64 suite, 60 files one per process | 57 PASS; test_boot 1 failed = cov_r_s2_screens' known stop (U-1251, as in the boot gate); test_gfx_dialogs2 see above; test_gfx_reports 1 failed: control 8 at hole_stats (below), passes after the list change | 11_x64_suite.log, 11_x64_test_*.log |

test_gfx_reports control 8 (Surface::callBounds leaves the clip narrowed): with the S2.13 DLL it changes 8 judged pixels of hole_stats, all
one-level differences of the hole-stats panel's blended border (e.g. (244, 33) plain (214, 206, 206), control (222, 206, 206)) over the roster
screen the port leaves in the root; this step draws the bulletin board between rep_roster and hole_stats, so the root under the panel is the
board, which the control does not change: 0 judged pixels change (11_tools/ctl8_judged.py on both DLLs). The control stays RED on
rep_financial 2, rep_shortcuts 2, rep_roster 1, best_scores 2, end_of_year 1 judged pixels (11_ctl8.log); hole_stats was dropped from its list.

## 6. Reproduce (mirror root port/build/S2_11_head)

```
~/bin/py -3.12 <abs>/log/roadmap/S2/build.py <abs>/port/build/S2_11_head <abs>/port/build/S2_11 --log log/roadmap/S2/11_build.log
~/bin/py -3.12 port/test/build.py --arch Win32
sh <abs>/log/roadmap/S2/11_tools/gates.sh [segs64 segs32 boot census suite64 dlg32]
SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 -m port.test.test_gfx_dialogs2 <abs>/log/roadmap/S2/11_rects.tsv     # the table
SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 <abs>/log/roadmap/S2/11_tools/diag.py <scenario> <checkpoint> <out dir>   # where a rectangle fails
~/bin/py -3.12 <abs>/log/roadmap/S2/11_tools/rects.py                      # the rectangles from the reference frames
~/bin/py -3.12 log/roadmap/S2/13_tools/compare.py segs|boot|census ...
~/bin/py -3.12 log/roadmap/S2/11_tools/make_patch.py <scratch>             # 11_shared.patch and its check
```

## 7. Open

- Base: 11_merge.md section 0 (b3d61e41 + wave 5; applies cleanly, tick.cpp's result differs from the mirror only by S3.3's U-1120 hunks).
  championship tournament_end may render there: not judged yet (U-1218), the test skips it.

- U-1215 bulletin photos; U-1216 the ball's head (S3.5 / S3.6); U-1217 drawWindow's other arms; U-1218 the leader-board layout of the results;
  U-1219 P5_simfoto's frame judged by no checkpoint.
- Cleanup after acceptance: port/build/S2_11_head (with its port/build/Win32), port/build/S2_11, S2_11d, S2_11d_run.

## CLAUDE.md paragraph (after the S2.13 paragraph)

```
Roadmap S2.11, dialogs over the course 2 (2026-10-25, notes log/roadmap/S2/11_notes.md, U-1215..U-1219): the TOURNAMENT RESULTS strip
(showTournamentResults' scorecard layout), the bulletin board (showBulletinBoard's trophy shelf) and the SimFoto / message ball are drawn
by three families, gfx_results.cpp (drawStringRightAt, results.begin), gfx_simfoto.cpp (loadInterfaceArt.ballArt, drawWindow.golfer,
simfoto.begin) and gfx_bulletin.cpp (the board's art cuts of loadInterfaceArt kIa11..kIa16, the board image, events, photo pins, the
fixed sprites). Findings that shaped it: the results rows and the close button read the bands' SpriteRef +0x18 / +0x1c, which only the
backend holds (tick_panels.inc reads gfx_sprite_cut with a backend: drawing positions only, the y break never fires before the rank
break); the port's tickMessage (commands_layout.inc, registered) never handed drawWindow to the backend; every pinned SimFoto sits in
narrateShot's pumpAndRedraw(0), so its frame is narrateShot's own, which the port's shots.cpp now draws with a backend, running
drawWindow's window-height pass and putting g_textBuffer / g_textBuf / g_ui_821c50 / g_value587ea0 back (commands.drawWindowGfx). The
ball's head (Surface_drawScaled of a heads cut, S3.6; palette by composeGolferSprite, S3.5) is not drawn: the probe holds the ball's
0x8c square (U-1216). Gate (test_gfx_dialogs2.py, rectangles measured on the reference frames by log/roadmap/S2/11_tools/rects.py, held
pixels by the families' probes): all 17 rectangles of 13 checkpoints 0 failing pixels, x64 and Win32 (results strips 101,719 judged px
each; the bulletin board all 480,000); championship tournament_end has no snapshot (U-1120). Segments 205 / 205 x64 + Win32, 0 digests
changed; boot equal to S2.13. Controls: 10 RED, 0 blind.
```
