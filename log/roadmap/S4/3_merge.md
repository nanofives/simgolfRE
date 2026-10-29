# S4.3 merge request (the LPC-10 speech codec, module audio_codec)

Everything below is proven in the mirror build `port/build/S4_3_mirror` (log/roadmap/S4/3_mirror.py, a copy of 7_mirror.py: HEAD of port/ plus every
accepted S4 module as it is on disk (audio_core, audio_chan, audio_mixer + audio_x80.inc, audio_seq, audio_ds, audio_dev, audio_sound) plus this step's
files plus 7_mirror.py's edits (2_cabi C ABI patch, the S4.4 / S4.8 releases in sg_state_free, S4.4's `__ftol` row in audio_crt_name)). The module
depends on S4.2's merge (audio.h, audio_core.cpp, soundstate.h, the C ABI patch) and on S4.8's audio_dev.h / audio_dev.cpp (SndLocals, U-1420):
apply 2_merge.md and 8_merge.md first. It is independent of S4.9.

## 1. port/src/sim/modules.inc

Append one line after the last audio module line (order does not matter):

```
SG_MODULE(audio_codec)
```

(touch port/src/registry.cpp afterwards: MSBuild does not rebuild it when modules.inc changes.) Without it
`port/test/test_static.py::test_every_module_file_is_listed` fails on `audio_codec`. port/CMakeLists.txt needs no change (it globs src/sim/*.cpp).

## 2. Files outside files_owned

None edited. S4.2's port/test/sound_replay.py, audio.h, audio_core.cpp and re/tools/snd_hook2port.py are used as they are; S4.8's audio_dev.h
(SndLocals) is included, not edited. The step's runner `log/roadmap/S4/3_run.py` (a copy of 7_run.py: own golden file, own mirror, OWN_STEPS
S4.2..S4.8 with S4.3, no clock keys, no x87 word keys) wraps sound_replay.

## 3. Proposals (not applied, not needed by this step's gate)

a. S4.2b: replace the body of `CodecFrame` (head of port/src/sim/audio_codec.cpp) by audio.h's frame mechanism; it is the module's only stack
   mechanism (U-1420). The class's interface (`CodecFrame fr(ctx, frame_bytes); fr.ebp(); fr.loc<T>(off)`) is what the bodies use.
b. S4.2b's sound_replay.py: an option that sets a callee's negative-control mode while a caller's key runs (U-1425), and the per-vector x87
   state of S4.4 for U-1421 if the codec's callers turn out to run at 0x0f7f.

## 4. UNCERTAINTIES.md

Append the rows of `log/roadmap/S4/3_uncertain.tsv` (U-1420..U-1425).

## 5. CLAUDE.md paragraph (after the S4.7 paragraph)

```
Step S4.3, the LPC-10 speech codec (2026-10-24, module audio_codec, notes log/roadmap/S4/3_notes.md, U-1420..U-1425): the 42 rows of sound.dll
0x3a900..0x41a00 (debug /Od /GZ C in f2c shape) ported against S4.2's audio core. The bodies are the C3 hook sources carried over by
log/roadmap/S4/3_xlat.py into a virtual-pointer layer (P<T> / R<T> in the head of audio_codec.cpp: element loads and stores in the context's
address space, so the hooks' expressions and their float spills keep their order) and edited by hand (log/roadmap/S4/3_parts/body.cpp, assembled
by 3_assemble.py). Locals the original passes by address live in the original's /GZ frame at its ebp offsets through ONE helper, CodecFrame
(S4.8's SndLocals underneath, 0xcc-filled); the CRT leaves sqrt / floor / log / exp / __ftol are inline. Judge: log/roadmap/S4/3_run.py
(golden port/test/golden_audio_codec.json): 57 keys, 628 vectors, 57 GREEN in three layouts with 0 fallbacks, census tiers EXACT 53 / EQUAL_PI 4;
default replay 57/57 on x64 and on Win32 (every key at word 0x027f); mode 1 of all 42 functions RED; mode-2 spill controls (one `fstp dword`
skipped) RED in codecHighPassFilter, lpcSynthesize, codecRemoveMean. Coverage of the original: 8,402 of 8,422 instructions, 357 of 385 jcc both ways.
```
