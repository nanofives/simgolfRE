# S7 merge request, waves 1-3 as ONE merge (worker S7.9, 2026-10-26), base HEAD 78ffb06d

This file REPLACES log/roadmap/S7/MERGE.md (waves 1, 2, 3), 7_merge.md, 5_merge.md and 8_merge.md as the merge instructions: merge
S7 from here only. Everything below was built and measured in the mirror port/build/S7_9_head (`~/bin/py -3.12
log/roadmap/S7/9_mirror.py` = `git archive 78ffb06d` + the S0 junctions + shim/build, re/replay, extracted + S7.8's files from
port/build/S7_8_head + the shared edits of this file; builds in the mirror's own port/build/x64 and port/build/Win32 with
SG_FREETYPE_ROOT=C:\Users\maria\vcpkg\installed). Notes: 9_notes.md. Files: 9_files.txt. It fixes what integrator I7 refused
(log/roadmap/merges/I7.md, Verdicts): (1) test_stuck_delivery_recorded_inflated, (2) Win32 test_equivalence_all, (3) freetype.dll
beside the app.

## 1. One command

```
~/bin/py -3.12 log/roadmap/S7/9_apply.py <root> --files     # <root> = the repo root (shared tree) or a scratch copy
```
It copies section 2's files from the mirror, then applies section 3 in order, anchored, refusing to apply twice (I7's Root helper).
Checked: every patch passes `git apply --check` against HEAD 78ffb06d and against the shared working tree of 2026-10-26 (whose
tick.cpp, tick_hook.h, tick.h, portsim_c.h, portsim_c.cpp, runner32.cpp, CMakeLists.txt, modules.inc, gfx.cpp and gfx.h equal HEAD), and the
script applied to a scratch `git archive 78ffb06d` gives the mirror's bytes in every file it writes (9_notes.md section 5).

## 2. Step-owned files: copy from the mirror port/build/S7_9_head (same paths)

| file | origin |
|---|---|
| port/include/portsim/sim/live.h | S7.8 (S7_8_head) unchanged |
| port/src/sim/live.cpp | S7.8 + S7.9: live.recorder control 10 (the n-th inflated dispatch recorded without its CURSOR record); Core sets / clears the gfx refresh hook (9_gfx.patch) and presents from it instead of a LiveIo override |
| port/test/live.py | S7.8 + S7.9: `stuck_replay`, `StuckReplay`, `STUCK_SCENARIOS`, `run_parallel` (the CLI's per-scenario processes as a function) |
| port/test/test_live.py | S7.8 + S7.9: the stuck test on ui_tour / ui_tour_v, its control (4 cases), the Win32 recording identity, test_equivalence_all through run_parallel |
| port/app/** (CMakeLists.txt, host.*, input_map.*, main.cpp, pacing.*, present.*, saves.*, window.*, tools/) | S7.8 (S7_8_head) unchanged; supersedes the shared tree's untracked copy (S7.4's host / main) |
| port/test/app_replay.py, test_app_input.py, test_app_shell.py, test_app_replay.py | equal in the shared tree and S7_8_head |
| port/test/test_app_live.py, test_app_ingame.py, test_app_present.py | S7.8 (S7_8_head) |

## 3. Shared files (9_apply.py does all of it)

Apply order (9_apply.py): 9_tick.patch, 9_capi.patch, 9_cmake.patch, 9_gfx.patch, then items 5-8.

1. `git apply log/roadmap/S7/9_tick.patch` (16 hunks): port/src/sim/tick.cpp (10), port/src/sim/tick_hook.h (4),
   port/include/portsim/sim/tick.h (2) = 7_tick.patch (wave 1: tk::LiveIo, inert in trace runs) + S7.9 (U-1681):
   `HeadlessTerrain::release` and `portsim::release_headless_terrain(const Ctx &)`, which erase a freed context's blocks from tick.cpp's
   process-wide map. 8_tick.patch (wave 3's `LiveIo::present_point` and its call after the Window::refreshFull drop in TkBase::call)
   is NOT in it any more: the present point moved into the gfx backend (item 4).
2. `git apply log/roadmap/S7/9_capi.patch` (7 hunks): port/include/portsim_c.h, port/src/portsim_c.cpp, port/test/runner32/runner32.cpp =
   7_capi.patch (sg_live_* ABI, runner op 40; S3.2's ops 28..32 are already in HEAD and untouched) + S7.9: `~sg_ctx()` calls
   release_headless_terrain.
3. `git apply log/roadmap/S7/9_cmake.patch`, then touch port/src/registry.cpp:
   - port/src/sim/modules.inc: `SG_MODULE(live)` appended;
   - port/CMakeLists.txt (I7's finding): `add_subdirectory(app)` goes BEFORE S5's FreeType block, and that block's shared-library
     branch gains, after its foreach:
     ```
     if(TARGET simgolf_port)   # roadmap S7: the app links portsim; add_custom_command(TARGET) works only in its own directory
       add_custom_target(simgolf_port_freetype ALL
                         COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:freetype>" "$<TARGET_FILE_DIR:simgolf_port>")
       add_dependencies(simgolf_port_freetype simgolf_port)
     endif()
     ```
     With it freetype.dll sits beside simgolf_port.exe on x64 and Win32 and the running app has that copy in its module list
     (9_ft_modules_*.log); with the copy moved aside the x64 app loads C:\Program Files\Eclipse Adoptium\jdk-21.0.10.7-hotspot\bin\
     freetype.dll from PATH (control, 9_ft_modules_x64_control.log). The standalone app build (`cmake -S port/app -DPORTAPP_SHELL=ON`,
     test_app_shell) gets no copy (it configures without SG_FREETYPE_ROOT), as I7 noted.
4. `git apply log/roadmap/S7/9_gfx.patch` (3 hunks; section 3a): port/src/sim/gfx.cpp (2: the `refresh_hooks()` map and
   `gfx_set_refresh_hook` after `owned()`; the hook call at the end of `b_refresh_full`, after the back buffer copy),
   port/include/portsim/sim/gfx.h (1: `GfxRefreshHook` and the `gfx_set_refresh_hook` declaration before `gfx_progress_fast`). With
   it every Window::refreshFull the backend handles presents in a live run, whichever path issued it (U-1670); live.cpp (section 2)
   sets the hook for its run and clears it on exit; trace runs never set one.
5. UNCERTAINTIES.md: append the rows of 1, 2, 3, 4, 6, 7, 5, 8 and 9 _uncertain.tsv (U-1600..U-1606, U-1610..U-1619, U-1620..U-1626,
   U-1630..U-1637, U-1650..U-1659, U-1660..U-1664, U-1640..U-1643, U-1670..U-1673, U-1680..U-1685); status edits: U-1600, U-1601,
   U-1604 resolved, U-1611 / U-1612 path and status, U-1613 / U-1614 path additions (texts as I7's apply_shared.py), U-1642 and U-1643
   resolved by S7.8, U-1670 resolved by S7.9 (texts in 9_apply.py). Already resolved in their TSV: U-1661, U-1664, U-1672, U-1681.
6. re/analysis/sim/mainLoop_structure.md row P6 (8_merge.md section 4): `the display present slot [[0x0083ad50]+0x9c] (0x00418cd1)`
   -> the Display::setScale(1, 1, 1) text (9_apply.py P6_NEW).
7. CLAUDE.md: five paragraphs before `Function bodies are not always contiguous` (after the S5 paragraph), in this order: S7 wave 1
   (MERGE.md section 2), S7.4 (4_merge.md section 4), S7.5 (5_merge.md section 4), S7.8 (8_notes.md section 6), S7.9 (9_notes.md
   section 7).
8. `git add -f` the notes: log/roadmap/S7/files.txt lists waves 1-3's log files, 9_files.txt this step's.

## 3a. Routing: 9_gfx.patch edits another section's shared files

9_gfx.patch changes port/src/sim/gfx.cpp and port/include/portsim/sim/gfx.h, the gfx backend S2 (body families) and S3 (terrain
family gfx_terrain.cpp, which shares gfx.cpp's state) work in; S7 owns neither. The change is additive: one per-context hook map, one
setter, and one call at the end of the Window::refreshFull body; no body, family table or drawing result changes (segments 205 / 205
and the boot gate unchanged, every gfx test file of the suite PASS on x64 and Win32). It applies to HEAD 78ffb06d and to the shared tree
of 2026-10-26 (`git apply --check`); if S2 or S3 edits gfx.cpp first, re-run `~/bin/py -3.12 log/roadmap/S7/9_mirror.py --patches`
against the new base or apply the two hunks by hand.

Not a merge edit, still open to S3 (8_merge.md section 3, U-1673): LiveTerrain::set_keep_log / calls(), after which host.cpp's
TerrainLogBound drops its const_cast.

## 4. Gates (mirror, log/roadmap/S7/9_*.log / .tsv; baseline 78ffb06d = I7b's log/roadmap/merges/I7/b/*.tsv)

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

## 5. Reproduce

```
~/bin/py -3.12 log/roadmap/S7/9_mirror.py && ~/bin/py -3.12 log/roadmap/S7/9_mirror.py --build --w32 --ft
sh log/roadmap/S7/9_gates.sh                     # every gate, one heavy step at a time (9_g_<step>.log)
~/bin/py -3.12 log/roadmap/S7/9_compare.py segs|boot|equiv|app <a.tsv> <b.tsv>
~/bin/py -3.12 log/roadmap/S7/9_mirror.py --patches     # regenerate 9_tick / 9_capi / 9_cmake.patch from the mirror
```
