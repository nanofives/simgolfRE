# S10.9 merge items (for the S10 lead / orchestrator)

S10.9 wrote no tracker, Ghidra or shared file. Its new files are listed in 9_files.txt.

## 1. New names files, ready to commit
| id | rows | systems writeup |
|---|---|---|
| re/names/smsg_sound_1.tsv | 268 | re/analysis/systems/smsg_sound_1.md |
| re/names/smsg_jgld_1.tsv | 22 | re/analysis/systems/smsg_jgld_1.md |
| re/names/smsg_jgld_crt.tsv | 54 | re/analysis/systems/smsg_jgld_crt.md |
| re/names/smsg_terrain_crt.tsv | 60 | re/analysis/systems/smsg_terrain_crt.md |

There is no `_globals.tsv`: names_validate reports `missing` for it, which is not an error.

Gate as run on 2026-10-29. Each command passes:
```
~/bin/py -3.12 re/tools/names_sanitize.py re/names/smsg_{sound_1,jgld_1,jgld_crt,terrain_crt}.tsv re/analysis/systems/smsg_*.md   # 0 replaced
~/bin/py -3.12 re/tools/names_validate.py smsg_sound_1 smsg_jgld_1 smsg_jgld_crt smsg_terrain_crt   # bad 0, cross-file duplicates 0
~/bin/py -3.12 re/tools/names_check_evidence.py re/names/smsg_{sound_1,jgld_1,jgld_crt,terrain_crt}.tsv   # 20 checked, 0 bad
~/bin/py -3.12 log/roadmap/S10/9_tools/check_cites.py re/names/smsg_{sound_1,jgld_1,jgld_crt,terrain_crt}.tsv   # 414 in-body addresses, 0 bad
```

## 2. Tracker and master, in this order (CLAUDE.md "Callers named to unblock held rows" recipe)
1. Back up the master, then apply the names to each DLL. Use ApplyNames without `overwrite`: every target is `FUN_`.
   ```
   <ghidra>\support\analyzeHeadless.bat C:\Users\maria\Desktop\Proyectos\SimGolf SimGolf -process sound.dll -noanalysis -scriptPath ghidra\scripts -postScript ApplyNames.java re\names
   ```
   Repeat with `-process jgld.dll` and `-process Terrain.dll`.
2. Re-export each DLL's function list with ListFunctions, then delete log/xref_<module>.json.
3. Run these:
   ```
   ~/bin/py -3.12 scripts/re_classify.py batch re/names/smsg_sound_1.tsv re/names/smsg_jgld_1.tsv re/names/smsg_jgld_crt.tsv re/names/smsg_terrain_crt.tsv --to C1
   ~/bin/py -3.12 re/tools/c2_note.py --all --out log/c2_batch_s10_9.tsv
   ~/bin/py -3.12 scripts/re_classify.py batch log/c2_batch_s10_9.tsv --to C2
   ```
   c2_note skips functions of 5 bytes or less, so those rows stay at C1.

## 3. Boundaries rows for re/match/boundaries.tsv
Ten jgld rows and ten Terrain rows, exact text in log/roadmap/S10/9_boundaries.tsv (module, entry, size, reason).
- Each one is a debug-heap CRT routine whose body runs past our extent. The libcmtd.lib member is instruction-identical
  over SMSG's size.
- The reason column lists the false function starts inside each extent: the 11-byte pieces that b1 did not remove.
- In the master, those starts go with `ghidra/scripts/DeleteFunctions.java`. Then `SetBodies.java <entry>:<size>` gives
  each routine its full body.

## 4. Renames of existing rows (sound.dll)
19 placeholder names, all constructors and destructors, listed in log/roadmap/S10/9_renames_sound.tsv (file, addr, old,
new, evidence). Examples: `Ch5be88_ctor` -> `Midi::ctor`, `Sound5b6a8_dtor` -> `Seq::dtor`.
- Rule: SMSG's PDB names the address a ctor / dtor of class C, and our body stores the vtable that SMSG's public names
  for C.
- Apply: edit each row's name in its file, then run `re_classify.py retag <file>` and ApplyNames `overwrite` on sound.dll.
- No jgld or Terrain rows qualified.

## 5. Deferred: SMSG functions that are not functions in our master
145 rows (sound 143, jgld 1, Terrain 1), listed in log/roadmap/S10/9_deferred_not_functions.tsv.
- Each needs `CreateFunctions.java` and an extent check before a names row can be checked.
- b1 says no reachable code lies outside a function. These are unreferenced bodies behind thunks nothing calls.

## 6. Uncertainties
U-1890 .. U-1894 for UNCERTAINTIES.md, in log/roadmap/S10/9_uncertain.tsv.
