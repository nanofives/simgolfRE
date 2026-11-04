# P9A: the shell and the menus (Mariano's third play-test, items 1, 2, 3 and 5; 2026-11-03)

Tree: port/build/P9A (git archive of 2fd3553c plus junctions). Nothing committed. Files: P9A_files.txt. Patch against HEAD:
P9A.patch (`git apply --check` passes on the main tree). Profile rows: P9A_profile.tsv (8 `play` rows, also appended to
re/port/play_profile.tsv and kPlayProfile in port/app/main.cpp). Screenshots: P9A_shots/. Every switch defaults to 1 (the
original); `original` and `fixed` are unchanged (segments 205 / 205 equal to I25).

| item | class | change | default | off form |
|---|---|---|---|---|
| 1 no console | app build | simgolf_port.exe is a WIN32-subsystem program; simgolf_port_console.exe is the console twin | new exe layout | run simgolf_port_console.exe |
| 2 Esc goes back | 6 `play` switches | Esc acts as the screen's Back / Done / Exit / close / OK click | `play`: 0 | `--profile fixed` (or each switch at 1) |
| 3 first-frame hover | 2 `play` switches (both causes are the original's) | no title item lit before the cursor nears one; a title item only over its art | `play`: 0 | `--profile fixed` |
| 5 band streaks | app option | `--wide-fill edge` (new default) | edge | `--wide-fill soft` (P7's), or SG_APP_P9=off |

## 1. Opening without a console

**Decision: two executables from one set of objects.** `simgolf_port.exe` (the player's, the name every launcher already uses)
is linked for the WIN32 subsystem with `/ENTRY:mainCRTStartup`, so `main()` stays the entry and no console window opens from a
shortcut, Explorer or `start`. `simgolf_port_console.exe` is the same objects linked for the console subsystem, for a terminal
that wants to wait for the process. port/app/CMakeLists.txt: an OBJECT library `simgolf_port_objs` (main.cpp, window.cpp,
present.cpp, pacing.cpp, saves.cpp, host.cpp, stdio_log.cpp) and the two executables; the `simgolf_port` target depends on the
console one, so every existing `--target simgolf_port` build (test_app_shell / test_app_live / test_app_ingame) makes both.
freetype.dll is copied beside both (same directory).

**stdout / stderr (port/app/stdio_log.{h,cpp}, called first thing in main(), with `--data` read ahead of the parser):**
- stdout is a pipe, a file or NUL: nothing changes. Every test and tool starts the app with subprocess pipes
  (port/app/tools/app_drive.py `launch`: stdout=PIPE, stderr=PIPE, CREATE_NO_WINDOW; port/test/app_replay.py through it;
  test_gl_wgl_selfcheck / test_app_audio / test_defects_golfsfx through subprocess.run with capture), so the READY line and
  every message reach them as before, and no log file is written into their data directories (check 13 below).
- no handle (Explorer, a shortcut, a terminal starting the WIN32 build) or a console (the console build in a terminal): both go to
  `<data>/simgolf_port.log` (the previous run's kept as `simgolf_port.prev.log`) and to the console: the one the process was
  given, else the parent's (`AttachConsole(ATTACH_PARENT_PROCESS)`). One pipe and a copy thread feed both; the CRT streams are
  first reopened on NUL (a WIN32 process starts with descriptors 1 / 2 that have no handle: without the reopen every write was
  lost, measured), then pointed at the pipe; an atexit handler flushes, closes the write ends and joins the thread.
- `SG_APP_STDIO=keep` disables it. A terminal starting the WIN32 build gets its prompt back at once (the shell does not wait for a
  GUI program), so the app's lines appear after the prompt: use simgolf_port_console.exe there.

**Evidence** (`~/bin/py -3.12 log/roadmap/P9/P9A_stdio_check.py <Release dir> <scratch>`, P9A_stdio_check.tsv): 13 / 13 PASS on the
final x64 build: PE subsystem 2 / 3; pipes: `--profile-list play` (46 lines) and an unknown-argument error equal through both
exes; no handles (STARTF_USESTDHANDLES with NULL handles + DETACHED_PROCESS): the log holds exactly the piped output, the previous
log is kept, a parser error reaches the log; a new console running cmd /c <exe>: both exes write the log; pipes write no log.
Every app test and drive run of this step (sections 6, 7) launched the WIN32 exe through pipes.

**Release scripts (not edited, scripts/lan_release is not mine):** nothing must change for players: stage_lan_release.py
`APP_FILES` and github_release.py `PORT_FILES` ship `simgolf_port.exe`, the LAN updater's shortcut and both `Play SimGolf.bat`
point at it, and it now opens no console. Optional: add `simgolf_port_console.exe` to both lists (troubleshooting from a
terminal; it needs the same freetype.dll and VC runtime beside it).

## 2. Esc goes back

Survey: P9A_esc_survey.md (30 screens; every key test checked against original\golf_clean.exe with capstone). How a key reaches a
loop: WM_KEYDOWN -> rootWin_onChar stores the virtual key in gDirtyFlagA, WM_CHAR -> rootWin_onKey stores the char (0x1b) in
gDirtyFlagB (commands.cpp root_handler); pumpMessages 0x0045c030 never touches either word; flushRedraw 0x0045ae70 ends a loop
on any key.

**Already take Esc in the original (no change):** selectDifficultyScreen (any key gives -1: 0x0043a856 / 0x0043a870),
showThemePackSelect (any key = Cancel: 0x00473114), pickAProScreen (Esc tested at 0x0043b23c = Back), loadGameBrowserScreen (Esc
at 0x0043c5cb = Cancel; also the championship course list, the in-game L browser and the world map's Load), showPopup with key
handling (flag4 1: Esc returns -1, 0x0046db4a, table 0x0046dda8 entry 14; the System menu, Information and yes / no prompts),
textInputBox (Esc empties the text, 0x0045b528), runScrollText (Esc -1, 0x0046e117), and every screen that any key ends: the F-key
reports, the course report, the end-of-year screen, the roster, the shortcuts screen, the routing map, hole stats, the tournament
results, the bulletin board, the Top 10 screen and the credits.

**Esc already does something else (left as the original):** the hire dialog (Esc returns -1 at 0x00459800, no hire; its OK
click is a click on no row, which returns 2 at 0x00459837 and hireEmployee hires row 2, so Esc as OK would hire);
saveGameDialog (Esc empties the name and the invalid-name message shows); the character editor's comment line (Esc empties it).

**Changed (switch default 1 = the original; 0 in `play`).** Each arm reads the char word after the loop's pump; with an Esc
(gDirtyFlagB 0x1b) it takes both words (take_key / take_code, commands.cpp `cm::esc_pressed`) and does what the control's click
does; any other key is left exactly as the original leaves it (these loops never read the words; no flushRedraw is added). At
the screen's entry, with the switch at 0, an Esc left in the char word from before the screen is cleared (`cm::drop_stale_esc`),
so it cannot close the screen on its first pass (courseActionMenu needs none: its caller's pumpLight ML+0x11db6 clears the words).

| switch | screen | Esc acts as | original instruction the arm reproduces | port site |
|---|---|---|---|---|
| courseActionMenu.escIgnored | world map / location screen | Back: returns -1 | hot 3: 0x00470634 `cmp eax, 3; je 0x0047089e` | saveload.cpp after the loop's pump |
| showBuyLandScreen.escIgnored | buy land | Done: returns 0 | 0x004592e7 `cmp eax, -2; je 0x004593e9` | buyland.cpp inner loop |
| showCharacterEditor.escIgnored | character editor | a press on Exit (case 19) and the release wait | 0x00439d8f..0x00439dba (done at 0x00439d9d) | chareditor.cpp after pump_pass |
| pickCharEditor.escIgnored | the editor's look picker | its cancel (a right click): returns -1 | 0x0043857d | chareditor.cpp |
| showPopup.escIgnoredNoKeys | popups without key handling (flag4 0: Preferences, the tournament par list; 0x0046db31 skips the key block) | the close button: returns the mask | 0x0046dcdf -> 0x0046dd86 | commands_popup.inc, flag4 0 only |
| skillPointDialog.escIgnored | skill points (also from the character intro and the board's award) | a click on OK: closes when every point is used, else the points-left popup | 0x0045f644 `cmp esi, [esp + 0x10]; jle 0x0045f844`, popup 0x0045f64e | dialogs.cpp |

Not changed: the title menu. Its only exit control is the exit icon, which quits the game (top10.sve, Default.pro, the closing
movie, the credits), not a back step; an Esc there would quit. Left for Mariano (arm point in the survey, item 7).
Character editor: Esc = Exit keeps the edits (Exit's own behaviour). Its Cancel (case 18, 0x00439d74) restores the record but
leaves the editor open; an Esc that discards the edits would be Cancel then Exit (two controls, not one click): not done.

**Evidence.** Live: title -> Sandbox -> world map -> Esc, `play` profile, the WIN32 exe, --cursor message
(P9A_hover_drive.py; P9A_shots/esc_loc_before_after.png): before (b83d8213) the world map stays; after, the title menu
returns (Sandbox lit: the cursor still rests on it). The session replays through sg_run_boot: PASS (it has no frame head, so the
replay compares the boot only: 0 / 0 records). The other five arms were built and gated (segments 205 / 205 at switch 1) but not
driven live in this step.

## 3. Hover near a button on the first frame

Reproduced live (P9A_hover_drive.py on the b83d8213 app, `play`, 96 dpi, the 1067 x 600 wide window, --cursor message so the run
does not depend on the real mouse; P9A_shots/hover_before_msg_grid.png):
- `title_first` (no mouse message yet; the cursor answer is (0, 0)) and `title_far` (logical (400, 330), the middle of the
  screen): **Start New Game is lit** with the cursor far from it.
- `title_near` (logical (300, 437), right of the Sandbox Mode plate, inside the golf ball): **Sandbox Mode is lit**.

Both causes are the ORIGINAL's (mainLoop's title menu, ML = 0x0040f5c0; disassembly checked with capstone):
1. Each pass starts its selection at `ebx = (F+0x2c != 0) ? -1 : 1` (0x00421055 `mov ebx, [esp+0x30]`; `neg; sbb; and
   0xfffffffe; inc` up to 0x0042106c). F+0x2c is cleared on the menu's entry (ML+0x11a46) and set only when a hotspot comes within
   range (0x004210a1 `mov dword [esp+0x30], 1`). Until the cursor first nears an item, item 1 (Start New Game) is drawn lit, and a
   click anywhere starts a new game: the click branch tests only the button word (ML+0x11cb1) and ebx != -1 (ML+0x11cba).
2. An item is chosen when `distance(x - hx, (y - hy) * 4 - 0x18) < 0xa0` (0x0042106f..0x004210a1, distance 0x0040acd0) around its
   text centre: an area about 0x140 px wide and 0x50 tall, mostly outside the button art.

Port-side causes checked and not found: the viewport the cursor answers use is set at window creation and updated after every
dispatched message (main.cpp run_live_window, LiveSession::set_viewport), PanelRemap acts only on frames with a world layer (none
on the 2D screens), and --cursor system answers GetCursorPos from the first read. The P5B edge guard answers the last inside
point (start value (400, 300)) while the cursor is outside the client or the window is not in front; (400, 300) lies in no title
hotspot range (both rules) and in no difficulty ellipse (approx_distance(40, 67) = 87, not under 0x50), so it was not changed.
Other screens' hit areas are the original's too (difficulty: four ellipses of radius 0x50 with dx / 2 in
select_difficulty_screen; world map buttons: radius 0x19 / 0x14 around each button centre); not changed, no report names them.

**Switches (tick.cpp kDefects, arms in tick_menu.inc's hover loop):**
- `mainLoop.titleDefaultItem1` (0 in `play`): the pass starts with no item (-1); the search is unchanged.
- `mainLoop.titleHotspotRange` (0 in `play`): a labelled item (hotspots 0..4) is chosen only with the cursor inside its button
  art's box: showTitleScreen's source rectangles (0x0043d054..0x0043d129; port gfx.cpp kButtons), which the menu draws at the
  item's own position F+0x150 / F+0x152 (the same x / y): item 0 0x144 x 0x79, 1 0x16b x 0x8d, 2 0x10f x 0x6f, 3 0x120 x 0x73,
  4 0x11f x 0x73. Among boxes holding the cursor the original's distance decides (the boxes of items 3 and 4 overlap by 13 x 23
  px). The exit icon (hotspot 5) is mainLoop.titleExitHotspotWide's (P5A). The box is the art's bounding box, so its transparent
  corners still count; the sim has no per-pixel art mask (the cut sizes are the backend's).
After (P9A app, the same run; P9A_shots/hover_after_msg_grid.png): no item lit at `title_first`, `title_far` and `title_near`;
Sandbox lit at `title_over`. App-side consequence: the P6B exit click (the icon's centre (749, 556)) is inside the icon, so the X
button's title-menu exit still works.

## 5. The band fill

New `WideFill::Edge` (port/app/present.{h,cpp}, `Presenter::fill_bands_p9`, called from the P7 band function only for the new
modes): per side, the band's first 24 logical columns blend P7's edge colour (the per-row mean of the outer 4 logical columns,
blurred vertically by d / 2, at most 48 logical rows) into the screen's background with a smoothstep weight; past those columns
the band is the background only. The background per row is the per-channel median of the outer 64 logical columns, blurred
vertically over +-150 logical rows. A shape narrower than half of those columns (a pin, its shadow, a highlight bar or label cut
by the edge) is not in the median, so nothing is carried farther than 24 logical columns. The content rectangle is never touched.
`Mirror` (the frame's outer columns mirrored, blurred by d / 3, darkened to 40 % outwards) was rendered too and rejected: it
reflects whole shapes (the globe, the plates, the buttons) into the bands (P9A_shots/band_location_sheet.png, third panel).
Parameters: a numpy prototype compared 32 / 64 / 96 columns, +-64 / 150 rows and the 50th / 70th percentile on the location
screen; 32 columns left a grey block in the Jamaica rows (the plate inside the median's columns), 64 columns / +-150 rows / the
median removed it and stays neutral on dark screens.
Option: `--wide-fill edge|mirror|soft|clamp|black`, default `edge`; SG_APP_P9=off (set by port/test/conftest.py) gives P7's
`soft`. In-game frames keep their black / world bands (BandFill hint, unchanged).

**Screenshots, 1920 x 1080** (`log/roadmap/P9/P9A_band_shots.py <before exe> <after exe>`: the pinned original reference frame
of each screen through the app's `--compose`; band_<screen>_before_soft / _edge / _mirror / _default.png and
band_<screen>_sheet.png): location (cov_b_location buttons, fb_1's screen), title (sandbox_basic menu), theme packs
(cov_r_s2_screens theme_packs), load browser (cov_a_keys k_dlg_L), credits (cov_r_quit credits). On all five the default is edge
and the content rectangle is identical in every mode. Before: the Jamaica pin's shadow streaks across the right band
(band_location_crop_before_edge.png left half), the theme-pack highlight bar becomes a long yellow wedge, the title's side lines
stretch out; after: each stops within the fade. Grids: band_soft_grid4.png / band_edge_grid4.png. The 4:3 window (96 dpi 800 x
600) has no bands, so it is unchanged.
**Selftest** `--selftest-p9a` 5 / 5 (uniform frame -> uniform bands; a 6-row dark shape cut by the right edge changes no band
pixel past the fade and does change the band near the frame; content unchanged by the band; default fill is edge); control
`--negctl p9a-band-soft` (Soft in Edge's place): `p9a.edge_cut_shape_not_carried_past_the_fade` FAILS.

## 6. Gates
| gate | result |
|---|---|
| builds | x64 and Win32 rc 0, 0 errors (P9A_build_x64.log, P9A_build_Win32.log, the final passes) |
| segments x64, default switches | 205 / 205 PASS, status and digest equal to log/roadmap/merges/I25/segs_x64.tsv on all 205 rows (P9A_segs_x64.tsv) |
| selftests x64 (the WIN32-subsystem exe) and Win32 | --selftest 37 / 37, p5b 6 / 6, p7 9 / 9, p8b 11 / 11, p9a 5 / 5 (P9A_selftests_x64.log, P9A_selftests_Win32.log) |
| stdio | 13 / 13 (section 1) |
| pytest files, one per process (P9A_pytests.sh, P9A_pytest_<file>.log) | 8 files, 0 failures (section 7) |

## 7. Pytest files
One file per process (log/roadmap/P9/P9A_pytests.sh: w8_lock + test slot each; SG_PORTSIM_DLL / SG_APP_EXE = this tree's x64
build, the WIN32-subsystem simgolf_port.exe; conftest's environment incl. SG_APP_P9=off):

| file | result |
|---|---|
| port/test/test_static | 6 passed (re-run after the comments citing original addresses were rewritten as function+offset; the first run refused them) |
| test_profiles | 21 passed (play_profile.tsv = kPlayProfile, no switch in both tables) |
| test_app_shell | 18 passed (its standalone CMake build makes both executables) |
| test_app_input | 16 passed |
| test_app_replay | 14 passed |
| test_app_ingame | 14 passed |
| test_app_present | 3 passed (4 deselected by its markers) |
| test_app_live | 13 passed |

`--profile-list play` lists the 8 new switches at 0.

## 8. Left / for the parent
- Release scripts: optional `simgolf_port_console.exe` in stage_lan_release.py APP_FILES and github_release.py PORT_FILES
  (section 1); nothing is required.
- The title menu's Esc (quit) and the character editor's discard-on-Esc are decisions for Mariano (section 2).
- The five Esc arms other than the world map's were not driven live.
- Other screens' large hit areas (difficulty ellipses) are the original's; no switch, no report names them.
