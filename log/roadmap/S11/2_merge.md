# S11.2 merge notes (shared files; the integrator applies them)

The Ghidra master SimGolf.rep was written in this step (golf_clean.exe only, 5 labels removed; backup
ghidra/backup/SimGolf.rep.pre_s11names_20261101 + SimGolf.gpr.pre_s11names_20261101). Nothing below is applied yet.

## 1. ghidra/scripts/ApplyNames.java: replace with log/roadmap/S11/2_ghidra/ApplyNames.java

Two fixes, both measured on the master (2_applynames_dryrun.log against 2_applynames_v2_dryrun.log):

- Function rows are compared with the name the rename would give (scope `<` `>` `*` -> `(` `)` `P`, invalid characters
  of the last part replaced), not only with the raw row text. Before: 32 Terrain.dll rows (`std::list<Tile*>::...`,
  `ifstream::{vbase dtor}`, `operator new`) printed RENAME on every run and were renamed to the name they already had
  (export 2_export_before/Terrain.dll.tsv: `std::list(TileP)::list`, `ifstream::{vbase_dtor}`, `operator_new`, all
  USER_DEFINED). After: renamed=0 on all four programs.
- withdrawn.tsv rows whose reason starts `label <name>` (stream G1, 2026-10-18: 0x004ba800 gRngState, 0x0059d91c
  gTextBuffer2Index, 0x0056d1d8 g_animals, 0x00578372 g_typeAttr, 0x004e6d70 g_recordOccupancy) now remove that
  USER_DEFINED label at the address (UNLABEL line, counted in `reset`) unless a globals row names it there again. Before,
  such a row was skipped (no function at the address), so four of those labels were still the primary symbol in the
  master (2_diff_before.tsv, category withdrawn_label_present); gRngState had already been replaced by the globals row
  f_k_dbl_4ba800.

## 2. re/names/withdrawn.tsv: append one row (exact text, tab-separated)

```
golf_clean.exe	0x0058587a	label g_golferRoster withdrawn 2026-10-08 (round 17), recorded 2026-11-01 (S11.2): field +0x2a (name char[0x22]) of record 0 of g_walkers 0x00585850 (64 x 0x4c, exe_0_globals.tsv); appendWalkerName reads `mov cl, [eax + 0x58587a]` at 0x0046760f with eax = index * 0x4c (0x00467606..0x0046760c, exe_2.tsv)
```

Round 17 removed the row from exe_2_globals.tsv (CLAUDE.md, seventeenth round) but the label stayed in the master and no
withdrawn row recorded it. S11.2 removed the label with ghidra/scripts/DeleteLabels.java 0x0058587a (2_apply_write.log).
With this row, the next ApplyNames run (fixed copy) keeps it removed and 2_compare.py checks it.

## 3. re/functions_ghidra*.tsv: no change

The four read-only ListFunctions re-exports after the write (log/roadmap/S11/2_functions/) equal the committed
re/functions_ghidra.tsv, functions_ghidra_jgld.dll.tsv, functions_ghidra_Terrain.dll.tsv and
functions_ghidra_sound.dll.tsv line for line (CR ignored; 2,934 / 1,602 / 1,076 / 2,358 functions). Labels are not in
those files.

## 4. Decision for the lead: the four `entry` rows

smsg_exe_lib.tsv 0x004a682f _WinMainCRTStartup, smsg_jgld_lib.tsv 0x10082770 _DllMainCRTStartup,
smsg_terrain_lib.tsv 0x10019b80 _DllMainCRTStartup, smsg_sound_lib.tsv 0x100430fe __DllMainCRTStartup: the master
names each `entry` with source IMPORTED (the PE loader's entry-point symbol), and ApplyNames keeps IMPORTED names by its
documented rule (CLAUDE.md "Names and systems": IMPORTED always kept). These are the only rows of 2_diff_after.tsv. To
carry them, either rename the four by hand (one script call, entry point marks unaffected) or let ApplyNames replace an
IMPORTED name when the row's file is a library-identity file; both change the policy, so S11.2 left them.

## 5. CLAUDE.md, after the S10 paragraph "Merged by I16" (one line)

```
S11.2 (2026-11-01, log/roadmap/S11/2_notes.md): the master carried every re/names row except four IMPORTED `entry` names (smsg_*_lib CRT startups, kept by ApplyNames' rule) and five withdrawn labels: ApplyNames skipped withdrawn `label <name>` rows (G1's g_animals, g_typeAttr, g_recordOccupancy, gTextBuffer2Index) and round 17's g_golferRoster had no withdrawn row; all five removed (backup ghidra/backup/SimGolf.rep.pre_s11names_20261101), ApplyNames now applies withdrawn label rows and no longer re-renames the 32 Terrain rows whose names hold `<` `*` or spaces. Check with log/roadmap/S11/2_compare.py over an ExportNames.java export.
```

## 6. Pool slots

simgolf_pool's three slots date from 2026-10-17 (before I16 and this step). `scripts/ghidra_pool.sh sync` refreshes the
unlocked ones from the master; not run here (the pool is not in S11.2's files).
