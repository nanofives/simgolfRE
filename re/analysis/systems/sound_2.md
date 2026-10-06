# sound.dll — systems in range 0x10003f30 .. 0x100188d0 (ID sound_2)

Second naming pass over the lower half of `sound.dll`: the 162 functions round 1 (sound_0) left
unnamed between RVA 0x10003f30 and 0x100188d0. 146 are named with evidence in `re/names/sound_2.tsv`;
7 globals in `re/names/sound_2_globals.tsv`; the 16 left unnamed are in `log/naming/sound_2_notes.txt`.
All addresses are RVAs on image base 0x10000000. Facts only. Builds on `sound_0.md`/`sound_1.md`, which
established the device classes (Midi_Device, Wave_Device, Wave_In_Device), the SoundTimer, the command
rings and the mmio WAV/MIDI I/O.

## 1. DirectSound buffer wrapper (`DsBuffer`, 0xd4 bytes)

The biggest result of this pass. Each DirectSound buffer the engine owns is wrapped in a 0xd4-byte
object. `DirectSoundDevice::createPoolBuffer` 0x10012120 / `createPoolBufferEx` 0x10012bb0 allocate it,
`DsBuffer::ctor` 0x1000eef0 zero-initializes it and `DsBuffer::initDesc` 0x1000f590 fills the embedded
DSBUFFERDESC, then `DsBuffer::create` 0x1000fbb0 calls `IDirectSound::CreateSoundBuffer` (interface
vtbl+0xc).

### Layout (offsets proven by the interface calls)
| off | field |
|---|---|
| +0x00 | flag word (bit1 = primary buffer) |
| +0x18 | play position in samples (written by getPlayPosition) |
| +0x44 | busy/fade counter (GetStatus result gate; fade timer in serviceTick) |
| +0x48/+0x4c | timed-position accumulators |
| +0x54 | play start time (timeGetTime) |
| +0x5c | owner back-pointer (the device/voice) |
| +0x60 | `IDirectSoundBuffer*` |
| +0x64 | `IDirectSound3DBuffer*` (secondary buffers) |
| +0x68 | `IDirectSound3DListener*` (primary buffer) |
| +0x6c | `IDirectSoundNotify*` |
| +0x70 | DSBUFFERDESC (dwSize 0x24, +0x74 dwFlags 0xe8, +0x78 dwBufferBytes 0x10000, +0x80 lpwfxFormat) |
| +0xa8 | cached WAVEFORMATEX (0x12 bytes) |
| +0xac | nSamplesPerSec, +0xb4 nBlockAlign (used for position<->sample math) |
| +0xc4 | cached DirectSound attenuation |
| +0xc8/+0xcc/+0xd0 | 3D position x/y/z |
| +0xbc/+0xc0 | prev/next links in g_dsBufferList |

### IDirectSoundBuffer vtable slots used (confirms the names)
QueryInterface(0), Release(8), GetCurrentPosition(0x10), GetFormat(0x14), GetStatus(0x24), Lock(0x2c),
Play(0x30), SetCurrentPosition(0x34), SetFormat(0x38), SetVolume(0x3c), SetFrequency(0x44), Stop(0x48),
Unlock(0x4c), Restore(0x50). Methods: setVolume 0x1000f1c0, setVolumeRaw 0x1000f190, applyOwnerVolume
0x1000f2b0, setFrequency 0x1000f160, setPosition 0x1000f790, setPrimaryFormat 0x1000f990, stop 0x1000f760,
play 0x10010db0, restartPlayback 0x1000f020, getTimedPosition 0x10010be0, getPlayPosition 0x100106b0,
getPosition 0x10010660 (dispatcher), unlock 0x10010370, restore 0x100101a0, isBusy 0x10010e30,
getWriteRegion 0x100103b0, lockCaptureRegion 0x10010860, lockEntire 0x1000f810, destroy 0x1000ef60,
release 0x1000f8f0, releaseIfOwned 0x10010060. `mapDsError` 0x1000f480 maps the DirectSound HRESULTs to
SNDERR codes.

### 3D audio
`DsBuffer::query3D` 0x1000fef0 QueryInterfaces `IID_IDirectSound3DBuffer` (0x1005d57c) into +0x64 for a
secondary buffer and `IID_IDirectSound3DListener` (0x1005d58c) into +0x68 for the primary.
`DsBuffer::queryNotify` 0x1000ff60 fetches `IID_IDirectSoundNotify` (0x1005d52c) into +0x6c (all three
IIDs verified by their GUID bytes). `Ds3DBuffer::setPosition*` 0x10011000/0x10011070/0x100110c0/0x10011110
drive `IDirectSound3DBuffer::SetPosition` (vtbl+0x4c) on +0x64; `Ds3DListener::setPosition` 0x100135a0
drives `IDirectSound3DListener::SetPosition` (vtbl+0x38) on the device's listener interface +0xd8
(cached x/y/z at +0x1ac/+0x1b0/+0x1b4).

### DirectSoundDevice buffer pool
The device object (the Wave_Device of sound_0) keeps a per-device buffer list at +0x168 (count +0x174)
plus the global active list `g_dsBufferList` 0x100b4a34/38, and slot counters +0x14c/+0x150/+0x154/+0x158.
Pool methods: setCooperativeLevel 0x100120a0 (`IDirectSound::SetCooperativeLevel` vtbl+0x18, records
g_dsCoopLevel), duplicateBuffer 0x10012260 (`DuplicateSoundBuffer` vtbl+0x14), acquireBuffer 0x100128b0,
freeIdleBuffer 0x100124c0, freeBufferById 0x10012690, removeBuffer 0x100132e0, releaseAllBuffers
0x100133d0, unlinkDsBuffer 0x100134e0, setMasterVolume 0x10013560 (writes +0x188, re-applies DsBuffer
volume to every pooled buffer). `setBufferSize` 0x1000f0c0 sets +0x78 before creation.

## 2. Software mixer / resampler family

Round 1 verified three of these (mixRenderA 0x10013890, mixRenderRatedB 0x10014820, mixRenderRatedC
0x10025190). This pass names the 22 members in range. They are `__cdecl(out, in, count, voice, mode)`
routines; `mode == 1` selects the stream/3D voice layout, where the sample source is a sub-object at
voice+8 with fields `{+0x18 data, +0x1c len, +0x20 pos, +0x24 loop}`. Output is `short*`. Rate-converting
members read the 16.16 fixed-point fields at voice +0x1e8/+0x1f8 (step) and +0x220 (sample-rate ratio;
0x10000 == unity) and a ring at voice +0x1358; unity-copy members do not.

Structure: an outer routine dispatches by the 16.16 rate fields to a unity-copy inner (all ratios
0x10000) or a resampling inner. Named outers and inners:
- A group: mixRenderOuterA2 0x10013dd0 -> mixResampleInnerA 0x10013b10.
- B group: mixRenderRatedB 0x10014820 / mixRenderOuterB2 0x10014d20 -> mixCopyInnerB 0x10014250,
  mixResampleInnerB 0x10014510.
- C group: mixRenderOuterC 0x10016850 -> mixCopyInnerC 0x10016340, mixResampleInnerC 0x100165a0.
- stream group: streamWaveSamples 0x10030ba0 (sound_1) picks streamRenderBlock 0x100184a0 (count>11) or
  streamRenderTail 0x100188d0 (count<=11); both use double-precision mixing.
- standalone members (reached only through function pointers): resampling mixResampleD..L
  (0x100152b0, 0x100156c0, 0x10015cf0, 0x10015f70, 0x10016d90, 0x10017160, 0x10017520, 0x100178c0,
  0x10018110) and unity-copy mixCopyD..F (0x10015aa0, 0x10017c10, 0x10017eb0).

The upper-half dispatchers (0x10036280 / 0x100371d0, out of range) select a renderer by
`this+0x182 == 2` (stereo) and `this+0x18e == 8` (8-bit); the exact bit depth / channel count of each
individual renderer in this range was not pinned, so the family letters are unique disambiguators, not
format claims.

## 3. Sound / WaveStream objects and the command rings

`Sound::ctor` 0x10003f30 / `Sound::dtor` 0x10004000 (vtable 0x1005b000) wrap a loaded audio file; the
chunk list is at +0xa4c and `processWaveChunks` 0x10005770 turns it into sample buffers.
`WaveStream::ctor` 0x10006020 (vtable vt_WaveStream 0x1005b1c0, dtor 0x10006160 from sound_0) is the
streaming variant; `initStreamBuffer` 0x10006e00 allocates its 0x20000-byte block. A Sound registers
itself into the global list `g_soundRegistry` 0x100b5000 via `registerSound` 0x10005e00 and leaves it
via `unregisterSound` 0x10005eb0.

Both Midi_Device and Wave_Device own a 4096-entry command ring (mask 0xfff). The Midi ring is at +0x50
(head +0x4050, tail +0x4054): `Midi_Device::drainCommandQueue` 0x1000a330 consumes it via
`Midi_Device::scanCommandRing` 0x1000aaf0. The Wave ring is at +0x1d8 (head +0x41dc): pushed by
`WaveCmdQueue::pushNode` 0x1000c2f0 / `pushValue` 0x1000c3d0, executed by
`Wave_Device::processCommandQueue` 0x1000c420 (switch over command types 1-4) with
`Wave_Device::scanCommandRing` 0x1000dcf0, and flushed by `Wave_Device::flushCommandQueue` 0x1000cc70.
`Wave_Device::serviceTick` 0x1000c230 is the per-tick driver: it dispatches commands, advances each
pooled buffer's fade counter (+0x44 += +0x1b4) replaying finished ones, and writes captured PCM through
the writeWaveFile path (sound_0) to the stream `g_waveStream` 0x100b49f4.

### Lifecycle constructors / destructors found
Midi_Device::ctor 0x10009be0 (vtable vt_MidiSeq 0x1005b28c) / dtor 0x10009cb0; Wave_Device::dtor
0x1000b2e0 (vtable 0x1005b304); Wave_In_Device::ctor 0x1000e870 / dtor 0x1000e8f0 (vtable vt_WaveInDevice
0x1005b3d4); openCapture 0x1000ead0 / closeCapture 0x1000ec00. Wave output start/stop/close
(startOutput 0x1000bf90, stopOutput 0x1000bef0, closeOutput 0x1000be00) register/unregister the device
with the SoundTimer and start/stop the pooled DirectSound buffers. Scalar deleting destructors (the MSVC
`??_G` thunk: call dtor, free when flag&1): Sound 0x10003fd0, WaveStream 0x10006130, Midi_Device
0x10009c80, Wave_Device 0x1000b2b0, Wave_In_Device 0x1000e8c0, DsBuffer 0x10012b80, cmdNode 0x1000cc40,
CtrlObject-derived 0x10005740.

## 4. Generic containers

A doubly-linked list node {+0 prev, +4 next, +8 value} with a {head, tail, count} header recurs across
the engine. Append helpers: appendNode 0x100137f0, appendNodeB 0x10005e30, appendNodeC 0x1000e680,
appendNamedNode 0x10011810 (0x14-byte node with a copied name). Drain-all helpers (identical copies, one
per container instantiation): clearNodeListA..G (0x10013620, 0x10013750, 0x1000e5e0, 0x10005a60,
0x10005b60, 0x10006c70, 0x1000ae80). Find/seek: findAndUnlinkNode 0x100136c0, findAndUnlinkNodeB
0x1000e700, removeSoundNode 0x1000e050, listPopFront 0x10005c00, seekListToIndex 0x100116a0,
getNodeByIndex 0x10011790. A separate growable array {+4 buffer, +0xc count} uses resizeDynArray
0x10006f00 / freeDynArray 0x10006ec0.

## New globals (`re/names/sound_2_globals.tsv`)
| addr | name | what |
|---|---|---|
| 0x1005d52c | IID_IDirectSoundNotify | QueryInterface riid (bytes verified) |
| 0x1005d57c | IID_IDirectSound3DBuffer | QueryInterface riid (bytes verified) |
| 0x1005d58c | IID_IDirectSound3DListener | QueryInterface riid (bytes verified) |
| 0x100b5000 | g_soundRegistry | global Sound list head |
| 0x100b5008 | g_soundRegistryTail | its tail |
| 0x100b49bc | g_midiOutputFlags | MIDI-output enabled flags (bit1) |
| 0x10063dfc | g_dsPollTuning | play-cursor calibration value (0x370/0x374) |
