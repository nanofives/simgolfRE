# S10: import and verify SMSG-decomp's work (2026-10-29)

Mariano's decision, 2026-10-29: copy everything from github.com/SMSG-decomp and accept the licence issues. The snapshot is in
third_party/smsg/ (PROVENANCE.md). Local, gitignored toolchains sit in tools/vc6sp5 (VC6 SP5), tools/vc6sp2 (SP2) and tools/dxsdk8.

SMSG-decomp is an independent matching decompilation of the same v1.03. It claims all 5,982 functions at 100%, but no rebuilt binary is
byte-exact. Our investigation of it, from a subagent on 2026-10-29, is summarised in the auto-memory note smsg-decomp-external-project.md.
Its tables, built from the SMSG data, are in that session's scratchpad. Rebuild what you need from third_party/smsg.

## Rule

Nothing of theirs enters our trackers on their word alone (NO-GUESSING, re/CONFIDENCE.md).
- A source counts when our re/tools/match.py compiles it to 100% against OUR anchored binaries. That is C4 match evidence, promoted by
  named CSVs as re/C4_PLAN.md says.
- A name or field meaning counts when our disassembly supports it. Each one needs a cited instruction address in re/names or the
  model.
- Their PDBs go only into a scratch Ghidra pool clone, never into the master.

## Goals, in value order

1. **Matched sources.** Convert their per-module sources into re/match files with `// MATCH: <module> <addr> <decorated name>` and
   `// FLAGS` lines, then run match.py over every function. Keep each 100% result as a canonical source. Put each miss in
   re/match/wip/ with the score. Report the counts per module: new 100% matches against our current 2,265, and C3 rows that become C4
   candidates. Their flags are in third_party/smsg/SMSG-decomp/smsg.toml and the .dsp files.
2. **The toolchain selector.** sound.dll's C++ objects were built by SP5's C2 12.00.8966, which we only now have. Add a
   per-file toolchain line to match.py (for example `// TOOLCHAIN sp5`, default 8168). Re-run the 116 sound.dll near-misses in
   re/match/wip/ with it.
3. **Library identity.** Add SP2's debug CRT and SP5's CRT to the FidDb (re/tools/build_fiddb.py). Report how many of the 264 unnamed
   jgld / Terrain library functions it names.
4. **Names.** Diff their PDB names and types against re/names, hooks.csv and our function lists. Propose rows only where our
   placeholders (FUN_ or generic names) get a better name supported by our disassembly. Also cover:
   - the five contradictions the investigation found: 0x004038f0, 0x00442180, 0x0040e720, 0x0042dea0, 0x00422530;
   - the golfer fields U-0009..U-0016;
   - whether the "animal" records at 0x0056d1b8 are VIP homes.
   Output goes to re/names/smsg_*.tsv with systems writeups, and must pass names_validate, names_check_evidence and names_sanitize.
5. **Their 535 sound.dll and 57 jgld names** where we have none, under the same evidence rule.

## Limits

- No port/, shim/ or original\ change.
- No C3 / C4 hook change.
- No Ghidra master write. Names reach the master through the orchestrator's ApplyNames run after the merge.
- Tracker writes (re_classify) are listed in MERGE.md for the orchestrator to run.
- match.py runs are light, but keep at most 4 compile jobs in parallel. The I15 merge is running its gates until about 20:00.

## Gate

- Every new re/match file's MATCH lines verify at 100% with match.py.
- The match test tier passes (`py -3.12 -m pytest -m match`), and so does the static tier.
- Every names file passes the three names tools.
- MERGE.md lists the C4 promotions by named CSV and the names rows, with the counts.
