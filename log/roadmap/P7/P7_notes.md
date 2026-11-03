# P7: widescreen, stage 1 (2026-11-03)

Tree: port/build/P7 (git archive of f04c7591). Plan: log/roadmap/P7/PLAN.md (logical size options, every place 800 x 600 is
assumed, layout against simulation, the stage list and what each later stage needs). Changed files: P7_files.txt. Every change is
in the Windows host (port/app). No port/src, port/include or port/test file changed, so the simulation, the segment gate and
every recording replay are untouched by construction. Class (b): app-side behaviour, behind an app option. Its default is OFF
until Mariano has seen it, as the brief asks.

## 1. What `--wide` does

`simgolf_port --wide` (or `--wide on`, or `SG_APP_WIDE=on`; a flag on the command line wins over the environment):

- The window opens with a client of `--wide-width` x 600 logical pixels, default 1067 x 600 (16:9), scaled by the dpi like the
  800 x 600 window (window.cpp size_for_dpi). It stays resizable (P5B item 4).
- The game still draws its 800 x 600 frame. The content rectangle is unchanged: present.cpp fit_viewport fits the 4:3 frame at
  full height and centres it. The input translator and the cursor answers therefore map client points to the same logical
  points as before. No input code changed.
- Side bands. Before P7 the bands left and right of the content were black. With `--wide`, on a frame the host marks Extend, the
  bands carry the frame's own edge colours outward. The content's pixels are never touched (selftest
  p7.content_pixels_untouched). Two fills (`--wide-fill`):
  - `soft` (default): each band column takes, per row, the mean of the frame's outer 4 logical columns (in client pixels: the
    scaled image's edge columns), blurred vertically. The blur radius grows with the distance d from the frame: min(d / 2 client
    pixels, 48 logical rows). Next to the frame the band continues the image exactly. Further out, the lines and the wisps of
    items that touch the edge spread into the background colour.
  - `clamp`: the frame's outermost column repeated, no blur.
  - `black` (with `--wide`): the 16:9 window with black bands.
  Rows of a letterbox (a client taller than 4:3) stay black in every mode.
- Which frames get the fill. LiveSession marks each present Extend or Black (host.cpp track_screen, present):
  - Extend: the full-screen 2D screens shown before a game's frame loop runs. That covers the boot's loading screens, the title
    menu and its sub-screens (theme packs, difficulty, pick a pro, character editor, load browser, credits), the location screen
    and the world-load screens.
  - Black: once the frame loop runs. That covers the course view and every report or dialog drawn over it. Stage 1 changes
    nothing in game, as the brief asks.
  - The rule reads the game's own word and writes nothing. A frame head, or a body pump (P6 / H1) with g_difficulty != -1, sets
    in-game. Any pump that reads g_difficulty == -1 clears it: the title menu stores -1 there at 0x0040f8ab (the rule P5B's close
    logic uses). A new career's loading and location screens therefore come before the first frame head and read as 2D again.
- session.json gains `p7` {wide, wide_width, wide_fill}. The session summary gains `presents_band` {extend, black}.

## 2. `--wide` off is byte-identical

- The presenter's fill runs only when `set_wide` is not Off AND the frame's hint is Extend. Without `--wide` the presenter is never
  set wide, so render() is the pre-P7 code path.
- The window's logical width defaults to kLogicalW (800).
- The host's band hint and its counters are written on every present, but they are read only by the presenter (and summary_json).
  The sim thread reads g_difficulty, which close_check already read. It writes nothing, so the trace and framehash.bin do not
  change.
- Selftest rows p7.wide_off_equals_pre_p7_render and p7.black_hint_equals_wide_off check it on a 1920 x 1080 smooth render.
- Segments x64 (section 5) are unchanged by construction (no portsim file changed) and were run anyway.

## 3. Evidence

- `--selftest-p7` 9 / 9 PASS (P7_selftest_p7.log):
  - wide off equals the pre-P7 render;
  - a Black hint equals wide off;
  - the content pixels are untouched;
  - Extend lights the bands;
  - a uniform frame fills uniformly (soft and clamp);
  - clamp repeats the edge column at 1x;
  - the soft blur grows with distance (a one-row white line: 255 next to the frame, 0 < v < 64 at the far edge, lit 10 rows
    above);
  - letterbox rows stay black.
- Control `--negctl p7-band-black` (every hint Black): 5 rows RED, the four wide-off / untouched / letterbox rows still PASS
  (P7_selftest_p7_negctl.log).
- `--selftest` 37 / 37 and `--selftest-p5b` 6 / 6 unchanged.
- Screenshots composed by the app's own presenter (`simgolf_port --compose <in.555> <cw> <ch> <out.bmp> --wide ...`) from the pinned
  original's reference frames, which equal the port's frames on these screens. They are in log/roadmap/P7/shots/ (index.tsv),
  each screen at 1920 x 1080 soft, 1920 x 1080 clamp and 1067 x 600 soft. The screens: title menu (sandbox_basic menu), location
  screen (cov_b_location buttons), loading screen, difficulty, theme packs, championship courses, pick a pro, character editor,
  and one in-game frame with its Black hint (sandbox_basic sandbox_20).
- Live app (P7_drive.py): section 4.

## 4. Live run

`~/bin/py -3.12 log/roadmap/P7/P7_drive.py --dpi 96 --fill soft --scale nearest` (log P7_drive_96.log, result
live/result_dpi96_soft_nearest.json, captures live/{title,location,ingame}_dpi96_soft_nearest.png, recording
live/rec_dpi96_soft_nearest/ without trace.trace). Posted messages only, `--profile fixed`.

The run:
- The window opened with a 1067 x 600 client (content 800 x 600 at x 133).
- On the title menu, the script clicked Sandbox at the client point that shows the 800-space point (130, 445). The location
  screen came, so the click mapped back through the centred viewport.
- On the location screen it clicked Monterey (270, 32). The world loaded and the frame loop ran.
- Esc on the welcome window, then the in-game capture.

| measure | value |
|---|---|
| title menu: non-black side-band pixels | 160,200 of 160,200 |
| location screen: non-black side-band pixels | 160,200 of 160,200 |
| in game: non-black side-band pixels | 0 of 160,200 (stage 1: black) |
| presents_band | extend 56, black 57 |
| records | 57 |
| recording replayed through sg_run_boot (app_replay.gate_one) | PASS, 57 / 57 records |

## 5. Gates

- Builds: x64 and Win32 rc 0, 0 errors (P7_build_x64.log, P7_build_Win32.log; both compile main.cpp and present.cpp). The Win32
  `--selftest-p7` also reads 9 / 9.
- Segments x64 (P7_segs_x64.tsv, log P7_segs_x64.log): 205 / 205 PASS. Status and digest equal to
  log/roadmap/merges/I25/segs_x64.tsv on all 205 rows.
- `--selftest` 37 / 37, `--selftest-p5b` 6 / 6, `--selftest-p7` 9 / 9, control 5 RED (P7_selftest*.log).
- Not run: the app pytest files. No behaviour changes without `--wide`. The two code paths that run without it are the extra
  read of g_difficulty per pump and the band counters. Neither touches a presented pixel, the trace or framehash.bin.
- Process: the w8 lock could not be taken from 14:26 to about 14:58, because the 16 GB commit-charge need was out of reach
  (orchestrator's note: the pagefile is now 16 GB, so pass `--need-gb 8`). Every P7 run after that used `--need-gb 8`.

## 6. What the screens look like, and choices left to Mariano

- Title menu: its background is a soft lavender wash with a dark line at mid-height. With `soft` the wash continues to the window
  edges and the line fades out across each band. With `clamp` the line runs to the window edge and the wash shows faint
  horizontal streaks.
- Location screen: the left edge is one flat colour (the band is exact). Items on the right edge (the money box frame, the Jamaica
  label) leave grey wisps that `soft` spreads and dims.
- Theme packs / difficulty / pick a pro / editor: flat or near-flat edges, so the bands read as a continuation.
- In game: black bands (stage 1). The wider course view and the HUD at the edges are stages 2 and 3 of the plan.
- Mariano should choose: `soft` or `clamp` as the fill, whether `--wide` becomes the default, and the plan's open decisions
  (PLAN.md section 5: the sim visibility rule, and the anchor of each HUD panel).

## 7. Later stages (PLAN.md section 4 / 5, short)

- Stage 2 (wide course view):
  - A per-context render view outside the GameState. g_screenWidth stays 800, because worldToScreen's on-screen test gates
    playSoundAt's RNG draw (U-0860) and story visibility, scaleX sizes two fonts, and the gfx root's height comes from the width.
  - GlRaster / WglSink at W x 600 (U-1310) and Terrain::resize on the render side.
  - renderTerrainStrip widths for W (terrain_live.cpp:343-362 knows three sizes).
  - The scene / overlay drawn through the render view.
  - Course clicks through the render view, and the edge scroll at the wide edges (H1, tick_frame.inc:8288-8293).
- Stage 3 (HUD anchoring):
  - A compositor that moves each HUD rectangle (drawn in 800-space as now) to an anchor in the wide frame and maps a click inside
    it back to 800-space, so no click handler changes.
  - One anchor decision per panel.
- Stage 4: dialogs / reports centred over the wide course.
- Stage 5: W chosen from the window's aspect at session start and recorded.
