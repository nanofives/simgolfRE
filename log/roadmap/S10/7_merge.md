# S10.7 merge items (for the S10 lead / orchestrator)

S10.7 wrote no tracker, Ghidra, shared or log/diff file. Its files are listed in 7_files.txt. Run commands from the repo root.

## 1. New names files (ready to commit)
| id | rows | systems writeup |
|---|---|---|
| re/names/smsg_exe_lib.tsv | see 7_notes.md section 3 | re/analysis/systems/smsg_exe_lib.md |
| re/names/smsg_jgld_lib.tsv | | re/analysis/systems/smsg_jgld_lib.md |
| re/names/smsg_sound_lib.tsv | | re/analysis/systems/smsg_sound_lib.md |
| re/names/smsg_terrain_lib.tsv | | re/analysis/systems/smsg_terrain_lib.md |

No `_globals.tsv` (names_validate prints `missing`, not an error). Gate, each passing on 2026-10-29:
```
~/bin/py -3.12 re/tools/names_sanitize.py re/names/smsg_{exe,jgld,sound,terrain}_lib.tsv re/analysis/systems/smsg_{exe,jgld,sound,terrain}_lib.md   # 0 replaced
~/bin/py -3.12 re/tools/names_validate.py smsg_exe_lib smsg_jgld_lib smsg_sound_lib smsg_terrain_lib   # bad 0, cross-file duplicates 0
~/bin/py -3.12 re/tools/names_check_evidence.py re/names/smsg_{exe,jgld,sound,terrain}_lib.tsv   # 0 citations checked, 0 bad
```
Regenerate: `~/bin/py -3.12 log/roadmap/S10/7_tools/build_rows.py` then `write_md.py` (inputs 7_score_<key>.tsv, 7_gaps_<key>.tsv).

## 2. Tracker and master (CLAUDE.md "Callers named to unblock held rows" recipe)
1. Back up the master. ApplyNames WITHOUT `overwrite` (21 rows target `FUN_`, the rest are already named by FID):
   `<ghidra>\support\analyzeHeadless.bat C:\Users\maria\Desktop\Proyectos\SimGolf SimGolf -process <module> -noanalysis -scriptPath ghidra\scripts -postScript ApplyNames.java re\names`
   for golf_clean.exe, jgld.dll, sound.dll, Terrain.dll.
2. Re-export each function list (ListFunctions), delete log/xref_<module>.json.
3. `~/bin/py -3.12 scripts/re_classify.py batch re/names/smsg_<id>.tsv --to C1`, then `~/bin/py -3.12 re/tools/c2_note.py --all --out log/c2_batch.tsv` and `batch log/c2_batch.tsv --to C2`.

## 3. C4 by library identity (a)
`log/roadmap/S10/7_promote_cmds.txt`: the copy step into log/diff first (the gate reads log/diff), then seven `re_classify.py
promote ... --to C4 --evidence <csv>` lines. Four rows pass the gate on this evidence alone (exe `__ArrayUnwind` 0x004a5793, sound
0x4288e, 0x45680, 0x48268); sound `__ftol`, `strstr`, `strcpy` also pass on their old naked-transcription CSV (the round-36
decision demoted them: the identity CSV replaces it). Two lines rest on byte identity (U-1881): sound 0x48268 and 0x4a010; drop
them if the lead does not count it. Rows identical but still at C1 / C2 (61) are in 7_c2_identical.tsv: they need C3 first.

## 4. Boundaries (c)
`log/roadmap/S10/7_boundaries.tsv` (header line first; append the rest to re/match/boundaries.tsv): the rows whose 100% needed an
extent other than our list's. 20 further rows exist already in S10.9's 9_boundaries.tsv (7_boundaries_have.tsv). Entry labels
that no function of ours starts at (107) get no boundaries row. Check afterwards (U-1884): `~/bin/py -3.12 re/tools/match.py
re/match/m36c_crt_asm.cpp --no-evidence`.

## 5. FidDb (re/tools/build_fiddb.py)
Not run and not needed: see 7_notes.md section 6. The lib list in build_fiddb.py is unchanged.
