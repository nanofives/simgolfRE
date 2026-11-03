# P4A: money / reports and golfers / stories fix switches (G30)

Tree port/build/P4A (git archive of 7f7860c5). Seven switches, one per row, each an `int` flag defaulting to 1 = the
original; 0 = the fix. Every behaviour change sits inside a `SG_DEFECT(...)` arm; at 1 the code path is the old one
statement for statement. Switch names follow `<function>.<camelCaseWhat>`; where re/port/defect_decisions.tsv proposed a
name that is not a function, the row's `switch` column needs the new name:

| row | defect_decisions.tsv name | implemented name | module |
|---|---|---|---|
| D17 | namedStoryIndex.stride | loadGolferRoster.namedStoryStride | loaders |
| D19 | loadGolferRoster.skillsNextRecord | (same) | loaders |
| D23 | finance.lowWordOnly | mainLoop.cashLowWordOnly | tick |
| D24 | showFinancialReport.sumWriteBack | (same) | commands |
| D26 | tileMood.wrap | emitGolferThought.tileMoodWrap | events |
| D44 | resetGolferState.singleHoleRecord | (same) | golfers |
| D48b | computeStoryReply.clampLowerBound | (same) | events |

The loaders, tick and commands modules had no DefectDef table; each now has one (ModuleDef's defects / defect_count).

## D23 mainLoop.cashLowWordOnly (port/src/sim/tick.cpp, tick_frame.inc, tick_menu.inc, course.cpp)
Sites: the month cash history g_cashHistory (int16[500]) gets `mov ax, word [g_cash_div100]` at mainLoop 0x00417995 (store
0x004179b4) and again at 0x00417cb3 (store 0x00417cce); the new-course path copies the low word into the year-0 ledger entry
`other` (+0xe) at 0x0040fc16 / 0x0040fc1d. switchToCourse 0x00407dc3 makes the same low-word store into
g_financeYears[g_year % 100].other (course.cpp switch_to_course_chosen); it is the same bug, so the same switch covers it
(course.cpp declares `extern int g_defect_cash_low_word`).
Fix: the value stored is g_cash_div100 saturated to -32768..32767 (helper `cash_word` in tick.cpp).
Design choice (conservative): the brief asked to keep the full value. Both destinations are int16 tables of the exe's .data
image (GameState is that image with static_asserts on every offset; the save writes g_cashHistory 0x3e8 bytes and
g_financeYears 0x7d0 bytes as such, saveload_fields.inc ser 33 / 44). Keeping 32 bits would need new storage outside
GameState, which changes the segment image size and every reader (showHistographScreen, showEndOfYearScreen, the financial
report, reports.cpp). Saturation keeps the layout and the save format unchanged, and above $3,276,700 the chart and the
ledger show the cap instead of a jump to a negative value. Save format: unchanged.

## D24 showFinancialReport.sumWriteBack (port/src/sim/commands.cpp, commands_screens.inc)
Site: showFinancialReport 0x0044f8f0..0x0044f911 stores the int16 sum of entries 0..7 into +0x10 of every drawn year row.
Fix: the sum is computed and drawn as before (the same appendNumber calls, so g_textBuf is unchanged), the row's +0x10 is
not written; the total cell's negative-colour test uses the int16 of the sum instead of the stored word (equal to it at 1).
Save format: unchanged (+0x10 keeps whatever it held: 0 after the clear in loadInterfaceArt, or an older save's sum).

## D19 loadGolferRoster.skillsNextRecord (port/src/sim/loaders.cpp)
Sites: in the theme .pro loop the record pointer advances by 0x38 before the skill stores (0x0046630b, 0x0046633b, 0x00466354),
so the ten skill bytes and their sum land in pro record n + 1 (the last one past the table).
Fix: they are stored into the record being loaded (the same body as negctl mode 4, which stays a negative control of the
switch-1 path). Save format: g_golfPros is saved as before; its content now holds each .pro golfer's own skills.

## D17 loadGolferRoster.namedStoryStride (port/src/sim/loaders.cpp)
Sites: the opening-day story search walks the 0x64-byte file-name table (`add esi, 0x64` 0x00465ae8) and stores the slot number
k into g_namedStoryIndex (0x00465af6); its readers use it in the 50-byte story-id unit of g_storyDefs: pairGolfersWithStory
compares it with a story id (0x0045e012) and stores it into golfer +0xb0 (0x0045e6f8).
Which unit: g_storyDefs is the first 5000 bytes of the file table copied as 100 x 50 (rep movsd 0x4e2 dwords,
0x00465abe..0x00465acd), every reader indexes it with a 50-byte stride (0x0045e001..0x0045e007, 0x00466c7d..0x00466c90), and
golfer +0xb0 holds story ids in that unit, so file k is story id 2k (model_b.md 4.6). The fix keeps the readers' unit and
converts at the one writer: g_namedStoryIndex = 2k. A match in slot k >= 50 is a file not copied into g_storyDefs (no story id
names it), so the index is left unchanged, as when no slot matches. Save format: unchanged; golfers of a fixed run hold even
ids where the original stores the slot number.

## D26 emitGolferThought.tileMoodWrap (port/src/sim/events.cpp, events_think.inc)
Sites: `inc cl` on the byte g_tileMoodUp[tile] (0x004687fd..0x0046880d) and g_tileMoodDown[tile] (0x0046882b..0x0046883b) with
no test. Fix: a byte at 255 is not incremented. Save format: unchanged.

## D44 resetGolferState.singleHoleRecord (port/src/sim/golfers.cpp, golfers_pro.inc)
Sites: the 18-iteration loop (edi = 0x12, 0x0046d124 .. `dec edi; jne` 0x0046d1dc) indexes one hole record,
g_holesPlusOne * 0x208 (0x0046d11d..0x0046d129), every pass, and applies the four thresholds twice (0x0046d172..0x0046d1a1
and 0x0046d1a7..0x0046d1d6). Fix: pass n works on hole record n (hole n is record n, record 0 the swap slot: model notes of
phase 1), so records 18..1 each get their par once; the thresholds run once (the second application changes nothing, as v is
unchanged between them). Save format: unchanged (hole records now carry re-derived pars).

## D48b computeStoryReply.clampLowerBound (port/src/sim/events.cpp, events_story.inc)
Site: `movsx eax, word [ecx + 0x57956c]` (+0xb4 story counter) at 0x00466a73 and `movsx ecx, word [ecx + 0x57955c]` (+0xa4
mood) at 0x00466a7a, pushed as clamp(mood, counter, 0x63) at 0x00466a81..0x00466a85 (clamp 0x00467130 is clamp(x, lo, hi)).
Fix as briefed: clamp(mood, 0, 0x63).
What the code does and does not show about the intent: of the six other clamp calls with the bound 0x63 in the exe, three push
0 as the lower bound (0x00413f94, 0x00414bd3, 0x0044f79f) and three push a field + 1 (0x0040fc97..0x0040fc98, 0x0040fcbb..0x0040fcbe,
0x0042f42d..0x0042f430), so a non-zero lower bound is not by itself out of place here; the mood field is written clamped to -10..10 by
emitGolferThought (0x0046868c..0x004686a7, golfer_record.md +0xa4), so clamp(mood, 0, 0x63) gives 0..10 and the chapter
(c - 1) / 2 gives 0..4; the story counter is written as c * 7 / 8 + mood by matchUpdate (0x00428892..0x004288e3), a running
value the 0..0x63 bound also fits. The code does not determine whether the author meant clamp(mood, 0, 0x63) or
clamp(counter, 0, 0x63); this switch implements the briefed form. Effect at 0 (measured by the unit test below): mood 5,
counter 40, key 100 gives chapter 2 where the original gives 19. Chapters above 4 are then reached only through the
holeQuadrants arm (v = key). Flagged for the orchestrator: the alternative clamp(counter, 0, 0x63) equals the original whenever
counter >= mood and counter <= 0x63.

## Checks (final source, 2026-11-03)
- Build x64 and Win32: rc 0, no error or warning lines (P4A_build_x64.log, P4A_build_win32.log).
- Segments gate x64 at the default switches: 205 / 205 PASS, status and digest equal to log/roadmap/merges/I25/segs_x64.tsv on
  every row, 0 rows differing (P4A_segs_x64.tsv / .log).
- port/test/test_static.py 6 / 6 (the port code cites no original address: the switch comments name functions and ML+ offsets;
  the addresses are in this file) and port/test/test_defects_p4a.py 3 / 3 on x64 (P4A_unit_x64.log): all seven switches
  registered with default 1; resetGolferState at 1 leaves holes 3 / 5 at par 1 (only g_holesPlusOne's record 7 is visited), at 0
  gives them 6 / 2; computeStoryReply with mood 5, counter 40, key 100 gives chapter 19 at 1 and 2 at 0.
- Not run: the Win32 backend's tests, the boot gate, the app.
- Not exercised by a test at 0: D17, D19 (need theme files), D23 (needs a month change), D24 (modal report), D26 (needs the
  thought path at one tile 256 times). Their 0 arms were checked by reading the code only.
