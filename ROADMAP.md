# ROADMAP — SimGolf RE

## P0 Foundation (DONE 2026-10-02)
- [x] ISO extracted without the installer (7z + unshield); portable install in `original/`.
- [x] SafeDisc 2.60.52 removed (Safedisc2Cleaner) -> `golf_clean.exe`, anchored.
- [x] No registry dependency (static proof); fonts self-loaded.
- [x] Shim (winmm proxy): windowed, Bink DIB fix, skip-intro, patch loader, SG_HOOK registry, test exports.
- [x] Ghidra master + pool + headless tools; MCP wired in `.mcp.json`.
- [x] Frida harness (`re/tools/game.py`): spawn, windowed, virtual cursor, screenshots, PID hygiene.
- [x] Path-1 A/B tool with negative control; confidence gate `scripts/re_classify.py` with unit tests.
- [x] Test suite: static / runtime / ghidra / slow layers.
- [x] First function through the ladder: `0x004490d0` `Terrain::tileAt` -> C2 (C3 correctly refused: dead code).

## P0.5 Measurement (DONE 2026-10-02)
- [x] INT3 coverage census (stable: 680/680/678 over 3 runs); exe 707/2207, Terrain.dll 344/1050 in sandbox.
- [x] Three-axis scoreboard (`scripts/scoreboard.py`), replay accepted as C3 verification.
- [x] Record/replay in Unicorn; first C3: Terrain.dll `0x00001d50` `Terrain::tileAt` (2000 real calls).
- [x] v1.00 unwrapped; version diff (2098 identical, 48 changed, 11 operand-only); PATCH_MAP links 6 patch notes.
- [x] Failure tools: SBFL (Ochiai), ddmin, invariants; TESTING_STRATEGY.md.

## P0.6 Scenarios + matching (DONE 2026-10-02)
- [x] Scenarios course_season (time warp, ~6 years) and championship (tournament); 4 of 6 patch-linked
      functions now reached (landmark-donation notice and golfer eject panel still not).
- [x] VC6 matching scaled: 26 functions 100%; VC6 objects linked into the shim (tileAt detour = original bytes).

## P1 Map the live surface (next)
- [x] Gary's shots driven; championship rounds complete (5/5).
- [x] Golfer panel scenario (organic); landmark notice reached by injection (organic trigger: U-0003).
- [x] All 22 live exported Terrain.dll methods matched at 100% (29 functions total).
- [x] 29 unexported Terrain.dll callees matched (every one under 600 bytes on the list; 58 functions total).
- [x] Large callees matched too (render, drawTileObjects, isCulled, elevate/lowerCorner, ctor, drawTile,
      texture loader, 0x10038900), setTypeId's callees and loadLighting: 73 functions at 100%.
- [x] golf_clean.exe: 145 live functions at 100% (/O2), 25 partials in re/match/wip/ (2026-10-04).
- [ ] More golf_clean.exe: golfer AI (FUN_00467a00 thoughts, FUN_00427380 hole end), story chain.
- [x] Organic story chain mapped (U-0003 partial): golfer_stories scenario advances chapters on a
      multi-hole course.
- [x] +0xa4 mapped: golfer mood, thought deltas in FUN_00467a00 (U-0003).
- [x] 5-hole saved-game fixture (stories5.sve) + load flow; saves need original\saved games\ (now created).
- [ ] Organic landmark: chapter 4 for both partners not reached yet on the fixture.
- [ ] Neighbourhood recording (target + callers + callees in one run).
Goal: know which functions actually run, so confidence work targets reachable code.
- [ ] More census scenarios (a played round, save/load, tournament) -> `re/coverage/<scenario>.tsv`.
- [ ] Subsystem map of `golf_clean.exe` (2,207 functions) seeded from strings/exports/callers:
      course, golfer, economy, AI/pathing, UI, save. C0 -> C1 sweep via `re_classify.py`.
- [ ] Canonical scenarios (deterministic inputs + seeds) for C4 evidence (DEFERRED D-0002).
- [ ] U-0001: why `jgld.dll` (debug) and not `jgl.dll`.

## P2 Confidence work on live code
- [ ] First live function to C3 (reachable caller, A/B GREEN), then C4 with a scenario ON/OFF run.
- [ ] Struct recovery: Terrain header (`+0x14` width, `+0x18` height, tiles at `+0x3a4`, 0x248-byte tiles),
      golfer, course, hole.

## P3 Targeted patches (driven by user requests)
- [ ] Pick the first gameplay patches; each one: analysis note -> `patches/<name>.ini` (signature-checked)
      or an SG_HOOK behaviour change -> runtime test proving the effect.
