# S10.6 merge list

## Files to take as they are (owned by S10.6)

re/tools/smsg_import.py (ICF holder across smsg files, error rows as `// SMSG-ERROR` comments), tests/test_smsg_import.py (+4
tests), re/match/smsg_sound_*.cpp (28 canonical files, 489 MATCH lines, unchanged set), re/match/wip/smsg_sound_*.cpp (22
comment-only files), log/roadmap/S10/6_* and runs/sound_*.tsv, runs/summary.tsv (rows appended by the re-runs; the S10.5 rows
for the same files stay above them). Evidence CSVs: 489 sound match CSVs in log/diff (all 100.0%, written by
`match.py re/match/smsg_sound_*.cpp`). Superseded: log/roadmap/S10/5_sound_wip/ (S10.5's MATCH-line wip copies).

## 1. The 143 `no Ghidra size` procedures (U-1875): to apply, in this order

a. Append `log/roadmap/S10/6_boundaries_sound.tsv` (143 rows, `sound.dll<TAB>0x<va><TAB><size><TAB><reason>`) to
   re/match/boundaries.tsv. After that match.py scores the 143 and the match tier accepts them as function entries.
b. Create the functions in the Ghidra master (backup first) with the arguments in `log/roadmap/S10/6_setbodies_args.txt`
   (one line, `<hex va>:<size>` pairs, 1,704 characters):
   `ghidra/scripts/SetBodies.java $(cat log/roadmap/S10/6_setbodies_args.txt)` headless on sound.dll, `-process sound.dll
   -noanalysis`; then re-export `re/functions_ghidra_sound.dll.tsv` (ListFunctions.java) and delete `log/xref_sound.dll.json`.
   Three extents are code-only (the PDB size includes the switch tables): 0x100250b0:0x56, 0x100392c0:0x53, 0x1002e2f0:0x62.
c. `~/bin/py -3.12 re/tools/smsg_import.py project sound/sound.dll/sound.dsp` writes the new canonical lines (143 at 100%),
   then `~/bin/py -3.12 re/tools/match.py re/match/smsg_sound_<file>.cpp` for the 22 files that gain lines (evidence), then
   `smsg_import.py inventory sound`. The 143 get names through the C0 -> C2 recipe (re/names row, ApplyNames, batch) before they
   can be promoted; they have no hooks.csv row.
Checked against our disassembly (6_extent.tsv columns last_ins, next_entry_gap, inside_listed, decodes_exact): all 143 end on a
`ret` / `ret N` / tail `jmp` at the extent, none starts inside a listed function, none overlaps another, every one of 143 reads
100.0% with SP5 and with 8168.

## 2. Tracker (orchestrator runs these; I ran none, `re_classify.py check` passed for all 56)

`log/roadmap/S10/6_promote_cmds.txt`: 56 lines `~/bin/py -3.12 scripts/re_classify.py promote 0x<rva> --module sound.dll --to C4
--evidence log/diff/<rva>_sound_<name>.match.csv` for the sound.dll C3 rows now matched at 100%. None of them needs
`// TOOLCHAIN sp5` (all 100.0 with 8168 in the line's trailing comment). The one SP5-only address, 0x10010920
CDSB::get_emulated_write_info, is C2 dead: no step.

## 3. Shared files

- CLAUDE.md "Matching decompilation": `smsg_import.py` writes an ICF-folded address once (the first smsg file by name holds it,
  later files get `// ALSO` lines; smsg_dedupe.py is only for runs made before 2026-10-29), sends scoring errors to wip as
  `// SMSG-ERROR:` comments and writes no canonical file without a new 100% line. Sound.dll: 34 sources, 489 new 100%
  functions in 28 files (+143 once their extents are applied), lpc10.lib's 40 procedures were all matched already.
- UNCERTAINTIES.md: rows U-1875..U-1877 from log/roadmap/S10/6_uncertain.tsv.
