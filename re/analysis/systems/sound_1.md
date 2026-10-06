# sound.dll — ID sound_1 (0x10027e20 – 0x10051319, 404 functions)

> Audit 2026-10-06: 10 round-1 names of this range were withdrawn and 0x10047771 renamed _unlock (cited strings/imports never
> touch; re/names/withdrawn.tsv) and 7 rows corrected in re/names/sound_1*.tsv. Where this writeup gives `+0xNN`
> offsets taken from Ghidra's `param_1[N]` (dword indices), the byte offset is 4*N: destroyMixerVoices walks from
> +0x6c (not +0x1b), flushMmioBuffer and startDevicePlayback test the flags byte at +0x58, WaveInDevice_ctor
> stores the count at +0x38, caps at +0x8 and a WAVEFORMATEX (mono, 8000 Hz, 16-bit) at +0x48. The names TSV is
> authoritative over this text.

Module `original\sound.dll`, DirectSound/winmm audio engine, release build. Imports span
KERNEL32 (threads, events, critical sections, heap), USER32 (a hidden message window),
ole32 (`CoCreateInstance` for DirectSound), WINMM (`waveIn*`, `midiOut*`, `mmio*`, `time*`)
and DSOUND (one ordinal import). The DLL's exports (below this range) name the class layer:
`Wave_Device`, `Midi_Device`, `Wave_In_Device`, their `Dll_*_Device` factories,
`WaveInDeviceMgr`, and the C API `create_sound`/`delete_sound`/`release_sound`/
`init_sound_timer`/`get_sound_version`. This range holds the worker thread, the WAV/MP3 file
loader, the wave-input (capture) device, and the statically linked MSVC6 C runtime.

## 1. Sound worker thread and hidden message window

A dedicated thread owns a hidden window and a message loop; the rest of the engine talks to it
by posting thread messages.

- `g_soundThreadState` (0x100b5038) is the thread-control struct: byte flag at +0x00 (bit0 =
  running), a `CRITICAL_SECTION` at +0x04 (also addressed directly as `g_soundThreadCrit`
  0x100b503c), thread id at +0x1c, thread handle at +0x20, manual-reset event at +0x24.
- `initSignalQueue` (0x1002a7c0) constructs it (InitializeCriticalSection + CreateEventA).
- `startSignalThread` (0x1002a920) ResetEvents, sets bit0 and starts the worker thread at
  entry `0x10001366` (below this range) via `_beginthread`, then SetThreadPriority(,0).
- `stopSignalThread` (0x1002a810) clears bit0, signals the event under the lock, Sleep(0),
  and clears the id/handle fields.
- `soundThreadPump` (0x1002a970) is the thread body: sets `g_soundThreadRunning` (0x100b5060),
  records `g_soundThreadId` (0x100b5054), waits on `g_soundThreadEvent` (0x100b505c) with
  `MsgWaitForMultipleObjects`, and drains `PeekMessageA`, dispatching each message to
  `soundWindowProc` (0x100288a0) under `g_soundThreadCrit`.
- `postSoundThreadMessage` (0x1002aa90) posts to the thread id at this+0x1c with
  `PostThreadMessageA`, retrying with an `OutputDebugStringA((string in the binary))` + Sleep(100).

The message window itself:
- `createSoundWindow` (0x10028740) registers class `snd_class` (WNDCLASSA `g_sndWindowClass`
  0x10063e18) and creates the hidden window `snd_window`, storing the HWND at this+8.
- `destroySoundWindowObj` (0x10028630) is the scalar deleting destructor (vtable
  `vtbl_SoundWindowObj` 0x1005b8ac); it unsubclasses with SetWindowLongA(hwnd,-4,saved-proc).
- `soundWindowProc` (0x100288a0, DefWindowProcA) and `soundWindowSubclassProc` (0x10028b70,
  CallWindowProcA) handle buffer-completion messages (ids around 0x7eb/0x7ec) carried on
  WAVEHDR objects.

## 2. WAV / MP3 file loading (mmio)

RIFF files are read through the Win32 `mmio*` API.

- `parseRiffWave` (0x100326a0, 3168 bytes) is the core reader: `mmioOpenA` then
  `mmioDescend`/`mmioAscend`/`mmioRead` over the chunks `RIFF` (0x10063ee0), `WAVE` (0x10063ed8),
  `fmt ` (0x10063ed0), `data` (0x10063eb8), `cue ` (0x10063ec0), `smpl` (0x10063ec8), detecting
  `MP3`/`mp3` (0x10063ee8/0x10063eec) tagged payloads. It fills `_MMCKINFO` chunk structs.
- `openMmioSound` (0x10030840) opens a file and wires its MMIOINFO buffer, reading float
  samples; `loadWaveResource` (0x100316c0) is the thiscall entry that validates the voice index
  (capped at 100 at this+0x68, SNDERR 10/0x10) and loads.
- `streamWaveSamples` (0x10030ba0) streams decoded PCM into a caller `double*` buffer, gated on
  stream handle `g_100b49f4` (SNDERR 0x13 when absent).
- `closeMmioSound` (0x10035ca0) `mmioClose`s the handle at obj+0x44, frees the MMIO buffer,
  decrements `g_openMmioCount` (0x100b4a08) and unlinks the sound from `g_openSoundList`
  (0x100b49c8). `flushMmioBuffer` (0x10037920) uses mmioAdvance/mmioSeek/mmioSetBuffer on the
  I/O window.

The RIFF object layout uses flag bits at obj+0x16 (bit1 = open, bit6 = buffered) and an MMIO
handle at obj+0x44; several small helpers around 0x10031000–0x10034000 are thin `mmioClose`/
`mmioRead`/`mmioSeek` wrappers over the same layout (left unnamed, see notes).

## 3. Wave input (capture) device — `Wave_In_Device`, vtable 0x1005c604

- `WaveInDevice_ctor` (0x10038650) installs vtable `vtbl_WaveInDevice` (0x1005c604), records the
  `waveInGetNumDevs` count at +0xe, reads caps with `waveInGetDevCapsA` into +2, and sets the
  default format (8000/16000 Hz, 2ch, 16-bit) at +0x13/+0x14/+0x15. Device handle lives at +0x44,
  the WAVEFORMATEX at +0x48, and a WAVEHDR list head at +0x5c.
- `WaveInDevice_open` (0x10039050) calls `waveInOpen(this+0x44, this+0x48, callback 0x1000179e,
  this, CALLBACK_FUNCTION)`; `0x1000179e` is the exported `WaveInDeviceMgr::call_back`.
- `WaveInDevice_close` (0x10039140) stops/resets and `waveInUnprepareHeader`s the WAVEHDR list
  under `g_soundThreadCrit`, then `waveInClose`.
- `WaveInDevice_start`/`_stop`/`_reset`/`_addBuffer`/`_requeueBuffer`/`_queryCaps`
  (0x10038b20/0x10038a00/0x10038f10/0x10038da0/0x100393d0/0x10038f60) are the thin per-API
  methods. `onWaveInCallbackMessage` (0x10038fd0) turns the device callback (message 0x3c0) into
  either `PostMessageA(g_soundWindow 0x100b49c0, 0x7f0, ...)` or a post to the sound thread via
  `postSoundThreadMessage`, depending on bit5 of `g_soundEngine->+0x1a4`.

## 4. Named voices and the engine object

- `g_soundEngine` (0x100b4a20) is the global engine instance (flags at +0x1a4, a sub-object at
  +0xf8). 0x1002fc10 (unnamed since 2026-10-06; the
  name `createBlankVoice` cited a string it does not reference) allocates a 0x10-byte object, constructs it through
  0x10001299 and passes it to a method of the engine.
- `buildVoiceRxName` (0x1002ed50) and `buildVoiceTxName` (0x1002f220) are constructors that set a
  default level (field[1]=0x7f) and copy the literal names `Voice Rx` (0x10063e80) /
  `Voice Tx` (0x10063e8c) into the object's std::string at this+3.
- `destroyMixerVoices` (0x1002c5e0, vtable 0x1005bcb4) frees 0x28 (40) voice-pointer slots from
  this+0x1b. Timing helpers `markSoundTime` (0x1002ffa0), `updateSoundTimer` (0x1002ffd0) and
  `getPlaybackTime` (0x1002baa0) stamp `timeGetTime()` into object fields; `startDevicePlayback`
  (0x1002b7c0) lazily builds the device sub-object (this+0x10) and calls its vtbl+0x1c method.

### Software mixers (unnamed)

`0x10027e20` and `0x100281d0` (748/691 bytes, reached only through function pointers) are the two
software sample mixers. Both read a voice struct (param_4) whose sample source at +8 is a buffer
descriptor {data +0x18, length +0x1c, position +0x20, loop point +0x24}; `param_5==1` selects a
3D/stereo slot layout (fields +0x1e8/+0x1f8/+0x1358) versus a plain layout (+0x68/+0x78/+0xb8).
They are documented but not given rows (no import/string/caller evidence — see notes).

## 5. C runtime (0x10041aad – 0x10051319)

The tail of the range is the statically linked MSVC6 (12.00.8168) C runtime: `_beginthread`
(crtBeginThread 0x10042690), `_endthread` (0x10042a8e), the `HeapAlloc`/`HeapFree`/`HeapReAlloc`/
`HeapSize` allocator wrappers (0x10042bd1/0x10042cc9/0x10045252/0x100455eb), heap init/select
(0x10045d10/0x10045bc8), `_XcptFilter` (0x100459c3), process exit (0x10045181), lock-table init
(0x1004751d), small-block VirtualAlloc commit (0x10047148), stdio/startup (0x10047771,
references the (string in the binary) banner 0x1005f1b4), `_ValidateRead`/`Write`
(0x10049604), and the `%lu` integer formatter (0x100414f0). FidDb additionally identifies
0x10049604 (`_ValidateRead`/`_ValidateWrite`) and 0x1004e4b3 (`__free_lc_time`). The many small
odd-addressed fragments between 0x10041aad and 0x10051319 are CRT internals and were not named
individually.

## Open questions

- The software mixers 0x10027e20/0x100281d0 have no import/string/caller evidence in this slice
  (called via pointers from the engine); their exact bit-depth/channel variant is not determined.
- Writers of `g_soundEngine` (0x100b4a20), `g_soundWindow` (0x100b49c0) and the open-sound list
  head live below this range (the exported device layer) and were not confirmed here.
