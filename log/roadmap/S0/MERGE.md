# S0 merge request: baseline and frame census (lead cmvm8390a20svo115gkycw1d6, 2026-10-24)

Section S0 is DONE: steps S0.1, S0.2 and S0.3 accepted (re/roadmap/S0_steps.tsv); no step held. Files: log/roadmap/S0/files.txt.
Everything was measured on HEAD 4bfefc2a in the export port/build/S0_head (log/roadmap/S0/export.md). The section adds no port code.
Its only code is port/test/framegate.py + test_framegate.py, two new test files that import nothing new from port/src.

## 1. Shared-file edits, in apply order

### 1.1 UNCERTAINTIES.md: append these 11 rows at the end of the table, after the last existing row (currently U-1078)

| U-1100 | - | tooling | [roadmap S0.1] Eleven Win32-backend failures are tests that cannot run on the Win32 runner as written: test_semantic_input.py:215 `_spec` builds `LibSpec(backend="x64", dll=str(portlib.path))` and under SG_BACKEND=win32 `portlib.path` is portsim_runner32.exe, so `CDLL` raises WinError 193 (4 tests here, 4 in test_integrator through the same loader); test_holestats' two `clock queries ... served` tests read `run_detail`, which is '' because Lib32 has no `.dll` (integrator.run_detail returns '' when `dll` is None); test_tick::test_run_frames_reports_why_it_stopped reads `lib.dll.sg_run_frames_detail` (NoneType). Read from the tracebacks in log/roadmap/S0/1_suite_win32/ and the code at HEAD 4bfefc2a. | Make the three helpers backend-aware (spec from the backend, run_detail over a runner op) or mark the tests x64-only; until then the 11 failures are the Win32 baseline. | open |
| U-1101 | - | tooling | [roadmap S0.1] Nine Win32-backend test failures are not diagnosed: test_golden_digests in test_browser (20 scenarios differ from the pinned digests), test_course_menu (3: save_dialog, load_then_cancel, load_a_game), test_save_dialogs (22) and test_saveload (12), and the five test_saveload `test_save_writes_the_loaded_body_back_byte_for_byte[...]` tests, whose `<tmp>/install/Saved Games/roundtrip.sve` is missing after saveGame ran with `st.files_root(root)` set (x64: present, 5 / 5 pass). The same files pass on x64 with the same pinned digests. A probe (log/roadmap/S0/1_probe_files_root.py) shows files_put followed by files_size / files_read agree on both backends and `files_root` returns None on both; the probe does not run the game's own save, so it does not separate a runner32 write-through defect from a digest that depends on the backend. Observed only: the failing keys are the browser / load / save scenarios. | Run one failing key on both backends with SG_TICK_DUMP / the call log and diff the file operations; check runner32.cpp op 23 (files_root) against the Win32 DLL's write path. | open |
| U-1102 | - | tooling | [roadmap S0.1] The shared port/corpus junction is live: four S1 recordings dated 2026-10-24 (cov_r_bio 08:18, cov_r_palette_hot 05:58, cov_r_theme_cancel 06:55, cov_r_tutorial 06:52; scenario_refs.json entries uncommitted) are in port/corpus/traces and log/p2/p24 with captures, so `boot --all` and test_boot enumerate them. test_boot read 88 passed against 85 recorded (3 extra scenarios present at collection), the boot CLI 65 scenarios (61 + 4; cov_r_bio FAIL 579 of 687 records, sg_run_boot -11 missing exe constants / game file) 3 m 10 s later. The 61 HEAD scenarios are unchanged (58 PASS, 2 KNOWN, 1 NO_FRAMES). A later baseline run in the export sees whatever S1 has added by then. | Judge HEAD by the 61 scenarios of log/p5/boot_gate_loc.tsv (re/roadmap/BASELINE.md section 5); the lead can freeze the traces in the export with a copy instead of a junction if exact reproduction is wanted. | open |
| U-1110 | - | tooling | [roadmap S0.2] framegate.py places a snapshot only by the trace: the frame holding the checkpoint's clock call and the clock-call sites since that frame's head (classify()). At HEAD 4bfefc2a the port exposes no ordering between its drawing calls, its Terrain passes and a gfx snapshot: gfx.log gives counts and the distinct unsupported reasons as a set, the LiveTerrain log has no clock position, and tick's SG_TICK_TRACE (every TkBase call) costs about 17,000 lines per in-game frame (sandbox_basic 150 MB, 49 s against 7.5 s), too much for the census. | Merge 2_merge.md items 1-2 (gfx.cpp writes its first-occurrence list and a per-snapshot terrain flag into gfx.log); framegate.py reads them when present (columns unsup_order = gfx, terrain_rule + snapshot_terrain). | open |
| U-1111 | - | tooling | [roadmap S0.2] The terrain rule calls every checkpoint of an in-game frame `yes` unless the context names a full-screen modal (showCreditsScreen) or the frame left for the title menu. Five MODAL:waitTicks checkpoints are full-screen screens whose clock reads are waitTicks' only (cov_a_career career_editor, cov_a_golfer button_0, cov_a_keys3 q_editor, cov_a_keys k_worldmap, cov_k1_routing routing: reference frames viewed); the trace cannot tell them from popups over the course view (16 others viewed). They read terrain yes. | The gfx snapshot_terrain flag of 2_merge.md item 2 (a full-screen 2D write after the frame's terrain pass clears it) decides them from the run. | open |
| U-1112 | - | tooling | [roadmap S0.2] Screen kinds of the boot frame come from the waitTicks period of the loop the checkpoint sits in: waitTicks(1, 0) = the title menu (tick_menu.inc ML+0x11a69), 5 ticks or a last read in pumpAndRedraw with courseActionMenu.setup drawn = the location screen (courseActionMenu; the 5-tick wait is measured on cov_b_location and cov_l_point, the port's courseActionMenu drops its waits). Another boot screen waiting 1 or 5 ticks per pass would get the title or location regions; none of the 39 boot checkpoints of the 61 traced scenarios does (2_context_census.tsv), so this is not observed. | Name the screen from the run (gfx notes which screen body drew last) when the backend draws more boot screens. | open |
| U-1113 | - | tooling | [roadmap S0.2] unsup_order: without the gfx first-occurrence list the order of the unsupported reasons comes from the probe run's TkBase trace (first call of the reason's function, SG_TICK_CALL_LIMIT 3,000,000 = about 175 in-game frames). A body's own reason (`Surface_stretchBlit: a SpriteRef the backend has no sprite for`) occurs at or after its function's first call, drops that modules hand to gfx_drop directly (saveload.cpp gx) are never traced, and reasons first seen after the limit are unordered; such rows read `partial` (course_season sandbox .. season_120) or `final` when every reason of the run came before the snapshot. | 2_merge.md item 1. | open |
| U-1114 | - | tooling | [roadmap S0.2] gfx.render (port/test/gfx.py) runs the boot through (frames up to the last checkpoint) + 1 records. A checkpoint in a frame's H1 wait is snapshotted at the next cursor read or dispatch past it, which lies in the following frame's body, so a last checkpoint in an H1 wait gets no snapshot (course_season hole_open alone: NOT_RUN). framegate.py runs one frame further and to the end of the trace when that passes the last FRAME record (cov_r_quit's title and credits checkpoints). | 2_merge.md item 4 (same fix in gfx.render); test_gfx's checkpoints are not affected (menu and cov_b_location are in the boot frame). | open |
| U-1115 | - | tooling | [roadmap S0.2] re/analysis/data/scenario_refs.json lists 609 checkpoints in its 64 scenarios (the summaries agree), not 649 as the S0.2 / S0.3 rows say; 34 of them (cov_a_quit 15, cov_a_quit_title 9, golfer_events 10) have no trace in port/corpus/traces and read NOT_RUN MISSING. | The lead corrects the count in the S0.3 row; traces for the three scenarios are a phase 2.4 matter (U-0681 for golfer_events). | open |
| U-1120 | - | tooling | [roadmap S0.3] cov_a_champ, championship and cov_k3_career: sg_run_boot returns -11 (missing exe constants / game file) after every record (573/573, 2305/2305, 60/60) as soon as one gfx.snapshotAt is pending, with or without LiveTerrain; the same boot without snapshotAt (boot gate, and gfx.attach alone) passes with code 0. framegate marks the 22 checkpoints NOT_RUN (bad_boot: div is None). The sim hashes of all records match. 3_rerun_noend.py re-labels that boot KNOWN and keeps the rows whose snapshot was taken (17 of 22: 15 UNSUPPORTED, 2 PASS); 5 checkpoints took no snapshot and stay NOT_RUN. Cause of -11 not found (the call path that raises it at end of input with a snapshot pending). | Find the call path that returns -11 at the end of input while a gfx.snapshotAt is pending (gfx_end_of_input in port/src/sim/gfx.cpp, the boot loop in tick_boot.inc); until then 3_rerun_noend.py labels those boots KNOWN. | open |
| U-1121 | - | criterion | [roadmap S0.3] terrain_verdict (G2): the terrain pass draws no pixels at HEAD, so the denominator is the in-game view region's area (334,888 px). Of 521 terrain-yes rows with a snapshot 1 PASS (cov_a_keys/k_worldmap, view 0 px) and 520 FAIL (view one-level 0.004..4.292 % of area; the 70-px beyond-one-level limit is the usual failure). championship/tournament_end has no snapshot. | Re-read the verdict with the drawn-pixel denominator once S3 draws the terrain pass into the back buffer. | open |

### 1.2 port/test/README.md: one paragraph at the end of the phase 5 part (after the gfx / terrain paragraphs)

```
port/test/framegate.py (roadmap S0.2): every checkpoint of a pinned scenario in one boot run with the gfx backend and LiveTerrain
attached. It writes one TSV row per checkpoint: status, screen kind, context, the terrain rule, px per region (one-level / beyond one
level) and the unsupported calls before the snapshot. `~/bin/py -3.12 -m port.test.framegate --all --jobs 2 --out <tsv> --resume`
is the census (re/roadmap/frame_census.tsv, frame_causes.tsv). Tests: test_framegate.py.
```

### 1.3 CLAUDE.md: append this paragraph after the "Phase 5 step 2, the location screen" paragraph

```
Roadmap S0, baseline and frame census (2026-10-24, re/roadmap/BASELINE.md, frame_census.tsv, frame_causes.tsv, notes
log/roadmap/S0/, U-1100..U-1102, U-1110..U-1115, U-1120 / U-1121). Every regression gate was measured on HEAD 4bfefc2a in a
git-archive export (port/build/S0_head, with junctions to the gitignored data), so the S1 / S4 / S6 working-tree edits were not in it.
Results: repo static 600 / 2 skipped, port suite x64 41 / 41 files, segments 205 / 205 on x64 and Win32 with 0 digests changed, boot
gate 58 PASS / 2 KNOWN / 1 NO_FRAMES on the 61 HEAD scenarios, tick 250, sve_roundtrip 15, gl_raster 139, terrain_state 500,
terrain_render 40, terrain_gl_setup 6, gfx 17, port static 6, terrain_live 144. The Win32 backend suite has no record after phase 3.R:
20 failures in 8 files are its baseline. Of these, 11 are x64-only helpers (U-1100) and 9 are browser / load / save digests and
files not yet diagnosed (U-1101). port/corpus is shared, so `boot --all` also runs uncommitted recordings: count the HEAD scenarios
only (U-1102). port/test/framegate.py runs every checkpoint of a scenario in one boot with gfx.attach and LiveTerrain attached. It
writes one row per checkpoint with: status PASS / UNSUPPORTED / FAIL / NOT_RUN; screen kind (title, location, ingame, frame); the
terrain-on-screen rule from the trace's clock sites (U-1110 / U-1111); px per region (in-game: hud_title, hud_meters, hud_message,
toolbar, bottom_panel, view); one-level and beyond-one-level px; the unsupported drawing calls before the snapshot. LiveTerrain at
an address of its own leaves every sim hash equal. The census over HEAD's 609 checkpoints (64 scenarios) gives PASS 45 (title menu,
location screen and one world map), UNSUPPORTED 525 and NOT_RUN 39. Of the 39, 34 have no trace (cov_a_quit, cov_a_quit_title,
golfer_events) and 5 got no snapshot. U-1120 covers those 5: with a gfx.snapshotAt pending, sg_run_boot returns -11 at the end of
cov_a_champ, championship and cov_k3_career. Terrain checkpoints are 522, and 520 of them rank under `terrain not drawn`. Terrain
pixels never reach the gfx back buffer at HEAD, because terrain_state's draw passes only set the view quadrant. The G2 criterion
(1.5 % one-level, 70 px beyond one level, glraster constants) is applied to the view region's area until S3 draws the terrain
(U-1121). 2D causes (the first unsupported drawing call): Widget_writePixel 316, Surface_blit3 with an unmodelled source or
destination 168, Image_draw with a SpriteRef the backend has no sprite for 39, Surface_drawScaled2 2. No non-PASS row lacks an
unsupported call.
```

## 2. Recommendations for S2 / S3 (port code; not integrator edits, nobody owns them in S0)

From log/roadmap/S0/2_merge.md (S0.2) and 3_notes.md. Each one makes the census exact from the run instead of from the trace:
1. gfx.cpp `a_log`: write the unsupported reasons in first-occurrence order and drop the 64-reason cap (U-1110, U-1113). framegate then
   skips its probe run, about 40 s per scenario.
2. gfx.cpp + terrain_dll.cpp `update_terrain_render`: a per-snapshot terrain flag (`gfx_note_terrain`; cleared by a full-screen 2D write,
   copied at refreshFull; `snapshot_terrain\t<k>\t<0|1>` in gfx.log) (U-1110, U-1111: five full-screen modals read terrain yes today).
3. gfx.cpp `kNotDrawing`: add playSound, Snd::status, capsLockOff, emptyFunc_4a0890, saveCourseJpg, Widget_readPixel, enterModal483d30
   (tick.cpp kDrops reasons say they do not draw).
4. port/test/gfx.py `render`: `max_records = frames + 2` and `to_end` past the last FRAME record (U-1114; framegate already does it).
5. runner32 / lib32.py: sg_ts_live ops so LiveTerrain also attaches on Win32 (no census status depends on it at HEAD).
6. U-1120: the -11 at end of input with a snapshot pending (gfx_end_of_input).
framegate.py reads items 1 and 2 when gfx.log has them and keeps its trace rules otherwise.

## 3. Section gate: commands and expected numbers (from the export root, or the tree after merge)

```
~/bin/py -3.12 -m pytest port/test/test_framegate.py -q -p no:cacheprovider          # 32 passed (about 42 s)
~/bin/py -3.12 -m port.test.framegate --all --jobs 2 --out <tsv> --resume            # 609 rows at HEAD: PASS 45, UNSUPPORTED 525, NOT_RUN 39
~/bin/py -3.12 log/roadmap/S0/3_rerun_noend.py <scenario>   (cov_a_champ, championship, cov_k3_career; U-1120)
~/bin/py -3.12 log/roadmap/S0/3_rank.py                     # writes frame_census.tsv / frame_causes.tsv (rankings sum to 564 non-PASS)
```
The regression gates and their numbers are re/roadmap/BASELINE.md (sections 1-6 and "What a later run must hold"). The census is not a
regression gate: S2 / S3 re-run it after their merges and expect PASS to rise.
Controls (S0.2): gfx negctl 1 on sandbox_basic/menu FAIL 49,162 px; a swapped reference on cov_b_location/buttons FAIL 1,412 px; blind:
the region rectangles. Win32 sample (S0.3): 13 rows (9 non-PASS, 4 PASS) equal to x64.

## 4. For the orchestrator

- Commit with `git add -f` for the log/roadmap/S0 paths (log/* is ignored). Leave log/roadmap/S0/3_census.tsv out: 517 KB, the raw first run;
  re/roadmap/frame_census.tsv is the merged result.
- The S1 scenarios (cov_r_theme_cancel, cov_r_palette_hot, cov_r_tutorial, cov_r_bio) are not in this census. Re-run framegate on them after S1 merges.
- The export port/build/S0_head (gitignored) can be deleted once S0 is committed. Its junctions point into the shared tree: delete the
  junctions first (`cmd /c rmdir <junction>`), never recursively through them.
