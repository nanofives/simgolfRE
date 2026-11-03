# P8B: widescreen stage 2 (the wide course view) and the world at the window's pixel resolution (2026-11-03)

Tree: port/build/P8B (git archive of b786c29a). Brief: log/roadmap/P8/BRIEF.md section P8B, plus decision G32 (2026-11-03,
re/roadmap/DECISIONS_PENDING.md), which REPLACES three of the brief's defaults: (1) the sim's visibility test follows the wide view
behind new `play` switches; (2) HUD stage 3 now (panels at the wide view's edges, scaled with the window); (3) `--wide` ON by default
and DYNAMIC (width and render scale follow window resizes; width changes recorded). Sections 1-6 are stage 2 as first delivered;
sections 7-9 are the G32 work and say where it changed sections 1-6. Changed files: P8B_files.txt. Profile rows: P8B_profile.tsv.
Nothing is committed.

## 0. Progress log (for a fresh session; newest last)
- Survey: present.cpp (P7 bands), host.cpp present / cursor, terrain_live.cpp, gfx_terrain.cpp, gl_raster clip and viewport code,
  terrain_render.cpp is_culled / render loops, terrain_dll.cpp updateTerrainRender. Design fixed (section 1).
- 17:20 implementation built on x64; first runs in P8B_runs/. 17:41 gates. 17:53 final tree: builds, selftests, segments, sim
  equality, framegate, live app, screenshots, times all done (section 3). Remaining work is section 5 only.

- 18:00-18:45 G32: view block + play switches, dynamic world resize + recorded width writes, wide scene (side scenery), HUD
  compositor + input remap, gates re-run (section 9).

## 1. Design (stage 2 defaults; G32 replaced the visibility, HUD and `--wide off` defaults: section 7)
- Logical option A: W x 600, W from the window's aspect at the session's start (`--wide-width` sets the window, default 1067);
  g_screenWidth / g_screenHeight stay 800 / 600, so the SIM keeps the 800-wide visibility test, the screen words, the scaled fonts
  and the sprite divisor. The HUD stays the 800-wide frame centred at x + (W - 800) / 2 (anchoring is stage 3). `--wide` stays off.
- The RENDER VIEW is the WORLD LAYER of the attached LiveTerrain (terrain_live.h LiveTerrainWorld), outside the GameState and the
  address space: a second GlRaster of pw x ph pixels, made before the first Terrain call, fed the whole GL setup through a fan sink
  (terrain_live.cpp WorldFan: every call to both rasterisers in order; texture names must come out equal, else the layer is off):
  - each setup viewport(x, y, w, h) is mapped to (ox + x * fw / 800, y * ph / 600, w * fw / 800, h * ph / 600), with
    fw = ph * 800 / 600 the 800 frame's width in world pixels and ox = (pw - fw) / 2 (fit_viewport's integer rule);
  - the projection is UNCHANGED (same ortho calls, same matrices: every vertex's clip coordinates equal the 800 render's), and the
    x clip planes are moved out to |x| <= kx * w with kx = pw / fw (GlRaster::set_clip_x; kx = 1 runs the old expressions);
  - the dither matrix's column is taken from window x - ox (GlRaster::set_dither_dx), so the frame's own columns keep their phase;
  - isCulled's x bounds are widened by the side margin m = ceil(ox * 800 / fw) + 1 logical pixels, shifted by the zoom field like
    halfW (terrain_render_set_cull_margin; 0 = the original's test); y bounds unchanged (600 rows).
  The render scale is s = ph / 600 (1, 1.8 at 1920 x 1080, 2.4 at 2560 x 1440): the course is rasterised at the window's pixels.
- Every terrain operation on the 800 x 600 root surface is mirrored on the world's surfaces (gfx_terrain.cpp):
  windowPaint clears the world root; a pass runs again on the world rasteriser (TerrainGlobals::view_index put back afterwards);
  a STRIP pass is drawn there as a full render of the same pivot and angle over a cleared surface and the strip's shifted copy is
  skipped (a shift of 32 / 20 logical pixels has no exact counterpart at s = 1.8 or 2.4); blit 1 copies world root to world back,
  blit 0 world back to world root. renderTerrainStrip's widths (A, L10, L14, L18) therefore stay the 800 x 600 row: they are the
  tile footprint at the 800 projection's scale and camera tilt (initSystem keeps 800 x 600: terrain_gl_setup.cpp screen_scale
  38.682186, ortho 1767 x 1325), not a function of the window width; the 1024 x 768 / 1280 x 1024 rows come with other ortho sizes
  and tilts (40.541603, 40.83222), which the wide view does not use.
- T, the frame's terrain picture (800 x 600), follows the root surface: the root after windowPaint and blit 0, and after a pass the
  pixels the pass changed. At each present the frame's pixel p shows the world where back[p] == T[p] (LiveTerrain::world_frame's
  mask), else the frame's own pixel (scaled as today); the columns beside the frame show the world. A frame whose back buffer holds
  no terrain (gfx_back_terrain 0: the 2D screens, the full-screen overlays) is presented as before P8B (P7 stage 1).
- Input (item 4): clicks and every cursor read keep the unclamped logical point (x < 0 or > 799 over the side columns; the original
  receives signed words there: Viewport comment in input_map.h). H1's edge-scroll read (View_toLocal at ML+0xebb4, tested at
  ML+0xebc4..ML+0xebe3) carries a site (FrameIo::cursor_site = kCursorSiteEdgeScroll, LiveIo::body_cursor_site,
  LiveHost::cursor_at); the app answers it 0 / 0x31f only at the client's own left / right edge and clamps it to 1..0x31e elsewhere
  (HostOptions::wide_edge). The answer is what the trace records, so a replay needs no site. Panels keep 800-space hit tests.

## 2. Options (port/app/main.cpp; every default = the pre-P8B app)
- `--render-scale 1|native|<pw>x<ph>` (default 1). native = the client at the session's start (with --wide the whole client,
  without it the 4:3 content rectangle); <pw>x<ph> as given (live-trace screenshots and benchmarks); 1 = a world layer only with
  --wide (W x 600, s = 1). No world layer = the presenter's pre-P8B path (set_frame, no compose call).
- `--wide-edge on|off` (default on; used only with --wide and a world layer).
- live-trace only: `--world-shots i,j`, `--shot-size <cw>x<ch>` (world_shot_<i>.bmp / frame_shot_<i>.bmp), `--world-check`.
- `--selftest-p8b` (control `--negctl p8b-mask-ignored`). Development switch SG_WORLD_PASS_LOG=<file> (one line per world pass).
- session.json: `p8b` {render_scale, world_pw, world_ph, wide_edge} and the session's `p8b` {world, wide_edge, presents_world,
  edge_reads, edge_clamped}; live-trace session.json `world` {pw, ph, fw, ox, margin, passes, pixels, us, checks, ...};
  world_layer.json in the record directory.
- Not changed: `--wide` default off, `--scale`, `--wide-fill`; a window resized during a session keeps the world layer's size and the
  presenter scales it (nearest / bilinear by --scale) onto the content rectangle's mapping (WorldImage, compose_world).

## 3. Evidence (logs in log/roadmap/P8/)
| gate | result |
|---|---|
| builds | x64 and Win32 rc 0, 0 errors (P8B_build_x64.log, P8B_build_Win32.log) |
| segments x64, default options | 205 / 205 PASS, status and digest equal to log/roadmap/merges/I25/segs_x64.tsv on all 205 rows (P8B_segs_x64.tsv, final tree) |
| app selftests | --selftest 37/37, --selftest-p5b 6/6, --selftest-p7 9/9, --selftest-p8b 8/8 (x64; Win32 p8b 8/8); control p8b-mask-ignored: 2 rows RED (mask0_content_is_the_frame, mask_selects_per_logical_pixel) |
| framegate, default options | sandbox_basic (10 rows) and cov_b_location (5 rows): status, total / one-level / beyond px equal to log/roadmap/merges/I24/census.tsv on all 15 (P8B_framegate.tsv) |
| sim with the world layer on | --live-trace, --profile fixed: trace.trace and framehash.bin byte-identical to the run without it for sandbox_basic (439 records) and course_season (6,296 records), both at --wide --render-scale 2560x1440 and at 4:3 1920x1440 (P8B_runs/eq_*) |
| central 800 columns at s = 1 (--world-check, W 1067, ox 133) | sandbox_basic 442 checks, 15,394 px (35 per check); course_season 6,414 checks, 211,919 px (33 per check); cov_l_camera 238 checks, 2,039 px. Every differing pixel lies within 15 px of the frame's left / right edge and differs by one 5-bit level: the triangles the 800 render clips at its own x planes (S3.21's clipping) are drawn unclipped in the world. Before set_dither_dx: 156,065 px per check (ox = 134, the 4x4 dither phase), all one level |
| live app, 96 dpi, --wide --render-scale native (1067 x 600, P8B_drive.py) | recording 87 records replayed through sg_run_boot: PASS; 87 presents with a world layer; side columns non-black 79,800 / 79,800 px; cursor parked mid side column 12 frames: 639 of 272,000 view px change (water and animation, no scroll), edge_reads 87, edge_clamped 24; cursor at client x = 0: 250,849 px change (the view scrolls); control --wide-edge off: parking scrolls (250,842 px) |

Screenshots: P8B_shots/ (index.tsv; 40 files): sandbox_basic presents 300 / 447 and course_season (a built hole) presents 1000 /
3000 / 6304, each wide at 1920 x 1080 and 2560 x 1440 and 4:3 (world 1440 x 1080 in a 1920 x 1080 client, sandbox also 1920 x 1440
in 2560 x 1440), each with `world` (P8B) and `frame` (the same present as P7 stage 1 shows it) for a side by side; the live session's
captures (`live_*`, edge on and off). At s > 1 the terrain's textures are sampled per window pixel (no 800 x 600 block structure);
the 2D frame (HUD, sprites, labels) is the 800 frame scaled.

## 4. Pass times (P8B_times.tsv, P8B_times.py; LIVING-PC Ryzen 5 3600, x64 Release, --live-trace runs)
Full render (entry kind 2; course_season's 2, cov_l_camera's 10 camera moves), ms, median / max:

| world layer | s | world pass, 4 workers | world pass, no workers | 800 pass in the same run, 4 workers |
|---|---|---|---|---|
| none | | | | 6.94 / 7.01 (no workers 23.9 / 28.2) |
| 1067 x 600 | 1 | 8.84 / 9.07 | 35.97 / 36.25 | 6.70 / 7.18 |
| 1920 x 1080 | 1.8 | 24.69 / 24.72 | 88.13 / 93.08 | 6.90 / 6.96 |
| 2560 x 1440 | 2.4 | 32.61 / 33.43 | 138.05 / 138.27 | 6.90 / 7.05 |

Local renders (kind 3, 105 in course_season), world median 0.45 / 1.15 / 1.53 ms with 4 workers. A camera-move frame does both
passes on the sim thread: about 40 ms at 2560 x 1440 with 4 workers, inside the 87-100 ms frame wait; without workers 165 ms,
longer than a frame. Edge scrolling now draws a full world render per step instead of a strip (the 800 strip pass stays).

## 5. Open items and limits
1. SCENE SPRITES IN THE SIDE COLUMNS (done after G32: section 7.3; the text below is the stage-2 finding). The world layer holds terrain only; sprites, labels, overlay lines and the HUD come
   from the 800 frame. tileToScreen (tick_render.inc) queues a tile's sprites only when its screen point is within
   x in [-0x40, 800 + 0x40) (g_screenWidth, which the sim keeps at 800), so the side columns beyond 64 px have no queued scenery, and
   trees straddling the frame's edges are cut at x = 0 / 799 (visible in every wide shot). Mirroring the queue draws into the world
   would only move the cut out by 64 px. Routes: (a) a render-only scenery pass for the side columns (the tile objects drawn into
   the world from the tile data, writing no GameState byte), or (b) a `play` switch letting the sim's visibility follow the wide
   view (more golfers on screen: playSoundAt's RNG draw and story advances change, U-0860; needs Mariano's decision, PLAN.md 5).
2. Sprites, labels and overlay lines are not redrawn at s: they are the 800 frame's pixels scaled (nearest / smooth). Native
   sprite and line drawing at s is part of route 1(a).
3. The central columns at s = 1 differ from the 800 frame near its x edges (section 3), one level, from the moved clip planes.
4. Dialogs and reports that dim the whole frame clear gfx's terrain flag, so those frames have black side columns (stage 4).
5. Corner case: the world rows of a letterbox client (taller than 4:3) stay black; a resize keeps the world layer's pixel size.
6. Only x64 was exercised in the app sessions and live-trace runs; the Win32 build and its --selftest-p8b pass.

## 6. How to run
- Wide course view at the window's pixels: `simgolf_port --install <dir> --wide --render-scale native`.
- Screenshots of a corpus trace: `~/bin/py -3.12 log/roadmap/P8/P8B_lt.py <scenario> <out> -- --profile fixed --wide
  --render-scale 2560x1440 --world-shots <present indices> --shot-size 2560x1440 --scale smooth` (from port/build/P8B).
- Measurements: `bash log/roadmap/P8/P8B_batch.sh shots|cs_shots <indices>|times|cam`, then `~/bin/py -3.12 log/roadmap/P8/P8B_times.py`.
- Live check: `~/bin/py -3.12 log/roadmap/P8/P8B_drive.py --dpi 96 --render-scale native [--wide-edge off]`
  (G32: `--profile play --resize 1400x600,960x600,1600x900 --click-pause [--negctl view-write-unrecorded] --tag <t>`).

## 7. G32 (decision of 2026-11-03): what was added
### 7.1 Sim visibility (`play` switches, P8B_profile.tsv)
- The VIEW BLOCK (port/include/portsim/sim/bridge.h): 64 KB of the context's address space at 0xFDF00000, outside the GameState and
  every image, mapped at boot (BootCrt, tick_boot.inc). Word +0 is the logical view width W (800 = the original). The host writes it
  only inside a frame-head call and records it as a harness write (DELIVER w32). A replay applies it before the same frame body
  (tick.cpp TraceInput::begin, head_writes). No trace format change.
- `worldToScreen.wideViewVisibility` (bridge.cpp, both x tests of world_to_screen): at 0 the on-screen x window is
  [-m - E, 800 + m + E) with E = (W - 800) / 2: the whole wide view, the same share of it as the original's 800 is of its own view,
  centred. It decides playSoundAt's RNG draw and whose stories advance. y unchanged (600 rows).
- `tileToScreen.wideViewVisibility` (tick_render.inc tileToScreen): the same widening of its x window (-0x40 .. 800 + 0x40), so the
  scenery buildSceneSprites queues covers the side columns.
- Both default 1 (`original` and `fixed` unchanged: segments 205/205 = I25); both 0 in `play` (main.cpp kPlayProfile).

### 7.2 `--wide` on by default and dynamic
- main.cpp: `--wide` defaults ON; `--render-scale auto` (the new default) = native with --wide, no world layer without it.
  `--wide off` is the pre-P8B app (no world layer, no view writes). SG_APP_P8B=off turns the default off; port/test/conftest.py sets
  it, so the app tests that compare with the original's recordings run as before.
- LiveSession::update_view_target (window thread, at each viewport change): W = floor(800 * client width / content width), at least
  800; the world layer = the client's pixels (native) or W x 600. LiveSession::apply_view (sim thread, each frame head): the view
  block write (recorded) when W changed, and LiveTerrain::resize_world when the size changed. GlRaster::resize_frame keeps textures
  and state; the fan re-maps the last setup viewport; clip planes, dither phase and cull margin follow; the world is redrawn whole
  from the camera's last full or strip pass at the next updateTerrainRender return.
- Control `--negctl view-write-unrecorded` (the width stored but not recorded): its replay FAILS at record 1 (g_tileScreenX,
  g_tileScreenY and g_rng differ).

### 7.3 The wide scene (side-column sprites)
- gfx_scene.cpp: at renderSpriteQueue's start (the new drop renderSpriteQueue.begin, tick_render.inc) the wide scene's base is made:
  W x 600 logical, the root surface in the frame's columns, the world layer sampled down beside them. Every queue entry and golfer
  label is then drawn a second time with that surface in the root's place and x + E; the 800 root is put back after each one, so the
  frame is unchanged. At the queue's end s1 = the root.
- LiveTerrain::world_frame lays the scene's sprite pixels (scene != base) over the world (nearest). The frame pixel p shows that world
  image where back[p] == T[p] (terrain) or back[p] == s1[p] != s0[p] (a pixel the queue drew; s0 = the root at the queue's start).
  Everything else stays the frame's: 2D drawn before the queue (walker name labels, the fountain sprays) and the HUD after it.
- The wide scene's central 800 columns equal s1 exactly (a dump of sandbox_basic's last queue pass: 0 px differ).

### 7.4 The HUD compositor (stage 3) and the input remap
- present.h kHudPanels (800-space). Chosen from the union of every in-game present's 2D pixels over sandbox_basic, course_season,
  golfer_events, ui_tour, cov_a_build and cov_a_keys (P8B_runs/ui_union_all.png, the `--ui-union` runs):

  | panel | rectangle | anchor |
  |---|---|---|
  | title: logo, course name, date | 0,0 - 250,95 | left edge |
  | meters: money, happiness, rating | 630,0 - 800,145 | right edge |
  | bottom band: radial menu, System button, the palette and the panels drawn in its place (info cursor, building, employee view / detail, golfer action, golfer info) | 0,425 - 800,600 | left edge |
  | message ticker, dialogs, reports, tooltips outside a rectangle | | stay centred |

  The golfer and employee panels are drawn inside the bottom band (tick_frame.inc ML+0xa225..ML+0xa369 call them where the
  palette is), so they move with the band to the left edge, not to the right edge the decision lists. Moving them right needs their
  own rectangles inside the band, which the shared area does not give without measuring each panel's extent: left to Mariano.
- Presenter::compose_world: the frame's 2D pixels inside a panel rectangle move by the anchor's distance to the window's edge
  (left: -origin_x client pixels; right: + the right band's width); the place they leave shows the world image. Everything is
  scaled by window height / 600, so the panels keep their size relative to the window.
- PanelRemap (present.h): a client point over a moved panel's 2D pixel is mapped back to its 800-space place, for mouse messages
  (AppWindow::input, before the translator) and the system cursor's answers (LiveSession::cursor). No click handler changes.
  `--hud-anchor on|off` (default on with --wide). Control `--negctl p8b-hud-unmoved`.
- Limits: a point in a panel's vacated place still reaches the game at that 800-space point (the game's hit test sees the panel
  there); grid and overlay lines drawn after the queue inside a rectangle move with the panel; a tooltip half outside a rectangle is
  split.

## 8. Options after G32 (main.cpp)
`--wide on|off` (default on; SG_APP_P8B=off), `--render-scale auto|1|native|<pw>x<ph>` (default auto), `--wide-edge on|off`,
`--hud-anchor on|off`, live-trace `--view-width W` (the view block's width at the first frame head, recorded), `--ui-union`
(development), `--world-shots`, `--shot-size`, `--world-check`, `--selftest-p8b`.

## 9. Evidence after G32 (final tree)
| gate | result |
|---|---|
| builds | x64 and Win32, 0 errors (a C4244 in bridge.cpp that the app tests' /WX builds refused was fixed) |
| segments x64 | 205 / 205, status and digest equal to I25 on every row |
| selftests x64 and Win32 | --selftest 37/37, p5b 6/6, p7 9/9, p8b 11/11; controls p8b-mask-ignored 2 RED, p8b-hud-unmoved 3 RED |
| framegate | sandbox_basic + cov_b_location 15/15 equal to the I24 census |
| sim, `fixed`, --wide --render-scale 2560x1440 against --wide off | trace.trace and framehash.bin byte-identical, sandbox_basic (439) and course_season (6,296) |
| app test files, SG_APP_P8B=off | test_app_shell 18 passed, test_app_present 3 passed, test_app_input 16 passed, test_app_replay 14 passed, test_app_ingame 14 passed (P8B_t_*.log; test_app_live and test_app_audio not run) |
| live, `play`, resizes to 1400x600, 960x600, 1600x900 | 4 width writes, 3 world resizes, replay PASS 117/117; control view-write-unrecorded FAIL at record 1 |
| live, `play`, click on the moved pause button | client (105, 567) remapped (3 events), g_flags bit 2 set from frame 92, replay PASS 119/119 |

Screenshots: P8B_runs/scene_play (side scenery, 1920 x 1080), hud_play / hud_cs (HUD at the edges: sandbox 1920 x 1080, a built
course 2560 x 1440), P8B_live/*_hud.png and *_play_resize.png (the live window, resized during the session).
