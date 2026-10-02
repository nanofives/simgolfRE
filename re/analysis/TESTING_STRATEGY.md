# Testing strategy: verifying functions, seeing neighbours, and learning from failures

Written 2026-10-02 after the first full pass of the tooling. Every claim below cites a tool in this repo
or an observed run.

## 1. The layers and what each one proves

| Layer | Tool | Proves | Blind to |
|---|---|---|---|
| Static anchors | `tests/test_static.py` | the bytes every address refers to | behaviour |
| Coverage census | `re/tools/coverage_census.py` (INT3, `shim/src/coverage.cpp`) | which functions a scenario runs, per phase | correctness |
| Path-1 A/B | `re/frida/diff_hook.py` | body equality on hand-picked inputs, incl. extremes | inputs the author did not think of |
| Record/replay | `re/frida/record.py` + `re/frida/replay.py` | body equality on the game's real inputs and memory, live == emulated == reimpl | inputs the scenario never produces |
| Invariants | `re/tools/invariants.py` | contracts the original honoured on every recorded call | rare paths |
| Version diff | `re/tools/version_diff.py` | which functions a known behaviour change touched | unchanged code |
| Scenario ON/OFF | (D-0002) | the game with the hook live | – |

Path-1 and replay are complementary, not alternatives: on `Terrain::tileAt` the game only ever produced
x, y in -1..50 (2000 recorded calls), while path-1 covers INT_MIN/INT_MAX.

## 2. One test, visibility on neighbours

1. **Invariants as a contract between neighbours.** A recording of a callee states what its callers send
   and receive. `Terrain::tileAt`'s inferred invariants (`re/replay/TerrainDll_tileAt/invariants.json`):
   NULL iff x or y outside 0..49, `ret - this ≡ 0x3a4 (mod 0x248)`. They were inferred from data only,
   and they reproduce the disassembly constants (`imul eax,eax,0x248` at `0x10001da2`,
   `+0x3a4` at `0x10001dab`). When any of the 8 named callers (`Terrain::render`, `localRender`,
   `stripRender`, `tileHit`, ...) is reimplemented, replaying the callee's checks over the calls it makes
   flags a broken caller without a full-scenario diff (`invariants.py --check`).
2. **Neighbourhood recording** (next step): one scenario run can record the target *and* its callers and
   callees at once. Every neighbour gets a replay corpus for free, and fidelity alone (live == emulated
   original) tells whether the capture is complete before anyone writes a reimplementation.
3. **Coverage as test-impact map.** The census says which functions a scenario touches; joining it with
   `hooks.csv` says which reimplementations a scenario exercises. A scenario that fails while touching
   one changed reimplementation points at it.
4. **Version diff as a behavioural oracle.** The 1.01-1.03 patch notes say what changed; the diff says
   where (`re/versions/PATCH_MAP.md`). A behaviour test written for a patch-note item ("50-year
   retirement removed") has a known set of functions behind it.

## 3. Getting value out of failures

Rule: **a failure is evidence about the system under test OR about the harness, and we must find out which
before acting on it.** Four failures from 2026-10-02, each one useful:

| Failure | What it really was | How it was found | What it taught |
|---|---|---|---|
| Coverage census "hangs" (Frida, then INT3) | Another app window covered the game; screen-pixel capture saw that window | Control run (coverage on, **zero** breakpoints) still "hung" 1/3; the saved screenshot showed the other window | Capture the window surface (BitBlt from the window DC), never screen pixels. Always run the zero-change control. |
| ddmin found culprit `0x00401000` | Noise: the oracle was flaky, so ddmin minimized noise | Re-running the "culprit" alone: hang, menu, hang; "all except it": hang | Delta debugging needs a deterministic oracle. Validate the culprit by repetition before believing it. |
| Replay RED, 2000/2000 fidelity failures, reads at `0x29c0504` | The image dump was taken after Frida patched the target: "original" jumped into Frida's trampoline | The replay reports the unmapped read address; it was in Frida's allocation | Make failures name their cause (`misses` lists addresses). Dump images before instrumenting. |
| "Sandbox" click opened "Start New Game"'s difficulty screen | The real mouse's hover / the default-focus button won the race against the virtual cursor | Golden screenshot showed the focus highlight; the flow changed when the user's mouse moved | Virtual cursor from the first frame (`SIMGOLF_VCURSOR`), longer settle, every step asserts the next screen. |
| First fix, `PrintWindow`, gave black frames (5/60, then 18/100 with retries) | `PrintWindow` asks a legacy GDI window to repaint; the game does not always answer | Black-frame rate measured per method; BitBlt from the window DC: 0/100 at the menu, 0/50 in game, correct under an opaque topmost window | Measure a fix's own failure rate before adopting it (`tests/test_capture.py`). |
| In-game clicks did nothing (main menu worked) | Two input paths: buttons from GetKeyboardState (posted messages never set VK_LBUTTON), and the click position from lParam, which Windows rescaled by 1/1.25 from the DPI-aware harness | Message trace showed WM_LBUTTONDOWN arriving at (92,396) for a click sent at (115,495) | Instrument the input path before guessing UI coordinates; check DPI conversions on every boundary. |
| Interceptor test `16 > 20` | Asserting a *rate* on a loop that slows down when unfocused | 1 failure in 5 runs, value printed | Assert that something happens, not how fast. |
| C3 refused for `0x004490d0` | Correct: dead code | Gate printed the one failing check | A gate that refuses is working; read the failing line. |

Techniques that turn a failing scenario into a location:
- **Spectrum-based fault localization** (`re/tools/sbfl.py`, Ochiai): coverage of passing vs failing runs
  ranks the functions that run mostly when it fails. Cheap now that coverage is stable (680/680/678
  functions over three runs of the same scenario).
- **Hook bisection**: with several reimplementations ON and a scenario RED, ddmin over the set of
  `SIMGOLF_HOOKS_OFF` values finds the minimal set of hooks that breaks it (deterministic oracle required).
- **First divergence**: for ON/OFF scenario runs, record the call sequence of the touched functions and
  align them; the first differing call (function, args, return) is the place to look.
- **Mutation testing of the tests**: every negative control in the suite (`override_re`, `wrong_stub`) is a
  hand-made mutant. Generalizing it (flip a comparison, off-by-one a constant in a reimplementation)
  measures whether path-1 vectors and replay corpora would catch real mistakes.

## 4. Matching decompilation (VC6)

Resolved 2026-10-02 with the user's Visual Studio 6 disc: cl 12.00.8168 is the compiler build recorded
in all three Rich headers (C and C++ objects). `re/tools/match.py` compiles annotated sources and
compares normalized instructions (absolute addresses, call targets masked; internal branches relative).
Results: the 4 Terrain accessors match 100% in BOTH builds from ONE source (release `/O2` in the exe,
debug `/Od /ZI /GZ` in Terrain.dll); `Terrain::tileHit` (170 instructions, x87 float, CRT calls) 100%.
Negative controls: equivalent-but-different source 52%, wrong flags 7-72%.

What matching adds that replay cannot: it determines source **shape** (`-a + 24` vs `24 - a`, `||`
early-return, delegation to inline `Tile` methods visible only in the debug build), and it is checked
for every input at once. What it cannot do: it does not distinguish spellings that compile identically
(`2.0f` vs `2.0`), so notes must say where the source is not determined. A practical method that worked:
enumerate plausible spellings for each differing statement and let the matcher pick (8 variants for
`tileHit`, seconds).
