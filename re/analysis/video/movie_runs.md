# The original's intro and closing movies, measured (S8.2, 2026-10-24)

Instance `instances\s8_2` (never `original\`; the listing hash of `original\` was identical before and after: `log/roadmap/S8/2_original_listing.md5`).
Machine state: this PC had no audio device answered to the game (scenario_det.js device-less answers, `C3FT_AUDIO=none`, the default since b2), so
binkw32 opens no wave device; the movies play silent and paced by the virtual clock. Raw results: `log/roadmap/S8/2_results.json`
(`~/bin/py -3.12 re/tools/s8_movie_runs.py analyze report`), run folders `log/c3/alt/refs/cov_r_s8_full|cov_r_s8_intro/<label>/` (git-ignored),
run logs `log/roadmap/S8/runs/`.

## 1. What was run

| scenario | what it does | ON (SIMGOLF_SKIP_INTRO=0) | skip (SIMGOLF_SKIP_INTRO=1) |
|---|---|---|---|
| `cov_r_s8_intro` | process start, checkpoint `intro_early` at 2,000 virtual ms, main-menu screen matched, `menu` 3 s later, `menu_late` 5 s later | `rec_on`-style runs `i1 i2 i3` (+ `recB`, `onI`, `plain_on`) | `j1 j2 j3` (+ `skipA`, `skipI`) |
| `cov_r_s8_full` | the same start, hover and click on the title menu's exit icon, a checkpoint every 2 s through the closing movie and the credits (`quit_00..09`, `credits`), Esc, process exit | `rec_on on2 on3` (+ traces `t1 t2 t3`, replays `rp1 rp2 rp3`) | `skip_rec skip2 skip3` |

Scenario source `re/tools/scenario_s8_2.py`, driver `re/tools/s8_movie_runs.py` (runs `scenario_refs.py` / `trace_tool.py` unchanged; adds the
environment, the scenario import, an optional binkw32 recorder and `analyze`). Commands:

```
export SIMGOLF_ALLOW_MULTI=1 SIMGOLF_GAME_DIR=instances/s8_2 C3FT_OBJHASH=image
~/bin/py -3.12 re/tools/s8_movie_runs.py --skip-intro 0 --bink-log ref run --instance s8_2 --scenario cov_r_s8_full --label rec_on --record --clock-log 400000
~/bin/py -3.12 re/tools/s8_movie_runs.py --skip-intro 0 --bink-log ref run --instance s8_2 --scenario cov_r_s8_full --label on2 --clock-log 400000
~/bin/py -3.12 re/tools/s8_movie_runs.py --skip-intro 1 --bink-log ref run --instance s8_2 --scenario cov_r_s8_full --label skip_rec --record --schedule log/roadmap/S8/runs/sched_full_skip.json --clock-log 400000
unset C3FT_OBJHASH
~/bin/py -3.12 re/tools/s8_movie_runs.py --skip-intro 0 trace record --instance s8_2 --scenario cov_r_s8_full --label t1 --out port/corpus/traces/s8_2/full_on_t1.trace
S8_REPLAY_EXIT=1 ~/bin/py -3.12 re/tools/s8_movie_runs.py --skip-intro 0 trace replay --instance s8_2 --scenario cov_r_s8_full --label rp3 --trace port/corpus/traces/s8_2/full_on_t1.trace --out port/corpus/traces/s8_2/full_on_rp3.trace
~/bin/py -3.12 re/tools/s8_movie_runs.py analyze identity --scenario cov_r_s8_full rec_on on2 on3
~/bin/py -3.12 re/tools/s8_movie_runs.py analyze statediff --scenario cov_r_s8_full --on rec_on on2 on3 --skip skip_rec skip2 skip3
~/bin/py -3.12 re/tools/s8_movie_runs.py analyze clockdiff --scenario cov_r_s8_full rec_on skip_rec
~/bin/py -3.12 re/tools/s8_movie_runs.py analyze movies --scenario cov_r_s8_full rec_on
```

## 2. Three runs, identical results

For each condition and scenario, three runs agree in: the main-thread clock-call sequence (kind, slot module, caller; 138,628 calls ON and 83,683
skip in `cov_r_s8_full`; 78,500 ON and 23,554 skip in `cov_r_s8_intro`), per checkpoint the main-thread `timeGetTime` and `QueryPerformanceCounter`
call counts, the `.bin` state hash, the frame hash and the object hashes of the compared classes (sim, hidden, ui, engine, render, const), and the
exit-file hashes and exit code 0 of `cov_r_s8_full` (`analyze identity`: 4 of 4 groups `all_identical`). The three recorded traces of the ON run
(`t1 t2 t3`) are byte-identical (299,543 bytes, sha256 prefix `d04ed10092efa790`, `trace_tool validate` OK).
Controls on the recorder: `plain_on` (no binkw32 recorder, no object-hash images) and `i1` have the same clock digest `78500:7fad38dc58b6` and
state hash `162ce36b499ecb35`; `skipA` and `j1` the same `23554:7a69040b560a`. The recorder and the images do not move the timeline.

## 3. The movies

Both movies end by clip end in every run: `BinkClose` sees `FrameNum == Frames` (the eight header dwords read at the handle, in the order of
binkw32's BINK struct: Width, Height, Frames, FrameNum, LastFrameNum, FrameRate, FrameRateDiv, ReadError; ReadError is the word at handle+0x1c that
BinkPlayer::play 0x00487090 tests at 0x004870de, and it is 0).

| | intro | closing |
|---|---|---|
| path passed to `BinkOpen` | `./flics/SMSG_introfinal.bik`, string at 0x004c8a0c | `./flics/SMSG_closingfinal.bik`, string at 0x004c6bb4 |
| call site | showTitleScreen 0x0043cd97 via playAudioFile 0x0043cce0 | mainLoop 0x0040faa4 via playAudioFile |
| header at open | 800 x 600, 825 frames, 15 / 1 | 800 x 600, 589 frames, 15 / 1 |
| header at close | FrameNum 825, LastFrameNum 825 | FrameNum 589, LastFrameNum 589 |
| `BinkWait` calls | 51,640 (50,815 return non-zero, 825 return 0) | 36,851 (36,262 / 589) |
| `BinkDoFrame`, `BinkBufferLock`, `BinkCopyToBuffer`, `BinkBufferUnlock`, `BinkGetRects`, `BinkBufferBlit` | 825 each | 589 each |
| `BinkNextFrame` | 824 | 588 |
| `BinkBufferOpen` / `BinkBufferSetOffset` / `BinkClose` / `BinkBufferClose` | 1 / 2 / 1 / 1 | 1 / 2 / 1 / 1 |
| `BinkSetSoundSystem`, `BinkOpenDirectSound` | 0 calls | 0 calls |
| virtual time by the QPC clock, `BinkOpen` return to `BinkClose` | 54,940 ms (825 / 15 = 55.0 s) | 39,207 ms (589 / 15 = 39.27 s) |
| virtual time by the `timeGetTime` clock, same window | 0 ms | 0 ms |
| main-thread QPC calls in that window, by export | `BinkWait` 51,639, `BinkDoFrame` 2,476 (3 per frame), `BinkNextFrame` 824, `BinkClose` 1; the others 0 | `BinkWait` 36,850, `BinkDoFrame` 1,768, `BinkNextFrame` 588, `BinkClose` 1 |

Ordered binkw32 calls (arguments as the recorder read them; `BinkBufferOpen`'s window handle differs per run, 16,386,864 in `rec_on`, 13,830,088 in
`on2`, so it is not a stable value):

```
BinkSetIOSize(0x9c4000)                             -> 0x9c4000        (play: call 0x004870bd, size 10,240,000)
BinkOpen(path, 0x1000000)                           -> handle          (play: call 0x004870cd, flags 0x1000000)
BinkBufferSetOffset(buf, 0, 0)                      -> 1               (binkw32 calls it inside BinkBufferOpen)
BinkBufferOpen(hwnd, 800, 600, 0)                   -> buffer          (the shim's hook forces the DIB-section type, see shim.cpp h_BinkBufferOpen)
BinkBufferSetOffset(buf, 0, 0)                      -> 1               (play: call 0x00487116)
per frame, uniform over all 825 / 589 frames:
  BinkWait(handle) x n -> 1 ... then -> 0           (n = 61 in 469 runs of both movies together, 62 in 941, 63 in 2: the two 63s are frame 1 of each movie)
  BinkDoFrame(handle) -> 0
  BinkBufferLock(buf) -> 1
  BinkCopyToBuffer(handle, dest, 3200, 600, 0, 0, flags) -> 0     (flags 0x80000003 on frame 1, 3 on every later frame)
  BinkBufferUnlock(buf) -> 1
  BinkGetRects(handle, 3) -> 1 on frame 1, 0 on every later frame
  BinkBufferBlit(buf, handle+0x34, count) -> 1 / rects pointer   (count = the BinkGetRects result; 0x34 = the offset the exe passes)
  BinkNextFrame(handle) -> 3360 (intro) / 2416 (closing)           (not called after the last frame)
BinkClose(handle) -> 1, BinkBufferClose(buf) -> 1
```

The per-frame sequence DoFrame, Lock, CopyToBuffer, Unlock, GetRects, Blit, NextFrame equals BinkPlayer::frame 0x00487180 (note
`00487180_BinkPlayer_frame.md`); the wait loop equals BinkPlayer::play 0x00487090 at 0x00487133. No `BinkSetSoundSystem` call is made because play
only makes it for a non-zero fourth argument and the recorded run passes none (the resourceQuery result, 0x0043cd45; U-1713).

## 4. Where the time goes: binkw32 reads QueryPerformanceCounter, not timeGetTime

With the intro ON the main thread makes exactly 54,945 more QueryPerformanceCounter calls than with it skipped, all from `binkw32.dll+b6e7`
(94,157 against 39,212 in `cov_r_s8_full`; in `cov_r_s8_intro` the first call of the process into `binkw32.dll+b6d3` is added as well, 1 against 0,
because the closing movie of the full scenario also makes it). BinkOpen itself makes 6 of them (1 at `b6d3`, 5 at `b6e7`), the window of section 3
holds 54,940. Every other clock site is equal in both conditions: `jgld.dll+6a414` QPC 13,160, `golf_clean.exe+5bfe3` 24,187, `+5bfb3` and `+5bfb7`
2,592 each, `+4bbd9` 1,936 (`cov_r_s8_full`). The closing movie costs 39,212 calls at `binkw32.dll+b6e7` in both conditions (the skip run adds the single `b6d3` call, which the first `BinkOpen` of a process makes). `timeGetTime` is
called 0 times from binkw32 and 0 times from the exe inside either movie, so the virtual `timeGetTime` clock does not advance during a movie:
checkpoint T values are equal in both conditions at every checkpoint (`menu` 1,053,828, `quit_00` 1,058,676, `credits` 1,079,886), the scenario
polls (which fire from the `timeGetTime` stub only) never run inside a movie, and `intro_early` (2,000 virtual ms after process start) already shows the
main menu with the intro ON (frame hash equal to `menu`).

## 5. State differences, intro ON against the skipped run

`analyze statediff` compares full `.data` images (the GameState, 0x381174 bytes) at every checkpoint; a dword counts as attributed to the intro only
when it is identical in all three runs of each condition and differs between the conditions.

| object | address | skip | ON | when | cause |
|---|---|---|---|---|---|
| `g_ui83ab9c` (engine) | 0x0083ab9c | 0 | 1 | `intro_early`, `menu`, `hover_exit` (and `menu_late` in `cov_r_s8_intro`): every checkpoint before the closing movie | BinkPlayer::play stores 1 at 0x00487124 and never clears it after a clip end; only rootWin_onPoint 0x0046563a, rootWin_onUnpoint 0x00465671 and rootWin_onKey 0x004657a0 store 0 (a click or key ends the loop through the read at 0x0048713d). The closing movie sets it to 1 in the skip run too, so from `quit_00` on both are 1 and nothing differs |

Everything else is equal at every checkpoint, among them `g_seedTime` 0x0051b380 (0x00100000 in both), `g_rng` 0x00822d9c (0x1f98dc65 in both), the
playAudioFile deadline word 0x005aa774 (0 in both: 0x0043cd64 clears it after the play) and all 203 sim objects. The cursor
(`ShowCursor(0)` / `ShowCursor(1)` around the loop, 0x0048712b / 0x00487161) leaves no GameState difference. The expected differences in the seed and the RNG do not
exist: the seed is `timeGetTime * 37` at seedFromTime 0x00467110, and no movie code reads `timeGetTime`.
Heap addresses are the only other thing that differs (about 47,150 dwords per pair of runs, between two runs of the SAME condition as well: 46,873
between two skip runs, 47,150 between two ON runs), so they are process noise and not intro effects. The same table, from `cov_r_s8_intro`: 1
attributed dword at each of its three checkpoints, the same one.

The pinned intro-skipped references agree: my skip run and my ON run both equal the pinned `cov_a_quit_title` reference (log/c3/alt/refs/cov_a_quit_title/det_b)
in state at all nine checkpoints they share (`menu`, `hover_exit`, `quit_00`..`quit_05`, `credits`), and `sandbox_basic`'s `menu` checkpoint is SAME for
both. (Frames differ at `quit_01` by 2,336 px and at `credits` by 34,502 px in both conditions alike: the scenarios' checkpoint timing is not the same.)

## 6. Tool status

- `scenario_ab.py` / `scenario_refs.py run`: work on intro-ON runs unchanged (the environment override `--env SIMGOLF_SKIP_INTRO=0`). The runs hit no failure.
- `trace_tool.py record`: works; three traces of `cov_r_s8_full` ON are byte-identical, validate OK, 138,749 clock records (31,431 `timeGetTime`, 107,318 QPC;
  top sites `binkw32.dll+b6e7` 94,157, `golf_clean.exe+0045bfe3` 24,278, `jgld.dll+6a414` 13,160). The trace has no FRAME records (no frame loop in
  this path).
- `trace_tool.py replay`: with the intro ON it replays with 0 site mismatches, 0 overrun, 575 of 575 deliveries and all 14 checkpoints equal to the
  record, but for an EXITING scenario it ends the trace at the last recorded poll, before the Esc dispatch and ExitProcess (the replay script never sets
  `exit_expected`), so the replayed trace is 296 bytes shorter and `DIFFERS` (`rp1`). `S8_REPLAY_EXIT=1` in `s8_movie_runs.py` wraps the replay script (expects
  the exit and keeps answering polls): `rp3` is BYTE-IDENTICAL to the source. `trace_tool.py` itself is unchanged (not owned by S8.2); the fix belongs in
  `trace_tool.install` (merge note).
- Input cannot be delivered during a movie: scenario input is delivered from the `timeGetTime` stub, which a movie never calls (U-1710).

## 7. Consequences for the port video module (S8.3)

The movie changes no simulation object and no seed; the only GameState word it touches is the byte 0x0083ab9c (set to 1 at the first frame loop, written 0 by
the three root-window input handlers). Movie time is invisible to every `timeGetTime` reader: a port that plays the intro must feed the trace's
QPC values (54,945 main-thread QPC calls from the binkw32 site) to nothing that reaches simulation state, and a replay of an intro-ON trace needs
the QPC records only to keep the clock-call ordinal aligned (trace_tool replay does that: 0 mismatches).

## Open (log/roadmap/S8/2_uncertain.tsv)

U-1710 no key abort measured (no poll or input inside a movie); U-1711 no frame inside a movie is captured (checkpoint images show the post-movie screen); U-1712 raw return
values of `BinkNextFrame` (3,360 / 2,416) and the `BinkGetRects` / `BinkBufferBlit` pair are recorded, not interpreted; U-1713 the fourth argument of BinkPlayer::play is 0 on both
call sites in these runs (no `BinkSetSoundSystem`), the sound-enabled path is not exercised; U-1714 `trace_tool replay` of an exiting scenario; U-1715 the clock-log and object-image
runs of `cov_r_s8_intro` were taken with the shared schedule file of the scenario directory (its timings equal for both conditions).
