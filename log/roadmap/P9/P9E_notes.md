# P9E: the open layer-compositor fixes, one at a time (2026-11-04)

Tree port/build/P9E (git archive cde8b72d + junctions, P9E_mirror.py). Brief: P9E_BRIEF.md. Patches: P9E_<n>.patch against the tree after
fix n - 1 (P9E_1 against cde8b72d), made by P9E_patch.sh (files list and an apply check in P9E_<n>_files.txt; snapshots in P9E_snap/).
Light testing (the brief): builds, one smoke run with a 1920 x 1080 screenshot per fix, segments x64 when sim code changed
(P9E_gates.sh -> P9E_<n>_gates.log). Nothing committed or published.

Status: fix 1 done (P9E_1.patch), fix 2 done from the code path, not reproduced live (P9E_2.patch), fix 3 done (P9E_3.patch),
fix 4: Mariano chose option B (after fixes 1-3 merged at 94c204dc); done in P9E_4.patch against 94c204dc's port/ (section
"Fix 4, option B" at the end). P9E_4_draft.patch is the earlier, discarded draft.
Options added (all off with SG_APP_P9E=off, set in port/test/conftest.py): --hud-popups, --hud-close, --hud-vacated, --hud-cursor; play switch
mainLoop.vacatedPanelClick (0 in kPlayProfile and re/port/play_profile.tsv). The tree port/build/P9E is at fix 4, built x64 + Win32.

## Fix 1: popups at their panel's place
Cause: showPopup / textInputBox (the System menu is showSystemMenu's showPopup(0xfa, 0x154), commands_menus.inc system_popup) draw
into the root at 800-space places. With P9D's carry their drawing is the centred gap layer of the carried frame, composed at the
frame's place, while the radial menu they belong to is composed at the left edge: at 1920 x 1080 the System popup sat 240 client px
right of the radial menu, over the palette's old place (P9E_shots/r1920x1080_cov_r_quit_248_before.png).
Fix (`--hud-popups on`, the default with --hud-modal; off = P9D; SG_APP_P9E=off, set in port/test/conftest.py):
- sim, records only (gfx.h gfx_set_hud_popups / gfx_hud_popup_begin / _end; gfx.cpp, gfx_body.h, gfx_terrain.cpp,
  commands_popup.inc DialogMark): every showPopup / textInputBox body is a PIECE. Its opener is (1) the panel layer open when it
  starts (a popup a panel's own code opens), else for a click (g_rootInput's button word set) (2) the piece that owns the clicked
  pixel (a popup opened from a popup goes with it: Rename Your Course's textInputBox is drawn over the System popup by design), (3)
  nothing when the clicked pixel was drawn since the last boundary outside every popup (a modal screen, a label), (4) the topmost
  panel layer with a pixel at the click (the System button -> the bottom band). A key-opened popup has no click (the quit popup
  after the System menu's Exit: mainLoop clears the button word at ML+0xee78 before it), so it stays centred. A per-pixel owner
  plane (the piece open when the root pixel last changed, observed at each piece boundary and publish) is published with the
  frame's pieces and their boxes. The pieces end at the next frame's picture (terrain pass, its restore, window paint). The frame,
  the root, the back buffer and the GameState are not touched, and no drawing call is added, so with capture off nothing changes.
- app (present.h / present.cpp, main.cpp): in a centred gap layer that carries popups, the pixels a piece with an opener owns are
  composed at the opener's anchor offset (left -origin_x, right + the right band), clamped so the piece's box stays in the window,
  and are their own remap layer at that offset (clicks and hovers inside the moved popup reach it at its 800-space place). P9C runs
  of such a layer move with the piece that owns their box's centre.
Checks (P9E_1_gates.log): build x64 and Win32 0 errors 0 warnings; segments x64 205 / 205 PASS, 0 rows differ from
log/roadmap/merges/I25/segs_x64.tsv (P9E_1_segs_compare.log). Smoke: live-trace of cov_r_quit (the app, 1920 x 1080,
`fixed`, P9E_shots.py; before = --hud-popups off), 735 presents, rc 0, stop quit, both modes.
Screenshot: P9E_shots/r1920x1080_cov_r_quit_248_after.png (the System popup next to the radial menu at the left edge, as in the
original's frame, its text redrawn with it; the quit popup above it, key-opened, centred), _pair.png = before / after.
_389: the save-name box opened from the quit popup stays centred, the System popup's leftover stays with the radial menu.

## Fix 2: the frame that closes the hire dialog
Cause (code path; gfx.cpp hud_publish / gfx_layer_begin, commands_layout.inc draw_employee_detail_panel): the employee detail
panel's own drawing calls hireEmployee (once per opening, g_ui_59ca54 == 0: its action 0 / 1 clears it, commands_panels.inc
employee_action), so the hire dialog runs INSIDE the panel layer's scope. P9D closes that layer at the screen's first call and
publishes the dialog's frames over the frame's layers (split, then carried). After the dialog returns, the same frame goes on, and
the next panel layer it opens ended the carry (P9D: "the next frame's first layer ends a carried capture") and took the root as the
frame's base: the dimmed root with the dialog over it and the panels drawn before the dialog at their 800-space places. That
frame's present showed those panels once more at their old places.
Fix (`--hud-close on`, the default with --hud-modal; off = P9D; SG_APP_P9E=off): gfx.h gfx_set_hud_close. A publish made inside an
open panel scope that arms the carry marks it (HudCapture::carry_split); the next gfx_layer_begin of that frame continues the carried
capture instead of ending it: the gap since the screen's last publish is closed as a centred layer and the frame's later layers are
added after it, so the close frame is published like the screen's own frames (the frame's base, its layers before the dialog at
their anchors, the dialog's drawing centred, the later layers at their anchors). The next frame's picture (terrain pass, restore,
paint) or a publish outside every scope ends it as before. Records only; files gfx.h, gfx_body.h, gfx.cpp, main.cpp.
Dev aid: SG_P9E_PRESENT_LOG=<file> (main.cpp, live-trace) writes one line per present: index, HUD serial, carried, none, published
with this back buffer, id:anchor:dim per layer.
Not reproduced live: no recording I ran reaches the split hire. cov_r_s2_screens' hire (presents 1034..1055) is opened by mainLoop
(ML+0xa373, g_msgState53df54 on a type 0xff walker), outside every layer, so it is carried and its close frame 1056 already composes
like 1058. cov_b_holes18, cov_b_people and cov_a_career show no dim frame. A live drive on the Hawaii sandbox (P9E_drive.py --seq
hire, one hole built first) opened the employees panel, but its slot click opened no hire dialog there. Inert check: present logs
with --hud-close on and off are identical on cov_r_s2_screens (1,802 presents) and cov_b_holes18 (4,764), and the 1920 x 1080
shots of presents 1055 / 1056 are pixel-identical (P9E_shots/fix2_r1920x1080_cov_r_s2_screens_105{5,6}_pair.png).
Checks (P9E_2_gates.log): build x64 and Win32 0 errors 0 warnings; segments x64 205 / 205 PASS, 0 rows differ from I25.

## Fix 3: a click where a moved panel used to be acts on the course
Cause: with the HUD moved to the window's edges, the course shows in the panels' 800-space places. A press there reaches the game
at that 800-space point (no layer covers it, so the remap leaves it), and mainLoop's click dispatch tests rectangles of the 800 x 600
frame: below y 0x1b8 a press at x < 0xc8 is the System menu / radial menu (showSystemMenu's nearestMenuSpot), below y 0x1e0 it goes
to the panel handlers of g_panelListMode (ML+0xee40..ML+0xeec5). So it acted on the panel that is no longer there (live, 1920 x
1080: a click on the course at the Buildings button's old place opened the Buildings palette).
Why not app-only: the world point under such a click IS its 800-space point (the world layer is aligned with the frame), so sending
the press anywhere else moves it to another tile. The sim alone cannot tell it either: a remapped press onto the panel's real place
and a vacated one arrive at the same 800-space point. So the app marks it, and one `play` switch in the dispatch reads the mark.
That is the smallest correct change. The hit tests themselves (nearestMenuSpot, hitTestSlot432f90, ...) are untouched and see
only real panel presses.
Fix (default on; `--hud-vacated off` or SG_APP_P9E=off = P9D; the sim side is the play switch mainLoop.vacatedPanelClick, 1 = the
original, 0 in kPlayProfile and re/port/play_profile.tsv, so `fixed` and every test are unaffected):
- app (present.h / present.cpp PanelRemap::map(x, y, vacated), window.h / window.cpp InputEvent::vacated, host.h / host.cpp): a
  mouse button message is VACATED when no moved layer has a pixel under its client point (a centred pixel there, a golfer's label,
  counts as vacated too: in the 800-space frame it was over the panel) and its 800-space point lies in a moved panel layer's box
  (without layers: a moved kHudPanels rectangle). When the live session delivers a press (onPress1 / onPress2) it writes the view
  block's word +4 (bridge.h kPortViewVacatedVa) = 1 for a vacated press, 0 for any other, as a recorded harness write (DELIVER
  w32) only when it changes, before the batch is applied, so a replay applies it at the same point. session.json `p9e`
  {vacated_clicks, vacated_writes}.
- sim (bridge.h / bridge.cpp view_click_vacated, tick.cpp, tick_frame.inc at ML+0xee40): with the switch at 0 and the word set,
  the press takes the world path a41e4b8 (ML+0xeef8), as a press above y 0x1b8 does; screenToTile's tile (ML+0xee20) is the
  press's own. The word is 0 in every recording made before (the view block is zero-filled), so nothing changes there.
Checks (P9E_3_gates.log): build x64 and Win32 0 errors 0 warnings; segments x64 205 / 205 PASS, 0 rows differ from I25;
port/test/test_profiles.py 21 passed (the new profile row; P9E_3_test_profiles.log).
Replay: both live recordings (on / off) replay through sg_run_boot (app_replay.gate_one, P9E_gate_rec.py) PASS 151 / 151
under `play` (P9E_3_replay.log): the vacated word is a recorded write the replay applies at the same point. Smoke (P9E_drive.py --seq vacated, the live app,
1920 x 1080, `--profile play`, Hawaii; off = --hud-vacated off): with the radial menu at the left edge (client 0..360) its old
place (240..600) shows the course; a press at the Buildings button's centred point (800-space (118, 495), client (452, 891))
opened the Buildings palette before and does nothing to the HUD after; the People button's old place likewise; the Buildings
button at its anchored place still opens the palette (vacated_writes 2: 1 at the first vacated press, 0 at the anchored one).
Screenshot: P9E_live/vacated_1920x1080_buildings_pair.png (top before: the palette opened by a click on the course; bottom after:
no palette), the step shots vacated_1920x1080_{off,on}_*.png.

## Fix 4: the tool preview at a moved panel's old place (NOT FIXED: needs a decision, options below)
What shows there (live app, 1920 x 1080, P9E_drive.py --seq cursor, Hawaii, `fixed`): with a build tool held and the cursor over a
moved panel, the game takes the remapped 800-space point (the panel's own place) as the cursor and draws the tool's preview at the
tile under it, so the preview sits at the panel's OLD place. Shots: P9E_live/cursor_1920x1080_palm_off_05_over_title.png (the grey
placement tile at the title's old place, cursor over the title at the left edge) and cursor_1920x1080_green_{off,on}_07_over_title
.png / _08_over_meters.png (with a tee placed and the green tool: the green and flag preview at the title's / meters' old place, the
line to the tee and its "340 yards" label). Over the palette (y >= 0x1e0) nothing is drawn: P7 already skips its previews when the
cursor's y is at or below 0x1e0 (ML+0xa774 placement box, ML+0xb3d0 hovered-cell tooltip; edi = 0x1e0).
Why the brief's fix does not reach it: P9B section 6 took these previews for centred 2D layers. They are WORLD sprites. P7 queues
them with addSprite (placement box, object sprites, the flag preview) and the terrain pass draws them into the world picture, so
they are part of the world layer under the frame, not a layer the compositor can move. I built the brief's version (a "cursor
piece": the 2D pixels drawn by P6_ToolLabel and P7 outside every panel layer, recorded with fix 1's owner plane, composed at the
offset of the layer of the last cursor answer, only the clusters joined to the cursor's 800-space point). In every case above it
changed nothing: on and off gave the same picture. It is NOT in the tree and NOT in any P9E_<n>.patch. It is kept as
P9E_4_draft.patch (against the fix 3 tree, applies cleanly), P9E_edit4.py.
Options for Mariano:
A. (recommended, small) Treat "cursor over a moved panel" as "cursor over the HUD", the original's own rule for the bottom band.
   The host writes the view block's word +8 = 1 at each frame head while the last cursor answer was remapped by a moved panel
   layer, as a recorded harness write like G32's width (PanelRemap already knows it). A `play` switch (1 = original, 0 in
   kPlayProfile) makes P7's two "inside the course" tests (ML+0xa774, ML+0xb3d0) fail then, so no preview is drawn while the
   cursor is over a panel, as over the palette. Effect: the preview disappears instead of sitting at the old place. Sim + app, like
   fix 3. Over the translucent title strip the original shows the preview faintly through the strip; with A it is gone.
B. (large) Move the preview with the panel. Tag the sprites P7 / P6_ToolLabel queue (addSprite between their start and end) in the
   backend's sprite queue, have the terrain pass record their pixels as their own layer (and the world under them without them),
   and compose that layer at the offset of the cursor's layer. That is the brief's "anchor to the cursor's layer" for world sprites.
   It touches the terrain renderer and P8B's world_frame. The preview would then sit under the moved panel, mostly hidden by it, as in
   the original.
C. Leave it: the preview at the old place shows which tile the game takes as the cursor tile, which is where the click would build
   if it went to the world (it does not: the press goes to the panel).

## Fix 4, option B (Mariano's choice, 2026-11-04): the tool preview moves with the panel the cursor is over
What was measured first (live app, 1920 x 1080, P9E_drive.py --seq cursor --tee --tool 365,527: a tee placed, the green tool held;
replay with layer dumps in P9E_runs/green_lt): P7 draws its previews right after the sprite queue, through addSprite ->
drawQueuedSprite.draw (the flag / green sprites) and Surface_drawSprite (the translucent placement box), so they exist twice: in the
800 frame as pixels of a centred gap layer (layer 4 of present 186, box 303,94-800,256, with the line to the tee and its "340
yards" label), and, for the sprites, in G32's wide scene, from which the world picture is made. With the cursor over a moved panel's
pixel (the title) the remap answers the panel's 800-space point, so both copies sit at the panel's old place. Over a transparent
pixel of a panel (the meters' gaps, the radial menu's edge) the point is not remapped and the preview is already at the cursor.
Over the palette itself (800-space y >= 0x1e0) P7 draws no preview (ML+0xa774, ML+0xb3d0), so there is nothing to move there.
Why the earlier draft changed nothing: it took the last moved answer from every PanelRemap::map call, and the translator's own
cursor reads (window.cpp LiveHost::cursor, the real system cursor) reset it; it also left the world copy in place.
Fix (`--hud-cursor on`, the default with the HUD layers; off = P9E fix 3's picture; SG_APP_P9E=off). Records only on the sim side;
the frame, the root, the back buffer and the GameState are unchanged, so segments are unaffected:
- sim (gfx.h gfx_set_hud_cursor / GfxCursorScope, gfx_body.h, gfx.cpp gfx_drop, gfx_scene.cpp, terrain_live.h / .cpp, tick.cpp,
  tick_frame.inc): tick.cpp brackets P6_ToolLabel and P7 with GfxCursorScope. Inside it the backend keeps the wide scene at the
  first pass's start and the last pass's end (LiveTerrainWorld::scene_pre / scene_cur; cleared at the queue's start), and marks
  the root pixels changed by each Surface_drawSprite / drawQueuedSprite.draw call (HudCapture::cursor2d, published as
  GfxHudFrame::cursor2d). LiveTerrain::world_frame(..., cursor_apart) lays the scene without those sprites (scene_pre where the two
  differ), and LiveTerrain::world_cursor gives the sprites alone at world resolution plus their 800 x 600 mask.
- app (host.cpp / host.h, window.cpp / window.h, present.cpp / present.h, main.cpp): LiveSession hands every cursor answer the
  sim gets to the remap (PanelRemap::set_cursor_answer: the client offset the remap moved it by, from the system cursor or from
  the mouse message's InputEvent::remap_dx). make_world_image takes the previews out of the world picture (WorldImage::cur_*). The
  presenter leaves the frame's copy of them out of the centred layers. It draws the world-resolution sprites, and the frame's 2D
  sprite pixels (cursor2d), at the offset of the cursor answer's layer, in the frame's draw order (where its copy of them was). With
  the cursor over the course the offset is 0 and the picture is the same as before, with the sprites at world resolution.
Not moved: the line from the tee to the cursor and its distance label (lineTo / drawCenteredText, 2D). A rigid move would take
the line's tee end off its tee, so they stay where the frame draws them.
Checks (P9E_4_gates.log): build x64 and Win32 0 errors 0 warnings; segments x64 205 / 205 PASS, 0 rows differ from I25
(P9E_4_segs_compare.log). P9E_4.patch applies to a clean export of
94c204dc and gives the tree (P9E_4_files.txt).
Smoke (live app, 1920 x 1080, `fixed`, Hawaii; before = --hud-cursor off): P9E_live/cursor_1920x1080_07_over_title_pair.png.
Before: the green and flag preview at the title's old place (client ~540, 60). After: they sit on the title panel at the cursor and
the old place shows the course; the tee line still ends at the old place. cursor_1920x1080_10_over_palette_top_pair.png: the
cursor over the palette's top edge, where it is not over a palette pixel. The preview is at the cursor before and after.
