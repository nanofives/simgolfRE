# S4.7 notes: the sound objects (module audio_sound)

Status: DONE (2026-10-24), all 66 rows of S4.7 ported and judged. Merge request: log/roadmap/S4/7_merge.md. Uncertainties U-1460..U-1465:
log/roadmap/S4/7_uncertain.tsv. Files: log/roadmap/S4/7_files.txt. Builds on S4.2 (log/roadmap/S4/2_notes.md, re/analysis/data/sound_replay.md)
and follows S4.5 (log/roadmap/S4/5_notes.md): S4.2's files (audio.h, audio_core.cpp, sound_replay.py, snd_hook2port.py) are used as they are.

## What was built

| file | what |
|---|---|
| `port/src/sim/audio_sound.cpp` | module `audio_sound`: the 66 functions of the S4.7 rows (Sound, Sound5b6a8, Sound5c450, MultiSound, SoundGroup, SoundWindowObj, SoundTimer, registerSound / unregisterSound, create_sound, Engine_playSound, getPlaybackTime, Sound_* helpers). ASSEMBLED ONCE by `log/roadmap/S4/7_assemble.py` from the converter's output, then EDITED IN PLACE by the scripts `7_fix1.py` .. `7_fix5.py` (host-pointer idioms, stack buffers, vtable calls, the five blocks the converter refused). The .cpp is the source from here on; re-running the assembler would overwrite the edits. |
| `port/include/portsim/sim/audio_sound.h` | the module's header (no API: `kAudioSoundX87Functions` = Sound5b6a8_ctor) |
| `port/test/test_audio_sound.py` | 373 tests (189 pass by default, 184 need SG_EMU_ORACLE=1) |
| `port/test/golden_audio_sound.json` | golden digests: 117 keys, 2,146 vectors, tier, regions, x87 control words |
| `log/roadmap/S4/7_run.py` | the runner (a copy of S4.5's 5_run.py: own mirror build and golden file; OWN_STEPS covers S4.2/4/5/6/7/8; the clock VM for getPlaybackTime, U-1462) |
| `log/roadmap/S4/7_mirror.py` | the private build `port/build/S4_7_mirror`: HEAD + every accepted S4 module (audio_core, audio_chan, audio_mixer + audio_x80.inc, audio_seq, audio_ds, audio_dev) + audio_sound, with the S4.4 / S4.8 releases in sg_state_free and S4.4's `__ftol` row in audio_crt_name; x64 and Win32 + runner |
| `log/roadmap/S4/7_convert.py`, `7_conv/audio_sound.cpp`, `7_assemble.py`, `s7_patches.py`, `7_parts/head.cpp`, `7_parts/hand1.cpp` (empty), `7_blk.py`, `7_fix1..5.py` | converter wrapper and its output (62 of 66 blocks converted, 5 refused: SoundTimer::start, SoundWindowObj_ctor, SoundWindowObj_subclass, MultiSound_setSlotSource, MultiSound_setPitchCents), the head (helpers, X80, fcell), and the edit scripts |
| `log/roadmap/S4/7_cc.py`, `7_cover.py`, `7_fbcount.py`, `7_cycle.sh` | compile check with errors per block, original-instruction coverage per function, the fallback re-count of other steps' keys on this build |

## Method

1. `7_convert.py --step S4.7 --module audio_sound --compile` converted the hook sources (c3ar, c3au, c3cl, c3cx, c3dh, c3dj, c3do, c3dt, c3ew, c3ex, c3fd,
   c3fg, c3fh, c3fk, c3fl, c3fq): 61 blocks, 5 refused (unbalanced macro arguments, a missing definition). First compile: 101+
   errors (host-pointer dereferences `*p`, `void**` locals, `char buf[]` stack buffers, function-pointer typedefs, `strcmp` / `memset` on virtual addresses).
2. Edits: memory through ld / st, `strcmp` inlined as `str_diff` (the originals inline the compare), `memset` as store loops, vtable calls as
   `snd_vcall`, imports as `snd_import`, CRT calls (strstr 0x42690, strrchr 0x424b0) through the dispatcher, the five refused functions written from
   the hook source and the disassembly addresses its comments cite. Sound5b6a8_ctor's x87 seed uses S4.5's X80 helpers (head), FuncDef.fpcw 0x027f,
   mode 2 = plain doubles.
3. Port rules kept: no image-range literal (test_static passes on audio_sound.cpp and audio_sound.h), every call outside the function through the
   dispatcher, module globals through snd_obj / snd_arr (array members need snd_arr: `Ptr<uint8_t[N]>` does not compile), wrapping arithmetic unsigned.
4. Locals the original passes by address: two 0x100-byte stack buffers (Sound_setName, SoundTimer::onTick's message) share the 16 cells of `fcell`
   0..15; nothing else needed a cell. fcell is the ONE helper (U-1460): S4.2b replaces it.

## Judge

- Oracle (decision G1): S4.1's fixture VM rebuilds each key, the original runs in Unicorn over the user's sound.dll, the port runs as the second arm
  on the same memory in layouts A / B / C. 117 registry keys of the 66 rows (every row has a C3 path-1 key; no derived key): 2,146 vectors.
- Default replay (no emulator, no Node): bundles (port/corpus/audio, local) + golden digests, x64 and Win32.
- Negative controls: mode 1 of every function; mode 2 of Sound5b6a8_ctor (BLIND, U-1461).

## Gate numbers (2026-10-24)

- ORACLE (`7_run.py oracle --update`, 7_oracle_r1.log / .tsv, then `--keys getPlaybackTime` 7_oracle_gpt.log; merged 7_oracle_final.tsv): 117 keys,
  2,146 vectors in three layouts, 117 GREEN (port equals the emulated original on the return text and every region byte, A 100%, B and C 100%), 0 RED,
  0 errors, 0 fallbacks to S4.2 / S4.4 / S4.5 / S4.6 / S4.7 / S4.8 functions. Tiers of the emulated original against the C3 CSVs (S4.1 compare):
  EXACT 19, EQUAL_PI 98. The first full run read 116 GREEN + getPlaybackTime RED (4 of 12 vectors stopped at the unmodelled WINMM!timeGetTime, S4.1's
  PARTIAL_ERR); U-1462 explains the clock VM that fixed it. No sNaN-quirk forgiveness needed.
- DEFAULT replay from bundles: x64 117 keys, 116 PASS and 1 SKIP (Sound5b6a8_ctor_wf7f: 3 vectors at word 0x0f7f, `SG_E_UNSUPPORTED` on x64), 0 FAIL
  (7_replay_x64.log); Win32 117 / 117 PASS with the 0x0f7f vectors run under their word (7_replay_w32.log).
- Negative controls (`7_run.py negctl --mode 1`, 7_negctl_r1.tsv): mode 1 of all 66 functions RED, 0 blind. x87 mode 2 (`7_run.py x87ctl`, 7_x87ctl.tsv):
  Sound5b6a8_ctor 3/3 at every key for the correct body AND the mutant: BLIND (U-1461).
- Coverage of the ORIGINAL by the keys (`7_cover.py`, 7_coverage.tsv): 3,354 of 3,388 instructions (99.0%), 288 of 333 conditional jumps both ways;
  59 of 66 functions fully executed (thin spans in U-1464).
- pytest: default `~/bin/py -3.12 -m pytest port/test/test_audio_sound.py` 189 passed, 184 skipped (117 + 66 oracle tests, 1 x64 key judged on Win32)
  in 170 s (7_pytest_default.log). SG_EMU_ORACLE=1: 372 passed, 1 skipped (the x64 key judged on Win32) in 640 s (7_pytest_oracle.log).
- Segment gate x64 on the mirror (`integrator segments --dll port/build/S4_7_mirror/x64/Release/portsim_c.dll --jobs 3`, 7_segments_mirror.tsv / .log):
  205 / 205 PASS in 144 s, `integrator compare` against S4.5's 5_segments_mirror.tsv: 0 digests differ. (Win32 segment gate not re-run: the sim never
  reaches the module; the Win32 build compiles it and its runner replays the 117 keys.)
- Boot gate on the mirror (`SG_PORTSIM_DLL=<mirror dll> port.test.boot --all --jobs 3`, 7_boot_gate_mirror.log): 67 scenarios, 62 PASS, 2 KNOWN, 1 NO_FRAMES,
  2 FAIL (cov_r_bio, cov_r_flic_input), identical to S4.5's boot gate and BASELINE: not this step's.
- test_static (7_test_static.log): `test_no_original_addresses_in_port_code` fails only on port/src/sim/audio_wave.cpp (S4.9's, two literals);
  `test_every_module_file_is_listed` fails on the unmerged audio modules until the modules.inc lines are applied. audio_sound.* are clean.

## Pending callees of accepted steps: fallback counts after this step lands

Re-run on this build (S4_7_mirror, layout A, no --update, 7_fb_*.log):

| step | key set | before | after |
|---|---|---|---|
| S4.4 audio_mixer | Mixer_ctor | GREEN_PENDING, 36 fallbacks to S4.7 | GREEN, 0 fallbacks (7_fb_s44_mixer_ctor.log) |
| S4.8 audio_dev | 104 dev keys | 222 fallbacks (S4.7's rows + CRT _atexit) | 104 / 104 GREEN, 36 left, all in the 12 dynInit_* keys (3 each: the static CRT's _atexit and the eh helpers it calls, no step owns them, S4.8 U-1477), 0 to S4.7 (7_fb_s48_dev.log) |
| S4.8 audio_ds | 70 ds keys | (not counted) | 70 / 70 GREEN, 0 fallbacks (7_fb_s48_ds.log) |
| S4.5 audio_seq | 113 keys, layout A | 472: S4.8 436, S4.7 25, S4.9 11 | 15: S4.9 11 (MidiReader_dtor), CRT 4 (operator delete x2, operator new x2); 0 to S4.7 and S4.8; every vector equal (Seq_setTempo_wf7f's x64 mismatch at word 0x0f7f is the known X87_WIN32, judged on Win32) (7_fb_s45.log; the TSV's RED column is the A-only layout rule of the runner) |

S4.7's own fallbacks (U-1463): 130 calls over its 117 keys, none to an accepted step.

## Findings (from the C3 notes and this step's transcription; nothing was changed)

See U-1465. Also: SoundTimer::onTick's late-tick branch writes a debug line through sprintf + OutputDebugStringA with a 0x100-byte stack buffer; the
port keeps it (cells 0..15) and its imports reach the host or the AudioSink.

## Reproduce

```
~/bin/py -3.12 log/roadmap/S4/7_convert.py --step S4.7 --module audio_sound --compile --out log/roadmap/S4/7_conv/audio_sound.cpp
~/bin/py -3.12 log/roadmap/S4/7_assemble.py          # ONCE: writes port/src/sim/audio_sound.cpp (then 7_fix1..5.py in order); the committed .cpp already has them
~/bin/py -3.12 log/roadmap/S4/7_cc.py port/src/sim/audio_sound.cpp errmap      # compile errors per block
~/bin/py -3.12 log/roadmap/S4/7_mirror.py --w32                                # x64 + Win32 mirror (port/build/S4_7_mirror)
SG_EMU_ORACLE=1 ~/bin/py -3.12 log/roadmap/S4/7_run.py oracle --update --out log/roadmap/S4/7_oracle_r1.tsv
SG_EMU_ORACLE=1 ~/bin/py -3.12 log/roadmap/S4/7_run.py negctl --mode 1 --out log/roadmap/S4/7_negctl_r1.tsv
~/bin/py -3.12 log/roadmap/S4/7_run.py replay                                  # x64 from bundles
~/bin/py -3.12 log/roadmap/S4/7_run.py replay --w32                            # Win32, x87 words applied
~/bin/py -3.12 log/roadmap/S4/7_run.py x87ctl --out log/roadmap/S4/7_x87ctl.tsv
SG_EMU_ORACLE=1 ~/bin/py -3.12 log/roadmap/S4/7_cover.py --out log/roadmap/S4/7_coverage.tsv
SG_EMU_ORACLE=1 ~/bin/py -3.12 log/roadmap/S4/7_fbcount.py 5                   # another step's keys on this build (SG_PORTSIM_DLL=<mirror> for 4 / 8)
~/bin/py -3.12 -m pytest port/test/test_audio_sound.py                         # default, 3 min
SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_sound.py         # + the live oracle and its controls
~/bin/py -3.12 -m port.test.integrator segments --dll port/build/S4_7_mirror/x64/Release/portsim_c.dll --jobs 3 --out <tsv>
```
After the merge the mirror is unnecessary: `7_run.py` reads `DLL64` (port/build/S4_7_mirror/x64): point MIR at the integrator's build or set SG_PORTSIM_DLL.

## For S4.2b / S4.10

- Stack locals are behind `fcell(ctx, n)` in the head of audio_sound.cpp. Two functions use it (Sound_setName, SoundTimer::onTick: cells 0..15 as one
  0x100-byte buffer each), every use is `fcell(ctx, 0)` plus ordinary `ld` / `st` / `snd_call` arguments, so a frame API is a one-line change.
- S4.2b had published no API when this step finished (log/roadmap/S4/2b_notes.md absent; 2b_wave1 holds only a staged audio_core.cpp / audio.h
  with no snd_frame).
- Runner feature needed from sound_replay.py: the clock VM (U-1462) and the OWN_STEPS list including S4.4 / S4.8.

## CLAUDE.md paragraph (after the S4.5 paragraph)

```
Step S4.7, the sound objects (2026-10-24, module audio_sound, notes log/roadmap/S4/7_notes.md, U-1460..U-1465): the 66 rows of S4.7 ported against S4.2's audio core
(portsim/sim/audio.h): Sound / Sound5b6a8 / Sound5c450 constructors and destructors, MultiSound and SoundGroup (start, stop, 3D position, rate glide, pitch cents, slot
source), SoundTimer (the multimedia-timer client list, start / stop / onTick), SoundWindowObj, the sound registry, create_sound, Engine_playSound, getPlaybackTime.
port/src/sim/audio_sound.cpp was assembled once from the converter's output (7_assemble.py) and edited in place (7_fix1..5.py). Judge: log/roadmap/S4/7_run.py over
sound_replay (own golden port/test/golden_audio_sound.json): 117 keys, 2,146 vectors, 117 GREEN in three layouts, tiers EXACT 19 / EQUAL_PI 98; mode 1 of all 66 functions
RED; Sound5b6a8_ctor (x87 tempo seed, FuncDef.fpcw 0x027f) runs the original's x87 sequence on Win32 and its mode-2 control is BLIND (constants only, U-1461).
getPlaybackTime's four clock vectors run on a copy of the fixture VM whose WINMM answers timeGetTime with a constant (U-1462). Locals the original passes by address (two
0x100-byte buffers) live in the cells of `fcell` (U-1460). With this step in the build Mixer_ctor (S4.4) is GREEN with 0 fallbacks (was 36), audio_dev's 104 keys have 36 left
(CRT _atexit only; was 222) and audio_seq's layout-A fallbacks fall from 472 to 15 (S4.9 11, CRT 4). Coverage of the original by the judging keys: 99.0% of instructions, 288 of
333 jcc both ways.
```
