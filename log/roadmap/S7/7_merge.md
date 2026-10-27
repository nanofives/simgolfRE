# S7.7 merge instructions: S7 (live core + app) on HEAD 2c05f02a

Worker S7.7, 2026-10-24. Base: HEAD 2c05f02a (S1 + S8 merged; port/build/S7_7_head/HEAD_COMMIT). This file REPLACES
log/roadmap/S7/1_merge.md (S7.1) and log/roadmap/S7/6_merge.md (S7.6): merge S7 from here only. S7.2's (2_merge.md) and S7.3's
(3_merge.md) lines still apply as written; they are repeated in sections 3 and 4 so that one file covers all of S7. Everything below was
built and measured in the mirror port/build/S7_7_head (recipe log/roadmap/S7/7_mirror.py: `git archive 2c05f02a`, the S0 junctions +
shim/build + re/replay, S7's four live files, the two patches, S7.1's modules.inc line, S7.2's CMake line, and the shared tree's port/app
and app tests). Notes: log/roadmap/S7/7_notes.md. Files: 7_files.txt.

## 1. New files: copy from the mirror into the shared tree (same paths)

| file | what |
|---|---|
| port/include/portsim/sim/live.h | LiveHost / LivePump / LiveMarker / LiveOptions / LiveResult, run_live, make_trace_live_host, make_script_live_host, make_prefix_live_host (S7.7) |
| port/src/sim/live.cpp | module `live`: the core, the recorder (U-1604: Load dispatches marked by a palette QPC record, focus keys as MSG records with no HANDLER), SCRIPT HOST (S7.7: `wait_load` step, SG_LIVE_SCRIPT_LOG), PREFIX HOST (S7.7), negctl `live.recorder` modes 1..8 |
| port/test/live.py | the harness; S7.7: OP_LIVE 40, scripts `bio_type` / `load_input`, SCRIPT_FILES / SCRIPT_PREFIX / KIND_PREFIX / script_data, input_paths |
| port/test/test_live.py | the tests; S7.7: MIN_RECORDS / roundtrip checks for the two scripts, backends test over script_data, two NEGCTL rows |

Reference copies: `port/build/S7_7_head/<path>`, also kept in `log/roadmap/S7/7_live/<path>` (git-ignored log/ dir, add with -f if
the copies should be staged). The copies in port/build/S7_1_head, S7_6_head and S7_6_h2 are superseded.

## 2. Shared files: two patches (replace 1_tick.patch, 1_capi.patch and 6_capi.patch)

```
git apply log/roadmap/S7/7_tick.patch    # port/src/sim/tick.cpp, port/src/sim/tick_hook.h, port/include/portsim/sim/tick.h
git apply log/roadmap/S7/7_capi.patch    # port/include/portsim_c.h, port/src/portsim_c.cpp, port/test/runner32/runner32.cpp
```
Both pass `git apply --check` against HEAD 2c05f02a and against the shared working tree of 2026-10-25 01:10, which holds S3.2's
uncommitted runner32.cpp (`log/roadmap/S7/7_make_patches.py`, log 7_make_patches.log).
- 7_tick.patch: S7.1's 1_tick.patch, regenerated against HEAD 2c05f02a. The changed lines are the same and only the hunk offsets moved
  (S1's tick.cpp edits sit between them). It contains no S7.7 change, because the live core reads S1's parse_trace and TraceInput as
  they are.
- 7_capi.patch: 1_capi.patch + 6_capi.patch, plus two changes. (a) The live run's runner32 op is now 40, not 28: S3.2's uncommitted
  runner32.cpp uses 28..32 for LiveTerrain (U-1661). The block sits before `op == 20` and the `handle ops 17..27` comment line is
  left alone, so the patch applies next to S3.2's. (b) sg_live_run_builtin kind 3 = the prefix host (data = u32 trace length, trace,
  script text). The header comment says so.

Inert in a trace run, as S7.1 measured: segments 205 / 205 with 0 digests changed and the boot gate unchanged (7_notes.md section 2).

## 3. modules.inc and CMake

- port/src/sim/modules.inc: append `SG_MODULE(live)` at the end, then `touch port/src/registry.cpp`.
- port/CMakeLists.txt (S7.2's line): append `add_subdirectory(app)` at the end.

## 4. port/app and the app tests

Untracked in the shared tree. The files there are S7.6's and are unchanged by S7.7 (the mirror syncs them in and reported no
difference). Merge them as listed in 6_merge.md section 4, plus S7.2's / S7.3's files (2_files.txt, 3_files.txt):
port/app/**, port/test/test_app_input.py, port/test/test_app_shell.py, port/test/test_app_live.py.

## 5. UNCERTAINTIES.md, CLAUDE.md

- Append log/roadmap/S7/1_uncertain.tsv, 2_uncertain.tsv, 3_uncertain.tsv, 6_uncertain.tsv and 7_uncertain.tsv (U-1600..U-1609,
  U-1610..U-1619, U-1620..U-1629, U-1650..U-1659, U-1660..U-1664). Status changes: U-1600 and U-1601 resolved (6_merge.md section 5);
  U-1604 resolved (S7.7, 2026-10-24) with the text of 7_notes.md section 1; U-1661 and U-1664 are already marked resolved in
  7_uncertain.tsv. U-1650 stays open: it is now unblocked, because S1's bio EditBox exists at HEAD.
- CLAUDE.md: one paragraph for S7 after the S8 paragraph: 7_notes.md section 1 (it covers S7.1, S7.6 and S7.7; S7.2 / S7.3 have their own
  in 2_notes.md / 3_notes.md section 1).
