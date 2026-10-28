# S3 resubmission: waves 2 + 4 + 5 + S3.7 + S3.8 as ONE merge (assembled and measured by S3.11)

Replaces log/roadmap/S3/MERGE_w2.md (refused by I6), MERGE_w4.md and the per-step requests 4c_merge.md, 5_merge.md, 5b_merge.md,
7_merge.md, 8_merge.md as a set: their texts stay the source of every edit (the applier below reads them, nothing is retyped), this file
fixes the order, the meeting points and the gate. files_w5.txt lists every file. Nothing is staged. Notes: log/roadmap/S3/11_notes.md.

Base: HEAD bbc9072f (S6 wave 2, S3 waves 1 / 3, S2 wave 5 committed). The tree that was measured is port/build/S3_11_src, made by

```
~/bin/py -3.12 log/roadmap/S3/11_assemble.py [--recopy]                       # git archive HEAD + junctions + own files + every edit
(cd port/build/S3_11_src; ~/bin/py -3.12 port/test/build.py --arch x64; ~/bin/py -3.12 port/test/build.py --arch Win32)
```

(port/build/S3_11_src/port/build/x64 and Win32 are junctions to port/build/S3_11 and S3_11_w32.) To apply to another root, e.g. the
integrator's mirror or the shared tree after the integrator's own merges: `11_assemble.py --root <root>` (copies the own files from the
shared tree unless the root IS the shared tree, then applies the same edits; each anchor must occur exactly once, an edit already present
is refused, so a second run stops at the first edit). Logs: log/roadmap/S3/11_assemble.log (the measured tree, steps w2..s38; S3.11's edits were added after it) and 11_assemble_verify.log (the whole chain from a fresh archive, equal to the measured tree; every edit with its step
and number).

## 1. Own files (copy as they are from the shared tree)

| step | files |
|---|---|
| wave 2 S3.4 / S3.4b | port/src/sim/flic.cpp, port/include/portsim/sim/flic.h, port/test/test_flic.py, port/test/test_defects_flic.py, re/tools/flic_cells_rec.py, re/frida/js/flic_cells.js |
| wave 2 S3.4c | port/test/test_menu.py, port/test/golden_menu.json, port/test/test_terrain_live.py, port/test/terrainlive.py (tracked files, S3.4c's versions) |
| wave 4 S3.6 / S3.6b | port/src/sim/gfx_hud.cpp, port/include/portsim/sim/gfx_hud.h, port/test/test_gfx_hud.py, port/src/sim/gfx_pixel.cpp, port/include/portsim/sim/gfx_pixel.h, port/test/test_gfx_pixel.py |
| wave 5 S3.5 / S3.5b | port/src/sim/gfx_scene.cpp, port/include/portsim/sim/gfx_scene.h, port/test/test_gfx_scene.py (S3.5b's versions) |
| S3.7 | port/src/sim/gfx_text.cpp, port/include/portsim/sim/gfx_text.h, port/test/test_gfx_text.py (the shared tree's version: S3.7's late cov_a_flags `end` frame) |
| S3.8 | port/src/sim/terrain_overlay.cpp, port/include/portsim/sim/terrain_overlay.h, port/test/test_terrain_overlay.py |

Each equals its step's final mirror (11_notes.md section 1); gfx_scene.cpp and test_flic.py then get S3.11's edits (section 2 item 6). NOT part of this merge: the shared tree's port/src/sim/gl_raster.cpp / .h,
port/test/test_gl_raster.py, port/test/glraster.py, port/src/sim/gfx_terrain.cpp as S3.10 is editing them in place; this tree has
HEAD's versions of those, with S3.8's two gfx_terrain.cpp edits applied.

Logs (S6's, needed by the regenerated decisions table): put log/roadmap/merges/I6/prepared_s3w2/s3_4b_decisions.tsv back as
log/roadmap/S6/s3_4b_decisions.tsv and log/roadmap/merges/I6/prepared_s3w2/merge_decisions.py.with_D53 as
log/roadmap/S6/merge_decisions.py (the D53 input read; diff against S6's current file = exactly that read, the `D5 fix (S3 wave 2)`
label and `--root`). 11_assemble.py runs that generator text in memory against the prepared input, so the measured
re/port/defect_decisions.tsv equals I6's mirror port/build/I6 byte for byte.

## 2. Shared-file edits, in apply order (each anchor exactly once on HEAD bbc9072f; the full chain's log: 11_assemble_verify.log)

1. **Wave 2 (MERGE_w2.md, exactly as I6's apply_shared.s3w2 applied it in port/build/I6):** modules.inc `SG_MODULE(flic)` (append);
   menu.cpp 2a include `portsim/sim/flic.h`, 2b the column read through `flic::resolve_name`, 2c `flic::sheet_to_gfx` in load_flic's
   else branch; re/port/defect_profiles.tsv the `flic.treeAliasNames 0 D5 ...` line; re/port/defect_decisions.tsv regenerated (D53,
   75 rows: 13 fix / 62 keep); re/PORT_PLAN.md row 7 in I6's prepared form (75 rows, 13 fix, 62 keep, D53, profile fixed = 9 switches);
   UNCERTAINTIES U-1330..U-1339 and U-1701 resolved; CLAUDE.md paragraph (section 4).
2. **Wave 4 (MERGE_w4.md):** gfx_families.inc `SG_GFX_FAMILY(hud)` + `SG_GFX_FAMILY(pixel)` after `SG_GFX_FAMILY(dialogs)`;
   log/roadmap/S3/6_edits.py EDITS 1..18 (paths relative to port/: gfx.cpp x8, gfx_body.h x2, tick.cpp x3, menu.cpp x1,
   commands_layout.inc x4); 6b_edits.py is empty; 6_merge.md section 2 needs nothing (done in S3.6b's gfx_pixel.cpp).
3. **Wave 5 = S3.5 as rebased by S3.5b:** 5_merge_edits.py EDITS 1..15 (gfx_families.inc `scene` after `terrain`, tick_render.inc x2,
   tick_frame.inc, menu.cpp, tick.cpp, tick_scene.inc x3, gfx.cpp x2, gfx_hud.h x2, gfx_hud.cpp x2), EDITS_AFTER_S2W5 1..2
   (gfx_reports.cpp: the Surface_drawSprite hand-over), EDITS_HUD 1..7 (gfx_hud.cpp); then 5b_edits.py EDITS 1..2 (golfers_world.inc,
   golfers.cpp: placeAnimal's queue entry) and 5b_edits_tests.py EDITS_TESTS 1..5 (test_gfx_routing.py, test_gfx_dialogs.py,
   test_gfx_reports.py x3).
4. **S3.7:** gfx_families.inc `SG_GFX_FAMILY(text)` (append, last line); 7_edits.py EDITS 1..27 (gfx.cpp x5, commands_internal.h,
   commands.cpp, commands_panels.inc x2, gfx_hud.cpp x13, gfx_body.h, include/portsim/sim/gfx.h, tick.cpp, gfx_prim_text.cpp x2).
5. **S3.8:** 8_edits.py EDITS 1..8 (modules.inc `SG_MODULE(terrain_overlay)` after flic, gfx_terrain.cpp x2, gfx.cpp
   b_panel_backdrop's drawMapLine bar, routing.cpp x3, test_gfx_routing.py: the routing map's hole line, approved by the orchestrator).
6. **S3.11:** log/roadmap/S3/11_edits.py EDITS 1..4 (gfx_scene.cpp: TEST-ONLY body `gfx_scene.setPalette`; test_flic.py: the hook-up
   test draws each sheet cell with the slot palette as Image_draw's mask; why: 11_notes.md section 2) and the CLAUDE.md paragraph of
   section 6. So port/src/sim/gfx_scene.cpp and port/test/test_flic.py in the merged tree are the shared tree's copies PLUS these edits.

Then touch port/src/registry.cpp and port/src/sim/gfx.cpp (MSBuild does not track the .inc files).

### Where the steps meet (resolved, all measured in this tree)

- gfx_families.inc final order: selftest, terrain, scene, surface, prim_text, credits, prim_fill, prim_text2, routing, boot_screens,
  reports, dialogs, hud, pixel, text. No body name is registered twice (gfx.check 0 duplicates, 125 bodies, x64 and Win32), so the
  order decides nothing; scene sits after terrain as 5_merge_edits.py's anchor puts it, the others are appended in apply order.
- modules.inc: `flic` then `terrain_overlay` (8_edits.py anchors on the flic line), both after `video`.
- gfx.cpp: wave 4 (6_edits) -> S3.5 (gfxs::image_draw hand-over, gfx_scene.h include) -> S3.7 (gfx_text.h include after gfx_scene.h,
  draw_text_string's markup hand-over, drawWindow.frame registration removed, the measuring drop) -> S3.8 (drawPanelBackdrop's bar).
  The result equals S3.7's mirror port/build/S3_7_src plus exactly S3.8's 5-line hunk in b_panel_backdrop.
- gfx_hud.cpp: S3.6b's file -> S3.5's EDITS 14-15 and EDITS_HUD -> S3.7's 13 edits; equal to S3.7's mirror (byte-identical up to line ends).
- tick.cpp: equal to S3.7's mirror (6_edits 9-11, S3.5 edit 6, S3.7 edit 25).
- Surface_drawSprite / Widget_fillRect: one body each, the `reports` family's (gfx_reports.cpp), as decided (S2_S3_SPLIT.md):
  S3.6b's gfx_pixel.cpp registers Widget_writePixel only; S3.5's gfx_scene.cpp does not register Surface_drawSprite and
  gfx_reports.cpp's body hands it every case outside its quarter band (5_merge_edits EDITS_AFTER_S2W5). gfx.check: both names
  `reports`.
- test_gfx_routing.py: S3.5b's test edit (scaled Image_draw is drawn) and S3.8's (the hole line drops replaced by lineTo) touch
  different tests; the result equals S3.8's mirror plus S3.5b's hunk and S3.5b's mirror plus S3.8's hunk.
- framegate.py: no step of this resubmission edits it (S3.2's edits are in HEAD).
- routing.cpp: S3.8's three edits, on HEAD's routing.cpp (S2's, owned by S3 for these edits by the orchestrator's approval).

## 3. UNCERTAINTIES.md

55 new rows, 3 existing rows edited; texts from the steps' files, `|` inside a cell written `\|`, non-hex address cells
left empty, rows inserted after the last table row, in this order:

| step | rows | from |
|---|---|---|
| wave 2 | U-1330..U-1339 added; U-1701 `resolved` + `settled by roadmap S3.4 as U-1330: ...` | MERGE_w2.md section 5 |
| wave 4 | U-1350..U-1359 added; U-1359 then `resolved` + 6b_merge.md section 2's text; U-1380..U-1384 added | 6_uncertain.tsv, 6b_uncertain.tsv |
| S3.5 / S3.5b | U-1332 (added by wave 2) replaced by 5_uncertain.tsv's row (`resolved (S3.5)`); U-1340..U-1349 added; U-1385..U-1387 added; U-1349 then `partial` + 5b_merge.md section 3's text | 5_uncertain.tsv, 5b_uncertain.tsv |
| S3.7 | U-1360..U-1368 added; U-1069 `partial`, U-1072 `resolved`, U-1353 `resolved`, each with 7_merge.md section 4's text appended | 7_uncertain.tsv |
| S3.8 | U-1370..U-1377 added (their `Terrain.dll 0x...` / `golf_clean.exe 0x...` address cells left empty, as I5 / I6 did) | 8_uncertain.tsv |

S3.11 adds no U-row (U-1380..U-1384 are S3.6b's, used as written). In the measured tree `diff HEAD:UNCERTAINTIES.md` has 58 changed lines.

## 4. CLAUDE.md (one paragraph per step, inserted in this order)

1. Wave 2: MERGE_w2.md section 6's paragraph + `Re-merged with S3 waves 4 / 5 (roadmap S3.11): the defect row is D53 of
   re/port/defect_decisions.tsv (75 rows, 13 fix / 62 keep, its input log/roadmap/S6/s3_4b_decisions.tsv) and profile fixed has 9
   switches.` + 4c_merge.md section 2's sentence; BEFORE the `Roadmap S3 wave 3, the terrain in the frame` paragraph (so the S3 waves read
   in order).
2. S3.6 / S3.6b: 6_merge.md section 5's paragraph with 6b_merge.md section 4's sentence in place of `The golfer list and detail panels ...
   are handed over and reported, not drawn.` and `GATE MEASURE: framegate --no-live (no terrain; S3.2's terrain is judged under G2) and
   no scene sprites, so` in place of `GATE MEASURE: the port draws no terrain or scene yet, so`.
3. S3.5: 5_notes.md section 8's paragraph, its round-2 paragraph appended as `Round 2: ...`.
4. S3.5b: 5b_merge.md section 4's paragraph.
5. S3.7: 7_notes.md section 9's paragraph.
6. S3.8: 8_notes.md section 6's paragraph.
7. S3.11: section 6 below.

Items 2-7 go after the S3 wave 3 paragraph, before `Phase 7 defect decisions, section S6 of the roadmap orchestrator`.

## 5. Gate (tree port/build/S3_11_src = HEAD bbc9072f + every edit above; builds port/build/S3_11 x64, S3_11_w32 Win32, 0 warnings)

```
- port suite x64, one file per process (log/roadmap/S3/11_suite_by_file.sh, results 11_suite_x64/, compared with I6's
  log/roadmap/merges/I6/suite_merged in 11_suite_compare_x64.log): 69 files, 68 PASS; test_boot 94 + 1 failed (cov_r_s2_screens,
  BASELINE's known stop at record 1,156); 58 files equal to I6's counts; new / changed: test_flic 10 (with 11_edits.py; 1 failed
  before it, the S3.5 x S3.4 interaction of 11_notes.md section 2; re-run 11_suite_x64_rerun.log), test_defects_flic 8, test_menu
  24 + 47 skipped, test_terrain_live 398, test_gfx_hud 34 + 7 skipped, test_gfx_pixel 17, test_gfx_scene 118, test_gfx_text 32 + 20
  skipped, test_terrain_overlay 12 + 6 skipped, test_vectors 298 + 488 skipped and test_win32 179 + 242 skipped (the 9 new registry
  names: flic.load, flic.toGfx, overlay.rasterLine / logStart / logWrite, gfx.family.hud / pixel / scene / text; two skipped tests
  each in test_vectors, one in test_win32); test_gfx_dialogs 32, test_gfx_routing 35, test_gfx_reports 36 (S3.5b's test edits);
  repo static 614 passed / 2 skipped.
- Win32 (SG_BACKEND=win32, 26 files touched by S3, 11_win32_files.txt, 11_suite_w32/, 11_suite_compare_w32.log): 25 PASS; test_tick
  1 failed + 249 = I6's Win32 baseline (test_run_frames_reports_why_it_stopped, one of BASELINE's 20); test_gfx_text 32 + 20
  skipped (the re-run S3.7's late note asked for), test_terrain_live 395 + 3 skipped, test_gfx_hud 34 + 7, test_gfx_scene 118,
  test_gfx_pixel 17, test_terrain_overlay 12 + 6, test_flic 10, test_defects_flic 8, test_menu 24 + 47, test_gfx_dialogs 32,
  test_gfx_routing 35, test_gfx_reports 36, test_profiles 19; every other file equal to I6's Win32 counts.
- gfx.check: 0 duplicate bodies, 125 bodies (126 with S3.11's test-only gfx_scene.setPalette), x64 and Win32; Surface_drawSprite
  and Widget_fillRect each one body, `reports` (11_gfx_check.log).
- segments: x64 205 / 205, Win32 205 / 205, 0 rows differ from log/roadmap/merges/I6/segs_*.tsv in status, frames or digest, x64 =
  Win32; course_season/08 on Win32 run alone three times: PASS 612 / 612 with the committed digest ef6771e18df8c87a each time
  (11_segs_compare.log).
- boot: 70 rows on x64 and on Win32, 0 differ from the committed rows; HEAD's 61 at 58 PASS / 2 KNOWN / 1 NO_FRAMES;
  cov_r_s2_screens stops at 1,156 of 1,268 (11_boot_compare.log, 11_boot_compare_win32.log).
- frame census (port.test.framegate --all --jobs 2, 11_census.tsv; ranked by S0's 3_rank.py into 11_census_out/; compared by
  log/roadmap/merges/I6/census_compare.py, 11_census_compare.log): 698 rows, PASS 91 (committed 87), FAIL 549, UNSUPPORTED 45 (was 598),
  NOT_RUN 13; 0 PASS rows lost (against the committed census or S0's); 4 gained (cov_l_camera after / edge / end / step0);
  transitions UNSUPPORTED -> FAIL 549, UNSUPPORTED -> PASS 4. Per row against the committed run (11_census_px.py): 599 rows with
  fewer differing px, 87 equal, 0 more; differing px over the rows judged in both 110,404,244 -> 7,183,814. FAIL rows: median 103 px,
  90th percentile 270, 240 with no pixel beyond one level; 31 rows over 10,000 px (the screens with no drawing yet: cov_r_s2_screens
  bulletin / editor, the character editor rows of cov_a_keys3 / cov_a_golfer / cov_r_bio, championship tour_0180 ...;
  11_census_fail_spread.log).
- S3.2 terrain verdict (2_census.py terrain, 11_terrain_verdicts.tsv): 527 PASS, 0 FAIL, 75 NOT_MEASURED, every one of the 602 rows
  equal to I6's in frame / drawn / diff / one / big / verdict (11_terrain_merge.log). The full run lost cov_k3_build's boot once (12
  rows `the port kept no course view ... (boot FAIL)`, no reason recorded); run alone twice it is PASS 12 / 12 with I6's numbers
  (11_terrain_k3build*.tsv), and the final table takes that re-run.
- Census ranked by the S3 rule (2_census.py rank, 11_census_ranked_s3rule.tsv), primary cause per row: FAIL with no unsupported
  call 477, PASS 91, terrain not measured 75, `no body: Surface_createFull_fwd1` 23, `no body: showRoutingMapScreen.teeSprite` 22,
  no trace 10. (I6's ranking: Widget_writePixel 443, terrain not measured 75, Surface_stretchBlit 70, Surface_drawScaled2 13, no trace
  10.) FAIL rows by region with differing px: view 532, toolbar 458, hud_title 456, hud_meters 438, hud_message 435, bottom_panel 265.
```

## 6. CLAUDE.md paragraph (S3.11; applied by 11_assemble.py's last step)

```
Roadmap S3.11, the S3 resubmission (2026-10-25, notes log/roadmap/S3/11_notes.md, merge log/roadmap/S3/MERGE_w5.md): S3 waves 2
(S3.4 / S3.4b / S3.4c), 4 (S3.6 / S3.6b) and 5 (S3.5 / S3.5b) and S3.7 / S3.8 assembled as ONE tree on HEAD bbc9072f by
log/roadmap/S3/11_assemble.py (every edit from the steps' own edit lists, each anchor exactly once, `--root <dir>` applies them to
another tree). One interaction no step's gate ran: S3.5's resolution of U-1332 (a sheet cell draws with its queue entry's palette
object) failed S3.4's test_flic hook-up test, which drew a cell with no mask; the test now passes the slot palette as Image_draw's mask
through the TEST-ONLY body gfx_scene.setPalette (11_edits.py). Gates on the tree: port suite x64 68 / 69 files (test_boot's known
cov_r_s2_screens stop), Win32 S3 files 25 / 26 (test_tick's baseline failure), segments 205 / 205 x64 + Win32 with 0 digests changed,
boot rows unchanged on both, gfx.check 0 duplicates, census 698 rows PASS 91 (was 87, none lost, no row with more differing px),
S3.2 terrain verdict 527 / 527 unchanged.
```

## 7. When I7 commits

I7 (S2 waves 6-7, S5, S7 waves 1-2) edits gfx.cpp, gfx_families.inc, modules.inc, tick.cpp, menu.cpp, commands*.inc, test_gfx_reports.py and
CLAUDE.md / UNCERTAINTIES.md as well. Every edit of this merge was checked against a copy of I7's mirror port/build/I7 as applied at 13:46
(`11_assemble.py --root port/build/S3_11_i7chk`, 11_i7chk.log): every anchor occurs exactly once there too. After I7's commit, rebuild the tree
on it and re-measure: `11_assemble.py --dst S3_11_src --recopy --commit <I7 HEAD>`, both builds, then the commands of 11_notes.md section 4.
`--root <tree>` applies the same edits to an integrator's mirror.
