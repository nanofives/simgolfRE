# S7.6 notes: the app on the live core (worker S7.6, 2026-10-24)

Row S7.6 of re/roadmap/S7_steps.tsv. Base HEAD c6d7d78b, built on S7.1 (accepted, unmerged), S7.2 and S7.3 (accepted). Status: DONE,
every gate item met (section 2). Mirror: port/build/S7_6_head (`~/bin/py -3.12 log/roadmap/S7/6_mirror.py [--build]`: git archive of
HEAD, the S0 junctions + shim/build + re/replay, S7.1's four files and patches, S7.1's modules.inc line, S7.2's CMake line, the
shared tree's port/app and app tests synced in). Merge: 6_merge.md. Files: 6_files.txt. U-rows: 6_uncertain.tsv (U-1650..U-1659).
S1 / S8 had not merged (HEAD is still c6d7d78b), so U-1604 (PumpKind::Load, focus keys 12 / 13) stays open for after their merge.

## 1. Result (paragraph for CLAUDE.md)

Roadmap S7.6, the app on the live core (2026-10-24, port/app/host.h / host.cpp, notes log/roadmap/S7/6_notes.md, U-1650..U-1659;
closes U-1600 and U-1601): `simgolf_port.exe --install <dir>` now plays. A LiveSession (the app's LiveHost) runs portsim::run_live on
a sim thread above normal priority: the window thread queues each message's root-handler commands (S7.2's translator) and a pump
delivers them, never a press and its release in one dispatch (U-1653). The cursor comes from GetCursorPos or, with `--cursor message`,
from the last mouse message. H1's frame wait runs in real time (pacing.h frame_wait over timeGetTime). The back buffer is presented
(gfx.dump) at every frame wait and modal pump call where it changed. Every session writes trace.trace, framehash.bin, presents.tsv,
shown.tsv, frames.tsv and waits.tsv. IDLE PASSES (U-1601): the core simulates TraceInput::batch over a loop's calls since the last
dispatch (live.cpp `taker`). A modal Blocking call is `idle_ok` while a later call at the same cursor count could still take a
dispatch. The title menu's hover loop reads the cursor every pass, so it declines pass after pass, waking on a cursor move or every
`--idle-ms` (default 200). A loop that pumps twice at one count gets one idle pass, then must deliver. That dispatch is recorded
with a CURSOR record in front of it, filled by the port's next read: the replay refuses at the first call and takes it at the second
through the `stuck` rule. The same mechanism records the trace host's stuck deliveries, which closes U-1600: 0 violations in 70
scenarios (S7.1: 25) and every trace-host recording replays (S7.1: 63 of 66). The core writes a clock record after each modal
Blocking pump call and after H1's wait, so a LiveMarker position is an exact gfx.snapshotAt position in the replay. Gate
(port/test/test_app_live.py, 13 passed, app driven only by app_drive.py's posted messages, the PC at 100 % CPU from other processes):
- title menu client pixels = the original's sandbox_basic `menu` checkpoint, 0 px;
- title idle CPU 3.1 % of one core over 10 s (0.00 % with --idle-ms 0);
- three hover moves give three new presents, the client pixels equal each presented frame;
- Sandbox then Monterey, 422 records;
- 355 frame periods of 87..100 ms (median 93), 58 with bit 17 of 125..135 (median 128);
- sg_run_boot over trace.trace = framehash.bin in every record;
- all 370 presents = the replay's snapshots at their marks, and equal drawing-call counts (the same passes);
- shown.tsv = presents.tsv.
Controls RED: pacing-off, a dropped dispatch, a stale present, an idle pass left out. Equivalent mutants found:
dropping Monterey's press (the location screen acts on the release), and snapshots-by-position against a dropped pass (hence the
draw counts). S7.1's gates hold with the changes:
- equivalence 66 PASS + 1 NO_FRAMES on S7.1's 67 (+3 new corpus recordings, both paths agree), x64 = Win32;
- segments 205/205, 0 rows differ, x64 and Win32;
- boot HEAD-61 58/2/1;
- test_live 23 passed on both backends; test_app_shell 18; test_app_input 16; port test_static 6.

## 2. Gate, measured

Gate row (S7_steps.tsv) item by item. Commands run from port/build/S7_6_head (section 4).

| item | measured | where |
|---|---|---|
| `--install <dir>` reaches the title menu | the first modal present; client pixels = gfx.reference("sandbox_basic", "menu"), 0 of 480,000 px; a plain `--install --data` start (system cursor) reaches a modal present and closes at the pump | test_title_menu_is_the_original_frame, test_plain_install_start_reaches_title |
| through real messages to the sandbox | app_drive.py PostMessageW only: 3 mouse moves, click (130, 445) Sandbox, click (270, 32) Monterey; 4 dispatches, 422 records, stop `host` at a frame head | test_session_ran (6_app_live_facts.json `session`) |
| frame heads held at 87 ms (+125 with bit 17) over >= 300 frames | 355 periods: min 87, max 100, median 93, mean 92.85, 99.7 % <= 99; bit 17 (--flag17 250:310, a recorded harness write): 58 periods min 125, max 135, median 128; frame work 3..10 ms (median 4) | test_pacing_held, 6_app_live_facts.json |
| blocking pumps sleep on input: CPU < 5 % idle on the title menu | 3.10 % of one core over 10 s at --idle-ms 200 (earlier runs 2.32 / 2.65 / 3.27 / 3.59); 0.00 % at --idle-ms 0, 1.72 % at 500 (6_probe.py) | test_title_idle_cpu |
| hover drawing follows the cursor (U-1601) | each of 3 cursor moves (another item, Sandbox, no item) gives a new modal present, the three frames differ, the client pixels equal each frame (rebuilt by the replay's snapshot at its mark, sgh64 checked) | test_hover_follows_the_cursor |
| idle passes recorded, the replay takes the same passes | 82 idle passes; the replay with gfx.snapshotAt at all 370 presents' positions gives every presented frame (sgh64), and the gfx backend's `handled` counts of the replay equal the session's gfx_log.txt | test_presents_replay |
| presented pixels = the gfx back buffer at each present | 370 presents (7 modal, 363 frame waits): every frame the window thread took (shown.tsv, hashed from the Presenter's own frame) = the sim thread's back buffer at that present (presents.tsv) = the replay's snapshot there; client pixels at the 4 quiescent points (title, 3 hovers) = the presenter's mapping of the frame | test_shown_frames_are_the_presents, test_presents_replay, test_hover_*, test_title_* |
| the session's trace replayed with sg_run_boot = the session's framehash in every record (x64 app) | 422 / 422 records, T and 203 sim hashes | test_recording_replays |
| S7.1's gates | equivalence: S7.1's 67 rows identical (66 PASS + 1 NO_FRAMES), 3 new corpus rows (cov_a_quit PASS, cov_a_quit_title NO_FRAMES, cov_r_s2_screens: live and boot both FAIL against its capture, as at HEAD), replay of the trace host's recordings 70 / 70 (S7.1: 63 / 66), violations 0 (S7.1: 25); Win32 = x64 in all 70 rows | 6_equiv_x64.tsv / _w32.tsv |
| | segments 205 / 205 PASS, 0 rows differ from merges/S6-1/segs_*.tsv, x64 and Win32 | 6_segs_x64.tsv / _win32.tsv |
| | boot gate: HEAD's 61 = 58 PASS / 2 KNOWN / 1 NO_FRAMES (cov_a_play ERRORed once with a MemoryError reading its capture while the PC was out of memory, and PASSES 595 / 595 alone: 6_boot_cov_a_play.log); S1's extra rows as in BASELINE.md (cov_r_bio, cov_r_flic_input FAIL; four PASS) | 6_boot_gate.tsv, 6_boot_compare.log |
| | test_live.py 23 passed x64, 23 passed SG_BACKEND=win32 (`-m "not slow"`; the slow all-scenario test is the equivalence gate above) | 6_test_live_x64.log / _w32.log |
| | test_app_shell 18 passed, test_app_input 16 passed, port test_static 6 passed | 6_test_app_shell.log, 6_test_app_input.log, 6_test_static.log |

## 3. What was built

### The live core (port/src/sim/live.cpp, live.h; reference copies in the mirror)

- `taker(calls, count, paced)`: TraceInput::batch (class test, cursor test with the `stuck` rule, PACE) simulated over the loop's
  calls since the last dispatch plus this one; replaces S7.1's replay_takes_here. may_deliver = this call takes a dispatch recorded
  now. With an inflated count of +1 it can also be this call (the stuck path). must_deliver is true for a Blocking call where no
  later call at the same count could take a dispatch after a decline here. `idle_ok` = a modal Blocking unpaced call that is not
  must_deliver.
- INFLATED dispatch: a CURSOR record in front of the dispatch, which the port's next cursor read fills (or the last answer at the
  frame's end). An inserted clock record shifts its index. Count: LiveResult::inflated (cov_a_keys 1).
- Clock records after every modal Blocking pump call (decline or deliver) and after H1's frame wait (`separator(false)`: a
  timeGetTime at waitTicks' site, which no port code reads). They make LiveMarker positions exact. A paced delivery is recorded as
  [separator][pass-read placeholder][HANDLERs], and PACE counts only pace-site reads.
- LiveMarker (`LiveHost::marker`), `LiveResult::mark_pos` (filled by Recorder::bytes from kMark events, which are not written).
- Negative controls live.recorder: what 5 (the cursor read after the n-th idle pass not recorded), what 6 (the n-th host dispatch
  applied, not recorded).
- Script host: `idle N`, `mark` (with gfx attached the back buffer goes to `$live/mark_k.555`). idle_ok calls are answered like S7.1's
  must_deliver calls (the player acts) unless an `idle` wait runs, and an idle_ok call with input it cannot take here declines (S7.1:
  stop). S7.1's three scripts give the same records.
- C ABI (6_capi.patch): sg_live_pump.idle_ok (appended), sg_live_stats_ext, sg_live_marks. No tick.cpp / commands.h change.

### The app (port/app/host.*, main.cpp)

host.h's header comment lists what each callback answers.
- Sessions stop at the next frame head or pump after the window closes.
- A quit closes the window.
- `--flag17 a:b` is test-only.
- Controls: `--negctl pacing-off | present-late | recorder:<mode>`.
- The press / release split rule: 6_probe session 2 lost Monterey's click when both came in one dispatch (U-1653).
- Thread priority: under 100 % CPU load from other processes the normal-priority sim thread woke late (median 95, 75.7 % within
  99 ms). With THREAD_PRIORITY_ABOVE_NORMAL it measured 99.7 % within 99 ms (U-1655).

## 4. Commands

```
~/bin/py -3.12 log/roadmap/S7/6_mirror.py --build                 # mirror sync + x64 build (port/build/S7_6 of the mirror)
cd port/build/S7_6_head
~/bin/py -3.12 port/test/build.py --arch Win32                     # Win32 build + runner (port/build/Win32 of the mirror)
~/bin/py -3.12 -m pytest port/test/test_app_live.py -p no:cacheprovider -v         # the gate (own build port/build/S7_6_app), 13 passed
export SG_PORTSIM_DLL=$PWD/port/build/S7_6/Release/portsim_c.dll
~/bin/py -3.12 -m pytest port/test/test_live.py -p no:cacheprovider -m "not slow"   # 23 passed; SG_BACKEND=win32 too
~/bin/py -3.12 -m port.test.live equiv --all --jobs 3 [--backend win32] --out <tsv>
~/bin/py -3.12 -m port.test.integrator segments --dll $SG_PORTSIM_DLL --jobs 3 --out <tsv>      # --backend win32 for Win32
~/bin/py -3.12 -m port.test.integrator compare log/roadmap/merges/S6-1/segs_x64.tsv <tsv>
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out log/roadmap/S7/6_boot_gate.tsv
cd ../../..; ~/bin/py -3.12 log/roadmap/S7/6_boot_compare.py; ~/bin/py -3.12 log/roadmap/S7/6_make_patches.py
~/bin/py -3.12 log/roadmap/S7/6_probe.py [--frames N] [-- <app args>]   # one driven session, CPU / pacing printout
```

## 5. Controls

| control | where | result |
|---|---|---|
| pacing off (`--negctl pacing-off`) | test_control_pacing_off | frame periods median < 87: RED |
| a dropped input batch (`recorder:` 6 + 16 * 3, Monterey's release) | test_control_dropped_batch | the replay's records differ: RED |
| stale present (`present-late`: the window gets the previous present's frame) | test_control_stale_present | shown.tsv != presents.tsv: RED |
| an idle pass left out (`recorder:` 5 + 16 * 1) | test_control_idle_pass_left_out | the replay's drawing-call counts differ: RED |
| an idle pass left out, script host (5 + 16 n, n = 0, 10, 20) | test_live.py test_idle_pass_left_out_red | draw counts (live, replay) differ, e.g. Image_draw 208 / 201, Window::refreshFull 56 / 55: RED x3 |
| a dropped dispatch, script host (6 + 16) | test_live.py test_dropped_batch_red | RED |
| S7.1's four controls | test_live.py | RED, unchanged |

Equivalent mutants (arithmetic checked):
- Dropping Monterey's PRESS (dispatch 2) changes no record: the location screen takes the click from the release, which still comes.
- Snapshots by recording position are blind to a dropped idle pass. Replay pass j reads the record of live pass j + 1, and each
  snapshot triggers at the record after its mark, so it shows the frame drawn from the same record as live (tested n = 0, 3, 7, 10,
  15, 20: all equal). The pass count shows only in the drawing calls (7 Image_draw, 6 drawCenteredText, 1 refreshFull per title
  pass), which the gate compares.

## 6. Open

- U-1650 focus-child callback answers 0 at HEAD.
- U-1651 the cursor-source split.
- U-1652 the idle-pass rate against the original's continuous passes (the location-screen blink is slower).
- U-1653 the press / release split.
- U-1654 the port's waitTicks drops (900 / 600 / 100 ms holds missing in the app).
- U-1655 pacing under load.
- U-1656 cursorless loops after one idle pass.
- U-1657 title-only sessions have no record.
- U-1658 CPU of paced waits not measured.
- U-1659 in-game presents are judged by hashes, not on-screen pixels.
- U-1604 (S1 / S8 not merged).
- S7.4 can take its recordings from <data>/recordings/<stamp>-<pid>/ (trace.trace + framehash.bin; frame_ra 0, so the trace's clock
  sites are the offsets themselves).
