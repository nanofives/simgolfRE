# S7.1 merge instructions (live run core)

Worker S7.1, 2026-10-24. Base: HEAD c6d7d78b (port/build/S7_1_head/HEAD_COMMIT). Everything below was built and measured in the
mirror port/build/S7_1_head (git archive of HEAD + the S0 junctions + shim/build, re/replay); notes log/roadmap/S7/1_notes.md.

## 1. New files (copy from the mirror into the shared tree, same paths)

| file | what |
|---|---|
| port/include/portsim/sim/live.h | LiveHost / LivePump / LiveOptions / LiveResult, run_live, make_trace_live_host, make_script_live_host |
| port/src/sim/live.cpp | module `live`: the core (tk::LiveIo + InputSource + LiveWriteLog), the recorder, the SCRIPT HOST, the negctl switch `live.recorder` |
| port/test/live.py | harness: run_builtin (x64 ctypes / Win32 runner op 28), equivalence, roundtrip, SCRIPTS, CLI `-m port.test.live equiv|roundtrip` |
| port/test/test_live.py | the tests (marker `port`; test_equivalence_all is `slow`) |

The mirror copies are the reference: `port/build/S7_1_head/<path>`.

## 2. Shared files: apply the patches (both apply cleanly to HEAD c6d7d78b AND to the shared working tree of 2026-10-24 with
S1's uncommitted tick.cpp / commands.h edits in it: `git apply --check` OK, log/roadmap/S7/1_make_patches.py)

```
git apply log/roadmap/S7/1_tick.patch    # port/src/sim/tick.cpp, port/src/sim/tick_hook.h, port/include/portsim/sim/tick.h
git apply log/roadmap/S7/1_capi.patch    # port/include/portsim_c.h, port/src/portsim_c.cpp, port/test/runner32/runner32.cpp
```

What they do (every change is inert in a trace run: `FrameIo::live` / `FrameInput::live` are nullptr and `TraceInput::write_hook`
is nullptr there, so the trace-driven path is byte-identical; measured: segments 205 / 205 digests and the boot gate unchanged,
1_notes.md section 4):
- tick_hook.h: `struct tk::LiveIo` (clock_site / body_cursor / async_key), `FrameIo::live`, the three readers ask it when set;
  `tk::tick_frame_io` declared (it was defined in tick.cpp only).
- tick.h: forward `tk::LiveIo`, `FrameInput::live`.
- tick.cpp: `#include "portsim/sim/live.h"`; TkBase::clock_site and tick_frame_io pass `live` through; TraceInput gets a
  `write_hook` (told of each harness write it applies) and `friend class TraceHost`; new `class TraceHost` (a LiveHost over
  parse_trace + TraceInput) after `g_local_reg`, and `make_trace_live_host` after the tk namespace.
- portsim_c.h / portsim_c.cpp: sg_live_run (C callbacks), sg_live_run_builtin (kind 1 trace, 2 script), sg_live_recording,
  sg_live_records, sg_live_stats (12 counters); `sg_ctx::live_res`. The helpers sit outside `extern "C"`.
- runner32.cpp: the Api gets sg_live_run_builtin / sg_live_recording / sg_live_stats / sg_run_frames_detail; handle op 28 (live
  run with a built-in host; reply records, recording bytes, 12 stats, detail).

## 3. modules.inc (shared): append one line

```
SG_MODULE(live)
```
at the end of port/src/sim/modules.inc (after `SG_MODULE(terrain_live)` at HEAD), then `touch port/src/registry.cpp` (MSBuild does
not rebuild it for modules.inc).

## 4. Interactions with in-flight work

- S1 (uncommitted at 2026-10-24: tick.cpp parse_trace focus keys -> RootHandler 12 / 13, `PumpKind::Load` for loadFlic's pump):
  the patches apply over it, but the live core does not know those yet (they are not in HEAD). After S1 merges, two follow-ups
  (U-1604): `Core::batch` must pass `PumpKind::Load` calls to the host (today a modal non-Blocking call returns false without
  asking), the recorder must write a Load dispatch with a palette-QPC record right after its HANDLER records (parse_trace's
  `cur->load`), and commands 12 / 13 must be written as MSG 0x100 / 0x102 records with no HANDLER after them. Until then the
  trace host's equivalence on cov_r_flic_input / cov_r_bio is not claimed (both FAIL the boot gate at HEAD already, U-1102).
- S7.3 (app shell) uses `run_live` with its own LiveHost (C++), or sg_live_run from C. The host contract is in live.h (top
  comment and LivePump): block inside `pump` when `must_deliver`, stop the session at a frame head (frame_head returns false).
- S7.4 (app recordings in the boot gate): a recording is a schema v1 trace with meta `"live":1`; boot.py needs a capture
  (framehash.bin) to compare against, which a live session provides itself (LiveResult T / hashes, the framehash.bin order).

## 5. Nothing else

No CMakeLists change (src/sim/*.cpp is globbed), no CLAUDE.md / UNCERTAINTIES.md / PORT_PLAN.md edit by me: the U-rows are in
log/roadmap/S7/1_uncertain.tsv for the integrator.
