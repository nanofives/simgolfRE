# S2.13 merge request: S2.7 + S2.8 + S2.9 in one tree, the U-1278 owner decision and the U-1254 fix

Base: HEAD 2c05f02a + waves 1-4 (MERGE.md with 1_test_static.patch and the README section, MERGE_wave2.md = S2_3_head's new files +
3_shared.patch, MERGE_wave3.md = S2_4_head's new files + 4_shared.patch, MERGE_wave4.md's new files and the scenario_refs.json key
cov_r_s2_screens). This request REPLACES 7_shared(_wt).patch, 8_shared.patch and 9_shared.patch and the three steps' new-file lists:
apply only what is below. Everything was built and judged in the mirror `port/build/S2_13_head` (made by
`~/bin/py -3.12 log/roadmap/S2/13_tools/make_mirror.py`: a git archive of HEAD, junctions as log/roadmap/S0/export.md, every
layer a 3-way merge; 13_mirror.log lists each file). Files: log/roadmap/S2/13_files.txt.

## 0. Base the patch targets (lead's note of 2026-10-25)

13_shared.patch targets **HEAD 2c05f02a + waves 1-4** (MERGE.md, MERGE_wave2.md, MERGE_wave3.md, MERGE_wave4.md), which was HEAD
when this request was finished: integrator I5's S2 waves 1-4 commit had not landed (`git log`). Once it lands, check the patch on
it with `~/bin/py -3.12 log/roadmap/S2/13_tools/check_on_commit.py HEAD <scratch>` (the commit's copies of the 16 files, `git apply
--check`, apply, compare with port/build/S2_13_head): it must print rc 0 and `equal to the mirror: yes`. On bare 2c05f02a it fails, as it
must (gfx_body.h, gfx_families.inc, test_s2_6_scenario.py do not exist there and commands.cpp / routing.cpp lack waves 2-3). If I5's
commit differs from waves 1-4 as written (for example the lead's test_s2_6_scenario.py edited), regenerate with make_mirror.py
--commit <new HEAD> minus the waves layer, or ask S2.13 for a rebase.

## 1. New files (copy from the mirror, `port/build/S2_13_head/<path>` -> `<path>`)

| file | from | S2.13 change |
|---|---|---|
| port/src/sim/gfx_boot_screens.cpp | S2.7 | none |
| port/test/test_gfx_boot_screens.py | S2.7 | none |
| port/src/sim/gfx_reports.cpp | S2.8 | owns the five shared bodies (section 3): Surface_drawSprite gets S2.9's guards (the load arm at +8, an alpha outside [0, 1] draws nothing, a display scale other than (1, 1, 1) reported); controls 9, 10, 20, 21 (S2.9's dialogs 3, 8, 10, 11) |
| port/src/sim/gfx_reports.h | S2.8 | none |
| port/test/test_gfx_reports.py | S2.8 | none |
| port/src/sim/gfx_dialogs.cpp | S2.9 | its Widget_fillRect, Surface_drawSprite, Widget_draw, drawStringAt, drawStringAt2 bodies and controls 3, 7, 8, 10, 11 removed (8 bodies left) |
| port/test/test_gfx_dialogs.py | S2.9 | the moved controls run as `gfx.family.reports` modes 9 / 1 / 10 / 20 / 21; the blend model is draw16_1002ed60's; the sloped-line test now checks the line is drawn |

Copy them with item 2 (gfx_families.inc names the three families; a gfx_<f>.cpp missing from it fails test_static).

## 2. Shared-file edits: log/roadmap/S2/13_shared.patch

One patch (LF), `git apply log/roadmap/S2/13_shared.patch` from the repo root AFTER 3_shared.patch and 4_shared.patch (and wave 4's
files). Made by `~/bin/py -3.12 log/roadmap/S2/13_tools/make_patch.py <scratch>` (diff of the mirror against its pre-step copies
port/build/S2_13_pre); its check (13_patch_check.log): the working tree's copies of the 16 files + 3_shared + 4_shared equal the
patch base; `git apply --check` passes and the applied files equal the mirror's; the same on CRLF copies under core.autocrlf=true
with `git apply --ignore-whitespace` (all three patches).

| file | change (step) |
|---|---|
| port/src/sim/gfx_families.inc | `SG_GFX_FAMILY(boot_screens)`, `(reports)`, `(dialogs)` after routing (S2.7, S2.8, S2.9) |
| port/src/sim/menu.cpp | load_flic's loading-screen block, keeping HEAD's `cm::pump_load(ctx)` right after it (S2.7, = 7_shared_wt); pick_a_pro_screen / show_theme_pack_select drawing (S2.7); the `loadInterfaceArt.dialogArt` drop (S2.9) |
| port/src/sim/commands_screens.inc | select_difficulty_screen / show_loading_screen drawing (S2.7); the report screens' drawing and texts (S2.8) |
| port/src/sim/tick_boot.inc | BootSetup's title art (S2.7) |
| port/src/sim/commands.cpp | `#include "gfx_reports.h"` (S2.8) |
| port/src/sim/gfx_body.h, gfx.cpp | Sprite8::cut_w / cut_h, crop_sprite stores them (S2.8) |
| port/src/sim/holestats.cpp, routing.cpp | hole stats, shortcuts and roster drawing (S2.8) |
| port/src/sim/commands_popup.inc, saveload.cpp | showPopup / textInputBox / saveGameDialog drawing (S2.9) |
| port/src/sim/reports.cpp | both steps' include lines merged; S2.9's grading page (Page / ReportDraw); S2.8's end-of-year drawing and texts; **U-1254 fix** (section 4) |
| port/test/framegate.py | a boot that stops on a missing name judges the checkpoints before its last equal record (S2.8, U-1270) |
| port/test/golden_reports.json, golden_commands.json | showEndOfYearScreen / showPlayerCommentsReport re-pinned through the oracle (S2.8; = 8_tools/merge_goldens.py's keys) |
| port/test/test_s2_6_scenario.py | the lead's U-1253 xfails replaced: the boot equals the capture up to record 1156 (no KNOWN entry), stops only at buy land; new control showEndOfYearScreen mode 6 (U-1254) RED at record 771 in the cursor tile only; mode 1 RED at 771 in g_memberTypeHistory; on Win32 (no run detail text) the buy-land stop is judged by its record |

NOT included (as asked): the boot.py KNOWN entry for cov_r_s2_screens (none is needed now: no record before 1156 differs).
Nothing for modules.inc, CMakeLists.txt, tick.cpp, gfx.h, test_static.py.

## 3. U-1278 decision: one body per name, owner `reports`

Surface_drawSprite, Widget_fillRect, Widget_draw, drawStringAt and drawStringAt2 are gfx_reports.cpp's; gfx_dialogs.cpp keeps none
of them; gfx.check() reports 0 duplicates (13_u1254_x64.log). Reasons, from the code and both steps' tests:
- Surface_drawSprite: jgld Sprite::drawAlpha sets its flag byte [ebp-0x10] to 1 at 0x10018943 and clears it only when the
  destination's format record (slot +0xe4) has +4 == 1 (0x100189af..0x100189b5); in the quarter band the flag set calls thunk
  0x100011ea -> draw16_1002ed60 (0x1001907d), cleared 0x1000126c -> draw16_10043430 (0x1001909a). S2.8 measured the references:
  the quarter blend with draw16_1002ed60's 555 masks reproduces 99.4 % of rep_comments' toolbar pixels, the 565 masks 21.9 %
  (U-1271). S2.9's 565 body was judged only on held pixels (its unit test checked its own reading). The display's +4 value itself
  was not read (U-1207). S2.9's guards are kept on the reports body (c3da.cpp SurfaceDrawSprite_re: +8 != 0 returns 0x18).
- Widget_fillRect: S2.8's body is jgld line16 with the octant walk and the Surface_box clip; on axis-parallel lines it is S2.9's
  (vline16 / hline16, ends inclusive, a point draws nothing), and S2.9's caret / text-box frames still have 0 failing px.
- Widget_draw / drawStringAt / drawStringAt2: the two bodies were the same computation (font swap, x, x - (w >> 1), x - w).
- Controls kept: S2.9's frame controls 3, 8, 10 run as reports 9, 10, 20 (RED on k_dlg_S / k_dlg_J), its unit controls 7, 11 as
  reports 1, 21 (RED). test_gfx_dialogs 32 / 32, test_gfx_reports 36 / 36 on the merged tree.

## 4. U-1254 fix (port/src/sim/reports.cpp)

Cause: the original's end-of-year loop makes a View_toLocal cursor read on every pass, after pumpMessages (c3er.cpp
CareerRetirementFlow_re: 0x0044e42f waitTicks, 0x0044e437 pumpMessages, 0x0044e44b View_toLocal, 0x0044e457 refreshFull, the hover
test 0x0044e46d..0x0044e488), and the course report's loop before its pump (c3fr.cpp ShowCourseReport_re 0x0045316a, pump
0x004531e4). The port's report_loop made no read, so the end-of-year screen left its CURSOR records (296 in frame 770 of
cov_r_s2_screens, positions 198804..202027) unread and P6's View_toLocal after the screen took record 0 of the frame (175, 535)
instead of the last one (576, 246). report_loop now reads the cursor through the context's InputSource in the original's order
when the display exists (g_graphicsDevice != 0, as tick_render.inc viewToLocal), and with a backend draws the button hover
(g_spriteRef_820f40[1] while inside, [0] on leaving, each with a refreshFull; the end-of-year pass's refreshFull). Control:
showEndOfYearScreen mode 6 leaves the read out.
Result: the cov_r_s2_screens boot equals the capture in every record up to 1156 and stops there on showBuyLandScreen (U-1251); with
mode 6 record 771 differs in g_cursorTileX / g_cursorTileY only, the old symptom (13_u1254_x64.log, Win32: 13_w32_test_s2_6_scenario.log).

## 5. UNCERTAINTIES.md

Append the rows of 7_uncertain.tsv (U-1260..U-1263), 8_uncertain.tsv (U-1270..U-1278), 9_uncertain.tsv (U-1280..U-1283),
12_uncertain.tsv (U-1254) and 13_uncertain.tsv (U-1205..U-1207). Status changes:
- U-1252 `resolved` with 7_merge.md section 3's text.
- U-1253 `resolved`: "not U-1062's mechanism (S2.12: the original's call logs with and without FRAMEHASH are identical); the cause
  is U-1254".
- U-1254 `resolved`: "the port's end-of-year loop made no per-pass View_toLocal read (c3er.cpp 0x0044e44b), so frame 770's P6 read
  took the pre-screen cursor record; reports.cpp report_loop reads it in the original's order (and the course report's, c3fr.cpp
  0x0045316a); the boot passes record 771, control showEndOfYearScreen mode 6 RED (roadmap S2.13)".
- U-1278 `resolved`: "owner `reports` for all five names (log/roadmap/S2/13_merge.md section 3); gfx.check 0 duplicates".

## 6. CLAUDE.md

Append the three steps' paragraphs (7_merge.md section 5, 8_merge.md section 5, 9_notes.md "CLAUDE.md paragraph") after the S2.4
paragraph, with one correction in S2.9's: replace `Widget_fillRect (jgld drawLine's axis-aligned vline16 / hline16, ends inclusive),
Surface_drawSprite (Sprite::drawAlpha's 0.25 band = draw16_10043430: 1/4 palette + 3/4 destination on 565 field masks, stored
without the final >> 4, kept), Widget_draw / drawStringAt (x - (w >> 1)) / drawStringAt2 (x - w) with the font swap,` by
`(Widget_fillRect, Surface_drawSprite, Widget_draw, drawStringAt, drawStringAt2: the reports family's bodies after S2.13),` and
`(gfx_dialogs.cpp, 13 bodies)` by `(gfx_dialogs.cpp, 8 bodies)`. Then:

```
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
```

## 7. Gate commands after the merge (repo root, port/build/x64)

```
~/bin/py -3.12 -m pytest port/test/<file> -q -p no:cacheprovider       # one file per process: test_gfx, test_gfx_credits,
    test_gfx_routing, test_gfx_boot_screens, test_gfx_reports, test_gfx_dialogs, test_static, test_commands, test_tick,
    test_reports, test_s2_5_traces, test_s2_6_scenario (and SG_BACKEND=win32 for the gfx files)
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --jobs 4 --out <tsv>       # 205 / 205, 0 differ (same win32)
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out <tsv>                              # 65 / 2 / 2 / 1 on 70 rows
~/bin/py -3.12 -m port.test.framegate --all --jobs 2 --no-probe --out <tsv>              # census (13_notes.md)
```
