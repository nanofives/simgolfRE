# S7.1 live run core: notes (worker S7.1, 2026-10-24)

Row S7.1 of re/roadmap/S7_steps.tsv. Base HEAD c6d7d78b. Mirror: port/build/S7_1_head (git archive of HEAD, junctions of
log/roadmap/S0/export.md plus shim/build and re/replay; HEAD_COMMIT inside). Builds in the mirror: port/build/x64 and
port/build/Win32 (logs 1_build_w32.log). Merge: 1_merge.md. Files: 1_files.txt. U-rows: 1_uncertain.tsv (U-1600..U-1606).

## 1. What was built

`run_live` (port/include/portsim/sim/live.h, port/src/sim/live.cpp) runs the same boot and frame steps as tick_run_frames
(through `tk::tick_frame_io`) and asks a `LiveHost` for every outside value when the port reads it:

| request | where the port reads it | LiveHost call |
|---|---|---|
| frame-head clock | before each frame (FrameInput::t_head) | `frame_head(frame, t)`; false ends the session |
| the frame body's clock read (H1's g_frameMs, ML+0xf07f) | TkBase::clock_site via `tk::LiveIo` | `clock(site, body=true)` |
| a wall-time loop's clock read (seedFromTime, pumpAndRedraw, credits, hole stats) | InputSource::clock | `clock(site, body=false)` |
| a cursor read | TkBase::cursor_read (body) / InputSource::cursor (modal) | `cursor(modal)` |
| GetAsyncKeyState outside the root handlers | TkBase::key_async | `async_key(vk)` |
| a pump call | InputSource::batch | `pump(LivePump)` -> Deliver / Decline / Stop |
| PACE | InputSource::pace | `pace(site)` |
| H1's frame wait | TkBase::wait_point | `wait_point()` |

The sim runs on the caller's thread and blocks inside the callbacks; there is no time limit in the port. Harness memory writes
exist only for test hosts (the trace host): the host stores them and reports each through `LiveHost::writes` (LiveWriteLog), the
core records them.

Hooks into tick (patch 1_tick.patch): `tk::LiveIo` + `FrameIo::live` + `FrameInput::live`, consulted by clock_site / cursor_read /
key_async only when set. A trace run never sets them, so the trace path is unchanged (section 4).

### The recording (schema v1, re/model/trace_schema.md)

Every answer becomes a record when it is given: CLOCKRUN (consecutive answers of one site and slot with step 1 joined), FRAME
(index, pos, t, the 0x60-byte snapshot read from the state by GameState member names), CURSOR, KEYSTATE (body reads at H1's
GetAsyncKeyState site ML+0xf10c; the press handlers' Ctrl / Shift reads at rootWin_onPress1 / onPress2's sites, taken from the
corpus: 0x5599c / 0x559b0 / 0x55aa6 / 0x55aba from the frame-head ra), HANDLER, DELIVER w32 (test hosts), a final POLL, END
(sub 1 when the game quit). Meta: `{"frame_ra":..., "live":1, "scenario":..., "schema":1, "slots":[golf_clean.exe tgt, jgld.dll
qpc], "snapshot":[the nine (va, len) pairs computed from the GameState members], "spec":...}`. frame_ra is a run option (0 writes
the sites themselves; port/test/live.py passes mainLoop's frame-head return address so recordings read like the corpus's).

The recording is shaped for the replay's rules (tick.cpp parse_trace and TraceInput::batch), so that sg_run_boot hands each
dispatch to the pump call that took it live:
- CLASS: a body dispatch (FrameIo::pump_site P6 / H1) needs the last non-handler clock record to be the frame head or jgld's
  Palette::update QPC; a modal one any other site. The recorder writes a separator when the context is wrong or a batch is open
  (consecutive HANDLER records with no clock record between are one dispatch): a QPC at the palette ra (`0x80000000 | 1 << 24 |
  0x6a414`) for body, a timeGetTime at waitTicks' site (ML-relative 0x4c384, read by no port code) for modal.
- PACE: a paced loop's dispatch gets a PLACEHOLDER pass read in front of it (TraceInput delivers when at most one pass read before
  the dispatch is unread; the port reads the pass clock after its pump, so without it the replay delivers one pass early). Its
  value is filled when the port makes that read; reads at other sites answered meanwhile go in front of it.
- WRITES: a harness write recorded at clock count n is applied by the replay at the first point whose position is > n, so after
  writes the next CURSOR / dispatch gets a separator first; writes made inside frame_head are written after the FRAME record (head
  writes).
- the core keeps, per frame, the pump calls since the last dispatch that took nothing (class, kind, cursor count, pace site,
  pass reads consumed) and evaluates TraceInput's refusal rules (class, cursor count with the `stuck` rule, PACE) for a dispatch
  recorded now: `LivePump::may_deliver` is false when an earlier call would take it. `must_deliver` is true for every Blocking pump
  that is not paced (U-1601); a blocking host is asked again until it delivers or stops. A delivery against may_deliver is applied
  and counted in LiveResult::violations.

### The hosts

- C++ `LiveHost` (S7.3's app), C callbacks `sg_live_host` + `sg_live_run` (portsim_c.h), and two built-ins through
  `sg_live_run_builtin` (Win32 runner op 28):
- TRACE HOST (tick.cpp `TraceHost`, the equivalence oracle): parse_trace + its own TraceInput; body clock = the frame's first value at
  the site (TkBase::clock_site's rule), cursor / async = the frame's records in order with the last record carried into the next
  frame (tick_run_frames' after_frame), modal reads, pumps, PACE and writes through TraceInput (a `write_hook` reports its writes).
- SCRIPT HOST (live.cpp, test scripts): steps seed, frame_ms, cursor, async, click, rclick, key, vkey, batch, wait_frames,
  wait_modal, wait_body, wait_passes, wait_flags, stop. Clock: every answer the previous + 1, a frame head at least frame_ms (99)
  after the last. At a must_deliver pump with nothing queued it skips its waits to the next input step (the player acts now) or stops.

## 2. Gates and numbers

### (a) EQUIVALENCE (trace host vs sg_run_boot), all boot scenarios

```
cd port/build/S7_1_head
~/bin/py -3.12 -m port.test.live equiv --all --jobs 3 --out log/roadmap/S7/1_equiv_x64.tsv                   # 16.6 min
~/bin/py -3.12 -m port.test.live equiv --all --jobs 3 --backend win32 --out log/roadmap/S7/1_equiv_w32.tsv   # 22.2 min
```
x64 (1_equiv_x64.tsv / .log): 67 corpus scenarios with a capture; 66 PASS (every record's T and 203 sim hashes equal to sg_run_boot
over the same trace, same record count), 1 NO_FRAMES (cov_b_location, no frame record in the capture, boot.py does not run it).
The capture verdict of the live records equals boot.py's verdict of sg_run_boot's in every row: over HEAD's 61 scenarios 58 PASS,
2 KNOWN (cov_a_golfer, cov_b_holes18, U-1062), 1 NO_FRAMES, as re/roadmap/BASELINE.md; the 6 S1 recordings (U-1102): cov_r_palette_hot,
cov_r_s8_play, cov_r_theme_cancel, cov_r_tutorial PASS, cov_r_bio and cov_r_flic_input FAIL, the same for both paths.
Win32 (1_equiv_w32.tsv / .log): the same 66 PASS + 1 NO_FRAMES; every row (status, capture verdict, records, violations,
replay result) equal to the x64 row.

Extra, the trace host's OWN recording replayed by sg_run_boot: equal to the live run in 63 of 66; cov_a_keys, ui_tour, ui_tour_v
diverge (-11) where TraceInput used its `stuck` rule (25 deliveries in 7 scenarios counted as violations, U-1600). This does not
enter gate (a); live hosts never produce the pattern.

### (b) ROUND TRIP, scripted sessions (live.py SCRIPTS)

```
~/bin/py -3.12 -m port.test.live roundtrip                # x64
SG_BACKEND=win32 ~/bin/py -3.12 -m port.test.live roundtrip
```
| script | what | records | dispatches | pump calls body / modal / paced | x64 | Win32 |
|---|---|---|---|---|---|---|
| sandbox_play | title -> Sandbox -> Monterey -> play: Esc, build palette, two slots placed, right click, h, Esc, P twice | 338 | 26 | 676 / 4 / 0 | PASS | PASS |
| report | title -> sandbox -> F1 and F2 report screens, each closed by Esc | 112 | 12 | 224 / 6 / 0 | PASS | PASS |
| modal_wait | '.' and '/' reach pumpAndRedraw; 40 and 15 declined passes, then Esc / a click | 92 | 10 | 184 / 4 / 58 | PASS | PASS |

PASS = trace_format.parse accepts the recording and validate (virtual-clock rule off) finds nothing, sg_run_boot over it gives the
live run's records in every record (T + 203 hashes, same count), 0 violations, the script ran to its end (stop "host"). x64 and
Win32 give identical records and byte-identical recordings (test_live.py::test_roundtrip_backends_identical). The report
session's modal calls (6 = title menu 2 + location screen 2 + one per report) show both report screens' loops took input.
C ABI: a ctypes callback host with the script host's policy gives the same records and the same recording bytes
(test_live.py::test_callback_host_equals_script_host).

### (c) the trace path is unchanged with the patches applied

Measured in the mirror with both patches and the module applied:
- segments x64 205 / 205 PASS, 0 rows differ from log/roadmap/merges/S6-1/segs_x64.tsv (1_segs_x64.tsv / .log, 168 s);
- segments Win32 205 / 205 PASS, 0 rows differ from log/roadmap/merges/S6-1/segs_win32.tsv (1_segs_win32.tsv / .log, 242 s);
- boot gate `-m port.test.boot --all --jobs 3` (1_boot_gate.tsv / .log, 6.2 min) compared by 1_boot_compare.py (a copy of
  merges/S6-1/boot_compare.py) with S0's HEAD baseline: HEAD's 61 scenarios 58 PASS / 2 KNOWN / 1 NO_FRAMES, 0 rows differ; the six
  S1 recordings as in BASELINE.md (cov_r_bio, cov_r_flic_input FAIL, the other four PASS);
- port test_static 6 passed (module listed, no image-range literal);
- test_live.py: 17 passed x64 (1_test_live_x64.log, 191 s) and 17 passed SG_BACKEND=win32 (1_test_live_w32.log, 221 s; the callback
  test always runs on x64).

## 3. Controls (live.recorder, mode = what + 16 n; test_live.py NEGCTL, run in 1_test_live_x64.log / _w32.log)

| control | run | mode | verdict |
|---|---|---|---|
| a clock answer left out of the recording (n = 0: seedFromTime's) | roundtrip modal_wait | 1 | RED: the replay stops in the boot (NeedInput seedFromTime, -11), 0 of 92 records |
| two dispatches reordered in the recording (4 and 5: '.' and Esc) | roundtrip modal_wait | 66 | RED: state differs at record 31 (g_golfers, g_textBuf, g_storyPairs, g_rng ...) |
| two dispatches reordered (6 and 7) | roundtrip modal_wait | 98 | RED: replay -11 at record 32 |
| one cursor read answered off by 1 (x + 1 at read 2655) | equiv cov_k2_build | 42484 | RED: g_cursorTileX at record 767 |

Blind / equivalent mutants found on the way (arithmetic checked): dispatches 0 and 1 swapped (the title click's onPoint and its
press: the menu choice comes from the hover read, so the same item is taken; PASS); a cursor read answered one read late (mode 3,
sandbox_basic reads 0 / 500 / 1500, cov_k2_build reads 283 / 334, PASS): in the corpus the cursor holds still within a frame
except in the boot's menus, so the next read returns the same point; x + 1 at 26 other probed reads of cov_k2_build
(1_cursor_ctl_search.log): no tile column boundary within one pixel there. Clock drop n = 5 (mode 81) makes the replay never
finish (U-1602): RED by its budget, not used as a control.

## 4. Commands that reproduce everything (from port/build/S7_1_head)

```
~/bin/py -3.12 port/test/build.py --arch x64 ; ~/bin/py -3.12 port/test/build.py --arch Win32
~/bin/py -3.12 -m pytest port/test/test_live.py -q -p no:cacheprovider          # x64 (+ the Win32 identity test)
SG_BACKEND=win32 ~/bin/py -3.12 -m pytest port/test/test_live.py -q -p no:cacheprovider
~/bin/py -3.12 -m port.test.live equiv --all --jobs 3 [--backend win32] --out <tsv>
~/bin/py -3.12 -m port.test.live roundtrip [--backend win32]
~/bin/py -3.12 log/roadmap/S7/1_cursor_ctl_search.py cov_k2_build 40           # the pixel control's search
~/bin/py -3.12 log/roadmap/S7/1_make_patches.py                                # from the shared root: patches + apply check
```
Development switch: SG_LIVE_LOG=<file> appends each delivery the replay would place at an earlier call, with the calls before it.

## 5. Open

- U-1600 trace-host recordings with TraceInput `stuck` deliveries; U-1601 must_deliver for unpaced modal loops (no idle passes in a
  live session); U-1602 a malformed recording can hang the replay; U-1603 synthetic deadline-clock site; U-1604 S1's Load pump and
  focus keys after S1 merges; U-1605 a mid-frame stop; U-1606 the trace host's tail idle for gfx.
- Real-time pacing, the window, input translation: S7.2 / S7.3.
