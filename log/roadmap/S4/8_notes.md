# S4.8 notes: DirectSound and devices (modules audio_ds and audio_dev)

Status: DONE (2026-10-24). Merge request: log/roadmap/S4/8_merge.md. Uncertainties U-1470..U-1479: log/roadmap/S4/8_uncertain.tsv.
Files: log/roadmap/S4/8_files.txt.

## What was built

| file | what |
|---|---|
| `port/src/sim/audio_ds.cpp` | module audio_ds: the 54 ds rows (52 hook-sourced + return0Pop4, DirectSoundDevice::setMasterVolume transcribed from their instructions), `snd_ds_error` (mapDsError's table), the soft 80-bit x87 model of DsBuffer::setVolume |
| `port/src/sim/audio_dev.cpp` | module audio_dev: the 62 dev rows (57 hook-sourced + dynInit_soundWindowObj100b49b8 from c3fza + get_sound_version, stubReturn2fA/B, delete_sound from their instructions), `SndLocals`, `snd_fpcw`, `audio_dev_release`, configuration entries `audio_dev.fpcw` / `audio_dev.esp` |
| `port/include/portsim/sim/audio_dev.h` | SndLocals (locals passed by address at the original's offsets below the entry esp; frame block 0xFEB00000 + 64 KB without one), snd_set_entry_esp, snd_fpcw / snd_set_fpcw, audio_dev_release, snd_ds_error |
| `port/test/test_audio_ds.py`, `test_audio_dev.py` | default: per-key replay vs goldens x64, all keys Win32, negctl RED per function vs goldens, sink-log check; ds also the frame-block fallback; SG_EMU_ORACLE=1: per-key live oracle, ds the exhaustive setVolume grid |
| `port/test/golden_audio_ds.json`, `golden_audio_dev.json` | emulator digests (174 keys, 2,531 vectors), regions, tiers, fallback counts, the 0x0f7f key's control word, the original's decoded import stand-in logs (36 keys) |
| `log/roadmap/S4/8_replay.py` | the replay front end over S4.2's sound_replay (no edit to it): per-module goldens, the S4.8 mirror, DERIVED / CUSTOM / EXCLUDED keys, StateProxy (fpcw + entry esp per call), the emulator stack top mapped into the port, the frame block, `sinklog` |
| `log/roadmap/S4/8_fixtures.js` | fixtures for the emu-only rows with no twin (setMasterVolume, delete_sound) and postMidiNotification's engine-set replacement keys |
| `log/roadmap/S4/8_mirror.py` | copy of 2_mirror.py: MIR port/build/S4_8_mirror, modules audio_ds / audio_dev, sg_state_free's audio_dev_release |
| `log/roadmap/S4/8_cc.py`, `8_dis.py` | one-file compile check (cl /W4 x64); disassembler over the user's sound.dll with thunk / IAT / hooks.csv names |
| `log/roadmap/S4/8_conv/` | the converter's raw output (snd_hook2port.py --rva ... --module audio_ds / audio_dev) the modules started from |

## Gate numbers (2026-10-24)

- ORACLE (decision G1; `8_replay.py oracle`, logs 8_oracle_ds.log / 8_oracle_dev.log, TSVs 8_oracle_{ds,dev}.tsv): ds 70 keys
  1,215 vectors, dev 104 keys 1,316 vectors: port == emulated original on the return text and every region byte in layout A
  and in B and C, 0 errors. Tiers of the emulated original against the CSVs (S4.1 compare): ds EXACT 6 / EQUAL_PI 61 /
  EMU_ONLY 3 (derived and custom keys), dev EXACT 8 / EQUAL_PI 86 / EMU_ONLY 10. Fallbacks: ds 0; dev 222 calls over three
  layouts (74 per layout, 8_fallbacks.txt): SoundTimer::registerClient 24 and ::start 24 (registerTimerClient keys),
  SoundWindowObj_dtor 12 (atexitDtor_100b49b8), SoundWindowObj_ctor 2 (dynInit_soundWindowObj100b49b8): S4.7's rows; the
  static CRT's _atexit (rva 0x42647) 12 (the six dynamic initializers): no step owns it (U-1477).
- Keys: every one of the 116 rows has a key. 162 registry keys of the step's rvas minus postMidiNotification (FIXTURE_ERR,
  waits for the live engine) plus 4 DERIVED keys (return0Pop4@, stubReturn2fB@ on setField6cReturn0; stubReturn2fA@,
  get_sound_version@ on release_sound_null) plus 9 CUSTOM keys (setMasterVolume@ / @empty, postMidiNotification@eng /
  @eng0 / @noeng / @noeng1, delete_sound@ready / @notimer / @none).
- Negative controls (`8_replay.py negctl`, live oracle, 8_negctl_{ds,dev}.tsv): mode 1 of all 116 functions RED (ds 54, dev
  62), 0 blind; again against the goldens in the default pytest. Earlier modes that read GREEN were replaced (ds: DsBuffer::ctor
  +0x68, an equivalent mutant because initDesc clears it too; DsBuffer::create / query3D modes on paths no vector reaches;
  setVolume's first-product precision). DsBuffer::setVolume mode 2 (every step at 0x027f) RED on 6 of 30 vectors (the 0x0f7f
  key: the control word matters); mode 3 (the first product alone at 0x027f) is blind on the 30 vectors and RED on the
  exhaustive grid.
- setVolume exhaustive (`test_oracle_setvolume_exhaustive`, SG_EMU_ORACLE=1, 8_pytest_setvolume_grid.log): the ORIGINAL in
  Unicorn without the fixture VM vs the port, control words 0x027f 0x0f7f 0x037f 0x0e7f x 134 master levels x 130 volumes =
  69,680 calls equal (eax and +0xc4). Its first run found a port bug: at 64-bit precision a round-up computed `keep >> 64`
  (undefined; nonzero on x64), halving the mantissa: 122 calls at 0x037f differed; fixed (carry test by wrap-around).
- DEFAULT replay from bundles against the goldens: ds 70 / 70 keys PASS, dev 104 / 104 (fallbacks 74 = the recorded ones),
  x64 and Win32 (8_replay_{ds,dev}_{x64,w32}.log).
- Sink log (`8_replay.py sinklog`, 8_sinklog_{ds,dev}.log): the port's import calls (targets equal to an IAT slot's value:
  with no host, the AudioSink import commands DLL!name + slots) against the emulated original's import stand-in log (decoded
  by the oracle into the goldens): ds 5 keys with a log, all EQUAL, 97 import commands (timeGetTime 69, CoCreateInstance 8,
  CoInitialize 8, CoUninitialize 12); dev 31 keys, all EQUAL, 359 commands (KERNEL32 Enter/LeaveCriticalSection 22 + 22,
  GetCurrentThreadId 5, OutputDebugStringA 32, ResetEvent 12, SetEvent 8, SetThreadPriority 6, Sleep 40; USER32 CallWindowProcA
  40, DefWindowProcA 36, MsgWaitForMultipleObjects 9, PeekMessageA 41, PostMessageA 11, PostThreadMessageA 61, SetWindowLongA
  14), plus 85 log records written by original code the host ran (fallback callees), attributed apart. Controls
  (8_sinklog_controls.log): startSignalThread mode 1 (priority 1) and stopSoundThreadFull mode 1 (no Sleep(0)) DIFF;
  postSoundThreadMessage mode 1 (a return code only) EQUAL, as expected for a check of import commands.
- pytest (8_pytest_ds.log, 8_pytest_dev.log): test_audio_ds 130 passed, 71 skipped (oracle); test_audio_dev 170 passed, 104
  skipped.
- Segment gate on the mirror (8_segments_mirror*.tsv / .log): x64 205 / 205 PASS, 0 differ from log/p5/segs_x64_loc.tsv; Win32
  205 / 205, 0 differ from log/p5/segs_win32_loc.tsv and from the x64 run.
- Boot gate on the mirror: 62 PASS / 2 KNOWN / 1 NO_FRAMES / 2 FAIL, equal to S4.6's mirror run row for row ("Boot gate" below).
- test_static (port/test/test_static.py): the address test passes on every S4.8 file (it fails on a sibling's video.cpp);
  test_every_module_file_is_listed lists audio_ds / audio_dev until the modules.inc lines are merged.

## How the port meets the original (design as built)

- Calls: every non-inline call goes through audio.h's dispatcher by thunk rva (`th`), body rva, virtual slot (`com`: stdcall
  with the interface first for COM; `vt`: thiscall), or IAT slot (`imp` / snd_import). DirectSoundEnumerateA is called
  through the image's `jmp [iat]` stub at rva 0x3a978, as the original does (c3fk claims it; U-1472).
- Locals passed by address (U-1470): SndLocals reserves the original's frame (`sub esp, n`, pushes) below the entry esp and
  the body uses the original's offsets: DsBuffer::setPosition (play entry-4, write entry-8), lockEntire (p1 -4, b1 -8, p2
  -0xc, b2 -0x10), getPlayPosition (play -0xc, write -8, zeroed), lockCaptureRegion (play -4, write -8, zeroed), isBusy
  (status -4, starts as `this` from `push ecx`), DirectSoundDevice::create (mono WAVEFORMATEX -0x28, stereo -0x14),
  pumpWaveMessages and soundThreadPump (MSG -0x1c). Bytes the original leaves uninitialised stay as they are on the stack.
  The first version used a zero-filled frame block for all of them: getPlayPosition read RED on 14 of 15 vectors because
  c3fl's stub logs the out-pointer values (the emulator stack addresses 0x7f0fdbe8 / 0x7f0fdbec).
- The replay harness (8_replay.py) gives the entry esp the original's arm entered with (Machine.native_call at depth 0:
  `((STACK_INIT - 0x400) & ~0xF) - 4 * stack slots - 4`), maps the top 32 KB of the emulator stack into the port, and runs
  the port with the emulator at depth + 1 and esp 0x400 below the entry, so the host calls the port makes build their
  frames below the port's locals (at depth 0 they would overwrite them).
- x87 (DsBuffer::setVolume, U-1471): the caller's control word comes from the emulator's FPCW (| 0x40, U-1479) and is
  recorded in the golden entry; the body runs on a soft 80-bit value (x_mul / x_ftol / x_ln_fyl2x in audio_ds.cpp), the
  same code on x64 and Win32, so the Win32 build does not depend on the CPU's x87.
- delete_sound (rva 0x97d0) has no hook source: transcribed from its instructions (comment in audio_dev.cpp); its fixture
  stands in the callees other steps own with recording stubs and gives CommandQueue::push (this step's) a heap stand-in.

## Reproduce

```
~/bin/py -3.12 log/roadmap/S4/8_mirror.py --w32
~/bin/py -3.12 log/roadmap/S4/8_replay.py oracle --family ds  [--update] --out log/roadmap/S4/8_oracle_ds.tsv
~/bin/py -3.12 log/roadmap/S4/8_replay.py oracle --family dev [--update] --out log/roadmap/S4/8_oracle_dev.tsv
~/bin/py -3.12 log/roadmap/S4/8_replay.py negctl --family ds|dev --out log/roadmap/S4/8_negctl_<f>.tsv
~/bin/py -3.12 log/roadmap/S4/8_replay.py replay --family ds|dev [--w32]
~/bin/py -3.12 log/roadmap/S4/8_replay.py sinklog --family ds|dev
~/bin/py -3.12 -m pytest port/test/test_audio_ds.py ; ~/bin/py -3.12 -m pytest port/test/test_audio_dev.py
SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_ds.py -k setvolume_exhaustive
~/bin/py -3.12 -m port.test.integrator segments --dll port/build/S4_8_mirror/x64/Release/portsim_c.dll --jobs 3 --out <tsv>
~/bin/py -3.12 -m port.test.integrator segments --backend win32 --dll32 port/build/S4_8_mirror/w32/Release/portsim_c.dll \
    --runner32 port/build/S4_8_mirror/w32/Release/portsim_runner32.exe --jobs 3 --out <tsv>
SG_PORTSIM_DLL=port/build/S4_8_mirror/x64/Release/portsim_c.dll ~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out <tsv>
```

## For the next S4 workers

- SndLocals: if a function passes a stack local's address, reserve the original's frame and use its offsets; check the
  fixture's stubs (a logged pointer makes the address compared state).
- The converter (re/tools/snd_hook2port.py) failed outright on c3fza.cpp (no definition found for C3fzaDynInit_re), on
  pumpWaveMessages / soundThreadPump / postSoundThreadMessage (KeyError ';') and DsBuffer::queryNotify / restore (unbalanced
  parenthesis), and its output needed hand edits for: dereferences of out-pointer variables (`*out = v`), global references
  used as lvalues (`c3fk_gref(...) = v`), addresses of locals passed to calls (`&l1`), member-function-pointer casts through
  `vtbl[k]` (`DsIfaceCall2_t`, `ComGetCaps_t`, `SlotVoid0_t`), `++ld<T>(...)`, a parameter named `ctx` (enumerate), the c3fh
  X80 helpers and the c3fu negctl macros (C3FU_NULL, NEG(n, a, b)).
- A registry key whose fixture waits for the live game (c3dh_waitengine) needs a custom key with the state set by the fixture.
- Emu-only rows: a twin key of the same slots (DERIVED) when the body is a constant; else a fixture of the step (CUSTOM,
  evaluated into the key's VM context by KeyRun8.setup).

## Boot gate

`SG_PORTSIM_DLL=port/build/S4_8_mirror/x64/Release/portsim_c.dll ~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out
log/roadmap/S4/8_boot_gate_mirror.tsv` (log 8_boot_gate_mirror.log): 67 scenarios, 62 PASS / 2 KNOWN (cov_a_golfer, cov_b_holes18:
U-1062) / 1 NO_FRAMES (cov_b_location) / 2 FAIL (cov_r_bio: sg_run_boot -11; cov_r_flic_input: g_cachedCameraTileX at record 1).
Row for row (status, records, detail) equal to S4.6's mirror boot gate (log/roadmap/S4/6_boot_gate_mirror.tsv) on the 66
scenarios both have; the 67th, cov_r_s8_play (added by a sibling since), PASS 216 / 216. The two FAILs are in S4.6's run too:
not caused by this step (no exe code calls the sound.dll port).

## Open

- U-1472 (import stubs inside the image), U-1477 (S4.7's four callees and _atexit as fallbacks), U-1473 (postSoundThreadMessage's
  endless retry with a failing sink), U-1470 (entry esp passing for nested port calls: S4.10 / audio.h).
