# S2.3 notes: the credits screen (showCreditsScreen) and the surface / image lifecycle primitives

Status: DONE. All work is in the mirror `port/build/S2_3_head` (git archive of HEAD c6d7d78b, HEAD_COMMIT inside, with junctions
log/, original/, port/corpus/, re/replay, re/versions, shim/build, tests/fixtures, tests/golden) plus S2.1's files
(log/roadmap/S2/1_files.txt) and 1_test_static.patch. Builds: x64 `port/build/S2_3` (the census) and `port/build/S2_3b` (final
code, every other gate), both through log/roadmap/S2/build.py; Win32 in the mirror's own `port/build/Win32` (built on demand by
SG_BACKEND=win32). Merge: log/roadmap/S2/3_merge.md (new files from the mirror + log/roadmap/S2/3_shared.patch, which applies to
the current working tree).

## What the original does (sources: shim/src/re/c3en.cpp ShowCreditsScreen_re, re/analysis/video/0044b9c0_showCreditsScreen.md)

Two Surfaces (SpriteRef objects) and one Image on the stack; the Image gets an 800 x 600 surface (Surface_createFull), the
background `creditsbckgrd.pcx` (string 0x004d21cc = pool f_strpool_4d21a4 + 0x28) is loaded with flags 1 and taken whole by the
first Surface (Surface_blitTakeDirty, trim 0), the logo `bink64.pcx` (0x004d21c0 = pool + 0x1c) is loaded and taken by the second
Surface at the size its own surface reports (slots +0xdc / +0xd8), trim 1, then Image_releaseAll. The lines of section `CREDITS`
(0x004d21b8 = pool + 0x14) of `CREDITS.txt` (0x004d21ac = pool + 8) are read; setDrawTarget(Root, g_fontRef_821020),
Widget_setQuad(Root, 0x80007fff, -1, 2, 2). Each pass: Image_draw(bg, Root, 0, 0, 0), the lines from y down (a line equal to `$bink`
= pool + 0 draws the logo with Surface_drawRegion at ((0x320 - logo +0x10) / 2, y), any other drawText(Root, s, 0x32, y,
strlen)), Window::refreshFull, waitTicks(1, 0).

Facts established for the bodies (each from the cited source):
- Image's vtable 0x004ba858 slot +0x10 is return0_479820 (exe bytes at 0x004ba868), so loadImageData always takes the 8-bit
  paletted decoder for an Image (c3ee.cpp LoadImageData_re's test); loadImage flag 1 uses the Image's own palette cache +0xac
  (LoadImage_re), filled with the file's entries 0..0xff like flag 2's fresh palette.
- Surface_drawRegion calls the handle's slot +0x5c (c3be.cpp); in jgld's Sprite vtable 0x1011d380 slot +0x5c is the thunk
  0x10001041 -> Sprite::draw 0x10015180 (slot +0x44, Image_draw's, is drawB 0x10015480). Sprite::draw's 8-to-16 case is
  draw8to16 = thunk 0x10001168 -> draw16_1001cde0, whose loops test `cmp byte [esi], 0xff; jae` (re/match/jgld_blit16.cpp), so only
  0xff is transparent; drawB's draw16_1001bc40 tests 0xfe.
- Surface_blitThunk(this, dst, x1, y1, x2, y2, w, h) = Surface_blit3(this, dst, x1, y1, w, h, x2, y2, w, h) (c3da.cpp note).
- drawText: NULL text, no font at +0x5c or font +4 == 0, or min(strlen, limit) <= 0 draw nothing; otherwise drawRichText ->
  drawTextRun with the main colour +0x6c and the shadow +0x7c (-1 = none) (c3ek.cpp, c3et.cpp notes).
- The kept lines are stripped and trimmed (Reader::readLine 0x00488230 runs stripNewline 0x004925d0 and trimSpaces 0x004925b0);
  the reference frames show them at x 0x32 with no leading space.
- Asset bytes measured: creditsbckgrd.pcx 800 x 600 has no byte 0xfe or 0xff; bink64.pcx 64 x 64 has 29 bytes 0xfe and none 0xff.

## What changed (mirror paths; details in 3_merge.md)

- New family files: port/src/sim/gfx_surface.cpp (`surface`), gfx_prim_text.cpp (`prim_text`), gfx_credits.cpp (`credits`); each
  header lists the bodies, the specification of each and the negative-control modes.
- gfx_body.h: Gfx::family_state + state_of<T>(family) (prim_text keeps the per-target quad, credits the kept lines).
- gfx.cpp: load_image accepts flag bit 0 (was always unsupported).
- commands_screens.inc / commands.cpp: show_credits_screen makes the drawing calls (no GameState write); `#include gfx.h`.
- tick.cpp: TraceInput::clock calls gfx_progress_fast(clock_pos + 1) per served read (negctl tick.frame 11 turns it off). Without
  it every credits snapshot was taken at the end of the input (the port's frames all showed the last scroll position, 13,489 ..
  20,374 px off), because the scroll consumes clock reads and no dispatch or cursor read.
- port/test/test_gfx_credits.py (47 tests).

## Gate (logs in log/roadmap/S2/3_*)

| gate | result | log |
|---|---|---|
| cov_r_quit credits, quit_00..03 | 5 PASS, 0 px in every region (was 479,973 .. 479,975 px) x64; Win32 via test_gfx_credits frames | 3_framegate_v2.tsv, 3_test_w32.log |
| framegate sandbox_basic + cov_b_location + cov_r_quit | 30 rows: 25 equal to S2.1's 1_framegate_x64.tsv, 5 better, 0 worse | 3_framegate_x64.tsv, 3_framegate_x64_compare.log |
| census (all 64 scenarios, 609 rows) | 604 equal to re/roadmap/frame_census.tsv in status and px, 5 better (credits), 0 worse; PASS 45 -> 50, UNSUPPORTED 525 -> 520, NOT_RUN 39 (the U-1120 scenarios re-labelled KNOWN as S0.3 did: 3_rerun_noend.py + 3_census_patch.py) | 3_census_x64.tsv, 3_census_x64_known.tsv, 3_census_compare.log |
| test_gfx_credits | 47 passed x64, 47 passed Win32 | 3_test_unit_x64.log, 3_test_frames_x64.log, 3_test_w32.log |
| test_gfx + port test_static | 31 passed x64 (25 + 6); test_gfx 25 Win32 | 3_test_gfx_x64.log, 3_test_w32.log |
| test_commands, test_tick (shared files touched) | 83 passed / 44 skipped, 250 passed (S0 baseline: same) | 3_test_commands.log, 3_test_tick.log |
| segments | 205 / 205 PASS x64 and Win32; compare with log/p5/segs_*_loc.tsv: 0 differ | 3_segs_*.log, 3_segs_*_compare.log |
| boot | 67 rows equal to S2.1's 1_boot_gate.tsv in status, records, expected; HEAD's 61: 58 PASS / 2 KNOWN / 1 NO_FRAMES | 3_boot_gate.tsv, 3_boot_compare.log |

## Controls (15 new modes, all RED; 1 old mode blind)

On the five credits frames (px per checkpoint quit_00..03, credits, 3_negctl_frames.log): prim_text 1 black text 1,791 .. 12,584;
prim_text 2 text one pixel right 1,685 .. 11,873; credits 2 line k with line k + 1's text 2,891 .. 18,432; tick.frame 11 no
snapshot at clock reads 13,489 .. 20,374. Equivalent on those frames (0 px, measured for prim_text 3, surface 4, credits 3) and
RED in unit tests instead (U-1223): surface 1 / 2 / 3 / 8 (lifecycle: an erased object still drawn), surface 4 (trim off: the
logo's kept width 6 instead of 3 moves the centred logo), surface 5 (0xff drawn, on an untrimmed border), surface 6 (trims
ignored), surface 7 (thunk corners swapped), prim_text 1 / 2 / 3 (drawText with the GDI provider), credits 1 (logo at 0x32),
credits 2, credits 3 (a `   $bink` line no longer matches). The loadImage flag-0 change has no switch: its control is the S2.1
build, on which the five frames differ by 479,973 .. 479,975 px, plus test_load_image_flag0_*. gfx.attach 3 stays blind (U-1200:
the background has no 0xfe).

## Reproduce (from the mirror root port/build/S2_3_head)

```
~/bin/py -3.12 <abs>/log/roadmap/S2/build.py <abs mirror> <abs>/port/build/S2_3b --log log/roadmap/S2/3_build_b.log
env SG_PORTSIM_DLL=<abs>/port/build/S2_3b/Release/portsim_c.dll ~/bin/py -3.12 -m pytest port/test/test_gfx_credits.py -q -p no:cacheprovider
env SG_BACKEND=win32 ~/bin/py -3.12 -m pytest port/test/test_gfx_credits.py port/test/test_gfx.py -q -p no:cacheprovider
env SG_PORTSIM_DLL=... ~/bin/py -3.12 -m port.test.framegate --all --jobs 2 --no-probe --out <abs>/log/roadmap/S2/3_census_x64.tsv
~/bin/py -3.12 <abs>/log/roadmap/S2/3_rerun_noend.py <championship|cov_a_champ|cov_k3_career> > <abs>/log/roadmap/S2/3_rerun_<s>.json
~/bin/py -3.12 log/roadmap/S2/3_census_patch.py <census> <out> <rerun jsons>; ~/bin/py -3.12 log/roadmap/S2/3_compare_fg.py re/roadmap/frame_census.tsv <out>
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --dll <dll> --jobs 4 --out <tsv>   (and --backend win32)
env SG_PORTSIM_DLL=... ~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out <abs>/log/roadmap/S2/3_boot_gate.tsv; ~/bin/py -3.12 log/roadmap/S2/3_boot_compare.py
```

## Open

- U-1220..U-1226 (3_uncertain.tsv). The most useful next checkpoint: one near the end of the scroll (the logo on screen) would
  judge Surface_drawRegion, the logo crop and the credits centring on a frame (U-1221, U-1223).
- first_unsupported_drawing against frame_census.tsv differs in 213 rows with status and px unchanged: 209 are S2.1's
  first-occurrence order (Surface_blit3 / Image_draw -> Widget_writePixel or Surface_drawScaled2; on the 30-row subset S2.3's
  column equals S2.1's 1_framegate_x64.tsv in every non-PASS row), and 4 PASS title rows (cov_b_title theme / menu2, ui_tour and
  ui_tour_v menu_done) now have no unsupported drawing call before the snapshot.
- Cleanup after acceptance: the mirror port/build/S2_3_head and builds port/build/S2_3, S2_3b.
