# S3.15 notes: the HUD top-bar residue (wave 7)

Worker: claude2 opus (Happy session under lead cmvqfhvak0fuzo1159rmz3zkp), 2026-10-29. No game run. U-1810..U-1814.
Mirror port/build/S3_15 = `12_assemble.py --dst S3_15 --commit ee6efc1d` + this step's edits (15_assemble.py reproduces it; verified
equal to a fresh assembly with `diff -rq --strip-trailing-cr`, port/build/S3_15_verify).

## 1. Finding: the residue is the terrain, not a 2D body

I14's census has the one-level px of hud_title / hud_meters / hud_message / toolbar in 446 FAIL rows (repeated counts 9/16/4/1,
1/35/29/4, 15 + toolbar 18). `15_terrsplit.py` splits every differing pixel of a checkpoint by where it is made, using the
ORIGINAL's root surface at updateTerrainRender's return (S3.2's recorder, log/p5/terrain/<label>/frames/<chk>/exit.bin) and the
port's root at the same frame (terrainLive.keepFrame):
- T: the terrain exits already differ and neither side changes the pixel afterwards;
- TB: the terrain exits differ and a 2D body draws over it (a blend of an already-off pixel);
- B: the terrain exits are equal and the final frames differ (a 2D body's own error).

| checkpoint | hud_title | hud_meters | hud_message | toolbar | bottom_panel | view | B (2D) |
|---|---|---|---|---|---|---|---|
| sandbox_basic/sandbox_00 (frame 22) | 8 T | 15 T + 1 TB | 4 T | 1 T | 45 T | 51 | 0 |
| cov_k1_river/r05 (frame 77) | 1 T | 33 T + 2 TB | 29 T | 3 T + 1 TB | 25 T + 1 TB | 173 T + 2 TB | 0 |
| cov_k3_camera2/x_edge3 (frame 101) | 0 | 0 | 0 | 0 | 50 T | 0 | 0 |
| championship/tournament_start (frame 82) | 7 T + 4 TB | 12 T + 4 TB | 6 T | 0 | 0 | 58 T + 6 TB | 0 |

(log/roadmap/S3/15_split_sandbox.log, 15_split_more.log.) Every TB pixel is the half-bright of the status-bar frames
(drawScaledSprite421b90 with g_halfBrightLut) over a terrain pixel already one level off, e.g. (679, 82): port exit (13, 18, 9) ->
(6, 9, 4), original exit (13, 18, 10) -> (6, 9, 5): the 2D bodies are exact. The top-bar regions contain bare terrain (the strip,
the frames' gaps and corners): the residue is GlRaster's one-level residual (U-1392), the same that S3.16's bottom_panel rows hold.

## 2. The rasteriser residual (U-1392) re-measured, no rule found

Against glreplay.exe (the real software GL, exact oracle; 1b_probe.py's category split) on c7/fuzz1_z0: all 240 one-level pixels
are textured (tex 224 of 452,026 px, blend 26 of 48,707, untextured t0 0). In 15_margin.py every bad pixel's value lies within
0.015 .. 0.08 8-bit units of a dither threshold (`frac(v * 31 / 255 * 16) ~ 0.5`), colour clamped to 1.0, bilinear texels; the rate
does not grow with the column from the span start or the row from the top (15_spanpos.py: 2.5 .. 6 per 10,000 in every bin), so it is
not an accumulating DDA. Experiments in the mirror's GlRaster (removed again; log/roadmap/S3/15_tunes1..3.log), counts of one-level
px against the default 224: float32 plane evaluation of s, t relative to vertex 0 / the top vertex / the bottom vertex 223..225,
absolute plane 277, texel units 225; bilinear weight bits 4..16 (floor / round) 236..54,029; filtered texel or final colour held with
4..10 fractional bits 283..4,975; integer rounding stages 33,956..120,194. Section 11 / 18 of re/PHASE5_TERRAIN_PLAN.md reached the
same wall. So the exact arithmetic of the generic implementation is not reproduced by any of these models.

## 3. The fix: the host's own OpenGL as a terrain backend (U-1810)

glreplay.exe replays the original's passes through Windows' generic opengl32 onto a 16-bit BI_BITFIELDS 555 DIB (biHeight +600)
bit-exactly: c7 sandbox, fuzz1_z0, zoom1, z2_r1, hole: 0 of 480,000 px each. The port's renderer issues the same calls (S3.1:
237 draw-list passes line for line), so the same implementation behind the port's GlSink gives the original's terrain.

- `port/include/portsim/sim/gl_wgl.h`, `port/src/sim/gl_wgl.cpp` (module gl_wgl, registers nothing, like gl_raster): `gl::WglSink`,
  a GlSink over opengl32 loaded at run time (LoadLibrary + GetProcAddress: nothing new to link; off Windows a stub whose ok() is
  false), on a DIB and pixel format equal to the original's (PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL, 16 colour bits); calls before
  context_create are ignored like a driver's; the context is made current on the calling thread when it is not; texture names are
  mapped port -> implementation (gen_textures_mapped); the vertex / normal arrays are the port's TerrainGlobals arrays, re-pointed per
  pass (set_arrays). `gl::SetupPair` feeds the setup to GlRaster and the WglSink together.
- `terrain_live.h/.cpp`: `LiveTerrain::use_host_gl(on)`, `host_gl()` (null unless selected, available and with a context),
  `SG_GL_BACKEND=wgl` read in the constructor; harness entry `terrainLive.hostGl(on)` (before the first Terrain call; returns 1 when
  in use).
- `gfx_terrain.cpp` b_pass: with host_gl() the pass loads the root surface into the DIB, draws through the WglSink, and copies the
  DIB back; otherwise GlRaster as before (default unchanged). Controls 8..11 (below).
- `port/test/framegate.py`: `--host-gl` / run_scenario(host_gl=True) (variant column `host-gl`).
- `port/test/test_gl_wgl.py` (6 tests, Windows only).
- modules.inc: `SG_MODULE(gl_wgl)` (shared: 15_merge.md).

The default stays GlRaster: on by default would change the app's frames (host.cpp, S7) and every frame golden pinned on GlRaster's
pixels; that is the lead's / Mariano's decision (asked 2026-10-29, 15_merge.md section 3).

## 4. Numbers (x64 unless said; light runs only, I15 held the heavy slots)

framegate with the host backend against I14's census rows of the same scenarios (15_cmp.py):
- run a (15_fg_wgl_a.tsv, 15_cmp_a.log): sandbox_basic, cov_k1_river, championship, cov_k3_camera2: 113 rows, FAIL -> PASS 107, PASS ->
  PASS 3, FAIL -> FAIL 3; HUD top-bar px 6,078 -> 356; 0 rows lose PASS.
- run b (15_fg_wgl_b.tsv, 15_cmp_b.log): cov_a_build, cov_a_golfer, cov_l_bridge, cov_k3_champ, cov_a_champ, cov_k3_camera: 107 rows,
  FAIL -> PASS 75, UNSUPPORTED -> PASS 3, PASS -> PASS 6, FAIL -> FAIL 17, UNSUPPORTED -> UNSUPPORTED 6; 0 rows lose PASS. Every one of
  these 23 rows has 0 one-level top-bar px; what is left: cov_a_golfer's golfer info panel, which the port does not draw (9 rows:
  hud_title 3,312, hud_message 561, view ~103,000; U-1814), and view pixels of other causes (cov_a_build terrain_* 3..14 px,
  cov_l_bridge obj5, cov_k3_camera y_inj3, cov_a_champ's unsupported rows).
- The three FAIL rows of run a: cov_k3_camera2 y_inj4 (a fountain spray sprite in `toolbar`, U-1812), cov_k1_river r20 (a splash
  sprite in `hud_message` and strip view pixels, U-1813), championship tour_0180 (view only, 296 px beyond one level). None is terrain.
- Win32 (SG_BACKEND=win32, 15_fg_wgl_w32.tsv): sandbox_basic 10 PASS, cov_k3_camera2 24 PASS + y_inj4, equal to x64.
- test_gl_wgl.py: 8 passed x64 (15_pytest_gl_wgl_x64.log, 117 s) and Win32 (15_pytest_gl_wgl_w32.log, 143 s): the three scenarios,
  the default backend unchanged, the fallback (control 12), the same frames on two runs (cov_k3_camera2, every image byte-equal),
  controls 8 and 10.
- port test_static 6 / 6 (15_pytest_port_static.log); test_terrain_live x64: see section 6.

## 5. Controls (family `terrain`, gfx_terrain.cpp)

- 8 the DIB with RGB565 masks: RED, sandbox_basic 9 of 10 rows FAIL (15_fg_wgl_ctl8.tsv); in test_gl_wgl.py.
- 10 the root surface not loaded into the DIB before a pass: RED on cov_k3_camera2 (23 of 25 FAIL, 15_fg_wgl_ctl10_cam.tsv: the strip
  passes after renderTerrainStrip's shifted copy); BLIND on sandbox_basic (10 PASS: with a still camera the DIB already holds what the
  root holds); in test_gl_wgl.py.
- 9 rows top-down (biHeight < 0): BLIND, sandbox_basic 10 PASS (the implementation writes the same memory rows either way).
- 11 texture names unmapped: BLIND, sandbox_basic 10 PASS, cov_k3_camera2 24 PASS (the implementation's names equal the port's,
  U-1811).
- test_default_backend_is_unchanged: without hostGl sandbox_00 keeps I14's 125 px (all one level), so the default path is GlRaster's.
- 12 no pixel format (as a host without a generic 16-bit one): terrainLive.hostGl returns 0, framegate refuses a run that requires
  the host GL, and a run that only asks for it (`host_gl="try"`) equals the default GlRaster run row for row (the clean fallback).
RED 2 / 4, blind 2 (9, 11) with the reason; 12 is the fallback check (passes by design).

## 5b. The split over every I14 FAIL row (lead's request, 2026-10-29)

`15_split_all.py` (one framegate boot per scenario, GlRaster default; heavy token per batch of 8, 15_split_batches.sh / .log) over all
537 FAIL rows of 67 scenarios -> 15_split_rows.tsv; `15_split_summary.py` -> 15_split_summary.tsv (per I14 top-bar pattern). Method
`exit` (470 rows): against the original's terrain root recorded at updateTerrainRender's return (S3.2's exit.bin); method `hostgl`
(67 rows of the 6 scenarios with no recording: cov_r_*): a pixel that differs with GlRaster and is equal with the host-GL terrain is
terrain-origin. Top-bar regions: 469 rows terrain-origin only, 41 rows no top-bar px, 27 rows with 2D-made px. Every repeated pattern
is 100 % terrain (terrain / 2D px): 9/16/4/1 137 rows 3,710 / 0; 1/35/29/4 70 rows 4,830 / 0; 8/16/4/1 37 rows 1,072 / 0; 11/16/6/0 20
rows 660 / 0; 9/16/3/1 17 rows 493 / 0; 15/0/0/18 15 rows 495 / 0; 0/0/0/2 11 rows 22 / 0. Totals terrain / 2D: hud_title 3,169 /
44,650, hud_meters 8,010 / 283, hud_message 3,718 / 7,852, toolbar 1,512 / 232, bottom_panel 7,042 / 34, view 37,931 / 1,433,228.

The 27 rows with 2D-made top-bar px, grouped (frames re-run with the host-GL terrain so only 2D differences remain:
15_img/<scenario>.<checkpoint>.top.png = port | original | differing px):

| group | rows | top-bar px | what draws it | owner |
|---|---|---|---|---|
| golfer info panel not drawn (U-1814) | cov_a_golfer 10, cov_r_bio panel, cov_a_keys2 C_overwrite, cov_a_keys3 panel | hud_title 3,312, hud_message 560-561 each | drawGolferInfoPanel (dialogs.cpp keeps its state effects, makes no drawing call) | S3.16 (lead) |
| animated water sprites at the left screen edge | cov_a_keys3 season, cov_k1_paths cart, cov_k1_routing end | hud_title 80-104, x 0..17 y 0..40 | scene sprites over the water left of the course logo (a ripple / rock animation frame; scattered px beyond one level) | scene / view (S3.16) |
| fountain spray (U-1812) | cov_k3_camera2 y_inj4 | toolbar 232 | the fountain's spray sprite at x 0x17..0x61 y 0x19f..0x1c3 | scene / view (S3.16) |
| splash in the water (U-1813) | cov_k1_river r20 | hud_title 1, hud_message 123 | a splash sprite at x 0xfa..0x122 y 0xb..0x28 | scene / view (S3.16) |
| bird sprite | cov_a_flags ztut | hud_title 6, hud_message 42 | the stork / bird scene sprite at x ~0x10a y 0..0x13 (animation frame) | scene / view (S3.16) |
| a different moment of the frame | cov_a_flags zdemo | hud_title 905, hud_meters 259, hud_message 395 | the status-bar cash differs (192 vs 198) and the ticker text and golfers moved: the snapshot is not at the original's moment, not a body | S3.16's framegate snapshot-position fix |
| translucent ellipse missing | golfer_stories season_end | hud_title 396, x 60..102 y 70..81 | a light half-ellipse the original draws over the water under the logo, absent in the port (an overlay ring / highlight) | S3.8 overlay / S3.16 |
| none left with the host-GL terrain | cov_b_inject 3, cov_b_inject2 3 | hud_meters 4 (under GlRaster) | 0 px with the host GL: terrain from a pass after the recorded exit, not 2D | (terrain) |

Lead for U-1392 from S3.16 (2026-10-29, its tool log/roadmap/S3/16_terrain_cmp.py, image 16_out/tcmp_cs.png): course_season sandbox,
port terrain-only root (keepFrame exit, frame 22) against s32_course_season's exit.bin: 189 px differ (1 beyond one level), in vertical
bands over the whole frame height: x 0..15 49 px, x 656..687 87 px, x 784..799 19 px, about 34 scattered singles; uniform grass in tile
middles, not tile edges. cov_k1_river r00 shows the same bands (x 656..687 68 px, 784..799 19). Column bands point at a per-x term of
the attribute setup (the viewport scale / fixed-point staircase of PHASE5 section 18 depends on x only) rather than per-triangle
noise; not followed up here (the host-GL backend replaces GlRaster where exactness is needed).

## 5c. The two full censuses (after I15, 2026-10-29; 15_gates.sh census_gl / census_wgl, --jobs 1, one token each)

15_census_gl.tsv (GlRaster, the default) and 15_census_wgl.tsv (`--host-gl`), compared by 15_census_cmp.py -> 15_census_patterns.tsv
(per I14 pattern) and 15_census_cmp.tsv (the 576 rows whose status or px differ between the backends).
- GlRaster: 698 rows, PASS 100 / FAIL 537 / UNSUPPORTED 51 / NOT_RUN 10, equal to I15's census in every row (status and every
  region's px): the default path is unchanged.
- host GL: PASS 612 / FAIL 61 / UNSUPPORTED 15 / NOT_RUN 10. FAIL -> PASS 476, UNSUPPORTED -> PASS 36, FAIL -> FAIL 52, UNSUPPORTED ->
  UNSUPPORTED 12; 0 rows lose PASS. px GlRaster -> host GL: hud_title 51,482 -> 47,969, hud_meters 9,143 -> 699, hud_message 12,398 ->
  8,482, toolbar 1,961 -> 232, bottom_panel 7,317 -> 41, view 1,579,402 -> 1,539,282.
- Per pattern (rows, GlRaster -> host GL): 9/16/4/1 137 FAIL -> 126 PASS + 11 FAIL; 1/35/29/4 70 -> 70 PASS; 0/0/0/0 41 -> 31 PASS + 10
  FAIL; 8/16/4/1 37 -> 36 + 1; 11/16/6/0 20 -> 20 PASS; 9/16/3/1 17 -> 16 + 1; 15/0/0/18 15 -> 15 PASS; 0/0/0/2 11 -> 10 + 1; 6/8/6/1 10
  -> 10 PASS; 3321/16/565/1 10 -> 10 FAIL (golfer panel); 5/0/4/8 10 -> 9 + 1; I14 UNSUPPORTED 51 -> 36 PASS + 15 UNSUPPORTED (full table
  in 15_census_patterns.tsv).
- With the host GL, 23 in-game rows keep top-bar px, all named in 5b's table (golfer panel 14 incl. cov_r_bio exit, sprites 6, zdemo,
  season_end) plus cov_b_holes18 holes (UNSUPPORTED: its hole overlay, hud_meters 440). The FAIL -> FAIL rows keep only view / bottom
  pixels of other causes (S3.16 / S3.17 / S3.9).
- Segments and boot were not run here: S3.9 runs them on the combined wave 7 tree (lead, 2026-10-29); the host GL is opt-in, so
  default runs do not change.

## 6. Open / not run yet

- Gates that need the heavy slots (after I15): the census with `--host-gl` over all 64 scenarios, segments 205 / 205 and boot 66 / 2 / 2
  on x64 and Win32 (they cannot change: segments and boot attach no gfx backend and no LiveTerrain, and the host GL is opt-in), the
  test_terrain_live / test_gfx_* files.
- Not in this step: the overlay lines (terrain_overlay's rasterLine model, U-1370) could also draw through the WglSink; the app
  (port/app host.cpp) would select the host GL with `SG_GL_BACKEND=wgl` already (the LiveTerrain constructor reads it) but has no
  option of its own.

## 7. Commands

```
~/bin/py -3.12 log/roadmap/S3/15_assemble.py --dst S3_15                      # tree (12_assemble + 15_edits.py)
cd port/build/S3_15 && SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed' ~/bin/py -3.12 port/test/build.py --arch x64   # and --arch Win32
export SG_PORTSIM_DLL=$PWD/port/build/x64/Release/portsim_c.dll SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'
~/bin/py -3.12 ../../../log/roadmap/S3/15_terrsplit.py sandbox_basic s32_sandbox sandbox_00         # section 1
~/bin/py -3.12 -m port.test.framegate sandbox_basic cov_k1_river championship cov_k3_camera2 --no-probe --host-gl --out <tsv>
~/bin/py -3.12 ../../../log/roadmap/S3/15_cmp.py <tsv>                                               # against I14's census
~/bin/py -3.12 -m port.test.framegate sandbox_basic --no-probe --host-gl --negctl gfx.family.terrain=8 --out <tsv>
~/bin/py -3.12 -m pytest port/test/test_gl_wgl.py -q -p no:cacheprovider                            # SG_BACKEND=win32 for Win32
re/tools/glexact/glreplay.exe port/corpus/gl/c7 sandbox                                                # the oracle (repo root)
~/bin/py -3.12 log/roadmap/S3/15_make_edits.py                                                       # regenerate 15_edits.py
```
(The runs of section 4 used `SG_GL_BACKEND=wgl` before `--host-gl` existed; both select the same thing.)
