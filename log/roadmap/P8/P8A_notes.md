# P8A: the game-styled loading banner (2026-11-03, tree port/build/P8A on b786c29a, not committed)

## What changed
- P6B's GDI box (Presenter::paint, host fonts and brushes in the window DC) is gone. The banner is now drawn by the gfx backend
  with the game's own art and font: new body `loading.banner` in the `text` family (port/src/sim/gfx_text.cpp, appended to
  kBodies, one new function above it). It issues the calls drawWindow issues for the ticker window: setDrawTarget with
  g_font51b360, drawFrameR3 at (x - 8, y - 8, 0x1a0, 0x50, 1) centred horizontally at 5/12 of the height, then
  drawTextSurface's path (text_surface, colour 0x80007fff) for `Loading...` at (x + 0xc, y + 0x10), as drawWindow does for sel == -1.
- Why the ticker window and not showLoadingScreen: its bar and picture are drawn only for the boot's art set and consume two
  Random::range draws (sim state); the ticker frame is drawn by the same calls on any screen and touches no sim word.
- Progress bar: no bar art exists in the game outside showLoadingScreen, so it is a track and fill in the banner's own colours
  (white 0x7fff border, panel dark 0x2108, the text-frame gold 0x7fe7). It fills done / expected pumps, at most 95 %, as P6B did.
- Isolation: the body swaps g.root for a copy of g.back while it runs and puts g.root, root_terrain back; the picture goes to
  `$gfx/banner.555`. The real back buffer and the GameState are not written (the body only reads GameState). Per-target colour
  state kept by prim_text (setQuad) is overwritten by the text call exactly as every text draw does; not restored (the next frame
  text calls set it again first).
- Host (port/app/host.cpp / host.h): `LiveSession::post_loading(ctx, done)` runs on the sim thread, calls gfx_drop("loading.banner"),
  reads banner.555 and hands it to the window as an ordinary frame with index kBannerIndex (0xffffffff, band Extend). At the end of
  a load (done == 0) the frame the load interrupted (last_presented_) is put back on screen. kMsgLoading, Presenter::set_loading
  and the paint code are removed; main.cpp skips the shown.tsv hash line for kBannerIndex frames.
- `--loading-overlay on|off` (default on) and SG_APP_P6B=off unchanged. With the option off or in every test (conftest), no
  banner call is made: segments and boot paths do not run it.

## Files
port/src/sim/gfx_text.cpp (body added; P8B owns gfx*: a one-function append, no other edit), port/app/host.cpp, host.h, main.cpp,
present.h, present.cpp (loading code removed only). present.* loading functions are gone, so P8B may edit the rest freely.
Deliverables: log/roadmap/P8/P8A_notes.md, P8A_drive.py (driver), P8A_build.log, P8A_build_dll.log, P8A_segs_x64.tsv,
P8A_side_by_side.png, P8A_runs/{dpi96,scaled,dpi192}/ (dpi192 is a failed run: the driver's clicks miss at --dpi 192, ignore).

## Evidence
- Screenshots: P8A_runs/dpi96/load_04.png (800x600 window), P8A_runs/scaled/load_04.png (window resized to 1600x1200,
  --resizable --scale smooth): the banner in the game's frame, panel and font over the location screen, 0.6 s into the load.
- Side by side (P8A_side_by_side.png): banner next to the game's own save dialog frame (cov_a_keys k_dlg_S reference). No pinned
  frame holds the ticker window itself and two driver runs (after the load, 6 x 12 frames) showed no message banner, so the
  comparison with the ticker frame is by construction (same drawFrameR3 / font / colour calls), not a pixel comparison.
- Gates on this tree: segments x64 205 / 205 PASS, status and digest equal to log/roadmap/merges/I25/segs_x64.tsv on all 205
  rows (0 differ); test_gfx_text 32 passed / 20 skipped (oracle opt-in); test_app_shell 18 passed; port test_static 6 passed.
- Load timing with the banner (driver runs, click to third frame head): 2.15 s, 1.74 s (scaled), 1.88 s; P6B measured 1.53-1.67 s for
  click to first frame head; the banner call adds one back-buffer copy plus a drawFrameR3 per 50 ms.

## Not measured
- Sim hashes of a live session with the banner on versus off were not compared (the body does not write GameState by construction;
  no recording was kept). The 192 dpi run did not reach the world (driver issue). The text colour is the ticker's 0x80007fff
  main colour; its on-screen tint over the location screen looks pale orange.
