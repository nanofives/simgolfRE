# S3 wave 8 plan (lead cmvu18cc, 2026-10-30; plan only, no worker spawned)

Base: f2dff003 (wave 7 merged). Census on it: GlRaster (default) 698 rows PASS 111 / FAIL 577 / UNSUPPORTED 0 / NOT_RUN 10;
`--gl auto` (host GL) PASS 638 / FAIL 50 / 0 / 10 (log/roadmap/S3/20_out/S3_20/census_auto.tsv).

## 1. Is S3's gate met with GlRaster as the default?

Gate (re/roadmap/sections.tsv, S3): every census checkpoint with terrain on screen has the 2D layers at 0 px and the terrain
region within G2.

- Terrain half: MET. All 527 terrain checkpoints are within G2 (S3.2's verdicts; GlRaster unchanged since, 299 passes bit-identical).
- 2D half: NOT MET, in two different senses.
  1. Measure: 527 of the 577 GlRaster FAIL rows fail only on terrain pixels: one-level GlRaster residue (U-1392) showing through or
     next to HUD regions. They become PASS when the terrain is exact (host GL), with no 2D change. framegate counts them as region px;
     it does not tell terrain-origin pixels from 2D-layer pixels. S3.15's split (15_split_rows.tsv) shows every repeated HUD pattern is
     100 % terrain. Read literally, the gate judges these under G2 (terrain), and they meet it; the census does not say so.
  2. Real 2D residue: the 50 rows that still FAIL with host GL (terrain exact) hold non-terrain px. 38 of them have 1..65 px
     (mostly one-level, at sprite / blend edges over terrain); 12 have more: cov_b_holes18 holes 2,829, cov_r_s2_screens hire 1,397
     (the four animated employees, known since S2.8), cov_k3_camera y_inj3 847 and cov_k3_camera2 y_inj4 414 (fountain / splash
     sprites), golfer_stories season_end 685 (a translucent half-ellipse the port does not draw), cov_b_inject / inject2 3 + 3 rows of
     15 px beyond one level, cov_r_s2_screens course / radial_hover 65 and the five report rows 20 px beyond.
- My view: S3 is not closed. The terrain criterion is met; the census needs a terrain-origin rule (a gate-measure decision) to show
  it; and about 50 rows of 2D residue remain, of which about 12 are visible defects worth fixing and the rest one-level blend edges.

## 2. Steps

| step | what | expected census movement (default GlRaster) | cost | game run | decision |
|---|---|---|---|---|---|
| W8.1 | Census terrain-origin rule: framegate classifies each differing px as terrain-origin (equal with the host-GL terrain, or equal to the original's exit.bin root where recorded) and judges those under G2; prints both verdicts (`raw` and `split`) | split verdict: FAIL 577 -> about 50 (527 rows to PASS); raw verdict unchanged | 3-4 h | no | YES: gate-measure change (Mariano / orchestrator) |
| W8.2 | U-1392 time-boxed hunt (4-8 h, stop if no rule found): S3.16's lead is new evidence: the residue sits in vertical COLUMN bands (x 0..15, 656..687, 784..799) over whole frames, i.e. an x-only term of GlRaster's attribute setup (viewport scale / fixed-point staircase), not per-triangle noise. Bisect with glreplay.exe on c7 passes | if found: GlRaster default reaches the host-GL numbers (PASS about 638) with no gate change; if not: none | 4-8 h | no | no |
| W8.3 | 2D residue classification: images of the 50 host-GL FAIL rows (15_img method) and a cause per row: body missing / blend edge (U-1347 class) / snapshot timing / terrain pass after the recorded exit | none (report) | 2-3 h | no | no |
| W8.4 | 2D residue fixes from W8.3, by cause: (a) golfer_stories season_end half-ellipse (overlay highlight; S3.8 line / ring family); (b) camera y_inj3 / y_inj4 and the water rows (fountain, splash sprites at the screen edge: scene family, port/corpus/scene has the original's root); (c) cov_b_holes18 holes 2,829 (cause first); (d) cov_r_s2_screens hire animated employees (addSprite FLIC cells inside the modal dialog); (e) the 20-px report rows and course / radial 65 px | about 12-20 rows to PASS (the visible ones); the 1..23 px blend-edge rows stay unless W8.3 finds one cause | 6-10 h (2 workers) | (d) may need a recording of the hire dialog's sprite calls; others no (corpus/scene + exit.bin exist) | no |
| W8.5 | showRoutingMapScreen.walkers (mode 0: walker list and markers; no body). No census row reaches it: a new pinned scenario opening the routing map in mode 0, its trace / segment, then the body with an emulator call oracle | 0 on existing rows; + the new rows | 5-7 h | YES (scenario pin: det_a == det_b == det_c on an instance) | no |
| W8.6 | Remove the unused body showCharacterEditor.bio (gfx_chareditor.cpp) after S3.19's EditBox family replaced it; gfx.check 138 -> 137 bodies, 0 duplicates | 0 | 0.5 h (fold into W8.1 or W8.4) | no | no |
| W8.7 | U-1810: run the app's `--gl auto` self-check on the second PC (MARIANO-PC) and record pass / fallback and its reason | 0 (default unchanged) | 1 h | no game; needs the other machine | no |

U-1818 (host GL slower per pass) and U-1819 (opengl32 hang with several contexts): G13 is final (GlRaster default, no banded
work), so both stay open as documentation with S3.20's measurements; no step.

## 3. Recommended order

W8.1 (ask the decision now) in parallel with W8.2 and W8.3; then W8.4 by W8.3's causes; W8.6 folded in; W8.5 only if Mariano wants
the routing-map walker drawing covered (it needs a game slot); W8.7 whenever the second PC is on. Without W8.1 and W8.2 the default
census stays at about 111 PASS whatever the 2D steps do, because the terrain residue dominates the HUD regions.

## 4. Wave 9 candidates (orchestrator, 2026-10-31: decided after wave 8 is merged and the split census is in)

1. **U-1309, GlRaster's textured interpolation** (deferred from W8.2; S3.21 notes section 10, 21_uncertain.tsv).
   - Where it stands: S3.21 part 1 made vertex arithmetic and clipping exact (census 111 -> 122 PASS, differing px 71,871 -> 11,972,
     0 rows worse). Part 2's 16.16 colour stepping is exact on untextured triangles (ramp 705 -> 0 px, Gouraud 481 -> 90) and changes
     nothing on the terrain, which is textured (GL_MODULATE): every textured variant tried was equal or worse.
   - Probe first: a slow ramp of one texture coordinate over a smooth texture, and a slow colour ramp over a constant texture,
     read with S3.21's feedback / glreplay method; then the rule in both pixel loops (general and S3.10's fast loop) behind a
     `#tune`, default on only if exact (the S3.21 rule).
   - Expected value: if exact, GlRaster (the default renderer) reaches the host-GL census numbers (about 638 PASS of 698 against
     122) without a gate change; 529 of today's 566 FAIL rows hold only 1-19 terrain px. If W8.1's split verdict is adopted as the
     gate, the gate value is smaller (the split already judges terrain under G2) and the value is exactness of the default renderer
     and the raw census. If not exact: a narrower finding (U-1309 split by stage), default unchanged.
   - Cost: probes 2-3 h, implementation 3-5 h, gates about 3 h of heavy runs (census 32 min, terrain verdicts 31 min, stream /
     chain passes, segments and boot x64 + Win32 about 35 min, S3.10 equivalence); 8-10 h box, one opus worker, no game run,
     no decision needed unless the rule is inexact.
2. **lowerEdgeCorner's draw pass (U-1952 settled by S3.27, U-1955..U-1957).** Port renderTileAtFiveOffsets (0x10038900) and
   enableBlackLight1 (0x10003830) as terrain pass kind 5 handed to the backend from LiveTerrain::lower_edge_corner before the state call
   (27_notes.md section 3 has the files, rule, oracle and control). Expected: the eight cov_r_s2_screens split FAIL rows to PASS under the
   split verdict; other rows of the 50 recordings with lowerEdgeCorner may move (U-1955). Cost 4-6 h, one opus worker, no game run
   (S3.27's recordings are on disk; the angle argument must be added to the recorder's meta first, U-1956).
3. W8.5 (routing-map walkers, needs a game slot) and W8.7 (U-1810 on MARIANO-PC) if not done in wave 8.
