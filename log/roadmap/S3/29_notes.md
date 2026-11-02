# S3.29 notes: U-1309, GlRaster's textured interpolation (wave 9, candidate 1)

Worker: claude2 opus (Happy session under lead cmvu18cc81fcpp715vy6fd2dv), 2026-10-31, 07:53 start. No game run. Time box 10 h. U-1965..U-1969.
Tree: port/build/S3_29 = `w8_assemble.py --dst S3_29 --commit 2291a342` (the wave 8 stack: S3.21 part 1 on, 21b's `#tune 30` off) + this
step's edits (29_edits.py, 29_assemble.py --only-29). Base for the edits: port/build/S3_29_base (the same assembly, untouched).

RESULT: the generic GL's textured interpolation is measured and in GlRaster behind `#tune 32 1`, in BOTH pixel loops (the general loop
and S3.10's fast loop, workers included): a textured triangle whose texture has a GL_LINEAR filter is interpolated in float32 (float32
gradients from the lowest vertex, rows walked from the largest y, float32 sums along each span), and its unblended GL_MODULATE pixels
are filtered, modulated and dithered in float32 (texel = byte / 255.0f). Exact on 9 probes (among them two boundary probes built so
the final arithmetic decides every target pixel); NOT exact on 4 (1-2 float32 ulps of u or v on 35 of about 1 M pixels, U-1965).
DEFAULT STAYS OFF (the brief's rule: exact on every probe). On the terrain the rule lowers the residue but the residue is mostly other
stages (per-vertex lighting, coverage ownership, blending: U-1968), so it does not bring the census near the host-GL numbers: with the rule the raw census goes 123 -> 131 PASS of 698
(differing px 6,203 -> 4,221, 0 PASS lost, 0 rows worse, the split verdict unchanged), section 6. Every gate of the lead's close-out list
passed (section 6).

## 1. Where the residue is (wave 8 tree, before any change)

`29_resid.py` on the seven c7 passes S3.21 used (sandbox, zoom1, z2_r1, hole, fuzz1_z0, fuzz7_z1, view3): 173 one-level px, ALL in
GL_LINEAR-filtered pixels (rho > 1; the terrain's textures are mag NEAREST / min LINEAR, rho 2.26 on the drawn pixels; fuzz passes also
draw 7 % NEAREST-magnified pixels, none of them in the residue), 20 blended. Margins (8-bit units): 76 under 0.02, 51 in 0.02..0.1, 52
above 0.1.

## 2. Probes and the rule (log/roadmap/S3/29_probes.py through re/tools/glexact/glreplay.exe; fits 29_texfit.py)

Scenes (port/corpus/gl/probe/s329_*, local): slow ramps (uramp / vramp: the texture coordinate through a smooth texture; cramp: the
colour over a constant texture) with both filters LINEAR, both NEAREST, or the terrain's mix; `alt_big` (alternating 0 / 255 texel
columns at du/dx = 2 + eps texels per px: the filtered red is 255 * alpha, so u's error shows with a gain of 255 per texel; s up to 25);
`tris_lin`, `perm_rand`, `perm_big` (random triangles, random textures at 2..4 texels / px, vertex order rotated and winding reversed
at random); `fin_near` / `fin_lin` (boundary probes: 1,452 constant-colour quads each sampling one texel or one fixed bilinear mix,
the colour chosen so the exact value sits on a 5-bit boundary for one dither cell: the arithmetic decides that cell); `fin_blend`
(the same over a flat background with GL_BLEND); `iso*` (the terrain's tile shape: four triangles around the centre, texcoords 0 /
0.5 / 1, Gouraud colours); `iso_cov*` (iso with one constant-colour texture per neighbouring triangle: the real frame names each
pixel's owner).

What the probes showed, in order (wrong px of alt_big unless stated; 29_flips.py reads the u error at the dither flips):
1. The u error grows along each span from the left edge, its growth rate stepping up where s crosses powers of two: float32
   accumulation. Plane 64,599 -> per-pixel float32 sums from an exact row start 40,986.
2. The row starts are walked too, from the largest-y vertex: walk with double gradients 32,397, with float32 gradients 6.
3. Gradient form (float32, every product / sum rounded): deltas from one vertex c, area = dxAC dyBC - dxBC dyAC, r = 1 / area, t1 =
   dyAC r .. t4 = dxBC r, dA/dx = dAC t2 - dBC t1, dA/dy = dBC t3 - dAC t4, with c = the LOWEST vertex (first in stream order on a tie):
   alt_big 1, perm_big 935 -> 28 (other anchors 55..65; divide instead of reciprocal 31; x87-style double intermediates 24..29 on
   perm_rand against 16).
4. Restart: when the middle vertex lies left of the long edge (the left edge turns there), the first row below it takes float32 of the
   exact plane again: perm_big 28 -> 12, perm_rand 31 -> 16. Row step = float32(float32(-dA/dy * rows) + float32(dA/dx * pixels)),
   start = float32(previous + step) (two separate adds 111, repeated pixel steps 117, precomputed little / big steps 23, an
   edge-carried walk with a prestep to the first pixel 159..1,041, all on perm_rand).
5. Colour: walked the same way at the 5-bit scale, float32(colour) x 31 (scale 1: cramp_lin 93, 255: 85, 31: 0; gour_rand 306 -> 0).
6. Final stage (the boundary probes): texel = byte / 255.0f (a float32 DIVISION), GL_LINEAR weights (1 - a)(1 - b), a (1 - b),
   (1 - a) b, a b in float32 from a = frac(u - 0.5) of the float32 u, summed in texel order, times the float32 5-bit colour, plus the
   dither offset (k + 0.5) / 16 in float32, floored: fin_near 10,064 -> 0, fin_lin 9,792 -> 0 (byte * float32(1 / 255) 784 / 2,144;
   byte-scale product then / 255 1,056 / 3,104; the double model with the walk 9,136 / 9,264).
7. Path choice: textures with a GL_LINEAR filter only. All-NEAREST textures take another path: cramp_near 141,660 and uramp_near 75,964
   differ with the plane and with the walk alike (U-1966); the terrain's mix (mag NEAREST, min LINEAR) follows the walk also on
   magnified, NEAREST-sampled pixels (cramp_mix 31 -> 0, uramp_mix 22 -> 0). Untextured triangles keep their own path (S3.21b's
   16.16 stepping, `#tune 30`; gour_untex 481 unchanged).

Probe table (GlRaster px differing from the real GL; coverage differences, equal in both columns, in brackets):

| probe | rule off (plane) | `#tune 32 1` |
|---|---|---|
| alt_big | 64,599 | 1 |
| const_rand (S3.21) | 285 | 5 |
| gour_rand (S3.21) | 306 | 0 |
| gour_const (S3.21) | 15 | 0 |
| cramp_lin / cramp_mix | 31 / 31 | 0 / 0 |
| uramp_lin / uramp_mix / vramp_lin | 22 / 22 / 22 | 0 / 0 / 0 |
| tris_lin | 4 (+5) | 0 (+5) |
| perm_rand | 133 (+3) | 16 (+3) |
| perm_big | 935 (+1) | 12 (+1) |
| fin_near / fin_lin | 10,064 / 9,792 | 0 / 0 |
| fin_blend (blended: double blend kept) | 11,264 | 10,272 |
| cramp_near / uramp_near (all-NEAREST: not this path) | 141,660 / 75,964 | unchanged |
| iso / iso_z1 / iso_z0 (one-level / beyond / coverage) | 69 / 25 / 13, 121 / 13 / 8, 84 / 50 / 32 | 38 / 25 / 13, 30 / 13 / 8, 71 / 50 / 32 |

What is left (U-1965): every remaining wrong pixel of perm_rand / perm_big / const_rand / alt_big flips with 1-2 float32 ulps of u or v
(29_needed.py; one needs 10 ulps in a row start 34 rows down); the variants listed above do not reproduce them.

## 3. Blending (U-1967), not in the rule

29_blendfit.py on fin_blend (1,115,136 channel values): GlRaster's double blend 11,264 wrong; the best float32 form, src * a + dst * (1 - a)
in the 5-bit scale with a = alpha byte / 255.0f and the dither added in float32, 976 (61 of 1,452 boundary cells); the other forms 1,008 ..
5,792. Not exact, so blended walked pixels keep GlRaster's double blend (also under `#tune 32`).

## 4. The terrain residue that is not interpolation (U-1968)

With the rule the c7 one-level px go 173 -> 154 (fuzz7_z1 7 -> 0, zoom1 11 -> 7, z2_r1 11 -> 8, view3 7 -> 5, sandbox 11 -> 10; fuzz1_z0
119 -> 117, hole 7 -> 7). What stays (29_pxdump.py, 29_locate.py):
- fuzz1_z0's mass: whole tile bands (rows 414..434) with both signs and margins up to 0.017 5-bit units: per-vertex colour, the
  lighting stage (U-1308).
- 32 px within 0.005 px of a triangle edge (the same 32 with the rule off; GlRaster's tie rules 1..5 change nothing on c7: no centre
  is exactly on an edge): coverage ownership. On iso_cov / iso_cov_z0 51 / 100 px take another triangle than the exact-edge rule gives,
  the real owner's edge passing 2e-6 .. 2.4e-3 px on the far side of the centre, also within the first rows of an edge; fixed-point
  edge walks at 2^-4 .. 2^-16 (49 .. 111), float32 edge walks (53 .. 55 / 100) and unsnapped vertices (114 / 197) do not reproduce it
  (29_covfit.py). These are also the iso probes' pixels beyond one level.
- 20 blended px (section 3).

## 5. GlRaster changes (port/src/sim/gl_raster.cpp, port/include/portsim/sim/gl_raster.h; kTunes 32 -> 40)

- `GlRaster::TexWalk` + `tex_walk_()`: per row of the triangle's box the first covered pixel (the general loop's coverage test) and the
  float32 row-start values of u, v (texel units) and r, g, b (x 31), and the float32 x gradients (section 2, steps 2..5). Built in
  triangle() when `#tune 32` != 0, the setup is the fixed-point one with the float32 vertex rule (tune 26 = 28 = 0), the triangle is
  textured and its texture is not GL_NEAREST in both filters.
- General loop: per row a float32 accumulator from the row's first pixel, one float32 sum per pixel; u, v and the colours of the
  pixel come from it; an unblended GL_MODULATE pixel (tune 11 = 0) goes through `walk_final()` (step 6).
- Fast loop (S3.10): the job carries the walk (a shared_ptr, so the workers' 4-row bands read the same row starts); a walked pixel
  skips the checked reciprocal path, takes the same accumulator and `walk_final()` or, when blended, the exact branch's expressions with
  the walked values. Allowed knobs for the fast loop: 11 and 32..35.
- Knobs: 32 = 1 the rule; 33 the gradient anchor (1 highest, 2 first in stream order: controls); 34 = 1 no restart (control); 35 = 1 the
  double final stage with the walked values (control). With tune 32 = 0 no walk is built and both loops run as before (section 7).
- port/test/test_gl_raster.py: test_s329_walk_is_exact_on_the_probe (9 probes), test_s329_walk_residue_is_bounded (the 4 inexact ones at
  their measured counts), test_s329_control_rule_off_reads_red (6), test_s329_control_wrong_detail_reads_red (35 = 1 on fin_near / fin_lin,
  33 = 1 on alt_big, 33 = 2 and 34 = 1 on perm_big), test_s329_walk_is_the_same_in_every_pixel_loop (general / fast / exact / 4 workers
  on c7 sandbox, fuzz1_z0, fuzz7_z1), test_s329_walk_lowers_the_c7_one_level_pixels (154 / 173). The probe tests skip without the local
  probe scenes.

## 6. Gates (log/roadmap/S3/29_gates.sh, 29_census.sh, 29_equiv.sh; logs and TSVs in 29_out/)

| gate | result | reference |
|---|---|---|
| builds x64 / Win32 (port/test/build.py) | OK (29_build_x64.log, 29_out/g_build_w32.log) | - |
| probes (section 2) | 9 exact with the rule; 4 not (perm_rand 16 + 3 coverage, perm_big 12 + 1, const_rand 5, alt_big 1) | rule off: table of section 2 |
| S3.10 bit identity (12_equiv.py on S3_29, 268 passes + chain) | default: fast / exact / 4 workers / chain differ from the general loop on 0; `SG_RASTER_TUNE=32=1`: 0 (29_out/equiv_*.log) | S3.21: 0 |
| stream replays in G2 (1b_g2_stream.py, 271), rule ON | 271 / 271 OK; one-level 1,938, beyond 246 (29_out/g2_stream_on.log) | S3.21: 2,296 / 246 |
| draw-list chains in G2 (1_g2.py, 237), rule ON | 237 / 237 OK; one-level 1,387, beyond 185 (29_out/g2_chain_on.log) | S3.21: 1,605 / 185 |
| stream / chain with the rule OFF (default) | stream 271 / 271 OK, one-level 2,296, beyond 246; chain 237 / 237 OK, 1,605 / 185 (29_out/g2_stream_off.log, g2_chain_off.log) | S3.21: the same four numbers |
| default frames against the wave 8 build (29_framehash.py: SHA-256 of every stream replay and chain frame, S3_29's DLL against port/build/S3_W8's) | 505 passes (271 stream + 234 chain), 0 differ (29_out/framehash_*.json) | - |
| full census, rule ON (`SG_RASTER_TUNE=32=1`, framegate --split, 70 scenarios, 698 rows; 29_census_cmp.py vs w8_out/census_split.tsv) | raw 131 PASS / 557 FAIL / 0 / 10 NOT_RUN (was 123 / 565 / 0 / 10): FAIL -> PASS 8, PASS lost 0, rows with more px 0 (fewer 516); differing px 6,203 -> 4,221 (one-level 4,936 -> 2,954, beyond 1,267 -> 1,267); split 677 / 11 / 0 / 10 unchanged, 0 PASS lost, terrain-origin px 5,111 -> 3,129, 2D px 1,092 unchanged | w8 raw 123 / 565, split 677 / 11 |
| full census, default | not run (lead's close-out list): the default frames equal the wave 8 build's on all 505 recorded passes (row above) | - |
| segments x64 / Win32 (integrator segments --jobs 1) | 205 / 205 PASS both; 0 rows differ in status, digest or frames from w8_out/segs_x64.tsv / segs_w32.tsv | 205 / 205 |
| boot x64 / Win32 | 66 PASS / 2 KNOWN / 2 NO_FRAMES both; 0 rows differ in status or records from w8_out/boot_x64.tsv (x64) and from merges/I20 / I19 boot_win32.tsv (Win32; wave 8 kept no Win32 boot table) | 66 / 2 / 2 |
| test_gl_raster.py (x64), first -k s329 run | 26 passed, 2 failed on bounds that counted coverage pixels; bounds corrected (perm_rand 19, perm_big 13 = values + coverage) | - |
| test_gl_raster.py whole file (x64, corrected bounds) | 278 passed (the 250 earlier tests + the 28 S3.29 ones) | 250 |
| port test_static.py | 6 passed | 6 |
| test_gl_wgl.py / test_gl_wgl_selfcheck.py (x64, with w8_fix_i21b.py) | 9 passed / 17 passed, 7 skipped (the 7 app tests that need SG_APP_EXE); test_default_rasteriser_passes_the_self_check passed with the rule off | - |
| c7 seven passes (21_tunes.py) | one-level 173 -> 154, beyond 22 -> 22 | - |

Process: the step ran past its 10 h box (the session that held it ended after 11:52 while the census ran; the census finished at 16:14;
the box ended 17:53). The lead accepted the step as a finding and asked for the default-path gates without new work (close-out,
18:35-19:26): log/roadmap/S3/w8_fix_i21.py and w8_fix_i21b.py applied to port/build/S3_29 (they touch test_gfx_pixel.py,
test_gfx_reports.py, gl_wgl.cpp / .h, test_gl_wgl*.py, none of 29_edits.py's three files; the wave 8 DLL used for the frame hashes has no
fix layer, which does not reach GlRaster), x64 and Win32 rebuilt, then 29_final.sh: 13 holds, one build, frame dump, G2 run, boot step or
pytest file each, inside one test slot (nested test-slot calls pass through), all rc 0.

## 7. Controls

- Rule off (the plane): every inexact-before probe RED (table of section 2; test_s329_control_rule_off_reads_red).
- Wrong details RED: double final stage (35 = 1) fin_near 0 -> 9,136, fin_lin 0 -> 9,264; highest-vertex anchor (33 = 1) alt_big 1 -> 33,172;
  stream-first anchor (33 = 2) perm_big 12 -> 44; no restart (34 = 1) perm_big 12 -> 28.
- S3.10's controls with the rule on (12_equiv.py, SG_RASTER_TUNE=32=1): `#workerctl 1` 239 passes changed (RED); `#fastpath 4` 0 (blind by
  construction: walked pixels never take the reciprocal path the control perturbs; with the rule off it reads 46 as before); `#fastpath 3`
  0 (blind, U-1389, as before).

## 8. Commands

```
~/bin/py -3.12 log/roadmap/S3/w8_assemble.py --dst S3_29 --commit 2291a342
~/bin/py -3.12 log/roadmap/S3/29_assemble.py --root port/build/S3_29 --only-29        # or 29_assemble.py --dst <tree> for a fresh one
~/bin/py -3.12 log/roadmap/S3/29_make_edits.py                                        # 29_edits.py from S3_29 against S3_29_base
cd port/build/S3_29 && export SG_PORTSIM_DLL=$PWD/port/build/x64/Release/portsim_c.dll
~/bin/py -3.12 ../../../log/roadmap/S3/29_probes.py [name ...] | alt | perm | fin | finlin | blend | iso | isocov   # the real frames
~/bin/py -3.12 ../../../log/roadmap/S3/29_cmp.py [--scenes a,b] "" "32=1"            # GlRaster against them per knob set
~/bin/py -3.12 ../../../log/roadmap/S3/29_texfit.py <scene> [model ...] [--final fd]  # the Python models (w6_plane_free = the rule)
~/bin/py -3.12 ../../../log/roadmap/S3/29_flips.py s329_alt_big --gain 31 --save f.npy; 29_ducmp.py; 29_needed.py; 29_detail.py
~/bin/py -3.12 ../../../log/roadmap/S3/29_blendfit.py; 29_covfit.py; 29_resid.py c7 [--knob=32=1]; 29_locate.py; 29_pxdump.py
~/bin/py -3.12 ../../../log/roadmap/S3/21_tunes.py "" "32=1"                          # the seven c7 passes
sh log/roadmap/S3/29_equiv.sh; sh log/roadmap/S3/29_gates.sh g2_all x64_all w32_all; bash log/roadmap/S3/29_census.sh on
~/bin/py -3.12 log/roadmap/S3/29_census_cmp.py log/roadmap/S3/29_out/census_on.tsv
```
(the probes need re/tools/glexact/glreplay.exe in the tree: untracked, copied from the repo root by the assembly.)
