# S3.15 merge note: the host-OpenGL terrain backend (wave 7)

Base: ee6efc1d + wave 6 (12_assemble.py = port/build/I15's tree). Apply: `~/bin/py -3.12 log/roadmap/S3/15_assemble.py --root <tree>
--only-15` on a tree that already holds wave 6, or `--dst <name> --commit <c>` for a fresh one. Every edit is anchored (old text
exactly once); new files must not exist. Notes: log/roadmap/S3/15_notes.md. Files: 15_files.txt.

## 1. Own files (15_edits.py EDITS and NEW_FILES)

- new port/include/portsim/sim/gl_wgl.h, port/src/sim/gl_wgl.cpp (module gl_wgl: the WglSink and SetupPair; opengl32 loaded at run
  time, no new link dependency)
- new port/test/test_gl_wgl.py
- port/src/sim/gfx_terrain.cpp (b_pass: the host-GL branch; controls 8..11 documented in the header)
- port/src/sim/terrain_live.cpp, port/include/portsim/sim/terrain_live.h (use_host_gl, host_gl(), SG_GL_BACKEND=wgl, harness entry
  terrainLive.hostGl)
- port/test/framegate.py (`--host-gl`, run_scenario(host_gl=...)): in its own file 15_framegate_edits.py (7 anchors, none in
  the snapshot code), for the lead's merge with S3.16's framegate edits

gfx_terrain.cpp and terrain_live.* are also wave 6's (S3.10 / S3.12); these edits are anchored on wave 6's text. S3.16 / S3.17 were
told (2026-10-29) that this step edits them.

## 2. Shared file (15_edits.py SHARED_EDITS; for the integrator)

port/src/sim/modules.inc, after `SG_MODULE(gl_raster)`:
```
SG_MODULE(gl_wgl)
```
(port test_static requires every src/sim/*.cpp to be a listed module; touch port/src/registry.cpp after it, 15_assemble.py does.)

## 3. Decision for the lead / Mariano: the default backend

Built opt-in (default GlRaster, every existing digest and frame unchanged). Turning the host GL on by default on Windows would make
the in-game frames equal the original's in the terrain, but it changes: the app's frames (port/app, S7: a one-line
`terrainLive.hostGl` / SG_GL_BACKEND choice in host.cpp), test_terrain_live's course-view G2 numbers (they would read 0) and any golden
pinned on GlRaster's pixels. The same question was answered for text in S5 (GDI on Windows, FreeType elsewhere).

## 4. UNCERTAINTIES.md (append; rows in 15_uncertain.tsv)

U-1810 (host-GL backend measured on this host only), U-1811 (control 11 blind: texture names equal), U-1812 / U-1813 (sprite
residue left in `toolbar` / `hud_message`, owner S3.16), U-1814 (drawGolferInfoPanel draws nothing: a step of its own).

## 5. CLAUDE.md paragraph (after the S3 wave 6 paragraph)

```
Roadmap S3.15, the HUD top-bar residue (2026-10-29, notes log/roadmap/S3/15_notes.md, merge 15_merge.md, U-1810..U-1814): I14's
446 FAIL rows with one-level px in hud_title / hud_meters / hud_message / toolbar hold TERRAIN pixels, not 2D ones: split against the
original's root at updateTerrainRender's return (S3.2's exit.bin, 15_terrsplit.py) every differing pixel of sandbox_basic,
cov_k1_river, cov_k3_camera2 and championship already differs in the terrain, and the half-bright frames over them are exact. The
remaining GlRaster residual (U-1392: textured pixels within 0.015..0.08 8-bit units of a dither threshold) was not closed by float32
plane, fixed-point weight, texel or colour models. Instead the terrain can be drawn by the host's own OpenGL: gl::WglSink
(gl_wgl.h, opengl32 loaded at run time onto the original's 16-bit 555 DIB, glreplay.exe's method, which is bit-exact on the c7
passes), selected per LiveTerrain by terrainLive.hostGl(1) / SG_GL_BACKEND=wgl / framegate --host-gl; default GlRaster. Full census with it: PASS
100 -> 612 of 698 (476 FAIL and 36 UNSUPPORTED rows to PASS), 0 rows lose PASS, the GlRaster census equal to I15 row for row, x64 = Win32; what is
left is cov_a_golfer's undrawn golfer info panel (U-1814) and sprite / view content of S3.16's. Controls 8 (565 DIB) and 10 (root not
loaded into the DIB) RED; 9 (top-down DIB) and 11 (texture names unmapped) blind with the reason (U-1811).
```
