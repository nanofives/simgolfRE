# Phase 3.8 plan: integrator and the stopped segments (draft 2026-10-20)

State at the start (HEAD a93513fe, pushed): 35 / 35 TIME-FORWARD and 109 / 155 COMMAND segments PASS; 46 COMMAND segments stop
on an unregistered name, 0 diverge (log/p3/p37a_cmdsegs.tsv). Phase 3 gates (PHASE23_PLAN.md section 5): (a) vectors done per
module, (b) done, (c) (d) (e) (f) open.

## 0. Baseline first

- A stale `pytest port/test` (python.exe started 2026-10-20 00:20) holds port/build/x64/Release/portsim_c.dll, so the full
  suite run of log/p3/suite_run failed to link (1,182 errors, LNK1104). Kill that PID only, re-run port/test (last good 1,045)
  and static (600, passed).
- CMake: make registry.cpp depend on src/sim/modules.inc (OBJECT_DEPENDS), so nobody has to touch it by hand again.
- Every child builds in its own `port/build/<child>` and loads it through SG_PORTSIM_DLL; only the parent builds port/build/x64.
  modules.inc, tick.cpp kDrops / fallbacks and commands.cpp registrations are edited by the parent at merge (children send the lines).

## 1. The 46 stopped segments, by cause

| Cause | Segments | Stops in | Work | Stream |
|---|---|---|---|---|
| Text_put, no TextMetrics provider (U-0944, U-0947) | 17 | showPopup, showSystemMenu, demolishObject | width source, see section 2 | 38a |
| drawGolferInfoPanel (6.3 KB, C4) | 8 | the golfer info panel (P6) | port; it reaches skillPointDialog / buildGolferComment / computeStoryReply | 38d |
| showCourseReport (14 KB, C4) | 5 (+ ui_tour/00 twins) | P5 / H1 | port the effect (g_courseReportScore, g_flags, purse, postEvent) | 38c |
| shotStyleButtonCode (46 B, C2, ui) | 4 | H1 | port from the instructions (not a straddler: scope missed it) | 38b |
| loadGameBrowserScreen | 3 | cov_a_flags | GameFiles directory listing (U-0864, also unblocks loadGolferRoster) + loadGame / loadCourse | 38b |
| showHoleStatsScreen | 2 | P6 | clock-aware InputSource (U-0949): the trace's next clock value at a site | 38e |
| showEndOfYearScreen (5.7 KB, C3) | 2 | P5 | port the effect (input-independent) | 38c |
| showBulletinBoard (modal) | 2 | cov_b_holes18 | port; needs skillPointDialog and scrollTextWindow (drawScrollText layout over Text_put, U-0950) | 38d after 38a |
| showCharacterIntro | 2 | ui_tour/02 twins | skillPointDialog as a Blocking-pump command (U-0865) | 38d |
| courseActionMenu (5 KB) | 1 | switchToCourse | port; popups (Text_put) and loadCourse / loadGame | 38b after 38a |

The other 3.7b gaps that no segment reaches stay logged in re/port/commands_split.tsv for the UI phase; 3.8 ports only what a
segment or a gate scenario reaches (log/p3/p37b/seg_reach.py decides).

## 2. Text widths (the decision that unblocks the most)

Widths are GDI's: jgld Font::textWidth = GetTextExtentPoint32A on a CreateFontIndirectA font over the game's own font file
(Font::loadFile AddFontResourceA). Recommended:

1. **Host GDI provider** (Windows, test and phase 4 backend): register the font file from the install as Font::loadFile does,
   build the LOGFONT exactly as Font::create, run drawMarkupRun's token pass (c3o.cpp), measure with GetTextExtentPoint32A.
2. **Recorded widths as its oracle**: an opt-in recorder extension (trace_rec.js, side file `<scenario>.widths.tsv`: font,
   string, result; traces and pinned references unchanged) on the scenarios whose segments reach Text_put. The provider must
   equal every recorded width before a segment run uses it.
3. Portable widths without GDI (TrueType advances + GDI rounding at the font's ppem) are phase 4's problem: written down as an
   open question, not attempted.

## 3. Gate work (3.8 proper, stream 38f)

- **(c) end to end.** course_season's 10 segments are contiguous (frames 167 .. 6,294): chain them from segment 0's image through
  the concatenated traces with no restart in between, checking every frame record and all 10 checkpoints. Same for the story /
  mood scenarios (stories_fixture, stories9_fixture, mood5_fixture, golfer_stories) where their segments chain; where they do not,
  capture one long segment per scenario (segment_corpus.py, game run on an instance). Correction to the plan's wording: the
  season is 600 virtual s = about 6,100 frames = 0.75 game year (8,192 frames a year, time_model.md), not 6 years. To claim
  several years, record a longer course_season variant (about 50,000 frames).
- **(d) championship through the command API.** Translate the trace's raw handler records of championship/00..02 into semantic
  commands (3.7b kinds 0x100..0x106: shot style, aim, swing), run them with no RootHandler command, and require the same frame
  records. A raw record with no semantic translation is a reported gap, not a fallback.
- **(e) divergence ledger.** One table re/port/divergences.tsv: every segment not PASS with its cause (missing name, U-number,
  defect switch). The gate is 0 rows without a cause. Each defect switch gets one segment or state test where turning it OFF
  changes the result.
- **(f) speed.** Run 8,192 TIME-FORWARD frames from a course_season image with no input; must finish under 8,192 x 87 ms = 713 s
  (the original's frame wait), x64 and Win32. Expected far under; report frames per second.
- **Backends.** The full segment gate also on SG_BACKEND=win32 (x87 sites), results identical.

## 4. Streams (children on claude2, own build dirs, parent merges)

| Stream | Content | Model | Needs the game |
|---|---|---|---|
| 38a | text widths: GDI provider, recorder extension, widths check; then showPopup / showSystemMenu / textInputBox segments | opus | yes (recorder, one instance) |
| 38b | shotStyleButtonCode, GameFiles listing + loadGameBrowserScreen, loadGolferRoster; courseActionMenu once 38a lands | sonnet | no |
| 38c | showCourseReport, showEndOfYearScreen (cmdemu oracle + segments) | sonnet | no |
| 38d | skillPointDialog, showCharacterIntro, drawGolferInfoPanel; showBulletinBoard + drawScrollText layout after 38a | opus | no |
| 38e | clock-aware InputSource (U-0949), showHoleStatsScreen | opus | no |
| 38f | chained end-to-end runs, command-API championship, divergence ledger, speed check, Win32 gate | opus | only for new long captures |

Order: section 0, then 38a / 38b / 38c / 38e / 38f in parallel, 38d's first half in parallel and its second half after 38a.
Done when the COMMAND gate is 155 / 155 PASS (or each remaining one in the ledger with a cause the user accepted) and gates
(c) .. (f) hold.

## 5. Result (2026-10-20)

All six planned streams plus five that the segments made necessary (38g routing / shortcuts / membership screens, 38h
showCharacterEditor, 38i semantic command API, 38j mainLoop's title menu, 38k a test seam) are merged (log/p38/MERGES.md).
Measured by the parent on the merged tree (log/p38/final_cmd.tsv, final_tf.tsv, final_gate_d.tsv):

- COMMAND 165 / 165 PASS (155 of the original plan + 10 of course_season_6y), TIME-FORWARD 35 / 35, x64 = Win32 (38f, 38j).
- Gate (c): course_season and the multi-year course_season_6y (50,923 frames, 6.22 game years) pass as one run each; chains
  335 / 335 PASS.
- Gate (d): championship/00..02 PASS driven by semantic commands only; all 165 COMMAND segments do too (38i).
- Gate (e): re/port/divergences.tsv; one row without a cause, chooseShotTarget.strictFlagSticky (U-1012: unreached).
- Gate (f): 8,192 frames in 23 s (x64) / 29 s (Win32) against 713 s; a no-input year does not exist in the original (U-1010).
- Open: U-1000 (c3eo.cpp hook rating v / 10 vs the original's v / 20), Select A Theme and quit paths (U-1050), the bio entry of
  the character editor (U-1032), the scroll-text layout reached by no segment (oracle only).
