# S2 merge request, wave 7: tournament results, bulletin board, SimFoto / message ball (step S2.11, accepted)

Apply AFTER wave 6 (MERGE_wave6.md / 10_merge.md). Complete instructions: log/roadmap/S2/11_merge.md (section 0 = base: b3d61e41 +
wave 5 + wave 6; section 1 = 4 new files from port/build/S2_11w6_head; 11_shared.patch, 12 files, regenerated on wave 6, git apply
--check rc 0 there, 11 of 12 results equal to the combined mirror, tick.cpp differing only by S3.3's U-1120 hunks already in b3d61e41).
Gates (S2.11 mirror, then re-run on the combined wave-6 build 11w6_suite.log: test_gfx_s2_10 4 + 9 skipped, test_gfx_reports 36,
test_gfx_dialogs2 27, test_gfx_dialogs 32): 17 dialog rectangles on 13 pinned checkpoints 0 failing judged px, x64 and Win32; segments
205 / 205 x64 + Win32 0 digests changed; boot equal to S2.13; census PASS 83 -> 84 (cov_r_s2_screens bulletin), no status worse;
10 / 10 controls RED. Held: the message ball's head (S3.5 / S3.6, U-1216), bulletin photos are course captures (S3, U-1215),
championship tournament_end skipped until judged (U-1218). test_gfx_reports control 8 drops hole_stats (reading came from the
screen left under the panel; RED on its other 5 checkpoints). Do not commit *_img* or *.tsv.d dirs. Files: 11_files.txt.
