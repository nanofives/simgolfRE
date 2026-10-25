# S2.4 notes: the routing map (showRoutingMapScreen) and the load browser (loadGameBrowserScreen)

Status: PARTIAL. cov_k1_routing `routing` and cov_a_keys `k_dlg_L` are 0 px; cov_a_keys `k_dlg_R` is 370 px (was 476,812),
all inside the map's hole overlay that S3.4 / S3.5 / S3.8 own (U-1231). Every regression gate is unchanged. All work is in the mirror `port/build/S2_4_head` (a copy of
`port/build/S2_3_head`'s tree: git archive of HEAD c6d7d78b + S2.1's files + 1_test_static.patch + S2.3's files and 3_shared.patch;
HEAD_COMMIT inside; junctions log/, original/, port/corpus/, re/replay, re/versions, shim/build, tests/fixtures, tests/golden made
with PowerShell New-Item -ItemType Junction). Build: x64 `port/build/S2_4` through log/roadmap/S2/build.py; Win32 in the mirror's
own `port/build/Win32` (built on demand by SG_BACKEND=win32). Merge: log/roadmap/S2/4_merge.md.

## What the original does (sources cited per call in the port files)

- showRoutingMapScreen (c3et.cpp ShowRoutingMap_re): per redraw pass, the bottom art and the mode's screen (g_spriteRef_8224c8
  cells, only while g_map822b80[0] is 0), the display scale (1, 1, 5), two passes of tile diamonds (drawRoutingPoly =
  fillConvexPoly on the root window's surface, c3dv.cpp / c3db.cpp: pass 1 0x80001284 under tiles whose unk_0058dd34 type byte is
  0xff, pass 2 the mode's colour: modes -1 / 0 by the type record byte +0x26 through a jump table and tileBlocked, mode 1 the two
  shade grids, mode 2 rateLot / evalPlacementArea), the title in g_fontRef_821f28, `ROUTING MAP` in g_fontRef_821020, the mode's
  legend in g_fontRef_821ee8, per hole the list row (mode -1: the selection highlight as two fillWindowRect, the number, par,
  yards, the average minutes) and its map overlay (tee sprite, line to the waypoint, the hole number drawCenteredText2 in
  0x80007ff0, line to the green, flag sprite), the walker list (mode 0), the scale back to (1, 1, 1), the hovered button's image,
  the mode's own button, the compass, Window::refreshFull.
- loadInfoScreenArt (c3fv.cpp LoadInfoScreenArt_re) cuts every report screen's art at boot; the routing map's 39 cells come from
  it. The port's boot (tick_boot.inc) made only its five fonts; its image half is now gfx body `loadInfoScreenArt.images`.
- loadGameBrowserScreen (c3fw.cpp LoadGameBrowserScreen_re): a 15-element frame Surface array and a frame Image; loads
  interface\Title_LoadGame_MO (7 cuts) and interface\Title_LoadGame (5 cuts); per redraw the Image whole onto the root
  (Surface_blitThunk), the two disabled buttons when nothing is selected, the title (g_fontRef_519a40), the scroll bar
  (fillWindowRect), up to 16 names (g_font51b360, drawText4049d0 at x 0x140, rows of 16), `(more...)`, the selection's details
  page, Window::refreshFull; hover changes redraw the buttons, a dwell above 0x1e draws the tip (blitPanel style 0 once,
  drawCenteredText2), the load path draws `Loading...`.
- drawCenteredText2 (c3dq.cpp): Widget_setQuad(target, colour, 1, 0, 1) + drawCentered2: the run drawer (c3et.cpp
  AllocTextBuffer_re) draws the shadow run in palette colour 1 at (+0, +1) first, then the main run.
- fillWindowRect / Win_fillRect (c3db.cpp) -> Widget_fillArea (c3ab.cpp) -> Surface slot +0x44 = jgld Surface::fillRectDispatch
  0x10009770 (re/match/jgld_surface.cpp; the slot read from the vtable 0x1011d0b0 in original/jgld.dll: +0x44 -> thunk
  0x10001172 -> 0x10009770) -> fillRect16 (re/match/jgld_surface16.cpp): bit-31 colour's low word, IntersectRect with the clip.
- fillConvexPoly16 (c3db.cpp) with polyEdgeStep (c3x.cpp), polyEdgeAdvance and fillSpanDown (c3t.cpp): spans [x0, x1) clipped
  to [clip left, clip right - 1), rows while row < clip bottom - 1.
- blitPanel (c3da.cpp): style 0 tiles g_sprites5a53c8 cell 9 (the core's loadInterfaceArt.images cuts it), other styles the nine
  g_surfaces541580 cells; each tile through Surface_blit2 -> Sprite::drawG with g_color16Lut (gfxb::draw_g16).
- Display::setScale = display slot +0x9c (re/names/jgld_3.tsv; c3ei.cpp displaySetScale).

## What changed (mirror paths)

New family files (each header lists bodies, specification and negative-control modes):
- port/src/sim/gfx_prim_fill.cpp, family `prim_fill`: fillWindowRect, Win_fillRect, blitPanel, Display::setScale; exports
  gfxf::fill_rect16, gfxf::fill_convex_poly16, gfxf::blit_panel, gfxf::blit_scale.
- port/src/sim/gfx_prim_text2.cpp, family `prim_text2`: drawCenteredText2 (shadow = run palette entry 1 as RGB (0x80, 0, 0), U-1230).
- port/src/sim/gfx_routing.cpp, family `routing`: loadInfoScreenArt.images (the whole manifest in the original's order; the route
  buttons' rectangles from f_tab_layoutInts_4ba62c through ctx.exe(): it is .rdata, not in the GameState), drawRoutingPoly.diamond.
- port/test/test_gfx_routing.py (35 tests).
Shared-file edits (log/roadmap/S2/4_shared.patch, LF, against the working tree after 3_shared.patch):
- gfx.cpp: Image_draw reports a display scale other than (1, 1, 1) (`Image_draw: a display scale other than (1, 1, 1)`) instead
  of drawing unscaled; declaration of gfxf::blit_scale.
- gfx_families.inc: SG_GFX_FAMILY(prim_fill), (prim_text2), (routing).
- routing.cpp: struct MapDraw, the original's drawing calls of one redraw pass, made only with a backend attached; redraw() takes
  the hovered button and the selected hole. A text the original builds in g_textBuffer (numbers, `m` minutes, mode 0's title
  suffix, mode 2's `Building a` line) is built there, drawn and the 0x1000 bytes put back, so a pass with a backend leaves the
  GameState as a pass without one. Negative control showRoutingMapScreen 10 (highlight one row lower). The hole overlay's
  sprites and lines are drops without a body (showRoutingMapScreen.teeSprite / .line / .flagSprite), the employee mode's walker
  list one more (showRoutingMapScreen.walkers): reported in gfx.log.
- saveload.cpp: the browser's drawing (browser_setup / browser_teardown, the redraw, hover, tip, click-off and loading-message
  calls; details_text(ctx, draw) draws between the existing text steps, which already leave the original's strings in the text
  buffer). Negative control loadGameBrowserScreen 9 (names one pixel right).
- tick_boot.inc: gfx_drop("loadInfoScreenArt.images") after the five loadInfoScreenArt fonts.

## Gate (logs in log/roadmap/S2/4_*)

| gate | result | log |
|---|---|---|
| cov_k1_routing routing | PASS, 0 px (S2.3: 475,025 px) | 4_census_x64.tsv, 4_test_routing_x64.log |
| cov_a_keys k_dlg_L | PASS, 0 px (S2.3: 440,683 px) | same |
| cov_a_keys k_dlg_R | 370 px, every region but `view` 0, bbox x 410..468 y 367..399 (S2.3: 476,812 px): the hole overlay's tee / flag sprites and GL lines (U-1231) | 4_fg_dev.tsv, 4_img/ |
| census, 64 scenarios / 609 rows, against S2.3's raw 3_census_x64.tsv | PASS 48 -> 50 (both raw, U-1120 rows NOT_RUN in both); same 89, better 508 (2 to PASS, the rest fewer px), worse 12: all UNSUPPORTED before and after with the same first unsupported call (no body: Widget_writePixel) at 96.5 .. 99.9 % of the frame wrong; their px grew by 344 .. 1,012. Two causes: (a) cov_a_keys in-game frames after the routing map / browser: the backend does not draw the in-game frame, so the root surface still holds the last full screen, now the routing map instead of an older screen (cov_a_keys k_B rendered with S2.3's and this DLL: 472,940 changed px, all leftovers); (b) the character editor rows (cov_a_career career_editor, cov_a_golfer button_0 / button_3, cov_a_keys3 q_editor): the editor's frame stays unsupported and now some of its primitives (fillWindowRect, drawCenteredText2, blitPanel) draw | 4_census_x64.tsv / .log, 4_census_compare.log, 4_census_worse.log |
| test_gfx_routing | 35 passed x64, 35 passed Win32 | 4_test_routing_x64.log, 4_test_gfx_x64.log, 4_test_w32.log |
| test_gfx + test_gfx_credits + port test_static | 25 + 47 + 6 passed x64 (113 with routing); test_gfx + credits 72 Win32 (107 with routing) | 4_test_gfx_x64.log, 4_test_w32.log |
| test_commands, test_sve_roundtrip, test_tick (files touched feed them) | 83 passed / 44 skipped, 15 passed, 250 passed (S2.3: 83 / 44, 250) | 4_test_commands.log, 4_test_sve_roundtrip.log, 4_test_tick.log |
| segments | 205 / 205 PASS x64 and Win32; compare with log/p5/segs_*_loc.tsv: 0 differ | 4_segs_*.log, 4_segs_*_compare.log |
| boot | all 67 rows equal to S2.3's 3_boot_gate.tsv in status, records, expected (PASS 62, KNOWN 2, NO_FRAMES 1, FAIL 2; HEAD's 61: 58 / 2 / 1) | 4_boot_gate.tsv, 4_boot_compare.log |
| boot with gfx attached (framegate's own boot verdict) | PASS on cov_a_keys and cov_k1_routing: the drawing leaves every sim hash equal | 4_census_x64.tsv boot_status |

## Controls

FRAME RED (test_gfx_routing.py test_frames_negctl_red, k_dlg_r_labels): prim_fill 1 (fill one row short), prim_fill 3 (polygon
span one pixel longer), routing 1 (route buttons cut from the next table entry), routing 2 (diamond one pixel wider), routing 3
(route screens one row short), routing 4 (no report art), showRoutingMapScreen 10 (highlight one row lower),
loadGameBrowserScreen 9 (names one pixel right), prim_text2 1 (no shadow) and 2 (shadow offset (1, 1)) on k_dlg_R (its px grow
above the 370 baseline). UNIT RED (blind on the frames, said why): prim_fill 2 (style 0 tiles: only the browser tip, not on a
gated frame), prim_fill 4 (Display::setScale state: no Image_draw runs at another scale on the frames), plus the unit tests for
prim_fill 1 / 3 and prim_text2 1 / 2. The diamond's shape is also checked against a second implementation of the scan written
from the transcriptions (test_diamond_equals_the_scan_model, four positions incl. both clip edges).
prim_fill 2 measured on the three frames: k_dlg_R 370, k_dlg_L 0, routing 0 px, unchanged (4_negctl_prim_fill_2.log): an
equivalent mutant there, RED in test_blit_panel_negctl_2_red. Total: 12 modes RED (10 on the frames, 2 only in unit tests), 0 blind.

## Reproduce (mirror root port/build/S2_4_head)

```
~/bin/py -3.12 <abs>/log/roadmap/S2/build.py <abs>/port/build/S2_4_head <abs>/port/build/S2_4 --log log/roadmap/S2/4_build.log
env SG_PORTSIM_DLL=<abs>/port/build/S2_4/Release/portsim_c.dll ~/bin/py -3.12 -m pytest port/test/test_static.py port/test/test_gfx.py port/test/test_gfx_credits.py port/test/test_gfx_routing.py -q -p no:cacheprovider
env SG_BACKEND=win32 ~/bin/py -3.12 -m pytest port/test/test_gfx.py port/test/test_gfx_credits.py port/test/test_gfx_routing.py -q -p no:cacheprovider
env SG_PORTSIM_DLL=... ~/bin/py -3.12 -m port.test.framegate cov_k1_routing --checkpoints routing --no-probe --out <tsv>
env SG_PORTSIM_DLL=... ~/bin/py -3.12 -m port.test.framegate cov_a_keys --checkpoints k_dlg_R,k_dlg_L --no-probe --out <tsv>
env SG_PORTSIM_DLL=... ~/bin/py -3.12 -m port.test.framegate --all --jobs 2 --no-probe --out <abs>/log/roadmap/S2/4_census_x64.tsv
~/bin/py -3.12 log/roadmap/S2/3_compare_fg.py log/roadmap/S2/3_census_x64.tsv log/roadmap/S2/4_census_x64.tsv
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --dll <dll> --jobs 4 --out <tsv>   (and --backend win32)
env SG_PORTSIM_DLL=... ~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out <abs>/log/roadmap/S2/4_boot_gate.tsv
```
log/roadmap/S2/4_tools/va2sym.py maps the original's data addresses to model symbol + offset (used to write the port code).

## Open

- U-1230..U-1234 (4_uncertain.tsv). k_dlg_R: 370 px left in the hole overlay (U-1231), owned by S3.4 / S3.5 / S3.8.
- Cleanup after acceptance: the mirror port/build/S2_4_head and the build port/build/S2_4.
