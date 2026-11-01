# S3.21 notes: U-1392, GlRaster's one-level terrain residue (wave 8, W8.2)

Worker: claude2 opus (Happy session under lead cmvu18cc81fcpp715vy6fd2dv), 2026-10-30. No game run. Time box 8 h. U-1305..U-1309.
Tree: port/build/S3_21 = `w7_assemble.py --dst S3_21 --commit f2dff003 --plain` + this step's edits (21_edits.py, 21_assemble.py).

RESULT (5 h 10 min of the 8 h box): two rules of Windows' generic GL that GlRaster lacked, both measured exactly with a feedback-mode
probe and now in GlRaster (default on, `#tune 28 1` / `#tune 29 1` restore the old behaviour): (1) the float32 vertex arithmetic (the
S3.16 column bands: vertices within a float ulp of a 2^-8 tie), 973,749 / 973,749 corpus vertices equal; (2) frustum clipping drawn as
fans (the bands at the viewport edges). Residue: stream passes one-level 18,234 -> 2,296, census differing px 71,871 -> 11,972, census
111 -> 122 PASS with 0 lost, every regression gate unchanged. What is left (U-1309) is the attribute interpolation, measured to be
incremental 16.16 fixed-point stepping (exact on a ramp probe, 480 -> 90 on Gouraud quads), not implemented: GlRaster is not yet
bit-exact, so the census cannot reach the host-GL 638.

## 1. Where the residue is (before any change)

`21_xhist.py` (GlRaster's stream replay of a corpus pass against the original's dib_post, per 16-px column group) on c7 sandbox,
fuzz1_z0, zoom1, z2_r1, hole with the f2dff003 DLL (port/build/I19): 1,453 one-level px, of them x groups 0 (213), 41 (145),
42 (146), 49 (101): the S3.16 bands (x 0..15, 656..687, 784..799) are there in the stream replays, so they are GlRaster's, not the
live path's. `21_tris.py classify` splits the 1,453 by the triangle under each pixel (keep_float triangle ids):
- 320 in triangles with a vertex outside the viewport (rate 30.5 / 1e4 of those triangles' pixels);
- 1,102 in triangles with a vertex whose 256 * x' (section-18 model) lies within 0.03 of a rounding tie (rate 17.8 / 1e4);
- 31 elsewhere (0.2 / 1e4).
`21_tris.py snaperr`: the tie class is sharp: 1,095 of the 1,133 non-clip px sit in triangles with a coordinate within 0.01 (in
units of 2^-8 px) of a tie. The vertex columns in the bands: x 672.344 (256 x' frac 0.4937), x 0.0528 (0.5053); the bands at the
edges are the clipped triangles (vertices at x -15.95 / 800.40).

## 2. The generic GL's vertex arithmetic (measured; rule found)

Tool: `21_glfeed/glfeed.exe` (generated from re/tools/glexact/glreplay.c by 21_glfeed/gen.py + main.c.txt) replays a pass in
GL_FEEDBACK mode (GL_3D_COLOR_TEXTURE); `GLFEED_POINTS=1` draws every block as GL_POINTS, so the feedback holds one point per vertex
kept, in order. Feedback window coordinates are on the 2^-8 grid (every value of 973,749): they are the snapped vertices.
Probes (21_probe.py, synthetic corpus directories run through glfeed.exe):
- `viewport` (identity matrices, points given in NDC, 7,525 float32 values swept across ties): window = round_half_even(256 *
  (f32(f32(ndc) * f32(h - 2^-8)) + c)) / 256 (c, h: viewport centre and half-size): 0 wrong in x and y; GlRaster's double
  formula 162 / 185 wrong, a float32 sum before the rounding 611 / 500.
- `ortho` (the terrain's glOrtho alone, 13,243 points): every multiply and add of the transform in float32: 0 / 0; double
  accumulation (x87 style) 209 / 77.
- `rotations` (glRotatef 45 about y, glRotated 38.68 about x): any element arithmetic gives the same floats: 0 / 0.
- `transform` / `transform_fit` (glRotatef 37 about (0.3, 0.7, 0.5) + glTranslatef, identity projection, 14,000 points): the
  transform adds the translation FIRST (m3 + x m0 + y m1 + z m2, each op float32), and the rotation diagonal is the Red Book form
  x^2 + (1 - x^2) c (the (1 - c) x^2 + c form is one float ulp off on m00; 0 / 0 with the Red Book form).
On the recorded passes (`21_vfit.py`, `21_mvpfit.py`, `21_transfit.py`, `21_vcheck.py`): the 135-degree passes (zoom1 ...) needed one
more rule: the MVP is UPDATED by every modelview operation (MVP = ((P Rx) Ry) T, each a float32 matrix product with terms summed
k = 0..3), not recomputed as P * MV: zoom1's MVP[0][3] / MVP[1][3] (cancellation in c tx + s tz) are then exact.
RESULT (`21_vcheck.py`, log 21_vcheck_all.log): all 268 corpus passes (c3..c7, g2..g6), 973,749 vertices: 0 wrong in x and y;
GlRaster's double model 14,282 x / 6,427 y wrong.

## 3. The generic GL's clipping (measured; rule found)

Triangle feedback (glfeed.exe without GLFEED_POINTS): a clipped polygon comes back as a fan of triangles whose new vertices lie
on the viewport edge (window 2^-8 / w - 2^-8), with interpolated texture coordinates. `21_clipfit.py` models: cull first on the
snapped window positions of the unclipped triangle (glFrontFace GL_CW: back = strictly positive area; zero area is kept), then
Sutherland-Hodgman in clip space against x >= -w, x <= w, y >= -w, y <= w, z >= -w, z <= w, the classic output order (for each
edge prev -> cur: the intersection when the edge crosses, then cur when inside), t = d_in / (d_in - d_out) with d = w +- coordinate
in float, attributes in + t (out - in) in float from the inside vertex; the polygon is the fan of its first vertex.
c7 sandbox 3,380 / 3,380 triangles, zoom1 2,678, z2_r0 1,736, view3 3,333: positions and texcoords float-equal. fuzz1_z0: 3,933 /
3,933 positions equal; 36 triangles differ in a clipped vertex's texcoords, all in a block that binds texture 0 (incomplete, drawn
untextured), so no pixel depends on them (U-1307).

## 4. GlRaster changes (port/src/sim/gl_raster.cpp, port/include/portsim/sim/gl_raster.h)

- A float32 matrix state beside the double one (FMat fmv_ / fproj_ / fmvp_, fstack_[2], fstack_mvp_): fmul_ (each product and sum a
  volatile float, k = 0..3), fpost_ (projection ops recompute MVP = P * MV, modelview ops multiply the MVP too), load_identity /
  push / pop keep it; rotated builds the float32 rotation (Red Book diagonal, U-1306), translatef / ortho their float matrices.
  The double matrices stay for the lighting (eye-space normals and the light direction, unchanged).
- emit: with `#tune 28` = 0 (the new default) the vertex's float32 clip coordinates (translation first) go to Vtx::c and
  window_of_ gives the unrounded window position (Vtx::x / y) and the snapped one (Vtx::qx / qy, the measured viewport rule).
- triangle: the fixed-point setup takes qx / qy as its vertices (coverage and attributes, the fast loop's integers too); `#tune 28 1`
  restores the earlier scaled double model.
- clip_triangle_ (`#tune 29` = 0, only with tune 28 = 0): a triangle with a vertex outside the view volume is culled on its snapped
  positions (zero area not a back face), clipped (section 3) and drawn as the fan of the clipped polygon; the fan's own triangles
  skip the cull test. Unclipped vertices keep their own values (matched by clip coordinates and texcoords).
- SG_RASTER_TUNE=<i>=<v>,... sets knobs for every rasteriser of the process (the census control below; unset = all 0).
- port/test/test_gl_raster.py: test_s321_rules_meet_the_tight_bound (c7 sandbox / zoom1 / z2_r1 / hole: one-level sum <= 80,
  measured 40), test_s321_old_behaviour_breaks_the_tight_bound (the controls `#tune 28 1` 1,213 and `#tune 29 1` 216: RED),
  test_s321_vertex_rule_is_no_worse_than_the_double_model (every c7 pass).
Edits: 21_edits.py (20 anchored edits in the 3 files, generated by 21_make_edits.py from port/build/S3_21_base = plain f2dff003;
21_assemble.py applies them, verified to give the tree byte for byte). No shared file.

## 5. Numbers

c7, seven passes (21_tunes1.log; one-level / beyond): default 173 / 22; `#tune 28 1` (old vertex model) 1,730 / 23; `#tune 29 1`
(no clipping) 472 / 22. Per pass sandbox 188 -> 11, zoom1 342 -> 11, z2_r1 542 -> 11, hole 141 -> 7, fuzz1_z0 240 -> 119, fuzz7_z1
181 -> 7, view3 96 -> 7. The old knobs re-run on top of the rules (tune 3 colour bits, 8 texel rounding, 9 two-stage lerp, 12 float
lighting, 17 float attribute arithmetic) are all equal or worse (21_tunes1.log).
All recorded passes (G2, the S3.1b scripts; before = 1b_g2_stream.log / 1b_g2_all.log, whose rasteriser S3.10 kept bit-identical):
- stream replays (1b_g2_stream.py, 271 passes): 271 OK; one-level 18,234 -> 2,296 (0.046 % -> 0.006 % of 39.3 M drawn), beyond
  262 -> 246; by corpus c7 7,934 -> 691, g2 1,386 -> 42, g3 4,190 -> 1,317, g4 101 -> 3, g5 2,233 -> 117, g6 2,334 -> 113, c3..c6
  37 -> 11. 6 passes more one-level: g3 i028 / i083 paths and water strips 107 -> 120 / 121, i029 / i084 83 -> 94, g4 i022 / i029
  0 -> 1 (21_g2_stream.log).
- draw-list chains (1_g2.py, 237 passes, the port's renderer driving GlRaster): 237 OK; one-level 10,300 -> 1,605, beyond 188 -> 185.
- S3.10 bit-identity (12_equiv.py, SG_TREE=S3_21): 268 passes + chain: fast / exact / 4 workers differ from the general loop on 0;
  controls `#fastpath 4` change 46, `#workerctl 1` 239, `#fastpath 3` 0 (blind as in S3.12, U-1389) (21_equiv.log).
(Gate results: section 7, 21_out/g_gates.log. The G2 / equivalence runs above used the build before the SG_RASTER_TUNE reader and the
_CRT_SECURE_NO_WARNINGS line were added; the pipeline code is the same.)

## 6. What is left (U-1309) and the bounded search

After the two rules the remaining one-level pixels are in the attribute stage, not the setup: synthetic probes (21_modprobe.py on
port/test/glprobe.py's Scene, 48 quads of 90 px): Gouraud colour, texturing off, 481 of 388,710 px; constant texture under Gouraud
colour 15; constant colour over a random bilinear texture 285; Gouraud over a random texture 306. On the untextured probe the
wrong pixels are symmetric (240 one level up, 240 down), each within 1e-4..9e-4 of a 5-bit step: a colour error up to about 3e-5.
21_interpfit.py ruled out (every variant 454..481 wrong of 388,620 inner px, GlRaster 480): float32 / double plane evaluation
anchored at a vertex, colours scaled by 1 / 31 / 255; fixed-point span start + step at 2^-12..2^-23; the 5-bit / 8-bit scaled value
held with 6..23 fractional bits before the dither; planes from the unsnapped vertices; constant sample offsets within +-2/512 px;
edge walking with the edge x held to 2^-2..2^-16. A free per-triangle offset gives 194 (scattered offsets: over-fitting). Margins of
the remaining c7 pixels (21_margin.py): sandbox / zoom1 a mix of 3e-5..1e-4 (the float-lighting size, U-1308) and 0.4..2.7 8-bit
units (texel-level differences: coverage or filter decisions, not followed); fuzz1_z0 0.005..0.08 at the top rows. On the
constant-colour bilinear probe (285 px) the old knobs on top of the rules: weight bits 8 / 12 / 16 (floor or round) 17,805 / 9,940 /
1,101 / 726 / 331, texcoord fraction bits 12 / 16 / 20 1,101 / 331 / 278, filtered texel rounded 22,082: none removes it.

FINDING (not implemented): the attribute interpolation is incremental fixed-point stepping. A slow red ramp over the whole screen
(21_ramp.py dense: red rising by 3 dither steps across x and 7.3 across y, each dither phase's flip column read per row; 440 flips)
shows the real red = exact + an error that is LINEAR over each triangle, zero at the vertex of largest y and growing along the walk
(tri 2 about -3.3e-7 per row up its vertical left edge and about -2e-7 per pixel along a span). Model (21_interpfit.py edgewalk):
rows walk from the largest-y vertex towards smaller y; each row's first covered pixel takes the previous row's start value plus ONE
step trunc(31 * 2^16 * (-dv/dy * rows + dv/dx * (ix - ix_prev))) / (31 * 2^16), i.e. the colour scaled to the 5-bit range and held
as 16.16 fixed point, truncated; the first row's start is the exact plane value; along a span each pixel adds trunc(31 * 2^16 *
dv/dx) / (31 * 2^16). Result: the dense ramp 0 wrong of 477,802 inner px (the exact plane 705); the random Gouraud probe 480 -> 90
(all 90 in the 24 triangles whose horizontal edge is at the top, rows 29..85 of their walk). Not removed by: floor or rounding
instead of truncation (747 / 2165), the y and x parts held separately (210), float32 gradients from any anchor vertex (90), a
16.16 / 2^-8..2^-30 DDA for the edge's x schedule (88..90). So the stepping rule is right in kind and in its quantum, one detail of
the walk (which start value a row takes after the edge's x schedule changes, or where the first row starts) is still open.
Not implemented in GlRaster: it changes the interpolation of every attribute (colour, and texture coordinates if they step the same way, which is not measured; the
constant-colour bilinear probe's 285 px have the same size) in both pixel loops (the general loop and S3.10's fast loop with its
boundary checks), which the remaining time box does not hold together with the gates. Lead for the next step (U-1309).
No rule for this remaining part is implemented within the time box.

## 7. Gates (log/roadmap/S3/21_gates.sh on port/build/S3_21; every step under the wave-8 lock, --jobs 1; logs 21_out/g_<step>.log)

| gate | result | base (f2dff003 / I19) |
|---|---|---|
| builds x64 / Win32 (port/test/build.py, warnings as errors) | OK | - |
| test_gl_raster (x64) | 248 passed (the S3.21 tests included) | - |
| all recorded passes in G2 (stream 271, chain 237) | 271 / 271, 237 / 237 OK | 271 / 237 OK |
| S3.10 bit-identity (12_equiv.py: fast / exact / 4 workers / chain vs general) | 0 differ on 268 + chain | 0 |
| census sample (sandbox_basic, cov_k1_river, championship, cov_k3_camera2, cov_a_build; 133 rows) | 4 PASS / 129 FAIL, 0 PASS lost, 0 rows with more px; px 24,960 -> 1,902 | 4 / 129 |
| census control `SG_RASTER_TUNE=28=1,29=1` (sandbox_basic, cov_k1_river; 82 rows) | 20,287 px = I19's, every row equal (RED against 1,145 with the rules) | - |
| census sample Win32 (the same 82 rows) | equal to x64 in every row (status, px per region) | - |
| S3.2 terrain verdicts (2_census.py terrain) | 527 PASS / 75 NOT_MEASURED; diff px 94,821 -> 6,039, one-level 94,133 -> 5,376, beyond 688 -> 663; 5 checkpoints with 1..2 more beyond px (cov_r_course loaded / run_a, stories9_fixture loaded / view / season_end: totals 282 -> 26, 322 -> 30); exact checkpoints 13 -> 15 | 527 / 75 |
| full census (framegate --all --no-probe, GlRaster default; 21_census_cmp.py vs merges/I19/census.tsv) | 698 rows: 122 PASS / 566 FAIL / 0 UNSUPPORTED / 10 NOT_RUN; FAIL -> PASS 11, PASS lost 0, rows with more px 0; px 71,871 -> 11,972 (one-level 66,458 -> 6,593, beyond 5,413 -> 5,379); by region view 45,842 -> 9,849, hud_meters 8,884 -> 721, bottom_panel 7,284 -> 216, hud_message 3,985 -> 313, hud_title 3,915 -> 596, toolbar 1,961 -> 277; FAIL rows by px: 1-9 339, 10-19 190, 20-49 28, 50-99 4, 100-999 3, 1000+ 2 (before 13 / 3 / 24 / 242 / 292 / 3) | 111 / 577 / 0 / 10 |
| test_terrain_render (x64) | 522 passed | - |
| segments x64 (integrator segments --jobs 1) | 205 / 205 PASS, `integrator compare` vs merges/I19/segs_x64.tsv: 0 rows differ | 205 |
| boot x64 (boot --all --jobs 1) | 66 PASS / 2 KNOWN / 2 NO_FRAMES, every row equal to merges/I19/boot_x64.tsv | 66 / 2 / 2 |
| segments Win32 | 205 / 205 PASS, 0 rows differ from merges/I19/segs_win32.tsv | 205 |
| port test_static | 6 passed | 6 |
| boot Win32 | 66 PASS / 2 KNOWN / 2 NO_FRAMES, every row equal to merges/I19/boot_win32.tsv | 66 / 2 / 2 |

Controls: `#tune 28 1` (old vertex model) and `#tune 29 1` (no clipping) break the tight bound in test_gl_raster (1,213 and 216 against
<= 80: RED 2 / 2) and are the switches the census control uses (`SG_RASTER_TUNE=28=1,29=1`: f2dff003's rows back exactly: RED against the
rules' 1,145 px on the same 82 rows). S3.10's controls still read: `#fastpath 4` 46 passes changed, `#workerctl 1` 239, `#fastpath 3` 0
(blind, U-1389, as before). Blind: none of the new controls is equivalent on its data. Default: the rules are on (21_merge.md: the brief's
"lowers but does not remove" sentence is the lead's call; one line or SG_RASTER_TUNE reverts).

## 8. Commands

```
~/bin/py -3.12 log/roadmap/S3/21_assemble.py --dst S3_21             # tree = plain f2dff003 + 21_edits.py (or --root <tree>)
~/bin/py -3.12 log/roadmap/S3/21_make_edits.py                       # regenerate 21_edits.py from port/build/S3_21 against S3_21_base
sh log/roadmap/S3/21_gates.sh build_x64 build_w32 t_test_gl_raster sample sample_ctl28 sample_w32 terrain census t_test_terrain_render \
   t_test_static segs_x64 boot_x64 segs_win32 boot_win32           # each step one `w8_lock.py run S3.21` hold, logs 21_out/
~/bin/py -3.12 log/roadmap/S3/21_census_cmp.py log/roadmap/S3/21_out/census.tsv [--out <tsv>]   # vs merges/I19/census.tsv
# the oracle probes (no game; glfeed.exe built by `~/bin/py -3.12 log/roadmap/S3/21_glfeed/gen.py` + 21_glfeed/build.bat, x86 cl)
GLFEED_POINTS=1 log/roadmap/S3/21_glfeed/glfeed.exe port/corpus/gl/c7 sandbox <out.f32>             # per-vertex feedback points
S321_SCRATCH=<dir> ~/bin/py -3.12 log/roadmap/S3/21_vcheck.py [corpus ...]                          # the vertex rule on every pass
S321_SCRATCH=<dir> ~/bin/py -3.12 log/roadmap/S3/21_probe.py viewport|ortho|rotations|transform|transform_fit
S321_SCRATCH=<dir> ~/bin/py -3.12 log/roadmap/S3/21_clipfit.py c7 sandbox classic_in2out_zero      # the clipper vs triangle feedback
cd port/build/S3_21 && export SG_PORTSIM_DLL=$PWD/port/build/x64/Release/portsim_c.dll
~/bin/py -3.12 ../../../log/roadmap/S3/21_xhist.py c7 sandbox fuzz1_z0 zoom1 z2_r1 hole            # residue per 16-px column group
~/bin/py -3.12 ../../../log/roadmap/S3/21_tunes.py "" "28=1" "29=1"                                 # c7 seven passes, knobs
~/bin/py -3.12 ../../../log/roadmap/S3/21_modprobe.py                                               # MODULATE / Gouraud probes
~/bin/py -3.12 ../../../log/roadmap/S3/21_ramp.py dense; ~/bin/py -3.12 ../../../log/roadmap/S3/21_interpfit.py edgewalk   # U-1309
```
(glprobe needs re/tools/glexact/glreplay.exe in the tree: it is untracked, copy it from the repo root, as in S3.1b.)

## 9. Open (U-1305..U-1309)

- U-1309 the attribute stepping (section 6): the model is exact on the slow-ramp probe and leaves 90 of 480 on random Gouraud quads;
  then texture coordinates, then both of GlRaster's pixel loops. This is where the census FAIL rows' last 1..19 px are.
- U-1308 float lighting (at most 4e-5 of a level), U-1307 untextured clipped texcoords (no pixel), U-1306 general-axis rotation details
  (not used by the terrain), U-1305 the perspective divide (not used).
- Not run: test_gl_raster on the Win32 backend (the sg_raster_* entry points are x64 ctypes only, as since S3.1b); x64 = Win32 was judged
  on the census sample rows (82 / 82 equal) and on segments / boot.

## 10. Part 2 (S3.21b): the attribute stepping in GlRaster, opt-in; a finding, default unchanged

Layer: 21b_edits.py (4 anchored edits: gl_raster.cpp 3, test_gl_raster.py 1; the header is unchanged), generated by 21b_make_edits.py
from the part-1 state of the files (port/build/S3_21_p1) and applied by `21b_assemble.py --root <tree>` AFTER 21_assemble.py; verified:
base + 21_edits = part 1, part 1 + 21b_edits = port/build/S3_21, byte for byte. Part 1 stays mergeable alone.
Code: in the general loop, `#tune 30` <> 0 runs a pre-pass per triangle (rows from the largest y down, each row's first covered pixel, the
first row's plane value, later rows one truncated 16.16 step, pixels the truncated x step) and overrides the colour; `#tune 31 <F>` also
steps the texture coordinates in texel units at 2^-F. tune 30: 1 every triangle with the 5-bit quantum 31 * 2^16; 2 untextured triangles
only; 3 / 4 / 5 textured triangles with 255 * 2^16 / 2^16 / 31 * 2^19. Any non-zero tune 30 sends the triangle to the general loop (S3.10's
fast-loop rule: every knob but 11 must be 0), so the fast loop is not changed; with tune 30 = 0 (the default) both loops are unchanged.
Probes (21b_probes.py against the real GL's stored frames; px differing, all one level, coverage 0; log 21b_probes.log):

| knobs | ramp_dense | gour_untex | gour_const (constant texture x Gouraud) | const_rand (bilinear, constant colour) | gour_rand |
|---|---|---|---|---|---|
| default (plane) | 705 | 481 | 15 | 285 | 306 |
| 30=1 | 0 | 90 | 298 | 285 | 434 |
| 30=1, 31=16 / 12 / 8 | 0 | 90 | 298 | 3,535 / 41,313 / 192,588 | 3,199 / 35,096 / 147,134 |
| 30=2 (untextured only) | 0 | 90 | 15 | 285 | 306 |
| 30=3 / 4 / 5 | 0 | 90 | 38 / 9,559 / 46 | 285 | 322 / 7,490 / 315 |

So the measured stepping holds for untextured Gouraud (exact on the ramp, 481 -> 90), but under GL_MODULATE neither the colour nor the
texture coordinates step that way: every stepped variant is worse than the plane on the textured probes. The terrain is textured: on the
seven c7 passes (21_tunes.py, 21b_tunes_c7.log) default 173 / 22, `30=2` 173 / 22 (identical: no untextured terrain pixel among the
residue), `30=1` 182 / 22 (worse). The terrain's remaining one-level pixels are therefore in the textured interpolation, which this model
does not describe: NOT exact, default left OFF, as the lead's rule says.
Gates of the layer: x64 and Win32 build; test_gl_raster 250 passed (248 + test_s321b_stepping_is_exact_on_the_ramp_probe with `30=2`, and
its control test_s321b_control_stepping_off_reads_red: the plane gives 705 > 100, RED; both skip without the local probe scene); S3.10
bit-identity (12_equiv.py, 21b_equiv.log): 268 passes + chain, fast / exact / 4 workers differ from the general loop on 0; controls `#fastpath 4` 46, `#workerctl 1` 239, `#fastpath 3` 0, identical to part 1. Not re-run for the layer: the 271 / 237 G2 passes, the census,
segments and boot, because with the default (tune 30 = 0) no code path of the default pipeline changes (the pre-pass and the override are
skipped; the fast loop is untouched), and the orchestrator asked for a quiet window for I20.
Open (U-1309, narrowed): the textured (MODULATE) colour and texture-coordinate interpolation of the generic GL; the 90 untextured px of
gour_untex (horizontal-top-edge triangles). Next probe: a slow ramp of the texture coordinate on a smooth texture (one coordinate at a
time) and a slow colour ramp over a constant texture, read the way 21_ramp.py dense reads the untextured colour.
