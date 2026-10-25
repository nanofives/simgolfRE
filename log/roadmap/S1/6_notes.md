# S1.6 notes: S1.4 + S1.5 shared-file patches merged and gated in one mirror

## Result
One combined diff `log/roadmap/S1/tick_combined.diff` (unified, `a/port/...` `b/port/...`, CRLF bodies kept, 194 lines, 6 files:
include/portsim/sim/chareditor.h, include/portsim/sim/commands.h, src/sim/commands.cpp, src/sim/commands_internal.h, src/sim/menu.cpp,
src/sim/tick.cpp). `git apply --check --ignore-whitespace` passes on HEAD 4bfefc2a; none of the six files has an uncommitted edit in the tree.
Apply it once, in this order, in place of 4_load_pump.diff and 5_merge.md sections 1-3: the single diff holds everything.

## Overlap in tick.cpp (the only one)
Both steps edit the CLOCKRUN `else { ctx_ra = ra; open_batch = false; }` arm of parse_trace. S1.5 inserts `focus_resolve(); focus_flush();`
before `ctx_ra = ra`; S1.4 inserts `if (open_batch && cur && !cur->body && sub == 2 && ra == kPaletteQpcRa) cur->load = true;` before it.
Resolution: S1.5's two calls first, then S1.4's line, then `ctx_ra = ra`. They touch disjoint state (focus_held / cur->cmds vs cur->load), so
order does not change either's result. S1.4's other tick.cpp hunks (TBatch::load; TraceInput::batch PumpKind::Load branch) and S1.5's four other
parse_trace replacements are textually disjoint. S1.4's context lines for the clock hunk no longer match after S1.5, so the combination is
expressed as string replacements (log/roadmap/S1/6/patches6.py), not as patch hunks. commands.h gets both S1.4's PumpKind::Load and S1.5's
handler 12 / 13 comment; chareditor.h S1.5's comment (5_merge.md section 3).

## Mirror and builds
`~/bin/py -3.12 log/roadmap/S1/6/mirror6.py` (S16_BASE=1: HEAD alone): tracked port/ files at HEAD, own files from the working tree (chareditor.cpp,
test_chareditor.py, golden_chareditor.json, test_chareditor_bio.py, test_s1_3_scenarios.py, test_s1_4_scenarios.py), then patches6.PATCHES; writes the diff.
Builds: port/build/S1_6 (x64, 6_build.log), port/build/S1_6_w32 (Win32 portsim_c + portsim_runner32, 6_build_w32.log), cmake from the VS 2022 Build Tools
(not on PATH; `/m` is mangled by MSYS, build without it). Win32 segments need `--dll32 ... --runner32 ...` explicitly: without them integrator tries to
build port/build/Win32 from the live tree (2 ERROR rows, a sibling's audio files), that run was discarded.

## Gates (SG_PORTSIM_DLL = the mirror DLL, never port/build/x64)
| gate | result |
|---|---|
| boot `--all` x64 (6_boot_gate.tsv/.log) | 63 PASS, 2 KNOWN (cov_a_golfer, cov_b_holes18), 1 NO_FRAMES (cov_b_location). Against S0/1_boot_gate.tsv only 3 rows differ: cov_r_bio FAIL(579/687) -> PASS 687/687, cov_r_flic_input new PASS 75/75, cov_r_tutorial re-pinned by S1.4 (1026 -> 1195 records) PASS |
| new scenarios | cov_r_bio 687/687, cov_r_flic_input 75/75, cov_r_tutorial 1195/1195, cov_r_theme_cancel 123/123, cov_r_palette_hot 565/565, all PASS (the last two equal their S0 rows) |
| segments x64 (6_segs_x64.tsv) | 205/205 PASS, status + digest of every row equal log/roadmap/S0/1_segs_x64.tsv (0 of 205 differ) |
| segments Win32 (6_segs_win32.tsv) | 205/205 PASS, 0 of 205 differ from S0/1_segs_win32.tsv |
| test_chareditor | 19 passed, 4 skipped (SG_EMU_ORACLE opt-in) |
| test_chareditor_bio | 28 passed, 8 skipped (oracle opt-in) |
| test_s1_3_scenarios | 8 passed |
| test_s1_4_scenarios, SG_S14_LOAD_PUMP=1 | 9 passed (the two flic tests run) |
| test_tick | 250 passed (559 s) |
| test_commands | 83 passed, 44 skipped (no pinned digests / oracle opt-in) |
Logs: log/roadmap/S1/6/t_<file>.log. One file per process throughout. SG_EMU_ORACLE=1 runs were not repeated: both steps ran them on their own mirrors and the
combined diff changes neither step's code, only the order of two disjoint statements.

## Not done / open for the integrator
- After merging, delete the `needs_load_pump` marker and the env name in port/test/test_s1_4_scenarios.py (4_merge.md section 1).
- UNCERTAINTIES.md, CLAUDE.md paragraphs, test_static module-list row: 4_merge.md / 5_merge.md sections 2, 4, 5 stay as written; no new U-row from this step.
- Win32 boot gate not run (the brief asks boot x64, segments both).
