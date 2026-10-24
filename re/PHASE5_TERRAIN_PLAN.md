# Phase 5, step 3: the Terrain.dll renderer (OpenGL sub-plan)

Status: PLAN (2026-10-23). Nothing here is built yet. Facts below are from files in the repo; items marked MEASURE are not known.

## 1. What exists

- `portsim::TerrainApi` (port/include/portsim/sim/terrain_api.h) is the boundary: 20 Terrain.dll methods the exe imports plus engine
  services (`render_strip`, `blit_full`, `blit_screen`, `window_paint`, `acquire_dc`, `os_major_version`, `operator_new`).
  Phase 3.1a measured that no sim-class write of any exe caller depends on a value Terrain.dll returns (U-0607, U-0800), so rendering
  is a pure sink. The test implementation is `terrain_fake.cpp` (recording); tick.cpp has a `HeadlessTerrain` for heap objects.
- The state the renderer needs already lives in the 32-bit address space: Terrain object (width +0x14, height +0x18, tile array +0x3a4,
  0x248-byte tiles: type +0x24, corner heights +4 + 8k, wall bytes +0x234 + side).
- Matched sources of every Terrain.dll function (re/match/terrain_*.cpp, 170 functions at 100%) are the reading reference; all 170 are C4.
- Imports used for drawing: OPENGL32 45 functions (fixed-function only: glBegin/glEnd, vertex arrays via glArrayElement, glLightfv,
  glMaterialfv, glTexImage2D, glOrtho, glRotated/glRotatef/glTranslatef/glScalef, glBlendFunc, glLineWidth), GLU32 gluBuild2DMipmaps /
  gluBuild1DMipmaps, GDI32 ChoosePixelFormat/SetPixelFormat (g_pfd 0x10063e10), wglCreateContext/MakeCurrent/DeleteContext. There is no
  glReadPixels and no SwapBuffers import: the frame is presented with glFlush (Terrain::stripRender ends in glPopMatrix, glFlush).
- Assets: Data\Textures\<Parkland|Tropical|Desert|Links>\<type><A..E>000N.bmp, Path*.tga (8 files), CliffTest.bmp, RetainWallA.bmp,
  strata.bmp (terrain_textures.cpp); lighting from a text file read through ifstream (loadLighting).

## 2. Open measurement, do this first

The gfx gate compares jgld's back buffer. Terrain.dll draws with OpenGL into the window DC, not into that buffer (step 2 already noted
that Terrain::drawLine bars are not in the back buffer). MEASURE before designing anything:

1. Take an in-game checkpoint (course_season) and capture BOTH the jgld back buffer and the window pixels (scenario_ab.py has
   `window_px_differ` / `C3FT_FRAME=window`). Count terrain pixels in each.
2. Find how the exe composites the view: renderTerrainStrip 0x0044a6e0 -> stripRender, then `blit_full` / `blit_screen` (the
   `render_strip`/`blit_*` engine services). Establish whether the course view reaches the back buffer through a glReadPixels-free path
   (e.g. a GDI copy from the GL window DC) or only the window.
3. Record the GL command stream per frame (Frida on the 45 imports, main thread, at one checkpoint) so a port renderer can be compared
   call by call before any pixel is compared.

Outcome decides the gate: (A) terrain pixels are in the back buffer: pixel compare as in steps 1-2; (B) window only: compare window
captures, which depends on the host GL driver, so the gate becomes the GL call stream plus a tolerance image compare.

## 3. Design

Two layers, so the correctness claim does not depend on a GPU:

1. **Command layer (exact).** Port `Terrain` logic (stripRender, Tile::render, drawTile, drawTileObjects, culling, lighting, path and
   spline drawing, zoom/rotation) as C++ that emits a GL command stream through an abstract `GlSink`. Judged by comparing that stream with
   the original's recorded stream (section 2.3), argument for argument (floats as bit patterns). This also removes the need for
   `terrain_fake`'s hand-written recorder at the Terrain boundary.
2. **Rasteriser (tolerance).** A `GlSink` implementation: a small software fixed-function rasteriser (textured, lit, depth-tested
   triangles, 16-bit target to match the back buffer) so headless x64 and Win32 runs need no GL. A second sink may forward to a real
   OpenGL context for the playable build. Image compare against the original uses a measured tolerance, not exactness (driver rounding),
   stated per checkpoint; a negative control (wrong normal, wrong texture index) must exceed it.

Texture handling: load the user's own BMP/TGA through `GameFiles` (never copied into the repo), mip chain as gluBuild2DMipmaps.

## 4. Order of work

1. Measurements in section 2 (one scenario, no port code). Deliverable: a note with the compositing path and the recorded stream.
2. `GlSink` interface + recording sink; port `Terrain::stripRender` skeleton with flat tiles; compare streams for sandbox_basic's first
   course frame.
3. Tile::render with textures, normals and lighting; heights and walls; path tiles; zoom levels 0..2; rotation (4 angles x 8 directions).
4. Software rasteriser sink; image compare with the section 2 tolerance; negative controls.
5. Wire as the `TerrainApi` implementation (`TerrainApi::render_strip`, `render`, `local_render`, `path_update_render`) behind gfx.attach;
   segment (205) and boot gates must stay unchanged, since Terrain state never feeds sim hashes.
6. Overlays: sprites, panels, text with markup (the rest of the in-game frame, plan step 4.2).

## 5. Risks

- Host-dependent pixels (driver, antialiasing, mipmap filter): hence the stream-first gate.
- The original reads uninitialised stack and /GZ fill in several Terrain functions (corners 2/4/6 of elevate/lowerCorner fault in the
  original, see the C3 notes). The port must pick a documented behaviour per defect (re/port/divergences.tsv).
- x87 code in normalize/faceNormal/hermitePoint: the 53-bit control word is the A/B thread's; match with the Win32 runner, not x64 SSE.
- Scope creep: audio and font rasterisation are separate steps (plan 4.3, 4.4).

## 6. Measurements (2026-10-23)

1. DONE, back buffer vs window: `re/tools/scenario_refs.py run --scenario course_season --label gl_probe` (hooks off, log/c3/alt/refs/course_season/gl_probe):
   all 12 checkpoints (sandbox, hole_open, season_060..600) have source `backbuffer` and `window_px_differ` 0, and the frames show the
   full 3D course view (terrain, trees, water, golfers). So the terrain pixels ARE in jgld's back buffer: case (A), pixel compare.
2. DONE, why: Terrain.dll's g_pfd (0x10063e10, read from the file) has dwFlags 0x28 = PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL, cColorBits 16,
   cDepthBits 0, no DOUBLEBUFFER, and initSystem takes the display surface's DC (acquire_dc). The GL runs on Microsoft's generic software
   implementation (opengl32 on a DIB DC) and draws straight into the game's 16-bit back buffer. No depth buffer was requested, which fits
   stripRender drawing its list in painter's order. Consequence: the reference rasteriser is that software GL (its triangle fill, texture
   filter and 16-bit dithering rules), not a GPU driver, so an exact-pixel target is plausible. Whether it is reproducible bit for bit on this
   Windows build is the first thing to test (render one recorded stream twice, then against a second PC).
3. DONE (first pass), GL call stream: `re/tools/gl_record.py` + `re/frida/js/gl_rec.js` hook the 47 OPENGL32 / GLU32 exports of Terrain.dll
   (all 47 installed, lazily, once Terrain.dll and opengl32 are mapped) and record calls made from Terrain.dll code until N glFlush, with raw
   stack dwords (floats and doubles as bit patterns), the floats behind the glXXXfv pointers and an FNV of texture memory. IMPORTANT: with the
   shim's hooks on, the GL calls come from WINMM.dll (the C3/C4 Terrain reimplementations run live), so the original stream needs
   `--hooks-off` (SIMGOLF_HOOKS_OFF=all). One sandbox frame of the original (log/p5/gl/orig_a.gl.txt, not committed): 31,567 calls =
   glLoadIdentity, glPushMatrix, glRotated(g_screenScale), glRotatef(45 + angle) 0x42340000 = 45.0, glTranslatef((25 - X) * 100, 0, (25 - Y) * 100),
   then 3,672 triangles (glBegin(GL_TRIANGLES) / 3 x [glTexCoord2fv + glArrayElement] / glEnd, i.e. vertex arrays indexed per vertex, texcoords
   by pointer) in 2,184 texture-bind runs, glPopMatrix, glFlush. No glEnable / glLightfv / glMaterial call occurs per frame (state is set once at
   init: 518 glGenTextures / glTexImage2D / 510 glTexEnvi were seen at load). The tile loop issues no glNormal per vertex: lighting normals come
   from the normal array. Two recordings differ only in the camera pivot (the first glTranslatef: 0xc42f0000 vs 0xc4960000), because the
   camera was moved by real-time arrow keys: the stream depends on the camera, so reproducibility needs the pinned scenario_det timeline
   (scenario_ab.py checkpoint) rather than this live harness. NEXT: arm the recorder inside a scenario_ab checkpoint (course_season
   hole_open / season_060) so each checkpoint's stream is paired with its back-buffer frame, and record the init stream (glViewport, glEnable,
   glLightfv, glMaterialfv, glHint, texture upload order) from process start, which this run did not cover.

4. DONE, software GL determinism (`re/tools/glexact/glexact.c`, `build.bat`; x86, set CL= first or cl hangs on the inherited /MP12):
   a synthetic terrain-like scene (initGL state: line smoothing, texture 2D, lighting with light 0, cull face, vertex and normal arrays,
   glArrayElement + glTexCoord2fv, GL_TRIANGLES, 2 mip-mapped 64x64 textures via gluBuild2DMipmaps, glRotated / glRotatef / glTranslatef)
   rendered through the same setup as Terrain.initSystem (16-bit BI_RGB DIB section, PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL, 16 colour
   bits). Result on this PC (GL vendor Microsoft Corporation, renderer "GDI Generic"): 3 passes in each of 3 separate processes give the
   same FNV-64 of the DIB bits (8b2f0fb9d9f1b523) and byte-identical BMPs; 480,000 px, 223 distinct colours. So the software GL is bit-exact
   run to run and process to process on one machine. Note: ChoosePixelFormat granted format 204 with cDepthBits 16 although the descriptor
   asks for 0, so a depth buffer exists in the original (the painter's-order reading in section 6.2 is not proven; depth testing is only
   used if the DLL enables GL_DEPTH_TEST: grep the 47 imports, glEnable is called with the capability list to confirm). NOT yet shown: the
   same hash on a second PC or Windows build, and this scene is synthetic (the recorded stream's arrays and textures were not replayed).

5. DONE, init stream from process start (`gl_record.py --init --hooks-off`; gl_rec.js now installs through Process.attachModuleObserver and is
   armed from the first call; marks `main_menu` and `sandbox`; logs log/p5/gl/init_a / init_b, not committed). Two recordings of the original
   Terrain.dll code agree call for call after masking pointers: the first 4,120 calls (everything before the main menu) are byte-identical,
   and so is every non-draw call afterwards. Content of the stream:
   - Before the main menu, 518 x [glGenTextures, glTexParameteri x4, glTexEnvi, glBindTexture, glTexImage2D] with NO GL context yet
     (wglCreateContext comes after the `main_menu` mark, at sandbox start), so those uploads reach no context and are lost; the same 518
     textures (510 RGB + 8 RGBA, all 64x64, 473 distinct images by hash) are uploaded again after the context exists. Same set in both runs.
   - Context: wglCreateContext(hdc), wglMakeCurrent. initGL: glHint(PERSPECTIVE_CORRECTION, FASTEST), glEnable(LINE_SMOOTH),
     glHint(LINE_SMOOTH, NICEST), glEnable(TEXTURE_2D), glEnable(LIGHTING), glFrontFace(0x900 = GL_CW), glEnable(CULL_FACE),
     glEnableClientState(VERTEX_ARRAY, NORMAL_ARRAY). Lighting: GL_LIGHT0 position (-0.607331, 0.768866, 0.199985, 0), ambient (0.784314,
     0.784314, 0.705882, w), diffuse (0.941176 x3, 0.784314), specular (1.0, 1.0, 0.960784, 0.941176), glEnable(LIGHT0); glMaterialfv(FRONT,
     SPECULAR, 1 1 1 1), glMateriali(FRONT, SHININESS, 13). The fourth float of each light vector is read past the file's float[3]
     (ambient w = 0x36e8040 in one run and 0x3632040 in the other: the only value that differs; diffuse w is the next array's first element),
     which is the loadLighting defect already noted; it does not reach the image (no blending, no alpha channel in RGB555).
   - Then glDeleteTextures x ~925 (ids 0..), 518 re-uploads, glViewport(0, 0, 800, 600), GL_PROJECTION, glLoadIdentity,
     glOrtho(-884, 883, -663, 662, -5000, 5000), GL_MODELVIEW, glLoadIdentity, glVertexPointer(3, GL_FLOAT, 0, Terrain.dll 0x100b28c8),
     glNormalPointer(GL_FLOAT, 0, 0x10070a18).
   - Texture state: min filter 0x2601 (GL_LINEAR), mag filter 0x2600 (GL_NEAREST), wrap S/T 0x2901 (GL_REPEAT), TEXTURE_ENV_MODE 0x1e01
     (GL_REPLACE). No mip-maps: gluBuild1D/2DMipmaps never run in this path, and glTexImage2D level 0 only (RGB 0x1907 or RGBA 0x1908,
     GL_UNSIGNED_BYTE). glEnable capabilities used: 0xb20, 0xde1, 0xb50, 0xb44, 0x4000. NO glEnable(GL_DEPTH_TEST) (0xb71): the 16-bit depth
     buffer ChoosePixelFormat granted is never used, so draws are painter's order as read in section 6.2 (now confirmed).
   - Consequences for the glexact probe: it used mip-mapped GL_LINEAR_MIPMAP_LINEAR, GL_MODULATE and the wrong light data; the next probe
     must use the recorded state (GL_REPLACE, LINEAR / NEAREST, no mip-maps, GL_CW winding, the recorded light, ortho and viewport) before
     its bit-exact verdict is read as covering the game's path.

6. DONE, software GL determinism with the RECORDED state (glexact.c rebuilt from section 5: glFrontFace(GL_CW), glTexEnvi REPLACE,
   MAG NEAREST / MIN LINEAR, wrap REPEAT, glTexImage2D level 0 only, no mip-maps, the recorded light, material, glOrtho(-884, 883, -663, 662,
   -5000, 5000) and viewport 800x600, 16-bit BI_RGB DIB, PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL, no GL_DEPTH_TEST). Results: 5 passes in
   each of 3 processes give one FNV-64 (f6d186e667fb959a) and byte-identical bitmaps; 40,539 non-background pixels, 515 distinct colours.
   Negative control (GLEXACT_CTL=1, one vertex 1 unit higher): different hash, 71 px differ. Two traps caught on the way: (1) with GL_CW and
   the old index order every triangle was culled and the three runs agreed on an all-black frame (hash "same", 0 px): a determinism result
   needs a non-blank image, now checked; (2) with GL_REPLACE the light does not tint the texture (the lit colour is replaced), so lighting
   (and with it the loadLighting garbage w component) cannot change the terrain pixels except through specular, which REPLACE also drops.
   Verdict: on this PC and Windows build, the game's GL path is bit-exact run to run and process to process. Still open: a second PC / build,
   and a replay of the game's own arrays and textures (the recorded stream holds neither the vertex array contents nor the texture bytes,
   only their hashes), which is what the port's rasteriser must finally equal.

7. DONE, real-frame replay: a captured terrain draw pass replays BIT-EXACT through Windows' software GL. `gl_record.py --init --snap --hooks-off`
   (gl_rec.js snapshot mode, enabled by rpc before the menu) captures, for the last pass (glPushMatrix .. glFlush) of a sandbox boot after a camera
   nudge: the stream (stream.gl.txt, with the ids glGenTextures / glDeleteTextures handed out), the bytes of every glTexImage2D (473 distinct
   64x64 images, tex/<hash>.bin), the vertex and normal array memory up to the largest glArrayElement index (13,911 vertices, 12 bytes each),
   and the DIB the context draws into before the pass and after glFlush (read through the DC of wglMakeCurrent: 800x600, 16 bpp, BI_BITFIELDS
   0x7c00 / 0x3e0 / 0x1f, biHeight +600). `re/tools/glexact/glreplay.c` rebuilds that DIB and the pixel format (format 204, "GDI Generic"),
   replays every state call from wglCreateContext to the pass (draw blocks and flushes of earlier passes skipped, texture ids mapped), puts the
   pre DIB in, replays the pass and compares. Result (log/p5/gl/snap_a, 30,598 stream lines, 3,598 triangles): 0 of 480,000 pixels differ, hash
   d8be662c44cef51e equals the game's, twice; the game changed 471,326 pixels in the pass (1,332 distinct colours; the frame is the real textured
   terrain with paths, rough, rocks and water). Negative control (GLREPLAY_CTL=1, one glBegin..glEnd block skipped): hash differs, 172 px.
   The pre-pass DIB is all zeros: the exe draws nothing under the terrain, so the pass needs no prior back-buffer content. So the TARGET for the
   port's renderer is now concrete and reproducible: given the stream, the arrays and the 64x64 textures, produce these 480,000 words.
   Open: (a) the same replay on a second PC / Windows build; (b) other zooms, rotations and a pass with path tiles / walls (one pass so far);
   (c) deriving the stream, arrays and textures from the port's own GameState instead of from a recording, which is the port's job
   (Terrain::stripRender / drawTile / Tile::render in C++); (d) the rasteriser itself, or shipping a call into the host's GL.

8. DONE, replay corpus (`re/tools/gl_corpus.py`, `glreplay.exe <root> <step>`, `re/tools/glexact/replay_all.sh <root>`; data in log/p5/gl/corpus/c2,
   not committed): one boot of the original Terrain.dll code, hooks off, sandbox Monterey, 24 steps (built hole, cart-path drags, terrain edit
   '=' / '-', the buildings-palette terrain tools, then PgUp rotations at zoom 0, 1, 2 and back), a snapshot of the last completed pass after each
   step (a nudge right and back once when a step drew nothing). Result: 21 snapshots, ALL 21 replay bit-exact (0 differing pixels), 0 unhandled
   calls, 0 texture uploads without bytes. Honest count: 18 distinct passes (z1_r0 = z1_r1 = zoom1, z0_r3 = terrain_tools: the same pass sent twice),
   and 3 of them (hole, paths, edit_raise: 207-235 draw blocks of the immediate-mode local redraw, glNormal3fv / glVertex3fv / glTexCoord2f) changed
   no pixel (pre DIB = post DIB), so their EXACT is weak evidence. The other 15 changed 177,588 to 458,647 pixels each: full strip passes
   of 3,400-3,500 triangle blocks at zoom 0, 8,664-9,939 at zoom 1, 11,289-18,929 at zoom 2, each with its own rotation; 242 distinct
   texture ids are bound across them (473 uploaded). New call kinds beyond section 5's frame: glEnable / glDisable / glBlendFunc (blended path and
   overlay geometry; `paths`, `terrain_tools` and every pass from z0_r0 on), glVertex3f next to glVertex3fv (rotated views). Unknown: whether any
   pass drew cliff / retaining-wall geometry (the GL stream does not name it; the terrain-tool drags were clicks at guessed palette spots).
   Recorder fix this needed (gl_rec.js multi mode): passes are not pure draws. Overlay drawing inside a push / pop pair (the 395 glMatrixMode
   calls between strip passes) changes state that outlasts the pass, so the first corpus (c1) kept passes out of the stream and 10 of 13 replays
   were wrong (the replay drew nothing: matrix mode left in the wrong state). The stream now keeps every call except the bodies of glBegin..glEnd
   blocks (they live in the snapshot's pass.gl.txt with the complete pass), and stateIdx is the stream line where the pass starts. Lesson: a
   replay that "agrees" with an unchanged frame (differing pixels = changed pixels) drew nothing; read ndiff against the changed count.
   Still open: a second PC / build; cliff / wall / bridge / water passes; passes at other course types (the 473-texture set is per course type);
   and the port itself producing these streams from GameState.

9. DONE, the port's GL sink and Terrain::render (2026-10-23; port/include/portsim/sim/gl_sink.h, terrain_render.h,
   port/src/sim/terrain_render.cpp, C ABI sg_gl_* in portsim_c.h, harness port/test/glcorpus.py, tests port/test/test_terrain_render.py).
   - FIRST TARGET: the corpus's entry-call field (gl_rec.js hooks the four exported render entry points) shows the game calls
     Terrain::render(tile, angle) for the full view and Terrain::localRender(tile, pivot, angle) for local redraws, never stripRender
     (kind 1) and never pathUpdateRender (kind 4) in these scenarios. So the first port slice is render + Tile::render + the three
     immediate-mode helpers (drawSkirts, drawWalls, drawPaths + drawCornerOverlays) + isCulled + setViewAngle + tileAt.
   - GlSink: an abstract interface of the 19 GL calls that path makes (matrix ops, glRotated / glRotatef / glTranslatef, texture bind,
     glBegin / glEnd, glTexCoord2f / 2fv, glArrayElement, glNormal3fv, glVertex3f / 3fv, enable / disable, glBlendFunc, glFlush).
     GlStreamSink (header-only) writes one text line per call, floats as bit patterns and doubles low dword first, in the recorder's
     form with the pointer argument of the glXXXfv calls dropped (the values behind it kept). A rasteriser sink is the next backend.
   - Terrain::render is written from the specification: the Terrain object and its tiles are read through the context's 32-bit address
     space (Ptr / Memory, faults where the original would fault: an unmapped tile, a vertex or texture index past the supplied arrays);
     TerrainGlobals carries what Terrain.dll keeps beside its code (vertex and normal arrays, texture-id table, wall / fence textures,
     texture-coordinate table, side normals, screen scale, the view quadrant setViewAngle writes). Original quirks kept: Tile::render's
     texture cache starts as the debug build's 0xcccccccc stack fill (the first face always binds), isCulled leaves A..D at the same
     fill for a window size other than 800x600 / 1024x768 / 1280x1024.
   - EVIDENCE: corpus c7 (re/tools/gl_corpus.py, 40 steps, 34 snapshots, 31 started by Terrain::render; `port.test.glcorpus export`
     copies the 34 into port/corpus/gl/c7). All 31 port streams equal the original's line for line (up to 198,000 lines each: zoom 0,
     1, 2 and four rotations, built hole and edits, plus the fuzzed passes below), and all 34 snapshots replay bit-exact through
     Windows' software GL (replay_all.sh). Eight negative controls (sg_gl_negctl 1..8: skirt depth, texture cache off, isCulled 90 / 180
     swapped, fence textures swapped, path half-width 16.0, x walked upward at angle 0, overlay masks 5 / 9 swapped, rotated texture one
     quadrant off) each read RED on at least one pass. Before the fuzzed passes three of them (5, 7, 8) read GREEN everywhere: the real
     scenarios drew one lone path tile and no type-7 tile, so path bands toward neighbours, the overlay masks and the rotated textures
     were unjudged; the UI clicks that were meant to build paths and bunkers did not take effect, so the corpus was widened differently:
   - STATE FUZZING OF THE ORIGINAL (gl_rec.js glrecFuzz): a queued mutation of the live Terrain object's draw fields (path flags on ~25%
     of tiles, wall flags and heights on every side, hidden tiles for skirts, type-7 tiles, random rotation and variation) is applied
     ON THE GAME THREAD at the next Terrain::render entry and undone right after that pass's glFlush, and the pass is kept under its own
     snapshot kind (22). The first version mutated from the rpc thread and raced the render thread: the pass lines and the Terrain object
     snapshot belonged to different moments, 12 comparisons failed with an offset pairing (fuzzed state with plain stream and the other
     way round). Lesson: touch the original's state only from its own thread, at a function boundary.
   - Limits: x64 harness only (runner32 has no sg_gl ops); localRender / pathUpdateRender / stripRender and the draw list are not ported
     (their snapshots are in the corpus: kind 3 steps `*_local`); TerrainGlobals is filled from a recorded Terrain.dll image, not yet from
     the port's own Terrain model (the setters that build faces, vertices, normals and texture ids are the next big block); the texture
     coordinate and side-normal tables are still read from the snapshot instead of the user's own Terrain.dll file; no rasteriser yet.
   NEXT: localRender + its draw list (drawTile ordering) and pathUpdateRender on kind-3 / kind-4 snapshots; then the rasteriser sink
   judged against dib_post of the corpus; then the state side (Terrain::setType, elevateCorner, layPath, setWall ... against tobj / vertex
   snapshots before and after each call).

10. DONE, the software rasteriser (2026-10-23; port/include/portsim/sim/gl_raster.h, port/src/sim/gl_raster.cpp, C ABI sg_raster_* and
    sg_gl_render_to_raster, harness port/test/glraster.py, fit tool glraster_fit.py, tests port/test/test_gl_raster.py). GlRaster is a GlSink
    with the original's setup calls added (viewport, ortho, lights, material, texture objects, front face, enables); a recorded stream
    replays into it (exec_line), and the port's own Terrain::render can drive it directly. RESULT on corpus c7, against the original's
    back buffers (dib_post): not bit-exact, within a measured tolerance. On all 34 passes the rasteriser's pixel equals the original's in 96.8
    to 98.8 % of the drawn pixels, every other pixel is ONE 5-bit level off in one channel (independent per channel), and 0 to 62 pixels per
    pass (a few per frame) differ by more: seams where the original covers or leaves a pixel the rasteriser does the opposite for. Criterion in
    the tests: one-level pixels <= 3.5 % of the drawn, <= 70 pixels more than one level. All 34 stream replays and all 31 Terrain::render
    chains (port renderer -> rasteriser, no recorded stream between) meet it; eight negative controls (no dither, dither offset 0, white
    lighting, nearest minification, texture env REPLACE, blending ignored, front faces culled, no specular) each break it.
    What the data fixed (each was a hypothesis the corpus judged; the first run was 99 % of pixels wrong by 6 to 10 levels):
    - The original's `glTexEnvi(GL_TEXTURE_2D, GL_TEXTURE_ENV_MODE, GL_REPLACE)` names the wrong target (0xde1, not 0x2300): GL_INVALID_ENUM,
      ignored. The mode stays GL_MODULATE, so the lit colour multiplies every texel (all that made the frame ~9 % too bright).
    - Lighting (vertex, smooth shaded): light 0 directional (its direction transformed by the modelview at the glLightfv call), ambient and
      diffuse from the recorded values, specular with exponent 13 and material specular (1,1,1,1), GL default global ambient 0.2 and material
      ambient 0.2 / diffuse 0.8, viewer at infinity, no GL_NORMALIZE, normals through the inverse transpose of the modelview. float32 or
      double lighting gave the same pixels; a 256..1024-entry specular table is wrong (17,000 more errors), the exponent is exact.
    - Writing to the 16-bit RGB555 surface: out = floor(c * 31 / 255 + (k + 0.5) / 16) per channel, k from the 4x4 ordered-dither matrix
      {15,7,13,5 / 3,11,1,9 / 12,4,14,6 / 0,8,2,10} indexed by (window y & 3, x & 3), the same for R, G, B; fitted from the pre-quantisation
      colours (0.5 % residual; 31/256 or a 0.1 % scale error multiplies the errors by 3 to 9; 8-bit rounding of any intermediate is wrong).
    - The eight RGBA path textures (LoadTGA) set their filters with glTexParameterf, the values arriving as floats (9729.0 =
      0x46180400 = GL_LINEAR for both minification and magnification); a replay that drops glTexParameterf leaves them at the GL
      defaults and draws every path band wrongly. (An earlier version of this note read 0x46180400 as GL_NEAREST and claimed the
      eight textures share one object 0xcccccccc: that id belongs to the PRE-context load of the Terrain constructor, see section 12;
      the passes bind ordinary names 0x1ff..0x206.)
    - A bound texture object with no image (id 0: type-7 tiles whose rotated texture slots were never loaded) is incomplete: OpenGL draws it
      untextured, the fragment being the lit colour (the pale diamonds of the fuzzed passes).
    - Rest: front face GL_CW, back faces culled, window y = memory row, texture filter chosen per triangle from the texel-per-pixel scale
      (<= 1 magnification: NEAREST; else LINEAR bilinear at level 0, REPEAT), source-alpha blending against the destination expanded as
      d * 255 / 31 (the path textures' alpha is 0 or 255, so it acts as a cut-out), sampling at the pixel centre.
    Did not help (glraster_fit.py knobs, all left off): bilinear weight bits 4..12 and weight rounding, texture-coordinate fraction bits,
    light colour at 8..16 bits, vertex snapping to 2^-2..2^-12 pixel, float32 geometry and float32 lighting, a 1/64 pixel shift of the
    sampling position (5x more errors: the optimum is exactly the pixel centre), edge tie-break variants (never exercised), rounding of the
    filtered texel or of each lerp stage, the dither offset and scale grid. So the exact double pipeline is the best found; the one-level
    noise stays unexplained (it sits within 0.004 levels of a quantisation threshold in the median, so the original's arithmetic differs
    from the double pipeline by about 1e-4 relative somewhere in the texture or colour path).
    Limits: lines, points and the overlay primitives (Terrain::drawLine and friends: GL_LINES with smoothing, glOrtho 2D passes) are not
    rasterised; only Terrain::render passes are chained (localRender snapshots replay through the stream path only); x64 harness; the
    rasteriser reads the state stream the recording holds, because the port does not yet produce the original's setup calls itself.
    NEXT: localRender and the draw list in the renderer, then the exe-side overlay drawing, then producing the state stream (initGL,
    lighting file, projection) from the port, and finally the Terrain state side.

11. DONE, the remaining one-level noise investigated with probes of the REAL software GL (2026-10-23; port/test/glprobe.py,
    glshift_fit.py, glcover_fit.py; rasteriser changes in gl_raster.cpp). Method: synthetic scenes in the recorder's text form run through
    glreplay.exe (Windows' generic OpenGL, which writes its DIB) and through GlRaster, compared pixel by pixel, one pipeline stage per
    scene. Findings, stage by stage:
    - Flat colours (256 grey levels, glColor4f c / 255, unlit, untextured): EXACT, 0 of 480,000 pixels differ; the quantisation
      floor(c * 31 / 255 + (k + 0.5) / 16) with the 4x4 matrix fits every one of 442,368 pixels of the probe.
    - Gouraud triangles at random fractional positions: 0.2 % of the drawn pixels differ (153 of 76,415), 17 of them coverage; after the
      attribute-position fix below 80 differ. Interpolating colours is not the noisy stage.
    - Texture magnification (0.25 .. 0.75 texel per pixel, NEAREST): EXACT, 0 differences. Bilinear at 1.25 .. 3 texels per pixel is the
      filter with u - 0.5 / v - 0.5 texel centres and REPEAT (97.5 % / 94.5 % / 89.9 % of the channel values at 1.25 / 2 / 3 before the
      fix; nearest, no half-texel shift and a box filter match 0.1 .. 40 %). Weights are not quantised (8 .. 16 bit weights, fixed-point
      or reduced-mantissa texture coordinates, vertex or texcoord snapping all raise the error).
    - WHERE the bilinear filter and the interpolated colours are evaluated: at integer texel-per-pixel scales the weights are constant
      inside a quad, so the real GL's weights can be fitted per quad. They are the exact ones shifted by a pure scale about the screen
      centre: sample position = pixel centre + 1.046e-5 * (x - 400) in x and 1.355e-5 * (y - 300) in y (no offset, no cross term, no
      quadratic term; rms residual 0.0008 px over 48 quads; mean match with the fitted weights 99.3 % against 94.5 % with the exact ones).
      It is the same for projections of 0.5 .. 4.4 world units per pixel (so it belongs to the viewport, not the matrices), equals about
      0.0041 px at the viewport edges in both axes (about 1 / 256 px), and does NOT apply to nearest sampling (0 differences without it)
      nor to coverage. Implemented as kAttrScaleX / Y in gl_raster.cpp for bilinear texture coordinates and for colours. Effect on corpus c7:
      one-level pixels 1.2 .. 3.2 % -> 0.5 .. 1.6 % of the drawn pixels (e.g. sandbox 7,186 -> 2,780 differing pixels), pixels more
      than one level off unchanged (0 .. 62 per pass); the test criterion is tightened to 2 % / 70 pixels.
    - Magnification vs minification: rho <= 1 is nearest (probes: 0.90 .. 1.0 nearest, 1.005 and up bilinear). At rho within about 1e-7 of
      1.0 the original's choice is float32 noise (a 1.0 texel-per-pixel probe gives bilinear in 21 of 48 quads, nearest in the others);
      the rasteriser treats rho <= 1 + 1e-6 as nearest and so differs on those quads only. Corpus passes have rho 1.4 .. 3, away from it.
    - Coverage: the original's edge position has a symmetric noise of about +-0.004 px (a pixel centre 0.004 px outside is covered 17 % of
      the time, 0.004 px inside 86 %); 47 of 22,660 covered pixels in a 480-triangle probe (0.2 %) differ from exact edge functions.
      No bias by edge direction, side or position along the scanline. Not reproduced by: vertex snapping at 2^-2 .. 2^-16 and grids of
      1/2000 .. 1/100 px, span-end snapping, scanline walkers in fixed point (8 .. 24 bits) or float32, float32 edge functions (two forms),
      a per-pixel-cell sample offset (160 -> 128 misclassified of 3,318 with 32 free offsets).
    - Residual after all of this: a per-quad position error of about 0.0007 px (the two triangles of one quad differ by 0.0009 px in y,
      not in x) that does not depend on the distance from the quad's edges; on an extreme-contrast random texture it is a 5 % pixel
      mismatch at 2 texels per pixel, on the game's textures 0.5 .. 1.6 %.
    NEXT for this thread, if the last half percent matters: the origin of the 1 / 256-pixel viewport constant and of the per-triangle y
    residual (a probe with triangles of controlled vertex order and top vertex would separate prestep from setup); the float32 rho
    arithmetic for the rho == 1 case.

12. DONE, the GL setup produced by the port instead of replayed (2026-10-23; port/include/portsim/sim/terrain_gl_setup.h,
    port/src/sim/terrain_gl_setup.cpp, GlSink setup virtuals in gl_sink.h, tests port/test/test_terrain_gl_setup.py and the setup-driven
    chain in test_gl_raster.py). What the original's Terrain.dll does before the first frame, reconstructed from its matched sources and the
    recorder's stream (9,191 calls up to initTerrain's glNormalPointer), now produced from the user's own files through GameFiles:
    - Terrain::Terrain names the tile types and calls relight() BEFORE any GL context exists: all 518 texture files of the Parkland set
      are loaded and every GL call is a no-op in the driver. glGenTextures then writes no name, each loader goes on with its uninitialised
      local (0xcccccccc, the debug build's stack fill: the recorded `glBindTexture 0xde1,0xcccccccc` lines) and the texture table ends up
      full of 0xcccccccc. 4,120 recorded calls, pure waste.
    - Terrain::initSystem: wglCreateContext + wglMakeCurrent, initGL (hint, line smooth, texture 2D, lighting, front face CW because
      the y-flip flag is set, cull face, vertex and normal arrays), initLighting -> rebuild -> loadLighting("ParklandLighting.txt") (the
      three colour lines after #AMBIENT / #DIFFUSE / #SPECULAR as integer / 255, light 0 from (-0.5, 0.1, -1, 0) rotated 40 degrees about
      x and 45 about y with the original's quirk that the second component uses the already rotated first, normalised, white specular
      material, shininess 13), relight again (m_texturesLoaded is set, so reloadTextures first: 925 glDeleteTextures of nine names each
      over the whole 0x25 x 0x19 table, still full of 0xcccccccc, then the same 518 loads with a context: names 1..518), resize(800, 600)
      (viewport, orthographic extent 1767 x 1325 halved with the original's arithmetic shift, y flipped).
    - Terrain::initTerrain: glVertexPointer / glNormalPointer (the buildArrays step is state, not GL, and is not ported here).
    - Terrain::setZoomLevel (levels 4 / 2 / 1 = zoom 0 / 1 / 2): the projection redone from scratch, w and h scaled by 2 or 4.
    - Light colours: the arrays are float[3] but glLightfv reads four floats; on the original's stack they lie specular, diffuse, ambient,
      so each fourth float is the first of the next array and the ambient's is garbage that differs from run to run (set to 0 here,
      masked in the comparison); it does not reach a pixel.
    EVIDENCE. The port's 9,191 calls equal the recorded ones line for line (pointers, context handles and the ambient w dropped), down
    to the light direction bits, all 1,036 texture hashes (the recorder's FNV of the bytes the loaders pass to glTexImage2D) and the 925
    deletes with their 0xcccccccc names; the texture table the setup leaves equals the one in a snapshot of the original's Terrain.dll image
    (518 distinct names). Controls: one flipped byte in one texture, an edited lighting file and a missing texture file each change the
    stream; an unmodified copy of the files does not. END TO END, with no recorded GL state at all: the port's setup drives GlRaster
    (which now ignores calls without a context and hands out names 1, 2, 3 ... like the original's driver), its table feeds the renderer,
    and Terrain::render draws each snapshot over the snapshot's Terrain object and arrays: all 31 Terrain::render passes meet the frame
    criterion (zoom 1 and 2 passes through setZoomLevel); a control with the lighting file edited (ambient 200 -> 60) breaks it.
    What still comes from the recording in that chain: the Terrain object (tiles, faces) and the vertex / normal arrays, i.e. the state side
    of Terrain.dll, and the starting back buffer; the texture-coordinate and side-normal tables and the screen scale are still read from the
    snapshot (the screen scale is also produced by the setup, 38.682186, but the renderer's globals take the snapshot's value).
    Not covered: courses other than Parkland (Desert / Tropical / Links textures and lighting go through the same code; their files were
    not recorded), mip-mapped loads and 1-D textures (the loader throws MissingResource: no call site uses them), loadNewCourseType.
    NEXT: the state side (the Terrain object: tiles, faces, vertex / normal arrays, buildArrays, setType, elevateCorner, layPath, setWall),
    then localRender and the draw list, then the overlay drawing.

13. DONE (steps 1-4), the state side of Terrain.dll (2026-10-23; port/include/portsim/sim/terrain_state.h, port/src/sim/terrain_state.cpp,
    C ABI sg_ts_* in portsim_c.h, tests port/test/test_terrain_state.py, recorder re/frida/js/terrain_calls.js + re/tools/terrain_calls.py,
    corpus reader port/test/terraincorpus.py). Goal: the renderer's inputs (the Terrain object, its tiles and faces, the vertex / normal
    arrays) produced by the port instead of a snapshot.
    - LAYOUT (step 1). Clean pack(1) structs with static_asserts: TerrainFace (0x38: v[3], tc[6], texture +0x24, piece +0x28, normal
      +0x2c), TerrainTile (0x248: level[8] with the corner heights at the odd indices, type +0x24, rotation +0x28, x / y, neighbour[4] by
      side 0 = y-1, 1 = y+1, 2 = x-1, 3 = x+1, face_count +0x44, face[8], path bytes +0x208..+0x20e, wall heights +0x210, wall flags
      +0x234, variation +0x240, normals_dirty +0x244), TerrainTypeInfo (0x18) and TerrainObjectHeader (0x3a4; +0x00 is initSystem's
      HGLRC, not a plain int: U-1074). The object stays in the 32-bit address space so the renderer reads what the state side writes. The
      original allocates it with the debug CRT's operator new: every byte no code writes holds 0xcd (the even level slots, +0x20, the wall
      heights, wall flags 1 / 3 / 5 / 7 / 8, the face bytes after `piece`), and the port's object reproduces that.
    - CONSTRUCTION AND ARRAYS (step 2): Terrain::Terrain's state part, Tile::reset, Faces::build (with the shared-edge copies between faces
      2 / 3 and 4 / 5), initGrid (each tile owns its own 3 x 3 vertex patch in a 150 x 150 grid: tiles share no vertex), buildArrays
      (neighbour links), plus terrain_apply_gl_state, which writes the GL setup's results (type names with strcpy's 0xcd tails, variation
      counts, flags, window size, zoom, texture table, course type) into the object. EVIDENCE: built from scratch over a 0xcd block at the
      recorded address, every construction-determined byte equals the original's object in all 34 c7 snapshots and all 14 t1 views (two
      object addresses, zoom 0 / 1 / 2), and the vertex grid's x / z equal all 22,500 entries.
    - NORMALS: faceNormal, normalize, Tile::smoothNormals (its four corner cases, including the read of neighbour face 1 that the (0, 3)
      case never recomputes), Tile::calcNormals, the exported calcNormals / calcAllNormals; x87 at 53 bits as double arithmetic rounded to
      float at each store. EVIDENCE: over a snapshot's own vertices, recomputing every tile the original had recomputed (not dirty in the
      snapshot) gives the normal array bit for bit (all 22,500) and every face normal by value, on all 34 c7 snapshots (dirty and fuzzed
      ones included) and all 14 t1 views; nothing else in the object changes. The only byte difference is the sign of zero on faces the
      original never recomputed since Faces::build (U-1075).
    - FACE TEXTURES: setFaceTexture (type overrides, face-to-slot map), blendFaceTextures with edgeKind / isSolidType / isOpenType /
      flagCode / idClass, reblendNeighbourhood, resetFaceTextures (types 0 / 9). They need g_collarInfo, a heap block no c7 file holds, so a
      second recording was made: re/tools/terrain_calls.py (corpus t1, log/p5/terrain/t1, exported to port/corpus/terrain/t1) logs every
      exported Terrain call from process start with raw arguments, return value, setType's drawn variation and passCollarInfo's table
      (31,644 calls: getInstance, passCollarInfo, initSystem, initTerrain, 5,021 setType, 17,942 elevateCorner, 5,000 calcNormals, 113
      lowerCorner, 253 setWall, 30 layPath, a course switch loadNewCourseType + resetTerrain, the sandbox edits, 3,110 drawLine), with
      snapshots taken on the game thread at a Terrain::render entry after each scenario step and before / after the first calls of each
      mutator (64 snapshots). EVIDENCE: on all 14 t1 views, resetFaceTextures on the type 0 / 9 tiles and blendFaceTextures(-1) on every
      tile leave the WHOLE Terrain object byte-equal to the original's: every texture and piece of the final state is a function of that
      state (types, rotations, variations, collar table, course type).
    - Negative controls (sg_ts_negctl): 1 no shared-edge copies, 2 grid one column off, 3 x neighbours swapped (construction); 4 cross
      product reversed, 5 centre sum without face 7, 6 (0, 3) without the diagonal, 7 normalize summing in float (normals); 8 flagCode
      weight 11, 9 face slots 2 / 3 swapped, 10 edgeKind's water arm 1 (textures): all RED. Blind on purpose: 11 Tile::reset's default
      type (the exe's setType overwrites every tile before any snapshot).
    - History, not state (U-1075): the centre vertex heights (a running max / min over elevateCorner / lowerCorner) and which faces were
      ever recomputed depend on the order of the exe's calls. Corner heights (level x 15.0) and edge midpoints (the mean of their corners)
      are functions of the state.
    - MUTATORS (step 3, 2026-10-23): Terrain::setType (the variation is rand() % the type's count, then setRotation and setTypeId),
      Tile::setTypeId with enter / leaveType6 (+-20.0) and enter / leaveType7 (-+13.0) vertex walks, setPathFlagForType,
      resetFaceTextures; elevateCorner / lowerCorner (corner level x g_splineHeight, the centre's running max / min, the type-7
      lowering), lowerEdgeCorner (its renderTileAtFiveOffsets draws and sets the view quadrant, then lowerCorner), layPath / updatePath,
      setWall, setSplineHeight (g_splineHeight 0x10063e54 IS the corner step: the matched sources name it so in setSplineHeight and read
      it as the 15.0 step in elevateCorner), resetTerrain, loadNewCourseType (lighting, reloadTextures, relight through the setup code),
      setZoomLevel's zoom, initSystem's flags / size / handle, setViewAngle for render / localRender / stripRender, the constructor's member
      constructors (Faces count 0, PathInfo / WallInfo clears, the draw list header: allocator byte 0xcc, head node, size 0). rand(): Terrain.dll's
      debug CRT on the game thread, _holdrand from 1, h = h * 214013 + 2531011, (h >> 16) & 0x7fff; all 5,021 recorded variations of t1
      equal it with no other draw in between. The even corners (2 / 4 / 6) take the switch's missing default: the twelve face / vertex
      locals keep the /GZ fill 0xcccccccc and the port computes the same wrapped addresses, which fault (no recording reaches them: the
      original dies there). The exported methods are one dispatcher, terrain_dll_call (C ABI sg_ts_dll_begin / sg_ts_call), fed the exe's
      calls as recorded: undecorated name, ecx, the five stack dwords, passCollarInfo's table.
    - REPLAY (steps 3-4). Three recordings of the original, each replayed from an empty 0xcd block (getInstance builds the object and
      runs the port's GL setup from original/), every outermost exported call in log order, the state compared at every snapshot position:
      t1 (31,642 calls, 64 positions), t2 (29,158 calls with 60 INJECTED calls, 128 positions), t3 (26,508 calls, the same injections, 124
      positions, plus frames). INJECTION (re/frida/js/terrain_calls.js tcInject, re/tools/terrain_calls.py --inject): the paths no
      scenario reaches are called on the ORIGINAL, on the game thread at a Terrain::render entry, logged as ordinary calls with a snapshot
      after each: a 2 x 2 type-6 block entered and left (leaveType6's += corner), a type-7 block in every rotation with lowerCorner on it,
      paths on buildings, updatePath, the setFaceTexture overrides (tricky green 0x80, water depths, 0x15, 0x16 rotations 0x40..0x47),
      setWall with the flag inside a filled dword, setSplineHeight 20.0 between two elevateCorner, lowerEdgeCorner at 0 / 90 / 180 / 270,
      and loadNewCourseType(1) (Desert, with water next to open types) and back. EVIDENCE: at all 316 positions the Terrain object (the
      context handle and the draw list's head node compared as zero / nonzero), the 22,500 vertices, the 22,500 normals, the texture
      table and the view quadrant equal the original's byte for byte; getInstance and render return what the original returned. This
      settles U-1075: the centre heights and which faces were ever recomputed come out right when the calls are replayed.
      TEXTURE NAMES: the Desert switch first left 553 table entries different: Windows' software GL handed out 1, 2, 3 ... again after
      reloadTextures had deleted every name (Desert got 1..553, Parkland back 1..518), where the port's sinks counted on. GlStreamSink
      and GlRaster now give the lowest name not in use and free deleted ones (U-1077: only the delete-everything case is measured).
    - Negative controls 1..21 (sg_ts_negctl; 12..21 for the mutators: centre min for max, leaveType6 without its += corner, rand
      increment, setPathFlagForType without 0x16, type-7 lowering 12.0, setTypeId without leaveType7, enterType7 dirtying every
      neighbour, setWall's whole dword, layPath without the direction flag, setViewAngle 90 -> 2): every one reads RED on the replay of t2.
      On t1 alone 13, 15, 16 and 19 read GREEN (no type-6 tile, no lowerCorner on type 7, no path on a building, setWall flags only
      0 / 1): that is why t2 injects those calls.
    - CHAIN (step 4, corpus t3 = terrain_calls.py --frames --inject: gl_rec.js beside the call recorder, the last Terrain::render pass of
      each step saved in gl_corpus form under frames/ with the call-log position it was fetched at). For each of the 15 frames the log is
      replayed up to the render call the frame shows (the latest render with the pass's this / tile / angle after the last state change:
      a fetch can race the render the step's own snapshot started), the port's GL setup from original/ sets up the rasteriser at the
      replayed zoom (its texture names equal the replayed table), and the port's Terrain::render draws over the original's starting back
      buffer. No recorded Terrain object, vertex or normal array, texture table or screen scale is used; the wall / fence textures are
      entries [0x22..0x24][0][0] of the replayed table. Inputs still from the recording: the starting back buffer and the DLL's constant
      texture-coordinate and side-normal tables. EVIDENCE: at every frame the replayed state equals gl_rec's snapshot of that pass, and
      the frame is pixel-identical to the snapshot-driven chain (port/test/glraster.py run_chain_from_setup, which stays for c7, the
      corpus without a call log); 11 frames (zoom 0 and 1) meet test_gl_raster's criterion, the 4 zoom-2 frames have no pixel more
      than one level off but 2.63 .. 2.82 % one-level pixels against the 2 % limit (xfail, U-1076: the same with the snapshot's object).
    - Tests: port/test/test_terrain_state.py (489: construction, normals, textures, the three replays at 316 positions, 21 replay
      controls, chain state and frames). Corpora local: port/corpus/terrain/t1, t2, t3 (export with port.test.terraincorpus).
    NEXT: a TerrainApi implementation over terrain_dll_call, so the port's exe-side callers (terrain_dll.cpp, rebuildTerrainGrid ...)
    build the object from the GameState grids, judged against a terrain_calls recording of a pinned scenario (same calls, same
    order); the constant texture-coordinate / side-normal tables from the user's Terrain.dll file; the zoom-2 one-level noise; then
    localRender and the draw list, Desert / Tropical / Links frames, overlay primitives.

14. DONE, the Terrain interface over the port's own Terrain.dll state, judged on pinned scenarios (2026-10-23/24;
    port/include/portsim/sim/terrain_live.h, port/src/sim/terrain_live.cpp, C ABI sg_ts_live_*, harness port/test/terrainlive.py, tests
    port/test/test_terrain_live.py, recorder mode re/tools/terrain_calls.py --scenario).
    - LiveTerrain is a TerrainApi whose Terrain.dll methods go to terrain_dll_call (section 13) on a Terrain object the port builds in the
      context's address space; getInstance (the boot's static initializer) maps it with operator new's 0xcd fill, so the exe side reads
      the real tile fields (types, corner levels, wall bytes) where a headless run reads a zeroed stand-in. The engine services of the
      interface (input pump, window paint, blits, operator new for the render list) are a headless run's (tick.h make_headless_terrain).
      initSystem gets a nonzero DC token, since the headless engine has no display surface but the port's rasteriser is one. Every call is
      logged in the recorder's line form, and copies of the state are kept after given call counts.
    - TerrainApi gained reset_terrain (default 0, so the recording fake and the headless runs are unchanged): refreshTerrain
      (re/analysis/terrain/00449520) calls Terrain::resetTerrain with ecx = gTerrain before rebuildTerrainGrid, and the port's
      refresh_terrain had no way to pass it on. The first comparison showed it: the first 12,705 calls of sandbox_basic were equal, then
      the original reset the grid on the course switch and the port rebuilt over the old one (3,169 extra lowerCorner calls).
    - RECORDER, PINNED: terrain_calls.py --scenario <name> replays a pinned scenario through scenario_refs.py (instance and boot locks,
      the family's restore of saves, the pinned schedule under the virtual clock) with terrain_calls.js appended to its agent (the pattern
      of trace_tool.py); at each checkpoint the main thread waits in the clock stub outside every Terrain call, so the state is copied
      there (tcSnapNow). The hooked runs are compared with the pinned references: SAME for sandbox_basic (10 checkpoints),
      course_season (12), cov_k1_terrain (5), cov_b_paths (4), cov_l_bridge (13), cov_k1_river (72) and cov_b_holes18 (6). cov_r_reload
      differs at its last checkpoint in two hooked runs while a run without the recorder is SAME (U-1078): it is used up to `run_a`.
    - EVIDENCE: the port boots each scenario on its trace (the boot gate) with LiveTerrain attached. Over those 8 scenarios its 205,008
      calls of the interface's methods equal the original's line for line (method, tile as tile index, every other argument, the drawn
      variation of every setType), and at all 124 checkpoints the Terrain object (context handle and draw-list head as zero / nonzero),
      the 22,500 vertices, the 22,500 normals, the texture table and the view quadrant equal the original's byte for byte; the boot gate
      still passes (PASS, cov_b_holes18 KNOWN as before). Not compared: the exe's Terrain calls outside the interface, 14,257 drawLine
      (overlay lines the port's exe side drops) and one stripRender (renderTerrainStrip's, an engine service of the interface). Controls on sandbox_basic: 22 (the exported resetTerrain does nothing), 14 (rand)
      and 12 (centre min for max) read RED.
    - INSTALL HYGIENE, found on the way: cov_r_reload's boot gate failed at record 378 with and without LiveTerrain, and with the DLL of
      22c8abc8 too. The sandbox walks of this phase (gl_corpus.py, terrain_calls.py without --scenario) boot the game in original/, and
      it had written autosaves and dated saves into original/saved games, which the port's boot runs read as the install, so the load
      browser listed other files. The folder was restored to log/c3/alt/s36a/pristine (today's files kept in
      log/p5/terrain/original_saves_backup_20261023) and cov_r_reload passes again; both walk tools now wrap the boot in
      game.preserve_saves(), which puts the folder back exactly.
    NEXT: localRender and the draw list (the kind-3 snapshots), the constant texture-coordinate / side-normal tables from the user's
    Terrain.dll file, the zoom-2 one-level noise (U-1076), and Desert / Tropical / Links frames; then the in-game frame with the terrain
    drawn by the port behind the 2D layers.

15. DONE, Terrain.dll's constant tables from the user's file (2026-10-24; terrain_state.h terrain_load_dll_constants). The side normals
    (RVA 0x63c40, 96 bytes) and the texture-coordinate table (RVA 0x63ca0, 92 floats) are initialised .data of Terrain.dll (file offset =
    RVA, no base relocation inside), equal to the running original's bytes in every recording. The port reads them from GameFiles
    "Terrain.dll" through the PE section table at getInstance and checks each against an FNV-1a-64 (no table bytes in the port); a file
    with one byte changed is refused (MissingResource), no file leaves them empty. terrain_apply_gl_state also fills the wall / fence
    textures (entries [0x24] / [0x22] / [0x23][0][0] of the table) and initSystem's screen scale. EVIDENCE: at all 316 replay positions
    of t1 / t2 / t3 the two tables, the three textures and the scale equal the original's; the t3 chain (15 frames) now takes nothing
    from the recording but the starting back buffer and gives the same pixels as before.
