# Phase 3 stage C, 3.7: tick and commands (2026-10-19)

Read first: `re/model/STAGE_B_BRIEF.md` (rules, files, uncertainty and report conventions, still valid), `port/CORE.md`,
`port/test/README.md`, `re/PHASE23_PLAN.md` sections 7-8, `re/analysis/sim/time_model.md`, `re/analysis/sim/mainLoop_structure.md`,
`log/c3/c3fy_deps.tsv`, the CLAUDE.md paragraphs of stage B (bridge, events, golfers, shots, terrain, course, economy) and
`re/port/scope_straddlers.tsv` (126 straddlers, 62 in the ui module).

Stage B is committed (03213dc2). Registered modules: util core tiles text course economy golfers bridge shots events terrain*.
Call peers by hooks.csv name through the registry; exported C++ helpers are in port/include/portsim/sim/*.h.

## Split (two children, disjoint files)
- **3.7a TICK**: module `tick` (port/src/sim/tick*.cpp, test_tick.py, uncertainties U-0920.., notes log/p3/p37a_notes.txt).
  (1) The leaf straddlers stage B left local: startMessage, appendCents, logTick, playSound policy (sim-visible part only), and
  any other straddler that is NOT a UI click/handler (rows of scope_straddlers.tsv in modules other than ui, plus the C2/C3
  sim-core rows nobody ported: canAfford, membershipReport, addScore, matchUpdate, updateGolfers, hireEmployee sim part). Once
  they are registered, stage B's local copies must be replaced by calls (edit only the call sites' `local copy` guards, smallest
  change, with the owner's tests re-run: events, course, shots, golfers).
  (2) The frame step: one function `tickFrame(ctx, input)` (C ABI: wire `sg_run_frames` in port/src/portsim_c.cpp, replacing
  SG_E_NOT_IMPLEMENTED) following mainLoop's phases PRE, P1..P7, H1, H2 from mainLoop_structure.md and time_model.md: one tick per
  unpaused frame, `g_date += 1`, pause bit, frame-counted time, the RNG seeded once from t0, all waits dropped,
  pumpAndRedraw(0) modelled as "wait for a command". The oracle is the segment corpus (190 segments): the TIME-FORWARD ones
  (35, no input) are 3.7a's gate: after N frames every one of the 203 sim-object hashes of each frames.bin record must match
  (SegmentRunner). Report the first divergent record and object for every segment that fails, and how many pass. Do not guess
  to make a hash match: locate the first divergent frame with first_divergent_record / first_byte_divergence, cite the original
  code, then fix.
- **3.7b COMMANDS**: module `commands` (port/src/sim/commands*.cpp, test_commands.py, uncertainties U-0940.., notes
  log/p3/p37b_notes.txt). The 62 UI-module straddlers (click/handler functions with command effects: clickGolferPanel,
  handleEmployeeAction, showBulletinBoard, build/demolish tool clicks, hire, price, preferences ...). For each: split into the
  COMMAND EFFECT (port: a `Command` struct/enum in port/include/portsim/sim/commands.h and an `apply_command(ctx, cmd)` that
  performs exactly the original's sim-object writes and RNG draws) and its drawing/dialog (dropped, named in the notes). Evidence
  per function: vectors where they exist, else the live oracle (port/test/liveraw.py) or an emulator comparison like
  test_terrain.py's SG_EMU_ORACLE, else a state test; and the 155 COMMAND segments (their trace slices contain the recorded
  input: map each recorded DELIVER click/key to a Command through the original's hit tests only if the original function does it
  inside the straddler; otherwise record the mapping as a documented gap). 3.7b does NOT touch tickFrame; 3.7a does not touch
  commands. The interface between them: 3.7a's tickFrame takes a `std::vector<Command>` queue per frame and calls
  `apply_command` for each at the point in the frame where the original handles input (H1), so 3.7b defines Command first
  (commit-ready header within your first 30 minutes and tell 3.7a through the parent).

## Both
Shared build: build in your own dir (`cmake -S port -B port/build/<id> -A x64`, `SG_PORTSIM_DLL`), and keep a shared tree
compile-clean at every save. Touch port/src/registry.cpp after editing modules.inc. Run `port/test` and `-m static` before the
final report (under 30 lines): functions done/total, vectors, segment numbers, controls, defects, uncertainties, file list.
