# S7.5 merge request: the in-game frame in the app (worker S7.5, 2026-10-25)

Base measured: mirror port/build/S7_5_head = `git archive b3d61e41` + S3.2's files and shared edits (log/roadmap/S3/files_w3.txt,
2_merge_edits.py) + S7 wave 1 (log/roadmap/S7/MERGE.md: 7_live files, 7_tick.patch, 7_capi.patch, SG_MODULE(live),
add_subdirectory(app), the shared tree's port/app with S7.4's patches). Recipe: `~/bin/py -3.12 log/roadmap/S7/5_mirror.py --build
--w32`. Apply AFTER S7 wave 1 and S3 wave 3 (S3.2); nothing here touches portsim's sources.

## 1. Files (copy from the mirror, same paths)

| file | change |
|---|---|
| port/app/main.cpp | `--terrain live\|headless` (default live: a portsim::LiveTerrain at `--terrain-va`, default 0, attached after gfx.attach in the live session, --present-trace and --live-trace, as port/test/framegate.py does), `--live-trace <scenario\|file>` + `--snapshots a,b,..` (run_live with a LiveTracePresenter over a corpus trace; presents.tsv / snapshot_k.555 / framehash.bin / session.json), `--negctl reg:<registry name>=<mode>` (a port function's negctl switch for the run), session.json gains terrain / terrain_calls / terrain_passes |
| port/app/host.h, host.cpp | LiveTracePresenter (presents where LiveSession presents: H1's wait 'w', modal pump 'm' / 'p'; records how many snapshots existed per present); LiveSession::terrain_view and waits.tsv columns 7 / 8 (cumulative terrain passes and pixels) |
| port/test/test_app_ingame.py | new: the S7.5 gate (13 tests incl. 4 controls and one xfail, U-1640) |

The mirror's port/app started as a copy of the shared tree's (`5_mirror.py --status` lists exactly these three app files as
differing). If the shared port/app changed since 2026-10-25 05:00, merge by hand: the changes are self-contained blocks.

## 2. Shared / other sections' files (requests)

### 2.1 port/test/test_app_live.py (S7.6's test, S7's own file): apply log/roadmap/S7/5_test_app_live.patch
`git apply log/roadmap/S7/5_test_app_live.patch`. With the app drawing the terrain by default, S7.6's replay must attach LiveTerrain
like the app (`_live_terrain` after gfx.snapshotAt in both replays). Without it test_presents_replay fails on the first in-game present
(log/roadmap/S7/5_test_app_live_asis.log: 12 passed, 1 failed, present 7); with it 13 / 13 pass (5_test_app_live.log).

### 2.2 To S3 (not a merge edit, a need): rasteriser speed, U-1640
A frame with two full terrain passes (the zoom keys) works 144..186 ms in the app on this PC (two sessions at 100 % load, one with less), so it overruns the
87 ms frame (periods 148..191 ms); a one-pass frame works 35..75 ms (port/src/sim/gl_raster.cpp). The app keeps the
original's rule after an overrun (no wait). test_app_ingame.py::test_terrain_pass_frames_fit_the_frame is xfail(strict=False) until
it passes.

### 2.3 To S2 / S3 (needs, U-1641): the in-game HUD panels, the radial toolbar and build palettes, and the sprites (trees,
buildings, golfers, ball, flag) have no gfx body at this base; the app shows what the backend draws, so nothing in the app changes
when they come.

## 3. UNCERTAINTIES.md
Append log/roadmap/S7/5_uncertain.tsv (U-1640..U-1643).

## 4. CLAUDE.md paragraph (after the S7 wave 1 paragraphs)

```
Roadmap S7.5, the in-game frame in the app (2026-10-25, notes log/roadmap/S7/5_notes.md, U-1640..U-1643): simgolf_port.exe now
attaches a LiveTerrain (`--terrain live`, the default; `headless` = tick.cpp's stand-in) after gfx.attach, as framegate does, so
the course is drawn into the gfx root surface and presented with the 2D bodies; waits.tsv carries the cumulative terrain passes /
pixels. `--live-trace <scenario>` runs the live core over a corpus trace with a LiveTracePresenter (presents where a session
makes them) and gfx.snapshotAt at `--snapshots`; `--negctl reg:<name>=<mode>` sets any port negctl switch. Gate
(port/test/test_app_ingame.py, 12 passed + 1 xfail): an in-game session driven by posted messages (Monterey, seven camera moves,
four zoom keys, a hole built) has every present (84) equal to the replay's back buffer with LiveTerrain at the same recorded
position, and its view differs from a headless replay in 358,874 px; over cov_a_keys, cov_k1_routing, sandbox_basic and
course_season every checkpoint (49, 46 in game) produced on the app's live path equals framegate's frame in every pixel and the
present that showed it, and the three in-game rows framegate passes (k_worldmap, k_dlg_L, routing) pass; 368 in-game frames hold
87..98 ms (median 93). Open: frames with two terrain passes (zoom) work 144..186 ms (U-1640); HUD panels, toolbar
and sprites are not drawn at this base (U-1641). Controls RED: terrain pass skipped in the app and on the live path, pacing off,
headless terrain. S7.6's test_app_live replays now attach LiveTerrain (5_test_app_live.patch).
```

## 5. Gate commands (mirror root port/build/S7_5_head)

```
export SG_APP_EXE=$PWD/port/build/S7_5/app/Release/simgolf_port.exe SG_PORTSIM_DLL=$PWD/port/build/S7_5/Release/portsim_c.dll
~/bin/py -3.12 -m pytest port/test/test_app_ingame.py -p no:cacheprovider -v     # 12 passed, 1 xfailed (5_test_app_ingame.log)
unset SG_APP_EXE SG_PORTSIM_DLL; ~/bin/py -3.12 -m pytest port/test/test_app_live.py -p no:cacheprovider -v   # 13 (with 2.1)
```
Other gates on this mirror: 5_notes.md section 3.
