# Phases 2 and 3 plan: the oracle and the headless simulation core (written 2026-10-18, not started)

Numbering is that of re/PORT_PLAN.md: phase 2 = ORACLE (a test harness that judges port code against recorded original
behaviour, without running the original at port-test time), phase 3 = HEADLESS SIMULATION CORE (everything that changes game
state; no drawing, no sound, no UI). Formats and loaders are phase 4 (only two small loaders are pulled forward, see 3.5).

## 1. What exists to build on

- Model: re/model/simgolf_model.h (structs), addresses.tsv (1,119 objects with va, size, type, stride, stream, evidence),
  sve_fields.tsv, model_ctypes.py (symbol -> bytes mirror), descriptors for exe constants (decision: constants stay in the
  user's own exe and are read from it at startup).
- Behaviour recordings: 58 pinned scenarios (re/analysis/data/scenario_refs.json: a deterministic run = virtual clock, input
  schedule, checkpoint dumps of every .sve global + RNG words + frames), 116 saves with matching same-instant dumps (G2), 2,900+
  hook sources whose vectors are in re/frida/registry.d / fixtures.d, record/replay (Unicorn) recordings, mainLoop spec
  (re/analysis/sim/mainLoop_structure.md: the frame is a straight chain; P5 is the simulation tick, preamble and H1/H2 hold
  membership, news, date and the command handlers).
- Function inventory by subsystem (hooks.csv, exe): sim 31, golfer 37, economy 13, course 22, terrain 51, ai 1, save 14, util 314,
  render 219, ui 649, boot 134, input 41, audio 103, net 17, video 27, frontend 13, crt 201. The sim core needs the first six
  (about 155) plus the util / leaf helpers they call, plus the mainLoop phases that mutate state.
- Known defects to carry as compatibility switches (not fixed): model_defects.md (35 entries), mainLoop findings, the g_year
  refresh on save.

## 2. Decisions needed from the user (before launch)

1. **Language and build of the port.** Recommendation: C++17 library `portsim` (the model header is C99 and compiles as C++),
   CMake, no third-party dependencies in the core, a plain C ABI export layer (`extern "C"`) so the Python harness can call it
   through ctypes. Alternatives: plain C99 (closest to the model, more boilerplate), Rust (safer, rewrites the header).
2. **Where the port lives.** Recommendation: `port/` in this repo for now (public repo, code written from specifications, no
   original code or data), split into its own repo later if wanted.
3. **State representation for phases 2-3.** Recommendation: a generated `GameState` struct whose member order reproduces the
   original data section layout of the model (padding members for untyped bytes, size asserts), so a checkpoint comparison is
   a per-object memcmp against the original's dump; the port code takes `GameState&` and uses named members only, never
   addresses. An idiomatic cleanup of the state is a later, separately tested step.
4. **Accuracy target.** Recommendation: bit-exact with the original, including its defects, behind named compatibility switches
   (default on). Fixing a defect is a phase 7 decision per entry in model_defects.md.
5. **Corpora storage.** Recommendation: local only (gitignored `port/corpus/`): memory dumps and vectors contain game data;
   commit generators, manifests (sha256 per corpus file) and the scenario trace schemas only.

## 3. Phase 2: the oracle (7 steps, about 6 children)

**2.0 Scope manifest (1 child).** `re/port/scope.tsv`: every exe function assigned to a port module (sim-core, formats,
backend-gfx, backend-audio, ui, boot-only, crt-dropped, net) with the mainLoop phase(s) that reach it and a rule-based reason.
Output also: the sim-core function list (target about 250 including leaves) ordered leaves-first by call depth, with the
globals each touches (from addresses.tsv) and the existing C3/C4 evidence (hook source, vectors, branch notes).

**2.1 Premise check on the original (1 child).** The oracle idea "start from a state image at checkpoint k, advance with the
recorded clock and inputs, compare with checkpoint k+1" is only valid if the original itself reproduces k+1 when restarted from
k. Saves omit state (the g_year refresh shows it). Measure on the original: run continuous vs restart-from-image (full data
section dump written back into a fresh process at the same frame boundary, the sprite queue and other UI-only state excluded):
which objects differ at k+1 and why. Output: the HIDDEN STATE list (state the sim needs that no save holds), the rule for a
valid segment start, and whether a full image restore works (pointer-containing objects are excluded by model flags).

**2.2 GameState generator and checkpoint tracer (1 child).** `re/tools/model_state_gen.py` -> `port/include/gamestate.h`
(generated, size and offset asserts, unit tests against addresses.tsv) and `re/tools/state_image.py` (parse a checkpoint
dump or a full data-section dump into the struct via the mirror). Extend scenario_ab/scenario_det (opt-in flag, default
byte-identical) to write, at every checkpoint, a hash per model object (1,119) plus an optional full data-section image, and a
per-frame hash of the sim-scope objects, so the first divergent frame and object are located without re-running.

**2.3 Time and input model (1 child).** Audit every clock read (timeGetTime, QPC, GetTickCount) and input read reachable from the
frame (frame head 0x0040fc29, H1 wait, P5 date advance, waitTicks) and state exactly how they feed the sim: frame
pacing, game speed, pause, the date advance rule, the RNG seed. Output re/analysis/sim/time_model.md and the trace schema
(`frame` records: virtual clock reads in call order, input state consumed, command-level events H1/H2 raised) and the recorder
(scenario_det extension) that writes `port/corpus/traces/<scenario>.trace`.

**2.4 Segment corpus (1 child, after 2.1-2.3).** From the 58 pinned scenarios build the SEGMENT TABLE: each segment =
(start image, clock/input trace, end checkpoint hashes), classified TIME-FORWARD (no input: stories, mood, season idle
stretches, autosave), COMMAND (inputs expressible as commands: build, hire, tool use, shots) or UI-ONLY (excluded). Replay
determinism is proven on the original (two replays equal) before a segment is accepted. Output `port/corpus/segments.json`
(manifest committed, data local), counts per class.

**2.5 Function-vector corpus (1 child).** Re-run the recorded vectors of the sim-scope functions in CAPTURE mode (diff_hook
machinery) writing raw arguments, input regions, return value and output regions (symbol-level, through the address map)
into `port/corpus/vectors/<function>.vec`; index with counts, distinct-result spread and branch notes from the C3 purpose files.
Targets: every sim-scope leaf with the C3 minimums (>= 10 vectors), stateful functions with their regions, and a flag for
functions whose vectors depend on the heap or imports (the segment corpus covers those).

**2.6 Harness (1 child).** `port/test/` pytest package: loads `portsim` through ctypes, builds GameState from corpus regions,
calls a function by registry name (C ABI `sg_call(name, args, state)`), compares return and regions, reports spread and
coverage like diff_hook; segment runner (`sg_run_frames`) with first-divergence localisation (frame, object); NEGATIVE
CONTROLS (a deliberately wrong body per tier must read RED). First client: Random::range (a pure function with state) to
prove both directions end to end.

## 4. Phase 3: the headless simulation core (about 9 children, 3 stages)

Stage A (after 2.2/2.6): **3.0 core** (skeleton, build, RNG with the double scale and __ftol rounding, math helpers, clamp,
distance, tile and cell accessors, util leaves). Stage B (parallel, each owns one module file and its corpus tests):
**3.1 world and course editing** (tile grids, cellAt, heightfield and coverage rebuilds, stepCourseEditQueue / applyTerrainEdit,
generateRandomCourse, course records), **3.2 economy and membership** (finance rows, ratings and trends, membership and the
type table, the year/date rules, awards), **3.3 golfers** (updateGolfers' states, pathStep, shot targeting glue, resetGolfer,
arrivals and bans), **3.4 shots** (simulateShot, simulateBallFlight, trialShot, chooseShotTarget; the x87 sites with the
control word and rounding discipline of c3ap/c3fh, SSE2 double only if bit-identical on the corpus), **3.5 events, news and
stories** (patronEvent, the message family, advanceStory and the story state machine; the two minimal loaders it needs: the
story file and the golfer roster, read from the user's install), **3.6 walkers, animals and ambient** (updateWalkers,
tickAnimals, placeAnimal, habitats, random ambient events of P5). Stage C: **3.7 tick and commands** (the frame sim step
from the mainLoop spec: preamble, P5, and the H1/H2 command effects as a COMMAND API with the effect of every command on
state; mouse-to-command translation stays in the later UI phase) and **3.8 integrator** (frame loop, segment parity runs, the
6-year season, performance check).

## 5. Gates (measurable)

Phase 2 done when: scope.tsv covers every function; GameState compiles with size asserts and loads every pinned checkpoint with
object hashes equal to the original's; the premise check is written up with the hidden-state list; segment table and vector
corpus exist with counts; the harness runs Random::range green and its negative control red; time_model.md is reviewed.
Phase 3 done when: (a) every sim-scope function is green on its vectors (the C3 distinct-result and branch discipline) with
negative controls; (b) every TIME-FORWARD segment matches at every checkpoint (state objects and RNG words); (c) the
6-year course_season run and the stories / mood scenarios match end to end; (d) a championship round matches with shots issued
through the command API; (e) 0 unexplained divergences, each known defect reproduced behind its switch; (f) the core runs a
game year in less than the original's frame pacing would take (headless speed check).

## 6. Risks

- The restart-from-image premise (2.1) may fail for state in non-saved objects: the plan handles it by the hidden-state list and
  full-image starts; if the original is not restartable at frame boundaries, segments start at the title menu instead.
- Heap-pointer objects (UI, engine) are not portable images: excluded by model flags; the sim scope must not depend on them
  (2.0 finds the dependencies; any sim function reading a heap object is reported).
- x87: the original's control word and 80-bit intermediates; bit-exactness is decided per site on the corpus.
- Corpus size and copyright: local only, regenerable; manifests committed.
- Commands vs raw input: the H1/H2 handlers mix input parsing and state effects; the command API is derived from their
  effects, so the first complete spec of each command is stage C's main work and may need extra scenarios.
- Audio-device timeline: scenario determinism uses the stand-in default (C3FT_AUDIO=none).
