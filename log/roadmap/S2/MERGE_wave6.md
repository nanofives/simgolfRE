# S2 merge request, wave 6: buy land, syncTiles, character editor and skill-dialog drawing (step S2.10, accepted PARTIAL)

Apply AFTER wave 5 (13_merge.md). Complete instructions: log/roadmap/S2/10_merge.md (new files + 10_shared.patch, 8 files), base
b3d61e41 + 13_shared.patch. Lead check (2026-10-25): on a scratch copy of today's working-tree files, 13_shared.patch then 10_shared.patch apply.
New module `buyland` (port/src/sim/buyland.cpp: showBuyLandScreen simulation + drawing, syncTiles no longer a drop) needs its
modules.inc line (10_merge.md). Gates (S2.10 mirror): x64 14 / 14 files, Win32 10 / 10, segments 205 / 205 x64 + Win32 0 digests changed,
boot 70 rows PASS 66 / KNOWN 2 / NO_FRAMES 2 (cov_r_s2_screens now PASS 1268 / 1268), census 698 rows PASS 83, no row worse in status.
The four character-editor checkpoints (cov_r_s2_screens editor, cov_a_keys3 q_editor, cov_a_golfer button_0, cov_a_career career_editor)
reach 0 px only together with S3.5's gfx_scene.cpp (composeGolferSprite layers, addSprite) and S3.6's files: measured in a tree with
those (10_s3_*), 18-24 k px without them. buy_land keeps 161 px of the hole's GL line (U-1294, S3.8). Held: scroll text (no
checkpoint, U-1295), the purchase judged by the oracle only (U-1292). Do not commit *_img* or *.tsv.d dirs. Files: 10_files.txt.
