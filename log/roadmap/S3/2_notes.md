# S3.2 notes: terrain into the frame

Worker: opus (session cmvmz6jmr54ajo115p48ehw57; started on claude2, moved to claude3 when claude2 reached 100 %), 2026-10-24/25.
Lead: S3 (cmvmns8sn3fsbo115g4dgm1nf). Game slot: instance `instances\s3_2` (pinned winmm.dll 381ed2b2 copied from instances\p4q).

## 1. What the original does (established before wiring)

- The GL context draws into the ROOT surface. initTerrain hands Terrain::initSystem the DC from slot +0x28 of the jgld surface object
  at [g_rootWindow + 0x278] (re/analysis/terrain/00449790_initTerrain.md); recorded: the bits of that DC's bitmap (800 x 600, 16 bpp,
  BI_BITFIELDS 0x7c00 / 0x3e0 / 0x1f, biHeight +600) are the root surface, and memory row 0 is the TOP row of the picture
  (log/roadmap/S3/2_tools/orient.py: 309,787 of 480,000 pixels of the recorded root equal the checkpoint frame with memory row 0 = top,
  10,325 with row 0 = bottom; the rest is the HUD the frame draws later). GlRaster's row 0 is DIB memory row 0
  (port/test/glraster.py), so the rasteriser's buffer and the backend's top-down Surface16 root are the same array: no flip.
- updateTerrainRender (re/analysis/terrain/004498a0_updateTerrainRender.md, shim/src/re/c3ez.cpp): Window::paint(g_rootWindow,
  0x80000000) clears the root surface (no parent -> Widget_clearArea -> surface slot +0x44 (NULL, 0x80000000) -> fillRect16 -> clear16:
  bit 31 set = a direct colour, masked to 16 bits = 0; re/analysis/render/00478b30, 00009770, 0000da70 notes). Recorded: every pass's
  `pre` copy after a Window::paint equals the port's all-zero root in every pixel (diff 0 on the 5 render passes judged).
  The passes draw over it; Surface_blitThunk(root, back, 0, 0, 0, 0, w, h) then copies the root surface to the exe's back surface
  g_image51b068 ("creates the back surface", gameMain note); with the camera unmoved the back surface is copied whole onto the root
  surface instead (and changed tiles are local-rendered over it); renderTerrainStrip draws stripRender's strip, then copies the back
  surface's rectangle (bp, di, X, Y) to (si, bx) on the root surface (the shifted old picture). Surface_blitThunk(this, dst, x1, y1,
  x2, y2, w, h) = Surface_blit3(this, dst, x1, y1, w, h, x2, y2, w, h): this's rectangle at (x1, y1) onto dst at (x2, y2).
- So the gfx surface the terrain goes to is the ROOT surface (gfx.root), and Window::refreshFull later copies root -> back buffer like
  every other 2D draw.

## 2. What was built

- `port/src/sim/gfx_terrain.cpp` (new, body family `terrain`): terrain.windowPaint (root := 0), terrain.pass(kind, self, a0, a1, a2)
  (root copied into LiveTerrain's GlRaster, the vertex / normal arrays given, terrain_render_entry of S3.1, root copied back; sets
  root_terrain), terrain.blit(dir, sx, sy, tx, ty, w, h, unknown) (the whole-surface copies and the strip's shifted copy, clipped like
  StretchBlt 1:1, through LiveTerrainView::back_surface), terrain.frameBegin / frameEnd (harness copies `$gfx/terrain_<k>_*.555`).
  Negative controls g_negctl_gfx_terrain 1..5 (header comment).
- `terrain_live.cpp / .h`: LiveTerrain owns a GlRaster (800 x 600, no context at construction like the constructor's texture load);
  render / localRender / pathUpdateRender call the state side (view quadrant) then hand the pass to the backend; window_paint,
  blit_full, blit_screen hand their drops over; render_strip is renderTerrainStrip (the C3 note's mapping: widths A / L10 / L14 / L18 by
  screen size, shift by zoom, r2 by rotation, direction D from the step, case e -> source / destination / size), its stripRender's
  state effect through terrain_dll_call (not logged: the recorder comparison keeps TerrainApi methods only, and the snapshot
  positions count logged calls). Harness entries by sg_call: terrainLive.keepFrame(k), passes, pixels, frames.
  Nothing is drawn unless a Gfx is attached; nothing writes the GameState or the address space.
- `terrain_state.h / .cpp`: SetupTee (the TerrainDll's setup sink: the GlStreamSink plus the attached rasteriser), so the constructor,
  initSystem, loadNewCourseType and setZoomLevel GL calls build the rasteriser's context, textures (named by the same lowest-free rule;
  names_differ reported), light, projection.
- `terrain_dll.cpp`: update_terrain_render brackets its body with terrain_live_frame(begin / end) (frame ordinal = the process's
  updateTerrainRender call count from 0, the same count the recorder keeps).
- Win32 (S0 recommendation 5): runner32.cpp ops 28..32 (sg_ts_live_attach / detach / object / log / snapshot), lib32.py State32.live_*,
  terrainlive.py live_attach / live_detach / live_object / live_log / live_snapshots dispatching on the backend.
- Recorder: re/frida/js/terrain_calls.js + re/tools/terrain_calls.py `--pass-frames` (root DIB copied natively at updateTerrainRender
  entry / return and at each depth-0 pass entry / return, last frame plus the last frames with a pass / a stripRender / a
  pathUpdateRender kept and sent at each checkpoint as frames/<cp>[.pass|.strip|.path]/{entry,exit,pass<j>_pre,pass<j>_post}.z
  (zlib) + meta.json) and `--frames-lite` (entry + exit only; the census recordings).
- Tests: port/test/test_terrain_live.py (course view G2 per recorded copy, the family's controls, x64 = Win32 pixels), terrainlive.py
  (recorded_frames, port_frames, run_trace, g2).
- Census tool log/roadmap/S3/2_census.py (`terrain` verdicts, `rank`), controls report 2_controls.py, mirror 2_mirror.py +
  2_merge_edits.py (the shared edits of 2_merge.md, applied to the source copy only).

## 3. Numbers (mirror port/build/S3_2_root, builds S3_head/port/build/S3_2 and S3_2_w32)

- Recordings (instance s3_2, hooks off, C3FT_AUDIO=none; every pinned run `scenario_refs.py check` SAME as det_b):
  full --pass-frames: s32_sandbox, s32_course_season, s32_cov_b_holes18, s32_cov_b_career_types, s32p_cov_l_camera, s32p_cov_b_paths,
  s32p_cov_k1_terrain, s32p_cov_k3_camera, s32q_cov_b_holes18 (the organic stripRender, dir 5, frame 938), s32q_cov_k1_paths;
  Tropical study s32t_cov_b_s34_hawaii (Tree alias in the instance during the run, removed after; its trace beside it);
  --frames-lite s32c_<s> for the other 52 census scenarios (51 + cov_a_quit). Frida spawn hung 5 times (TransportError, the game
  left suspended in s3_2 and killed by its PID); every scenario got a run on a retry.
- Themes: Parkland (loadNewCourseType 0: sandbox, careers carolina / novascotia), Desert (1: lasvegas), Links (3: scotland, ireland,
  wales, ...), Tropical (2: hawaii) all within G2.
- COURSE VIEW GATE (test_terrain_live.py test_course_view_equals_the_original): every recorded copy of the 11 full recordings
  (frame exit, every pass's pre and post: render, localRender, the strip, zoom 1, rotation 6, map-edge cameras) meets G2 over the
  pixels the original's frame drew; one-level 148..275 px of 43,104..469,568 drawn, beyond one level 0..9. Pass `pre` copies after a
  Window::paint are 0 px off. x64: 252 passed (log/roadmap/S3/2_pytest_terrain_live_x64.log, before the Tropical case; 4 more with it).
- CENSUS (2_census.py over re/roadmap/frame_census.tsv's terrain-yes rows + cov_a_quit, whose trace came after S0): 527 / 527
  terrain verdicts PASS, beyond-one-level at most 9 (2_terrain_verdicts.tsv). Framegate census with this build
  (2_census_fg_x64.tsv): 609 rows, PASS 54 (S0 45), UNSUPPORTED 545, NOT_RUN 10 (no trace); terrain_drawn = 1 on 526 of 527
  terrain-yes rows (cov_a_keys k_worldmap 0: the world map covers it, and that row PASSes) and on 2 terrain-no title rows (U-1315).
  Ranked (2_census_ranked.tsv): `terrain not drawn` 0 rows (S0 520); primary causes now Widget_writePixel 517, PASS 54, Image_ctor
  19, NOT_RUN 10, playAudioFile 7, Surface_drawScaled2 2.
- CONTROLS (2_controls.log, test_terrain_negctl_reads_red): 5 / 5 RED, 0 blind: 1 wrong zoom RED on 1 / 6 (the one zoomed frame,
  440,738 px beyond one level), 2 wrong theme set RED on 10 / 12 (the 2 GREEN are Parkland careers, where the kept set is right,
  U-1316), 3 pass skipped 9 / 9, 4 strip copy without its shift 1 / 21 (the strip frame, 67,621 px), 5 Window::paint not clearing
  12 / 12 (3,201 px). Plus the old LiveTerrain controls 22 / 14 / 12 still RED (x64).
- WIN32: LiveTerrain attaches through runner ops 28..32: test_terrain_live (calls / states / boot) 141 passed, 3 skipped (sg_ts_negctl
  has no runner op) on SG_BACKEND=win32 (2_pytest_terrain_live_w32.log); x64 = Win32 terrain pixels, every kept copy of
  s32_sandbox identical (test_x64_equals_win32_terrain_pixels). x64 old tests 144 passed (= S0).
- REGRESSIONS: segments x64 205 / 205 PASS, 0 differ from log/roadmap/merges/S6-1/segs_x64.tsv; Win32 205 / 205, 0 differ
  (2_segs_*.tsv). Boot gate HEAD's 61: 58 PASS / 2 KNOWN / 1 NO_FRAMES, 0 rows differ from S0 (2_boot_compare.log). test_gfx 25,
  port test_static 6 (with the family line), test_terrain_gl_setup 6, test_terrain_state 500, test_gl_raster 177.

## 5. Open items

U-1310 (800 x 600 only), U-1311 (back surface depth inferred), U-1312 (no organic pathUpdateRender in any recording), U-1313 (one
organic strip direction), U-1314 (resolved by the cov_a_quit recording: no row is left not measured), U-1315 (title rows keep the flag).
Not this step's: the census's remaining causes are 2D (Widget_writePixel is S2's NEED, U-row in S3 STATUS) and sprites (S3.5).
Proposed census rule for S3.9: 2_merge.md 1.5.

## 4. Commands

```
~/bin/py -3.12 log/roadmap/S3/2_mirror.py --w32                    # source copy + merge edits + both builds
cd port/build/S3_2_root
export SG_PORTSIM_DLL=<S3_head>/port/build/S3_2/Release/portsim_c.dll
export SG_PORTSIM_DLL32=<S3_head>/port/build/S3_2_w32/Release/portsim_c.dll SG_PORTSIM_RUNNER32=<...>/portsim_runner32.exe
~/bin/py -3.12 -m pytest port/test/test_terrain_live.py -q -p no:cacheprovider
SG_BACKEND=win32 ~/bin/py -3.12 -m pytest port/test/test_terrain_live.py -q -k "not course_view and not terrain_negctl and not x64_equals"
   (needs port/build/Win32 -> S3_2_w32 junction in the root)
~/bin/py -3.12 ../../../log/roadmap/S3/2_controls.py
~/bin/py -3.12 -m port.test.framegate --all --jobs 3 --no-probe --out ../../../log/roadmap/S3/2_census_fg_x64.tsv
~/bin/py -3.12 ../../../log/roadmap/S3/2_census.py terrain --jobs 3 --out ../../../log/roadmap/S3/2_terrain_verdicts.tsv
~/bin/py -3.12 ../../../log/roadmap/S3/2_census.py rank ../../../log/roadmap/S3/2_census_fg_x64.tsv ../../../log/roadmap/S3/2_terrain_verdicts.tsv --out ../../../log/roadmap/S3/2_census_ranked.tsv
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --dll $SG_PORTSIM_DLL --jobs 6 --out ../../../log/roadmap/S3/2_segs_x64.tsv
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out ../../../log/roadmap/S3/2_boot_gate.tsv ; ~/bin/py -3.12 log/roadmap/S3/2_boot_compare.py (repo root)
# recordings (repo root, instance s3_2):
SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 re/tools/terrain_calls.py --scenario <s> --instance s3_2 --label s32_<s> --pass-frames
sh log/roadmap/S3/2_tools/census_rec.sh          # --frames-lite for every census scenario
sh log/roadmap/S3/2_tools/tropical_rec.sh        # the Tropical study scenario with the Tree alias
```
