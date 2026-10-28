# S3.6 notes: Widget_writePixel, the in-game Surface_blit3 and the HUD panels

Worker session cmvn220ah5iupo115bsu69hor (claude3 after the license move), lead cmvmns8sn3fsbo115g4dgm1nf. Status: PARTIAL
(two bottom-panel modes are handed over and reported, not drawn: section 5).

## 1. Build

`log/roadmap/S3/6_mirror.py [--w32] [--no-build] [--no-hud]` makes port/build/S3_6_src (S2_4_head + S3 wave 1 + this step's
files + the edits of `6_edits.py`) and builds port/build/S3_head/port/build/S3_6 (x64), S3_6_w32 (Win32), S3_6_base (no edits).
Tests run from port/build/S3_6_src with SG_PORTSIM_DLL at the build (the gate runs used a copy in S3_6_run so a rebuild never
touches a running DLL). Both builds compile without warnings.

## 2. The pixel family (done first, at the lead's request)

port/src/sim/gfx_pixel.cpp. Widget_writePixel (c3fu.cpp WidgetWritePixel_re): the low word of the colour at (x, y) on the root
or a hud family surface, nothing outside it (signed tests). Widget_fillRect (c3ae.cpp, jgld Surface::drawLine -> line16 of
c3bc.cpp): x1 == x2 vline16, y1 == y2 hline16 (ends ordered, clipped, both ends drawn, a zero-length run draws nothing), else
Bresenham over the four octants with line16's pre-walk and early exits; a palette colour index is reported.

Finding: the census rows of S2's credits (cov_r_quit quit_00 .. credits) reported Widget_writePixel only because a run's
unsupported list is cumulative; the in-game frames before them call it (mainLoop's type-0x11 tile decoration), the credits do not.

Oracle: port/test/test_gfx_pixel.py, 17 tests: unit tests on the root (low word, the four out-of-bounds sides, fillRect ends,
clip, zero length, the two-pixel ball mark, the palette report, one pixel per column on a diagonal) and the frame test: mainLoop's
terrain-tool window separator (Widget_fillRect 0x11a..0x21a at y 0x40, colour 0x0210) equals the original's back buffer on
cov_a_keys k_render_dot and k_render_slash. Controls RED: mode 1 (pixel one row lower, unit test), mode 2 (colour bit 0 flipped,
unit test and both frames). No pinned reference isolates a writePixel decoration pixel over the port's terrain-less frame
(U-1355). Widget_fillRect is also registered by S2.8 / S2.9 (U-1356, merge section 2).

## 3. The hud family

port/src/sim/gfx_hud.cpp, every body written from the C3 / C4 hook named in its comment:

- Surface_blit3 beyond the core's 8-bit 1:1 case: a 16-bit source (root or a family surface), a non-root destination (a family
  surface made root-sized at first use: P5_tournament's F+0x36c and the routing render's F+0x624 Images), nearest-pixel
  magnification (U-1350).
- loadInterfaceArt(1)'s whole image half: 48 tables generated from c3fv.cpp into the GENERATED block
  (`log/roadmap/S3/6_gen_iart.py`, `--check`); loadWorldAssets' build-palette cuts (terrain buttons, tool strip, layout sheet).
- Surface_blit2 / Surface_drawScaled2 / drawScaledSprite421b90 (Sprite::drawG; the shade table by value, U-1351),
  Surface_drawScaled / drawScaledSprite421b60 under a display scale: jgld draw16_1001bc40 (float scale, the 16.16 steps, the
  x-mirrored scaled body; the golfer heads at (1, 1, 2) and (-1, 1, 2), U-1357); uncut SpriteRefs draw nothing (U-1352).
- drawFrameR3, drawFrameS2, drawTerrainToolbar, showMainRadialMenu's drawing, showInfoCursorPanel's drawing (tool icons by
  state, hole icon, labels centred with drawCenteredText at 0x004338bb), drawAmenitiesPanel with drawAmenityRow's strip and
  icons, drawGolferActionPanel (buttons, the five shot buttons by style and lie, the start buttons, the bag of clubs) and
  drawMatchScoreboard (names and the challenge number from the registered buildGolferName / appendNumber run over a saved and
  restored GameState, so the body leaves no write).
- Reported, never drawn by this step: hover tips (`tooltip:`, U-1353), the sprite queue (`addSprite:`, U-1354), the golfer list
  and detail panels (section 5).

Controls (g_negctl_gfx_hud): 1 blit3 one row short, 2 magnification one source column right, 3 the art loads do nothing,
4 every art cut takes the other trim, 5 the half-bright table never chosen, 6 Surface_drawScaled one pixel right, 7 an uncut
SpriteRef reported (the pre-S3.6 behaviour), 8 a scaled drawB one source column right, 9 shot button 4 with the next frame image,
10 the scoreboard's hole columns one pixel right.

## 4. The gate

Measure (`log/roadmap/S3/6_hudgate.py <out> <scenarios>`): the port draws no terrain or scene yet, so the framegate runs twice
with the test-only root fill SG_GFX_HUD_BGFILL=0x0000 / 0x7fff after every present (gfx_hud.cpp after_present, edit 4); a pixel
equal in both runs was drawn opaquely in the snapshot's frame and is compared with the reference in hud_title, hud_meters,
toolbar and bottom_panel. A row is clean when no unsupported reason before the snapshot is this step's (OWN_PREFIX).
`6_sheet.py` writes per-row crops and bounding boxes, `6_attrib.py` assigns each residual group to its owner.

Census: Surface_blit3 was reported in 174 rows of exactly 19 scenarios (re/roadmap/frame_census.tsv); in the final runs of
those 19 scenarios (log/roadmap/S3/6_gate/f0/*.tsv) no row reports Surface_blit3, Widget_writePixel or Widget_fillRect. The other
45 scenarios are section 4b.

Final gate (log/roadmap/S3/6_gate.log, 6_gate/hudgate.tsv, 6_gate/sheet.tsv, 6_attrib.log): 169 in-game rows, 155 clean; the
14 not clean are ui_tour / ui_tour_v rows after the people screen, which carry this step's reported golfer list panel. Residual
opaque pixels that differ, by owner (6_attrib.py):

| px | groups | cause |
|---:|---:|---|
| 676,897 | 36 | S2 full-screen screens (tournament results, report, character editor, golfer info) |
| 16,837 | 25 | S3.8 the radial-menu tip bar (drawPanelBackdrop's drawMapLine, an OpenGL line) |
| 5,854 | 29 | S3.5 the hole flag (addSprite) |
| 5,624 | 8 | S3.8 terrain grid lines through the meters region, plus the S2 event popup's edge on zoom_box (cov_b_inject rows) |
| 3,168 | 10 | S3.6 open: the golfer list panel (rows not clean) |
| 3,166 | 15 | S2 golfer info panel portrait over the title strip |
| 1,422 | 1 | cov_a_champ n_prompt: the original's back buffer has terrain, not the HUD, at the leaderboard (modal frame) |
| 703 | 14 | S3.8 terrain grid lines through the HUD regions |
| 622 | 6 | S2 dialog frame over the meters |
| 60 | 3 | S3.5 a building sprite over a walker label |

18 small groups were attributed by looking at their crops (6_gate/sheet/<scenario>.<checkpoint>.png): cov_a_keys k_misc and
cov_b_inject / cov_b_inject2 debug_overlay (terrain grid lines, S3.8), k_dlg_S (the save-name dialog, S2), cov_a_career
career_popup (a popup, S2), cov_b_inject / cov_b_inject2 zoom_box / forced_events / end (grid lines, S3.8; the event popup over
the title strip on zoom_box, S2; the ticker text at the top edge on end, S3.7), and five single pixels (C_prompt, season x2, k_dlg_S, career_popup)
at popup or line ends. Rows with nothing but these causes: every remaining opaque HUD pixel equals the original.

Earlier passes (6_hg2 .. 6_hg8) are the history of the bodies above: the amenities palette (cov_a_build bottom panel 3,968 -> 1,132),
the scaled heads (cov_a_golfer rows clean 0 -> 28), the shot panel and scoreboard (cov_b_shots 2,000 px per shot row -> 0) and
the centred labels (cov_a_build 1,132 -> 219, cov_a_champ 3,990 -> 198).

Reproduce: `cd port/build/S3_6_src && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 ../../../log/roadmap/S3/6_hudgate.py ../../../log/roadmap/S3/6_gate <the 19 scenarios>`,
then `6_sheet.py` and `6_attrib.py` on the same directory.

## 4b. Census of the other scenarios (lead's request, 2026-10-25)

`framegate <45 scenarios + S1's 6> --no-probe --jobs 2` with the final build (log/roadmap/S3/6_census51.tsv / .log;
cov_r_quit again alone, 6_census_quit.tsv, because its boot returned -11 under --jobs 2 and ran alone; S1's six through
`6_census_s1.py`, which reads the shared repo's scenario list, 6_census_s1.tsv). Per scenario: rows, rows with an S3.6 reason
(any of Widget_writePixel, Widget_fillRect, Surface_blit3, Surface_blit2, Surface_drawScaled*, drawScaledSprite421b*, the
frame / toolbar / radial menu / info, amenities, golfer action and employee panel bodies, the art loaders), and the rows
whose first unsupported drawing call in the S0 census was Widget_writePixel:

| scenario | rows | S3.6 reason | writePixel first in S0 | status S0 -> now |
|---|---:|---:|---:|---|
| championship | 6 | 0 | 0 | 1 PASS, 5 UNSUPPORTED (unchanged) |
| course_season | 12 | 0 | 12 | 11 UNSUPPORTED, 1 UNSUPPORTED -> FAIL |
| course_season_6y | 12 | 0 | 12 | 11 UNSUPPORTED, 1 UNSUPPORTED -> FAIL |
| cov_a_build2 | 10 | 0 | 9 | unchanged |
| cov_a_build3 | 8 | 0 | 7 | unchanged |
| cov_a_flags | 11 | 0 | 0 | unchanged |
| cov_a_play | 8 | 6 | 7 | unchanged; the 6 carry drawEmployeeViewPanel (the open golfer list) |
| cov_a_quit | 15 | 0 | 14 | unchanged |
| cov_a_quit_title | 9 | 0 | 0 | 9 PASS |
| cov_b_build | 7 | 0 | 7 | 5 UNSUPPORTED, 2 -> FAIL |
| cov_b_career | 6 | 0 | 6 | unchanged |
| cov_b_career_types | 12 | 0 | 12 | unchanged |
| cov_b_holes18 | 6 | 3 | 6 | 5 UNSUPPORTED, 1 -> FAIL; the 3 carry drawEmployeeViewPanel |
| cov_b_location | 5 | 0 | 0 | 5 PASS |
| cov_b_locations | 2 | 0 | 0 | 1 UNSUPPORTED, 1 -> FAIL |
| cov_b_paths | 4 | 0 | 4 | unchanged |
| cov_b_people | 5 | 0 | 5 | unchanged |
| cov_k1_label | 3 | 0 | 3 | unchanged |
| cov_k1_magazine_d2900 | 3 | 0 | 3 | unchanged |
| cov_k1_paths | 4 | 0 | 3 | unchanged |
| cov_k1_river | 72 | 0 | 71 | 71 UNSUPPORTED -> FAIL |
| cov_k1_routing | 4 | 0 | 3 | unchanged |
| cov_k1_steps | 3 | 0 | 2 | unchanged |
| cov_k1_terrain | 5 | 0 | 4 | unchanged |
| cov_k1_washers | 5 | 0 | 4 | unchanged |
| cov_k2_build | 8 | 0 | 8 | unchanged |
| cov_k2_label | 6 | 0 | 6 | unchanged |
| cov_k3_build | 12 | 0 | 12 | 11 UNSUPPORTED, 1 -> FAIL |
| cov_k3_camera | 21 | 0 | 21 | 19 UNSUPPORTED, 2 -> FAIL |
| cov_k3_camera2 | 25 | 0 | 25 | 25 -> FAIL |
| cov_k3_career | 5 | 0 | 5 | unchanged |
| cov_k3_champ | 13 | 0 | 0 | unchanged |
| cov_k3_hole19 | 8 | 0 | 8 | unchanged |
| cov_l_bridge | 13 | 0 | 12 | unchanged |
| cov_l_camera | 13 | 0 | 13 | unchanged |
| cov_l_point | 5 | 0 | 4 | 4 -> FAIL |
| cov_r_course | 3 | 0 | 0 | unchanged |
| cov_r_quit | 15 | 0 | 14 | unchanged (run alone) |
| cov_r_reload | 3 | 0 | 0 | unchanged |
| cov_r_sve | 3 | 0 | 0 | unchanged |
| golfer_events | 10 | 0 | 0 | NOT_RUN (no trace, as in S0) |
| mood5_fixture | 4 | 0 | 0 | unchanged |
| sandbox_basic | 10 | 0 | 9 | 9 -> FAIL |
| stories9_fixture | 4 | 0 | 0 | unchanged |
| stories_fixture | 4 | 0 | 0 | unchanged |
| cov_r_bio (S1) | 7 | - | 0 | NOT_RUN: boot -11 at record 579 in this mirror (S1's chareditor code is not in it) |
| cov_r_flic_input (S1) | 4 | 0 | 2 | 2 PASS, 2 NOT_RUN: boot diverges at record 1 (S1's PumpKind::Load is not in this mirror) |
| cov_r_palette_hot (S1) | 6 | 0 | 6 | 5 UNSUPPORTED, 1 -> FAIL |
| cov_r_s2_screens (S2) | 32 | 9 | 0 | boot stops at record 771 (U-1253) for 10; the 9 carry drawEmployeeViewPanel |
| cov_r_theme_cancel (S1) | 6 | 0 | 2 | 4 PASS, 2 -> FAIL |
| cov_r_tutorial (S1) | 34 | 0 | 0 | unchanged |

So no row of the 51 reports an S3.6 reason other than the reported golfer list (18 rows in cov_a_play, cov_b_holes18,
cov_r_s2_screens). With the 19 scenarios of section 4 every census scenario that had Widget_writePixel as its first drawing
cause (49 scenarios, 477 rows in the current re/roadmap/frame_census.tsv, which holds S1's and S2's rows as well) was run
and judged, except cov_r_flic_input's 2 rows hud / end (its boot diverges at record 1 without S1's PumpKind::Load). 118 rows went UNSUPPORTED -> FAIL: no unsupported reason is
left and they fail only on pixels (the terrain is not drawn; terrain column yes in every one). Their HUD regions went through
the opaque gate (section 4c).

## 4c. Opaque HUD gate on the reason-free FAIL scenarios

`6_hudgate.py 6_gate2 course_season cov_b_build cov_b_holes18 cov_b_locations cov_k1_river cov_k3_build cov_k3_camera
cov_k3_camera2 cov_l_point sandbox_basic` (course_season_6y left out: 585 s per run, same course as course_season): 169 in-game
rows, 166 clean (the 3 carry the golfer list). Residuals by owner (6_attrib2.log): 48,329 px S3.5 amenity items in the
amenities palette (drawAmenityRow's addSprite: flower beds and benches over the empty slots, cov_k3_build / cov_k3_camera),
4,723 S3.8 radial-menu tip bar, 2,626 S3.5 hole flag, 1,345 the open golfer list, 315 S3.8 grid lines (one group with a
building over the title strip), 20 S3.5 a building over a label, 15 S2 golfer portrait, and two single-pixel groups
(cov_b_holes18 holes 3 px at the title strip's edge, cov_k1_river r20 1 px).

## 5. Open (this step's)

- drawEmployeeViewPanel (golfer list) and drawEmployeeDetailPanel: handed over by edit 7 and reported, not drawn (U-1359). The
  gate's measure cannot see a panel that draws nothing, which is how they were missed before edit 7.
- Hover tips go to S3.7 (lead, 2026-10-25), their drawMapLine bars to S3.8; the hole flag and amenity items to S3.5 (U-1353,
  U-1354). Widget_fillRect: S2's body wins when both are in the tree (U-1356).
- U-1350 (magnification and self-overlap), U-1358 (amenity rows the checkpoints reach) wait for frames that show them.

## 6. Controls and oracles, measured

port/test/test_gfx_hud.py (26 tests in the run, 29 min, x64; 23 after the blind controls left the RED list): cut geometry against the original's SpriteRef records of segment start images
(sandbox_basic, cov_a_build: more than 1,000 each, 0 differ), uncut SpriteRefs all NULL-handled in the original (cov_a_build),
HUD regions equal on 15 frames (cov_a_build 6, cov_a_golfer 3, cov_a_champ 3, cov_b_shots 3; the shot panel and scoreboard among
them). RED: modes 3 and 4 (cut geometry), 6, 9 and 10 (frames). BLIND, each measured, recorded in FRAME_NEGCTL_BLIND: mode 1 (a
16-bit copy one row short changes no frame of cov_a_golfer / cov_b_shots / cov_a_keys: the copies return the root's own content),
mode 2 (magnification one column right changes 4 pixels of cov_b_shots end, in the zoom view over terrain the port does not draw;
log/roadmap/S3/6_ctl12), mode 5 (the shade table: shaded pixels are blends, outside the opaque measure), mode 8 (scaled drawB:
the thought bubbles and heads at (1, 1, 2) / (-1, 1, 2) are drawn between checkpoints; no judged frame changes, 6_ctl8). So the
16-bit / magnified Surface_blit3 rules (U-1350), the shade-table choice (U-1351) and the scaled drawB (U-1357) have no frame
oracle yet; they are written from the matched jgld source and the C3 hooks. Mode 7 restores the pre-S3.6 report of uncut
SpriteRefs (the no-op is judged by the handle test).

port/test/test_gfx_pixel.py 17 passed (section 2).

## 7. Regression gates (final build)

- Segments: 205 / 205 PASS on x64 and on Win32 (S3_6_w32 DLL + runner), 0 digests differ from S3.1's tables
  (log/roadmap/S3/1_segs_*.tsv) and x64 = Win32 (6_segs_x64.tsv, 6_segs_win32.tsv).
- Boot gate: HEAD's 61 scenarios 58 PASS / 2 KNOWN / 1 NO_FRAMES, 0 rows differ from S0 (6_boot_gate.tsv, 6_boot_compare.log).
  Rows outside HEAD's 61: cov_r_bio, cov_r_flic_input FAIL as in S3.1's run; cov_r_s2_screens FAIL (an S2 recording in the
  shared corpus whose code is not in this mirror).
- port/test, one file per process, x64 (6_pytest_x64.log): test_static 6, test_gfx 25, test_gfx_credits 47, test_gfx_routing 35,
  test_gfx_eoi 5, test_gfx_pixel 17, test_gfx_hud 23 passed, 3 failed: the blind controls 1, 5, 8, since moved to FRAME_NEGCTL_BLIND (the file now collects 23).
- Rerun after the blind list (6_pytest_rerun_x64.log, 6_pytest_rerun_win32.log): test_gfx_pixel 17 passed and
  test_gfx_hud 23 passed on x64 (SG_PORTSIM_DLL at the build) and on Win32 (SG_BACKEND=win32, the S3_6_w32 DLL and runner
  through a junction port/build/S3_6_src/port/build/Win32), each file in its own process; the hud file's frame tests run
  framegate on the same backend.
