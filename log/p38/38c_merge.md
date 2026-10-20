# 38c merge lines (for the parent)

## 1. port/src/sim/modules.inc (append one line)

```
SG_MODULE(reports)
```

Without it `port/test/test_static.py::test_every_module_file_is_listed` fails (reports.cpp is not listed) and the two
functions are not in the registry (tick then stops on `showCourseReport` / `showEndOfYearScreen`).
My own builds use `log/p38/38c_mirror.py` (HEAD + reports.cpp + this line) into `port/build/38c`.

## 2. port/test/test_tick.py (OPEN_MISSING: delete two entries, both are ported now)

```
    "showCourseReport": "3.7b gap",
    "showEndOfYearScreen": "3.7b gap",
```

After the merge the segments' status is (log/p38/38c_cmdsegs.tsv, built from HEAD + reports):
cov_a_career/02, cov_a_career/04, cov_a_keys/03, cov_b_career/01, cov_b_career/02 PASS; ui_tour/00 and ui_tour_v/00 now
run showCourseReport and stop later in `showRoutingMapScreen` (97 of 247 frames; a 3.7b gap nobody has in 3.8's table).
If those two are listed as expected-missing names in test_tick.py, the entry for them is `showRoutingMapScreen`.

## 3. re/port/commands_split.tsv (replace the two gap rows; columns addr name class command_effect dropped evidence gap)

```
0044cff0	showEndOfYearScreen	command	g_memberTypeHistory slot of this month, g_debtYears counter, loadStoryStrings / landmarkName calls of the highlight lines, appendNumber / appendCents formatting (g_textBuf), addScore + drawScoreSummary after the loop	drawing, text assembly, waits, audio request	emulator oracle 400 + 500, golden 150	-
0044fb30	showCourseReport	command	g_courseReportScore, g_flags bits 13 / 22, g_tournamentPurse, postEvent 0xc / 0x10 / 0x15, appendNumber / appendCents / appendHoleName formatting (g_textBuf), g_ui_569498 = 0 on the way out	drawing, text assembly, the display slot call, waits	emulator oracle 400 + 500, golden 150	-
```

## 4. UNCERTAINTIES.md

Append the two rows of `log/p38/38c_uncertain.tsv` (U-0980, U-0981).

## 5. CLAUDE.md (one paragraph, after the 3.7b / 3.7a notes)

```
Phase 3.8 38c reports (2026-10-20, port/src/sim/reports.cpp, tests port/test/test_reports.py + golden_reports.json, notes log/p38/38c_notes.md, U-0980..U-0981):
showCourseReport (modes 0 table, 1 / 2 grading page) and showEndOfYearScreen as command effects over the 3.7b pump model; 400 + 500
oracle cases each GREEN, 11 negative controls RED against the digests and the original; five of the seven stopped segments PASS, the
other two continue to showRoutingMapScreen.
```
