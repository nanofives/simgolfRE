# P9D: HUD twice in modal frames, palette tip placement (2026-11-04)

Tree port/build/P9D (git archive 0ab75c1c). Patch: P9D.patch against 0ab75c1c (applies cleanly, P9D_patch.sh). Files: P9D_files.txt.
Nothing committed or published. Testing cut to the orchestrator's minimal scope (Mariano play-tests).

## Fix 1: HUD drawn twice in modal frames (done)
Cause (layer dumps, `SG_P9D_LAYER_DUMP=1`, P9D_runs/dump_s2*):
- The F-key reports run at the frame's report block (tick_frame.inc ML+0xead5..ML+0xeb56), after the panel layers closed. Their dim
  (Surface_drawSprite of g_spriteRef_821040 at alpha 0.25, drawAlpha's member draw16_1002ed60, floor((P + 3 D) / 4) per channel) had
  no blend record. Every pixel it changed became an opaque pixel of one centred layer (0,5-800,600) that held dimmed copies of the
  panels at their 800-space places, while the panel layers showed undimmed at their anchors. Present 396 (financial report): 89,185
  such copies.
- Every other modal screen (hole stats, best scores, System menu and popups, preferences, hire, skill points, cancel prompt, land
  pages) refreshed when no layer had opened since the last publish, or inside an open panel layer (hire, from
  drawEmployeeDetailPanel). P9B published no layers there, so G32's rectangles moved the dimmed panels and cut the screen's art.
  End of year is drawn before any panel of its frame (the original frame has no HUD), and the rectangles cut it too.
Fix (`--hud-modal on`, the default; off = P9B / P9C's capture; `SG_APP_P9D=off`). The GameState, root and back buffer are unchanged:
- sim (gfx.cpp, gfx_body.h, gfx.h, gfx_reports.cpp, gfx_scene.cpp, gfx_dialogs.cpp, gfx_terrain.cpp): both drawAlpha paths of the
  quarter member record their pixels as layer kind 4. A dim of every root pixel with one word marks its layer `dim`. After each
  layered publish the capture is carried until the next frame's picture (terrain pass, terrain.blit restore, window paint, next
  layer), so a modal screen's refresh publishes the last frame's base and layers plus its own drawing as a centred layer.
  `reports.begin` / `dialogs.begin` inside an open panel layer close that layer there. P9C text runs of the carried layers are
  carried with them.
- app (present.cpp/.h, host.cpp, main.cpp): kind 4 is composed with the quarter rule, and a `dim` layer first dims the whole window
  (world, bands, panels at their anchors). A modal-capture frame without layers (end of year) is shown unmoved instead of
  rectangle-cut. Adds `--selftest-p9d` and control `--negctl p9d-dim-local`.
Checks: x64 and Win32 build 0 errors (sim code changed). Segments x64 205/205, 0 rows differ from I25 (P9D_segs_compare.log).
Already run before the scope cut and kept: --selftest-p9d 7/7 (x64, Win32), its control 3 RED; earlier selftests unchanged; framegate
15 rows equal to P9C's; trace/framehash IDENTICAL 8/8 with the capture off / P9B / P9D (P9D_simeq.tsv); test_static, test_profiles,
test_app_shell, test_app_input, test_app_replay passed (the remaining app files were stopped at the scope cut).
Screenshot (after): P9D_shots/r1920x1080_cov_r_s2_screens_396_after.png (financial report: each panel once, at its anchor, dimmed;
palette label 189 = 248 * 3/4, money meter 255 -> 189). Pairs for every modal screen at both sizes: P9D_shots/*_pair.png (before =
--hud-modal off).

## Fix 2: palette tool tip (no change needed)
The Tropical bush tip is drawn by showInfoCursorPanel's drawing (gfx_hud.cpp b_info_panel, dropped inside the frame's
showInfoCursorPanel call, ML+0xa225), inside the bottom band's layer scope. While it shows, the live layer log grows the bottom band's
layer from 216,482-800,600 to 216,389-800,600 (anchor left). Its frame is at x 640..800 (gfx_text.cpp info_panel_tool_tip: X =
clamp(slot x + 0x20, 0x50, 0x2d0)), right-aligned as the original clamps it. At 1920 x 1080 the tip's right edge and the palette's
right end are both at client x 1440. Centred, the tip would span 1392..1680. So the tip already moves with its panel.
P9D_tips.py hovered 16 HUD controls (radial buttons, palette slots and tools, theme toolbar, amenities rows) at 1920 x 1080 and
1280 x 720. Every tip seen was a pixel of its panel's layer, and no centred layer appeared (result_<size>.json). Both recordings
replay PASS 403/403. Golfer action and employee panel tips are drawn inside their own panels' scopes (code path, not driven).
Screenshot: P9D_tips/tip_palette_slot_top_7_1920x1080.png.

## Left
- Popups (System menu, showPopup, textInputBox) stay at their 800-space place, so the System menu now overlaps the palette.
- The one frame that closes a modal opened inside a panel layer (hire) can show some panels at their 800-space places too.
