# Phase 4: the boot path (U-1050's Setup part) - plan (2026-10-22)

Goal: the port starts from nothing but the user's own golf_clean.exe and game files. Today every port run starts from a
frame-head image of the ORIGINAL (a segment's start.data.bin). After this work a run starts from the exe's initialised data
(`sg_exe_fill_state(ctx, 1)`), runs the boot (C++ static construction, gameMain's simulation part, mainLoop's Setup with
loadInterfaceArt(1)), then the title menu and session already ported (tick_menu.inc), and reaches frame 1 with the original's
simulation state.

## Measured starting point (cov_l_point, capture log/p2/p21/cov_l_point/p4boot_a)

Against the exe's initialised image (fill_state 1), the original's state on the location screen (boot + title menu + the
Sandbox click) differs in 43 of the 203 sim objects; from there to frame 1 (the Monterey click, the session start) 20 more
change (all on paths already ported). The 43 come from the boot loaders (golfer types, story definitions, phrases,
celebrities, pros, top scores, theme, tile-type table), the RNG seed (seedFromTime), loadInterfaceArt's table resets and RNG
draws, the ticker table unk_0058dd34, and the menu's own resets.

## Gate

`sg_run_frames` with a trace that starts at process start (port/corpus/traces/<scenario>.trace, recorded from spawn): the
records before the first FRAME form a frame 0 (boot + menu), consumed by the boot body and the ported menu; the run is
compared record by record with the original's per-frame hashes of a capture run (log/p2/p24/<scenario>/c_a/framehash.bin,
recorded from frame 1) and its checkpoints. First target: frame 1 of cov_l_point (sandbox) equal in all 203 sim objects;
then whole scenarios from boot (sandbox_basic, cov_b_career_types, championship).

## Steps

1. Infrastructure: a boot flag on sg_run_frames (C ABI `sg_run_boot`), frame 0 in parse_trace, a Python BootRunner
   (port/test/boot.py) that builds the state from the exe and compares with c_a's frame records; first-divergence report.
2. Boot body (port/src/sim/boot.cpp): gameMain's simulation part (screen size words, seedFromTime from the trace's clock,
   showTitleScreen, loadSoundTable as far as they write simulation state), Setup (the tile-name table copy, the ticker seed,
   loadTopScores, Default.pro, course type and theme, loadInfoScreenArt, showLoadingScreen(0), loadInterfaceArt(1), the
   ticker table), then menu_session.
3. loadInterfaceArt(1): the full arm's table resets and its load manifests (kIa*) reduced to their simulation effect (the
   sprite-sheet cell allocation, as menu_flics.inc does for loadWorldAssets), plus the data loaders it calls.
4. C++ static construction: whatever the gate shows the dynInit_* constructors write into simulation objects.
5. Negative controls per step; the segment gate must keep its 203 digests.

## Result (2026-10-22)

Done. `sg_run_boot` (C ABI, x64 and the Win32 runner op 27) runs a trace from process start over `sg_exe_fill_state(1)`:
`parse_trace(..., boot)` makes the records before the first FRAME a frame 0, `Frame::boot` runs `BootCrt` (staticInit_820f04,
gTerrain from the TerrainApi, the tile list head), `BootGameMain` (screen size, the display token, initSinTable, the ten
startup fonts through the text provider, the UI defaults, seedFromTime from the trace's clock read) and `BootSetup` (the
converted Setup: tile attributes, menu slots, loadTopScores, Default.pro, course type, theme, loadInfoScreenArt's fonts,
showLoadingScreen(0), loadInterfaceArt(1), the ticker table), then the title menu and session (port/src/sim/tick_boot.inc).
loadInterfaceArt(1) is a generated table (re/tools/gen_iart.py -> menu_iart.inc: 441 steps, 386
loadFlic, two palette cuts). The gate is `~/bin/py -3.12 -m port.test.boot --all --jobs 3` (port/test/boot.py, tests
port/test/test_boot.py): every record of every scenario against log/p2/p24/<scenario>/c_a/framehash.bin.

Gate, x64 = Win32 on every row (log/p4/boot_gate.tsv, boot_gate_win32.tsv): 57 of 60 PASS, cov_a_golfer and cov_b_holes18
KNOWN (U-1062, cursor tile only, 67 and 7 records), cov_b_location NO_FRAMES. The segment gate keeps all 203 digests on both
backends (log/p4/segs_x64_boot2.tsv, segs_win32_boot2.tsv).

What the boot runs forced, in the order found:
- the palette geometry: cropSprite stores a cut's width, height and kind before it reads a pixel, and the build palette's
  hit test reads the height. loadWorldAssets now stores the 52 terrain buttons and the tool strip (per course type),
  loadInterfaceArt(1) its two cuts into g_sprites570cb8 (cov_k2_build, cov_a_build, cov_b_build: a click picked the wrong
  tool); test_palette_cuts_equal_the_original compares all 96 with the original's frame-head images for course types 0, 1, 3;
- a transcription defect in c3fy.cpp and the port: the tee-already-built message missed its third strcat (0x0041ebbe,
  " (If you want to move the tee for hole "), U-1063;
- WM_MOUSEMOVE: jgld's window procedure reads the cursor once per mouse move (all 5,422 in the corpus); the port does not run
  it, so those reads no longer count toward which pump takes a dispatch (cov_a_golfer's hole-stats screen), U-1064;
- showTutorialText (pages 1..29, converted from c3ai.cpp), listThemes and showThemePackSelect were ported; the hire dialog
  kept only appendNumber of each row's text, which grew g_textBuffer over the FontRefs (ui_tour), U-1065;
- the scenario's own install files: cov_b_title boots with Themes/Default.pro (scenario.py "files"), now attached by
  segment_files.files_for; GameFiles.normalize collapses repeated separators as Win32 does (Setup's loadThemeFile opens `Themes\\Default.pro`: the theme is still empty there).

Not reached by any scenario, so unverified: tutorial pages 1..29 (every corpus call has page 0), the theme screen's Cancel,
palette groups 21 / 22 in a click (their cut fields are compared as state instead), U-1066.

Quit (same day): cov_r_quit runs from boot through mainLoop's QUIT block to its return, every record equal, and the files the
quit writes equal the original's (re/analysis/data/sve_roundtrip.md "The quit"); the boot gate holds 61 scenarios: 58 PASS,
2 KNOWN, 1 NO_FRAMES, x64 = Win32.
