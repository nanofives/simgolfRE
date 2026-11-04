# P9B: the layered 2D compositor (2026-11-03 / 04)

Tree: port/build/P9B (git archive of 2fd3553c). Brief: log/roadmap/P9/BRIEF.md section P9B (items 6, 7, 8, the tropical bush of
fb_3 / fb_4; item 4 as an interface design only). Changed files: P9B_files.txt. No profile rows (no sim switch). Nothing committed.

## 1. What was wrong (P8B / G32)
G32's HUD compositor moved rectangles of the finished 800 x 600 frame (present.h kHudPanels, compose_world): every 2D pixel inside a
panel rectangle moved with the panel, and a pixel was "the panel's" when it differed from the frame's terrain picture T. Measured on
the P8B tree's own images (`before` in P9B_shots/):
- fb_2 / item 6: the message window drawn over the meters' rectangle (630..800 x 0..145) or the title's (0..250 x 0..95) is cut: its
  part inside the rectangle moves to the edge (r1920x1080_cov_b_career_75 / _1200 pairs: the welcome and membership windows, the
  portrait cut by the title rectangle). Golfer name labels inside the bottom band's rectangle (y >= 425) move with the band away from
  their golfer (sb pairs, cov_r_s2_screens_336, cov_b_career_150: "Gary Golf" 240 client px left of Gary at 1920 x 1080).
- item 7 (fb_3 dented edges): pixels the panels blend (blitPair1's mask blend for the frame / palette borders, Sprite::drawG's
  half-bright shade for the title strip and meter boxes) carry the terrain that was under them in the frame; moved, they show that
  old terrain at the new place (live_*_hawaii_ingame pair: the title box's lighter, wrong-coloured interior).
- fb_4 / item 8: a hovered slot's tip is drawn by the palette above the palette (y 0x185..0x202, partly above y 425), so the
  rectangle splits it: the part below 425 moves left, the header stays (live_1920x1080_hawaii_tip_top_7 / tip_bottom_right pairs: the
  "Tropical bush 1000" tip of fb_4 and "Palm tree 1000"). The palette's scenery icons that overflow the palette top are split the
  same way. That is the item-8 "hovering looks wrong" and the fb_4 bush: confirmed.
- The tropical bush icon itself (fb_3): the original draws the scenery tools' icons as tree sprites that overflow the palette's top
  edge (log/c3/alt/refs/cov_a_build/det_b/terrain_03.png, a parkland course of the ORIGINAL: the tree and palm icons rise above the
  palette's top right). Its look is the original's; what P8B broke was cutting it at the rectangle edge.
- (found on the way, fixed) with `--cursor message` the cursor answers took the mouse message's point before G32's remap
  (window.cpp AppWindow::input remapped a local copy of lp; LiveSession::cursor read e.lp), so hovers over a moved panel tested the
  wrong 800-space point (the right-end slots answered the Stream / Rocks tips). The system-cursor path (play) remapped already.

## 2. Design: layers made where the pixels are drawn
- SIM SIDE (gfx backend, port/src/sim/gfx.cpp + gfx_body.h, API in port/include/portsim/sim/gfx.h). With capture on
  (gfx_set_hud_capture, called only by the app's host when the layered compositor is on) the backend records, between two
  Window::refreshFull bodies, which drawing calls belong to which HUD panel:
  - the frame code opens a layer around a panel's drawing (GfxLayerScope, RAII, nested scopes count once): tick.cpp TkBase::call
    around the callees gfx_layer_of_callee names (showMainRadialMenu, showInfoCursorPanel, drawBuildingPanel, drawTerrainToolbar,
    drawAmenitiesPanel = bottom band, left edge; drawEmployeeViewPanel, drawEmployeeDetailPanel, drawGolferActionPanel = the panel
    layer, right edge), and tick_frame.inc around the inline passes P6_Hud (the golfer strip = bottom band; from ML+0x9929 the title
    strip, course title, date and counts rows = title layer, left edge) and P6_StatusBar (the three status boxes = meters layer,
    right edge); showTournamentResults is the title layer too (its per-frame leader board is drawn in the title strip's place, frame
    at (0, 8), 0x90 wide; its scorecard mode dims the whole frame, which clears the terrain flag, so no world and no layers there);
    P7's aiming block ML+0xc0b4..ML+0xc54d (club, distance, lie and % lines at x 0x196, the club-stat rows from (0x1ee, 0x21c)) is a
    panel-layer range: those texts are drawn in the golfer action panel's place by mainLoop itself, not by drawGolferActionPanel
    (without it they stayed behind when the panel moved right). drawGolferInfoPanel is NOT one: its popup is drawn at the top of
    the frame over the view (the original's cov_a_golfer panel_0 reference), so it stays centred.
  - at a layer's open the root surface is snapshot; at its close every root pixel that differs from the snapshot becomes a pixel of
    that layer (bounding box + per-pixel record). Pixels changed outside every layer (labels, the ticker, previews, dialogs) form
    centred layers in draw order (the gap before each layer and the tail before refreshFull), and the back buffer's own child-window
    composition (gfxe::compose) a last centred layer. The root before the first layer is the BASE.
  - the two blend primitives record how a pixel was drawn when it was drawn over the segment's background (a pixel the layer had not
    drawn yet): Surface_stretchBlit's mask blend (kind 2: source word, weight k) and Sprite::drawG's shade byte (kind 3: shade table
    0 = g_color16Lut's block 1, 1 = g_halfBrightLut's table). A pixel the layer drew over its own earlier pixel, or rewritten after the
    blend, is opaque (kind 1). Text, sprites and fills are opaque.
  - Window::refreshFull publishes {base, back buffer copy, layers}; a refreshFull inside an open layer (a modal screen) publishes no
    layers. Nothing is written into the GameState, the root or the back buffer: the 800 x 600 frame every gate sees is unchanged, and
    with capture off (every test, gate and corpus run) no code path changes except one relaxed atomic load per frame-code call.
- APP SIDE. host.cpp make_world_image: when the frame's published layers were published with exactly this back buffer, P8B's world
  mask is taken over the BASE (terrain or queued sprite pixels of the base), and the WorldImage carries the layers (WorldImage::hud).
  present.cpp: render scales the base instead of the frame, compose_world puts the world where the base mask says, then
  compose_layers draws every layer in draw order at its anchor (left: -origin_x client px, right: + the right band's width, centred:
  0), at the frame's scale (window height / 600). Client pixel (x, y) of a layer at offset dx shows the layer's 800-space pixel under
  (x - dx, y): nearest one pixel, smooth the four around the client pixel's centre, each composed over the client pixel already there
  (d): transparent = d, opaque = its word, kind 2 = blitPair1's channel rule over d's 5-bit channels, kind 3 = the shade table at d's
  RGB555 word; the bilinear sum of the four gives smooth panel edges with no terrain pixel chosen by a mask inside a panel, and a
  translucent pixel shows what is under it at the panel's new place.
- REMAP by coverage (PanelRemap::set_layers / map): the topmost layer with a pixel under the client point (each at its own place)
  decides; a moved layer maps the point back to its 800-space place, a centred layer leaves it. Mouse messages and both cursor
  sources go through it.
- Golfer / employee panels at the right edge (G32's ask): done for drawGolferActionPanel / drawEmployeeViewPanel / DetailPanel, which
  are separate draw calls in the palette's place (ML+0xa27d, ML+0xa2ee), plus the shot texts above; the radial menu stays at the left
  edge. The golfer info popup stays centred (it is a top-of-frame popup in the original, not a bottom panel: P8B's notes 7.4
  placed it in the bottom band).

## 3. Options (port/app/main.cpp)
- `--hud-layers on|off` (default on; used with --wide and --hud-anchor on, the defaults). Off = G32's rectangle compositor as shipped
  in b83d8213. SG_APP_P8B=off (port/test/conftest.py) turns --wide off, so the app tests that compare with the original's recordings
  see no layers: no SG_APP_P9 entry was needed.
- `--selftest-p9b`; controls `--negctl p9b-blend-opaque` (blended layer pixels keep their in-frame words) and `--negctl
  p9b-rect-move` (G32's rectangles although the frame has layers); both also act in the live window and in live-trace --world-shots.
- Development: SG_P9B_LAYER_LOG=<file> (host.cpp) appends one line per layered present (serial, id:anchor:box:pixels/blended).
- session.json `p8b.presents_layered` counts presents composed with layers.

## 4. Evidence (logs in log/roadmap/P9/; every gate row is the FINAL tree, P9B_final.sh -> P9B_gates.log; the live Hawaii drives and
the cov_b_career / cov_r_s2_screens pairs were made before the last change, the P7 shot-text range and the leader-board tag, which
touch only championship frames)
| gate | result |
|---|---|
| builds | x64 and Win32 rc 0, 0 errors, 0 warnings (P9B_build_x64.log, P9B_build_Win32.log) |
| segments x64, default options | 205 / 205 PASS, status and digest equal to log/roadmap/merges/I25/segs_x64.tsv on all 205 rows (P9B_segs_x64.tsv) |
| app selftests x64 and Win32 | --selftest 37/37, p5b 6/6, p7 9/9, p8b 11/11, p9b 10/10; controls p9b-blend-opaque 2 RED (the blend rows), p9b-rect-move 7 RED, p8b-mask-ignored 2 RED, p8b-hud-unmoved 3 RED (P9B_gates.log, P9B_selftest*_*.log) |
| framegate, default options | sandbox_basic (10 rows) + cov_b_location (5 rows): status, total / one-level / beyond px equal to log/roadmap/merges/I24/census.tsv on all 15 (P9B_framegate.tsv) |
| sim with capture on (live-trace, `fixed`) | trace.trace and framehash.bin byte-identical for --wide off, --wide --hud-layers off (rectangles) and --wide (layers, with world shots composed): sandbox_basic 439 records, course_season 6,296, championship 2,305 (P9B_simeq.tsv) |
| live app, `fixed`, Hawaii sandbox (P9B_drive.py) | 1920 x 1080 and 1280 x 720, layers on and off: each session's recording replays through sg_run_boot PASS 132 / 132; 132 / 132 presents layered with layers on, 0 with off (P9B_drive_*.log) |
| app test files (conftest env, final tree) | test_app_shell 18 passed, test_app_present 3 passed (4 deselected), test_app_input 16 passed, test_app_replay 14 passed, test_app_ingame 14 passed (P9B_t_*.log) |

Screenshots (P9B_shots/, index.tsv; every `_pair.png` is before (top, --hud-layers off = b83d8213's image) / after (bottom)):
- fb_2 / item 6: r1920x1080_ and r1280x720_cov_b_career_75 (the welcome window over the meters), _1200 (a portrait message over the
  title and meters), _150 / _225 (Gary's thought label "Now we need to build a green.", the palette open), cov_r_s2_screens_334 / _336
  (golfer name label beside the palette, the radial hover label `Build Course`).
- fb_3 / fb_4 / item 8 on the tropical course (live_<size>_hawaii_*): tip_top_7 (the "Tropical bush 1000" tip of fb_4 at the palette's
  right end), tip_bottom_right ("Palm tree 1000"), tip_top_right (the hovered bush icon, no tip there: the slot boxes are the sim's),
  ingame (item 7: the title box's translucent interior), hover_buildings (the radial Buildings button hovered).
- the right-edge panel: r1920x1080_ / r1280x720_championship_300 / _690 / _720 (Gary's turn: the shot panel with its Attitude /
  Club / Distance / Lie texts at the right edge, the radial menu at the left, the leader board with the title at the left; _720 a
  portrait message over the title / meters rectangles).
- 800 x 600 at 96 dpi: no side bands, so the layered compositor never moves a layer (anchor needs origin_x > 0) and the frame is shown
  as before; sandbox_basic presents 100 / 300 / 447 at --render-scale 800x600 --shot-size 800x600 are byte-identical
  with --hud-layers on and off (P9B_runs/id800_on|off).

## 5. The P9C text interface (item 4, designed here, not built)
A text run is captured at the text family's run drawer (gfx_text.cpp drawTextRun / gfxb::draw_text_string, the one path every
marked-up and plain HUD text takes) while the layer capture runs:
```
struct GfxTextRun {             // gfx.h, published in GfxHudFrame::runs (draw order)
  uint32_t font_key;            // the FontRef the target's +0x5c..+0x68 named (gditext's font table key)
  std::string face;             // the LOGFONT the port built for it (gditext.cpp Font::create): face, height, weight, italic,
  int32_t height, weight;       //   quality, so a host can create the same font at another size
  bool italic;
  uint16_t colour;              // RGB555 of the run; shadow: colour, dx, dy (drawTextSurface's shadow run)
  bool shadow; uint16_t shadow_colour; int8_t sdx, sdy;
  int32_t x, y;                 // baseline start in 800-space (after centring / right alignment: the position the glyphs got)
  int32_t clip[4];              // the target's clip rectangle in 800-space
  std::string text;             // the bytes drawn (markup already resolved into runs)
  int32_t layer;                // index into GfxHudFrame::layers of the layer that was open (or the centred gap) when it drew
  int32_t box[4];               // the 800-space pixel box the run wrote
};
```
and GfxLayerPx gains `uint16_t run` (run index + 1, 0 = none): the run drawer marks every root pixel it writes in the layer's
record plane (as note_blend does), so a presenter knows which layer pixels are the run's glyphs. P9C's presenter then (1) treats the
run's pixels as transparent in its layer (the panel art under the text stays: the glyph pixels were blended over it, so the
layer's art under them comes from the base / the layer pixels the run overwrote, kept in a second per-layer plane `under`), and
(2) rasterises `text` with the same font at height * (window height / 600) at the run's scaled position in the layer's client place,
clipped to the scaled clip box, through the context's TextMetrics (GDI CreateFontIndirect or FreeType), with ClearType or greyscale
at window resolution. Widths the sim reads stay the 800-space ones (TextMetrics at the original size): only the picture changes.

## 6. Left / limits
- A point in a moved panel's VACATED place (where the panel was in 800-space, now showing world) still reaches the game at that
  800-space point, and the game's own hit tests see the panel there. The layers cannot change that: the panels' hit tests are in the
  sim (hitTestSlot432f90, nearestMenuSpot, ...). Removing it needs `play` switches in those hit tests fed by the panel offsets (a view
  block word, as G32's width) or a click swallow in the vacated place; neither is done.
- Things the game draws at the 800-space cursor point while the cursor is over a moved panel (P7's tool previews and labels when a
  tool is held over the palette) are centred layers: they appear at the panel's old place. A `follows the cursor's layer` anchor
  (the layer the remap used for the last cursor answer) would move them with the cursor; not needed for the reported cases, not done.
- The bottom band's golfer strip (P6_Hud, y 0x23f) is a bottom-band layer (left edge); in tournament frames (g_flags bit 0x200000) it
  is not drawn.
- Text and sprites inside layers are the 800 frame's pixels scaled (nearest / smooth); window-resolution text is P9C (section 5).
- ClearType text drawn over the world by a panel (the palette's tool labels above the palette top) carries its old background in its
  fringe pixels (opaque in the layer); P9C's runs remove that.
- Hit test of the tropical scenery slots: on Hawaii, 800-space (735, 512) gives the Tropical bush tip and (770, 565) the Palm tree
  tip, while (770, 527), inside the drawn bush icon, gives no slot: the boxes come from hitTestSlot432f90 over the slot sprites'
  heights (commands_panels.inc: y in [top - 0x2c, top), top = the SpriteRef height - 0xa + the table's y), the original's rule.
  Not changed.
