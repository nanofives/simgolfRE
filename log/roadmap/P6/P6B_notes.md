# P6B: X exits to Windows, a loading indicator, sooner hover tips (Mariano's second play-test, 2026-11-03)

Tree: port/build/P6B (a `git archive` of f04c7591 plus junctions). Files: P6B_files.txt (16 modified files, none new). Profile
row: P6B_profile.tsv. Tool: log/roadmap/P6/P6B_drive.py (`quit --answer quit|save|wait`, `title`, `load [--extra ...]`), run from
the tree root with SG_PORTSIM_DLL set; its outputs are P6B_runs_*.json and P6B_runs/<run>/ (each session's recording directory).

| item | class | change | default |
|---|---|---|---|
| 1 X exits to Windows | (a) a P5B bug, plus (b) app behaviour | `--close-prompt on` (P5B's option) now leaves through the game's own exit | on (SG_APP_P5B=off: off) |
| 2 loading indicator | (b) app option | `--loading-overlay on|off` | on (SG_APP_P6B=off: off; conftest sets it) |
| 3 sooner hover tips | (b) play switch | `hoverTip.tenFrameDwell` (default 1 = the original's 10 frames) | `play`: 0 |

## 1. Closing the window with X

**Cause (class a: P5B's code differs from what it meant to do).** P5B closed the app after the in-game prompt when a pump ran
"on the title menu". It tested that as `frame_ == 0 || g_difficulty == -1`. The -1 store at 0x0040f8ab (mainLoop+0x2eb) is on the
boot path only. The title menu that H2_QuitSave returns to (ML+0x11875) writes g_date = 0 and g_flags = 0x40000000 but leaves
g_difficulty at the session's value. So after a game the test never fired, and the session went back to the title menu.
Reproduced on the base build: X, then Down x3 + Enter ("So long for now."). &QuitSave.sve is written, the app stays on the title
menu and close_at_menu reads 0 (P6B_runs/quit_base).

**Change.**
- Port, a notification only. tick_menu.inc calls `io.live->title_menu_pass()` after the title menu's refreshFull
  (ML+0x11ca5). That is LiveIo::title_menu_pass (tick_hook.h), forwarded as LiveHost::title_menu (live.h, live.cpp). It
  changes no control flow, and trace runs have no LiveIo, so sg_run_frames / sg_run_boot are unchanged by construction.
- App (host.h / host.cpp).
  - `title_seen_` is set by that call and cleared at every frame head, so "on the title menu" is exact.
  - **X on the title menu (or at the boot):** the window hides at once (ShowWindowAsync SW_HIDE), and the exit is armed.
  - **X in a game:** P5B's flow is unchanged. unk_0059b734 = 1 is written at the frame head and H1 opens the game's prompt.
    - "Save, then quit" (saveGameDialog) or "quit": the game's quit save returns to the title menu, whose first pass arms
      the exit (the window hides there).
    - "Wait": the next frame body cancels, as before.
    - Stage 2 arms too: with g_debtYears >= 3 and flag 0x1000000, H1 goes to the quit save with no prompt (ML+0x11585).
  - **Armed:** the next cursor read is answered with the exit icon's centre (749, 556). The icon is (0x2d1, 0x210) + 0x3a x
    0x39 from Setup's slots; the centre is inside it for play's rule and nearest to hotspot 5 for the original's. At the same
    read the button word g_rootInput + 0x10 = 1 is stored as a recorded harness write (`writes->wrote`, DELIVER w32). The
    menu's own click test (ML+0x11cb1) then takes hotspot 5 (selection -2, ML+0x11db1), and Session's QUIT path runs:
    - top10.sve (ML+0x4ad..ML+0x4ce);
    - promptAndSaveFile(0, -2, 0), which writes Default.pro;
    - the closing movie (unavailable: skipped);
    - the credits, which end on the button word (an Esc is also posted every 100 ms while a Blocking pump waits);
    - sQuit: the session ends with stop `quit` and the window closes.
  - The first version required a modal read and was never taken (title_exits 0 in the runs before the reboot). The title
    loop's View_toLocal does not come as a modal read, so the next read of any kind is used.
  - Fallbacks, so the window can always be left:
    - a second X closes at once (no save), as P5B did;
    - an X on a non-frame screen that is not the title loop (location / difficulty screens, the world load) stops the session
      when no title pass comes within 0.5 s;
    - an exit that has not ended after 15 s stops.
  - session.json `close_exit` {title_exits, esc_posted, fallback_stops}.
- GameState: nothing changes without an X press. After one, the only effects are the cursor answer and the one recorded
  button-word write, so every session replays.

**Checked** (P6B_runs_title.json, P6B_runs_quit_{quit,save,wait}.json; `exit_s` counts from the last posted key):

| run | result | exit_s | files written | replay |
|---|---|---|---|---|
| X on the title menu | stop `quit` | 1.76 | top10.sve, Themes/Default.pro | replay to the trace's end writes byte-equal files |
| X in game, "quit" | stop `quit` | 0.47 | &QuitSave.sve, top10.sve, Default.pro | gate PASS 58 / 58 records, replayed files equal |
| X in game, "save, then quit" | stop `quit` | 2.84 (the driver's 2 s pause before the save dialog's Enter included) | + "Ocean's Edge 2 March 2001.sve" | gate PASS 59 / 59 |
| X in game, "Wait" | the game goes on (close_cancelled 1), killed by the driver after 20 s | n/a | none | gate PASS 275 / 275 |

Not changed: during the title-menu exit the game's own exit sounds (startSoundSlot 0x2d / 0x81, playSound 0x81) play for about
1 s while the window is hidden.

## 2. The delay entering a map

**What the screen shows (the original's behaviour).**
- The game's loading screen (showLoadingScreen) appears only in two places:
  - from loadFlic every 100th load, and only while g_difficulty is -1, i.e. during the boot (menu.cpp load_flic);
  - once, at ML+0x12043, after a title-menu selection and before the location screen.
- After a location click, the world load (Session -> loadWorldAssets' loadFlic calls, each with a doAllTasks pump) presents
  nothing. The location screen stays frozen until the first frame (P6B_runs/load_nooverlay/loading.png, 0.4 s into the load).

**Measured** (Monterey from the sandbox location screen, this PC, P6B_runs_load_*.json and the sessions' `loads`):

| build | click -> first frame head |
|---|---|
| base (P5B's exe, same load code) | 1.416 s, 1.430 s |
| P6B, overlay on | 1.668 s, 1.573 s, 1.527 s |
| P6B, overlay off | 1.570 s |

- The P6B runs vary by about 0.15 s between runs, with or without the overlay. The overlay's own cost (one GDI box per 50 ms,
  window thread) is inside that spread. The extra over the base comes from these runs, not from a separate measurement.
- The world load is 120 Load-kind pumps lasting 1,529 / 1,625 ms. Its first pump comes 28-30 ms after the click.
- **Avoidable part:** flic::sheet_to_gfx (decoding every sheet's frames into the backend's sprites up front) takes 929 /
  1,063 ms of it (120 calls), about 60-65 % of the load. The rest is the simulation's own load work. Deferring the decode to a
  cell's first draw (or caching decoded sheets across loads) would remove most of that second from the click-to-course time.
  It is not done here: it touches the S3.4 sheet path that test_flic judges.
- The boot's 386 loads take 1.64-1.68 s, with sheet decode 1.16 s; the game's own loading screen shows then.

**Change (class b, app option `--loading-overlay`, default on).**
- LiveSession::pump times every run of PumpKind::Load pumps after the first title-menu pass and posts kMsgLoading (WM_APP + 4)
  at most every 50 ms. The load ends at the next pump of another kind or at a frame head.
- Presenter::paint draws a "Loading..." box with a progress bar (done / expected pumps, at most 95 %) centred on the content
  rectangle.
  - expected = the previous load's pump count, or kWorldLoadPumps = 120 for the first (the measured Monterey load).
  - The box goes into the window DC only: the frame data, frame hashes, recordings and replays are unchanged.
- The box appears about 30 ms after the click (the first Load pump) and stays until the course's first frame
  (P6B_runs/load_overlay/loading.png). session.json `loads`: start, duration, pumps, ms since the last input, sheet-decode ms
  and calls (flic.cpp sheet_to_gfx_stats; the timing does not change what it does).

## 3. Hover tips in the build palette

**Cause (the original's behaviour).** Every HUD panel keeps one counter pair: g_panelState + 4 (the last hovered control) and
g_ui_5aa554 (its count, +1 per frame on the same control, reset on a change). A tip is drawn only when the count is above 10.

| function | compare | address |
|---|---|---|
| showInfoCursorPanel 0x00433190 (the Build Course palette), tool tip | `cmp dword [0x5aa554], 0xa; jle` | 0x00433257 |
| showInfoCursorPanel, slot tip | `cmp eax, 0xa; jle` | 0x004338ff |
| drawTerrainToolbar | same compare | 0x00433f90 |
| drawBuildingPanel | same compare | 0x004343a5 |
| drawAmenitiesPanel | same compare | 0x004351ed |
| drawEmployeeViewPanel | same compare | 0x00435e2a |
| drawGolferActionPanel | same compare | 0x00436925 |

Function extents are from re/functions_ghidra.tsv. The tip therefore comes 11 frames after the first hovered frame: about 1.0 s
at mainLoop's 87..100 ms frame (median 93).

**Change (class b, play switch `hoverTip.tenFrameDwell`, gfx.cpp kDefects, variable and `hover_tip_dwell()` in gfx_hud.cpp).**
- Default 1 = the original's 10. At 0 the threshold is 3: the tip comes 4 frames after the first hovered frame, about 0.37 s
  (Windows' own hover time is 400 ms).
- Used at the six gfx_hud.cpp dwell sites (the drawing keeps its own copy of the counters) and at drawBuildingPanel in
  tick_panels.inc, whose tip branch the port runs in the frame.
- The counters are ui class. At 0, drawBuildingPanel builds its tip text in g_textBuffer (hidden class) and appendNumber's
  g_textBuf (the segment gate's NON_CAUSAL object) sooner. No compared object depends on it, and default 1 is the original.
- In kPlayProfile (main.cpp) and P6B_profile.tsv. `simgolf_port --profile-list play` lists `hoverTip.tenFrameDwell 0`.
- The character editor's own tip (chareditor.cpp, `> 10`, another screen) is left as it is.
- Not measured in a run: the delay follows from the frame count.

## Gates

- Builds x64 and Win32: rc 0, 0 errors, 0 warnings (P6B_build_x64.log, P6B_build_Win32.log; both are the incremental passes
  after the last edit).
- Segments x64 (P6B_segs_x64.tsv): 205 / 205 PASS, status and digest equal to log/roadmap/merges/I25/segs_x64.tsv on all 205
  rows.
- One file per process, SG_APP_PROFILE=fixed, SG_APP_P5B=off and SG_APP_P6B=off through conftest (P6B_pytest_*.log):

  | file | result |
  |---|---|
  | port/test/test_static | 6 passed |
  | test_app_shell | 18 passed |
  | test_app_input | 16 passed |
  | test_app_replay | 14 passed |
  | test_app_ingame | 14 passed |
  | test_app_present | 3 passed (4 deselected by its markers) |
  | test_app_live | 13 passed |

- Item 1's four drive runs replay through sg_run_boot (section 1).
- Lock: every heavy run took w8_lock with `--need-gb 8` (the orchestrator's instruction: pagefile 16 GB, commit limit 31.8 GB).
