# S7.6 merge instructions (the app on the live core)

Worker S7.6, 2026-10-24. Base: HEAD c6d7d78b (port/build/S7_6_head/HEAD_COMMIT). Built and measured in the mirror
port/build/S7_6_head (recipe log/roadmap/S7/6_mirror.py: git archive of HEAD, the S0 junctions + shim/build + re/replay, S7.1's
four files and two patches, S7.1's modules.inc line, S7.2's CMake line, the shared tree's port/app and app tests synced in).
Notes: log/roadmap/S7/6_notes.md. Files: 6_files.txt.

This step builds on S7.1 (accepted, not merged): merge S7.1 first (log/roadmap/S7/1_merge.md), then this.

## 1. S7.1's files, superseded by this step's copies

S7.1's mirror copies (port/build/S7_1_head) of these four files are replaced by this step's (port/build/S7_6_head, same paths):

| file | what this step changed |
|---|---|
| port/include/portsim/sim/live.h | LivePump::idle_ok, LiveMarker + LiveHost::marker, LiveResult idle_declines / inflated / mark_pos, the replay-rule comment (idle passes, U-1601) |
| port/src/sim/live.cpp | the core simulates TraceInput::batch over the loop's calls (`taker`): idle_ok / must_deliver from it, the inflated dispatch (a CURSOR record in front, filled by the next read; closes U-1600), a clock record after every modal Blocking pump call and after H1's frame wait (marks), LiveMarker, negative controls 5 (an idle pass left out) and 6 (a dispatch left out), script host steps `idle N` / `mark` |
| port/test/live.py | idle_frames (U-1601 frame + draw-count check), gfx_handled, sg_live_marks / sg_live_stats_ext readers, IDLE_SCRIPT |
| port/test/test_live.py | idle_ok in the ctypes pump struct and the callback host, four new tests (idle passes, idle pass left out x3, dropped batch, the inflated stuck delivery) |

## 2. Shared files: patches

```
git apply log/roadmap/S7/1_tick.patch    # S7.1, unchanged
git apply log/roadmap/S7/1_capi.patch    # S7.1, unchanged
git apply log/roadmap/S7/6_capi.patch    # this step, over 1_capi.patch: port/include/portsim_c.h, port/src/portsim_c.cpp
```
6_capi.patch: `sg_live_pump.idle_ok` (a field appended at the end, so a caller built against S7.1's struct still reads the
same prefix), `sg_live_stats_ext` (idle declines, inflated dispatches), `sg_live_marks` (mark positions). The Win32 runner is
unchanged (op 28's reply is S7.1's; the two new calls are x64 only). Checked by log/roadmap/S7/6_make_patches.py: 1 then 6 apply
to HEAD's files and to the shared working tree of 2026-10-24.

No tick.cpp / commands.h / tick_hook.h change beyond S7.1's 1_tick.patch.

## 3. modules.inc

S7.1's `SG_MODULE(live)` line only (1_merge.md section 3). Nothing more.

## 4. port/app (owned by S7 since S7.2 / S7.3; untracked in the shared tree, the files there are this step's)

| file | |
|---|---|
| port/app/host.h, port/app/host.cpp | new: LiveSession (the LiveHost of the app) |
| port/app/main.cpp | the live session is the default mode (run_live_window / live_run); options --idle-ms --pass-ms --cursor --flag17 --max-records; negctl pacing-off / present-late / recorder:<mode>; session.json / presents.tsv / shown.tsv / frames.tsv / trace.trace |
| port/app/CMakeLists.txt | host.cpp in simgolf_port |
| port/test/test_app_live.py | new: the step's gate (its own build in port/build/S7_6_app) |

port/app/pacing.*, present.*, window.*, saves.*, input_map.*, tools/* are S7.2 / S7.3's, unchanged.

## 5. UNCERTAINTIES.md, CLAUDE.md

Append log/roadmap/S7/6_uncertain.tsv (U-1650..U-1659). Mark U-1600 resolved (the trace host's stuck deliveries are recorded
inflated and replay: 6_notes.md section 3) and U-1601 resolved (idle passes, with the residue in U-1652 / U-1656). One CLAUDE.md
paragraph: 6_notes.md section 1.
