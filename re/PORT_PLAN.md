# Port plan: a portable SimGolf reimplementation (written 2026-10-15)

Goal: a program that plays SimGolf from the user's own game files and does not load or patch
`golf_clean.exe`, `jgld.dll`, `Terrain.dll` or `sound.dll`. The hook work (C3/C4) becomes the
specification and the test oracle. It is not the code base.

## Decisions

- **Target (the user, 2026-10-15): portable reimplementation, no original binary.** A link-compatible rebuild from the
  matched sources was considered and not chosen.
- **Assets stay the user's.** The port reads the original install's files (PCX/BMP/FLIC, sounds, themes, courses, saves).
  It ships none of them.
- **Code is written from the behaviour notes and the A/B vectors, not pasted from `re/match`.** The matched sources are
  VC6-exact transcriptions with literal addresses. They are a reference for reading, not for copying.
- **C4 stops being the gate.** Remaining C3/C2 rows are promoted only when a port module needs them.

## Survey baseline (worker report, 2026-10-15)

- exe: 2,497 functions, 1,678 tracked. The 819 untracked are CRT (232), C++ unwind funclets (547), import thunks (27),
  one libjpeg function and 12 game-code stubs. Game logic is effectively fully tracked: 1,630 have hooks (C3/C4).
- Hooks do not port as written: 111 linked sources use literal exe addresses (9,660 occurrences), 16 naked bodies rebuild
  the original's stack frame. `golf_math.cpp`, the "pure" file, still reads three globals.
- Globals: 136 exe globals named; about 69 `.sve` fields are still `g_<VA>` (U-0004). Record strides are documented for
  about 20 tables (golfers, courses, walkers, tiles, objects, rosters, themes, sound slots).
- Not covered by hooks: `mainLoop` 0x40f5c0, `gameMain` 0x45baf0, `loadWorldAssets` 0x43dbe0, `loadInterfaceArt` 0x442180,
  `loadInfoScreenArt`, `loadFlic`, netService / netSendMessages and about ten screens with frame-local SEH or overruns.
- Formats documented: `.sve`, course `.cse`/`.srf`, themes, `.pro`/`.chr`, roster, story text, PCX, BMP, FLIC, WAV, MIDI,
  Terrain lighting. Not documented: Bink.
- Resolved (g1, 2026-10-16): golfer records are 156 x 0x100 from 0x005794b8 (re/analysis/data/golfer_record.md); the
  earlier stride-0x80 array at 0x57956e does not exist, and 0x005794c0 is field +0x08 of record 0.

## Survey scripts (re-run 2026-10-17 after the function-boundary repair b1; `re/tools/port_survey_globals.py`, `re/tools/port_survey_hooks.py`)

Globals (`log/port_globals.tsv`): the 1,812 game-logic functions (0x401000-0x4a4ecf; 1,537 before b1, whose index left
mainLoop's far blocks and 275 game functions outside every range: vtable methods, atexit / callback targets and
functions only mainLoop's far code calls; see
`re/analysis/data/function_boundaries.md`) reference **5,589 distinct data addresses** in .rdata/.data/.bss (5,302 before),
grouped into **960 clusters** (gap <= 0x40; 955 before). 138 addresses are named exactly and 67 clusters have a named
member, so **893 clusters, touched by 584 functions, are unnamed** (888 and 500 before b1). Caveat: an immediate that
happens to fall in the data range counts as an address, so the figures are an upper bound. The largest unnamed clusters
are initialised-data tables, not mutable state, and should be classified first: 0x4c80d0-0x4d2074 (1,602 addrs, 37 fns;
two clusters of 848 and 732 before mainLoop's far code was scanned), 0x4d21a4-0x4d3540 (252, 18 fns), 0x4e28e0-0x4e3d54
(204, 4 fns), 0x4e43ac-0x4e4584 (98, 24 fns). The most-shared mutable cluster is 0x83aa78-0x83ac30 (84 addrs, 116 fns).
The golfer-record sweep (`re/tools/record_fields.py`, 0x5794b8 x 0x100 x 156) finds the same 3,120 references, now all
inside function ranges (444 were in code outside every range).

Hooks (`log/port_hooks.tsv`; sources only, so b1 did not change it): 1,589 exe SG_HOOKs (1,586 addresses) in 187 linked
sources, classified by their own detour body only (helpers are not followed, so dependence is a lower bound): **pure 313,
globals only 177, original callees 570, both 517, naked 12**. So 1,276 (80%) call original code or read literal globals
directly, and only 313 are address-free.

## mainLoop and gameMain (looked at 2026-10-15)

- `gameMain` 0x45baf0: 1,166 bytes, `ret 0x10`, called from `entry` 0x4a682f. Registers fonts, creates the window, loads
  sounds, seeds the RNG, calls `showTitleScreen` 0x43cd70 (0x45bf0f) and then `mainLoop` (0x45bf48). Writes about 40
  globals, mostly in 0x4e42c8-0x4e4580 (UI/window state) and 0x83ab6c-0x83b9f0.
- `mainLoop` 0x40f5c0: 8,035 bytes in 2 ranges (the Ghidra list), 2,259 instructions, an SEH frame, 56 distinct callees
  and a jump table at 0x421618. It is **called once per process**, so neither can be A/B'd call by call like the C3 rows:
  the only oracle is a whole-game scenario with the hooks on and off (`re/frida/scenario_ab.py`, spawn-reach covers boot).
- Consequence for the port: reimplement both in the port's own structure (menu state machine, per-frame update, HUD),
  and verify them by frame-boundary checkpoints, not by function vectors. Of the 55 callees hooks.csv records for
  `mainLoop`, 45 are C4 already (its verified leaves), 5 are C2 (loadWorldAssets 0x43dbe0, loadInterfaceArt 0x442180,
  loadInfoScreenArt 0x44bde0, drawWorldObjects, loadGameBrowserScreen) and 5 are not tracked rows (CRT, a mid-function
  target 0x420e35). So the open work inside `mainLoop` is its own control flow plus those five loaders and screens.

## Architecture

| module | est. fns | source of truth |
|---|---|---|
| platform (window, input, timers, files, RNG, math) | ~150 | hook notes; RNG is `Random::next` at C3/C4 |
| data model (typed structs for every table above) | n/a | globals TSVs, strides, `.sve` map; the first deliverable |
| simulation (golfers, stories, ball flight, economy, course edit, tick order) | ~150 + mainLoop | hooks as spec, A/B vectors as tests |
| ui toolkit and screens | ~590 | uitree fixtures, ui_tour scenario frames |
| game render (sprites, world draw, image loaders) | ~210 | PCX/BMP/FLIC notes, golden frames |
| gfx backend (replaces jgld.dll) | ~300 | Display (86 slots) and Surface (60 slots) vtable maps |
| terrain (replaces Terrain.dll) | ~170 | 26 imported methods, matched sources as reading reference |
| audio engine (replaces sound.dll) | ~400 | `Snd` class contract, 12 exports, mixer notes |
| save and formats | ~30 | `sve.py`, save notes, fixtures |
| video | ~27 | FLIC notes; Bink needs a decoder or a skip path |

Dropped from the port: the CRT, unwind funclets, vendored libpng/zlib/libjpeg (use system libraries).

## Phases

1. **Data model.** Name and type the remaining `.sve` globals and the big blocks (0x59fc60, 0x5794b8), resolve the
   golfer-record conflict, produce one header of structs with offsets. This blocks everything else.
2. **Oracle.** Turn the C3/C4 A/B vectors and the six canonical scenarios into a port test harness: same inputs,
   compare state and frames against the original recorded once. Port modules are tested against recorded traffic, not
   by running the original.
3. **Headless simulation core.** RNG, golfers, ball flight, economy, course editing, `mainLoop` ordering (hook and
   verify `mainLoop`/`gameMain` first: they are C2 now). Gate: a championship round and a 6-year season reproduce the
   original's checkpoints (scenario_ab state words).
4. **Formats and loaders.** `.sve` round trip, course files, themes, rosters, image loaders, the asset manifest
   (`loadWorldAssets`, `loadInterfaceArt`).
5. **Backends.** Software 16-bit gfx backend first, then terrain, then audio. Frame comparison against golden shots.
6. **UI and screens.** Toolkit first, then screens in `ui_tour` order.
7. **Decisions on original defects.** One list: keep (affects saves or balance) or fix (hangs, overruns). Start from the
   findings in CLAUDE.md.

## First steps

- Resolve the golfer-record conflict and produce the typed struct header (phase 1).
- Cover `mainLoop`, `gameMain`, `loadWorldAssets`, `loadInterfaceArt` with hooks (they define tick order and assets).
- Run the two scripts the survey could not: globals referenced by game-logic functions, and pure vs state-dependent hooks.
