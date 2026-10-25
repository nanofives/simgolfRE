# S2.8 notes: the report screens over the course (F2-F9 reports, hole stats, best scores, end of year, hire, preferences)

Status: PARTIAL by scope, DONE for S2.8's own screens. Of the eleven checkpoints of the row, eight equal the original in every judged
pixel (rep_comments, rep_histograph, rep_financial, rep_shortcuts, rep_roster, hole_stats, best_scores, end_of_year), hire differs only
in its four animated employees (addSprite of FLIC cells: roadmap S3, 1,397 px pinned, U-1272), and two belong to S2.9's functions
(rep_report1 = showCourseReport(1), preferences = showPopup's check list; U-1276). end_of_year .. pre_land are judged by framegate now
(U-1270 fix; for framegate this also needs the mirror-only KNOWN entry, see below). All work is in the mirror `port/build/S2_8_head` (a copy of `port/build/S2_4_head`, HEAD c6d7d78b, junctions per
log/roadmap/S0/export.md; the mirror's re/analysis/data/scenario_refs.json also got S2.6's cov_r_s2_screens key = S2.6 merge item 2).
Builds: x64 `port/build/S2_8` (log/roadmap/S2/build.py), Win32 in the mirror's own `port/build/Win32` (build.py --arch Win32, targets
portsim_c and portsim_runner32; configure it ONCE before parallel Win32 jobs: four integrator jobs configuring the same dir at once
left a cache without /EHsc and every segment ERRORed). Merge: log/roadmap/S2/8_merge.md.

## The judging rule (these are not full screens)

The reference frames show every report as a panel over the in-game frame, and every report first dims the WHOLE frame: Surface_drawSprite
of black.pcx (g_spriteRef_821040) with alpha 0.25 and flag 1. The course, HUD and toolbar beneath are S3's. So test_gfx_reports.py
judges the pixels a screen determines: the boot runs three more times with the root surface filled with 0x0000, 0x7fff, 0x2a55 at the
screen's first call (body `reports.begin`, reports-family negctl modes 101..103, not controls); a pixel whose port value changes with
the underlay is HELD (the dimmed course, the art's blended edges, transparent cut-outs, the system / information menus under
preferences and best scores), every other pixel must equal the reference. The judged counts are pinned (JUDGED), so a screen that stops
drawing an opaque element (its pixels would become held) reads RED. The dimming itself lies under held pixels only; its arithmetic is
judged by unit tests and by a measurement on the references (below).

## What the original does (sources cited at each port call)

- Surface_drawSprite (c3da.cpp) -> jgld Sprite::drawAlpha (slot +0x94, c3dk.cpp). The quarter band (alpha in [0.25 - g, 0.25 + g],
  g = 0.03125) calls draw16_1002ed60 while drawAlpha's flag byte is set (it starts 1 and is cleared only when the destination's format
  word [1] is 1, 0x100189af; arm 0x1001905d..0x1001909a), else draw16_10043430. draw16_1002ed60's loop (re/match/jgld_blit16.cpp):
  bytes >= 0xff skipped, with the char flag also >= 0xf8; P (palette word) and D (destination) shifted left 4, red+blue under 0x7c1f0
  and green under 0x3e00, ((P + D) >> 1 + D) >> 1 per group, masked, summed, >> 4 = floor((P + 3 D) / 4) per 5-bit channel.
  Measured: quarter-blending radial_hover with black reproduces rep_comments' toolbar rows 470..599 in 99.4 % of the pixels with these
  masks, 21.9 % with draw16_10043430's 565 masks (the rest is animation), so the reference display's format word [1] is not 1 (U-1271).
- Widget_draw / drawStringAt / drawStringAt2 / Widget_draw2 (c3dq.cpp): FontSwap (a font whose +4 is set replaces +0x5c, zeroes +0x60 /
  +0x64), then drawText / drawStringCentered (x - (run >> 1)) / drawStringRightAt (x - run) / drawCentered2 (x + (w - run) / 2) of
  c3df.cpp. All report texts are plain (the total's sign is byte 0xa7, not markup), so the markup scanner passes them whole.
- Widget_fillRect (c3ae.cpp) -> Surface vtable 0x1011d0b0 slot +0x64 = Surface::drawLine -> line16 (c3bc.cpp; hline16 / vline16 in
  c3ay.cpp). Surface_box (c3be.cpp) -> slot +0x34 = Surface::setClip (also a GDI clip region); slot +0xc4 = Surface::callBounds resets
  it (jgld_surface.cpp).
- Display slot +0xb4 (0x10067820) is `xor eax, eax; ret`: the shortcuts' middle line is 0x80000200, the hire highlight 0x80007f40 (U-1277).
- addSprite (c3ei.cpp) draws a FLIC sheet cell at once through drawQueuedSprite (U-1272).
- Layout values read from the art: a SpriteRef's +0x18 / +0x1c are cropSprite's REQUESTED w / h (c3ee.cpp CropSprite_re), which the
  port's GameState never holds (the art is the backend's): Sprite8 now records cut_w / cut_h and gfx_sprite_cut returns them.

## What changed (mirror paths; 8_merge.md lists the patch)

- port/src/sim/gfx_reports.cpp (new), family `reports`: Surface_drawSprite, Widget_draw, drawStringAt, drawStringAt2, Widget_draw2,
  Surface_box, Surface::callBounds, Widget_fillRect, reports.begin; gfx_sprite_cut.
- port/src/sim/gfx_reports.h (new): gfxr::ScreenDraw: `on` (a backend attached), gx(name, args), text(build, draw) (the text built in
  g_textBuffer, then g_textBuffer, g_textBuf and g_ui_821c50 get their bytes back), put / append / append_va, cut_w / cut_h, ctl(m).
- Screens (8_shared.patch): show_financial_report, show_player_comments_report, show_best_scores_screen, show_histograph_screen,
  show_hire_employee_dialog (commands_screens.inc), HoleStats (holestats.cpp), show_shortcuts_screen + the 63-entry text table generated
  by log/roadmap/S2/8_tools/gen_shortcuts.py from c3es.cpp, show_membership_roster + RosterDraw (routing.cpp), show_end_of_year_screen
  (reports.cpp). Two GameState changes, both the original's writes to non-simulation buffers: the comments report builds each row in
  g_textBuffer (quote, buildGolferComment's text, quote; the percentage; the hole) because buildGolferComment rebuilds the whole buffer
  and must run once with the original's prefix; the end-of-year screen builds its highlight lines as the original does (loadStoryStrings
  / landmarkName at their places). Their golden digests were re-pinned through the oracle (8_merge.md item 3).
- Shared: gfx_families.inc, gfx_body.h (cut_w / cut_h), gfx.cpp (crop_sprite stores them), commands.cpp (include), framegate.py
  (U-1270). The mirror's boot.py also carries S2.6's KNOWN entry for record 771 (U-1253); by the lead's decision (2026-10-24) it is
  NOT in the merge patch until U-1253's cause is measured. Relying on it: only framegate's rows end_of_year, skill_points,
  skill_popup, practice, cancel_prompt, editor, pre_land of cov_r_s2_screens (NOT_RUN without it). test_gfx_reports.py's frames do
  not: GX.render snapshots in boot.run's `after`, before the record comparison, so end_of_year is judged either way.
- port/test/test_gfx_reports.py (new): the judge (held_masks, judge, CLI `-m port.test.test_gfx_reports [checkpoints] [--images d]
  [--negctl name=mode]`), pinned frames, controls, unit tests with independent models of draw16_1002ed60 and line16.

## Gate (logs in log/roadmap/S2/8_*)

| gate | result | log |
|---|---|---|
| cov_r_s2_screens, judged pixels | rep_comments 53,893 / 0, rep_histograph 411,234 / 0, rep_financial 195,269 / 0, rep_shortcuts 266,940 / 0, rep_roster 434,887 / 0, hole_stats 100,373 / 0, best_scores 45,359 / 0, end_of_year 92,616 / 0 (judged / differing); hire 148,198 / 1,397, all in x 556..586, y 167..403 (the addSprite employees) | 8_test_x64.log |
| rep_report1, preferences | not S2.8's code (U-1276); with this tree preferences differs in 46,283 of 47,233 judged px (showPopup's check list is unsupported here) | - |
| framegate cov_r_s2_screens | end_of_year .. pre_land now judged (were NOT_RUN); land_popup / buy_land / end stay NOT_RUN after record 1156 (showBuyLandScreen, U-1251) | 8_fg_base.tsv |
| test_gfx_reports x64 | 24 passed, 12 slow deselected | 8_test_x64.log |
| test_gfx_reports Win32 (SG_BACKEND=win32) | 24 passed, 12 slow deselected; the same pinned counts | 8_test_w32.log |
| test_gfx 25, test_gfx_credits 47, test_gfx_routing 35, port test_static 6, test_holestats 10, test_routing 11 | passed x64 | 8_test_files_x64.log |
| test_gfx + test_gfx_credits + test_gfx_routing Win32 | 107 passed | 8_test_gfx_w32.log |
| test_reports, test_commands | 14 / 83 passed after the two goldens were re-pinned (SG_EMU_ORACLE=update: oracle GREEN on every golden case) | 8_test_goldens.log |
| test_tick 250, test_sve_roundtrip 15 | passed x64 | 8_test_tick_sve.log |
| segments x64 | 205 / 205 PASS, 0 differ from log/p5/segs_x64_loc.tsv | 8_segs_x64*.log |
| segments Win32 | 205 / 205 PASS, 0 differ from log/p5/segs_win32_loc.tsv | 8_segs_win32*.log |
| boot | S2.4's 67 rows equal in status / records / expected; 3 newer corpus scenarios (cov_a_quit PASS 301, cov_a_quit_title NO_FRAMES, cov_r_s2_screens FAIL at 1156 = showBuyLandScreen) | 8_boot_gate.tsv, 8_boot_compare.log |
| census, 64 + 1 scenarios / 641 rows, against S2.4's 4_census_x64.tsv | 609 shared rows: same 453, better 109, worse 47. Of the 47, 5 are cov_a_quit rows that were NOT_RUN in the baseline (no trace then; S2.5) and are judged now. The other 42 were UNSUPPORTED before and after, with the same first unsupported drawing call. Their px grew by 1 .. 14,560 because the in-game frame, which the backend does not draw, now shows a later leftover root screen: a report drawn earlier in the same boot. This is the stale-root effect S2.4 recorded. Status changes: NOT_RUN -> PASS 19 (cov_a_quit / cov_a_quit_title title rows), NOT_RUN -> UNSUPPORTED 5; 32 new rows (cov_r_s2_screens); raw PASS 50 -> 72 | 8_census_x64.tsv, 8_census_compare.log |

## Controls (log/roadmap/S2/8_controls.tsv; family modes, sg_negctl_set "gfx.family.reports")

RED on the frames (14): 2 drawStringAt2 one px right, 3 drawStringAt (run + 1) >> 1, 4 Widget_draw one px lower, 6 Widget_draw2
(w - run + 1) / 2, 8 callBounds leaves the clip, 11 financial year labels, 12 comment rows, 13 best-scores foot cell, 14 hole-stats
title, 15 roster names, 16 shortcuts middle line colour, 17 end-of-year line step, 18 hire band heads, 19 histograph cash labels.
Blind on the frames, RED in unit tests (3): 1 the quarter blend halved (the dimming lies under held pixels only:
test_draw_sprite_negctl_1_red), 5 a line one pixel longer (no histograph line on a first-month course: test_widget_fill_rect_negctl_5_red),
7 Surface_box without effect (no line crosses the box: test_surface_box_negctl_7_red). Total 17 modes, 17 RED, 0 blind overall.

## Reproduce (from the mirror root port/build/S2_8_head)

```
~/bin/py -3.12 <abs>/log/roadmap/S2/build.py <abs>/port/build/S2_8_head <abs>/port/build/S2_8 --log log/roadmap/S2/8_build.log
export SG_PORTSIM_DLL=<abs>/port/build/S2_8/Release/portsim_c.dll
~/bin/py -3.12 -m port.test.test_gfx_reports --images <dir>                     # judged / diff for all eleven checkpoints
~/bin/py -3.12 -m pytest port/test/test_gfx_reports.py -q -p no:cacheprovider [-m slow]
~/bin/py -3.12 <abs>/log/roadmap/S2/8_tools/controls.py --out log/roadmap/S2/8_controls.tsv
~/bin/py -3.12 -m port.test.framegate cov_r_s2_screens --no-probe --out log/roadmap/S2/8_fg_base.tsv
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --dll $SG_PORTSIM_DLL --jobs 4 --out <tsv>   (Win32: --backend win32 after
    building port/build/Win32 once with build.py --arch Win32 and --target portsim_runner32)
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out log/roadmap/S2/8_boot_gate.tsv
```
Tools in log/roadmap/S2/8_tools/: patch_*.py (the edits as applied to the mirror; their replacements let a line break match blank lines),
gen_shortcuts.py, controls.py, make_patch.py, merge_goldens.py. 4_tools/va2sym.py maps the original's data addresses to model symbols.

## Open

- U-1278: duplicate body names with S2.9's dialogs family; the lead decides the owner (reported 2026-10-24).
- U-1276: rep_report1 / preferences after S2.9 merges: `-m port.test.test_gfx_reports rep_report1 preferences`.
- U-1272 hire's employees (S3), U-1273 histograph event-marker text (GDI clip region), U-1274 hover images of the five loops without a
  cursor read, U-1275 the financial report's growing g_textBuffer (pre-existing), U-1277 the display's vtable.
- Cleanup after acceptance: port/build/S2_8_head, port/build/S2_8.
