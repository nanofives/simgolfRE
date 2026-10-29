# smsg_sound_2: names for the sound.dll bodies our function lists lack (S10.13, 2026-10-29)

Source of the names: third_party/smsg/release/sound.pdb (log/roadmap/S10/pdb/sound.procs.tsv), as in smsg_sound_1.
A row is here only when OUR binary (original/sound.dll, capstone) supports it. The rows cover the 143 procedures of
log/roadmap/S10/9_deferred_not_functions.tsv that S10.9 deferred because the Ghidra master has no function there. Their
extents come from log/roadmap/S10/6_boundaries_sound.tsv; S10.6 checked them against our disassembly, and every one
compiles at 100% from SMSG's source.

## What the 143 bodies are
Each body is the target of one incremental-link thunk in the thunk table at the start of .text (`jmp rel32`, for
example 0x100013c5 -> 0x10008e00). log/roadmap/S10/13_tools/collect13.py scans the whole of .text for `call`, `jmp` and
`jcc rel32` onto the body or its thunk. It also runs re/tools/pointer_callers.py for stored pointers and table slots.
- For 139 bodies nothing references the thunk or the body: they are dead in this build, the code incremental
  linking keeps (b1).
- Four bodies are referenced:
  - update_sound 0x10009a70, a `ret`: its thunk is stored into the timer object by init_sound_timer (0x1000932f);
  - SoundList::scaleVolumeToGroup 0x1002cd30, a `ret 4`: called from two dead group functions;
  - CDSMgr::get_eax 0x10012020: called from the dead Dll_Wave_Device::get_eax;
  - CMidiFile::reload 0x1001e120: the tail-jump target of the dead CMidiMgr::reload.
- Two more rows (jgld.dll 0x10091200 and Terrain.dll 0x10024ad0, SMSG `$$$00001`) are not functions. Each is the first
  range of _strchr in our lists: `lea eax, [edx - 1]; pop ebx; ret`, reached by a `je` inside _strchr.

## Rules (S10.9's, with the class pin adapted to dead code)
S10.9 pins the class by an installer or a named caller. A dead body has neither, so the class pin is one of these:
- VT: the body stores a vtable. Here SMSG's public at that table names the class, or our names do (vtbl_MixBuffer,
  vt_DeviceBaseIntermediate).
- THIS: the body passes `this` unchanged to a method we name (Midi_Device::reload -> Midi_Device::loadFile).
- NEIGHBOUR: the nearest named functions below and above the body that carry a class prefix carry the same class C of
  ours, and C is the class 9_tools/classmap.py maps SMSG's class to (log/roadmap/S10/13_neighbours.tsv). VC6 lays out
  the functions of one object file contiguously. Names without a class prefix are skipped and listed in the file
  (mapDsError, stubReturnBA).
- FREE: SMSG names a free function, so there is no class to pin.
The verb rules are S10.9's: ACCEPT_GET, ACCEPT_SET and ACCEPT_VT, plus hand verdicts. A hand verdict rests on the IAT
slots called (midiOutOpen, waveInAddBuffer, MessageBoxA, the IDirectSoundBuffer slots Restore / GetCurrentPosition /
Unlock), on our named callees, and on sibling fields. The ledger is log/roadmap/S10/13_ledger.tsv and the hand verdicts
are in log/roadmap/S10/13_manual.tsv.

Every accepted row's purpose ends `Dead in this build`. When the tracker takes these rows they are C2 end-state `dead`
candidates (re/CONFIDENCE.md), not C3 candidates.

## Counts
Accepted 51 (ACCEPT_GET 5, ACCEPT_SET 9, ACCEPT_VT 7, hand 30). Rejected 94:
- 58 with no class pin;
- 29 stubs: constant or empty bodies, including the seven Dll_Wave_Device EAX accessors and update_sound;
- 4 whose verb the body does not support;
- 1 forwarder to a constant stub;
- 2 that are parts of _strchr.

## Findings
- update_sound is reachable (its thunk is stored at 0x1000932f) but is missing from our function list, because the
  body is a single `ret`. The other 142 sound.dll bodies are dead.
- The dead Wave_Device / VoiceBase group functions give the layout of the 16 sound-group records of 0x18 bytes at
  VoiceBase + 0x24: +0x24 enabled byte, +0x25 mute byte, +0x28 level 0..0x7f, +0x2c member list, +0x38 count.
  VoiceBase_dtor destructs an embedded array at +0x24.
- DsBuffer::setsamplesize stores channels * bits into the block-align word +0xb4, without the division by 8.
  DsBuffer::setnumchannels accepts only 0 and 1.
- Seq::TT_xpose_trackset is the dead twin of the store loop in Seq_cmdHandler10 (0x10023b1a). Track +0x45c is the
  pitch offset that Midi_Device::noteOn adds (0x100217be).
