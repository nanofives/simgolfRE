# S2.9 merge notes (dialogs over the course: popups, text input box, System menu, SGA grading page)

Base: the combined S2.1-S2.4 tree (waves 1-3: MERGE.md, MERGE_wave2.md, MERGE_wave3.md; the mirror `port/build/S2_4_head`).
Everything was built and judged in the mirror `port/build/S2_9_head` (a copy of `port/build/S2_4_head`'s tree, HEAD_COMMIT
c6d7d78b, junctions log/, original/, port/corpus/, re/replay, re/versions, shim/build, tests/fixtures, tests/golden).

## 1. New files (copy from the mirror, `port/build/S2_9_head/<path>` -> `<path>`)

- port/src/sim/gfx_dialogs.cpp     family `dialogs` (13 bodies: loadInterfaceArt.dialogArt, drawPanelS2, drawTextSurface,
                                   Widget_fillRect, Surface_drawSprite, Widget_draw, drawStringAt, drawStringAt2,
                                   showPopup.readColour / .oddBox / .oddRepaint, dialogs.begin / dialogs.end; negctl modes
                                   1-11, probe modes >= 0x10000)
- port/test/test_gfx_dialogs.py    32 tests (the 14 dialog rectangles of 10 checkpoints, 7 frame controls, unit tests)

Copy them together with item 2: gfx_families.inc names the family, and gfx_dialogs.cpp uses gfxf::fill_rect16 /
blit_scale (gfx_prim_fill.cpp, S2.4) and gfxp::draw_text_run (gfx_prim_text.cpp, S2.3).

## 2. Shared-file edits: log/roadmap/S2/9_shared.patch

One patch (LF), `git apply log/roadmap/S2/9_shared.patch` from the repo root AFTER 4_shared.patch. Made and checked by
`~/bin/py -3.12 log/roadmap/S2/9_tools/make_patch.py` (diff of the mirror against port/build/S2_4_head; `git apply --check`
and an apply in a scratch tree holding the S2_4_head files, each result equal to the mirror's file).

| file | base | change |
|---|---|---|
| port/src/sim/gfx_families.inc | S2.4 | `SG_GFX_FAMILY(dialogs)` after routing |
| port/src/sim/commands_popup.inc | HEAD | drawing helpers (popup_gfx_on, popup_gx, DialogMark), draw_popup_pass (one pass of showPopup's drawing); show_popup: the markers, setDrawTarget, the odd-y colour read, a pass drawn before every pump, the odd-y repaint on a click leave; text_input_box: the markers and the box's drawing at the top of every pass (three frames, title, field, text, caret through Widget_fillRect with the head measured by Text_put while its byte is cut and put back), the underscore and the refresh after the cut loop; the `(void)` casts of title / x / y removed (now used) |
| port/src/sim/saveload.cpp | S2.4 | save_game_dialog: the markers, drawPanelS2(0x36, 0x50, 0x274, 0x50), setDrawTarget(root, g_font51b360), drawCenteredText2(f_strpool_4c3f74, 0x180, 0x58, white); header comment |
| port/src/sim/reports.cpp | HEAD | includes (gfx.h, cstring, vector); struct Page and ReportDraw (the grading page's drawing calls, texts built in g_textBuffer with g_textBuffer and g_textBuf put back); show_course_report fills Page during the computation, and for modes 1 / 2 issues the markers, the page head, the page and (mode 1) the modal loop's first refresh. Mode 0's table draws nothing, as before |
| port/src/sim/menu.cpp | HEAD | after `loadInterfaceArt.images`: `gfx_drop(ctx, "loadInterfaceArt.dialogArt", nullptr, 0)` when a backend is attached |

Nothing else: modules.inc, CMakeLists.txt, gfx.cpp, gfx.h, gfx_body.h, tick.cpp, commands.cpp registrations and test_static.py
unchanged (the glob picks the new .cpp). Without a backend attached every new call is a no-op (gfx_drop returns at its
g_gfx_attached test), so the GameState of every run without a backend is unchanged.

## 3. UNCERTAINTIES.md

Append the 4 rows of log/roadmap/S2/9_uncertain.tsv (U-1280..U-1283) after the S2.4 rows.

## 4. CLAUDE.md paragraph (after the S2.4 paragraph)

(see log/roadmap/S2/9_notes.md, section "CLAUDE.md paragraph")

## 5. Gate commands after the merge (repo root, port/build/x64)

```
~/bin/py -3.12 -m pytest port/test/test_gfx_dialogs.py -q -p no:cacheprovider                         # 32 passed (~10 min)
env SG_BACKEND=win32 ~/bin/py -3.12 -m pytest port/test/test_gfx_dialogs.py -q -p no:cacheprovider    # 32 passed
~/bin/py -3.12 -m port.test.test_gfx_dialogs log/roadmap/S2/9_rects.tsv                                 # the per-rectangle table
```
