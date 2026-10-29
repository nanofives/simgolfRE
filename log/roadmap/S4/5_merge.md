# S4.5 merge request (sequencer and MIDI, module audio_seq)

Everything below is proven in the mirror build `port/build/S4_5_mirror` (log/roadmap/S4/5_mirror.py: HEAD of port/ plus S4.2's files
and S4.6's audio_chan as they are on disk plus this step's files plus exactly these edits). The module depends on S4.2's merge
(audio.h, audio_core.cpp, soundstate.h, the C ABI patch): apply log/roadmap/S4/2_merge.md first. It calls S4.6's functions
(Channel_allocVoiceSlot, Voice_setLevel, ...) through the dispatcher, so S4.6's merge (6_merge.md) should land with or before it
(without it those calls fall back to the host, which the oracle counts but the default tests do not need).

## 1. port/src/sim/modules.inc

Append one line after `SG_MODULE(audio_chan)` (or after `SG_MODULE(audio_core)` when audio_chan is not merged yet):

```
SG_MODULE(audio_seq)
```

(touch port/src/registry.cpp afterwards: MSBuild does not rebuild it when modules.inc changes.) Without it
`port/test/test_static.py::test_every_module_file_is_listed` fails on `audio_seq`. port/CMakeLists.txt needs no change (it globs
src/sim/*.cpp).

## 2. Files outside files_owned of other steps

None edited. S4.2's port/test/sound_replay.py, audio.h, audio_core.cpp and re/tools/snd_hook2port.py are used as they are; the step's
own runner `log/roadmap/S4/5_run.py` (a copy of S4.6's 6_run.py) wraps sound_replay: own golden file, own mirror build, the x87
control word of each vector, the quiet-bit rule of U-1451 (no vector needed it here). `log/roadmap/S4/5_convert.py` wraps S4.2's converter
so that a function it cannot parse becomes a TODO block instead of ending the run (its `collect()` raises SystemExit, which the per-row
`except Exception` of main() does not catch; 8 of the 80 rows hit that).

## 3. Request on S4.2's audio.h (not applied)

Same request as 6_merge.md section 3: a frame API for the locals the original passes by address. audio_seq.cpp's `fcell(ctx, n)` (24 cells
of 16 bytes in the 0x1c0 bytes of image padding below audio_chan's frame_cell band) should become `snd_frame(ctx, cell, bytes)` of the
core; S4.10 decides (U-1440).

## 4. UNCERTAINTIES.md

Append the rows of `log/roadmap/S4/5_uncertain.tsv` (U-1440..U-1446).

## 5. New files of this step outside port/ and log/

- `port/test/golden_audio_seq.json`: golden digests pinned from the emulated original (113 keys, 1,840 vectors), the file the default
  tests read. Commit it with the step.
- `port/corpus/audio/*.json.z` (local, gitignored): the replay bundles. Regenerate with
  `SG_EMU_ORACLE=1 ~/bin/py -3.12 log/roadmap/S4/5_run.py oracle --update`.

## 6. Gate command lines and expected numbers (for S4/MERGE.md)

See log/roadmap/S4/5_notes.md "Reproduce" (numbers in "Gate numbers").

## 7. CLAUDE.md paragraph (after the S4.6 paragraph)

See the end of log/roadmap/S4/5_notes.md.

## 8. Notes for the integrator

- No function name is registered by two S4 modules (checked 2026-10-24: the 80 names of audio_seq against the FuncDef tables of audio_core / audio_chan / audio_dev / audio_ds / audio_mixer on disk: 0 in common).
- Seq_setTempo carries FuncDef.fpcw 0x027f and the same X80 helper as audio_chan (head of audio_seq.cpp); S4.4's own helper is
  `port/src/sim/audio_x80.inc`. Unify when S4.4 merges (not compared here).
- `kAudioSeqX87Functions` (audio_seq.h) names Seq_setTempo: a caller that runs it under a control word other than 0x027f must use the Win32 backend.
