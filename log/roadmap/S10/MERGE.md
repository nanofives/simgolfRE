# S10 merge request: SMSG-decomp import and verification (2026-10-29)

Lead: S10 section lead. Steps re/roadmap/S10_steps.tsv, status log/roadmap/S10/STATUS.md, files log/roadmap/S10/files.txt.
Every number below was measured with OUR tools against OUR anchored binaries. Nothing here was written into a tracker,
the Ghidra master, port/, shim/ or original\. log/ is gitignored (.gitignore `log/*`): add the log/roadmap/S10 files of
files.txt with `git add -f`. log/diff CSVs are gitignored too: regenerate them (section 3) before promoting.

## 0. Decisions for the orchestrator / Mariano (nothing below depends on them)

D1. Renames whose old name is a port key (column P in 8_merge.md section 2, 10_merge.md section 2): 18 (S10.8) + 40
    (S10.10) rows rename hooks.csv names the port uses as registry / vector / golden keys. Options: (a) a port step adds
    aliases, then rename; (b) rename re/names + Ghidra only and keep hooks.csv names (needs a retag that does not copy
    the name); (c) defer. Lead recommendation: (c) for this merge, apply only the `-` rows; schedule (a) as a port step.
D2. mainLoop 0x0040f5c0 (C2, held NO_PATH1 in re/c3_hold.tsv) reads 100% from SMSG's MainLoop.cpp (75,108 bytes, S10.2,
    U-1858). A match is not a C2 -> C3 step (re/CONFIDENCE.md). Whether a 100% match plus the c3fy scenario evidence
    lifts it is a gate question. Lead recommendation: keep C2, record the match in the hold reason.
D3. Names whose verb and class our disassembly checks but whose noun is SMSG's (S10.9 rules ACCEPT_GET / SET / FWD,
    re/analysis/systems/smsg_sound_1.md; S10.10 FWD / SLOT). Each row's evidence column says `name from SMSG-decomp's
    PDB` and the purpose column states only instruction facts. Lead accepted them; the orchestrator may hold the FWD
    rows if it wants the noun proven too.
D4. Byte identity as library-object identity (S10.7, U-1881): two sound CRT rows (0x48268, 0x4a010) whose bodies hold jump
    tables are equal byte for byte with relocation sites masked. Lead recommendation: count it (section 5 item 2).

## 1. Shared-file edits, in apply order

1. CLAUDE.md "Matching decompilation", after the `// FLAGS <module>: ...` paragraph (S10.1 1_merge.md, S10.5 5_merge.md):
   `match.py also reads // INCLUDE <dir> lines (repo-relative, search order; // INCLUDE $VC places VC6's include
   directory, else it comes last) and // COMPILE <path> (cl compiles that file in place of the annotated one: under /ZI
   an #included file's __LINE__ is a constant, not the per-function line variable). A file's // TOOLCHAIN sp5 line runs
   VC6 SP5's cl 8804 / c1xx 8964 / c2 8966 from tools/vc6sp5 (sound.dll's C++ objects: Rich entry 11/8966 x34 equals the
   obj @comp.id, the back end's build); sp5c2 is RTM's front end with SP5's C2 (/B2), indistinguishable by the Rich
   header; default 8168. In the exe (no relocations) an original operand whose value the obj holds as a plain literal is
   compared as that literal (U-1886: 0x807fff, flag words such as 0x800000 in the image range).`
2. CLAUDE.md: the S10 paragraph of section 6 below, after the "SMSG-decomp snapshot" paragraph.
3. UNCERTAINTIES.md: the rows of log/roadmap/S10/{1,2,3,4,5,6,7,8,9,10}_uncertain.tsv (U-1850..U-1897); U-1886 is
   resolved by the literal rule (rgb_ycc_start 100%, tests/test_match_toolchain.py).
4. (done by the lead) re/names/smsg_exe_1.tsv rgb_ycc_start 0x004b4ca0: evidence now cites the 100% match in
   re/match/smsg_golf_jpeg_jccolor.cpp under the literal rule (was 98.2%); names_validate / check_evidence re-run: 0 bad.

## 2. New and changed files (take as they are; full list in files.txt)

- Tools: re/tools/smsg_pdb.py (MSF 7.0 PDB reader), re/tools/smsg_import.py (SMSG source -> annotation-only re/match
  wrapper, per-file .dsp flags, PDB -> obj symbol mapping, ICF holder, inventory), re/tools/smsg_dedupe.py (only for runs
  made before S10.6's converter fix), re/tools/smsg_hand.py (S10.2 helper, unused: drop if unwanted), re/tools/match.py
  (`// INCLUDE`, `// COMPILE`, `// TOOLCHAIN 8168|sp5|sp5c2`, `--toolchain`, the U-1886 literal rule; every pre-S10 file
  keeps its result), tests/test_smsg_import.py, tests/test_match_toolchain.py, tests/test_match.py (skips a file whose
  toolchain directory is absent: tools/vc6sp5 is local).
- Canonical match files, all MATCH lines 100% (lead re-check of all 104 files: 1,104 / 1,104, log/roadmap/S10/
  lead_smsg_recheck.log empty = no line below 100%): re/match/smsg_golf_*.cpp 32 files / 249 lines (golf.exe/src),
  smsg_golf_jackal_*.cpp 46 / 335, smsg_golf_jpeg_*.cpp 12 / 25 (incl. the 24 libjpeg statics S10.8 found and rgb_ycc_start),
  smsg_jgld_*.cpp 4 / 6, smsg_sound_*.cpp 28 / 489. New distinct MATCH addresses: exe 609, sound.dll 489, jgld.dll 6,
  Terrain.dll 0 (all 169 of their Terrain procedures were already ours at 100%: the converter's calibration). Distinct
  MATCH addresses over re/match/*.cpp: 2,931 -> 4,035.
- re/match/wip/smsg_golf_GolferShot.cpp, smsg_golf_Toolbar.cpp, smsg_golf_jackal_multidebug.cpp (4 misses: 97-99.9%,
  MultiDebug::~MultiDebug 92.9%), re/match/wip/smsg_sound_*.cpp (22 comment-only files, `// SMSG-ERROR:` rows).
- Names: re/names/smsg_exe_1.tsv (42), smsg_sound_1.tsv (268), smsg_jgld_1.tsv (22), smsg_jgld_crt.tsv (54),
  smsg_terrain_crt.tsv (60), smsg_{exe,jgld,sound,terrain}_lib.tsv (1,132, S10.7), re/analysis/systems/<id>.md each. Gate (lead re-run):
  names_validate bad 0 / cross-file duplicates 0, names_check_evidence 461 citations 0 bad.
- Step logs and tables: log/roadmap/S10/* (notes, merge lists, inventories inventory_<module>.tsv, runs/, PDB tables pdb/).

## 3. Tracker and master, in this order

1. Regenerate the gitignored evidence CSVs (log/diff) for the canonical files:
   `for f in re/match/smsg_*.cpp; do ~/bin/py -3.12 re/tools/match.py $f; done` (writes GREEN CSVs only: every line is 100%).
2. C3 -> C4 by 100% match, 158 rows (no address twice): log/roadmap/S10/2_promote_cmds.txt (25, golf.exe/src),
   3_promote_cmds.txt (77, jackal.lib; includes TextView::build 0x0048e900, so 5_merge.md's command is the same one),
   6_promote_cmds.txt (56, sound.dll). Lead ran `re_classify.py check` (read-only) on the 102 exe rows: 101 pass, gameMain
   0x0045baf0 is refused by the open uncertainty U-1351 (S3.6, a structural row on its note): skip it or settle U-1351 first.
   S10.6 ran check on the 56 sound rows: all pass. Plus 7 rows by library identity (section 5, 7_promote_cmds.txt, copy step
   first; 2 of them depend on D4). Expected after: C4 + 163 (section 7 drops strcpy 0x4a010), or + 162 without D4.
3. Names, new rows (C0 -> C1 -> C2), CLAUDE.md "Callers named to unblock held rows" recipe: back up the master; ApplyNames
   (no overwrite: every target is FUN_) on each module for re/names (golf_clean.exe, sound.dll, jgld.dll, Terrain.dll);
   re-export the function lists, delete log/xref_<module>.json; then
   `~/bin/py -3.12 scripts/re_classify.py batch re/names/smsg_exe_1.tsv re/names/smsg_sound_1.tsv re/names/smsg_jgld_1.tsv re/names/smsg_jgld_crt.tsv re/names/smsg_terrain_crt.tsv re/names/smsg_exe_lib.tsv re/names/smsg_jgld_lib.tsv re/names/smsg_sound_lib.tsv re/names/smsg_terrain_lib.tsv --to C1`,
   `~/bin/py -3.12 re/tools/c2_note.py --all --out log/c2_batch_s10.tsv`, `re_classify.py batch log/c2_batch_s10.tsv --to C2`
   (bodies of 5 bytes or less stay C1).
4. Function extents: append to re/match/boundaries.tsv log/roadmap/S10/9_boundaries.tsv (20 jgld / Terrain debug-heap CRT
   routines; their false 11-byte inner starts go with DeleteFunctions.java, then SetBodies.java), 6_boundaries_sound.tsv
   (143 sound.dll bodies our master lacks; SetBodies.java $(cat log/roadmap/S10/6_setbodies_args.txt) on sound.dll), and
   S10.7's rows (section 5). Then re-run `smsg_import.py project sound/sound.dll/sound.dsp` (143 more canonical lines), their
   evidence, and `smsg_import.py inventory sound`. The 143 have no hooks.csv row: names first (U-1877).
5. Renames (decision D1): the `-` rows of 8_merge.md section 2 / 2c, 9_renames_sound.tsv (19 placeholder ctors / dtors) and
   10_renames.tsv; apply by editing the name in its file, ApplyNames overwrite, `re_classify.py retag <file>`. P rows wait.
6. Model corrections (S10.8, 8_merge.md sections 2b and 3): g_animalRecords 0x0056d1b8 -> VIP residents (re/model/
   addresses.tsv, re/analysis/data/model_c.md, sections/c.h, model_name_changes.md; P: 13 port files use the old name, so the
   symbol rename goes with D1), golfer fields U-0010 / U-0015 / U-0016 resolved, U-0009 partial.

## 4. Gate (section S10 row), commands and expected numbers

- `~/bin/py -3.12 -m pytest tests/test_match.py tests/test_match_tools.py tests/test_smsg_import.py tests/test_match_toolchain.py -q`:
  S10.6's run 484 passed (test_match + test_match_tools + test_smsg_import); test_smsg_import + test_match_toolchain 18 passed
  (lead re-run). On a PC without tools/vc6sp5 the 28 smsg_sound files skip with the reason.
- Every smsg MATCH line 100%: `for f in re/match/smsg_*.cpp; do ~/bin/py -3.12 re/tools/match.py $f --no-evidence; done`
  -> 1,104 lines, none below 100% (lead re-check).
- `~/bin/py -3.12 -m pytest -m static -q`: 770 passed / 2 skipped / 46 deselected in 557 s (lead run on the final tree,
  log/roadmap/S10/lead_static_tier.log; S10.1's run before the bulk files was 640 / 2).
- Names: `~/bin/py -3.12 re/tools/names_sanitize.py <files>` 0 replaced; `names_validate.py smsg_exe_1 smsg_sound_1 smsg_jgld_1
  smsg_jgld_crt smsg_terrain_crt smsg_exe_lib smsg_jgld_lib smsg_sound_lib smsg_terrain_lib` (lead re-run: bad 0, cross-file duplicates 0); `names_check_evidence.py` 461 citations, 0 bad.
- No port/, shim/, original\ or master change: `git status --short port shim original` shows only files that were already
  modified before S10 started (S3's gl_raster / terrain work in port/).

## 5. S10.7, library-object identity
Library-object identity (log/c3/m36e_lib_match.py's comparison: match.py normalisation, relocated operands masked) of every
PDB library procedure against the archive member; CSVs in log/roadmap/S10/7_lib/<module>/ (never log/diff), notes
7_notes.md, merge 7_merge.md.

| module | archive that agrees | library procedures (distinct addr) | identical 100% | misses | not an archive member |
|---|---|---|---|---|---|
| jgld.dll | SP2 libcmtd.lib | 318 | 317 | `__amsg_exit` 66.7% | 0 |
| Terrain.dll | SP2 libcmtd.lib + libcimtd.lib | 472 | 471 | `__amsg_exit` 66.7% | 0 |
| golf_clean.exe | RTM libcmt.lib (234; SP5 / SP2 226: heap code differs) | 262 | 234 | `__amsg_exit` 80.0% | 27 (26 Terrain thunks, `_RtlUnwind@16`) |
| sound.dll | SP5 libcmt.lib + libcimt.lib (393 of 394; RTM 368) | 554 | 425 | `__amsg_exit` 53.3% | 128 import thunks |

Controls: the original's first opcode byte flipped, 146 / 146 RED. FidDb rebuild not run: the identity pass plus a reverse
search of every leftover function against every archive body leaves 208 unnamed (exe 20, jgld 77, sound 31, Terrain 80),
mostly bodies of 16 bytes or less (7_notes.md section 6).

1. Names: re/names/smsg_{exe,jgld,sound,terrain}_lib.tsv (1,132 rows) + re/analysis/systems/smsg_*_lib.md; most targets are
   already named in the master by FID (ApplyNames without overwrite renames the 21 FUN_ ones). 35 rows where an existing
   name disagrees with the library symbol: 7_conflicts.tsv (U-1882), not applied.
2. C3 -> C4 by identity, 7 rows: log/roadmap/S10/7_promote_cmds.txt (copy step into log/diff, then promote). exe
   `__ArrayUnwind` 0x004a5793; sound `__ftol` 0x42550, `strstr` 0x42690, `__ArrayUnwind` 0x4288e, `strchr` 0x45680 (the round-36
   naked transcriptions, now backed by the vendor's object), `doubleExponentField` 0x48268 and `strcpy` 0x4a010 on BYTE identity
   (jump tables inside the body: equal bytes with relocation sites masked, U-1881). Decision D4: does byte identity count as
   library-object identity? Lead recommendation: yes (it is stricter than the instruction comparison); otherwise drop those two.
3. Boundaries: 7_boundaries.tsv (114 rows) for re/match/boundaries.tsv; see section 3 step 4 and the lead's simulation below.

## 6. CLAUDE.md paragraph (after "SMSG-decomp snapshot (third_party/smsg)")

Section S10, SMSG import and verification (2026-10-29, steps re/roadmap/S10_steps.tsv, notes log/roadmap/S10/<k>_notes.md,
merge log/roadmap/S10/MERGE.md, U-1850..U-1897). `re/tools/smsg_pdb.py` reads their MSF 7.0 PDBs (procedure rva / size / name /
.obj / archive, publics); `re/tools/smsg_import.py file|project|inventory|calibrate` turns an SMSG source into an annotation-only
re/match wrapper (`// COMPILE` their file, `// INCLUDE` their .dsp / build.bat dirs, `// FLAGS` from the .dsp project line plus
per-source overrides, `// TOOLCHAIN sp5` for sound.dll, one MATCH line per PDB procedure of that .obj: the public at the rva,
else the obj symbol by undecorated name, overloads scored, an ICF-folded address held once), scores every line, writes 100%
lines to re/match/smsg_<pdb>_[<lib>_]<stem>.cpp and misses to re/match/wip/. Never `--evidence` while exploring: match.py
writes a CSV for every result and a RED CSV breaks the C4 gate of a row already at C4. Calibration: all 169 procedures of
their Terrain sources compile to 100% at addresses we already matched (after `// COMPILE` replaced `#include`: under /ZI an
included file's __LINE__ is a constant); every address we matched in golf / jackal / jgld / libpng / zlib / lpc10 also reads
100% from their source. New 100% addresses: golf_clean.exe 609 (golf.exe/src 249, jackal.lib 335, jpeg.lib 25), sound.dll
489 (+143 bodies our function lists lack, 100% with the PDB extent, 6_boundaries_sound.tsv), jgld.dll 6, Terrain.dll 0;
misses 4 (97-99.9%, MultiDebug::~MultiDebug 92.9%). mainLoop 0x0040f5c0 is 100% from their MainLoop.cpp. SP5: `// TOOLCHAIN
sp5` runs SP5's cl / c1xx / c2 (C2 12.00.8966 = sound.dll's Rich entry, proven by the obj @comp.id); it lifts none of our 54
sound near misses and only one of their 489 sound functions needs it (0x10010920, 99.4% with 8168): sound.dll's C++ objects
were built by SP5 but 8168 and SP5 emit the same code for nearly every function. Library-object identity (S10.7): jgld 317 /
318 and Terrain 471 / 472 library procedures are SP2 libcmtd members, the exe 234 / 262 RTM libcmt, sound 425 / 554 SP5 libcmt
(the rest are import / Terrain thunks; `__amsg_exit` is below 100% in all four, U-1880); a FidDb rebuild was not needed.
Names (S10.8 / S10.9 / S10.10 / S10.7): 1,578 new rows in re/names/smsg_*.tsv with our citations, and rename lists
(8_merge.md, 9_renames_sound.tsv, 10_renames.tsv) of which rows marked P rename port keys (decision D1). Findings: golf.pdb is
right on four of five contradictions (0x004038f0 drawWalkers, 0x0040e720 openHole, 0x0042dea0 rateCourseHoles, 0x00422530
golferShotReach); the 0x0056d1b8 records are celebrity vacation-home residents (spawned by mainLoop 0x00417a5a on type-5 lots,
string 0x004c6444), not animals; golfer fields U-0010 / U-0015 / U-0016 resolved. match.py: an exe operand the obj holds as a
plain literal is compared as a literal (U-1886).

## 7. Lead check of the boundaries proposals (before anyone applies them)

log/roadmap/S10/lead_tools/bound_sim.py scored every MATCH line of re/match/*.cpp with a scratch boundaries.tsv = ours + all
277 proposed rows (9_boundaries.tsv 20, 6_boundaries_sound.tsv 143, 7_boundaries.tsv 114; re/match/boundaries.tsv untouched,
no evidence written; output log/roadmap/S10/lead_bound_sim_{0,1}.tsv). 4,034 of 4,035 lines stay at 100%. The one that drops:
m36c_crt_asm.cpp sound.dll strcpy 0x1004a010, 100% -> 3.4%, because 7_boundaries.tsv gives it 240 bytes (strcat.obj's whole
strcpy + strcat body) while our 7-byte extent models strcpy's entry falling into the loop it shares with strcat 0x1004a081.
So: apply 7_boundaries.tsv WITHOUT its 0x1004a010 row, and skip 7_promote_cmds.txt's 0x0004a010 line (its CSV was scored over
the 240-byte extent; it also depends on D4). C4 count then: + 163 with D4 (exe +102, sound +61), + 162 without. After
applying, run the match tier once: `~/bin/py -3.12 -m pytest tests/test_match.py tests/test_match_tools.py -q`.
