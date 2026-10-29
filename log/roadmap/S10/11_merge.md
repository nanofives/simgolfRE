# S10.11 merge items (for the S10 lead / orchestrator)

S10.11 wrote no tracker, Ghidra, port, shim or shared file and edited no existing file. Its new files are listed in 11_files.txt.
Source: the demo sound.dll's NB11 CodeView records (log/roadmap/S10/11_dl, gitignored), read by re/tools/cv_debug.py.

## 1. New files, ready to commit
| file | content |
|---|---|
| re/tools/cv_debug.py | our NB09 / NB11 CodeView reader (procs, publics, data, UDTs, class layouts with vtable offsets) |
| tests/test_cv_debug.py | static: a synthetic NB11 PE (symbols, class layout, numeric leaves, refusals), plus the demo counts when the extracted file exists |
| re/names/demo_sound_1.tsv | 13 sound.dll rows (no `_globals.tsv`: names_validate prints `missing`, not an error) |
| re/analysis/systems/demo_sound_1.md | the writeup: source, method, rules, rejections |

Gate as run on 2026-10-29. Each command passes:
```
~/bin/py -3.12 re/tools/names_sanitize.py re/names/demo_sound_1.tsv re/analysis/systems/demo_sound_1.md   # 0 replaced
~/bin/py -3.12 re/tools/names_validate.py demo_sound_1                                                    # bad 0, cross-file duplicates 0
~/bin/py -3.12 re/tools/names_check_evidence.py re/names/demo_sound_1.tsv                                 # 0 checked, 0 bad
~/bin/py -3.12 log/roadmap/S10/11_tools/check_cites.py re/names/demo_sound_1.tsv                          # 50 in-body citations, 0 bad
~/bin/py -3.12 -m pytest tests/test_cv_debug.py -q                                                         # 5 passed
```
Run names_validate again after wave 1's smsg_* files merge (cross-file duplicates are checked against every names file
on disk; it read 0 with the tree as it stood at 18:00).

## 2. Tracker and master, in this order (same recipe as 9_merge.md section 2)
1. Back up the master. Then run ApplyNames on sound.dll without `overwrite`: all 13 targets are `FUN_`.
2. Re-export sound.dll's function list with ListFunctions, then delete log/xref_sound.dll.json.
3. Run these:
   ```
   ~/bin/py -3.12 scripts/re_classify.py batch re/names/demo_sound_1.tsv --to C1
   ~/bin/py -3.12 re/tools/c2_note.py --all --out log/c2_batch_s10_11.tsv
   ~/bin/py -3.12 scripts/re_classify.py batch log/c2_batch_s10_11.tsv --to C2
   ```
   The five set_total_class_size bodies are 8 bytes, so c2_note promotes them. Bodies of 5 bytes or less stay at C1,
   and none of the 13 is that small.

## 3. Renames (decision D1: rows marked P rename port keys too)
Each rename below needs ApplyNames with `overwrite` on sound.dll, then `re_classify.py retag`.

### 3a. The LPC-10 codec: 42 rows, all P
log/roadmap/S10/11_renames.tsv has the full evidence per row (file, addr, old, new, evidence, port).
- Every retail function here is instruction-identical to the demo procedure, with relocated operands masked.
  voicin_ is identical up to the switch table after its code.
- Those demo procedures sit in the demo's `.\Debug\<routine>.obj` modules, and the demo symbol is the routine's C
  name: lpc10_decode, analys_, dyptrk_ and so on.
- Our names are behavioural (sound_18 b6 and earlier), so these renames are the lead's choice, not a correction. The
  instructions agree with both spellings.

| file | addr | old | new |
|---|---|---|---|
| sound_18.tsv | 0x1003a980 | codecDecodeFrame | lpc10_decode |
| sound_7.tsv | 0x1003aa50 | codecSetParams | lpcini_ |
| sound_18.tsv | 0x1003aa90 | codecAllocEncoderContext | create_lpc10_encoder_state |
| sound_18.tsv | 0x1003aae0 | codecInitEncoderContext | init_lpc10_encoder_state |
| sound_18.tsv | 0x1003aea0 | codecAllocDecoderContext | create_lpc10_decoder_state |
| sound_18.tsv | 0x1003aef0 | codecInitDecoderContext | init_lpc10_decoder_state |
| sound_18.tsv | 0x1003b150 | codecEncodeFrame | lpc10_encode |
| sound_18.tsv | 0x1003b230 | codecSynthesizeFrame | synths_ |
| sound_18.tsv | 0x1003b5a0 | codecDecodeParams | decode_ |
| sound_18.tsv | 0x1003bf30 | bitPermute | chanwr_0_ |
| sound_5.tsv | 0x1003c180 | bitPack | chanwr_ |
| sound_5.tsv | 0x1003c1d0 | bitUnpack | chanrd_ |
| sound_7.tsv | 0x1003c220 | quantizeParams | encode_ |
| sound_18.tsv | 0x1003c640 | codecAnalyzeFrame | analys_ |
| sound_18.tsv | 0x1003d040 | codecHighPassFrame | prepro_ |
| sound_18.tsv | 0x1003d0a0 | codecSynthPostFilter | deemp_ |
| sound_7.tsv | 0x1003d1d0 | lpcSynthesize | bsynz_ |
| sound_7.tsv | 0x1003d8d0 | reflectionToLpc | irc2pc_ |
| sound_7.tsv | 0x1003da50 | interpSubframeParams | pitsyn_ |
| sound_7.tsv | 0x1003e2c0 | intPow | pow_ii |
| sound_7.tsv | 0x1003e380 | floatCopySign | r_sign |
| sound_18.tsv | 0x1003e400 | floatRoundHalfAway | i_nint |
| sound_7.tsv | 0x1003e480 | medianOfThree | median_ |
| sound_18.tsv | 0x1003e520 | codecCorrectNibble | ham84_ |
| sound_18.tsv | 0x1003e600 | codecRevertUnstableCoeffs | rcchk_ |
| sound_18.tsv | 0x1003e6d0 | codecLdlSolveClamped | invert_ |
| sound_18.tsv | 0x1003e9f0 | codecCovarianceMatrix | mload_ |
| sound_8.tsv | 0x1003ec60 | codecRmsEnergy | energy_ |
| sound_8.tsv | 0x1003ed00 | codecRemoveMean | dcbias_ |
| sound_9.tsv | 0x1003edc0 | codecAlignAnalysisWindow | placea_ |
| sound_18.tsv | 0x1003f310 | codecPitchTrackDp | dyptrk_ |
| sound_18.tsv | 0x1003f720 | codecVoicingDecision | voicin_ |
| sound_18.tsv | 0x10040000 | codecRefinePitchLag | tbdm_ |
| sound_18.tsv | 0x10040330 | codecInverseFilter2 | ivfilt_ |
| sound_18.tsv | 0x100404d0 | codecLowPassFir31 | lpfilt_ |
| sound_9.tsv | 0x10040740 | codecSelectWindowFromJumps | placev_ |
| sound_9.tsv | 0x10040ab0 | codecMarkCorrelationJumps | onset_ |
| sound_18.tsv | 0x10040da0 | codecPreEmphasis | preemp_ |
| sound_18.tsv | 0x10040e30 | codecHighPassFilter | hp100_ |
| sound_8.tsv | 0x10040f60 | codecRing5Add | random_ |
| sound_9.tsv | 0x10041020 | codecWindowFeatures | vparms_ |
| sound_18.tsv | 0x100414f0 | codecAmdfPitch | difmag_ |

The CRT routines among the identity matches already carry the same names up to C decoration: strcat, strlen and the
other 13. Two are kept on purpose. `$$$00001` is chkstk.obj's local label, so _alloca_probe stays. _stricmp and
__strcmpi are two publics of one body.

### 3b. Placeholder names the demo layout replaces (3 rows)
Each row is a paired slot whose demo and retail bodies touch the same fields (EQUAL, log/roadmap/S10/11_slotfeat.tsv).
The demo layout names the field the retail body writes.

| file | addr | old | new | evidence | port |
|---|---|---|---|---|---|
| sound_6.tsv | 0x10020f50 | Seq_setField730Clamped | Seq::set_volume | clamps the argument to 0x7f (0x10020f56, 0x10020f5d) and stores it at this+0x730 (0x10020f68); demo CMidiMgr +0x730 m_volume; paired demo slot body CMidiMgr::set_volume; CMidiMgr is our Seq (classmap.py) | P |
| sound_6.tsv | 0x100227a0 | Seq_setField240 | Seq::set_nswitch_threads | stores the argument at this+0x240 (0x100227a4), returns 0 (0x100227aa); demo CMidiMgr +0x240 m_nswitch_threads; paired demo slot body CMidiMgr::set_nswitch_threads | P |
| sound_11.tsv | 0x1002bff0 | Channel_setField5cClamped1200 | Wave::set_pitch | clamps the argument to -1200..1200 (0x1002bff4, 0x1002c002), stores it at this+0x5c (0x1002c00e) and passes it to the object at this+0x40 (0x1002c011); demo Wave +0x5c m_pitch, +0x40 m_pdllsnd; paired demo slot body Wave::set_pitch | P |

### 3c. Not proposed: 42 other paired slots whose names row differs from the demo method
These rows are in log/roadmap/S10/11_rename_candidates.tsv: the slot pairs are EQUAL, but our name differs from the demo's
in its method part (startStream against CAviMgr::start, registerSound against CMidiMgr::attach, ...). Each is a review
item for a later pass. This step did not check their verbs.

## 4. Class layout proposals for re/model (sound.dll), 72 fields in 23 demo classes
Every row comes from the same kind of paired slot body. It gives the RETAIL instruction that reads (r) or writes (w)
`[this + offset]`. `this` is ecx at entry, or a register copied from it. The member name, type and declaring class come
from the demo record, after flattening bases and embedded members (log/roadmap/S10/11_tools/layouts.py).

For a secondary vtable (`??_7C@@6BB@@@`), `this` is the B subobject. For example, Chooser's MidiCtrl part sits at
Chooser+0x2c, so offsets on that table are B's.

The width column is the operand size at that instruction. The demo type is the debug build's. Retail class sizes are
confirmed only for the five classes whose set_total_class_size constant equals the demo size (MidiCtrlDiskRead 0x28,
Stepper 0x38, Randomizer 0x40, Chooser 0x60, ChannelCtrl 0x38). Treat the rest as proposals (U-1900).

Source TSV: log/roadmap/S10/11_fields.tsv (also gives the demo method and the number of supporting accesses).

| demo class | our prefix | offset | member | type (demo) | declared in | retail instruction | r/w | width | retail function (our name) |
|---|---|---|---|---|---|---|---|---|---|
| AviSound | - | 0x40 | m_pdllsnd | Sound* | Sound | 0x1002eb43 | r | 4 | 0x1002eb40 openSourceType4 |
| CAviMgr | - | 0x58 | m_flags | AviFlags | CAviMgr | 0x10006b00 | r | 4 | 0x10006b00 startStream |
| CAviMgr | - | 0xb4 | m_frame_size | ulong | CAviMgr | 0x10006db0 | r | 4 | 0x10006db0 CAviMgr::get_frame_size |
| CAviMgr | - | 0xc8 | m_cur_frame | int | CAviMgr | 0x10006d96 | r | 4 | 0x10006d90 CAviMgr::get_cur_frame |
| CAviMgr | - | 0xcc | m_start_frame | int | CAviMgr | 0x10006d90 | r | 4 | 0x10006d90 CAviMgr::get_cur_frame |
| CMidiMgr | Seq | 0x20c | m_control_track | ulong | CMidiFile | 0x10005f90 | r | 4 | 0x10005f90 Seq::get_control_track |
| CMidiMgr | Seq | 0x214 | m_flags | mmgr_flags | CMidiMgr | 0x10005e0e | r | 4 | 0x10005e00 registerSound |
| CMidiMgr | Seq | 0x240 | m_nswitch_threads | int | CMidiMgr | 0x100227a4 | w | 4 | 0x100227a0 Seq_setField240 |
| CMidiMgr | Seq | 0x71c | m_time_ticks | ulong | CMidiMgr | 0x10021370 | r | 4 | 0x10021370 Seq::get_time |
| CMidiMgr | Seq | 0x730 | m_volume | ulong | CMidiMgr | 0x10020f68 | w | 4 | 0x10020f50 Seq_setField730Clamped |
| CMidiMgr | Seq | 0x738 | m_current_active_set | ulong | CMidiMgr | 0x100231a6 | r | 4 | 0x100231a0 Seq::set_active_tracks |
| CMidiMgr | Seq | 0x73c | m_key | int | CMidiMgr | 0x10005fb0 | r | 4 | 0x10005fb0 Seq::get_key |
| CMidiMgr | Seq | 0x740 | m_flic_frame | uint | CMidiMgr | 0x10006000 | r | 4 | 0x10006000 Seq::get_frame |
| CVoiceRxMgr | - | 0x58 | m_flags | VRx_Flags_tag | CVoiceRxMgr | 0x10031083 | r | 1 | 0x10031080 Channel_start |
| CVoiceRxMgr | - | 0xb4 | m_buffer_size | ulong | CVoiceRxMgr | 0x100311b0 | r | 4 | 0x100311b0 CVoiceRxMgr::get_buffer_size |
| CVoiceTxMgr | - | 0x6c | m_buffer_size | ulong | CVoiceTxMgr | 0x10031456 | w | 4 | 0x10031450 WaveInStream_start |
| CVoiceTxMgr | - | 0x70 | m_pbuffer_out | _MMIOINFO* | CVoiceTxMgr | 0x10031967 | r | 4 | 0x10031960 CVoiceTxMgr::return_buffer |
| CVoiceTxMgr | - | 0x80 | m_buffers_processed | int | CVoiceTxMgr | 0x100319eb | w | 4 | 0x100319e0 WaveInStream_resetCodec |
| CVoiceTxMgr | - | 0x84 | m_buffers_taken | int | CVoiceTxMgr | 0x100319f2 | w | 4 | 0x100319e0 WaveInStream_resetCodec |
| CVoiceTxMgr | - | 0x88 | m_buffers_returned | int | CVoiceTxMgr | 0x100319fe | w | 4 | 0x100319e0 WaveInStream_resetCodec |
| CVoiceTxMgr | - | 0x8c | m_start_id | int | CVoiceTxMgr | 0x100319e5 | r | 4 | 0x100319e0 WaveInStream_resetCodec |
| CVoiceTxMgr | - | 0x90 | m_lpc10_state | lpc10_encoder_state* | CVoiceTxMgr | 0x10031a09 | w | 4 | 0x100319e0 WaveInStream_resetCodec |
| ChannelCtrl | - | 0x8 | m_total_class_size | int | MidiCtrl | 0x1001cb80 | w | 4 | 0x1001cb80 ChannelCtrl::set_total_class_size |
| ChannelCtrl | - | 0x1c | m_type | enum MIDI_CONTROL | MidiCtrl | 0x1001cb64 | w | 4 | 0x1001cb60 ChannelCtrl::set_type |
| Dll_Midi_Device | Midi_Device | 0x24 | m_flags | DllMidiDeviceFlags | Dll_Midi_Device | 0x1000a1b3 | r | 1 | 0x1000a1b0 Midi_Device::enableOutput |
| Dll_Midi_Device | Midi_Device | 0x2c | m_ptimer | CTimer* | Dll_Midi_Device | 0x1000a140 | r | 4 | 0x1000a140 Midi_Device::suspend |
| Dll_Midi_Device | Midi_Device | 0x30 | m_rate | ulong | Dll_Midi_Device | 0x1000afa4 | w | 4 | 0x1000afa0 Midi_Device::set_rate |
| Dll_Midi_Device | Midi_Device | 0x34 | m_time_past | ulong | Dll_Midi_Device | 0x1000af24 | r | 4 | 0x1000af20 Midi_Device::add_to_time_past |
| Dll_Wave_Device | Wave_Device | 0x1a4 | m_flags | DllWaveDeviceFlags | Dll_Wave_Device | 0x1000be03 | r | 1 | 0x1000be00 Wave_Device::closeOutput |
| Dll_Wave_Device | Wave_Device | 0x1a8 | m_ptimer | CTimer* | Dll_Wave_Device | 0x1000e7f0 | w | 4 | 0x1000e7f0 Wave_Device::timer_unhook |
| Dll_Wave_Device | Wave_Device | 0x1ac | m_rate | ulong | Dll_Wave_Device | 0x1000e814 | w | 4 | 0x1000e810 Wave_Device::set_rate |
| Dll_Wave_Device | Wave_Device | 0x1b0 | m_time_past | ulong | Dll_Wave_Device | 0x1000e1d4 | r | 4 | 0x1000e1d0 Wave_Device::add_to_time_past |
| Dll_Wave_Device | Wave_Device | 0x1b4 | m_time_since_last_update | ulong | Dll_Wave_Device | 0x1000e1dc | w | 4 | 0x1000e1d0 Wave_Device::add_to_time_past |
| Dll_Wave_In_Device | Wave_In_Device | 0x30 | m_ptimer | CTimer* | Dll_Wave_In_Device | 0x1000ec03 | r | 4 | 0x1000ec00 Wave_In_Device::closeCapture |
| Dll_Wave_In_Device | Wave_In_Device | 0x34 | m_rate | ulong | Dll_Wave_In_Device | 0x1000ee34 | w | 4 | 0x1000ee30 Wave_In_Device::set_rate |
| Dll_Wave_In_Device | Wave_In_Device | 0x38 | m_time_past | ulong | Dll_Wave_In_Device | 0x1000ece0 | w | 4 | 0x1000ece0 unnamed |
| Dll_Wave_In_Device | Wave_In_Device | 0x3c | m_time_since_last_update | ulong | Dll_Wave_In_Device | 0x1000ed09 | w | 4 | 0x1000ed00 Wave_In_Device::add_to_time_past |
| Midi | - | 0x40 | m_pdllsnd | Sound* | Sound | 0x1002d690 | r | 4 | 0x1002d690 Midi::play |
| MidiCtrl | - | 0x8 | m_total_class_size | int | MidiCtrl | 0x1001be80 | w | 4 | 0x1001be80 NoteMap::set_total_class_size |
| MidiCtrl | - | 0x19 | m_flags.state | uchar | MidiCtrlFlags | 0x10005c70 | w | 1 | 0x10005c70 unnamed |
| MidiCtrl | - | 0x1c | m_type | enum MIDI_CONTROL | MidiCtrl | 0x1001be64 | w | 4 | 0x1001be60 NoteMap::set_type |
| MidiCtrlDiskRead | - | 0x8 | m_total_class_size | int | MidiCtrl | 0x10005d40 | w | 4 | 0x10005d40 MidiCtrlDiskRead::set_total_class_size |
| Midi_Device | MidiDeviceBase | 0x14 | m_pdll_device | Sound_Device* | Sound_Device | 0x10029fa2 | r | 4 | 0x10029f90 MidiDeviceBase_openInner |
| Randomizer | - | 0x8 | m_total_class_size | int | MidiCtrl | 0x1001c6c0 | w | 4 | 0x1001c6c0 Randomizer::set_total_class_size |
| Randomizer | - | 0x19 | m_flags.state | uchar | MidiCtrlFlags | 0x1001c7c3 | w | 1 | 0x1001c7a0 streamReadByte |
| Randomizer | - | 0x1c | m_type | enum MIDI_CONTROL | MidiCtrl | 0x1001c6a4 | w | 4 | 0x1001c6a0 Randomizer::set_type |
| Randomizer | - | 0x2c | m_on_off_table | uchar* | Randomizer | 0x1001c7b7 | r | 4 | 0x1001c7a0 streamReadByte |
| Randomizer | - | 0x30 | m_cur_index | uint | Randomizer | 0x1001c7a3 | r | 4 | 0x1001c7a0 streamReadByte |
| Randomizer | - | 0x3c | m_range | uint | Randomizer | 0x1001c7a6 | r | 4 | 0x1001c7a0 streamReadByte |
| Sound | SubChannel | 0x40 | m_pdllsnd | Sound* | Sound | 0x1002b136 | r | 4 | 0x1002b130 SubChannel::unload |
| Sound | SubChannel | 0x44 | m_sf | SoundFlags_tag | Sound | 0x100056a0 | r | 4 | 0x100056a0 SubChannel::is_loaded |
| Sound | SubChannel | 0x48 | m_pprev | Sound* | Sound | 0x100056e0 | r | 4 | 0x100056e0 SubChannel::get_prev |
| Sound | SubChannel | 0x4c | m_pnext | Sound* | Sound | 0x100056c0 | r | 4 | 0x100056c0 SubChannel::get_next |
| Sound | SubChannel | 0x54 | m_type | enum SOUNDTYPE | Sound | 0x10005700 | r | 4 | 0x10005700 SubChannel::get_type |
| Stepper | - | 0x8 | m_total_class_size | int | MidiCtrl | 0x1001c510 | w | 4 | 0x1001c510 Stepper::set_total_class_size |
| Stepper | - | 0x1c | m_type | enum MIDI_CONTROL | MidiCtrl | 0x1001c4f4 | w | 4 | 0x1001c4f0 Stepper::set_type |
| Stepper | - | 0x30 | m_add | int | Stepper | 0x1001c5a0 | r | 4 | 0x1001c5a0 Stepper::update |
| Stepper | - | 0x34 | m_cur_add | int | Stepper | 0x1001c580 | r | 4 | 0x1001c580 Stepper::do_control |
| TrackCtrl | - | 0x8 | m_total_class_size | int | MidiCtrl | 0x1001cc80 | w | 4 | 0x1001cc80 TrackCtrl::set_total_class_size |
| TrackCtrl | - | 0x1c | m_type | enum MIDI_CONTROL | MidiCtrl | 0x1001cc64 | w | 4 | 0x1001cc60 TrackCtrl::set_type |
| VoiceRx | - | 0x40 | m_pdllsnd | Sound* | Sound | 0x1002eee0 | r | 4 | 0x1002eee0 VoiceRx::get_nbuffers |
| VoiceTx | - | 0x40 | m_pdllsnd | Sound* | Sound | 0x1002f510 | r | 4 | 0x1002f510 VoiceTx::release |
| VoiceTx | - | 0x58 | m_flags | VTXFlags_tag | VoiceTx | 0x1002f533 | r | 1 | 0x1002f530 VoiceTx::start |
| Wave | - | 0x40 | m_pdllsnd | Sound* | Sound | 0x1002b5d0 | r | 4 | 0x1002b5d0 Wave::is_loaded |
| Wave | - | 0x60 | m_reverb_mix | float | Wave | 0x1002c274 | w | 4 | 0x1002c270 Ch5bb90_set3DBuffer |
| Wave | - | 0x64 | m_ms_length | uint | Wave | 0x1002ba80 | r | 4 | 0x1002ba80 Wave::get_ms_length |
| Wave_Device | VoiceBase | 0x14 | m_pdll_device | Sound_Device* | Sound_Device | 0x100292e2 | r | 4 | 0x100292d0 VoiceBase_openInner |
| Wave_In_Device | WaveInDeviceBase | 0x14 | m_pdll_device | Sound_Device* | Sound_Device | 0x1002a432 | r | 4 | 0x1002a420 WaveInDeviceBase_openInner |
| dllSoundList | - | 0xf8 | m_pCurSound | CWaveMgr* | dllSoundList | 0x1002f850 | r | 4 | 0x1002f850 dllSoundList::is_playing |
| dllSoundList | - | 0xfc | m_pQueuedSound | CWaveMgr* | dllSoundList | 0x1002fa63 | r | 4 | 0x1002fa60 dllSoundList::play |
| dllSoundList | - | 0x104 | m_curQueued | uint | dllSoundList | 0x1002f830 | r | 4 | 0x1002f830 dllSoundList::isAlreadyQueued |
| dllSoundList | - | 0x108 | m_curVolume | int | dllSoundList | 0x1002fd9d | w | 4 | 0x1002fd80 dllSoundList::set_scaled_volume |

## 5. Uncertainties for UNCERTAINTIES.md
U-1900..U-1904 are in log/roadmap/S10/11_uncertain.tsv, in UNCERTAINTIES.md columns.

## 6. Data not committed
log/roadmap/S10/11_dl/ is gitignored through log/*. It holds the download, the extracted cabinets and the demo
sound.dll (`36_sound.dll`, md5 ccf6c2f658ac6a2f83341d0fcc8a315c). The demo's other three modules were extracted too
(`iscab.py <Disk1> get <name> <outdir>`), and only sound.dll carries CodeView records in the file:
- golf.exe (24_golf.exe) has no debug directory entry.
- jgl.dll has an NB10 reference to `C:\jackdev\jgl\Release\jgl.pdb`.
- Terrain.dll has an NB10 reference to `C:\Projects\3DTerrainLowPoly\Debug\Terrain.pdb`.
No .pdb is in the installer's file list (11_dl/iscab_list.txt).
