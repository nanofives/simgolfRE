# Stream 38c: showCourseReport and showEndOfYearScreen (2026-10-20)

Module `reports` (port/src/sim/reports.cpp, no header: both functions are reached through the registry by their hooks.csv names),
tests port/test/test_reports.py, pinned digests port/test/golden_reports.json. Specification: the hook transcriptions
shim/src/re/c3fr.cpp (showCourseReport 0x0044fb30) and c3er.cpp (showEndOfYearScreen 0x0044cff0, hooks.csv name; its note file is
named CareerRetirementFlow), the method of log/p3/p37b_notes.txt.

## What the port keeps

showCourseReport(mode): mode 0 the per-hole table (no simulation write except the formatting calls), modes 1 / 2 the grading page.
- The 18-hole accumulation (sums S, counts, the three difference columns, flags, best) with every division as `div32` (a zero divisor
  is a DivideError: 23 of the 500 wide-seed showCourseReport cases end that way on both sides) and wrapping arithmetic; variables that
  the original carries from one hole to the next (cnt, flags, best, time, pace ...) are carried.
- Grading page (modes != 0): bucketValue(g_holesPlusOne - 1) picks crit / flags / cnt / best; a bucket above 3 (g_holesPlusOne 20 or
  more) takes them from the last hole of the loop (stale values, as the original does); star marks per criterion, -999 for a criterion
  with no star, the facility count over bits 6..19 of g_buildingKindMask, the three tour events (postEvent 0xc when g_holesPlusOne > 9,
  0x10 when > 18, 0x15 at a score of exactly 100 with > 18) only for a score of 90 or more with the facility criterion passed; then
  score <= 0 clears flag bit 22 only; score > 0 writes g_courseReportScore, g_tournamentPurse = ((score - 50) / 5 + 5) * (grade + 1) *
  g_holesPlusOne * 2 and sets flag bits 22 and 13.
- Mode 0 rows and the subtotal row after hole 18 keep their appendHoleName / appendCents / appendNumber calls in the original's order with
  the original's arguments (they write the hashed g_textBuf).
- Mode 2 returns right after the page; modes 0 / 1 wait in the one-button loop (pump_messages Blocking, button tested before
  flush_redraw), then clear g_ui_569498 and pump_light.
showEndOfYearScreen:
- The year rows' formatting calls (cash row appendNumber x2, skill row appendCents x2), this month's slot of g_memberTypeHistory =
  low word of the live member count (index (g_date / 1024) % 500, sign kept: a negative date writes below the array, as the original),
  the highlight lines' calls (event kind 0x120 -> loadStoryStrings(subject, -1, 0, 0), 0x160 -> landmarkName(subject, 0)), the debt
  counter (g_cash_div100 < 0 and flag bit 24 clear: +1, else 0), the loop that ends on the LEFT button (button == 1) or any key
  (flush_redraw), pump_light, addScore and, unless it returns -1, drawScoreSummary. The audio request is the optional playSound.
- Dropped (no simulation write, checked with the oracle): widget, image and string drawing, the surface and display calls, window
  refresh, waits, the text assembled in g_textBuffer, startSoundSlot.

## Evidence

- Emulator oracle (SG_EMU_ORACLE=1, port/test/cmdemu.py, the original in Unicorn with the drawing callees stubbed as in 3.7b's screens,
  the display object of showCourseReport's slot 0x9c given a real stdcall stub): eax, consumed input, the whole data image (minus the
  3.7b drawing mask) and, for showEndOfYearScreen, the recorded loadStoryStrings calls. 400 cases per function in the pytest run and
  500 more with seed `wide` (log/p38/38c_oracle_run.log, log/p38/38c_final_stats.log): all GREEN. Original outcome classes in the
  500-case runs: showCourseReport mode 0 169 ok / 9 divide error, mode 1 176 / 7, mode 2 132 / 7; showEndOfYearScreen 379 ok / 121 wait
  for input (no left button or key in the script), port equal in every class.
- Case generator (`_report_image`): four course variants: normal (six quality levels), `perfect` (18 holes of par 4 that score 100;
  single-field perturbations take marks away), `low` (every criterion keeps one or two stars, score 39..41: reaches the negative
  `(score - 50) / 5` quotient), `special` (all-zero tables: the cnt == 0 arms; a row that cancels its start value 8: the R94 == 0 arms);
  g_holesPlusOne from {19, 12, 10, 6, 5, 1, 0, 20, 25}; 75% of the cases start from a quiet input state so the loop runs several passes.
  Scores reached by the port in a 600-case port-only run: 39..41 and 53..100 (a score between 42 and 52 has no generator).
- Branch coverage of the original's own jcc (log/p38/38c_cover.py, 38c_missing.py): showCourseReport 204 sites, 115 with both sides;
  showEndOfYearScreen 59 sites, 33. Every remaining side is one of: `test <constant string address>; je` (never taken: the `strlen`
  idiom of the text calls, about 80 sites in showCourseReport and 25 in showEndOfYearScreen), the hover and button-lit state of the
  loop (0x453191 / 0x453195 / 0x4531b8, 0x44e490 / 0x44e4bb: drawing), the null display pointer (0x44fb70), h < 1 (0x450051, h starts at
  1), the `jle` on a colour value (0x450e13, never falls through; c3fr.cpp comment), `tm > 300` with a zero time criterion (0x452271:
  the criterion is zero only for 295..300), the empty-holes table foot (0x45300e).
- Golden digests: 150 fixed cases per function pinned from the port after the oracle was GREEN on them (`SG_EMU_ORACLE=update`);
  the digest covers eax, the error code, consumed input, the whole image and the recorded peer calls.
- Negative controls (11, all RED against the digests and against the original, `SG_EMU_ORACLE=1` run: 16 passed in 444 s):
  showCourseReport 1 purse `+ 4`, 2 only bit 22 set, 3 full-marks event 0x14, 4 facility bits from 7, 5 a zero-star facility
  criterion does not zero the report, 6 leave stores 1 in the mode byte; showEndOfYearScreen 1 member history count + 1, 2 debt
  counter not cleared outside debt, 3 a right click ends the screen, 4 loadStoryStrings gets subject + 1, 5 no addScore after the loop.
  Two earlier controls were equivalent mutants and were replaced: `-998` for a zero-star length criterion (any later criterion keeps the
  total negative) and landmarkName's second argument (it only changes the masked g_textBuffer).

## Segments (log/p38/38c_cmdsegs.tsv, port built from HEAD + reports.cpp + the module line)

| Segment | Before | Now |
|---|---|---|
| cov_a_career/02 | FAIL 786/1020 (showCourseReport) | PASS 1020/1020 |
| cov_a_career/04 | FAIL 797/981 (showEndOfYearScreen) | PASS 981/981 |
| cov_a_keys/03 | FAIL 35/35 (showCourseReport) | PASS 35/35 (the end checkpoint lies inside the report's loop: "ran to a Blocking pump with no input left") |
| cov_b_career/01 | FAIL 64/219 (showCourseReport) | PASS 219/219 |
| cov_b_career/02 | FAIL 64/248 (showEndOfYearScreen) | PASS 248/248 |
| ui_tour/00 | FAIL 1/247 (showCourseReport) | FAIL 97/247: showCourseReport passes, stops in showRoutingMapScreen (not registered; F5 key) |
| ui_tour_v/00 | FAIL 1/247 (showCourseReport) | FAIL 97/247: same |

No segment diverges. The full COMMAND set was not re-run (only these seven reach the two names); other children's modules were not in
the build.

## Findings in the original (the port reproduces each)

- Grading page with g_holesPlusOne >= 20: bucketValue returns -1 and crit, flags, cnt, best come from the last iteration of the hole
  loop; `9 / (4 - flags)` is a #DE when that stale flag word is 4 (only the third difference column passed on the last hole).
- A report with score <= 0 clears only flag bit 22; g_courseReportScore and g_tournamentPurse keep their old values.
- Time criterion: g3 is raised from 0 to 1 only when the summed time is 295..300 (the `tm > 300` test cannot fail there).
- showEndOfYearScreen: a right click does not end the screen (the test is `button == 1`), a key does; flush_redraw runs on every pass
  including the one that ends on the left button; addScore runs twice when it does not return -1 (once here, once in drawScoreSummary);
  g_memberTypeHistory is indexed by the signed remainder, so a negative g_date writes before the array.
- The grade page's `9 / step` and the count criteria use `cnt` from the bucket, not the hole count.

## Pitfalls hit

- `lib.load()` without SG_PORTSIM_DLL builds into port/build/x64 (the parent's tree): one of my first runs did and failed to link
  (LNK1104, a test holding the DLL). Always export SG_PORTSIM_DLL first. The partial objects it left in port/build/x64 are rebuilt
  by the next normal build; nothing was edited there.
- Generating 600 cases as a list needs 2 GB (3.6 MB per image): `iter_cases` is lazy; a Unicorn instance and its hook closures form a
  cycle, so `emu()` calls gc.collect().
- showCourseReport's display slot 0x9c is `ret 0xc`: a stub that pops nothing shifts the stack and the function reads another mode
  (first runs showed the table drawn for mode 2).
- Precedence trap in a nested conditional expression made the `low` variant use a random g_holesPlusOne.

## Open items

- U-0980 (display slot), U-0981 (loadStoryStrings recorded, landmarkName text masked): log/p38/38c_uncertain.tsv.
- ui_tour/00 and ui_tour_v/00 wait for showRoutingMapScreen (F5): nobody in 3.8's table has it.
- The modal loops' hover state (button lit) is not modelled (drawing).
- Merge lines: log/p38/38c_merge.md (modules.inc, test_tick.py OPEN_MISSING, commands_split.tsv, UNCERTAINTIES.md, CLAUDE.md).

## Tools left in log/p38/

38c_mirror.py (HEAD + own files private build), 38c_explore.py (port-only outcome histogram), 38c_oracle_stats.py (oracle classes),
38c_cover.py / 38c_missing.py (jcc coverage and the missed sides with their instructions), 38c_dbg.py, 38c_dis.py (disassembly range),
38c_scoredbg.py (Python mirror of the grading arithmetic).
