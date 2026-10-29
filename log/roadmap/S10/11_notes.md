# S10.11 notes: the SimGolf demo's sound.dll debug records (2026-10-29)

Status: DONE. Merge items are in 11_merge.md, files in 11_files.txt, uncertainties U-1900..U-1904 in 11_uncertain.tsv.
No download (the lead fetched it), nothing from the download was run, and no existing file was edited. No tracker,
Ghidra, port, shim or original\ write.

## Process notes (today's rules)
- Every file write was 60 lines or fewer. Longer files were built from parts appended with `cat`.
- Every Python run that read the download used `~/bin/py -3.12 -I` with the scripts in 11_tools.
- Every run was short (the longest took 7 s), so nothing ran in the background.
- No tool call was refused.

## Format identification (11_tools/probe_exe.py, mscf.py, iscab.py)
- simgolfdemo.exe is a small PE stub plus an overlay. At file offset 0x292a2 sits a Microsoft cabinet, MSZIP, one
  folder, 13 files: InstallShield Disk1 (Setup.exe, data1.hdr, data1.cab, data2.cab, ...).
- data1.hdr / data2.cab are InstallShield 6 cabinets (`ISc(`, version 0x0100600c). The file descriptors are 0x57 bytes
  with 64-bit sizes; a compressed file is a run of chunks, each a u16 length followed by raw deflate.
- 1,792 files are listed (11_dl/iscab_list.txt). sound.dll is file 36 in volume 2: 2,404,920 bytes, and its md5
  ccf6c2f658ac6a2f83341d0fcc8a315c equals the header's.
- The debug directory of the demo sound.dll holds three entries:
  - MISC: `C:\civilization3/sound.dll`;
  - FPO;
  - CODEVIEW: NB11 at file offset 0x751a0, 1,925,272 bytes, the whole CodeView block inside the image.

## re/tools/cv_debug.py (our parser)
- NB09 and NB11 are parsed in full. NB10 and RSDS are refused with the .pdb path; MSF 7 PDBs are smsg_pdb.py's job.
- It reads the subsection directory, then:
  - sstModule (module names);
  - sstAlignSym, sstGlobalSym, sstGlobalPub and sstStaticSym (S_GPROC32 / S_LPROC32, S_PUB32, S_LDATA32 / S_GDATA32,
    S_UDT, S_THUNK32);
  - sstSegMap (segment to section);
  - sstGlobalTypes (LF_CLASS / STRUCTURE / UNION, field lists with LF_INDEX continuations, members, bases, vbases,
    LF_ONEMETHOD / LF_METHOD with the vtable offsets of introducing virtuals, LF_POINTER / ARRAY / MODIFIER / ENUM).
- NB11 records carry 32-bit type indices (0x100b, 0x1004 ...), NB09 records 16-bit ones (0x0205, 0x0004 ...). Both
  forms are in the tables; only NB11 has been seen on real data.
- CLI: `info | procs | publics | classes [name] | json <out>`.
- Test: tests/test_cv_debug.py, 5 passed. A synthetic NB11 PE checks symbols, layout, numeric leaves and refusals;
  the demo counts are checked when the extracted file exists.
- Demo counts:
  - 389 modules, 2,146 procedures (2,034 global, 112 local), 3,395 publics, 698 data symbols, 6,324 UDTs, 129 thunks;
  - 11,040 type records with 1,493 defined aggregates.
  - The procedures split by module into 1,743 in `.\soundO\*.obj` (the game code: the 1,743 the brief mentioned),
    42 in `.\Debug\*.obj` (LPC-10) and 361 CRT (`build\intel\xmt_obj`, the debug CRT).
- Only sound.dll carries records in the file. golf.exe has no debug entry; jgl.dll and Terrain.dll carry NB10
  references to .pdb files the installer does not ship.

## Demo to retail map
1. Code identity, `11_tools/idmap.py <demo dll> log/roadmap/S10/11_idmap.tsv` (log 11_idmap.log).
   - Retail extents: the Ghidra function list (first range and size) plus SMSG's PDB lengths. Operands carrying a base
     relocation are masked, as are calls and jumps out of the body. A demo procedure maps when exactly one retail
     extent of the same length has the same key sequence.
   - Second rule, EXACT_CODE: when the tail of the demo procedure is relocated dwords (a switch table), the code before
     it is compared.
   - Results by size class (EXACT + EXACT_CODE / none / undecoded):
     - <=16 bytes: 1 / 29 / 1
     - 17-64: 6 / 1,142 / 0
     - 65-256: 26 / 618 / 2
     - 257-1024: 16 / 242 / 11
     - >1024: 8 + 1 / 37 / 6
   - Total 58 (57 EXACT + 1 EXACT_CODE), 0 ambiguous. By module: LPC-10 42 of 42, CRT 16 of 361, game code 0 of
     1,743. The demo game code is /Od /ZI (frames `sub esp, 0x44`, stores through [ebp-4]) and the retail code /O2.
   - All 58 retail addresses already have a names row (11_exact_names.tsv). The CRT names agree. The 42 codec names
     differ: rename proposals in 11_merge.md 3a.
2. Vtable pairing. This is a second, structural method, reported apart from identity.
   - `11_tools/vtables.py`: runs of relocated .text pointers in .rdata whose first address an instruction holds, with
     E9 thunks followed. On the demo it finds all 45 `??_7` publics plus filter_table / mmx_filter_table (47 tables),
     and 46 on retail.
   - `vtpair.py <demo dll> 11_vtpair.tsv`: the LCS of slot counts in .rdata order, plus counts unique on both sides,
     pairs 45 tables (1,140 slot rows). Unpaired: demo CMMError and mmx_filter_table, retail 0x1005b830 (U-1901,
     U-1902).
   - `slotfeat.py`: compares the fields (displacements off a non-esp / ebp base) and imports of each slot pair. Results:
     EQUAL 527, OVERLAP 230, RETAIL_SUBSET 16, DISJOINT 50, EMPTY_BOTH 316, NO_EXTENT 1.
   - pool.named() leaves out demo_sound_* files, so these numbers reproduce after the names file exists.

## Names (re/names/demo_sound_1.tsv, 13 rows)
- Pool: the retail bodies of paired slots with no names row in any re/names file. That is 146 bodies (`slotpool.py`).
- The 28 that are not stubs and are EQUAL or overlapping were reviewed by hand (`candfacts.py` writes 11_candfacts.txt
  with the listing, the demo member at each [this + d], and pointee vtables; `fwdcheck.py` checks the forwarders).
- Accepted 13, under S10.9's rules plus the demo's member and vtable records:
  - five set_total_class_size: the stored constant equals the demo class size;
  - Wave_Device::timer_unhook: clears m_ptimer;
  - Seq::is_trackset_playing: returns DllTrackSet m_ntracks_playing != 0;
  - Seq::set_active_range_hi: calls our TrackSet::set_hi;
  - Seq::get_track_velocity_offset / set_track_velocity_offset: CMidiTrack m_velocity_offset;
  - Seq::add_switch_range: m_switch_tracklo / hi / m_nswitchers / m_switch_table;
  - CAviMgr::init: m_wfx / m_frame_rate / m_frame_size / m_frame_buffer_size;
  - dllSoundList::play: calls the play slot of m_pCurSound.
- Rejected 133 (11_ledger.tsv): 118 stubs (U-1903) and 15 by hand (same-forwarder targets, verbs without support, two
  bodies not reviewed).
- Class prefixes follow S10.9's classmap (CMidiMgr is Seq, Dll_Wave_Device is Wave_Device).
- Counts: mapped by identity 58, by slot pairing 1,140 slot rows (527 EQUAL); named 13; rejected 133; renames
  proposed 45 (42 codec + 3 placeholders); 42 more listed for review (11_rename_candidates.tsv).

## Class layouts (11_fields.tsv, 11_merge.md section 4)
- `fieldprop.py`: in EQUAL slot bodies, retail `[this + d]` accesses (`this` = ecx at entry or a register copied from
  it, dropped at a call unless it is callee-saved).
- Each access is resolved through the demo layout flattened over its bases and embedded members (`layouts.py`). For a
  secondary vtable, the subobject class is used. Bitfield words are named by the word.
- Result: 72 fields in 23 classes, each with a retail instruction address, 0 unresolved accesses.

## Reproduce (all from the repo root, a few seconds each)
```
T=log/roadmap/S10/11_tools; D=log/roadmap/S10/11_dl; O=log/roadmap/S10
~/bin/py -3.12 -I $T/mscf.py $D/simgolfdemo.exe 292a2 $D/outer
~/bin/py -3.12 -I $T/iscab.py $D/outer/Disk1 get sound.dll $D/demo
~/bin/py -3.12 -I re/tools/cv_debug.py info $D/demo/36_sound.dll
~/bin/py -3.12 -I $T/idmap.py $D/demo/36_sound.dll $O/11_idmap.tsv
~/bin/py -3.12 -I $T/pool.py $O/11_idmap.tsv $O/11_exact_names.tsv
~/bin/py -3.12 -I $T/renames.py $O/11_exact_names.tsv $O/11_renames.tsv
~/bin/py -3.12 -I $T/vtpair.py $D/demo/36_sound.dll $O/11_vtpair.tsv
~/bin/py -3.12 -I $T/slotpool.py $O/11_vtpair.tsv $O/11_slotpool.tsv
~/bin/py -3.12 -I $T/slotfeat.py $D/demo/36_sound.dll $O/11_vtpair.tsv $O/11_slotfeat.tsv
~/bin/py -3.12 -I $T/fieldprop.py $D/demo/36_sound.dll $O/11_slotfeat.tsv $O/11_fields.tsv
~/bin/py -3.12 -I $T/candfacts.py $D/demo/36_sound.dll $O/11_slotfeat.tsv $O/11_candfacts.txt
~/bin/py -3.12 -I $T/ledger.py $O/11_slotpool.tsv $O/11_slotfeat.tsv re/names/demo_sound_1.tsv $O/11_ledger.tsv
```
Then the names gate in 11_merge.md section 1.

## Open
- U-1900: class layouts beyond the five sizes confirmed by set_total_class_size.
- U-1901 / U-1902: unpaired tables.
- U-1903: the stub rule. 118 stubs are named by the demo slot but have no supporting instruction.
- U-1904: MIDI_CONTROL enumerators. cv_debug parses LF_ENUMERATE, but the enum field lists were not printed.
- 42 rename candidates are unreviewed, and the 1,743 non-virtual game procedures are not mapped (no identity under
  /Od against /O2). Call-graph propagation from the slot pairs is the next method.
