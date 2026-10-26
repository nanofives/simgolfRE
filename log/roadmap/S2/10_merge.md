# S2.10 merge request (character editor, skill-point dialog, buy land, syncTiles)

Base: port/build/S2_13_head (HEAD 2c05f02a + S2 waves 1-5). The step's mirror is port/build/S2_10_head (a copy of S2_13_head with the
junctions of log/roadmap/S0/export.md); port/build/S2_10_base holds the pre-step copies of port/src, port/include, port/test and
re/tools (the patch base). Notes: log/roadmap/S2/10_notes.md. Files: log/roadmap/S2/10_files.txt.

## 1. New files (copy as they are)

| file | what |
|---|---|
| port/src/sim/buyland.cpp | module `buyland`: showBuyLandScreen (sim + the original's drawing calls), syncTiles |
| port/src/sim/gfx_buyland.cpp | body family `buyland`: drawTextLinesAt, drawRight |
| port/src/sim/gfx_chareditor.cpp | body family `chareditor`: showCharacterEditor.setup / .background / .teardown / .bio, drawMessageBuffer |
| port/test/test_buyland.py | the emulator oracle of showBuyLandScreen, golden digests, 8 + 3 controls |
| port/test/golden_buyland.json | its pinned digests (pinned only after the oracle was GREEN, SG_EMU_ORACLE=update) |
| port/test/test_gfx_s2_10.py | the frames (editor x4, skill dialog, buy land) and the 7 drawing controls; skips the S3-dependent ones in a tree without the `hud` / `scene` families |

## 2. Shared-file edits: log/roadmap/S2/10_shared.patch (8 files, `git apply` from the tree root)

| file | edit |
|---|---|
| port/src/sim/modules.inc | `SG_MODULE(buyland)` appended (touch port/src/registry.cpp after it: MSBuild does not track the .inc) |
| port/src/sim/gfx_families.inc | `SG_GFX_FAMILY(buyland)` and `SG_GFX_FAMILY(chareditor)` appended (touch gfx.cpp) |
| port/src/sim/tick.cpp | kDrops: the `syncTiles` line removed (buyland.cpp registers it; its g_terrain820f2a byte is a simulation-visible write, 10_notes.md section 3) |
| port/src/sim/chareditor.cpp | the editor's drawing calls (struct EdDraw), every one under `g_gfx_attached` (generator: log/roadmap/S2/10_tools/edit_chareditor.py, exact anchors) |
| port/src/sim/dialogs.cpp | the skill-point dialog's and drawThoughtBubble's drawing calls (struct SkDraw; thought_bubble gains x, y, draw with defaults that keep the other callers as they were), showCharacterIntro's drawWindow.frame (generator: 10_tools/edit_dialogs.py) |
| port/src/sim/gfx.cpp | drawCenteredText / drawText4049d0 first run the Widget_setQuad body (set_quad_root), as their originals do; negctl mode 7 (U-1293) |
| port/src/sim/gfx_prim_text2.cpp | drawCenteredText2 first runs Widget_setQuad(colour, 1, 0, 1); negctl mode 3 |
| port/test/test_s2_6_scenario.py | the boot now runs to the end: test_boot_stops_only_at_buy_land replaced by test_boot_runs_to_the_end (1268 / 1268), docstring |

## 3. S3.5's part (taken by S3.5 into its own gfx_scene.cpp; nothing of mine goes into that file)

The editor's figure, the skill dialog's bubble and the buy-land sprites need three things of the `scene` family, which S3.5 added
on request (port/build/S3_5_root/port/src/sim/gfx_scene.cpp as of 2026-10-25 05:09, with its own controls 10 / 11 / 12):
1. gfxs::image_draw takes Image_draw's mask argument as the palette object for the call (c3df.cpp ImageDraw_re passes
   mask->m_4 to Sprite::drawB, which sets m_10 for the call and restores it, re/match/jgld_sprite.cpp);
2. a body `composeGolferSprite` (the palette layers into g_images[0]); 3. a body `addSprite` (gfxs::add_sprite).
The S2.10 frames that need them (test_gfx_s2_10.py) skip in a tree without the `hud` / `scene` families. Merge S3.5 / S3.6 and
this step in either order; the frames are measured on port/build/S2_10_s3 (S2_10_head + S3_5_root merged, S3.5's file as is).

## 4. Order and checks

- BASE: HEAD b3d61e41 (S2 waves 1-4 + S3 wave 1) + 13_shared.patch (wave 5; its new files as listed in 13_files.txt). Checked
  on a `git archive b3d61e41 port` with 13_shared.patch applied: 10_shared.patch applies cleanly, and the 8 files then equal the
  step mirror's except tick.cpp's S3.3 U-1120 lines, which b3d61e41 has and the mirror (S2_13_head, based on 2c05f02a) does not.
  If the wave-5 commit lands first, apply on it the same way.
- `gfx.check` must report 0 duplicate bodies: drawTextLinesAt, drawRight, drawMessageBuffer and the showCharacterEditor.* names
  are new over S2 waves 1-5 and S3.5 / S3.6 as of 2026-10-25 (S2.11 runs beside this step: check its families).
- Backend keys: gfx_local_key(0x7a000) (the editor's frame Image).
