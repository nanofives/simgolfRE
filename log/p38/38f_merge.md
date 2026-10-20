# 38f merge lines (integrator)

## 1. port/src/sim/modules.inc
Append one line (the test-only batch-override module, port/src/sim/integrator.cpp + port/include/portsim/sim/integrator.h):

```
SG_MODULE(integrator)
```

## 2. port/src/sim/tick.cpp (TraceInput: the batch-override hook for gate d)
`git apply log/p38/38f_tick.patch` (made against the working tree after 38e's tick_cpp.patch; `git apply --check` passed on
2026-10-20). The two added lines, if you prefer to edit by hand:

- after `#include "portsim/sim/commands.h"` add `#include "portsim/sim/integrator.h"`
- in `TraceInput::batch`, after `    out = b.cmds;` add
  `    if (const std::vector<Command> *rep = batch_override(ctx_, b.pos)) out = *rep;  // phase 3.8 gate (d): a replacement`

Without an override table (every run except port/test/semantic.py's) `batch_override` returns nullptr: behaviour unchanged
(the 190-segment gate on HEAD gave identical rows and digests without the hook, log/p38/38f/segments_x64_head_nohook.tsv,
and with it, log/p38/38f/segments_x64_head.tsv: `integrator compare` 190 rows, 0 differ).

## 3. UNCERTAINTIES.md
Rows U-1010 .. U-1014 from log/p38/38f_uncertain.tsv. U-1013 resolves U-0650's open question for course_season (the
caller is mainLoop 0x0041e283); suggest appending "resolved for course_season by U-1013" to U-0650's path column.

## 4. Shared files I edited directly (not on the brief's do-not-edit list; small, additive)
- re/frida/scenario_ab.py: `sc_course_season_6y` (course_season with season_s=5000, every_s=500) and its SCENARIOS entry.
- re/tools/segment_corpus.py: `max_segments` allows 10 segments for course_season_6y as for course_season.
- re/analysis/data/scenario_refs.json: the course_season_6y entry, written by `scenario_refs.py regen` (the tool's own manifest
  write; 49 lines added, "updated" changed).
- re/port/segments_manifest.tsv and port/corpus/segments.json: the 10 course_season_6y rows (all accepted), written by
  `segment_corpus.py extract` (the diff adds those 10 rows only).
- port/test/test_static.py `test_manifests_readable`: accepted count 190 -> 200 (with a comment), because of those rows.
- re/tools/trace_format.py: a slice whose first QPC value needs more than 32 bits keeps the low 32 bits in the header field
  and the whole value in meta `q0_64`; parse() reads it back (course_season_6y's later segments start past 2^32: plan
  failed with struct.error before). Traces with a 32-bit q0 are byte-identical to before; tests/test_trace_format.py and
  tests/test_segment_corpus.py pass (28). re/model/trace_schema.md and its copy port/schemas/trace.md: the q0 row says so.
- re/port/divergences.tsv: new, written by `~/bin/py -3.12 -m port.test.integrator ledger` (inputs in log/p38/38f/; `--run
  --dll <dll>` regenerates them).

## 5. CLAUDE.md (suggested paragraph, port section after "Phase 3.7a input")
See log/p38/38f_notes.md section "CLAUDE.md paragraph".

## 6. Optional (not needed by my files)
port/test/lib32.py always runs the shared port/build/Win32 DLL and runner and rebuilds them when stale; a child cannot point it
at its own Win32 build. port/test/integrator.py works around it in-process (`load_lib` with --dll32 / --runner32 / --build).
An env override (`SG_PORTSIM_DLL32`, `SG_RUNNER32`) in lib32.py would let `SG_BACKEND=win32` test runs use a private build.
