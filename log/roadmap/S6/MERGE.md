# S6 merge, wave 2 (G3: D42c and D52b3 into the `fixed` profile)

Wave 1 is committed as 32c11a36 (its MERGE.md text lives in that commit). This file and files.txt list wave 2 only. Nothing here is staged.

## Shared-file edits (apply in this order)

1. UNCERTAINTIES.md: append the four rows of `log/roadmap/S6/uncertain_rows_w2.md` (U-1591, U-1592, U-1598, U-1599) at the end of the table.
   The wave-1 rows are already there. `uncertain_rows.py` now reads every shape the workers wrote (the generator bug the integrator found:
   a headerless 4_uncertain.tsv and a `text` column with function names in the address cells), keeps every id and asserts no duplicates;
   4_uncertain.tsv now has a header. `log/roadmap/S6/uncertain_rows.md` regenerates all 33 rows and the wave-1 ones match the committed text
   apart from the hand fixes.
2. port/test/integrator.py, the switch -> evidence map (after the `generateRandomCourse.searchBound` entry, around line 877): add
   ```
                 "loadGolfPro.negativeWindowBound": "port/test/test_defects_golfers_w2.py::test_probe_course_season_hangs_on_returns_off, "
                                                    "test_synthetic_hang_on_returns_off (state)",
                 "generateRandomCourse.placementBound": "port/test/test_defects_gen_w2.py::test_blocked_site_search_hangs_at_1_and_returns_at_0 (state)",
   ```
   (the last entry of the dict has no trailing comma today: move the closing `}`).
3. CLAUDE.md: replace the wave-1 S6 entry's last sentences about the `ask` rows by the paragraph below (or append it after the wave-1 entry).
4. re/PORT_PLAN.md status table row 7: `decisions made (re/port/defect_decisions.tsv, 74 rows: 12 fix, 62 keep; G3 answered the 26 asks); profile fixed =
   8 switches over the hang and fault defects (re/port/defect_profiles.tsv); profile original = every switch 1`.

re/port/defect_decisions.tsv (regenerated: `~/bin/py -3.12 log/roadmap/S6/merge_decisions.py --write`, new `decision` column) and
re/port/defect_profiles.tsv (two rows appended) are S6's own files and are in files.txt.

## CLAUDE.md paragraph (wave 2)

S6 wave 2 (2026-10-24, notes log/roadmap/S6/7_notes.md and 8_notes.md, U-1591, U-1592, U-1598, U-1599): Mariano's answer G3 kept the 24 `ask` rows of groups
overruns, wrong values and course / terrain / money at `keep` and turned the two hangs into `fix`, so re/port/defect_decisions.tsv reads 74 rows,
12 fix / 62 keep, with a `decision` column (D5 fix, G3 fix, G3 keep) and profile `fixed` has 8 switches (re/port/defect_profiles.tsv). D42c:
loadGolfPro with g_difficulty 5 or 6 never ends in the original (4 - d < 0 makes the acceptance bound fall without end); switch
loadGolfPro.negativeWindowBound (own switch, tests port/test/test_defects_golfers_w2.py, 45): the port's old behaviour at those difficulties was an
endless spin like the original's, now switch 1 raises NoReturn once the falling bound is below every value the roster can give and switch 0 ends the draw
with pick 0; where the original completes the check never fires and 1 and 0 agree, the 11 of 36 synthetic states in which the original hangs in Unicorn are exactly
the ones the port gives up on, and the golfers vectors are 367/367 GREEN at 1 and 0 (U-1591 the cutoff argument, U-1592 the emulator stops at loadThemeFile's
file reads on the states where the original passes). D52b3: the feature-site searches gated by evalPlacementArea hang the original at location 4 (2 x 2 site,
0x0047172e..0x0047176f) and at locations 5, 6, 8, 12, 13, 15 (5 x 5 site, 0x004718a4..0x004718e7) when no reachable cell is accepted (300 M instructions
exhausted at about 921,000 iterations on the port's own images); switch generateRandomCourse.placementBound (wave 1's searchBound keeps D52b1 / D52b2),
tests port/test/test_defects_gen_w2.py (30; controls for all 8 locations equal the original byte for byte at 1 and 0, location 3 on the same image
completes): at 0 location 4 stamps no object and locations 5..15 skip the whole feature (U-1598, U-1599, port choices). The opt-in emulator oracle for
terrain_gen (115 tests) passes. The old S6.7 note that wave 1's loadGolfPro guard raised NoReturn at d = 5 / 6 was wrong: the port spun.

## Gate (expected numbers, on a build holding the S6 files; the lead's mirror is port/build/S6_w2_mirror/b/Release)

- `~/bin/py -3.12 -m port.test.profiles validate` -> `profile fixed: 8 member(s) of 14 switches`
- test_profiles 19, test_defects_gen_w2 30, test_defects_gen 13, test_terrain_gen 14 (101 opt-in skipped), test_golfers 60, test_defects_golfers_w2 45,
  test_core 54 pass; test_defects_golfers (wave 1) 25 and test_tick 250 in log/roadmap/S6/lead_gate_wave2_long.log
- test_static: 5 pass and `test_every_module_file_is_listed` fails on the working tree because of siblings' unlisted modules (audio_*, video, textlab,
  fttext*), not on S6's files (S6 adds no module file); in the lead's HEAD-based mirror there is nothing else to list.
- The segment gate and boot gate belong to the integrator: the default (all switches 1) changes behaviour only at g_difficulty 5 / 6 inside loadGolfPro
  (an endless spin becomes NoReturn), and no pinned scenario plays at those difficulties.
