# S0.2 notes: port/test/framegate.py (HEAD 4bfefc2a, export port/build/S0_head)

Status: DONE. Worker session cmvm8h13e2293o115w5soougj (claude2, opus), 2026-10-24.

## What it is

`port/test/framegate.py` runs every checkpoint of a pinned scenario in ONE boot run (port/test/boot.py, the boot gate's run) with
the gfx backend attached (one gfx.snapshotAt per checkpoint at the trace ordinal of the checkpoint's clock call, as gfx.render) and
LiveTerrain attached (sg_ts_live_attach), and writes one TSV row per checkpoint. Tests: `port/test/test_framegate.py` (32: 19
static, 13 port runs). Usage and every column: the module docstring.

    cd port/build/S0_head          (copy port/test/framegate.py and test_framegate.py in first; ROOT = the export)
    export SG_PORTSIM_DLL=$PWD/port/build/S0_2/Release/portsim_c.dll
    ~/bin/py -3.12 -m port.test.framegate sandbox_basic cov_b_location course_season cov_r_quit --out log/roadmap/S0/2_sample.tsv
    ~/bin/py -3.12 -m pytest port/test/test_framegate.py -q -p no:cacheprovider         # 32 passed in 42 s
    ~/bin/py -3.12 -m port.test.framegate --all --jobs 2 --out <tsv> --resume            # the census (S0.3)

Per-scenario results are kept in `<out>.d/<scenario>.json` with a signature of the options and backend; `--resume` reuses only
those with the same signature. `--jobs` is capped at 2 (`MAX_JOBS`, effective_jobs()); with more than one scenario every scenario
runs in its own process (`--worker`), as boot.py --jobs does.

## Row format (for S0.3's ranking)

status PASS / FAIL / UNSUPPORTED / NOT_RUN with detail = px by region (FAIL), the first unsupported DRAWING call (UNSUPPORTED) or
the boot status / reason (NOT_RUN: no trace, no snapshot, boot FAIL at or before the snapshot's frame). Columns kind, context,
frame, wait_ticks, terrain (yes / no) + terrain_rule, total_px, one_level_px / pct (of the frame), beyond_px, regions
(`name=px/one/pct/beyond;...`, pct of the region's area, regions partition the frame so they add up to total_px), unsup_before
(distinct reasons before the snapshot, gfx's own count), first_unsupported, first_unsupported_drawing (NOT_DRAWING excluded),
unsupported_before (`|`-joined, in first-occurrence order), unsup_order (gfx / probe / final / final+probe / partial / none),
boot_status + boot_records, liveterrain (object address, interface calls, render passes, init), backend, variant (negctl / swapped
reference), reference. A difference is per 5-bit channel level: the frames are RGB555 expanded to 8 bits, so `>> 3` gives the
level; one-level = the largest channel difference is 1.

## Screen kinds and regions (framegate.py: INGAME_REGIONS, LOCATION_REGIONS, menu_region_list)

- title: gfx.menu_regions (labels first, then buttons and the logo) + background.
- location (courseActionMenu, saveload.cpp): loc_title (heading centred at (0x7e, 0x13) / (0x7e, 0x21)), loc_cash (0x2e4, 0x12),
  loc_legend (plates at y 0x226), loc_buttons ((0x2d4, 0x198), (0x2d4, 0x1ca), (0x2dc, 0x214)), loc_map = the rest.
- ingame (mainLoop's frame, tick_frame.inc): hud_title (P6_Hud: (3, 3), (0x30, 5), text at x 0x90, counts row y 0x26),
  hud_meters (P6_StatusBar boxes (0x289, 0xd), (0x2a5, 0x37), (0x2bb, 0x63)), hud_message (the line centred at (0x190, 0xa)),
  toolbar (showMainRadialMenu: base image at (0, 0x1b3), c3fi.cpp), bottom_panel (P6_Panels' panels and P6_Hud's golfer strip
  from x 0xcc, y 0x23f), view = the rest. Anchors are the code's; extents measured on sandbox_basic/sandbox_00 and
  cov_a_build/bldg_8 (cited in the source).
- frame: anything else (the credits, unknown boot screens): the whole frame.

## Where a checkpoint is (classify(), from the trace)

The frame holding the checkpoint's clock call (0 = the boot) and the clock-call sites from that frame's head to the call
(function names from re/functions_ghidra.tsv ranges). waitTicks and jgld's QPC are shared by every loop, the H1 wait's own read
(0x0041e631..0x0041e6b1, time_model.md) is the frame's: HEAD (the frame head's read), H1WAIT (at most H1_WAIT_READS = 11 reads
after the last H1 read: one waitTicks(1, 0) = 11 reads; 2_h1_tail.py measured 0..11 for every in-frame checkpoint in the wait and
>= 2,021 for the 18 that had left it), MODAL:<fn> / MODAL_AFTER_H1:<fn> (another function read the clock), AFTER_H1:title (left
the frame: one QPC read after the wait, showTitleScreen's palette, and a waitTicks(1, 0) loop) / AFTER_H1:waitTicks (left the
frame into a waitTicks loop with no QPC: prompts at 10 ticks, textInputBox at 1), BOOT:<fn>. wait_ticks = n of the waitTicks(n, 0)
loop from the period of its reads (10 n + 1). Boot kinds: 1 tick = title (tick_menu.inc ML+0x11a69), 5 ticks or a last read in
pumpAndRedraw with courseActionMenu.setup drawn = location (U-1112).

Census of the contexts over all checkpoints from the traces alone (2_context_census.py -> 2_context_census.tsv): 609 checkpoints
(not 649: U-1115), 34 without a trace; H1WAIT 480, HEAD 7, MODAL:waitTicks 21, MODAL:pumpAndRedraw 10, AFTER_H1:waitTicks 4,
AFTER_H1:title 9, MODAL_AFTER_H1:showCreditsScreen 5, BOOT title 34, BOOT location 5.

## Terrain on screen (terrain_rule())

At HEAD no terrain pixel reaches the gfx back buffer (terrain_state's draw passes set only the view quadrant) and the run exposes
no order between Terrain passes, 2D writes and a snapshot (U-1110), so the rule comes from the frame code and the trace:
mainLoop's FramePreamble calls updateTerrainRender unconditionally (tick_frame.inc ML+0xf13), whose every path renders the course
view or restores it (blit_screen); so `yes` for every checkpoint of an in-game frame, `no` for the boot frame (title, location,
Setup, Session), for a frame that left for the title menu (AFTER_H1:title) and for a full-screen modal named by its clock read
(showCreditsScreen). Checked on reference frames (contact sheets): 24 HEAD / H1WAIT frames all show the course view; of 16 modal
ones 11 are popups over it and 5 full-screen screens (U-1111: they read yes); the credits and the 9 AFTER_H1:title frames show no
course. Census: 522 yes, 53 no, 34 no trace. The LiveTerrain column is evidence that the view exists in the port's run
(course_season: initSystem + initTerrain, 107 render passes), not part of the rule, so x64 and Win32 rows agree.

## Results (2_sample.tsv, 42 rows; seconds = boot run only, the order probe adds about 35 s for course_season)

- sandbox_basic (4.6 s): menu PASS (0 px, 0 unsupported, kind title); sandbox_00..40 UNSUPPORTED `no body: Widget_writePixel`,
  terrain yes, 479,802..479,833 px (the port's in-game back buffer still holds the location screen with some HUD text: no
  terrain, no HUD bodies).
- cov_b_location (5.9 s): refused, site_hover, buttons, tip PASS (location), back PASS (title): as test_gfx.py.
- course_season (34 s): all 12 UNSUPPORTED, terrain yes, kind ingame, H1WAIT; hole_open regions hud_title 18,944 / hud_meters
  22,381 / hud_message 7,798 / toolbar 34,260 / bottom_panel 61,532 / view 334,861 px; boot PASS 6,296 / 6,296 with both attached.
- cov_r_quit (6.0 s): menu, exit_leave, title1, exit_saved, title2 PASS (title, the latter four after the session); game, exit_*
  UNSUPPORTED terrain yes; quit_00..credits UNSUPPORTED kind frame terrain no (480,000-px frame, 0.10..0.13 % one-level).
- Win32 (2_win32_check.py with the step's own Win32 build port/build/S0_2_w32): sandbox_basic menu + sandbox_00 rows equal the
  x64 rows in every column but liveterrain (n/a) and backend (2_backend_x64.json / 2_backend_win32.json).

## Controls

- gfx negctl 1 on menu: FAIL, 49,162 px (2_ctl_negctl1.tsv; test_negctl_gfx1_menu_reads_fail).
- swapped reference (cov_b_location buttons against tip's frame): FAIL, loc_buttons 1,412 px (2_ctl_swap.tsv; test).
- sim hashes: boot PASS with every record (23/23, 439/439, 6,296/6,296, cov_r_quit PASS) with gfx and LiveTerrain attached, at the
  pin recording's Terrain address (0x38f8040 sandbox_basic, 0x3908040 course_season) and at LiveTerrain's own (terrain_va=0 ->
  0x2840000): boot.run compares every frame record with log/p2/p24/<scenario>/c_a/framehash.bin, the boot gate's reference
  (test_sim_hashes_unchanged_with_both_attached). cov_b_location has no frame record (NO_FRAMES, as in the boot gate).
- Classifier: static tests pin HEAD / H1WAIT / MODAL / AFTER_H1 / BOOT contexts and kinds on sandbox_basic, course_season,
  cov_b_location, cov_k2_simfoto, cov_r_quit, cov_k3_career; NOT_DRAWING is checked against tick.cpp's kDrops reasons.
- Blind: the region rectangles are not judged by a control (a wrong rectangle moves px between regions, totals unchanged).

## Defects found on the way

- gfx.render stops one frame early for a last checkpoint in an H1 wait (no snapshot: course_season hole_open alone); framegate runs
  one frame further and to the end past the last FRAME record (U-1114, 2_merge.md item 4).
- gfx.log counts audio / input / pacing drops as unsupported drawing (Snd::status, playSound, capsLockOff ...): framegate's
  NOT_DRAWING keeps them out of the status (2_merge.md item 3).

## Open

U-1110..U-1115 (2_uncertain.tsv); merge requests 2_merge.md (items 1-2 make the order and the terrain flag exact from the run).
For S0.3: the order probe (SG_TICK_TRACE + SG_TICK_CALL_LIMIT 3,000,000 in a second process, trace deleted after) costs about
35 s per scenario with unsupported calls; `--no-probe` skips it (unsup_order then `final` / `partial`), `--probe-limit` widens it.
