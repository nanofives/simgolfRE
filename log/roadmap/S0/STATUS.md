# S0 STATUS (lead cmvm8390a20svo115gkycw1d6, claude2 opus; section row S0 of re/roadmap/sections.tsv)

steps: 3/3 accepted (S0.1 baseline, S0.2 framegate, S0.3 census), 0 held, 0 running: SECTION DONE, merge request log/roadmap/S0/MERGE.md
workers alive: 0 (all three closed); claude2 sessions under S0: this lead only
game slots held: 0
BASELINE.md: written 2026-10-24 (static 600/2, x64 suite 41/41, segments 205/205 x64+Win32 0 digests, boot 58/2/1 on 61 HEAD scenarios, Win32 suite 20 failures = baseline)
census: re/roadmap/frame_census.tsv 609 rows at HEAD: PASS 45, UNSUPPORTED 525, NOT_RUN 39; frame_causes.tsv: terrain not drawn 520; 2D Widget_writePixel 316, Surface_blit3 168, Image_draw 39, Surface_drawScaled2 2
G2 applied: terrain_verdict on the view region (1.5 % / 70 px, glraster constants), view area as denominator while no terrain is drawn (U-1121)
open U-rows: U-1100, U-1101 (Win32 suite), U-1102 (live corpus), U-1110..U-1115 (framegate rules), U-1120 (-11 with snapshot pending), U-1121
files: log/roadmap/S0/files.txt; HEAD export port/build/S0_head can be removed after the commit (delete junctions first)
last event: 2026-10-24 S0.3 accepted and closed; MERGE.md + files.txt written
