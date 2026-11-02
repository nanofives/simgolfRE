# S3.29 brief (wave 9, candidate 1): U-1309, GlRaster's textured interpolation

Read log/roadmap/S3/W9_COMMON.md first (base tree, lock, delivery, census, report). Then log/roadmap/S3/21_notes.md (whole: part 1's
two exact rules and part 2's stepping model, sections 9-10), 21_uncertain.tsv (U-1305..U-1309), 21_merge.md, WAVE8.md section 4, 15_notes.md
(the host-GL renderer and the residue it removes), 20_notes.md (self-check), 12_notes.md (S3.10's fast path and its boundary checks).

Tree: port/build/S3_29 (w8_assemble.py --dst S3_29; it holds S3.21 part 1 ON and 21b's `#tune 30` stepping OFF). Step id for the lock:
S3.29. U ids U-1965..U-1969. Time box 10 h. You own port/src/sim/gl_raster.cpp / .h and port/test/test_gl_raster.py this wave (S3.28
does not touch them).

## Task

The residue left after S3.21 (most census FAIL rows hold 1..19 px) is in the textured (GL_MODULATE) interpolation, which part 2's
untextured colour stepping did not describe. Measure it the way S3.21 measured the vertex rule: probes replayed through Windows' generic
GL (re/tools/glexact/glreplay.exe, the 21_glfeed feedback method), first a slow ramp of one texture coordinate over a smooth texture and a
slow colour ramp over a constant texture, then whatever they point to (texel addressing, coordinate stepping, perspective, the modulate
product's rounding, the dither threshold). Find the rule, put it in BOTH pixel loops (the general loop and S3.10's fast loop with its
boundary checks) behind a `#tune` knob.

## Gate

- Probes: the rule's differing px against the real GL on each probe (state the numbers before / after).
- The 271 stream passes and 237 draw-list chains within G2, with the one-level totals before / after (S3.21's 2,296 / 1,605).
- S3.10 bit-identity: fast / exact / 4-worker loops against the general loop, 0 differ (21b_equiv-style), worker controls RED.
- Full census (raw + split) against w8_out/census_split.tsv: 0 PASS lost and 0 rows with more px in raw and split; raw PASS count and
  differing px before / after.
- Segments 205 / 205 and boot 66 / 2 / 2 on x64 and Win32 with 0 digests / rows differing.
- A control that turns the rule off reads RED.
- DEFAULT: the rule goes ON by default ONLY if it is exact on every probe and the stream / chain passes (no newly wrong pixel anywhere).
  Otherwise it stays behind the switch, default off, and the step is a finding with the numbers. Stop at the box (10 h) either way.
