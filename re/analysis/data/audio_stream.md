# The AudioSink command stream of the exe's sound layer (S4.10)

Module `audio_exe` (port/src/sim/audio_exe.cpp, portsim/sim/audio_exe.h) ports golf_clean.exe's sound layer: the Snd /
SndBase / SndRoot / VoiceRx classes (class tables at exe rva 0xbac68, 0xbacf8, 0xbadec, 0xbadf8), playSound,
startSoundSlot, initSound, loadSoundTable, shutdownSound(System), the sound-library loader (soundVersionCheck,
freeSoundLib, the g_soundFns slots) and the device holders. It runs on the port's sound.dll engine (audio_core and the
S4.3..S4.11 modules): the exe calls sound.dll through the function pointers soundVersionCheck fills with GetProcAddress, and
the port follows them into the engine like any other call.

## What the stream is

With an `AudioSink` attached (`audio_exe.useSink`), every call into the module from outside it (depth 0: the frame step,
a command, a test) is written to the sink as a command, in call order:

| line (as `audio_exe.streamLog` writes it) | meaning |
|---|---|
| `import portsim!frame <t>` | the first request of a frame: `t` is that frame's head clock (`tk::cur_io().t_head`) |
| `import golf_clean.exe!<name> <args...>` | one request: the hooks.csv name of the exe function and its argument slots in decimal |
| `import <DLL>!<API> <args...>` | an import the engine (or the exe's sound layer) calls with no host: KERNEL32 heap and sync, WINMM mmio / timers / MIDI, DSOUND, ole32, USER32 |
| `com <iface>.<slot> <args...>` | a call through a COM interface object the sink handed out (1 = IDirectSound, 2 = IDirectSoundBuffer) |

`port/test/audio_stream.py` (`frame_lines`) rewrites the frame marker as `frame <record> <t>` (record k = the segment's
frame-hash record whose T equals t, -1 if none) and a request as `req <name> <args...>`, and writes one file per segment to
port/corpus/audio_stream/ (local).

The DeviceSink of the device-present test also annotates some lines after `  ; `: the file an `mmioOpenA` opened (with
`(absent)` when the install has no such file), and for `com 2.19` (IDirectSoundBuffer::Unlock) the byte counts, the count of
bytes that are not silence and an FNV-1a digest of the unlocked region.

## The segments (gate b)

All 205 start images hold g_soundReady = 0 (no device: the recordings ran with `C3FT_AUDIO=none`), so every request
returns on its first test and nothing else reaches the sink.

| | |
|---|---|
| segments | 205 PASS, every digest equal to log/roadmap/S0/1_segs_x64.tsv |
| segments with requests | 169 |
| requests | 4,964 (playSound 4,937, startSoundSlot 16, Snd::status 11) |
| frames with requests | 4,599, all matched to a frame-hash record (0 `frame -1`) |
| other commands | 0 |

Reproduce: `~/bin/py -3.12 -m port.test.audio_stream stream --dll <portsim_c.dll> --jobs 3 --out <tsv>` (one row per segment,
log/roadmap/S4/10_streams_x64.tsv), then compare its `digest` column with the baseline.

tick.cpp's kDrops list resolves a name before the registry, so the three names must leave kDrops for the requests to
reach the module (log/roadmap/S4/10_merge.md).

## The device-present path (gate c)

`audio_exe.deviceFixture(1)` takes a segment start image and clears what the original's device-less start left: the
sound-library globals (soundDll, g_soundReady, g_soundFns, g_soundHead / g_soundTail), the device holders' +0x14 and every
g_sounds record's device / list / name words (+0x40, +0x48, +0x4c, +0x50, +0x68). `audio_exe.deviceStep(step, n)` then makes
the calls gameMain and the frame make: 1 initSound(1, 0x8a), 2 loadSoundTable(), 3 playSound(i, 100, 0, 0, 0) for the first
n slots whose file the install holds, 4 n multimedia-timer ticks (the callback timeSetEvent registered, called
(id, 0, user, 0, 0) stdcall, the virtual clock +10 ms each), 5 shutdownSoundSystem().

The DeviceSink (audio_exe.cpp) answers the imports with no host:
- the heap from 16 MB chunks mapped on demand at 0xF4000000..0xFC000000, a freed block reused by the next allocation of its
  rounded size;
- critical sections, events, TLS, Interlocked, version queries; CreateThread is recorded and never run;
- timeGetTime / GetTickCount from a virtual clock; timeSetEvent records the callback;
- mmio over GameFiles: buffered and unbuffered reads, seeks, RIFF descend / ascend, get / set info, advance, setBuffer, and
  memory files with the semantics re/frida/js/mmiostandin.js measured against winmm (the buffer is the file and always
  buffered, mmio allocates it when pchBuffer is NULL, MMIO_CREATE starts it empty, seeks stay in [0, pchEndRead],
  mmioAdvance(READ) returns MMIOERR_CANNOTREAD);
- DirectSoundCreate / DirectSoundEnumerateA (one driver, `Primary Sound Driver`, GUID NULL) and CoCreateInstance; the
  IDirectSound and IDirectSoundBuffer methods (CreateSoundBuffer allocates the storage; Lock / Unlock / Play / Stop /
  GetCurrentPosition / GetCaps);
- midiOutOpen; waveIn reports no device.

Result on sandbox_basic/00 (`~/bin/py -3.12 -m port.test.audio_stream device`): initSound 0, loadSoundTable 0 (219
allocations), 178 playSound requests (the slots whose file exists, U-1499), 40 ticks, shutdownSoundSystem 0; 0 host
fallbacks; the IDirectSoundBuffer Unlocks carry rendered, non-silent samples. The stream digest is pinned as `$device` in
port/test/golden_audio_exe.json.
