# sound.dll — systems in range 0x10003f30 .. 0x10027ad0 (ID sound_0)

Reverse-engineering notes for the lower half (403 functions) of `sound.dll`, the SimGolf audio
library (DirectSound / winmm, MSVC 6 release build with incremental-linking export thunks). All
addresses are RVAs on image base 0x10000000. Facts only; unverified items are in
`log/naming/sound_0_notes.txt`.

## Overview

`sound.dll` is a self-contained audio engine exporting a small C API plus three device classes.
Exports resolved through their 5-byte `jmp` thunks (target = real body):

| export | ordinal | body | role |
|---|---|---|---|
| `create_sound` | 1 | 0x100093e0 | factory: Sound object from a filename |
| `delete_sound` | 2 | 0x100097d0 | destroy a Sound object |
| `init_sound_timer` | 3 | 0x100092e0 | create + init the global multimedia timer |
| `release_sound` | 4 | 0x10009390 | shut the library down, stop the timer |
| `get_sound_version` | 11 | 0x10009370 | version constant |
| `Dll_Midi_Device::create_device` | 7 | 0x10009a90 | construct the Midi_Device singleton |
| `Dll_Midi_Device::delete_device` | 8 | 0x10009bb0 | destroy it |
| `Dll_Wave_Device::create_device` | 5 | 0x1000b050 | construct the Wave_Device singleton |
| `Dll_Wave_Device::delete_device` | 6 | 0x1000b1a0 | destroy it |
| `Dll_Wave_In_Device::create_device` | 9 | 0x1000e910 | construct the Wave_In_Device singleton |
| `Dll_Wave_In_Device::delete_device` | 10 | 0x1000ea00 | destroy it |
| `WaveInDeviceMgr::call_back` | 12 | 0x10038fd0 | waveIn callback (body out of range) |

The class names (`Midi_Device`, `Wave_Device`, `Wave_In_Device`, `WaveInDeviceMgr`, `SNDERR`) come
from the DLL's own export decoration, so they are evidence, not guesses. Most functions return
small integer error codes from an `SNDERR` enum (0 = ok; seen: 3, 6, 10, 0xb, 0xc, 0x13, 0x1e, 0x20,
0x22, 0x25, 0x26).

## Imports used by this half

- **winmm MIDI**: `midiOutOpen` (0x10009da0), `midiOutShortMsg` (0x1001ef10, 0x100215c0, 0x10021a90,
  0x100246a0), `midiOutClose`.
- **winmm MMIO**: `mmioOpenA/Read/Write/Seek/Close/GetInfo/StringToFOURCCA` — WAV and MIDI file I/O.
- **winmm timers**: `timeGetDevCaps/timeBeginPeriod/timeSetEvent/timeKillEvent/timeEndPeriod/timeGetTime`.
- **ole32**: `CoInitialize/CoCreateInstance/CoUninitialize` — DirectSound COM creation (0x10011c60).
- **DSOUND** ord2 (`DirectSoundCreate`) and `DirectSoundEnumerateA` (0x1003a978) are referenced from
  the upper half; this half reaches DirectSound through the COM `IDirectSound` interface created in
  0x10011c60 and through 0x100114e0 (enumerate).
- **user32**: `PostMessageA` (+CreateWindowExA etc. in the upper half) — async notifications.
- **kernel32**: critical sections and `CreateThread` family (upper half owns the worker thread).

## 1. Multimedia timer (`SoundTimer`)

Globals: `g_soundTimer` (0x100b49ec), lock `g_timerLock` (0x100b4990, a CRITICAL_SECTION),
reentrancy guard `g_timerBusy` (0x100b49a8).

- `SoundTimer::init` 0x100089f0 — `timeGetDevCaps` -> wPeriodMin at +0x18, sets default fields
  (period 0x1e, 22050/44100/16-bit), `InitializeCriticalSection(g_timerLock)`.
- `SoundTimer::start` 0x10008ce0 — `timeBeginPeriod` + `timeSetEvent(period, callback 0x1000128a,
  this, flags)`, stores the timer id at +0x40.
- `SoundTimer::onTick` 0x10008ed0 — the `timeSetEvent` callback (reached through the 0x1000128a
  trampoline). Measures elapsed ms with `timeGetTime`, logs `"timer Late!!! by %d ms"`
  (0x10063d98) when overdue, and calls virtual slots +0x44 (with the delta) and +0x38 on every
  client in the list headed at +0x4c. `init_sound_timer` 0x100092e0 wires this up.
- `SoundTimer::stop` 0x10008ac0 / `stopKeepLock` 0x10008bf0 / `killEvent` 0x10008d70 —
  `timeKillEvent`+`timeEndPeriod` (the first also `DeleteCriticalSection`).
- Clients register/unregister with `registerClient` 0x100088e0 / `unregisterClient` 0x10008920
  (linked under the lock; `linkClient` 0x10009040). A `Midi_Device`/`Wave_Device` registers itself
  so that `onTick` drives it.

## 2. Command ring buffer

Each device owns a 4096-entry command ring at +0x50 with head index at +0x4054 (mask 0xfff).
`CommandQueue::init` 0x10008860 / `clear` 0x10008890 zero it; `push` 0x1000a240 allocates a 0x10-byte
node (4 fields) and enqueues (returns 0x22 when full); `at` 0x100088c0 indexes it. The service
routine `Device::serviceVoices` 0x1000a050 (a timer client) drains commands via `dispatchCommand`
0x1000a670, then updates each active voice.

## 3. MIDI playback (`Midi_Device`)

Singleton `g_midiDevice` (0x100b4a1c), vtable fragment `vt_MidiSeq` (0x1005b28c), HMIDIOUT at +0x3c.

- Open output: `Midi_Device::openOutput` 0x10009da0 (`midiOutOpen`, registers with the timer).
- SMF loading: `loadFile` 0x1001d810 (from disk via mmio) and `loadFromMemory` 0x1001dce0 (from an
  mmio memory buffer) scan `MThd` (0x10063d90) / `MTrk` (0x10063e08) chunks; `closeFile` 0x1001e3d0.
  The master loader `loadSoundFile` 0x100041d0 (3762 bytes) dispatches RIFF-WAV vs MIDI.
- Event playback: `playEvent` 0x10020870 decodes a channel/system event with running status and
  dispatches it — note-on -> `noteOn` 0x100215c0, controller 7 -> `setChannelVolume` 0x100213d0,
  meta/sysex -> `handleMetaEvent` 0x10021bf0 (0x2f end-of-track sets loop/stop flags). Short
  messages go out via `midiOutShortMsg` (0x100215c0, 0x10021a90, 0x100246a0). `flushMessages`
  0x1001ef10 drains the 128-entry pending-message table at +600 (all-notes-off style flush).
- `create_sound` 0x100093e0 picks the format by file extension, matching `rmi`/`aif`/`mid`/`wav`
  (0x10063db4..0x10063dc0) with `strstr` (0x10042690).

## 4. Wave output (`Wave_Device` over DirectSound)

Singleton `g_waveDevice` (0x100b49f0). `IDirectSound` interface stored at device +0; primary buffer
`g_primaryBuffer` (0x100b49f8), secondaries `g_secondaryBuffer1/2` (0x100b49fc, 0x100b4a00).

- `DirectSoundDevice::create` 0x10011c60 — `CoInitialize` + `CoCreateInstance(CLSID 0x1005d67c,
  IID 0x1005d5dc)`, then `SetCooperativeLevel` through the interface; cooperative level from
  `g_dsCoopLevel` (0x100b4a3c).
- `createBuffer` 0x10012d10 / `createBufferFmt` 0x10012fe0 create `IDirectSoundBuffer`s (the latter
  with a supplied WAVEFORMATEX; default format `g_defaultWaveFormat` 0x10063dc8 = PCM 22050/16/mono)
  and link them into the active-buffer list `g_dsBufferList` (0x100b4a34, nodes link +0xbc/+0xc0).
- `enumerate` 0x100114e0 -> `DirectSoundEnumerateA` (0x1003a978).
- Lifecycle: `Wave_Device::start` 0x1000baa0, `restart` 0x1000c020, `stop` 0x1000b960; teardown
  `DirectSoundDevice::close` 0x10011290 (`CoUninitialize`), `releaseAll` 0x10011970, `stopAll`
  0x10011a80.

### Software mixer / resampler
A family of large `__cdecl(out, in, count, voice, mode)` routines renders and mixes PCM into the
output buffer using 16.16 fixed-point rate fields on the voice (step/ratio at +0x1e8, +0x1f8,
sample-rate ratio at +0x220; 0x10000 == unity; ring at +0x1358). When all ratios are unity the
rate-converting variant (e.g. 0x10014820) falls back to a straight copy path (0x10014250). Verified
examples: 0x10013890, 0x10014820, 0x10025190. The full family (~35 functions: 0x10013890-0x1001a910
and 0x10025190-0x10027ad0) is called from upper-half renderers 0x10030ba0 / 0x10036280 / 0x100371d0;
individual format variants (8/16-bit, mono/stereo, interpolated or not) were not distinguished one by
one — see the notes file. `Voice::update` 0x1001f8c0 advances one voice per tick (float/fixed timing,
virtual +0x134, double at +0x720).

## 5. Wave input (`Wave_In_Device`)

Singleton `g_waveInDevice` (0x100b4a24), vtable `vt_WaveInDevice` (0x1005b3d4), active capture object
`g_waveInCapture` (0x100b4a04). `create_device` 0x1000e910 allocates 0x4c bytes and sets defaults
(+0x40 = 0x7f). Thin forwarders `op1` 0x1000ed70 / `op2` 0x1000eda0 call into the capture object
(0x10039450 / 0x10039470, upper half); `waveIn*` imports themselves are in the upper half. Returns
0x13 when no device is open.

## 6. WAV file I/O (mmio / RIFF)

`g_openMmioCount` (0x100b4a08) tracks open mmio handles (incremented by every opener, decremented by
every closer).

- Read: `Wave_Device::openMemoryMmio` 0x10006420 (`'MEM '` memory IOProc, MMIOINFO at +0x6c),
  `readFmtChunk` 0x10005440, `appendBuffer` 0x10006850, `freeBufferList` 0x100064e0,
  `openMmioFile` 0x100086f0, `closeMmioHandle` 0x10008770, `closeMmioNodeList` 0x10006330.
- Write: `writeWaveFile` 0x10007010 / `writeWaveFileB` 0x100075e0 (RIFF WAVE writers, `mmioWrite` +
  `timeGetTime`), chunk writers `writeWaveChunk` 0x10007df0 / `writeWaveChunkB` 0x10008070, header
  writers `writeWaveHeaderA/B/C` 0x10007c30/0x10007cc0/0x10007d60.
- `WaveStream::destroy` 0x10006160 (vtable `vt_WaveStream` 0x1005b1c0) closes every mmio node in a
  list.

## 7. Notifications

`g_soundThreadState` (0x100b5038) is the sound thread state struct (flag +0, CRITICAL_SECTION +4, thread id +0x1c, handle +0x20, event +0x24; corrected 2026-10-06, it is not an HWND): callers pass its address to postSoundThreadMessage 0x1002aa90. (the PostMessageA path uses a window handle held by the object, e.g. [esi+0x1420] in 0x1000dbe0) `postNotification` 0x1000dbe0, `postMidiNotification`
0x10020b60, and the per-device `dispatchCommand` handlers (`Midi_Device::dispatchCommand` 0x1000a670,
`Wave_Device::dispatchCommand` 0x1000cd80) post completion/notification messages via `PostMessageA`.

## 8. Base object / containers

`CtrlObject::ctor` 0x1001bc50 builds a debug block header (vtable `vt_CtrlObject` 0x1005b19c, 4cc tag
`'ctrl'` at +4, declared size 0x18, 0x44444444 guard fill); ~12 functions reference this vtable, so it
is a shared base. A linked-list/queue template (0x1001b570, 0x1001b870, 0x1001c080, 0x1001c990,
vtables 0x1005b58c / 0x1005b59c / 0x1005b5c0) underlies the device buffer and track lists. Small node
helpers: `listNode3Init` 0x100087f0, `pairStore` 0x10008810, `pairClear` 0x10008840.

## Global map (see `re/names/sound_0_globals.tsv`)

| addr | name | what |
|---|---|---|
| 0x100b4990 | g_timerLock | timer CRITICAL_SECTION |
| 0x100b49a8 | g_timerBusy | onTick reentrancy guard |
| 0x100b49ec | g_soundTimer | global SoundTimer |
| 0x100b4a1c | g_midiDevice | Midi_Device singleton |
| 0x100b49f0 | g_waveDevice | Wave_Device singleton |
| 0x100b49f8/fc,0x100b4a00 | g_primaryBuffer / secondaries | IDirectSoundBuffers |
| 0x100b4a34/38 | g_dsBufferList / tail | active DirectSound buffer list |
| 0x100b4a3c | g_dsCoopLevel | SetCooperativeLevel arg |
| 0x100b4a24 | g_waveInDevice | Wave_In_Device singleton |
| 0x100b4a04 | g_waveInCapture | active capture object |
| 0x100b4a08 | g_openMmioCount | open mmio handle count |
| 0x100b5038 | g_soundThreadState | sound thread state struct (address passed to postSoundThreadMessage) |
