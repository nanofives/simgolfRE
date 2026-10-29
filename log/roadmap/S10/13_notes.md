# S10.13 notes: names for the 145 deferred bodies and the 35 library-name conflicts (2026-10-29)

Status: DONE. RE names work only: nothing written to port/, shim/, original\, the Ghidra master or the tracker, and no
existing file changed. The merge items are in 13_merge.md and every file written is in 13_files.txt.
Tool refusals: none.

## Task 1: the 145 deferred bodies (sound 143, jgld 1, Terrain 1)
Facts come from our binary only. `13_tools/collect13.py` writes 13_facts.json:
- the body is decoded with capstone from the extent in 6_boundaries_sound.tsv;
- a raw scan of .text finds every rel32 `call` / `jmp` / `jcc` onto the body or its incremental-link thunk;
- pointer_callers.references finds stored pointers and table slots.
Result:
- 139 of the 143 sound bodies have no reference beyond their own thunk's `jmp`: they are dead code.
- update_sound (`ret`) is stored by init_sound_timer (0x1000932f).
- Three are referenced only from dead bodies.
- The jgld and Terrain `$$$00001` rows are the first range of _strchr in our lists, reached by `je` 0x1009122d /
  0x10024afd.

S10.9's rules need a class pin from an installer or a named caller, and dead code has neither. I kept S10.9's verb
rules and pinned the class from our own data instead, with one of four pins:
- VT: a vtable store;
- THIS: `this` passed to a named method;
- NEIGHBOUR: the nearest class-carrying named functions on both sides carry the class classmap.py maps SMSG's class
  to (`13_tools/neighbours.py` -> 13_neighbours.tsv; VC6 object-file contiguity, U-1920);
- FREE: a free function.
The hand verdicts are in 13_manual.tsv. `13_tools/build13.py` writes 13_ledger.tsv (145 rows) and
re/names/smsg_sound_2.tsv. The write-up is re/analysis/systems/smsg_sound_2.md.

| verdict | count |
|---|---|
| ACCEPT_GET | 5 |
| ACCEPT_SET | 9 |
| ACCEPT_VT | 7 |
| ACCEPT_HAND (IAT slots, COM slots, our named callees) | 30 |
| accepted, by pin | NEIGHBOUR 40, VT 7, FREE 3, THIS 1 |
| REJECT_NO_CLASS_PIN | 58 |
| REJECT_STUB (constant / empty, incl. the 7 EAX accessors, update_sound) | 29 |
| REJECT_VERB (CAviMgr::discard_frame, CVoiceRxMgr::discard_buffer, CWaveFile::fade_in, CWaveMgr::free_all) | 4 |
| REJECT_FWD_STUB (Dll_Wave_Device::get_eax 0x1000e390 -> a `mov eax, 0xb` stub) | 1 |
| REJECT_PART_OF_FUNCTION (the two _strchr ranges) | 2 |
| total | 51 accepted, 94 rejected |

No smsg_jgld_2 / smsg_terrain_2 file is needed: their two rows are rejected.

Gate (commands in 13_merge.md):
- names_sanitize: 0 replaced;
- names_validate smsg_sound_2: 51 rows, bad 0, cross-file duplicates 0;
- names_check_evidence: 0 citations, 0 bad. The evidence cites IAT slots by address, because the xref index has no
  entry for these bodies;
- 13_tools/check_cites.py, the S10.9 copy extended to the facts' extents: 186 in-body citations, 0 bad.
Controls:
- check_cites with one citation moved off an instruction start (0x10008e0b -> 0x10008e0c) reads BAD, rc 1: RED.
- The raw scan finds the known dead-to-dead calls (0x1000e3a5, 0x10029765, 0x100298c0) and the thunk store
  0x1000932f: a positive check.

Findings:
- update_sound is a reachable function missing from our list.
- The 16 sound-group records of 0x18 bytes at VoiceBase + 0x24.
- DsBuffer's block-align word is channels * bits (U-1921).
- Seq::TT_xpose_trackset writes the pitch offset track +0x45c that Midi_Device::noteOn adds.

## Task 2: the 35 conflicts (U-1882)
For each row I read the body under our function list (13_conflict_bodies.log) and checked the IAT names with pefile.
13_conflicts_settled.tsv gives a decision with evidence per row: 11 renames, 24 kept.
- The three U-1882 named are renamed:
  - crtCommitPages -> ___old_sbh_alloc_block: it allocates from a page and commits only as a fallback;
  - amsgExitThunk -> __fptrap: `__amsg_exit(2)`, the floating-point error;
  - crtValidatePointer -> _ValidateWrite: IsBadWritePtr only.
- Also renamed:
  - initFpControl -> __cfltcvt_init: it fills a function table and does no FPU work;
  - the three ostream destructors (wrong class in ostream::scalarDtorAdj8);
  - CItranscendental1 -> _exp;
  - sbhPageFree, sbhFindFreeRun and fpStatusExceptionType, which each name only part of the body.
The renames table with the P column (old name in shim/src / re/frida; none in port/) is 13_merge.md section 2.

## Open
U-1920..U-1924 (13_uncertain.tsv): the strength of the neighbour pin, DsBuffer format words, CWaveMgr::free_all slot
5, the `GT_request` noun, and the Sound5b6a8 = Seq equivalence. The 58 no-pin bodies could take a name once a second
pin (shared this-field offsets with live methods) is measured.

Reproduce:
```
~/bin/py -3.12 log/roadmap/S10/13_tools/collect13.py
~/bin/py -3.12 log/roadmap/S10/13_tools/neighbours.py
~/bin/py -3.12 log/roadmap/S10/13_tools/build13.py
```
