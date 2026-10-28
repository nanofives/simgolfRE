# S3.7 merge request (marked-up HUD text, the ticker window, the hover tips; U-1069 / U-1072 / U-1353)

Proven in the S3.7 build: `log/roadmap/S3/7_mirror.py` makes port/build/S3_7_src from the I6 integrator's tree (port/build/I6:
HEAD b3d61e41 + S6 wave 2 + S3 waves 2 / 3 + S2 wave 5, frozen once into S3_7_src/_i6port, sha256 in S3_7_src/I6_SNAPSHOT),
S3.6 / S3.6b's files and edits (6_edits.py, 6b_edits.py; the files frozen from the working tree into S3_7_src/_s36), S3.5's
files and its 5_merge_edits.py (EDITS, EDITS_AFTER_S2W5, EDITS_HUD), then this step's own files and the edits of section 3
(`log/roadmap/S3/7_edits.py`, exact-match anchors, CRLF-aware) and builds port/build/S3_head2/port/build/S3_7 (x64) and S3_7_w32
(Win32). Apply AFTER S3.5's and S3.6's merges (edit 5 anchors on S3.6's gfx_hud.cpp lines, edit 1 on S3.5's gfx_scene.h include
in gfx.cpp). If S3.6b's final gfx_hud.cpp differs from the frozen copy, the anchors (old text) of edits 5 are in 7_edits.py.

## 1. Own files (new, copy as they are)

port/src/sim/gfx_text.cpp, port/include/portsim/sim/gfx_text.h, port/test/test_gfx_text.py (list: 7_files.txt).

## 2. Family line

port/src/sim/gfx_families.inc, after the last family line (any place: no body name is shared with another family once edit 3
below removes the core's drawWindow.frame):

```
SG_GFX_FAMILY(text)
```

Touch port/src/registry.cpp and port/src/sim/gfx.cpp after it (MSBuild does not track the .inc files).

## 3. Shared-file edits (exact old / new text: the EDITS list of log/roadmap/S3/7_edits.py, in this order)

| # | file | edit |
|---|---|---|
| 1 | src/sim/gfx.cpp | `#include "portsim/sim/gfx_text.h"` after the gfx_scene.h include |
| 2 | src/sim/gfx.cpp | gfxb::draw_text_string: a text holding `{ } [ ] $ = ^` or a tab, or a colour without bit 31 (a run-palette index), goes to `gfxt::draw_string` (drawRichText's markup scanner, the run drawer's tabs and palette colours) instead of being reported; plain text keeps the core path unchanged |
| 3 | src/sim/gfx.cpp | the core's `{"drawWindow.frame", b_draw_window}` registration is removed (the text family registers the whole drawWindow); `b_draw_window` marked `[[maybe_unused]]` (the integrator may delete it and draw_message_buffer if nothing else calls them) |
| 4 | src/sim/commands_internal.h, commands.cpp, commands_panels.inc | `draw_window_effect(ctx, mode, x = 0xf0, y = -8)`: the height is computed inline (the same writes as window_height: g_value587ea0 = 15 r + 0x20, at least 0x60 with a golfer) and, with a backend attached, `gfx_drop(ctx, "drawWindow.frame", {x, y, 0, mode, h}, 5)` after the wrap; the two help-window callers (clickBuildTool433d30 / pickBuilding, -3) pass (0xc8, 0x154). This is what draws the ticker (tickMessage) at all: before it the in-game message window was never handed to the backend |
| 5 | src/sim/gfx_hud.cpp (S3.6's) | the hover tips S3.6 reported are drawn: showInfoCursorPanel's tool tip and slot tip (`gfxt::info_panel_tool_tip / _slot_tip`), drawAmenitiesPanel's dwell tip and mode-4 building row, drawGolferActionPanel's tip, the employee panels' tips (dwell() takes the tip as a callback), drawTerrainToolbar's tip with the shared dwell counter (a helper `toolbar_dwell` defined after InfoPanelState, declared before b_terrain_toolbar); `#include "portsim/sim/gfx_text.h"` |
| 6 | src/sim/gfx_prim_text.cpp | Widget_setQuad's words also reach `gfxt::set_quad` (the text family's drawStringRightAt draws in the root's state-0 colours); `#include "portsim/sim/gfx_text.h"` |
| 7 | src/sim/gfx_body.h, include/portsim/sim/gfx.h, src/sim/gfx.cpp, src/sim/tick.cpp | a measuring drop: Gfx gets `has_value / value`, gfx.h / gfx.cpp `bool gfx_take_value(Ctx &, int32_t &)`, and tick.cpp's drop case returns the value a body set (Text_put's width) when a backend is attached; without one the frame keeps the drop's 0 as before. The frame code uses Text_put only to place the thought labels' drawMapLine bars and bubbles (drawing only) |

## 4. UNCERTAINTIES.md

- Append the rows of log/roadmap/S3/7_uncertain.tsv (U-1360 .. U-1368).
- U-1069: status `partial`, append: "Roadmap S3.7: the cursor image and the tooltip are settled for the frames (U-1360: the image
  cursor exists only over an EditBox, the tooltip text only through Tooltip::show; no pinned in-game checkpoint shows either);
  whether g_blitSaveBuf is the presented surface stays open."
- U-1072: status `resolved`, append: "Roadmap S3.7: in-game, drawMessageBuffer's texts (drawWindow's ticker and help windows,
  the tool tip's description) never take drawTextSurface's shadow in the pinned frames (line height not above 16 for
  g_font51b360 / g_fontRef_519fd8); the shadowed texts that do reach a frame (drawCenteredText2, drawTextSurface) use run-palette
  entry 1, measured by S2.4 (U-1230) and equal in every frame that holds one (the routing map, the boot loading screens, the
  dialogs); no pinned frame reaches another index (the census reports none). The palette objects themselves: U-1361."
- U-1353: status `resolved`, append: "Roadmap S3.7: every tip is drawn (gfx_text.cpp, through gfx_hud.cpp's edits 5); each
  equals the original's calls under the emulator oracle (test_gfx_text.py: 60 cases per tip kind) and the tool tip with its
  theme text equals the reference frame (cov_a_flags zdemo); their drawMapLine bars are S3.8's (U-1367)."

## 5. CLAUDE.md paragraph (after the S3 wave paragraphs)

See log/roadmap/S3/7_notes.md section 9 (ready to paste).

## 6. Note for the integrator (not an S3.7 edit)

In this tree (I6 + S3.5 + S3.6, with or without S3.7) five tests of S2's files fail the same way: test_gfx_dialogs /
test_gfx_reports test_draw_sprite_other_alpha_is_unsupported, test_gfx_routing test_image_draw_at_another_scale_is_unsupported,
test_gfx_reports test_frames_report_the_unmodelled_sprites and test_frame_controls_red_all[8] (log/roadmap/S3/7_base_fails.log on
the base build): they expect `unsupported` for cases S3.5's merge edits 8 / 10 now draw. They belong to S3.5's merge.

## 7. Gate (numbers in 7_notes.md)

```
~/bin/py -3.12 log/roadmap/S3/7_mirror.py --w32
cd port/build/S3_7_src && SG_PORTSIM_DLL=<x64 dll> ~/bin/py -3.12 -m pytest port/test/test_gfx_text.py        # default suite
SG_EMU_ORACLE=1 ... pytest port/test/test_gfx_text.py                                                           # + the oracles
SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 <abs>/log/roadmap/S3/7_textgate.py <out dir> --jobs 2                       # opaque text gate
```
