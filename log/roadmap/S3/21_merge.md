# S3.21 merge (wave 8, W8.2: U-1392)

## Code

`~/bin/py -3.12 log/roadmap/S3/21_assemble.py --root <tree>` applies 21_edits.py (20 anchored edits) to port/src/sim/gl_raster.cpp,
port/include/portsim/sim/gl_raster.h and port/test/test_gl_raster.py of a tree holding f2dff003's port/ (verified: base + edits =
port/build/S3_21 byte for byte). No shared file (modules.inc, tick.cpp, CMakeLists.txt ...) is touched; gl_raster is an existing module.

Default: the two measured rules are ON (`#tune 28` = 0, `#tune 29` = 0). They are exact reproductions of two stages of the generic GL
(every vertex of the 268 corpus passes, the clipper's output triangles on the passes checked), not fitted approximations, and they meet
the brief's default conditions (all recorded passes within G2, S3.10 bit-identity, lower residue on c7 and on the census sample with no
row worse and no PASS lost). They do NOT remove the residue (the attribute stepping, U-1309, is left), so the brief's sentence "a rule
that lowers the residue but does not remove it is a finding, not a default" applies to the step as a whole: if the lead reads it that
way, the default goes back with one line (`tune` defaults are 0 = the rules; set `tune[28] = tune[29] = 1` in GlRaster's constructor, or
`SG_RASTER_TUNE=28=1,29=1` per process), and the old renderer comes back row for row (census control, 21_notes.md section 7).

## CLAUDE.md (suggested paragraph, after S3.20's)

Roadmap S3.21, U-1392 part 1 (2026-10-30, notes log/roadmap/S3/21_notes.md, U-1305..U-1309): GlRaster's terrain residue came from
two stages of Windows' generic GL it did not model, both measured exactly with a feedback-mode probe (log/roadmap/S3/21_glfeed/
glfeed.exe: the pass replayed in GL_FEEDBACK, GLFEED_POINTS=1 one point per vertex): (1) the VERTEX RULE: float32 matrices, the MVP
updated by every modelview operation (not recomputed as P * MV), clip = m3 + x m0 + y m1 + z m2 with every operation float32 (the
translation FIRST), window = round-half-even(f32(ndc * f32(h - 2^-8)) + centre) to 2^-8: all 973,749 vertices of the 268 corpus passes
equal (the double model missed 20,709, the vertices within a float ulp of a 2^-8 tie: S3.16's x bands); (2) CLIPPING: triangles crossing
the view volume are culled on their snapped positions (zero area kept), clipped in clip space (x >= -w, x <= w, y, z; classic
Sutherland-Hodgman order; t = d_in / (d_in - d_out) and attributes in float from the inside vertex) and drawn as the fan of the
polygon. GlRaster's `#tune 28 1` / `#tune 29 1` restore the old model / no clipping, `SG_RASTER_TUNE=<i>=<v>,...` sets knobs for a
process. One-level px of the 271 stream passes 18,234 -> 2,296, of the 237 draw-list chains 10,300 -> 1,605; census sample (5 scenarios,
133 rows) differing px 24,960 -> 1,902 with no row worse and no PASS lost; full census: 21_notes.md section 7. What is left (U-1309) is
the attribute interpolation: incremental 16.16 fixed-point stepping of the 5-bit-scaled colour, truncated, rows walked from the
largest-y vertex (exact on a slow-ramp probe, 480 -> 90 on random Gouraud quads), not yet in GlRaster.

## UNCERTAINTIES.md

The five rows of log/roadmap/S3/21_uncertain.tsv (U-1305..U-1309); U-1392 stays open (narrowed to U-1309).

## Part 2 (S3.21b, optional layer)

`~/bin/py -3.12 log/roadmap/S3/21b_assemble.py --root <tree>` after 21_assemble.py applies 21b_edits.py (gl_raster.cpp 3 edits,
test_gl_raster.py 1). Default OFF (`#tune 30` = 0): frames unchanged. It is a measured but partial model (untextured triangles only, not
the terrain's residue): merging it is optional and changes nothing by default; leaving it out loses only the opt-in knob and its two tests.
UNCERTAINTIES.md: U-1309's row in 21_uncertain.tsv now narrowed (section 10 of the notes).
