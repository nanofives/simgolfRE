# S2.2 screen map (survey, no builds, no game runs)

Deliverable: `re/roadmap/S2_screens.tsv` (50 checkpoint rows + 27 family rows). Generator: `~/bin/py -3.12 log/roadmap/S2/2_gen.py`
(checkpoint rows are the CP table in it; family rows are computed from hooks.csv `callees` and the `kBodies` list of port/src/sim/gfx.cpp).

## What was covered
- Census (re/roadmap/frame_census.tsv, HEAD c6d7d78b): 609 checkpoints, 45 PASS, 525 UNSUPPORTED, 39 NOT_RUN. Contexts: H1WAIT 480, title BOOT 34,
  no-kind 34 (NOT_RUN, no trace), MODAL 31, AFTER_H1 title 9, HEAD 7, location BOOT 5, MODAL_AFTER_H1 frame 5, AFTER_H1 ingame 4.
- All 50 non-PASS rows with context MODAL* / AFTER_H1* / HEAD or kind frame are rows of the TSV. 36 references were viewed (column
  reference_viewed); 14 were not and say which sibling stands for them.
- H1WAIT sample (ui_tour people / reports, cov_a_keys k_files, k_dialogs, k_misc, k_cheats): every one is the course view with the HUD and a
  toolbar, no 2D screen or dialog. Their first unsupported call is Surface_blit3 with an unmodelled source or destination in all 480 rows of
  ui_tour, ui_tour_v, cov_a_keys, cov_a_golfer, cov_a_career: that is HUD / terrain work (S3 and the gfx plumbing), not an S2 family.
  ui_tour's `reports` checkpoint is the build palette with a tool tip, not a report.

## Findings
1. Screens that are NOT in the census as 2D: cov_a_keys k_dlg_L (load browser), k_dlg_R (routing map), cov_a_career career_editor,
   cov_a_golfer button_0, cov_a_keys3 q_editor and cov_k1_routing routing are full-screen 2D (no terrain pixels) but the census marks their
   `terrain` column `yes` (U-1210). The S2 gate "every census checkpoint with no terrain on screen" would skip them. Re-rule them (or add a screen
   kind) before gating S2.
2. Dialogs over the course (popups, SGA report, text input, tournament results strip, bulletin / SimFoto, shot analysis) show terrain behind them:
   the gate for those is the S3 terrain criterion plus 0 px in the dialog's own region.
3. The port's screen functions (commands_screens.inc, commands_menus.inc, commands_popup.inc, commands_panels.inc, chareditor.cpp, reports.cpp,
   routing.cpp, holestats.cpp, dialogs.cpp) issue NO drawing calls: stage 3.7b dropped all of it (header comments of reports.cpp and
   commands_screens.inc). Only saveload.cpp (load browser, course action menu), menu.cpp / tick_menu.inc (title, theme / difficulty flow) and
   tick_frame.inc / tick_panels.inc (mainLoop ports) call drops by name (`gx(...)`, `cc("Image_draw"...)`). So `drawing_callees` in the family rows
   is the ORIGINAL's drawing callee set (hooks.csv callees of the function, filtered to drawing names), which is what each family step must emit.
4. gfx.cpp kBodies (13 names): showTitleScreen, loadImage, Surface_blit, Surface_blit3, Image_draw, setDrawTarget, drawCenteredText,
   drawText4049d0, Window::refreshFull, Surface_fillRegion, Surface_stretchBlit, courseActionMenu.setup, loadInterfaceArt.images, drawPanelBackdrop,
   drawWindow.frame. Missing for the S2 families (callees_missing column): Image_ctor / Image_dtor / Surface_ctor / Surface_free /
   Surface_createFull (surface lifetime), Surface_drawScaled, Surface_drawSprite, Surface_blitThunk, Surface_blitTakeDirty, Surface_drawRegion,
   Text_put, Widget_setQuad, Widget_draw / Widget_draw2, Widget_fillRect, Win_fillRect, fillWindowRect, drawCenteredText2, drawTextSurface,
   drawStringAt / drawStringAt2 / drawStringRightAt / drawText / drawRight / drawTextLinesAt / drawCenteredTextLines, drawPanelS2, drawFrameR3,
   drawFrameS2, drawWindowFrame, drawMessageBuffer, blitPanel, pixelRed / Green / Blue, composeGolferSprite, addSprite, drawThoughtBubble,
   drawRectScaled, drawScrollText, Surface_box, loadBmpSurface. Most used by the most families: Widget_setQuad (reports, roster, tournament,
   buy land, routing, credits), fillWindowRect, drawCenteredText2, Surface_drawSprite, Text_put, drawStringAt / At2.
   "Has a body" is by name only: Image_draw and Surface_blit3 handle the sprite and surface kinds the title, location and world map screens use
   (U-1212); the census cause "Image_draw with a SpriteRef the backend has no sprite for" and "Surface_blit3 unmodelled source" are body gaps
   inside names that are listed.
5. Non-drawing drops the unsupported list names: `Snd::status` (cov_r_quit, cov_k3_career, cov_a_flags, cov_k1_routing, cov_k3_camera2 first
   unsupported). S2.1 adds it to kNotDrawing; those rows then show their next cause.
6. Families whose first screen comes from the title menu (difficulty, theme packs, pick a pro, load browser) are drawn by menu.cpp /
   saveload.cpp functions (`showThemePackSelect`, `pickAProScreen` in menu.cpp; `loadGameBrowserScreen` in saveload.cpp) at C3 (the others C4).

## Families no pinned checkpoint shows, with the closest scenario
ui_tour visits many of these screens but its checkpoints are taken after Esc (menu_done, people, reports, menus), so no frame holds them. Each
needs a checkpoint inserted inside the visit (re/tools/scenario.py ui_tour_lockstep, line numbers at HEAD):
| family | closest pinned scenario and the step |
|---|---|
| theme packs | ui_tour line 517 (Select A Theme), checkpoint before the Esc at 518 |
| difficulty | ui_tour line 521 (Start New Game), before Esc at 522; cov_b_title reaches it too |
| pick a pro / championship browser | ui_tour line 523 (Play a Championship), before Esc at 524 |
| hire employee dialog | ui_tour line 561, before the click at 562 |
| reports F1..F10 (financial, comments, best scores, histograph, hole stats, membership roster, end of year, shortcuts) | ui_tour line 578 loop, a checkpoint after each key's sleep (F6 is the world map: cov_a_keys k_worldmap PASSES) |
| preferences | cov_b_people `_cov_b_prefs` (line 1353): checkpoint after the click at 200,487 |
| system menu (own frame) | cov_r_quit exit_prompt shows it under the popup; a checkpoint before the retirement answer would show it alone |
| scroll text / skill points / golfer info panel | ui_tour skill-point steps (line 567-571) and cov_a_golfer panel_*; the panel there sits in the toolbar, not a modal |
| loading screen, buy land, main radial menu | no scenario step found; showLoadingScreen runs inside boot (tick_boot.inc) between title and course, buy land and the radial menu need new steps |
| bulletin board alone | ui_tour menus / cov_k1_animals bought show the message ball with a SimFoto, not the board's own list |

## Open uncertainty
See `2_uncertain.tsv` (U-1210..U-1214).
