# S7.5 notes: the in-game frame in the Windows app, and the play session (worker S7.5, 2026-10-25)

Row S7.5 of re/roadmap/S7_steps.tsv, run on claude3 (opus). No game run (needs_game no). Status: the three measurable gate items
are met except the overrun of two-pass frames (U-1640); the play session waits for Mariano (BLOCKED_DECISION, section 6).
Mirror: port/build/S7_5_head (`~/bin/py -3.12 log/roadmap/S7/5_mirror.py --build [--w32]`: `git archive b3d61e41` + the S0
junctions + S3.2's files_w3.txt files and 2_merge_edits.py edits + S7 wave 1: 7_live files, 7_tick.patch, 7_capi.patch
(runner32 op 40 next to S3.2's 28..32: both applied cleanly), SG_MODULE(live), add_subdirectory(app), the shared tree's port/app
and app tests). Builds in the mirror: port/build/S7_5 (x64, portsim_c + simgolf_port), port/build/Win32 (portsim_c +
runner32), port/build/S7_6_app (S7.6's standalone app build that test_app_live makes). Merge: 5_merge.md. Files: 5_files.txt.
U-rows: 5_uncertain.tsv (U-1640..U-1643).

## 1. What was built

- THE TERRAIN IN THE APP (port/app/main.cpp TerrainGuard): every run mode attaches a `portsim::LiveTerrain(ctx, va, {})` after
  gditext.attach / gfx.attach (and the snapshotAt calls), exactly where port/test/framegate.py calls sg_ts_live_attach, and
  detaches it before the context goes. `--terrain live` is the default; `--terrain headless` leaves tick.cpp's HeadlessTerrain
  (run_live / run_frames attach it when no TerrainApi is set). `--terrain-va <v>` places the Terrain object (default 0: "the first
  free 4 MB step above the GameState + 32 MB", terrain_live.h). With it, S3.2's body family `terrain` draws the passes into
  gfx.root and Window::refreshFull copies them to the back buffer that LiveSession presents: nothing else in the app changed.
- waits.tsv columns 7 / 8 (host.cpp): LiveTerrainView::passes / pixels_drawn, cumulative, so each frame's work can be read against
  the terrain it rasterised. session.json: terrain, terrain_calls (LiveTerrain's call log size, U-1643), terrain_passes.
- THE APP'S LIVE PATH OVER A TRACE (`--live-trace <scenario | file.trace> [--snapshots a,b,..] [--max-records n]`, main.cpp
  run_live_trace + host.h LiveTracePresenter): portsim::run_live with the corpus trace's host (make_trace_live_host: tick.cpp's own
  parse_trace / TraceInput, the S7.1 equivalence host), wrapped so that it presents where LiveSession presents (H1's frame wait 'w';
  every modal pump call the host is asked about, except a world load's, 'm' / 'p'), only when the back buffer changed, and records
  per present how many gfx.snapshotAt snapshots existed. gfx.snapshotAt works on this path: TraceInput consumes the trace's CURSOR
  records and dispatches as in sg_run_boot (sandbox_basic: 10 / 10 snapshots, 439 records, 0 violations).
- `--negctl reg:<registry name>=<mode>` (RegNegctl): any port function's negctl switch for one run (e.g. reg:gfx.family.terrain=3).
- port/test/test_app_ingame.py (new, section 2), log/roadmap/S7/5_bench.py (wall time live vs headless), 5_gates.sh (regression
  gates on the mirror), 5_play.bat (the play session's launcher), 5_mirror.py.

## 2. Gate, measured (test_app_ingame.py: 12 passed, 1 xfailed; log 5_test_app_ingame.log, facts 5_app_ingame_facts.json, the
less-loaded pacing session 5_app_ingame_facts_lowload.json / 5_test_pacing_lowload.log)

| gate item | measured |
|---|---|
| (1) an in-game frame presented by the app = the gfx back buffer of the port at the same record | one session, posted messages only (app_replay.Driver): title -> Sandbox -> Monterey -> welcome Esc, seven arrow-key camera moves, four zoom keys (X, Z, X, Z), Build Course palette, tee, green, right click, H, Esc; 381 records, 0 violations. Every present (84: m 5, w 79) equals the snapshot a replay of the session's own trace takes at its recorded position (sg_run_boot, gfx + LiveTerrain at 0, test_ingame_presents_equal_the_port_back_buffer); every frame the window thread took equals its present; the recording replays to the session's framehash in every record. The presents hold LiveTerrain's drawing: the last in-game present differs from a headless replay's frame at the same position in 358,874 px of the 450 rows above the toolbar; 15 terrain passes (6,640,782 px) on 11 frames. |
| (2) framegate's in-game checkpoints that PASS under S3 also PASS through the app's live path from the same trace | framegate census of this mirror (5_census_x64.tsv, 698 rows): PASS 82, of them in game 3: cov_a_keys k_worldmap, cov_a_keys k_dlg_L, cov_k1_routing routing (all three modal 2D screens over the course view). `--live-trace` over cov_a_keys, cov_k1_routing, sandbox_basic, course_season: all 49 checkpoints (46 in game) give a snapshot that equals framegate's own frame (sg_run_boot + LiveTerrain at framegate.terrain_address) in every pixel and equals the present the app made just before it (40 'w' presents at a frame wait, 7 'm' and 2 'p' in modal loops); the 3 in-game PASS rows are 0 px against the original's frame on the app path too. The 43 other in-game rows differ from the original by 370..458,926 px, exactly as framegate's frames do (UNSUPPORTED: HUD panels, toolbar and sprites have no body, U-1641). |
| (3) frame period under in-game load 87 ms +- the host's timer resolution | 368 in-game frames that waited and whose work fit: periods 87..98 ms, median 93 (100 % within 87 + 10 ms quantum + 2 ms). Two frames overran: the zoom keys' frames with two full terrain passes worked 185 / 144 ms (first full run), 186 / 166 ms (final run), 176 / 149 ms (a third session with less load); they make no waitTicks step (the original's rule after an overrun) and their periods are work + 4..5 ms. Five frames made no H1 wait at all (the five build clicks' frames: periods 5..12 ms; the original's control flow, waits.tsv has no row for them, U-0651). A frame with one terrain pass (an arrow key, a zoom step: 262,017..480,679 px) worked 35..75 ms and held 87 ms. The first two sessions ran with the PC at 100 % CPU from other sessions (12 logical processors), the third with less load. The strict check (no in-game overrun) is xfail, U-1640. |
| Mariano plays title -> sandbox -> one hole | BLOCKED_DECISION: start command and guide in section 6 |

Controls (RED, all four): `--negctl reg:gfx.family.terrain=3` in the app only (passes skipped): in-game presents differ from the
LiveTerrain replay; the same switch on `--live-trace sandbox_basic`: every in-game snapshot differs from framegate's;
`--negctl pacing-off` in game: median period below 87 ms; `--terrain headless`: no terrain pass in waits.tsv and the in-game present
is not the LiveTerrain replay's frame. What judges the terrain on the live path: the three PASS rows are 2D screens over the course
(S3.2 measured k_worldmap with no terrain on screen, terrain_drawn 0; for k_dlg_L and routing it was not measured), so their 0 px
against the original says little about the terrain; (2)'s pixel equality with framegate on the 43 other in-game rows does, and the
pass-skip control turns it RED (sandbox_basic, all 9 in-game rows differ). The pass-skip control was not run on the three PASS rows.

## 3. Regression gates on this mirror

| gate | measured | log |
|---|---|---|
| port test_static | 6 passed | 5_test_test_static.log |
| test_app_shell / test_app_input / test_app_replay | 18 / 16 / 14 passed | 5_test_test_app_*.log |
| test_app_live (S7.6) | as is: 12 passed, 1 failed (test_presents_replay: present 7, the first in-game present, has the terrain the replay without LiveTerrain lacks); with 5_test_app_live.patch (the replays attach LiveTerrain like the app): 13 passed | 5_test_app_live_asis.log, 5_test_app_live.log |
| test_live (S7.7), x64, not slow | 32 passed | 5_test_test_live.log |
| live equivalence (S7.1), x64 | 70 rows: 68 PASS, 2 NO_FRAMES; equal to S7.7's table in 69 rows, the 70th is cov_l_point, FAIL there (S7.7's known load flake, -11) and PASS here, as in S7.7's own re-run (7_equiv_x64_cov_l_point.tsv) | 5_equiv_x64.tsv / .log |
| segments x64 | 205 / 205 PASS, 0 rows differ from log/roadmap/merges/S6-1/segs_x64.tsv (digests equal) | 5_segs_x64.tsv, 5_segs_x64_compare.log |
| boot gate x64 | HEAD's 61: 58 PASS / 2 KNOWN / 1 NO_FRAMES, 0 rows differ from S0; all 70 rows (65 PASS, 2 KNOWN, 2 NO_FRAMES, 1 FAIL: cov_r_s2_screens 1156 / 1268, the unported showBuyLandScreen, U-1251) equal to the b3d61e41 merge's own table (log/roadmap/merges/S2S3-5/boot_x64.tsv) | 5_boot_gate.tsv, 5_boot_compare.log |
| framegate census | 698 rows: 82 PASS (in game 3), 592 UNSUPPORTED, 4 FAIL, 20 NOT_RUN: the counts the b3d61e41 commit states | 5_census_x64.tsv |

Win32 was not re-run: the app is x64 only, and nothing in portsim's sources differs from the parts S3.2 (Win32 LiveTerrain through
runner ops 28..32) and S7.7 (live op 40) measured on Win32; `sh log/roadmap/S7/5_gates.sh win32` runs it.

## 4. Facts that matter for later steps

- A frame that rasterises one terrain pass of 262,017..480,679 px (each arrow key's frame in these sessions; the view is 480,000
  px) works 35..75 ms in the app; frames without a pass 4..7 ms; each zoom key (X, Z) frame rasterised two passes. Which pass kind
  (render, strip, localRender) each was is not counted.
- In-game presents: a present is made only when the back buffer changed. S7.6's session, which leaves the welcome message up,
  made 1 'w' present in 420 frames after this change.
- course_season over --live-trace: 6,296 records, 107 terrain passes, 2,661,380 px (24,873 px per pass on average), 25,671 Terrain
  calls; wall time live 142.6 s against headless 123.3 s on the loaded PC (5_bench.log), 71.9 against 69.5 s with less load
  (5_bench2.log).
- The app path with LiveTerrain at address 0 gives framegate's pixels (framegate places the object at
  framegate.terrain_address: the pin recording's getInstance return when one exists, else 0) on all 49 checkpoints.

## 5. Open

U-1640 two-pass frames overrun (needs S3's rasteriser faster, or a measurement on an idle PC); U-1641 HUD panels, toolbar, palettes,
sprites not drawn at this base; U-1642 the app presents at H1's wait, the original at P6's present; U-1643 LiveTerrain's call log
grows without bound in a long session.

## 6. The play session (for Mariano)

START (PowerShell or cmd, from anywhere):

```
"C:\Users\maria\Desktop\Proyectos\SimGolf\port\build\S7_5_head\port\build\S7_5\app\Release\simgolf_port.exe" --install "C:\Users\maria\Desktop\Proyectos\SimGolf\original"
```

or double-click `C:\Users\maria\Desktop\Proyectos\SimGolf\log\roadmap\S7\5_play.bat`. In PowerShell put `& ` before the quoted exe.
After S7.5 merges into the shared tree, the same exe comes from any port build that includes add_subdirectory(app), e.g.
`cmake --build port/build/x64 --config Release --target simgolf_port` -> port\build\x64\app\Release\simgolf_port.exe (same
arguments). No original binary is loaded (golf_clean.exe is read as a file for its data); nothing in original\ is written.

WHAT TO DO (title menu -> sandbox -> one hole): the window shows the 800 x 600 picture scaled to the window, so the positions below
are in that picture (left / top from its top-left corner).
1. Title menu (drawn as the original): click `Sandbox Mode` (about 130, 445).
2. Location screen (drawn as the original): click Monterey (top row, about 270, 32).
3. The course loads (a second or two); press Esc to close the welcome message (the message box is NOT drawn yet: press Esc anyway).
4. Arrow keys move the camera; X and Z zoom (a zoom frame takes about 0.2 s on this PC: U-1640).
5. Build a hole. The radial toolbar at the bottom left is NOT drawn: click blind. Build Course palette button: about (45, 470).
   Tee tool: (300, 527); then click on the course where the tee goes. Green tool: (365, 527); click where the green goes. Right click
   drops the tool. Press H to open the hole. Esc closes the message that follows.
6. Close the window to end (the session ends at the next frame).

WHAT IS DRAWN: the title menu, the location screen, the credits, the routing map, the load browser, the world map and the course
terrain (all four course themes, the tees and greens you build), parts of the HUD's text (course name and date, the money and
rating numbers, some golfer name labels and thought texts, without their boxes).
NOT DRAWN YET (U-1641): the HUD panels, the radial toolbar and every build palette, the message ticker at the top ("Hole #1 ... is
now open for play"), message boxes over the course, trees, buildings, golfers, the ball and flags (log/roadmap/S7/5_course_season
pictures log/roadmap/S7/5_cs.png (course_season hole_open and season_060) and 5_sb20.png (sandbox_basic sandbox_20): the
port's frame left, the original's right). Sound: none (no audio output at this base). The simulation runs regardless:
golfers arrive and play, money moves, time passes.

WHERE THINGS GO: `%LOCALAPPDATA%\SimGolfPort\` (C:\Users\maria\AppData\Local\SimGolfPort):
- `recordings\<yyyymmdd-hhmmss>-<pid>\`: one directory per session (trace.trace, framehash.bin, presents.tsv, shown.tsv,
  frames.tsv, waits.tsv, input.jsonl, session.json, back.555 = the last frame). A recording replays with sg_run_boot (port/test
  app_replay.py `gate` style); send the directory if something goes wrong.
- `files\`: what the game writes (saves under `files\Saved Games\`, top10.sve, character files); reads see these first, then the
  install. Deleting `files\` resets the saves; the install is untouched.

## 7. Commands

```
~/bin/py -3.12 log/roadmap/S7/5_mirror.py --build --w32                 # mirror (created once) + builds
cd port/build/S7_5_head
export SG_APP_EXE=$PWD/port/build/S7_5/app/Release/simgolf_port.exe SG_PORTSIM_DLL=$PWD/port/build/S7_5/Release/portsim_c.dll
~/bin/py -3.12 -m pytest port/test/test_app_ingame.py -p no:cacheprovider -v      # the gate
unset SG_APP_EXE; ~/bin/py -3.12 -m pytest port/test/test_app_live.py -p no:cacheprovider -v   # S7.6's, with the patch
~/bin/py -3.12 -m port.test.framegate --all --jobs 2 --no-probe --out ../../../log/roadmap/S7/5_census_x64.tsv
cd ../../..; sh log/roadmap/S7/5_gates.sh x64; ~/bin/py -3.12 log/roadmap/S7/5_bench.py
```
