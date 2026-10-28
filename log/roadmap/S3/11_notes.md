# S3.11 notes: the S3 resubmission tree (waves 2 + 4 + 5 + S3.7 + S3.8 on HEAD)

## 0. Machine use (for S3.10 and the other workers)

- 2026-10-25: all heavy runs of this step are done (x64 suite 17:40-20:55, Win32 S3 files 20:59-22:25, segments 22:30-22:45,
  boot 22:45-23:04, framegate census 23:05-23:26, S3.2 terrain census 23:27-23:40 plus two single-scenario re-runs to 23:42).

## 1. The tree

`~/bin/py -3.12 log/roadmap/S3/11_assemble.py [--recopy] [--commit <rev>]` makes port/build/S3_11_src = `git archive` of the
commit (bbc9072f) + I6's junction list, copies the steps' own files from the shared tree and applies every shared-file edit
from the steps' own texts, anchored (each old text exactly once, an edit already applied refused), in the parent's order. Log:
11_assemble.log (first run, steps w2..s38) and 11_assemble_verify.log (the whole chain incl. s311 from a fresh archive: equal to the measured tree, CLAUDE.md copied over after it). port/build/S3_11_src/port/build/x64 and Win32 are junctions to port/build/S3_11 and S3_11_w32.

Own-file sources, checked: every own file in the shared tree equals its step's final mirror (S3_4c_src, S3_5b_src, S3_6b_src,
S3_7_src, S3_8_src) except port/test/test_gfx_text.py, which S3.7 changed after its mirror copy (14:00, the cov_a_flags `end`
frame of its late note; 7_notes.md section 4 numbers are with it). S3.10's in-place files (gl_raster.cpp / .h, test_gl_raster.py,
glraster.py, gfx_terrain.cpp) are taken at the commit's version; S3.8's gfx_terrain.cpp edits are applied to that.

Every anchor applied exactly once on bbc9072f (92 code / table edits, 20 UNCERTAINTIES / CLAUDE.md edits and the D53 regeneration from the steps, + 4 of S3.11), and the steps' edits also apply exactly
once on a copy of I7's current mirror port/build/I7 (applied 13:46; 11_i7chk.log), so the rebase onto I7's commit should be
mechanical. Cross-check against the steps' own mirrors: gfx.cpp = S3_7_src's + exactly S3.8's 5-line bar hunk; gfx_hud.cpp and
tick.cpp = S3_7_src's; gfx_terrain.cpp and routing.cpp = S3_8_src's; test_gfx_routing.py = S3_8_src's + S3.5b's test hunk.

## 2. Integration fixes (S3-owned files only; 11_edits.py, applied last by 11_assemble.py)

1. test_flic.py `test_sheet_to_gfx_draws_the_recorded_cells` failed in the assembled tree (1 failed, 9 passed) and fails the same
   way on S3.5b's own mirror port/build/S3_5b_src with its own build (S3.5 / S3.5b did not run test_flic): S3.4's hook-up test
   draws a sheet cell with Image_draw and no mask and expects the slot palette, while S3.5 resolved U-1332 (a sheet cell draws
   with its queue entry's palette object; gfxs::image_draw reports a cell-pool sprite with no palette word and no mask as
   `a sprite with no palette` and draws nothing). Resolved in S3.5's favour: TEST-ONLY body `gfx_scene.setPalette(key, words)`
   (gfx_scene.cpp) fills a palette object, and the test passes the slot palette as Image_draw's mask (drawB's key for the call);
   without the scene family the mask stays 0 and the core path draws as before. test_flic 10 passed (port/build/S3_11_fix).
   The x64 suite's DLL was rebuilt by lib.load() at 18:35 after this edit, so the suite files from about test_gfx on ran with it
   (the edit adds one test-only body, nothing else).

## 3. Gates (numbers in MERGE_w5.md section 5)

- port suite x64 one file per process: 69 files, 68 PASS (test_boot's known cov_r_s2_screens stop); every file not touched by S3
  equal to I6's counts; test_flic 10 after the section 2 fix. Repo static 614 passed / 2 skipped.
- Win32, the 26 files S3 touches: 25 PASS, test_tick's baseline failure (test_run_frames_reports_why_it_stopped, the same as I6's);
  test_gfx_text 32 + 20 skipped.
- gfx.check 0 duplicates (125 bodies; Surface_drawSprite and Widget_fillRect only in `reports`), x64 and Win32.
- segments 205 / 205 x64 and Win32, 0 digests differ from the committed tables, x64 = Win32; course_season/08 Win32 3 / 3 PASS.
- boot 70 rows x64 and Win32, 0 differ from the committed rows (HEAD's 61 at 58 / 2 / 1).
- census 698 rows: PASS 91 (was 87), 0 PASS rows lost, 0 rows with more differing px; S3.2 terrain verdict 527 / 527, all 602 rows
  equal to I6's; ranked by the S3 rule: FAIL with no unsupported call 477, PASS 91, terrain not measured 75, no body
  Surface_createFull_fwd1 23, no body showRoutingMapScreen.teeSprite 22, no trace 10.
- Transient: the full terrain census lost cov_k3_build's boot once (12 rows `boot FAIL`, no reason in the output); two runs of that
  scenario alone give 12 / 12 PASS with I6's numbers (11_terrain_k3build.tsv, 11_terrain_k3build_2.tsv); the final table takes the re-run.

## 4. Reproduce

```
~/bin/py -3.12 log/roadmap/S3/11_assemble.py [--recopy] [--commit <rev>] > log/roadmap/S3/11_assemble_verify.log   # port/build/S3_11_src
(cd port/build/S3_11_src && ~/bin/py -3.12 port/test/build.py --arch x64 && ~/bin/py -3.12 port/test/build.py --arch Win32)
STATIC=1 sh log/roadmap/S3/11_suite_by_file.sh                                   # x64, OUT=log/roadmap/S3/11_suite_x64
SG_BACKEND=win32 FILES="$(cat log/roadmap/S3/11_win32_files.txt)" STATIC=0 OUT=log/roadmap/S3/11_suite_w32 sh log/roadmap/S3/11_suite_by_file.sh
~/bin/py -3.12 log/roadmap/S3/11_compare_suite.py log/roadmap/merges/I6/suite_merged log/roadmap/S3/11_suite_x64     # (Win32: I6/suite_win32)
(cd port/build/S3_11_src && SG_BACKEND=win32 ~/bin/py -3.12 ../../../log/roadmap/merges/I6/gfx_check.py)               # and without SG_BACKEND
cd port/build/S3_11_src
~/bin/py -3.12 -m port.test.integrator segments --dll <S3_11 dll> --jobs 3 --out ../../../log/roadmap/S3/11_segs_x64.tsv
~/bin/py -3.12 -m port.test.integrator segments --backend win32 --dll32 <S3_11_w32 dll> --runner32 <runner> --jobs 3 --out .../11_segs_win32.tsv
  ... --only course_season/08 --jobs 1 --out .../11_segs_cs08_<k>.tsv   (k = 1..3);  ~/bin/py -3.12 log/roadmap/S3/11_segs_compare.py (repo root)
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out ../../../log/roadmap/S3/11_boot_x64.tsv      (SG_BACKEND=win32 for 11_boot_win32.tsv)
~/bin/py -3.12 log/roadmap/merges/I6/boot_compare.py log/roadmap/S3/11_boot_x64.tsv               (repo root)
~/bin/py -3.12 -m port.test.framegate --all --jobs 2 --out ../../../log/roadmap/S3/11_census.tsv --resume
SG_PORTSIM_DLL=<S3_11 dll> ~/bin/py -3.12 ../../../log/roadmap/S3/2_census.py terrain --jobs 3 --out ../../../log/roadmap/S3/11_terrain_verdicts.tsv
cd ../../..
~/bin/py -3.12 log/roadmap/S0/3_rank.py log/roadmap/S3/11_census.tsv log/roadmap/S3/11_census_out
~/bin/py -3.12 log/roadmap/merges/I6/census_compare.py log/roadmap/S3/11_census_out/frame_census.tsv - log/roadmap/S3/11_census.tsv
~/bin/py -3.12 log/roadmap/S3/11_census_px.py; ~/bin/py -3.12 log/roadmap/S3/11_terrain_merge.py
~/bin/py -3.12 log/roadmap/S3/2_census.py rank log/roadmap/S3/11_census.tsv log/roadmap/S3/11_terrain_verdicts_final.tsv --out log/roadmap/S3/11_census_ranked_s3rule.tsv
~/bin/py -3.12 log/roadmap/S3/11_files.py                                         # files_w5.txt
```

## 5. Controls

S3.11 adds no port behaviour. The one integration fix is judged both ways: test_flic's hook-up test with mask 0 (the steps' text)
reads RED on the merged tree (1 failed, 11_suite_x64/test_flic.txt) and GREEN with the slot palette as the mask (10 passed,
11_suite_x64_rerun.log, and on Win32); a wrong cell decode under the fixed test was not run as a separate control. Every step's own
controls are in its notes and ran inside the suite files above (test_gfx_scene, test_gfx_hud, test_gfx_text, test_terrain_overlay,
test_flic / test_defects_flic, test_menu, test_terrain_live, the S2 gfx files with S3.5b's edits).

## 6. Open

- I7 has not committed yet (HEAD is still bbc9072f): the rebase and re-measure on I7's commit are MERGE_w5.md section 7. Every anchor
  already applies once on I7's mirror as of 13:46.
- Census causes left by the S3 rule: 477 rows FAIL on pixels only (median 103 px; region counts in MERGE_w5.md section 5), the
  unported bodies Surface_createFull_fwd1 (23 rows) and showRoutingMapScreen.teeSprite (22 rows: the routing map's FLIC tee / flag
  sprites, S3.8 and S2.4 notes), 75 rows with no terrain recording, 10 with no trace.
- No new U-row (none was needed; the lead was not asked for ids).

## 7. Files

log/roadmap/S3/files_w5.txt (the union of the steps' lists, the shared files the edits write, the S6 log files to put back, and every
log/roadmap/S3/11_* path). Build dirs: port/build/S3_11 (x64), S3_11_w32 (Win32), S3_11_fix (the x64 build test_flic's fix was first
checked on), the tree port/build/S3_11_src, and port/build/S3_11_i7chk (the anchors check on I7's mirror; sources only).
