# Stream 38g: showRoutingMapScreen (2026-10-20)

Module `routing` (port/src/sim/routing.cpp, no header: the function is reached through the registry by its hooks.csv name), tests
port/test/test_routing.py, pinned digests port/test/golden_routing.json. Specification: the hook transcription shim/src/re/c3et.cpp
(showRoutingMapScreen 0x00456be0, batch c3et, 6,713 bytes), the method of log/p38/38c_notes.md.

## What the port keeps

- Entry: the walker count (flag byte set, state byte negative and not 0xfa), the scroll / selection / hovered-button locals, the
  hole permutation `perm[i] = i` (20 entries).
- Per redraw, no drawing: spreadAllShade in mode 1; the screen ends right after it when the map end flag (g_map822b80[0]) is set;
  appendCourseTitle(1); in mode -1 the hole list's time division (`record +0x1ec / +0x20 / 40` with `div32`, only for a hole with par
  and a non-zero count); in mode 0 the walker rows: a listed walker skipped by the scroll count, hidden below the eighth row (`more`),
  or shown (its sheet column `word +0x1c % g_sheetCols[table]` with `mod32`, a kind-0xc walker on column 0 gets kind 0xb written
  back, then appendWalkerName / appendDate(word +0x22 << 10) / appendNumber(word +0x24 * 100)); every listed walker's marker calls
  appendWalkerName again and counts its number.
- The input loop in the original's order: the cursor read (InputSource, NeedInput without one), the hit box (seven boxes: left,
  top / right / bottom mode buttons, quit, two scroll arrows), a changed hover redraws without pumping, then the Blocking pump,
  the button word (flush_redraw when zero), a click below row 0x118 sets mode = hit - 1 (quit ends), arrows scroll, a click at or
  below row 0x101 (or a key after one) ends, a left click selects the hole under it ((y - 0x68) / 17 + 1, + 9 at x >= 0x190, clamped
  1..18), a right click moves the selected hole to the target (hole record 0x208 bytes, waypoint record 0x18 bytes and permutation
  entry to the scratch slot, the holes between shift by one, the moved hole lands, every shifted hole's tee and green tile get the
  new number in the low five bits of the tile word map, no bounds test; the scratch records are zeroed).
- Exit: the inverse permutation applied to every golfer hole byte above 0 (a byte of 20 or more: defect, below), g_holesPlusOne =
  first hole with no par (19 when all have one), eax = that hole record's address.

Dropped (no simulation write): the map, hole and walker drawing, display slot 0x9c calls, window refresh, waits, rateLot /
evalPlacementArea / sampleHeight (value helpers: stack writes only), and the drawing scratch g_linePoint and g_map822b80[1] (U-1021).

## Defect (the known one) and its switch

`showRoutingMapScreen.exitRemapReadsFrame` (ref U-1020, default ON = original): the exit indexes a 20-dword stack table with the
golfer's hole byte; a byte of 20 or more reads the caller's frame. The port has no frame: ON it throws MissingResource (SG_E_NO_RESOURCE)
for such a byte, OFF it leaves the byte alone. A real game state holds hole bytes 0..19 (19 = the leaving slot), so only a corrupt state
reaches it. The oracle checks OFF against the original with those bytes masked and ON as "port reports the missing frame where the
original runs on".

## Evidence (opt-in SG_EMU_ORACLE=1, port/test/cmdemu.py; the original in Unicorn, drawing callees stubbed, pumps scripted)

- Oracle: eax, consumed input and the whole data image minus the 3.7b mask plus g_linePoint / g_map822b80[1].
  Cases: random images (mode in -2..9, walker lists of 0..64 listed records with states -1..-8 and rare random ones, sheet columns with
  zeros, 18 hole records with out-of-grid tile coordinates, waypoint records with -1, tile map, golfers' hole bytes with 5% of cases holding
  a byte >= 20) and scripted input (left / right clicks on the hole rows, button-row clicks, keys, arrow clicks, selecting and then moving
  a hole onto its own row, a final click on the strip or the button row), the cursor on a hit box 60% of the time.
  Result: see "Results" below.
- Hit box edges: 7 boxes x 16 corner points (x0-1, x0, x1-1, x1 by y0-1, y0, y1-1, y1), mode 0, long list; a button-row click writes
  hit - 1 into the mode (arrows 8 and 9), the quit box ends the screen, no box redraws: all equal the original.
- Branch coverage of the original's own jcc (log/p38/38g_cover.py, 400 cases, log/p38/38g_cover6.log): 141 sites, 128 with both sides.
  The 13 missing sides: drawing and colour tests (0x00456ebc area == -1 colour, 0x00456f35 / 0x00456fdc tileBlocked, 0x004579c5 scroll
  bar height, 0x00457af6 sprite flip), the display-pointer test of the last slot call (0x004586ab), 0x0045831b (button set after
  flushRedraw returned a key: dead in the port model, pumpInput483c30 dispatches nothing), 0x0045852c (`t == s` after `t < s` was
  ruled out: unreachable), 0x00458353 / 0x00458363 and the box-edge sides 0x00458280 / 0x004582a9 / 0x004582bc, which need a cursor
  sequence or an exact edge (U-1022; the edges are in the edge test).
- Golden digests: 120 fixed cases pinned from the port after the oracle was GREEN on them (`SG_EMU_ORACLE=update`); digest = eax,
  error code, consumed input, whole image.
- Negative controls (9, all RED against the digests, and against the original with SG_EMU_ORACLE=1): reorder leaves the moved hole's
  tee tile number; exit remaps through the permutation; hole count one more; mode = button instead of button - 1; scroll arrows
  swapped; right half of the list at x >= 0x180; waypoint record does not move; kind 0xc kept; walker date shifted by 9. The first run
  of control 6 was blind (no click x in 0x180..0x18f): the x generator got that range and the goldens were re-pinned.

## Segments (log/p38/38g_cmdsegs.tsv, port built from the working tree + routing)

| Segment | Before | Now |
|---|---|---|
| ui_tour/00 | FAIL 97/247 (showRoutingMapScreen, after 38c's showCourseReport) | FAIL 121/247: runs showRoutingMapScreen, stops in switchToCourse (missing: courseActionMenu) |
| ui_tour_v/00 | FAIL 97/247 | FAIL 121/247, same stop |

No hash divergence in either. `courseActionMenu` (5 KB) is 38b's row after 38a. The full COMMAND set was not re-run (only these two
segments reach the F5 map).

## Findings in the original (the port reproduces each)

- The exit's frame read for hole bytes >= 20 (U-1020).
- A button click writes `hit - 1` into the mode for any non-negative hit, so an arrow hovered at a click below row 0x118 writes 8 or 9
  (the edge test covers it); a click at y >= 0x101 ends the screen even when the click came with a key (the y is the last click's).
- A right click on the selected row (`t == s`) still copies the record to the scratch slot and back and renumbers its tiles.
- The tile renumbering has no bounds test: a tee or green tile outside the 50 x 50 grid writes a word before or after the tile word map
  (`x * 50 + y` in 32 bits); the port writes the same address (a fault where it leaves the mapped image).
- The list's column modulo reads g_sheetCols past its 590 entries for walker states below -64 (table index up to 0x2a0) and divides by
  zero where the table holds 0.

## Pitfalls hit

- `python3` is not on PATH here (a first edit script silently did nothing): use `~/bin/py -3.12`.
- Another stream's full port test run held 10 GB of commit and made Unicorn `mem_read` raise MemoryError in my coverage runs twice;
  re-running when the memory was back worked.
- The first generator made nearly every walker "listed" (random bytes with a negative state and a flag byte), so mode 0 always had more
  than eight; non-listed walkers now have no flag byte or a non-negative state.
- Zero entries in g_sheetCols made most mode-0 cases end in a #DE before any input; now 12% of cases carry zeros.

## Open items

- U-1020 (frame read), U-1021 (display slot / scratch / value helpers stubbed), U-1022 (cursor sequence branches): log/p38/38g_uncertain.tsv.
- courseActionMenu is the next stop of ui_tour/00 and ui_tour_v/00.
- Merge lines: log/p38/38g_merge.md (modules.inc, test_tick.py OPEN_MISSING, commands_split.tsv, UNCERTAINTIES.md, CLAUDE.md).

## Tools left in log/p38/

38g_mirror.py (working tree + routing private build tree), 38g_build.sh, 38g_stats.py (outcome classes and reached effects),
38g_cover.py (jcc coverage and the missed sides with their instructions).

## Results

`SG_EMU_ORACLE=1 SG_ROUTING_N=400 pytest port/test/test_routing.py -v` (log/p38/38g_oracle_full.log): 14 passed in 1,143 s, on the final
generator: 400 random cases equal the original (eax, consumed input, image), 400 more with the defect OFF and a golfer byte >= 20 in every
case, the 112 hit-box edge cases, the golden digests, all 9 negative controls RED against the digests and against the original, the
registry check. Default run (no oracle, log/p38/38g_default_run.log): 11 passed, 3 skipped (the oracle ones). Outcome classes of an
80-case sample (log/p38/38g_stats.py): 60 ok, 7 original #DE (port DivideError), 11 original waits for input (port NO_RESOURCE), 2 original
ok with the port reporting the frame read (a byte >= 20); effects reached: holes 26, tile words 26, waypoint records 26, hole count 56,
golfer bytes 20, walker kind 14, mode 4 of 80 (earlier generator; the final one adds the button-row ending). Not run: Win32 backend (no x87).

## Continuation: showShortcutsScreen and showMembershipRoster (parent's follow-up, same day)

Segment path (working tree + routing, log/p38/38g_cmdsegs2.tsv / 38g_cmdsegs3.tsv): ui_tour/00 and ui_tour_v/00 stopped at
showShortcutsScreen (178/247, from the parent), then at showMembershipRoster (202/247), and now PASS 247/247 (45 dispatches delivered, 0 left).

- showShortcutsScreen (0x0044e770, c3es): the page is drawing; the command effect is the modal loop (Blocking pump, cursor read, a left press
  inside the close button's box 0x2c6..0x2f1 x 0x16a..0x195 clears the keep-going byte, a key ends via flushRedraw) and the exit
  (g_ui_569498 = 0, pumpLight). Oracle 300 cases + golden 120 + 4 negative controls (right click ends too, box 0x10 narrower, mode byte left
  at 1, key does not end) all RED against digests and the original (log/p38/38g_shortcuts_oracle.log: 6 passed).
- showMembershipRoster (0x00454c50, c3eo): the one simulation effect is the pass marker: bit 7 of byte +2 of all 76 golfer-type records
  is cleared at the end of every redraw, which also clears the exclusion bit the records carried on entry (U-1023). Row selection and scroll
  offset only choose drawn rows and are not ported. Loop: zones close / up / down / none by cursor, Blocking pump, a button or key ends the
  pass, left press on the close button leaves, on an arrow redraws, other buttons leave unless the pass ended by leaving an arrow zone.
  The cursor read before the first wait (0x0045572a) is kept as a read so a trace cursor sequence stays aligned. Oracle 300 cases + golden
  120 + 5 negative controls RED (log/p38/38g_membership_oracle.log: 7 passed). Controls found equivalents twice: "a right click on the close
  button ends it too" (the original leaves on any non-1 button anyway) was replaced by a right click on an arrow; the first golden run
  was blind to the arrow controls until cases started with a press on the zone the cursor sits in.
- The oracle compares neither eax (both originals return a stubbed callee's value) nor anything the drawing writes (3.7b mask).
- Tool fix: 38g_mirror.py now copies untracked modules too (the working tree's modules.inc lists dialogs / saveload / ... which are not
  tracked yet); the previous mirror only had them by luck of the earlier tree.
