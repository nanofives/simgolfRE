# S10.1 merge list

## Files to take as they are (owned by S10.1)

re/tools/smsg_pdb.py, re/tools/smsg_import.py, re/tools/match.py (two directives, ownership passes to S10.5 on acceptance),
tests/test_smsg_import.py, re/match/smsg_golf_Geometry.cpp, re/match/smsg_golf_InputPoll.cpp, re/match/smsg_golf_Plot.cpp,
log/roadmap/S10/1_* and log/roadmap/S10/runs/*, log/roadmap/S10/inventory_{golf,jgld,sound,Terrain}.tsv.

## Tracker (orchestrator runs these; I ran none)

Evidence was written for the three canonical files only (`~/bin/py -3.12 re/tools/match.py re/match/smsg_golf_<f>.cpp`, 22
CSVs, all VERDICT GREEN; log/diff/*.match.csv is gitignored, so regenerate them the same way before promoting). Two of the 22
addresses are C3 rows that this lifts to C4:

```
~/bin/py -3.12 scripts/re_classify.py promote 0x004012d0 --to C4 --evidence log/diff/004012d0__RenderPlot__YAXHHH_Z.match.csv
~/bin/py -3.12 scripts/re_classify.py promote 0x00467600 --to C4 --evidence log/diff/00467600__BuildEmployeeName__YAXH_Z.match.csv
```

(drawRecords 0x004012d0 = their RenderPlot, 360/360 instructions; appendWalkerName 0x00467600 = their BuildEmployeeName,
80/80.) Of the other 20: 12 are already C4 (by scenario evidence; a GREEN match CSV now backs them too) and 8 are C2
(0x00402180 return0Pop4_402180, 0x00402190 retPop4_402190, six scalar deleting destructors 0x00402720..0x004028f0): a match is
not a C2 -> C3 step (U-1853), so no tracker action.

## Shared files

- CLAUDE.md "Matching decompilation": one sentence after the `// FLAGS <module>: ...` paragraph:
  `match.py also reads `// INCLUDE <dir>` lines (repo-relative, search order; `// INCLUDE $VC` places VC6's include
  directory, else it comes last) and `// COMPILE <path>` (cl compiles that file in place of the annotated one: under /ZI
  an #included file's __LINE__ is a constant, not the per-function line variable). re/tools/smsg_import.py writes such
  annotation-only wrappers for SMSG-decomp sources (log/roadmap/S10/1_notes.md).`
- UNCERTAINTIES.md: rows U-1850..U-1854 from log/roadmap/S10/1_uncertain.tsv.

Note: log/ is gitignored (.gitignore line 22 `log/*`); add the log/roadmap/S10 files of 1_files.txt with `git add -f`.
