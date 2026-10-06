# sound.dll — ID sound_4 (0x10027e20 .. 0x1003a210, the functions round 1 left unnamed)

Module `original\sound.dll`, release build, image base 0x10000000 (addresses are RVAs). This range
holds the software mixer/resampler, the channel/voice playback object model, the Sound / stream-reader
file-reader classes, the MIDI note-voice helpers, and the wave-input buffer helpers. Round 1
(`sound_0`, `sound_1`) named the surrounding export layer, the SoundTimer, the worker thread, the
mmio file loaders and the C runtime; the class vocabulary (`Midi_Device`, `Wave_Device`,
`Wave_In_Device`, `WaveInDeviceMgr`) comes from the DLL's own export decoration. Facts only;
open questions are in `log/naming/sound_4_notes.txt`.

## 1. Software mixer / resampler family (the main lead)

Round 0/1 found a family of `__cdecl(out, in, count, voice, mode)` PCM mixers but could not
distinguish the format variants. The three render dispatchers in this range resolve the whole family.

### Render hierarchy
- `renderChannel` 0x10035990 is the top-level per-channel render. It picks one of three paths from
  the channel flags at this+0x58/0x5c:
  - the streaming-decoder path `renderChannelStreamed` 0x10036660 when this+0x5c bit6 is set;
  - the single-voice path `renderChannelSingle` 0x10036280 when the channel plays as one voice;
  - otherwise it loops the 16 sub-voice slots at this+0x2d4 (stride 0x110) calling
    `renderChannelVoice` 0x100371d0 on each armed slot (armed bitmask = this+0x58 >> 12).
- `renderChannelSingle` 0x10036280 and `renderChannelVoice` 0x100371d0 both select the mixer body by
  the SAME three criteria: channel count (this+0x182 == 2 means stereo), bit depth (this+0x18e == 8
  means 8-bit) and the `count` argument compared with 0xb, which picks the accumulating or the
  overwriting body (corrected 2026-10-06: an earlier version of this note read it as point vs
  interpolated sampling; 0x10019770 and 0x10019b20 differ only in `fadd qword ptr [edi]` /
  `[edi+8]`, i.e. adding into the existing output).

### Mixer bodies (in sound_3's range, named in re/names/sound_3.tsv)
| body | output | format |
|---|---|---|
| 0x10019770 | accumulate | 16-bit mono |
| 0x10019b20 | overwrite  | 16-bit mono |
| 0x1001a2c0 | accumulate | 8-bit mono |
| 0x10019ec0 | overwrite  | 8-bit mono |
| 0x1001a910 | accumulate | stereo |
| 0x1001a6c0 | overwrite  | stereo |

### The two in-range mixers (reached only through function pointers)
`mixAdd8bitStereo` 0x10027e20 and `mixCopy8bitStereo` 0x100281d0 are two more bodies of the same
family, each `__cdecl(out, in, count, voice, mode)`. Both read 8-bit unsigned input (`*in*0x100 -
0x8000`), linearly interpolate with the 16.16 phase accumulator at voice+0x1358 (mode 1) / voice+0xb8
(mode 0), and scale by the L/R volumes at voice+0x1e8/+0x1f8 (mode 1) / voice+0x78/+0x68 (mode 0).
The difference: `mixAdd8bitStereo` ADDS into the 16-bit stereo output with 0x7fff/0x8000 saturation
(used for the second and later voices), `mixCopy8bitStereo` WRITES the output (used for the first
voice). The sample buffer descriptor read by all of them is {+0x18 data, +0x1c length, +0x20
position, +0x24 loop point}; mode 1 reads the stream buffer through voice+8 instead.

## 2. Channel and sub-voice object model

The playback (string in the binary) is a large object. `Channel_ctor` 0x10031e50 array-constructs its 0x10 (16)
sub-voice slots of 0x110 bytes at this+0x238 (element ctor `Voice_resetDefaults` 0x10037c40) and
zeroes the voice-stream list head/tail/count at this+0x1338..0x1354 and the embedded MMIOINFO at
this+0xc. The armed-voice bitmask lives in this+0x58 >> 12 (16 bits); format fields at this+0x182
(channels) and this+0x18e (bits); the pan is at this+0x224 and the base rate at this+0x1c0/0x1d8.

Sub-voice slot layout (base = this+0x238 + idx*0x110; its flag byte is slot+0x9c = channel+0x2d4 +
idx*0x110):
- +0x48 tag/note number, +0x50 fixed-point rate, +0x58 level (double), +0x5c/0x64/0x74/0x84 gain
  high dwords (1.0 default), +0x60 reverse flag, +0x68/0x78 unity-rate fields,
- +0x9c flag byte (bit2 finished, bit3 releasing), +0xec forward step, +0xf0/0xf8 envelope doubles,
  +0xc0 position, +0x2c0/0x2c4 position copies.

Helpers:
- `Voice_resetDefaults` 0x10037c40 — unity rate 0x10000 and gain 1.0.
- `Channel_startVoice` 0x100348f0 — resets a slot and computes its initial rate from channel+0x1c0.
- `Channel_allocVoiceSlot` 0x100382c0 — finds the first free bit, returns the slot address.
- `Channel_findVoiceByTag` 0x10038330 — finds an armed non-stopping slot whose +0x48 matches.
- `Voice_setLevel` 0x100383b0 / `Voice_setPanGains` 0x10037f30 — level and L/R gain for a slot.
- `Voice_setRateForward` 0x10038070 / `Voice_setRateReverse` 0x10038130 — forward vs reverse step.
- `Voice_noteOff` 0x100381f0 — begin release or mark finished.
- `Voice_stopAndFlag` 0x10034db0, `Voice_setLoopFlag` 0x10037760.
- `Channel_recomputeVoiceGains` 0x10035460 / `Channel_setPanGains` 0x10035340 — channel-wide gain/pan.
- `Channel_startVoiceTimed` 0x10039e70 — opens the device, (re)starts the SoundTimer and starts a
  voice; `Channel_clearBackref` 0x10038400.
- `voiceListRemove` 0x10032440, `Channel_closeStreamList` 0x10033f90, `Channel_releaseBuffers`
  0x100340b0, `serviceChannelPlayback` 0x10034430 (per-tick), `updateVoiceRate` 0x10032500.

`loadMultiChannel` 0x100399a0 parses a RIFF file into the embedded channel at this+0x70 and, for a
2..3-channel file, allocates the extra 0x1450-byte sub-channels (`SubChannel_ctor` 0x1002b4e0,
vtbl_SubChannel 0x1005bae0) at this+0x1440.

## 3. Sound and stream-reader file classes, open-sound list

Two mmio-backed reader classes share a buffer-node list at +0x5c (head) / +0x68 (count) and an
HMMIO at +0x44 or +0 depending on node type:
- `Sound` — vt_Sound5c450 0x1005c450, ctor `Sound5c450_ctor` 0x10030080 (over the `buildVoiceRxName` base),
  dtor `Sound5c450_dtor` 0x10030180, `Sound_resetBuffers` 0x10032020, `Sound_stop` 0x100303f0,
  `Sound_freeBufferList` 0x10030570.
- `StreamReader` — vtbl_StreamReader 0x1005c528, ctor `StreamReader_ctor` 0x10031200 (over the
  `buildVoiceTxName` base), dtor `streamReaderDtor` 0x100312a0.
- Node-list freers: `freeMmioHandleList` 0x10031490, `freeMmioHandleListGetInfo` 0x10031560.

The global open-sound list is a doubly-linked list headed at `g_openSoundList` 0x100b49c8 (named by
round 1); unlinking also updates `g_openSoundListPrevVal` 0x100b49cc, `g_openSoundListCursor`
0x100b49d0 and `g_openSoundCount` 0x100b49d4, and each unlink decrements `g_openStreamCount`
0x100b4a08.

### Decode / mmio buffering
- `beginDecodeStreamA` 0x10032130 and `beginDecodeStreamB` 0x10032290 open the decoder at this+0x54
  (codec id 0x100e0 vs 0x100f4), prime the first decoded block and feed the compressed data; both set
  this+0x58 bit28 and this+0x220 = 0x5622. `closeDecoder` 0x10032640 tears the decoder down.
- `mmioReadBuffered` 0x10034fe0 copies out of the mmio read window; `mmioSeekToSample` 0x100350e0
  and `rewindChannel` 0x100339c0 seek to a sample / data start; `fillDecodeRing` 0x10033300 and
  `serviceDecodeRing` 0x10034fb0 refill the decode ring.
- `MixBuffer_ctor` 0x1002cee0 (vtbl_MixBuffer 0x1005be84), `VoiceSlot_init` 0x1002ceb0,
  `allocVoiceSlot` 0x1002d100, `listAppendNodeA/B` 0x10038550/0x100385d0 (RIFF cue/data lists).

## 4. Playback-controller / device methods

A playback-controller object holds a Sound at +0x40 (or +0x10 in the `param_1`-int variants) and
drives it through its vtable (+0x58 mode, +0x6c set-mode, +0x7c start, +0x84 open, +0x48 queue):
`Engine_playSound` 0x1002add0, `Device_createAndStart` 0x1002bbe0, `Device_prepareSoundFlags`
0x1002bc60, `Device_lazyCreateSound` 0x1002bd80, `Device_createStartQueue` 0x1002bee0,
`Mixer_dtor` 0x1002cb00 (vtbl_Mixer 0x1005bdd8). `Object_setName` 0x1002bb10 / `Voice_setName`
0x1002d240 strdup a name into +0x50 / +0x204. `Sound_setSourceType` 0x1002ad10 maps a source-type id
to format bits in +0x44.

The device base ctors share a default level 0x7f at field[2]: `VoiceBase_ctor` 0x10029020
(vtbl_VoiceBase 0x1005b914), `MidiDeviceBase_ctor` 0x10029e70 (vtbl_MidiDeviceBase 0x1005b9e4),
`WaveInDeviceBase_ctor` 0x1002a240 (vtbl_WaveInDeviceBase 0x1005ba5c).

## 5. MIDI note helpers and the message window

The MIDI playback path (`Midi_Device::noteOn`/`sendShortMsg`/`setChannelVolume`, named by round 1)
allocates and tags channel voices through `Channel_allocVoiceSlot` / `Channel_findVoiceByTag`, sets
levels and pans through `Voice_setLevel` / `Voice_setPanGains`, and releases them through
`Voice_noteOff`. `randRange` 0x10028560 is an LCG used by noteOn for randomisation.

The hidden message window (round 1: `createSoundWindow`, `soundWindowProc`, `soundWindowSubclassProc`)
is subclassed by `SoundWindowObj_subclass` 0x100286f0 / `SoundWindowObj_unsubclass` 0x100287c0 and
torn down by `SoundWindowObj_dtor` 0x100286a0 (vtbl 0x1005b8ac). `pumpWaveMessages` 0x10028810 drains
buffer-completion messages 0x7e8..0x7ef into `soundWindowSubclassProc`. `SoundGroup_stop` 0x10039b30
and `SoundGroup_rewind` 0x10039c10 recurse over a composite sound's child array. `stopSoundThreadFull`
0x1002a8a0 is a worker-thread shutdown variant called from `Wave_Device::stop`.

## 6. Wave input (capture) device

Round 1 named the ctor/open/close/start/stop methods (`WaveInDevice_*`, vtbl 0x1005c604). This range
adds the destructor `WaveInDevice_dtor` 0x10038710 and the per-list header methods
`WaveInDevice_prepareHeaders` 0x10038b40 (waveInPrepareHeader) and `WaveInDevice_unprepareHeaders`
0x10038c70 (waveInUnprepareHeader), both of which translate MMRESULT into the SNDERR codes used
throughout the DLL.

## Globals added (see `re/names/sound_4_globals.tsv`)

| addr | name | what |
|---|---|---|
| 0x100b49cc/d0/d4 | g_openSoundListPrevVal / Cursor / g_openSoundCount | open-sound list unlink bookkeeping |
| 0x1005c450 | vt_Sound5c450 | Sound class |
| 0x1005c528 | vtbl_StreamReader | streaming reader class |
| 0x1005bae0 | vtbl_SubChannel | sub-channel/mixer-voice base |
| 0x1005bdd8 | vtbl_Mixer | playback-controller derived vtable |
| 0x1005be84 | vtbl_MixBuffer | mix buffer |
| 0x1005b914 / b9e4 / ba5c | vtbl_VoiceBase / MidiDeviceBase / WaveInDeviceBase | device base ctors |
| 0x1005b488 | g_levelScale | level-to-double scale constant |
