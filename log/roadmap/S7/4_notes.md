# S7.4 notes: app recordings in the boot gate, and the cross-check against the original (worker S7.4, 2026-10-25)

Row S7.4 of re/roadmap/S7_steps.tsv, run on claude3. Game slot: instance s7_4 only (re/tools/instances.py create s7_4; its
winmm.dll was replaced by the pinned references' build 381ed2b2 from instances\p24). Base: HEAD 2c05f02a + S7.7's
accepted, unmerged work, in the mirror port/build/S7_4_head. Merge: 4_merge.md. Files: 4_files.txt. U-rows:
4_uncertain.tsv (U-1630..U-1637).

## 1. Result

| gate item | measured | where |
|---|---|---|
| >= 3 app recordings replayed with sg_run_boot, each record = the app's own framehash (T + 203 sim hashes), x64 | hole 340 / 340, saveload 266 / 266, report 205 / 205: PASS | 4_gate_x64.tsv / .log |
| the same on Win32 | PASS, all three; x64 and Win32 give identical records, T and hashes (test_backends_identical) | test_app_replay.py, 4_test_app_replay.log |
| recordings hold title -> sandbox -> one hole; a save and load; a modal report | `hole`: Sandbox, Monterey, then the Build Course palette, a tee, a green, `H` (g_holesPlusOne changes at the `H` frame). `saveload`: the `S` key writes "Ocean's Edge 3 March 2001.sve" into the app's overlay; `L`, the browser row of that file, OK; g_date at record 149 returns to its value at record 77, the save point. `report`: F1..F4, each a modal loop with blocking dispatches in live frames, closed with Esc | port/corpus/app/*/ (local) |
| the input U-1610..U-1619 left unmeasured, where the app can produce it | `hole`: Shift click, Ctrl click (GetAsyncKeyState 0x8001 at the click, by `--keys message`), a double click (WM_LBUTTONDBLCLK), the wheel +120 / -120, Alt (WM_SYSKEYDOWN VK_MENU), Alt+X (WM_SYSKEYDOWN / UP 'X', TranslateMessage's WM_SYSCHAR), Shift+A | test_recordings_cover_the_inputs |
| controls | one input byte (the tee click's x 300 -> 364 in its HANDLER record): FAIL at a sim record at or after that frame. One framehash byte of record 200 (a sim hash, then T): FAIL at record 200 exactly, `sim` / `T` | test_app_replay.py |
| cross-check: one recording's input replayed into the ORIGINAL, per-frame sim hashes | `hole` (run x5): all 340 records equal in every sim object except g_frameMs, the H1 timeGetTime value (U-1637). The original's 59 root-handler calls equal the app's 59 in handler, a0, a1 and frame | 4_xcheck_x5_compare.json, 4_xcheck_x5_handlers.json |
| cross-check control | the tee click's x moved to 364 in the original's input (run x6_tee): first divergence record 185, the head right after the click's frame 185; objects g_tileFlags, g_tileLevel, g_tileType, g_holes, g_tileBuildCode, g_tileRefund, g_lastEditTileY | 4_xcheck_x6_compare.json |
| test_app_live still passing | 13 passed (its facts copied to 4_app_live_facts.json; the test rewrites 6_app_live_facts.json on every run) | 4_test_app_live.log |
| other app tests | test_app_shell 18 passed, test_app_input 16 passed (with 4_input_map.patch), port test_static 6 passed | 4_test_*.log |
| segment / boot gates | not re-run: portsim's sources (port/src, port/include, CMakeLists, runner32) are identical, line endings aside, to S7.7's accepted mirror port/build/S7_7_head, whose gates 7_notes.md records. Nothing in port/src or port/include changed. | `diff -rq --strip-trailing-cr` |
| repo static suite (mirror) | 613 passed, 2 skipped, 1 failed. The failure is tests/test_movie_inventory.py::test_committed_tables_equal_a_fresh_derivation. It fails the same way in S7.7's untouched mirror and passes in the shared tree, so it comes from the git-archive mirror, not from this step | 4_test_static_repo.log |

## 2. What was built

### The app (port/app/host.h, host.cpp, main.cpp; window.* and input_map.* by mirror patch)
- `--clock-origin <v>`: every clock answer is v plus the real milliseconds since the first answer. The app's first clock
  read is seedFromTime's (site 0x00467116, trace ra 0x574e7 + frame_ra 0x0040fc2f), the same as the original's first
  main-thread timeGetTime. With v = 0x00100000 (scenario_det.js's device-less g_seedTime, batch b2), g_seedTime and g_rng
  equal the original's. Pacing still runs on the real clock (head_t_).
- `--keys message`: GetKeyState / GetAsyncKeyState are answered from the key messages the window received: down 0x8000 /
  0x8001, the shim's SetVirtualKey forced-down answers (shim.cpp h_GetKeyState / h_GetAsyncKeyState). This covers the
  translator through WindowOptions::key_state (4_window.patch) and the core's async_key. It lets a driver that only posts
  messages make Shift / Ctrl clicks.
- The dispatch log, written at the session's end:
  - input_dispatch.tsv: seq, msg, wParam, lParam, arrival frame, commands, dispatch;
  - dispatches.tsv: live frame, body or modal, the body call's ordinal in the frame (0 = P6, 1 = H1), PumpKind, LivePump
    calls / cursor reads / may / must, commands, first and last seq;
  - body_calls.tsv: body pump calls per frame. Every frame of every recording has exactly 2.
- THE H1 RULE (host.cpp, pump), found by the cross-check:
  - P6 (pumpInput483bd0) loops 0x00483bb0, which jumps to display slot +0x128 (jgld 0x1006b0f0, PeekMessage(0, 0,
    PM_REMOVE)), until the queue is empty: any message.
  - H1 (pumpMessages 0x0045c030) first drains WM_MOUSEMOVE (PeekMessage 0x200..0x200 at 0x0045c03e..0x0045c04d). It then
    calls frameTick483cd0, which jumps to display slot +0x13c (0x00483ce2). That is jgld 0x1006b7e0: ONE
    PeekMessage(0x200..0x20a, PM_REMOVE) at 0x1006b7ff..0x1006b811, translated and dispatched.
  - So H1 takes at most one mouse-class message per frame and never a key. Before the fix, the app's H1 delivered a
    button-up and a WM_LBUTTONDBLCLK together; the original dispatched the double click at the next frame's P6.
  - The host's H1 now takes the first queued mouse-range event only (PeekMessage's range filter skips keys queued
    before it). Keys wait for the next P6.
  - test_h1_dispatches_hold_one_mouse_message checks the recordings.
- Translator fixes (4_input_map.patch, S7.2's files), all measured in the original by run x2 / x3:
  - uiKeyEvent 0x00495910 ORs 0x10000 (GetKeyState(0x10) high bit, 0x00495925), 0x20000 (0x11, 0x00495934) and 0x40000
    (0x12, 0x00495943) into the key-down's code. The original passed rootWin_onKey(0, 0x10010) for Shift's key-down,
    (0, 0x20011) for Ctrl's, (0, 0x40012) for Alt's, and rootWin_onChar(0x40058) for Alt+X. The translator passed the
    bare virtual keys.
  - The WM_CHAR's code is that code: the original passed onKey(0x61, 0x10041) for 'a' typed with Shift held.
  - WM_LBUTTONDBLCLK: the original called rootWin_onPress1b (handler 10, 0x004657e0) and then onPoint. The translator
    gave onPoint only. onPress1b stores the click point, sets the button word 0x00822d68 = 1 and sets 0x00822d78 /
    0x00822d74 = 1. The right button is translated by analogy (onPress2b 0x00465840, U-1631).
  - input_check.cpp's selftest expects the two steps.

### Recording and gating (port/test/app_replay.py, test_app_replay.py)
- `record <script>` launches the app with `--cursor message --keys message --clock-origin 0x100000 --no-activate --dpi 96`
  in a temporary data directory. app_drive.App.post sends posted messages only, and waits are counted in the session's
  own frame heads (frames.tsv) or in quiet presents.
  - A key is down, then up 0.25 s later. Posting both at once queues TranslateMessage's WM_CHAR after the key-up, which
    neither a real keyboard nor the original does.
  - Uppercase keys for mainLoop's H1 switch ('S', 'L') are posted as WM_CHAR alone (scenario.py _cova_char's rule).
  - The session directory is copied to port/corpus/app/<name>/, with posted.json (each posted message with its seq) and
    data/ (the overlay).
- `gate`: sg_run_boot over trace.trace (port/test/live.py boot_state: install files, GDI text) against framehash.bin,
  record by record.

### The cross-check (log/roadmap/S7/4_xcheck.py)
- WHY NOT trace_tool replay: its plan is one value per main-thread clock call of the original, and a delivery is placed by
  that call count. The app's trace holds the port's reads only (the original makes about 99 timeGetTime calls a
  frame, the port 1-2), so no plan can be built from it.
- The method: the scenario cov_r_s74x (s36a family, registered at run time, record mode, C3FT_FRAMEHASH=sim,
  C3FT_AUDIO=none) and an agent part appended to scenario_det.js:
  - in-game messages are posted on the main thread at the entry of the pump call that took them in the app, in the
    same live frame. The frame is FH.ctl + 16, the frame-head count, which equals the app's live frame. P6 = return
    address 0x004199e5 of `call pumpInput483bd0` at 0x004199e0; H1 = return 0x0041c0d2 of `call pumpMessages` at
    0x0041c0cd;
  - a message without commands (mouse move, wheel) is posted at P6 of the frame after it arrived;
  - only the driver's messages are posted: the original's pumps run TranslateMessage too (corpus: KEYDOWN 'H' + posted
    'h' gives two WM_CHAR records);
  - buttons and keys set SimGolfShim_SetVirtualKey when posted;
  - the wheel's lParam is mapped to the original window's screen point;
  - the cursor the game reads in live frame f (jgld ScreenToClient slot RVA 0x12c618) is the app's frame-f CURSOR value;
  - frame 0 (title menu, location screen) gets the app's two click groups through the harness, at the golden screens.
- `compare` checks the sim hashes per record, `handlers` the HANDLER sequence (the original's trace by trace_rec.js,
  installed after the agent part so it logs the cursor values the game was given).
- Runs (log/c3/alt/refs/cov_r_s74x/<label>):
  - x1 / x2: the first recording. Sim equal in all 334 records but g_frameMs. Handlers: four differences (the modifier
    bits, onPress1b, the H1 double click).
  - x3: after the H1 rule and the bits. One difference: the WM_CHAR code.
  - x5: everything fixed. 340 / 340 records and 59 / 59 handler calls.
  - x6_tee: the control, RED.
- Measured details:
  - The agent posted 49 messages, 0 left over.
  - In 11 of 340 frames the original read the cursor once more: the frames where a WM_MOUSEMOVE / WM_MOUSEWHEEL was
    posted, which jgld's window procedure answers with its own read (U-1064). The value sets are equal in all 340
    frames (U-1633).
  - The title menu's idle passes (the app's 401 calls before the Sandbox click) leave record 0 equal: all 203 objects.
- The wheel made no root-handler call in the original and changed no sim object (U-1632).
- WM_SYSCHAR reached no root handler in the original.

## 3. Commands

```
~/bin/py -3.12 log/roadmap/S7/4_mirror.py --build             # mirror (S7.7's recipe, own dirs) + the two patches + x64 build
cd port/build/S7_4_head; ~/bin/py -3.12 port/test/build.py --arch Win32
export SG_PORTSIM_DLL=$PWD/port/build/S7_4/Release/portsim_c.dll
~/bin/py -3.12 -m port.test.app_replay record hole            # also saveload, report (each about 1-2 min, an app window)
~/bin/py -3.12 -m port.test.app_replay gate [--backend win32] --out ../../../log/roadmap/S7/4_gate_x64.tsv
~/bin/py -3.12 -m pytest port/test/test_app_replay.py -p no:cacheprovider -v          # 14 passed
~/bin/py -3.12 -m pytest port/test/test_app_live.py -p no:cacheprovider -v            # 13 passed (quiet PC)
cd ../../..; SIMGOLF_ALLOW_MULTI=1 ~/bin/py -3.12 log/roadmap/S7/4_xcheck.py run --app hole --label x5 --trace   # ~5 min
~/bin/py -3.12 log/roadmap/S7/4_xcheck.py compare --app hole --label x5
~/bin/py -3.12 log/roadmap/S7/4_xcheck.py handlers --app hole --label x5
SIMGOLF_ALLOW_MULTI=1 ~/bin/py -3.12 log/roadmap/S7/4_xcheck.py run --app hole --label x6_tee --mutate tee     # control
```
The cross-check needs the recording it compares against. A re-recorded `hole` needs a new run: the cross-check of the
current port/corpus/app/hole is x5.

## 4. Controls

| control | result |
|---|---|
| input byte changed (tee x 300 -> 364, HANDLER a0 low byte 0x2c -> 0x6c), app replay | FAIL, `sim`, at or after the tee's frame (test_control_input_byte_changed) |
| framehash record 200, one byte of sim hash 57 | FAIL at record 200, `sim` |
| framehash record 200, one byte of T | FAIL at record 200, `T` |
| cross-check: the original given the tee click at x 364 | first divergence record 185 (the click's frame 185), tile and hole objects: RED |
| the arithmetic | the tee tile moves by 64 px (the tee's tile and its hole record change); the framehash controls change one checked byte |

Blind on purpose: none of the app-replay controls. The cross-check cannot see g_frameMs (masked, U-1637).

## 5. Open
- U-1630: modal dispatches in live frames (report, save dialog, browser) are not driven into the original. Only `hole`
  is cross-checked.
- U-1631: right double click by analogy.
- U-1632: the wheel on other screens.
- U-1633: cursor reads served by index.
- U-1634: key-table timing.
- U-1635: the H1 reorder is unexercised.
- U-1636: the translator patch waits for the merge.
- U-1637: g_frameMs.
- Mirror-only: test_movie_inventory fails in git-archive mirrors (S7.7's too). It is not this step's.
