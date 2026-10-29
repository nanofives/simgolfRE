# S4.2b notes: core consolidation of the sound.dll port

## THE CONSOLIDATED API (read this first; S4.3 / S4.5 / S4.7 / S4.9 / S4.9b / S4.10 / S4.11 write against it)

Source: `port/include/portsim/sim/audio.h` (S4.2b's version; in the tree once installed, until then
`log/roadmap/S4/2b_wave1/port/include/portsim/sim/audio.h`).

```cpp
// ---- names: every sound.dll function is registered as "snd.<hooks.csv name>" (lead rule, 2026-10-24); harness entries keep
//      their "audio_core.*" names. The dispatcher looks rva -> "snd." + kSoundFunctions[i].name.
// ---- the stack: one mechanism, no module frame blocks or cells
SndFrame f(ctx, n);          // the original's frame at the call: callee-saved pushes + `sub esp` + pushed locals
f.at(off)                    // the local at entry esp - off (the original's offset, read from the instructions)
f.arg(k)                     // the caller's k-th stack argument slot (entry esp + 4 + 4k), when the original reuses it
SndFrame f(ctx, 0); f.set_esp(((f.entry() - 4) & ~7u) - X);   // `push ebp; mov ebp, esp; and esp, -8; sub esp, ...` frames
// A port function the dispatcher calls enters at the esp a `call` leaves (caller esp - stack args - 4), so nested frames
// land where the original's do. Top-level esp: the harness ("audio_core.vector <esp> <limit>"); none: frame block 0xFEB00000.
// ---- the x87 unit: the core FPU model (S4.4's, moved to audio.h / audio_core.cpp)
x87::Unit &u = x87::unit(ctx);      // cw, TOP, C0..C3, tags, 8 x 80-bit registers; persists across calls like the thread FPU
u.push(x87::fld64(bits)); u.st(0) = x87::mul(u.st(0), x87::from_f64(m), u.cw); u.set_cc(x87::fcom(a, b)); u.pop();
// ops: add sub mul div from_f64 from_f32 fld64 fld32 fild fst64 fist fcom fabs_ fchs ln2 fyl2x to_double; Unit::fxch
snd_ftol(ctx);               // `call __ftol` with the value in u.st(0) (pops; "snd.audio_core.crt.ftol" is not used: CRT
                             //  entries stay "audio_core.crt.*")
SG_X87_GUARD(ctx, ...);      // X87Unmodelled -> the context's x87 detail, the call stops
// Compute at u.cw (the CALLER's word: a mixer leaves 0x0f7f set, U-1421), never at a constant word, and set u.set_cc where
// the original compares: the harness compares the x87 EXIT state (cw, TOP, C0..C3, registers in use) on every key.
// ---- calls: unchanged (snd_call / snd_vcall / snd_call_va / snd_import); an unclaimed `jmp [iat]` stub is the import.
```

Harness: `port/test/sound_replay.py --step S4.k [--family f]` (oracle / replay / negctl / sinklog / keys); per-step tables at
its top (STEPS golden, DERIVED, CUSTOM, EXCLUDED, ZERO_STACK, PATCHES, QUIRK_FUNCTIONS, NEGCTL_MODES, NEGCTL_EXTRA); fixtures
for custom keys in `port/test/sound_fixtures/*.js`. Every key runs with the entry esp the original entered with and the x87
unit loaded from the FPU the original found.

Status: see the end ("Status"). Merge request: log/roadmap/S4/2b_merge.md. Uncertainties U-1418, U-1419:
log/roadmap/S4/2b_uncertain.tsv. Files: log/roadmap/S4/2b_files.txt.

Phase 1 (S4.2 / S4.4 / S4.6 / S4.8 on the consolidated core) ran while wave 1 was queued, on copies under
`log/roadmap/S4/2b_wave1/port/` through `2b_dev.py`. The lead then lifted that rule: every file is now in the tree (2b_dev.py and
the copies are history), and phase 2 migrated S4.3, S4.5, S4.7, S4.9 and S4.9b, renamed every sound.dll function `snd.<name>`
and judged the whole resubmitted wave (section "Phase 2" below). Build: the mirror `port/build/S4_2b_mirror` (2b_mirror.py:
HEAD + the wave's files + the shared edits) for the sound_replay gates, the whole-tree mirror `port/build/S4_2b_full`
(2b_fulltree.py) for the port suite, segments and boot.

## What was done

### One stack mechanism (audio.h "the stack"; U-1418)

- `SndFrame f(ctx, n)` reserves the ORIGINAL's frame (callee-saved pushes, `sub esp`, pushed locals) below the function's ENTRY
  ESP (the address of its return address); `f.at(off)` = entry - off, `f.arg(k)` = the caller's k-th stack argument slot,
  `f.set_esp(esp)` for `and esp, -8` frames.
- Entry esp: a top-level call takes the esp the caller gives (`snd_set_entry_esp`, harness entries `audio_core.esp <esp> <limit>`
  and `audio_core.vector <esp> <limit>`); a port function the DISPATCHER calls enters at the esp a `call` leaves (the caller's
  esp minus its stack argument slots minus the return address; thiscall / fastcall register slots excluded). With no esp the
  core's frame block (0xFEB00000, 64 KB, entry = top - 0x44, zero-filled per frame) is the stack (production).
- The 14 bodies that pass a local's address now use the original's offsets, read from the instructions:
  S4.4 renderChannelSingle (frame 0x14; the cursor is the caller's count slot entry + 8: `mov [esp+0x1c], ecx` at 0x36318,
  `lea edx, [esp+0x24]` at 0x3632f), renderChannelStreamed (frame 0x3c; getPosition's want / off at entry - 0x24 / - 8
  (0x366b6..0x366c0), restore's p1 / n1 / p2 / n2 at - 0xc / - 0x10 / - 0x14 / - 0x18 (0x36704..0x3671d)), renderChannelVoice
  (`and esp, -8`: cursor at ((entry - 4) & ~7) - 0x18, 0x371ea / 0x37282); S4.6 Voice_startPlayback (`push ecx` slot entry - 4,
  0x20c40; frame 8 then 0x14 from 0x20c11), serviceChannelPlayback (`push ecx` slot entry - 4, 0x34497, frame 0x14), Voice::update
  (`and esp, -8`: cells at aligned - 0x18 / - 0x14 / - 8, 0x1f9c3 / 0x1f8dd / 0x1fb63); S4.8's eight SndLocals bodies unchanged
  in offsets (SndLocals -> SndFrame).
- Removed: S4.4's `x87::frame_push / frame_pop / kFrameVa` (0x6E000000) and `Frame`; S4.6's `frame_cell` (image tail padding);
  S4.8's `SndLocals`, `snd_set_entry_esp` (audio_dev.h), the pending-esp rule, `audio_dev.esp`, `audio_dev_release`.
- The oracle gives EVERY key the entry esp its original arm entered with and maps the top 32 KB of the emulator's stack into the
  port (S4.8's rule, now for all steps); host calls run with the emulator's esp 0x400 below the entry esp; a port frame below
  entry - 0x700 is refused (MissingResource) so it can never meet a host frame.

### The x87 unit as the core FPU model (audio.h "the x87 unit"; U-1419)

- `portsim::x87` (X80, the operations, `Unit`, `unit(ctx)`) moved from audio_mixer.h / .cpp to audio.h / audio_core.cpp part 0
  (audio_x80.inc is included there); the unit is per-context state that survives audio_attach and is dropped by audio_release
  (S4.4's `unit_release` merge line is no longer needed). New operations: `from_f32`, `fld32`, `ln2`, `fyl2x` (the reference
  emulator's model, S4.8's x_ln_fyl2x, on both backends), `to_double`, `Unit::fxch`; `SG_X87_GUARD` in audio.h.
- sound.dll's `__ftol` is the core's `audio_core.crt.ftol` (`snd_crt::ftol_`, `snd_ftol(ctx)`), called through the dispatcher by
  S4.4's renderers, S4.6's x87 bodies and S4.8's setVolume alike.
- Migrated onto the unit: S4.6's X80 (Win32 instruction groups under the thread's word / x64 doubles) -> value helpers over the
  unit at its word (x64 now judges the 0x0f7f keys: no more X87_WIN32 / SKIP), compares set the unit's condition codes; the
  S4.6 bodies that compared or computed in C doubles (Channel_releaseVoices, Channel_stopVoices, updateVoiceRate / the rate step
  of Channel_startVoice, Voice_setPanGains, Voice_noteOff, Channel_setRateGlide, Channel_setPanGains, Voice_setLevel; found by
  `log/roadmap/S4/2b_x87scan.py` -> 2b_x87scan.tsv) transcribed instruction by instruction; S4.8's DsBuffer::setVolume on the unit
  (fild / fmul / fmulp / __ftol / fldln2 / fxch / fyl2x in the original's order; its soft model and `snd_fpcw` / `audio_dev.fpcw`
  removed: the caller's word is the unit's); S4.2's nextRandomFloat leaves its float on the unit (fld dword; fsub dword).
- Voice_setLevel now calls Voice_setPanGains through its thunk 0x231a as the original does (0x383df), so a stand-in on the thunk
  serves it (S4.6 called the body rva).
- A host call sees the unit: when the page kSndFpuBox (0xFEAF0000) is mapped (the harness maps it in the port and the emulator)
  the core writes the unit's image there before every host call and loads it back after; sound_replay's host loads Unicorn's FPU
  from it and writes it back, so a host-run x87 callee computes on the port's FPU and the script host replays the effect.
- The harness compares the x87 EXIT STATE on every key of every step (S4.4's feature): the unit is loaded per vector with the FPU
  the original arm found and compared with the FPU it left (cw, TOP, C0..C3, registers in use and their bits; after the caller's
  fstp for float / double returns); the emulator's never-initialised tags are emptied at key start (U-1431).

### Import stubs (U-1472)

The dispatcher routes an unclaimed `FF 25 <slot>` stub inside the image (no port function at that rva) as the import of its
slot: with no host the AudioSink receives it by name (test_import_stub_reaches_the_sink); with a host, the slot's value.

### One replay front end (port/test/sound_replay.py; copy in 2b_wave1)

`--step S4.k [--family f]` for every step; no per-step wrapper. Folded in: golden per module (`STEPS`, `golden_path_of`);
DERIVED keys of all steps (S4.2 4, S4.4 10 incl. the 2 chained, S4.6 3, S4.8 4); CUSTOM keys with fixtures in
`port/test/sound_fixtures/` (S4.6's Voice::update 9 keys `s46_voice_update.js`, S4.8's 9 `s48_custom.js`, evaluated into each key's
VM context after setup); EXCLUDED (postMidiNotification); ZERO_STACK and PATCHES (S4.4); the entry esp and the x87 image per
vector (bundle format 2: `esp`, `x87`); x87 exit digests in the goldens; the U-1451 sNaN rule (QUIRK_FUNCTIONS, the four S4.6
functions; the golden pins the port's regions for a forgiven vector); GREEN_PENDING / PASS_PENDING (every fallback reaches
another step's row or the CRT no step owns); implog per vector in the goldens and `sinklog`; `--coverage` (instructions and
jcc sides of the emulated original per function: S4.4's and S4.6's coverage tools in one); negctl with NEGCTL_MODES (modes >= 2
per function) and NEGCTL_EXTRA (__ftol judged through S4.4's renderer keys); replay `--w32 [--soft]`, `--negctl fn:mode`.
6_run.py, 8_replay.py and test_audio_mixer.py's harness are no longer used by any test (test_audio_chan / ds / dev / mixer /
core import sound_replay only).

## Gate numbers, phase 1 (2026-10-24, S4.2 / S4.4 / S4.6 / S4.8; logs log/roadmap/S4/2b_*; phase 2 below)

Row's gate: no number lower than at acceptance. Every one equal or higher:

| | accepted | S4.2b (consolidated core) | log |
|---|---|---|---|
| S4.2 oracle | 68 keys, 1,219 vectors GREEN | 68 / 1,219 GREEN in A / B / C, now with the x87 exit state; 0 fallbacks | 2b_oracle_s42.log / .tsv |
| S4.4 oracle | 57 / 2,001, 56 GREEN + 1 GREEN_PENDING, x87 exit state | 57 / 2,001, 56 GREEN + 1 GREEN_PENDING (Mixer_ctor -> S4.7 Sound_setSourceType, 36 calls) | 2b_oracle_s44.log |
| S4.6 oracle | 168 / 3,344: 156 GREEN + 12 X87_WIN32 (judged on Win32 only) | 168 / 3,344: 156 GREEN + 12 GREEN_PENDING, 0 RED, x64 included (the 0x0f7f keys run on the unit), x87 exit state on every key | 2b_oracle_s46.log |
| S4.8 oracle | 70 + 104 keys, 1,215 + 1,316 vectors | ds 70 / 1,215 GREEN; dev 104 / 1,316: 87 GREEN + 17 GREEN_PENDING (222 calls: S4.7's SoundTimer / SoundWindowObj rows, the CRT's _atexit 0x42647) | 2b_oracle_s48ds.log, 2b_oracle_s48dev.log |
| goldens | | all five files equal to the accepted ones key for key (digests, tiers, regions, sNaN quirk entries): core 68, chan 168, ds 70, dev 104, mixer 57 | 2b_golden_compare.log |
| sNaN rule (U-1451) | 17 vectors in 7 keys | 17 vectors in the same 7 keys | 2b_oracle_s46.log |
| default replay x64 | | S4.2 68 / 68, S4.4 57 / 57, S4.6 168 / 168 (12 PASS_PENDING), S4.8 70 / 70 + 104 / 104 (17 PASS_PENDING) | 2b_replay_x64_*.log |
| default replay Win32 | S4.6: 168 PASS with 133 vectors at 0x0f7f (x64 skipped them) | S4.2 68, S4.4 57 (instructions) + 57 (`--soft`), S4.6 168, S4.8 70 + 104: all PASS | 2b_replay_w32_*.log, 2b_replay_x64w32*_S44.log |
| sink logs (S4.8) | 36 EQUAL (97 + 359 commands) | 5 + 31 = 36 EQUAL, 97 + 359 import commands, 85 records from host-run code | 2b_sinklog_ds.log / _dev.log |
| controls mode 1 | 60 + 39 + 101 + 116 | 61 (S4.2's 60 + __ftol, now the core's) + 38 (S4.4) + 101 + 116 = 316, all RED | 2b_negctl_s42_m1.tsv ... 2b_negctl_all.log |
| other controls | S4.4 long-double 8; S4.6 x87 mode 2 (Win32), Voice::update 2..8; setVolume 2 RED, 3 blind on 30 | S4.4 mode 2 8 / 8 RED; S4.6 mode 2 6 / 6 RED live on x64 (5 x87 functions + Voice::update); Voice::update modes 3..8 6 / 6 RED; setVolume mode 2 RED, mode 3 blind on the 30 vectors (as accepted; the grid judges it) | 2b_negctl_all.log |
| segments | 205 / 205 | x64 205 / 205 and Win32 205 / 205, 0 digests differ from log/roadmap/S0/1_segs_{x64,win32}.tsv nor between the backends | 2b_segments_*.tsv, 2b_segments_compare.log |

| pytest x64 (one file per process, default) | core 143 / 128 skipped; mixer 60; chan 264 / 286; ds 130 / 71; dev 170 / 104 | core 146 passed / 128 skipped (+3 new core tests: __ftol, the host box, the import stub); mixer 109 / 57 (the per-function controls against the goldens and the fyl2x probe added; the oracle per key skipped); chan 288 / 269 (x64 judges the 0x0f7f keys and the x87 controls); ds 131 / 71 (+1: the stack's entry esp / callee entry / limit); dev 170 / 104 | 2b_pytest_*_x64.log |
| pytest Win32 selections | | core 1, mixer 61 (57 keys + 4 probes), chan 7, ds 1, dev 1: all pass; chan's selection failed ONCE (1 failed + 1 error, only the summary kept; cause not established) and passed on two reruns | 2b_pytest_*_w32.log, 2b_pytest_test_audio_chan_w32_rerun*.log |
| setVolume grid | 69,680 calls equal | 69,680 calls equal (on the unit: the caller's word is the unit's), mode 3 RED on the grid | 2b_pytest_setvolume_grid.log |
| boot gate | S4.8 mirror: 67 scenarios 62 / 2 / 1 / 2 | 70 scenarios: 63 PASS / 2 KNOWN / 2 NO_FRAMES / 3 FAIL; the 67 shared rows equal S4.8's mirror run row for row (status, records, expected, detail); the 3 new scenarios (siblings' corpus) give the same rows on the baseline build without S4.2b (cov_a_quit PASS, cov_a_quit_title NO_FRAMES, cov_r_s2_screens FAIL at record 771 g_cursorTileX / Y) | 2b_boot_gate_mirror.tsv, 2b_boot_base_new3.log |
| test_static address rule | | passes on the tree (the S4.4 / S4.6 / S4.8 files) and on the wave-1 copies (same regex, scanned directly) | |

Integration seen by the oracle: with all four modules in one build, S4.6's keys reach S4.8's and S4.4's port functions
(CommandQueue::push, DsBuffer::*, Ds3DBuffer::*, Mixer_ctor) through the dispatcher instead of the development fallback; S4.6's
fallbacks dropped from 337 calls in layout A (accepted) to 156 per layout, all to S4.3 / S4.7 rows or the CRT's eh vector
iterators. The emulated original's digests did not change on any key, so these runs judge the composed port.

## Reproduce

```
~/bin/py -3.12 log/roadmap/S4/2b_mirror.py --w32                         # mirror DLLs + Win32 runner (HEAD + the wave + the shared edits)
~/bin/py -3.12 -m port.test.sound_replay oracle --step S4.k [--family ds|dev] [--update] [--keys ...] --out <tsv>
~/bin/py -3.12 -m port.test.sound_replay replay --step S4.k [--family f] [--w32 [--soft]]
~/bin/py -3.12 -m port.test.sound_replay negctl --step S4.k --mode m [--functions f ...] [--callee]
~/bin/py -3.12 -m port.test.sound_replay sinklog --step S4.8|S4.9|S4.9b [--family f] [--negctl snd.<fn>:<mode>]
bash log/roadmap/S4/2b_gates2.sh all                                      # every oracle / replay / control / sinklog of the wave
bash log/roadmap/S4/2b_pytest_all.sh                                      # the eleven audio test files, x64 + Win32 selections
~/bin/py -3.12 log/roadmap/S4/2b_golden_compare.py                        # emulator digests vs the accepted goldens (10 files)
~/bin/py -3.12 log/roadmap/S4/2b_apply_shared.py --prove                  # the shared edits against the mirror build
~/bin/py -3.12 log/roadmap/S4/2b_fulltree.py [--fresh]                    # whole-tree mirror port/build/S4_2b_full
sh log/roadmap/S4/2b_suite_by_file.sh                                     # port suite, one file per process, in the whole tree
~/bin/py -3.12 log/roadmap/merges/S4-3/suite_compare.py log/roadmap/S4/2b_suite log/roadmap/merges/S1S8-4/suite
(cd port/build/S4_2b_full && ~/bin/py -3.12 -m port.test.integrator segments [--backend win32] --jobs 3 --out <tsv>)
(cd port/build/S4_2b_full && ~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out <tsv>)
```

## Controls

`bash log/roadmap/S4/2b_negctl_all.sh` (live oracle, layout A; TSVs 2b_negctl_s4*_m*.tsv, log 2b_negctl_all.log): S4.2 61 / 61
RED (the 60 functions + audio_core.crt.ftol, judged through S4.4's renderChannelStreamed_fade* / renderChannelVoice* keys:
NEGCTL_EXTRA), S4.4 38 / 38 + mode 2 8 / 8, S4.6 101 / 101 + mode 2 6 / 6 + Voice::update modes 3..8 6 / 6, S4.8 ds 54 / 54 +
dev 62 / 62 + setVolume mode 2 RED, setVolume mode 3 BLIND on its 30 vectors (as accepted; the grid reads it RED). The default
pytest files repeat mode 1 of every function against the goldens from the bundles.

## Phase 2: the rest of the wave on the consolidated core (S4.3, S4.5, S4.7, S4.9, S4.9b), the `snd.` names, the whole wave judged

Migrations (each module's own stack-local helper and private x87 code replaced by the core's):
- S4.5 audio_seq, S4.7 audio_sound: `fcell(ctx, n)` (cells in image padding) -> one `SndFrame fr_cells(ctx, ...)` per function
  (2b_patch_fcell.py); their X80 blocks -> the unit's helpers (2b_patch_x80_seq_sound.py). The cells are SndFrame slots, not at the
  original's offsets (no key compares those addresses: U-1440 / U-1460 partial). Seq_buildPlayOrder's nextRandomFloat result now
  stays on the unit and is popped by the core's __ftol (snd_ftol), as at 0x23d42 / 0x23d47 (it left 3 registers on the unit).
- S4.3 audio_codec: CodecFrame keeps its interface (ebp, loc<T>) over the core's SndFrame at the original's /GZ offsets (the deepest
  chain needs 0x7a4 bytes: FRAME_LIMIT 0x1C00, U-1420 resolved). The codec's arithmetic stays C doubles at the caller's word
  0x027f (U-1421 open); its compares are C comparisons, so every codec body marks the unit's C0..C3 unknown (CxScope ->
  `cc_unmodelled()`, image flag bit 0 at offset 10) and the harness skips C0..C3 for those vectors (U-1419 note). floatCopySign
  now pushes its result on the unit (`fld qword [ebp-0x14]` 0x3e3ec) and the codec's float-call helper pops it (every call site
  stores with fstp).
- S4.9 audio_wave: WaveFrame -> SndFrame directly. openMmioSound's two clamp compares (0x3096a / 0x30982) and closeMmioSound's
  zero-or-NaN tests (0x35db3 / 0x35dc6) run on the unit (their C0 / C3 are the exit state); the six clamp loops of
  writeWaveChunk / B run the original's `fninit` (0x7e5e, 0x7f2a, 0x8121, 0x81aa, 0x824a, 0x82e8) on the unit (exit word 0x037f).
- S4.9b audio_soundfile: SfFrame -> `SndFrame F`; 9b_replay.py now forwards to `sound_replay --step S4.9b` (9b_snap/ is history
  and can be deleted), 9b_negctl.py reads `load_golden("S4.9b")`.
- Runners folded into port/test/sound_replay.py: STEPS goldens for S4.3 / S4.5 / S4.7 / S4.9 / S4.9b, the S4.9 / S4.9b custom keys
  (sound_fixtures/s49_wave.js, s49b_soundfile.js with the user's own files), QUIRK_FUNCTIONS + closeMmioSound, NEGCTL_MODES (codec
  spills, Seq_setTempo, Sound5b6a8_ctor, the S4.9 import controls, loadSoundFile 2..10), the U-1462 clock VM for getPlaybackTime
  (CLOCK_FUNCTIONS), STEP_OF_FUNCTION (loadSoundFile is S4.9b's), PORTED_STEPS for the GREEN_PENDING rule, `negctl --callee` (U-1425).
- Names: every sound.dll function is registered `snd.<hooks.csv name>` (60 core + 38 + 101 + 54 + 62 + 42 + 80 + 66 + 100 + 1); the
  dispatcher looks rva -> "snd." + name; harness entries keep `audio_core.*`. port/test/test_audio_names.py (static over the FuncDef
  tables + the built registry): no name registered twice (except HEAD's saveGame in tick and saveload, outside the port: listed as
  known), no name normalised (`::` -> `_`) equal to a .vec stem of another module, no sound.dll function without the prefix, no
  `snd.` name in an exe module or outside sound.dll's names (S4_functions.tsv, hooks.csv, re/names); controls for each rule. In the
  shared tree it fails on 74 `snd.FUN_<va>` names in a sibling's unmerged port/src/sim/audio_exe.cpp (not this wave's file; the rule
  wants hooks.csv names); in the wave's tree (port/build/S4_2b_full) it passes.
- Tests rewritten on sound_replay: test_audio_codec / seq / sound / wave / soundfile; `snd.` names in test_audio_core / mixer / chan
  / ds / dev; test_audio_dev's pinned fallbacks are now only the CRT's _atexit (S4.7's callees are ported in this build);
  test_audio_core's `_unported` takes a row no step owns (every step row is ported).

### Phase 2 gate numbers (2026-10-24/25, the final mirror build; logs log/roadmap/S4/2b_g2_*)

No number is lower than at acceptance. Oracle = live, layouts A / B / C, x87 exit state on every key, `--update`.

| step | gate at acceptance | phase 2 | logs |
|---|---|---|---|
| S4.2 | 68 keys / 1,219 GREEN, 61 controls | 68 / 1,219 GREEN, 0 fallbacks; replay x64 + Win32 68 / 68; mode 1 61 / 61 RED | 2b_g2_oracle_S42, replay_*_S42, negctl_S42_m1 |
| S4.3 | 57 / 628 GREEN, m1 42 / 42, m2 3 / 3, pytest 111 | 57 / 628 GREEN (C0..C3 not compared on 626 vectors: the codec bodies leave them unknown, U-1419; the two _oom vectors run no compare), 0 fallbacks; replay 57 / 57 both; m1 42 / 42, m2 3 / 3 RED; pytest 111 passed + Win32 7 | 2b_oracle_S43 (phase 2 run), g2_negctl_S43_m1/m2 |
| S4.4 | 57 / 2,001 (56 GREEN + 1 PENDING), m1 38, m2 8 | 57 / 2,001 GREEN, 0 fallbacks (Mixer_ctor's S4.7 callee is ported now); replay 57 x64, 57 Win32, 57 --soft; m1 38 / 38, m2 8 / 8 RED | 2b_g2_oracle_S44 |
| S4.5 | 113 / 1,840 (112 GREEN + 1 X87_WIN32), m1 80 / 80, Seq_setTempo m2 RED, pytest 199 | 113 / 1,840: 112 GREEN + 1 GREEN_PENDING (CRT -:0x424d7 / operator_delete), x64 included; replay 113 / 113 both; m1 80 / 80, m2 1 / 1 RED; pytest 200 passed + Win32 3 | 2b_oracle_S45 + 2b_oracle_S45 Seq_buildPlayOrder re-run, g2_negctl_S45_* |
| S4.6 | 168 / 3,344 (156 + 12 X87_WIN32 before S4.2b; 156 + 12 PENDING after phase 1) | 168 / 3,344: 162 GREEN + 6 GREEN_PENDING, 0 RED, 17 sNaN vectors forgiven as accepted; replay 168 both; m1 101 / 101, m2 6 / 6, Voice::update 3..8 6 / 6 RED; pytest 288 / 269 + Win32 7 | 2b_g2_oracle_S46 |
| S4.7 | 117 / 2,146 GREEN, m1 66 / 66, pytest 189 | 117 / 2,146: 107 GREEN + 10 GREEN_PENDING (CRT strstr, strrchr, sprintf, eh vector helpers, 0x2c270, 0x4280e); replay 117 both; m1 66 / 66 RED; Sound5b6a8_ctor m2 BLIND as accepted (U-1461, pinned by a test); pytest 190 passed + Win32 3 | 2b_oracle_S47, g2_negctl_S47_* |
| S4.8 | ds 70 / 1,215, dev 104 / 1,316, m1 54 + 62, sinklog 36 EQUAL | ds 70 GREEN; dev 104: 92 GREEN + 12 GREEN_PENDING (CRT _atexit only; 17 before, S4.7's callees are ported now); replay 70 + 104 both; m1 54 + 62 RED, setVolume m2 RED, m3 BLIND on 30 vectors as accepted (grid RED); sinklog 5 + 31 EQUAL, 97 + 444 import commands, 0 records from host-run code (85 before: those callees are port code now); pytest ds 131 / 71, dev 170 / 104, Win32 1 + 1; setVolume grid passes | 2b_g2_oracle_S48ds/dev, g2_sinklog_S48* |
| S4.9 | 247 keys GREEN, 100 / 100 RED, sinklog 83 / 83, pytest 353 | 247 / 2,404: 241 GREEN + 6 GREEN_PENDING (CRT strstr), 1 sNaN vector forgiven; replay 247 both; m1 100 / 100 RED; sinklog 83 / 83 EQUAL (2,614 commands), controls closeMmioHandle m2 DIFF on 3 keys, WaveInDevice_reset m2 DIFF on 6; pytest 353 passed + Win32 1 | 2b_oracle_S49, g2_oracle_S49fix, g2_negctl_S49_m1, g2_sinklog_S49* |
| S4.9b | 25 / 74, mmio log 25 / 25, 10 / 10 RED, pytest 41 | 25 / 74 GREEN_PENDING (CRT + U-1493's callees, counted, none into an accepted step's function); replay 25 both; modes 1..10 10 / 10 RED; sinklog 25 / 25 EQUAL (3,744 commands); pytest 41 passed + Win32 1 | 2b_oracle_S49b, g2_negctl_S49b_m*, g2_sinklog_S49b |
| goldens | | all ten files equal to the accepted ones key for key (digests, tiers, regions, sNaN entries): core 68, chan 168, ds 70, dev 104, mixer 57, codec 57, seq 113, sound 117, wave 247, soundfile 25 | 2b_g2_golden_compare.log |
| shared edits | | 2b_apply_shared.py --prove: modules.inc, portsim_c.h, portsim_c.cpp EQUAL to the mirror build's | |
| names | | test_audio_names 8 passed in the wave's tree; in the shared tree 1 failure from a sibling's audio_exe.cpp (above) | |
| port suite x64 (whole tree, one file per process) | HEAD's merge run log/roadmap/merges/S1S8-4/suite, 52 files | 63 files: the 11 audio files new (chan 288 / 269, codec 111 / 99, core 146 / 128, dev 170 / 104, ds 131 / 71, mixer 109 / 57, names 8, seq 200 / 193, sound 190 / 183, soundfile 41 / 25, wave 353 / 347); 46 files equal to HEAD's counts; test_video 32 / 24 and test_s8_2_scenarios 14 equal to I4's reruns; test_shots 33 / 1 on a re-run (the first run lost the Win32 runner at fixture setup, 7 errors, while siblings' runs held most of the commit charge); test_vectors 298 passed (equal) with 1,254 more skips and test_win32 179 passed (equal) with 627 more skips, every new skip a `snd.` / `audio_core.` name with no exe vector file (604 sound.dll functions + 23 harness entries); test_boot 94 passed + 1 failed: the 3 extra rows are siblings' corpus recordings not at HEAD, and the failing cov_r_s2_screens fails identically (record 771, g_cursorTileX / Y) on the build without S4.2b (2b_boot_base_new3.tsv) | 2b_suite/, 2b_suite_compare_S1S8.log |
| segments | 205 / 205 x64 + Win32 (I4: merges/S1S8-4/segs_*.tsv) | 205 / 205 PASS on x64 and on Win32 in the wave's tree; every row's status, frames and digest equal to I4's TSV of the same backend, and x64 = Win32 on all 205 | 2b_g2_segs_x64.tsv, 2b_g2_segs_win32.tsv |
| boot gate | HEAD's 61: 58 PASS / 2 KNOWN / 1 NO_FRAMES, S1 / S8 rows as measured | 58 / 2 / 1, 0 HEAD rows differ from I4's baseline (merges/S1S8-4/boot_compare.py), the six S1 / S8 section rows as measured (cov_r_bio 687 ... cov_r_s8_play 216); the 3 siblings' rows as on the build without S4.2b | 2b_g2_boot_x64.tsv |
| repo static | 614 / 2 (I4) | 630 passed / 2 skipped = 614 + tests/test_fixvm.py 16 (S4.1); test_movie_inventory needs the `extracted` junction I4 used (added to 2b_fulltree.py; 7 / 7 on the re-run) | 2b_suite/static.txt, static_movie_inventory_rerun.txt |

## Phase 3: S4.10 and S4.11 folded in (2026-10-25, HEAD b3d61e41)

- The resubmitted wave is S4.1 .. S4.11. The shared edits of 10_merge.md (modules.inc audio_exe, tick.cpp kDrops, tick_boot.inc's three
  boot calls, the U-1411 / U-1415 / U-1416 / U-1494 statuses, U-1421's text) and 11_merge.md (modules.inc audio_crt, S4_functions.tsv,
  U-1493) are in 2b_apply_shared.py; the requests that touch the wave's own files (audio_core.cpp lookups, audio_wave.cpp argument
  fixes, sound_replay.py's S4.11 install) are made in the tree. 2b_mirror.py takes its shared port files from the script.
- HEAD moved to b3d61e41 (S2 + S3): I3's wave-1 script is run with one check relaxed (the CLAUDE.md paragraph before "Phase 7" is
  now S3's; the S4 paragraph still goes there, in section order).
- STACK RESIDUE (audio.h SndResidue, U-1418 note): the original pushes words its callee does not read (`push 0xe; call [edx + 0x40]`,
  slot 0x40 = `mov eax, [ecx + 0x1ac]; ret`, the 0xe then setRegionMs's second argument; `push 8` on the stream-open path; ecx loaded
  before flushSoundList, which reads no argument). S4.10 dropped them to pass the core's arity check, which made six S4.9 keys RED
  (their stand-ins log those slots). Inside a SndResidue scope the call passes them and a port callee of a smaller arity is entered
  anyway; the scope covers the direct call only. The six keys are its evidence (RED without the scope's words, GREEN with).
- audio_exe.cpp (S4.10, owned by S4.2b for the merge): the image-size literals as shifts and the image base by name (test_static's
  address rule passes); the 74 `snd.FUN_<va>` names follow the NAME RULE: an unnamed sound.dll function is registered by its name in
  the master's listing re/functions_ghidra_sound.dll.tsv (Ghidra's default FUN_<va>, va = 0x10000000 + rva) while hooks.csv, re/names
  and S4_functions.tsv have no name at that rva; test_audio_names accepts exactly that (controls: FUN_1002e3c0, which is
  GlblHeader::init, and FUN_1002e3c1, not a function entry, are refused). audio_crt joins SOUND_MODULES.
- Tests: test_audio_dev's pinned fallbacks are empty (S4.11 ports _atexit); test_audio_soundfile's docstring drops the pending note.
- Suite fallout of S4.10's kDrops removal (its own gates did not run these files), fixed in the wave:
  - the UI tests (test_commands, test_dialogs, test_chareditor, test_chareditor_bio, test_reports) and test_events' bulletin board
    and live-oracle control judged playSound / startSoundSlot / Snd::status as absent: their emulator oracles stub those
    addresses and liveraw's captures had playSound stood in. Registered, the real playSound ran S4.10's sound layer on synthetic
    states with no constructed g_sounds and no install (faults, MissingResource, changed digests). audio_exe.stubRequests (a
    test-only per-context switch, default off) makes the three return 0 and run nothing, the port-side counterpart of those
    stubs; the 18 places these files (and liveraw.py) create a state set it when the build has audio_exe. Goldens unchanged.
  - test_bridge's playSoundAt test now attaches original/ (the real playSound maps the user's exe from the game files, as a
    segment run does); test_framegate's kDrops table check knows the three left kDrops for audio_exe.
  - SndGroup_isDisabled / isMuted were registered ret `int`; the exe corpus and the registry (c3r.py) say `uint8` (al): aligned
    (test_vectors' interface check). The exe functions S4.10 registers that have vector files now run there: 310 passed (HEAD
    298), the extra skips are `no vector file` names only.

### Phase 3 gate numbers (2026-10-25, HEAD b3d61e41, the final mirror build; logs log/roadmap/S4/2b_g3_*)

| gate | at acceptance | phase 3 |
|---|---|---|
| sound_replay oracle (`--update`, layouts A / B / C, x87 exit state) | S4.2 68, S4.3 57, S4.4 57, S4.5 113, S4.6 168, S4.7 117, S4.8 70 + 104, S4.9 247, S4.9b 25, S4.11 61 | all 1,087 keys / 16,859 vectors GREEN, 0 GREEN_PENDING, 0 fallback calls (S4.11's census said 4,800 -> 0; here 0 in every step), 17 + 1 sNaN vectors forgiven as accepted |
| audio_stream (S4.10) oracle | 182 keys / 1,861 | 182 / 1,861: 178 GREEN + 4 GREEN_PENDING on the exe callees other modules own (return0_479820, NodeList / B / C::dtor, 10_merge.md section 5); the three arrayStaticInit_448160 keys run by name (`--keys`), as in S4.10's 10_oracle_new2.tsv |
| goldens | | twelve files equal to the accepted ones key for key (core 68, chan 168, ds 70, dev 104, mixer 57, codec 57, seq 113, sound 117, wave 247, soundfile 25, crt 61, exe 183 incl. the device stream) |
| S4.10 streams / device | 205 / 205, 4,964 requests; device 181 requests, 13,020 lines | 205 / 205 PASS, 4,964 requests, 0 other commands; device 181 requests, 0 fallbacks, 13,020 lines, stream sha256 equal to the pinned `$device` |
| replay x64 + Win32 | | every step 0 failures on both backends (S4.2 .. S4.11, S4.8 ds + dev, S4.4 --soft) |
| controls | as phase 2, S4.11 28 / 28 | S4.2 61, S4.3 42 + 3, S4.4 38 + 8, S4.5 80 + 1, S4.6 101 + 6 + Voice::update 6, S4.7 66, S4.8 54 + 62 + setVolume m2, S4.9 100, S4.9b 10 modes, S4.11 28: all RED; Sound5b6a8_ctor m2 and setVolume m3 BLIND as accepted |
| sink logs | 5 + 31 + 83 + 25 EQUAL | 5 + 31 + 83 + 25 EQUAL (97 + 444 + 2,614 + 3,744 import commands); closeMmioHandle m2 DIFF on 3 keys, WaveInDevice_reset m2 on 6 |
| shared edits | | 2b_apply_shared.py --prove: modules.inc, portsim_c.h, portsim_c.cpp, tick.cpp, tick_boot.inc EQUAL to the mirror build's; 70 UNCERTAINTIES rows |
| port suite x64 (whole tree on b3d61e41, one file per process) | I5's run log/roadmap/merges/S2S3-5/suite | 70 files: the 13 audio files new (chan 288 / 269, codec 111 / 99, core 146 / 128, crt 96 / 61, dev 170 / 104, ds 131 / 71, exe 302 / 5, mixer 109 / 57, names 11, seq 200 / 193, sound 190 / 183, soundfile 41 / 25, wave 353 / 347); every other file at I5's counts after the fixes above (re-run 2b_suite_r2: bridge 25, chareditor 19 / 4, chareditor_bio 28 / 8, commands 83 / 44, dialogs 27 / 7, events 50, framegate 32, reports 14 / 2), test_boot 94 + the same cov_r_s2_screens failure as I5, test_s8_2_scenarios 14 (= I5's rerun), test_vectors 310 passed (298 + 12 exe functions with vector files) and test_win32 185 passed (179 + 6), their extra skips all `no vector file` names | 2b_suite/, 2b_suite_r2/, 2b_suite_compare_S2S3.log |
| repo static | 614 / 2 | 630 passed / 2 skipped (+ tests/test_fixvm.py 16) |
| segments | 205 / 205 x64 + Win32 (I5) | 205 / 205 on both, 0 rows differing from I5's TSVs, x64 = Win32 | 2b_g3_segs_*.tsv |
| boot gate | 67 baseline rows + S2's 3 (I5) | 0 baseline rows differ, S2's 3 as stated (merges/S2S3-5/boot_compare.py) | 2b_g3_boot_x64.tsv |

## Open

- U-1418 (frames modelled only where an address is passed; host frames 0x400 below the entry), U-1419 (want_st0 host replies not
  pushed on the unit; fyl2x is the emulator's; value helpers keep intermediates off the register stack; C0..C3 not compared for
  bodies with C compares: the codec, cc_unknown).
- U-1421: the codec runs at its caller's control word; every key is at 0x027f. U-1440 / U-1460 partial (seq / sound cells not at the
  original's offsets).
- Counted fallbacks left (S4.11): the static CRT (strstr, strrchr, sprintf, _atexit, the eh vector helpers 0x4280e / 0x42902,
  operator_delete, 0x424d7) and U-1493's unnamed callees (0x2e3c0, 0x5f40, 0x2cf50, 0x2c270); none reaches an accepted step's
  function (GREEN_PENDING rule).
- A sibling's unmerged port/src/sim/audio_exe.cpp registers 74 sound.dll functions as `snd.FUN_<va>`, which test_audio_names
  rejects (the rule wants hooks.csv names); HEAD's saveGame is registered by both tick and saveload.
- The .cpp files are the sources: never re-run 3_assemble.py / 6_assemble.py / 7_assemble.py.
- audio_exe.stubRequests is test-only: the UI tests stub the audio requests because their emulator oracles do. A test that
  judges the sound layer under those screens needs constructed g_sounds records and the install (boot or segment states).
- In the shared tree (no shared edit applied) test_static's module list and test_framegate's kDrops check fail until the
  merge; both pass in the whole tree.

## Status

DONE (2026-10-25): the resubmitted wave S4.1 .. S4.11 is in the tree, with no shared file edited. Every shared edit is made by
log/roadmap/S4/2b_apply_shared.py, proven against the mirror build and applied in the whole-tree mirror port/build/S4_2b_full on HEAD
b3d61e41. The gates in the phase-3 table ran there: port suite, repo static, segments x64 + Win32, boot gate. Merge request
log/roadmap/S4/2b_merge.md. The Win32 backend runs the audio files' selections only (core 1, mixer 61, chan 7, ds 1, dev 1, codec 7,
seq 3, sound 3, wave 1, soundfile 1, crt 1, all passing; exe has none) plus the setVolume grid, not the whole Win32 suite (HEAD's has
20 known failures, U-1100 / U-1101).

## CLAUDE.md paragraph (after the S4.9b paragraph; replaces what the S4.4 / S4.6 / S4.8 paragraphs say about frames, the x87 unit's location and the runners)

```
Step S4.2b, the sound.dll port's core consolidated and the S4 wave resubmitted (2026-10-24/25, notes log/roadmap/S4/2b_notes.md,
merge request 2b_merge.md, U-1418 / U-1419). ONE STACK (portsim/sim/audio.h): SndFrame keeps a local the original passes by
address at the original's offset below the function's entry esp (f.at, f.arg, f.set_esp for `and esp, -8` frames); a top-level
call takes the caller's esp (audio_core.vector), a callee the dispatcher calls enters at the esp a `call` leaves, a direct C call
enters below the live frame, production falls back to a frame block at 0xFEB00000. It replaced S4.4's frame_push, S4.6's frame
cells, S4.8's SndLocals, S4.5 / S4.7's fcell, S4.3's CodecFrame body, S4.9's WaveFrame and S4.9b's SfFrame. THE X87 UNIT is the
core's FPU model (audio.h, audio_core.cpp, audio_x80.inc): every module's x87 code runs on it at the unit's control word on x64
and Win32 alike, a host call sees it through the box page 0xFEAF0000, sound.dll's __ftol is audio_core.crt.ftol, and the harness
compares the x87 EXIT state (cw, TOP, C0..C3, registers) on every key; bodies whose compares are C comparisons (the codec) mark
C0..C3 unknown (cc_unmodelled) and are not compared there. The dispatcher routes an unclaimed `jmp [iat]` stub as the import
(U-1472). NAMES: every sound.dll function is registered `snd.<hooks.csv name>` (harness entries `audio_core.*`);
port/test/test_audio_names.py rejects a duplicate name, a name equal to another module's .vec stem, a sound.dll function without
the prefix and a `snd.` name that is not a sound.dll name. port/test/sound_replay.py is the one front end for every step
(`--step S4.k [--family f]`; oracle / replay / negctl [--callee] / sinklog / keys); the step runners are retired. The whole wave
in one build: S4.2 68, S4.3 57, S4.4 57, S4.5 113, S4.6 168, S4.7 117, S4.8 70 + 104, S4.9 247, S4.9b 25 keys, 0 RED (fallbacks
only into the static CRT and U-1493's unnamed callees, GREEN_PENDING), every emulator digest equal to the accepted goldens, every
mode-1 control RED (S4.2 61, S4.3 42, S4.4 38, S4.5 80, S4.6 101, S4.7 66, S4.8 54 + 62, S4.9 100, S4.9b 10 modes), sink logs
5 + 31 + 83 + 25 EQUAL. The shared edits are one script, log/roadmap/S4/2b_apply_shared.py (I3's wave-1 script, then the nine
modules.inc lines and U-1418 .. U-1494), proven against the mirror build.
```
