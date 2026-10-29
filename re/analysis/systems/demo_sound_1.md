# demo_sound_1: sound.dll names from the SimGolf demo's debug records (S10.11, 2026-10-29)

Source: the SimGolf demo installer from archive.org (Simgolfdemo, simgolfdemo.exe, md5
f80d456930834cec5042fc01d36f22a2), read as bytes only. Its outer layer is a Microsoft cabinet at file offset 0x292a2
(log/roadmap/S10/11_tools/mscf.py), holding an InstallShield 6 set (data1.hdr / data2.cab, read by iscab.py). The demo's
sound.dll (2,404,920 bytes, md5 ccf6c2f658ac6a2f83341d0fcc8a315c, link time 0x3b982899, MISC debug entry
`C:\civilization3/sound.dll`) is a debug build (/Od, Edit and Continue frames) whose CodeView block is in the file: an
NB11 block at file offset 0x751a0, 1,925,272 bytes, read by our own re/tools/cv_debug.py. It holds 2,146 procedure
records, 3,395 publics, 698 data symbols and 11,040 type records (1,493 defined classes / structs / unions).

The names are the demo build's own symbol names. A row is here only when OUR binary (original/sound.dll, capstone)
supports it, as in S10.9 (re/analysis/systems/smsg_sound_1.md). The purpose column states what the retail instructions do.

## How a demo function reaches a retail address
1. Code identity (log/roadmap/S10/11_tools/idmap.py): normalised instruction sequences with relocated operands masked.
   58 of 2,146 demo procedures map to exactly one retail function. 57 are instruction-identical: 41 LPC-10 codec
   functions (built /Od in both) and 16 CRT assembly routines. The 58th, voicin_, is identical up to the switch table
   that follows its code. None of the 1,743 game-code procedures (.\soundO, debug against retail /O2) is identical.
   All 58 retail addresses already have a names row, so they give no row here. The 42 codec names are rename
   proposals (log/roadmap/S10/11_merge.md).
2. Vtable pairing (vtables.py, vtpair.py): every relocated run of code pointers in .rdata whose first address an
   instruction holds. On the demo this finds all 45 `??_7` vtable publics plus filter_table and mmx_filter_table.
   The 47 demo tables and 46 retail tables pair 45 to 45 by the longest common subsequence of their slot counts, in
   .rdata order, plus slot counts unique on both sides (Patch: 3 slots, which moved). slotfeat.py checks every slot
   pair: the fields (displacements off a non-stack base) and imports of the demo body against the retail body. Of
   1,140 slot pairs, 527 are EQUAL, 230 overlap, 50 are disjoint and 316 touch nothing on either side. A row here
   needs an EQUAL or overlapping pair and support from the retail instructions.

## Rules for a row
- The retail body stores or reads the member that the demo class layout names at that offset, and the verb fits.
  For example, set_total_class_size stores the class's own size from the demo record into MidiCtrl::m_total_class_size.
- Or the retail body calls a callee already named in our files whose name fits the verb (TrackSet::set_hi), or a
  virtual slot that the demo record names (CWaveMgr +0x1c play).
- Class prefixes follow S10.9's classmap.py: CMidiMgr is our Seq, Dll_Wave_Device is our Wave_Device.

## Rejected (log/roadmap/S10/11_ledger.tsv)
- Constant or empty slot stubs: 118 of the 146 unnamed bodies (203 slot rows that touch nothing on either side). No
  instruction supports their verb (S10.9's stub rule), even though the demo names the slot.
- Forwarders whose target slot holds the same forwarder: Midi::get_time, Wave::get_ndevices,
  Wave::get_device_description, Wave::stop_instance, Wave::fade_instance, Wave_In_Device::set_codec and
  Wave_In_Device::set_vxw_key.
- Bodies whose writes do not fit the verb: MidiCtrl::refresh, Dll_Wave_In_Device::update_sound and CVoiceTxMgr::stop.
  The last one tail-jumps slot +0x14, which the demo names unload.
- Not reviewed in depth: Wave_Device::select, MidiCtrlTarget::start_control, CMidiTrack::start_control,
  NoteMap::do_control and CWaveMgr::set_link.

## Counts
13 rows. Class layout proposals with retail instruction citations are in log/roadmap/S10/11_fields.tsv and
11_merge.md.
