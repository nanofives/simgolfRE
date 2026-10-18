# Clock and input trace schema, version 1 (phase 2.3, 2026-10-18)

A trace is everything one deterministic run of the original took from outside its own memory: the value and call site of
every main-thread clock read in call order, every input the harness delivered, what the game read back as input, the
polls and checkpoints, and one record per mainLoop frame. It is the input schema of the headless port (phase 3.7 / 3.8)
and of the segment corpus (step 2.4). The time and input model it records is re/analysis/sim/time_model.md.

Tools: writer `re/frida/js/trace_rec.js` (records) + `re/tools/trace_tool.py` (header, record / replay drivers), reader and
checks `re/tools/trace_format.py` (`parse`, `validate`, `summary`, `diff`, `replay_plan`, `Writer` for synthetic traces).
Files: `port/corpus/traces/<scenario>.trace` (local corpus, gitignored) and a sidecar `<file>.json` (run folder, host
conditions, agent statistics, sha256, comparison with the pinned reference); the sidecar is not part of the trace.

## 1. Conventions

- Little-endian, no padding, no alignment. `u8/u16/u32/u64/i32` as usual.
- **Ordinal / position.** Every main-thread clock call (timeGetTime through any module's import slot, or
  QueryPerformanceCounter) has an ordinal 0, 1, 2, ... in call order. A record's `pos` is the number of clock calls made
  before it was written, so a record with `pos = n` happened after clock call n-1 returned or, for deliveries and polls,
  inside clock call n-1 (the harness acts only from inside a main-thread timeGetTime call, scenario_det.js onDue).
- **Call site** (`ra`): the return address of the call. golf_clean.exe addresses are VAs (the exe has no relocations);
  a DLL address is `0x80000000 | id << 24 | rva` with id 1 jgld.dll, 2 sound.dll, 3 Terrain.dll, 4 binkw32.dll,
  5 winmm.dll (the shim), so a trace does not depend on where a DLL was mapped (jgld.dll is relocated under the shim:
  0x03d90000 in the runs measured). `0xff000000 | low 24 bits` = an address in no module of that table.
- **Timer handles.** Timer::startMmPeriodic / startMmOneShot (0x00486dc0 / 0x00486e40) pass the Timer object, a heap
  pointer, as SetTimer's nIDEvent and timeSetEvent's dwUser, and the timer procedures post 0x401 with it as wParam
  (0x00486f63..0x00486f71, 0x00486fc0..0x00486fce). The trace names a timer by its registration ordinal (0, 1, ... per
  SetTimer / timeSetEvent call).
- Nothing host- or time-of-day-dependent is written into the trace itself (no labels, dates, host names, heap pointers);
  two runs of one schedule must give byte-identical files. Host facts go to the sidecar. The real keyboard's toggle bits
  (NumLock, ...) do appear in KBSTATE / KEYSTATE results, because the game reads them: a trace is host-specific there.

## 2. File layout

```
offset 0   char[8] magic "SGTRACE\0"
       8   u16 version (1)
      10   u16 header size (32)
      12   u32 meta_len
      16   u32 step     virtual ms per main-thread timeGetTime call (1)
      20   u32 t0       value of the first timeGetTime call (0x00100000)
      24   u32 qstep    QPC counts per main-thread QPC call (= host QPC frequency / 1000; 10,000 here)
      28   u32 q0       value of the first QPC call (0x10000000)
      32   meta_len bytes of UTF-8 JSON, keys sorted, no spaces:
           {"schema":1, "scenario":..., "frame_ra":4258863, "slots":[[module, kind, rva], ...],
            "snapshot":[[va, len], ...], "spec":"re/model/trace_schema.md"}
           slots = the import slots the run redirected (index = the `slot` field of CLOCKRUN), kinds tgt / qpc /
           SetTimer / KillTimer / timeSetEvent / timeKillEvent / audio:*
      ...  records until the end of the file; the last one is END
record:    u8 type, u8 sub, u16 payload length, payload
```

## 3. Record types

| type | name | sub | payload | meaning |
|---|---|---|---|---|
| 0x01 | CLOCKRUN | kind 1 tgt, 2 qpc | u32 first, u32 count, u32 ra, u32 slot, u64 v0 | `count` consecutive clock calls (ordinals first..first+count-1) from one call site and slot; call k returns v0 + k*step (tgt) or v0 + k*qstep (qpc). A run breaks at any other record, another site, another slot or a value off the step. |
| 0x02 | FRAME | 0 | u32 frame, u32 pos, u32 t, 96 bytes snapshot | mainLoop frame head: written right after the timeGetTime call at 0x0040fc29 (ra 0x0040fc2f), `t` its value. Snapshot (in this order): g_rootInput 0x00822d58 (0x40), key code 0x00822b98, key 0x00822b9c, char 0x0083821c, 0x00838208, g_ui83ab9c 0x0083ab9c (4 bytes), g_flags 0x0059e7b8, g_date 0x00834170, g_rng 0x00822d9c. |
| 0x03 | DELIVER | kind | u32 pos, then by kind: 1 cursor `i32 x, i32 y`; 2 vkey `u32 vk, u32 state` (0 pass-through, 1 forced up, 2 forced down); 3 post `u32 cls, u32 hwnd, u32 msg, u32 wp, u32 lp` (cls 0 NULL hwnd = the thread queue, 1 the game window, 2 explicit hwnd); 4 w32 `u32 addr, u32 value`; 5 timer `u32 tkind (1 window, 2 mm), u32 handle, u32 t`; 6 note (no fields) | what the harness did: SimGolfShim_SetVirtualCursor, SimGolfShim_SetVirtualKey, PostMessageW from the main thread, a fixture's memory write, a virtual timer firing (window timer: the TIMERPROC called as (hwnd, WM_TIMER, id, t); mm timer: the callback called as (id, 0, dwUser, 0, 0)). |
| 0x04 | POLL | bit0 checkpoint, bit1 final | u32 pos, u32 index, u8 len, name | a poll of the driver (every 100 virtual ms); with bit0 the checkpoint dump taken there and its name. Exactly one poll has bit1: the last, after which the trace ends. |
| 0x05 | MSG | 1 = wp is a timer handle | u32 pos, u32 msg, u32 wp, u32 lp | a message entering jgld's window procedure 0x100692b0 on the main thread, in dispatch order; only input classes: 0x100..0x109, 0x113, 0x200..0x20e, 0x400..0xbfff (others, e.g. registered 0xc000+ messages, are counted in the sidecar only) |
| 0x06 | KEYSTATE | 1 GetKeyState, 2 GetAsyncKeyState | u32 pos, u32 ra, u32 vk, u32 result (low 16 bits) | a key-state read through golf_clean.exe's import (after the shim's virtual-key overlay) |
| 0x07 | KBSTATE | 1 = success | u32 pos, u32 ra, u16 n, n x (u8 vk, u8 value) | GetKeyboardState (capsLockOff 0x004321d0, once per frame): the non-zero bytes of the 256-byte state |
| 0x08 | CURSOR | 1 = success | u32 pos, i32 x, i32 y | the cursor in client coordinates as jgld's Display::getCursor 0x10066780 returns it (GetCursorPos then ScreenToClient; logged after ScreenToClient) |
| 0x09 | HANDLER | handler id | u32 pos, u32 a0, u32 a1 | entry of a root-window input handler (vtable 0x004ba2fc slots 29..39): 1 onPress1 0x00465580 (x, y), 2 onPoint 0x00465610 (x, y), 3 onUnpoint 0x00465650, 4 onPress2 0x00465680 (x, y), 5 onPoint2 0x00465720, 6 onUnpoint2 0x00465740, 7 onKey 0x00465750 (a, code), 8 onChar 0x004657b0 (c), 9 onChar0 0x004657d0, 10 onPress1b 0x004657e0, 11 onPress2b 0x00465840 |
| 0x0a | TAKE | 1 key, 2 code | u32 pos, u32 ra, u32 value | a non-zero result of the take-and-clear readers 0x0045ae50 ([0x00822b9c]) / 0x0045ae30 ([0x00822b98]); ra 0x0041c0d7 / 0x0041c0e2 = H1's key dispatch input |
| 0x0b | SLEEP | 0 | u32 pos, u32 ra, u32 ms | Sleep from golf_clean.exe (wall time only) |
| 0x0c | MAPVK | 0 | u32 pos, u32 ra, u32 code, u32 maptype, u32 result | MapVirtualKeyA from golf_clean.exe (Window::dispatchAccel, ListBox::onKey): keyboard-layout dependent |
| 0x0d | TIMERSET | 1 SetTimer, 2 KillTimer, 3 timeSetEvent, 4 timeKillEvent | u32 pos, u32 ra, u32 handle, u32 period, u32 fn, u32 extra | a timer registration or kill by golf_clean.exe (handle = registration ordinal; extra = fuEvent for timeSetEvent, the API result for kills) |
| 0x7f | END | 0 | u32 n, u32 frames, u32 polls, u32 dumps | totals; nothing follows |

Readers must reject an unknown type (version 1 has no skippable records).

## 4. Validity rules (`trace_format.validate`)

1. Header magic, version 1, header size 32, metadata parses.
2. CLOCKRUN ordinals are contiguous from 0; count >= 1; kind matches the slot's kind in `meta.slots`.
3. Virtual clock rule (traces of scenario_det.js runs; `--any-clock` skips it): the i-th timeGetTime call returns
   t0 + i*step and the i-th QPC call q0 + i*qstep, over the whole file.
4. Every other record's pos is <= the clock calls before it and never decreases.
5. FRAME indices count from 0; a FRAME follows a clock call whose ra is `meta.frame_ra` and its pos equals the clock count.
6. POLL indices count from 0; exactly one final poll; END is last and its totals equal the counts.

## 5. Replay contract (what makes a trace sufficient)

`trace_tool.py replay` boots the same install and runs the original with: the main thread's timeGetTime / QPC returning,
call by call, the recorded values (a call whose exe return address differs from the recorded one is counted as a
mismatch); at each recorded delivery position, the recorded deliveries in recorded order (cursor, virtual keys, posts to
the game window found by class `JackalClass`, memory writes, virtual timer deliveries to the timer registered under that
handle ordinal); polls and checkpoint dumps at the recorded positions. The scenario script, its schedule and
scenario_det.js's virtual clock are not used on the main thread (its timeGetTime counter stays 0). The replay writes its
own trace; sufficiency is shown when that trace is byte-identical to the source and the checkpoints equal the pinned
reference (results in re/analysis/sim/time_model.md section 9).

Conditions a replay shares with the record (outside the trace): the install files and shim build, the audio stand-in
(C3FT_AUDIO=none), the b5 stack seed, the scenario family's restore of saves / themes before boot, and the host's
keyboard toggle state and keyboard layout (MapVirtualKeyA).

## 6. Sizes measured (2026-10-18, LIVING-PC)

| scenario | frames | clock calls (tgt / qpc) | records | file bytes | bytes per frame |
|---|---|---|---|---|---|
| sandbox_basic | 439 | 66,744 (48,278 / 18,466) | 20,520 | 580,656 | 1,322 |
| course_season | 6,296 | 936,098 (627,816 / 308,282) | 275,931 | 7,867,550 | 1,249 |
| mood5_fixture | 245 | 57,910 (33,734 / 24,176) | 13,430 | 371,014 | 1,514 |

CLOCKRUN records are 81.5-82.9% of the bytes (the frame wait's waitTicks pattern starts a new run after 1, 1, 9 and 1 calls).
One frame is one date unit and a game year is 8,192 frames, so a recorded game year is about 10 MB uncompressed; the
files are repetitive and compress well if storage matters.
