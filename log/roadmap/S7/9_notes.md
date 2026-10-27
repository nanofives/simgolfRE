# S7.9 notes: S7 waves 1-3 resubmitted as one merge on HEAD 78ffb06d, with I7's three fixes (worker S7.9, 2026-10-26)

Row S7.9 of re/roadmap/S7_steps.tsv, run on claude2 (opus). No game run. Base: HEAD 78ffb06d (S2 waves 6-7 + S5 merged by I7; S7 refused).
Mirror port/build/S7_9_head (`~/bin/py -3.12 log/roadmap/S7/9_mirror.py`: `git archive 78ffb06d`, the S0 junctions + shim/build, re/replay,
extracted, S7.8's files from port/build/S7_8_head, 7_tick + 8_tick + 7_capi applied, SG_MODULE(live), I7's CMake edit). Builds: the
mirror's own port/build/x64 and port/build/Win32 (`9_mirror.py --build --w32 --ft`, i.e. port/test/build.py with
SG_FREETYPE_ROOT=C:\Users\maria\vcpkg\installed, FreeType 2.14.3 found on both arches); the app tests build their standalone apps in
the mirror's port/build/S7_6_app / S7_5_app themselves. Environment: `. log/roadmap/S7/9_env.sh [win32]`. Merge: 9_merge.md (replaces
MERGE.md waves 1-3, 7_merge.md, 5_merge.md, 8_merge.md), one script 9_apply.py. Files: 9_files.txt. U-rows: 9_uncertain.tsv
(U-1680..U-1685).

Mirror pitfall found on the way: `git apply` run inside the mirror (port/build is ignored inside the repo) finds the repo and SILENTLY
skips every path outside the current directory; 9_mirror.py applies with GIT_CEILING_DIRECTORIES set (9_mirror_create.log shows the
first, skipped run and the applied one).

## 1. Fix 1: test_stuck_delivery_recorded_inflated (I7 refusal 1)

cov_a_keys takes no stuck delivery on this base (9_inflated_probe_x64.log: inflated 0, as I7 measured; U-1680 records which
scenarios lost theirs since b3d61e41). Over the three scenarios I7 named (9_stuck_probe_x64.log, the live run over the corpus
trace, then sg_run_boot over its recording):

| scenario | records | live inflated | replay deliveries at another cursor count | verdict |
|---|---|---|---|---|
| ui_tour | 2,478 | 4 | 4 | PASS |
| ui_tour_v | 2,460 | 4 | 4 | PASS |
| cov_r_s2_screens | 1,268 | 4 | 5 | the extra one: U-1682 |

The old assertion (`inflated >= 1` on top of an equivalence PASS) did not judge the mechanism: a new control shows the frame records
cannot see it. live.recorder control 10 (live.cpp write_batch, mode 10 + 16 n) records the n-th inflated dispatch WITHOUT its CURSOR
record, so the replay hands it to the loop's earlier call at the same cursor count. On ui_tour all four n give a different recording
(9_ctl10_probe.log: 4 distinct sha256) and the replay still equals the live run in every frame record and every gfx drawing-call count
(stuck_draws, 9_stuck_probe logs before the rewrite): the two calls fall in one pass of a screen loop. What sees it is TraceInput's
own accounting (tick.cpp batch: `misaligned`, reported by run_frames_detail as "N at another cursor count"): plain replay 4, control 3
(9_detail_probe.log). So the new judge (live.py `stuck_replay`): the live run equals sg_run_boot over the corpus trace, the replay of the
recording equals the live run, violations 0, and the replay's misaligned count equals the live run's inflated count. Bisection
(9_stuck_bisect.py, cov_r_s2_screens): the live inflated dispatches are at records 318, 342, 366, 515 and the replay's misaligned ones
at 318 (two), 342, 366, 515; TraceInput counts a delivery with MORE cursor reads than recorded as misaligned too, so the equality
holds on ui_tour / ui_tour_v only and the test uses those two (U-1682 for the fifth).

Tests (port/test/test_live.py): `test_stuck_delivery_recorded_inflated[ui_tour|ui_tour_v]` (lib64), `test_stuck_cursor_record_left_out_red`
(control 10 at n = 0 and 3 on both: FAIL with replay count = inflated - 1 and replay_vs_live empty: 4 / 4 RED), and
`test_stuck_recording_backends_identical` (lib64 + lib32: the Win32 live recording of ui_tour equals x64's byte for byte; the Win32
runner serves neither count, U-1684).

## 2. Fix 2: Win32 test_equivalence_all, and what the runner kept (I7 refusal 2, U-1681)

Retention explained: tick.cpp's HeadlessTerrain keeps its blocks (16 MB operator-new arena, the 1.4 MB Terrain object block of segment
images, the list sentinel) in a process-wide `std::map<const Ctx *, Blocks>`, and nothing erased an entry when its context was freed. A
context the heap placed at an address with no entry made a new 16 MB one; a context reusing an address reused the entry (re-zeroed and
re-mapped). Every equivalence row makes three contexts (boot, live, replay), so a runner grew by up to 48 MB per scenario, and whether
it did depended on the build's heap layout (I7's run_leak_probe.log: the boot runs in one build, the live runs in the other).

Fix (shared, in 9_tick.patch / 9_capi.patch): `portsim::release_headless_terrain(const Ctx &)` (tick.h, tick.cpp
HeadlessTerrain::release erases the entry) called by `~sg_ctx()` (portsim_c.cpp), i.e. by sg_state_free on both backends. No run
changes: an entry was always re-zeroed and re-mapped before use. Measured on ONE Win32 runner (I7's run_leak_probe.py /
runner_res_probe.py on the mirror):

| run | fixed build | control: the release taken out |
|---|---|---|
| cov_r_bio, 4 boot + 4 live runs | 4 MB after every run (9_leak_probe_fixed.log) | 20 MB from the first run (9_leak_probe_control.log) |
| 8 scenarios' equivalence (cov_a_build2 .. cov_b_people) | 4-5 MB throughout (9_runner_res_fixed.log) | 52 -> 117 MB (9_runner_res_control.log) |

And the test no longer depends on it: `test_equivalence_all` runs one process per scenario (live.py `run_parallel`, the CLI's `--jobs`
path made a function; each process loads its own library, so on Win32 each scenario gets a fresh runner), 3 at a time, and checks the
70 rows' status, boot_status and live_capture_status. The app's Ctx (port/app/main.cpp, one per session) is not an sg_ctx and does not
call the release (one entry per process).

## 3. Fix 3: freetype.dll beside the app, and the present point moved into the gfx backend (U-1670)

CMake (9_cmake.patch, I7's working edit): `add_subdirectory(app)` before S5's FreeType block, whose shared-library branch gains an
`if(TARGET simgolf_port)` copy target. Module lists of the running app (9_ft_modules.py: launch on the title menu, EnumProcessModulesEx,
kill the own PID): x64 and Win32 load port/build/{x64,Win32}/app/Release/freetype.dll (9_ft_modules_x64.log, _w32.log) although PATH
holds C:\Program Files\Eclipse Adoptium\jdk-21.0.10.7-hotspot\bin\freetype.dll; control: with the copy moved aside the x64 app loads
the JDK's (9_ft_modules_x64_control.log).

Wave 3 had never run on a base with S2.7's boot screens (I7 merged waves 1-2 only). On 78ffb06d both test_app_live::test_presents_replay
and test_app_ingame's present gate failed on present 0 (9_g_suite_x64_run1.log): S2.7's boot draws the title art and the loading
screens, and the loading screens (menu.cpp load_flic) and every modal screen that draws through its own module (`gx` in buyland,
chareditor, dialogs, holestats, menu, reports, routing, saveload) issue Window::refreshFull straight to the backend, never through
TkBase::call where 8_tick.patch put the present point. So the app presented the title at position 2 and nothing until the menu, while
the replay's snapshot at position 2 already showed a loading screen (9_present0_probe.log: app 0xb5f6..., replay 0xbc9e...; the app's
own `--live-trace` run reproduces it: snapshot 0xbc9e, its back buffer 0xb5f6). Fix (9_gfx.patch, gfx.cpp / gfx.h; 9_tick.patch no
longer carries 8_tick's hunks): `gfx_set_refresh_hook(ctx, fn, arg)`, called at the end of the backend's Window::refreshFull body (the
only writer of the back buffer), whatever path issued it; live.cpp's Core sets it for the run and clears it on exit; trace runs never
set one. After it (9_present0_probe2.log) a title session presents 5 frames, all 'r': the title, three loading screens and the menu, and
the replay equals presents 1-4; presents 0 and 1 share position 2 (no clock read between the title's refresh and the first loading
screen's), which no clock-ordinal snapshot can tell apart (U-1685). test_app_live / test_app_ingame now judge the last present at each
position and require the shared ones to be exactly [present 0]: 13 passed, 12 passed + 1 xfail (9_apptests_fix.log).

## 4. Gates (mirror; baseline 78ffb06d = I7b's log/roadmap/merges/I7/b/*.tsv, S7's own rows = I7's live_equiv / app_replay TSVs)

| gate | measured | log |
|---|---|---|
| port suite x64, one file per process (75 files, all port/test/test_*.py incl. test_live and the six test_app_* files) | 75 / 75 PASS; test_live 39 passed (incl. test_equivalence_all), test_app_live 13, test_app_ingame 12 + 1 xfail (U-1640), test_app_present 7, test_app_shell 18, test_app_input 16, test_app_replay 14, port test_static 6 | 9_g_suite_x64.log, 9_suite_x64/ |
| port suite Win32 (I7's 58-file list + test_buyland's slot: 59 files) | 50 PASS; the same 8 files with the same 20 failures as I7's record (browser 1, course_menu 1, holestats 2, integrator 4, save_dialogs 1, saveload 6, semantic_input 4, tick 1; the failing test ids are I7's, 9_win32_failed_ids.txt); test_live 38 passed + test_equivalence_all, whose 12 ERROR rows came when the 2-hour limit of the wrapper shell stopped its child processes, re-run alone below | 9_g_suite_win32.log, 9_suite_win32/ |
| test_equivalence_all (slow), x64 / Win32 | PASSED (70 scenarios, one process each) 15 min / 21 min | 9_g_equivall_slow_x64.log, _win32.log |
| `port.test.live equiv --all --jobs 3` x64 / Win32 | 70 rows: 68 PASS, 2 NO_FRAMES on both; 0 rows differ from I7's live_equiv_x64.tsv, 0 rows differ between backends | 9_equiv_x64.tsv, 9_equiv_win32.tsv |
| `port.test.app_replay gate` x64 / Win32 | hole 340 / 340, report 205 / 205, saveload 266 / 266 PASS; 0 rows differ from I7's | 9_app_replay_*.tsv |
| segments x64 / Win32 | 205 / 205 PASS both; 0 rows differ from 78ffb06d (I7b's segs_*.tsv, digests included) | 9_segs_*.tsv |
| boot gate x64 / Win32 | 70 rows: 66 PASS, 2 KNOWN, 2 NO_FRAMES both; 0 rows differ from 78ffb06d (I7b's boot_*.tsv) or between backends | 9_boot_*.tsv |
| repo static (mirror root) | 614 passed, 2 skipped | 9_g_repo_static.log |

Run hygiene: one heavy step at a time (9_gates.sh, steps split into invocations under the 2-hour background limit; step 4 on with a
free-commit floor of 8 GB, MINFREE, as sibling sessions held the free commit charge at 14-15 GB). Comparisons: 9_compare.py.

## 5. Merge check

9_apply.py on a scratch `git archive 78ffb06d` (port/build/S7_9_apply): the four patches apply (`git apply --check` also passes on the shared working tree), 61 UNCERTAINTIES rows,
10 status edits, the P6 correction and the five CLAUDE.md paragraphs; `diff -rq` of its port/ against the mirror's port/ (build and corpus
excluded) is empty (9_apply_scratch.log).

## 6. Controls

| control | result |
|---|---|
| live.recorder 10 (inflated dispatch without its CURSOR record), ui_tour / ui_tour_v n = 0, 3 | 4 / 4 RED (replay count inflated - 1); frame records and drawing calls blind (section 1) |
| the release taken out of ~sg_ctx (one Win32 runner) | RED: 52 -> 117 MB over 8 scenarios, 20 MB after one cov_r_bio run (section 2) |
| freetype.dll moved aside from the x64 app dir | RED: the app loads the JDK's freetype.dll (section 3) |
| S7.1-S7.8's controls (test_live NEGCTL rows, idle-pass, dropped batch, U-1604, test_app_* controls) | in the suite (section 4) |

Blind: none new beyond the frame-record / drawing-call blindness of control 10, which the count check covers.

## 7. Paragraph for CLAUDE.md

Roadmap S7.9, S7 waves 1-3 merged as one (2026-10-26, notes log/roadmap/S7/9_notes.md, merge 9_merge.md / 9_apply.py, U-1680..U-1685):
rebased on 78ffb06d with I7's three refusals fixed. (1) cov_a_keys no longer takes the live core's stuck delivery (U-1680); the test
now runs ui_tour / ui_tour_v (4 inflated dispatches each) and judges the call through TraceInput's count of deliveries at another cursor
count, which must equal the live run's inflated count: live.recorder control 10 (the CURSOR record left out) is RED there and blind in
every frame record and drawing call. (2) tick.cpp's HeadlessTerrain kept its 16 MB arena per freed context in a process-wide map keyed
by the Ctx address, so one Win32 runner grew up to 48 MB per equivalence scenario and died near 2 GB: ~sg_ctx now calls
release_headless_terrain (one runner stays at 4-5 MB over 8 scenarios, 52 -> 117 MB without it), and test_equivalence_all runs one
process per scenario (live.py run_parallel). (3) port/CMakeLists.txt adds the app before S5's FreeType block, which copies freetype.dll
beside simgolf_port (the x64 app had loaded a JDK's from PATH). Since S2.7 the boot's loading screens and the module-drawn modal screens
refresh outside TkBase::call, so the app presented none of them: the present point is now a refresh hook the gfx backend calls at the end
of every Window::refreshFull body (gfx_set_refresh_hook, set by the live core for its run only; U-1670 resolved); the boot's title and
first loading screen share one recording position, so the app tests judge the last present at each position (U-1685). Gates: port suite x64 75 / 75 files, Win32 the known 8 files / 20 failures, test_equivalence_all x64 + Win32,
live equivalence 68 PASS + 2 NO_FRAMES x64 = Win32, app replay 3 / 3, segments 205 / 205 and boot 66 / 2 / 2 on both backends with 0 rows
differing from 78ffb06d, repo static 614 / 2.

## 8. Commands

```
~/bin/py -3.12 log/roadmap/S7/9_mirror.py                       # create the mirror (once)
~/bin/py -3.12 log/roadmap/S7/9_mirror.py --build --w32 --ft    # builds
~/bin/py -3.12 log/roadmap/S7/9_mirror.py --patches             # 9_tick / 9_capi / 9_cmake / 9_gfx.patch against HEAD
sh log/roadmap/S7/9_gates.sh [first] [last]                     # the gates (9_g_<step>.log), one heavy step at a time
sh log/roadmap/S7/9_suite.sh x64|win32 [files]                  # the port suite one file per process
( . log/roadmap/S7/9_env.sh; ~/bin/py -3.12 $L/9_stuck_probe.py )               # section 1 table (+ control 10)
( . log/roadmap/S7/9_env.sh win32; ~/bin/py -3.12 $R/log/roadmap/merges/I7/runner_res_probe.py <scenarios> )   # section 2
~/bin/py -3.12 log/roadmap/S7/9_ft_modules.py <simgolf_port.exe> <install> <scratch data dir>                  # section 3
~/bin/py -3.12 log/roadmap/S7/9_apply.py <root> --files         # the merge
```

## 9. Open

U-1680 (which commit moved cov_a_keys' stuck delivery), U-1682 (cov_r_s2_screens' fifth misaligned delivery), U-1683 (the recording
differs with the gfx backend attached), U-1684 (Win32 runner serves no live counts), U-1685 (presents sharing a position); S7.8's
U-1671 (cursor / tooltip presents), U-1673 (S3's set_keep_log request) and S7.5's U-1640 (zoom frames overrun) unchanged.
