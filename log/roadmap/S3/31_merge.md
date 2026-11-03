# S3.31 merge note

## Port tree

Apply after w10_base.py (I22's port tree):

    ~/bin/py -3.12 log/roadmap/S3/31_assemble.py --root <tree> --only-31

31_edits.py holds 17 anchored edits. They touch only S3.31's own files: port/src/sim/terrain_overlay.cpp,
port/include/portsim/sim/terrain_overlay.h and port/test/test_terrain_overlay.py. No shared file changes; modules.inc is
unchanged.

Check: a fresh `31_assemble.py --dst S3_31chk` equals port/build/S3_31 byte for byte (w7_assemble.py --verify: 417 / 417, 0 differ).

## UNCERTAINTIES.md

Append the six rows of log/roadmap/S3/31_uncertain.tsv (U-1977..U-1982).

These existing rows change status:
- U-1826 (S3.24, the second-vertex cap drop): resolved by line rule 9; its measured remainder is in U-1977 and U-1982.
- U-1829 (S3.24, the k_dlg_R value): stays open; its measurement is in U-1978.

## CLAUDE.md paragraph (after the S3.24 / S3.23 paragraphs)

Roadmap S3.31, the overlay line's second-vertex holes and float32 ends (2026-11-01, notes log/roadmap/S3/31_notes.md, edits
31_edits.py, U-1977..U-1982; U-1826 resolved):

- **Rules.** terrain_overlay.cpp's line rule 9 is the new default. overlay.setRule / SG_OVERLAY_RULE: 2 = S3.24, 8 = the float32
  ends alone. Controls: 12 = the double snap, 13 = no holes.
- **Float32 ends.** drawLine's ends go through the generic GL's float32 vertex stage:
  - clip = m12 + v * m0 in float;
  - the segment clipped in clip space in float;
  - window = float(ndc * float(h - 2^-8)) + h, rounded half to even to 2^-8.

  GL_FEEDBACK (31_feed.py over 21_glfeed) gives these positions for every integer x and y. The double snap differs at x 200 / 599,
  y 150 / 450 and at clipped ends.
- **Second-vertex holes.** In the canonical frame (u major, v minor, both increasing from a to b, origin b, s = |minor / major|,
  o = v - u s, N the span count) the real GL generates no fragment where 0 < frac(o + N/2) < 1 - s and
  u' + v' s > 1 - sqrt(1 + s^2) / 2.
  - (u', v') is measured from b' = a + round(b - a), all on the ends before the clip.
  - Exact ties are kept, and no holes are made when the clip moved the first end (U-1982).
  - Found with dense 1/16-px maps of the canonical plane (31_shape.py) and integer-end sweeps (31_cap.py).
- **Results against the real GL.**
  - The 9,534 dumped game calls go from 104 px (rule 2) to 43, with 0 newly wrong.
  - Every probe set has 0 newly wrong (len 2,254 -> 58).
  - cov_a_build brush_465's three calls are exact.
- **Not solved.** cov_a_keys k_dlg_R keeps its 1 px. The real side distance on that line is the model's rotated by +2.46e-8 rad about
  the first end, measured by alpha bisection (31_alpha.py), with the arithmetic not determined (U-1978).
- **Gates on port/build/S3_31.**
  - Census, 698 rows against I22: raw 126 / 562 / 10 unchanged; split 686 PASS / 2 FAIL (was 685 / 3); 0 PASS lost, 0 rows with more
    px; cov_a_build brush_465 split FAIL -> PASS (raw 11 -> 8 px, all terrain-origin).
  - Segments 205 / 205 and boot 66 / 2 / 2 on x64 and Win32, 0 rows differing from I22. test_terrain_overlay 56 / 6 skipped on both.
  - Controls 12 and 13 RED.
