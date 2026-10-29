# S4.4 merge request (mixers and channel renderers, module audio_mixer)

Proven in the mirror build `port/build/S4_4_mirror` (log/roadmap/S4/4_mirror.py: HEAD of port/ + S4.2's accepted files and
C ABI patch + this step's files + exactly the edits below). Apply after S4.2's merge request (log/roadmap/S4/2_merge.md):
the module calls the audio core (portsim/sim/audio.h) and the C ABI it adds. audio.h and sound_replay.py are NOT changed.

## 1. port/src/sim/modules.inc

Append one line after S4.2's `SG_MODULE(audio_core)`:

```
SG_MODULE(audio_mixer)
```

(touch port/src/registry.cpp afterwards.)

## 2. port/src/sim/audio_core.cpp (S4.2's file): sound.dll's __ftol through the dispatcher

renderChannelStreamed and renderChannelVoice call sound.dll's static CRT `__ftol` (rva 0x42550, st(0) in, edx:eax out).
Its hooks.csv row collides with other modules' `__ftol` rows, so the dispatcher needs the CRT table entry:

a. after `#include "portsim/sim/audio_sink.h"` add `#include "portsim/sim/audio_mixer.h"`;
b. in `audio_crt_name`, after the line `  if (rva == snd_crt::strncpy_) return "audio_core.crt.strncpy";` add

```cpp
  if (rva == kSndFtolRva) return "audio_mixer.crt.ftol";  // S4.4: __ftol (portsim/sim/audio_mixer.h)
```

## 3. port/src/portsim_c.cpp: the x87 unit's release

a. after S4.2's `#include "portsim/sim/audio.h"` add `#include "portsim/sim/audio_mixer.h"`;
b. in S4.2's `sg_state_free`, after `if (ctx) portsim::audio_release(ctx->ctx);` add

```cpp
  if (ctx) portsim::x87::unit_release(ctx->ctx);  // S4.4: the x87 unit
```

(the unit is per-context state keyed by the Ctx address, like the audio core's; without this a new context allocated at a
freed one's address would inherit its FPU state.)

The Win32 runner needs no change (audio_mixer.* entries go through sg_call, images through context files).

## 4. Files of this step

All inside files_owned: port/src/sim/audio_mixer.cpp, port/include/portsim/sim/audio_mixer.h, port/src/sim/audio_x80.inc,
port/test/test_audio_mixer.py, port/test/golden_audio_mixer.json (57 keys, 2,001 vectors), log/roadmap/S4/4_*.
Local corpus (gitignored): port/corpus/audio/<key>.json.z for the 57 keys (bundles with an `x87` entry image per vector);
regenerate with `~/bin/py -3.12 -m port.test.test_audio_mixer oracle --update`.

## 5. UNCERTAINTIES.md

Append the rows of `log/roadmap/S4/4_uncertain.tsv` (U-1430..U-1437).

## 6. Gate command lines and expected numbers (for S4/MERGE.md)

```
~/bin/py -3.12 log/roadmap/S4/4_mirror.py --w32                                    # until merged: mirror DLLs (~5 min)
~/bin/py -3.12 -m port.test.test_audio_mixer oracle --out <tsv>                    # 57 keys: 56 GREEN, 1 GREEN_PENDING (Mixer_ctor: S4.7)
~/bin/py -3.12 -m port.test.test_audio_mixer replay [--w32]                         # 57 keys PASS (Mixer_ctor PASS_PENDING)
~/bin/py -3.12 -m port.test.test_audio_mixer negctl --mode 1 --out <tsv>           # every function RED
~/bin/py -3.12 -m port.test.test_audio_mixer negctl --mode 2 --out <tsv>           # the c3fh long-double control RED on all 8 resamplers
~/bin/py -3.12 -m pytest port/test/test_audio_mixer.py [--backend win32]            # default: bundles + goldens + x87 operation probes
SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_mixer.py              # + the live oracle per key
```

## 7. Notes for the other S4 steps

- The x87 helpers that run under the CALLER's control word (S4.5's Seq_setTempo, DsBuffer::setVolume, Channel_setRate /
  recomputeVoiceGains / setReverseRate / stepVolumeFade, Voice_resetDefaults, Sound5b6a8_ctor; log/c3/c3fh_notes.txt
  section C) should compute on the same unit (`portsim::x87::unit(ctx)` and the x87:: operations): a mixer leaves 0x0f7f
  and up to five registers there, and the port's next sound.dll call must see them, as the timer thread does.
- A port function that passes a stack local's address to a callee uses the frame block (`x87::frame_push` / `frame_pop`).
- port/test/test_audio_mixer.py extends sound_replay in-process (LivePort subclass, KeyRun setup / arm / run wrappers,
  bundles with an `x87` field, goldens with `x87_exit` digests); a step judging x87 code can import it (`use_mirror`) or
  fold the same into sound_replay.py.

## 8. CLAUDE.md paragraph (after S4.2's)

```
Step S4.4, the mixers and channel renderers (2026-10-24, module audio_mixer, notes log/roadmap/S4/4_notes.md,
U-1430..U-1437): 38 sound.dll rows, judged against the emulated original by port/test/test_audio_mixer.py (S4.2's harness
plus the x87 unit). portsim/sim/audio_mixer.h holds the VIRTUAL X87 UNIT, per-context state that persists across sound.dll
calls like the thread's FPU (control word, TOP, C0..C3, tags, eight 80-bit registers): the Voice_mix* leaves and the stream
renderers load 0x0f7f themselves (64-bit significand, chop), never restore it and return with 4-5 registers pushed;
renderChannelSingle / Streamed / Voice compute on top. Every x87 instruction of these bodies is one x87:: operation under the
unit's word: on Win32 the instruction itself (inline asm on tbyte operands), on x64 port/src/sim/audio_x80.inc (128-bit exact
significands, one rounding per operation to 24 / 53 / 64 bits by the precision and rounding control, the SDM NaN rules); the
two agree on every probe record, and both equal Unicorn except that the x87 quiets a signaling NaN on fld / fst qword and
Unicorn does not (U-1430). The harness loads the unit per vector with the FPU the original arm found and compares the unit
the port leaves with the FPU the original left (cw, TOP, C0..C3, registers in use and their bits; the caller's fstp applied
for double returns); it empties the emulator's never-initialised tags at key start (digests unchanged on all keys, U-1431).
Stack locals passed to callees live in the frame block (0x6E000000, 64 KB). Keys: the 47 registry keys, six MMX renderers as
derived keys on the rated mixers' fixtures (two of them twice), and two CHAINED keys where renderChannelSingle /
renderChannelVoice run the real mixers (stubs reverted in the fixture VM, sample records in a fixture-owned region): 57 keys,
2,001 vectors, port == emulated original in layouts A / B / C, x64 and Win32 from bundles; Mixer_ctor is GREEN_PENDING on
S4.7's Sound_setSourceType. Negative controls: mode 1 of every function RED, mode 2 (the c3fh long-double body: 53-bit
doubles rounded to nearest) RED on all eight resamplers.
```
