# S10 merge request, wave 2 (2026-10-29): follow-ups S10.11, S10.12, S10.13

Wave 1 (log/roadmap/S10/MERGE.md, files.txt) goes in first and is independent: no wave-2 worker edited a wave-1 file.
Files of wave 2: log/roadmap/S10/files_w2.txt (add the log/ ones with `git add -f`). U ids U-1900..U-1929.

## S10.13: names for the deferred sound.dll bodies, and the library-name conflicts

1. re/names/smsg_sound_2.tsv: 11 rows + re/analysis/systems/smsg_sound_2.md. Lead cut: S10.13 delivered 51 rows; 40 of
   them took their class only from the nearest named functions on both sides (`pin: the nearest named functions with a class
   prefix`, U-1920), which is an inference from layout, not from the body. The lead moved those 40 to
   log/roadmap/S10/13_rows_neighbour_pin_held.tsv (the delivered file is kept as 13_smsg_sound_2.as_delivered.tsv). The 11
   kept rows are pinned by a vtable store in the body (7), a call that passes `this` to a named method (1), or are free functions named by hand from the body (3: init_sound 0x10009240, get_sound_type 0x100099d0, do_message 0x1002f710).
   names_validate smsg_sound_2 after the cut: bad 0, cross-file duplicates 0. The systems writeup still describes all 51;
   it says which rule each row used.
   Order: S10.6's 6_merge.md section 1 a-b first (boundaries rows + SetBodies.java on sound.dll), then ApplyNames (no
   overwrite), `re_classify.py batch re/names/smsg_sound_2.tsv --to C1`, c2_note --all + batch --to C2. Every row's purpose
   ends `Dead in this build`: after endstate.py's checks, `re_classify.py mark <addr...> --status dead`.
2. The 35 conflicts of 7_conflicts.tsv (U-1882): 11 renames, 24 kept, evidence per row in 13_conflicts_settled.tsv, the
   rename table in 13_merge.md section 2. No old name occurs in port/src or port/test; 6 occur in shim/src SG_HOOK name
   strings and re/frida fixtures / registry keys, which look rows up by address (b6 practice), so the renames can go in
   without D1. Apply: edit the 11 names, names_validate exe_5 sound_1 sound_5 sound_8 undisc_sound, ApplyNames overwrite on
   golf_clean.exe and sound.dll, re-export, `c2_note.py --refresh` + `re_classify.py retag`. U-1882 then closes.
3. UNCERTAINTIES.md: U-1920..U-1924 (13_uncertain.tsv).
4. Finding: update_sound 0x10009a70 is reachable (its thunk is stored by init_sound_timer at 0x1000932f) but is missing from
   our function list; it is a bare `ret`.

## S10.12: the remaining match misses

Residual = every MATCH / WIP address of re/match/wip/*.cpp with no 100% line in re/match/*.cpp: 5 of 191, plus MultiDebug
(already 100% in our golf_hand_04.cpp). Table log/roadmap/S10/12_residual.tsv, notes 12_notes.md, merge 12_merge.md.

| address | function | result | cause |
|---|---|---|---|
| 0x00434cf0 | RenderBuildingToolbar (drawAmenitiesPanel row) | 99.9%; 100% only under `// TOOLCHAIN sp5` | one load pair scheduled differently |
| 0x00436e50 | RenderEmployeePanel | 99.7%; 100% only under sp5 | operand order of one add |
| 0x004a55d2 | ehVectorConstructorIterator | 81.8% with Ghidra's 106 bytes; 100% (44/44) with 130 | extent: its __finally funclet sits in the same COMDAT |
| 0x00427380 | ProcessHoleCompletion | 99.9% after 796 permuter compiles, sp5 / sp5c2 too | scheduling |
| sound 0x10048268 | `_expfield` (doubleExponentField) | 42.9% after 34 compiles and variants | different sequence (S10.7 has it by byte identity) |
| 0x0049c780 | MultiDebug::~MultiDebug from their source | 92.9% | an extra unwind state (golf_hand_04.cpp holds it at 100%) |

1. Lead decision (U-1910): the SP5 result for the two exe functions is NOT evidence. golf_clean.exe's Rich header lists only
   build-8168 objects, and 7 of the 609 canonical exe smsg lines drop below 100% under SP5. The lead moved
   re/match/smsg_hand_toolbar_sp5.cpp to re/match/wip/ (no evidence CSV was ever written; 12_promote_cmds.txt's 0x00434cf0 line is
   not to be run). If the orchestrator decides otherwise, `git mv` it back and follow 12_merge.md section 2.
2. Boundaries (U-1912): append to re/match/boundaries.tsv
   `golf_clean.exe	0x004a55d2	130	ehVectorConstructorIterator body 0x4a55d2..0x4a563c plus its contiguous __finally funclet 0x4a563c..0x4a5654 (VC6 emits both in one COMDAT)`
   then `git mv re/match/wip/s10_12_ehvec.cpp re/match/smsg_hand_ehvec.cpp`, `~/bin/py -3.12 re/tools/match.py
   re/match/smsg_hand_ehvec.cpp` (expected 100.0%, 44/44), and the match tier. The row is already C4 by scenario: no promote.
   No re/match/*.cpp line sits at 0x004a55d2, and the entry already exists in our function list, so the row changes no other
   extent (match.py's boundary_overrides only adds entries it does not already have).
3. MSVC6-tooling (read only): no register-allocation or scheduling model there, only class / diff / link probes.
4. UNCERTAINTIES.md: U-1910..U-1914 (12_uncertain.tsv).

## S10.11: the SimGolf demo's sound.dll debug records

The lead downloaded archive.org/details/Simgolfdemo `simgolfdemo.exe` (41,170,929 bytes, md5 f80d456930834cec5042fc01d36f22a2)
into log/roadmap/S10/11_dl/ (gitignored through `log/*`; never executed: the first S10.11 worker hung on a download approval and
was replaced). The worker extracted it with its own Python readers: PE stub, Microsoft cabinet at 0x292a2, InstallShield 6
data1.hdr / data2.cab. The demo sound.dll (md5 ccf6c2f658ac6a2f83341d0fcc8a315c) is a debug build with an NB11 CodeView block:
2,146 procedures (1,743 game, 42 LPC-10, 361 CRT) and 1,493 class layouts; the demo golf.exe has no debug directory, jgl.dll and
Terrain.dll only NB10 references to .pdb files the installer does not ship. New tool re/tools/cv_debug.py + tests/test_cv_debug.py
(5 passed, synthetic records). Notes 11_notes.md, merge 11_merge.md (exact rename / field tables).

- Code identity: 58 demo functions map to exactly one retail function, 0 ambiguous (all 42 LPC-10, 16 CRT asm); none of the
  1,743 game functions (the demo is /Od, retail /O2). Structural: vtable pairing 45 of 47 tables, 1,140 slot pairs.
- Names: re/names/demo_sound_1.tsv, 13 rows + re/analysis/systems/demo_sound_1.md; 133 rejected (118 constant stubs, 15 by hand).
  Gate (lead re-run): names_validate demo_sound_1 with every smsg_sound file bad 0 / cross-file duplicates 0, names_check_evidence
  0 bad. Order: ApplyNames (no overwrite) on sound.dll, re-export, `re_classify.py batch re/names/demo_sound_1.tsv --to C1`,
  c2_note --all + batch --to C2.
- Renames (11_merge.md section 3), all P: 42 codec rows to the LPC-10 C names (codecDecodeFrame -> lpc10_decode ...: both names
  fit the code, so a naming choice for the orchestrator, part of D1) and 3 placeholders replaced by demo field names
  (Seq_setField730Clamped -> Seq::set_volume, Seq_setField240 -> Seq::set_nswitch_threads, Channel_setField5cClamped1200 ->
  Wave::set_pitch). 42 further mismatches are listed for review only.
- Layouts: 72 field proposals in 23 classes, each cited at a retail instruction (11_merge.md section 4), for re/model; class sizes
  are confirmed for 5 classes only (U-1900).
- Lead decision (U-1903): the 118 constant stubs the demo names stay unnamed, as under S10.9's rule (a constant body supports no
  verb). UNCERTAINTIES.md: U-1900..U-1904 (11_uncertain.tsv).
- Next method if wanted: call-graph propagation from the paired slots to the non-virtual game functions.

## Gate for wave 2 (lead re-runs)

- `~/bin/py -3.12 -m pytest tests/test_cv_debug.py -q`: 5 passed. `test_match -k smsg_hand`: no canonical smsg_hand file after the
  lead's move (both wave-2 sources sit in re/match/wip until their decision / boundaries row).
- names_validate demo_sound_1 smsg_sound_2 (with all smsg files): bad 0, cross-file duplicates 0; names_check_evidence 0 bad.
- Wave 2 edited no wave-1 file, no port/, shim/, original\, master or tracker. New repo files: re/tools/cv_debug.py,
  tests/test_cv_debug.py, re/names/demo_sound_1.tsv, re/names/smsg_sound_2.tsv, re/analysis/systems/{demo_sound_1,smsg_sound_2}.md,
  re/match/wip/{s10_12_*.cpp, smsg_hand_toolbar_sp5.cpp}.

## CLAUDE.md sentence (append to the S10 paragraph of MERGE.md section 6)

`Wave 2: the SimGolf demo's sound.dll (archive.org Simgolfdemo, InstallShield 6) is a debug build with NB11 CodeView records
(2,146 procedures, 1,493 class layouts; re/tools/cv_debug.py): 58 map to retail by code identity (LPC-10, CRT asm), the rest only
through vtable slots (45 of 47 tables), giving 13 names and 72 cited field proposals. Two exe misses reach 100% only with SP5's
front end, but the exe's Rich header lists only build 8168 objects, so they are not evidence (U-1910).`
