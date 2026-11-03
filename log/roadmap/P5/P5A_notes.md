# P5A notes: in-game screens from Mariano's play-test (2026-11-03)

Tree port/build/P5A (base b89154c4). Every change keeps the default switches byte-identical: segments x64 205 / 205 with 0 digests
different from log/roadmap/merges/I25/segs_x64.tsv (P5A_segs_x64.tsv / .log). Builds x64 and Win32 rc 0 (P5A_build_x64.log,
P5A_build_win32.log). Files: P5A_files.txt. New play switches: P5A_profile.tsv (also appended to the tree's re/port/play_profile.tsv
and to port/app/main.cpp kPlayProfile, so the app's default `play` has them).

| item | class | what changed |
|---|---|---|
| 1 exit hover too big | b | switch `mainLoop.titleExitHotspotWide` (play 0) |
| 2 theme highlight cut on the left | b | switch `showThemePackSelect.column5StackWord` (play 0) |
| 3 world map highlight only after moving | a | live core + app host: the world map runs its timer passes with no input |
| 4 Course Report / Top 10 Designers do nothing | a | the two screens' drawing added (they made no drawing calls) |

## 1. Title menu: the exit's hover area (class b, the original's behaviour)

Cause, from the disassembly of mainLoop's title loop (0x00421055..0x004210b0, ML+0x11a95..ML+0x11af0): the loop takes the nearest
of six hotspot points, `distance(x - hx, (y - hy) * 4 - 0x18) < 0xa0` (start bound `mov ebp, 0xa0` at 0x00421059, the `lea ecx,
[eax*4 - 0x18]` at 0x0042107f, `call 0x40acd0` at 0x00421091, `cmp eax, ebp; jge` at 0x00421099). The hotspot points are Setup's
frame slots (tick_menu.inc MenuSeedSlots: x at F+0xf4, y at F+0xf6). For the five labelled buttons the point is the label centre;
for the exit (hotspot 5) it is (0x2d1, 0x210), the top-left corner of its 0x3a x 0x39 icon (showTitleScreen's seventh button cut,
c3fb.cpp kButtons row `{0x2d1, 0x210, 0x3a, 0x39, ...}`). So the exit highlights over an area about 0x140 px wide and 0x50 tall
centred on the icon's corner, most of it left of and above the icon. The port's tick_menu.inc is that code (log/p38/38j's conversion
of c3fy.cpp), so the port equals the original here.

Change: switch `mainLoop.titleExitHotspotWide` (tick.cpp kDefects, default 1 = the original). At 0 the exit hotspot is taken only
with the cursor inside its icon rectangle (x 0x2d1..0x30a, y 0x210..0x248), where it wins (d = 0); outside it never wins; the five
labelled hotspots keep their test. The `distance` call is still made for hotspot 5 (pure function).

Checked: cov_a_quit (its quit is a click at (750, 553), inside the icon) and cov_a_quit_title boot with the switch at 1 and at 0:
PASS 301 / 301 and NO_FRAMES both times, as at HEAD (P5A_exit_boot.py). Not separately frame-tested (light testing per G30).

## 2. Theme pack screen: the highlight `cut off on the left` (class b)

Cause: U-1252 / U-1260 (log/roadmap/S2/7_notes.md, gfx_boot_screens.cpp COLUMN 5). showThemePackSelect reads a sixth column x word
for flag bit 5 that it never writes (c3fw.cpp TpCols: five words stored at 0x004725cf..0x004725eb), and on the current theme's row
a clear bit draws the yellow cell A[4] at x = w - 0x2b. The pinned original runs (det_a / det_b / det_c / c_a of cov_r_s2_screens)
hold w = 0x1a: a 0x57-wide yellow cell over x 0..69 of the Standard row, left of the list panel, with the panel's white border
between it and the real row highlight. That is what Mariano's crop shows (yellow, a white line, then `Standard`); the port draws
w = 0x1a to match those references (theme_packs checkpoint 0 px). So it is the original's own picture.

U-1260 is still open: I tried to measure the original with no scenario agent (P5A_theme_probe.py: re/tools/game.py on
instances\p5a, harness.js only). The harness's virtual cursor did not reach the title menu's hover in those runs, so the click took
the menu's initial selection 1 (Start New Game: the selection word starts at 1 until a hover, 0x0042105e..0x0042106c) and reached
the difficulty screen in all six runs (P5A_img/). The harness-free value of w is therefore still not measured. The six game
processes those runs left in instances\p5a were stopped by PID afterwards (game.py's context manager had not ended them).

Change: switch `showThemePackSelect.column5StackWord` (gfx.cpp gets a kDefects table; the flag lives in gfx_boot_screens.cpp,
default 1 = the cell at w = 0x1a as before). At 0 no column-5 cell is drawn and nothing is reported unsupported. Drawing only: no
GameState byte depends on it. test_gfx_boot_screens.py 32 passed at the default (P5A_t_test_gfx_boot_screens.log).

## 3. World map (courseActionMenu): the site highlight only after moving the mouse (class a, port bug)

What the original does: its loop never waits for input. Per pass (c3fw.cpp CourseActionMenu_re): draw with the nearest site of the
previous pass, View_toLocal (0x0047015e), the buttons, Window::refreshFull (0x004703ae), waitTicks(5, 0) (0x004703ba), View_toLocal
again (0x004703ca), the nearest site within 0x1e (0x004703e3..0x00470461), pumpMessages (0x00470463), and around again when no
button is down. So with the cursor still on a site, the second pass (50 ms later) draws that site's wide plate and its text, and the
map pin blinks (bit 3 of the pass counter).

Why the port did not: the port's loop is the same (saveload.cpp course_action_menu), but its pump is PumpKind::Blocking and the app's
host answers a modal Blocking pump either with input or, at an `idle_ok` call, with an idle pass. The live core (live.cpp batch)
marked every courseActionMenu pump `must_deliver`: its `later` test models the next pump call at the SAME cursor count, while this
loop reads the cursor twice before every pump. So the host waited for a click (a WM_MOUSEMOVE gives no command), and the loop ran
no pass: the plate drawn on entry (nearest = -1) stayed until the next click.

Change:
- `InputSource::timed_pass(ms)` (commands.h, beside pace()): a modal loop names its original pass period for its NEXT Blocking pump
  call. course_action_menu calls `timed_pass(50)` before its pump.
- live.cpp: with the hint, `later` is true (a dispatch recorded at the next call has a higher cursor count, so TraceInput passes
  this call for it: `j.count < count`, not stuck), so the call is `idle_ok`; `LivePump::timed_pass_ms` carries the period.
- host.cpp: an idle_ok call with timed_pass_ms declines once per max(pass_ms, period) whether or not anything moved, i.e. the
  loop's timer passes. App option `--no-timed-passes` (HostOptions::timed_passes, default on) restores the old wait.
- The trace recorder writes nothing new: a decline at an idle_ok call is recorded exactly as the title menu's idle passes (a
  separator and the passes' CURSOR records), and trace-driven sources ignore the hint.

Checked (P5A_app_check.py, sessions recorded to a scratch directory, never port/corpus/app; P5A_app_check.log): title -> Sandbox ->
one WM_MOUSEMOVE onto Monterey's site (270, 32) -> 2 s without input -> back arrow.
- With the change: 118 idle passes, the presents after the move alternate between two frames (the pin blink), and the frame
  rebuilt from the recording at the first of them (P5A_frames/p5a_worldmap_3543382.png) shows Monterey's wide plate and its text
  `Buy 250 acres of rolling coastal parkland property`; the frame before the move (p5a_worldmap_3543322.png) shows none. The
  recording replays through sg_run_boot: PASS.
- With `--no-timed-passes` (the old behaviour): 45 idle passes (all on the title menu), no present after the move until the back
  click's dispatch. Replay PASS.
- test_live.py (the live core and its recorder controls): 38 passed.
Other modal loops that read the cursor before their pump and have a timer pass in the original (the theme / difficulty / pick-a-pro
screens read it after their pump, as the title menu, and already get idle passes) were not changed.

## 4. `Course Report` and `Top 10 Designers` do nothing (class a, port bug)

Which menu: the in-game Information button's popup (showSystemMenu spot 3, OptionsMenuBuild 0x00432720 in c3fb.cpp). Its text is
f_strpool_4c7d80 + f_strpool_4c7d00: 0 Repeat Last Message, 1 Course Report, 2 Player Comments, ... 10 Best Scores, 11 Top 10
Designers. Answer 1 returns -0x70 (0x004327b4) = the F1 key: H1 toggles g_ui_569498 to 2 (ML+0xe888) and ML+0xead5 calls
showCourseReport(0), the per-hole table. Answer 11 calls drawScoreSummary 0x00473470 directly (0x00432886).

Why the port did nothing: both ran their simulation part and their wait, but made no drawing call at all. reports.cpp said `mode
0's table draws nothing` (only modes 1 / 2, the association's grading page, had S2.9's drawing), and commands_menus.inc's
draw_score_summary had no drawing (it is also what the end-of-year screen shows after its loop, reports.cpp addScore ->
drawScoreSummary). With the gfx backend attached the player saw the course unchanged and the next click left the wait.

Change (drawing only, made with an attached backend; texts the original builds in g_textBuffer are built there and the buffers put
back, as S2.8 / S2.9 do, so the GameState with a backend equals the one without):
- reports.cpp ReportDraw: table_head / table_row / table_subtotal / table_foot, transcribing c3fr.cpp ShowCourseReport_re's mode-0
  calls (0x0044fb70..0x00453128): the dim sprite and Display::setScale, the head g_spriteRef_821308[0], per hole a row band [2]
  with the hole name, length, par, average, time, pace, the three +Len / +Acc / +Img cents, the type text (f_ptrtab_4c2a18 + 0x10),
  fee, revenue and profit with their icons (cells 8.., 0x19.., 0x1c.., 0x1f.., 0x28.., 0x31 of the same SpriteRef array; icon x from
  f_tab_layoutInts_4ba62c dwords 3..11 in the user's exe), the subtotal band [4] after hole 18 (its time minutes are the last hole's,
  as the original, 0x00450b60; its third percentage icon never drawn, the `jle` at 0x00450e13), the foot [6] with the legend and the
  twelve column titles (f_ptrtab_4d2054 + 0x24, x from 0x004d20ac on), and the OK button at (0x2de, foot text line + 0xf) with its
  hover. Every cell index, string offset and table index was checked against the hook's addresses.
- commands_menus.inc draw_score_summary: c3er.cpp DrawScoreSummary_re's calls at their places in the row loop: a frame Image
  (gfx_local_key(0x7c000)) loads the background (f_strpool_4e4130 + 0x5c) and copies it onto the root, loads the cell sheet (+0x40);
  per row its cell (f_tab_dwords_4e3e00), the name three times (with Text_put's width underline for the row addScore marked new, its
  word at g_scrollTextWin + 0x58), the cash / skill / fun lines, the total three times, the multiplier line, row 0's own text; then
  Window::refreshFull, the wait, Image_dtor.

Checked:
- ui_tour replayed by the port with the backend (P5A_frames.py, boot PASS): snapshots inside the F1 report (clock position 0x36000,
  between its key 0x3552e and its Esc 0x36137) and inside Top 10 Designers (0x4d500, after its click 0x4c9e2):
  P5A_frames/f1_table.png (the COURSE REPORT table: Hole 1, Total, legend, column titles, OK) and P5A_frames/top10.png (the TOP TEN
  DESIGNERS trophies with names, totals and lines). No pinned original checkpoint lies inside either screen, so these are not
  pixel-compared with the original (a pinned capture inside them would be the next step).
- In the app: app_replay's `report` script (F1..F4) recorded with this build and replayed: PASS 203 / 203.
- Segments unchanged (0 digests) and test_reports.py / test_commands.py: see the last section.

## Tests run (one file per process, w8_lock + test-slot)

| file | result | log |
|---|---|---|
| test_profiles.py | 21 passed | P5A_t_test_profiles.log |
| test_gfx_boot_screens.py | 32 passed | P5A_t_test_gfx_boot_screens.log |
| test_reports.py | 14 passed, 2 skipped | P5A_t_test_reports.log |
| test_commands.py | 83 passed, 44 skipped | P5A_t_test_commands.log |
| test_live.py | 38 passed, 1 deselected | P5A_t_test_live.log |
| segments x64 | 205 / 205, 0 status / digest differences against I25 | P5A_segs_x64.tsv |

## Open

- U-1260 (the original's harness-free column-5 word) stays unmeasured; the probe script needs a cursor path that reaches the title
  menu's hover (the scenario tooling's SetVirtualCursor from the main thread does).
- The F1 table and Top 10 page have no pinned reference frame; a capture of the original inside each (ui_tour already presses F1
  and opens Top 10) would let framegate judge them.
