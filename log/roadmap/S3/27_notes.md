# S3.27 notes (wave 8 follow-up, group 2 / U-1952: the GL stream of the map-corner render)

Worker: claude2 (Opus), 2026-10-31. Lead: cmvu18cc81fcpp715vy6fd2dv. Game slot: instance `instances\s3_w8` (S3.25's, unchanged).
No port code and no shared file changed. Every boot and replay ran through `w8_lock.py run S3.27` (four holds: two boots, two
glreplay batches). U-1955..U-1959 reserved; rows used are in 27_uncertain.tsv.

## 1. Result

U-1952 is settled. The near-black dither in cov_r_s2_screens' map corner (box 781..799 x 451..466) is drawn by
**Terrain::lowerEdgeCorner** (Terrain.dll 0x1000a680, exported; the exe calls it through the import slot 0x4ba158 from
applyTileElevation 0x00449fa0 at 0x0044a1a5), whose first call **Terrain::renderTileAtFiveOffsets** (0x10038900, called at
0x1000a6b2, matched in re/match/terrain_edge.cpp) DRAWS: it sets up the same transform as localRender, and when a neighbour toward
the camera is missing or hidden it calls Terrain::enableBlackLight1 (0x10003830: glDisable(GL_LIGHT0), GL_LIGHT1 ambient / diffuse
/ specular all 0, glEnable(GL_LIGHT1)) and renders the tile five times at the offsets (+10, 0, -10), (-10, 0, -10), (-10, 0, +10),
(+10, 0, +10), (0, 0, 0) (cumulative glTranslatef (10, 0, -10), (-20, 0, 0), (0, 0, 20), (20, 0, 0), (-10, 0, -10)), then
glDisable(GL_LIGHT1), glEnable(GL_LIGHT0). With only the global ambient left (0.2 x 0.2 = 0.04 x the texel, GL_MODULATE) the
tile comes out as a near-black dithered silhouette 10 units larger on each side.

The port does not draw it: `port/src/sim/terrain_state.cpp` handles lowerEdgeCorner's STATE side only (it reads the pivot's x,
sets the view angle and lowers the corner; comment "renderTileAtFiveOffsets(tile, other, amount) first: it draws, ..."), and
`LiveTerrain::lower_edge_corner` (port/src/sim/terrain_live.cpp:126) only makes that state call: unlike render / localRender /
pathUpdateRender it hands no pass to the graphics backend. So this is a Terrain.dll draw path the port does not model, not an
order difference and not a draw-list difference.

The pass that reaches the box: lowerEdgeCorner ordinal 3 of the run = Terrain call seq 20144 of S3.25's calls.txt
(`lowerEdgeCorner(tile (37,25), corner 3, pivot (36,15), angle 0)`), 1,497 px written (x 746..799, y 451..498), 208 of them in
the box. Its neighbour (37,26) (seq 20145 corner 1, 20146 corner 3) adds 177 and 26 px below the box. The other nine of the
twelve calls in this interval write 0 px.

## 2. How it was found

1. **Recording** (`27_gl_record.py` + `27_gl_rec.js`, a copy of re/frida/js/gl_rec.js with a per-kind ordinal of depth-0 entries
   and a keep rule; kept passes are sent at once from the game thread). Run 1 `s327_s2`: Terrain::render ordinals 0..2 and every
   localRender / stripRender / pathUpdateRender begun after render 1 and before render 2 (86 passes: render o0 = seq 9966, render
   o1 = seq 19931, the 82 localRenders 20156..20235, 20269, 20384, then the strip and render after `course`). Run 2 `s327_s2b`: the
   same plus lowerEdgeCorner (body 0x1000a680) as entry kind 5 (98 passes). Both runs SAME as det_b at all 32 checkpoints.
2. **Draw lists**: the port's stream (port.test.glcorpus.render on each recorded snapshot, S3_26 tree's x64 DLL, read-only) equals
   the original's pass lines in all 86 passes of run 1 (`27_streams.py`, 27_out/streams_s327_s2.log). Windows' GL replays render o0,
   render o1 and localRender o5 bit-exact (re/tools/glexact/glreplay.exe, 27_out/glreplay.log).
3. **Box per pass** (`27_analyze.py passes`): before `course` exactly three passes write the box: render o1 (163 px), lowerEdgeCorner
   o3 (208 px), localRender o5 = seq 20161 (143 px). Right after render o1 the original's corner has the port's shape (terrain
   reaching further right, 0x0000 beyond; row 458 `... 29e8 25e7 2e09 0000 ...`); lowerEdgeCorner o3 covers it with the dither
   (written values only 0x0000 329, 0x0020 316, 0x0420 236, 0x0421 616); localRender o5 repaints the left part. In run 1 (no kind-5
   hook) these draws showed as 1,649 px changed between render o1's result and localRender o0's starting DIB, and the GL state stream
   in that interval held, apart from 62 drawLine sequences (bars at x 8..524), exactly 12 blocks with glDisable(0x4000),
   glLightfv(0x4001, 0x1200 / 0x1201 / 0x1202, 0,0,0,0), glEnable(0x4001) and the five glTranslatef: one per lowerEdgeCorner call
   (seq 19994, 19995, 20004, 20144..20146, 20148..20151, 20153, 20154).
4. **Counterfactual** (`27_counterfactual.py`): localRender o5's starting DIB with every pixel the kind-5 passes wrote (and no
   localRender o0..o4 overwrote, 1,448 of 1,649) set back to render o1's result, replayed through Windows' GL: its box equals the
   port's `course` frame (S3.26, 26_img/host) in all 304 px, and differs from the original's own result in 65 px, all inside the
   box (bbox x 781..799, y 451..466): U-1952's 65 px. Row 458: original `29e8 0000 0421 0020 ...`, counterfactual and port
   `29e8 25e7 2e09 0000 ...`. No later pass before `course` writes the box (the strip and render o2 come after it).

Why it persists to `course`: in updateTerrainRender's unmoved-camera branch (re/analysis/terrain/004498a0_updateTerrainRender.md)
the back surface is blitted onto the root (0x00449abd), rebuildTileData runs (0x00449acb) and reaches applyTileElevation's
lowerEdgeCorner, the changed tiles are local-rendered (0x00449b36..0x00449bd4), and the root is copied back to the back surface;
nothing clears the corner again before the next full render (seq 22650, after `course`). The port runs the same order
(port/src/sim/terrain_dll.cpp update_terrain_render_body, lower_step is the only caller of lower_edge_corner).

Pass data (run 2, `k005_lowerEdge_o3`): 374 lines, 40 triangles = 5 x 8 faces, 120 glArrayElement over 9 vertices (indices 0x2c61..
0x2c63, 0x2cf7..0x2cf9, 0x2d8d..0x2d8f: x 1200 / 1250 / 1300, y 15, z 0 / 50 / 100 in g_vertices 0x100b28c8, normals (0, 1, 0)),
textures 0x37 / 0x38 / 0x39 per face and the wall texture 0x206 bound by drawSkirts (no skirt drawn).

## 3. Fix for wave 9 (described, not applied)

- `port/src/sim/terrain_render.cpp` / `terrain_render.h`: `Renderer::render_tile_at_five_offsets(tile, pivot, angle)` transcribed
  from re/match/terrain_edge.cpp (0x10038900): glLoadIdentity, glPushMatrix, glRotated(g_screenScale, 1, 0, 0), glRotatef(45 + angle,
  0, 1, 0), the translate from the PIVOT's position (gated on pTile != NULL), setViewAngle; the per-angle neighbour test (three
  positions, NULL or hidden); when it holds, enable_black_light1 (re/match/terrain_small3.cpp 0x10003830: glDisable(0x4000), the
  three glLightfv(0x4001, ambient / diffuse / specular, zeros) in that order, glEnable(0x4001)), the five glTranslatef + tile_render,
  glDisable(0x4001), glEnable(0x4000); always glPopMatrix, glFlush. terrain_render_entry gets kind 5 (tile, pivot, angle). The GL
  sink must carry glLightfv for light 0x4001 and glEnable / glDisable(0x4001) so the stream equals the original's lines; GlRaster may
  keep ignoring GL_LIGHT1 (exact while its colours are 0, which enableBlackLight1 guarantees, U-1957); WglSink passes them through.
- `port/src/sim/terrain_live.cpp` LiveTerrain::lower_edge_corner: when tile != 0, hand the pass to the backend
  (`to_gfx("terrain.pass", {5, self, tile, tile2, angle})`) BEFORE the state call lowers the corner: the original draws
  (0x1000a6b2) before Tile::lowerCorner (0x1000a6be), and the pass reads the live vertex / normal arrays and tile faces. Its view-angle
  write equals the state side's (both setViewAngle(angle)).
- `port/src/sim/gfx_terrain.cpp`: terrain.pass accepts kind 5 (it runs inside updateTerrainRender's frame, like localRender).
- Oracle: export run 2's kind-5 passes for the draw-list tests (test_terrain_render, glcorpus.all_snaps with kind 5). The recorder
  keeps only a0..a2, so the angle (lowerEdgeCorner's 4th argument) must be added to the snapshot meta first (U-1956; it is 0 in all
  12 calls here, calls.txt). Pixels: the three visible passes (1,497 / 177 / 26 px) through GlRaster and the host GL. Negative control:
  kind 5 skipped gives back the port's present corner (this note's counterfactual, 65 px RED on course).
- Expected rows: with `framegate --gl host`, cov_r_s2_screens course and radial_hover (65 px) and the six report rows (five at 20
  px, rep_roster 11) lose the box difference; S3.26 (26_notes.md group 2, U-1952) places all of these rows' differing px in this
  box, so they are expected to PASS there (not run). Default (GlRaster) census: the box pixels move toward the original, the rows keep GlRaster's
  U-1392 residue elsewhere. Segments / boot unchanged by construction (the backend writes no GameState byte). Other scenarios:
  lowerEdgeCorner occurs in 50 of 91 terrain recordings under log/p5/terrain (483 calls), so other census rows can change too where
  no later full render clears the silhouette before a checkpoint (U-1955).

## 4. Reproduce

    SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 log/roadmap/S3/w8_lock.py run S3.27 --what "<run>" -- \
        <python.exe> log/roadmap/S3/27_gl_record.py --scenario cov_r_s2_screens --instance s3_w8 --label s327_s2b
    ~/bin/py -3.12 re/tools/scenario_refs.py check --scenario cov_r_s2_screens --run s327_s2b        # SAME (32)
    ~/bin/py -3.12 log/roadmap/S3/27_analyze.py passes --label s327_s2b
    cd port/build/S3_26 && SG_PORTSIM_DLL=$PWD/port/build/x64/Release/portsim_c.dll PYTHONPATH=. ~/bin/py -3.12 ../../../log/roadmap/S3/27_streams.py s327_s2
    ~/bin/py -3.12 log/roadmap/S3/w8_lock.py run S3.27 --what glreplay -- bash log/roadmap/S3/27_glreplay.sh k001_render_o1 k007_local_o5   # run 1's steps
    ~/bin/py -3.12 log/roadmap/S3/27_counterfactual.py build; (lock) bash log/roadmap/S3/27_glreplay_cf.sh cf_k019_local_o5; ~/bin/py -3.12 log/roadmap/S3/27_counterfactual.py compare

Run 1's script was 27_gl_rec.js without the kind-5 entry (the only difference; the keep rule's kind 5 was added for run 2).

## 5. Process

- The first take of run 2 was refused because the glreplay hold of the same step was still queued (`already has a live take`):
  w8_lock allows one take per step; the second hold was chained after the first.
- original\ was not touched; recordings in log/p5/gl/corpus/s327_s2*, replays in log/p5/s327/, runs in
  log/c3/alt/refs/cov_r_s2_screens/s327_s2*, images in 27_img/ (all gitignored, game bytes).
