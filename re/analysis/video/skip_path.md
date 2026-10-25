# The movie skip path of the port (S8.3, 2026-10-24)

Module `video` (`port/src/sim/video.cpp`, `port/include/portsim/sim/video.h`), tests `port/test/test_video.py`, pinned digests
`port/test/golden_video.json`, boot scenario tool `re/tools/s8_boot_capture.py`. Inputs: S8.1's inventory (`MOVIES.md`, `re/port/movies.tsv`),
S8.2's measurements of the original (`movie_runs.md`), the C3 / C4 notes of playAudioFile 0x0043cce0, BinkPlayer::setTarget 0x00487050,
BinkPlayer::close 0x00487060, BinkPlayer::play 0x00487090, BinkPlayer::frame 0x00487180 and the pumps 0x00483bd0 / 0x00483c90 / 0x00483cf0.
Decision D2: no Bink decoder this round. The movie goes through a `MovieHost`; with no host attached the default host answers every
open with "unavailable", and the module takes the path the original itself takes when `_BinkOpen` answers NULL.

## 1. The two movies

| movie | caller, call site | path (string, GameState symbol + offset) | `immediate` | deadline |
|---|---|---|---|---|
| intro | showTitleScreen 0x0043cd70, `call 0x0043cce0` at 0x0043cd97 (`push ebx` 0x0043cd91 with `xor ebx, ebx` 0x0043cd8e) | `./flics/SMSG_introfinal.bik`, 0x004c8a0c = `f_strpool_4c89ac` + 0x60 | 0 | timeGetTime + 0x1b58 (0x0043ccfa / 0x0043cd00 / 0x0043cd05) |
| closing | mainLoop 0x0040f5c0, `call 0x0043cce0` at 0x0040faa4 (`push 1` 0x0040fa9d) | `./flics/SMSG_closingfinal.bik`, 0x004c6bb4 = `f_strpool_4c6ba4` + 0x10 | 1 | 0 (0x0043cd0c) |

Both callers discard playAudioFile's result: showTitleScreen continues with `add esp, 8; lea ecx, [esp + 0x14]` (0x0043cd9c / 0x0043cd9f),
mainLoop with `call 0x0045c150` (pumpLight, 0x0040faa9) whose own eax replaces it.

## 2. playAudioFile and BinkPlayer as the module implements them (disassembly re-read 2026-10-24)

```
playAudioFile(path, immediate)                               0x0043cce0
  fileExists(path) == 0 -> return 0                          0x0043cce6, je 0x0043ccf0 (nothing else runs; eax = that 0)
  g_audio5aa774 = immediate ? 0 : timeGetTime() + 0x1b58     0x0043ccf8 / 0x0043ccfa / 0x0043cd00 / 0x0043cd05 / 0x0043cd0c
  window = display->slot[+0x20]()                             0x0043cd16..0x0043cd1e (ecx = [0x0083ad50], no argument)
  BinkPlayer::setTarget(&g_bink, window)                      0x0043cd27: g_bink+0xc = window (0x00487054)
  pumpInput483bd0()                                           0x0043cd2c: g_inputPhase 0x3f, showScreen loop, g_inputPhase 0
  Window::paint(root 0x00519a60, 0x80007fff)                  0x0043cd3b (drawing)
  sound = resourceQuery(&g_musicDevice)                       0x0043cd45
  BinkPlayer::play(&g_bink, path, 0, 0, sound)                0x0043cd55
  r = BinkPlayer::close(&g_bink)                              0x0043cd5f
  g_audio5aa774 = 0                                           0x0043cd64
  return r
BinkPlayer::play(this, path, x, y, sound)                    0x00487090
  this+0xc == 0 -> return 7                                   0x00487093..0x0048709a
  sound != 0 -> _BinkSetSoundSystem(&slot _BinkOpenDirectSound, sound)   0x004870a3..0x004870b2
  _BinkSetIOSize(0x9c4000); h = _BinkOpen(path, 0x1000000); this+4 = h   0x004870bd / 0x004870cd / 0x004870d5
  h == 0 or h->error (+0x1c) != 0 -> return 1                 0x004870d8 / 0x004870de..0x004870e3 / 0x0048716b
  b = _BinkBufferOpen(this+0xc, h->width, h->height, 0); this+8 = b; b == 0 -> return 1   0x004870f6 / 0x004870fe / 0x00487101
  this+0x10 = x; this+0x14 = y; _BinkBufferSetOffset(b, x, y)                                0x00487110 / 0x00487113 / 0x00487116
  g_ui83ab9c = 1; ShowCursor(0)                               0x00487124 / 0x0048712b
  loop: _BinkWait(h) != 0 -> pumpInput483c90(); pumpInput483cf0(); again   0x00487137 / 0x0048713b / 0x00487153 / 0x00487158
        g_ui83ab9c == 0 -> leave; BinkPlayer::frame() false -> leave        0x0048713d..0x00487144 / 0x00487148..0x0048714f
  ShowCursor(1); return 0                                     0x00487161 / 0x00487165
BinkPlayer::close(this)                                       0x00487060
  this+4 != 0 -> _BinkClose; this+8 != 0 -> r = _BinkBufferClose; this+4 = this+8 = 0; return r (0 without a buffer)
```

## 3. The skip path (the default host: BinkOpen answers NULL)

This is the path of every pinned recording before S8.2: the shim's `skip_intro` answers NULL for a name containing `introfinal`
(shim.cpp `h_BinkOpen`), and the module reproduces it for both movies. Its effect on the GameState, per movie:

| object (class) | intro (file present) | closing (file present) | evidence |
|---|---|---|---|
| `g_audio5aa774` (audio) | clock + 7000 during the call, 0 after it | 0 during and after | 0x0043cd05 / 0x0043cd0c / 0x0043cd64 |
| `g_bink` +0xc target (pointer) | the host's window (1 in the port) | the same | setTarget 0x00487054 |
| `g_bink` +4 handle, +8 buffer (pointer) | 0 after the call | 0 | play 0x004870d5 stores NULL, close 0x00487082 / 0x00487085 |
| `g_bink` +0x10 / +0x14 offsets | untouched | untouched | written only after a buffer opened (0x00487110 / 0x00487113) |
| `g_inputPhase` (ui) | 0x3f during pumpInput483bd0, 0 after | the same | 0x00483bd0 / 0x00483bf6 |
| `g_ui83ab9c` (engine) | untouched | untouched | its only setter 0x00487124 lies after the NULL exit |
| eax | 0 (close with no buffer) | 0 | close 0x0048708a, discarded by both callers |
| any sim object | none | none | the boot gate below; S8.2 section 5 |

With the file absent (fileExists 0) nothing is written and eax is 0. A handle with a non-zero error word ends the same way, except that
the original also calls `_BinkClose` on it from close (0x0048706e); the final state is the same (both words 0).

The clock: the intro's deadline consumes the one timeGetTime read playAudioFile makes. In a trace that read is a tgt record at return
address 0x0043cd00, which the port keys as site 0x2d0d1 (offset from mainLoop's frame-head return address 0x0040fc2f, the key of
`InputSource::clock`); S8.2's intro-ON trace holds it with value 1048577 (record 2 of `full_on_t1.trace`), the same record the skip-intro
traces hold. Without an InputSource clock the intro throws NeedInput{playAudioFile}, like seedFromTime.

## 4. Why the port's skip path reproduces an intro-ON recording (questions 1 and 2 of the step)

**QPC records the port never asks for.** With the intro ON the original's main thread makes 54,945 more QueryPerformanceCounter calls,
all from binkw32 (`0x8400b6e7` / `0x8400b6d3` in trace site notation), and no timeGetTime call inside either movie (S8.2 section 4). The
port's trace reader (`parse_trace` in port/src/sim/tick.cpp) puts only kind-1 (timeGetTime) CLOCKRUN values into a frame's clock list;
a kind-2 (QPC) run only moves the clock ordinal and the dispatch context `ctx_ra`. Every clock read of the port is keyed by its call site
(`InputSource::clock`, `TkBase::clock_site`), so binkw32's QPC records are neither consumed nor mismatched: they are skipped. Ordinals
(`pos`) of the later records shift by 54,946 in the intro-ON trace, and every use of them (dispatch, cursor, write, snapshot positions)
compares ordinals of the same trace. The QPC runs sit between the deadline record and the jgld palette QPC run (records 2..5 of
`full_on_t1.trace` and of `cov_r_s8_play.trace` alike); no HANDLER, CURSOR or MSG record lies inside them (no input reaches a movie in
a recording, S8.2 U-1710), so no dispatch is attributed to a pump of the movie loop. Measured: with the HEAD build (playAudioFile still a
drop, the movie not run at all) and with the video module merged, the boot gate passes the intro-ON scenario cov_r_s8_play 216 / 216
records from the first frame head, and the run to the end delivers 31 dispatches, 0 left, 929 clock queries served, 0 missing.

**g_ui83ab9c changes no simulation read.** Its five references in the exe (log/roadmap/S8/2_ab9c_refs.tsv): writes 0 by rootWin_onPoint
0x0046563a, rootWin_onUnpoint 0x00465671, rootWin_onKey 0x004657a0, write 1 by BinkPlayer::play 0x00487124, and one read, BinkPlayer::play
0x0048713d (the frame loop's run test). No other function reads it, and it is an `engine`-class object, which the segment and boot gates do
not compare. The playAudioFile deadline `g_audio5aa774` is written only by playAudioFile (0x0043cd05, 0x0043cd0c, 0x0043cd64) and read only by the three
handlers' deadline tails (a scan of .text for the address as an operand, 2026-10-24): rootWin_onPoint 0x0046561d / 0x00465632, rootWin_onUnpoint
0x00465650 / 0x00465669, rootWin_onKey 0x00465780 / 0x00465798; each clears g_ui83ab9c when the word is 0 (`je`) or timeGetTime is above
it (`cmp eax, [0x5aa774]; jbe` skips the clear, so a clock equal to the deadline does not end the movie). Outside playAudioFile the word is 0, so every click or key clears g_ui83ab9c; a non-zero
deadline is seen only by a dispatch from a pump inside playAudioFile (none in any recording). So the skip path does not
need to set g_ui83ab9c for the boot gate or for any sim-class object, and it must not (the original's NULL path does not). Against an
intro-ON recording of the original the port therefore differs in one engine byte, g_ui83ab9c = 0 where the original holds 1, from the
intro until the first click or key (S8.2 section 5 measured that 1); the pinned skip-intro corpus holds 0 there, as the port does.

## 5. What a decoder host must do (and what the module already does for it)

The module drives a host exactly as BinkPlayer::play drives binkw32 (section 2): `window()` for setTarget, `open(path, info)` (true only
for a movie whose handle has no error word), `open_buffer`, `show_cursor(false/true)`, `wait()` (true = not due yet), `frame()` (false =
clip end, i.e. the frame counter reached the frame count, no NextFrame after the last frame), `close_movie()`, `close_buffer()` (its result
is playAudioFile's result: 1 in S8.2's runs). The module itself makes every GameState effect: the deadline word, the target, the handle /
buffer words (tokens 1 / 1 for an open movie and buffer; pointer class), the offsets, g_ui83ab9c = 1 before the frame loop (left at 1
after a clip end until the next click or key, as S8.2 measured), the two input pumps per not-due pass (g_inputPhase 2 / 8, a Drain dispatch
each; the root handlers end the movie by clearing g_ui83ab9c when the deadline is 0 or their clock is past it), and close.

A host must reproduce, to stay equal to the original: (1) movie time runs on its own clock, never on the simulation's: the original's
movie makes no timeGetTime call, so the virtual T does not advance during a movie and no scenario poll runs inside one; a host that
replays a recording must consume no tgt record (the deadline read is the module's); (2) the frame count and the per-frame not-due
answers decide how many pump passes run, which matters only when input arrives during the movie (the intro's 7 s budget: a key before
clock + 7000 does not end it, a key after does; the closing movie ends on any key); (3) a host whose open fails after allocating must free
in open() (the module calls close_movie only for an open movie); (4) sound: playAudioFile passes resourceQuery's result to play, which
then calls `_BinkSetSoundSystem`; the module passes 0, the value of every recorded run (device-less), U-1720.

## 6. Evidence

| check | result | how |
|---|---|---|
| emulator oracle: the original playAudioFile / play / close / setTarget under Unicorn against the port | 23 / 23 cases equal (image, eax, frame counter), x64 and Win32 | `SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_video.py -k oracle` |
| branch coverage of the original over the cases | 12 / 12 conditional jumps both sides (playAudioFile 2, play 8, close 2) | `test_oracle_branch_coverage` |
| pinned digests | 23 / 23, x64 = Win32 | `test_pinned_digest` |
| boot gate, intro-ON scenario cov_r_s8_play | PASS 216 / 216 from the first frame head; to the end: mainLoop returned, the host saw both opens in order, top10.sve, Themes/Default.pro and &QuitSave.sve equal the original's | `test_boot_intro_on_scenario`; `~/bin/py -3.12 -m port.test.boot cov_r_s8_play` |
| S8.2's intro-ON trace (title menu only, no frame head) | runs to its end, 0 dispatches left, both opens, top10.sve equal to the S8.2 run's | `test_boot_s8_2_full_trace` |
| capture determinism | two frame-hash captures identical (216 records), trace FRAME (index, T) equal to the capture's | `re/tools/s8_boot_capture.py check c1 c2 --trace t1` |

Negative controls (`playAudioFile` modes, all RED): 1 the deadline word not cleared, 2 the skip path sets g_ui83ab9c, 3 the skip path not
taken, 4 the closing movie dropped (also RED on the boot run: the host sees only the intro), 5 the intro's deadline one millisecond early
(only the key-at-the-deadline case can see it). The boot gate is blind to all five by construction (no sim object), which is why they are
judged by the oracle-pinned digests and the boot run's host log.
