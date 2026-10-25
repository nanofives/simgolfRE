# S3.1b notes: the rasteriser residual on type-7-heavy states (U-1300)

Worker: claude2 opus (session cmvmu9ghb48c1o115mer6hp9w), 2026-10-24. Lead: S3 (cmvmns8sn3fsbo115g4dgm1nf). No game run.

## 1. Result

U-1300 is resolved. GlRaster missed G2 on 16 of g6's 28 passes because of two rules of Windows' generic OpenGL it did not model:

1. **The specular term is a table lookup** (U-1390). The generic GL does not evaluate (n.h)^shininess: it reads a table of 256 entries
   spanning [t, 1], t = 0.0005^(1 / shininess), at the NEAREST entry, no interpolation:
   `i = round((n.h - t) * 255 / (1 - t))`, value `(t + i * (1 - t) / 255)^shininess`, 0 for n.h <= t.
   (port/src/sim/gl_raster.cpp `spec_lookup`; `#tune 27 1` restores the exact pow.)
2. **The tie rule for pixel centres exactly on an edge** (U-1391): an edge (counter-clockwise, window y = memory row) owns them when it
   points to +y, or to +x when horizontal (as spans: ymin <= y + 0.5 < ymax, xl < x + 0.5 <= xr). The old rule took -x on horizontal
   edges. (`inclusive` in GlRaster::triangle; `#tune 10 2` restores the old rule; tune 10's other values are unchanged.)

How each was found:
- g6's passes replay BIT-EXACT through the real software GL (re/tools/glexact/glreplay.exe on g6/i012 and i005: 0 of 480,000 pixels
  differ), so glreplay is an exact oracle for any edited pass.
- log/roadmap/S3/1b_probe.py splits a pass into its glBegin..glEnd blocks by category (t0: bound texture without an image, drawn with
  the lit colour; tex: textured, blending off; blend: blending on), keeps every state call and only one category's blocks, renders
  that variant with glreplay and with GlRaster from the same dib_pre (scratch root port/build/S3_1b_probe, a copy of the corpus's
  stream and a junction to its tex/). Before the fixes, i012: t0 6.25 % one-level (4,836 of 77,415), tex 0.37 %, blend 0.16 %; g5's
  i012 t0 had the same 6.29 %. i005: 134 of its 145 big pixels were in blend blocks.
- 1b_margin.py: every one of i012's t0 errors is the same pixel colour (232.1254, 232.1254, 226.4513; all type-7 tiles of that pass
  share one normal) at dither cell k = 12, where our colour lies 0.0034 (8-bit units) above the threshold 28.21875 / 31 * 255.
- port/test/glprobe.py `untex` (new): 1,452 constant-colour 16 x 16 quads (each covers every dither cell 16 times). Unlit float
  colours: 0 differences, with texturing off and with an incomplete texture bound alike (so `incomplete = untextured` holds and no
  8-bit quantisation of colours happens). Lit with the terrain's light (initGL's glLightfv / glMaterial values): 2,080 differing
  pixels. 1b_litfit.py with one light term at a time: ambient only and diffuse only 0 of 4,356 quad-channels outside the interval
  the dither cells allow; specular only 540 .. 627 per seed, errors of both signs up to 0.3 level.
- The real specular values form steps in n.h (two different n.h giving the same 1/16-level interval while the pow moves through ten
  of them); step 0.0017 near 1 = (1 - t) / 255 with t = 0.5573 = 0.0005^(1/13). Fit over 4,923 quads of four seeds: 0 violations
  with eps = 0.0005 exactly, N = 256, round-to-nearest index; 641 with floor, 121 / 197 with N = 255 / 257, 793 with the exact pow;
  float32 or double evaluation both 0 (not determined, U-1390). After the change every probe pixel is equal at shininess 1, 5, 13,
  40 and 128 (pow: 656 .. 7,584 differing).
- i005's big pixels came from five path-band triangles whose horizontal bottom edge is at window y 344.5024, which the fixed-point
  setup rounds to 344.5 exactly: the real GL gave row 344 to them, GlRaster to the band below. `glprobe tie` (new): triangles with
  vertices on pixel centres inside |ndc| < 0.5 (the setup leaves them unmoved), axis-aligned edges forced in 40 %; coverage
  differences per rule of tune 10: 288 (old), 923, 48 (new), 699, 508, 669. The 48 left are centres exactly on a vertex (40) or
  on a diagonal edge (8), with both choices for the same edge direction: no span rule gives them (U-1391).

## 2. Numbers (mirror port/build/S3_head = git archive c6d7d78b + S3.1's files, build port/build/S3_head/port/build/S3_1b)

- G2 of the PORT's stream (S3.1's 1_g2.py, kinds 1 / 3 / 4, log/roadmap/S3/1b_g2_all.log, comparison 1b_compare_g2.log): 237 / 237
  OK (S3.1: 221). g6 28 / 28 (was 12): mean one-level 1.21 % -> 0.04 %, worst 2.47 % -> 0.18 %, big px 746 -> 85 (worst pass 145 ->
  10). The other 209: 209 / 209, mean one-level 0.46 % -> 0.03 %, worst 1.25 % -> 0.21 %, big px 243 -> 103 (worst 24 -> 6); no pass
  has a higher one-level rate.
- G2 of the ORIGINAL's recorded stream (1b_g2_stream.py: every draw-list snapshot plus c7's 34 Terrain::render passes,
  log/roadmap/S3/1b_g2_stream.log, before = S3.1's DLL 1b_g2_stream_before.log, comparison 1b_compare_stream.log): 271 / 271 OK
  (before 255 / 271: the 16 g6 misses). Per corpus mean one-level before -> after: c3 0.34 -> 0.01, c4 0.33 -> 0.01, c5 0.35 -> 0.00,
  c6 0.45 -> 0.00, c7 0.46 -> 0.06 (worst 1.05 -> 0.21, big px 139 -> 74, worst 50 -> 13), g2 0.48 -> 0.04, g3 0.49 -> 0.03, g4 0.44
  -> 0.04, g5 0.37 -> 0.04, g6 1.21 -> 0.04 %; no pass worse.
- t3 (1b_t3.py over port/corpus/terrain/t3/frames, 1b_t3_before.log / 1b_t3_after.log / 1b_compare_t3.log): 15 / 15, mean 0.49 ->
  0.05 %, worst 0.56 -> 0.09 %, big 0 -> 0. So c7 + t3 (49 passes): mean one-level 0.47 % -> 0.06 %.
- Suites in the mirror: test_gl_raster 177 passed (139 old + 38 new, 1b_pytest_test_gl_raster.log), repo `-m static` 595 passed,
  5 skipped, 2 failed (the two shim-file tests a git archive cannot satisfy, the same as S3.1's mirror run), test_terrain_render 494 passed + 28 XPASS (the g6 xfails, S3.1's file: see
  1b_merge.md section 1), test_terrain_state 500 passed (t3 chain frames, zoom-2 control rejection incl. tune 26), port test_static 6.
- Segments: x64 205 / 205 PASS, 0 differ from log/roadmap/merges/S6-1/segs_x64.tsv; Win32 (S3_1b_w32 DLL + runner) 205 / 205, 0 differ
  from segs_win32.tsv (1b_segs_x64.tsv / 1b_segs_win32.tsv). The rasteriser is reached only through sg_raster_* / sg_gl_render_to_raster;
  no simulation path calls it, so the boot gate was not re-run (the change cannot reach it).

## 3. Controls

- Rasteriser controls 1 .. 8 (`#tune 11`): still break the criterion on c7 (test_negative_control_breaks_the_criterion, 8 / 8) and at
  zoom 2 on t3 (test_criterion_rejects_the_controls_at_zoom2, 1-5, 7, 8 and the old setup model tune 26). With the noise floor now
  ten times lower, each still breaks it: RED 8 / 8.
- New, for the two rules: `exact pow` (tune 27 = 1) on g6/i012 and `tie rule -x` (tune 10 = 2) on g6/i005 each turn a passing pass
  into a failing one (test_u1300_rule_is_needed_on_g6, RED 2 / 2); tune 27 = 1 gives more one-level pixels than the table on every
  c7 pass (test_specular_table_beats_the_exact_pow, 34 / 34); the probes: pow differs from the real GL on the lit probe while the
  table equals it in every pixel (test_probe_lit_untextured_quads_are_exact), and the new tie rule has the fewest coverage
  differences of the six (test_probe_tie_rule). Blind: none of the new controls is equivalent on its data (each changes the verdict).
- S3.1's draw-list controls 9 .. 14 and their pixel check pass unchanged inside test_terrain_render.

## 4. Commands

```
~/bin/py -3.12 log/roadmap/S3/1b_mirror.py --w32           # source copy port/build/S3_1b_src + builds S3_1b, S3_1b_w32 in the mirror
cp re/tools/glexact/glreplay.exe port/build/S3_head/re/tools/glexact/   # the exe is untracked, so not in the archive
cd port/build/S3_head && export SG_PORTSIM_DLL=$PWD/port/build/S3_1b/Release/portsim_c.dll
PYTHONPATH=. ~/bin/py -3.12 ../../../log/roadmap/S3/1b_probe.py g6/i012_raised_strip g6/i005_raised_pathupd g5/i012_raised_strip
PYTHONPATH=. ~/bin/py -3.12 -m port.test.glprobe untex | tie
PYTHONPATH=. ~/bin/py -3.12 ../../../log/roadmap/S3/1b_litfit.py spew 5      # one light term per config: terrain|amb|dif|difw|spe|spew
PYTHONPATH=. ~/bin/py -3.12 ../../../log/roadmap/S3/1_g2.py > ../../../log/roadmap/S3/1b_g2_all.log
PYTHONPATH=. ~/bin/py -3.12 ../../../log/roadmap/S3/1b_g2_stream.py > ../../../log/roadmap/S3/1b_g2_stream.log
PYTHONPATH=. ~/bin/py -3.12 ../../../log/roadmap/S3/1b_t3.py
~/bin/py -3.12 -m pytest port/test/test_gl_raster.py port/test/test_terrain_state.py port/test/test_terrain_render.py -q   # one file per process
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --dll $SG_PORTSIM_DLL --jobs 8 --out ../../../log/roadmap/S3/1b_segs_x64.tsv
~/bin/py -3.12 -m port.test.integrator compare ../../../log/roadmap/merges/S6-1/segs_x64.tsv ../../../log/roadmap/S3/1b_segs_x64.tsv
# (Win32: --backend win32 --dll32 port/build/S3_1b_w32/Release/portsim_c.dll --runner32 port/build/S3_1b_w32/Release/portsim_runner32.exe)
cd ../../.. && ~/bin/py -3.12 log/roadmap/S3/1b_compare_g2.py log/roadmap/S3/1_g2_all.log log/roadmap/S3/1b_g2_all.log
~/bin/py -3.12 log/roadmap/S3/1b_compare_stream.py log/roadmap/S3/1b_g2_stream_before.log log/roadmap/S3/1b_g2_stream.log
```

## 5. Files and open items

Files: log/roadmap/S3/1b_files.txt. Merge (S3.1's test file, UNCERTAINTIES.md, the plan's section 6 item 19): 1b_merge.md.
The probes write their scenes under port/corpus/gl/probe/ (gitignored corpus, as glprobe.py always did).

- U-1390: specular table measured with a directional light and an infinite viewer only (the terrain's setup); float32 vs double not
  determined.
- U-1391: centres exactly on a vertex or a diagonal edge (48 in the tie probe; what remains of the big pixels, at most 13 per pass).
- U-1392: the remaining 0.0 .. 0.21 % one-level noise is not split further; section 18's per-triangle residual (U-1076) needs
  re-measuring with the table.
- Not run: the Win32 backend of test_gl_raster (the sg_raster_* entry points are driven through the x64 ctypes harness only, U-1100);
  the boot gate (not reachable from the change, see section 2).
