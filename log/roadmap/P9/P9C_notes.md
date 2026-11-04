# P9C: text at the window's resolution (item 4 of P9's feedback, 2026-11-04)

Tree: port/build/P9C (git archive of c5e66b59 + junctions). Brief: log/roadmap/P9/P9C_BRIEF.md. Patch against c5e66b59:
P9C.patch (checked to apply to a clean export of c5e66b59 and to give the tree's files: P9C_patch.sh). Files: P9C_files.txt. No
profile rows (no sim switch). Nothing committed.

Mariano's item 4: "font rendering is still pixelated after widescreen". Before P9C every text was rasterised once into the 800 x 600
back buffer and scaled with the frame (or with its P9B HUD layer). Now, with `--text-res native` (the default), the app draws every
text again at the window's pixel size with the game's own font, colour, origin and clip, and the 800 x 600 frame every gate sees
is unchanged.

## 1. Design

### 1.1 One choke point
Every text the graphics backend draws reaches a surface through one provider call, `TextMetrics::draw_glyphs` (jgld's TextOutA),
at three call sites, all wrapped (grep over port/src/sim: no other TextOutA or draw_glyphs caller outside the providers):
- gfx.cpp `draw_text_string` (drawCenteredText, drawTextWithColor, drawTextSurface and its shadow run, drawMessageBuffer,
  drawPanelBackdrop, the reports / dialogs / routing / credits families' plain strings);
- gfx_text.cpp `draw_run`'s `put_glyphs`, root branch (drawRichText / drawTextRun: every marked-up HUD text, drawWindow / the ticker
  / message windows, the HUD tips, drawStringAt*, drawStringRightAt, the shadow runs of style 0..3, the loading banner);
- gfx_text.cpp `put_glyphs`, window-target branch (the bio EditBox's text, roadmap S3.19).

### 1.2 The capture (sim side, port/src/sim/gfx.cpp, gfx_body.h, API in portsim/sim/gfx.h)
`gfx_set_text_capture(ctx, true)` (only the app's host calls it, for `--text-res native`). Each draw_glyphs call is bracketed by
`text_run_begin` (a snapshot of the target surface) and `text_run_end`, which diffs the surface and keeps a RUN
(`GfxTextRun`): the FontRef and Font::create's arguments for it (`game_font`: face, size, style bits, the LOGFONTA gditext builds),
the COLORREF, TextOutA's origin (TA_BASELINE) and bytes, the box of the changed pixels plus a margin of 2 (inside the target's clip),
and the box's words before and after the call. A window target's run is moved by the window's position (gfx_editbox.cpp
`gfxe::window_origin`). A FontRef outside the 15 startup fonts is not kept (`no_font`; 0 in every run measured).

Runs live on their surface (`Gfx::TextCapture::live`) until nothing of them shows. At every Window::refreshFull they are PEELED newest
first over a copy of the back buffer (`gfx_text_peel`): a box pixel that still holds the run's own `after` word is visible (`vis`),
and a visible pixel whose `before` differs from `after` is put back to `before`. What is left is `GfxTextFrame::clean`: the back
buffer without the visible low-resolution glyphs. A later write over a run (a panel, a tooltip, the next screen) breaks the chain
at those pixels, so the run is not visible there and nothing of it is removed or redrawn there.

Three rules found on real frames:
- REPEATS (the status meters, a label in a box nothing repaints): the game draws the same string over its own last copy every
  frame. Once the ClearType blend settles, the new run equals an older one in every word of before and after; the older copy is
  removed and the new one kept at the end (`merged`: 5,190 of 5,641 runs in sandbox_basic). Without it one frame carried 884
  runs, 400 copies of "0.00".
- QUARTER RULE: a run the screen has covered keeps a few pixels whose new word equals its `after` by chance (the loading screen's
  white quote under the location screen's white globe gave white dots). A run is kept only when at least a quarter of its glyph
  pixels still show; the peel is then made again without the dropped ones (`peel_kept`).
- THE WIDE SCENE (P8B G32): renderSpriteQueue draws every golfer label a second time into the wide scene, which becomes part of the
  world image, so the world under a label carried a low-resolution copy (a ghost under the sharp label; the scale-1 measurement of
  section 3.3 found it as 99..206 px per frame). Runs drawn during that pass (`on_wide`, flag `TextCapture::wide`) are the scene's
  (target 2): at the queue's end they are peeled off a copy of the scene (`LiveTerrainWorld::scene_text`), `world_frame(..., text
  = true)` lays that copy, and the ones beside the frame's 800 columns are published as WORLD RUNS (layer -3) drawn over the world.

With P9B's HUD layers the same peel is made per layer segment: when a layer (or the centred gap before / after one) closes
(`text_close_segment`), the runs drawn in it are peeled over the root, get `layer` = the layer's index and `vis_layer`, and the
layer pixels the peel changed become patches (`GfxTextLayerPatch`): what the layer holds without the glyph (transparent where the
layer had not drawn, marked k = 0xff so a click there is still the layer's; else the blend record or the opaque word under it).
Runs older than the frame's first layer are peeled over the layers' base (`base_clean`, layer -1).

Nothing is written into the GameState, the root surface or the back buffer; runs only read them. The widths the frame code reads
(Text_put, measureTextWidth, every centring and wrap) come from the provider at the 800-space size as before: the window-resolution
fonts exist only in the app's presenter.

### 1.3 The presenter (port/app/textres.*, present.*)
- `Presenter::set_text(GfxTextFrame)` with each frame (the host takes the runs published with exactly that back buffer:
  `take_text_frame`; the world mask is also computed over the run-free picture: `WorldImage::mask_text` / `px_text`).
- Step 1: the picture without the glyphs: the flat path scales `clean` instead of the frame; the layered path uses a copy of the
  HUD frame with `base_clean` and the layer patches (`hud_text_`); the world image without the scene's label glyphs.
- Step 2: `TextRaster::draw` for each run, in draw order: the base's and the world's runs after the world is composed, each layer's
  runs right after that layer is composed (so a later layer stays over them), at the layer's anchor offset. A run is drawn with
  the same LOGFONTA as gditext's Font::create (face, weight, italic, underline, OUT_TT_ONLY_PRECIS) at lfHeight
  -round(size * client_h / 600), TextOutA at its scaled origin (TA_BASELINE, transparent) onto a 16-bit RGB555 DIB holding the
  client pixels under it (the game's own text path: jgld's TextOutA onto a 16-bit surface), and only the client pixels whose
  800-space pixel (the presenter's floor mapping) is visible in the run are written back. Ink outside the run's scaled box is
  clipped, never reflowed, and counted (`clipped_runs`, `clipped_px`). Line breaks, centring and clipping stay the 800-space
  ones: every run's origin is its 800-space TextOutA origin scaled. A shadow run is its own run, so its 1 or 2 px offset is
  scaled with it.
- The font file is the one the context's gditext provider registered (FR_PRIVATE, process-wide), so the face resolves exactly as
  the sim's does (GDI resolution rules of S5: `Arial Bold` -> arial.ttf, Manual SSi / Klepto from the install).
- At a content rectangle of exactly 800 x 600 nothing is redrawn and nothing is peeled: the frame's text is already at the
  window's resolution, so native == frame by construction there. `--text-unit-redraw` (live-trace measurement) forces the redraw
  at scale 1 through the same path; section 3.3 shows it gives the frame's pixels too.

### 1.4 Antialiasing: `system` (lfQuality 0) by default
P9C_shots/aa_compare_cov_b_career_75.png (rows: frame, native system, native cleartype, native grey; the welcome window at 1920 x
1080, nearest x2): `system` and `cleartype` are the same picture here (identical BMPs), because lfQuality 0 follows the host's
font smoothing and this host has ClearType on. `grey` (ANTIALIASED_QUALITY) is thinner and lighter than the game's own text.
Chosen: `system`, because it is what the game's own text uses (lfQuality 0, roadmap S5: the original's labels are ClearType on
the 16-bit DIB), the client image is shown 1:1 (SetDIBitsToDevice, no scaling after it), so ClearType's subpixel order holds, and it
keeps the weight and colour of the 800 x 600 text. On a host with smoothing off it gives what the game's text gives there.
`--text-aa cleartype|grey` force one.

## 2. Options (port/app/main.cpp)
- `--text-res native|frame` (default native). frame = P9B's picture (the 800 x 600 text scaled with the frame). SG_APP_P9C=off sets
  frame; port/test/conftest.py sets it for the app tests (they compare client images with the frame's pixels).
- `--text-aa system|cleartype|grey` (default system), `--text-unit-redraw` (measurement), `--selftest-p9c`, controls `--negctl
  p9c-unremoved | p9c-wrong-scale | p9c-wrong-colour` (also in the live window and live-trace shots), and gditext control 9
  (`--negctl reg:gditext.attach=9`: Font::textWidth measured with the font at 9 / 5 of its size, the width a window-resolution
  font would feed the simulation). Development: SG_P9C_RUN_LOG=1 (live-trace world shots write runs_<present>.tsv).
- session.json: live `p9c` {text_res, text_aa, runs_drawn, clipped_runs, clipped_px, failed}, its `session.p9c` {text_native,
  presents_text, runs}; live-trace `p9c` {captured, published_last, dropped, merged, no_font, frames, shot_runs, shot_clipped_runs,
  shot_clipped_px, shot_failed, shot_render_us}.

## 3. Evidence (final tree; P9C_gates.log, P9C_final.sh)
| gate | result |
|---|---|
| builds | x64 and Win32 rc 0, 0 errors, 0 warnings (P9C_build_x64.log, P9C_build_Win32.log) |
| segments x64, default options | 205 / 205 PASS, status and digest equal to log/roadmap/merges/I25/segs_x64.tsv on all 205 rows (P9C_segs_x64.tsv, P9C_segs_compare.log) |
| app selftests x64 and Win32 | --selftest 37/37, p5b 6/6, p7 9/9, p8b 11/11, p9a 5/5, p9b 10/10, p9c 10/10 on both (P9C_selftest*_*.log) |
| p9c controls (x64 = Win32) | p9c-unremoved 3 rows RED (both scale-1 redraw rows, the 2x direct-draw row: the low-resolution glyphs show beside the new ones); p9c-wrong-scale 2 RED (2x direct draw, ink height); p9c-wrong-colour 4 RED (both scale-1 rows, 2x direct draw, ink colour) |
| framegate, default options | sandbox_basic (10 rows) + cov_b_location (5): status, total / one-level / beyond px equal to P9B's run (= the I24 census) on all 15 (P9C_framegate.tsv, P9C_framegate_compare.log) |
| sim with capture and redraw on (live-trace, `fixed`, --wide --render-scale 1920x1080 with world shots) | trace.trace and framehash.bin byte-identical for --wide off / frame / native: sandbox_basic 439 records, course_season 6,296, championship 2,305 (P9C_simeq.tsv) |
| sim-side width control (gditext control 9: Font::textWidth from the font at 9/5 of its size) | the 203-object framehash does not see it in sandbox_basic or cov_r_quit (a text width reaches only UI-class layout and hit tests, which the segment gate does not hash), but the 800 x 600 frames every gate compares do: cov_r_quit with it presents 738 frames against 735, every one different from index 0 on; frame and native give the same 735 present hashes (P9C_ctl9.tsv) |
| 800 x 600, 96 dpi, scale 1 | 26 presents over 5 scenarios (title, location, game frames with HUD layers, labels, ticker, popups, the system menu, reports): native == frame in every pixel of all 26. The forced redraw at scale 1 (--text-unit-redraw: peel and draw again through the window-resolution path) == frame in 25 of 26; cov_r_s2_screens 606 differs in 25 px of 480,000, at most 25 per 8-bit channel: 22 on the golfer labels `Sid` / `Gary Golf` where they touch the golfers' sprites in the wide scene, 3 single pixels of the palette's tool labels (P9C_unit.tsv) |
| live app, `fixed`, Hawaii sandbox (P9C_drive.py), 1920 x 1080 and 1280 x 720, native and frame | each of the four sessions' recordings replays through sg_run_boot PASS (173..189 records); native draws 3,417 / 3,383 runs, 0 failed (P9C_drive_*.log, P9C_live/result_*.json) |
| app test files (conftest env, one file per process) | test_static 6, test_profiles 21, test_app_shell 18, test_app_input 16, test_app_replay 14, test_app_ingame 14, test_app_present 3 (4 deselected), test_app_live 13 passed (P9C_t_*.log) |

Wider than its scaled box (clipped, not reflowed; brief item 3): with the margin of a third of the font size, live-trace world shots
4 of 1,978 drawn runs (128 px, all at 1280 x 720 on the report screens of cov_r_s2_screens), 0 at 1920 x 1080; the live drives 10
of 3,383 runs (165 px) at 1280 x 720, 0 of 3,417 at 1920 x 1080 (P9C_clip.tsv). With the first margin (2 px) it was 44 of 3,391 at
1280 x 720 (the dots of an i above the box). Presenter::render per world shot is within noise of frame mode (1280 x 720 9.9..19.8 ms
against 10.8..19.6; 1920 x 1080 17.8..46.0 against 16.7..48.8; P9C_clip.tsv): the runs cost GDI calls on small DIBs.

Screenshots (P9C_shots/, index.tsv; `<tag>_<scenario>_<present>_before.png` = --text-res frame, `_after.png` = native, `_pair.png` =
before over after; 1920 x 1080 and 1280 x 720, --scale smooth, the P9C x64 build of the final tree):
| scenario, present | screen |
|---|---|
| sandbox_basic 4 | title menu |
| sandbox_basic 6 | location screen (loading quotes covered, the globe free of dots) |
| sandbox_basic 447, cov_b_career 75 / 150 / 1200 | in game: HUD title, meters, bottom band; the welcome message window over the meters (ticker / message); Gary's thought label `Click on the big 'Build Course' button.` and name label; a portrait message |
| cov_r_s2_screens 4 / 336 / 396 / 420 / 606 / 1317 | title menu; radial hover label (a tip); F2 financial report; a report page; the best-scores / system menu frame; the skill-point popup |
| cov_r_quit 248 | showPopup dialogs: the exit prompt over the System menu (shadow runs in red) |
| championship 690 | the shot panel's texts at the right edge |
Live (P9C_live/, `<shot>_<size>_text{frame,native}.png`): banner (the P8A loading banner `Loading...`), welcome, ingame, tip_top_7
(the `Tropical bush 1000` tool tip), tip_bottom_right, tip_top_right, hover_buildings. aa_compare_cov_b_career_75.png: section 1.4 (rows frame, native system, native cleartype, native grey; the cleartype / grey runs were made before the final margin change, which does not touch that crop).
800 x 600 equality images: P9C_runs/unit_<scenario>_{frame,native,redraw}/rec/world_shot_<present>.bmp.

LAN: at Mariano's request (this chat, 2026-11-04) the final x64 build was published to the mini-server as an uncommitted test
build: staged from port/build/P9C/port/build/x64/app/Release into port/build/P9C/port/build/lan_release_p9c with label
`SimGolf port c5e66b59 + P9C test build (text at window resolution, uncommitted)`, deploy_lan_release.sh sent 1 file
(simgolf_port.exe); no commit, no push, no GitHub release, nothing appended to log/roadmap/lan_releases.tsv (P9C_lan_publish.log).
release_due.py reads c5e66b59 from the label.

## 4. Text paths and what stays at 800-space
Redrawn: every draw_glyphs call (section 1.1): drawTextRun / drawRichText (markup and plain), draw_text_string, drawTextSurface's
shadow run, drawCenteredText / drawCenteredText2, drawStringAt / drawStringAt2 / drawStringRightAt, drawWindow / the ticker / message
windows, the HUD tips and tool tips, the bio EditBox (window target, flat path), the loading banner (its own run list: TextCapture's
banner sink, gfx_text_banner), the golfer labels of P8B's wide scene beside the frame (world runs).
Left at 800-space, by design or measured:
- a run under a later write (a dimmed frame drawn over the text, a panel, the next screen): the visible part is what the game shows,
  so it is kept as the frame's pixels; a run with less than a quarter of its glyph pixels visible is not redrawn at all;
- a text drawn with a FontRef outside the 15 startup fonts (`no_font`: 0 in every run of this work);
- a run of a HUD segment that made no layer (its pixels changed nothing in the root), and a window target's run on the layered path
  (the EditBox exists only in the character editor, a 2D screen without layers: the flat path redraws it);
- at an 800 x 600 content rectangle nothing is redrawn (section 1.3).

## 5. Left / limits
- FreeType: the app attaches only the GDI provider (gditext.attach), so the redraw is GDI's with the same LOGFONTA; a FreeType host
  would need TextRaster to draw through fttext_raster at the scaled ppem (its blend is GDI ClearType's, U-1520's x hinting is not).
- Hinting at the new size is the host's: Manual SSi Bold at 24 px (1280 x 720) merges the dot of an i into its stem (`Unlimited`
  in the money meter); at 36 px (1920 x 1080) the dots show. A string's width at the new size differs from the scaled 800-space one
  by a few pixels (hinting), so a centred or right-aligned string sits up to that much off its scaled place; the origin is the
  800-space one scaled, as the brief asks.
- The scale-1 redraw's 25 px on golfer labels touching sprites (section 3) mean the wide scene's label peel and the world under a
  label differ in those pixels from the frame's; at other sizes they are part of the label's antialiased fringe.
- Not changed: P9B's modal-report frames (cov_r_s2_screens 396: the radial menu and meters drawn twice) and the palette tool tip at
  its centred place while the palette moved left (P9C_live tip_top_7) are P9B's layer placement, the same with --text-res frame.
