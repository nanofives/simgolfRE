# sound.dll — systems in ID sound_7 (0x10033c70 .. 0x1003e6d0)

Round-3 naming pass over the 68 functions rounds 1-2 left unnamed in this span. They fall into two
groups: the **multi-channel Sound / Channel playback object model** (0x10033c70 .. 0x1003a800, the
RIFF/mmio and resampler helpers that sound_4 left as leads) and the **floating-point audio codec**
leaves and context initializers that sound_5 left unnamed (0x1003aa50 .. 0x1003e6d0). Facts only;
addresses are RVAs on image base 0x10000000. Unnamed functions and open questions are in
`log/naming/sound_7_notes.txt`.

## 1. Multi-channel Sound object (vtable 0x1005c7e8)

The top-level playback object built by `create_sound` (0x100093e0). It derives from the SubChannel
base (ctor 0x1002b4e0, dtor 0x1002b620) and **embeds a Channel at +0x70** (the large playback object
documented in sound_4 section 2). The embedded Channel is reached by passing `this+0x70` to every
Channel method.

### Structure (MultiSound)
- +0x00    vtable 0x1005c7e8
- +0x70    embedded Channel (so Channel+0x58 flags = MultiSound+0xc8, Channel+0x5c = MultiSound+0xcc)
- +0xc8    (= Channel+0x58) flags: bit0 playing, bit1 open, bit6 active
- +0xcc    (= Channel+0x5c) flags: bit2 engine-notify, bit5 3D-enabled
- +0x228   status extra bit (OR'd into MultiSound_getStatus as 0x8)
- +0x13f8  pitch ratio (double) written by MultiSound_setPitchCents
- +0x1420  source type (also mirrored at +0x1ec)
- +0x1428  flag; +0x1430 owned buffer (freed in dtor)
- +0x1438  sub-channel count; +0x1440 sub-channel pointer array

### Entry points
- ctors: `MultiSound_ctor` 0x10039690, `MultiSound_ctorWithType` 0x10039790; dtors
  `MultiSound_dtor` 0x10039870 and the deleting `MultiSound_deleteDtor` 0x10039760.
- transport: `MultiSound_start` 0x10039ce0, `MultiSound_stop` 0x10039c80, `MultiSound_release`
  0x10039c40, `MultiSound_enqueueCmd3` 0x10039bd0. All but stop push a command into the
  WaveCmdQueue (`WaveCmdQueue::pushNode` 0x1000c2f0 on `g_soundEngine` 0x100b4a20): start=cmd 1,
  cmd3=3, release=6.
- getters/3D: `MultiSound_getStatus` 0x1003a560, `MultiSound_is3DEnabled` 0x1003a800,
  `MultiSound_set3DPosition` 0x1003a5b0, `MultiSound_findStreamNode` 0x1003a730,
  `MultiSound_setVoiceBit1On` 0x1003a080, `MultiSound_setPitchCents` 0x1003a210.
- `loadMultiChannel` 0x100399a0 (named by sound_4) parses the RIFF file into the embedded Channel and
  allocates the sub-channels at +0x1440.

### Pitch from cents (MultiSound_setPitchCents 0x1003a210)
Takes a value in +-1200 cents (0x4b0), wraps it mod 1200, splits into semitones (/100) and cents
(%100), indexes the per-semitone rate tables 0x1005c608 / 0x1005c66c (int) and the ratio table
0x1005c6d0 (double) at index semitone+0xc, linearly interpolates by `cents * 0.01` (constant
0x1005c910 = 0.01), and applies the result through `Channel_setPitchRate` 0x10035750 (stores the rate
at Channel+0x220), saving the resulting ratio double at +0x13f8.

## 2. Channel methods (the embedded playback object)

The Channel (ctor `Channel_ctor` 0x10031e50, sound_4) has 16 sub-voice slots at +0x238 (stride 0x110,
flag byte slot+0x9c = +0x2d4), a DirectSound buffer at +0x54, flags at +0x58/+0x5c, volume doubles at
+0x1d0 (target) / +0x1e0 (current) / +0x1c8 (fade step) / +0x210, base rate at +0x208/+0x1d8/+0x1c0,
and a stream-buffer list at +0x1348. The armed-voice bitmask is +0x58>>0xc (16 bits). Constant
0x1005c5e8 = 1/65536 converts a 16.16 fixed-point rate (0..0x10000) to a [0,1] double; 0x1005b498 =
0.0 is the silence sentinel.

- lifecycle: `Channel_closeAndFree` 0x10033c70 (mmioClose, unlink from `g_openSoundList` 0x100b49c8,
  free decode lists +0x4ce/+0x4d2, vector-destruct the 16 sub-voices).
- voice alloc/transport: `Channel_allocAndInitVoice` 0x10034770, `Channel_releaseVoices` 0x10034ac0,
  `Channel_stopVoices` 0x10034b90, `Channel_broadcastVoiceBit1` 0x100377f0.
- rate/pitch: `Channel_setRate` 0x100351c0, `Channel_setReverseRate` 0x100355d0, `Channel_setRateGlide`
  0x10034c80 (glide over `*g_soundTimer` 0x100b49ec ticks), `Channel_setPitchRate` 0x10035750.
- volume/fade: `Channel_setVolume` 0x10035150, `Channel_serviceBuffer` 0x10035f80, `Channel_stepVolumeFade`
  0x10035fc0 (the fade state machine driving `DsBuffer::setVolume`/`stop`). `Channel_stubReturnB`
  0x10035f60 is an unsupported-op vtable slot (returns SNDERR 0xb).
- queries: `Channel_checkOpen` 0x100357e0, `Channel_readCurrentSample` 0x100358c0 (one-sample meter),
  `Channel_findStreamNode` 0x10035770, `Channel_setStreamSource` 0x10037720.
- DirectSound 3D: `Channel_set3DPosition` 0x10037b00 and the X/Y/Z setters 0x10037b40/b80/bc0 plus the
  3D-buffer stub 0x10037c00, all forwarding to the Ds3DBuffer at +0x54 under the +0x5c bit5 guard
  (else SNDERR 0xb).
- `MmioBuffer_init` 0x10037e10 seeds an mmio buffer descriptor (0x48-byte MMIOINFO at +0, 0x12-byte
  WAVEFORMATEX at +0xa0), called by openMmioSound and Wave_Device::appendBuffer.

### Generic node-list helpers
A doubly-linked list of {prev@0, next@+4, payload@+8} nodes with head/tail/count at [0]/[1]/[3]:
`nodeListClear` 0x10039580 (and its unwind-funclet twin `nodeListClear2` 0x100384b0),
`nodeListPopFront` 0x10039620, `nodeListAppendValue` 0x10038430. `MemBlock_alloc` 0x100394f0 /
`MemBlock_deleteDtor` 0x10038ae0 manage a {ptr,size} buffer object; `freePointerAndClear` 0x100394c0
is the trivial delete-and-null helper. Wave capture: `WaveInDevice_allocCaptureBuffers` 0x10038890
(builds the 0x20-byte capture descriptors + node list at +0x5c/+0x60/+0x68) and the deleting dtor
`WaveInDevice_deleteDtor` 0x100386e0.

## 3. Audio codec context + DSP leaves

sound_5 documented the codec entry points (`codecProcessBlock` 0x1003a980, `codecProcessResource`
0x1003b150, `codecSynthStage` 0x1003b230, `codecTransformStage` 0x1003b5a0, `codecStageDispatch`
0x1003c640) but left the arithmetic leaves and the context initializers unnamed. This round names them
by what the arithmetic literally computes.

### Configuration and context init
`codecSetParams` 0x1003aa50 writes the config globals (`g_codecOrder` 0x100b58a0 = 10,
g_100b58a4 = 0xb4/180, `g_codecMode` 0x100b58a8 = 1). Two contexts are allocated and initialized:
`codecAllocContextA` 0x1003aa90 / `codecInitContextA` 0x1003aae0 (0x2544 bytes, from loader 0x100319e0;
seeds pitch range 3000 and gain defaults) and `codecAllocContextB` 0x1003aea0 / `codecInitContextB`
0x1003aef0 (0xc00 bytes, from loader 0x10030fb0; frame length 0x3c, 16-bit seed constants
0xad57/0x78ac/0x4236 at +0xbe0). The codec is an LPC/pitch-pulse vocoder: order 10, frame 180.

### DSP leaves
- `quantizeParams` 0x1003c220 (caller codecProcessResource): __ftol of the inputs, a 16-step binary
  search over threshold table `g_codecExpThresholds` 0x100647e4 for an exponent, then nonlinear
  quantization of the parameter vector through tables 0x100646e8 / 0x10064684 / 0x100646c4 /
  0x10064558, using `intPow` 0x1003e2c0.
- synthesis (callers codecSynthStage): `reflectionToLpc` 0x1003d8d0 (step-up recursion k->a, gain =
  sqrt(prod(1-k^2))*g), `lpcSynthesize` 0x1003d1d0 (noise/pulse excitation then the all-pole LPC
  filter, energy-normalized), `interpSubframeParams` 0x1003da50 (reflection-coefficient interpolation
  in the log-area-ratio domain log((1+r)/(1-r)) with pitch-pulse placement), `applyIir2` 0x1003d0a0
  (second-order IIR with state at param+0xbec..0xbfc), `codecApplyFixedCoeffs` 0x1003d040 (forwards to
  0x10040e30 with the constant table `g_codecPulseCoeffs` 0x10064a18).
- transform (callers codecTransformStage / codecStageDispatch): `intPow` 0x1003e2c0,
  `floatCopySign` 0x1003e380, `medianOfThree` 0x1003e480, `decodeTableSymbol` 0x1003e520 (7-bit code
  -> nibble via `g_codecSymbolTable` 0x10064a80 with parity/escape handling),
  `substituteIfBelowThreshold` 0x1003e600 (replace a block with a reference block when all magnitudes
  are below 0x1005ebd4), and the clamped linear solver `gaussSolveClamped` 0x1003e6d0 (Gaussian
  elimination with a pivot-magnitude guard 0x1005ec40 and coefficient clamp to +-0.999).

## Globals added (see `re/names/sound_7_globals.tsv`)

| addr | name | what |
|---|---|---|
| 0x100b58a0 | g_codecOrder | codec LPC order (=10), set by codecSetParams, read by quantizeParams |
| 0x100b58a8 | g_codecMode | codec mode (=1) |
| 0x10064a80 | g_codecSymbolTable | 128-entry 7-bit-code -> nibble table (decodeTableSymbol) |
| 0x100647e4 | g_codecExpThresholds | binary-searched exponent thresholds (quantizeParams) |
| 0x100646e8 / 0x10064684 / 0x100646c4 / 0x10064558 | g_codecQuantTableA / BiasTable / DivTable / ReorderTable | quantizeParams lookup tables |
| 0x10064a18 | g_codecPulseCoeffs | fixed pulse/coefficient table (codecApplyFixedCoeffs, lpcSynthesize) |

g_100b58a4 (=0xb4/180, codec frame length) is written by codecSetParams but its reader was not
pinned to a single function, so it is left out of the globals TSV and noted here only. The per-semitone
rate/ratio tables 0x1005c608 / 0x1005c66c / 0x1005c6d0 and the fade/rate constants 0x1005c5e8 (1/65536)
/ 0x1005c910 (0.01) / 0x1005b498 (0.0) are read-only constants cited in the rows above.
