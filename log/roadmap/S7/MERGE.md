# S7 merge request, wave 1 (lead S7, 2026-10-25): live core + Windows app, steps S7.1 S7.2 S7.3 S7.4 S7.6 S7.7

Base: HEAD 2c05f02a. Wave 1 is self-contained: the app plays from the title menu into the sandbox. Wave 2 (S7.5 in-game presentation and
the play session) comes later as its own merge.

## 1. Apply, in this order (exact text and locations: log/roadmap/S7/7_merge.md, which replaces 1_merge.md and 6_merge.md)

1. Copy the four live files from the mirror port/build/S7_7_head (same paths; also in log/roadmap/S7/7_live/):
   port/include/portsim/sim/live.h, port/src/sim/live.cpp, port/test/live.py, port/test/test_live.py.
2. `git apply log/roadmap/S7/7_tick.patch` (tick.cpp, tick_hook.h, tick.h) and `git apply log/roadmap/S7/7_capi.patch`
   (portsim_c.h, portsim_c.cpp, runner32.cpp). Both pass `git apply --check` on 2c05f02a and on the shared tree with S3.2's
   uncommitted runner32.cpp. The live runner op is 40, because S3.2 takes ops 28..32 (U-1661). Keep the numbers apart when S3 merges.
3. port/src/sim/modules.inc: append `SG_MODULE(live)`, then touch port/src/registry.cpp.
4. port/CMakeLists.txt: append
   ```
   # Roadmap S7: the Windows host (input translation library and its trace checker; S7.3 adds the app executable).
   add_subdirectory(app)
   ```
5. Stage the untracked port/app/** and port/test/test_app_input.py, test_app_shell.py, test_app_live.py as they are in the shared tree.
6. UNCERTAINTIES.md: append the rows of 1_uncertain.tsv, 2_uncertain.tsv, 3_uncertain.tsv, 6_uncertain.tsv, 7_uncertain.tsv
   (U-1600..U-1606, U-1610..U-1619, U-1620..U-1626, U-1650..U-1659, U-1660..U-1664). Status: U-1600, U-1601, U-1604, U-1661, U-1664 resolved.
   log/ files are ignored: stage the notes with `git add -f` (files.txt lists them).

7. S7.4 (added 2026-10-25): its two patches (4_window.patch, 4_input_map.patch) are ALREADY APPLIED to the shared port/app by the
   lead, so the shared port/app equals the measured mirror port/build/S7_4_head; do not apply them again. Stage also
   port/test/app_replay.py and port/test/test_app_replay.py. UNCERTAINTIES.md: append 4_uncertain.tsv (U-1630..U-1637) and change
   S7.2's U-1611 / U-1612 / U-1613 / U-1614 exactly as log/roadmap/S7/4_merge.md section 3 says. CLAUDE.md: add 4_merge.md section 4's
   paragraph after the one below.

## 2. CLAUDE.md paragraph (after the S8 paragraph)

Roadmap S7, the Windows host, wave 1 (2026-10-24/25, log/roadmap/S7, plan re/roadmap/S7_steps.tsv, U-1600..U-1664): `port/app` builds
`simgolf_port.exe` (port/CMakeLists.txt add_subdirectory(app)), which plays from the user's install (`--install <dir>`, saves under its
own `--data` dir, never in the install; no original binary loaded). LIVE CORE (module `live`, port/src/sim/live.cpp, live.h; tick.cpp
asks a LiveHost through tk::LiveIo, inert in trace runs): the frame loop takes the frame-head clock, every clock read, pump batches,
cursor reads and GetAsyncKeyState from a host on demand and records every answer as a schema v1 trace plus framehash.bin; a host
that answers from a corpus trace equals sg_run_boot in 68 of 70 boot scenarios plus 2 NO_FRAMES, x64 = Win32, 0 replay
differences; idle passes of unpaced modal loops (title menu hover) are recorded so the replay takes the same passes (U-1601);
S1's world-load pump and focus-child keys are handled (U-1604). INPUT (port/app/input_map.*): Windows messages to root-handler
commands, cursor and async-key answers, 0 mismatches over all 26,861 MSG records of the 67 corpus traces. APP (host.*, window.*,
present.*, pacing.*, saves.*): sim thread above normal priority, frame wait in real time (355 periods 87..100 ms, median 93; 125..135
with g_flags bit 17), the gfx back buffer presented at every frame wait and modal pump (title menu 0 px against the original's
sandbox_basic `menu` checkpoint at 96, 144 and 240 dpi), idle on the title menu 3.1 % of one core, every session recorded and
replayed through sg_run_boot equal in every record (422 / 422). Tests: port/test/test_live.py (32 x64, 32 Win32), test_app_live.py
13, test_app_shell.py 18, test_app_input.py 16. Open: U-1610..U-1619 (double click, wheel, Alt, modifiers, chars above 0x7f
unmeasured), U-1620..U-1626, U-1650 (bio EditBox keys from the real keyboard), U-1660, U-1662, U-1663.

## 3. Gate commands and expected numbers (measured by S7.7 in port/build/S7_7_head; lead checked the logs)

| gate | expected | log |
|---|---|---|
| `~/bin/py -3.12 -m port.test.live equiv` x64 / Win32 | 70 rows: 68 PASS, 2 NO_FRAMES, 0 rows differ between backends | 7_equiv_*.tsv, 7_equiv_compare.log |
| segments x64 / Win32 | 205 / 205, 0 rows differ | 7_segs_compare.log |
| boot gate | HEAD's 61 at 58 / 2 / 1, 0 rows differ; six S1 / S8 rows PASS | 7_boot_compare.log |
| `pytest port/test/test_live.py` (x64, SG_BACKEND=win32) | 32 / 32 passed | 7_test_live_*.log |
| `pytest port/test/test_app_live.py` | 13 passed | 7_test_app_live.log |
| `pytest port/test/test_app_shell.py`, `test_app_input.py`, port test_static | 18, 16, 6 passed | 7_test_app_*.log, 7_test_static.log |

| `~/bin/py -3.12 -m port.test.app_replay gate [--backend win32]` (S7.4) | hole 340 / 340, report 205 / 205, saveload 266 / 266 PASS on both backends | 4_gate_x64.tsv, 4_lead_gate_w32.tsv |
| `pytest port/test/test_app_replay.py` (S7.4) | 14 passed | 4_test_app_replay.log |

S7.4 cross-check (needs a game run, not a merge gate): recording `hole` replayed into the original on instance s7_4 gives 340 / 340
records equal in every sim object except g_frameMs (a clock value, U-1637), 59 / 59 root-handler calls identical.
Known flake: under heavy machine load the equivalence row cov_l_point failed once with -11 in sg_run_boot and passed alone.
test_app_live.py rewrites log/roadmap/S7/6_app_live_facts.json on every run.

Files: log/roadmap/S7/files.txt.

## Wave 2 (S7.5, added 2026-10-25): the in-game frame in the app

Apply after wave 1 and S3 wave 3 (S3.2), exactly as log/roadmap/S7/5_merge.md says: copy port/app/main.cpp, host.h, host.cpp and the
new port/test/test_app_ingame.py from the mirror port/build/S7_5_head, `git apply log/roadmap/S7/5_test_app_live.patch`, append
5_uncertain.tsv (U-1640..U-1643), and add 5_merge.md section 4's CLAUDE.md paragraph. Gate (mirror b3d61e41 + S3.2 + wave 1):
test_app_ingame 12 passed + 1 xfail (U-1640: frames with two full terrain passes, the zoom keys, work 144..186 ms and overrun the 87 ms
frame; a need for S3's rasteriser speed), test_app_live 13 with the patch, test_live 32, test_app_shell / input / replay 18 / 16 / 14,
live equivalence 68 PASS + 2 NO_FRAMES, segments 205 / 205 0 differ, boot 0 rows differ from b3d61e41's table, census 698 rows equal to
the b3d61e41 counts (82 PASS).

## Wave 3 (S7.8, added 2026-10-25): present at the original's present point, bounded memory in long sessions

Apply after wave 2, as log/roadmap/S7/8_merge.md says: `8_tick.patch` (tick.cpp / tick_hook.h: LiveIo::present_point, called at
Window::refreshFull in a live run only), then copy the step's files from port/build/S7_8_head (live.h, live.cpp, port/app/host.h,
host.cpp, main.cpp, test_app_ingame.py, test_app_live.py, new test_app_present.py); these supersede wave 2's copies of the same files.
Also 8_merge.md's S3 request (LiveTerrain set_keep_log, U-1673) and the mainLoop_structure.md P6 correction (slot +0x9c is
Display::setScale, the window present is Display::present +0xa4 at the end of drawTooltip). Gate: course_season_6y 51,092 records with
framehash.bin equal to the original's capture byte for byte and private bytes flat after frame 2,000; equivalence 68 PASS + 2 NO_FRAMES;
segments 205 / 205 x64 + Win32, 0 digests changed; boot rows equal to S7.5's; test_live 32, test_app_ingame 12 + 1 xfail, shell 18,
input 16, replay 14, test_app_present 7; test_app_live 12 / 13 (test_pacing_held's bit-17 median 138 against 137 under machine load;
S7.5's build fails the same way under the same load). U-rows: 8_uncertain.tsv (U-1670..U-1673); U-1642 and U-1643 resolved.
