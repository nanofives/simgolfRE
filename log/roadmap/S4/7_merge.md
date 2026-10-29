# S4.7 merge request (the sound objects, module audio_sound)

Everything below is proven in the mirror build `port/build/S4_7_mirror` (log/roadmap/S4/7_mirror.py: HEAD of port/ plus every accepted S4 module as it is
on disk (audio_core, audio_chan, audio_mixer + audio_x80.inc, audio_seq, audio_ds, audio_dev) plus this step's files plus exactly these edits). The module
depends on S4.2's merge (audio.h, audio_core.cpp, soundstate.h, the C ABI patch): apply log/roadmap/S4/2_merge.md first. It is independent of S4.9.

## 1. port/src/sim/modules.inc

Append one line after the last audio module line (order does not matter):

```
SG_MODULE(audio_sound)
```

(touch port/src/registry.cpp afterwards: MSBuild does not rebuild it when modules.inc changes.) Without it
`port/test/test_static.py::test_every_module_file_is_listed` fails on `audio_sound`. port/CMakeLists.txt needs no change (it globs src/sim/*.cpp).

## 2. Files outside files_owned of other steps

None edited. S4.2's port/test/sound_replay.py, audio.h, audio_core.cpp and re/tools/snd_hook2port.py are used as they are; re/tools/fixvm/syslibs.js
(S4.1) too: the clock VM of U-1462 is a copy made at run time by 7_run.py. The step's own runner `log/roadmap/S4/7_run.py` (a copy of S4.5's 5_run.py)
wraps sound_replay: own golden file, own mirror build, the x87 control word of each vector, OWN_STEPS = S4.2 / S4.4 / S4.5 / S4.6 / S4.7 / S4.8,
the clock VM for getPlaybackTime.

## 3. Proposals (not applied, not needed by this step's gate)

a. audio_core.cpp `audio_crt_name` + FuncDefs for the static CRT calls this module makes through the development fallback (U-1463): strstr (rva 0x42690,
   used by create_sound), strrchr (0x424b0, Sound_setName), sprintf (0x42577, SoundTimer::onTick), the eh vector helpers 0x42902 / 0x4280e
   (Sound5b6a8_ctor / dtor). S4.2 owns the file; S4.10 needs them to reach 0 fallbacks.
b. Channel_set3DBufferStub (rva 0x37c00, thunk 0x2c27, `mov eax, 0xb`; held in re/c3_hold.tsv): MultiSound::set3DBuffer calls it 35 times in its keys; it needs a
   port function or a claim for S4.10's zero-fallback gate.
c. re/tools/fixvm/syslibs.js (S4.1): add `timeGetTime: [0, () => <constant>]` to the winmm table (U-1462) so that `CLOCK_KEYS` in 7_run.py can go.
d. S4.2b / S4.10: replace `fcell` (head of audio_sound.cpp) by the core frame API (U-1460). It is the only stack mechanism of the module.

## 4. UNCERTAINTIES.md

Append the rows of `log/roadmap/S4/7_uncertain.tsv` (U-1460..U-1465).

## 5. New files of this step outside port/ and log/

- `port/test/golden_audio_sound.json`: golden digests pinned from the emulated original (117 keys, 2,146 vectors), the file the default tests read. Commit it
  with the step.
- `port/corpus/audio/*.json.z` (local, gitignored): the replay bundles. Regenerate with `SG_EMU_ORACLE=1 ~/bin/py -3.12 log/roadmap/S4/7_run.py oracle --update`.

## 6. Gate command lines and expected numbers (for S4/MERGE.md)

See log/roadmap/S4/7_notes.md "Reproduce" (numbers in "Gate numbers").

## 7. CLAUDE.md paragraph (after the S4.5 paragraph)

See the end of log/roadmap/S4/7_notes.md.

## 8. Notes for the integrator

- No function name is registered by two S4 modules (checked 2026-10-24: the 66 names of audio_sound against every FuncDef table in port/src/sim: 0 in common).
- After this merge the callers' keys improve: S4.4 Mixer_ctor GREEN_PENDING -> GREEN, S4.8's and S4.5's fallbacks to S4.7 vanish (7_notes.md table). Their goldens
  do not change (the host call and the port function return the same values by their own gates).
- `kAudioSoundX87Functions` (audio_sound.h) names Sound5b6a8_ctor: a caller that runs it under a control word other than 0x027f must use the Win32 backend.
- The oracle's CLOCK_VALUE (0x1234) is only the VM's timeGetTime answer for getPlaybackTime's keys; the golden digests of that key were pinned with it.
