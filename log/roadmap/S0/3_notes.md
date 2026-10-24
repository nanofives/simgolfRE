# S0.3 notes: the frame census at HEAD 4bfefc2a (64 pinned scenarios, 609 checkpoints)

Status: DONE. Worker session cmvmcwc022bwro115rvfltyac (claude2, sonnet), 2026-10-24.

## Deliverables
- re/roadmap/frame_census.tsv: 609 rows (one per checkpoint of the export's scenario_refs.json), framegate's columns plus
  `terrain_verdict`, `terrain_view_px_one_beyond` (px/one/beyond of the view region), `terrain_one_pct_of_area`, `twod_cause`, `primary_cause`.
  Header comment line (starts with `#`) states the G2 rule.
- re/roadmap/frame_causes.tsv: `ranking` (primary | 2d_all | 2d_terrain_no | 2d_terrain_yes), cause, checkpoints_blocked, scenarios,
  terrain_yes, terrain_no, example_checkpoint. Each ranking sums to the 564 non-PASS rows (primary, 2d_all) or its subset.

## Numbers
- 609 rows: PASS 45, UNSUPPORTED 525, NOT_RUN 39 (34 MISSING trace: cov_a_quit 15, cov_a_quit_title 9, golfer_events 10; 5 with no snapshot, U-1120).
- Kinds: ingame 522 (UNSUPPORTED 520, PASS 1, NOT_RUN 1), title 43 (PASS 39, NOT_RUN 4), location 5 (PASS), frame 5 (UNSUPPORTED), none 34 (no trace).
- terrain-yes rows 522: verdict PASS 1 (cov_a_keys/k_worldmap), FAIL 520, none 1 (championship/tournament_end, no snapshot).
- Primary ranking: terrain not drawn 520 (60 scenarios); NOT_RUN MISSING 34; Widget_writePixel on the 5 credits-frame rows (kind frame, terrain no) 5;
  no snapshot 5.
- 2D ranking (first unsupported DRAWING call of the 525 UNSUPPORTED rows): no body: Widget_writePixel 316 (35 scenarios), Surface_blit3 "a source or
  destination the backend does not model" 168 (19), Image_draw "a SpriteRef the backend has no sprite for" 39 (5), no body: Surface_drawScaled2 2
  (cov_b_locations/phoenix). There are NO "FAIL with no unsupported call" rows: every non-PASS 2D row has an unsupported drawing call before its snapshot.
  S2 can take its screen families from the 2d_* rankings (the first call is the order the backend meets them; the full list per row is `unsupported_before`).

## Rules (G2, decided by the lead 2026-10-24)
terrain_verdict for a terrain-yes row: on the in-game `view` region, one-level px / region AREA <= glraster.MAX_ONE_LEVEL (0.015) and beyond-one-level
px <= glraster.MAX_BIG (70), both imported from port/test/glraster.py (not copied). The terrain pass draws no pixels at HEAD, so there is no drawn-pixel
count: the denominator is the view region's area (334,888 px, framegate.label_map("ingame")). 2D regions keep the 0 px rule (status PASS = 0 px).
Primary cause: terrain-yes and verdict FAIL -> `terrain not drawn` whatever the 2D cause (the 2D cause stays in `twod_cause`); otherwise the 2D cause.
Raw region numbers stay in `regions`; the view's px/one/beyond also in `terrain_view_px_one_beyond`.

## Commands (export root port/build/S0_head, SG_PORTSIM_DLL = port/build/S0_3/Release/portsim_c.dll)
    ~/bin/py -3.12 -m port.test.framegate --all --jobs 2 --out log/roadmap/S0/3_census.tsv --resume   (64 scenarios, about 35 min wall, foreground)
    ~/bin/py -3.12 log/roadmap/S0/3_rerun_noend.py <scenario> > log/roadmap/S0/3_rerun_<scenario>.json   (cov_a_champ, championship, cov_k3_career)
    ~/bin/py -3.12 log/roadmap/S0/3_rank.py        (merges the three re-runs, writes log/roadmap/S0/3_out/frame_census.tsv and frame_causes.tsv; copied to re/roadmap/)
    SG_PORTSIM_DLL=<S0_3_w32 dll> SG_BACKEND=win32 ~/bin/py -3.12 log/roadmap/S0/3_win32_check.py <re/roadmap/frame_census.tsv>
Builds: port/build/S0_head/port/build/S0_3 (x64) and S0_3_w32 (Win32, with the runner), cmake from the VS 2022 BuildTools path (not on PATH).
framegate.py was copied from the shared tree (port/test/framegate.py, untracked there) into the export.

## Win32 sample (3_win32_check.log)
13 rows (9 non-PASS: UNSUPPORTED x7 by 4 causes, NOT_RUN x2, and 4 PASS: sandbox_basic/menu, cov_b_location/refused, cov_r_quit/title1, ui_tour/menu):
status, kind, total px, beyond px and the regions string equal to the x64 census row in all 13. LiveTerrain is not attached on Win32 (as S0.2).

## Controls
No new control: the census reuses S0.2's controls (gfx negctl 1 FAIL 49,162 px; swapped reference FAIL). The terrain_verdict is arithmetic on recorded
numbers, checked on its two ends: the only PASS has view 0 px; the smallest FAIL has one-level 0.004 % (fails on beyond px). Blind: the region rectangles.

## Defect found (U-1120)
cov_a_champ, championship and cov_k3_career: sg_run_boot returns -11 (missing exe constants / game file) after ALL records whenever any gfx.snapshotAt is
pending (even one at the first checkpoint; LiveTerrain on or off); without snapshotAt the boot passes with code 0 (the boot gate, and gfx.attach alone,
checked on cov_k3_career). framegate's census run therefore gave 22 NOT_RUN rows for them. Every sim hash matched, so 3_rerun_noend.py re-labels that
boot KNOWN (never PASS) and the 17 snapshots that were taken give rows (15 UNSUPPORTED, 2 PASS); 5 checkpoints took no snapshot and stay NOT_RUN. Their boot_status
reads KNOWN with the reason. The cause of -11 is not found. The first census run (3_census.tsv) keeps the unmerged 22 NOT_RUN rows.

## Open
U-1120 (above), U-1121 (G2 denominator note). The 34 MISSING-trace checkpoints need recordings (U-1115 context). Terrain pixel numbers are real only
after S3 draws the terrain.
