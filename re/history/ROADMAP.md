# Roadmap sections S0-S11 and P3-P9 (2026-10-24 ..)

Moved verbatim from CLAUDE.md on 2026-11-04. Order as it was: S0, S2, S3, S4, S6, S1, S8, S5, S7, P3, then S10, S11, P4-P9, S9. Step notes live in log/roadmap/<section>/.

Roadmap S0, baseline and frame census (2026-10-24, re/roadmap/BASELINE.md, frame_census.tsv, frame_causes.tsv, notes
log/roadmap/S0/, U-1100..U-1102, U-1110..U-1115, U-1120 / U-1121). Every regression gate was measured on HEAD 4bfefc2a in a
git-archive export (port/build/S0_head, with junctions to the gitignored data), so the S1 / S4 / S6 working-tree edits were not in it.
Results: repo static 600 / 2 skipped, port suite x64 41 / 41 files, segments 205 / 205 on x64 and Win32 with 0 digests changed, boot
gate 58 PASS / 2 KNOWN / 1 NO_FRAMES on the 61 HEAD scenarios, tick 250, sve_roundtrip 15, gl_raster 139, terrain_state 500,
terrain_render 40, terrain_gl_setup 6, gfx 17, port static 6, terrain_live 144. The Win32 backend suite has no record after phase 3.R:
20 failures in 8 files are its baseline. Of these, 11 are x64-only helpers (U-1100) and 9 are browser / load / save digests and
files not yet diagnosed (U-1101). port/corpus is shared, so `boot --all` also runs uncommitted recordings: count the HEAD scenarios
only (U-1102). port/test/framegate.py runs every checkpoint of a scenario in one boot with gfx.attach and LiveTerrain attached. It
writes one row per checkpoint with: status PASS / UNSUPPORTED / FAIL / NOT_RUN; screen kind (title, location, ingame, frame); the
terrain-on-screen rule from the trace's clock sites (U-1110 / U-1111); px per region (in-game: hud_title, hud_meters, hud_message,
toolbar, bottom_panel, view); one-level and beyond-one-level px; the unsupported drawing calls before the snapshot. LiveTerrain at
an address of its own leaves every sim hash equal. The census over HEAD's 609 checkpoints (64 scenarios) gives PASS 45 (title menu,
location screen and one world map), UNSUPPORTED 525 and NOT_RUN 39. Of the 39, 34 have no trace (cov_a_quit, cov_a_quit_title,
golfer_events) and 5 got no snapshot. U-1120 covers those 5: with a gfx.snapshotAt pending, sg_run_boot returns -11 at the end of
cov_a_champ, championship and cov_k3_career. Terrain checkpoints are 522, and 520 of them rank under `terrain not drawn`. Terrain
pixels never reach the gfx back buffer at HEAD, because terrain_state's draw passes only set the view quadrant. The G2 criterion
(1.5 % one-level, 70 px beyond one level, glraster constants) is applied to the view region's area until S3 draws the terrain
(U-1121). 2D causes (the first unsupported drawing call): Widget_writePixel 316, Surface_blit3 with an unmodelled source or
destination 168, Image_draw with a SpriteRef the backend has no sprite for 39, Surface_drawScaled2 2. No non-PASS row lacks an
unsupported call.
Roadmap S2 wave 1, the gfx body families (2026-10-24, notes log/roadmap/S2/1_notes.md, 2_notes.md, steps re/roadmap/S2_steps.tsv,
screen map re/roadmap/S2_screens.tsv, U-1200, U-1210..U-1214). Drawing bodies of the gfx backend can now live in their own files:
`port/src/sim/gfx_<family>.cpp` includes port/src/sim/gfx_body.h (Image8 / Sprite8 / Surface16 / Gfx and the shared helpers, moved
out of gfx.cpp), defines a GfxBodyDef table and `kGfxFamily_<family>`, and is listed by one `SG_GFX_FAMILY(<family>)` line in
port/src/sim/gfx_families.inc, which gfx.cpp expands into explicit references (portsim is a static library, so a self-registering
object would be dropped) and one registry entry `gfx.family.<family>` carrying the family's negctl switch. New test entries
`gfx.call(name_va, a0..a10)` (one body without a trace) and `gfx.check()` (`$gfx/bodies.txt`, the count of names registered
twice); test-only family `selftest` (gfx_selftest.cpp). S0 recommendations 1, 3, 4: gfx.log lists every distinct unsupported reason
in first-occurrence order with no cap (`first	<k>	<reason>`), kNotDrawing gains playSound, Snd::status, capsLockOff,
emptyFunc_4a0890, saveCourseJpg, Widget_readPixel, enterModal483d30, and gfx.render's budget is the FRAME records up to the last
checkpoint + 2 with to_end past the last FRAME record (record_budget). Gates: test_gfx 25 / 25 x64 and Win32 (17 + 8; 6 of the 8
RED with the selftest line removed), segments 205 / 205 x64 + Win32 with 0 digests changed, boot 58 / 2 / 1 of HEAD's 61, framegate
sandbox_basic + cov_b_location + cov_r_quit equal to the census in status and px (first_unsupported now the first drawing call,
Widget_writePixel instead of Snd::status), port test_static 6 / 6 with the family-aware module listing. Controls 8 / 9 RED, gfx 3
blind on every PASS checkpoint (U-1200). Screen map (S2.2): of the non-PASS census rows, the pinned full-screen 2D checkpoints are
credits (cov_r_quit, 5), load browser (cov_a_keys k_dlg_L), routing map (k_dlg_R, cov_k1_routing routing) and the character editor
(cov_a_career career_editor, cov_a_golfer button_0, cov_a_keys3 q_editor); the census marks the last six terrain yes (U-1210). The
port's screen functions make no drawing calls (3.7b dropped them), so each screen family adds its calls. Twelve families have no
pinned checkpoint (difficulty, theme packs, pick a pro, hire, F1-F10 reports, end of year, preferences, scroll text / skill points,
shortcuts / roster, buy land, loading, radial menu). The 480 non-PASS H1WAIT rows are course view plus HUD (S3). claude2 workers have
no allow rule for a bare `cmake`, so a cmake call waits on an approval nobody sees: S2 workers build through
log/roadmap/S2/build.py (`~/bin/py`, VS Build Tools' cmake).
Roadmap S2.3, the credits screen (2026-10-24, notes log/roadmap/S2/3_notes.md, U-1220..U-1226): cov_r_quit's five credits
checkpoints (quit_00..03, credits: the scroll at five positions) equal the original's back buffer in every pixel, x64 and Win32.
Three body families: `surface` (gfx_surface.cpp: Image_ctor / dtor / releaseAll, Surface_ctor / free / createFull erase the key's
backend entries; Surface_blitTakeDirty = Surface_blit; Surface_drawRegion = jgld Sprite::draw -> draw16_1001cde0, which draws
0xfe and skips only 0xff, at the trims; Surface_blitThunk = Surface_blit3 with one shared size), `prim_text` (gfx_prim_text.cpp:
Widget_setQuad's colour words per target in the new per-family Gfx::state_of, drawText's min(strlen, limit) characters in the
main colour, no shadow) and `credits` (gfx_credits.cpp: the logo at its own size, the kept lines read by the backend, one line per
call, the logo centred by its kept width). loadImage's flag bit 0 decodes with the file's palette. The port's show_credits_screen
makes the original's drawing calls (credits_gx, local keys). TraceInput::clock advances the gfx snapshots past a served clock read
(ordinal + 1): the credits scroll reads the clock once per pass and dispatches nothing, so before it every snapshot came from the
end of the input. Gates: census 609 rows, 604 unchanged, the 5 credits rows UNSUPPORTED -> PASS (PASS 45 -> 50); segments
205 / 205 x64 + Win32, 0 digests changed; boot equal to S2.1 in all 67 rows (HEAD's 61: 58 / 2 / 1); test_gfx_credits 47 x64 and
Win32, test_gfx 25, port test_static 6, test_commands 83 / 44 skipped, test_tick 250. Controls: 4 RED on the frames (prim_text 1,
2, credits 2, tick.frame 11), every other mode RED in a unit test (equivalent on the five frames, U-1223); gfx.attach 3 stays blind
(the background has no byte 0xfe, U-1200).
Roadmap S2.4, the routing map and the load browser (2026-10-24, notes log/roadmap/S2/4_notes.md, U-1230..U-1234): cov_k1_routing
`routing` and cov_a_keys `k_dlg_L` equal the original's back buffer in every pixel, x64 and Win32; cov_a_keys `k_dlg_R` (two holes
built) differs in 370 px, all in the map's hole overlay: the tee / flag sprites (FLIC sheet cells through the sprite queue's
scaled draw, S3.4 / S3.5) and the OpenGL tee-to-green lines (Terrain::drawLine, S3.8), reported as drops without a body. Three
body families: `prim_fill` (gfx_prim_fill.cpp: fillWindowRect / Win_fillRect = jgld fillRect16 with a direct colour, blitPanel
style 0 / nine-cell over Sprite::drawG, Display::setScale as state, fillConvexPoly16 with its edge walker and span filler),
`prim_text2` (drawCenteredText2: a shadow run in run-palette colour 1 at (+0, +1), entry 1 measured as RGB (0x80, 0, 0), U-1230)
and `routing` (gfx_routing.cpp: loadInfoScreenArt's whole art manifest, now drawn at boot; the map's tile diamond). Image_draw
reports a display scale other than (1, 1, 1). routing.cpp's MapDraw and saveload.cpp's browser issue the original's drawing
calls; a text the original builds in g_textBuffer is built there, drawn and the buffer put back, so the GameState with a
backend equals the one without. Gates: census 609 rows against S2.3, PASS 48 -> 50 (raw), 12 already-unsupported rows grew by 344 .. 1,012 px from
stale root content and partly drawn character-editor frames (first unsupported call unchanged); segments 205 / 205 x64 + Win32, 0 digests changed; boot
equal to S2.3; test_gfx_routing 35 x64 and Win32, test_gfx 25, test_gfx_credits 47, port test_static 6. Controls: 12 modes RED
(10 on the frames, 2 only in unit tests), 0 blind.
Roadmap S2.5 / S2.6, scenarios for the 2D screens (2026-10-24, notes log/roadmap/S2/5_notes.md, 6_notes.md, U-1250..U-1253).
S2.5 recorded the missing traces of cov_a_quit and cov_a_quit_title (the p24-era runs ended at the exit with a 147-byte .trace.json;
trace_rec.js's ExitProcess hook now ends them; no re-pin): every one of their 24 checkpoints now runs, the port's boot gives cov_a_quit
PASS 301 / 301 and cov_a_quit_title NO_FRAMES (it never leaves the title menu and credits), and with the S2.4 backend framegate reads
19 PASS (every terrain-free row) and 5 in-game rows UNSUPPORTED. S2.6 pinned cov_r_s2_screens (re/tools/scenario_s2_6.py, driven by
re/tools/s2_6_runs.py, which imports it because scenario.py's hook then globbed only scenario_s1_*.py): 32 checkpoints inside the 2D
screens no earlier checkpoint showed (loading at T 100 / 200 / 300, theme packs, difficulty, championship course list, pick a pro,
radial hover label, reports F2 / F3 / F4 / F7 / F8 / F9, the F10 board, hole stats, best scores, the system menu alone, preferences,
hire, end of year, skill points and their popup, the land popup, buy land), det_a == det_b == det_c on instances\s2_6, the replay
from the trace SAME at 31 of 32 (theme_packs differs in the frame only, 2,170 px, U-1252). The scenario writes g_date and
g_eventFlags567a1c to reach end of year and buy land. The port's boot equals the capture up to record 1156 except the cursor tile
at record 771 (U-1253, cause not measured), then stops at the unported showBuyLandScreen (U-1251). Scroll text has no checkpoint: the
character editor's load control stops the lockstep clock inside runScrollText (U-1250).
Roadmap S2.7, the boot screens (2026-10-24, notes log/roadmap/S2/7_notes.md, U-1260..U-1263): cov_r_s2_screens loading_t200,
loading_t300, theme_packs, difficulty, champ_courses and pick_a_pro equal the original's back buffer in every pixel. Family
`boot_screens` (gfx_boot_screens.cpp): showLoadingScreen.draw (picture r and hint r2 from the screen's two Random::range draws,
drawMessageBuffer with drawTextSurface's shadow, the bar), selectDifficultyScreen / showThemePackSelect setup, redraw, hover, tip,
teardown, pickAProScreen setup / textSurface / teardown; the screens' port functions issue them, Setup now draws the title art
under the boot's loading screens, and load_flic issues the refreshFull and consumes waitTicks(10, 0)'s clock reads that follow
each boot loading screen (a checkpoint inside that wait shows it). U-1252 settled: showThemePackSelect draws a sixth column cell at
an uninitialised stack word w (U-1260); the pinned runs have w = 0x1a, the trace recorder's runs a w that hides it; the backend
uses 0x1a (SG_GFX_THEMEPACK_COL5 overrides). Controls 13 modes RED (11 on the frames, 2 in unit tests).
Roadmap S2.8, the report screens over the course (2026-10-24, notes log/roadmap/S2/8_notes.md, U-1270..U-1278): on cov_r_s2_screens
rep_comments, rep_histograph, rep_financial, rep_shortcuts, rep_roster, hole_stats, best_scores and end_of_year equal the
original's back buffer in every judged pixel, and hire differs only in its four animated employees (addSprite FLIC cells, S3,
1,397 px pinned). JUDGED PIXELS: every report dims the whole in-game frame (black.pcx through Sprite::drawAlpha's quarter band =
draw16_1002ed60's 555 arithmetic, U-1271) and lies over the course the backend does not draw yet, so test_gfx_reports.py boots
three more times with the root filled with 0x0000 / 0x7fff / 0x2a55 at the screen's first call (`reports.begin`, negctl modes
101..103) and judges only the pixels that do not change with that underlay; their counts are pinned. Family `reports`
(gfx_reports.cpp): Surface_drawSprite, Widget_draw / Widget_draw2 / drawStringAt / drawStringAt2 (font swap, left / centred /
right / centred in a width), Widget_fillRect (jgld line16 with its octant walk and clip), Surface_box / Surface::callBounds (the
clip), reports.begin; gfx_reports.h ScreenDraw gives the screens gx calls, texts built in g_textBuffer under save / restore and the
art's cut sizes (Sprite8::cut_w / cut_h = SpriteRef +0x18 / +0x1c). framegate now judges the checkpoints before a boot that stops
on a missing name (U-1270). rep_report1 and preferences are S2.9's functions (U-1276). Gates: segments 205 / 205 x64 + Win32,
0 digests changed; boot equal to S2.4 on its 67 rows; goldens of showEndOfYearScreen / showPlayerCommentsReport re-pinned through
the oracle; controls 17 modes, 14 RED on the frames, 3 RED in unit tests.
Roadmap S2.9, dialogs over the course (2026-10-24, notes log/roadmap/S2/9_notes.md, U-1280..U-1283): showPopup (every popup and
the System menu, which is a showPopup), textInputBox with saveGameDialog's panel, and showCourseReport's association page are
drawn by the backend. The course under a dialog is S3's, so each dialog is judged in its own rectangle, measured on the
reference frame (the region of the fill colour 0x4e79 grown by 5 px; the page's panel colour), on the pixels the dialogs decide:
family `dialogs`' probe (negctl modes >= 0x10000) fills the root surface when a dialog stack starts, and a pixel is judged when
three probe backgrounds (black, white, a pseudo-random pattern) and the plain run agree. All 14 rectangles of the 10 pinned
checkpoints (cov_r_quit exit_prompt / exit_save, cov_a_keys k_dlg_S / k_dlg_J / k_C, cov_a_keys2 C_prompt, cov_a_champ /
cov_a_play n_prompt, cov_a_build2 remove_prompt1, cov_k3_career prompt / prompt2) have 0 failing pixels, x64 and Win32; held:
9..32 px per popup (blended border corners), 28,579 on the page (its drawAlpha pass and transparent parts). Family `dialogs`
(gfx_dialogs.cpp, 8 bodies): the dialog art loadInterfaceArt cuts (popup border and masks, option bullets, the icon row),
drawPanelS2, drawTextSurface, (Widget_fillRect, Surface_drawSprite, Widget_draw, drawStringAt, drawStringAt2: the reports family's bodies after S2.13), showPopup's
odd-y box (the colour read from the root under it), dialogs.begin / dialogs.end. The screens' calls are in commands_popup.inc,
saveload.cpp and reports.cpp (texts built in g_textBuffer, which is put back with g_textBuf). Gates: census 609 rows against
S2.4, same 427 / better 146 / worse 36 (5 = cov_a_quit rows S2.5 made runnable, 31 = dialog pixels left in unsupported in-game
frames, first unsupported call unchanged); segments 205 / 205 x64 + Win32, 0 digests changed; boot equal to S2.4. Controls: 11
modes RED (8 on the frames, 4 in unit tests), 0 blind.
Roadmap S2.13, integration of S2.7-S2.9 (2026-10-25, notes log/roadmap/S2/13_notes.md, U-1205..U-1207): the three steps' trees
merged on HEAD 2c05f02a + waves 1-4 by 3-way merges (log/roadmap/S2/13_tools/make_mirror.py), one patch 13_shared.patch. U-1278:
the five body names both reports and dialogs registered (Surface_drawSprite, Widget_fillRect, Widget_draw, drawStringAt,
drawStringAt2) are the reports family's only (gfx.check 0 duplicates): jgld Sprite::drawAlpha's flag byte starts 1 and is cleared
only when the destination format's +4 is 1, and with it set the quarter band is draw16_1002ed60 (RGB555 masks), the member S2.8
measured on the references; the dialogs' controls moved with the bodies (reports modes 9, 10, 20, 21 and 1). U-1254 fixed: the
end-of-year and course-report modal loops make the original's per-pass View_toLocal cursor read (reports.cpp report_loop; after
the pump on the end-of-year screen, before it in the course report), so cov_r_s2_screens' boot equals the capture through record
1156 and stops only at the unported showBuyLandScreen (U-1251); control showEndOfYearScreen mode 6 brings back the record-771
cursor-tile divergence. Gates: test_gfx 25, credits 47, routing 35, boot_screens 32, reports 36, dialogs 32, port static 6,
commands 83 / 44 skipped, tick 250, reports 14 (16 with SG_EMU_ORACLE=1), s2_5 3, s2_6 4 (x64); segments 205 / 205 x64 + Win32,
0 digests changed; Win32 gfx files and s2_6 pass; boot 70 rows 65 PASS / 2 KNOWN / 2 NO_FRAMES / 1 FAIL (cov_r_s2_screens at 1156).
Roadmap S2.11, dialogs over the course 2 (2026-10-25, notes log/roadmap/S2/11_notes.md, U-1215..U-1219): the TOURNAMENT RESULTS strip
(showTournamentResults' scorecard layout), the bulletin board (showBulletinBoard's trophy shelf) and the SimFoto / message ball are drawn
by three families, gfx_results.cpp (drawStringRightAt, results.begin), gfx_simfoto.cpp (loadInterfaceArt.ballArt, drawWindow.golfer,
simfoto.begin) and gfx_bulletin.cpp (the board's art cuts of loadInterfaceArt kIa11..kIa16, the board image, events, photo pins, the
fixed sprites). Findings that shaped it: the results rows and the close button read the bands' SpriteRef +0x18 / +0x1c, which only the
backend holds (tick_panels.inc reads gfx_sprite_cut with a backend: drawing positions only, the y break never fires before the rank
break); the port's tickMessage (commands_layout.inc, registered) never handed drawWindow to the backend; every pinned SimFoto sits in
narrateShot's pumpAndRedraw(0), so its frame is narrateShot's own, which the port's shots.cpp now draws with a backend, running
drawWindow's window-height pass and putting g_textBuffer / g_textBuf / g_ui_821c50 / g_value587ea0 back (commands.drawWindowGfx). The
ball's head (Surface_drawScaled of a heads cut, S3.6; palette by composeGolferSprite, S3.5) is not drawn: the probe holds the ball's
0x8c square (U-1216). Gate (test_gfx_dialogs2.py, rectangles measured on the reference frames by log/roadmap/S2/11_tools/rects.py, held
pixels by the families' probes): all 17 rectangles of 13 checkpoints 0 failing pixels, x64 and Win32 (results strips 101,719 judged px
each; the bulletin board all 480,000); championship tournament_end has no snapshot (U-1120). Segments 205 / 205 x64 + Win32, 0 digests
changed; boot equal to S2.13. Controls: 10 RED, 0 blind.
Roadmap S3 wave 1, the terrain draw-list passes and the rasteriser's last two rules (2026-10-24, steps S3.1 / S3.1b, notes
log/roadmap/S3/1_notes.md, 1b_notes.md, U-1300..U-1304, U-1390..U-1392):
port/src/sim/terrain_render.cpp now ports localRender (25 window tiles, no culling of its own; drawTileObjects' camera-facing wall
neighbours, recursive and culled against pTile), stripRender (odd directions are two complete nested passes, 8 + 2, 2 + 4, 4 + 6,
8 + 6, each with its own push / list / flush) and pathUpdateRender (no setViewAngle: it keeps the quadrant the previous pass left;
left edge clipped to x + 1, U-1302), with drawTile's sorted insert (its `other angle` branch test never holds, so the secondary walk
always runs) and the draw list empty at every pass entry (137 snapshots); terrain_render_entry serves sg_gl_render(_to_raster)'s
entry kinds 1 strip, 2 render, 3 local, 4 pathUpdate. Recorder: re/tools/gl_record.py --scenario runs a pinned scenario through
scenario_refs.py with gl_rec.js from process start and --inject queues draw passes on the game thread (Frida runs NO hook for a
call made inside an Interceptor.attach callback, so the injection is an Interceptor.replace of render / localRender; state the
module observer's tryInstall reads must be declared before it). Recordings g2 .. g6 (instances\s3_1, cov_b_holes18 / cov_b_paths,
every run SAME as its reference; port/corpus/gl/<label> via `port.test.glcorpus export`). Evidence: 237 draw-list passes equal the
original line for line (33 localRender, 160 stripRender, 44 pathUpdateRender; zoom 0 / 1 / 2, four angles, directions 1..8);
GlRaster from the port's stream meets G2 on 209 / 209 passes outside g6 (one-level 0.08 .. 1.25 % of drawn, at most 24 beyond);
g6 (60 % type 7, border column not hidden) missed on 16 / 28 with the original's own stream too (U-1300), which S3.1b traced to two
rules of Windows' generic GL that GlRaster lacked (found with glreplay.exe, which replays g6 bit-exact): the specular term is a
256-entry table over [0.0005^(1/n), 1] read at the nearest entry, not the exact pow (gl_raster.cpp spec_lookup; 0 of 4,923 probe
quads wrong; tune 27 = 1 restores the pow, U-1390), and a pixel centre exactly on a horizontal edge belongs to the edge pointing to
+x (tune 10 = 2 restores the old rule, U-1391). Now every pass meets G2 (g6 28 / 28) and one-level pixels fell everywhere: the 209
other draw-list passes 0.46 -> 0.03 % mean, c7 + t3 0.47 -> 0.06 %; the residual 0 .. 0.21 % is not split further (U-1392);
controls 9 .. 14 RED on the streams (14 redraws a tile over itself: blind in pixels). Unjudged: passes that draw no tile leave no
snapshot (U-1301), NULL tile / pivot paths (U-1303), vacuous G2 on 4 localRender passes (U-1304). The renderer is still reached only
through sg_gl_*; LiveTerrain's draw passes into the back buffer are step S3.2. U-1120 resolved (S3.3): a boot run with to_end
ended in its last frame's Blocking pump with no input left, NeedInput (a MissingResource) surfaced as -11 and pending snapshots
were never taken; run_frames now takes them there (cov_a_champ, championship, cov_k3_career: 22 census rows run).
Roadmap S3 wave 2, FLIC sheet cells (2026-10-24, steps S3.4 / S3.4b, notes log/roadmap/S3/4_notes.md, 4b_notes.md, U-1330..U-1339): module
flic (port/src/sim/flic.cpp, portsim/sim/flic.h) decodes the user's .flc files (GameFiles) into loadFlic's sheet cells: cropSprite result,
SpriteRef geometry +0x10 .. +0x28, jgld Sprite words, pixel bytes, column and sequence counts, slot and shadow palettes; menu.cpp's
load_flic hands them to an attached gfx backend through flic::sheet_to_gfx (nothing written into the GameState). Oracle: the original's
sheet memory after every loadFlic (re/tools/flic_cells_rec.py + re/frida/js/flic_cells.js, pinned scenarios, hooks off): 443 / 443
entries of menu_flics.inc's tables equal for Parkland, Links, Desert and Tropical, 1,698 distinct loads (148,926 cells, boot loads
included) equal on x64 and Win32; controls (delta chunk skipped, palette chunk ignored, frames reversed, 0xfe drawn: thin, U-1334) RED.
FINDING IN THE STOCK GAME (U-1330, settles U-1701): the exe names `trees\Tropic\Tree Cerc` / `Tree Drac` (space; v1.00 too) while the
install, the v1.03 patch and the cab hold Tree_Cerc / Tree_Drac, so the original dies at loadWorldAssets' first tropical tree load
(flicOpenStream returns 0, then Image_draw reads 0x8 at 0x00473e67) on every tropical course of the shipped install
(log/roadmap/S3/4_rec_hawaii_stock.log, instances\s3_4); copies under the exe's names (instance only) make all 114 tropical loads
complete. Decision D5 option a (S3.4b): defect switch flic.treeAliasNames, default 1 = Fault{8, 4} at that load as the original, 0
(profile fixed) = flic::resolve_name returns the underscore names; it acts in load_flic's column path with or without a backend,
since the columns decide later cell allocations. Open: U-1332 (the palette a sheet cell is drawn with: sprite-queue step S3.5), U-1333 /
U-1334 / U-1335 (evidence limits), U-1338 (other missing main files still run on with status 1; no shipped load reaches one).
Re-merged with S3 waves 4 / 5 (roadmap S3.11): the defect row is D53 of re/port/defect_decisions.tsv (75 rows, 13 fix / 62
keep, its input log/roadmap/S6/s3_4b_decisions.tsv) and profile fixed has 9 switches. test_menu's emulated original faults like the stock game (Image_draw on loadFlic's NULL stream reads 0x8): its stock tropical cases are
pinned against that Fault at flic.treeAliasNames = 1 and `_alias` cases (the spaced copies) run the port at 0; test_terrain_live runs
S3.2's hawaii recording (made with the copies) at 0 and checks the Fault at 1 (roadmap S3.4c).
Roadmap S3 wave 3, the terrain in the frame (2026-10-24/25, step S3.2, notes log/roadmap/S3/2_notes.md, U-1310..): the
original's GL draws into the ROOT surface (initTerrain gives Terrain::initSystem the DC of the jgld surface at
[g_rootWindow + 0x278]; that DIB's memory row 0 is the picture's top row, so GlRaster's buffer and gfx.root are the same
array, no flip). updateTerrainRender clears the root through Window::paint(g_rootWindow, 0x80000000) (clear16 with colour 0),
the passes draw over it, Surface_blitThunk copies root -> the exe's back surface g_image51b068, an unmoved camera copies the
back surface whole onto the root and local-renders changed tiles, and renderTerrainStrip draws the strip and copies the shifted
old picture back. Body family `terrain` (port/src/sim/gfx_terrain.cpp: terrain.windowPaint, terrain.pass, terrain.blit) runs
LiveTerrain's passes through S3.1's terrain_render_entry into a GlRaster over gfx.root; the GL setup is the port's own
(terrain_gl_setup). gfx.log carries a per-snapshot terrain flag (snapshot_terrain) and framegate a `terrain_drawn` column;
LiveTerrain attaches on Win32 too (runner32 / lib32 ops). EVIDENCE: the original's root surface recorded at the return of every
updateTerrainRender (re/tools/terrain_calls.py --pass-frames / --frames-lite, instance s3_2, every hooked run SAME as its
reference): per census checkpoint the port's root at the last pass before the checkpoint meets G2 over the pixels the original
drew on all 527 terrain checkpoints (log/roadmap/S3/2_terrain_verdicts.tsv; S0's `terrain not drawn` 520 -> 0), four themes,
x64 = Win32 terrain pixels; controls (wrong zoom, wrong theme texture set, pass skipped, strip shift, Window::paint not clearing)
RED. Segments 205 / 205 and boot 58 / 2 / 1 unchanged (the backend writes no GameState byte). Frida spawns hung 5 times on the
instance (U-0683 pattern): kill the own PID and retry. Open: U-1310 (rasteriser fixed at 800 x 600), U-1311 (the back surface's
depth inferred from results), U-1312 (pathUpdateRender never runs organically), U-1313 (one organic strip direction framed),
U-1315 (two title rows keep the terrain flag at 1).
Roadmap S3.6, HUD panels and the in-game Surface_blit3 (2026-10-25, notes log/roadmap/S3/6_notes.md, U-1350..U-1359): two
graphics-backend families. `pixel` (gfx_pixel.cpp): Widget_writePixel (the low word, nothing outside the surface) and
Widget_fillRect (jgld line16: both ends drawn, a zero-length run draws nothing); the census rows after an in-game frame
reported Widget_writePixel only because a run's unsupported list is cumulative. `hud` (gfx_hud.cpp): Surface_blit3's 16-bit
sources, family surfaces and nearest-pixel magnification; loadInterfaceArt(1)'s whole image half (48 generated tables from
c3fv.cpp, `log/roadmap/S3/6_gen_iart.py --check`) and loadWorldAssets' build-palette cuts, checked against the original's
SpriteRef records in segment start images (>1,000 per scenario, 0 differ); Surface_blit2 / drawScaled2 (Sprite::drawG with
g_halfBrightLut), Surface_drawScaled under a display scale (jgld draw16_1001bc40: float scale, 16.16 steps, the x-mirrored
body), drawFrameR3 / S2, drawTerrainToolbar, the radial menu, showInfoCursorPanel, drawAmenitiesPanel, drawGolferActionPanel and
drawMatchScoreboard (names and numbers from the registered buildGolferName / appendNumber over a saved and restored GameState).
The golfer list and the employee detail panel (drawEmployeeViewPanel / drawEmployeeDetailPanel, S3.6b) are drawn from
c3fb.cpp / c3fc.cpp over the state commands_layout.inc hands over before its own writes (the detail panel's scroll step and
this frame's list are recomputed); names come through g_textBuffer with the buffer put back. No pinned frame holds the golfer
list or a selected employee, so their evidence is an EMULATOR CALL ORACLE (test_gfx_hud.py, SG_EMU_ORACLE=1): the original
functions under Unicorn with every drawing callee a recorder give the same calls (callee, image, position, colour, text) as
the port's `gfx_hud.recordPanels` log on 240 varied segment start images per panel; the default suite pins the port's log
digests; controls 11..15 RED. The detail panel's employee figures (addSprite) and its circle (drawCircle) go to S3.5 / S3.8. GATE MEASURE: framegate --no-live (no terrain; S3.2's terrain is judged under G2) and no scene sprites, so `log/roadmap/S3/6_hudgate.py` runs framegate twice with the test-only root fill
SG_GFX_HUD_BGFILL=0x0000 / 0x7fff after every present; a pixel equal in both was drawn opaquely and is compared with the
reference per HUD region. Hover tips are reported as `tooltip:` (S3.7 / S3.8) and the sprite queue's draws as `addSprite:`
(S3.5).
Roadmap S3.5, scene sprites (2026-10-25, log/roadmap/S3/5_notes.md, 5_merge.md, U-1340..U-1348): body family `scene`
(port/src/sim/gfx_scene.cpp / portsim/sim/gfx_scene.h, tests port/test/test_gfx_scene.py) draws the sprite queue
(drawQueuedSprite.draw / .label from tick_render.inc, renderSpriteQueue.end), Sprite_draw, Surface_drawSprite and the scaled
Image_draw path (gfx.cpp hands them over through gfxs::image_draw), plus the palette objects loadWorldAssets loads
(`loadWorldAssets.palettes`) and composeGolferSprite's palette layers. The jgld bodies behind them: Sprite vtable 0x1011d380 slots
+0x6c drawG, +0x78 blt16a (plane blit through build16BitColorTable's five blocks), +0x4c blt16c, +0x94 drawAlpha (15 bodies by alpha
window, programs generated from re/match/jgld_blit16.cpp), +0x44 drawB, all on one 16.16 scaling skeleton under the display scale
(drawQueuedSprite sets (s, s, d) and restores (zoom, zoom, 4); P6 sets (1, 1, 1)). U-1332 resolved: blt16a / blt16c store the
key's palette word in the Sprite and never restore it, so a sheet cell draws with its queue entry's g_images palette. Evidence: the
ORIGINAL jgld bodies in Unicorn on constructed sprites (40 cases, x64 = Win32), sandbox_basic's three checkpoints exact on more than
50,000 scene px each (one-level only where a blend meets the terrain, U-1347), controls 5 of 6 RED (5, palette-word persistence, is
blind on the pinned scenes). Cells drawn with no backend sprite are cells whose cut kept nothing; the original's records agree
(section 6). buildSceneSprites' alpha now reaches the backend as float bits (tick_scene.inc). The exact sprite-pixel oracle for every
checkpoint, the original's root at renderSpriteQueue's return (5_tools/scene_rec.py), needs a game run and has not been recorded.
Round 2: the exact oracle ran (5_tools/scene_rec.py on instance s3_5, hooks off: the original's root at
every renderSpriteQueue return, 66 scenarios SAME as det_b, 585 copies in port/corpus/scene/): 550 of 582 checkpoints equal in every scene
pixel up to one level, the rest overlay lines the port does not draw and one missing standing figure (U-1349). drawAlpha on the root
draws the flag-set member of each window's pair (555 masks; round 1 had the 565 members); Surface_drawSprite has one body, gfx_reports.cpp's,
which hands gfxs::surface_draw_sprite every case outside its quarter band; bodies composeGolferSprite and addSprite and Image_draw's mask
palette were added for S2.10; the hole flag and the amenities in gfx_hud.cpp draw through gfxs::add_sprite.
Roadmap S3.5b, S3.5 rebased (2026-10-25, log/roadmap/S3/5b_notes.md, 5b_merge.md, U-1385..U-1387): S3.5's scene family on the
committed tree plus S3 waves 2-4 (5b_mirror.py), the exact queue oracle re-measured on all 585 recorded checkpoints
(`5_sprite_gate.py --queue --all`, port/corpus/scene): 584 have a port copy (cov_r_s2_screens end stops the port's boot), 47,950,391
scene px judged, 29 checkpoints with pixels beyond one level (was 32): the five cov_r_bio rows that were unjudged in the S3.5 base
now run and hold only the golfer-selection ring (S3.8's), the others are rings, path lines and isolated pixels under blends. The
standing figure the port missed (cov_k1_animals bought / menu2, cov_b_inject end) is placeAnimal's queue entry
(`enqueueSprite(sx, sy, sy, 0x1e, 0x28, cell, 4, (n + 0x9c) | 0x100)`, an animal record drawn with the golfer palette path),
dropped before and now queued for an attached backend (golfers_world.inc): 187 / 216 / 92 px beyond one level -> 0, control 13
(an entry past the golfer table draws nothing) RED; test_standing_figure_equals_the_original / _negctl_red in test_gfx_scene.py.
Roadmap S3.7, the marked-up HUD text, the ticker and the hover tips (2026-10-25, notes log/roadmap/S3/7_notes.md, U-1360..U-1368):
body family `text` (port/src/sim/gfx_text.cpp). The root text object (the Image at g_rootWindow + 0x274) has markup ON from
Image_ctor (f_cfg_4e4210 = 1), so every frame text is drawRichText's: the family ports the scanners, drawTextRun (style fonts and
colours, baseline, shadow run, tab stops of 30, '_', the run palette's entry 1) and drawMarkupRun / Text_put, and the core's
draw_text_string hands it every text with a markup character, a tab or a palette colour. drawWindow is the family's whole body
(golfer portrait, picture windows, the slide-in, drawFrameR3 / S2 / drawWindowFrame, the centring) with drawMessageBuffer; the
TICKER had never reached the backend (commands.cpp draw_window_effect handed nothing over) and now does. The HUD panels' hover tips
(showInfoCursorPanel's tool tip, transcribed from the instructions, with its theme text, and slot tip; drawTerrainToolbar's,
drawGolferActionPanel's, the employee panels', drawAmenitiesPanel's tip and building row) are drawn through S3.6's gfx_hud.cpp;
their drawMapLine bars are S3.8's. Text_put became a measuring drop (gfx_take_value: with a backend the frame code gets the width
that places the thought labels' bars). EVIDENCE: an emulator oracle down to a fake jgld surface (setFont / colours / draws / end)
over the ORIGINAL text functions (391 random marked-up cases), drawWindow (300) and the six panels' tips (60 per kind), all equal;
goldens in the default suite; the opaque two-fill frame measure over every pinned scenario: no in-game checkpoint keeps a bad
pixel the family drew (the 9 rows with any are other steps' content over or under it). The image cursor exists only over an EditBox
and the widget tooltips only through Tooltip::show: neither is on a pinned frame (U-1360, settles U-1069's cursor / tooltip parts);
drawWindow's and the tips' texts never take drawTextSurface's shadow (g_font51b360 H 15, g_fontRef_519fd8 H 10), U-1072 resolved.
Controls: 10 RED on the oracles, 5 on frames.
Roadmap S3.8, overlay lines (2026-10-25, notes log/roadmap/S3/8_notes.md, U-1370..U-1377): Terrain::drawLine is the only overlay
primitive the exe reaches (Terrain::drawCircle, both splines and renderTile have no caller), and Terrain::initGL leaves
GL_LINE_SMOOTH on with GL_NICEST, so the lines are antialiased. Measured with glreplay.exe on synthetic drawLine sequences: coverage
at a pixel centre = clamp(w / 2 + 0.5 - |n|) * clamp(t + 0.5) * clamp(L - t + 0.5), the segment clipped to the viewport then its
ends snapped by U-1076's fixed-point setup, width clamped to 10, a zero-length line draws nothing, alpha * coverage blended and
dithered as the triangles (probes: 108 of 74,466 changed pixels differ; footprint-edge pixels under 0.13 coverage, U-1370). Module
`terrain_overlay` (overlay.rasterLine, overlay.logStart / logWrite) and the bodies drawMapLine, drawMinimapSegment, setLinePoint,
lineTo, drawCircle in the terrain family (the line point in the overlay's state; the GL context, flip byte and screen size from the
Terrain object at gTerrain; no context = nothing drawn); drawPanelBackdrop's bar and the routing map's hole line call them. The
port's drawLine calls equal the recorded GL streams in count, width, colour and alpha (cov_b_holes18 3,362; cov_b_paths 3,221 + 13
tooltip bars S3.7 draws); in the census the line pixels are 109,292 of 117,446 equal to the original, judged ones 0.36 % one level.
Roadmap S3.11, the S3 resubmission (2026-10-25, notes log/roadmap/S3/11_notes.md, merge log/roadmap/S3/MERGE_w5.md): S3 waves 2
(S3.4 / S3.4b / S3.4c), 4 (S3.6 / S3.6b) and 5 (S3.5 / S3.5b) and S3.7 / S3.8 assembled as ONE tree on HEAD bbc9072f by
log/roadmap/S3/11_assemble.py (every edit from the steps' own edit lists, each anchor exactly once, `--root <dir>` applies them to
another tree). One interaction no step's gate ran: S3.5's resolution of U-1332 (a sheet cell draws with its queue entry's palette
object) failed S3.4's test_flic hook-up test, which drew a cell with no mask; the test now passes the slot palette as Image_draw's mask
through the TEST-ONLY body gfx_scene.setPalette (11_edits.py). Gates on the tree: port suite x64 68 / 69 files (test_boot's known
cov_r_s2_screens stop), Win32 S3 files 25 / 26 (test_tick's baseline failure), segments 205 / 205 x64 + Win32 with 0 digests changed,
boot rows unchanged on both, gfx.check 0 duplicates, census 698 rows PASS 91 (was 87, none lost, no row with more differing px),
S3.2 terrain verdict 527 / 527 unchanged.
Roadmap S3.13, the I9 refusal fixed (2026-10-27, notes log/roadmap/S3/13_notes.md, merge log/roadmap/S3/MERGE_w5b.md): integrator I9
refused S3.11's resubmission on 78ffb06d because S3.7's text family and S2 waves 6 / 7 drew the same things in three places no gate had
run together. One body per name now. drawStringRightAt is the text family's (S3.7: drawRichText at x - drawMarkupRun, the original's
pair); the `results` twin and its control are gone. drawWindow.frame is the text family's whole drawWindow, and it calls S2.11's
held-pixel probe (gfx_simfoto.cpp's exported window_probe / head_probe) for sel >= 0 where S2.11's golfer body ran it, so
`drawWindow.golfer` is gone; simfoto controls 2..4 and results 1 are text 12..15 (simfoto 4 had read BLIND in I9, text 14 reads RED).
Two duplicate draws caused I9's dialogs2 and text-frame failures and the skill dialog's 8 px: since S3.7 draw_window_effect draws the
window, so S2.11's second draw_window_gfx in tickMessage drew the ticker twice, and S2.10's drawWindow.frame drop after
showCharacterIntro's draw_window_effect drew the intro window again at the ticker's default (0xf0, -8); showCharacterIntro now passes
its own position (0xc8, 0x172). integrator.py's switch-evidence map gained flic.treeAliasNames (test_defects_flic.py). The edits are
log/roadmap/S3/13_edits.py (36 anchored edits in 9 files, generated by 13_make_edits.py from the fixed mirror), applied after
11_assemble.py's steps by 13_assemble.py. Gates on port/build/S3_13: gfx.check 0 duplicates / 131 bodies, static 614 / 2, port suite
x64 75 / 75 files, Win32 75 files 61 PASS: the known 8 files / 20 tests, 5 x64-only files on the Win32 harness, test_fttext's 2 host-GDI metric tests (U-1393; they pass and fail with the host on x64 and in I9's mirror too); all 9 I9 clash files PASS, segments 205 / 205 x64 + Win32 + freetype with 0 digests changed, boot 70 rows 66 / 2 / 2 on
all three equal to I9, chains 342 / 342, ledger 1 row without a cause (as at I7), census 698 rows PASS 100 (59 rows with fewer differing
px than I9, none with more), S3.2 terrain 527 PASS. Process: a background shell's 2-hour limit stopped the suite loop while a pytest ran;
the pytest finished and wrote its result, but its loop must be restarted (13_suite_rest.sh FROM=<file>), and long suites run detached.
S3.14 (2026-10-27, notes log/roadmap/S3/14_notes.md, edits 14_edits.py applied by 14_assemble.py after 13_assemble.py): I12 found
test_app_ingame and test_app_live failing on this tree where d66aa755 passes. Three causes where S3's frame meets S7's app:
(1) once the course animates every in-game frame presents, so an app session's last present lies in its final frame, and
run_frames' boot last-frame branch took pending gfx snapshots only on NeedInput; it now takes them when that frame runs to its end
(U-1394, control tick.frame 12). (2) The test-slot hook runs pytest at BELOW_NORMAL and the app inherited it, which put the host's
ABOVE_NORMAL sim thread below every normal-priority thread: under 10 busy processes its frame waits ended 1-5 s late, and
d66aa755 itself failed with I12's `frame 40 never came (at 0)`; app_drive.launch now starts the app in the Normal class (U-1395,
SG_APP_PRIORITY=inherit is the control, RED on pacing under load). (3) The drivers took 1.5 s without a present as the title menu,
but S3's boot loading screens are 431-776 ms apart (flic sheets decoded for the backend; 117-174 ms at d66aa755) and a click posted
among them goes to the boot's load pump; app_replay._to_monterey and test_app_live._to_title now wait for the title menu's frame
(app_replay.title_menu_hash = the pinned sandbox_basic `menu` reference, U-1396). The app files' pacing gates measure the host:
run them one file per process at low load.
Roadmap S3 wave 6, S3.10 / S3.10b / the LiveTerrain log switch (2026-10-28 / 29, step S3.12, notes log/roadmap/S3/12_notes.md, merge
log/roadmap/S3/MERGE_w6.md, U-1388 / U-1389 / U-1399 / U-1673 / U-1398). Base e179bd24: 01d84188's committed gfx_terrain.cpp held S3.10's
thread calls while its gl_raster.h did not (7 C2039 / C2737 errors on x64 and Win32, all in that file; no other committed file used
working-copy-only code), and e179bd24 put I13's file back. S3.10: GlRaster's default pipeline runs a fast pixel loop that writes the old
loop's bits (edge functions exact as int64 in units of 2^-16, each row's covered span from the three inside tests, each pixel first with
reciprocals and kept only when every texel and 5-bit rounding decision is clear of its boundary) on pixel-loop workers that own interleaved
4-row bands (GlRaster::set_threads; gfx_terrain.cpp starts min(4, hardware threads / 2), SG_RASTER_THREADS=<n>); stream lines `#fastpath
<m>`, `#threads <n>` and the worker control `#workerctl 1`; SG_TERRAIN_PASS_LOG times each pass. S3.10b: terrain_state.cpp resolves the
Terrain object once per exported call and reads and writes it in place (VPtr), no float changed. LiveTerrain keeps a compact record per
call and formats the lines only when log() is read; log_size() / drop_log() / set_keep_log(false) / calls() let the app's TerrainLogBound
bound the log without a const_cast (U-1673). The app tests take SG_APP_FACTS_DIR (U-1398), and test_app_ingame's zoom test is strict (its
U-1640 xfail removed). Evidence against plain e179bd24 (whose port/ equals I13's mirror): all 299 recorded stream and chain passes
bit-identical to the old rasteriser with and without 4 workers, census 698 rows PASS 100 with every row's pixel count unchanged, terrain
527 / 527, segments 205 / 205 and boot 66 / 2 / 2 on x64 and Win32, live equivalence 68 + 2 and app replay 3 / 3, all with 0 rows
differing; port suite x64 82 / 82 files, Win32 the 22 known ids; controls RED: `#workerctl 1` on 270 / 299 passes, `#fastpath 4` on 47 /
268 (`#fastpath 3` blind, U-1389), terrain_state 23, LiveTerrain 6 (log off) / 7 (setType variation lost). App (LIVING-PC, Ryzen 5 3600,
MSVC 19.44 Release x64, three interleaved sessions): zoom frames 39..58 ms at 54..82 % load against 133..175 ms for plain e179bd24 at
59..96 %; idle (1..9 %) every terrain-pass frame 10..30 ms against 25..112 ms. The app's live-trace frame at cov_a_keys k_dlg_J varies in
about 1 run of 6 without workers too (U-1399).
Roadmap S3.17, the last two missing gfx bodies (2026-10-29, notes log/roadmap/S3/17_notes.md, merge 17_merge.md, U-1830..U-1833):
I14's census had 44 UNSUPPORTED rows stopping on `no body: Surface_createFull_fwd1` (22) and `no body: showRoutingMapScreen.teeSprite`
(22); a run's unsupported list is cumulative, so every row after the call kept the status. The routing map's tee and flag are now the
original's addSprite calls of the sheet cells (routing.cpp MapDraw::hole_map: tee = last column of sheet 0x18d + clamp(par - 3, 0, 2),
row 0, shade 4, flags 0x5e; flag = sheet 0x189 + g_courseType, column 0, row 3, shade 2, flags g_courseType + 0x60), drawn at once by
the scene family's addSprite body: cov_a_keys k_dlg_R 135 px (134 beyond one level) -> 1 px one level on a hole line (U-1832).
Surface_createFull_fwd1 (mainLoop's forwarder for the 'B' snapshot and the '.' / '/' routing render images) is Surface_createFull's
body with (this, a1, a2, 0, a3, a4, a5), the order checked on the original under Unicorn (U-1833). The 42 other rows now FAIL with
exactly I14's px, all HUD / bottom-panel / view residue (S3.15 / S3.16) except ui_tour `menus`' 252 px beyond one level, the money
popup `-$600` the port does not draw (S3.16's drawMoneyLabels gap). cov_a_champ n_yes / end were not fwd1: the boot's last frame never
made TraceInput's tail idle passes (only a boot with no frame head did), so the title menu after `Yes, cancel.` stopped at its first
pumpMessages before it drew; tick_run_frames' last-frame branch now sets them (control tick.frame 13), both PASS 0 px (U-1831). The
boot gate never runs that branch (it stops at the end checkpoint; only to_end runs: framegate, gfx.render, test_boot / test_gfx_eoi /
test_video). Controls RED: showRoutingMapScreen 11 / 12, surface 9 (unit test; equivalent on the frames: Image_ctor already erased
the key), tick.frame 13. Tests port/test/test_gfx_s3_17.py.
Roadmap S3.18, the bulletin board's photos (2026-10-29, notes log/roadmap/S3/18_notes.md, merge 18_merge.md, U-1840..U-1845): the
photo is not narrateShot's: showBulletinBoard's first call with a pending photo position saves the root's 0xc8 x 0xa0 rectangle at
(clamp(sx - 0x64, 0, 0x258), clamp(sy - 0x64, 0, 0x1b8)) as `snapshots\accomp<record>.bmp` (saveBmp, 24-bit, rows bottom-up, no
padding), and the board loads every listed event's file into ONE photo Image (loadBmpSurface; FitSurface only grows it, so a missing
file redraws the previous event's photo) and blits it. The shipped install (original\, every instance, the cab) has no `snapshots`
folder, so the original's fopen fails and no pinned frame shows a photo; data1.hdr's directory table lists `Snapshots` (U-1840).
gfx_bulletin.cpp's `showBulletinBoard.capture` writes the file through GameFiles only when that directory exists (dialogs.cpp hands
it the call) and `.photo` reads it back: the capture's bytes equal the ORIGINAL saveBmp's and the decode the ORIGINAL loadBmpSurface's
under Unicorn (SG_EMU_ORACLE=1, port/test/test_gfx_s3_18.py); the file goes through the GameFiles write overlay (the app's --data
directory), and a run over an install holding `snapshots` leaves every install file byte-identical. cov_b_holes18's 5 UNSUPPORTED rows now FAIL with exactly I14's px (HUD
and view residue); the board is modal inside P5, so no H1WAIT checkpoint shows it. cov_r_bio's 2 rows (the bio EditBox, a window-tree
child drawn only inside the bio loop and covered by the editor's next pass) are HELD for S3.19 (U-1844); all 7 rows were
UNSUPPORTED only through framegate's cumulative reason list (U-1845).
Roadmap S3.19, the bio EditBox (2026-10-29, notes log/roadmap/S3/19_notes.md, merge 19_merge.md, U-1846..U-1849): the character
editor's bio EditBox is a window-tree child of the root with its own 0x12c x 0xc8 window surface at (0x1e0, 0x70) (Window::create
with flags 0x2020: no edges, innerSize unchanged). Body family `editbox` (gfx_editbox.cpp) ports EditBox::drawContents (the
EditBox vtable's slot +0x58, called by Window::show and by Window::doLayout after every edit): Window::paint, the bevel passes,
the inset clip, wrapToLines, lineIndexAt, the first visible line kept around the caret, drawTextLines and the caret line;
gfx_text.cpp draws marked-up text on a window target (its surface, clip, the Image's fonts and text object; measureMarkupRun
with wrapTextToWidth and the break tail) and gfx.cpp's Window::refreshFull composes shown children over the root's copy
(Window::blitToTarget). chareditor.cpp's bio entry hands the calls over (show, every key the EditBox takes, a refresh per pass,
close). drawContents STORES the first visible line [+0x57c], a ui-class GameState word the port's bio_entry never writes; the
backend keeps the same rule in its own state; its only reader (pixelToCharPos, from Up / Down and the caret clicks) is not
ported and cov_r_bio never changes it (U-1846). The colours are palette indices: Window::paint gets Palette::nearest(white),
which the port does not store, so the backend paints direct white; text and caret are index 0, drawn black (jgld
Palette::setDefault's table; the palette at the bio is not measured, U-1847). The port's boot never runs the EditBox's static constructor, so the backend reads an unconstructed Image (vtable word 0)
as Image_ctor / EditBox::create leave it; the caret does not blink (U-1848). Evidence: the emulator CALL oracle
(port/test/test_gfx_s3_19.py, SG_EMU_ORACLE=1: the ORIGINAL drawContents under Unicorn with recording callees against the port's
record mode) equal on 300 / 300 varied EditBox states, controls 1 2 3 4 8 RED; a composed frame of a constructed bio state
(controls 5 6 7 9 RED); a cov_r_bio boot with the backend PASS with no unsupported reason left for the bio, and framegate's
cov_r_bio exit / end UNSUPPORTED -> FAIL with S3.18's px (107,138 / 130, HUD and view residue); no reference frame can hold the
box (U-1849). Finding in the original: with 32 wrapped lines wrapToLines' end entry lands on drawContents' return address (its
line table holds 32), so it returns into the text; the bio cannot reach 32 lines.
Roadmap S3.16, wave 7: 2D pixels beyond one level in the view (2026-10-29, notes log/roadmap/S3/16_notes.md, edits
16_edits.py applied by 16_assemble.py on 12_assemble's tree, U-1820..U-1825). Drawing calls the port had dropped now reach an
attached graphics backend: drawLabels / drawMoneyLabels' setDrawTarget + drawCenteredText2 (tick_leaves.inc: the floating text
and money labels, cov_l_camera `-600` 227 px), narrateShot's shadow line and flight arc (setLinePoint / lineTo, shots.cpp),
drawGolferInfoPanel's whole panel (dialogs.cpp, from c3fr.cpp GolferPanel_re in its order: about 100,000 px on 15 rows -> 0),
composeGolferSprite(0x99)'s palette before drawFlyingBall's preview-golfer addSprite (tick_ball.inc; a registered function's
drawing half must be handed over with gfx_drop), and the Window::refreshFull the panel handlers make between drawWindow and
pumpAndRedraw (commands_panels.inc help_window, canAfford; cov_r_palette_hot undo_help 46,200 -> 0; clickSlotPanel /
clickAmenityPanel also drew at the ticker's position). GATE HARNESS: a checkpoint's clock call is its POLL record's position
when the poll rode on a non-tgt call (gfx.checkpoint_ordinal, framegate): status.tgt pointed at H1's g_frameMs read, after the
frame's present (7 rows, 53-2,648 px each -> 0..2; SG_GFX_CHECKPOINT_AT=tgt restores the old mapping). The bottom_panel
residue (26 / 29 / 45 / 50 one-level px) is terrain, not 2D (U-1821).
Roadmap S3.15, the HUD top-bar residue (2026-10-29, notes log/roadmap/S3/15_notes.md, merge 15_merge.md, U-1810..U-1814): I14's
446 FAIL rows with one-level px in hud_title / hud_meters / hud_message / toolbar hold TERRAIN pixels, not 2D ones: split against the
original's root at updateTerrainRender's return (S3.2's exit.bin, 15_terrsplit.py) every differing pixel of sandbox_basic,
cov_k1_river, cov_k3_camera2 and championship already differs in the terrain, and the half-bright frames over them are exact. The
remaining GlRaster residual (U-1392: textured pixels within 0.015..0.08 8-bit units of a dither threshold) was not closed by float32
plane, fixed-point weight, texel or colour models. Instead the terrain can be drawn by the host's own OpenGL: gl::WglSink
(gl_wgl.h, opengl32 loaded at run time onto the original's 16-bit 555 DIB, glreplay.exe's method, which is bit-exact on the c7
passes), selected per LiveTerrain by terrainLive.hostGl(1) / SG_GL_BACKEND=wgl / framegate --host-gl; default GlRaster. Full census with it: PASS
100 -> 612 of 698 (476 FAIL and 36 UNSUPPORTED rows to PASS), 0 rows lose PASS, the GlRaster census equal to I15 row for row, x64 = Win32; what is
left is cov_a_golfer's undrawn golfer info panel (U-1814) and sprite / view content of S3.16's. Controls 8 (565 DIB) and 10 (root not
loaded into the DIB) RED; 9 (top-down DIB) and 11 (texture names unmapped) blind with the reason (U-1811).
Roadmap S3.20, the host-OpenGL startup self-check (2026-10-30, decision G13 final, notes log/roadmap/S3/20_notes.md, merge 20_merge.md,
U-1815..U-1819): GlRaster stays the default terrain renderer; the host's OpenGL (S3.15's gl::WglSink) is opt-in. In the app,
`--gl auto` (or SG_GL_BACKEND=auto) runs a startup self-check and uses the host GL only if it passes, else GlRaster; `--gl host` (= wgl)
takes the host GL without the check. framegate has the same `--gl` (default raster) and columns renderer / renderer_reason. The app logs
its choice (stderr `simgolf_port: terrain renderer ...`, terrain_renderer.json). The check (gl_wgl.h wgl_self_check,
LiveTerrain::choose_renderer) runs the port's own GL setup from the user's install into a WglSink the way the live path does, draws the
19 triangles of the original's recorded pass c7/z2_r1 that reach a 32 x 32 rectangle (port/src/sim/gl_wgl_ref.inc, generated by
log/roadmap/S3/20_make_ref.py) and compares SHA-256 of the rectangle with the hash of the ORIGINAL's pixels there. Host GL equals the
original in 1,024 / 1,024 px; GlRaster differs in 45 (U-1392). Fallbacks, with the reason logged: wrong pinned hash (control 13),
perturbed reference (14), 565 DIB (8), no 16-bit generic pixel format (12). Census: default (GlRaster) 111 PASS / 577 FAIL / 10 NOT_RUN
of 698, equal to S3.9's row for row; opt-in auto 638 / 50 / 10, 0 PASS lost. Why not the default (U-1818): a full host-GL pass takes
33..43 ms against GlRaster's 6..12 ms with workers; app terrain frames median 45 / max 95 ms against 19 / 40. A banded host GL
(20_band/, measurement only) gave the same pixels and median 21 / max 46 ms, but hung inside opengl32 twice (U-1819).
Roadmap S3.21, U-1392 part 1 (2026-10-30, notes log/roadmap/S3/21_notes.md, U-1305..U-1309): GlRaster's terrain residue came from
two stages of Windows' generic GL it did not model, both measured exactly with a feedback-mode probe (log/roadmap/S3/21_glfeed/
glfeed.exe: the pass replayed in GL_FEEDBACK, GLFEED_POINTS=1 one point per vertex): (1) the VERTEX RULE: float32 matrices, the MVP
updated by every modelview operation (not recomputed as P * MV), clip = m3 + x m0 + y m1 + z m2 with every operation float32 (the
translation FIRST), window = round-half-even(f32(ndc * f32(h - 2^-8)) + centre) to 2^-8: all 973,749 vertices of the 268 corpus passes
equal (the double model missed 20,709, the vertices within a float ulp of a 2^-8 tie: S3.16's x bands); (2) CLIPPING: triangles crossing
the view volume are culled on their snapped positions (zero area kept), clipped in clip space (x >= -w, x <= w, y, z; classic
Sutherland-Hodgman order; t = d_in / (d_in - d_out) and attributes in float from the inside vertex) and drawn as the fan of the
polygon. GlRaster's `#tune 28 1` / `#tune 29 1` restore the old model / no clipping, `SG_RASTER_TUNE=<i>=<v>,...` sets knobs for a
process. One-level px of the 271 stream passes 18,234 -> 2,296, of the 237 draw-list chains 10,300 -> 1,605; census sample (5 scenarios,
133 rows) differing px 24,960 -> 1,902 with no row worse and no PASS lost; full census: 21_notes.md section 7. What is left (U-1309) is
the attribute interpolation: incremental 16.16 fixed-point stepping of the 5-bit-scaled colour, truncated, rows walked from the
largest-y vertex (exact on a slow-ramp probe, 480 -> 90 on random Gouraud quads), not yet in GlRaster.
S3.21b (21b_edits.py, opt-in #tune 30): the 16.16 fixed-point colour stepping is exact on untextured triangles (ramp 705 -> 0 px,
Gouraud 481 -> 90) and changes nothing on the textured terrain, so it stays off; U-1309 is the textured (GL_MODULATE) interpolation.
Both part 1 rules are on by default (orchestrator, 2026-10-31: exact stage models, not fitted); SG_RASTER_TUNE=28=1,29=1 restores the
old renderer row for row.
Roadmap S3.24, the overlay line's footprint (2026-10-31, notes log/roadmap/S3/24_notes.md, edits 24_edits.py, U-1826..U-1829;
U-1370 resolved): Windows' generic GL draws an antialiased line per column of the major axis as at most N = floor(w * L / |major|) + 2
pixels, the centres whose minor offset o from the line centre times the tie sign (x-major sign(dx) * sign(dy), y-major sign(dx)) lies
in [-N/2, N/2); S3.8's coverage formula gives the values inside. Measured with glreplay.exe on 16-dither-phase probe sets (the cut
meets N/2 at every width and angle where S3.8's support reached past it) and fitted on exact ties. terrain_overlay.cpp's line rule 2
is the default (overlay.setRule / SG_OVERLAY_RULE: 0 = S3.8's model, 6 = no tie decision, 1 / 3 / 4 / 5 / 7 other tie signs;
control 11 = rule 0); SG_OVERLAY_DUMP=<file> dumps every drawn call with its box before / after. Results: S3.8's probe scenes 108 / 88 /
1 / 18 -> 7 / 12 / 0 / 5 px with none newly wrong; the game's own 9,534 dumped calls through the real GL 879 -> 104 px, none newly
wrong; the 31 overlay-line census rows (22_causes.tsv group 1) 29 PASS with the host-GL terrain (left: cov_a_build brush_465 3 px, the
second-vertex end-cap drop U-1826; cov_a_keys k_dlg_R 1 px value); the default GlRaster census 111 / 577 / 0 / 10 equal in status,
0 PASS lost, 0 rows with more px, 29 rows with fewer (71,871 -> 71,517); segments 205 / 205 and boot 66 / 2 / 2 unchanged; unit tests
33 on x64 and Win32. Open: the end-cap drop at the second vertex of wide lines (U-1826) and a few y-major / non-45-degree ties (U-1827).
Roadmap S3.23, the census split verdict (2026-10-31, decision G18, notes log/roadmap/S3/23_notes.md, edits 23_edits.py): framegate.py
--split runs each scenario a second time with the host-GL terrain (S3.15's WglSink, bit-exact against the original's terrain). A
differing pixel is terrain-origin when it differs under GlRaster and equals the reference under host GL, on rows with terrain on
screen; every other differing pixel is 2D. Split PASS = 2D px 0 in every region and terrain-origin px within G2 over the view area (the
census's G2 denominator, U-1121). Raw columns stay unchanged (raw = f2dff003 on 698 / 698 rows); a host without a 16-bit generic pixel
format reads UNJUDGED, never PASS. Census 70 scenarios: split 638 / 50 / 10 / 0 = the host-GL census row for row; every split FAIL is
2D only; the largest terrain-origin count is 270 px (8 beyond), inside G2. Test seam SG_FRAMEGATE_PLANT=<both|raster>:<region>:<n>
(controls: a 2D pixel planted in hud_title stays FAIL; a terrain-only difference beyond G2 fails as terrain-origin).
port/test/test_framegate_split.py 17 x64 + Win32.
Roadmap S3.26, wave 8's 2D residue (2026-10-31, notes log/roadmap/S3/26_notes.md, merge 26_merge.md, U-1834 settled, U-1950..U-1952):
four of the six cause groups S3.22 found in the host-GL census's 50 FAIL rows are fixed, each from S3.25's recordings of the original.
Zoom box marker (cov_b_inject / inject2 zoom_box, forced_events, end, 15 px each): Widget_readPixel was a not-drawing drop returning
0, so mainLoop P5's `== 0x7fff` test (0x00418b98) never let the port draw g_sprite567b08; family `pixel` now has its body (c3db.cpp
GetPixel_re) and hands the root pixel back through gfx_take_value with a backend (without one the drop still returns 0). Hire dialog
(cov_r_s2_screens hire, 1,397 px): the four employees are addSprite of cellAt(idx + 0x20e, frame mod cols, s) (0x00459777); the port
had passed cell 0. Selected-golfer sprite (golfer_stories season_end, 685 px): f_w_4c2e00[2] = -2 (sim class, the original's value
too) and P4 tests only -1, so the original reads `golfer -2` inside g_sprites5791f8 (element 4's cut size 80 x 80 places it, element
5's handle byte gates it); the port keeps SpriteRef fields in the backend only, so P4's three reads go through gfx_sprite_ref_read
(gfx_pixel.cpp) when a backend is attached (U-1950: the handle byte is answered 1). Animated sprites (cov_k3_camera2 y_inj4 414 px,
cov_b_holes18 holes 2,829 px): not a drawing body: the checkpoint's POLL rode on the frame-head clock call (its pos is a FRAME record's;
S3.25's call log has 153 resetSpriteQueue calls there), and the snapshot rule skipped that frame's own cursor reads, so the port showed
the next frame's spray columns; gfx.poll_position (framegate, gfx.checkpoint_ordinal, test_app_ingame) now takes such a checkpoint at
the head call (pos - 1); 9 of 688 checkpoints have such a POLL (U-1951), SG_GFX_CHECKPOINT_AT=poll16 restores S3.16's rule. All six rows
PASS on host GL. Not fixed: cov_k3_camera y_inj3 (847 px): worldToScreen(0xc7ff, 0xd800) is tile (49, 54), past the grids; its level
read g_tileLevel[2504] lands on byte 0 of g_surfaces5439e0, the vtable 0x004ba2d8 Surface_ctor stores (0xd8 = -40, sy + 215 instead of
+ 15), and the port's GameState holds no vtable words (U-1834 settled; kept as an explained frame divergence on cov_k3_camera y_inj3 (orchestrator, 2026-10-31)).
cov_r_s2_screens course / radial_hover / report rows (65 / 20 / 11 px) are terrain: terrainlive over S3.25's recording gives 20,204 =
20,204 Terrain calls and 32 / 32 states equal, and the original's pass draws a near-black dither over the map's right edge that the
port's draw list does not hold (U-1952). Development switch SG_GFX_SNAPSTATE=1 (gfx.cpp) keeps the GameState with every gfx snapshot
(`$gfx/state_<k>.bin`; log/roadmap/S3/26_snap.py). Controls RED: pixel 2 and 3, showHireEmployeeDialog 3, SG_GFX_CHECKPOINT_AT=poll16.
S3 wave 8 heavy-run lock (2026-10-31): log/roadmap/S3/w8_lock.py take | release | run <step> -- <cmd> | status enforces one S3
heavy run at a time (a build, a pytest file, a census batch or a game boot): `take` writes w8_wait_<step> (epoch, its pid), blocks
until that file is the oldest live wait file, the lock dir w8_heavy.lock is free and >= 16 GB commit charge is free; wait files of a dead
take pid are removed; `release` refuses another step's lock; `run` always releases; `--priority` (lead / orchestrator only) ranks first,
used for I20's quiet window (w8_i20_window.py). A hand-written lock seen by a waiter is logged to w8_breaches.log. It replaced a written
oldest-first rule that workers broke four times in one night (a helper that checked only the lock dir, an unlocked finish step).
S3.22 (W8.3) classified the 50 host-GL FAIL rows into seven cause groups (22_causes.tsv, images local) and removed the unused body
showCharacterEditor.bio (W8.6, gfx.check 137 bodies); S3.25 (W8.4b) recorded the original for groups 2-7 on instance s3_w8 (8 runs SAME
as det_b, port/corpus/w8rec/ and log/p5/terrain/s325_* local, 25_notes.md).
Merged-tree census on 2291a342: raw 123 / 565 / 0 / 10 (0 PASS lost, 576 rows fewer px, 71,871 -> 6,203), split 677 / 11 / 0 / 10; the
11 are group 5 (explained), group 2 (U-1952, 8 rows), U-1826, U-1829.
Roadmap S3.28, lowerEdgeCorner's black-light pass (2026-10-31, notes log/roadmap/S3/28_notes.md, merge 28_merge.md, U-1955..U-1957,
U-1960, U-1961): U-1952's near-black dither in cov_r_s2_screens' map corner is Terrain::lowerEdgeCorner's draw half,
renderTileAtFiveOffsets (Terrain.dll 0x10038900, re/match/terrain_edge.cpp): localRender's transform without a draw list and, when a
neighbour toward the camera is outside the grid or hidden, enableBlackLight1 (0x10003830: light 0 off, light 1 on with zero ambient /
diffuse / specular) and the tile rendered at five offsets, leaving only the global ambient term (0.04 x texel). It is terrain pass kind
5 (terrain_render.h: a0 tile, a1 pivot, a2 angle = lowerEdgeCorner's arguments 0, 2, 3); LiveTerrain::lower_edge_corner hands it to the
backend before the state call (the original draws before Tile::lowerCorner), and it takes no p<j> pass number (the original's pass
copies count kinds 1..4). GlRaster needs no GL_LIGHT1 model while its colours are 0. Recorder log/roadmap/S3/28_gl_rec.js /
28_gl_record.py: entry a3 (lowerEdgeCorner's angle), renderTileAtFiveOffsets as a direct kind-5 entry for injection, kind-5 passes that
draw nothing kept. Recording s328_s2b (cov_r_s2_screens, SAME as det_b): 12 organic passes (angle 0, as every organic lowerEdgeCorner
call of the pinned terrain recordings) and 20 direct calls on the edges of the land (x, y = 12..37) at 0 / 90 / 180 / -90, each edge
tile drawing at exactly two angles. All 32 streams equal the original's line for line; GlRaster meets G2 and the host GL equals the
original's back buffer on every pass. Census (698 rows) against the wave 8 stack: exactly the 8 cov_r_s2_screens rows of group 2 move,
split 677 -> 685 PASS (all 8 PASS), raw 123 -> 126 PASS (rep_histograph, rep_report1, rep_roster; the other five keep 1..4 px of
GlRaster terrain residue, none beyond one level), 0 PASS lost, 0 rows worse. Controls RED: kind 5 skipped (gfx.family.terrain 15: 65
px in the corner box, 20 beyond one level), renderer 15 / 16 / 18 on the streams, 17 on the streams and the host GL (blind in GlRaster
by construction).
Roadmap S3.29, U-1309 (2026-10-31, notes log/roadmap/S3/29_notes.md, U-1965..U-1969): the generic GL's textured interpolation, measured
with probes replayed through glreplay.exe (log/roadmap/S3/29_probes.py; slow ramps, alternating 0 / 255 texels that amplify u's error
255 x, random triangles with permuted vertex order, and boundary probes whose exact value sits on a 5-bit boundary for one dither cell so
the arithmetic decides it; fitted by 29_texfit.py), is in GlRaster behind `#tune 32 1` (default off), in both pixel loops: a textured
triangle whose texture has a GL_LINEAR filter is interpolated in float32 (gradients by the t1..t4 form from the lowest vertex, rows walked
from the largest y with the row start = float32(previous + float32(-dA/dy rows) + float32(dA/dx pixels)), restarted from the exact plane
where the left edge turns at the middle vertex, float32 sums along each span; colour at the 5-bit scale), and an unblended GL_MODULATE
pixel is filtered, modulated and dithered in float32 with texel = byte / 255.0f (GlRaster::TexWalk, walk_final). Exact on 9 probes
(alt_big 64,599 -> 1 is one of the 4 that are not: 1-2 float32 ulps of u or v left, U-1965); all-NEAREST textures take another path
(U-1966); the blend's float form is not determined (976 of 1,115,136 channel values, U-1967). The terrain's remaining residue is mostly
not interpolation (per-vertex lighting, coverage ownership 2e-6 .. 2.4e-3 px from an edge, blending: U-1968): c7's seven passes one-level
173 -> 154 with the rule; full census with the rule raw 123 -> 131 PASS of 698, differing px 6,203 -> 4,221, 0 PASS lost, split verdict
unchanged (677 PASS).
S3 wave 9 (2026-10-31, merge log/roadmap/S3/MERGE_w9.md, stack w9_assemble.py): on 2291a342 + wave 8 + the two I21 fix layers
(w8_fix_i21.py: the pixel family's body count and the hire RESIDUAL after S3.26; w8_fix_i21b.py: test_gl_wgl's sandbox_00 pin 125 -> 9 px
and the self-check's GlRaster control, which turns S3.21's rules off with glWgl.rasterCheck(1) since the default GlRaster now passes the
check) S3.28 and S3.29 give census raw 126 / 562 / 0 / 10 and split 685 / 3 / 0 / 10 (0 PASS lost, 0 rows worse); the 3 split FAIL rows
left are cov_k3_camera y_inj3 (U-1834, explained), cov_a_build brush_465 (U-1826) and cov_a_keys k_dlg_R (U-1829). w8_lock.py: a
`--priority` step that made the last hold yields one turn (alternation), one pytest file per hold, and `run` starts its command with
CreateProcess (wrap ~/bin/py in `bash -c`; test-slot the same: `test-slot bash -c '...'`).
S3 wave 10 (2026-11-02/03, merge log/roadmap/S3/MERGE_w10.md + log/roadmap/merges/I24.md, layers 31 / 30 / 32 applied in that order by
their assemblers, U-1970..U-1983): S3.30 adds trace DELIVER kind 7 `call` (pos, addr, argc, a0, a1), written by trace_rec.js, applied by
TraceInput in trace order and re-recorded by the live core (`LiveWriteLog::called`), so golfer_events' injected landmark call replays
(boot PASS 2522 / 2522); S3.31 is overlay line rule 9 (float32 line ends and the second-vertex holes, U-1826; brush_465 split PASS);
S3.32 makes Surface_ctor's vtable stores for the boot's 147 static Surface objects, the word read from the user's exe through
f_imm_473ab9 (U-1834 option b, default ON, control tick.frame 14; y_inj3 split PASS). Census raw 127 / 571 / 0 / 0, split 697 / 1 / 0 / 0:
the one split FAIL is cov_a_keys k_dlg_R's 1 px (held, G27 = a). tests/test_trace_format_s1.py fails on any tree without S3.30's
trace_format.py, since the shared corpus holds the kind-7 golfer_events.trace.
Section S4, audio engine model (2026-10-24, lead notes log/roadmap/S4/STATUS.md, steps re/roadmap/S4_steps.tsv, function map
re/roadmap/S4_functions.tsv: 744 sound.dll rows, 573 C3/C4 + 31 C2 ported in S4.2..S4.9, 71 CRT and 69 dead not ported).
The path-1 CSVs keep only an FNV-1a hash of each region's raw bytes, and every one of the 19,785 regions of the 980 sound.dll
keys is a fixture block holding the run's own addresses (heap blocks, list links, vtable pointers of the relocated DLL), none
recorded, so the test_bridge.py replay cannot reproduce them. Decision G1 (Mariano, 2026-10-24): the oracle is the ORIGINAL
function in Unicorn over the user's sound.dll on its replayed fixture, port == emulated original byte for byte, the emulated
original tied to the CSV on every address-independent field; default runs check golden digests. All 205 segment start
images hold g_soundReady 0x83afc4 = 0: every segment runs the exe's device-less sound path.
Step S4.1, sound.dll replay (2026-10-24, re/analysis/data/sound_replay.md, notes log/roadmap/S4/1_notes.md, U-1400..U-1409):
the 980 sound.dll path-1 keys replay without the game. re/tools/fixvm/fixvm.js (Node) runs the fixture JS and the real
stand-in helpers over a mock Frida API (frida.js, mem.js, syslibs.js); port/test/sound_emu.py runs the ORIGINAL sound.dll in
Unicorn, relocated (5,769 HIGHLOW entries), DllMain first, traps for callbacks / fake DLL exports / Interceptor redirects
served by the VM over a synchronous page-syncing link; KeyRun repeats diff_hook.js per key (regions, arm built after the
fixture, per-vector resolve, both arms). re/tools/sound_census.py -> re/roadmap/S4_census.tsv, three layouts (a field equal in
A, B and C is position-independent): 181 EXACT, 785 equal on every position-independent field, 5 DIFF (__ftol_snd: Unicorn
sets no x87 exception flag and stores 0 for fistp of an empty st(0); 42 rows per key match with the flag byte searched),
2 distinct-count differences (U-1403), 5 partial (RaiseException SEH, timeGetTime), 2 fixtures that wait for the live engine.
Controls 10 RED (fixture byte flipped, wrong relocation base, callee stub +1), 2 blind. Facts: the live TLS index of
sound.dll's CRT was 27; the CSVs print doubles shortest (2026-10-07) or as 17 truncated digits (2026-10-14);
KeyRun(second_arm=...) is the port hook. Tests: tests/test_fixvm.py (16, static marker, ~35 s).
Step S4.2, the audio core (2026-10-24, module audio_core, notes log/roadmap/S4/2_notes.md, U-1410..U-1417): sound.dll's
port runs over the context's 32-bit space with the image at a caller-chosen base. port/include/portsim/sim/soundstate.h
(GENERATED by re/tools/soundstate_gen.py from re/names/sound_*_globals.tsv, hooks.csv and the user's section table):
portsim::SoundState = .data (0x54098 B, members at their offsets, static_asserts), snd_rva constants (.rdata vtables and
tables), kSoundFunctions (rva -> hooks.csv name of all 744 rows). portsim/sim/audio.h: audio_attach (the harness maps the
image, or map_image maps the user's sound.dll from GameFiles after checking .data size and an .rdata hash), snd_obj /
snd_va / ld / st, and the DISPATCHER every sound.dll port function calls through: snd_call (rva, thunk or body),
snd_vcall (virtual slot), snd_call_va, snd_import (IAT slot). Order: a VA the host claims (a fixture's Interceptor.replace)
-> host; inside the image -> follow E9 thunks -> the registered port function of that rva; inside but unported -> host as
FALLBACK (development only, counted; gates want 0); outside -> host as EXTERNAL. Hosts: the C ABI callback sg_snd_set_host
(live), or the in-library script host (a recorded transcript from "$audio/script", requests checked slot by slot, replies and
host memory writes replayed), configured through sg_call "audio_core.setup" etc., so the Win32 runner needed no change.
audio_sink.h: AudioSink (import / com commands by DLL!name from the image's import directory, interface objects whose
vtables route back), RecordingSink. port/test/sound_replay.py: ORACLE (SG_EMU_ORACLE=1) runs the port as KeyRun's second
arm on the emulator's memory in layouts A / B / C, the emulator serving the port's host calls, and ties the emulated
original to the CSV with S4.1's census compare(); DEFAULT replays bundles (port/corpus/audio, local, 17 MB) against golden
digests (port/test/golden_audio_core.json), x64 and Win32; SG_S4_MIRROR=<dir> points it at a mirror build's x64 / w32 DLLs.
First family: the 60 S4.2 rows (lists, nodes, queues, helpers; 64 keys + 4 derived keys of the emu-only rows: PtrList_pushFront
and PtrList_insert borrow their instruction-identical twins' fixtures, stubReturnBB the (this, uint32) key setField6cReturn0's):
1,219 vectors (1,159 of the 64 keys) equal to the emulated original in all three layouts, 0 fallbacks, tiers EXACT 8 /
EQUAL_PI 55 / DISTINCT_DIFF 1 / EMU_ONLY 4; mode 1 of all 60 functions RED. re/tools/snd_hook2port.py converts hook sources
(accessor roles found by body shape, thunk / slot / IAT calls to the dispatcher, VAs in comments to RVAs, macro-made detours
expanded, adapters and FuncDef rows): its UNEDITED output for the 57 hook-sourced S4.2 functions compiles at /W4 and passes all
64 keys from the bundles (`--module audio_conv`, log/roadmap/S4/2_conv_replay.log). Segment gate 205/205 x64 and Win32,
digests equal to a HEAD build; boot gate unchanged.
S4 wave 2 (2026-10-24/25, steps S4.2b..S4.11, notes log/roadmap/S4/<k>_notes.md, U-1418..U-1499 and U-1540..U-1549): sound.dll's
engine is ported in eleven modules over the audio core: audio_mixer (the Voice_mix* leaves, renderers and streamRender at the
original's x87 word, Win32 real x87 and an x64 software 80-bit path that equals it on every vector and 60,000 probe operations),
audio_chan (channels, voices, Voice::update), audio_seq (sequencer, MIDI, tracks), audio_sound (Sound / MultiSound /
SoundGroup, create_sound, Engine_playSound), audio_ds (DirectSound as AudioSink commands, whose logs equal the import stand-in's)
and audio_dev (devices, Ctrl, command queues, SoundTimer, the GetProcAddress exports), audio_wave (wave-out / wave-in devices,
mmio RIFF readers over GameFiles, stream readers, decode rings), audio_soundfile (loadSoundFile, the Sound vtable's slot 4: no
user file uses its chunk format, so every path but the unknown-tag exit is judged on synthetic banks, U-1492), audio_codec (the
LPC-10 speech codec), audio_crt (the CRT helpers, so 0 development fallbacks remain) and audio_exe (the exe's Snd layer,
playSound / startSoundSlot / initSound / loadSoundTable on the port's engine). S4.2b consolidated the core: ONE stack mechanism
(SndFrame: a function's locals at the original's offsets below its entry esp), the virtual x87 unit as the core FPU model,
unclaimed `jmp [iat]` stubs routed as imports, SndResidue for words a caller pushes that its callee never reads, `snd.<name>`
registration (an unnamed function keeps Ghidra's FUN_<va> until it is named), and one front end, port/test/sound_replay.py
`--step S4.k`. Oracle (G1): 1,087 keys / 16,859 vectors equal to the emulated original with 0 fallbacks, and the twelve golden
files pinned. Segments run with the audio module attached and write an AudioSink request stream (205 / 205, 4,964 requests, every
digest unchanged). All 205 start images hold g_soundReady 0, so the requests are recorded at the exe's sound layer; a device-present
fixture run drives the engine end to end (181 requests, 0 fallbacks). Test-only switch `audio_exe.stubRequests` stands in for the
audio calls the UI oracles stub; sg_state_free (audio_release -> audio_exe_release) drops audio_exe's per-context state, which a
context allocated at a freed context's address used to inherit (I8: test_buyland's Win32 digests varied run to run). Unicorn rules folded into the oracle: the x87 exception flags are not set (searched), and a
signaling NaN copied through fld / fst is quieted by the CPU, not by Unicorn (U-1451, 17 + 1 vectors forgiven on the quiet bit
only). U-1421: the codec's caller control word is not measured. Three live runs with an audio device never entered the codec:
all 319 install sounds are PCM and openMmioSound / loadWaveResource are the network voice path (D6). The port keeps the codec at
0x027f. Findings in the original: renderChannelStreamed hangs on an underrun at the read limit; create_sound maps `.aif` to the
default; the MIDI loaders never check `MTrk`; loadSoundFile never leaves its chunk loop on a truncated file (U-1490); both
create_device exports report success after a failed allocation; Midi_Device::shutdown skips every second track.
Phase 7 defect decisions, section S6 of the roadmap orchestrator (2026-10-24, notes log/roadmap/S6/, U-1550..U-1597): `re/port/defect_decisions.tsv`
holds every defect of model_defects.md (#1-#52, some split by arm) and every non-PASS row of divergences.tsv (grouped by cause) with its class
(hang / fault / overrun-silent / wrong-value / dead-state / port-gap / harmless), six effect columns (save / balance / sim / frame / hang / fault),
a recommendation and the switch: 74 rows, 10 `fix`, 26 `ask`, 38 `keep` (generator `log/roadmap/S6/merge_decisions.py --write`; schema
log/roadmap/S6/SCHEMA.md). The classification came in two passes: S6.1 / S6.2 from the model notes, then S6.6 re-read the cited instructions and ran
the ORIGINAL in Unicorn on constructed states (log/roadmap/S6/6_probes/, one probe per defect, each hang with a completing control at the same
budget). Corrections it made: spawnWalker with 64 live walkers completes (writes the record over g_editQueueX, returns 0x40) instead of hanging;
stampCourseTiles' scan only reads and the original already refuses a full table; showMembershipRoster copies a type name, not the 0x230-byte slot;
D22's register test is equivalent; rebuildHeightfield's relax pass and buildGolferBioText's picks cannot loop on the shipped tables; the movie id
list of serviceMovie has no writer; and one new hang, loadGolfPro with g_difficulty 5 or 6 never ends even with every pro record filled (D42c, `ask`).
Profiles (port/test/profiles.py, tests test_profiles.py 19): `original` = every sg_defect switch 1 and is the oracle (equal to no profile); `fixed`
= the members of re/port/defect_profiles.tsv, and only decision D5's class: a hang or fault whose fix changes no save, balance value, simulation state
or frame on a run the original completes. Six switches, each default 1, each judged by the probe state that hangs or faults in the original (the
port returns cleanly at 0) and by a completing control whose result is identical at 1 and 0, plus a wrong body that reads RED:
placeHabitat.clusterLoopBound (slot 127 with 0xff after the table is treated as taken), spawnObject.cellSearchBound (return before the animal record
is claimed), loadGolfPro.divideAndRejectGuard (a zero divisor or an empty roster ends the draw at pick 0: U-1590, no original run exists there),
placeAnimal.zeroFrameCount, replaceInText.tailLimit (a tail of 0x200 or more is refused: the original overwrites its return address from there;
U-1597 truncation instead), generateRandomCourse.searchBound (the type-0x11 search skips the feature, the clubhouse search takes the window centre:
U-1596, a port choice). Tests port/test/test_defects_golfers.py (25) and test_defects_gen.py (13). The 26 `ask` rows (every fix there changes a
save, balance, sim state or a frame) are in log/roadmap/S6/ASK_LIST.md for Mariano: D1 D5 D6 D10 D11 D12 D17-D21 D23 D24 D26 D32 D37-D39 D42c D44
D47b D48a D48b D49a D52a D52b3. Findings in the original: loadGolfPro hangs for difficulty 5 and 6; replaceInText derails the return address from a
tail of 0x200 bytes (0x204 returns to the tail's own bytes); spawnObject's cell search draws about 250,000 times before the budget ends when all 16
cells are taken; the five existing switch rows keep their evidence, chooseShotTarget.strictFlagSticky still has none (U-1012).
S6 wave 2 (2026-10-24, notes log/roadmap/S6/7_notes.md and 8_notes.md, U-1591, U-1592, U-1598, U-1599): Mariano's answer G3 kept the 24 `ask` rows of groups
overruns, wrong values and course / terrain / money at `keep` and turned the two hangs into `fix`, so re/port/defect_decisions.tsv reads 74 rows,
12 fix / 62 keep, with a `decision` column (D5 fix, G3 fix, G3 keep) and profile `fixed` has 8 switches (re/port/defect_profiles.tsv). D42c:
loadGolfPro with g_difficulty 5 or 6 never ends in the original (4 - d < 0 makes the acceptance bound fall without end); switch
loadGolfPro.negativeWindowBound (own switch, tests port/test/test_defects_golfers_w2.py, 45): the port's old behaviour at those difficulties was an
endless spin like the original's, now switch 1 raises NoReturn once the falling bound is below every value the roster can give and switch 0 ends the draw
with pick 0; where the original completes the check never fires and 1 and 0 agree, the 11 of 36 synthetic states in which the original hangs in Unicorn are exactly
the ones the port gives up on, and the golfers vectors are 367/367 GREEN at 1 and 0 (U-1591 the cutoff argument, U-1592 the emulator stops at loadThemeFile's
file reads on the states where the original passes). D52b3: the feature-site searches gated by evalPlacementArea hang the original at location 4 (2 x 2 site,
0x0047172e..0x0047176f) and at locations 5, 6, 8, 12, 13, 15 (5 x 5 site, 0x004718a4..0x004718e7) when no reachable cell is accepted (300 M instructions
exhausted at about 921,000 iterations on the port's own images); switch generateRandomCourse.placementBound (wave 1's searchBound keeps D52b1 / D52b2),
tests port/test/test_defects_gen_w2.py (30; controls for all 8 locations equal the original byte for byte at 1 and 0, location 3 on the same image
completes): at 0 location 4 stamps no object and locations 5..15 skip the whole feature (U-1598, U-1599, port choices). The opt-in emulator oracle for
terrain_gen (115 tests) passes. The old S6.7 note that wave 1's loadGolfPro guard raised NoReturn at d = 5 / 6 was wrong: the port spun.
Roadmap S1, loose ends of phases 3 and 4 (2026-10-24, log/roadmap/S1, plan re/roadmap/S1_steps.tsv, five step workers on claude2 and an
integration check): U-1067 resolved: trace_format.validate rejects a first-generation input MSG (WM_KEYDOWN/UP, SYSKEY, mouse buttons) with
no earlier harness DELIVER (WM_CHAR and WM_MOUSEMOVE are derived and exempt; a trace slice is exempt up to its second FRAME, U-1151);
266 of 266 stored traces validate. U-1060 resolved: the four header read sites (loadGame, loadCourse) and the two writers are pinned by
port/test/test_sve_header_readers.py and none reads past the NUL, so the leftover g_textBuffer residue is dead data. U-1012 resolved:
port/test/test_shots_strict.py builds GameStates (42 of 700 seeds) where the original keeps chooseShotTarget's strict flag (frame word
[esp+0xb4], one store at 0x00423b0e, read at 0x00423b20) after a later landing fails the test; the port with the switch ON equals the
original in Unicorn (eax and the whole image), OFF is RED 8 / 8 (SG_EMU_ORACLE=1). U-1066: cov_r_theme_cancel (the Cancel after a row
press in the same visit; showThemePackSelect control 2 RED) and cov_r_palette_hot (Undo hotspot 0x15 and wrench tab 0x16) are pinned;
the palette SpriteRefs 84 / 88 are read only by showInfoCursorPanel's drawing, so loadInterfaceArt control 3 reads GREEN by hash and
test_s1_3_scenarios.py compares all 96 cut fields with the original's images (U-1170); cov_r_tutorial runs showTutorialText pages 1..29 and
30 (the page word 0x00567a18 and bit 15 of g_flags are harness writes: no instruction stores a nonzero page, U-1182), boot 1,195 / 1,195,
controls 1..4 RED. U-1051: cov_r_flic_input posts a key in courseActionMenu's wait; the original dispatches it from the 15th pumpInput483bd0
of loadFlic (loadWorldAssets makes no clock read); the port takes it with PumpKind::Load (U-1181, U-1183 open). U-1032: the character editor's
bio entry is ported over the EditBox family (chareditor.cpp: create, setText, resizeBuffer, insertChar, onKey without Up / Down, resetFields,
releaseState); the original routes keys to a focused EditBox through the window tree, so a key it takes leaves a MSG record with no HANDLER
record and tick.cpp parse_trace makes those RootHandler commands 12 / 13 (none exist in the 64 older traces); the bio loop makes no clock call,
so cov_r_bio posts its keys from the scenario agent's own doAction at leaveModal483d80 (DELIVER logged); oracle 7 x 300 EditBox cases and 120
editor cases (212 bio entries) equal the original, golden_chareditor.json re-pinned for cases 43, 61, 90 after the oracle was GREEN. New
scenarios register through the hook at the end of re/tools/scenario.py (re/tools/scenario_s1_*.py). Mirror run with every patch: boot gate 63
PASS / 2 KNOWN / 1 NO_FRAMES (the three rows that differ from the S0 baseline are cov_r_bio, cov_r_flic_input, cov_r_tutorial), segments 205 / 205
x64 and Win32 with every digest equal. Findings in the original: Window::key / dispatchAccel feed an EditBox before any root handler; the
bio loop never reads the clock; the page word has no nonzero store. Open: U-1170..U-1172, U-1180..U-1183, U-1190..U-1195, U-1151, U-1160..U-1162.
S8 video, skip path (2026-10-24, D2: no decoder this round; re/analysis/video/MOVIES.md, movie_runs.md, skip_path.md, re/port/movies.tsv,
flics_census.tsv): the game plays exactly two Bink movies, both through playAudioFile 0x0043cce0 -> BinkPlayer::play 0x00487090 (its only caller): the
intro SMSG_introfinal.bik from showTitleScreen 0x0043cd97 (path 0x004c8a0c) and the closing SMSG_closingfinal.bik from mainLoop 0x0040faa4 (path
0x004c6bb4); jgld.dll, Terrain.dll and sound.dll import nothing from binkw32. No FLIC is a cutscene: all 1,893 .flc files are loadFlic sprite animations
(S3); the serviceMovie family (0x0049c8e0) is an unused RIFF/mmio player (table 0x00840698 never written). Measured on the original with the intro ON
(instance s8_2, three runs per condition identical): both play to the clip end (825 and 589 frames), binkw32 paces with QueryPerformanceCounter (54,945
main-thread calls for the intro) and never reads timeGetTime, so the movies add no virtual T and no scenario poll runs inside them; against the skipped
intro the only run-stable GameState word that differs is g_ui83ab9c 0x0083ab9c (0 -> 1, written by BinkPlayer::play, cleared by the root-window input
handlers); g_seedTime, g_rng and every sim object are equal. Port: module `video` (port/src/sim/video.cpp, portsim/sim/video.h) ports playAudioFile and
BinkPlayer::play / setTarget / close behind a MovieHost; the default host answers "unavailable", i.e. the original's own BinkOpen-NULL path (deadline word
set and cleared, g_bink target set, one pumpInput483bd0, g_ui83ab9c untouched), which is what the pinned corpus recorded; the intro call is made from the
boot (tick_boot.inc), the closing call from the QUIT block (playAudioFile is no longer a drop). binkw32's QPC records never enter a clock list
(parse_trace keeps timeGetTime values and reads are site-keyed), so an intro-ON trace needs nothing else. Evidence: emulator oracle 23 / 23 cases, 12 / 12
branches both ways; 5 negative controls RED; new intro-ON boot scenario cov_r_s8_play (re/tools/s8_boot_capture.py) PASS 216 / 216 and to the exit with the
quit files equal; boot rows 61 / 61 unchanged, segments 205 / 205 x64 = Win32 with 0 digests changed. U-1700..U-1723 (input during a playing movie is
modelled not measured; the drawing around a movie and the sound argument are not ported).
Section S5, text without GDI (2026-10-24, roadmap orchestrator; steps re/roadmap/S5_steps.tsv, notes log/roadmap/S5/<k>_notes.md,
report re/analysis/data/text_freetype.md, U-1500..U-1532): a FreeType TextMetrics provider (module fttext) beside the GDI one, judged
against it. FreeType 2.14.3 core (no zlib / png / bzip2 / brotli / harfbuzz) comes from vcpkg in C:\Users\maria\vcpkg\installed
(decisions D4 and G4; license FTL, its credit line goes into the documentation of any binary that links it) as an OPTIONAL CMake
dependency: `SG_FREETYPE_ROOT` (or `-DPORTSIM_FREETYPE_ROOT`) at configure links it and copies freetype.dll beside the DLLs; without
it module fttext throws MissingResource{"FreeType"} and nothing else changes. gditext was split into MarkupTextMetrics (the
original's Text_put / drawMarkupRun / measureTextWidth chain) and GdiTextMetrics (the GDI primitives; every output row-identical to
before); FtTextMetrics shares the chain. FACES (measured with GetFontData hashes on LIVING-PC): GDI resolves lfFaceName by the
family name, else by the full name of a face alone in its family, else to Arial, so the game's `Arial Bold` 9 / 10 and `Times New
Roman Bold` 18 are drawn with arial.ttf (regular) and `Comic Sans MS Bold` italic 100 with ariali.ttf; Manual SSi (Bold) and Klepto
ITC TT are the install's manu3_.ttf / klepto__.ttf. A host without Windows fonts must supply arial.ttf, ariali.ttf and times.ttf
($SG_FONT_DIR). WIDTHS: ppem = size, interpreter v35, the five FontRef words from VDMX else OS/2 win metrics, advances = hdmx at the
ppem, else linear rounded when head flags bit 4 is clear or LTSH allows, else v35 hinted (3,136 / 3,136 probe cases); FreeType equals
GDI and the recordings on every printable row (11,459 widths, 9,079 Text_put, 10,776 drawMarkupRun, 15 / 15 metric words, x64 =
Win32); only lone 0x0a rows differ, which the original itself measures differently per process (U-0960). PIXELS: the original's labels
and GDI's are ClearType subpixel on the 16-bit DIB (lfQuality 0 follows the host's ClearType setting: on, contrast 1200, RGB here).
Module textlab + port/test/labels.py hold a label corpus (6,540 (FontRef, text) pairs from the widths recordings, 15,960 jobs, plus the
calls of the six gfx checkpoints that pass at HEAD; GDI reference 0 px against itself and against the original's frames). The
FreeType raster (fttext_raster.{cpp,h}) reproduces GDI ClearType's blend exactly (per channel in the 5-bit space through gamma
contrast / 1000 tables, 0 / 7,168) and its 6-sample box filter (exact on Arial), but not GDI's ClearType x hinting (U-1520):
8,736,593 corpus px differ (about 70 % of GDI's ink; Klepto 0.26-0.29, Arial 0.60, Manual SSi 0.61-0.81) and the six checkpoints
differ by 1,735 to 15,347 px each, all inside label boxes. The simulation reads text only through widths, so with
`SG_TEXT_METRICS=freetype` (segments.py / boot.py / integrator.py; default gdi) the segment gate stays 205 / 205 and the boot gate 58 /
2 / 1 on x64 and Win32. The port's default stays GDI; the report's last section is Mariano's decision (recommendation: keep GDI on
Windows, FreeType for hosts without GDI, judged by these numbers). Process lessons: on claude2 `cmake` is not an allowed command
(`~/bin/py` is), so children run every configure and build from a Python script; two S5.3 workers stalled on unanswered permission
prompts before that rule was in their brief.
Roadmap S7, the Windows host, wave 1 (2026-10-24/25, log/roadmap/S7, plan re/roadmap/S7_steps.tsv, U-1600..U-1664): `port/app` builds
`simgolf_port.exe` (port/CMakeLists.txt add_subdirectory(app)), which plays from the user's install (`--install <dir>`, saves under its
own `--data` dir, never in the install; no original binary loaded). LIVE CORE (module `live`, port/src/sim/live.cpp, live.h; tick.cpp
asks a LiveHost through tk::LiveIo, inert in trace runs): the frame loop takes the frame-head clock, every clock read, pump batches,
cursor reads and GetAsyncKeyState from a host on demand and records every answer as a schema v1 trace plus framehash.bin; a host
that answers from a corpus trace equals sg_run_boot in 68 of 70 boot scenarios plus 2 NO_FRAMES, x64 = Win32, 0 replay
differences; idle passes of unpaced modal loops (title menu hover) are recorded so the replay takes the same passes (U-1601);
S1's world-load pump and focus-child keys are handled (U-1604). INPUT (port/app/input_map.*): Windows messages to root-handler
commands, cursor and async-key answers, 0 mismatches over all 26,861 MSG records of the 67 corpus traces. APP (host.*, window.*,
present.*, pacing.*, saves.*): sim thread above normal priority, frame wait in real time (355 periods 87..100 ms, median 93; 125..135
with g_flags bit 17), the gfx back buffer presented at every frame wait and modal pump (title menu 0 px against the original's
sandbox_basic `menu` checkpoint at 96, 144 and 240 dpi), idle on the title menu 3.1 % of one core, every session recorded and
replayed through sg_run_boot equal in every record (422 / 422). Tests: port/test/test_live.py (32 x64, 32 Win32), test_app_live.py
13, test_app_shell.py 18, test_app_input.py 16. Open: U-1610..U-1619 (double click, wheel, Alt, modifiers, chars above 0x7f
unmeasured), U-1620..U-1626, U-1650 (bio EditBox keys from the real keyboard), U-1660, U-1662, U-1663.
Roadmap S7.4, app recordings in the boot gate (2026-10-25, notes log/roadmap/S7/4_notes.md, U-1630..U-1637; U-1611 /
U-1612 resolved):
- `port/test/app_replay.py record <hole|saveload|report>` drives the app with posted messages only. The app runs with
  `--cursor message --keys message --clock-origin 0x100000`, and the session goes into port/corpus/app/<name>/
  (local), together with posted.json and the app's new dispatch log (input_dispatch.tsv, dispatches.tsv, body_calls.tsv).
- `app_replay.py gate [--backend win32]` runs sg_run_boot over each recording against its own framehash.bin.
  hole 340, saveload 266 and report 205 records PASS on x64 and Win32 (port/test/test_app_replay.py, 14 tests).
- The recordings hold:
  - Shift / Ctrl clicks, a double click, the wheel, Alt+X and Shift+A;
  - one hole built and opened;
  - a save the `S` key wrote into the app's overlay, loaded back through the browser (g_date returns to the save's value);
  - the F1..F4 report screens (modal).
- CROSS-CHECK (log/roadmap/S7/4_xcheck.py, instance s7_4): the app's raw input was posted into the original at the same
  pump calls, by frame-head count:
  - P6 = the `call pumpInput483bd0` at 0x004199e0;
  - H1 = the `call pumpMessages` at 0x0041c0cd;
  - the cursor reads were answered from the app's CURSOR records;
  - the virtual keys were set as the messages were posted.
  Result: all 340 records equal the original's in every sim object but g_frameMs, which stores a timeGetTime value
  (U-1637). All 59 root-handler calls are identical in handler, arguments and frame.
- The cross-check found three things, fixed:
  - the translator dropped uiKeyEvent's modifier bits;
  - the translator dropped the double click's rootWin_onPress1b;
  - the host let H1 take several messages or a key. H1 takes ONE mouse-range message: pumpMessages drains WM_MOUSEMOVE,
    then display slot +0x13c (jgld 0x1006b7e0) makes one PeekMessage(0x200..0x20a). P6 loops PeekMessage(0, 0) until
    the queue is empty.
- Control: the tee click moved by 64 px in the original's input diverges at record 185, the head right after that click.
Roadmap S7.5, the in-game frame in the app (2026-10-25, notes log/roadmap/S7/5_notes.md, U-1640..U-1643): simgolf_port.exe now
attaches a LiveTerrain (`--terrain live`, the default; `headless` = tick.cpp's stand-in) after gfx.attach, as framegate does, so
the course is drawn into the gfx root surface and presented with the 2D bodies; waits.tsv carries the cumulative terrain passes /
pixels. `--live-trace <scenario>` runs the live core over a corpus trace with a LiveTracePresenter (presents where a session
makes them) and gfx.snapshotAt at `--snapshots`; `--negctl reg:<name>=<mode>` sets any port negctl switch. Gate
(port/test/test_app_ingame.py, 12 passed + 1 xfail): an in-game session driven by posted messages (Monterey, seven camera moves,
four zoom keys, a hole built) has every present (84) equal to the replay's back buffer with LiveTerrain at the same recorded
position, and its view differs from a headless replay in 358,874 px; over cov_a_keys, cov_k1_routing, sandbox_basic and
course_season every checkpoint (49, 46 in game) produced on the app's live path equals framegate's frame in every pixel and the
present that showed it, and the three in-game rows framegate passes (k_worldmap, k_dlg_L, routing) pass; 368 in-game frames hold
87..98 ms (median 93). Open: frames with two terrain passes (zoom) work 144..186 ms (U-1640); HUD panels, toolbar
and sprites are not drawn at this base (U-1641). Controls RED: terrain pass skipped in the app and on the live path, pacing off,
headless terrain. S7.6's test_app_live replays now attach LiveTerrain (5_test_app_live.patch).
Roadmap S7.8, the app's present point and a long session's memory (2026-10-25, notes log/roadmap/S7/8_notes.md, merge 8_merge.md,
U-1670..U-1673; U-1642 / U-1643 resolved): the original's back buffer reaches its window only through jgld Display::present (vtable
slot +0xa4), which the exe calls once, at the end of drawTooltip 0x0047cdb0; Window::refreshFull reaches it through refreshRect (H1
0x0041e29b in an ordinary frame), the jgld Listener's paint / mouse-move slots and the tooltip through the other callers. P6's
[[0x0083ad50]+0x9c] is Display::setScale(1, 1, 1), not a present (mainLoop_structure.md row P6 corrected). tick.cpp calls
LiveIo::present_point after the Window::refreshFull drop of a live run, and the app presents there ('r'): 0 frame-wait presents over
four traces and the in-game session. U-1643's terrain log was small after the world load (25,404 of 25,671 calls in it over
course_season_6y); the growth was the live core's in-memory records (3.3 KB per frame). LiveOptions::trace_file spills the recording
to disk every 4,096 events (byte-identical to the in-memory one) and on_record hands each frame record to the app, which writes
framehash.bin as it goes: over course_season_6y's 51,092 records private bytes stay flat after frame 2,000 and framehash.bin equals
the original's capture byte for byte. `--in-memory` restores the old way.
Roadmap S7.9, S7 waves 1-3 merged as one (2026-10-26, notes log/roadmap/S7/9_notes.md, merge 9_merge.md / 9_apply.py, U-1680..U-1685):
rebased on 78ffb06d with I7's three refusals fixed. (1) cov_a_keys no longer takes the live core's stuck delivery (U-1680); the test
now runs ui_tour / ui_tour_v (4 inflated dispatches each) and judges the call through TraceInput's count of deliveries at another cursor
count, which must equal the live run's inflated count: live.recorder control 10 (the CURSOR record left out) is RED there and blind in
every frame record and drawing call. (2) tick.cpp's HeadlessTerrain kept its 16 MB arena per freed context in a process-wide map keyed
by the Ctx address, so one Win32 runner grew up to 48 MB per equivalence scenario and died near 2 GB: ~sg_ctx now calls
release_headless_terrain (one runner stays at 4-5 MB over 8 scenarios, 52 -> 117 MB without it), and test_equivalence_all runs one
process per scenario (live.py run_parallel). (3) port/CMakeLists.txt adds the app before S5's FreeType block, which copies freetype.dll
beside simgolf_port (the x64 app had loaded a JDK's from PATH). Since S2.7 the boot's loading screens and the module-drawn modal screens
refresh outside TkBase::call, so the app presented none of them: the present point is now a refresh hook the gfx backend calls at the end
of every Window::refreshFull body (gfx_set_refresh_hook, set by the live core for its run only; U-1670 resolved); the boot's title and
first loading screen share one recording position, so the app tests judge the last present at each position (U-1685). Gates: port suite x64 75 / 75 files, Win32 the known 8 files / 20 failures, test_equivalence_all x64 + Win32,
live equivalence 68 PASS + 2 NO_FRAMES x64 = Win32, app replay 3 / 3, segments 205 / 205 and boot 66 / 2 / 2 on both backends with 0 rows
differing from 78ffb06d, repo static 614 / 2.
Roadmap S7.10, audio out (2026-10-29, notes log/roadmap/S7/10_notes.md, U-1686..U-1692): `simgolf_port --audio on|off|model|capture`
(default on since S7.11) keeps the sim's device-less sound path (GameState unchanged: --live-trace sandbox_basic trace.trace and
framehash.bin equal with and without audio) and copies every request of audio_exe (Req depth 0, the new
`audio_exe_set_observer`) to a SHADOW context on an audio thread (port/app/audio_out.*): the exe's initialised data plus the
device-holder constructors (`audio_exe_construct_holders`), gameMain's initSound(1, 0x8a) before the first non-CRT request,
shutdownSoundSystem at the end, the timer client called at its 30 ms period on that thread. Its sink (port/app/ds_sink.*)
answers through S4's DeviceSink model (`DeviceModel`, `make_device_model`, `audio_exe_device_fixture`) and drives real
dsound.dll buffers (DSound) or a software capture (Capture). Gate (a): audio_stream.device_run's sequence through the app
sink gives $device 4b6ed61a x64 and Win32 (`simgolf_audio_check device`). Gate (c): a scripted title session's capture
against the install's WAVs (port/test/test_app_audio.py): clicks at volume / 127 and hard pan, static buffers at unity gain
then faded; controls unlock-skip and volume-ignored RED, pan / frequency equivalent (the engine sends no SetPan, SetFrequency
only at 22050 Hz). Finding: S4's model answers SetWindowLongA 0, so SoundWindowObj_subclass never kept the window and every
completion message went to hwnd 0 (a slot played once); the app sink answers a previous-procedure token and delivers the
messages to soundWindowSubclassProc. The real device was not available (DirectSoundCreate DSERR_NODRIVER, TV off, U-1691).
Roadmap S7.11, audio out edge cases (2026-10-30, notes log/roadmap/S7/11_notes.md, U-1693..U-1699): `--audio` defaults to
`on`. NO DEVICE: when DirectSoundCreate fails (or `--audio-no-device` / audio_check `--no-device 1` forces it) the sink answers
the engine's device probes as a PC without an output device does (scenario_baseline.md section 1: IDirectSound::Initialize
DSERR_NODRIVER, its Release 0, midiOutOpen / waveOutOpen errors), so the shadow's initSound takes the original's own failure
path (eax 3, g_soundReady 0, after exactly the measured probe order), its thread ends and the observer drops every later
request; the sim's trace.trace and framehash.bin equal `--audio off`'s. SHUTDOWN: the window session's WM_CLOSE stops the
shadow before DestroyWindow (gameMain's order); no thread id is left (Toolhelp32) and the handle count stops growing after a
process's first DirectSoundCreate. Pause (g_flags bit 2, key 0x50 at 0x0041d020) makes no sound call and focus muting is
DirectSound's own (the engine's buffers carry no GLOBALFOCUS / STICKYFOCUS, level DSSCL_PRIORITY), so neither needs code.
Roadmap S7.12, the course-sfx alias and the app's defect profile (2026-10-30, notes log/roadmap/S7/12_notes.md, U-1607 / U-1608 /
U-1627 / U-1628 / U-1638): switch `sound.golfSfxAlias` (audio_exe.cpp golf_sfx_name, decision G15, D54): at 1 the device sink's
mmioOpenA of the exe's `sounds\Golf sfx\` names fails on the extracted install's `Golf_Sfx` as the original's does (the CD header
iso\data1.hdr names it `Golf Sfx`; unshield wrote spaces as underscores), at 0 the install's Golf_Sfx file opens. Device run:
switch 1 = golden $device, switch 0 fcbc9195 (229 opens against 211) = an install with the exe's directory name line for line, x64
= Win32. `simgolf_port --profile original|fixed` (default fixed, compiled copy of defect_profiles.tsv, `--profile-list`); every
session.json records `profile` and every switch's value, and app_replay.gate_one replays under them. The tropical study trace runs
under fixed and faults under original (va 8, the stock game's). The same unshield rename hides six music files (U-1608). S7.13 (G16): the same switch opens the six theme music tracks under sounds\music\Desert Music / Links Music / Tropical Music from the install's underscore folders (kDirAliases); the seven names wrong on the CD stay absent; switch-0 device run 07f83f3f, 235 opens (24 aliased), x64 = Win32.
Roadmap P3, the long session's bimodal memory (2026-11-02, notes log/roadmap/P3/notes.md, edits log/roadmap/P3/edits.py,
U-2050..U-2053, U-1692 resolved): test_long_session_memory_bounded's number depended on the PC's audio device and on the heap.
(1) The app takes its last memory sample after audio.finish(), which frees the shadow audio context: with no device the shadow
fails at initSound and is gone before frame 2,000, with one (LIVING-PC has VB-Audio `CABLE Input` since 2026-11-01) it is alive
at frame 2,000 and freed at the end, so the measure read -19.9 MB. (2) Through (1), three leaks of ours in audio_exe.cpp
DeviceSink, which the app's DirectSound sink answers through: Release kept a buffer's storage (16 MB-chunk arena, at most 8) and
interface object (64 KB block), a freed block served only a request of its exact rounded size, and mmioClose kept mmio's own
buffer. A sink that reports a buffer's end (the real device, Capture) lets the engine reap idle sounds and load them again, so the
shadow grew with wall time (base1: 8 of 8 chunks after 1,571 s) until MissingResource silenced it. Fixed (edits.py A / A2 / A3,
control registry `audio_exe.deviceHeap` 1): the same session with the device holds a 13.4 MB arena and 2.0 MB live; capture WAVs
are byte-identical; port/test/test_audio_device_heap.py. (3) With audio off the growth is the sim's: the present log (24 B per
present; every in-game frame presents since S3) reallocated by 1.5x among three 960,000-B buffers per present, and whether its
983,160-B block found a free hole is the 0.97 MB between the modes (no-device runs 3,407,872 / 4,300,800 / 4,403,200;
SG_RASTER_THREADS=1 and --terrain headless show both modes). LiveTracePresenter reserves the log from the trace's frame count
(no-device growth 892,928 / 962,560), and the memory test measures up to the last in-session sample. test_app_audio's dsound
shutdown test fails with a real device on any build (cycle 1 releases 6 handles, U-2050).

Section S10, SMSG import and verification (2026-10-29, steps re/roadmap/S10_steps.tsv, notes log/roadmap/S10/<k>_notes.md,
merge log/roadmap/S10/MERGE.md, U-1850..U-1897). `re/tools/smsg_pdb.py` reads their MSF 7.0 PDBs (procedure rva / size / name /
.obj / archive, publics); `re/tools/smsg_import.py file|project|inventory|calibrate` turns an SMSG source into an annotation-only
re/match wrapper (`// COMPILE` their file, `// INCLUDE` their .dsp / build.bat dirs, `// FLAGS` from the .dsp project line plus
per-source overrides, `// TOOLCHAIN sp5` for sound.dll, one MATCH line per PDB procedure of that .obj: the public at the rva,
else the obj symbol by undecorated name, overloads scored, an ICF-folded address held once), scores every line, writes 100%
lines to re/match/smsg_<pdb>_[<lib>_]<stem>.cpp and misses to re/match/wip/. Never `--evidence` while exploring: match.py
writes a CSV for every result and a RED CSV breaks the C4 gate of a row already at C4. Calibration: all 169 procedures of
their Terrain sources compile to 100% at addresses we already matched (after `// COMPILE` replaced `#include`: under /ZI an
included file's __LINE__ is a constant); every address we matched in golf / jackal / jgld / libpng / zlib / lpc10 also reads
100% from their source. New 100% addresses: golf_clean.exe 609 (golf.exe/src 249, jackal.lib 335, jpeg.lib 25), sound.dll
489 (+143 bodies our function lists lack, 100% with the PDB extent, 6_boundaries_sound.tsv), jgld.dll 6, Terrain.dll 0;
misses 4 (97-99.9%, MultiDebug::~MultiDebug 92.9%). mainLoop 0x0040f5c0 is 100% from their MainLoop.cpp. SP5: `// TOOLCHAIN
sp5` runs SP5's cl / c1xx / c2 (C2 12.00.8966 = sound.dll's Rich entry, proven by the obj @comp.id); it lifts none of our 54
sound near misses and only one of their 489 sound functions needs it (0x10010920, 99.4% with 8168): sound.dll's C++ objects
were built by SP5 but 8168 and SP5 emit the same code for nearly every function. Library-object identity (S10.7): jgld 317 /
318 and Terrain 471 / 472 library procedures are SP2 libcmtd members, the exe 234 / 262 RTM libcmt, sound 425 / 554 SP5 libcmt
(the rest are import / Terrain thunks; `__amsg_exit` is below 100% in all four, U-1880); a FidDb rebuild was not needed.
Names (S10.8 / S10.9 / S10.10 / S10.7): 1,578 new rows in re/names/smsg_*.tsv with our citations, and rename lists
(8_merge.md, 9_renames_sound.tsv, 10_renames.tsv) of which rows marked P rename port keys (decision D1). Findings: golf.pdb is
right on four of five contradictions (0x004038f0 drawWalkers, 0x0040e720 openHole, 0x0042dea0 rateCourseHoles, 0x00422530
golferShotReach); the 0x0056d1b8 records are celebrity vacation-home residents (spawned by mainLoop 0x00417a5a on type-5 lots,
string 0x004c6444), not animals; golfer fields U-0010 / U-0015 / U-0016 resolved. match.py: an exe operand the obj holds as a
plain literal is compared as a literal (U-1886).
Merged by I16 (2026-10-29, log/roadmap/merges/I16.md): C4 2,765 -> 2,928 (exe +102, sound +61; gameMain held by U-1351, sound strcpy
0x1004a010 kept at its 7-byte extent), 1,578 names C0 -> C2, the 143 sound bodies created (sound.dll list 2,358, 632 smsg sound lines),
19 false starts removed per debug DLL, 23 renames without port keys; rows marked P (port keys) and the g_animalRecords symbol rename
wait for a port step (D1); mainLoop stays C2 (D2). ApplyNames.java dies on a DuplicateNameException when an ANALYSIS label of the
same name sits at the entry (21 FID labels such as __lockexit 0x004a51cf) after saving the renames before it; I16 used a copy that
drops the label and retries (log/roadmap/merges/I16/ghidra/ApplyNamesI16.java). ApplyNames reads every re/names/*.tsv, other sessions'
untracked files included: apply from a directory holding only the files being merged. UNCERTAINTIES.md addr cells must be one hex
number or empty (re_classify parses every row).
Wave 2: the SimGolf demo's sound.dll (archive.org Simgolfdemo, InstallShield 6) is a debug build with NB11 CodeView records
(2,146 procedures, 1,493 class layouts; re/tools/cv_debug.py): 58 map to retail by code identity (LPC-10, CRT asm), the rest only
through vtable slots (45 of 47 tables), giving 13 names and 72 cited field proposals. Two exe misses reach 100% only with SP5's
front end, but the exe's Rich header lists only build 8168 objects, so they are not evidence (U-1910).
Section S11, RE upkeep wave 1 (2026-11-01, G23; steps re/roadmap/S11_steps.tsv, notes log/roadmap/S11/<k>_notes.md, merge
log/roadmap/S11/MERGE.md, U-2000..U-2023). S11.1, the five C3 rows: gameMain 0x0045baf0 reaches C4 by SMSG GameMain.cpp (100%
320/320, no inline asm) once U-1351 is resolved (the original's 18 Sprite::drawG-wrapper call sites each pass a constant table, 16
g_halfBrightLut and blitPanel's two g_color16Lut, log/roadmap/S11/1_shade_sites.py; U-1351 had held gameMain only through its addr
cell); sound strcpy 0x1004a010 by library-object identity with SP5 LIBCMT.LIB strcat.obj `_strcpy` (0xf0 bytes, 0 relocations,
15 / 15 rows, 1_strcpy_lib.py; S10.7's CSV had been dropped at I16 only for its extent proposal). Held at C3 (re/c3_hold.tsv):
drawAmenitiesPanel 0x00434cf0 (SMSG Toolbar.cpp 99.9% under 8168, one reload pair after the second tooltip call 0x00432620 in the
other order, 100% only under SP5's front end, U-1910 / U-2001; route: scenario_ab.py over cov_a_build / cov_k3_build) and
fillSpanDown 0x00493000 / fillSpanUp 0x00493080, which SMSG PolyFill.cpp matches 100% (41/41, 40/40) as compiled C functions whose
whole body is one inline __asm block: which G26 (Mariano, 2026-11-02) ruled is not C4 match evidence: they need scenario evidence
(U-2000 resolved; CSVs parked in log/roadmap/S11/1_ev/, not log/diff). S11.2: the Ghidra master already carried every re/names row (5,222 function rows, 1,091
globals rows) except the four IMPORTED `entry` names of the smsg_*_lib CRT startups (kept: the loader's symbol) and five withdrawn
labels: ApplyNames skipped withdrawn `label <name>` rows at data addresses (G1's g_animals, g_typeAttr, g_recordOccupancy,
gTextBuffer2Index) and round 17's g_golferRoster 0x0058587a had no withdrawn row; all five were removed (backup
ghidra/backup/SimGolf.rep.pre_s11names_20261101). ApplyNames now applies withdrawn label rows and compares function rows by the
mapped name (the 32 Terrain rows with `<`, `*` or spaces were re-renamed to the same name on every run). Check:
log/roadmap/S11/2_compare.py over an ExportNames.java export (log/roadmap/S11/2_ghidra/). S11.3: re/roadmap/S11_c2_triage.tsv
holds every open C2 row (1,881: exe 500, jgld 337, sound 561, Terrain 483) with endstate verdict, callers / import callers /
pointer installers, reach (called 1,015, pointer-installed 657, pointer-no-installer 14, no caller 195), port module and 100% MATCH
line (exe 295, sound 289, jgld 22, Terrain 0); generator log/roadmap/S11/3_triage.py. 220 rows passed endstate.py and were marked
(dead 188, trivial 32), open C2 1,881 -> 1,661. endstate.py passes the four PE entry points as dead (U-2020; excluded by hand). 280
of the exe's 296 pointer-installed open rows already have a 100% match but no C3 (vtable / callback bodies): the largest group left.
Roadmap S11.4, the S10 renames that waited for the port (2026-11-01, notes log/roadmap/S11/4_notes.md, merge 4_merge.md, U-2030..U-2032):
the 70 `P` rows of S10 (8_merge.md sections 2 / 2b, 10_renames.tsv, 9_renames_sound.tsv minus three already applied) are applied by
log/roadmap/S11/4_assemble.py --root <dir> [--names]: re/names + hooks.csv retag (4_names.py), 936 anchored edits in 190 files of port/,
shim/src (comments only: 84 files, code and strings identical by lexer), re/model (addresses.tsv rows, the stream rows' types, the section
headers) and two new files, then the generated headers rebuilt (simgolf_model.h, model_ctypes.py, gamestate.h / _meta.inc /
_addresses.inc, soundstate.h) and model_merge checked to reproduce addresses.tsv. g_animalRecords -> g_vipResidents (type VipResident),
g_recordBanks -> g_homeLayouts (type SgHomeLayout), spawnObject -> spawnVipResident, placeAnimal -> updateVipResident, tickAnimals ->
updateVipResidents, drawFlyingBall -> drawWalkers, announceBuilding -> openHole, holeMagazineEvent -> rateCourseHoles, rateTile ->
golferShotReach, pumpInput483bd0 -> doAllTasks, frameTick483cd0 -> doMouse, enterModal483d30 -> flushInput, drawText4049d0 ->
drawTextWithColor, the sound placeholders Ch5* / Ctrl5* / Sound5* -> their class names, and the rest of 4_renames.tsv. Keep the old
spellings: registry keys, fixtures, SG_HOOK name tokens (the shim log and scenario CSV stems), pytest ids, the defect switch ids
spawnObject.cellSearchBound / placeAnimal.zeroFrameCount (U-2032) and every corpus file. The corpus is never rewritten:
re/port/corpus_aliases.tsv + port/test/renames.py translate at the readers (vec stems, raw capture keys, the manifest and coverage
tables, a .vec document's `sym` locations in re/tools/vector_lib.read_vec, segment frames.json / checkpoints.json, the captures'
framehash.json); without them every boot row ERRORs and every segment fails its load check. Merged by I25 on cdeb2609 (log/roadmap/merges/I25.md, fast gates by
G30): builds x64 / Win32 rc 0, static 820 passed (tests/test_port_scope.py and port/test/audio_stream.py read re/port/scope.tsv's
old names through port/test/renames.py), segments 205 / 205 x64 and Win32 with 0 digests changed, boot 67 / 2 / 2 on both, app
replay 3 / 3, gfx.check 0 duplicates, the 8 golden files, test_tick 250, test_golfers 60 and the audio files. Golden digests that hash a renamed callee name (commands.recordPeer logs FNV-1a of the
name) are re-pinned through SG_EMU_ORACLE=update after the oracle is GREEN.
S11.5 (2026-11-01, log/roadmap/S11/5_notes.md, U-2020 resolved, U-2040..U-2042): re/tools/endstate.py's dead checks refuse a function
containing the module's AddressOfEntryPoint or a TLS callback (`not the PE entry point or a TLS callback`; none of the four modules has a
TLS directory) and count an export inside a function's range, not only at its entry; before, the four CRT startups passed as dead (S11.3
left them out of its marks by hand). tests/test_endstate_entry.py: the entry points refused, every hooks.csv dead / trivial row still
passing, stubbed TLS / export cases, controls RED.
P4, the port diverges from the original (2026-11-03, decision G30, merge log/roadmap/P4/MERGE.md, brief log/roadmap/P4/BRIEF.md): profile
`play` = every `fixed` member plus re/port/play_profile.tsv (17 rows: the five existing gameplay switches and twelve new ones for D11 D12 D17
D19 D21 D23 D24 D26 D44 D47b D48a D49a), compiled into port/app/main.cpp (kPlayProfile) and the app's default; `--profile` or
SG_APP_PROFILE chooses another, and port/test/conftest.py sets SG_APP_PROFILE=fixed so app tests that compare with the original's
recordings are unchanged. Every switch still defaults to 1, so `original` stays the oracle (segments 205 / 205 x64 + Win32 with 0
digests changed). computeStoryReply.clampLowerBound (D48b) exists but is not in `play` (intended clamp not determined); D5 / D6
(grid-edge reads, about 30 sites) have no switch yet. A trace recorded from the original stops driving a `play` run once the game
diverges (course_season: 1,426 of 6,296 frames, then a modal wait): replay gates under `play` need recordings made under `play`.
P5, first play-test feedback (2026-11-03, merge log/roadmap/P5/MERGE.md): two more `play` switches (mainLoop.titleExitHotspotWide: the
title exit is chosen only over its icon; showThemePackSelect.column5StackWord: U-1260's stray cell not drawn); two port bugs fixed (the
world map now runs its 50 ms passes without input, `--no-timed-passes` restores the wait; the course report and top-10 trophy screens
draw); app options, each default on and all off with SG_APP_P5B=off (port/test/conftest.py): --edge-guard (no edge scroll from a cursor
outside the focused client area), --zoom-keys all|numpad|off (+ / - become the game's z / x in the frame loop; `all` drops the original's
+ money / - terrain keys), --close-prompt (X opens the game's exit prompt), --resizable and --scale auto|nearest|smooth (4:3 bars, mouse
mapped back). Widescreen is scoped only (P5B_notes.md: 0x320 / 0x258 hard-coded at about 180 sites, rasteriser fixed at 800 x 600).
P6 + P7, second play-test feedback (2026-11-03, merge log/roadmap/P6/MERGE.md): X now leaves the app through the game's own
title-menu exit (P5B's title test used g_difficulty == -1, stored only on the boot path 0x0040f8ab; tick_menu.inc now notifies the host
through LiveIo::title_menu_pass, and the app clicks the exit icon at the next cursor read, so top10.sve / Default.pro are written).
Three `play` switches keep walkers, golfers and the camera centre on the 50 x 50 grid (updateWalkers.cursorTargetOffGrid,
updateGolfers.walkOffGrid, mainLoop.cameraFollowOffGrid: the original follows an unbounded cursor tile and only H1's keys clamp the
camera). hoverTip.tenFrameDwell shows HUD tips after 3 frames instead of 10. App options: --loading-overlay (default on; the world load
is about 1.5 s, flic sheet decoding 60-65 % of it) and widescreen stage 1 `--wide` (default off; 16:9 window, bands filled from the 2D
screens' edge colours, black in game; plan log/roadmap/P7/PLAN.md). SG_APP_P6B=off turns P6B's options off (conftest sets it).
P8A (2026-11-03, merge log/roadmap/P8/MERGE_P8A.md): the world-load indicator is the gfx body `loading.banner` (text family), drawn
with the ticker window's own calls (drawFrameR3, g_font51b360, drawTextSurface's colour) plus a bar in the panel colours, on a copy of the
back buffer (no GameState or back-buffer write); P6B's GDI box is gone. Standing rule (Mariano): every UI the port adds uses the game's own
fonts and art through the gfx backend. Widescreen decisions G32 (re/roadmap/DECISIONS_PENDING.md): sim visibility window scaled to the wide
view behind a `play` switch, HUD panels anchored to the edges and scaled with window height, `--wide` on by default and following resizes.
P8B (2026-11-03, merge log/roadmap/P8/MERGE_P8B.md, notes P8B_notes.md 7-9): widescreen stages 2 + 3. `--wide` is ON by default and
follows window resizes (view width W in a view block at 0xFDF00000 outside the GameState, written at a frame head as a recorded harness write);
the world (terrain via GlRaster::resize_frame, scene sprites incl. side-column scenery, overlay lines) renders at the window's native pixels
(`--render-scale auto`); the HUD compositor (present.h kHudPanels) moves the title panel left, the meters right and the bottom band left, scaled
with window height, and PanelRemap maps clicks back to 800-space. `play` switches worldToScreen / tileToScreen.wideViewVisibility widen the sim's
visibility and scenery windows to the wide view. SG_APP_P8B=off (conftest) or `--wide off` gives the pre-P8B app.
P9 (2026-11-04, merge log/roadmap/P9/MERGE.md, notes P9A_notes.md / P9B_notes.md): simgolf_port.exe is a WIN32-subsystem app (output in
<data>/simgolf_port.log, and the console when launched from one); simgolf_port_console.exe is the console build. Esc goes back on six screens
that ignore it in the original (play switches, P9A_esc_survey.md); the title menu's false hover is the original's (Start New Game lit from the
first frame, 160 px hover radius): play switches mainLoop.titleDefaultItem1 / titleHotspotRange. Band fill `--wide-fill edge` (default) fades
the edge colour into the screen's background. The HUD is now LAYERED (P9B): the gfx backend captures each panel's drawing as its own RGBA layer
where the game draws it (TkBase::call's GfxLayerScope by callee, the shot panel's texts by mainLoop range), the presenter composes world,
centred 2D and the layers at their anchors with their blends redone, and clicks are remapped by layer coverage; golfer action / employee
panels go to the right edge. `--hud-layers off` = P8B's rectangles. SG_APP_P9=off (conftest) restores P9A's defaults. Text at the window's
resolution is P9C (interface in P9B_notes.md section 5).
P9C (2026-11-04, item 4 of P9's feedback, brief log/roadmap/P9/P9C_BRIEF.md, notes P9C_notes.md, patch P9C.patch on c5e66b59): text at the
window's resolution. Every text reaches a surface through TextMetrics::draw_glyphs (gfx.cpp draw_text_string, gfx_text.cpp put_glyphs root
and window-target branches); with capture on (`gfx_set_text_capture`, app only) each call is kept as a GfxTextRun (FontRef, Font::create's
LOGFONT, colour, TextOutA origin, bytes, box with before / after words). At each Window::refreshFull the visible runs are peeled off a copy
of the back buffer (per P9B layer segment too, with layer patches), and the app's presenter (port/app/textres.*) redraws each run with the
same GDI font at height * window height / 600, clipped to its scaled box, never reflowed. Rules found on frames: repeats of the same run
merged (meters: 884 runs per frame before), a run kept only while a quarter of its glyph pixels show, golfer labels the wide scene draws
again peeled off the world image. Widths the sim reads stay the 800-space ones. Options `--text-res native|frame` (default native;
SG_APP_P9C=off in conftest = frame), `--text-aa system|cleartype|grey` (default system = ClearType here), `--selftest-p9c`, controls
`--negctl p9c-unremoved|p9c-wrong-scale|p9c-wrong-colour` and gditext 9 (scaled widths: changes every cov_r_quit 800 x 600 present).
Evidence on port/build/P9C: builds x64 / Win32 0 errors, selftests both incl. p9c 10 / 10, segments x64 205 / 205 = I25, framegate 15 rows
= P9B, trace and framehash byte-identical off / frame / native (sandbox_basic, course_season, championship), 800 x 600 native == frame on 26
presents, live replays 4 / 4, the 8 app test files pass. Clipped runs 0 at 1920 x 1080, 4 / 1,978 and 10 / 3,383 at 1280 x 720. Left:
FreeType not used for the redraw; Manual SSi Bold's i dot merges at 24 px (Windows' rendering); P9B's HUD drawn twice in modal report frames
and the palette tooltip staying centred; the scale-1 measurement mode differs by 25 px on one frame (two golfer labels touching sprites).
Roadmap S9.1, the network survey (2026-10-29, re/analysis/net/NET_MODEL.md, tables re/port/net_functions.tsv /
net_messages.tsv / net_state.tsv, tool re/tools/net_survey.py, tests tests/test_net_survey.py, U-1750..U-1754, U-1760..U-1763):
the Jackal network layer is linked but never active in v1.03 or v1.00. g_netSession 0x008400b8 is .bss with no store in the
image; the four entries into the layer (gfxFlushA / gfxFlushB from the input pumps, uiIdleNetPoll, Window::onCommand) test it
and return; g_netInterface 0x008400b0 (IDirectPlay2A) is only ever stored 0 (0x00497d78). No DPLAYX.dll import descriptor
(golf.exe, golf_clean.exe, v1.00: only the name string 0x004bf82e, unreferenced), no LoadLibraryA of it, no CreateThread /
CreateEventA / socket import, no Receive / Open / EnumSessions call; the DirectPlay GUIDs 0x004bc3e8..0x004bc428 are
unreferenced and the connect / host / join dialogs exist only in jackal.txt. The 13 interface calls: Close, Release,
EnumGroupPlayers (callback netMarkRecordCb), 9 Send, GetPlayerName. Packet = type word, sequence dword, payload at +8;
types 0x10 roster (host only), 0x20 keep-alive every 2 s, 0x80 peer drop, 0x100 / 0x200 stream data / end; DPSYS 3 / 5 /
0x31 / 0x102 / 0x103 parsed by netPoll; no simulation state is sent (membership, liveness and a stream only; host migration
by slot order). All 205 segment images hold 0 in every net global. The firewall here is BlockInbound with notification and
has no rule for the game. S9.2 part 1 built a generic DirectPlay recorder (re/frida/js/net_rec.js, reader
re/tools/net_trace.py, 24 static tests); its Frida part never ran, since no organic run can reach DirectPlay.
Decision G14 (Mariano, 2026-10-29): S9 closes as `the original has no network play`; recordings, replay and interop gates
dropped; the dormant layer is ported as specification only (S9.3: module net, 12 bodies + 13 registered
names over a recording NetHost at the IDirectPlay2A slot level, never called from tick; port/test/test_net.py 29 passed, 13 / 13
controls RED; only netNameOf (20 rows) and netPump's address-free rows are CSV-exact path-1 replays, because the other
net path-1 CSVs hash fixture blocks that hold the run's own heap addresses, so the rest are judged by instruction-level
assertions on rebuilt session objects with the host call log checked call by call; uiIdleNetPoll, netMarkRecordCb,
netService, netSendMessages have no path-1 rows and stay host calls; netHandleControl.slot15Stays default 1; U-1770..U-1774).
P9D (2026-11-04, brief log/roadmap/P9/P9D_BRIEF.md, notes P9D_notes.md, light testing at Mariano's request): the HUD drawn twice in modal
frames. Reports run after the frame's panel layers and their full-screen dim (a 0.25-alpha sprite blend) was not recorded as a blend, so it
became an opaque centred layer holding dimmed copies of the panels (present 396: 89,185 px); other modal screens published no layers and fell
back to P8B's rectangle cut (the hire dialog in three pieces). Now the dim is a recorded blend applied to the whole window, a modal screen reuses
the last frame's base and layers with its own drawing as one centred layer, a modal opened from inside a panel closes that panel's layer, and a
layer-less modal frame is shown unmoved. `--hud-modal on|off` (default on, SG_APP_P9D=off). The palette tip already rode in the palette's layer
(P9C's tip_top_7 picture was the palette's right end, not a centred tip). Builds x64 + Win32, segments x64 205 / 205 = I25, --selftest-p9d 7 / 7.
Left: popups (System menu) stay at their 800-space place; the frame closing the hire dialog can show panels at their 800-space places.
P9E (2026-11-04, brief log/roadmap/P9/P9E_BRIEF.md, notes P9E_notes.md, one fix per commit, light testing). Fix 1: showPopup / textInputBox
popups (the System menu) opened from a HUD panel are composed and remapped at that panel's anchor (opener = the open panel layer, else the
panel under the opening click); key-opened popups stay centred. Sim side records only (gfx_set_hud_popups, owner plane); `--hud-popups
on|off`, SG_APP_P9E=off. Builds x64 + Win32, segments x64 205 / 205 = I25.
