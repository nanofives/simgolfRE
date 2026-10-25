# S3.3 (U-1120 -> U-1320): sg_run_boot -11 with a pending gfx.snapshotAt

## Cause
Not the snapshot itself. framegate sets `to_end=True` whenever a checkpoint's snapshot lies past the last FRAME record, so `sg_run_boot` is called
with max_frames = records + 1. That reaches the block in `run_frames` (tick.cpp, "a boot run's last frame": `boot && k == frames.size()`), which
runs the last frame with `tk::tick_frame_io` and does NOT catch `NeedInput`. The last frame of cov_a_champ, championship and cov_k3_career ends
in a Blocking pump (`pumpMessages`, `dispatch()` in tick.cpp: `kind == Blocking && src->exhausted()` -> `throw NeedInput{where}`), and
`NeedInput` derives from `MissingResource` (commands.h), which `run_frames_guarded` (portsim_c.cpp) maps to SG_E_NO_RESOURCE = -11, text
"missing exe constants / game file" in lib.py. Name of the missing "resource": `pumpMessages` (run_detail "missing: pumpMessages; trace slice has
no further FRAME record; ..."). Every sim hash matched because all records were done. The boot gate does not use to_end, so it never reached it.
Reproduced WITHOUT any snapshot: `3_repro.py <scenario> none` with to_end=True gives -11 in the old build; so "pending snapshot" was a correlation.
The no-FRAME-head path (cov_b_location) already caught NeedInput and called gfx_end_of_input; the with-FRAME path did not.

## Fix (tick.cpp, see 3_merge.md / 3_tick.patch)
Catch NeedInput in that block, call gfx_end_of_input (every snapshot still pending is taken from the back buffer), return 0 with a detail. The
end-checkpoint block's NeedInput catch also calls gfx_end_of_input.

## Numbers (mirror port/build/S3_head + S3_3 x64; build log 3_build.log)
- 3_repro.py, snapshotAt 100: cov_k3_career PASS 60/60, cov_a_champ PASS 573/573, championship PASS 2305/2305 (3_repro.log); old build: FAIL -11 on the same.
- framegate (LiveTerrain attached, probe-limit 3,000,000) with the fix: cov_k3_career 5 rows (3 UNSUPPORTED, answered PASS, end PASS), cov_a_champ 11
  (10 UNSUPPORTED, 1 PASS), championship 6 (5 UNSUPPORTED, 1 PASS), boot_status PASS in all 22; the S0.3 census had 22 NOT_RUN / KNOWN rows for these
  (17 snapshots + 5 without). Now all 22 have snapshots: 0 NOT_RUN. UNSUPPORTED rows keep their first unsupported call (Widget_writePixel etc.) as before.
  (jsons: 3_framegate_*.json; census table not regenerated here: the integrator re-runs framegate.)
- test_gfx_eoi.py: 5 passed (3_pytest_gfx_eoi.log). Control: the same file against the pre-fix DLL (S3_4_gfx build) 4 failed / 1 passed
  (3_pytest_gfx_eoi_control.log): the three to_end boots and the pending-past-the-end snapshot read RED, the no-to_end test (unchanged path) passes.
- Gates measured with 3_tick.patch applied (mirror port/build/S3_head, builds S3_3 x64 and S3_3_w32; build logs 3_build.log, 3_build_w32.log):
  - segments x64 205/205 PASS; `integrator compare` against merges/S6-1/segs_x64.tsv: 205 rows, 0 differ (3_segs_x64*).
  - segments Win32 205/205 PASS, 0 differ against merges/S6-1/segs_win32.tsv (3_segs_win32*).
  - boot gate `boot --all --jobs 3` (3_boot_gate.tsv, 3_boot_compare.py / .log): HEAD's 61 scenarios 58 PASS / 2 KNOWN / 1 NO_FRAMES, 0 rows differ
    from S0/1_boot_gate.tsv. The corpus holds 9 extra recordings that are not HEAD's (U-1102): 6 PASS or NO_FRAMES (cov_a_quit PASS, cov_a_quit_title
    NO_FRAMES, cov_r_palette_hot, cov_r_s8_play, cov_r_theme_cancel, cov_r_tutorial PASS) and 3 FAIL: cov_r_bio (-11 at record 579 in
    showCharacterEditor, the same row as in S0's baseline), cov_r_flic_input (diverges at record 1, g_cachedCameraTileX) and cov_r_s2_screens
    (g_cursorTileX / Y at record 771, then missing showBuyLandScreen). Those need S1 / S2 code the HEAD mirror lacks, and all three fail before their last frame.
  - test_gfx_eoi.py: x64 5 passed, Win32 5 passed (3_pytest_gfx_eoi*.log, 3_pytest_w32.py). The Win32 runner carries no sg_run_frames_detail,
    so the detail assertion applies only when a detail exists.

## Files
port/test/test_gfx_eoi.py, log/roadmap/S3/3_* ; shared edit only as text/patch (3_merge.md).

## Open
- None. U-1120 resolved; no U-1320..1329 rows needed.
