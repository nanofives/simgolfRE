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
