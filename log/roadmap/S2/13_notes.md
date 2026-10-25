# S2.13 notes: integration of S2.7 + S2.8 + S2.9, the U-1278 owner decision and the U-1254 fix

Worker on claude3 (session cmvn6m2cs666po115mdej5vdr), lead cmvmns3ax3frbo1152mcm7eg2. Merge request: 13_merge.md. Files: 13_files.txt.
Uncertainties: 13_uncertain.tsv (U-1205..U-1207).

## 1. The mirror

`~/bin/py -3.12 log/roadmap/S2/13_tools/make_mirror.py` (13_mirror.log) builds port/build/S2_13_head:
1. `git archive 2c05f02a` (HEAD) + the junctions of log/roadmap/S0/export.md (log, original, port/corpus, re/replay, re/versions,
   shim/build, tests/fixtures, tests/golden);
2. waves 1-3: every file port/build/S2_4_head changed against c6d7d78b (an archive of it in port/build/S2_13_c6) 3-way merged onto
   HEAD with `git merge-file` (commands.cpp, tick.cpp, tick_boot.inc clean: they carry S1 / S8 changes too), wave 1's README section;
3. wave 4: the working tree's test_s2_5_traces.py, scenario_s2_6.py, s2_6_runs.py, test_s2_6_scenario.py (the lead's rewrite) and
   the cov_r_s2_screens key of scenario_refs.json;
4. port/build/S2_13_pre = copies of every file the steps touch at this point (the patch base);
5. S2.7, S2.8, S2.9: every file each step mirror changed against S2_4_head, 3-way merged. Not taken: S2.8's boot.py KNOWN entry, S2.7's
   copies of the S2.6 files (older than wave 4's). Conflicts, resolved by hand: gfx_families.inc (all three lines), menu.cpp (S2.7's
   loading-screen block with HEAD's `cm::pump_load(ctx)` after it, = 7_shared_wt.patch), reports.cpp's include lines (both sets).

Builds: x64 `~/bin/py -3.12 log/roadmap/S2/build.py <abs>/port/build/S2_13_head <abs>/port/build/S2_13 --log log/roadmap/S2/13_build.log`;
Win32 `~/bin/py -3.12 port/test/build.py --arch Win32` from the mirror root (its own port/build/Win32; 13_build_w32.log).

## 2. U-1278: one body per name

The duplicate names were Surface_drawSprite, Widget_fillRect, Widget_draw, drawStringAt, drawStringAt2. All five are now gfx_reports.cpp's;
gfx_dialogs.cpp lost its five bodies (and its `widget_text` helper) and keeps 8. gfx.check() = 0 duplicates (13_u1254_x64.log).
Evidence for Surface_drawSprite, read from original/jgld.dll with capstone (Sprite::drawAlpha 0x10018920):
- 0x10018943 `mov byte [ebp-0x10], 1`; 0x100189a2 `call [edx+0xe4]` on the destination, 0x100189af `cmp dword [eax+4], 1`,
  0x100189b5 `mov byte [ebp-0x10], 0` only when equal;
- the quarter band arm 0x1001905d: flag != 0 -> 0x1001907d `call 0x100011ea` (`jmp 0x1002ed60`, draw16_1002ed60); flag == 0 ->
  0x1001909a `call 0x1000126c` (`jmp 0x10043430`, draw16_10043430).
S2.9's comment read `format word [1] == 1, so the 555 arm`, which inverts the test above; S2.8 measured the 555 member on the reference
frames (99.4 % against 21.9 %, U-1271). The display's +4 value was not read (U-1207). S2.9's guards were added to the reports body (c3da.cpp
SurfaceDrawSprite_re 0x00474440: `this +8 != 0` returns 0x18; an alpha outside [0, 1] returns 3 with nothing drawn; the display scale).
Widget_fillRect: S2.8's line16 (octants, clip) is a superset of S2.9's axis-only body (same vline16 / hline16 rule). The three text
bodies computed the same thing in both. Controls: S2.9's dialogs modes 3, 7, 8, 10, 11 are reports modes 9, 1, 10, 20, 21 (new: 9, 10,
20, 21; 1 already existed as the half blend); test_gfx_dialogs.py runs them on `gfx.family.reports` and its blend model is
draw16_1002ed60's; its sloped-line test now checks that the line is drawn (8 pixels for (0,0)-(5,7)).

## 3. U-1254: the stale cursor after showEndOfYearScreen

Trace (12_trace_dump.txt): frame 770 (pos 198140) holds 299 CURSOR records: 248 x (175, 535) from pos 198804, one every 11 clock
positions (the end-of-year loop's passes), the cursor delivery at 201524, 48 x (576, 246), the click handlers at 202027 (onPress1 /
onUnpoint), the Esc at 206172 and the frame's last three reads at 206172 (P6). The original's loop (c3er.cpp CareerRetirementFlow_re)
reads View_toLocal (0x0044e44b) after pumpMessages (0x0044e437) on every pass; the port's report_loop read nothing, so 296 records
stayed unread and P6's View_toLocal took record 0. Fix (reports.cpp): report_loop reads the cursor through input_source(ctx) when
g_graphicsDevice != 0 (tick_render.inc viewToLocal's rule), after the pump on the end-of-year screen and before it in the course
report (c3fr.cpp ShowCourseReport_re 0x0045316a, pump 0x004531e4); with a backend the end-of-year pass also issues its refreshFull and
both loops draw the button hover (g_spriteRef_820f40[1] inside, [0] on leaving, at (0x226, y) / (0x2bd, 0x18e), each with a refresh).
Control: showEndOfYearScreen mode 6 (no read). Result (13_u1254_x64.log): boot PASS through record 1156, stops at 1156 / 1268 on
`missing: showBuyLandScreen` (U-1251); mode 6: first divergence record 771, objects g_cursorTileX / g_cursorTileY only. The run still
leaves 1,580 CURSOR records unread elsewhere (U-1206); the course-report half is judged only by segments and the reading (U-1205).
test_s2_6_scenario.py rewritten (13_merge.md section 2): 4 tests, x64 4 passed.

## 4. Gates (mirror, x64 DLL port/build/S2_13/Release, Win32 port/build/S2_13_head/port/build/Win32/Release)

| gate | result | log |
|---|---|---|
| x64 files, one per process | test_static 6 (after moving the jgld addresses out of a comment: the first run failed on them), test_gfx 25, credits 47, routing 35, boot_screens 32, s2_6_scenario 4, s2_5_traces 3, reports 36, dialogs 32, commands 83 / 44 skipped, tick 250 | 13_x64_suite.log, 13_x64_test_*.log |
| test_reports (golden_reports.json) | 14 passed / 2 skipped; with SG_EMU_ORACLE=1 16 passed (the emulated original agrees with the new cursor reads) | 13_x64_test_reports*.log |
| Win32 gfx files + s2_6 | 7 files pass (section 5) | 13_w32_test_*.log |
| segments | 205 / 205 PASS x64 and Win32, 0 digests differ from log/p5/segs_*_loc.tsv | 13_segs_*.tsv / _compare.log |
| boot --all | 70 rows: PASS 65, KNOWN 2 (cov_a_golfer, cov_b_holes18: U-1062), NO_FRAMES 2 (cov_b_location, cov_a_quit_title), FAIL 1 (cov_r_s2_screens at 1156, U-1251). Against 7_ / 8_ / 9_boot_gate.tsv: cov_r_bio and cov_r_flic_input FAIL -> PASS (S1 is in HEAD, not in the steps' c6d7d78b bases); vs 9_ also cov_a_keys / golfer_stories (load failures there, PASS on S2.9's re-run) | 13_boot_gate.tsv, 13_boot_compare.log |
| census --all | 698 rows (70 scenarios; S1's five and cov_r_s2_screens' 32 included): PASS 83, UNSUPPORTED 580, NOT_RUN 35. Every row PASS in any of 7_ / 8_ / 9_census (union 72) is PASS; no row's first unsupported drawing call differs from every step's; 69 UNSUPPORTED rows have more px than the best step run (88.9-99.9 % of the frame wrong; 3 of them more than all three: cov_a_career career_editor, cov_a_keys2 C_overwrite, cov_r_s2_screens editor, the character editor at 99.7-99.9 %): the root keeps the last screen the backend drew under frames it cannot draw, and the merged tree draws more screens than each step alone (the classes S2.4 / S2.7 / S2.9 reported) | 13_census_x64.tsv, 13_census_compare.log, 13_census_analysis.log |
| patch | 13_shared.patch (16 files) applies to the working tree's copies + 3_shared + 4_shared, LF and CRLF (autocrlf) | 13_patch_check.log |

Controls: reports 9 / 10 / 20 (frames) and 1 / 21 (unit) RED (test_gfx_dialogs); showEndOfYearScreen 6 and 1 RED (test_s2_6_scenario);
every earlier control of the three steps still RED in their test files (all pass). Blind: none new.

## 5. Win32

SG_BACKEND=win32, one file per process (13_gates.log [7/7], 13_w32_test_*.log): test_gfx 25, credits 47, routing 35, boot_screens 29 + 3
skipped (the SG_GFX_THEMEPACK_COL5 tests, as in S2.7: the runner inherits its environment at spawn), reports 36, dialogs 32,
s2_6_scenario 4. The first s2_6 run failed test_boot_stops_only_at_buy_land on a harness gap, not behaviour: the Win32 runner returns
an empty run_detail, so the `showBuyLandScreen` text cannot be read there (records 1156, no divergence, as x64); the test now judges
that stop by its record when no detail text exists, and passes x64 and Win32.

## 6. Reproduce

```
~/bin/py -3.12 log/roadmap/S2/13_tools/make_mirror.py            # then the hand resolutions of section 1 and the edits (13_shared.patch + new files)
~/bin/py -3.12 log/roadmap/S2/build.py <abs>/port/build/S2_13_head <abs>/port/build/S2_13 --log log/roadmap/S2/13_build.log
sh log/roadmap/S2/13_tools/run_files.sh <abs mirror> <dll> x64 <abs>/log/roadmap/S2/13_x64 test_static.py ...
(cd mirror; SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 <abs>/log/roadmap/S2/13_tools/u1254_check.py)
sh log/roadmap/S2/13_tools/gates.sh                              # rebuilds, segments x64 + Win32, boot, census, Win32 gfx files
~/bin/py -3.12 log/roadmap/S2/13_tools/compare.py segs|boot|census ...
~/bin/py -3.12 log/roadmap/S2/13_tools/make_patch.py <scratch>
```

## 7. Open

- The patch targets HEAD 2c05f02a + waves 1-4; I5's waves 1-4 commit had not landed at the end of this step. When it lands:
  `13_tools/check_on_commit.py HEAD <scratch>` must give rc 0 and `equal to the mirror: yes` (13_merge.md section 0).

- U-1205 the course report's read order has no pinned judge; U-1206 other loops with unread CURSOR records; U-1207 the display's
  format +4 not read. U-1251 (buy land) is what stops cov_r_s2_screens now.
- The 69 grown UNSUPPORTED census rows go away only when S3 draws the course and HUD.
