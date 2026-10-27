# S7.2 notes: input translation for the Windows host

Step row: `re/roadmap/S7_steps.tsv` S7.2. Worker: sonnet high, session cmvmrt0pk3tg9o115x7tb66f4. Status: DONE (gate met, see section 1).

## 1. Result

`port/app/input_map.{h,cpp}` is a pure C++17 translator (no window, no Win32 header, no portsim dependency): one window message
`(msg, wParam, lParam)` plus a `HostInput` (the host's GetKeyState / GetAsyncKeyState / MapVirtualKeyA / cursor / focus-ring /
clock answers) gives the ordered `Step`s the original took, as the corpus traces record them: root-handler calls (handler id, a0,
a1, async_ctrl, async_shift, t) and the reads made while handling the message (with the answer returned). The checker
`port/app/tools/input_check.cpp` feeds it every MSG record of every trace in `port/corpus/traces` and compares.

```
traces 67   MSG records 26,861 (all translated)   HANDLER records 23,432 = 23,432 produced   owned reads 42,441   cursor answers 5,614
focus-child keys (handlers 12 / 13) 25   mismatches 0
scaled host windows (1600x1200 +8,31 / 1000x750 / 1920x1440 +3,40): 0 mismatches each
selftest 15 / 15; pytest port/test/test_app_input.py 16 passed (31.8 s), log/roadmap/S7/2_test_app_input.log
```

The brief says 64 traces; the corpus holds 67 (cov_r_bio, cov_r_s8_play, cov_r_flic_input and others recorded after the brief was
written). cov_a_quit, cov_a_quit_title and golfer_events have a sidecar and no trace, as before (U-1115).

Reproduce (in the HEAD export with the one CMake line, recipe `log/roadmap/S0/export.md`, root `port/build/S7_2_head`):

```
~/bin/py -3.12 log/roadmap/S7/2_mirror.py --build          # sync the files into the mirror + build simgolf_input_check (x64 Release)
cd port/build/S7_2_head
port/build/S7_2/app/Release/simgolf_input_check.exe --selftest port/corpus/traces/*.trace
port/build/S7_2/app/Release/simgolf_input_check.exe --viewport 1600,1200,8,31 port/corpus/traces/*.trace
port/build/S7_2/app/Release/simgolf_input_check.exe --negctl 1 port/corpus/traces/*.trace      # 1..10, see section 3
~/bin/py -3.12 -m pytest port/test/test_app_input.py -p no:cacheprovider
```

The shared tree's `port/CMakeLists.txt` was not touched (2_merge.md has the line). `port/test/test_app_input.py` builds the checker
with `cmake -S port/app` into `port/build/S7_2_app`, so it also runs in the shared tree before the merge.

## 2. The mapping (measured; every row holds for all records of its class)

Window procedure = jgld windowProc 0x100692b0; it calls forwarders that hand the exe's listener (g_listener 0x1012872c, class not
identified: U-1610) `(dbl, x, y)` etc. through its vtable (c3cd_notes.txt: slot 3 key, 4 character, 5 mouse move, 7 left press, 8 left
release, 9 right press, 10 right release). The root handlers are vtable 0x004ba2fc slots 29..39 (trace HANDLER ids 1..11).

| message (corpus count) | steps, in trace order | evidence |
|---|---|---|
| WM_KEYDOWN 0x100 (5,441) | GetKeyState(16), (17), (18); MapVirtualKeyA(vk, 2); then, unless a focused child took the key: result != 0 -> handler 8 onChar(vk, 0); result == 0 -> handler 7 onKey(0, vk). A focused child instead: command 12 (a0 = vk). The vk is also stored as the "last key-down" | 5,274 H8 with a0 == vk, 159 H7 with (0, vk) and result 0, 8 child-taken (cov_r_bio) |
| WM_CHAR 0x102 (5,949 with a handler, 17 child-taken) | unless a focused child took it: handler 7 onKey(ch & 0xff, last key-down's vk), or command 13 (a0 = ch) | a1 == last vk in 5,948 of 5,949 (the one other is a WM_CHAR before any key-down: a1 = 0, the initial state) |
| WM_KEYUP 0x101 (5,419) | GetKeyState(16), (17), (18); handler 9 onChar0(0, 0) | 5,419 |
| WM_MOUSEMOVE 0x200 (5,614) | one cursor read (the window procedure's own, logical coordinates); no handler | read == host cursor in 5,614; == lParam in 5,610 (U-1617 note below) |
| WM_LBUTTONDOWN 0x201 (1,945) | handler 2 onPoint(x, y) | a0, a1 == the lParam words (signed 16-bit), no flip, no offset |
| WM_LBUTTONUP 0x202 (1,944) | handler 1 onPress1(x, y) [the click]; GetAsyncKeyState(17); GetAsyncKeyState(16); handler 3 onUnpoint(0, 0) | click Ctrl / Shift = the two answers (0 or 0x8001) |
| WM_RBUTTONDOWN 0x204 (266) | handler 5 onPoint2(x, y) | |
| WM_RBUTTONUP 0x205 (266) | handler 4 onPress2(x, y); GetAsyncKeyState(17), (16); handler 6 onUnpoint2(0, 0) | |

Notes on what the table implies for the host:

- A click is recognised on button RELEASE (time_model.md section 7); the press only records the point. The translator produces both.
- The coordinates the handlers get are the message's lParam (message time); the cursor reads of the same frame are the live cursor at
  read time. They are different inputs and stay different in the translator (`lparam_point` vs `HostInput::cursor`). In the corpus
  the harness moves its virtual cursor separately from the posted messages in 4 of 5,614 mouse-move reads (cov_r_bio 1, cov_r_reload 3,
  e.g. lParam (792, 29), cursor (632, 553)): the checker answers the cursor from the DELIVER cursor records, and `mousemove_cursor_eq_lp`
  / `_ne_lp` report the 5,610 / 4 split.
- lParam's repeat count / previous-state bits are ignored (jgld's onKey reads only the key and the down flag: c3dg Purpose of 0x10069c70),
  so a held key repeats WM_KEYDOWN and the game sees every repeat, as the original did under Windows.
- WM_CHAR a0 is the low byte (jgld onChar reads `byte ptr [ebp+0xc]`, 0x10069d6a); the corpus' largest is 122 (U-1618).
- Not in the corpus: WM_*DBLCLK (the class style is 0x2b, CS_DBLCLKS included, `Display::createWindow` 0x100657e0, so a real fast second click
  becomes one: U-1612; translated as the press), wheel / middle button (dropped, U-1613), WM_SYS* keys (U-1614), modifier bits in a key code
  (U-1611). They return `Evidence::Unmeasured`; anything else `NotInput`.
- Focus-child keys: a key message that no root handler follows was taken by a focused child (`tick.cpp parse_trace`, S1.5). The translator
  asks `HostInput::focus_child_takes(msg, wp)` after the message's reads and emits command 12 / 13 instead of the root handler. It cannot
  decide that itself (U-1616).

## 3. Controls (checker `--negctl N`, tests parametrised in test_app_input.py)

| N | wrong body | mismatches | verdict |
|---|---|---|---|
| 1 | left / right buttons swapped | 6,631 | RED |
| 2 | WM_CHAR takes onChar's shape, WM_KEYDOWN onKey's | 11,382 | RED |
| 3 | y flipped (599 - y) | 10,035 | RED |
| 4 | y + 23 (caption height) | 10,035 | RED |
| 5 | Ctrl bit of the click dropped | 4 | RED (thin: 2 Ctrl-held clicks, both in cov_a_golfer) |
| 6 | focused child never takes a key | 50 | RED |
| 7 | a key-down always onChar | 159 | RED |
| 8 | WM_CHAR's code is its own character | 1,106 | RED |
| 9 | Shift bit of the click dropped | 8 | RED (thin: 4 Shift-held click records) |
| 10 | the host window's scale / origin ignored | 0 identity / 10,035 with `--viewport 1600,1200,8,31` | BLIND on the corpus (identity window), RED on the scaled run |

Arithmetic checked: controls 3 and 4 move every mouse coordinate (no corpus y is a fixed point of 599 - y); 5 and 9 change only clicks
with the modifier down, which is why they are few; 10 cannot differ when client == logical, hence the `--viewport` mode (physical
coordinates `ceil(logical * client / 800) + origin`, the smallest point that floors back exactly), which is the oracle for the scaling.
What the checker does NOT judge independently: the answers to GetKeyState / GetAsyncKeyState / MapVirtualKeyA (they come from the
trace, the translator must ask for the right key in the right order, make the right number of reads per class, pass the async answers into
the click's fields and decide handler 7 / 8 from the MapVirtualKeyA result); the focus callback (U-1616); `Command::t` (U-1615).
The cursor answer is judged independently (virtual cursor from the DELIVER records vs the CURSOR record).

## 4. How S7.3 uses it

1. Keep one `Translator` per window, call `translate(msg, wp, lp, host)` for every key and mouse message in dispatch order (the
   last key-down's vk is state).
2. `HostInput` live: `GetKeyState`, `GetAsyncKeyState`, `MapVirtualKeyA(code, 2)`, `GetCursorPos` + `ScreenToClient` (physical client
   pixels; the `Viewport` converts), `timeGetTime`; `focus_child_takes` = the port window tree's key dispatch (today only chareditor.cpp's bio
   entry exists, U-1191).
3. `Viewport`: client width / height of the 800x600 content area and its origin inside the host window (letterbox); Translator::to_logical
   maps physical -> logical with floor division, so out-of-range points stay out of range as in the original (signed words, no clamp).
4. Copy each `Call` step into `portsim::Command{RootHandler, handler, a0, a1, async_ctrl, async_shift, t}`; the `Read` steps are the
   answers the original's UI code would have read (a record of them belongs in the session trace). The message-owned reads are all this
   library covers: the frame's own GetAsyncKeyState(VK_SHIFT) reads (P6 / H2), GetKeyboardState (capsLockOff) and View_toLocal's three
   cursor reads per frame are the sim's, answered by the host from the live state.
5. Dispatch timing (which pump takes a batch) is S7.1's: the corpus puts 952 of 954 dispatches in H1 right after the frame's palette
   updates (time_model.md section 7).

## 5. Open items

U-1610..U-1619 (`log/roadmap/S7/2_uncertain.tsv`): the listener class (1610), modifier bits in key codes (1611), double clicks (1612),
wheel (1613), Alt keys (1614), `t` (1615), the focus callback (1616), MapVirtualKeyA's dead keys and layout (1617), characters above 0x7f
(1618), the scaling control being blind on the corpus (1619). Their common cure is one recording that posts those messages through the
harness (WM_LBUTTONDBLCLK, Shift / Ctrl / Alt held keys, WM_CHAR 0xf1, a wheel step) and reads the HANDLER records after each; that needs a
game slot, which this step did not have (needs_game no).

## 6. Files

See `2_files.txt`. Mirror driver `2_mirror.py` (syncs the five owned files into `port/build/S7_2_head` and builds). Nothing was built
into `port/build/x64` or any other build dir of the shared tree; the mirror's build dir is `port/build/S7_2_head/port/build/S7_2`, the
test's is `port/build/S7_2_app`, both gitignored.
