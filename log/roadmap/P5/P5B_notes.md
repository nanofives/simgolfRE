# P5B: the app's window and input (Mariano's play-test of 70c2e89c, 2026-11-03)

Tree: port/build/P5B (git archive of b89154c4). Every change is in the Windows host (port/app) plus one line in
port/test/conftest.py; no port/src or port/include file changed, so the simulation, the segment gate and every recording replay
are untouched by construction (the segments x64 gate is still run, section 7).

All five items are class (b): app-side behaviour of the original that Mariano wants changed (item 5 is class (c)). Each new
behaviour is the app default and has its own switch; `SG_APP_P5B=off` turns all of them back to S7's behaviour at once, and
port/test/conftest.py sets it (`os.environ.setdefault`), so the existing app tests keep comparing nearest-pixel frames, driving
an unfocused window and closing it with WM_CLOSE. A flag on the command line wins over the environment.

| item | option (default first) | where |
|---|---|---|
| 1 edge scroll only for the game's own cursor | `--edge-guard on|off` | host.cpp LiveSession::cursor / async_key |
| 2 + / - zoom | `--zoom-keys all|numpad|off` | window.cpp AppWindow::input (zoom_key), host.h in_frame_body |
| 3 X asks the game's quit prompt | `--close-prompt on|off` | window.cpp WM_SYSCOMMAND, host.cpp request_close / close_check |
| 4 resizable, 4:3 fitted, smooth scaling | `--resizable on|off`, `--scale auto|nearest|smooth` | window.cpp, present.cpp |
| 5 widescreen | not implemented | section 5 |

session.json gains `p5b` {edge_guard, zoom_keys, zoom_remaps, close_prompt, resizable, scale} and the session block gains
close_prompt, close_flag_writes, close_at_menu, close_cancelled, edge_guard, guarded_cursor_reads.

## 1. Edge scrolling (class b)

Cause. mainLoop H1 reads the cursor once per frame through View_toLocal (ML+0xebb4, port/src/sim/tick_frame.inc:8288) and moves the
camera one step per side when x <= 0 (ML+0xebc4), y <= 0 (ML+0xebd3), x >= 0x31f (ML+0xebd9) or y >= 0x257 (ML+0xebe3). The cursor
the original gets is GetCursorPos + ScreenToClient with no clamp (jgld Display::getCursor; the app's LiveSession::cursor did the
same, host.cpp), so in a window a cursor anywhere left of / above / right of / below the client area, or on another monitor,
reads as an edge, focused or not. That is the original's own behaviour in a window (it was written for full screen, where the
cursor cannot leave the 800x600 area).

Change (`--edge-guard on`, HostOptions::edge_guard). A cursor read counts only when the system cursor lies inside the client
area AND the window is the foreground window AND WindowFromPoint at the cursor is the window (not a window on top of it). Any other
read is answered with the last answer given inside, clamped to 1..798 x 1..598 (one pixel away from every edge test), and counted
(guarded_cursor_reads). Inside the client, the letterbox / pillarbox bars still map to x < 0 etc. (item 4), so in a maximised
window the screen edge still scrolls. GetAsyncKeyState answers 0 while the window is not the foreground window (keys and buttons
held for another program are not the game's). The `--cursor message` source (test drivers) is unchanged.
GameState: only through the cursor answers H1 and the hover tests read; they are recorded in the trace like every answer, so
the recording replays.

Checked (log/roadmap/P5/P5B_drive.py edge, runs in log/roadmap/P5/P5B_runs/edge_on|edge_off): the title menu with the window not
in the foreground and the system cursor at screen (2424, 1790), 4 s: guard off, all 20 cursor answers are (2347, 1657) (x >= 0x31f
and y >= 0x257: in a game H1 would scroll right and down every frame); guard on, all 20 are (400, 300) (the start value: no inside
answer yet), guarded_cursor_reads 20.

## 2. Zoom with + and - (class b)

The original's zoom keys are H1's key 0x7a 'z' (zoom in, ML+0xd36d: g_zoomScale doubled while below 4, g_tileStep4c2840 = zoom *
8) and key 0x78 'x' (zoom out, ML+0xd327 / ML+0xd340: halved while above 1, or, with unk_005a9cbc set, that word cleared). The HUD's
own + / - buttons (bottom-left radial) are the same zoom. The H1 key is the WM_CHAR character (onKey's `a`, gDirtyFlagB,
commands.cpp root_handler case 7; setDirtyFlagB at ML+0xcb12), switched through the byte table at ML+0x1235c (tick_frame.inc
sw_h1_key).

What + and - already mean in the original (so the remap REPLACES them): key 0x2b '+' (both the numeric keypad's VK_ADD and
Shift+'=' give it) adds 0x64 to g_cash_div100 and sets g_flags bit 0x20000000 (ML+0xda7b..ML+0xda94); key 0x2d '-' lowers the
terrain corner under the cursor (ML+0xe3e1 onwards, playSound 0xc8); key 0x3d '=' (the unshifted VK_OEM_PLUS) raises it (ML+0xe590,
playSound 0xc7). onKey drops only the numeric keypad's digit keys (code 0x60..0x69, commands.cpp:187), so VK_ADD / VK_SUBTRACT reach
those cases. With `--zoom-keys all` (default, Mariano's request) the main keyboard's '-' and '=' no longer lower / raise terrain
from the keyboard (the terrain tool still does); `--zoom-keys numpad` remaps only VK_ADD / VK_SUBTRACT and keeps the main
keyboard's terrain keys; `off` is the original. THE ORCHESTRATOR SHOULD ASK MARIANO whether `all` or `numpad` is the default he
wants, given the terrain keys.

Change (window.cpp AppWindow::input, before the translator reads anything). While the game is in its frame body
(LiveSession::in_frame_body: the last pump call was P6 / H1, false in every modal loop: text boxes, popups, reports, the title
menu), a WM_KEYDOWN of VK_ADD (and VK_OEM_PLUS with `all`) becomes the WM_KEYDOWN of 'Z' and the WM_CHAR it produces becomes 'z';
VK_SUBTRACT (and VK_OEM_MINUS) become 'X' / 'x'; the key-ups follow. The character is always lower case, Shift or not: H1's
switch is case-sensitive, 'Z' 0x5a has no case and 'X' 0x58 is the sandbox's "reset tile / level tables, rebuild" key
(ML+0xd460, g_flags bit 0x1000000). Modifier
bits are left as the keyboard gives them (onKey's code = 0x5a | 0x10000 with Shift); H1's 'z' / 'x' cases do not read the code.
input.jsonl records the remapped message (so `--translate` re-checks what the game received), session.json counts zoom_remaps.

Checked (P5B_drive.py ingame, Monterey, numpad '-' '-' '+', main '-'; runs P5B_runs/ingame_all, ingame_off): the recording
replayed through sg_run_boot PASSES (117 / 117 records) and the final g_zoomScale is 1 (4 -> 2 -> 1 -> 2 -> 1), zoom_remaps 4;
with `--zoom-keys off` the same posted keys leave g_zoomScale 4 and set g_flags bit 0x20000000 (0x61000000 against 0x41000000):
the original's '+' case ran.

## 3. Closing the window with X (class b)

The original's own exit: the System menu's Exit row (showSystemMenu, commands_menus.inc case 7) stores 1 into unk_0059b734
(0x00432af1) and returns 0x1b; H1 then tests it at ML+0xf11a (after the [g_rootInput + 0x10] test at ML+0xf112) and enters
ML+0x11506 -> H2_Tail_00420acb, which clears it (ML+0x11511) and shows the end-of-game prompt (showPopup(400, 100, 1, 1, 0) at
ML+0x1179c: answer 1 saveGameDialog then the quit save, 0 / -1 back to the frame, any other the quit save; with g_debtYears >= 3 the
debt popup at ML+0x11557 first). The quit save returns kMenu (the title menu). Before P5B the window's X destroyed the window and
the session ended at the next frame head, with no save.

Change. AppWindow handles WM_SYSCOMMAND SC_CLOSE (the X and Alt+F4) through WindowOptions::on_close_button; a WM_CLOSE sent to the
window (the game's own quit, `--exit-after`, a test driver's app.close) still closes it. LiveSession::request_close:
- on the boot or the title menu (live frame 0, or g_difficulty == -1, which the title menu stores at 0x0040f8ab): the session
  stops and the window closes, as before;
- in a game: the next frame head writes unk_0059b734 = 1 through the core's write log (LiveWriteLog::wrote, a DELIVER w32 record
  the replay applies), exactly the store the System menu's Exit makes; H1 opens the game's prompt;
- the prompt took it (the word reads 0 again at a pump) and the player quits: the title menu is reached with no frame body in
  between, and the session stops there, so the window closes after the game's own quit save (and saveGameDialog when chosen);
- the player chooses "Wait, I don't want to quit yet!": the next frame body cancels the request;
- a second X before or while the prompt is open closes at once (no save), so a stuck modal screen can still be left.
GameState: only unk_0059b734 = 1 at the frame head after an X press in a game; nothing without a press.

Checked (P5B_drive.py ingame): after X in the game the process stays alive, the last presented frame is the game's prompt
(P5B_runs/ingame_all/back.png: "After a short career Gary Golf plans his retirement." with the three rows); Esc on it returns to the
frame (close_cancelled 1); a second X shows the prompt again (close_flag_writes 2); a third X during it ends the process in 0.27 s;
the recording with both writes replays PASS. P5B_drive.py title: X on the title menu ends the process in 0.81 s (stop "host",
0 records). Not run: answering "So long for now." with a click (the row coordinates were not measured); that path is the game's
own quit save, after which the title menu's g_difficulty == -1 stops the session.

## 4. Resizable window and smooth scaling (class b)

Change. `--resizable on` gives the window WS_THICKFRAME | WS_MAXIMIZEBOX; WM_SIZE refits the content rectangle (fit_viewport:
the largest 4:3 rectangle, centred, black bars), WM_GETMINMAXINFO keeps the client at least 400x300 (at 96 dpi), WM_DPICHANGED
keeps the user's size (Windows' suggested rectangle) instead of forcing 800x600 * dpi / 96. The mouse already maps through the same
Viewport (Translator::to_logical, LiveSession::cursor), so clicks land on the logical pixel shown. `--scale`: nearest = S7.3's
mapping; smooth = bilinear on 8-bit channels (pixel centre (x + 0.5) * 800 / w - 0.5, clamped, 8-bit weights, present.cpp);
auto (default) = nearest when the content is an exact multiple of 800x600 (1x, 2x: sharp pixels), smooth otherwise (Mariano's
150 % and other fractional factors, where nearest doubles every other row and column: the pixelated menu he saw). Recommendation:
auto (fractional nearest is the uneven pixel pattern; integer factors stay sharp).

Checked: `simgolf_port --selftest-p5b` 6 / 6 PASS (uniform frame stays uniform, smooth at 1x == nearest, auto at 2x == nearest,
auto at 1.5x == smooth != nearest, a black / white column pair blends to 63 at the 2x neighbour, letterbox bars stay black);
control `--negctl p5b-smooth-ignored` reads 2 rows RED. `--selftest` keeps its 37 rows PASS. P5B_drive.py resize: a 1300x900
client (content 1200x900 at x 50, scale 1.5), bars black on both sides, the title menu's Sandbox and the location screen's
Monterey clicked at their scaled client points (50 + x * 1.5, y * 1.5): the world loaded, so the clicks mapped back; maximised to a
3840x1984 client (P5B_runs/resize_auto/*.png). Frames are not presented while the user drags the border (Windows' modal sizing loop
dispatches the app's present message to the window procedure, which ignores it); the next frame after the drag shows.

## 5. Widescreen (class c, not implemented)

What it would take, from the code:
- The game's screen size is two words: g_screenWidth / g_screenHeight (0x00822c8c / 0x00822c90), set to 0x320 / 0x258 at
  gameMain+0x6 / +0x10 (tick_boot.inc:12-13) and passed to the display's set-mode slot and to initTerrain (tick_boot.inc:86,
  tick_menu.inc:46). worldToScreen / screenToTile read them (tick_render.inc:32, 193, 210), so the projection follows them.
- But most layout is literal: 0x320 appears 88 times in 23 port/src/sim files and 0x258 90 times in 22 (HUD bars, panels,
  dialog centring, snapshot images, fills such as Widget_fillRect(.., 0x2b0, 0x1c2, 0x310, ..) at ML+0xb1f3); H1's edge scroll
  uses 0x31f / 0x257 (ML+0xebd9 / ML+0xebe3); the System menu's hit area is x < 0xc8, y > 0x1b8 (ML+0xee46 / ML+0xee57); the
  radial menu, toolbar and bottom panel are drawn at fixed positions. Each of these needs a rule (anchor to a corner, centre, or
  stretch), i.e. a per-site decision, not one constant.
- renderTerrainStrip's width table knows 800x600, 1024x768 and 1280x1024 only (terrain_live.cpp:343-362: A, L10, L14, L18); any
  other size leaves three of them uninitialised in the original, so a new size needs its own values (derivable: they scale with
  the view's tile count).
- The rasteriser and LiveTerrain are fixed at 800x600 (U-1310; a root surface of another size draws nothing), the gfx backend's
  back buffer and every body family assume kLogicalW x kLogicalH, the app (input_map.h kLogicalW / kLogicalH, present.cpp), and
  every recording / framegate reference is 800x600.
- The terrain view's visible tile window (P1 / P2 per-tile passes over g_tileStep4c2840 / [0x004c2840]) and the camera clamps
  are derived from the tile count for 800x600.
A cheaper first step that stays inside the original's own design: 1024x768 / 1280x1024 (still 4:3) through g_screenWidth /
Height, since renderTerrainStrip already has their widths; it still needs the literal HUD sites above and a rasteriser of that
size. A true 16:9 mode is that plus a layout decision for every literal site: a phase of its own (estimate: the 0x320 / 0x258
sites, about 180, each read and given an anchor rule, plus new reference frames, because no recording of the original exists
at another size).

## 6. Files

log/roadmap/P5/P5B_files.txt lists every changed file. Tool: log/roadmap/P5/P5B_drive.py (ingame / title / edge / resize; run
from the tree root, posted messages only, app_replay's Driver and gate).

## 7. Gates

- Builds: x64 and Win32 rc 0, 0 errors (P5B_build_x64.log, P5B_build_Win32.log; MSBuild skipped up-to-date targets in the
  final pass, so each log shows the incremental build after the last edit).
- Segments x64 (P5B_segs_x64.tsv): 205 / 205 PASS; status and digest equal to log/roadmap/merges/I25/segs_x64.tsv on all 205
  rows.
- App tests, one file per process, SG_APP_P5B=off through conftest (P5B_pytest_test_app_*.log): test_app_shell 18 passed,
  test_app_input 16, test_app_replay 14, test_app_ingame 14, test_app_present 3 (4 deselected by its markers), test_app_live 12
  passed + 1 failed: test_title_idle_cpu read 8.43 % against its 5 % limit while P5A held the machine. Re-run alone it passed
  (P5B_pytest_test_app_live_idle_rerun.log). With SG_APP_P5B=off, the only P5B code that runs is WM_SIZE (client_changed at the
  same size) and WM_SYSCOMMAND passing through to DefWindowProc. Neither runs on an idle title menu, so the failure is load
  (S3.14's note: the pacing gates measure the host).
- `--selftest` 37 / 37, `--selftest-p5b` 6 / 6 (control 2 RED).
- Drive runs (section 1-4): P5B_runs/. Both in-game recordings made with the new options replay PASS through sg_run_boot
  (117 / 117 records each).
