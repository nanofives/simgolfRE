# S10.7 notes: library identity for all four modules (2026-10-29, claude2 worker)

RE work only: no port/, shim/, original\, Ghidra master or tracker write, and no CSV in log/diff. Names, boundaries and promote
lines are for the merge (7_merge.md). Uncertainty ids U-1880..U-1884 are in 7_uncertain.tsv.

## 1. Result per module

Identity = match.py's comparison (normalized instructions, relocated operands masked) of the library member's function
object with our body, 100%. Where jump tables sit inside the body (memcpy, memmove) it is equal bytes with the relocation sites
masked and every section-local relocation holding address + label + stored addend (U-1881).

| module | archive that agrees | library procedures | distinct addresses | identical (100%) | misses | not a library member |
|---|---|---|---|---|---|---|
| jgld.dll | VC6 SP2 libcmtd.lib | 319 (PDB: libcmtd) | 318 | 317 | 1: `__amsg_exit` 66.7% | 0 |
| Terrain.dll | VC6 SP2 libcmtd.lib, libcimtd.lib | 473 (340 + 133) | 472 | 471 | 1: `__amsg_exit` 66.7% | 0 |
| golf_clean.exe | VC6 RTM libcmt.lib | 262 (PDB publics outside every procedure) | 262 | 234 | 1: `__amsg_exit` 80.0% | 27: 26 Terrain.dll method thunks, `_RtlUnwind@16` |
| sound.dll | VC6 SP5 libcmt.lib, libcimt.lib | 554 (PDB publics outside every procedure) | 554 | 425 | 1: `__amsg_exit` 53.3% | 128 KERNEL32 / USER32 / WINMM / OLE32 import thunks (`_Name@N`) |

- jgld's two PDB procedures at 0x1009a140 (`_strcmpi`, `_stricmp`) are one address, hence 319 rows and 318 addresses; Terrain's
  0x10031e10 likewise.
- golf.pdb and sound.pdb list no VC6 archive as a module (their CRT is linked without debug records), so the candidates are
  the publics no procedure covers inside .text (cands.py). jgld.pdb and Terrain.pdb name the archive per procedure.
- Which archive version: 7_agree_golf.tsv / 7_agree_sound.tsv score every candidate in the RTM, SP5 and SP2 libcmt separately.
  - Exe: 235 candidates have a member in libcmt; RTM is identical for 234, SP5 and SP2 for 226 each. The heap code
    (`_free`, `__heap_alloc`, `_realloc`, `__heap_init`, `___sbh_heap_init`, `___sbh_free_block`, `__msize`, `_calloc`) is
    where SP5 / SP2 differ from the exe: 12-88%.
  - sound.dll: 394 candidates have a member in libcmt; SP5 is identical for 393, SP2 for 391 (`_realloc` 84.2%,
    `___old_sbh_resize_block` 97.1%), RTM for 368 (it has no member for `__GetLinkerVersion`, `___heap_select` and the
    old-SBH family, and its heap routines differ: `_realloc` 44.0%).
  - jgld / Terrain: SP2's libcmtd. RTM's LIBCMTD.LIB and SP5's LIBCMTD / LIBCIMTD were tried too and never beat it.
- Extent the 100% needed (jgld / Terrain / exe / sound; libscore.py keeps, among the 100% variants, the one that compares the
  most bytes):
  - our list as it stands: 279 / 433 / 202 / 270;
  - PDB size: 25 / 26 / 15 / 66 (the exe and sound numbers include 1 / 9 with the fill bytes trimmed);
  - the library's own code length, jump tables cut off: 0 / 0 / 4 / 12;
  - the library body taken to the end of its section (an alias entry label inside it, such as `___from_strstr_to_strchr`
    inside `_strchr`): 9 / 8 / 4 / 5 (jgld and Terrain include 1 each with the fill trimmed);
  - byte identity: 5 / 5 / 9 / 72.
  The rows that needed anything but our extent get a boundaries line (section 4).

## 2. Misses and causes

`__amsg_exit` is the only function below 100% in any module (U-1880). The libraries' body calls `_FF_MSGBANNER` and tests one
error-mode word against 2; ours carry that banner test inline in all four modules (0x10082870, 0x10019c80, 0x004a6937,
0x1004319b). No SP2, SP5 or RTM archive, debug or release, has the inline form, so the cause is the CRT build, not an extent.
Functions that scored below 100% before the extent variants and byte identity, all identical now (7_miss_<key>.log has the
diffs of the first pass): `_memcpy`, `_memmove`, `_strchr` (alias labels and jump tables inside the body), `__CrtCheckMemory`
(S10.9's U-1891: a `jmp` to a label past our extent; byte identity over the whole body), `_sqrt`, `_log` (local `jne` targets
in the section), the SEH helpers (`__global_unwind2`, `__local_unwind2`, `___FrameUnwindToState` ...: extent). jgld and Terrain
0x10097ee0 / 0x1002fd00 scored 10% against `___crtGetLocaleInfoA` (a name guessed from the PDB procedure `crtGetLocaleInfoA`);
the PDB procedure there is the static `_crtGetLocaleInfoA@16` of getqloc.obj, which is identical (the 140-instruction
`___crtGetLocaleInfoA` of a_loc.obj is the function at 0x1009b050).

## 3. Names (b)

Rows for every identical address that has no row in any re/names file and starts a function of ours: re/names/smsg_exe_lib.tsv
188, smsg_jgld_lib.tsv 261, smsg_sound_lib.tsv 260, smsg_terrain_lib.tsv 423 = 1,132, with re/analysis/systems/smsg_<id>.md.
- 1,098 come from the PDB-driven scoring (185 / 248 / 256 / 409) and 34 (3 / 13 / 4 / 14) from the reverse search of section 6.
- Skipped: 242 addresses already have a row (45 / 68 / 68 / 61), 107 are entry labels or thunks that no function of ours starts
  at (4 / 1 / 101 / 1; ledgers 7_ledger_<key>.tsv).
- The master already names almost all of them (FID, applied 2026-10-03): only 21 target a `FUN_` function (jgld 1, sound 19,
  Terrain 1). The rows add the library identity evidence and give the tracker a row; ApplyNames without `overwrite` changes
  those 21 only.
- Conventions: golf and sound keep the PDB public name (S10.8: `__fpclear`), jgld and Terrain take the PDB's undecorated name
  (S10.9: `_msize`). C++ names read Class::member; a duplicate gets the address as suffix.
- Gate: names_sanitize 0 replaced, names_validate bad 0 / cross-file duplicates 0, names_check_evidence 0 citations (none
  made), commands in 7_merge.md.
- 7_conflicts.tsv: 35 identical functions whose existing name differs from the library symbol; three describe something else
  (U-1882). No existing row was changed.

## 4. C3 rows lifted to C4 by identity (a)

Identical rows by tracker level (7_gaps_<key>.tsv): C4 already 17 exe / 8 jgld / 17 sound / 1 Terrain; **C3 1 exe + 6 sound = 7**;
C2 or C1 61 (7_c2_identical.tsv: they need a C3 hook first); no row yet 1,098 (section 3; 1,132 with the reverse-search rows).

The seven C3 rows (commands and the copy step into log/diff in 7_promote_cmds.txt, merge section 3):

| address | name | library function | evidence form |
|---|---|---|---|
| exe 0x004a5793 | `__ArrayUnwind` | RTM ehvecdtr.obj `?__ArrayUnwind@@YGXPAXIHP6EX0@Z@Z` | instructions, PDB extent (94 bytes) |
| sound 0x1004288e | `__ArrayUnwind` | SP5 ehvecdtr.obj | instructions, library code length |
| sound 0x10042550 | `__ftol` | SP5 ftol.obj | instructions |
| sound 0x10042690 | `strstr` | SP5 strstr.obj | instructions |
| sound 0x10045680 | `strchr` | SP5 strchr.obj | instructions, library body to section end |
| sound 0x10048268 | `doubleExponentField` (`__checkTOS_withFB`) | SP5 common.obj | byte identity (U-1881) |
| sound 0x1004a010 | `strcpy` | SP5 strcat.obj | byte identity (U-1881) |

`re_classify.py check --to C4` was run on all seven (it reads, writes nothing): four are refused only for the missing match CSV
(exe `__ArrayUnwind`, sound 0x4288e, 0x45680, 0x48268), the other three pass on the old naked-transcription CSVs that the
round-36 decision does not count. Every other gate line passes for all seven (hook source, SG_HOOK, build, path-1, callers,
no STUB).

## 5. Extent fixes (c)

7_boundaries.tsv: 114 lines in re/match/boundaries.tsv's layout (module, entry, size, reason) for identical functions whose
100% needed an extent other than our function list's (PDB size, padding trimmed, library code length, library body to the end of
its section, byte identity), counted per module in section 1. 20 more are already in S10.9's 9_boundaries.tsv (7_boundaries_have.tsv,
not repeated). Entry labels that no function of ours starts at get none: they would create overlapping functions. Examples:
exe `__NLG_Notify` 19 bytes (ours 24), `_WinMainCRTStartup` 264 (253), `___FrameUnwindToState` 158 (103), `_memcpy` /
`_memmove` 821 bytes with their jump tables (ours 664); jgld `_strchr` 188 (192).

## 6. The FidDb question: not needed (recorded 2026-10-29, 17:20; the 20:00 gate was not reached and is not needed)

Unnamed functions of ours inside each module's library address range after the identity rows (7_leftover.tsv: list name `FUN_`,
no names row, no non-library PDB procedure there): exe 32, jgld 96, sound 49, Terrain 101. `7_tools/reverse.py` compares each with
EVERY function symbol of the archives (statics included, no PDB name used; same length plus up to 16 fill bytes, then match.py's
comparison). That is the comparison a FidDb pass makes (library body against ours), over the same archives.

| module | leftovers | identical to a library body | rows written (unique, 8 bytes or more) | unmatched |
|---|---|---|---|---|
| golf_clean.exe | 32 | 12 | 3 (`__unwind_handler`, `ExFilterRethrow`, `_siglookup`) | 20 (16 of 16 bytes or less, largest 78) |
| jgld.dll | 96 | 19 | 13 | 77 (44 of 16 bytes or less, largest 467) |
| sound.dll | 49 | 18 | 4 | 31 (27 of 16 bytes or less, largest 78) |
| Terrain.dll | 101 | 21 | 14 | 80 (47 of 16 bytes or less, largest 467) |

- Identical but not written: bodies under 8 bytes (the 4-byte `charNode::length` body is the same in many library functions, and
  `__fpclear`, 5 bytes) and bodies with more than one identical library symbol (`?FrameUnwindFilter@@YAHPAU_EXCEPTION_POINTERS@@@Z`
  has two, `__CrtSetBreakAlloc` five, `__CIasin` nine).
- Unmatched: mostly 16 bytes or less (the false starts and unwind funclets S10.9 found inside debug-heap routines) and a few
  pieces of larger functions. A hash of library functions names none of these either, so a rebuild of re/fid/simgolf_vc6.fidb with
  SP2 / SP5 libraries would add nothing the reverse search did not find. re/tools/build_fiddb.py is unchanged and was not run.
- The large unmatched bodies stay named `FUN_` (jgld 0x1007e8ed 157 bytes, 0x1007e9ad 145, 0x1007ea5d 158, 0x10082a82 467,
  0x10083fe9 163 resolved by reverse as `__check_range_exit`; Terrain has the same set). No claim is made about them (U-1883).

## 7. Reproduce (all from the repo root, `~/bin/py -3.12`)

```
for k in golf jgld sound Terrain; do ~/bin/py -3.12 re/tools/smsg_pdb.py procs $k > log/roadmap/S10/7_pdb_$k.procs.tsv; done
for k in golf jgld sound Terrain; do ~/bin/py -3.12 log/roadmap/S10/7_tools/cands.py $k; done    # 7_cand_<k>.tsv
for k in jgld Terrain; do ~/bin/py -3.12 log/roadmap/S10/7_tools/libscore.py $k; done            # 7_score_<k>.tsv, 7_lib/<k>/<archive>/*.match.csv (about 3 min)
for k in golf sound;  do ~/bin/py -3.12 log/roadmap/S10/7_tools/libscore.py $k; done
~/bin/py -3.12 log/roadmap/S10/7_tools/agree.py golf; ... agree.py sound                          # which archive version agrees
~/bin/py -3.12 log/roadmap/S10/7_tools/missdiff.py <k> > log/roadmap/S10/7_miss_<k>.log           # what is left below 100%
~/bin/py -3.12 log/roadmap/S10/7_tools/gaps.py; leftover.py > 7_leftover.tsv; reverse.py <k> (full pass, then `--only` pass)
~/bin/py -3.12 log/roadmap/S10/7_tools/build_rows.py; write_md.py; boundaries.py > 7_boundaries.tsv; conflicts.py > 7_conflicts.tsv; lift.py
```
libscore.py is a copy-and-extension of S10.9's libid.py and log/c3/m36e_lib_match.py: archives are indexed once, and each
function is tried with the extent variants of section 1 and with byte identity; the best 100% (most bytes compared) is kept and
its CSV is written under log/roadmap/S10/7_lib/ only. S10.9's files were not edited. Run at most two of these at once; the
jgld and Terrain reverse passes take about 10 minutes each.

## 8. Controls

- Wrong body: `7_tools/control.py` takes 146 identical rows (every 25th of each module plus every byte-identity row), flips the
  first byte of the original's body in memory (an opcode byte, never inside a relocated field) and scores again with every extent
  variant and byte identity: 146 of 146 read RED (7_control.log, 0 blind). A first version flipped the byte at one third of the
  body and left 37 rows at 100%: tiny bodies under 6 bytes were skipped and some bytes fell in masked operands, a flaw of the
  control, not of the scoring.
- Negative results in the data itself: `__amsg_exit` is below 100% in all four modules, so the 317 jgld, 471 Terrain, 234 exe and 425 sound identical
  rows are not a rubber stamp; the 128 + 27 no-member rows are import thunks, not archive functions.

## 9. Open items

- U-1880 `__amsg_exit` (all four modules), U-1881 byte identity as evidence, U-1882 three existing names that disagree with the
  library symbol, U-1883 coverage of library functions with no PDB record, U-1884 the boundaries lines against existing match sources.
- 61 identical functions are at C1 / C2 and need a C3 hook before C4 (7_c2_identical.tsv).
- The rows for the Terrain.dll method thunks in the exe (26, 0x004a4f10..0x004a4fa6) and the 128 sound.dll import thunks have no
  library member: they are import stubs, not CRT code.
