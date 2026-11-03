# S3.30 merge request (lead / integrator)

## 1. Code: log/roadmap/S3/30_edits.py (27 anchored edits in 7 files + 1 new file)

Applied by `~/bin/py -3.12 log/roadmap/S3/30_assemble.py --root <tree> --only-30` after `w10_base.py`; checked on a fresh
w10_base tree (`30_assemble.py --dst S3_30v`): port/ 418 / 418 files equal to port/build/S3_30 (`w7_assemble.py --verify`), and
re/tools/trace_format.py, re/frida/js/trace_rec.js, re/model/trace_schema.md byte-equal.

Files: re/tools/trace_format.py, re/frida/js/trace_rec.js, re/model/trace_schema.md, port/schemas/trace.md, port/src/sim/tick.cpp
(parse_trace / TraceInput / TraceHost only; no kDrops / fallback table line), new port/test/test_trace_call.py, and TWO FILES
OUTSIDE MY ROW'S files_owned: port/src/sim/live.cpp and port/include/portsim/sim/live.h. Reason: the live run core re-records
every harness item TraceInput applies (LiveWriteLog::wrote); without a matching `called` the live core's recording of golfer_events
would lack the call, and live equivalence (sg_run_boot over the live recording, the integrator's live_equiv gate, now 71 rows
with golfer_events) would fail. The change is additive: a default no-op virtual `LiveWriteLog::called`, Core::called recording
DELIVER sub 7 where a write is recorded, Recorder::Ev gains `d` (argc), head items keep writes and calls in order. No other
LiveWriteLog implementer (port/app/host.cpp) changes. No modules.inc / CMakeLists.txt / registry change (announceLandmark is
already registered by events.cpp).

## 1b. Test that depends on this merge

tests/test_trace_format_s1.py::test_every_stored_trace_still_validates reads the shared corpus, which already holds
port/corpus/traces/golfer_events.trace (DELIVER kind 7): it FAILS on any tree without 30_edits.py's trace_format.py and passes with it
(gate step s1_trace: 7 passed, log/roadmap/S3/30_work/g_s1_trace.log). Base: a2f1f3c4 (I22 + I23) has the same port/ as my w10_base tree, so
the edits apply unchanged.

## 2. Local corpus (gitignored, the integrator copies nothing): port/corpus/traces/golfer_events.trace + .json

rec2 of instances\s3_30 (sha256 91e7760d84e19ba1..., 3,167,288 bytes). The boot gate gains a row golfer_events (PASS 2522 / 2522);
the live equivalence gate and framegate's census gain golfer_events too (boot.scenarios() / live equiv --all glob
port/corpus/traces). The pinned reference, schedule and log/p2/p24/golfer_events/c_a captures are unchanged.

## 3. CLAUDE.md paragraph (append after the S3.26 / wave 8 paragraphs)

Roadmap S3.30, golfer_events' injected call (2026-11-01, decision G24, notes log/roadmap/S3/30_notes.md, edits 30_edits.py,
U-0681 / U-1115 resolved, U-1970..U-1975): the trace schema's DELIVER kind 7 `call` (`pos, addr, argc, a0, a1`, argc <= 2) carries
a function the harness called on the main thread from inside a clock call (log/c3/s36a_agent_ext.js action `call`: golfer_events'
landmark notice 0x004722c0 = the port's announceLandmark); trace_rec.js wrote it as kind 0 with no fields, which trace_format
rejected, so golfer_events had no corpus trace. trace_format.py decodes / writes / replays it, trace_rec.js writes it before the
call runs (and reports any other action it has no kind for), port/src/sim/tick.cpp applies it in TraceInput's trace-ordered list
of harness items with the DELIVER w32 writes (the registered port function of the address, mapped through kInjectedCalls by offset
from the frame-head return address; an unknown address stops the run with MissingResource), and the live core records it again
(LiveWriteLog::called). golfer_events re-recorded on instances\s3_30: equal to the p24 trace in every record except that one (kind
0 -> 7), its replay byte-identical, 10 / 10 checkpoints SAME as det_b; boot PASS 2522 / 2522 x64 and Win32; census split 10 / 10
PASS (raw 1 PASS, 9 FAIL with 4-9 terrain-origin one-level px), so the census reads raw 127 / 571 / 0 / 0 and split 695 / 3 / 0 / 0
of 698. Controls tick.frame 20 (call dropped), 21 (last argument + 1), 22 (run at the frame's start) diverge at record 2244, the
first frame head after the call. Lesson: the host's physical cursor resting where the game window opens sends WM_MOUSEMOVE that
no DELIVER explains (U-1970): park it outside the window (log/roadmap/S3/30_work/cursor.py) before a recording that must replay
byte-identical.

## 4. UNCERTAINTIES.md

Resolve (status column -> `resolved`, append to the description):
- U-0681: "Resolved by roadmap S3.30 (2026-11-01): DELIVER kind 7 `call` (pos, addr, argc, a0, a1) in trace_format.py,
  trace_rec.js, the schema, tick.cpp TraceInput and live.cpp; port/corpus/traces/golfer_events.trace recorded on instances\s3_30
  (replay byte-identical, boot PASS)."
- U-1115: "golfer_events' part resolved by roadmap S3.30 (its 10 checkpoints run; cov_a_quit / cov_a_quit_title by S2.5)."

Add the seven rows of log/roadmap/S3/30_uncertain.tsv (U-1970..U-1975) as they are.
