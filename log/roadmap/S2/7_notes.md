# S2.7 notes: the boot screens (showLoadingScreen, showThemePackSelect, selectDifficultyScreen, the championship course list,
pickAProScreen) on cov_r_s2_screens

Status: DONE. The six checkpoints are 0 px; U-1252 settled (an uninitialised stack word of the original, U-1260 asks the lead
for the default). All work is in the mirror `port/build/S2_7_head` (a robocopy of
`port/build/S2_4_head` without its junctions and builds, junctions log/, original/, port/corpus/, re/replay, re/versions,
shim/build, tests/fixtures, tests/golden re-created with PowerShell New-Item -ItemType Junction; HEAD_COMMIT c6d7d78b inside;
S2.6's re/tools/scenario_s2_6.py, s2_6_runs.py and port/test/test_s2_6_scenario.py copied from the working tree). boot.py is
S2.4's: the record-771 KNOWN entry the lead first asked for was refused by the orchestrator (STATUS.md), and every S2.7
checkpoint lies before record 771. Build x64 `port/build/S2_7` (log/roadmap/S2/build.py, log 7_build.log). Merge:
log/roadmap/S2/7_merge.md (new files + 7_shared.patch / 7_shared_wt.patch).

## Baseline (S2.4 DLL, the same mirror; 7_fg_base.tsv)

loading_t200 FAIL 479,810 px, loading_t300 FAIL 479,832, theme_packs FAIL 466,918, difficulty FAIL 459,260 (each screen drew
nothing: no gfx body, no unsupported call either), champ_courses PASS 0 (S2.4's load browser in championship mode), pick_a_pro
UNSUPPORTED 238,894 (the port's frame still held the course list).

## What the original does (sources cited per call in gfx_boot_screens.cpp's header)

- showLoadingScreen (c3fi.cpp ShowLoadingScreen_re): a Surface and an Image on the stack; r = Random::range(12): picture r
  (f_strpool_4c44ac + 0x1e4 - 0x2c r) loaded, cut (0x5a, 0xaf, 0x26c, 0xca, trim) and drawn at (0x5a, 0xaf); the text buffer
  emptied, r2 = Random::range(11): hint r2 appended (the eleven pool texts of the table the hook lists); drawMessageBuffer(0x32,
  0xc8, 0xf0, 0x80007fff) (c3dv.cpp: per-character Text_put widths, breaks at ' ', newline, '^', lines through drawTextSurface
  when the line height is above 0x10 and the colour white: setQuad(colour, 1, 0, 1) = a shadow run in run-palette colour 1 at
  (+0, +1)); a percent other than -1: the bar (two fillWindowRect, `Loading...` centred). No refreshFull.
- Who presents it: Setup draws the title art first (c3fy.cpp: TitleBase blit3, the seven items, five labels at ML+0x24e..0x2b4,
  no theme line), then showLoadingScreen(0) and refreshFull (ML+0x359 / ML+0x368). During loadInterfaceArt every 100th loadFlic
  (disassembly of loadFlic 0x0043d740: +0x34 showLoadingScreen(count / 4), +0x43 Window::refreshFull(root, 0), +0x4c
  waitTicks(10, 0), +0x54 pumpInput483bd0) draws and presents a new loading screen. The checkpoints loading_t200 / t300 are clock
  reads inside that waitTicks (trace: CLOCKRUN sites waitTicks+0x33 / +0x37 / +0x63 x 99 right before the POLL), so their frames
  show the loading screens of count 200 (percent 50: yellow x 400..499) and 300 (percent 75: x 450..499), measured on det_b.
- selectDifficultyScreen (c3en.cpp): setup (five cuts from TitleSelDiffMO, then TitleSelDiffUnSel); per pass: the redraw, the
  pump, the cursor, the choice, the back button's tip after 0x1e passes (drawPanelBackdrop over `Cancel`), refreshFull,
  waitTicks(2, 0).
- showThemePackSelect (c3fw.cpp): six FindFirstFileA probes per theme (flags), sheet cuts A (8) and B (4); per redraw the
  background, title, five column titles in g_font51b360, per row the current-theme highlight, the name and the column cells;
  hover and tip in the input loop.
- pickAProScreen (c3fw.cpp): loadGameBrowserScreen's frame layout with the pro sheet; per redraw the background, the disabled
  buttons while nothing is selected, the title, the .pro rows, the card of a selected pro.

## U-1252 settled (task: "settle whether the trace recorder or the port causes it")

showThemePackSelect reads its sixth column word ([esp+0x32]) for flag bit 5 but never writes it (c3fw.cpp TpCols: five words
stored at 0x004725cf..0x004725eb). The current theme's row therefore gets one more cell at x = w - 0x2b, with w whatever the
stack held. Measured:
- det_b vs t_rec: 2,170 px, bbox (0, 156)..(69, 186) = the Standard row (pen y 0x5c + 2 * 0x20 = 156, cell height 0x1f), x 0..69 =
  an 87-wide cell (0x57, A[4], measured at column 0: x 303..389) starting at x -17, so w = 0x2b - 17 = 0x1a in det_b
  (log/roadmap/S2/7_img/, u1252 crops in the scratchpad).
- Port with no column-5 cell (SG_GFX_THEMEPACK_COL5=none, 7_fg_col5none.tsv): t_rec 0 px, t_rep1 0 px, det_b 2,170 px with the
  same bbox. With w = -0x3334 (0xcccc, the debug fill, as the movsx reads it) the same. With w = 0x1a (the default): det_b 0 px.
Conclusion: neither the port nor the recorder's input: the original draws at an uninitialised stack word; the trace recorder's
own code on the main thread's stack changes what the word holds (the pinned runs: 0x1a, four of four: det_a, det_b, det_c, c_a
are equal; the recorder runs t_rec, t_rep1: a value that hides the cell). The port uses 0x1a so the census reference matches;
U-1260 records that the harness-free value is not measured and asks the lead for the default.

## What changed (mirror paths; hunks in 7_shared.patch)

- port/src/sim/gfx_boot_screens.cpp (new), family `boot_screens`, 13 bodies: showLoadingScreen.draw;
  selectDifficultyScreen.setup / .redraw / .tip / .teardown; showThemePackSelect.setup / .redraw / .hover / .tip / .teardown;
  pickAProScreen.setup / .textSurface / .teardown. Local helpers: draw_message (drawMessageBuffer over a text address, with
  drawTextSurface's shadow), the drawPanelBackdrop of the difficulty tip over `Cancel` (the core body reads the text buffer,
  which the port's select_difficulty_screen does not write), Surface_drawScaled at scale (1, 1, 1) drawn as Image_draw (the
  same path, c3df.cpp). Frame objects: gfx_local_key(0x70000 .. 0x73fff).
- port/src/sim/gfx_families.inc: `SG_GFX_FAMILY(boot_screens)`.
- port/src/sim/commands_screens.inc: show_loading_screen hands r, r2 to the body; select_difficulty_screen issues setup /
  per-pass redraw / tip (with a new dwell counter) / refreshFull / teardown.
- port/src/sim/menu.cpp: load_flic's refreshFull + waitTicks clock reads (boot path, backend attached); show_theme_pack_select
  and pick_a_pro_screen issue the drawing.
- port/src/sim/tick_boot.inc: Setup's title art (backend attached).
- port/test/test_gfx_boot_screens.py (new): frames, U-1252 mechanism, controls, unit tests.

Every new port line runs only with a graphics backend attached (`g_gfx_attached`), so the GameState without one is untouched.

## Controls (7_negctl_frames.log, test_gfx_boot_screens.py)

On the frames (px at the judged checkpoint): 1 loading_t200 22 (bar one pixel wider), 2 loading_t200 3,483 / t300 2,812 (no
hint shadow), 3 difficulty 3,197 (captions not lowered), 4 difficulty 474,842 (screen from the highlight sheet), 5 theme_packs
3,290 (column titles in the row font), 6 theme_packs 1,380 (characters probe skipped), 8 pick_a_pro 2,735 (disabled load button
not cut), 10 loading 378,616 (no title art), 11 loading 88,964 / 87,576 (no refreshFull after loadFlic's loading screen), 12
loading_t200 87,951 (wait reads not consumed: the snapshot slips to the next loading screen; t300 0), 13 pick_a_pro 576 (rows one
pixel right). Blind on the frames (no checkpoint has the cursor there), RED in unit tests: 7 (theme hover
images), 9 (difficulty tip y). 13 modes, 13 RED, 0 blind.

## Gate (logs in log/roadmap/S2/7_*)

| gate | result | log |
|---|---|---|
| cov_r_s2_screens loading_t200, loading_t300, theme_packs, difficulty, champ_courses, pick_a_pro | PASS, 0 px in every region (S2.4 DLL: 479,810 / 479,832 / 466,918 / 459,260 / 0 / 238,894); menu and title_back stay 0; framegate's boot verdict PASS (the drawing changes no sim hash) | 7_fg_dev.tsv, 7_fg_base.tsv |
| U-1252 | settled: 0 px against t_rec and t_rep1 with no column-5 cell (and with w = -0x3334), 2,170 px against det_b in its exact bbox; 0 px against det_b with w = 0x1a | 7_fg_col5none.tsv, test_gfx_boot_screens.py |
| census, 64 scenarios / 609 rows, against S2.4's 4_census_x64.tsv | same 545, better 54, worse 10; PASS 50 -> 69 (raw). Better: the 19 cov_a_quit / cov_a_quit_title title rows (NOT_RUN in 4_census: their traces came with S2.5 after it; equal to the lead's L_framegate_quit_s24.tsv) and 35 in-game / championship rows with fewer px. Worse: cov_a_quit's 5 in-game rows (NOT_RUN -> UNSUPPORTED: the same S2.5 traces; status and px equal to L_framegate_quit_s24.tsv, so not worse than S2.4 on the same corpus) and 5 cov_b_shots / cov_b_shots2 `results*` rows, UNSUPPORTED before and after with the same first unsupported call (Surface_stretchBlit with no sprite), +410 .. +516 px: the backend does not draw the tournament-results frame, the root still holds the last full screen it drew, now Pick A Pro (with the selected pro's card) instead of the course list (7_img/worse_S2_4 vs worse_S2_7: the two port frames differ in 209,059 px, both about 99.9 % wrong in that area: 208,263 vs 208,777). Same class as S2.4's 12 worse rows | 7_census_x64.tsv / .log, 7_census_compare.log, 7_worse_S2_4.tsv, 7_worse_S2_7.tsv |
| segments | 205 / 205 PASS x64 and Win32; compare with log/p5/segs_*_loc.tsv: 0 differ | 7_segs_x64*.log, 7_segs_win32*.log |
| boot | the 67 rows of S2.4's 4_boot_gate.tsv equal in status, records, expected (PASS 62, KNOWN 2, NO_FRAMES 1, FAIL 2; HEAD's 61: 58 / 2 / 1); extras since (U-1102): cov_a_quit PASS 301/301, cov_a_quit_title NO_FRAMES, cov_r_s2_screens FAIL at record 771 (U-1253, S2.6's) | 7_boot_gate.tsv, 7_boot_compare.log |
| test_gfx_boot_screens | 32 passed x64; Win32 29 passed, 3 skipped (the SG_GFX_THEMEPACK_COL5 tests: the out-of-process runner inherits its environment at spawn) | 7_test_x64.log, 7_test_w32.log |
| test_static, test_gfx, test_gfx_credits, test_gfx_routing, test_s2_6_scenario | 6, 25, 47, 35 passed, 3 passed + 2 xfailed x64; test_gfx 25, credits 47, routing 35 Win32 | 7_test_x64.log, 7_test_w32.log |
| test_commands, test_tick, test_sve_roundtrip (files touched feed them) | 83 passed / 44 skipped (S2.4: 83 / 44), 250, 15. The first test_commands run, under heavy machine load (several sibling suites), had 2 failures, one of them test_semantic_command_equals_the_click[clickAmenityPanel] (the other's name was cut by the log tail); the 7 semantic tests passed alone and the whole file passed on the re-run | 7_test_x64_b.log, 7_test_commands_rerun.log |

Build note: log/roadmap/S2/build.py --arch Win32 into the mirror's port/build/Win32 configured once with an empty CMAKE_CXX_FLAGS
(no /EHsc, C4530 as error); the directory was removed and `~/bin/py -3.12 port/test/build.py --arch Win32` (mirror root) built
it with the default flags (7_build_w32.log keeps the failed attempt). Cause not looked into.

## Reproduce (mirror root port/build/S2_7_head)

```
~/bin/py -3.12 <abs>/log/roadmap/S2/build.py <abs>/port/build/S2_7_head <abs>/port/build/S2_7 --log log/roadmap/S2/7_build.log
env SG_PORTSIM_DLL=<abs>/port/build/S2_7/Release/portsim_c.dll ~/bin/py -3.12 -m pytest port/test/test_gfx_boot_screens.py -q -p no:cacheprovider
env SG_PORTSIM_DLL=... ~/bin/py -3.12 -m port.test.framegate cov_r_s2_screens --checkpoints loading_t200,loading_t300,menu,theme_packs,difficulty,champ_courses,pick_a_pro,title_back --no-probe --out <tsv>
env SG_GFX_THEMEPACK_COL5=none SG_PORTSIM_DLL=... ~/bin/py -3.12 -m port.test.framegate cov_r_s2_screens --checkpoints theme_packs --no-probe --images <dir> --out <tsv>
env SG_PORTSIM_DLL=... ~/bin/py -3.12 -m port.test.framegate cov_r_s2_screens --checkpoints <...> --no-probe --negctl gfx.family.boot_screens=<mode> --out <tsv>
env SG_PORTSIM_DLL=... ~/bin/py -3.12 -m port.test.framegate --all --jobs 2 --no-probe --out <abs>/log/roadmap/S2/7_census_x64.tsv
~/bin/py -3.12 log/roadmap/S2/3_compare_fg.py log/roadmap/S2/4_census_x64.tsv log/roadmap/S2/7_census_x64.tsv
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --dll <dll> --jobs 4 --out <tsv>   (and --backend win32)
env SG_PORTSIM_DLL=... ~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out <abs>/log/roadmap/S2/7_boot_gate.tsv
```

## Open

- U-1260 (column-5 word default: the lead's decision), U-1261 (session loadFlic waits), U-1262 (the pro card), U-1263 (hover /
  tip states on no checkpoint). 7_uncertain.tsv.
- Cleanup after acceptance: the mirror port/build/S2_7_head and the build port/build/S2_7.
