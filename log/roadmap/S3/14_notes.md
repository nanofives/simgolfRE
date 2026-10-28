# S3.14 notes: the I12 meeting point of S3's in-game frame and S7's app (test_app_ingame, test_app_live)

Integrator I12 merged S3's resubmission (MERGE_w5b.md, `13_assemble.py`) on HEAD d66aa755 (S7 waves 1-3 committed) and found two app
files failing that pass at HEAD: test_app_ingame (4 failed: the session's `TimeoutError: frame 40 never came (at 0)`, 7 presents,
view 0 px, 0 fitting frames) and test_app_live (2 failed: test_pacing_held slow-frame median 139 > 137, test_presents_replay
`(1, [430])`). This step reproduced both in its own mirror, found three causes, fixed them as anchored edits (14_edits.py) and
gated the result. Nothing in the shared tree was edited, staged or committed (except the S7 facts files the app tests rewrite
through the log junction, section 6).

## 1. Trees and builds

- port/build/S3_14 = `14_assemble.py` = `git archive d66aa755` + 13_assemble.py's steps (11_assemble.py's w2 / w4 / s35 / s37 / s38
  / s311, then s313) + step s314 (14_edits.py and the CLAUDE.md addition). Built x64 and Win32 with port/test/build.py and
  SG_FREETYPE_ROOT=C:\Users\maria\vcpkg\installed (14_build_x64_final.log, 14_build_w32_final.log: 0 errors).
- port/build/S3_14_head = plain `git archive d66aa755` with the same junctions (14_head.py), built x64: the HEAD comparison.
- port/build/S3_14_base = `13_assemble.py --dst S3_14_base --commit d66aa755`, the tree before S3.14's edits (= I12's mirror's
  recipe); 14_make_edits.py generates 14_edits.py from it.
- port/build/S3_14_verify = `14_assemble.py --dst S3_14_verify` from a fresh archive: identical to S3_14 in port/src,
  port/include, port/test, port/app, re/port, UNCERTAINTIES.md and CLAUDE.md (section 5).
- port/build/I12 and port/build/x64 were not touched. Env for tests: `. log/roadmap/S3/14_env.sh` from a mirror root
  (SG_PORTSIM_DLL, SG_PORTSIM_W32_DIR / DLL32 / RUNNER32, SG_APP_EXE of that mirror).

## 2. Reproduction

- test_app_live (S3 mirror, probe log/roadmap/merges/I12/probe_presents.py -> 14_probe_live_s3.log): 430 presents, 420 frame
  heads; only the LAST present (index 429, frame 420 = the final frame, recording position 2,363,551) differs, and the replay
  gives NO snapshot for it (14_probe_last.py: the three presents before it equal, `snapshot none`). Reproduced every run.
- test_app_ingame (S3 mirror): the session reached the game in every run at the machine's ambient load; test_pacing_under_ingame_load
  failed in 3 of 15 sessions (14_repro_ingame_s3*.log, 14_diag_runs*.log), each time on ONE frame without a terrain pass whose work
  passed 87 ms: frame 107 109 ms, frame 301 91 ms, frames 84 / 85 89 / 442 ms. d66aa755 (S3_14_head): 0 of 6 (14_repro_ingame_head.log,
  14_head_x5.log). I12's TimeoutError was reproduced on BOTH trees only under load (section 3.2).
- I12's test_app_live pacing failure (slow median 139): this step's sessions gave 129.5 (14_probe_live_s3) and the gates' run passes;
  I12's own facts file showed 127 on another run. A load effect (section 3.2).

## 3. Causes

### 3.1 The final frame's present has no replay snapshot (U-1394)
port/src/sim/tick.cpp run_frames, the boot run's last-frame branch (`static_cast<size_t>(k) == frames.size()`, "the last frame ran to
the end of the trace"): gfx_end_of_input(ctx) ran only in the `catch (const NeedInput &)` arm (S3.3, U-1120). An app session
stops at the next frame head (host.cpp LiveSession::frame_head returns false when stopping), so the replay's last frame runs to its
end without NeedInput, and a gfx.snapshotAt pending past the last consumed clock read is never taken. Latent at d66aa755: its course
view draws no sprites, HUD text or ticker, so an idle view's back buffer never changes and the session's last present lies earlier
(I12's probe_presents_I10: 325 presents, last at frame 348 of 420). With S3 the course animates and every in-game frame presents
(430 'r' presents in 420 frames), so the last present always sits in the final frame.
Fix (edit 1): `if (!out_of_input && SG_NEGCTL(tk::g_negctl_frame) != 12) gfx_end_of_input(ctx);` before after_frame: the same
rule as S3.3 (every snapshot not yet taken lies past the last item the run could consume). It changes nothing without a gfx
backend (gfx_end_of_input is a no-op then), so segments / boot are unaffected by construction; the census's NOT_RUN rows are all
`MISSING: no trace` (golfer_events), so no census row had a pending snapshot at its run's end.

### 3.2 The app inherits the test runner's BELOW_NORMAL class (U-1395)
The machine's test-slot hook sets pytest to BELOW_NORMAL (Proyectos/.claude/scripts/testslot/proyectos_test_slot.py line 120,
`SetPriorityClass(..., 0x4000)  # BELOW_NORMAL, inherited by children`), and app_drive.launch passed no class: simgolf_port.exe ran
BelowNormal during test_app_ingame (Get-Process, 2026-10-27 23:2x). The host's sim thread is ABOVE_NORMAL (host.cpp
LiveSession::run, U-1655), which in a BELOW_NORMAL process is priority 7, below every normal-priority thread (8) on the machine.
Measured with a TEMPORARY diagnostic build (waits.tsv columns: page faults, sim-thread CPU, working set, present / gfx.dump / lock
waits, GDI TextOutA / surface copies, gfx bodies; 14_diag_frames.py; removed again, the verify tree proves it):
- the non-terrain spike frames are wall time with the thread off the CPU: frame 85 442 ms wall / 62 ms CPU, frame 321 82 / 0,
  frame 159 170 / 47 inside gfx bodies; 0 page faults, 0 ms waiting on frame_mu_ or mu_ in present(), GDI text 0.3 ms per frame;
- under 10 busy processes at normal priority (14_load.py): the S3 mirror's session took 296 s, its frame waits ended 1-5 s late
  (work 7-20 ms, waitTicks 1-7) and pacing failed; d66aa755 failed with I12's exact `TimeoutError: frame 40 never came (at 0)`
  (14_load_head.log; its Sandbox and Monterey clicks were dispatched by the boot's load pump: dispatches.tsv kind 3, section 3.3);
  the S3 mirror with the app at Normal class under the same load passed in 46 s (14_load_s3_normal.log).
S3's share: in-game frames do more work (non-terrain frame work 8-12 ms against 6-9 ms; gfx bodies 7.1-8.2 ms per frame) and the
boot is slower (3.3), so S3 needs less ambient load than d66aa755 to cross 87 ms or 1.5 s.
Fix (edits 2-3): app_drive.launch adds NORMAL_PRIORITY_CLASS (priority_flags()), the class an Explorer launch gives the app; env
SG_APP_PRIORITY=inherit keeps the parent's class (control). Only app_drive.launch starts real-time-paced sessions; the other
subprocess launches of simgolf_port.exe in the tests run --live-trace or the shell checks, which measure no pacing.

### 3.3 The drivers' title-menu wait is 1.5 s of quiet (U-1396)
app_replay._to_monterey and test_app_live._to_title took 1.5 s without a new present as the title menu. The boot's loading-screen
presents are 431-776 ms apart in the trace's clock on the S3 tree (14_boot_gaps.py over three sessions; d66aa755 117-174 ms: S3
decodes every loadFlic sheet for the backend between them, flic::sheet_to_gfx in menu.cpp load_flic), so on a loaded PC the quiet
wait returns among them, and a click posted then is dispatched by the boot's load pump (PumpKind::Load) and lost: the title menu
never sees Sandbox, and `frame 40 never came (at 0)`.
Fix (edits 4-7): app_replay.title_menu_hash() = sgh64 of the title menu's back buffer, computed from the pinned original's
sandbox_basic `menu` reference (c >> 3 restores the RGB555 word; 0x193d269df3e3ba0c, equal to the app's title present on both
trees); wait_shown_hash() polls shown.tsv for it. _to_monterey and _to_title wait for it before their quiet wait. The present-late
control (test_control_stale_present) shows every frame one present late, so its title frame never shows while the menu idles: it
passes `title_frame=False` (its assertion unchanged).

## 4. Gates (port/build/S3_14)

14_gates.sh (one line per step in 14_gates.log, full output in 14_g_<step>.log; comparisons by 14_compare.py in 14_*_compare.log).
13 = S3.13's results for the same S3 tree on 78ffb06d (13_gates.log); I12 = the integrator's run of the tree before S3.14's edits.

| gate | port/build/S3_14 | 13 / I12 | HEAD d66aa755 |
|---|---|---|---|
| test_app_* x64, one file per process, first and alone (14_g_app_x64.log, 14_suite/test_app_*.txt) | 6 / 6 PASS: ingame 12 + 1 xfailed, input 16, live 13, present 3 (4 deselected), replay 14, shell 18 | I12: ingame 4 failed, live 2 failed | ingame 12 + 1 xfailed, live 13 (I12's comparison); S3_14_head this step: ingame session + pacing 6 / 6 sessions |
| test_app_* Win32 | not run: HEAD runs no test_app_* file on Win32 (merges/I12/win32_files.txt) | - | - |
| segments x64 / Win32 (14_segs_*.tsv) | 205 / 205 each, 0 rows differ in status, frames, digest | 0 rows differ from 13 and from I12 | 205 / 205 |
| boot x64 / Win32 (14_boot_*.tsv) | 70 rows 66 PASS / 2 KNOWN / 2 NO_FRAMES each | 0 rows differ from 13 | 66 / 2 / 2 |
| frame census (14_census.tsv) | 698 rows: PASS 100, FAIL 537, UNSUPPORTED 51, NOT_RUN 10 | 0 rows differ from 13 in status, total px, beyond px | - |
| S3.2 terrain verdict (14_terrain_verdicts.tsv) | 527 PASS, 75 NOT_MEASURED | 0 rows differ from 13 | - |
| repo static (14_g_static.log) | 614 passed, 2 skipped | 614 / 2 | 614 / 2 |
| port suite x64, the other 76 files (14_g_suite_x64.log, 14_suite/) | 75 / 76 PASS; test_fttext 2 failed (test_font_metrics_equal_gdi_and_the_originals, _on_win32: the host GDI's FontRef metric words, U-1393, same failure and values as S3.13's reruns); with the app step 81 / 82 files | 13: 75 / 75 files (test_fttext passed at that hour, U-1393) | d66aa755's commit: x64 suite 74 / 75 files, test_fttext failing (U-1393) |
| port suite Win32, I7's 59 files (SG_BACKEND=win32, 14_g_suite_win32.log, 14_suite_win32/, 14_suite_compare_win32.log by 14_win32_compare.py) | 49 PASS; the other 10 fail exactly the 22 ids of merges/I10/win32_failed_ids.txt (the known 20 + test_fttext's 2, U-1393); 0 new, 0 known that passed; no file needed a re-run alone | I10 / I12: the same 22 | the same 22 |
| app pacing (14_pacing.py: 10 sessions per tree, c = S3_14, d = S3_14_head, alternating, test_app_ingame session_ran + pacing_under; 14_pacing.tsv) | c 10 / 10 session + pacing PASS, d 10 / 10. 14_pacing.py's `kept` regex matched nothing, so its stats columns were None; 14_pacing_parse.py read the 20 sessions it had kept in %TEMP% (s7_5_*, 2026-10-28 03:05..03:19, mapped by creation order and checked by present counts: every c 161-166, every d 87-94) into 14_pacing_parsed.tsv and copied their waits / frames / session files to 14_pacing/<v>_<k>/: frames with work under 87 ms, period median of medians c 91 / d 89 ms, max 98 / 98, 100 % within 99 ms in every session; frames over 87 ms work without a terrain pass: 0 in all 20; no-pass work median 6 / 4.5 ms, max 14 / 25 | - | (d) |

## 5. Controls

| control | reads | result |
|---|---|---|
| tick.frame 12 (the last frame's pending snapshots left untaken) | 14_probe_last.py on a fixed-tree test_app_live session (14_ctl_tick12.log) | RED: present 430 (frame 421, the last) `snapshot none`; mode 0: all three last presents equal |
| SG_APP_PRIORITY=inherit (the app at the runner's BELOW_NORMAL) under 10 busy processes | test_app_ingame session_ran + pacing_under (14_ctl_priority.log) | RED: pacing within-quantum 0.727, periods up to 208+ ms; the same load with the default (Normal) PASSES |
| the old 1.5 s quiet title wait (d66aa755's driver) under load at BELOW_NORMAL | test_app_ingame on S3_14_head (14_load_head.log) | RED: `TimeoutError: frame 40 never came (at 0)`, the clicks taken by the boot's load pump; the fixed driver under the same conditions reached the game (14_ctl_priority.log inherit run: session_ran PASS) |

No control is blind. The title wait has no in-tree switch: its control is d66aa755's driver (the code before edit 5).

## 6. Process notes

- 2026-10-28 (resumed session): all three S7 facts files were restored to d66aa755's content, file by file (`git cat-file
  --filters HEAD:<path>`, so the working copy matches a checkout; `git status` clean under log/roadmap/S7/); the last pacing
  session's ingame facts are kept as 14_facts_saved/8_app_ingame_facts.S3_14_pacing_d09.json. The Win32 suite runs no app file, and
  the status stayed clean after it. Earlier text of this note:
- The app tests write ROOT/log/roadmap/S7/8_app_ingame_facts.json, 8_app_live_facts.json, 8_present_facts.json; a mirror's log is
  a junction to the shared log, so every mirror run rewrites these committed files (I12's runs had too: they were already modified
  when this step began). The live and present files were saved first (14_facts_saved/) and put back after this step's runs; the
  ingame file had been rewritten by this step's first HEAD run before it was saved, so it holds this step's gate run's facts
  (copy 14_facts_saved/8_app_ingame_facts.S3_14_gate.json). U-1398.
- The app files' pacing gates measure the host's scheduling: run them one file per process, ideally alone (14_gates.sh runs them
  first, before the heavy steps).
- `~/bin/py` is a shell script; a Python runner that starts it as a subprocess needs py.exe (14_load.py takes the command after `--`).
- This step's kept app sessions (S7_5_KEEP=1 and the probes) are still in %TEMP% as s7_5_* / s7_6_* dated 2026-10-27, about 140 MB
  each (28 dirs; the two dated 2026-10-25 are not this step's): removing them was refused by the session's permission classifier.
  The notes cite s7_6_7q6t0lup, s7_6_kbw_les4, s7_5_hrm2kvke, s7_5_xw50wfl_, s7_5_s29pn28u, s7_5_rczsjjlr; the rest can go.
- The temporary diagnostic build (waits.tsv columns 9..19, gditext / gfx.cpp / host.cpp timers) was removed from the mirror before
  the gates; port/build/S3_14_verify (a fresh 14_assemble.py) equals the gated mirror, so no diagnostic code was gated.

## 7. Reproduce

```
~/bin/py -3.12 log/roadmap/S3/14_assemble.py [--dst S3_14] [--recopy]            # the tree (14_assemble*.log)
~/bin/py -3.12 log/roadmap/S3/14_head.py                                          # plain d66aa755 (S3_14_head)
(cd port/build/S3_14; SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed' ~/bin/py -3.12 port/test/build.py --arch x64; ... --arch Win32)
nohup sh log/roadmap/S3/14_gates.sh > log/roadmap/S3/14_gates.log 2>&1 &          # app files, segments, boot, census, terrain, static, suite
~/bin/py -3.12 log/roadmap/S3/14_compare.py segs|boot|census|terrain|suite
(cd port/build/S3_14; . ../../../log/roadmap/S3/14_env.sh; ~/bin/py -3.12 ../../../log/roadmap/merges/I12/probe_presents.py <dir>)
(cd port/build/S3_14; . ../../../log/roadmap/S3/14_env.sh; ~/bin/py -3.12 ../../../log/roadmap/S3/14_probe_last.py <session> 3 [--negctl tick.frame=12])
~/bin/py -3.12 log/roadmap/S3/14_boot_gaps.py <session dir>...                    # boot present gaps
~/bin/py -3.12 log/roadmap/S3/14_load.py 10 -- <py.exe> -3.12 -m pytest ...        # a test under 10 busy processes
~/bin/py -3.12 log/roadmap/S3/14_make_edits.py                                    # regenerate 14_edits.py from S3_14_base -> S3_14
```

## 8. Open

U-1396 partial (the location screen is still found by quiet), U-1397 (test_plain_install_start_reaches_title's quiet wait),
U-1398 partial (the S7 facts files are restored, but the tests still write there). U-1399 unused. The UNCERTAINTIES rows
U-1393 (13_uncertain.tsv) and U-1394..U-1398 are applied by neither 13_assemble.py nor 14_assemble.py: 14_merge.md section 4 asks
the integrator to append them.

## 9. Resubmission c

log/roadmap/S3/14_merge.md = MERGE_w5b.md + S3.14 (apply order, CLAUDE.md addition in its section 3, byte-identical to the fence
14_assemble.py read for port/build/S3_14_verify; UNCERTAINTIES section 4; facts files section 5; every gate number section 7);
14_files.txt = files_w5b.txt + S3.14's files.
