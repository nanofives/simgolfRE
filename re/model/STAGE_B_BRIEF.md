# Phase 3 stage B: shared brief (2026-10-19)

You are one of six parallel children (3.1a, 3.1b, 3.2, 3.3, 3.4, 3.5) porting simulation functions of SimGolf to the C++17
library `portsim` in `port/`. Read first: `port/CORE.md` (conventions: Ptr<T>, wrapping arith, defect switches, ExeConstants,
GameFiles, section 7 x87), `port/test/README.md`, `re/PHASE23_PLAN.md` sections 7-8, `re/model/DECISIONS.md`, and the finished
examples `port/src/sim/{util,core,tiles,text}.cpp` with `port/test/test_core.py`.

## Your function list
Rows of `re/port/simcore_order.tsv` whose `phase3_child` is yours (columns: level, addr, name, hook_source, evidence,
purpose_note, defects). Port leaves first (lowest level). A function whose callee belongs to another child: stub nothing;
call the callee through the registry/shared header once it exists, otherwise port it yourself in YOUR module only if it is a
tiny leaf, and report the overlap. Straddlers (`re/port/scope_straddlers.tsv`) are NOT yours (3.7).

## Rules
- Spec = the hook source (`shim/src/re/*.cpp`, the C3/C4 reimplementation), the purpose note, `re/match/*.cpp` as READING
  reference only, and the corpora. Do not copy literal original addresses; use GameState members via `state_arr/state_obj/...`.
- Bit-exact including original defects behind named `DefectDef` switches (default 1 = original). Faults in the original map to
  the port's Fault/DivideError exactly as CORE.md says.
- Your own files only: `port/src/sim/<module>.cpp` (one or more), your test file `port/test/test_<module>.py`, and ONE line
  per module in `port/src/sim/modules.inc` (append). Shared files (`CLAUDE.md`, `UNCERTAINTIES.md`, `modules.inc`) are append-only;
  re-read before editing, never rewrite. Uncertainty IDs: 3.1a U-0800.., 3.1b U-0820.., 3.2 U-0840.., 3.3 U-0860.., 3.4 U-0880..,
  3.5 U-0900.. . Notes: `log/p3/p3<id>_notes.txt`. One CLAUDE.md paragraph "Phase 3.<id> ..." (dated 2026-10-19 or later) added
  before the "Function bodies are not always contiguous" paragraph.
- Tests: every function needs vectors GREEN (only `status: ok`, `reloc_check` true), a negative control (wrong body RED), and where
  vectors are thin/none/one-sided, a segment-based or state test. Report PER FUNCTION: covered by vectors, segments, or both, and
  its vector coverage verdict. Do not weaken coverage warnings.
- x87 (3.4 especially, others if a function sets a control word): inline `__asm` under `_M_IX86` plus a C body for x64; set
  `FuncDef.fpcw`; verify with `SG_BACKEND=win32 ~/bin/py -3.12 -m pytest port/test -k <name>`. See CORE.md section 7 and
  `port/src/sim/x87probe.cpp`.
- Environment: `~/bin/py -3.12` only. You cannot write the git index (parent commits by explicit path). No game, Frida or Ghidra
  runs; do not edit corpora, hooks.csv, CHANGELOG, or other children's modules. If you need a shared header change, make the
  smallest additive change and report it.
- Before reporting run `~/bin/py -3.12 -m pytest port/test -q` and `-m static`; both must pass (other children's work may be
  in the tree: ignore failures that are clearly theirs and say so).
- Final report under 40 lines: functions ported/total, vectors run/passed, per-function coverage, controls, defects found,
  uncertainties filed, files to commit, anything left or blocked.
