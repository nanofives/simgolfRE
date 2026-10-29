# S4.6 merge request (channels and voices, module audio_chan)

Everything below is proven in the mirror build `port/build/S4_6_mirror` (log/roadmap/S4/6_mirror.py: HEAD of port/ plus S4.2's
files as they are on disk plus this step's files plus exactly these edits). The module depends on S4.2's merge (audio.h,
audio_core.cpp, soundstate.h, the C ABI patch): apply log/roadmap/S4/2_merge.md first.

## 1. port/src/sim/modules.inc

Append one line after `SG_MODULE(audio_core)`:

```
SG_MODULE(audio_chan)
```

(touch port/src/registry.cpp afterwards: MSBuild does not rebuild it when modules.inc changes.) Without it
`port/test/test_static.py::test_every_module_file_is_listed` fails on `audio_chan`. port/CMakeLists.txt needs no change (it globs
src/sim/*.cpp).

## 2. Files outside files_owned of other steps

None edited. S4.2's port/test/sound_replay.py, audio.h, audio_core.cpp and re/tools/snd_hook2port.py are used as they are; the step's own
runner `log/roadmap/S4/6_run.py` wraps sound_replay (its golden file, its mirror build, the derived keys of the emu-only rows, the
x87 control word of each vector, the sNaN quirk filter of U-1451, the Voice::update fixture file `log/roadmap/S4/6_fixtures.js`).

## 3. Needed change in S4.2's audio.h (a request, not applied)

A stack frame for the locals the original passes by address (U-1450). audio_chan.cpp's `frame_cell(ctx, n)` uses 16-byte cells in the
padding after sound.dll's last section; S4.10's production design should replace it by an audio-core API, e.g.

```cpp
// A zero-initialised scratch cell of `bytes` bytes (<= 64) in the context's address space, valid until the next call into the module,
// numbered like a stack slot; the same address in every context that attached at the same image base.
uint32_t snd_frame(Ctx &ctx, int cell, uint32_t bytes = 16);
```

and the callers (Voice_startPlayback, serviceChannelPlayback, Voice::update) switched to it.

## 4. UNCERTAINTIES.md

Append the rows of `log/roadmap/S4/6_uncertain.tsv` (U-1450..U-1455).

## 5. New files of this step outside port/ and log/

- `port/test/golden_audio_chan.json`: golden digests pinned from the emulated original (and from the port for the vectors the sNaN
  quirk of U-1451 forgives), the file the default tests read. Commit it with the step.
- `port/corpus/audio/*.json.z` (local, gitignored): the replay bundles. Regenerate with
  `SG_EMU_ORACLE=1 ~/bin/py -3.12 log/roadmap/S4/6_run.py oracle --update`.

## 6. Gate command lines and expected numbers (for S4/MERGE.md)

See log/roadmap/S4/6_notes.md "Reproduce" (numbers are in that file's "Gate numbers").

## 7. CLAUDE.md paragraph (after the S4.2 paragraph)

See the end of log/roadmap/S4/6_notes.md (it carries the final numbers).

## 8. Notes for the integrator

- No function name is registered by two S4 modules (checked 2026-10-24 over audio_core / audio_dev / audio_ds / audio_mixer / audio_chan tables).
- S4.4 carries its own x87 helper (`port/src/sim/audio_x80.inc`); this module's X80 (head of audio_chan.cpp, `x_*` over `xa_*` asm on Win32 / doubles on
  x64, plus the `PlainDoubles` control switch) is independent and sits in an anonymous namespace: both link. They can be unified after the merge; the
  oracles of both must stay GREEN (S4.6: `6_run.py replay --w32`, 168 keys).
- The sound.dll fixture file `log/roadmap/S4/6_fixtures.js` (Voice::update) is injected by `6_run.py`; if a C3-style round later adopts Voice::update,
  move it to re/frida/js/fixtures.d/ with a registry fragment (the keys Voice_update_a..h, z in 6_run.py SYNTH).
- `port/test/test_audio_chan.py` imports `log/roadmap/S4/6_run.py` by path: keep that file (or move it next to sound_replay.py and fix the one
  `spec_from_file_location` line in the test).
