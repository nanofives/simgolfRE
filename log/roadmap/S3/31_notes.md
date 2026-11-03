# S3.31 notes (wave 10): the overlay line residue (U-1826 cap drop, U-1829 k_dlg_R value)

Worker: claude2 (Opus), 2026-11-01. Lead: cmvy4ggg53y79p71hei2lzzug. Tree: `w10_base.py --dst S3_31` (port/build/S3_31, verified
0 differ from port/build/I22 before editing). No game run. Uncertainty rows U-1977..U-1982 (31_uncertain.tsv).

## Summary

- **U-1826 is found and implemented** as line rule 9, the new default. The rule: the second-vertex holes (section 2).
  - cov_a_build brush_465's 3 px are this rule's case.
  - On the real-GL probe sets and the 9,534 dumped game calls it gets 0 px newly wrong. The game calls go from 104 px (S3.24's
    rule 2) to 43.
- **Rule 9 also contains the generic GL's float32 vertex stage for the line ends** (section 1, also selectable alone as rule 8).
  It is an exact stage the overlay lacked. Measured: GL_FEEDBACK positions of every integer end equal.
- **U-1829 (k_dlg_R, 1 px) is a measured finding, not a rule** (section 3). The real side coverage of that pixel is
  0.9954962 .. 0.9954963 against the model's 0.9954954; the arithmetic behind it is not determined.
- **Switches.** overlay.setRule / SG_OVERLAY_RULE:
  - 2 = S3.24's model (old rule restorable);
  - 8 = rule 2 + the float32 vertex stage;
  - 9 = rule 8 + the holes (default).
- **Controls.** 12 = the double snap of the ends under rules 8 / 9; 13 = no holes.
- **Gates** (section 4): census raw unchanged and split 686 / 2 / 10 (I22 685 / 3 / 10), 0 PASS lost, 0 rows worse; segments 205 / 205
  and boot 66 / 2 / 2 on x64 and Win32 with 0 rows differing from I22; unit tests 56 passed / 6 skipped on both backends.

## 1. The float32 vertex stage (line rules 8 and 9; terrain_overlay.cpp float_ends)

S3.24's model snapped each end with U-1076's double formula c + (v - c)(1 - 2^-8/h), rounded half up, after a window-space clip.
S3.21 measured the real GL's vertex stage for triangles. Applied to drawLine's glVertex2i ends:
- glOrtho's elements are computed in double and stored as float; the MVP is the projection.
- clip = m12 + v * m0 is computed in float.
- The segment is clipped in clip space in float against x >= -1, x <= 1, y >= -1, y <= 1 in that order.
- t = d_in / (d_in - d_out); the new end = in + t (out - in), from the inside end.
- Window = float(ndc * float(h - 2^-8)) + h, rounded half to even to 2^-8.

Evidence:
- **GL_FEEDBACK** (31_feed.py over log/roadmap/S3/21_glfeed/glfeed.exe, GLFEED_POINTS=1). The real window coordinates of every
  integer x in 0..800 and y in 0..600, under both projections, equal the float32 rule (0 differ). The double snap differs at 3 per
  projection (x 200, x 599, y 150 with the flip byte set / 450 with it clear).
- **Pixels against the real GL** (31_cppcheck.py, the C++ body; log 31_cppcheck.log). Rule 8 against rule 2: 0 newly wrong on every
  set, 773 px fixed (the 9,534 dumped game calls 104 -> 91; ph_rand 377 -> 271; len 2,254 -> 2,007).
- **The clip matters.** The float32 vertex rule with S3.24's window-space clip made 17 px newly wrong, all on lines with an end off
  screen (31_work/a10_newbad.py). The clip-space float clip removes them.
- **Model agreement.** The C++ rule 8 equals the Python model (31_score.py f32c:span_dir3) pixel for pixel on every record.

## 2. U-1826 solved: the second-vertex holes (line rule 9; terrain_overlay.cpp second_vertex_hole)

**The rule.** Take the segment's canonical frame:
- u runs along the major axis and v along the minor one, both increasing from the first end a to the second end b, origin b;
- s = |minor / major|;
- o = v - u s is the pixel centre's minor offset from the line centre;
- N = floor(w L / |major|) + 2 is the span count;
- q = u' + v' s, measured from b' = a + round(b - a) per axis.

The real GL generates no fragment where both of these hold:

    0 < frac(o + N/2) < 1 - s        and        q > 1 - sqrt(1 + s^2) / 2

What it means:
- **First condition.** The pixel's span cell is the one its row held in the previous column, that is, its strand took a
  horizontal step.
- **Second condition.** The pixel lies beyond the end cap q = sqrt(1 + s^2) / 2 mirrored about the perpendicular through
  b + (0.5, 0).
- **Ends used.** a and b are the ends before the clip.
- **First end.** It has no holes.

**How it was found.**
- **Integer-end sweeps** (31_cap.py `len`, `rev`, `sweep`, `sweep8`; 31_work/a14, a16, a19):
  - the drops depend on slope and position only (any length, every octant, reversed lines);
  - they lie within 0.08 px of the perpendicular through b + (0.5, 0), in point-symmetric pairs about that point;
  - each position drops over one slope band.
- **Dense sub-pixel maps** (31_shape.py: one direction and width, 256 copies shifted by 1/16 px, glVertex3f ends) show the hole
  around points spaced 1/sqrt(1 + s^2) along that perpendicular:
  - the kept pixels next to it carry exactly the model's value, so it is a footprint gap, not a coverage change;
  - its q edge is flat at 1 - sqrt(1 + s^2)/2 to +-0.0002 for every f (slopes 0.28, 0.611, 0.625, 0.833, 0.85; widths 3, 5, 10);
  - its f extent is (0, 1 - s) (half-widths 0.1875, 0.083, 0.075 = (1 - s)/2; 31_work/a22, a23, a26).
- **b' rather than b.** Groups of copies of one line differ only in which end the 2^-8 snap moved. The real GL keeps the pixels a q
  from b would leave out when b moved but a did not, and drops them when both moved (ph_rand (7, 25) w 10; sweep8
  (687, 445)-(651, 467)).
- **Unclipped ends.** Measuring from the clipped ends made 6 unit-scene pixels newly wrong, all on clipped lines. A clipped second
  end leaves no hole on screen; a clipped first end moves neither b nor b'.
- **The integer-end band edges follow.** (-0.5, +1.5) drops for s in (0.6098, 2/3): the q edge gives the lower bound, f = 1 - s
  the upper.

**Exact ties** (31_cap.py `tie8`, `tie8b`, 31_work/a27_ties.py, a28_f0.py).
- At f = 0 or f = 1 - s exactly (integer ends inside the region where the window rule leaves them exact), the real GL keeps one
  of the two in every octant.
- For slopes 2/3, 4/5 and the tie8 slopes:
  - x-major lines going +x and y-major lines going -y keep f = 1 - s and drop f = 0;
  - the other four octants do the reverse.
- At 6/7 the sides flip, and widths 9 and 10 differ. So the real GL's own rounding decides exact ties (U-1982).
- Rule 9 keeps every exact tie (kHoleEps 1e-9). It can only miss a hole there, never add one. The reversed set's 100 remaining
  px are such ties (s = 2/3, lines going -x).

**Results** (Python model 31_holes.py, log 31_work/holes_unclipped.log; the C++ equals it pixel for pixel, 31_cppcheck9.log):

| set | rule 8 px | rule 9 px | newly wrong | fixed |
|---|---|---|---|---|
| syn_white | 35 | 6 | 0 | 29 |
| syn_game | 26 | 3 | 0 | 23 |
| syn_ties | 110 | 21 | 0 | 89 |
| ph_fan | 130 | 25 | 0 | 105 |
| ph_rand | 271 | 54 | 0 | 217 |
| ph_short | 216 | 24 | 0 | 192 |
| sweep | 336 | 69 | 0 | 267 |
| sweep8 | 210 | 43 | 0 | 167 |
| len | 2,007 | 58 | 0 | 1,949 |
| rev | 2,007 | 166 | 0 | 1,841 |
| **9,534 dumped game calls** | 91 | 43 | 0 | 48 |

**cov_a_build brush_465.** The real-GL replays of its dumped calls 6630, 6631 and 6633 drop (u, v) = (-0.5, +1.5) at slopes
19/31 and 20/32 (31_work/a13_brush.py), which rule 9 reproduces. The census row: section 4.

## 3. U-1829, the k_dlg_R value: measured, not solved

- **The call.** Record 2397 of 24_data/dump_cov_a_keys.bin is the white w2 hole line (424, 384)-(466, 399), alpha 1, over pre
  (4, 12, 4) at (460, 396). Replayed alone through the real GL it gives 7fff, the reference value; the model gives 7bfe
  (31_work/a3_kdlg.py). It is the line's own arithmetic, not the background.
- **Translation-invariant.** At 288 translations by multiples of 4 px the real GL gives 7fff every time (a12_ktrans.py).
- **The model's margin.** The model's coverage there is 0.9954954, 1.6e-6 below the coverage that gives 31.
- **Measuring real coverage.** 31_alpha.py `focus` bisects the alpha at which a pixel and a coverage-1 reference pixel of the same
  dither phase reach one 5-bit level on black (the value is linear in alpha * coverage, so c = a_ref / a_pix, to about 1e-7).
  The first version bisected 30 -> 31 over red 10, a level a pixel under coverage 0.994 never reaches below alpha 1: that gave the
  degenerate brackets of U-1980.
- **Findings on this line (42, 15) w 2** (31_data/nline_42_15.json, 31_work/a31_nline.py, a32_dir.py; 19 pixels with coverage
  0.39 .. 0.996):
  - the real side distance is the model's rotated by theta = +2.46e-8 rad about the first end, plus 3.2e-8 px;
  - this fits every pixel to within 1.7e-7;
  - at (36, 12) (k_dlg_R's pixel) the real coverage is [0.9954962, 0.9954963], above the threshold, while the model's is below it.
- **What the rotation is not.** Float32 versions of the direction give -1.6e-8 (float32 ux, uy or the float32 slope) or +4.0e-9
  (division by, or multiplication with, a float32 length / reciprocal). Neither matches +2.46e-8.
- **Other lines.** Single pixels on (31, 19), (40, 7) and (25, 24) differ from the model by under 6e-7.
- **Two more full lines** (31_work/a33_nline2.py, nline_38_21.log, nline_45_8.log; 22 and 23 side pixels):
  - (38, 21) w 2: theta = +1.07e-8 rad, residual at most 5.4e-8;
  - (45, 8) w 2: theta = +1.73e-8 rad, residual at most 3.0e-7;
  - (42, 15), refitted on its 16 valid pixels: +2.64e-8.

  No float32 construction of the direction tried (float32 ux / uy, division by or multiplication with a float32 length or
  reciprocal, a float32 slope with 1 / sqrt(1 + s^2), the minor-axis offset with a float32 slope) gives all three. Each misses at
  least one line by 1.5e-8 or more.
- **Not settled.** The arithmetic behind the rotation is not determined (U-1978). k_dlg_R stays split FAIL view=1. Rules 8 / 9
  do not change that pixel (31_work/a29_rows.py).

## 4. Gates (port/build/S3_31; logs 31_g_<step>.log, census in 31_out/)

- **Unit tests.** port/test/test_terrain_overlay.py: x64 56 passed, 6 skipped (31_g_pytest_x64.log); Win32 56 passed, 6
  skipped (31_g_pytest_w32.log). The pins are the same on both backends, so x64 = Win32 on every pinned scene.
- **C++ against the real GL** (31_cppcheck9.log, the S3_31 x64 DLL over every capture). The C++ equals the Python model on
  every record (py9_differ 0), and rule 9 has 0 newly wrong px on every set:

  | set | records | rule 2 | rule 8 | rule 9 |
  |---|---|---|---|---|
  | syn_white | 600 | 44 | 35 | 6 |
  | syn_game | 600 | 32 | 26 | 3 |
  | syn_ties | 900 | 118 | 110 | 21 |
  | ph_fan | 2,480 | 144 | 130 | 25 |
  | ph_rand | 3,840 | 377 | 271 | 54 |
  | ph_short | 5,440 | 230 | 216 | 24 |
  | sweep | 2,133 | 410 | 336 | 69 |
  | sweep8 | 1,432 | 245 | 210 | 43 |
  | len | 4,464 | 2,254 | 2,007 | 58 |
  | rev | 4,464 | 2,254 | 2,007 | 166 |
  | streams (the 9,534 dumped game calls) | 9,534 | 104 | 91 | 43 |

  Of rev's 166, 100 are exact ties, which rule 9 leaves as rule 8 (U-1982).
- **Controls.**
  - 12 (double snap under rules 8 / 9) gives rule 2's counts on every set above (c12 = r2), so it reads RED against rule 8.
    It is blind on the unit scenes rand / widths / short / flip, which hold no end where the two snaps differ (U-1979). It
    reads RED on the scene `vertex` built for it.
  - 13 (no holes under rule 9) gives rule 8's counts (c13 = r8), RED on every set and on 3 or more unit scenes.
  - S3.24's control 5 stays RED (test_terrain_overlay.py, criterion: more than 50 times the default's px).
- **Census** (31_out/census_split.tsv.d, 70 scenarios, 698 rows, `31_census_cmp.py` against log/roadmap/merges/I22/census_split.tsv):
  - raw: 126 PASS / 562 FAIL / 10 NOT_RUN, equal to I22 row for row in status;
  - split: 686 PASS / 2 FAIL / 10 NOT_RUN (I22 685 / 3 / 10);
  - PASS lost 0, rows with more px 0;
  - one row with fewer px: cov_a_build brush_465 raw 11 -> 8 (all 8 terrain-origin, 2D 0), split FAIL -> PASS.
  - The 2 split FAIL rows left are cov_a_keys k_dlg_R (1 px, U-1978) and cov_k3_camera y_inj3 (847 px, U-1834, explained).
- **Segments** (31_out/segs_x64.tsv, segs_win32.tsv; `31_rowcmp.py` against I22's segs_x64.tsv / segs_win32.tsv on every column
  but the timing): 205 / 205 PASS on x64 and on Win32, 0 rows differ, digests included.
- **Boot** (31_out/boot_x64.tsv, boot_win32.tsv, `31_rowcmp.py` against I22's): I22's 70 rows are equal on x64 and on Win32, 66 PASS /
  2 KNOWN / 2 NO_FRAMES, 0 rows differ. Each run has one more row, golfer_events FAIL at record 2,244 (objects f_w_4c2e00,
  unk_005a6d40, g_messageTimer, g_freeUnlockMask, g_rng), equal on x64 and Win32. Its trace port/corpus/traces/golfer_events.trace was
  written at 2026-11-01 16:55 by another step, after I22's boot run (02:54), so I22 has no row for it (U-1102: port/corpus is
  shared). It is not this step's: boot.py attaches only the text provider (no gfx backend, no LiveTerrain), so terrain_overlay.cpp
  never runs in a boot run, and the overlay writes no GameState byte.
- **Not run:** the census's final `--all --resume` pass, which only writes 31_out/census_split.tsv from the cache. The comparison
  reads the 70 cached scenarios (31_out/census_split.tsv.d). The 10 NOT_RUN rows are cached without split columns, so they read `''`
  there instead of NOT_RUN. The opt-in frame tests (SG_S38_FRAMES=1) were not run.

## 5. Option for the orchestrator (not mine to decide)

The host-GL terrain path (S3.15) could draw the overlay lines through the real GL too. That would remove the overlay residue
from host-GL frames, but it changes what the split verdict calls terrain-origin. After rule 9 the residue on the 9,534 dumped
game calls is 43 px (1 beyond one level).

## 6. Commands

```
~/bin/py -3.12 log/roadmap/S3/31_assemble.py [--dst S3_31chk]   (w10_base tree + 31_edits.py); --root <tree> --only-31
~/bin/py -3.12 log/roadmap/S3/24_build.py x64|Win32 --tree S3_31                                   (w8_lock)
~/bin/py -3.12 log/roadmap/S3/31_cap.py run <len|rev|fine> <out> | run1 <sweep|sweep8|tie8|tie8b> <out>   (glreplay)
~/bin/py -3.12 log/roadmap/S3/31_shape.py run <dx> <dy> <w> <out>; map <out>                       (dense canonical map)
~/bin/py -3.12 log/roadmap/S3/31_holes.py <npz>... --variant lt_gt_1e-9                            (hole model vs real GL)
cd port/build/S3_31 && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 ../../../log/roadmap/S3/31_cppcheck.py <npz>...   (C++ vs real GL)
~/bin/py -3.12 log/roadmap/S3/31_feed.py --flip 1|0                                                (GL_FEEDBACK ends)
bash log/roadmap/S3/31_census_seq.sh > log/roadmap/S3/31_out/seq.log 2>&1     (census, Win32 build + tests, segments, boot; w8_lock holds)
~/bin/py -3.12 log/roadmap/S3/31_census_cmp.py                                                    (against I22)
```
Data: log/roadmap/S3/31_data/*.npz (local, real-GL captures), 31_work/ (analysis scripts and logs).
