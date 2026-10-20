# 38e merge request (clock query U-0949, showHoleStatsScreen)

Three shared files change; the patched copies this stream built and tested are under `log/p38/38e/shared/` (same relative
paths), and the patches against HEAD a93513fe are `log/p38/38e/commands_h.patch` and `log/p38/38e/tick_cpp.patch`
(`git apply --check` passes on the working tree as of 2026-10-20).

## 1. port/src/sim/modules.inc: one line at the end

```
SG_MODULE(holestats)
```

Until this line is in, `port/test/test_static.py::test_every_module_file_is_listed` fails (holestats.cpp exists).
registry.cpp must be rebuilt after the edit (touch it unless the OBJECT_DEPENDS change of section 0 is merged).

## 2. port/include/portsim/sim/commands.h (additive): `git apply log/p38/38e/commands_h.patch`

- `InputSource::clock(int32_t site, uint32_t &t)`, virtual, default `return false`. Site = the clock call's return address
  minus mainLoop's frame-head return address (the trace's own key, like tick.cpp's kSite* constants). Contract: the
  source's NEXT read at that site after the last read it returned (any site); false = unknown / none left, and the
  ported loop throws NeedInput there.
- `QueueInput::set_clock(site, values)` + `QueueInput::clock` override: a per-site script for tests (values in order,
  none left = false). No existing behaviour of QueueInput changes (batch / exhausted / cursor untouched).

## 3. port/src/sim/tick.cpp (TraceInput, parse_trace, InputStats): `git apply log/p38/38e/tick_cpp.patch`

- `TraceFrame::clock_pos` (the clock ordinal of each `clock` entry), filled in parse_trace beside `clock`.
- `TraceInput::clock(site, t)`: forward scan of the frame's `clock` records from the entry after the last one it returned;
  reset per frame in `begin`. The frame body's own reads (FrameIo::clock / clock_site) do not move it.
- Stats: `clock_served`, `clock_none` (no record at the site left), `clock_late` (a modal dispatch delivered after the
  port took a clock read whose ordinal is >= the dispatch's position: the port read the clock past the point where the
  original dispatched). `InputStats::text()` appends `; clock queries N served, M none left, K dispatches late` ONLY when
  any of the three is non-zero, so every existing run_detail string is unchanged.
- No kDrops / fallback table entry changes.
- Composition with 38f's `log/p38/38f_tick.patch`: both apply in either order (its TraceInput::batch hunk lands with
  fuzz 1 next to this patch's `clock_late_` line, checked with `patch --dry-run`).

## 4. Not shared, already in the tree (this stream's own files)

- port/src/sim/holestats.cpp, port/include/portsim/sim/holestats.h (module `holestats`)
- port/test/test_holestats.py, port/test/golden_holestats.json
- port/test/cmdemu.py: two optional `run()` parameters, `clock=` (fn(ra) -> value | None) and `gate=` (fn() -> bool),
  additive next to 38b's `stub_py` / `import_handlers` (edited in place, both streams' edits present).

## 5. After merging: expected results (measured on port/build/m38e = HEAD + these changes)

- 190 accepted segments: 146 PASS (35 / 35 TIME-FORWARD, 111 / 155 COMMAND), 44 FAIL. Against the HEAD baseline the only
  changes are championship/02 (FAIL 335/394 -> PASS 394/394) and cov_a_keys2/02 (FAIL 2/254 -> PASS 254/254); the other
  188 have identical status, frame count and run_detail text (log/p38/38e/segs_before.tsv vs segs_final.tsv).
- port/test/test_holestats.py 10 passed / 3 oracle skips by default; 13 passed with SG_EMU_ORACLE=1.
- test_commands.py + test_tick.py + test_holestats.py on the mirror build: 327 passed, 47 skipped (oracle opt-ins and
  sibling goldens not pinned).
- Rows for UNCERTAINTIES.md: log/p38/38e_uncertain.tsv (U-1000, U-1001). Close U-0949 with: "InputSource::clock (38e):
  the trace's next clock record at a call site; showHoleStatsScreen ported over it, championship/02 and cov_a_keys2/02
  PASS with all 457 / 385 clock records at its five sites served in order".
- re/port/commands_split.tsv row 00453330: gap -> command (effect: g_msgState53df54 = 0, the page builds' peers
  appendHoleName / appendCents / appendRatingLabel / buildGolferComment, the consumed input).
