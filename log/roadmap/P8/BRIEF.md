# P8: the game-styled loading banner and widescreen stage 2 with native-resolution rendering (2026-11-03)

Mariano's requests (2026-11-03, after LAN release 649b6a5e):
1. "loading indicator should have the same font and style as any other game banner, be mindful for this and any future UI change
   we will make to it". STANDING RULE for every UI element the port adds: draw it with the game's own fonts and art (the banner /
   message window drawWindow + drawMessageBuffer, drawFrameR3 / S2, the FontRefs the game loads, its panel colours), through the gfx
   backend, never a host GDI box in the window DC.
2. "start working on widescreen, also work on the internal rendering to the selected resolution": the course view uses the wide
   window (log/roadmap/P7/PLAN.md stage 2), and the world is rendered at the window's real pixel resolution instead of an 800 x 600
   image scaled up.

Read first: CLAUDE.md's P4 / P5 / P6 + P7 paragraphs, log/roadmap/P4/BRIEF.md "Rules" (apply unchanged: own tree, default switches
and options unchanged = byte-identical to today, light testing, NO-GUESSING with addresses, w8_lock + test-slot for heavy runs,
`~/bin/py -3.12`, no bare `cmake`, never `pytest port/test` in one process, do not commit), log/roadmap/P7/PLAN.md and P7_notes.md,
log/roadmap/P6/P6B_notes.md section 2 (the current overlay).

Base: HEAD b786c29a (git archive in port/build/<ID>, made for you). Deliver in log/roadmap/P8/<ID>_* (notes, files list, profile rows
if any, build logs, segments x64 TSV equal to log/roadmap/merges/I25/segs_x64.tsv on status and digest, screenshots). Reply DONE or
BLOCKED.

## P8A: the loading banner (owns port/app/present.*, the loading part of host.*, and any new gfx body it needs)
- Replace P6B's GDI box with the game's own banner look: the same window / font / colours the game uses for its message banner (the
  ticker window drawWindow at the ticker position, or the boot's own showLoadingScreen bar if that fits better: decide from the art and
  say why), drawn by the gfx backend into a frame the presenter shows during the world load. The load pumps already present nothing;
  the banner must not write any GameState byte (sim hashes and recordings unchanged with the option on).
- Text "Loading..." in the game's font, progress as the game would show it (its bar art if one exists, else a bar in the panel colours).
- Keep `--loading-overlay on|off` (default on) and SG_APP_P6B=off.
- Screenshots of the banner at 96 dpi and in a scaled window; a side-by-side with a game banner (ticker / message window) frame.

## P8B: widescreen stage 2 + native-resolution world rendering (owns everything else: terrain_live, gl_raster, gl_wgl, gfx*, host /
window / main except the loading code, input_map)
Defaults Mariano has not overruled (state them in your notes): logical option A (W x 600, W from the window's aspect at session start,
`--wide-width` override); the SIM keeps the 800-wide visibility test, screen words, scaled fonts and sprite divisor (sim identical to
today with the same commands); HUD drawn centred at x + (W - 800) / 2 for now (anchoring is stage 3); `--wide` stays off by default.
1. Render view: a per-context view (logical W x 600, origin (W - 800) / 2, and a RENDER SCALE s = window pixels / logical pixels)
   outside the GameState, set by the app.
2. Terrain at W and at native resolution: lift U-1310 (GlRaster / WglSink at any size), the render-side Terrain::resize / viewport /
   projection / culling for W, renderTerrainStrip widths for W (checked against the original's three rows), and the world layer
   (terrain, sprite queue, overlay lines, labels) drawn at W*s x 600*s pixels: the projection and the scene bodies scaled by s, sprite
   art scaled (nearest or smooth by the existing --scale), lines at width * s.
3. The 2D UI layer stays 800 x 600 logical art, composed over the world layer at s (smooth / nearest per --scale).
4. Input: course clicks and the edge scroll through the render view; panels keep 800-space hit tests.
5. Gates: `--wide off` and render scale 1 byte-identical (segments x64 = I25, app selftests, framegate rows of sandbox_basic /
   cov_b_location unchanged); with --wide on, the sim hashes of a live-trace replay equal --wide off's; the central 800 columns of the
   world layer at s = 1 equal today's frame where no HUD covers them; screenshots of sandbox_basic and a built course at 1920 x 1080 and
   2560 x 1440, wide and 4:3; frame times at s = 1, 2.4 (1920 x 1080 from 800 x 600 height... measure what s is) with and without workers.
6. Coordinate with P8A only through the brief: P8A owns present.*'s loading code; if you must touch present.* for the layer
   composition, keep the loading functions untouched and say so in your files list.
