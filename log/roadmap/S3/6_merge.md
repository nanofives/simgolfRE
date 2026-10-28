# S3.6 merge request (Widget_writePixel, in-game Surface_blit3, HUD panels)

Proven in the S3.6 build: `log/roadmap/S3/6_mirror.py` builds port/build/S3_6_src from S2's accepted mirror
(port/build/S2_4_head = HEAD c6d7d78b + S2.1 + S2.3 + S2.4) plus S3 wave 1 (S3.1 / S3.1b files, their portsim_c.cpp / .h
edit, S3.3's 3_tick.patch, test_gfx_eoi.py), puts this step's own files in, applies exactly the edits of section 3
(`log/roadmap/S3/6_edits.py`, exact-match anchors, CRLF-aware) and builds into port/build/S3_head/port/build/S3_6 (x64) and
S3_6_w32 (Win32). The gates ran from port/build/S3_6_src with SG_PORTSIM_DLL at that build (log/roadmap/S3/6_notes.md).

Own files (new, copy as they are): see log/roadmap/S3/6_files.txt.

## 1. The pixel family (Widget_writePixel, done first at the lead's request)

Files: port/src/sim/gfx_pixel.cpp, port/include/portsim/sim/gfx_pixel.h, port/test/test_gfx_pixel.py.

port/src/sim/gfx_families.inc, append after the last family line:

```
SG_GFX_FAMILY(pixel)
```

Nothing else: tick.cpp already lists Widget_writePixel / Widget_fillRect as drops ("pixel write", "rectangle fill"), so with
a backend attached TkBase::call hands them to gfx_drop.

## 2. Widget_fillRect when S2.13 is in the tree (U-1356, U-1278)

S2.8's `reports` family (kept by S2.13) and S2.9's `dialogs` also register Widget_fillRect. If either is in the tree, delete
the pixel family's line from `kBodies` in port/src/sim/gfx_pixel.cpp:

```cpp
    {"Widget_fillRect", b_fill_rect_line},
```

(and mark `b_fill_rect_line` / `hline16` / `vline16` / `line16` `[[maybe_unused]]` or delete them). The reports body is the
same jgld line16 with Surface_box's clip; test_gfx_pixel.py already detects `gfx.family.reports` and then expects one pixel
body and runs its fillRect controls on reports mode 5. Without S2.13 nothing changes.

## 3. The hud family

Files: port/src/sim/gfx_hud.cpp, port/include/portsim/sim/gfx_hud.h, port/test/test_gfx_hud.py.

a. port/src/sim/gfx_families.inc, append (before or after `pixel`; order does not matter, no body name is shared):

```
SG_GFX_FAMILY(hud)
```

b. Shared-file edits. The exact old / new text of each is the EDITS list of `log/roadmap/S3/6_edits.py` (apply each with an
exact-match check; gfx.cpp and tick.cpp have CRLF line ends). In order:

| # | file | edit |
|---|---|---|
| 0a | src/sim/gfx.cpp | `load_image` keeps its NULL-name test and calls the new `load_image_named(Gfx&, Ctx&, std::string, ...)`, the same body over a host string (the hud family loads golfer-head paths it builds itself) |
| 0b | src/sim/gfx_body.h | declaration of `load_image_named` |
| 1 | src/sim/gfx.cpp | `#include "portsim/sim/gfx_hud.h"` after the commands.h include |
| 1b | src/sim/gfx.cpp | remove the core `{"loadInterfaceArt.images", b_iart_images},` entry from kBodies (the hud family owns the whole image half; its body runs the core body's loads in the same order) |
| 1c | src/sim/gfx.cpp | mark the now unregistered core `b_iart_images` `[[maybe_unused]]` (the integrator may delete it) |
| 1d | src/sim/gfx.cpp, gfx_body.h | `draw_g16` becomes a call of the new `draw_g16_lut(g, s, d, x, y, lut, lut_n)` with shade_lut(); its range test reads `w >= lut_n` |
| 3 | src/sim/tick.cpp | showMainRadialMenu fallback: the state steps move to `fb_show_main_radial_menu_state(t, a, tip)` (`tip = true` where it copies the label), and the registered fallback calls it, then with a backend attached `gfx_drop(t.ctx, "showMainRadialMenu.draw", {a0, a1, tip}, 3)` |
| 4 | src/sim/gfx.cpp | Window::refreshFull: `gfxh::after_present(g, ctx);` after `g.back.px = g.root.px;` (test-only background fill, nothing without SG_GFX_HUD_BGFILL) |
| 5 | src/sim/menu.cpp | load_world_assets: after `gs.g_flag4c89a8 = 0;` an attached backend gets `gfx_drop(ctx, "loadWorldAssets.images", nullptr, 0)` |
| 6 | src/sim/commands_layout.inc | show_info_cursor_panel: at its start an attached backend gets `gfx_drop(ctx, "showInfoCursorPanel.draw", {x, y}, 2)` |
| 2 | src/sim/gfx.cpp | b_surface_blit3: every case other than an 8-bit image onto the root at 1:1 goes to `gfxh::surface_blit3` first; the core's "a scaled blit" report is gone |
| 7 | src/sim/commands_layout.inc | draw_employee_detail_panel (at its start), draw_employee_view_panel and draw_golfer_action_panel (after their hand-over to the detail panel): an attached backend gets `gfx_drop(ctx, "<name>.draw", {x, y}, 2)` (template DROP in 6_edits.py) |

After modules / families changes touch port/src/sim/registry.cpp and gfx.cpp (MSBuild does not track the .inc files).

## 4. UNCERTAINTIES.md

Append the 10 rows of log/roadmap/S3/6_uncertain.tsv (U-1350..U-1359).

## 5. CLAUDE.md paragraph (after the S3 wave paragraphs)

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
The golfer list and detail panels (drawEmployeeViewPanel / drawEmployeeDetailPanel) are handed over and reported, not drawn. GATE MEASURE: the
port draws no terrain or scene yet, so `log/roadmap/S3/6_hudgate.py` runs framegate twice with the test-only root fill
SG_GFX_HUD_BGFILL=0x0000 / 0x7fff after every present; a pixel equal in both was drawn opaquely and is compared with the
reference per HUD region. Hover tips are reported as `tooltip:` (S3.7 / S3.8) and the sprite queue's draws as `addSprite:`
(S3.5).
