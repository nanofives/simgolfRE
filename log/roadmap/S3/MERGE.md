# S3 merge, wave 1 (S3.1: the terrain draw-list passes; S3.1b: the rasteriser's specular table and tie rule; S3.3: U-1120)

Wave 1 is S3.1 plus S3.1b (S3.1b finished before the wave merged; its only edit outside its own files, the g6 xfail removal in
S3.1's port/test/test_terrain_render.py, is already applied in the tree). files.txt lists wave 1 only. Nothing here is staged. Proven in the S3 mirror
(port/build/S3_head = git archive of c6d7d78b with junctions) by log/roadmap/S3/1_mirror.py, which applies exactly the edits of
section 1 to a source copy and builds port/build/S3_head/port/build/S3_1 (x64) and S3_1_w32 (Win32). Without section 1 the new
entry kinds return SG_E_NO_FUNCTION and test_terrain_render.py's draw-list tests fail (Terrain::render keeps working).

## Shared-file edits (apply in this order)

### 1. port/src/portsim_c.cpp and port/include/portsim_c.h (C ABI routing; text exact, CRLF line ends in portsim_c.cpp)

### 1a. port/src/portsim_c.cpp (C ABI routing, shared file)

The text below is `OLD_RENDER` / `NEW_RENDER` and `OLD_RASTER` / `NEW_RASTER` of log/roadmap/S3/1_mirror.py, which applies them
with an exact-match check (the file has CRLF line ends).

a. In `sg_gl_render`, replace

```cpp
  portsim::gl::GlStreamSink sink;
  try {
    switch (entry) {
      case 2: {
        float angle;
        std::memcpy(&angle, &a1, 4);
        portsim::terrain_render(ctx->ctx, sink, ctx->gl_globals, terrain, a0, angle);
        break;
      }
      default: return SG_E_NO_FUNCTION;
    }
  }
```

by

```cpp
  portsim::gl::GlStreamSink sink;
  try {
    if (!portsim::terrain_render_entry(ctx->ctx, sink, ctx->gl_globals, entry, terrain, a0, a1, a2)) return SG_E_NO_FUNCTION;
  }
```

b. In `sg_gl_render_to_raster`, replace

```cpp
  try {
    switch (entry) {
      case 2: {
        float angle;
        std::memcpy(&angle, &a1, 4);
        // the vertex and normal arrays the faces index are the renderer's globals; the rasteriser reads them for glArrayElement
        const portsim::TerrainGlobals &g = ctx->gl_globals;
        rast->r.set_arrays(g.vertices.data(), g.vertices.size() / 3, g.normals.data(), g.normals.size() / 3);
        portsim::terrain_render(ctx->ctx, rast->r, ctx->gl_globals, terrain, a0, angle);
        break;
      }
      default: return SG_E_NO_FUNCTION;
    }
  }
```

by

```cpp
  try {
    // the vertex and normal arrays the faces index are the renderer's globals; the rasteriser reads them for glArrayElement
    const portsim::TerrainGlobals &g = ctx->gl_globals;
    rast->r.set_arrays(g.vertices.data(), g.vertices.size() / 3, g.normals.data(), g.normals.size() / 3);
    if (!portsim::terrain_render_entry(ctx->ctx, rast->r, ctx->gl_globals, entry, terrain, a0, a1, a2)) return SG_E_NO_FUNCTION;
  }
```

The `(void)a2;` lines at the top of both functions may stay (a2 is now used) or go.

### 1b. port/include/portsim_c.h (documentation of sg_gl_render)

Replace

```
 * and write the GL calls it makes as text (portsim/sim/gl_sink.h): entry 2 = Terrain::render(a0 = tile address,
 * a1 = angle bits). out gets the text
```

by

```
 * and write the GL calls it makes as text (portsim/sim/gl_sink.h): entry 1 = stripRender(a0 = pivot, a1 = direction,
 * a2 = angle bits), 2 = Terrain::render(a0 = tile address, a1 = angle bits), 3 = localRender(a0 = tile, a1 = pivot,
 * a2 = angle bits), 4 = pathUpdateRender(a0 = pivot, a1 = angle bits); SG_E_NO_FUNCTION otherwise. out gets the text
```

No signature changes, no new export; the Win32 runner needs nothing (it has no sg_gl ops).

### 1c. port/src/sim/tick.cpp, function run_frames (S3.3, U-1120)

Shared file edited (in the mirror only, port/build/S3_head/port/src/sim/tick.cpp): `port/src/sim/tick.cpp`, function `run_frames` (the sg_run_frames / sg_run_boot driver).
Patch: log/roadmap/S3/3_tick.patch (unified diff against the mirror's HEAD copy; apply by hand if the shared file has moved: two hunks).

1. Boot's last frame, the block `if (boot && static_cast<size_t>(k) == frames.size())` ("a boot run's last frame"): wrap
   `tk::tick_frame_io` with `catch (const NeedInput &)` that calls `gfx_end_of_input(ctx)` and records `out_of_input`; the detail reads
   "the last frame ran to the end of the trace: a Blocking pump with no input left". `FrameExit ex` starts as `FrameExit::Wait`.
2. End-checkpoint block (`catch (const NeedInput &e)` that builds `stop`): add `gfx_end_of_input(ctx);` so snapshots still pending get the back buffer.

No other shared file changes (gfx.cpp, tick_hook.h, framegate.py, portsim_c.cpp untouched). framegate.py's `3_rerun_noend.py` relabel of S0.3 is no longer needed.
Tests to add to the suite list: port/test/test_gfx_eoi.py (mine, 5 tests, ~95 s).

The patch text is log/roadmap/S3/3_tick.patch (two hunks; the comment cites U-1120). Tests: port/test/test_gfx_eoi.py (new, 5 tests x64 and Win32).

### 2. UNCERTAINTIES.md

Existing row U-1120 (S0): status `resolved`, with `Resolved by roadmap S3.3: the -11 was NeedInput (a MissingResource) thrown by the boot's last frame's Blocking pump when no recorded input is left (boot runs with to_end), not the pending snapshot; run_frames now catches it there and in the end-checkpoint block and calls gfx_end_of_input (log/roadmap/S3/3_notes.md).` appended.

Append these eight rows (U-1300 .. U-1304 from S3.1, U-1300 already marked resolved by S3.1b; U-1390 .. U-1392 from S3.1b) at the end of the table (address cells empty: the rows concern tooling, the Terrain.dll draw passes or
evidence, with no single hex address):

| U-1300 |  | tooling | [roadmap S3.1] GlRaster misses the G2 criterion on 16 of the 28 draw-list passes of recording g6 (cov_b_paths, tiles fuzzed with 60 % type 7 and a non-hidden border column, view angle 90): one-level pixels 1.56 .. 2.47 % of the drawn (limit 1.5 %) on 13 stripRender / localRender passes, 108 .. 145 pixels more than one level off on 3 (limit 70). Replaying the ORIGINAL's recorded stream through GlRaster gives the same numbers (i012_raised_strip 4,421 one-level / 5 big; i005_raised_pathupd 1,142 / 145), and the port's stream equals the original's line for line on all 28, so the miss is the rasteriser's on that state, not the renderer's. Split (log/roadmap/S3/1_g6_diag.py): i012 opaque 3,958 of 4,421 one-level pixels; i005 blended 127 of 145 big pixels; the same strip pass on the lightly fuzzed g5 state (5 % type 7) has 627 opaque one-level pixels. 81 of i012's 1,374 binds name texture 0 (incomplete: drawn untextured). Not measured: which triangles (rotated type-7 textures, untextured lit colour, blended path bands over them) carry the errors. The g6 raster tests are xfail (non-strict) in port/test/test_terrain_render.py. Resolved by roadmap S3.1b: the miss was the generic GL's specular table (U-1390; the untextured type-7 tiles' constant lit colour sat on a quantisation threshold) and its tie rule for centres exactly on a horizontal edge (U-1391; i005's 145 pixels); all 28 g6 passes now meet G2 from the original's stream and from the port's (log/roadmap/S3/1b_notes.md). | Per-triangle error map of g6/i012 and g6/i005 (sg_raster_get_tri_ids + the pass's bind / glBegin sequence), then a probe of the generic GL on untextured lit triangles and on blended bands at angle 90. | resolved |
| U-1301 |  | evidence | [roadmap S3.1] A draw-list pass that calls no glArrayElement leaves no snapshot (gl_rec.js keeps a pass only when maxIdx >= 0), so the passes that draw no tile are not judged: stripRender with a direction outside 1..8 (injected as 9 in every set: no snapshot), directions 2 / 3 / 4 at zoom shift 2 around the g4 pivot, and pathUpdateRender in two of g2's three injection groups (sequence numbers 15..19 and 29..33: no tile drawn, why not read). For them the port emits the frame only (glLoadIdentity, glPushMatrix, glRotated, glRotatef, glTranslatef when the pivot is non-NULL, glPopMatrix, glFlush, and for an odd direction the two nested frames), read from re/match/terrain_strip.cpp / terrain_pathrender.cpp, not compared with a recording. | Let gl_rec.js keep a pass with maxIdx < 0 when it was injected (no arrays to copy) and compare those streams. | open |
| U-1302 |  | behavioural | [roadmap S3.1] pathUpdateRender clips its left edge to x + 1 (`left = pivot->getX() + 1` when x < 15 << zoom; the other three edges clip to the grid), so it walks column x = 0 where the other edges stop at the grid. The difference shows only on a non-hidden tile with a path in column 0: in 608 of the 681 Terrain objects of port/corpus (gl and terrain) the whole column 0 is type 0x14 (hidden); the others are construction-time states (type 4 before the course's setType calls) or fuzzed ones. So the quirk is unobservable in every recorded game state; control 12 reads RED only on the constructed g6 state (6 passes). Whether a game state with a non-hidden border column exists was not read from the exe's course code. | Read where the exe writes the border ring's type (rebuildTerrainGrid / generateRandomCourse) to settle whether column 0 can ever be non-hidden. | open |
| U-1303 |  | behavioural | [roadmap S3.1] localRender asserts pTile != NULL before its first GL call (the debug build's assert, re/match/terrain_localrender.cpp: base line + 3); the port throws a Fault there instead of the CRT's assert report. No recording or exe caller passes NULL (updateTerrainRender passes the list's tiles). pathUpdateRender and stripRender read a NULL pivot after their GL frame and fault there in the port as in the original's code order; also never recorded. | A Terrain-boundary caller that can pass NULL, or none (then nothing to do). | open |
| U-1304 |  | evidence | [roadmap S3.1] The G2 check is vacuous on four kind-3 passes: c3 hole, c5 hole_local and c6 hole_local draw 0 pixels in GlRaster (their 25 tiles are off the viewport around the translated pivot) and the original changed none (dib_pre = dib_post); their streams are still compared line for line. 14 of the other 17 kind-3 passes redraw unchanged tiles over an identical frame (changed 0 pixels), so their G2 compares the redraw with the earlier full frame's pixels. | A localRender recorded right after a state change of its tiles (an edit), where dib_pre and dib_post differ (c4's five passes are those: 465 .. 506 pixels changed). | open |
| U-1390 |  | behavioural | [roadmap S3.1b] The generic GL's specular term is a table of 256 entries over [t, 1], t = 0.0005^(1 / shininess), read at the NEAREST entry without interpolation (i = round((n.h - t) * 255 / (1 - t)), value (t + i * (1 - t) / 255)^shininess, 0 for n.h <= t). Measured with untextured constant-colour quads through re/tools/glexact/glreplay.exe (port/test/glprobe.py untex, log/roadmap/S3/1b_litfit.py): 0 of 4,923 quads outside the interval their 16 dither cells allow (white specular only, four seeds), against 793 for the exact pow; every pixel equal at shininess 1, 5, 13, 40 and 128. Not determined: whether the index and the table are evaluated in float32 or double (both give 0 violations), and the behaviour with a positional light or a local viewer (the terrain uses neither: a directional light 0, viewer at infinity). | A probe with a positional light / GL_LIGHT_MODEL_LOCAL_VIEWER, only if a future caller sets them. | open |
| U-1391 |  | behavioural | [roadmap S3.1b] Pixel centres exactly on an edge: the rule `an edge owns them when it points to +y, or to +x when horizontal` (spans: ymin <= y + 0.5 < ymax, xl < x + 0.5 <= xr) is the best of the six simple rules on the tie probe (port/test/glprobe.py tie: 48 coverage differences of about 4,600 pixels against 288 .. 923), and it takes g6/i005_raised_pathupd from 145 to 10 pixels more than one level off. The 48 left are centres exactly on a vertex (40) or on a diagonal edge (8), where the real GL's choice follows no direction rule (both signs occur for the same edge direction): a fixed-point edge walker's rounding, not modelled. In the recorded corpora such pixels are what is left of the pixels more than one level off (at most 13 per pass, c7 fuzz frames). | A probe of the real GL with one triangle per scene and its vertex moved along the 1/256 grid around a centre, to read the edge walker's start and step arithmetic. | open |
| U-1392 |  | evidence | [roadmap S3.1b] What is left of the one-level noise after U-1390 / U-1391 is 0.0 .. 0.21 % of the drawn pixels per pass on every recorded pass (c3 .. c7, t3, g2 .. g6; mean 0.03 .. 0.06 % per corpus), down from 0.33 .. 0.49 % (1.21 % on g6); its origin was not split further. Section 18's `per-triangle constant shift would still remove about 70 %` measurement (U-1076) was taken with the exact pow, whose error is constant per flat-normal triangle, so that residual needs re-measuring with the table before it is attributed to the setup. | Re-run section 18's per-triangle fit (sg_raster_get_tri_ids) on t3 / c7 with the table and the tie rule. | open |
### 3. re/PHASE5_TERRAIN_PLAN.md: append items 19 and 20 after item 18 of section 6

```
19. DONE, the draw-list passes (roadmap S3.1, 2026-10-24; log/roadmap/S3/1_notes.md): localRender, stripRender and pathUpdateRender
    with drawTile's sorted draw list and drawTileObjects' wall recursion in terrain_render.cpp (terrain_render_entry for the four
    entry kinds). Evidence: 237 draw-list passes of the original equal line for line (33 localRender, 160 stripRender, 44
    pathUpdateRender: c3 .. c7 plus the new recordings g2 .. g6 of re/tools/gl_record.py --scenario, injected on the game thread
    through an Interceptor.replace of render / localRender, every hooked run SAME as its pinned reference), and every pass rasterised
    from the port's stream meets G2 except 16 of g6's constructed type-7-heavy state, where the original's own stream misses it
    identically (U-1300, resolved by item 20). Controls 9 .. 14 RED; 12 only on g6 (U-1302).
20. DONE, the specular table and the tie rule (roadmap S3.1b, U-1300 / U-1390 / U-1391; 2026-10-24; notes log/roadmap/S3/1b_notes.md).
    Recording g6 (60 % type-7 tiles) missed the criterion on 16 of 28 passes with the ORIGINAL's own stream. Split by block category
    against glreplay.exe (log/roadmap/S3/1b_probe.py): the untextured tiles (bound texture 0, drawn with the lit colour) had 6.25 %
    one-level pixels, all on one constant colour sitting 0.0034 (8-bit units) above a quantisation threshold. Probes of lit
    untextured quads (glprobe.py untex) showed ambient and diffuse exact and the specular term a step function: the generic GL reads
    (n.h)^shininess from a 256-entry table over [t, 1], t = 0.0005^(1 / shininess), at the nearest entry (0 of 4,923 probe quads
    outside their intervals; exact at shininess 1 .. 128; gl_raster.cpp spec_lookup, tune 27 = 1 restores the pow). It was also most
    of the one-level noise everywhere: c7 0.46 -> 0.06 %, t3 0.49 -> 0.05 %, g2 .. g5 0.37 .. 0.49 -> 0.03 .. 0.04 %. g6/i005's
    145 pixels more than one level off were path bands whose horizontal bottom edge rounds to y = 344.5 exactly: centres on a
    horizontal edge belong to edges pointing to +x, not -x (tie probe 48 coverage differences against 288 for the old rule; tune 10 = 2
    restores it); 10 are left. Result: all 271 recorded-stream replays and all 237 draw-list chains meet G2, no pass worse.
```

### 3b. UNCERTAINTIES.md, existing row U-1076: append to its description

`S3.1b: the specular table (U-1390) removes most of the one-level noise this row describes (c7 mean 0.46 % -> 0.06 %, t3 0.49 % -> 0.05 %); the per-triangle residual must be re-measured with it (U-1392).`

### 4. CLAUDE.md: append after the "Roadmap S0" paragraph

```
Roadmap S3 wave 1, the terrain draw-list passes and the rasteriser's last two rules (2026-10-24, steps S3.1 / S3.1b, notes
log/roadmap/S3/1_notes.md, 1b_notes.md, U-1300..U-1304, U-1390..U-1392):
port/src/sim/terrain_render.cpp now ports localRender (25 window tiles, no culling of its own; drawTileObjects' camera-facing wall
neighbours, recursive and culled against pTile), stripRender (odd directions are two complete nested passes, 8 + 2, 2 + 4, 4 + 6,
8 + 6, each with its own push / list / flush) and pathUpdateRender (no setViewAngle: it keeps the quadrant the previous pass left;
left edge clipped to x + 1, U-1302), with drawTile's sorted insert (its `other angle` branch test never holds, so the secondary walk
always runs) and the draw list empty at every pass entry (137 snapshots); terrain_render_entry serves sg_gl_render(_to_raster)'s
entry kinds 1 strip, 2 render, 3 local, 4 pathUpdate. Recorder: re/tools/gl_record.py --scenario runs a pinned scenario through
scenario_refs.py with gl_rec.js from process start and --inject queues draw passes on the game thread (Frida runs NO hook for a
call made inside an Interceptor.attach callback, so the injection is an Interceptor.replace of render / localRender; state the
module observer's tryInstall reads must be declared before it). Recordings g2 .. g6 (instances\s3_1, cov_b_holes18 / cov_b_paths,
every run SAME as its reference; port/corpus/gl/<label> via `port.test.glcorpus export`). Evidence: 237 draw-list passes equal the
original line for line (33 localRender, 160 stripRender, 44 pathUpdateRender; zoom 0 / 1 / 2, four angles, directions 1..8);
GlRaster from the port's stream meets G2 on 209 / 209 passes outside g6 (one-level 0.08 .. 1.25 % of drawn, at most 24 beyond);
g6 (60 % type 7, border column not hidden) missed on 16 / 28 with the original's own stream too (U-1300), which S3.1b traced to two
rules of Windows' generic GL that GlRaster lacked (found with glreplay.exe, which replays g6 bit-exact): the specular term is a
256-entry table over [0.0005^(1/n), 1] read at the nearest entry, not the exact pow (gl_raster.cpp spec_lookup; 0 of 4,923 probe
quads wrong; tune 27 = 1 restores the pow, U-1390), and a pixel centre exactly on a horizontal edge belongs to the edge pointing to
+x (tune 10 = 2 restores the old rule, U-1391). Now every pass meets G2 (g6 28 / 28) and one-level pixels fell everywhere: the 209
other draw-list passes 0.46 -> 0.03 % mean, c7 + t3 0.47 -> 0.06 %; the residual 0 .. 0.21 % is not split further (U-1392);
controls 9 .. 14 RED on the streams (14 redraws a tile over itself: blind in pixels). Unjudged: passes that draw no tile leave no
snapshot (U-1301), NULL tile / pivot paths (U-1303), vacuous G2 on 4 localRender passes (U-1304). The renderer is still reached only
through sg_gl_*; LiveTerrain's draw passes into the back buffer are step S3.2. U-1120 resolved (S3.3): a boot run with to_end
ended in its last frame's Blocking pump with no input left, NeedInput (a MissingResource) surfaced as -11 and pending snapshots
were never taken; run_frames now takes them there (cov_a_champ, championship, cov_k3_career: 22 census rows run).
```

## Gate (expected numbers, on a build holding wave 1; lead's mirror build port/build/S3_head/port/build/S3_1b = S3.1 + S3.1b)

Commands in log/roadmap/S3/1_notes.md section 4 and 1b_notes.md section 3 (from the S3 mirror root, SG_PORTSIM_DLL = .../S3_1b/Release/portsim_c.dll):
- port/test/test_terrain_render.py: 522 passed (lead, S3_1b build with RASTER_OPEN empty, log/roadmap/S3/lead_w1_terrain_render.log);
  needs port/corpus/gl/c3..c6 and
  g2..g6 (local data, exported as in 1_merge.md section 5).
- test_gl_raster 177 passed; test_terrain_state 500 passed; test_terrain_live 144 + test_terrain_gl_setup 6 = 150 passed (lead, S3_1b
  build, log/roadmap/S3/lead_w1_live_glsetup.log); port
  test_static 6 passed.
- segments: x64 205 / 205 PASS and Win32 205 / 205 PASS, 0 digests differ, on S3.1's and on S3.1b's builds
  (log/roadmap/S3/1_segs_*.tsv, 1b_segs_*.tsv).
- boot: HEAD's 61 scenarios 58 PASS / 2 KNOWN / 1 NO_FRAMES, 0 rows differ from log/roadmap/S0/1_boot_gate.tsv (1_boot_compare.log, S3.1's build; S3.1b changes
  only gl_raster, which no simulation or boot path reaches).
- repo static: 600 / 2 skipped as BASELINE in the shared tree (in a git-archive mirror two shim-DLL tests fail for want of
  shim/build/winmm.dll: log/roadmap/S3/1_static_shim_tree.log shows them passing in the shared tree).

- S3.3 (on its own mirror build with 3_tick.patch): segments x64 205 / 205 and Win32 205 / 205, 0 rows differ from
  log/roadmap/merges/S6-1/segs_*.tsv (3_segs_*_compare.log); boot 58 / 2 / 1, 0 rows differ from S0 (3_boot_compare.log);
  test_gfx_eoi 5 passed x64 and Win32 (4 of 5 RED on the pre-fix DLL); framegate: the 22 rows of cov_a_champ, championship and
  cov_k3_career that were NOT_RUN / KNOWN now have snapshots.

## Local data (gitignored, not part of a commit)

port/corpus/gl/c3 .. c6 and g2 .. g6; log/p5/gl/corpus/g1 .. g6 (raw recordings); instances\s3_1.
