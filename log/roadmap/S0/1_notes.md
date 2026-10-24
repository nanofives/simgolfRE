# S0.1 notes: baseline of every regression gate at HEAD 4bfefc2a

Deliverable: re/roadmap/BASELINE.md (numbers, commands, wall times, logs, comparison with the last record). Uncertainty rows: log/roadmap/S0/1_uncertain.tsv (U-1100..U-1102).

## What was run (all inside port/build/S0_head, one heavy run at a time)
1. Build: `~/bin/py -3.12 port/test/build.py --arch x64` (3 m 05) and `--arch Win32` (2 m 06); logs 1_build/.
2. x64 suite one file per process: `sh log/roadmap/S0/1_suite_by_file.sh` (copy of log/p38/suite_by_file.sh with paths in the export; env OUT, FILES, SG_BACKEND, STATIC). Log 1_suite_x64.log, per file 1_suite/. 41 / 41 PASS.
3. Static suite: first run 2 failed (export lacked shim/build/winmm.dll); junctions `shim/build` and `re/replay` added to the export (not in export.md); rerun 600 passed, 2 skipped (1_static_rerun.log).
4. Win32 suite: same script with SG_BACKEND=win32 over the 33 files that use the `portlib` fixture except test_boot (1_suite_win32.log, 40 m 34): 25 PASS, 8 files with 20 failures (U-1100: 11, U-1101: 9).
5. Segment gate x64 / Win32 with `port.test.integrator segments` and `compare` against log/p5/segs_*_loc.tsv: 205 / 205, 0 digests differ (1_segs_*).
6. Boot gate `port.test.boot --all --jobs 3`: 61 HEAD scenarios 58 / 2 / 1 as recorded; 4 S1 scenarios from the shared corpus are extra (U-1102). 1_boot_compare.py reads both TSVs.
7. P5.3 gates, sve_roundtrip, test_tick: taken from the x64 per-file run (counts in BASELINE.md section 6).

## Differences from the last record, explained
- test_commands: the old whole-file run died in a pytest internal error; the per-test rerun at the time and now agree (83 PASS, 44 SKIP).
- test_boot 85 -> 88: S1 traces in the shared corpus (U-1102).
- test_framegate (S0.2's file) is not part of HEAD; it appeared in the export during the run and ran only in the Win32 pass.
- Every other count equals the record; five new P5.3 files have first numbers.

## Open
- U-1100 / U-1101: Win32 failures (20 tests) are the Win32 baseline until fixed; U-1101 is not diagnosed.
- U-1102: the corpus junction is live; copy instead of junction if exact reproduction of boot / test_boot is needed.
- Not run: the SG_EMU_ORACLE=1 opt-in oracles, runtime (game boot) suites, slow-marker suites of tests/ (they need the game and a slot; the row asks for the port gates).
