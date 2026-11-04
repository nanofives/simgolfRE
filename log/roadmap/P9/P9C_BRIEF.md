# P9C: text at the window's resolution (item 4 of P9's feedback)

Mariano's item 4, verbatim: "font rendering is still pixelated after widescreen, maybe we need to rework the font rendering system?"
Mariano said "go ahead on item 4" (2026-11-04).

Today every HUD and screen text is rasterised once into the 800 x 600 back buffer and then scaled up with the rest of the frame
(or with its HUD layer, P9B), so at 1920 x 1080 text is blocky or blurred. Goal: the player sees text drawn sharp at the window's
pixel size, in the game's own fonts and colours, at the place and clip the game gave it, while everything the simulation or any gate
sees stays exactly as it is.

Read first: log/roadmap/P9/BRIEF.md (rules section and "Standing rules"), log/roadmap/P9/MERGE.md, log/roadmap/P9/P9B_notes.md (all of
it; section 5 is the interface you build: GfxTextRun, GfxLayerPx.run, the `under` plane, the presenter's two steps),
log/roadmap/P8/P8B_notes.md sections 1, 7, 8, CLAUDE.md's S5 paragraph (gditext / fttext / textlab: GDI and FreeType providers, faces,
ClearType facts, U-1520) and S3.7 (gfx_text.cpp: drawTextRun, drawMarkupRun, drawRichText, drawWindow, tips), P8A / P9 paragraphs.
Rules: log/roadmap/P4/BRIEF.md "Rules", unchanged.

Tree: port/build/P9C = git archive c5e66b59 (HEAD, P9 merged) + junctions (made by log/roadmap/P9/P9C_mirror.py; log/ is the shared
tree's, so write your deliverables only under log/roadmap/P9/P9C_*). Work only in that tree. Do not commit.

## What to build
1. The capture (P9B notes section 5): GfxTextRun published per frame with the layer it belongs to (a HUD layer or the centred 2D
   layer), the glyph pixels marked in the layer's record plane, and the pixels the run overwrote kept (`under`) so the presenter can
   remove the low-resolution glyphs. Cover every text path that reaches the screen: drawTextRun / draw_text_string (markup and plain),
   drawTextSurface's shadow run, drawCenteredText / drawCenteredText2, drawStringAt*, drawWindow / ticker / messages, the tips, the
   bio EditBox text (window target), the loading banner (P8A). List any text path you leave at 800-space and why.
2. The presenter: for each run, remove its 800-space glyphs from the layer (restore `under`), then draw the string at
   height * (window height / 600) at the scaled position inside the layer's placement, clipped to the scaled clip, with the run's colour,
   shadow offset scaled, through the same face resolution the port's TextMetrics uses (GDI by default; FreeType when that provider is
   chosen). Decide antialiasing (ClearType vs greyscale) from screenshots and say why. The centred 2D screens (title, location, reports,
   dialogs) get the same treatment when they are scaled.
3. Widths the sim reads stay the 800-space ones: never feed a scaled metric into a GameState write or a hit test. Line breaks, centring
   and clipping are the 800-space ones scaled; if a scaled string is wider than its scaled box at the new size, clip it (do not reflow)
   and measure how often that happens on the screens you test.
4. App option `--text-res native|frame` (default native; frame = today's picture). Add SG_APP_P9C=off to port/test/conftest.py if your
   default changes any app test that compares with the original's recordings. At 800 x 600 with scale 1, native must give a picture
   equal to frame in every pixel (prove it).

## Evidence
- The 800 x 600 back buffer and the GameState unchanged: segments x64 TSV equal to log/roadmap/merges/I25/segs_x64.tsv on status and
  digest, app selftests x64 + Win32 (incl. --selftest-p8b / p9a / p9b), framegate rows of sandbox_basic / cov_b_location unchanged,
  the live-trace sim equality of P8B section 9.
- Before / after screenshots at 1920 x 1080 and 1280 x 720: title menu, location screen, an in-game frame with the HUD title / meters /
  bottom band, a message ticker, a golfer thought label, a tooltip, a report (F2), a showPopup dialog, the loading banner. Plus 800 x 600
  96 dpi native == frame.
- Controls RED: a run left unremoved (double text), wrong scale, wrong colour, sim-side width taken from the scaled font.
- app test files one per process (test_static, test_profiles, test_app_shell, test_app_input, test_app_replay, test_app_ingame,
  test_app_present, test_app_live) pass with conftest's environment.

Deliver in log/roadmap/P9/P9C_*: notes (design, what changed, evidence, what is left), files list, build logs, segments TSV, selftest
logs, screenshots, and a patch against c5e66b59. Heavy runs (builds, pytest files, game / app boots, segments) through
log/roadmap/S3/w8_lock.py run P9C -- ... and test-slot. Reply DONE or BLOCKED with the reason.
