# S3.1 notes: the terrain draw-list passes (localRender, stripRender, pathUpdateRender)

Worker: claude2 opus (session cmvmo08uh3hwho1158rxvqrd3), 2026-10-24. Lead: S3 (cmvmns8sn3fsbo115g4dgm1nf). Game slot: instance
`instances\s3_1` (pinned winmm.dll 381ed2b2 copied from instances\p4q), used for six recordings g1 .. g6.

## 1. What was done

- **Port** (port/src/sim/terrain_render.cpp, port/include/portsim/sim/terrain_render.h): `Renderer::local_render`,
  `strip_render`, `path_update_render`, the draw list (`draw_tile`: Terrain::drawTile's sorted insert with its same-position test),
  `draw_tile_objects` (the camera-facing wall neighbours, recursive), the pass frame (`begin_pass` / `end_pass`). Public:
  `terrain_local_render`, `terrain_strip_render`, `terrain_path_update_render`, and `terrain_render_entry(ctx, sink, g, kind,
  terrain, a0, a1, a2)` for the corpus's entry kinds 1..4 (raw argument dwords, floats as bits). Written from the reading
  reference re/match/terrain_localrender.cpp, terrain_drawtile.cpp, terrain_draw.cpp (drawTileObjects), terrain_strip.cpp,
  terrain_pathrender.cpp; nothing pasted. Behaviour kept from the original, each judged by a recording unless noted:
  - drawTile's `other angle` branch tests `y < it.y` after a walk that stopped at the first `it.y >= y`, so the test never holds
    and the secondary walk always runs (same insert position as the other branches' equal-key case).
  - localRender tests no culling for its 25 window tiles (only drawTileObjects' neighbours are culled, against pTile, while the
    camera is translated to `pivot`); it asserts pTile != NULL before any GL call (port: Fault, U-1303, unrecorded).
  - stripRender: odd directions are two complete nested passes (8 + 2, 2 + 4, 4 + 6, 8 + 6), each with its own push / list /
    flush, then the outer pass draws its empty list; `tileAt` before the grid test, so a NULL tile inside 0..49 faults.
  - pathUpdateRender calls no setViewAngle (it keeps the quadrant the previous pass left: glcorpus.load now takes the view index
    from the snapshot's data image, which changes nothing for the other kinds since they set it); left edge clipped to x + 1, right
    to 49 - x (U-1302); the pivot is read after the GL frame (a NULL pivot faults there).
  - The draw list in the Terrain object (+0x164ac4 allocator, +0x164ac8 head, +0x164acc size) is empty at every pass entry in all
    137 snapshots of c3 .. c7 (and every pass empties it): the port keeps the order in a vector, faults if the size is not 0.
- **C ABI**: sg_gl_render / sg_gl_render_to_raster route every kind through terrain_render_entry. portsim_c.cpp is shared:
  the edit is in log/roadmap/S3/1_merge.md section 1 and applied only in the step's private source copy (1_mirror.py).
- **Recorder** (re/tools/gl_record.py `--scenario`, re/frida/js/gl_rec.js): a pinned scenario through scenario_refs.py (OFF arm,
  hooks off, the original Terrain.dll code), gl_rec.js in snapshot multi mode from process start, snapshots in gl_corpus.py's
  layout under log/p5/gl/corpus/<label>/ (index.tsv, inject.json). INJECTION (`--inject [--set basic|wide|edge|edge7] [--at
  <checkpoints>]`, `--zoom-early z`): a queued list of stripRender / pathUpdateRender / localRender calls runs on the game thread at
  the next outermost render / localRender, then the back buffer is written back, so every hooked run stays SAME as its reference.
  Pseudo-entries: ['zoom', z] forces the Terrain object's zoom shift for the following calls, ['fuzz', seed, p] applies glrecFuzz's
  tile mutation (both undone when the injection ends).
  FINDING (first attempt g1, probe in this session): Frida runs NO hook for a call made from inside an Interceptor.attach
  callback (a 64-bit probe: 15 calls of A, 0 hits on B's listener when A's onEnter called B), so injecting from onEnter drew the
  passes with no GL call recorded. From an Interceptor.replace callback the nested hooks fire (15 / 15). The injection is therefore
  an Interceptor.replace of render (0x5990) / localRender (0x89e0), installed at the first glrecInject (the scenario's main thread
  waits at a checkpoint outside every Terrain call) or with the other hooks when queued from the agent source. Second finding (g4's
  first try): the module observer runs tryInstall while the agent evaluates, so state it reads must be declared before it (a `let`
  declared later raised a ReferenceError and no hook was installed).
- **Tests** (port/test/test_terrain_render.py, glcorpus.py): `all_snaps(kinds)` over CORPORA c3 .. c7 plus every other exported
  corpus; per pass: stream equality, G2 of the port's stream rasterised (glraster.run_chain), coverage of angles / zooms /
  directions, controls 9 .. 14 and a pixel check of 9 / 10 / 13. glcorpus `export <src> <label> [kinds]`.

## 2. Recordings (all OFF arm on instances\s3_1, `scenario_refs.py check` SAME at every checkpoint)

| label | scenario | injection | snapshots | check |
|---|---|---|---|---|
| g1 | cov_b_holes18 | basic, from onEnter (no GL recorded: the Frida finding) | 3 organic | not checked (kept for its organic passes) |
| g2 | cov_b_holes18 | basic after every checkpoint | 29 injected + 3 organic (one organic stripRender, dir 5) | SAME (6) |
| g3 | cov_b_paths | wide at paths, water (4 angles x 8 directions, zoom forced 1 / 2) | 104 injected + 2 organic | SAME (4) |
| g4 | cov_b_holes18 | --zoom-early 2: at the first pass with zoom shift 2 (a real zoom-2 projection) | 25 injected + 2 organic | SAME (6) |
| g5 | cov_b_paths | edge at raised: tiles fuzzed (path 50 %, walls 30 %, hidden / type 7 5 %), pivots near the grid edges | 27 injected + 2 organic | SAME (4) |
| g6 | cov_b_paths | edge7 at raised: the same with 60 % type 7 (the border column not hidden) | 27 injected + 2 organic | SAME (4) |

Naming: `i<seq>_<checkpoint that queued it>_<kind>`; a pass queued at two checkpoints because the first queue never ran is named
after the earlier one (g2's i034..i041 were queued at holes and again at hired and ran between hired and limit). Passes that draw
no tile leave no snapshot (U-1301): direction 9 never does, and some groups drew nothing (g2 pathUpdateRender 15..19, 29..33).
Forced-zoom strips (g3) draw tiles that lie off the current projection (0 pixels: their G2 is vacuous; the streams are compared);
g4's real zoom-2 passes draw.

## 3. Numbers (S3 mirror, build port/build/S3_head/port/build/S3_1)

- STREAMS: all 237 draw-list passes equal the original's line for line: kind 3 localRender 33 (c3 2, c4 7, c5 3, c6 3, c7 3,
  g2 .. g6 15: 5 organic, 10 injected), kind 1 stripRender 160 (g2 25, g3 90, g4 21, g5 12, g6 12; directions 1..8 at zoom
  shift 0 and 1, five directions at zoom 2, angles 0 / 90 / 180 / -90), kind 4 pathUpdateRender 44 (g2 5, g3 14, g4 5, g5 10, g6
  10; four angles, zoom 0 / 1 / 2). Plus the 31 Terrain::render passes of c7, unchanged. c1 / c2 hold no Terrain object and no entry kind (their
  snapshots predate tobj.bin and entry_kind) and cannot be replayed.
- G2 (port's stream -> GlRaster from the recorded state stream and starting buffer, against dib_post): 221 of 237 meet it
  (one-level <= 1.5 % of the drawn, <= 70 further), among them 12 of g6's 28; the 16 others are g6 (constructed state, U-1300),
  where replaying the ORIGINAL's recorded stream gives the identical miss. Vacuous: 4 kind-3 passes and g3's forced-zoom strips
  draw 0 pixels (U-1304). All 209 passes outside g6 pass, one-level 0.08 .. 1.25 % of the drawn (on the passes that draw), at most 24 pixels more than
  one level off (log/roadmap/S3/1_g2_all.log, `PYTHONPATH=. ~/bin/py -3.12 log/roadmap/S3/1_g2.py` in the mirror).
- CONTROLS (sg_gl_negctl, all on the draw-list passes; log/roadmap/S3/1_controls.log before g5 / g6):
  9 list reversed RED on every multi-tile pass (181 of 181 before g5 / g6); 10 second tile skipped RED on 181 / 181; 11 strip window
  one row later RED on 111 of 136 strips (25 GREEN; which rows they draw was not read); 12 path left clip at x RED only on g6
  (6 passes; equivalent on every recorded game state, U-1302); 13 no wall recursion RED on 31 (plus 22 of g5); 14 duplicates RED on
  81 (plus 26 of g5). Pixel check (test_draw_list_controls_reach_the_pixels, g3/i061_water_strip): correct body 1 pixel more
  than one level off, control 9 192, 10 329, 13 76 (all break G2); control 14 redraws a tile over itself, an equivalent mutant in
  the picture (1), judged by the streams. The old controls 1 .. 8 still read RED on the Terrain::render passes.
- SUITES in the mirror: test_terrain_render 494 passed, 16 xfailed (g6 G2), 12 xpassed (243 s); test_gl_raster 139 passed;
  test_terrain_state 500 passed; port test_static 6 passed. Segments: x64 205 / 205 PASS, 0 differ from
  log/roadmap/merges/S6-1/segs_x64.tsv; Win32 (S3_1_w32 DLL + runner) 205 / 205 PASS, 0 differ from segs_win32.tsv. Boot gate:
  section 5. The renderer is reached only through the sg_gl_* entry points; no simulation path calls it.

## 4. Commands

```
~/bin/py -3.12 log/roadmap/S3/1_mirror.py --w32                 # source copy + both builds
cd port/build/S3_head
export SG_PORTSIM_DLL=$PWD/port/build/S3_1/Release/portsim_c.dll
~/bin/py -3.12 -m pytest port/test/test_terrain_render.py -q     # SG_S31_RASTER=sample: one in four strips rasterised
~/bin/py -3.12 -m pytest port/test/test_gl_raster.py port/test/test_terrain_state.py -q
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --dll $SG_PORTSIM_DLL --jobs 8 --out log/roadmap/S3/1_segs_x64.tsv
~/bin/py -3.12 -m port.test.integrator compare log/roadmap/merges/S6-1/segs_x64.tsv log/roadmap/S3/1_segs_x64.tsv
~/bin/py -3.12 -m port.test.integrator segments --backend win32 --dll32 port/build/S3_1_w32/Release/portsim_c.dll \
    --runner32 port/build/S3_1_w32/Release/portsim_runner32.exe --jobs 8 --out log/roadmap/S3/1_segs_win32.tsv
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out log/roadmap/S3/1_boot_gate.tsv
# recordings (repo root, game slot):
SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 re/tools/gl_record.py --scenario cov_b_holes18 --instance s3_1 --inject --label g2
SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 re/tools/gl_record.py --scenario cov_b_paths --instance s3_1 --inject --set wide --at paths,water --label g3
SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 re/tools/gl_record.py --scenario cov_b_holes18 --instance s3_1 --zoom-early 2 --label g4
SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 re/tools/gl_record.py --scenario cov_b_paths --instance s3_1 --inject --set edge --at raised --label g5
SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 re/tools/gl_record.py --scenario cov_b_paths --instance s3_1 --inject --set edge7 --at raised --label g6
~/bin/py -3.12 re/tools/scenario_refs.py check --scenario <scenario> --run g<k>
~/bin/py -3.12 -m port.test.glcorpus export log/p5/gl/corpus/g<k> g<k> 1,3,4
~/bin/py -3.12 -m port.test.glcorpus export log/p5/gl/corpus/c<k> c<k> 3        # k = 3 .. 6
```

## 5. Boot gate and the other suites

- `port.test.boot --all --jobs 3` with the S3_1 DLL (log/roadmap/S3/1_boot_gate.tsv, .log): HEAD's 61 scenarios 58 PASS, 2 KNOWN
  (cov_a_golfer, cov_b_holes18: U-1062), 1 NO_FRAMES (cov_b_location); 0 rows differ from log/roadmap/S0/1_boot_gate.tsv in status,
  record count or expected count (log/roadmap/S3/1_boot_compare.py / .log). The 6 extra rows are S1's recordings in the shared corpus
  (U-1102): cov_r_bio FAIL (-11 after 579 of 687 records) and cov_r_flic_input FAIL (record 1, g_cachedCameraTileX), as in
  BASELINE.md; the other four PASS.
- test_terrain_live 144 passed, test_terrain_gl_setup 6 passed (both read glcorpus.load, whose view index now comes from the snapshot).
- Repo static in the mirror: 595 passed, 5 skipped, 2 failed. The 2 failures (tests/test_match.py::test_shim_links_the_vc6_compiled_
  tileat, tests/test_static.py::test_shim_deployed_and_current) read shim/build/winmm.dll, which a git archive does not hold; both pass
  in the shared tree (log/roadmap/S3/1_static_shim_tree.log). The 3 extra skips (test_replay x3, test_failure_tools) want the
  uncommitted re/replay recordings. No static test reads this step's files differently.

## 6. Open items

- U-1300: GlRaster on g6's type-7-heavy constructed state (rasteriser, not renderer).
- U-1301: passes that draw no tile are not recorded (direction outside 1..8, empty windows).
- U-1302: control 12 is equivalent on every recorded game state (border column hidden).
- U-1303: NULL pTile / pivot paths modelled, never recorded.
- U-1304: vacuous or redraw-only G2 on most localRender passes.
- Not done here (S3.2's): LiveTerrain / TerrainApi local_render, path_update_render and render_strip still go to the headless
  engine; the renderer is driven only through sg_gl_render(_to_raster). renderTerrainStrip's own arguments (x, y, dx, dy, rot, zoom)
  -> stripRender(pivot, dir, angle) mapping lives in the exe side (re/analysis/terrain/0044a6e0_renderTerrainStrip.md).
