# P9: third play-test feedback (2026-11-03, on LAN / GitHub release b83d8213)

Mariano's feedback, verbatim, with his screenshots in this folder:
1. "opening with CMD should be optional or a different exe"
2. "ESC on a menu should go back"
3. "on the first frame the clickable areas are not working properly, i may have the mouse somewhere near a button (but not over)
   and it would be detected as a hover"
4. "font rendering is still pixelated after widescreen, maybe we need to rework the font rendering system?"
5. fb_1_band_blur.png: "difumination is not bad for most cases, but sometimes when there's a shape cut off soon it looks odd"
   (the P7 band fill beside the 2D screens: the Jamaica label's shadow is smeared into a long streak)
6. fb_2_message_cut.png: "some messages are cut off when expanding widescreen, same with messages over golfer"
7. fb_3_palette_edges.png: "UI borders looks dented and not properly cut off, adjust transparency of elements on edges, also
   tropical bush looks wrong"; fb_4_bush_tip.png (the bush / palm / tropical tree icons and their labels drawn outside the palette,
   at another scale, over a tip window)
8. "hovering buttons looks wrong on widescreen"

Standing rules (Mariano): every UI the port adds uses the game's own fonts and art through the gfx backend, never a GDI box.

Read first: CLAUDE.md's P4 / P5 / P6 + P7 / P8A / P8B paragraphs, log/roadmap/P4/BRIEF.md "Rules" (apply unchanged: own tree, default
switches and options unchanged = byte-identical where the original is the oracle, light testing, NO-GUESSING with addresses, w8_lock +
test-slot for heavy runs, `~/bin/py -3.12`, no bare `cmake` (build with port/test/build.py from a Python script, SG_FREETYPE_ROOT
C:/Users/maria/vcpkg/installed/x64-windows or x86-windows), never `pytest port/test` in one process, do not commit),
log/roadmap/P8/P8B_notes.md (sections 1, 7, 8: the world layer, the HUD compositor kHudPanels / compose_world / PanelRemap and its
listed limits), log/roadmap/P6/P6B_notes.md, log/roadmap/P7/PLAN.md.

Base: HEAD 2fd3553c, a git archive in port/build/<ID> (made for you; junctions to original/, log/, port/corpus, tests/golden).
Deliver in log/roadmap/P9/<ID>_*: notes (design, what changed, evidence, what is left), a files list, profile rows if any, build logs,
segments x64 TSV equal to log/roadmap/merges/I25/segs_x64.tsv on status and digest, app selftests, before / after screenshots at
1920 x 1080 (and 96 dpi 800 x 600 where relevant). Reply DONE or BLOCKED with the reason. New app options default to the new behaviour
and are listed in the notes with their off form; app tests that compare with the original's recordings must keep passing with
port/test/conftest.py's environment (add an SG_APP_P9=off there if your defaults change those tests).

## P9A: the shell and the menus (owns port/app/main.cpp option parsing, port/app/CMakeLists.txt, window.*, input_map.*, the P7 band
fill in present.cpp (only that function), host.cpp's non-compositor parts, and sim-side switches for item 2)
- Item 1: build the player's exe without a console (WIN32 subsystem; stderr / stdout still go to a log file in the data dir, and to the
  parent console when launched from one, AttachConsole(ATTACH_PARENT_PROCESS)). Keep a console build for tests and tools under another
  name (simgolf_port_console.exe) or a `--console` flag: decide, say which tools / tests launch the exe and keep them working
  (port/app/tools/app_drive.py, port/test/app_replay.py, scripts/lan_release/stage_lan_release.py + github_release.py PORT_FILES, the
  LAN updater's shortcut). If you add an exe, list it for the release scripts (do not edit scripts/lan_release; say what to change).
- Item 2: Esc goes back on every menu / full-screen screen that has a back / cancel / done control and does not already take Esc in
  the original. First list the screens and what the ORIGINAL does on Esc there (from the code, with addresses). Implement as `play`
  switches (default 1 = original, 0 in kPlayProfile and re/port/play_profile.tsv), the Esc acting exactly as that control's click.
- Item 3: reproduce in the live app (cursor resting near, not over, a button on the first frame of a screen: title menu, location
  screen, difficulty, theme packs) and find the cause with evidence (cursor answers before the first WM_MOUSEMOVE, scaling / remap
  before the first resize, an uninitialised hover word the original also reads (courseActionMenu's are known: U-1260 family), ...).
  Fix what is the port's; if the original does the same, say so and propose a `play` switch.
- Item 5: the band fill must not smear a shape that is cut at the screen edge into a streak. Proposal to evaluate (decide from
  screenshots of every 2D screen): vertically blurred edge columns with a short fade to the screen's background colour, or a blurred and
  darkened mirror. Show before / after for the location screen (fb_1), title, theme packs, load browser, credits.

## P9B: the layered 2D compositor (owns present.* except the band fill, gfx*.cpp, terrain_live / gl_*, and host.cpp's compositor parts)
Items 6, 7, 8 and the bush in fb_3 / fb_4 have one cause class: P8B moves HUD panels by RECTANGLES of the finished 800 x 600 frame
(compose_world's back == T mask and kHudPanels), so anything else drawn inside a rectangle moves with it or is cut (the message ticker
in fb_2, golfer labels, tips across an edge), terrain under rounded panel corners is chosen per pixel (fb_3's dented border), the
scenery icons the palette draws through the scene / sprite path land in the world layer at another scale (fb_3 / fb_4 bush), and the
hover highlight is drawn in one layer while the panel moved in another (item 8: confirm). Replace it by layers made where the pixels are
drawn: the gfx backend tags every draw call with its origin (which HUD panel's draw function, by the call chain the port already has:
drawTerrainToolbar, drawAmenitiesPanel, the meters / title drawers, the radial menu, showInfoCursorPanel, the golfer / employee panels,
the tips they draw; everything else = the centred layer) and draws HUD panels into their own RGBA surfaces (alpha 0 where nothing was
drawn), so the presenter composes world, centred 2D, then each panel at its anchor, scaled with alpha (smooth edges, no terrain chosen
per pixel inside a panel). Remap clicks by panel layer coverage, not rectangle (the vacated-place limit goes away). Decide whether the
golfer / employee panels can now go to the right edge as G32 asked (P8B could not); do it if they are separate draw calls.
- Keep the 800 x 600 back buffer the GameState / gfx gates see unchanged (segments, framegate rows of sandbox_basic / cov_b_location, app
  selftests incl. --selftest-p8b, the live-trace sim equality of P8B section 9).
- Item 4 (fonts) is the NEXT step (P9C) on top of your layers; do not start it, but design the layer interface so a text run can later
  be kept as (font, size, colour, string, position, clip, layer) and rasterised at the window's resolution: write that interface down in
  your notes.
- Evidence: fb_2 / fb_3 / fb_4 / item 8 situations reproduced before and after (a tip near the palette's right end with the tropical
  scenery theme, a message window across a panel's rectangle, a golfer thought label, a hovered button), at 1920 x 1080 and 1280 x 720.
