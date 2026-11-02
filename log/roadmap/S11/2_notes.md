# S11.2 the Ghidra master carries every re/names row (2026-11-01, DONE)

## Result

- Rows checked: 5,222 function rows (5,222 addresses), 1,091 globals rows, 15 withdrawn rows, from the 104 tracked
  re/names files at HEAD d075dc56 (`git archive HEAD re/names` into 2_names_apply/; no untracked file in re/names, the
  working copies equal HEAD apart from line endings).
- Before (2_diff_before.tsv, 8 rows): 4 IMPORTED `entry` names where smsg_*_lib.tsv name the CRT startup, and 4 labels
  withdrawn.tsv withdraws (G1, 2026-10-18) still the primary symbol: gTextBuffer2Index 0x0059d91c, g_animals 0x0056d1d8,
  g_typeAttr 0x00578372, g_recordOccupancy 0x004e6d70. Every function and globals row was already applied (I16 and
  earlier runs).
- Write (golf_clean.exe only; 2_apply_write.log): ApplyNames (fixed copy, overwrite) UNLABEL x4, renamed 0, labeled 0;
  DeleteLabels.java 0x0058587a removed g_golferRoster (round 17 removed its globals row; it is field +0x2a of g_walkers
  record 0, appendWalkerName 0x0046760f; proposed withdrawn row in 2_merge.md item 2). The exe export differs from the
  one before in exactly those 5 SYM lines; jgld / Terrain / sound exports are byte-identical before and after.
- After (2_diff_after.tsv, 4 rows): only the four `entry` rows, reason in the file (IMPORTED kept by ApplyNames' rule;
  decision in 2_merge.md item 4).
- Master names no row backs (2_unbacked.log, 2_unbacked_user.tsv): 0 USER_DEFINED function names in any module; the 317
  USER_DEFINED exe labels left are b1's jump-table labels (`<fn>::override::jmp_<addr>::switch` / `case_<n>`). ANALYSIS
  / IMPORTED names (FID, loader) are not rows by design.
- Function lists: the read-only ListFunctions re-exports (2_functions/) equal the committed re/functions_ghidra*.tsv;
  no shared-copy update needed (2_merge.md item 3).

## Why the gap existed

ghidra/scripts/ApplyNames.java handles withdrawn.tsv rows only through `getFunctionAt(addr)`; G1's `label <name>
withdrawn` rows sit at data addresses, so every run skipped them silently. The fixed copy
(2_ghidra/ApplyNames.java) removes such a USER_DEFINED label unless a globals row names it again, and compares function
rows with the mapped name (the 32 Terrain rows with `<`, `*` or spaces were re-renamed to the same name on every run).
Dry runs: 2_applynames_dryrun.log (committed script: 32 no-op RENAMEs on Terrain, 4 KEEPs), 2_applynames_fixed_dryrun.log
(mapped-name fix: renamed 0), 2_applynames_v2_dryrun.log (both fixes: exactly 4 UNLABEL on the exe).

## Reproduce

```
pwsh: .\log\roadmap\S11\2_gh.ps1 -Mod <program> -Script ExportNames.java -ReadOnly -ScriptArgs @('<abs path>\<program>.tsv')
py -3.12 log\roadmap\S11\2_compare.py log\roadmap\S11\2_names_apply log\roadmap\S11\2_export_after <out.tsv>
py -3.12 log\roadmap\S11\2_unbacked.py log\roadmap\S11\2_names_apply log\roadmap\S11\2_export_after <out.tsv>
py -3.12 log\roadmap\S11\2_control.py log\roadmap\S11\2_names_apply log\roadmap\S11\2_export_after <scratch dir>
```

2_gh.ps1 runs analyzeHeadless on the master (`-process <program> -noanalysis`), script path one directory only (the
.bat splits a `;` list). Call it with `&` in a PowerShell session: `pwsh -File` passes `-ScriptArgs a,b` as one string.
ExportNames.java (2_ghidra/) writes FN rows (every function, getName(true), source) and SYM rows (every non-default
LABEL with its primary flag).

## Controls

2_control.py plants five kinds of difference in a copy of the names files (a function renamed, a global renamed, a
withdrawn row for a named jgld function, a row at an address with no function, a withdrawn label still in the master)
plus one that must not be flagged (a withdrawn label a globals row names again): all five found, the sixth not (RED,
2_control.log). 0 blind.

## Ghidra hygiene

No java process and no lock file on SimGolf.rep before the write; backup ghidra/backup/SimGolf.rep.pre_s11names_20261101
(20 files, 128.7 MB, robocopy /E) and SimGolf.gpr.pre_s11names_20261101 taken before it. The pool slots were not synced
(2_merge.md item 6).

## Open

- The four `entry` rows (policy decision, 2_merge.md item 4).
- 2_merge.md items 1, 2, 5 (ApplyNames replacement, withdrawn row for g_golferRoster, CLAUDE.md line) for the integrator.
- U-rows: none (nothing unclear; U-2010..U-2019 unused).
