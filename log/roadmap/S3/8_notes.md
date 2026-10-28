# S3.8 notes: overlay lines (Terrain::drawLine and the exe's line drawers)

Worker: opus (claude2, session cmvnqcdb785r3o115e5i7g5mk), 2026-10-25. Lead: S3 (cmvmns8sn3fsbo115g4dgm1nf). No game run (no slot needed:
the real GL is replayed offline by re/tools/glexact/glreplay.exe, the in-game evidence is the pinned references and GL streams).

## 1. What the original does (cited)

- The only overlay primitive the exe reaches is Terrain::drawLine (Terrain.dll RVA 0x48a0; re/analysis/terrain/
  000048a0_TerrainDll_Terrain_drawLine.md), imported once and called only through drawMapLine 0x004493d0 (re/tools/import_callers.py).
  Terrain::drawCircle (0x42a0, trivial), drawCardinalSpline (0x4c70), drawBezierSpline (0x5230) and renderTile (0x80e0) have no caller
  in this build and are not imported by the exe (U-1375): nothing of theirs to draw.
- drawLine(x1, y1, x2, y2, c, width, alpha): glOrtho(0, W, 0, H) when the Terrain object's byte +0x28 is set, else (0, W, H, 0)
  (0x1000493f..0x100049ba), lighting / texturing off, blending (SRC_ALPHA, ONE_MINUS_SRC_ALPHA) on, glLineWidth((float)width)
  (0x10004a4e), glColor4f(((c >> 7) & 0xf8) / 255, ((c >> 2) & 0xf8) / 255, ((c << 3) & 0xf8) / 255, alpha / 10.0) (0x10004a64..),
  one GL_LINES pair, then lighting and texturing on, blending off, matrices popped, glFlush.
- Terrain::initGL enables GL_LINE_SMOOTH with GL_LINE_SMOOTH_HINT = GL_NICEST (every stream in port/corpus/gl: `glEnable 0xb20`,
  `glHint 0xc52,0x1102` after wglCreateContext), so the lines are antialiased. In every recorded stream the 2-D glOrtho is
  (0, 800, 0, 600): the flip byte is set (25,413 calls, U-1372). Viewport 0, 0, 800, 600.
- The GL context draws into the root surface (S3.2), so the lines land in gfx.root and reach the frame through Window::refreshFull.
- Exe drawers (all drops of the frame loop): drawMapLine (Terrain::drawLine with its seven arguments, ecx = [g_terrain]),
  drawMinimapSegment 0x00406220 (the same seven arguments), setLinePoint 0x0040bf00 / lineTo 0x0040bf20 (the shared point
  g_linePoint5a412c: lineTo draws (x, y) -> point with (colour, width, alpha), then stores (x, y)), drawCircle 0x00407c60 (25 steps
  of angle 0x0aaaaaaa around a world point, radius (r << 10) / 100 toward zero, worldToScreen per step; after an invisible step the
  next visible one only sets the point, the others lineTo(sx, sy, c, 2, 7); c3eh.cpp DrawCircle_re), drawPanelBackdrop 0x00432620
  (bar drawMapLine(half - w, y, half + w, y, 0x80000000, 0xa, 5) before its text), the routing map (c3et.cpp: setLinePoint at the
  tee 0x00457738, lineTo(x, y, 0x80007fff, 2, 0xa) to the waypoint 0x00457833 and the green 0x004578f0).

## 2. Windows' generic GL antialiased line, measured (log/roadmap/S3/8_probe.py)

Method of port/test/glprobe.py: synthetic scenes of the exact drawLine call sequence over initGL's state, replayed by glreplay.exe into
a 16-bit DIB with a chosen dib_pre, and compared with a model. Findings:
- Coverage at a pixel centre = clamp(w / 2 + 0.5 - |n|) * clamp(t + 0.5) * clamp(L - t + 0.5) (n distance from the line, t position
  along it from the first end, each factor in [0, 1]): a white line at alpha 1 at 16 dither phases (scene ph1, width 3) pins the
  pre-quantisation value of each pixel to a 0.5 / 255 interval, and the ramp lands in it within 1.2 / 255 everywhere.
- The segment is clipped to the viewport [0, 800] x [0, 600] first and the clipped end gets the end ramp (every beyond-one-level pixel
  of the first random scene was on a line crossing the edge: 221 -> 10 with clipping).
- Ends go through U-1076's fixed-point setup (c + (v - c)(1 - 2^-8 / h), 8 fractional bits): differing pixels of the random scene
  1,187 -> 108 (4 / 6 / 12 bits: 1,420 / 1,212 / 567).
- Width clamped to 10 (12, 15, 20, 30 draw what 10 draws, scene short); a zero-length line draws nothing; 1..3 pixel lines exact.
- alpha * coverage blended against the 5-bit destination expanded by 255 / 31 and written with the triangles' 4x4 dither: the fully
  covered interior is exact (6 colours x 6 alphas, 944 px each, 0 differ).
- Left over (U-1370): pixels with coverage under 0.13 at the edge of the footprint near 45 degrees that the real GL does not draw;
  per-column spans cut at |offset| 2.0 (w 2, 44-45 deg), 2.5 (w 3, 40), 3.85 (w 5, 40), 4.0 (w 5, 42-44), 7.0 (w 10, 40), 7.5
  (w 10, 44) against ramp reaches 2.07 .. 7.60. One-level residual: U-1371.

## 3. What was built

- port/include/portsim/sim/terrain_overlay.h, port/src/sim/terrain_overlay.cpp (module `terrain_overlay`):
  overlay::draw_line (the model above on an RGB555 buffer: GL context test, line-width state with glLineWidth(<= 0) keeping the old
  width, colour floats as the original computes them, flip / ortho, clip, snap, ramp, blend, dither), apply_state (drawLine's lasting
  state on the terrain GlRaster: lighting / texturing on, blending off, blend function, colour, matrix mode), circle_points, and the
  gfx bodies drawMapLine, drawMinimapSegment, setLinePoint, lineTo, drawCircle (registered in gfx_terrain.cpp's family table; the
  line point lives in the overlay's family state, seeded from g_linePoint5a412c; the GL context, flip byte and screen size are read
  from the LiveTerrain's Terrain object header; nothing writes the GameState). Test entries overlay.rasterLine (one drawLine into a
  mapped buffer), overlay.logStart / overlay.logWrite (the drawLine call log, `$gfx/overlay_calls.txt`).
- Shared edits (8_merge.md, 8_edits.py): modules.inc, gfx_terrain.cpp (five bodies), gfx.cpp drawPanelBackdrop's bar, routing.cpp's
  hole line through setLinePoint / lineTo.
- Controls (g_negctl_terrain_overlay, sg_negctl_set "overlay.rasterLine"): 1 hard edge, 2 alpha ignored, 3 no clipping, 4 flip
  inverted, 5 no snap, 6 lineTo keeps the old point, 7 drawCircle segments 3 wide, 8 width not clamped, 9 drawn without a context,
  10 bodies draw nothing (the frame tests' baseline).

## 4. Numbers (mirror port/build/S3_8_src; builds S3_head2/port/build/S3_8, S3_8_w32, S3_8_base)

- PROBES vs the real GL (port/test/test_terrain_overlay.py, x64 and Win32 identical, pinned): (pixels the real GL changed, differing,
  one level, beyond) rand (74,466, 108, 98, 10), widths (27,689, 88, 48, 40), short (744, 1, 0, 1), flip (18,261, 18, 16, 2); every
  scene meets G2. The C++ body equals the Python model of 8_probe.py pixel for pixel on rand.
- CONTROLS: probe scenes 1 hard edge (26,637 differ / 18,231 beyond on rand), 2 alpha ignored (61,765 beyond), 3 no clipping (221
  beyond), 4 flip inverted (128,000 beyond on rand; the test uses flip), 5 no snap (1,177 one-level = 1.58 %), 8 width not clamped
  (short), 9 drawn without a context (unit test) RED; frames: 6 lineTo keeps the old point (k_render_dot: 3,927 beyond of 4,363), 7
  circle segments 3 wide (k_render_dot: 1,458 beyond of 3,610) RED. 6 is an equivalent mutant on cov_a_keys k_dlg_R (its holes have no
  waypoint: one lineTo per hole), recorded in the test. 10 is the frame tests' baseline (the bodies draw nothing).
  8_frame_controls.log, 8_pytest_frame_negctl.log.
- GL STREAMS (8_streams.tsv): the port's drawLine calls against the original's recorded streams, count and every width / colour /
  alpha float: g2 and g4 (cov_b_holes18) 3,362 / 3,362 equal; g3, g5, g6 (cov_b_paths) 3,221 of the original's 3,234 calls, all
  3,221 equal in order: the 13 missing are tooltip bars S3.7 draws through this body (with S3.7's files, 8_combo.py: 3,234 / 3,234
  equal, 8_streams_combo.tsv). Ends are not in the streams (U-1373).
- FRAME CENSUS (8_census.sh: framegate --all with images on the base and the S3.8 build, 70 scenarios, 698 rows; 8_linegate.tsv):
  statuses unchanged (PASS 91, UNSUPPORTED 467, FAIL 127, NOT_RUN 13 both), 267 rows fewer differing pixels, 0 rows more, total
  differing pixels 10,513,480 -> 10,404,569; every `no body: drawMinimapSegment / drawMapLine / drawCircle / setLinePoint / lineTo /
  showRoutingMapScreen.line` reason gone (base 413 / 401 / 87 / 72 / 72 / 22 rows); cov_b_location's five rows stay PASS (no GL
  context on the location screen, so drawPanelBackdrop's bar draws nothing, as the original). Line pixels (the S3.8 frame against
  the base frame) on 278 checkpoints: 117,446, of them 109,292 equal to the original, 580 one level, 7,574 beyond; 381 pixels the base
  had right are now wrong. JUDGED line pixels (no other beyond-one-level error of the base frame within 3 px: a later layer the port
  does not draw leaves its own errors there, e.g. cov_a_champ end shows the title menu, cov_r_bio and the editors a 2-D screen):
  94,825, one level 339 (0.36 %), beyond 192; per row G2 252 PASS / 18 FAIL. The 18: 13 small rows whose one-level share passes
  1.5 % (4 .. 36 px, U-1377), 4 rows of P4_ballLabelDraw / thought-label bars whose width comes from Text_put (0 until S3.7's edit 7:
  cov_a_golfer button_3, mod_10, shift_tee_300, cov_a_keys end; also k_render_slash before judging), 1 row of frame-code ends
  (cov_k3_camera y_inj3, U-1376).
- With S3.7's current files (8_combo.py, development check; cov_a_keys + cov_a_golfer, 8_linegate_combo.tsv): the bars get their
  widths; the cov_a_golfer rows then show the golfer info panel the port does not draw over the bars (S2's), not line errors.
- REGRESSIONS: segments 205 / 205 PASS x64 and Win32, 0 status / digest differences from the committed tables
  (log/roadmap/merges/S2S3-5/segs_*.tsv) and x64 = Win32 (8_compare.log); boot 70 rows, 0 differ from the committed table, HEAD's 61
  at 58 PASS / 2 KNOWN / 1 NO_FRAMES (cov_k1_paths stopped once on `missing: text font file` under three parallel jobs and passed
  491 / 491 alone; 8_boot_gate_first.tsv keeps the first row, 8_boot_rerun_k1paths.tsv the re-run). Port test_static 6, test_gfx 25,
  test_gfx_routing 34 / 35: the failing test_image_draw_at_another_scale_is_unsupported fails on the base build too (S3.5's
  Image_draw change; 8_pytest_routing_base.log).
- Other test files on the S3.8 build (8_pytest_related_x64.log, 8_pytest_related2_x64.log, 8_pytest_terrain_live_x64.log):
  test_gfx_boot_screens 32, test_gfx_credits 47, test_gfx_scene 114, test_gfx_hud 34 + 7 skipped pass; test_gfx_dialogs 31 / 32,
  test_gfx_reports 33 / 36, test_terrain_live 393 / 397 fail exactly the tests that fail on the base build without S3.8
  (8_pytest_dialogs_base.log, 8_pytest_reports_base.log, 8_pytest_terrain_live_base_hawaii.log: S3.5's Surface_drawSprite /
  Image_draw changes against S2's `unsupported` tests and the tropical study against S3.4b's tree-alias default fault), so they
  are the base tree's, for the S3.5 / S3.4b merges. test_terrain_overlay: x64 12 + 6 opt-in, Win32 12 + 6 opt-in
  (8_pytest_x64.log, 8_pytest_w32.log); SG_S38_FRAMES=1: the 4 frame tests and frame controls 6 / 7 pass (8_pytest_frames.log is
  the run before control 6 moved to k_render_dot, 8_pytest_frame_negctl.log after).
- Census, segments, boot and streams were run on the build before the last body change (the Terrain object is read through
  gTerrain instead of the LiveTerrain, so a run without LiveTerrain and without a GL context draws nothing silently: S2's location
  test renders without LiveTerrain); with LiveTerrain attached both read the same object: the final build's frames of cov_a_keys,
  cov_k1_routing and cov_b_location equal the census images in all 32 checkpoints (8_final_vs_census.log).

## 5b. Open items

U-1370 .. U-1377 (8_uncertain.tsv). For the lead: (1) S3.7's edit 7 (Text_put returns the measured width) is what sizes the ball /
thought label bars; with S3.7's current files the cov_b_paths stream matches in full (8_combo.py). (2) The routing map's walker
markers, narrateShot's and showBuyLandScreen's lines are not drawn (U-1374). (3) A recording with the drawLine vertices (gl_rec.js
keeping GL_LINES bodies) or the root surface around each drawLine would judge the ends and the one-level residue directly (U-1373,
U-1377): NEED_GAME if the lead wants it; nothing in this step waits on it.

## 6. CLAUDE.md paragraph

Roadmap S3.8, overlay lines (2026-10-25, notes log/roadmap/S3/8_notes.md, U-1370..U-1377): Terrain::drawLine is the only overlay
primitive the exe reaches (Terrain::drawCircle, both splines and renderTile have no caller), and Terrain::initGL leaves
GL_LINE_SMOOTH on with GL_NICEST, so the lines are antialiased. Measured with glreplay.exe on synthetic drawLine sequences: coverage
at a pixel centre = clamp(w / 2 + 0.5 - |n|) * clamp(t + 0.5) * clamp(L - t + 0.5), the segment clipped to the viewport then its
ends snapped by U-1076's fixed-point setup, width clamped to 10, a zero-length line draws nothing, alpha * coverage blended and
dithered as the triangles (probes: 108 of 74,466 changed pixels differ; footprint-edge pixels under 0.13 coverage, U-1370). Module
`terrain_overlay` (overlay.rasterLine, overlay.logStart / logWrite) and the bodies drawMapLine, drawMinimapSegment, setLinePoint,
lineTo, drawCircle in the terrain family (the line point in the overlay's state; the GL context, flip byte and screen size from the
Terrain object at gTerrain; no context = nothing drawn); drawPanelBackdrop's bar and the routing map's hole line call them. The
port's drawLine calls equal the recorded GL streams in count, width, colour and alpha (cov_b_holes18 3,362; cov_b_paths 3,221 + 13
tooltip bars S3.7 draws); in the census the line pixels are 109,292 of 117,446 equal to the original, judged ones 0.36 % one level.


## 5. Commands

```
~/bin/py -3.12 log/roadmap/S3/8_mirror.py [--w32] [--base] [--own-only] [--no-build]   # source copy port/build/S3_8_src + builds
S=port/build/S3_8_src; B=$(pwd -W)/port/build/S3_head2/port/build
SG_PORTSIM_DLL=$B/S3_8/Release/portsim_c.dll ~/bin/py -3.12 -m pytest -c $S/pytest.ini --rootdir $S $S/port/test/test_terrain_overlay.py -q
SG_BACKEND=win32 SG_PORTSIM_DLL32=$B/S3_8_w32/Release/portsim_c.dll SG_PORTSIM_RUNNER32=$B/S3_8_w32/Release/portsim_runner32.exe \
  ~/bin/py -3.12 -m pytest -c $S/pytest.ini --rootdir $S $S/port/test/test_terrain_overlay.py -q
SG_S38_FRAMES=1 (same, x64): the frame tests and frame controls 6 / 7
sh log/roadmap/S3/8_census.sh           # framegate --all with images, base and S3.8 builds, then 8_linegate.py -> 8_linegate.tsv
SG_PORTSIM_DLL=... ~/bin/py -3.12 log/roadmap/S3/8_streams.py g2=cov_b_holes18 g3=cov_b_paths g4=cov_b_holes18 g5=cov_b_paths g6=cov_b_paths --out log/roadmap/S3/8_streams.tsv
~/bin/py -3.12 log/roadmap/S3/8_inspect.py <img_base> <img_s38> <scenario> <checkpoint> [x0,y0,x1,y1]   # where line pixels miss
~/bin/py -3.12 log/roadmap/S3/8_compare.py   # segments / boot against the committed tables
```
