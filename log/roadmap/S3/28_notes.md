# S3.28 notes (wave 9, candidate 2: lowerEdgeCorner's black-light pass; U-1952 / U-1955 / U-1956 / U-1957)

Worker: claude2 (Opus), 2026-10-31. Lead: cmvu18cc81fcpp715vy6fd2dv. Tree port/build/S3_28 (`w8_assemble.py --dst S3_28 --commit
2291a342`), plain stack for the edits port/build/S3_28_base. Game slot: instance `instances\s3_w8`. Every heavy run went through
`w8_lock.py run S3.28` (one take at a time). U ids used: U-1960, U-1961 (28_uncertain.tsv; U-1955 / U-1956 / U-1957 settled there).

## 1. Result

Terrain::lowerEdgeCorner's draw half is ported. `Renderer::render_tile_at_five_offsets` (port/src/sim/terrain_render.cpp, reading
reference re/match/terrain_edge.cpp, Terrain.dll 0x10038900) sets localRender's transform without a draw list (glLoadIdentity,
glPushMatrix, glRotated(screen scale, 1, 0, 0), glRotatef(45 + angle, 0, 1, 0), glTranslatef from the pivot, setViewAngle), tests the
three neighbours toward the camera (angle 0: (x+1, y), (x, y-1), (x+1, y-1); 90: (x+1, y), (x, y+1), (x+1, y+1); 180: (x-1, y),
(x, y+1), (x-1, y+1); any other angle: (x-1, y), (x, y-1), (x-1, y-1); outside the grid or type 0x14 = open) and, when one is open,
runs `enable_black_light1` (re/match/terrain_small3.cpp 0x10003830: glDisable(GL_LIGHT0), glLightfv(GL_LIGHT1, AMBIENT / DIFFUSE /
SPECULAR, zeros), glEnable(GL_LIGHT1)), then Tile::render at the cumulative glTranslatef (10, 0, -10), (-20, 0, 0), (0, 0, 20),
(20, 0, 0), (-10, 0, -10), glDisable(GL_LIGHT1), glEnable(GL_LIGHT0); always glPopMatrix, glFlush. A NULL tile faults (the debug
assert). It is terrain pass kind 5 (`kEntryRenderTileAtFiveOffsets`, portsim/sim/terrain_render.h: a0 tile, a1 pivot, a2 angle =
lowerEdgeCorner's arguments 0, 2, 3), served by terrain_render_entry, so sg_gl_render / sg_gl_render_to_raster take it too.

`LiveTerrain::lower_edge_corner` (terrain_live.cpp) hands `terrain.pass {5, self, tile, pivot, angle}` to the backend BEFORE the state
call when tile != 0 (the original draws at 0x1000a6b2 before Tile::lowerCorner at 0x1000a6be). gfx_terrain.cpp's terrain.pass runs it
on GlRaster or the host GL like the other kinds; a kind-5 pass takes no pass number (p<j> copies), because the original's pass copies
count kinds 1..4 only (U-1961). GlSink already carried glLightfv / glEnable / glDisable; WglSink passes them through; gl_raster.cpp /
.h are untouched (light 1 with zero colours adds nothing to GlRaster's sum, U-1957).

## 2. Recorder (U-1956)

`28_gl_rec.js` (a copy of 27_gl_rec.js) records a3 for every entry, makes renderTileAtFiveOffsets (body 0x10038900) an entry of kind 5
(nested in lowerEdgeCorner it changes nothing, called directly it starts the pass), keeps kind-5 passes that draw nothing (no
glArrayElement) and keeps every organic kind-5 pass. `28_gl_record.py --inject-at course` calls renderTileAtFiveOffsets directly on the
game thread after `course` (27_gl_rec.js runInject: at the next outermost render / localRender, back buffer written back after) for
the tiles on the four edges of the land (x, y = 12..37 at `course`; every other tile is type 0x14, read from the recorded Terrain
object) and one inner tile, at 0, 90, 180 and -90 (the game's fourth angle), the pivot two tiles inward. Recording s328_s2b
(log/p5/gl/corpus/s328_s2b, exported kind 5 to port/corpus/gl/s328_s2b, both local): `scenario_refs.py check` SAME as det_b at all
32 checkpoints; 12 organic passes (angle 0, as all organic calls of the pinned terrain recordings) and 20 direct ones. The direct
passes split exactly as the matched source predicts: (37,25) draws at 0 / 90, (12,25) at 180 / -90, (25,37) at 90 / 180, (25,12) at
0 / -90, (25,25) never. A first recording s328_s2 (local only) put the tiles inside the hidden ring, where every tile draws at every
angle; it does not tell the neighbour sets apart and is not exported.

## 3. Oracle (port/test/test_terrain_s3_28.py)

- Streams: all 32 kind-5 passes equal the original's line for line, with glLightfv's light and parameter compared (glcorpus.normalize
  drops every non-pointer argument of a vector call too, so the file has its own `strict`).
- Pixels (28_out/pixels_s328_s2b.txt): 11 of the 32 passes write pixels on the original's back buffer: organic o3 / o4 / o5 (1,497 /
  177 / 26 px, S3.27's numbers) and the 8 drawing direct passes (1,085 .. 1,419 px each, at 0, 90, 180 and -90). The other 21 write
  none (the 9 other organic passes draw their triangles but change no pixel of the back buffer; why was not measured; the 12
  non-drawing direct passes have no triangle).
  - GlRaster from the port's stream over the recorded state stream and starting back buffer: G2 met on all 32; 2 px differ in all
    (o3: 1 px beyond one level of 1,497; i012 at 180: 1 px one level of 1,325), the rest exact.
  - Host GL (re/tools/glexact/glreplay.exe over a corpus copy holding the port's pass): equal to the original's back buffer in every
    pixel on all 32.
- The frame: cov_r_s2_screens `course`, map-corner box 781..799 x 451..466 (framegate's boot, LiveTerrain, GlRaster): 0 px beyond one
  level; with gfx.family.terrain 15, 65 px differ, 20 beyond one level.
- pytest test_terrain_s3_28 x64: 107 passed (first run 106 + the box control with a wrong threshold, fixed: the control's 65 / 20 are
  the wave 8 census's numbers for this row; re-run of `-k map_corner` in 28_out/g2_s328_box.log).

## 4. Gate

All heavy runs through `w8_lock.py run S3.28`; logs and TSVs in log/roadmap/S3/28_out/.

| gate | result |
|---|---|
| cov_r_s2_screens' 8 split FAIL rows (course, radial_hover, rep_comments / financial / histograph / report1 / shortcuts, rep_roster) | all 8 split PASS; raw: rep_histograph, rep_report1, rep_roster PASS (0 px), course 69 -> 4, radial_hover 68 -> 3, rep_comments 24 -> 4, rep_financial / rep_shortcuts 21 -> 1, none beyond one level (GlRaster terrain residue, U-1392) |
| full census, 698 rows, raw + split (28_census.sh, 28_census_cmp.py -> census_cmp.tsv) against w8_out/census_split.tsv | raw 123 / 565 / 0 / 10 -> 126 / 562 / 0 / 10, split 677 / 11 / 0 / 10 -> 685 / 3 / 0 / 10; 0 PASS lost and 0 rows with more differing px in raw and in split; EXACTLY 8 rows move, the 8 above (no other row of any scenario changes) |
| segments x64 | 205 / 205 PASS, 0 rows / digests differing from w8_out/segs_x64.tsv |
| boot x64 | 66 PASS / 2 KNOWN / 2 NO_FRAMES, all 70 rows equal to w8_out/boot_x64.tsv |
| segments Win32 | 205 / 205 PASS, 0 rows / digests differing from w8_out/segs_w32.tsv |
| boot Win32 | 66 PASS / 2 KNOWN / 2 NO_FRAMES, all 70 rows equal to w8_out/boot_x64.tsv |
| test_static | 6 passed (after the fix of section 6) |
| test_terrain_s3_28 x64 | 107 passed (106 + the box control re-run with its corrected threshold, 2 / 2 in g2_s328_box.log) |
| test_terrain_render x64 | 522 passed |
| test_gfx x64 | 25 passed |
| test_framegate_split x64 | 17 passed |
| test_terrain_live x64 (one hold, g2_terrain_live.log) | 432 passed, 0 skipped |
| test_terrain_s3_28 Win32 (SG_BACKEND=win32, one hold, g2_w32_s328.log) | 4 passed, 103 skipped: the 2 corpus-file tests and the 2 map-corner box tests (framegate's boot on the Win32 runner: box 0 px beyond one level; control gfx.family.terrain 15 RED) ran; the 103 DLL-bound tests (32 stream, 32 raster, 32 host-GL, 7 renderer controls) skip by design, see below |

The 3 split FAIL rows left are wave 8's group 5 (cov_k3_camera y_inj3, explained) and U-1826 / U-1829.
Why the kind-5 stream / pixel tests are x64-only: they drive the renderer through the ctypes DLL's sg_gl_render,
sg_gl_render_to_raster and sg_raster_* entry points (port/test/glcorpus.py, glraster.py). The Win32 runner (port/test/lib32.py,
runner32.cpp) serves the C ABI over a pipe with a fixed operation table (33 ops: state, calls, frames, boot, files, LiveTerrain) and
has no sg_gl / sg_raster op and no ctypes DLL (`self.dll = None`), so under SG_BACKEND=win32 those tests skip with that reason
(test_terrain_s3_28.py's `_skip(lib)`; the file was changed for this after the x64 run, which takes the same path as before on x64).
The renderer's code is the same C++ on both builds, and the Win32 build's kind-5 passes run inside every Win32 framegate boot (the 2
box tests above, the segments / boot gates do not draw).

Not run: test_gfx with SG_BACKEND=win32 and the pass-log run 28_passlog.sh (which kind-5 passes of course_season / cov_b_holes18 /
cov_b_paths / cov_k1_terrain / cov_l_bridge write pixels and which later render clears them); the first test_terrain_live run was
stopped mid-file (section 6) and re-run whole in its own hold (above).

### 4a. The 8 rows that move, and the lowerEdgeCorner calls behind them

Before = w8_out/census_split.tsv, after = 28_out/census_split.tsv (GlRaster, the default renderer). Raw columns: total differing px
(beyond one level); split columns: 2D px / terrain-origin px.

| cov_r_s2_screens checkpoint | raw before | raw after | split before | split after |
|---|---|---|---|---|
| course | FAIL 69 (20) | FAIL 4 (0) | FAIL 65 / 4 | PASS 0 / 4 |
| radial_hover | FAIL 68 (20) | FAIL 3 (0) | FAIL 65 / 3 | PASS 0 / 3 |
| rep_comments | FAIL 24 (20) | FAIL 4 (0) | FAIL 20 / 4 | PASS 0 / 4 |
| rep_histograph | FAIL 20 (20) | PASS 0 (0) | FAIL 20 / 0 | PASS 0 / 0 |
| rep_financial | FAIL 21 (20) | FAIL 1 (0) | FAIL 20 / 1 | PASS 0 / 1 |
| rep_report1 | FAIL 20 (20) | PASS 0 (0) | FAIL 20 / 0 | PASS 0 / 0 |
| rep_shortcuts | FAIL 21 (20) | FAIL 1 (0) | FAIL 20 / 1 | PASS 0 / 1 |
| rep_roster | FAIL 11 (11) | PASS 0 (0) | FAIL 11 / 0 | PASS 0 / 0 |

Every px that leaves these rows is a 2D-split px of the map-corner box 781..799 x 451..466 (S3.26 group 2 placed all of them there);
the terrain-origin px (U-1392's GlRaster residue) are unchanged in every row. The calls (port/corpus/terrain/pin_cov_r_s2_screens/
calls.txt, the 12 organic lowerEdgeCorner calls of the run, all before `course`, all with pivot (36,15) and angle 0; ordinal o<k> of
the recording = the k-th of them, corners 3,5,5,3,1,3,1,5,5,7,1,3 in both):

| ordinal | Terrain call seq | tile, corner | px written on the original's back buffer (= the port's through the host GL) |
|---|---|---|---|
| o3 | 20144 | (37,25), 3 | 1,497 (x 746..799, y 451..498; 208 in the box, S3.27) |
| o4 | 20145 | (37,26), 1 | 177 |
| o5 | 20146 | (37,26), 3 | 26 |
| o0..o2, o6..o11 | 19994, 19995, 20004, 20148..20151, 20153, 20154 | (see calls.txt) | 0 |

No other row of the 698 moved: 28_census_cmp.py compares status, total_px, beyond_px, split status, twod_px and terrain_origin_px of
every row and finds exactly these 8 different (census_cmp.tsv holds only them).

### 4b. Why no other row moves (U-1955)

The organic lowerEdgeCorner calls of the pinned terrain recordings (port/corpus/terrain/pin_*/calls.txt: course_season, cov_b_holes18,
cov_b_paths, cov_k1_terrain, cov_l_bridge 10 each, cov_r_s2_screens 12) all pass angle 0. The census measured the effect directly:
with every kind-5 pass now drawn, no checkpoint of any other scenario changes in any pixel (raw total_px and split columns equal to
wave 8 in all 690 other rows). Which of those scenarios' kind-5 passes write pixels at all, and which later Terrain::render clears them
before a checkpoint, was not measured (the pass-log run above).

## 5. Controls

| control | reads | where |
|---|---|---|
| sg_gl_negctl 15: renderTileAtFiveOffsets draws nothing | RED on all 32 streams; RED in GlRaster pixels of o3 | test_renderer_controls_are_red_on_the_streams, _reach_the_raster_pixels |
| sg_gl_negctl 16: second offset -10 | RED on the 20 drawing streams; RED in GlRaster pixels of o3 | same |
| sg_gl_negctl 17: light 1 ambient (0.2, 0.2, 0.2, 1) | RED on the 20 drawing streams and in the host GL's pixels of o3; BLIND in GlRaster's pixels by construction (light 0 only, U-1957) | test_renderer_controls_are_red_on_the_streams, test_light1_control_reaches_the_host_gl_pixels |
| sg_gl_negctl 18: 90 / 180 neighbour sets swapped | RED on the 4 direct passes whose arm flips (i006, i007, i011, i012); organic passes are angle 0 | test_neighbour_set_control_is_red_on_the_streams |
| gfx.family.terrain 15: LiveTerrain hands no kind-5 pass over | RED: cov_r_s2_screens `course` box 65 px differ, 20 beyond one level (= the wave 8 row) | test_map_corner_box_negctl_red |

## 6. Process

- w8_lock allows one take per step: a recording queued while the census held S3.28 was refused (`already holds the lock`). The census
  loop was stopped between batches; the batch running then wrote NOT_RUN rows with an empty ERROR for cov_a_build / build2 / build3 /
  cov_a_career (its later worker spawns failed after the stop); their cache entries were moved to 28_out/notrun_*.json and the census's
  final `--all --resume` pass ran them again. A first queue script waited on `w8_lock.py status` through stdout while status prints to
  stderr, so it went on early; fixed (`2>&1`), its stray loops stopped by PID.
- Control 18 was added after the census build; at mode 0 the code is the same, and the gates ran on the rebuilt DLL. The terrain_live.cpp
  comment fix of the static test (two Terrain.dll addresses cited, now named by function) came after the x64 build: comment only.
- My first gate script ran six pytest files in ONE hold (pytest_x64); the lead's rule (W9_COMMON.md, 2026-10-31) is one pytest file
  per hold, and that chained hold delayed I21 and S3.29. I stopped it in its last file, test_terrain_live, by killing the running
  pytest itself (w8_lock released normally); the lead's order, which reached me afterwards, was to let the running file finish and
  kill only the chain's wrapper. 28_gates2.sh runs one file / one segments or boot step per hold. build_w32 of the first script exited
  127 with no output (cause not determined); segs_w32 built the Win32 runner on demand.
- A background shell's 2-hour limit killed queue 2's wrapper shell, not its scripts (they ran on and were stopped by PID later).
- Delivery checks (28_out/assemble_v.log, assemble_root.log):
  - `28_assemble.py --dst S3_28v` (w8_assemble.py, then the edits) gives a tree equal to port/build/S3_28 in every port file.
  - `w8_assemble.py --dst S3_28r --commit 2291a342` followed by `28_assemble.py --root port/build/S3_28r --only-28` does too
    (`diff -rq` empty, build / corpus excluded).
  - A second `--root ... --only-28` on that tree is refused at edit 2 (`already applied`) and leaves the tree unchanged.
  - `--root port/build/S3_28_base --only-28 --dry` applies all 11 edits and the new file.
  - Repeated after the Win32 skip change of test_terrain_s3_28.py (28_edits.py regenerated): fresh w8_assemble + `--root` equal to
    port/build/S3_28 again, second apply refused unchanged.
- The session's process exited at 14:42 after the Win32 boot had landed (the account moved from claude2 to claude3); the notes'
  sections 4a and the root check were written after the resume.
