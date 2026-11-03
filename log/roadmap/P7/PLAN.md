# P7: widescreen for the port (plan, 2026-11-03)

Mariano's request: on a 16:9 window the game uses the whole width. The title menu and the other full-screen 2D screens fill the
extra width with their own background colours (no black bars); in game the course view gets wider and the UI (HUD bars, toolbar,
bottom panel, side buttons) moves out to the edges of the wider screen.

Tree: port/build/P7 (git archive of f04c7591). Line numbers below are in that tree. P5B's scope (log/roadmap/P5/P5B_notes.md
section 5) is the starting point; this plan re-counts it and adds what the sim / render split needs.

## 1. Logical screen size options

The game draws one 800 x 600 frame today. A wide mode needs a fixed LOGICAL size per session (recordings, the GameState, the gfx
surfaces and every coordinate the game reads must agree for a whole session), scaled to the window by the presenter as now.

| option | logical size | what grows | anchor work | notes |
|---|---|---|---|---|
| A | W x 600, W from the aspect (1067 for 16:9, 960 for 16:10, 1280 cap for 21:9) | the view, sideways only | horizontal only: every HUD y stays | recommended |
| B | 1280 x 720 | the view, both ways | horizontal AND vertical (every bottom-anchored panel moves down 120) | 2x on a 2560 x 1440 screen; more course rows than the original ever showed (camera clamps, edge tiles) |
| C | 1024 x 768 / 1280 x 1024 (4:3) | magnification, not area | none in the world code | the original's own modes: worldToScreen scales ix / iy by 80/64, 52/40 (W 0x400) or 104/64, 68/40 (W 0x500) (bridge.cpp:68-74) and renderTerrainStrip has widths for them (terrain_live.cpp:343-362), so the original shows the SAME area bigger there. Not what Mariano asked for |

Option A keeps 600 rows, so every vertical position in the HUD, the dialogs and the 2D screens is unchanged; only horizontal
anchoring needs rules. W is fixed when the session starts (from the window's aspect, or `--wide-width`), never per frame.

## 2. Where 800 x 600 is assumed

### 2.1 The screen-size words
- g_screenWidth / g_screenHeight (GameState class `render`, gamestate_meta.inc:1121-1122), set to 0x320 / 0x258 at gameMain+0x6 /
  +0x10 (tick_boot.inc:12-13), passed to the display's set-mode slot and to initTerrain (tick_boot.inc:86, tick_menu.inc:46,
  terrain_dll.cpp:365).
- Readers that are NOT pure drawing, so changing the words changes more than pixels:
  - worldToScreen (bridge.cpp:61-139): the projection AND the on-screen test (`sx < -m || sx >= W + m`). playSoundAt draws g_rng only
    when the point is on screen (U-0860), and story advances run only for golfers inside the screen rect (CLAUDE.md, the
    stories_fixture note), so a wider W changes the simulation (RNG sequence, stories) even with the same commands.
  - screenToTile / the screen-centre picks (tick_render.inc:193, 210-211): the click-to-tile hit test.
  - the sprite scale divisor (tick_render.inc:30-37: 0xd above 1000 px, 0xa from 0x4b0): a W of 1067 would shrink every sprite.
  - scaleX(v) = (g_screenWidth * v + 160) / 320 (commands.cpp:99, dialogs.cpp:756) and the two scaled fonts (gditext.cpp:287): a
    W of 1067 grows those fonts by a third.
  - the gfx root surface: Window::create's scaleX(0x140) x scaleX(0xf0) (gfx.cpp:90-97): its HEIGHT comes from the width, so the
    root is 4:3 by construction (1067 -> 1067 x 800).
- Decision this plan proposes: g_screenWidth / g_screenHeight stay 800 / 600 in every mode. The wide width lives outside the
  GameState (a per-context render view, set by the app), read only by render paths that are moved to it one by one. Then
  `--wide off` is byte-identical by construction and the sim reads exactly what it reads today.

### 2.2 Literal sizes in the port's game code (port/src/sim + port/include)
| literal | meaning where it is the screen | sites | files |
|---|---|---|---|
| 0x320 | 800 | 92 | 27 |
| 0x258 | 600 | 93 | 24 |
| 0x190 | 400 (half width; also other values) | 100 | 24 |
| 0x12c | 300 (half height; also other values) | 86 | 19 |
| 0x31f / 0x257 | right / bottom edge | 4 / 5 | 3 / 4 |

Files with the most 0x320 / 0x258 sites: gfx_routing.cpp 39, tick_frame.inc 26, saveload.cpp 17, gfx_hud.cpp 12,
gfx_boot_screens.cpp 10, commands_screens.inc 10, gfx_bulletin.cpp 8, audio_seq.cpp 6 (not screen: tempo values), routing.cpp 5,
gfx.cpp 5. Each site needs a reading (screen size or another 800 / 600) and, if screen, a rule (centre, anchor left / right,
stretch). 0x190 / 0x12c are counted as upper bounds: most are not the screen.

### 2.3 Input-side assumptions
- H1's edge scroll (tick_frame.inc:8288-8293 = ML+0xebb4..ML+0xebe3): x <= 0, y <= 0, x >= 0x31f, y >= 0x257 on the View_toLocal
  cursor. With a wider view the cursor's logical x runs from -(W-800)/2 to 800+(W-800)/2 and the test fires inside the view.
- The System menu's hit area x < 0xc8, y > 0x1b8 (ML+0xee46 / ML+0xee57), the panels' click handlers (commands_panels.inc), the
  radial menu, toolbar hit tests: all in 800-space and all inside command code (sim effects).
- bridge.cpp:159-160 clamps a point to 0..0x31f / 0..0x257.
- The app: input_map.h kLogicalW / kLogicalH, Translator::to_logical, the edge guard's clamp to 1..798 x 1..598 (host.cpp:426-427).

### 2.4 Terrain renderer
- LiveTerrain makes GlRaster(800, 600) (terrain_live.cpp:42) and WglSink(800, 600) (terrain_live.cpp:111); U-1310: the rasteriser is
  fixed at 800 x 600 and a root of another size draws nothing.
- renderTerrainStrip's width table knows 800 x 600, 1024 x 768, 1280 x 1024 only (terrain_live.cpp:343-362); other sizes leave three
  widths uninitialised in the original. A wide W needs its own A / L10 / L14 / L18 (they scale with the tile count across the view).
- Terrain::render's view passes, localRender's 5 x 5 window (terrain_render.cpp:120) and the culling (isCulled) work on the Terrain
  object's viewport (Terrain::resize, set by initTerrain from the two words). A wider view needs Terrain::resize(W, 600) on the
  RENDER side only (the Terrain object's sim-visible fields must not move: U-0607 / U-0800 say no sim write depends on Terrain.dll's
  returns, which keeps this possible).
- Camera clamps were set for the 800 view: a wider view shows ground beyond the map edge (the black ground of fb2_gary_offmap.png).

### 2.5 The drawing backend and the app
- gfx root / back buffer (gfx.cpp:92-97) and every body family assume the root's size; checkpoint snapshots, framegate references,
  segment captures and every recording are 800 x 600.
- The app: host.cpp's present() takes only an 800 x 600 back buffer (host.cpp:339 size test), the presenter's frame is 800 x 600,
  the window opened at 800 x 600 logical before P7.

## 3. Layout or simulation

Rule: nothing in a sim-class GameState object may change because the window is wider, unless a profile switch says so (G30's
`play` profile is the place for a deliberate divergence).

| item | class | how wide mode treats it |
|---|---|---|
| presenter, window size, band fill | layout (app) | stage 1 (done) |
| full-screen 2D screens (title, location, loading, theme packs, difficulty, pick a pro, editor, credits) | layout | stage 1: centred, bands extended |
| course view pixels (terrain, sprites, overlay lines) | render | stage 2: drawn at W through a render view |
| worldToScreen's on-screen test, playSoundAt's RNG draw, story visibility | SIM | keep the 800-wide central window by default (sim identical); a `play` switch could let the sim follow the wide view, decided by Mariano |
| screenToTile (click on the course) | command input | stage 2: a view click in the side areas maps through the render view; the game receives the 800-space coordinate the projection gives (possibly < 0 or > 799) |
| edge scroll (H1) | command input | stage 2: the edge is the wide window's edge (the app answers the cursor; a port-side view rect replaces the 0 / 0x31f literals in H1 only through a switch-free parameter whose default is 0..0x31f) |
| HUD bars, toolbar, bottom panel, side buttons, radial menu, tooltips | layout + command hit tests | stage 3: compositor (below) |
| reports / dialogs over the course | layout | stage 4 |
| scaled fonts, sprite scale divisor, gfx root size | render, but read g_screenWidth | stay at 800 (g_screenWidth unchanged) |

### The compositor idea for stage 3 (keeps every hit test in 800-space)
The game keeps drawing its HUD into the 800 x 600 frame exactly as now and keeps testing clicks in 800-space. The backend draws
the HUD bodies (gfx families hud, text, pixel, prim_*) into a UI layer with coverage, and the course (terrain, scene, overlay) into
a W x 600 world layer. A per-panel table gives each HUD rectangle an anchor (left / right / centre / stretch) in the wide frame;
the compositor moves each rectangle there over the world layer, and the input translator maps a click inside a moved rectangle back
to its 800-space position (outside every rectangle: the world layer's mapping). No sim code changes, so the panels' click handlers
see the same coordinates as today; the gate is invertible: undoing the moves must give the 800 frame byte for byte.

## 4. Stages

| stage | what | needs | gate |
|---|---|---|---|
| 1 (this pass) | app: `--wide`, window at W x 600 logical, 4:3 image fitted at full height, 2D screens' bands extended from their own edges (soft / clamp), in-game bands black | app only | `--wide` off byte-identical (selftest + segments x64 = I25); selftests; screenshots |
| 2 | wide course view | a per-context render view (W, origin (W-800)/2) outside the GameState; GlRaster / WglSink at W x 600 (U-1310); Terrain::resize(W, 600) on the render side; renderTerrainStrip widths for W (derived from the tile count, checked against the three known rows); the scene / sprite queue and overlay lines drawn through the render view; worldToScreen split into a render projection (W) and the sim visibility test (800, unchanged); the HUD drawn centred at x + (W-800)/2; the edge scroll at the wide edges; camera clamps for the wider view | sim hashes of every segment equal with the view at 800 and at W when the same commands run (the sim view pinned); the central 800 columns of the world layer equal today's frame where no HUD covers them |
| 3 | HUD anchoring | the compositor (UI layer with coverage, per-panel rectangle table, input remap); a decision per panel (title / meters / message bar: stretch or centre; toolbar: left; bottom panel: bottom-left or bottom-centre; radial menu and System menu button: bottom-left; employee / golfer panels: right) | moving every panel back reproduces the 800 frame; every recorded click lands on the same 800-space point |
| 4 | dialogs and reports over the course | centred over the wide course (their dimming pass widened) | per screen, as S2.8 / S2.9 judged them |
| 5 | settings | W from the window's aspect at session start; the choice recorded in session.json and in the recording's meta so replays use the same W | replay of a wide session equal in every record |

## 5. What the later stages need (summary)
- Terrain renderer width (stage 2): GlRaster and WglSink take W x 600 (U-1310 lifted: today a root of another size draws nothing),
  the GL viewport and projection for W (the original's projection is set in Terrain::initGL / resize from the two words; the render
  side must use W without writing the Terrain object's sim-visible fields), renderTerrainStrip's four widths for W, the tile window
  of render / localRender widened, and the culling test fed the wider frustum.
- HUD anchoring per panel (stage 3): a table of HUD rectangles in 800-space (the gfx_hud.cpp / gfx_text.cpp / gfx_pixel.cpp bodies
  already know each panel's position: drawTerrainToolbar, showInfoCursorPanel, drawAmenitiesPanel, drawGolferActionPanel,
  drawEmployeeView / DetailPanel, drawMatchScoreboard, the radial menu, tickMessage's ticker) with an anchor each, decided with
  Mariano; the compositor and the input remap.
- Hit tests (stages 2-3): course clicks through the render view (screenToTile receives the 800-space coordinate the wide view maps
  to; side clicks give x < 0 or x > 799, which the projection handles linearly), edge scroll at the wide edges, panel clicks
  through the compositor's remap; no click handler's code changes.
- Sim visibility (stage 2): keep worldToScreen's on-screen test at 800 (sim identical) or follow the wide view (a `play` switch:
  more sounds and story advances, since more golfers are visible). Needs Mariano's decision.
