# S3.5 merge: scene sprites (body family `scene`)

Own files (log/roadmap/S3/5_files.txt): port/src/sim/gfx_scene.cpp, port/include/portsim/sim/gfx_scene.h,
port/test/test_gfx_scene.py (copy as they are). Base the step was built and measured on (log/roadmap/S3/5_mirror.py):
port/build/S3_6_src (S2 waves 1-4 + S3 wave 1 + S3.6 with its 6_edits.py), plus S3.2's own files and its 2_merge_edits.py,
plus S3 wave 2's port files (S3.4 / S3.4b) with MERGE_w2.md edits 1 and 2a..2c. The shared-file edits below are the exact
(old, new) pairs of log/roadmap/S3/5_merge_edits.py, which 5_mirror.py applies with an exact-match check (each anchor once;
CRLF files get CRLF). Apply after S3.6's merge (edits 2c, 2e and 3 anchor on S3.6's lines) and after S3 waves 2 and 3.

## 1. Shared-file edits, in apply order

1. port/src/sim/gfx_families.inc: `SG_GFX_FAMILY(scene)` after `SG_GFX_FAMILY(terrain)`.
2. port/src/sim/tick_render.inc, drawQueuedSprite: the old body is renamed drawQueuedSpriteSim; the new drawQueuedSprite hands
   the entry to an attached backend before the simulation part (`drawQueuedSprite.draw`: the golfer palette layers read the
   look indices before the simulation part writes them back, as the original's compositor does) and after it
   (`drawQueuedSprite.label`: the name text the simulation part built). Nothing changes without a backend.
3. port/src/sim/tick_render.inc, renderSpriteQueue: after the loop, `gfx_drop(ctx, "renderSpriteQueue.end", nullptr, 0)` with a
   backend attached (the sprite-pixel gate's capture point; the body only counts and, when a test asked, copies the root).
4. port/src/sim/tick_frame.inc, P6_DisplaySlot9c: with a backend attached, `gfx_drop(ctx, "Display::setScale", {1, 1, 1}, 3)`
   (the original's display-scale slot call; the scene draws set (n, n, d) and restore (zoom, zoom, 4), and the HUD after P6
   draws at (1, 1, 1) as in the original).
5. port/src/sim/menu.cpp, load_world_assets: after S3.6's `loadWorldAssets.images` line, with a backend attached,
   `gfx_drop(ctx, "loadWorldAssets.palettes", nullptr, 0)` (the palette objects the course type's loads fill; the sprite queue
   draws with them).
6. port/src/sim/tick.cpp: `#include "portsim/execonst.h"` after `#include "portsim/sim/gfx.h"` (edit 7 reads exe constants).
7. port/src/sim/tick_scene.inc (buildSceneSprites): Surface_drawSprite's alpha reaches the backend as its float bits (the
   `scale` lambda and the kind-4 `s` computed from f_k_dbl_4ba818 / f_k_dbl_4ba478 through ctx.exe() with a backend attached,
   0.0f without one as before; the `sprite` lambda passes the bits, std::memcpy). Before this edit the alpha arrived as
   static_cast<uint32_t>(0.0f) = 0.
8. port/src/sim/gfx.cpp: `#include "portsim/sim/gfx_scene.h"`; b_image_draw's scale check (`a display scale other than (1, 1, 1)`)
   is replaced by `if (gfxs::image_draw(g, ctx, a)) return;` (a scale other than (1, 1, 1), or a sprite whose jgld palette word
   the scene family holds, is drawn by the scene family's drawB under the display scale; the title-menu path and gfx.cpp's
   controls are unchanged).
9. NEED from S3.6 (port/include/portsim/sim/gfx_hud.h, port/src/sim/gfx_hud.cpp): two exports of the hud family's state,
   `std::array<uint16_t, 256> *gfxh::palette_of(Gfx &, uint32_t key, bool create)` (the palette objects loadImage filled;
   create = an all-zero entry) and `const std::array<uint16_t, 0x10000> &gfxh::half_bright_table()`, plus `#include <array>`
   in the header. S3.6 owns these files: if its final version has other names, rename the three call sites in gfx_scene.cpp.

10. After S2 wave 5 (5_merge_edits.py EDITS_AFTER_S2W5; the orchestrator's decision: Surface_drawSprite has one body,
   gfx_reports.cpp's): `#include "portsim/sim/gfx_scene.h"` in gfx_reports.cpp, and in its b_draw_sprite, after the load-arm
   guard, a hand-over: on the root surface, a mask (a[5] != 0), an alpha in [0, 1] outside the quarter band, or a display
   scale other than (1, 1, 1) is drawn by `gfxs::surface_draw_sprite(g, ctx, a)` and the body returns. The quarter band at
   (1, 1, 1) with no mask stays S2's own code (draw16_1002ed60 = quarter_blend), with S2's clip and controls. gfx_scene.cpp
   no longer registers the name; its queue 0x200 arm calls the registered body (call_body).
11. After S3.6's merge (5_merge_edits.py EDITS_HUD, S3.6's gfx_hud.cpp; the lead: these figures are S3.5's, U-1354):
   `#include "portsim/sim/gfx_scene.h"`; showInfoCursorPanel's hole flag drawn through cellAt (registered, read only) and
   `gfxs::add_sprite` with the original's arguments (0x004337b5..0x00433820: sheet g_courseType + 0x189, column g_date %
   its columns when the tool f_w_4c2848 is 1, row 3; addSprite(point 1 + (0x1f, 0x18), 0, 0x28, 0x28, cell, 4, 0x63)); and
   amenity_row with drawAmenityRow's eleven arguments (c3ei.cpp DrawAmenitiesPanel_re: the five rows' p4 / p5 / p6 / p9 /
   p11), its ids, cells, both addSprite calls and the p2 / p5 steps (DrawAmenityRow_re), then Display::setScale(1, 1, 1).
   The two `addSprite: the sprite queue (...)` reports are gone. If S3.6's final gfx_hud.cpp differs from port/build/S3_6_src's,
   the anchors (old text) are in EDITS_HUD.

12. NOTE for the integrator (not an S3.5 edit): with S3.6's gfx_pixel.cpp and S2 wave 5's gfx_reports.cpp in one tree,
   Widget_fillRect is registered twice (gfx.check reports `duplicate Widget_fillRect pixel reports`; port/test/test_gfx.py
   test_family_table_registers_selftest fails). One of the two registrations must go, as for Surface_drawSprite.

## 2. Bodies other steps call (S2.10 asked for them; taken here)

- `composeGolferSprite` {g}: the palette layers into g_images[0] (a screen that composes a golfer outside the queue).
- `addSprite` {x, y, key, w, h, cell, shade, flags}: gfxs::add_sprite (no queue entry, no golfer label).
- `Image_draw`'s mask: gfxs::image_draw takes a[4] != 0 as drawB's key for the call (any scale; c3df ImageDraw_re passes
  mask->m_4, jgld Sprite::drawB sets and restores the palette word).

## 3. UNCERTAINTIES.md

Append the rows of log/roadmap/S3/5_uncertain.tsv (U-1340 .. U-1348) and mark U-1332 resolved with the text in 5_uncertain.tsv.

## 4. CLAUDE.md paragraph (after the S3 wave paragraphs)

See log/roadmap/S3/5_notes.md section 8 (the paragraph, ready to paste; section 10 adds round 2).

## 5. Gate (numbers in 5_notes.md)

```
~/bin/py -3.12 log/roadmap/S3/5_mirror.py --w32
cd port/build/S3_5_root && SG_PORTSIM_DLL=<x64 dll> ~/bin/py -3.12 -m pytest port/test/test_gfx_scene.py
SG_PORTSIM_DLL=<x64 dll> ~/bin/py -3.12 ../../../log/roadmap/S3/5_sprite_gate.py --all --jobs 2 --out <tsv>
```
