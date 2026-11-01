# S3.24 notes (wave 8, W8.4a): the overlay line's footprint rule (U-1370's residue)

Worker: claude2 (Opus), 2026-10-30/31. Lead: cmvu18cc81fcpp715vy6fd2dv. Tree: `w7_assemble.py --dst S3_24 --commit f2dff003 --plain`
(port/build/S3_24; plain copies of the three owned files in port/build/S3_24_base_src/). No game run. U-1826..U-1829
(24_uncertain.tsv). Edits: 24_edits.py (17 anchored hunks in terrain_overlay.cpp / .h and test_terrain_overlay.py, made by
24_make_edits.py), applied by `24_assemble.py [--dst S3_24]` or `--root <tree> --only-24`; checked: a fresh f2dff003 tree plus the
edits equals the mirror byte for byte (port/build/S3_24_chk). No shared file changed.

## 1. The rule (Windows' generic GL, measured with re/tools/glexact/glreplay.exe)

Per column of the major axis (x when |dx| >= |dy| of the clipped, snapped ends) the real GL draws at most

    N = floor(w * L / |major|) + 2

pixels (w = the line width clamped to 10, L the length): the pixel centres whose minor offset o from the line centre (at the
pixel centre's major coordinate) times the tie sign lies in the half-open window [-N/2, N/2). The tie sign is
sign(dx) * sign(dy) for an x-major line and sign(dx) for a y-major line (line rule 2, the new default). S3.8's coverage formula
gives the values inside the window, unchanged. U-1370's `footprint-edge pixels under 0.13 coverage` are the pixels of S3.8's
support outside this window.

Evidence:
- Span (24_span.py over the 16-phase sets): on every (width, angle) row where S3.8's support reaches past N/2, the largest |o|
  drawn and the smallest |o| not drawn meet at N/2 (cuts at 2.0, 2.5, 3.5, 4.0, 6.0, 6.5, 7.0, 7.5); on every other row the
  support stays inside N/2.
- Ties (a centre exactly at +-N/2, integer ends): every 45-degree tie of the probes follows the sign rule in all four directions;
  the dedicated tie set (900 lines with slopes p/q, q <= 8, 132 tie pixels, 24_tiefit.py) follows `x-major sign(dx dy), y-major
  -sign(dx dy)` (132 / 132), the census rows follow rule 2 (see 3). The two differ only for y-major lines with dy > 0; neither
  decides all ties of the random sets (8 of 193 left, U-1827). Rule 7 is that probe-best variant, kept as a development switch.

## 2. Probes (C++ body; real GL frames)

| set | S3.8 model (rule 0) | rule 2 (default) | newly wrong |
|---|---|---|---|
| S3.8 rand | 108 | 7 | 0 |
| S3.8 widths | 88 | 12 | 0 |
| S3.8 short | 1 | 0 | 0 |
| S3.8 flip | 18 | 5 | 0 |

(test_terrain_overlay.py pins these; the four `fewer and none new` tests pass on x64.) Larger Python-model sets (rule 2's tie side
as `b8s1`, which skips the viewport clip; equal to the C++ on in-viewport lines): syn_white 152 -> 44 (4 newly wrong), syn_game
144 -> 32 (1 newly wrong), ph_fan 735 -> 144, ph_rand 1,846 -> 377, ph_short 610 -> 230. The newly wrong pixels are the ties of
U-1827 and two pixels 0.0007 / 0.0009 beyond N/2 that the real GL draws. Rule 6 (no tie decision, |o| <= N/2 + 0.002) has 0 newly
wrong pixels on every set (syn_white 49, syn_game 56, ph_fan 344, ph_rand 845, ph_short 414) but keeps the ties the census needs
cut (cov_a_golfer / cov_a_champ / cov_a_keys stay FAIL under it).

## 3. The 31 census rows (group 1 of 22_causes.tsv), framegate with the host GL terrain (24_rows.py; 24_rows_result.tsv)

f2dff003 (S3.22's measurement): 31 FAIL, 1..52 px each. Rule 6: 6 PASS. Rule 2: **29 PASS**, every row equal or better:

- PASS (29): championship tour_0180; cov_a_build terrain_03 / _07 / _11; cov_a_champ champ_render, n_prompt, n_cancel, X;
  all 11 cov_a_golfer rows; cov_a_keys k_render_dot, k_render_slash; cov_a_keys3 panel; cov_a_play shot_keys; cov_b_decor terrain;
  cov_r_bio panel, exit; cov_r_tutorial p18; ui_tour practice; ui_tour_v practice.
- FAIL (2): cov_a_build brush_465, 3 px one level, the end-cap drop at a line's second vertex (U-1826); cov_a_keys k_dlg_R, 1 px
  one level on a hole line, neither tie, cut nor cap (value arithmetic, U-1829).

Attribution (24_rowclass.py, frames with the overlay off = control 10): every remaining pixel of batch A was a line pixel; the
pixels rule 6 left were exact ties (cov_a_champ's 11: the 45-degree black path line at o = +2 = N/2).

## 4. Gates

All on port/build/S3_24 (f2dff003 + 24_edits.py), x64 Release unless noted; logs 24_g_<step>.log, one heavy-lock hold each.

- Rows, host GL, default build (24_g_rows_default.log): 29 of 31 PASS (section 3), equal to the rule-2 run before the default
  changed (24_rows_r2.log). Control (the old rule back): rule 0 = f2dff003's px (S3.22's measurement on the same base, 24_rows.tsv);
  control 11 RED in the unit tests.
- Full default census, GlRaster terrain (5 batches of 14 scenarios, 24_out/census_r2_b*.tsv; 24_census_cmp.py ->
  24_census_cmp.tsv): 698 rows PASS 111 / FAIL 577 / UNSUPPORTED 0 / NOT_RUN 10, equal in status to f2dff003's census
  (20_out/S3_20/census.tsv) on every row: 0 PASS lost, 0 rows with more px, 29 rows with fewer px (71,871 -> 71,517). The 29 are
  exactly the census rows of section 3 (all FAIL under GlRaster: their terrain residue, U-1392, stays; e.g. cov_a_keys
  k_render_dot 103 -> 57, cov_b_decor terrain 118 -> 73).
- Recorded game lines (24_streams.py): 9,534 distinct drawLine calls of the 12 scenarios' runs (SG_OVERLAY_DUMP, each with its own
  background) through the real GL: rule 0 879 differing px (103 beyond one level) in 316 calls, rule 2 104 (1 beyond) in 99 calls,
  rule 6 473; newly wrong against rule 0: 0 for both (24_streams_score.txt).
- Unit tests test_terrain_overlay.py: x64 33 passed / 6 skipped (the opt-in frame tests), Win32 33 passed / 6 skipped, same pins:
  x64 = Win32 (24_g_pytest_w32.log; 24_g_pytest_x64.log is the first run, before PINNED_SPAN rand was corrected to the C++ count 7
  and control 5's criterion was changed).
- Segments x64 205 / 205 PASS, status / frames / digest equal to S3.22's run on the same base in all 205 (24_segs_x64.tsv). Boot x64
  66 PASS / 2 KNOWN / 2 NO_FRAMES, every row equal to S3.22's (24_boot_x64.tsv). Both are unchanged by construction: the overlay
  only draws into an attached graphics backend and writes no GameState byte (segments attach only the text provider).
- Gaps closed after the lead's review (2026-10-31, each step one `w8_lock.py run S3.24`, chain 24_chain2.sh, logs 24_g_<step>.log):
  port test_static 6 / 6 passed; segments Win32 205 / 205 PASS (24_segs_w32.tsv), status / frames / digest equal to this tree's x64
  run and to S3.20's Win32 table (20_out/S3_20/segs_win32.tsv) in all 205 (S3.22 ran no Win32 segments); boot Win32 66 PASS / 2 KNOWN /
  2 NO_FRAMES (24_boot_w32.tsv), every row equal to this tree's x64 and to S3.20's Win32 table; opt-in SG_S38_FRAMES=1
  frame tests x64 6 passed (the four frame line-pixel G2 tests and frame controls 6 / 7 RED); the 31 rows on Win32 with host GL
  (24_g_rows_w32.log, frames 24_data/frames_d2w): 29 PASS, and all 31 port frames equal the x64 frames (frames_d2) pixel for pixel.
- Not run: the Win32 default census (the x64 census and the Win32 rows / probes / segments / boot cover it).
- Controls: 11 (S3.8's footprint back) RED on the probe pins (it reproduces S3.8's counts exactly; on the census rows the old rule is f2dff003's px, not re-run with control 11); 1, 2, 3, 4, 8 RED
  on G2 as before; 5 (no snap) RED by distance (1,085 differing px against the default's 7; it is 0.04 % inside G2 now); 9 RED.
  Blind: none new.

## 5. What is open

- U-1826 end caps: at the second vertex only, wide lines (w >= 3) lose the outermost pixel next to the cap in some columns although
  it is inside S3.8's support (t - L 0.31 .. 0.49): 20 of 1,016 visible past-cap pixels of the white set. In the canonical frame
  (major and minor increasing, origin b) at (u, v) = (-0.5, +1.5) the pixel is dropped for slopes 0.615 .. 0.65 (w 3, 5, 10 alike)
  and drawn at 0.605 and below and at 0.667 .. 0.72 (24_capcol.py). No shifted-sample cap test (24_capscan.py) and no count from
  the window start (rule ecNN in 24_probe.py) separates them.
- U-1827 ties of y-major lines with dy > 0 and a few non-45-degree ties (fixed-bit slope rounding and float32 evaluation of the
  centre do not explain them: 24_tiefit.py, 24_tieprec.py).
- U-1371 values: where both draw, 34 of 490,113 channel values differ, all within 0.01 .. 0.04 of a quantisation step (24_arith.py).
- U-1828: the dump (SG_OVERLAY_DUMP) spans every frame of a run, so a dumped call is not tied to its checkpoint frame.

## 6. Port change (terrain_overlay.cpp / .h)

- `line_rule()`: default 2; `overlay.setRule(rule)` (eax = the previous rule) and SG_OVERLAY_RULE (read once) select 0 = S3.8's
  model, 1 / 3 / 4 / 5 / 7 = other fixed tie signs, 6 = no tie decision. Negative control 11 = rule 0 (S3.8's footprint).
- SG_OVERLAY_DUMP=<file> (development): each drawn call appends its arguments, the GL width used, flip, extent, box and the box's
  pixels before / after (read by 24_probe.py).
- test_terrain_overlay.py: the default's pinned counts (PINNED_SPAN), S3.8's counts kept for rule 0 and control 11, `fewer and
  none new` for rules 2 and 6, control 11 RED, control 5 judged by its distance from the default (it now sits 0.04 % inside G2).

## 7. Commands

```
~/bin/py -3.12 log/roadmap/S3/24_assemble.py --dst S3_24                      # tree + edits
~/bin/py -3.12 log/roadmap/S3/24_build.py x64|Win32                           # heavy (lock)
bash log/roadmap/S3/24_turn.sh "<what>" <command>                             # one lock hold (oldest wait file first)
bash log/roadmap/S3/24_gates.sh pytest_x64|pytest_w32|rows_default|census_<0..4>|segs_x64|boot_x64|streams
~/bin/py -3.12 log/roadmap/S3/24_census_cmp.py
~/bin/py -3.12 log/roadmap/S3/24_probe.py synth game|white|ties <out.npz>      # glreplay (lock); cmp <npz> --rule <r>
~/bin/py -3.12 log/roadmap/S3/24_phase.py run fan|rand|short <out.npz>         # 16-phase sets
cd port/build/S3_24 && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/24_rows.py --tag <t> [--rule r] [--negctl m] [--raster]
```
Data in log/roadmap/S3/24_data/ (npz probe sets, census frames frames_<tag>/, dumps dump_<scenario>.bin: 0.5 GB, delete freely).
