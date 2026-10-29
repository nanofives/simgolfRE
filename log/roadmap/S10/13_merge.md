# S10.13 merge items (for the S10 lead / orchestrator)

S10.13 wrote no tracker, Ghidra, port, shim or shared file and changed no existing file. Its new files are listed in
13_files.txt.

## 1. re/names/smsg_sound_2.tsv (51 rows) + re/analysis/systems/smsg_sound_2.md: take as they are, in this order

a. First apply S10.6's 6_merge.md section 1 a-b:
   - append log/roadmap/S10/6_boundaries_sound.tsv to re/match/boundaries.tsv;
   - run SetBodies.java on sound.dll with 6_setbodies_args.txt;
   - re-export re/functions_ghidra_sound.dll.tsv and delete log/xref_sound.dll.json.
   Every smsg_sound_2 address is one of those 143 entries. Without step a, ApplyNames would label addresses that are
   not functions.
b. names_validate does not ask for function entries: it accepted the 51 addresses as they are. The rows therefore sit in
   re/names directly, and log/roadmap/S10/13_rows_pending.tsv was not needed.
c. Then run the C0 -> C2 recipe:
   - ApplyNames on sound.dll (`-process sound.dll -noanalysis`, no overwrite: every address is `FUN_` once created);
   - `re_classify.py batch re/names/smsg_sound_2.tsv --to C1`;
   - `c2_note.py --all` + `batch --to C2`.
d. Every row's purpose ends `Dead in this build`: no live code reaches any of the 51. After endstate.py's checks
   (re/CONFIDENCE.md), run `re_classify.py mark <addr...> --status dead`.
   - These rows are not C3 candidates.
   - Of the 143 bodies only update_sound 0x10009a70 is reachable (its thunk is stored by init_sound_timer at
     0x1000932f). It is a `ret` stub, so it was rejected and has no row here.

Gate (re-run after any edit):
```
~/bin/py -3.12 re/tools/names_sanitize.py re/names/smsg_sound_2.tsv re/analysis/systems/smsg_sound_2.md   # 0 replaced
~/bin/py -3.12 re/tools/names_validate.py smsg_sound_2                   # 51 rows, bad 0, cross-file duplicates 0
~/bin/py -3.12 re/tools/names_check_evidence.py re/names/smsg_sound_2.tsv   # 0 citations checked, 0 bad
~/bin/py -3.12 log/roadmap/S10/13_tools/check_cites.py re/names/smsg_sound_2.tsv   # 186 in-body citations, 0 bad
```
The evidence cites IAT slots by address (`IAT slot 0x100b85f8`), never `import X`. names_check_evidence reads the
xref index, which has no entry for these bodies until the master has the functions.

## 2. The 35 library-name conflicts of 7_conflicts.tsv (U-1882): 11 renames, 24 kept

Decisions with evidence, one row each: log/roadmap/S10/13_conflicts_settled.tsv.
Rule:
- rename when our name describes something the body does not do, or names the wrong class;
- rename when it states only one part of what the body does and the library identity (S10.7, 100%) states the whole;
- keep when our name states the same function in other words.
Spelling follows S10.7: golf and sound keep the PDB public name, and C++ members are `Class::ctor / dtor /
scalar_deleting_destructor` as in smsg_sound_lib.

| file | addr | old | new | evidence (13_conflicts_settled.tsv row) | P |
|---|---|---|---|---|---|
| re/names/exe_5.tsv | 0x004a4fc4 | initFpControl | __cfltcvt_init | fills the six-slot table 0x4e4b00..0x4e4b14 with five code addresses, touches no FPU state | P: shim/src/re/c3cq.cpp, re/frida/js/fixtures.d/c3cq.js, re/frida/registry.d/c3cq.py |
| re/names/exe_5.tsv | 0x004abe6b | amsgExitThunk | __fptrap | `push 2; call __amsg_exit`: runtime error 2 is _RT_FLOAT | - |
| re/names/sound_8.tsv | 0x10041aad | iosResetVtable | ostream::dtor | ostream's destructor (vbase vtable store 0x1005ecd4); ostream_withassign::dtor tail-jumps into it | P: shim/src/re/c3cm.cpp, shim/src/re/c3cx.cpp, re/frida/js/fixtures.d/c3cm.js, re/frida/registry.d/c3cm.py, re/frida/registry.d/c3cx.py |
| re/names/undisc_sound.tsv | 0x10041b9d | ostream::scalarDtorAdj8 | ostream_withassign::scalar_deleting_destructor | calls ostream_withassign's destructor 0x10041c26: wrong class in our name | - |
| re/names/sound_8.tsv | 0x10041c26 | ostreamResetVtable | ostream_withassign::dtor | vtable store 0x1005ece4, then a tail jump to ostream::dtor | P: shim/src/re/c3cm.cpp, shim/src/re/c3cx.cpp, re/frida/js/fixtures.d/c3cx.js, re/frida/registry.d/c3cx.py |
| re/names/sound_5.tsv | 0x100432b0 | CItranscendental1 | _exp | `mov edx, 0x100675ea; jmp __ctrandisp1`: the CRT exp entry | - |
| re/names/sound_8.tsv | 0x10047103 | sbhPageFree | ___old_sbh_free_block | frees one block's paragraphs into its page | P: shim/src/re/c3cx.cpp, re/frida/js/fixtures.d/c3cx.js, re/frida/registry.d/c3cx.py |
| re/names/sound_1.tsv | 0x10047148 | crtCommitPages | ___old_sbh_alloc_block | allocates from a page (0x10047192); commits pages only when no page has room (0x10047283) | - |
| re/names/sound_8.tsv | 0x10047350 | sbhFindFreeRun | ___old_sbh_alloc_block_from_page | finds and claims a run (map byte store 0x10047377) | - |
| re/names/sound_8.tsv | 0x1004820c | fpStatusExceptionType | __convertTOStoQNaN | `fadd` of the QNaN constant 0x1005f220 into st(0) (0x10048219) | - |
| re/names/sound_1.tsv | 0x10049604 | crtValidatePointer | _ValidateWrite | IsBadWritePtr only (0x10049610) | P: shim/src/re/c3cm.cpp, shim/src/re/c3fd_negctl.cpp, re/frida/js/fixtures.d/c3cm.js, re/frida/js/fixtures.d/c3fd.js, re/frida/registry.d/c3cm.py, re/frida/registry.d/c3fd.py |

- P files: the old spelling stays in SG_HOOK name strings, registry keys and fixtures. Rows are looked up by address
  (the b6 practice), so nothing there needs to change. No port/src or port/test file holds any of the 11 old names.
- Apply: edit the name column of the 11 rows, then:
  - run names_validate on exe_5 sound_1 sound_5 sound_8 undisc_sound;
  - run ApplyNames with `overwrite` on golf_clean.exe and sound.dll;
  - re-export the function lists, then `c2_note.py --refresh --out log/c2_refresh.tsv` and `re_classify.py retag
    log/c2_refresh.tsv` (hooks.csv follows; levels unchanged).
  - Generated note filenames keep the old names.
- Kept (24): the eh vector iterators, operator new / delete / new[] in all four modules, the four sound critical-section
  wrappers, __GetLinkerVersion, ___sbh_heap_init, the old-SBH region functions (new / release / decommit / find /
  heap_check), __startTwoArgErrorHandling, __load_CW and __checkTOS_withFB.
  - Optional: append `library identity <symbol> (S10.7)` to their evidence column. No action is needed.

## 3. Shared files
- UNCERTAINTIES.md: rows U-1920..U-1924 from log/roadmap/S10/13_uncertain.tsv.
- U-1882 (7_uncertain.tsv) can close with section 2's decisions.
