# S11 wave 1 merge (steps S11.1 .. S11.3): RE upkeep before the port-key renames

Base: HEAD d075dc56 (2026-11-01). Nothing here touches port/, shim/ or a gate. The Ghidra master was already written by S11.2
(golf_clean.exe, 5 labels removed; backup ghidra/backup/SimGolf.rep.pre_s11names_20261101 + SimGolf.gpr.pre_s11names_20261101).
Step notes: log/roadmap/S11/1_notes.md, 2_notes.md, 3_notes.md; the step merge files 1_merge.md, 2_merge.md, 3_merge.md hold the
evidence behind each item below. Apply in this order.

## 1. UNCERTAINTIES.md

1a. U-1351 (S3's row, addr 0x0045baf0): status cell `open` -> `resolved`, append to its description:
` Resolved S11.1 (2026-11-01): the original's 18 call sites each pass a constant table: 16 [g_halfBrightLut], blitPanel's two
[g_color16Lut] (log/roadmap/S11/1_shade_sites.log); the port's drop sites match them site for site. The blind control (block 1 always)
remains a limit of the opaque-pixel frame gate, not a question about the original.`
(Why: U-1351's addr cell is gameMain's, and re_classify's gate refuses gameMain's C4 on it; its resolution column asked for the call
sites' table per site, which S11.1 recorded from the binary. 1_merge.md section 3.)

1b. Append the rows of log/roadmap/S11/1_uncertain.tsv (U-2000, U-2001) as table rows, and the rows of 3_uncertain.tsv if it exists.

## 2. Evidence file for sound.dll strcpy (log/diff is gitignored: run on the machine that promotes)

```
cp log/roadmap/S11/1_strcpy_lib.csv log/diff/0004a010_sound__strcpy.match.csv
```
(SP5 LIBCMT.LIB strcat.obj `_strcpy`, 0xf0 bytes, 0 relocations, 15/15 rows equal; library-object identity, D4 accepted at I16.)

## 3. Promotions (after 1 and 2)

```
~/bin/py -3.12 scripts/re_classify.py promote 0x0045baf0 --to C4 --evidence log/diff/0045baf0__WinMain_16.match.csv
~/bin/py -3.12 scripts/re_classify.py promote 0x0004a010 --to C4 --module sound.dll --evidence log/diff/0004a010_sound__strcpy.match.csv
```
Expected: C4 2,928 -> 2,930, C3 5 -> 3. Then `~/bin/py -3.12 -m pytest tests/test_changelog_integrity.py tests/test_classify.py -q`
and `~/bin/py -3.12 log/c3/hook_integrity.py`.

## 4. re/c3_hold.tsv: three lines (tab-separated; held at C3, no count changes)

The drawAmenitiesPanel line of 1_merge.md section 5 and the two fillSpan lines of 1_merge.md section 5b.
fillSpanDown / fillSpanUp are held by the lead pending Mariano's ruling on U-2000 (an all-asm body inside a compiled C function: SMSG
PolyFill.cpp, 100% 41/41 and 40/40). Their GREEN CSVs are parked in log/roadmap/S11/1_ev/, NOT in log/diff. If Mariano accepts: copy
both into log/diff, apply 1_merge.md section 2 (two MATCH lines in re/match/smsg_golf_jackal_PolyFill.cpp), run the two fillSpan
promote lines of 1_merge.md section 4, drop the two hold lines.

## 5. Ghidra names tooling (S11.2)

5a. Replace ghidra/scripts/ApplyNames.java with log/roadmap/S11/2_ghidra/ApplyNames.java (withdrawn `label <name>` rows now remove
the label; function rows compared by mapped name: renamed=0 on all four programs). 2_merge.md item 1.
5b. Append to re/names/withdrawn.tsv the g_golferRoster 0x0058587a row of 2_merge.md item 2 (exact text there).
5c. re/functions_ghidra*.tsv: no change (re-exports equal the committed files).
5d. The four IMPORTED `entry` rows (smsg_*_lib.tsv CRT startups) stay as the gate's listed exception (lead ruling: the master keeps
the loader's symbol; ApplyNames' IMPORTED rule unchanged).
5e. 2_merge.md item 5's CLAUDE.md line is folded into section 7 below; do not add it separately.

## 6. Dead / trivial marks (S11.3)

Run the eleven `re_classify.py mark` lines of log/roadmap/S11/3_merge.md as written (188 dead + 32 trivial = 220 rows: exe 2 + 21,
jgld 58 + 3, sound 27 + 4, Terrain 101 + 4). `mark` re-checks endstate.py per row and refuses a failing one; the four PE entry
points endstate.py passes as dead are excluded (U-2020). Expected after: end states 283 -> 503, C2 still open 1,881 -> 1,661.
The tool fix U-2020 asks for (an AddressOfEntryPoint / TLS-callback check in re/tools/endstate.py dead_checks) is a shared-tool
edit for a later step, not part of this merge.

## 7. CLAUDE.md paragraph (after the S10 "Merged by I16" / wave 2 paragraphs)

```
Section S11, RE upkeep wave 1 (2026-11-01, G23; steps re/roadmap/S11_steps.tsv, notes log/roadmap/S11/<k>_notes.md, merge
log/roadmap/S11/MERGE.md, U-2000..U-2023). S11.1, the five C3 rows: gameMain 0x0045baf0 reaches C4 by SMSG GameMain.cpp (100%
320/320, no inline asm) once U-1351 is resolved (the original's 18 Sprite::drawG-wrapper call sites each pass a constant table, 16
g_halfBrightLut and blitPanel's two g_color16Lut, log/roadmap/S11/1_shade_sites.py; U-1351 had held gameMain only through its addr
cell); sound strcpy 0x1004a010 by library-object identity with SP5 LIBCMT.LIB strcat.obj `_strcpy` (0xf0 bytes, 0 relocations,
15 / 15 rows, 1_strcpy_lib.py; S10.7's CSV had been dropped at I16 only for its extent proposal). Held at C3 (re/c3_hold.tsv):
drawAmenitiesPanel 0x00434cf0 (SMSG Toolbar.cpp 99.9% under 8168, one reload pair after the second tooltip call 0x00432620 in the
other order, 100% only under SP5's front end, U-1910 / U-2001; route: scenario_ab.py over cov_a_build / cov_k3_build) and
fillSpanDown 0x00493000 / fillSpanUp 0x00493080, which SMSG PolyFill.cpp matches 100% (41/41, 40/40) as compiled C functions whose
whole body is one inline __asm block: whether that counts under the round-36 inline-asm rule is Mariano's (U-2000; CSVs parked in
log/roadmap/S11/1_ev/, not log/diff). S11.2: the Ghidra master already carried every re/names row (5,222 function rows, 1,091
globals rows) except the four IMPORTED `entry` names of the smsg_*_lib CRT startups (kept: the loader's symbol) and five withdrawn
labels: ApplyNames skipped withdrawn `label <name>` rows at data addresses (G1's g_animals, g_typeAttr, g_recordOccupancy,
gTextBuffer2Index) and round 17's g_golferRoster 0x0058587a had no withdrawn row; all five were removed (backup
ghidra/backup/SimGolf.rep.pre_s11names_20261101). ApplyNames now applies withdrawn label rows and compares function rows by the
mapped name (the 32 Terrain rows with `<`, `*` or spaces were re-renamed to the same name on every run). Check:
log/roadmap/S11/2_compare.py over an ExportNames.java export (log/roadmap/S11/2_ghidra/). S11.3: re/roadmap/S11_c2_triage.tsv
holds every open C2 row (1,881: exe 500, jgld 337, sound 561, Terrain 483) with endstate verdict, callers / import callers /
pointer installers, reach (called 1,015, pointer-installed 657, pointer-no-installer 14, no caller 195), port module and 100% MATCH
line (exe 295, sound 289, jgld 22, Terrain 0); generator log/roadmap/S11/3_triage.py. 220 rows passed endstate.py and were marked
(dead 188, trivial 32), open C2 1,881 -> 1,661. endstate.py passes the four PE entry points as dead (U-2020; excluded by hand). 280
of the exe's 296 pointer-installed open rows already have a 100% match but no C3 (vtable / callback bodies): the largest group left.
```

## 8. Gate commands and expected numbers

```
~/bin/py -3.12 scripts/re_classify.py status --summary
   # C4 2,930 (exe 1,639, sound 607), C3 3 (exe 3, sound 0), C1 11, C2 open 1,661, held 113, end states 503
~/bin/py -3.12 -m pytest tests/test_classify.py tests/test_changelog_integrity.py -q
~/bin/py -3.12 log/c3/hook_integrity.py
~/bin/py -3.12 -m pytest -m static -q                      # expected 811 passed / 2 skipped (I21's count) or the integrator's current count
~/bin/py -3.12 log/roadmap/S11/3_triage.py                 # re-run after the marks: 1,661 rows, 0 rows with endstate dead / trivial left except the 4 entry points
```
The segment and boot gates are untouched by this wave (no port/ or shim/ file changes).
