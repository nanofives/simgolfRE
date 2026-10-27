# S7.8 notes: where the app presents (U-1642) and what a long session keeps in memory (U-1643) (worker S7.8, 2026-10-25)

Row S7.8 of re/roadmap/S7_steps.tsv, run on claude2 (opus). No game run. I6 had not landed (git log at start: b3d61e41), so the base is
S7.5's mirror. Mirror port/build/S7_8_head: `~/bin/py -3.12 log/roadmap/S7/8_mirror.py [--build] [--w32]` (5_mirror.py's create with the
root changed, then port/app, the app / live tests, the live files and S3.2's files copied from port/build/S7_5_head, so the start equals
S7.5's accepted tree; `diff -rq` of port/ against S7_5_head was empty before the first edit). Builds: port/build/S7_8 (x64:
portsim_c + simgolf_port), port/build/S7_8_w32. Merge: 8_merge.md (patches 8_tick.patch for the S7 lead, 8_step.patch for the step's
files, both made by 8_make_patches.py and checked with `patch -p1 --dry-run` on S7_5_head). Files: 8_files.txt. U-rows:
8_uncertain.tsv (U-1670..U-1673).

## 1. U-1642: where the original's back buffer reaches the window (measured from the code)

S7.5's premise was wrong. mainLoop_structure.md row P6 calls [[0x0083ad50]+0x9c] (0x00418cd1) the display present slot. It is
slot 39 of the Display vtable 0x1011d640: thunk 0x10001875 -> Display::setScale 0x10067d50 (read from jgld.dll's bytes; jgld_3.tsv:
it stores its three arguments into g_blitScaleX / Y / Den). P6 calls it with (1, 1, 1) (0x00418cbf..0x00418cd1; the port's
P6_DisplaySlot9c already said "the display's scale slot"). Display::present 0x10066470 (BitBlt of the back buffer g_backBufferSurface
0x1012873c to the screen DC, rop 0xcc0020, clipped to a rect: `call [0x1012c3a8]` (BitBlt) at 0x10066538 / 0x100665b2, the only calls through jgld's BitBlt slot; its one StretchBlt call is in Surface::stretchTo 0x10008f70, which Surface_fillRegion uses to copy onto the root surface, CLAUDE.md phase 5 step 2) is
slot 41 (+0xa4).

Who calls slot +0xa4 of the display:
- jgld.dll has no `call [reg+0xa4]` at all (byte scan of .text for FF 90..97 A4 00 00 00: no hit); Display::present has no direct
  caller and its only pointer is that vtable slot (re/tools/pointer_callers.py).
- golf_clean.exe has four `call [reg+0xa4]`: 0x0047cfba in drawTooltip 0x0047cdb0 (`mov ecx, [0x83ad50]; ... push esi; call [edx+0xa4]`,
  esi = drawTooltip's rect argument: the live display's present), 0x00475c15 in DC_calla4 (the slot of [this+4] with four arguments:
  a Surface slot, not Display::present, which pops one, `ret 4` at 0x100665e7), 0x0047c192 in Window::mouseDispatch244 (a window's
  own slot, two arguments) and 0x0048797e in NodeList::fwdChild41 (a node object). So drawTooltip is the only way to the window.
- drawTooltip's callers (re/tools/xref.py): Window::refreshRect 0x00480c20 (from Window::refreshFull 0x00480c80: compose
  0x0047fa30 and 0x0047b0d0, then drawTooltip(rect)), redrawRegion 0x00495770 (slot 1 = paint of the jgld Listener table 0x004ba87c:
  WM_PAINT), compositeScreen 0x0047fab0 and drawCursor 0x0047d130 / addDirtyRect 0x0047d060 (from uiPointerMove 0x004959b0 = slot 5,
  WM_MOUSEMOVE, and uiScrollEvent slot 6), Tooltip::show 0x00480220 / hide 0x00480360.
- In an ordinary in-game frame the present is H1's refreshFull at 0x0041e29b (ML+0xecdb, after emptyFunc_4a0890 on the camera
  path a41e290), before H1's frame wait 0x0041e631..0x0041e6b1. SG_TICK_TRACE over sandbox_basic (the port follows the original's
  control flow): 780 refreshFull calls, 439 of them `clamp > emptyFunc_4a0890 > Window::refreshFull` (one per frame record, 439
  records), 340 in the title menu loop (tick_menu.inc ML+0x11ca5), 1 in showLoadingScreen. The other frame-body refreshFull sites
  are H1 key cases (0x0041d89a routing render, 0x0041e277 a camera key, each followed by pumpAndRedraw).

In the port the gfx back buffer changes only in the Window::refreshFull body (gfx.cpp b_refresh_full; no other writer of `back` in
port/src/sim), i.e. exactly where the original presents. So:
- tick.cpp TkBase::call (case 1, a drop): after gfx_drop, `if (io.live && name == "Window::refreshFull") io.live->present_point()`
  (tk::LiveIo::present_point, default empty; a trace run has io.live null: nothing changes there). live.cpp's Core forwards it to
  LiveHost::present_point (live.h, default empty; PrefixHost forwards it too).
- host.cpp LiveSession / LiveTracePresenter present there (kind 'r'), and keep the frame-wait ('w') and modal-pump ('m' / 'p')
  presents for anything that changes the back buffer elsewhere (they present only a changed buffer, so after an 'r' they find
  nothing). session.json: presents_by_kind, present_points (opportunities by kind) and r_superseded = 'r' presents another 'r'
  replaced before any w / m / p point came (frames S7.5's points never showed).

Measured over the app's live path (`--live-trace`, LiveTerrain attached):

| trace | records | r | w | m | p | r_superseded |
|---|---|---|---|---|---|---|
| sandbox_basic | 439 | 327 | 0 | 3 | 0 | 0 |
| course_season_6y | 51,092 | 24,492 | 0 | 3 | 0 | 4 |
| (S7.5's points, sandbox_basic) | 439 | - | 324 | 6 | 0 | - |

test_app_present (checkpoint-plan record budgets, 8_present_facts.json): sandbox_basic r 327 / m 3 / superseded 0; course_season
(6,296) r 2,670 / m 3 / 4; cov_a_keys (2,103) r 1,220 / m 7 / 33; cov_k1_routing (422) r 311 / m 4 / 10; w 0 in all four. The
app's in-game session (test_app_ingame, real window): r 81, m 2, w 0 (S7.5's session: w 79, m 5).

LiveSession skips the frame-wait check when nothing modal ran since the frame's 'r' present (w_check_: set at the frame head and
by every modal pump / clock / cursor callback, cleared by an 'r' present): the 'r' present already costs one back-buffer dump per
frame, and the wait's dump found nothing in every trace above (LiveTracePresenter still checks every wait). test_app_live: 421
waits skipped, 0 'w' presents. The S7.5 design showed every back buffer of sandbox_basic
too, only later in the frame (at the H1 wait, after H1's tail); over course_season_6y it missed 4 back buffers that a later
refreshFull replaced before the wait (r_superseded 4). The 3 'm' presents are modal screens whose drawing goes through their own
module's gx path (U-1670).

## 2. U-1643: the terrain log, and what really grew

LiveTerrain's log (terrain_live.cpp log_) is small in play: course_season_6y logs 25,671 Terrain calls in 51,092 frames, and
25,404 of them come in the boot's world load (terrain_log_max 25,404 is that one frame); 267 come after. S7.5's "4.1 per frame"
was course_season's total over its frames, dominated by the load. host.cpp TerrainLogBound drops the lines at each frame end
(the app never reads them; U-1673 for the const_cast, S3 request in 8_merge.md section 3): the log is empty at every sample and at
the end.

The growth was the live core's: every frame record (T + 203 sim hashes = 1,628 B) and every recorder event was held until the
session ended. Before S7.8, over course_season_6y, private bytes went from 199,475,200 at frame 0 to 365,592,576 at the end
(3.3 KB per frame; 8_long_bound.log, the first run). Now (U-1672 resolved):
- live.h LiveOptions: `trace_file` (the recording is spilled to that file every `spill_events` = 4,096 events at a frame end: the
  body encoder Recorder::Encoder is fed the finished events, their bytes are written to `<file>.body`, events and FRAME snapshots
  are dropped; at the end the header goes first and the body is copied after it; LiveResult::trace stays empty), `on_record`
  (each frame record as it is made) and `keep_records` (false: T / hashes stay empty). Defaults keep every earlier behaviour (no
  trace_file, keep_records true): test_live, the C ABI and the equivalence gate run unchanged.
- host.h RecordStreams: the app (session and --live-trace) writes framehash.bin per record and lets the core spill trace.trace;
  `--in-memory` restores S7.5's way. LiveSession's summary: trace_streamed, spilled_events, held_events.
- Byte identity (8_stream_identity.log): sandbox_basic (no spill: 3,801 events < 4,096) and course_season (45,801 events spilled,
  785 held at the end): trace.trace and framehash.bin streamed = in memory, byte for byte.

Long session, course_season_6y over the app's live path (51,092 records, 22 min wall time on this PC, 8_long_stream.log):
framehash.bin equals the original's capture log/p2/p24/course_season_6y/c_a/framehash.bin byte for byte (T and 203 sim hashes in
every record); private bytes 230,719,488 at frame 2,000 and 230,719,488 at the end (largest sample 230,748,160); working set
173,391,872 at frame 0, 45,420,544 at the end (Windows trims it; peak 211,435,520 during the load); trace.trace 12,729,046 bytes
(367,257 events spilled, 3,136 held at the end); terrain log 0 lines at the end.


The second long pair (test_app_present, 8_present_facts.json `long_memory`, final build, less PC load): streaming 632 s, private
bytes 230,846,464 at the end, -65,536 B after frame 2,000; the control (`--in-memory --negctl terrain-log-keep`) 786 s, private
bytes 199,491,584 -> 366,587,904, +128,843,776 B after frame 2,000, terrain log 25,671 lines at the end; framehash.bin and the
recording (sha256 of trace.trace) equal between the two.

## 3. Gate

| item | measured |
|---|---|
| the app presents where the original's back buffer reaches the window | at every Window::refreshFull drop of the frame code ('r'), the original's refreshFull -> refreshRect -> drawTooltip -> Display::present (section 1); 0 frame-wait presents left over four traces and in the in-game session |
| every present = the port's back buffer and the replay's snapshot | test_app_ingame gate (1) (every present = the replay's snapshot at its position, FRAME_KINDS 'r' / 'w'), test_app_present: every checkpoint snapshot = the last present before it, four traces |
| >= 20,000 frames, memory bounded, terrain log bounded, sim hashes unchanged | course_season_6y 51,092 records, framehash.bin = the original's capture byte for byte, private bytes flat after frame 2,000 (+-65,536 B), terrain log 0 lines (section 2) |
| test_app_ingame, test_app_live, test_live, segments 205 / 205, boot gate unchanged | section 4 |

## 4. Regression gates (log/roadmap/S7/8_*.log / .tsv)

| gate | measured | build |
|---|---|---|
| port test_static | 6 passed (8_test_static.log) | final |
| live equivalence x64 (S7.1) | 70 rows: 68 PASS, 2 NO_FRAMES, as S7.5 (8_equiv_x64.tsv) | first (before control 9 and w_check_) |
| segments x64 | 205 / 205 PASS, 0 rows differ from merges/S6-1/segs_x64.tsv in status / frames / code / digest (8_segs_x64_compare.log) | first |
| boot gate x64 | 70 rows: 65 PASS, 2 KNOWN, 2 NO_FRAMES, 1 FAIL (cov_r_s2_screens, U-1251), 0 rows differ from S7.5's 5_boot_gate.tsv (8_boot_compare.log) | first |
| segments Win32 | 205 / 205 PASS, 0 rows differ from x64 (8_segs_win32_compare.log) | final |
| test_live x64, not slow | 32 passed, 1 deselected (the 70-row equivalence, run above) (8_test_live_x64.log) | final |
| test_app_shell / input / replay | 18 / 16 / 14 passed | final, before w_check_ |
| test_app_live | 12 passed, 1 failed: test_pacing_held, the bit-17 segment's median period 138.0 against 137 (8_test_app_live.log). S7.5's own build (port/build/S7_5_head, its S7_6_app) under the same load: the same test fails with 138.0 (6_app_live_facts.json restored afterwards). The normal segment: median 93, 100 % within 87 + 12. This is the PC's load (sibling sessions running sprite gates and framegate), not this step | final |
| test_app_ingame | 12 passed, 1 xfailed (U-1640), as S7.5 (8_test_app_ingame.log) | final |
| test_app_present (new) | 7 passed in 1,978 s, slow included (8_test_app_present.log) | final |

The first-build gates ran before two changes: recorder control 9 (live.cpp, active only with LiveOptions::trace_file and the
control switch) and LiveSession's w_check_ (host.cpp, the app only). Neither reaches a trace run, the C ABI's live runs or the
equivalence host (test_live 32, Win32 segments 205 and test_app_* reran on the final build).

## 5. Controls (all RED)

| control | measured |
|---|---|
| `--negctl present-point-off` (S7.5's points only), sandbox_basic | r 0, w > 0 (test_control_present_point_off): RED for w == 0 |
| `--in-memory --negctl terrain-log-keep` (S7.5's way), course_season_6y | +128,843,776 B after frame 2,000 against GROWTH_LIMIT 4 MiB; log 25,671 lines at the end: RED |
| `--negctl reg:live.recorder=9` (each spill drops its last event), course_season | trace.trace differs from the in-memory one while framehash.bin is equal: RED for the identity check |
| S7.6's present-late / pacing-off, S7.5's terrain-pass-skip / headless controls | still RED inside test_app_live / test_app_ingame (passed) |

Blind: no control moves a present between two points that show the same back buffer (by construction a present's pixels do not
depend on where it is made when nothing changes in between); what judges the point is the count of 'w' presents and r_superseded.

## 6. Paragraph for CLAUDE.md

Roadmap S7.8, the app's present point and a long session's memory (2026-10-25, notes log/roadmap/S7/8_notes.md, merge 8_merge.md,
U-1670..U-1673; U-1642 / U-1643 resolved): the original's back buffer reaches its window only through jgld Display::present (vtable
slot +0xa4), which the exe calls once, at the end of drawTooltip 0x0047cdb0; Window::refreshFull reaches it through refreshRect (H1
0x0041e29b in an ordinary frame), the jgld Listener's paint / mouse-move slots and the tooltip through the other callers. P6's
[[0x0083ad50]+0x9c] is Display::setScale(1, 1, 1), not a present (mainLoop_structure.md row P6 corrected). tick.cpp calls
LiveIo::present_point after the Window::refreshFull drop of a live run, and the app presents there ('r'): 0 frame-wait presents over
four traces and the in-game session. U-1643's terrain log was small after the world load (25,404 of 25,671 calls in it over
course_season_6y); the growth was the live core's in-memory records (3.3 KB per frame). LiveOptions::trace_file spills the recording
to disk every 4,096 events (byte-identical to the in-memory one) and on_record hands each frame record to the app, which writes
framehash.bin as it goes: over course_season_6y's 51,092 records private bytes stay flat after frame 2,000 and framehash.bin equals
the original's capture byte for byte. `--in-memory` restores the old way.

## 7. Open

U-1670 (modal screens drawing through their own module present at their next pump, not at their refreshFull), U-1671 (the cursor /
tooltip / WM_PAINT presents of the original: no gfx body draws the software cursor or tooltips), U-1673 (the const_cast until S3's
set_keep_log). test_app_live's pacing check fails under the PC's current load in S7.5's build too.

## 8. Commands

```
~/bin/py -3.12 log/roadmap/S7/8_mirror.py --build --w32            # mirror (created once) + builds
sh log/roadmap/S7/8_gates.sh x64                                      # equivalence, segments, boot gate (8_*.tsv)
sh log/roadmap/S7/8_apptests.sh [test_app_present ...]                # app test files, one process each (8_test_app_*.log)
~/bin/py -3.12 log/roadmap/S7/8_make_patches.py                       # 8_tick.patch + 8_step.patch against S7_5_head
cd port/build/S7_8_head && ./port/build/S7_8/app/Release/simgolf_port.exe --install "$PWD/original" --data <tmp> \
   --live-trace course_season_6y --record-dir <dir> [--in-memory] [--mem-every 1000]   # the long session by hand
```
