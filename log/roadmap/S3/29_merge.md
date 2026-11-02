# S3.29 merge (wave 9, candidate 1: U-1309)

## Code

`~/bin/py -3.12 log/roadmap/S3/29_assemble.py --root <tree> --only-29` after `w8_assemble.py` applies 29_edits.py (19 anchored edits) to
port/src/sim/gl_raster.cpp (16), port/include/portsim/sim/gl_raster.h (2) and port/test/test_gl_raster.py (1) of a tree holding the wave 8
stack's port/ (verified: base + edits = port/build/S3_29 byte for byte, on a fresh copy of the base). No shared file (modules.inc,
CMakeLists.txt, tick.cpp ...) is touched; gl_raster is an existing module. S3.28 does not touch these files this wave.

DEFAULT: OFF (`#tune 32` = 0). The rule is exact on 9 probes but not on 4 (U-1965), so by the brief's rule it stays behind the switch.
With tune 32 = 0 no walk is built and the default frames equal the wave 8 build's on every recorded stream and chain pass (29_notes.md
section 6). Opt in per stream with `#tune 32 1` or per process with `SG_RASTER_TUNE=32=1`.

## CLAUDE.md (suggested paragraph, after S3.21's / wave 8's)

Roadmap S3.29, U-1309 (2026-10-31, notes log/roadmap/S3/29_notes.md, U-1965..U-1969): the generic GL's textured interpolation, measured
with probes replayed through glreplay.exe (log/roadmap/S3/29_probes.py; slow ramps, alternating 0 / 255 texels that amplify u's error
255 x, random triangles with permuted vertex order, and boundary probes whose exact value sits on a 5-bit boundary for one dither cell so
the arithmetic decides it; fitted by 29_texfit.py), is in GlRaster behind `#tune 32 1` (default off), in both pixel loops: a textured
triangle whose texture has a GL_LINEAR filter is interpolated in float32 (gradients by the t1..t4 form from the lowest vertex, rows walked
from the largest y with the row start = float32(previous + float32(-dA/dy rows) + float32(dA/dx pixels)), restarted from the exact plane
where the left edge turns at the middle vertex, float32 sums along each span; colour at the 5-bit scale), and an unblended GL_MODULATE
pixel is filtered, modulated and dithered in float32 with texel = byte / 255.0f (GlRaster::TexWalk, walk_final). Exact on 9 probes
(alt_big 64,599 -> 1 is one of the 4 that are not: 1-2 float32 ulps of u or v left, U-1965); all-NEAREST textures take another path
(U-1966); the blend's float form is not determined (976 of 1,115,136 channel values, U-1967). The terrain's remaining residue is mostly
not interpolation (per-vertex lighting, coverage ownership 2e-6 .. 2.4e-3 px from an edge, blending: U-1968): c7's seven passes one-level
173 -> 154 with the rule; full census with the rule raw 123 -> 131 PASS of 698, differing px 6,203 -> 4,221, 0 PASS lost, split verdict
unchanged (677 PASS).

## Gates (29_notes.md section 6)

Default path unchanged: frames of all 505 recorded passes equal the wave 8 build's; G2 stream 271 / 271 (2,296 / 246) and chain 237 / 237
(1,605 / 185) as S3.21; segments 205 / 205 x64 + Win32 with 0 digests differing; boot 66 / 2 / 2 x64 + Win32 with 0 rows differing;
S3.10 bit identity 0 differ; test_gl_raster 278, port static 6, test_gl_wgl 9, test_gl_wgl_selfcheck 17 + 7 app skips (on a tree with
w8_fix_i21.py / w8_fix_i21b.py). Rule on: stream 1,938 / 246, chain 1,387 / 185, census raw 131 PASS (0 lost, 0 rows worse).

## UNCERTAINTIES.md

The five rows of log/roadmap/S3/29_uncertain.tsv (U-1965..U-1969). U-1309 is narrowed: its textured interpolation is measured (rule above,
opt-in); what is left of it is U-1965 / U-1966 / U-1967, and the terrain residue's other stages are U-1968 / U-1969.
