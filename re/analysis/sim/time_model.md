# Time and input model of the original (phase 2.3, 2026-10-18)

How wall time and input reach SimGolf's simulation, as the specification for the headless port (phase 3.7 tick and
commands, 3.8 integrator) and for the recorded traces (schema re/model/trace_schema.md). Every claim cites an address of
golf_clean.exe (VA) or a DLL (VA at the DLL's preferred base), or a measured trace. NO-GUESSING applies; open points are
filed as U-0650.. in UNCERTAINTIES.md.

Evidence: the static audit `re/tools/time_input_audit.py` -> `re/analysis/sim/time_input_sites.tsv` (253 call sites of 45
clock, timer, wait and input imports in the four modules, with the function holding each site); mainLoop's structure
(re/analysis/sim/mainLoop_structure.md); and traces of three pinned scenarios recorded under the virtual clock
(`re/tools/trace_tool.py`, measured on LIVING-PC, instance instances\p23, shim build 381ed2b2): sandbox_basic (439 frames),
course_season (6,296 frames), mood5_fixture (245 frames). Notes: log/p2/p23_notes.txt.

## 1. Summary in numbers

| quantity | value | evidence |
|---|---|---|
| clock APIs golf_clean.exe imports | timeGetTime only (+ SetTimer / KillTimer / timeSetEvent / timeKillEvent, Sleep) | import table; no GetTickCount, GetSystemTime, GetLocalTime, QPC, GetCursorPos |
| clock reads whose VALUE enters simulation state | 1: seedFromTime 0x00467110 (once, from gameMain) | section 5 |
| game time unit | the FRAME: g_date += 1 per frame in P5 | 0x0041762f..0x0041763e |
| date units | 1,024 frames = 1 game month, 8 months = 8,192 frames = 1 game year | appendDate 0x0040d7b9..0x0040d7d5 (`sar 0xa`, `and 7`, `sar 0xd`) |
| frame pacing rule | the H1 wait ends no earlier than frame-head time + 87 ms (175 / 2), or + 125 ms (250 / 2) with g_flags bit 17 | H1 wait 0x0041e639..0x0041e6b1 |
| frame period under the virtual clock (1 ms per call) | 99 virtual ms in 99.9% of frames (sandbox 438/438, course_season 6,289/6,295, mood5 244/244) | traces |
| frame period in real time | between 87 ms and 87 ms + one waitTicks(1) quantum (>= 10 ms) + one pump, when the frame's own work takes < 87 ms; the work time when it is longer | section 3 (from the loop structure; not measured on real time: U-0653) |
| pause | g_flags bit 2, toggled by key code 0x50 at 0x0041d020; P5 skips the whole tick, including the date, while frames keep running | 0x004175f2..0x00417603 |
| game speed setting | none observed: bit 3 (date to the next even value) and bit 17 (125 ms frames) have no setter in game logic, and the whole-word writes seen (title menu 0x40000000, loadGame, the five saves read) leave both clear | model_b.md 7.3, U-0160 |
| main-thread clock calls per frame | 99 timeGetTime + 0..84 QPC (Palette::update) in a normal frame | traces |
| timers alive during play | none in the three scenarios: the only registration (SetTimer, 50 ms, TIMERPROC 0x00486f40) is killed at the same clock position, before the title menu | TIMERSET records, 0x00486e23 / 0x00486ef9 |
| input delivery latency | key messages posted during frame f are dispatched in frame f + 1 (297 of 301 WM_KEYDOWN in course_season); mouse messages in frame f or f + 1 | traces |

## 2. Clock sources (static audit; full list time_input_sites.tsv)

| module | API | sites (call / via register) | functions | thread | feeds |
|---|---|---|---|---|---|
| golf_clean.exe | timeGetTime [0x004ba21c] | 26 / 36 | mainLoop frame head 0x0040fc29 and H1 wait 0x0041e639..0x0041e68e; waitTicks 0x0045bf80; pumpAndRedraw 0x0045c0c0; seedFromTime 0x00467110; playAudioFile 0x0043cce0; rootWin_onPoint / onUnpoint / onKey; Snd::play / Snd::busy; Button::beginPress / endCapture; scrollbarOnMouseMove / scrollbarArrowRepeat; showCreditsScreen; showHoleStatsScreen; movie* (0x0049c940..0x0049ccc0); net* (netService, netSendMessages, netServiceControl) | main | section 5 table |
| golf_clean.exe | SetTimer / KillTimer / timeSetEvent / timeKillEvent | 2 / 1 / 2 / 1 | Timer::startMmPeriodic 0x00486dc0, Timer::startMmOneShot 0x00486e40, Timer::stop 0x00486ec0 | main (registration) | section 6 |
| golf_clean.exe | Sleep [0x004ba0b0] | 3 | Button::endCapture 0x00488c4d, scrollbarArrowRepeat 0x004973a7, netSendShutdown 0x00497d89 | main | wall time only |
| jgld.dll | QueryPerformanceCounter | 1 | Palette::update 0x1006a40e (seeds jgld's own Random with the counter) | main | palette animation (frames only; U-0652) |
| jgld.dll | GetCursorPos + ScreenToClient | 1 + 1 | Display::getCursor 0x10066780 (Display vtable 0x1011d640 slot 11), called by golf_clean.exe only through View_toLocal 0x0047ab50 | main | cursor position |
| jgld.dll | WaitMessage | 1 | App::idle 0x1006b080 | main | none in the measured runs |
| sound.dll | timeGetTime (15 sites), timeSetEvent / timeKillEvent / timeBeginPeriod | 15 / 1 / 3 / 1 | SoundTimer, DsBuffer, Voice, Seq, renderChannel | sound timer thread; main only in DsBuffer::play during initSound with an audio device (b2) | audio only |
| Terrain.dll | (GetTimeZoneInformation in the CRT's tzset) | 1 | __tzset_lk | - | none |
| binkw32.dll | timeGetTime, QPC | imports | movie playback | main while a movie plays | video pacing |

The static CRTs of the four modules read the time zone (`__tzset_lk`) but no game code calls time() / localtime: no
date or time of day enters the game.

## 3. The frame and its pacing

mainLoop 0x0040f5c0's frame (mainLoop_structure.md): head 0x0040fc29 `call [timeGetTime]`, the value stored at F+0x1ec
(0x0040fc2f) and read once more, by the H1 wait (0x0041e641: `mov esi, [F+0x1ec]`; c3fy_norm.asm has no other reader).

H1 wait, 0x0041e631..0x0041e6b1, run on every path that reaches it:
1. skipped when `[0x0059b04c] != 0` (0x0041e631; .bss, no instruction writes it, so the wait always runs);
2. `g_frameMs [0x005a8c88] = timeGetTime() - frameStart` (0x0041e639..0x0041e64a; no instruction reads g_frameMs);
3. `g_tileWallMask[0] &= ~8` (0x0041e64f..0x0041e65a);
4. `target = frameStart + ((g_flags & 0x20000) ? 0xfa : 0xaf) / 2` = +125 or +87 (0x0041e663..0x0041e67e, signed
   halving: `cdq; sub eax, edx; sar eax, 1`);
5. while `timeGetTime() < target` (unsigned `jae` / `jb`, 0x0041e680, 0x0041e6b1): `waitTicks(1, 0)` (0x0041e686;
   ebx = 1 and ebp = 0 from 0x0041c54e / 0x0041c54c).

waitTicks(n, interruptible) 0x0045bf80: `ms = n * 1000 / 100` (0x0045bf8c..0x0045bfad: `*5*5*5*8`, `imul 0x51eb851f`,
`sar 5`) = 10 n; `start = timeGetTime()`; loop while `timeGetTime() - start < ms`: pumpInput483c30 0x00483c30, and when
`interruptible`: pumpMessages 0x0045c030 and drainAndRedraw 0x0045af00, returning 1 early (after waitRedrawReady) on a key
or a mouse press. mainLoop's own calls: frame wait (1, 0); 0x0041e2bc (90, 0) = 900 ms and 0x0041ec9a (10, ebp) in H1
command cases; 0x00421029 (1, 0) in the title menu; 0x0040fa34 (60, 0) on quit.

Measured under the virtual clock (every main-thread timeGetTime call advances 1 ms): a normal frame is the frame head (1
call), 0x0041e641 and 0x0041e661 (2), then 8 iterations of {waitTicks(1, 0): 0x0045bfb3, 0x0045bfb7, 9 x 0x0045bfe3; the
loop test 0x0041e690} (8 x 12), = 99 calls = 99 virtual ms. In real time each waitTicks(1) returns once 10 ms have passed
since its own start, so the loop ends at the first test at or after frameStart + 87: the frame lasts 87 ms plus at most one
waitTicks quantum and one pass of pumpInput483c30, unless the frame's work already took longer, in which case the wait is a
single test.

Frames that do not wait: 5 of course_season's 6,296 frames had no clock call between two frame heads (1 virtual ms):
an H1 / H2 path returned to the frame head (0x0040fc29 or 0x0040fc24) without reaching the wait (which path: U-0651).
Such frames advance the date like any other.

Frames that wait for input: pumpAndRedraw 0x0045c0c0 called with 0 (all 16 call sites push 0: canAfford 0x00406d30,
narrateShot 0x0040946a, mainLoop 0x0041d8a6 / 0x0041e283, chooseShotTarget 0x0042406e, prepareShot 0x0042454a,
updateGolfers 0x0042bf8d, pathStep 0x0042ec2f, clickSlotPanel, clickBuildTool433d30, pickBuilding, clickAmenityPanel,
courseActionMenu, drawScoreSummary) loops (0x0045c104 `test edi, edi; je`) until a mouse press ([0x00822d68]), a key
(flushRedraw 0x0045ae70) or the network flag [0x0083afcc], pumping and redrawing, then copies the key / character into
[0x00822d94] / [0x00822d7c] and consumes them. It is an input-gated block inside the frame, including inside the simulation
tick: course_season has one, 440 virtual ms long (clock sites 0x0045c0cc, 0x0045c0e5; which caller: U-0650).

## 4. Simulation time: P5 and the date

P5 0x004175f2: `[0x0059b738] = 0`; if `g_flags & 4` (PAUSED) jump to 0x004185bb, skipping the rest of the tick; else
updateGolfers 0x004289e0, updateWalkers 0x00402a40, 0x00409bf0, and when `[0x00569498] == 0 && [0x0053df54] == 0`
showBulletinBoard 0x0046e810 (0x0041762a); then `g_date += 1` and, with `g_flags & 8`, `g_date += g_date & 1`
(0x0041762f..0x0041764c); then the ambient-event gate `(g_date & 3) <= g_difficulty [0x00822c88]` (0x00417651..0x0041765c)
and two Random::range(0x32) draws (0x00417662..0x0041767e).

So the simulation advances by FRAMES: one tick and one date unit per unpaused frame, whatever the wall time. Measured: the
date advanced by exactly 1 in every frame of the three traces (date_step_per_frame min = max = 1; no paused frame). At
99 virtual ms per frame a game month (1,024 frames) is 101.4 virtual s and a game year (8,192 frames) 811 virtual s; in
real time at least 89.1 s and 712.7 s (87 ms frames).

The pause key: key code 0x50 (case 0x47 of the byte table 0x0042191c, 0x0041d020) toggles bit 2. The H1 dispatch clears
[0x00569498] on any key other than 0x50 (0x0041c57b..0x0041c580) and, while [0x00569498] is set, drops a space key
(0x0041c55b..0x0041c579).

## 5. Which clock reads change state

| read | value goes to | state class | port |
|---|---|---|---|
| seedFromTime 0x00467110 (gameMain, once) | g_seedTime [0x0051b380] = t, g_rng [0x00822d9c] = t * 37 (0x00467116..0x00467121) | SIMULATION (every Random draw follows) | reproduce: the seed is an input of the run (trace: the clock value at the first timeGetTime call, 0x00100000 under the virtual clock; real game: milliseconds since Windows started) |
| mainLoop frame head 0x0040fc29 | F+0x1ec, read only by the H1 wait | pacing | drop (a headless tick has no wait); the frame COUNT is the time base |
| H1 0x0041e639 | g_frameMs [0x005a8c88], never read | none | drop |
| H1 wait, waitTicks, pumpAndRedraw | loop counts: how many pumps / redraws / input dispatches happen | pacing; input timing | drop the waits, keep the input-gated block (pumpAndRedraw(0)) as "wait for a command" |
| playAudioFile 0x0043ccfa | [0x005aa774] = t + 7,000 (0x0043cd00..0x0043cd05) when its 2nd argument is 0 | UI: rootWin_onPoint / onUnpoint / onKey clear g_ui83ab9c [0x0083ab9c] only when [0x005aa774] is 0 or the time is past it (0x0046561d..0x0046563a) | UI phase; not simulation |
| Snd::play 0x004849c0, Snd::busy 0x00484b4b | Snd +0x68 start time; busy = now - start < Snd +0x64 | audio | drop with audio |
| Button::beginPress / endCapture, scrollbar* | button auto-repeat, scroll repeat | UI | UI phase |
| showCreditsScreen, showHoleStatsScreen, movie*, BinkPlayer | screen animation / video pacing | UI / video | drop |
| net* | network timeouts | network (single-player never enters it) | drop |
| jgld Palette::update QPC 0x1006a40e | jgld's Random seed for palette animation | frames only (U-0652) | renderer |

Indirect effect: the wall clock decides IN WHICH FRAME an input is consumed (input arrives between frames), and frames are
the simulation's time base. A trace pins this: every input record carries its clock position and the FRAME records
delimit frames, so a port replays commands at frame indices.

## 6. Timers

Timer (object fields +0x08 flags, +0x0c handle, +0x10 / +0x14 armed, +0x20 period, +0x24 posted,
+0x28 resolution): Timer::startMmPeriodic 0x00486dc0 / startMmOneShot 0x00486e40 call `timeSetEvent(period, res,
0x00486f90, this, 1 / 0)` when the period is below 50 ms (`cmp eax, 0x32; jae`, 0x00486de1 / 0x00486e5b), else
`SetTimer(display slot +0x20 (the game window), this, period, 0x00486f40)`. The TIMERPROC 0x00486f40 and the mm callback
0x00486f90 (when the object has no direct callback at +4) post message 0x401 with wParam = the Timer object to the window,
once until +0x24 is reset (0x00486f5d..0x00486f77, 0x00486fba..0x00486fd4); a direct callback at +4 is called from the mm
callback itself (0x00486fa0), on the timer thread in the real game.

Measured: the three scenarios register exactly one timer, SetTimer with period 50 ms and TIMERPROC 0x00486f40
(0x00486e23), killed at the same clock position by Timer::stop (0x00486ef9), before the title menu. No WM_TIMER and no
0x401 message reaches the window procedure during play. Timers serve the UI (Button press repeat through
Timer::initStartMmOneShot 0x00486d90, colour cycling startColorCycle 0x004832e0, scrollbars). sound.dll's SoundTimer
(timeSetEvent 0x10008d21) runs on its own thread and is audio only.

## 7. Input: sources and the order a frame consumes them

Input reaches the game through:
1. **Window messages** in the queue of the main thread, dispatched by jgld's App::pump* (0x1006b0f0..0x1006ba20) through
   the window procedure 0x100692b0 to the root window's handlers, which write g_rootInput 0x00822d58..0x00822d97 and the
   key globals. Measured message -> handler (the handler entered after the window procedure received the message, counts
   equal in every trace): WM_LBUTTONDOWN -> rootWin_onPoint 0x00465610 (x, y: [0x00822d8c] / [0x00822d90]);
   WM_LBUTTONUP -> rootWin_onPress1 0x00465580 (the CLICK: positions [0x00822d58..0x00822d64], Ctrl / Shift from
   GetAsyncKeyState(0x11 / 0x10) into [0x00822d70] / [0x00822d6c], button [0x00822d68] = 1, [0x00822d74] = 1) then
   rootWin_onUnpoint 0x00465650 ([0x00822d8c] = -1); right button the same with onPoint2 / onPress2 ([0x00822d68] = 2) /
   onUnpoint2; WM_KEYDOWN -> rootWin_onChar 0x004657b0 ([0x00822b98] = [0x0083821c] = the key); WM_CHAR ->
   rootWin_onKey 0x00465750 (a, code: [0x00822b9c] = a unless code is 0x60..0x69, [0x00822b98] = code; the pair per key: U-0654); WM_KEYUP ->
   rootWin_onChar0 0x004657d0 ([0x0083821c] = 0). A click is therefore recognised on button RELEASE.
2. **Key state reads** (bypass the queue): GetKeyState (uiKeyEvent 0x00495910: Shift / Ctrl / Alt for every dispatched
   key), GetAsyncKeyState (the click handlers' Ctrl / Shift; mainLoop P6 0x00418d7d and H2 0x0041ed39 read VK_SHIFT;
   isPrimaryMouseDown 0x0047abc0 with GetSystemMetrics(SM_SWAPBUTTON)), GetKeyboardState (capsLockOff 0x004321d0, once per
   frame: when CapsLock is toggled on it calls keybd_event(0x14, 0x45, 1, 0) at 0x004321f0, an OS-level key event).
3. **Cursor**: View_toLocal 0x0047ab50 -> Display::getCursor (GetCursorPos + ScreenToClient), 3 reads per normal frame.
4. **Keyboard layout**: MapVirtualKeyA in Window::dispatchAccel 0x0047c740..0x0047c8f4 (once per dispatched key) and
   ListBox::onKey.

Order in a normal frame, measured (course_season frame 1168, a key frame; sandbox frame 200, an idle one):
FRAME head (0x0040fc29) -> P1..P4 (Palette::update QPC x 0..84) -> P6/P7 cursor read -> capsLockOff GetKeyboardState ->
H1: pumpMessages 0x0045c030 (removes only WM_MOUSEMOVE, then frameTick483cd0 0x00483cd0 runs jgld's pumps, which dispatch
the queued key and button messages: MSG, uiKeyEvent's three GetKeyState, MapVirtualKeyA, the root handlers) -> cursor read -> takeKey 0x0045ae50 ([0x00822b9c], cleared) and takeCode 0x0045ae30 ([0x00822b98], cleared): the key command switch
0x0041c59e..0x0041c5b2 keyed by takeKey's value -> cursor read -> the wait -> H2: the mouse action reads [0x00822d68] (button of the last click),
the positions and GetAsyncKeyState(VK_SHIFT) -> next frame head.

Where messages were dispatched (window-procedure entries by the last clock site before them): inside mainLoop frames,
952 of 954 in H1 right after the frame's Palette::update QPC calls (or right after the frame head when there were none) and
2 inside pumpAndRedraw(0) (the key that ended the input-gated block); none inside the frame wait. All 28 dispatched inside a
waitTicks loop (pumpInput483c30) were before the first frame: the title menu, the location screen and the load screen.

## 8. What a headless port must reproduce, and what it can drop

Reproduce: (a) the frame as the unit of simulation time: one P5 tick and one date unit per unpaused frame; (b) the pause
bit and its key; (c) the seed g_rng = t0 * 37 and g_seedTime = t0 at boot, t0 an input; (d) commands applied at the frame
they were consumed in (the trace's frame index), in the frame's consumption order (keys in H1 before the mouse action in
H2); (e) the input-gated blocks (pumpAndRedraw(0)) as points where the tick waits for a command; (f) the click = button
release rule and the one-frame key latency, if raw input is replayed instead of commands; (g) the original's quirks: the
date step rule under bit 3, the 87 / 125 ms constants (only for real-time play).
Drop: every wait loop and its clock reads, g_frameMs, Sleep, QPC (unless the renderer reproduces palette animation),
audio and video timing, network timeouts, timers (UI only), capsLockOff's keybd_event, MapVirtualKeyA (UI keyboard layout).

## 9. Traces: determinism and sufficiency (measured 2026-10-18)

| scenario | frames | clock calls (tgt / qpc) | deliveries | file bytes | record x2 | replay from the trace |
|---|---|---|---|---|---|---|
| sandbox_basic | 439 | 66,744 (48,278 / 18,466) | 268 | 580,656 | byte-identical; 10/10 checkpoints SAME as det_b | trace byte-identical, 0 site mismatches, 745/745 deliveries and polls applied, 10/10 SAME |
| course_season | 6,296 | 936,098 (627,816 / 308,282) | 1,515 | 7,867,550 | byte-identical; 12/12 SAME | byte-identical, 0 mismatches, 7,730/7,730, 12/12 SAME |
| mood5_fixture | 245 | 57,910 (33,734 / 24,176) | 313 | 371,014 | byte-identical; 4/4 SAME | byte-identical, 0 mismatches, 646/646, 4/4 SAME |

In the replay, scenario_det.js's virtual clock never answered the main thread (its main-thread timeGetTime counter is 0
at every checkpoint): the recorded clock values and deliveries alone drove the original to the pinned references' state
and frames. Wall time per run: sandbox_basic 11.5-12.3 s, course_season 72-90 s, mood5_fixture 9.7-14.2 s.

## 10. Name corrections reported (not renamed)

- 0x0045ae30 `setDirtyFlagA` and 0x0045ae50 `setDirtyFlagB` return and clear [0x00822b98] / [0x00822b9c] (take-and-clear
  readers of the code / key the root handlers store); `gDirtyFlagA` / `gDirtyFlagB` are those key globals.
- rootWin_onPoint / onUnpoint are the left button's down / up handlers and rootWin_onPress1 the click on release; onChar
  receives WM_KEYDOWN, onKey WM_CHAR and onChar0 WM_KEYUP (section 7, measured).
- capsLockOff writes no global: [0x004ba1e8] / [0x004ba1ec] are the GetKeyboardState / keybd_event import slots.
