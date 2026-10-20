# Stream 38b notes (phase 3.8): shotStyleButtonCode, GameFiles listing, loaders, saved games, the load browser, courseActionMenu

Everything is built and run in a private mirror (`log/p38/b_mirror.py --build`: HEAD's port/ + this stream's files + a modules.inc
with the three new modules before `tick`; DLL in port/build/p38b_run) and tested with `log/p38/b_pytest.sh` (sets
SG_PORTSIM_DLL; never port/build/x64). Merge lines: `log/p38/38b_merge.md`. U-rows: `log/p38/38b_uncertain.tsv` (U-0970..U-0978).

## Segments (COMMAND gate, log/p3/p37a_cmdsegs.py with the mirror DLL, `log/p38/38b_segs_all2.tsv`)

| Segment | Before | Now | What unblocked it |
|---|---|---|---|
| cov_a_career/00, cov_a_champ/02, cov_a_keys/04, cov_b_decor/01 | stopped in shotStyleButtonCode | PASS | shotStyleButtonCode |
| cov_a_flags/01, 03, 04 | stopped in loadGameBrowserScreen | PASS | the browser + saveGame / loadGame / refreshTerrain + the install's file list |
| cov_a_keys/01 | stopped in courseActionMenu (via switchToCourse) | PASS | courseActionMenu + tierPrice |

Whole gate with the mirror DLL (log/p38/38b_segs_all2.tsv; three transient MemoryError rows of a run made while an oracle suite
ran in parallel were re-run alone, log/p38/38b_segs_err.tsv): 117 of 155 PASS (109 before), 38 FAIL, 0 regressions against
log/p3/p37a_cmdsegs.tsv. The 38 left stop in other streams' functions: Text_put 17, drawGolferInfoPanel 8, showCourseReport 5,
showHoleStatsScreen 2, showEndOfYearScreen 2, showBulletinBoard 2, showCharacterIntro 2.

## 1. shotStyleButtonCode 0x0040d760 (module `shotstyle`)

Written from the 15 instructions: argument - 3 is compared unsigned with 4 and indexes the five-slot table: 3 -> 0x800043f0,
4 -> 0, 5 -> 0x8000421f, 6 -> 0x80007ff0, 7 -> 0x80007fff, anything else 0. Evidence (test_shotstyle.py): the emulator oracle over
37 arguments (every number around the table, the unsigned wrap, extremes) equals the port and covers the `ja` both ways; pinned
table; two controls (4/5 swapped, range test idx > 3) read RED.

## 2. GameFiles listing (U-0864), listMatchingFiles, loadGolferRoster

- `GameFiles::list(pattern)` (FindFirstFileA walk: `*`, `?`, "*.*", case-insensitive, "." / ".." first then upper-cased order),
  `write`, `writable`, `remove`; Memory and Dir implementations. `DirGameFiles` keeps writes and deletes in an in-memory OVERLAY
  over the disk (the install is never written) unless `set_write_through` / `SG_FILES_WRITE_THROUGH=1`. `sg_files_put` with a
  root attached joins that overlay (harnesses add the fixture files a recording's install held). C++ unit tests in core_test.cpp.
  **test_listing_equals_windows**: 13 patterns over the install's Themes plus synthetic names (mixed case, '&', digits, a
  subdirectory, 100+ files) against WINDOWS' OWN FindFirstFileA on a real directory, through the port function with a Memory and a
  Dir GameFiles: equal (U-0970: 8.3 short names not modelled).
- `listMatchingFiles` 0x0043d2a0 (module `loaders`): 21 oracle cases incl. 105 and 130 matches past the 100-slot table, equal
  to the original (eax, whole data image) under Unicorn with Windows' listing behind the FindFirstFileA import stand-in.
- `loadGolferRoster` 0x004659a0: 29 oracle cases (all themes of the install, fallbacks, the flag bit, celebrity / pro files with
  CRLF, LF, Ctrl-Z, 100+ lines, short lines, '*' lines, lower / upper / odd skill characters, .pro golfers appended after 99 / 100
  records, a strtok NULL fault, both files missing -> the first scrollTextWindow call, compared as the data image at that call):
  equal. Controls: 11 wrong bodies (4 listMatchingFiles, 7 roster) all read RED against the pinned digests (the first
  version of control 4 was an equivalent mutant because the install's .pro files hold all-zero skills: fixtures now carry
  non-zero skill bytes).

## 3. Saved games, course files, browser (module `saveload`)

- Format: re/model/sve_fields.tsv (the G2 table) equals serializeCourse's 86 rows in the C3 source (generator
  `log/p38/gen_save_fields.py` checks it: 0 mismatches) -> `saveload_fields.inc` (offset into GameState + size).
  Independent check (test_saveload.py): the user's 5 real saves load into exactly the ranges the G2 table names, and a load then a
  save writes the body back byte for byte (length equal, 0x64-byte header differs by design).
- `saveGame` 0x0040b4a0 (supersedes tick's placeholder: same control numbering), `loadGame` 0x0040b9b0, `loadCourse` 0x0040b840,
  `refreshTerrain` 0x00449520 (rebuildTerrainGrid + the repaint byte; U-0972), tierPrice, `loadGameBrowserScreen` 0x0043b610,
  `courseActionMenu` 0x0046f550. Files go through GameFiles (the descriptor is 4 / -1: U-0971). 31 oracle cases for
  save / load / loadCourse (written bytes, peers' calls, whole data image) plus 6 refreshTerrain: equal.
- Browser oracle (test_browser.py): 26 scenarios (load / cancel / Esc / other keys / scroll / click below the list / delete /
  championship / theme reload / empty directory / button edge) with the original under Unicorn (drawing dropped, one input
  batch per pump, scripted pointer, files from a dictionary + Windows' listing, terrain / window callees recorded): equal on eax,
  data image, recorded calls and the files at the end. Course-menu oracle (test_course_menu.py): 22 scenarios incl. every sound
  arm of a purchase, no cash (the warning waits for a key), cheat level, seed, save dialog, load through the browser, the
  nearest-site boundary at 29 / 30, 23 idle hover iterations (the tips).
- THE SEGMENT FINDING: the original's recording of cov_a_flags listed EIGHT saved games (the pristine five + ztut / zdemo / zedit,
  mood5.sve with one flag bit, copied in by log/c3/c3fya_run.py): the click on row 7 selected ztut.sve and loaded it. With the
  pristine five the port's browser found no selection (41 objects diverged at record 2). `port/test/segment_files.py` derives the
  three files from the install (byte-equal to log/c3/c3fya/fixtures when present) and SegmentRunner attaches them (U-0974).
  Then one frame later g_tileWallMask differed: the original's loadGame ends with refreshTerrain, whose rebuildTerrainGrid + the
  byte [0x820f2a] = 1 make the next updateTerrainRender repaint (and write bit 1 of g_tileWallMask[0]); mainLoop's own calls
  of it are dropped by tick, so only loadGame needed the real one.

## 4. Controls and evidence summary

Default suite (no game, no oracle): pinned digests of the oracle's results (port/test/golden_38b.json, regenerated with
`SG_EMU_ORACLE=update`, which first requires oracle == port for every case) for listMatchingFiles, loadGolferRoster, saveload,
browser, courseActionMenu and shotStyleButtonCode; 7 + 11 + 14 + 8 + 3 = 43 negative controls, all RED against those digests;
tierPrice is judged by the 27 corpus vectors (GREEN) with 2 controls RED. Final run with the oracle on (`SG_EMU_ORACLE=update`: every case oracle == port before pinning): 198 passed in 140 s
(log/p38/38b_oracle_final.log); default mode (no oracle): the same files minus the opt-in oracle cases, all pass.

## 5. Files

Own: port/src/sim/{shotstyle,loaders,saveload}.cpp, saveload_fields.inc (generated), the three headers, tests
test_{shotstyle,loaders,saveload,browser,course_menu}.py, emu38b.py (the oracle helpers: CRT / low-level file stand-ins, Windows'
listing, recorded peers), segment_files.py, golden_38b.json, log/p38/*. Edited shared files (all additive, listed in
38b_merge.md): gamefiles.h/.cpp, portsim_c.cpp (sg_files_put), core_test.cpp, cmdemu.py (stub_py, import_handlers), segments.py.

## 6. Open items

- U-0970..U-0978 (log/p38/38b_uncertain.tsv). Browser tips and idle redraws are not covered by the oracle's pump (U-0975), the
  course menu's idle tips are (key steps, one iteration each).
- Win32 backend: not run (nothing here uses x87).
- test_static's "every module file is listed" fails in the mirror only for modules the parent has not merged (other streams' too).
- `showPopup` (browser delete) and `textInputBox` (menu save) are called through the registry: with 38a's TextMetrics provider
  they run for real; the oracle records them (answer 0 / no text).
- Findings in the original: loadGolferRoster's pro / celebrity readers fault (strtok NULL -> strcpy) on a line with too few
  fields and drop the last character of a final line without a newline; listMatchingFiles' frame buffer overruns for a name of
  100+ characters; loadGame calls rebuildTileCoverage with no argument (it takes none); the browser's autosave title reads leave
  g_saveFd at 4; the courseActionMenu site table writes outside its 16 slots for a site byte >= 16 (the port ignores those
  writes), and its blink / dwell / previous-hot slots start uninitialised (U-0978: the port starts them at 0).

## 7. Round 2 (parent's follow-up): saveCourse 0x0040b680, saveGameDialog 0x00405b10, promptAndSaveFile 0x00437910

All three are in `saveload` (none was in the round-1 work). With 38a's TextMetrics (module gditext) merged, built from the working
tree (`log/p38/b_mirror_wt.py`): ui_tour/01, ui_tour/05, ui_tour_v/01, ui_tour_v/05 PASS; cov_a_keys2/03 now passes saveCourse and
stops in drawGolferInfoPanel (38d's). Full gate: log/p38/38b_segs_r2_all.tsv (see the final report).
- saveCourse: written from the instructions (no C3 source): two 0x64-byte header lines, deserializeCourse's 57 fields written, the
  8-byte marker; setPendingCourseInfo through course.h.
- saveGameDialog: c3fa.cpp's transcription; textInputBox / showPopup / startMessage through the registry (recorded in the oracle).
- promptAndSaveFile: c3ep.cpp's transcription; the portrait that follows the prefix is the renderer's (U-0979).
- Evidence: test_save_dialogs.py, 28 oracle scenarios (default / custom / padded / illegal titles, negative and huge dates, existing
  files, every mode and index shape, championship, bad / empty / padded golfer names), 5 controls RED. Each of the other test files
  of this stream was re-run alone after the emu38b change and passes (loaders 55, saveload 63, browser 35, course menu + shotstyle 36).
- Full gate with the working tree's port + `saveload` (log/p38/38b_segs_r2_all.tsv, jobs 2): 136 of 155 PASS, no regression against
  my round-1 gate. The 19 left: drawGolferInfoPanel 9 (incl. cov_a_keys2/03), showBulletinBoard 2, showRoutingMapScreen 2,
  showCharacterIntro 2, and four that were "Text_put" stops before 38a's provider and now diverge at record 18-19 in
  showSystemMenu ("the frame left to the title menu": cov_b_career_types/00, 01, 02 and cov_k1_animals/00). They fail identically
  with this stream's three modules left out of the build (checked), so they belong to showSystemMenu / the text widths, not to the
  save code.
