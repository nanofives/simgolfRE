# Phase 1 stream brief (shared by streams B, C, D, E1, E2, F; written 2026-10-17)

You are one stream of phase 1 of the portable reimplementation (re/PORT_PLAN.md, re/PHASE1_PLAN.md, both approved).
Goal of the phase: a typed, evidence-backed model of every piece of game state and constant data the game logic touches,
so the port is written against structs and not addresses. You own a disjoint set of data clusters.

## Read first
CLAUDE.md (PORT PIVOT, b1, golfer record, c3fy paragraphs), re/PHASE1_PLAN.md, re/analysis/data/model_inventory.md (stream A's
method and validation: its error rate is 7.5% on known types and its element COUNTS are weak, 10 of 16 right), re/model/ownership.tsv
(your rows: column `stream`; E1/E2 rows are in re/model/ownership_e.tsv), re/model/inventory_clusters.tsv and
inventory_addresses.tsv (per-address reads/writes, widths, strides, function lists), re/model/vtables.tsv,
re/analysis/data/golfer_record.md and record_tables.md (the format of finished work), re/analysis/save/0040afa0_save_serializer.md
and re/tools/sve.py (the 86 save globals), re/tools/model_check.py and re/tools/record_fields.py, re/names/*_globals.tsv,
UNCERTAINTIES.md. Function bodies are in the repaired index (re/functions_ghidra.tsv, ranges column); decompile with
`~/bin/py -3.12 re/tools/decomp.py 0x<addr>` when the disassembly needs it (read-only, uses a pool slot).

## Decisions already taken (do not reopen)
1. Constants stay in the user's own exe; the model records DESCRIPTORS (name, element type, count, stride, file offset in the
   user's golf_clean.exe) and NEVER copies table bytes, strings or sprite data into the repo. Quote no binary text in notes
   (names_sanitize rewrites quoted strings).
2. The header is clean C99 with fixed-width types and `static_assert` sizes, NO original addresses inside it. Addresses live
   in the stream's address map.
3. Original defects (out-of-bounds, uninitialised reads) are catalogued, not "fixed" in the model.

## Deliverables (replace `S` with your stream id: b, c, d, e1, e2, f)
- `re/model/include/sections/S.h`: structs, enums, constants for your clusters (guarded, self-contained, `static_assert(sizeof...)`;
  field names in snake_case with offset comments; unknown bytes as `uint8_t unk_XX[n]` with a U-number).
- `re/model/S_addresses.tsv`: symbol, module, va, size, element_type, count, stride, signed, sve_field (86-global index or -),
  cluster ids, evidence (cited instruction addresses), confidence (high / medium / low).
- `re/names/model_S_globals.tsv` in the standard globals format (`module addr name type evidence`, names unique across all
  names files; run names_sanitize.py, names_validate.py model_S, names_check_evidence.py). Do not run ApplyNames and do
  not touch the Ghidra master: stream G applies names.
- `re/analysis/data/model_S.md`: what you typed, per cluster group with the evidence, corrections to stream A's classes,
  corrections to older notes, open ranges.
- `re/model/S_uncertain.tsv`: id (start at the prefix your brief gives), address, kind, what is unknown, what would decide it.
  Do not edit UNCERTAINTIES.md (stream G merges).
- Every typed range is checked with `re/tools/model_check.py` (static, all accesses of all functions): 0 errors, or each error
  explained in model_S.md (wrong type fixed, or an access the original makes out of bounds, already a defect).
- Hook cross-check: scan shim/src/re/*.cpp for literal addresses inside your ranges (the hooks read them with widths) and
  confirm width and stride agree with your types; list disagreements.
- Names: only with evidence (instruction addresses). Banned hedging words in names and notes: probably, likely, seems,
  appears, I think, presumably, might be, maybe. Unknown -> a U row, not a guess.
- Coverage table at the end of model_S.md: clusters done / total, addresses done / total, weighted by referencing-function
  count, and the reasons for any cluster left untyped. Clusters that belong to another stream: list them for the parent, do
  not type them.

## Rules
- Write only your own files (names above) plus scratch under log/p1/S/. Other streams run in parallel on other clusters:
  do not edit their files, `hooks.csv`, `re/analysis/CHANGELOG.md`, `re/functions_ghidra*.tsv`, shim sources or the Ghidra master.
- Python: `~/bin/py -3.12` (bare `py` is refused on this account). You cannot write the git index: do NOT git add or commit; the
  parent commits your files by explicit path. Do not use `git checkout -- <dir>` or any command that reverts shared files.
- Game runs are optional (validation of a type against live memory): only on your own instance
  (`powershell -NoProfile -File shim/build_instance.ps1 -Id p1S`), at most one game, respect the boot lock, never score a run
  that died before writing a comparison, no deploy to original\. Static evidence is the primary route.
- The save fixtures are tests/fixtures/stories/*.sve (3 files, untracked) and the saves the pinned scenarios write; use
  re/tools/sve.py to read fields.
- Final report: what was typed (counts), checker result, hook cross-check result, new names, the untyped remainder with reasons,
  surprises about older notes, files written. Keep the report under 150 lines.
