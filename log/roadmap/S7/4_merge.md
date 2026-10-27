# S7.4 merge (apply after S7.7's 7_merge.md)

Worker S7.4, 2026-10-25. Notes: log/roadmap/S7/4_notes.md. Files: 4_files.txt. U-rows: 4_uncertain.tsv (U-1630..U-1637).
Mirror that measured everything below: port/build/S7_4_head (`~/bin/py -3.12 log/roadmap/S7/4_mirror.py --build`).

## 1. Files S7.4 owns (take the shared tree's copy as it is)

- port/test/app_replay.py (new), port/test/test_app_replay.py (new)
- port/app/host.h, port/app/host.cpp, port/app/main.cpp (edited in the shared tree):
  - `--clock-origin <v>`: clock answers v + ms since the first answer;
  - `--keys message`: GetKeyState / GetAsyncKeyState from the received key messages;
  - the dispatch log (input_dispatch.tsv, dispatches.tsv, body_calls.tsv);
  - the H1 rule (see notes section 3).
- port/corpus/app/ is local (port/.gitignore `corpus/`): nothing to commit there.

## 2. Patches of files S7.4 does not own (apply with `patch -p1` from the repo root)

Both apply cleanly to the shared tree's files as of 2026-10-25. main.cpp needs 4_window.patch to compile: it sets
`WindowOptions::before_translate` / `key_state`.

1. `log/roadmap/S7/4_window.patch`: port/app/window.h, port/app/window.cpp (S7.3's). It adds two `WindowOptions` callbacks:
   `before_translate(msg, wp)`, called before the translator reads anything for a message, and `key_state(vk, async)`,
   an override of the translator's GetKeyState / GetAsyncKeyState answers (a result < 0 means ask Windows). Both default
   to empty, so behaviour is unchanged unless `--keys message` is given.
2. `log/roadmap/S7/4_input_map.patch`: port/app/input_map.cpp, port/app/tools/input_check.cpp (S7.2's). These are the
   translator fixes the cross-check measured in the original:
   - a key-down's code ORs in 0x10000 for Shift, 0x20000 for Ctrl and 0x40000 for Alt when GetKeyState's high bit is set
     (uiKeyEvent 0x00495918..0x00495943);
   - the WM_CHAR's code is that key-down code;
   - WM_LBUTTONDBLCLK / WM_RBUTTONDBLCLK give rootWin_onPress1b / onPress2b (handler 10 / 11) before onPoint / onPoint2;
   - the selftest's double-click check now expects these two steps.
   test_app_input: 16 passed with the patch.

## 3. UNCERTAINTIES.md

Append the eight rows of log/roadmap/S7/4_uncertain.tsv (U-1630..U-1637). Change S7.2's rows:
- U-1611 -> `resolved`, path text replaced by: "Measured by roadmap S7.4's cross-check, recording `hole` replayed into the
  original (log/roadmap/S7/4_notes.md section 4):
  - the original passes rootWin_onKey(0, 0x10010) for Shift's key-down, (0, 0x20011) for Ctrl's, (0, 0x40012) for Alt's;
  - it passes rootWin_onChar(0x40058) for Alt+X;
  - it passes rootWin_onKey(0x61, 0x10041) for the WM_CHAR of 'a' typed with Shift held.
  The bits are uiKeyEvent's (0x00495918..0x00495943). 4_input_map.patch adds them to the translator."
- U-1612 -> `resolved` for the left button: "S7.4 measured WM_LBUTTONDBLCLK -> rootWin_onPress1b(x, y) then onPoint(x, y)
  (onPress1b 0x004657e0: click point, button word 0x00822d68 = 1, flags 0x00822d78 / 0x00822d74 = 1). The right button
  is translated by analogy: U-1631."
- U-1613: append "S7.4: the wheel (+-120, in-game) made no root-handler call in the original and changed no sim object;
  other screens are U-1632."
- U-1614: append "S7.4:
  - WM_SYSKEYDOWN / WM_SYSKEYUP reach the root handlers as key-down / key-up with the Alt bit (onKey(0, 0x40012),
    onChar(0x40058), onChar0);
  - the WM_SYSCHAR TranslateMessage makes reaches no root handler.
  WM_DEADCHAR / WM_SYSDEADCHAR are still unmeasured."

## 4. CLAUDE.md (port section, after S7.7's paragraph)

Roadmap S7.4, app recordings in the boot gate (2026-10-25, notes log/roadmap/S7/4_notes.md, U-1630..U-1637; U-1611 /
U-1612 resolved):
- `port/test/app_replay.py record <hole|saveload|report>` drives the app with posted messages only. The app runs with
  `--cursor message --keys message --clock-origin 0x100000`, and the session goes into port/corpus/app/<name>/
  (local), together with posted.json and the app's new dispatch log (input_dispatch.tsv, dispatches.tsv, body_calls.tsv).
- `app_replay.py gate [--backend win32]` runs sg_run_boot over each recording against its own framehash.bin.
  hole 340, saveload 266 and report 205 records PASS on x64 and Win32 (port/test/test_app_replay.py, 14 tests).
- The recordings hold:
  - Shift / Ctrl clicks, a double click, the wheel, Alt+X and Shift+A;
  - one hole built and opened;
  - a save the `S` key wrote into the app's overlay, loaded back through the browser (g_date returns to the save's value);
  - the F1..F4 report screens (modal).
- CROSS-CHECK (log/roadmap/S7/4_xcheck.py, instance s7_4): the app's raw input was posted into the original at the same
  pump calls, by frame-head count:
  - P6 = the `call pumpInput483bd0` at 0x004199e0;
  - H1 = the `call pumpMessages` at 0x0041c0cd;
  - the cursor reads were answered from the app's CURSOR records;
  - the virtual keys were set as the messages were posted.
  Result: all 340 records equal the original's in every sim object but g_frameMs, which stores a timeGetTime value
  (U-1637). All 59 root-handler calls are identical in handler, arguments and frame.
- The cross-check found three things, fixed:
  - the translator dropped uiKeyEvent's modifier bits;
  - the translator dropped the double click's rootWin_onPress1b;
  - the host let H1 take several messages or a key. H1 takes ONE mouse-range message: pumpMessages drains WM_MOUSEMOVE,
    then display slot +0x13c (jgld 0x1006b7e0) makes one PeekMessage(0x200..0x20a). P6 loops PeekMessage(0, 0) until
    the queue is empty.
- Control: the tee click moved by 64 px in the original's input diverges at record 185, the head right after that click.
