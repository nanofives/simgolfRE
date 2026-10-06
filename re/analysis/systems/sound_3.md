# sound.dll — ID sound_3 (0x10018d10 – 0x10027ad0, 164 functions)

Second naming pass over `original\sound.dll`, covering the functions round 1 (sound_0/sound_1)
left unnamed in this address window. All addresses are RVAs on image base 0x10000000. Facts only;
unnamed functions and open questions are in `log/naming/sound_3_notes.txt`. Class vocabulary
(`Midi_Device`, `Wave_Device`, `Wave_In_Device`, `SNDERR`) comes from the DLL's export decoration,
as established by round 1. SNDERR integer codes seen here: 0 ok, 10 (0xa) bad arg, 0xb, 0x13 no
device, 0x14 not active, 0x22 queue/buffer full, 0x24 not found, 0x29 past end.

This range holds two subsystems: the **software voice mixer/resampler** (both ends of the range)
and the **MIDI/SMF sequencer and playback engine** (the middle), plus a shared doubly-linked-list
node container used by the sequencer.

## 1. Software voice mixer / resampler

A family of per-voice render routines turns a voice's PCM sample buffer into mixed output. They fall
into two groups selected by format.

### 1a. Accumulate/overwrite mixers (dispatcher-selected)

The dispatchers `FUN_10036280` and `FUN_100371d0` (both in sound_1's range, callers of this group)
pick a render routine by two voice fields and one count argument:

- voice +0x182 == 2 selects the stereo-input routine,
- else voice +0x18e == 8 selects the 8-bit-input routine,
- else the mono-16-bit routine;
- the dispatcher's count argument > 0xb selects the **overwrite** (set) variant, <= 0xb the
  **accumulate** (add) variant.

All six resample with linear interpolation and a per-channel gain ramp, reading the voice sample
buffer at +0x18 / +0xd8 (descriptor {data +0x18, len +0x1c, pos +0x20, loop +0x24}) and writing a
stereo `double*` mix accumulator.

| addr | name | input | output op |
|---|---|---|---|
| 0x10019770 | Voice_mixAddMono16 | mono 16-bit (short*) | `*out += s*gain` |
| 0x10019b20 | Voice_mixSetMono16 | mono 16-bit | `*out = s*gain` |
| 0x1001a910 | Voice_mixAddStereo16 | stereo 16-bit | add |
| 0x1001a6c0 | Voice_mixSetStereo16 | stereo 16-bit | overwrite |
| 0x1001a2c0 | Voice_mixAdd8bit | 8-bit (byte*) | add |
| 0x10019ec0 | Voice_mixSet8bit | 8-bit | overwrite |

The add-vs-overwrite split was confirmed from the output store (`*param_1 = s*g` with vs without
`+ *param_1`); the width from the sample-pointer type (short* / byte*); the channel count from the
stereo variant reading two interleaved streams.

### 1b. Streaming-ring renderers (reached through function pointers)

The remaining large routines (callers only through function pointers, like round 1's
`mixRenderRatedC` 0x10025190) read a voice through its streaming ring at +0x1358, resample by the
16.16 rate ratio at +0x220, write converted PCM through an output cursor, and store the advanced
cursor back to `*param_1`. They are distinguished here by input sample width (the finer
channel/interpolation distinction among same-width members was not separated):

- 16-bit: 0x10018d10, 0x100256c0, 0x10026260, 0x10027730, 0x10027ad0
- 8-bit: 0x10019060, 0x10019400, 0x10025c80, 0x10026d30
- 32-bit/packed (uint*): 0x10026800
- unity-rate copy paths (read +0x220 and ring position, no fractional sample fetch): 0x10027310,
  0x100274e0

## 2. MIDI / SMF sequencer

This is the bulk of the range. A **Sound** object (ctor 0x1001f4a0, dtor 0x1001ff50, vtable
0x1005b6a8) and a **sequencer/player** object (ctor 0x1001f6f0) drive SMF playback. The player
embeds an **SMF reader** (`MidiReader`, ctor 0x1001e750 / dtor 0x1001e7d0, vtable 0x1005b68c; base
`CtrlObject2` ctor 0x1001bca0, vtable 0x1005b590) at +0x18 or +0x60.

### Key structures (player `this`)

- +0x60: embedded reader / track container (used by `Seq_listGetAt` whose list head is at +0x19c).
- +0x218: track list head, +0x220 list cursor (used by the command handlers and finders).
- +0x214: play-mode/loop flag bits (bits 7-8 = play-order mode 0..2; 0x180 group; 0x200/0x400/0x800
  group).
- +0x238: track-index array, +0x23c its count; +0x248..+0x24c play-order range.
- +0x718 tempo, +0x720/+0x728 per-tick duration, +0x1bc ticks-per-unit.
- +0x744: 0x7f per-channel object slots; +0x1d1: 0x7f per-channel slots (Sound dtor).
- The command ring is at +0x50 (head +0x4050, tail +0x4054) — see round 1 (CommandQueue).

Track node (in the +0x218 / +0x60 lists): +4 id/key, +8 data pointer, +0x30/+0x34/+0x38 flags,
+0x50 next-trigger tick, +0x54 length. SMF reader fields: +0x2c byte cursor, +0x3c track data,
+0x40 tick, +0x4c delta, +0x5d tick scale.

### Parsing primitives

- `Midi_readVarLen` 0x1001e710 — the MIDI variable-length-quantity decoder.
- `Midi_scanTrack` 0x1001ef80 — scans an MTrk stream (VLQ + running status) accumulating delta time.
- `Midi_seekTrackToTick` 0x10024920 — scans to a target tick, storing the read position.
- `byteSwap16` 0x1001e6e0 — big-endian 16-bit read (SMF is big-endian).

### Playback flow

`Voice_startPlayback` 0x10020be0 enqueues command **1** (start) after resetting read state
(`Track_resetReadState` 0x1001e230, `Track_clearFlag8` 0x1001e530), pre-rolling every track
(`Seq_preRollTrack` 0x100205f0 / `Seq_preRollAllTracks` 0x100207f0) and stamping `timeGetTime`.
`Voice_requestStop`/`Voice_requestStopDrain` (0x10020ea0/0x10020ee0) enqueue command **2** (stop);
`Voice_requestCmd12` 0x10023ea0 enqueues command **0xc**.

`Seq_preRollTrack` decodes VLQ deltas and dispatches each due event through `Midi_Device::playEvent`
(0x10020870, round 1), advancing the read position (`Track_advancePos` 0x1001e9f0).
`Seq_stepTrackMeta` 0x10020720 does the same following a meta event.

### Command consumer (`Midi_Device::dispatchCommand` 0x1000a670, round 1)

The ring consumer switches on each node's command code; the handlers in this range are:

| code | handler | action |
|---|---|---|
| 7 | Seq_cmdHandler7 0x100231e0 | find track by this[0x1ce], call vtbl+0x84/+0x94 |
| 8 | Seq_cmdHandler8 0x10023540 | find track by id, re-mark event range (0x10023610) |
| 8,9 | Seq_cmdHandler9 0x100238b0 | find track by id, replay event range (0x10023950) |
| 10 | Seq_cmdHandler10 0x10023a60 | find track by id, operate on its data |
| 0xb | Seq_cmdHandler11 0x10023c70 | find track by id, set node+0x30 = arg |
| 0xc | Seq_cmdHandler12 0x10023c20 | set node+0x38 bit4 from arg&1 (per-track enable) |

`Seq_remarkEventRange` 0x10023610 and `Seq_replayEventRange` 0x10023950 walk an event index/tick
range via `Seq_listGetAt`, setting the resend flag and re-emitting events (`Seq_scheduleEvent`
0x10024b80, `Midi_Device::emitEvent` 0x100246a0). `Seq_allocTrackSlot` 0x100247c0 round-robins the
track array (global cursor g_trackRoundRobin 0x100b5010) to find a free slot.

### Channels and named sounds

- `Seq_setupChannel` 0x10020fe0 / `Seq_updateChannel` 0x10021fa0 / `Seq_dispatchChannelCmd`
  0x10024040 build and refresh the per-channel slots at +0x744; `Seq_loadChannelDesc` 0x10024280 and
  `Seq_mergeChannelDesc` 0x10024360 copy channel descriptors; `Seq_copyChannelParams` 0x10022280
  copies a 0x38-byte param block into +0x268 + idx*0x4c.
- `Seq_setPlayOrderMode` 0x10022750 sets the play-order mode and `Seq_buildPlayOrder` 0x10023d00
  fills the order array (seeded with g_playOrderSeed 0x100b4fe0 from `timeGetTime`).
- Named-sound registry: `Sound_lookupOrCreate` 0x10022320 find-or-creates a 0x21c-byte entry in the
  list g_cmdTableHead (0x100b4fe8, round 1) by name; `Sound_findByName` 0x10021240 is the read-only
  find; `Sound_reapIdle` 0x100204e0 sweeps unreferenced entries; `Sound_initEntry` 0x1001ab60
  initializes a new entry.

### All-notes-off / notifications

`Voice_allNotesOff` 0x10020d80 flushes pending MIDI messages (`Midi_Device::flushMessages` 0x1001ef10)
and walks the 0x7f channel slots calling `Voice_notifyChannelNotes` 0x1001ac40, which posts
notification code 5 to the owner window via `postNotification` (0x1000dbe0, round 1).

## 3. Doubly-linked list container (subsystem util)

A list {head@0, tail@4, count@0xc} of 0xc-byte nodes {next@0, prev@4, data@8} backs the sequencer's
track and event lists. `NodeList_pushBack` 0x10024de0, `NodeList_pushFront` 0x10024f90,
`NodeList_insert` 0x10024e60, `NodeList_initNode` 0x10025010 and `NodeList_clear` 0x10024d40 are its
operations; the finders `Seq_findTrackById` 0x10023080 and the dispatch handlers walk the +0x218
list. `Seq_listGetAt` 0x1001d780 returns the Nth element of the list at this+0x19c and is the single
most-called helper in the sequencer.

## New globals (see `re/names/sound_3_globals.tsv`)

| addr | name | what |
|---|---|---|
| 0x100b5010 | g_trackRoundRobin | round-robin track cursor (Seq_allocTrackSlot) |
| 0x100b4fe0 | g_playOrderSeed | timeGetTime stamp / shuffle seed for the play order |
| 0x1005b590 | vt_CtrlObject2 | reader base-object vtable |
| 0x1005b68c | vt_MidiReader | SMF reader vtable |
| 0x1005b6a8 | vt_Sound5b6a8 | Sound object vtable |
