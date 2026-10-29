# S4.10 merge request (module audio_exe)

All changes below were applied in the mirror (log/roadmap/S4/10_mirror.py, port/build/S4_10_mirror) and gated there: with
them, all 205 accepted segments PASS with every digest unchanged (log/roadmap/S4/10_streams_x64.tsv against
log/roadmap/S0/1_segs_x64.tsv). The files this step owns (audio_exe.cpp / .h, test_audio_exe.py, audio_stream.py,
golden_audio_exe.json, re/analysis/data/audio_stream.md) go in as they are; the edits below are to shared or sibling files.

## 1. port/src/sim/modules.inc

Append after the sound modules (any position after `SG_MODULE(audio_core)` works; the registry is by name):

```
SG_MODULE(audio_exe)
```

Then touch port/src/sim/registry.cpp (MSBuild does not rebuild it when only modules.inc changes).

## 2. port/src/sim/tick.cpp, kDrops

Remove these three lines (kDrops is resolved before the registry, so while they stay the module's functions never run from
the frame):

```
    {"playSound", "audio request"},
    {"startSoundSlot", "audio request"},
    {"Snd::status", "audio device status query; the frame discards it (ML+0xf3c)"},
```

Evidence that nothing else changes: all 205 segments PASS with the same digests (all start images hold g_soundReady = 0,
so each request returns at its first test). Without an AudioSink the module writes no stream; with one
(`audio_exe.useSink`) it records the requests.

## 2b. port/src/sim/tick_boot.inc (needed together with section 2)

A boot run starts from the exe's initialised data, where the 300 g_sounds records have no class table yet (the CRT
initializer dynInit_arrayStaticInit_448160 constructs them before gameMain). With startSoundSlot out of kDrops, its
Snd::applyField484750 reads `[vtable + 4]` at va 4 and every boot with frames faulted at record 0 (68 FAIL + 2 NO_FRAMES of 70 rows, measured). Three calls
make the boot's Snd records the ones the original has at the first frame (exact text; the mirror applies them as
BOOT_EDITS in 10_mirror.py):

In `BootCrt()`, after `    cc("staticInit_820f04");`:

```
    cc("arrayStaticInit_448160");   // the CRT initializer dynInit_arrayStaticInit_448160: the 300 g_sounds records
```

In `BootGameMain()`, after `    cc("showTitleScreen");`:

```
    cc("loadSoundTable");                                             // gameMain+0x42e
```

In `BootSetup()`, replace the comment line `    // ML+0x2f playSound(0x80, 0x64, 0, 0, 0): the audio backend's` with:

```
    cc("playSound", 0x80, 0x64, 0, 0, 0);                             // ML+0x2f
```

and in the comment block at the top of BootGameMain, `loadSoundTable (gameMain+0x42e) and the colour cycle are not the
boot's.` becomes `the colour cycle is not the boot's.`. initSound (gameMain, before seedFromTime) stays out of the boot: on
the recordings' device-less path its only lasting effect is g_soundReady = 0, which the exe image already holds.

arrayStaticInit_448160 is registered by audio_exe (its oracle keys arrayStaticInit_448160_live / _zero / _pat are GREEN,
tier EXACT). With these edits the boot gate gives HEAD's 61 rows and S1's / S8's six unchanged (58 PASS + 6, 2 KNOWN,
1 NO_FRAMES: 0 rows differ from log/roadmap/merges/S1S8-4/boot_x64.tsv); the three rows outside that table
(cov_a_quit PASS, cov_a_quit_title NO_FRAMES, cov_r_s2_screens FAIL at record 771 on g_cursorTileX / Y) are uncommitted
recordings of other steps, and cov_r_s2_screens fails the same way on port/build/x64 without audio_exe.

## 3. port/src/sim/audio_core.cpp, audio_crt_name

The core looks a sound.dll rva up in kSoundFunctions (hooks.csv names) and then audio_crt_name. This step registers 74
sound.dll functions with no hooks.csv row as `snd.FUN_<va>` (U-1498) and exports their rva -> name lookup. Add, next to
S4.11's line for audio_crt_function_name (or, if S4.11 is not merged yet, both):

```
const char *audio_exe_sound_name(uint32_t rva);  // audio_exe.cpp (S4.10)
```

at namespace scope beside the other declarations, and inside `audio_crt_name(uint32_t rva)`, after the S4.11 line
`if (const char *n = audio_crt_function_name(rva)) return n;`:

```
  if (const char *n = audio_exe_sound_name(rva)) return n;  // S4.10
```

## 4. port/src/sim/audio_wave.cpp (S4.9's module): argument defects the device-present run found

DsBuffer::setRegionMs (rva 0x10eb0, thunk 0x182f) is `ret 8` and reads [esp + 4] and [esp + 8]. Slot 0x40 of Wave_Device's
table (rva 0x5b304 + 0x40 -> thunk 0x1fd2 -> FUN_1000e830) is `mov eax, [ecx + 0x1ac]; ret`: it takes no argument, so the
value the caller pushed for it stays on the stack and becomes setRegionMs's SECOND argument.

(a) Seven sites push 0xe, call slot 0x40, push eax, call setRegionMs (open 0xb5af..0xb5bf, 0xb687..0xb697,
0xb69c..0xb6ac; start 0xbb62..0xbb72, 0xbbfe..0xbc0e, 0xbc13..0xbc23; restart 0xc112..0xc122). Replace each

```
snd_vcall(ctx, <dev>, 0x40, SndAbi::Thiscall, {0xe})
```

with `snd_vcall(ctx, <dev>, 0x40, SndAbi::Thiscall, {})` (seven occurrences), and each following

```
snd_call(ctx, 0x0000182f, SndAbi::Thiscall, {<buffer>, ms})
```

with `snd_call(ctx, 0x0000182f, SndAbi::Thiscall, {<buffer>, ms, 0xe})` (the mirror does it with
`.replace("), ms});", "), ms, 0xe});").replace(".va()), ms}).eax;", ".va()), ms, 0xe}).eax;")`).

(b) 0x32fc0..0x32fca (the stream-open path, f5c & 0x40 and not 0x80): `push 8; call [edx + 0x40]; mov ecx, [esi];
push eax; call 0x182f`. Replace

```
                unsigned v = snd_vcall(ctx, wd, 0x40, SndAbi::Thiscall, {8}).eax;  // 0x32fc4
                snd_call(ctx, 0x0000182f, SndAbi::Thiscall, {ld<u32>(ctx, self + 0x54), v});  // 0x32fca
```

with

```
                unsigned v = snd_vcall(ctx, wd, 0x40, SndAbi::Thiscall, {}).eax;  // 0x32fc4 (`ret`: the pushed 8 stays)
                snd_call(ctx, 0x0000182f, SndAbi::Thiscall, {ld<u32>(ctx, self + 0x54), v, 8});  // 0x32fc9 / 0x32fca
```

(c) flushSoundList (thunk 0x1f0f, rva 0x8470) reads no argument and no ecx; the core registers it cdecl, arity 0. Its five
call sites (0x7452, 0x7480, 0x7a31, 0x7a69, 0xbe34) load ecx first, which the port passed as a thiscall slot. Replace each
`snd_call(ctx, 0x00001f0f, SndAbi::Thiscall, {...})` with `snd_call(ctx, 0x00001f0f, SndAbi::Cdecl, {})` (five
occurrences).

Without (a) and (c) the core's arity check throws on the first device-present Wave_Device::open; with them the
device-present chain runs to the end. (b) is not reached by the device-present run (the path needs a stream whose flags
take it); it is fixed from the instructions.

## 5. Pending callees owned by other modules

The oracle judges these rows GREEN with the callee served by the replay host; in production they stop with
MissingResource{<name>} until the owner registers the callee (no change needed in audio_exe then: calls go by name):

- `return0_479820` (exe rva 0x79820, `xor eax, eax; ret`, the constant default of the window-class tables; a slot of the
  Snd tables), reached by NodeList::playChildSuspendingSounds.
- `NodeList::dtor` 0x487390, `NodeListB::dtor` 0x487a60, `NodeListC::dtor` 0x487b80 (engine-glue, C4), the tail-jump
  targets of the exit routines gMusicDeviceDtor_483e00 / gSfxDeviceDtor_483e40 / gVoiceDeviceDtor_483e80.
- the exe's CRT operator new / operatorDelete_4a4ffc: served by the module's own arena without a host (U-1495).

## 6. UNCERTAINTIES.md

Add the rows of log/roadmap/S4/10_uncertain.tsv (U-1495..U-1499). Update these existing rows:

- U-1411 -> resolved: `S4.10: sound.dll's heap entry points run as port code down to KERNEL32 (S4.11's audio_crt ports the
  static CRT allocator); its HeapAlloc / HeapReAlloc / HeapSize / HeapFree imports go to the AudioSink with no host, and the
  device-present run's DeviceSink serves them from an arena in the context's address space (16 MB chunks at 0xF4000000..
  0xFC000000, freed blocks reused by size). In the replay the fixture's heap stand-in still claims them.`
- U-1415 -> resolved: `S4.10 survey (log/roadmap/S4/10_thunk_survey.py, 10_thunk_refs.py, 10_thunk_survey.tsv): the table
  holds 1,207 thunks (rva 0x1005..0x2793). 660 targets are registered port functions; 65 are hooks.csv rows marked dead; 482
  have no hooks.csv row: 156 of them nothing references, 316 are referenced only from data (class-table slots), 10 from code
  (0x2e460, 0x2e3c0, 0x9210, 0x2e530, 0x1b1f0, 0x1c060, 0x1e680, 0x2f8b0, 0x2c270, 0x24260). No thunk target outside the
  registered set is reached by any S4.10 key or by the device-present run (0 host fallbacks); a call to one in production
  is MissingResource, never a silent skip.`
- U-1416 -> resolved: `S4.10 keeps the chosen addresses: sink objects 0xFEC00000 (64 KB), sink traps 0xFED00000..0xFEDFFFFF,
  the core's frames 0xFEB00000 and box 0xFEAF0000; the exe sound layer's arena is 0xFE000000 (1 MB) and the DeviceSink's
  heap 0xF4000000..0xFC000000, both below them. The device-present run (stream with 178 sounds, 40 ticks) and all 205
  segments run with all of them mapped and no overlap (Memory::map refuses an overlap).`
- U-1494 -> resolved: `S4.10: the production AudioSink serves mmioOpenA names from GameFiles (DeviceSink in audio_exe.cpp:
  the name as the engine passes it, backslashes turned to slashes and a leading `.\` dropped, read with GameFiles::read_all;
  memory files per re/frida/js/mmiostandin.js). The device-present run opens the install's .wav files this way.`
- U-1421 stays open, with this added: `S4.10 call graph (log/roadmap/S4/10_fpcw_paths.py, 10_fpcw_paths.txt): no function
  on either path loads a control word. codecDecodeFrame <- openMmioSound <- Sound5c450::openMmio, which has no direct caller
  and sits in class-table slot 0x5c4dc, called through [reg + 0x8c] from FUN_10029c90, FUN_1002be90, FUN_1002d9c0,
  FUN_1002ec70, FUN_1002f1c0, FUN_1002f6e0. codecEncodeFrame <- loadWaveResource <- WaveInDevice_requeueBuffer <-
  soundWindowProc (<- soundThreadPump) and soundWindowSubclassProc (<- pumpWaveMessages <- loadSoundFile,
  Midi_Device::enableOutput, Seq::loadFile). The only fldcw sites outside the CRT are the six Voice_mix* leaves and
  streamRenderBlock / streamRenderTail, reached through Wave_Device::serviceTick (table slot 14 of 0x5b304, the timer
  thread), which leave 0x0f7f. Which thread runs each codec path, and so the word it inherits, needs a game run: NEED_GAME.`

## 7. Tests

`port/test/test_audio_exe.py` (marker port). Default: static checks, replay of every pinned key, negative controls, two
segment streams, the device-present run and its controls. `SG_EMU_ORACLE=1` adds the live oracle on a sample of keys.
Run it in its own process (log/p38/suite_by_file.sh).
