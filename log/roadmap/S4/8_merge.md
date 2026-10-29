# S4.8 merge request (modules audio_ds and audio_dev)

Everything below is proven in the mirror build `port/build/S4_8_mirror` (log/roadmap/S4/8_mirror.py: HEAD of port/ plus
S4.2's files and C ABI patch, this step's files, and exactly the edits of sections 1 and 2). It assumes S4.2's merge
request (log/roadmap/S4/2_merge.md) is applied first: audio_core's modules.inc line, the C ABI addition, sg_state_free's
audio_release call.

## 1. port/src/sim/modules.inc

Append, after `SG_MODULE(audio_core)`:

```
SG_MODULE(audio_ds)
SG_MODULE(audio_dev)
```

(touch port/src/registry.cpp afterwards: MSBuild does not rebuild it when modules.inc changes.)

## 2. port/src/portsim_c.cpp

a. After `#include "portsim/sim/audio.h"` (S4.2's include) add `#include "portsim/sim/audio_dev.h"`.

b. In `sg_state_free`, after S4.2's `if (ctx) portsim::audio_release(ctx->ctx);` add

```cpp
  if (ctx) portsim::audio_dev_release(ctx->ctx);  // S4.8: frame block, pending entry esp (U-1470)
```

(audio_dev keeps a per-context table keyed by the Ctx address, like audio_core's; without the release a context allocated
at a freed one's address inherits its pending entry esp. The replay harness also clears the entry esp after every call.)

## 3. port/src/sim/audio_core.cpp (S4.2's file; proposals, not needed by this step's gate)

a. U-1472: in `snd_call_va`, an unclaimed target inside the image whose bytes are `FF 25 <slot VA>` (an import stub, e.g.
   rva 0x3a978 `jmp [iat 0xb83b4]` = DSOUND #2) should be routed as `snd_import` of that slot (so with no host it reaches
   the AudioSink by name) instead of a FALLBACK. DirectSoundDevice::enumerate calls that stub; in every replay key c3fk's
   fixture claims it, so the gate sees no fallback.

b. U-1470: `SndLocals` (portsim/sim/audio_dev.h) and the harness's entry esp belong in audio.h for S4.3..S4.9: functions
   that pass the address of a stack local are common in sound.dll (COM out-parameters, MSG, WAVEFORMATEX), and a fixture
   that logs the pointer compares the address. The dispatcher could then pass a callee's entry esp (caller esp minus the
   pushed slots and the return address) instead of the first-SndLocals-takes-it rule.

## 4. port/test/sound_replay.py (S4.2's file)

This step's replay front end is `log/roadmap/S4/8_replay.py` (+ `8_fixtures.js`). It changes nothing in sound_replay.py;
it wraps it (module attributes and subclasses). Two ways to merge:

- Simplest: copy `log/roadmap/S4/8_replay.py` to `port/test/sound_replay_s48.py` and `log/roadmap/S4/8_fixtures.js` to
  `port/test/sound_replay_s48_fixtures.js` (no edit: the wrapper finds its ROOT and its fixtures from either place, and
  port/test/test_audio_ds.py / test_audio_dev.py load port/test/sound_replay_s48.py when it exists).
- Or fold the hooks into sound_replay.py for S4.3..S4.9 (each is independent):
  1. a golden file per module (`--golden`; S4.8 uses port/test/golden_audio_ds.json / golden_audio_dev.json);
  2. DERIVED entries for emu-only rows with a twin, CUSTOM keys (a registry-shaped spec + a fixture defined in a step's own
     JS file, evaluated into the key's VM context after the scripts load: `KeyRun.setup` + `{"op": "eval"}`), EXCLUDED
     registry keys the VM cannot run (postMidiNotification: FIXTURE_ERR);
  3. per port call, after `audio_core.setup`: `audio_dev.fpcw <word>` (the emulator's FPCW | 0x40 in the oracle, recorded
     in the golden entry as "fpcw" when not 0x027f) and `audio_dev.esp <entry esp>` (`((STACK_INIT - 0x400) & ~0xF) -
     4 * stack slots - 4`), and `audio_dev.esp 0` after the call;
  4. STACK_PAGES = the emulator stack below STACK_INIT - 0x8000 (the top 32 KB is mapped into the port), and the live
     second arm runs the port with the emulator's depth + 1 and esp = entry esp - 0x400, so the port's host calls build
     their frames below the port's locals;
  5. the frame block range 0xFEB00000 + 64 KB mapped into the emulator before each vector;
  6. the original's import stand-in log per vector in the golden entry ("implog"), and the `sinklog` check.

## 5. New files of this step outside port/src

- `port/test/golden_audio_ds.json`, `port/test/golden_audio_dev.json` (files_owned): emulator digests, regions, tiers,
  the control word of the 0x0f7f key, the original's import logs. Regenerate with
  `SG_EMU_ORACLE=1 ~/bin/py -3.12 log/roadmap/S4/8_replay.py oracle --family ds|dev --update` (it also writes bundles).
- `port/corpus/audio/*.json.z` (local, gitignored): the bundles of the 174 keys (70 ds, 104 dev).

## 6. UNCERTAINTIES.md

Append the rows of `log/roadmap/S4/8_uncertain.tsv` (U-1470..U-1479).

## 7. Gate command lines and expected numbers (for S4/MERGE.md)

```
~/bin/py -3.12 log/roadmap/S4/8_mirror.py --w32                                   # until merged: mirror DLLs + runner
~/bin/py -3.12 -m pytest port/test/test_audio_ds.py                               # 130 passed, 71 skipped (oracle)
~/bin/py -3.12 -m pytest port/test/test_audio_dev.py                              # 170 passed, 104 skipped (oracle)
SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_ds.py               # + 70 oracle keys + the setVolume grid
SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_dev.py              # + 104 oracle keys
~/bin/py -3.12 log/roadmap/S4/8_replay.py oracle --family ds   --out <tsv>        # 70 keys GREEN, 1,215 vectors, 0 fallbacks
~/bin/py -3.12 log/roadmap/S4/8_replay.py oracle --family dev  --out <tsv>        # 104 keys GREEN, 1,316 vectors, 222 fallbacks (U-1477)
~/bin/py -3.12 log/roadmap/S4/8_replay.py negctl --family ds|dev --out <tsv>      # 54 + 62 functions RED, 0 blind
~/bin/py -3.12 log/roadmap/S4/8_replay.py replay --family ds|dev [--w32]          # 70 + 104 keys PASS from bundles
~/bin/py -3.12 log/roadmap/S4/8_replay.py sinklog --family ds|dev                 # 5 + 31 keys with import logs, all EQUAL
```

## 8. CLAUDE.md paragraph (after S4.2's)

```
Step S4.8, DirectSound and devices (2026-10-24, modules audio_ds and audio_dev, notes log/roadmap/S4/8_notes.md,
U-1470..U-1479): the 116 S4.8 rows of re/roadmap/S4_functions.tsv (DsBuffer / Ds3DBuffer / Ds3DListener /
DirectSoundDevice and the DsBuffer list; Ctrl objects, CommandQueue, sound timer clients, the MIDI device exports, the
sound thread and window procedures, the static-init stubs) are ported against portsim/sim/audio.h from the hook sources
(re/tools/snd_hook2port.py, then hand edits). Every DirectSound call is a COM method through the interface's vtable and
every DSOUND / ole32 / WINMM / USER32 / KERNEL32 import goes through its IAT slot, so with no host they are AudioSink
commands. portsim/sim/audio_dev.h: SndLocals keeps a local the original passes by address at the original's offset below
the entry esp the replay gives ("audio_dev.esp"; c3fl's GetCurrentPosition stub logs the pointer), else in a frame block
at 0xFEB00000; snd_fpcw is the caller's x87 control word ("audio_dev.fpcw"). DsBuffer::setVolume runs on a soft 80-bit
model of the reference emulator's x87 (fmul rounded to the word's precision and rounding, fyl2x through double):
equal to the emulated original on its 30 vectors at 0x027f / 0x0f7f and on a 69,680-call grid at four words (a carry
bug at 64-bit round-up found by the grid). Judged by log/roadmap/S4/8_replay.py over S4.2's sound_replay: 174 keys
(70 ds, 104 dev), 2,531 vectors, port == emulated original in layouts A / B / C, x64 and Win32 from bundles; the six
emu-only rows on twin keys or fixtures of the step (8_fixtures.js: setMasterVolume, delete_sound, postMidiNotification
with the engine set by the fixture: its registry key waits for the live engine); 116 / 116 functions RED; the port's
import commands equal the original's import stand-in logs on all 36 keys that have one. 74 fallback calls remain
(S4.7's SoundTimer / SoundWindowObj rows and the CRT's _atexit). Segment gate 205 / 205 x64 and Win32, digests equal.
```
