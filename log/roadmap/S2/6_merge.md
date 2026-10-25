# S2.6 merge lines

1. re/tools/scenario.py, the end-of-file import hook (after integrator I4 commits it): replace the glob line, whatever I4
   commits, with
```
for _p in sorted(_glob.glob(str(pathlib.Path(__file__).with_name("scenario_s[0-9]*_*.py")))):
```
   Until then re/tools/s2_6_runs.py imports re/tools/scenario_s2_6.py itself (pinning, capture, trace and replay ran through it).
   port.test.boot / framegate / test_s2_6_scenario.py do not need the registration (they read the trace and the capture).

2. re/analysis/data/scenario_refs.json: `scenario_refs.py regen` rewrites the whole manifest. Its entry for this step is the
   key "cov_r_s2_screens" (family s36a, reference log/c3/alt/refs/cov_r_s2_screens/det_b, schedule_sha256
   a3e32c277f53c51901a0546eb7224cadce617b54b8cadb44bd7e76a2a1449ad2, reference_sha256
   325f1f018e8896cb7c700f2943f0ef54f88645c674dac653aff98f00890426b1, 32 checkpoints, det_a == det_b == det_c, winmm 381ed2b2,
   instance s2_6, recorded 2026-10-24T18:31:38). Take that key from the working-tree file; the file's other changes are siblings'.

3. port/test/boot.py KNOWN (U-1253): add
```
    "cov_r_s2_screens": [(771, 772, _CURSOR, "U-1253")],   # cursor tile at the end-of-year exit click
```
   With it the boot gate reads FAIL only for the missing showBuyLandScreen after record 1156 of 1268 (U-1251); without it,
   FAIL at record 771. port/test/test_s2_6_scenario.py applies the same entry itself while it is absent.

4. UNCERTAINTIES.md: rows U-1250..U-1253 from log/roadmap/S2/6_uncertain.tsv.
