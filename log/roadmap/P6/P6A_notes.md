# P6A: Gary Golf off the map, and the camera off the map (simulation side)

Tree port/build/P6A (git archive of f04c7591). Both items are class (b): the port is the original's code in its transcription
and the original does the same; nothing here is a port bug. Three new switches, each default 1 = the original, each 0 in
profile `play` (re/port/play_profile.tsv, port/app/main.cpp kPlayProfile, rows in P6A_profile.tsv).

## Item 1: Gary Golf walks onto the black void

### Cause (original, golf_clean.exe)

The screenshot (fb2_gary_offmap.png) is a sandbox with no hole built yet. In that state Gary Golf is not a golfer record that
walks: he is walker kind 0xfa (-6) of g_walkers 0x585850. appendWalkerName names that kind from 0x004d6098 = `Gary Golf`
(compare 0x0046763e, copy 0x004676a4..0x004676cf).

updateWalkers 0x00402a40, kind 0xfa branch:
- 0x0040311b `cmp byte [esi+0x585863], 0xfa`, 0x00403124 `mov al, [0x575cb8]` (hole 1's par byte, record 0x575ab0 + 0x208):
  with no hole 1 and g_date bit 0x200 clear (0x0040312d..0x00403135) the target is the CURSOR tile:
  0x00403137 / 0x00403142 read g_cursorTileX 0x543d08 / g_cursorTileY 0x543d0c, 0x00403148 tests g_tileType[y + 50x]
  (0x5722e8) != 0x11 with no bound, 0x00403152..0x00403161 target = tile << 10 + 0x200. No clamp anywhere on this path.
- g_cursorTileX / Y come from screenToTile 0x00430020 (called by P6 at ML+0xa45a). Its body calls only tileToScreen 0x0042f940,
  worldToScreen 0x0042fb90 and distance 0x0040acd0 and has no grid bound, so with the cursor over the black area past the land
  it returns tiles outside 0..49.
- pathStep 0x0042e7e0 clamps both tiles to 0..0x31 (0x0042e820..0x0042e858), but it computes direction8 of the UNCLAMPED
  difference first (0x0042e80f, kept at [esp+0x20]) and, when the clamped start and target tiles are equal, returns that
  direction (0x0042e8bd..0x0042e8d2). With Gary on an edge tile and the cursor past that edge both clamp to the same tile, so
  pathStep keeps sending him toward the off-grid cursor, and every later call does the same (his own tile also clamps to the
  edge). The step (0x0040382f x, 0x00403864 y) has no grid test either. He walks off the land and stays where the cursor leads.
- The golfer record of the pro takes the walker's position in customizeCharacter (0x0040f5a1, audit123 table), so an
  off-map walker also gives an off-map golfer record.

Golfer records walking (updateGolfers 0x004289e0) have the same gap: the step 0x0042bad9..0x0042bb11 has no grid test, and the
reversal turn 0x0042b3af..0x0042b3c6 and the step overshoot are not bounded (log/c3/c3fyl/audit123.md item U1, not measured).
The ball itself is put back when it stops on a blocked tile, out of the grid included (tileBlocked at updateGolfers+0x4dcc,
reset to the shot start at updateGolfers+0x4dff..+0x4e14), so a ball cannot leave Gary off the map.

D5 / D6 (re/port/defect_decisions.tsv) are the grid-edge neighbour READS, a different defect; not touched.

### Change

Switch `updateWalkers.cursorTargetOffGrid` (golfers.cpp, golfers_walkers.inc), model row: this note. At 0:
- the kind 0xfa cursor target uses clamp(g_cursorTileX / Y, 0, 0x31) for both the 0x11 test and the target, so Gary walks to
  the edge tile under the cursor and stops there (his distance test then drops under 0x400);
- after every walker step x and y are clamped to 0..0xc7ff (the grid's world extent), so no walker can drift past the edge.
At 1 the body is unchanged.

Switch `updateGolfers.walkOffGrid` (tick.cpp, tick_golfers.inc): at 0 a golfer's position is clamped to 0..0xc7ff right after
the walking step (updateGolfers+0x30f9..+0x3131). Defensive: covers audit123's U1 for golfer records (Gary while he plays,
every visitor). No run of the original is known to reach it.

## Item 2: the camera leaves the map

### Cause (original)

The camera is the tile pair gCameraTileX 0x4c2ba0 / gCameraTileY 0x4c2ba4, the view centre (worldToScreen subtracts it,
bridge.cpp). The original's own bound is 0..0x31 on both axes, in H1 only: the arrow-key cases (ML+0xe971..ML+0xea73,
0x0041df31..0x0041e033, `push 0x31` + clamp 0x00467130) and the edge pan (0x0041e22b..0x0041e249). Every other writer stores
without a bound (log/c3/c3fyl/audit123.md lists them all):
- position followers: P7 shot aim 0x0041b4f8..0x0041b539 ((hole + 2 * golfer) / 3), H2 0x0041e73c..0x0041e754 and
  0x0041ef54..0x0041ef6c (golfer >> 10), updateGolfers 0x0042be6a..0x0042be82, clickEmployeeView 0x004356f6,
  handleEmployeeAction 0x00436e1f (walker), the panel-list letter keys' centring (commands_keys.inc panelListSelect),
  showBulletinBoard 0x0046e99c (photo position);
- offsets: Esc reset 0x0041c114..0x0041c133 (object 0 + 4 / - 4), key 0x30 0x0041c96a..0x0041c970, showTutorialText
  0x00460566..0x00460d0d (hole / object + up to 10), generateRandomCourse 0x00471671 (clubhouse + 6 / - 4).
The H1 clamp runs once per frame after the key case, and the render (updateTerrainRender at ML+0xf13) runs at the start of the
next frame, after P5 (updateGolfers), P7 and H2 have written again. So a followed golfer or walker off the land (item 1) takes
the view off the map in every frame. The original does the same.

### Change

Switch `mainLoop.cameraFollowOffGrid` (tick.cpp; helper `camera_on_map(Ctx&)`, declared in portsim/sim/tick.h). At 0 each of
the writers above clamps the pair to 0..0x31 right after it stores, the bound H1's own clamps use; the bound is taken from the
code. Sites: tick_frame.inc ML+0xbf79, ML+0xcb73, ML+0xd3b0, ML+0xf194, ML+0xf9ac; tick_golfers.inc updateGolfers+0x34a2;
tick_leaves.inc showTutorialText (on return, a scope guard); tick.cpp showBulletinBoard fallback; commands_keys.inc
panelListSelect; commands_panels.inc (employee detail list, employee_view_action); dialogs.cpp show_bulletin_board;
terrain_gen.cpp generateRandomCourse. stepCourseEditQueue's step (terrain_course.cpp) moves one unit toward a hole midpoint
<= 0x31 and the hole-midpoint writer ML+0xe7fa is bounded by in-grid hole tiles, so they were left alone.

What it does not do: 0..0x31 keeps the view CENTRE on the map, as the original's arrow keys and edge pan already do; at the
edge the original shows black around the land, and so does `play`. A tighter bound that keeps the whole 800 x 600 view on land
does not exist for every zoom: the land is a diamond in screen space, and zoomed out it fits inside the screen. If Mariano wants
less black at the edges, that is a new design choice (a zoom-dependent bound), not the original's clamp: item for a later pass.

## How checked

- Builds x64 and Win32 rc 0 (P6A_build_x64.log, P6A_build_Win32.log; the x64 build includes simgolf_port.exe).
- port/test/test_defects_p6a.py, 3 passed (P6A_t_test_defects_p6a.log): a sandbox_basic segment image with walker 0 as Gary
  (kind 0xfa) at tile (44, 25), hole 1 par 0, cursor tile (60, 25), 400 calls of updateWalkers: at 1 he walks past x 0xc7ff
  (the original's path); at 0 he stays inside the grid and ends within the arrival distance 0x400 of the edge tile's centre
  (the original's own stop test, updateWalkers' `approx_distance > 0x400`); with an in-grid cursor (30, 25) 1 and 0 give the
  same position on every call.
- port/test/test_profiles.py 21 passed (P6A_t_test_profiles.log).
- Segments x64 with default switches: 205 / 205 PASS, 0 rows differ from log/roadmap/merges/I25/segs_x64.tsv in status or
  digest (P6A_segs_x64.tsv, P6A_segs_x64.log).
- The camera and golfer switches have no unit test (light testing, G30); at 1 their code paths are the unchanged lines.
- Not run: the app with `play` on a live session (the fix was not watched in the window).

## Status (2026-11-03, before the planned LIVING-PC reboot)

Done: every deliverable above. Builds, test_defects_p6a, test_profiles and the segments gate all finished before the reboot
warning. No P6A lock wait or run is left; the old 16 GB take finished at 14:27 and the later holds released. Nothing is
pending for P6A; the next step is the orchestrator's merge of the files in P6A_files.txt (tree port/build/P6A). Later lock
runs from this worker will pass `--need-gb 8`.
