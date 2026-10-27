# S7.3 notes: the Win32 app shell (no simulation loop)

Step row: `re/roadmap/S7_steps.tsv` S7.3. Worker: opus high, session cmvmsgnhg3w8ho1151to4vkeg. Status: DONE (gate met, section 2).

## 1. Result (paragraph for CLAUDE.md)

Roadmap S7.3, the app shell (2026-10-24, port/app, notes log/roadmap/S7/3_notes.md, U-1620..U-1626): `simgolf_port.exe` (CMake target
simgolf_port, links the portsim library; `cmake -S port/app -DPORTAPP_SHELL=ON` builds it with port/ on its own, or port/'s
`add_subdirectory(app)`). window.cpp: a per-monitor-DPI-aware (v2) window whose client is MulDiv(800, dpi, 96) x MulDiv(600, dpi, 96)
(or `--dpi`), every key and mouse message through one S7.2 Translator with a live host (GetKeyState / GetAsyncKeyState /
MapVirtualKeyA / GetCursorPos + ScreenToClient / timeGetTime, each answer recorded), RootHandler commands copied from the steps
(to_command), TranslateMessage before dispatch as jgld's pumps do. present.cpp: the 4:3 content rectangle (fit_viewport = the
translator's Viewport), floor mapping logical = (client - origin) * 800 / w, colour (v << 3) | (v >> 2) per channel into a 32-bit DIB
drawn 1:1. pacing.cpp: the H1 frame wait and waitTicks of time_model.md section 3 over an injected clock (frame_target = head + 87, +125
with g_flags bit 17; unsigned tests; VirtualClock gives the measured 99 reads per frame; RealClock sleeps in MsgWaitForMultipleObjects
with timeBeginPeriod 1). saves.cpp: AppFiles, the install read-only + <data>/files/<logical name> for writes, tombstones in
<data>/deleted.lst, `$` names in memory, merged listings, unsafe names and a data directory inside the install refused (before it is
created). main.cpp `--present-trace <scenario|file> [--snapshot-at P] [--max-records N] [--put name=file]` runs tick_run_frames' boot
path (the exe's initialised .data at the address its PE header gives, GDI text + gfx attached) on a worker thread and presents the
snapshot or final back buffer; recording layout <data>/recordings/<stamp>-<pid>/ (session.json, input.jsonl, framehash.bin/.json in
the captures' layout, back.555, gfx_log.txt; trace.trace is S7.6's). Gate (port/test/test_app_shell.py, 18 passed in 126 s): sandbox_basic
`menu` frame = the original's reference = port/test/gfx.py's run (0 px), record 0 = the capture's 203 hashes; the window's client
pixels read from its DC by port/app/tools/app_drive.py equal the frame under the presenter's mapping at 96, 144 and 240 dpi (at 96 the
reference itself); 19 real posted messages per run logged in order, `--translate` re-translation 0 mismatches, every click's logical
point equal to Python's floor mapping; the cov_r_quit boot through the app writes top10.sve, Themes/Default.pro and &QuitSave.sve under
the data directory with the original's compared bytes, `--cat` reads them back, the install's sha256 tree digest unchanged; no original
binary in the module list; selftest 37 rows. Controls RED: present-stale, present-swap, input-bypass, and the selftest's pacing-no-bit17
/ pacing-signed / pacing-quantum, saves-install / saves-noreadback / saves-inside, present-stale / present-swap.

## 2. Gate, measured

Built and judged in the HEAD export `port/build/S7_3_head` (git archive of c6d7d78b, junctions original / log / port/corpus /
tests/golden / tests/fixtures / re/versions / tools/vc6, recipe log/roadmap/S0/export.md) with port/app and the test synced by
`log/roadmap/S7/3_mirror.py` (which also appends S7.2's `add_subdirectory(app)` to the export's port/CMakeLists.txt).

```
~/bin/py -3.12 log/roadmap/S7/3_mirror.py --build        # sync + build simgolf_port, portsim_c (port/build/S7_3_app of the export)
cd port/build/S7_3_head
~/bin/py -3.12 -m pytest port/test/test_app_shell.py -p no:cacheprovider -v   # 18 passed in 125.8 s (log/roadmap/S7/3_test_app_shell.log)
port/build/S7_3_app/Release/simgolf_port.exe --selftest                        # 37 passed 0 failed
```

| gate item | measured |
|---|---|
| present-trace frame vs the original's sandbox_basic `menu` checkpoint | 0 of 480,000 px (snapshot at clock ordinal 16,585, 1 record) |
| same frame vs port/test/gfx.py's run on the same build | 0 px; record 0 T 1,053,435 and 203 sim hashes = log/p2/p24/sandbox_basic/c_a/framehash.bin record 0 |
| client pixels (window DC capture) vs the frame scaled by the presenter's mapping | 0 px at 96 dpi (800 x 600, = reference), 144 (1200 x 900), 240 = this monitor (2000 x 1500) |
| posted real messages logged in order | 19 / 19 per run (plus the WM_CHAR TranslateMessage adds for 'F'); lParam as posted |
| app steps vs offline `--translate` (answers replayed) | 20 messages, 21 handler calls, 0 mismatches, each dpi |
| click logical points vs Python's floor((x - ox) * 800 / w) | 10 / 10 calls (handlers 2, 1, 5, 4) at each dpi; key-up = handler 9, key-down = handler 7 or 8 |
| saves: cov_r_quit boot through the app | top10.sve, Themes/Default.pro (records), &QuitSave.sve equal sve_refs' end files; under <data>/files only; `--cat` = disk bytes; install tree digest (sha256 of all 5,666 files) unchanged; saves_refused 0; "mainLoop returned" |
| data directory inside the install | refused, exit 3, directory not created |
| pacing (selftest, VirtualClock) | 99 reads per 87 ms frame (head 1 + wait 98, 8 waitTicks), 135 per 125 ms frame (11 waitTicks), waitTicks(1) 11 reads, waitTicks(0) none, 10 n ms, interruptible returns 1, unsigned wrap = no wait, long frame = one test |
| pacing (RealClock, idle) | 20 frames: period min 91, max 95, mean 93.2 ms (U-1625) |
| module list (EnumProcessModulesEx) | only simgolf_port.exe and system DLLs; no path under original\, none of golf*.exe, jgld / jgl / Terrain / sound / binkw32 / drvmgt |

Other paths: `cmake -S port` of the mirror (with the merge line) builds simgolf_port, portsim_c and simgolf_input_check
(log/roadmap/S7/3_build_merged.log; its selftest 37 / 37); S7.2's test_app_input.py 16 passed with the new CMakeLists
(log/roadmap/S7/3_test_app_input.log); port test_static 6 passed. Segments / boot gates: not re-run, because no file under port/src or
port/include changed (the app only links the library; portsim_c.dll is built from HEAD's sources) — they are unchanged by construction.

## 3. Controls

| control | how | result |
|---|---|---|
| present-stale (window) | Presenter keeps the first (black) frame | client != frame: RED |
| present-swap (window) | RGB555 bytes exchanged | client != reference: RED |
| input-bypass (window, 144 dpi) | commands from the raw lParam, no translator reads | `--translate` mismatches > 0: RED |
| selftest pacing-no-bit17 | bit 17 ignored | deadline_125 / frame_125 rows FAIL |
| selftest pacing-signed | signed frame-wait test | wrap_unsigned_no_wait FAIL |
| selftest pacing-quantum | waitTicks 1 ms per tick | 99-read / 11-read rows FAIL |
| selftest saves-install | writes into the (temp) install | write_lands_in_data_dir, install_unchanged FAIL |
| selftest saves-noreadback | data dir ignored on read | read_back, shadow FAIL |
| selftest saves-inside | inside-install data dir accepted | both refusal rows FAIL |
| selftest present-stale / present-swap | as above, on the mapping test | 5 mapping rows FAIL |

Arithmetic of the controls: stale and swap change every non-black pixel of a frame with colour (the menu has no all-black screen);
bypass yields no reads, so the scripted host reports unused answers and the steps differ; the saves controls run on a temporary
install in %TEMP% (never original\). No blind control.

## 4. For S7.6

- `AppWindow::set_sink` gives one InputEvent per message (answers, steps, `commands`); feed `commands` to S7.1's live core at its
  dispatch point. `WindowOptions::focus_child_takes` is the U-1616 callback.
- Present a frame with `Presenter::set_frame` + `AppWindow::invalidate` (gfx.dump(0) -> `$gfx/back.555` in AppFiles' memory layer, as
  present_trace does).
- Pacing: `frame_wait(RealClock&, head, g_flags, pump)` at the frame's wait point; `wait_ticks` for the modal waits.
- Record into the session directory main.cpp creates (trace.trace next to framehash.bin).

## 5. Open items

U-1620..U-1626 (`3_uncertain.tsv`): real keystrokes vs posted WM_CHAR (1620), non-integer scale look (1621), WM_DPICHANGED untested
(1622), capture of an off-screen window (1623), sleeping in the wait vs the original's busy pump (1624), pacing under load (1625),
tombstones for install saves (1626).

## 6. Files

`3_files.txt`. Builds only inside the export (port/build/S7_3_head/port/build/S7_3_app, .../S7_3); nothing built into port/build/x64.
Temporary data directories under %TEMP% (test fixtures remove them).
