# sound.dll — ID sound_6 (round 3, the small functions rounds 1-2 left unnamed)

Third naming pass over `original\sound.dll` (release build, image base 0x10000000, addresses are RVAs).
The 221 functions in `log/naming/sound_6.txt` are the leftovers rounds 1-2 skipped: the member functions
of the C++ object families whose constructors, destructors and vtables earlier rounds had already named
(Seq / Sound / MidiReader / the device-base wrappers / the SubChannel channel model), plus a generic
linked-list container and a note-mapping control-object family. 106 are named with evidence in
`re/names/sound_6.tsv`; 14 globals/vtables in `re/names/sound_6_globals.tsv`; the 115 left unnamed are in
`log/naming/sound_6_notes.txt`. Facts only. Class names written `<Role><vtableSuffix>` (e.g. `Ch5c178`)
follow the house convention used by rounds 1-2 (`Sound5b6a8`, `Sound5c450`) for a class identified only
by the RVA of the vtable it installs. Several bodies are byte-identical matched sources in
`re/match/sound_raw_02.cpp` / `sound_raw_03.cpp`.

## 1. Generic pointer list (`PtrList`, subsystem util)

A doubly-linked list of heap nodes recurs throughout the MIDI/sequencer code, embedded at a fixed offset
inside each owner object (most often owner+0xc, so the owner's cursor/count land at owner+0x14/+0x18).

- Header: `{head@0, tail@4, cursor@8, count@0xc}`.
- Node (operator_new 0xc bytes): `{prev@0, next@4, value@8}`.

Operations: `PtrList_init` 0x1001d130 (zero the header), `PtrList_pushBack` 0x1001d150 (append at tail),
`PtrList_pushFront` 0x1001d1d0 (prepend at head), `PtrList_insert` 0x1001d250 (link relative to the
cursor), `PtrList_deleteAll` 0x1001bb10 (free every node and call each element's vtbl slot0(1) deleting
destructor), `PtrList_freeNodesA` 0x1001f1c0 / `PtrList_freeNodesB` 0x10031ac0 (free nodes only, emitted
as unwind funclets). `ResourceList_pushBack` 0x10031b60 is another pushBack instantiation (caller
`loadWaveResource`). `nodeListAppendValue` 0x10038430 (named earlier) is the append used by
`DsDevice_registerBuffer` 0x10032410.

## 2. Control-object / note-map family (`Ctrl*`, subsystem audio)

A family of small polymorphic objects carrying a 4cc tag 0x6c727463 and an embedded `CtrlObject` base
(vtable 0x1005b19c, round 2) at a fixed offset, chained through a `PtrList`. They implement a MIDI
value-mapping chain.

### 2a. The value-map object (vtable 0x1005b5c0, 0xe4 bytes)

`Ctrl5b5c0_ctor` 0x1001bd90 installs vtable 0x1005b5c0 (embedded base 0x1005b59c at +0x2c) and fills a
128-byte identity table at +0x5c (0..0x7f) — the MIDI value range. Methods:

- `Ctrl5b5c0_transposeTable` 0x1001c1e0: SNDERR 10 if arg>9, else rotates the 128-byte table in 12-entry
  (octave) blocks by arg*12, masked &0x7f.
- `Ctrl5b5c0_buildTable` 0x1001c400: writes a mod-12 permutation into the table from an int array (using
  the accumulator at +0xe0), then calls transposeTable(0).
- `Ctrl5b5c0_mapValue` 0x1001c370: walks the element chain at +0xc, applies each element of type(+0x1c)
  3 or 4 through its vtbl+0x10, stops at type 5, and returns the table lookup +0x5c[value].
- `Ctrl5b5c0_resetElements` 0x1001bbb0 / `CtrlBase_resetElements` 0x1001c2a0 / `CtrlBase_flushElements`
  0x1001c2f0: iterate the chain calling each element's vtbl slot 0x14 (or 0x18).

### 2b. Chain element objects

Leaf elements carry a `type` at +0x1c read by `Ctrl5b5c0_mapValue`:
`Ctrl5b5f0_ctor` 0x1001c5c0 (type 5, the chain terminator; vtable 0x1005b5f0),
`Ctrl5b668_ctor` 0x1001cbf0 (type 1; vtable 0x1005b668),
`Ctrl5b638_ctor` 0x1001c870 (type 6; vtable 0x1005b638, embedded base 0x1005b614),
`Ctrl5b19c_ctor` 0x1001bee0 (stores 12 bytes of params at +0x5c..+0x67).

### 2c. Byte reader (reader base, vtable 0x1005b58c)

`streamReadByte` 0x1001c7a0 / `streamConsumeByte` 0x1001c7e0 read one byte from a buffer described by
`{buffer@+0xb, pos@+0xc, size@+0xf}`, calling a refill virtual (vtbl+0x14) when the cursor reaches the
end and caching the last byte at +0x19. This is the SMF reader's byte source. `Ctrl5b58c_dtor` 0x1001c080
/ `Ctrl5b58c_dtorEmbedded` 0x1001c990 / `MidiReader_freeContent` 0x1001b870 install vtable 0x1005b58c and
clear the element list.

### 2d. Destructors

Each derived class emits a base-class destructor that only resets the vtable to 0x1005b19c
(`CtrlObject_dtorA..D` 0x1001c560/0x1001c710/0x1001cbd0/0x1001ccd0) and a scalar deleting destructor
(`CtrlObject_scalarDtorA..D` 0x1001c530/0x1001c6e0/0x1001cba0/0x1001cca0). The reader/chain classes with
real cleanup are `ChainObject_dtor` 0x1001b570 / `ChainObject_scalarDtor` 0x1001bea0 and
`Ctrl5b58c_scalarDtor` 0x1001c950. `Ctrl_appendIfAccepted` 0x1001b9a0 adds a value to the +0xc chain only
when the owner's vtbl slot0 accepts it (else SNDERR 0x21).

## 3. Sequencer helpers (`Seq_*`, subsystem audio)

Member functions of the player object (ctor 0x1001f6f0, vtable 0x1005b6a8, round 3 of round 2) reached
through its vtable (no direct callers). Fields confirmed by round 2: +0x19c embedded reader list,
+0x218 track list, +0x60 reader/track container, +0x208/+0x204/+0x20c/+0x214 play range and flags, +0x730
a 0..0x7f level, +0x238 track array. The vtbl+0xb4 virtual returns the track count (used as a bound).

- `Seq_freeChannelObjects` 0x1001abd0: destroys the 0x7f-entry channel-object slot array and frees it.
- `Seq_setPlayRange` 0x1001ae00, `Seq_setCurrentIndex` 0x10022200 (bound-checked against vtbl+0xb4),
  `Seq_setField730Clamped` 0x10020f50 → `Seq_applyChannelLevel` 0x1002b260 (stores arg&0x7f at +4 and
  forwards to the inner device vtbl+0x40), `Seq_setField240` 0x100227a0.
- `Seq_clearTrackFlag8All` 0x1001f150 (clears +0x38 bit3 on every track), `Seq_removeTrackById`
  0x100228d0 (SNDERR 0x24 if absent), `Seq_getTrackFlagHigh` 0x10023f10, `Seq_getListElementAt`
  0x100241f0, `Sound5b6a8_scalarDtor` 0x1001f6c0.
- `nextRandomFloat` 0x100285a0 is the LCG float generator (constants 0x19660d / 0x3c6ef35f, bias g_float1
  0x1005b8a8 = 1.0) used by `Seq_buildPlayOrder` to shuffle the play order.

## 4. Device base wrapper (VoiceBase / MidiDeviceBase / WaveInDeviceBase, subsystem audio)

Three sibling classes (ctors 0x10029020 / 0x10029e70 / 0x1002a240, round 2) share an intermediate base
vtable `vt_DeviceBaseIntermediate` 0x1005b8b0 and wrap an inner device object at +0x14, forwarding each
virtual to it and returning an SNDERR default when the inner pointer is null.

- Open: `VoiceBase_openInner` 0x100292d0 / `MidiDeviceBase_openInner` 0x10029f90 /
  `WaveInDeviceBase_openInner` 0x1002a420 (this vtbl+4, then inner vtbl+0x10, then this vtbl+8).
- Close: `VoiceBase_closeInner` 0x10029320 / `MidiDeviceBase_closeInner` 0x1002a000 /
  `WaveInDeviceBase_closeInner` 0x1002a470 (inner vtbl+0x14 release, null +0x14).
- Destruction: `DeviceBase_scalarDtor` 0x10028fb0, the identical base dtors `DeviceBase_dtorA..C`
  0x10028ff0/0x10029f60/0x1002a3b0, `VoiceBase_dtor` 0x10029200 (array-destructs the embedded array at
  +0x24), and the per-class scalar dtors 0x10029140/0x10029f30/0x1002a380. `VoiceBase_clearList`
  0x10029170 frees the VoiceBase list at +8.
- The remaining pure forwarders (inner vtbl slot + null default only) are listed in the notes file with
  their slot and default; naming them as concrete operations needs the inner device's vtable map, not yet
  established.

## 5. SubChannel channel/voice object model (subsystem audio)

`SubChannel` (ctor 0x1002b4e0, vtable `vtbl_SubChannel` 0x1005bae0, round 2) is the base of a family of
playback channels. Each derived channel ctor installs 0x1005bae0 then its own vtable and sets a Sound
source type (`Sound_setSourceType` 0x1002ad10). The derived vtables found this round:

| class | vtable | ctor | dtor / scalar dtor |
|---|---|---|---|
| SubChannel | 0x1005bae0 | 0x1002b4e0 (r2) | 0x1002ac00 / 0x1002abd0 |
| Mixer | 0x1005bdd8 | `Mixer_ctor` 0x1002c9f0 | 0x1002cb00 (r2) / 0x1002cad0 |
| Ch5bb90 | 0x1005bb90 | 0x1002c490 | 0x1002b620 / 0x1002b5f0 |
| Ch5c000 | 0x1005c000 | 0x1002e550 | — |
| Ch5c178 | 0x1005c178 | 0x1002e930 | 0x1002ea30 / 0x1002ea00 |
| Ch5c234 | 0x1005c234 | — | 0x1002ef40 / 0x1002ef10 |
| Ch5c2ec | 0x1005c2ec | — | 0x1002f3f0 / 0x1002f3c0 |
| Ch5be88 | 0x1005be88 | `Ch5be88_ctor` 0x1002d450 | 0x1002d560/0x1002e660, scalars 0x1002d530/0x1002e630 |

`Ch5be88_ctor` is embedded in both a Sound (by `Sound5b6a8_ctor` 0x1001f4a0) and the player (by `Seq_ctor`
0x1001f6f0), so 0x1005be88 is the base voice object those two own. The channel destructors free the Sound
at +0x40 (`delete_sound` 0x100097d0) and owned blocks at +0x50.

Each channel exposes a source-open virtual that creates a Sound of a fixed source type at +0x40:
`SubChannel_openSourceType2` 0x1002deb0 (also plays via `Engine_playSound`), `openSourceType4` 0x1002eb40,
`openSourceType6` 0x1002f060, `openSourceType7` 0x1002f5e0. Other named channel ops: `Channel_start`
0x10031080 (posts wave command 1 to the global wave device when it accepts), `enqueueWaveCmdE` 0x1002fb70
(posts command 0xe), `Channel_resetPanState` 0x10033ba0, `Channel_setFormat` 0x10030fb0 (copies
`g_defaultWaveFormat` 0x10063ea0 when no format given), `Channel_parseWaveHeader` 0x10033990,
`Channel_applyFlags` 0x1002c040, `Channel_propagateFlag0` 0x1002ddb0, `Channel_releaseSound` 0x1002dff0,
`Sound_assignName` 0x1002ddf0, `unlinkFromActiveList` 0x1002b090 (maintains the global list head
`g_activeListHead` 0x100b5070 via +0x48 prev / +0x4c next). `MultiSound_setSlotSource` 0x1002f8d0 and
`MultiSound_freeSlots` 0x1002ce60 manage a 0x28-entry (40) sub-sound slot array at +0x58.

## 6. Wave-input stream (subsystem audio)

`WaveInStream_start` 0x10031450 (SNDERR 0x13 if the global wave-in device is absent, else
`Wave_In_Device::op1`) and `WaveInStream_resetCodec` 0x100319e0 (resets the +0x80 counters, allocates a
codec context into +0x90, restarts) drive capture. `DsDevice_registerBuffer` 0x10032410 appends a
DirectSound buffer to the device list at +0x1338.

## Globals (`re/names/sound_6_globals.tsv`)

The 13 vtables above plus `g_activeListHead` 0x100b5070, `g_defaultWaveFormat` 0x10063ea0 and `g_float1`
0x1005b8a8. Two runtime globals are referenced but not recorded (no writer identified this pass):
`DAT_100b4a20` (the active wave device the channels post commands to) and `DAT_100b4a24` (a wave-in
device-present flag) — see the notes file.
