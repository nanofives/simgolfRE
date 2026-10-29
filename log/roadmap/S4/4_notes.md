# S4.4 notes: the mixers and channel renderers (module audio_mixer)

Status: DONE (2026-10-24). Merge request: log/roadmap/S4/4_merge.md. Uncertainties U-1430..U-1437: log/roadmap/S4/4_uncertain.tsv.
Files: log/roadmap/S4/4_files.txt.

## What was built

| file | what |
|---|---|
| `port/include/portsim/sim/audio_mixer.h` | the VIRTUAL X87 UNIT (`portsim::x87::Unit`: control word, status word's TOP and C0..C3, which registers are in use, eight 80-bit physical registers; per context, persisting across calls like the thread's FPU), its operations (`add sub mul div fld64 from_f64 fild fst64 fist fcom fabs_ fchs`, each one x87 instruction's arithmetic under a control word), the image format the harness exchanges, the frame block (`frame_push / frame_pop`), `kSndFtolRva`. |
| `port/src/sim/audio_x80.inc` | the software 80-bit path: unpacked (sign, exponent, normalised 64-bit significand), exact intermediate results on a 128-bit significand plus sticky, ONE rounding per operation to 24 / 53 / 64 bits (precision control) by the rounding control, double stores rounded by the rounding control with denormals / overflow, integer stores with the indefinite, the SDM NaN and invalid rules. No third-party code. |
| `port/src/sim/audio_mixer.cpp` | part 1: the unit's side table, the Win32 backend (each operation the instruction itself: `fldcw` the unit's word, `fld tbyte` the operands, the instruction, `fstp tbyte`, `fnclex`, the thread's word back), `audio_mixer.*` harness entries (`x87load / x87save / x80soft / x80batch / report`), sound.dll's `__ftol` (`audio_mixer.crt.ftol`). Part 2: the 38 functions in five sections (A x87 resamplers, B integer mixers, C MMX renderers, D mixer objects, E channel renderers). |
| `port/test/test_audio_mixer.py` | the harness extension, the CLI (`oracle / replay / negctl`, `--coverage`), the pytest tests. |
| `port/test/golden_audio_mixer.json` | digests pinned from the emulated original: 57 keys, 2,001 vectors, each with the digest of the x87 exit state. |
| `log/roadmap/S4/4_mirror.py` | the mirror (2_mirror.py's copy): HEAD + S4.2's files and C ABI patch + this step's files + the two edits of 4_merge.md. |
| `log/roadmap/S4/4_x80probe.py`, `4_tags_check.py`, `4_debug.py`, `4_dis.py`, `4_dis/`, `4_conv/` | probe of the operations, the tag-normalisation proof, a per-vector debugger, the disassembly the bodies were read from, the section drafts (`4_conv/sec*.cpp`, spliced by `4_conv/splice.py`). |

## The x87 method (as built)

- Win32 runs the x87 instruction for every arithmetic step (decision: "inline x87 under _M_IX86 at the original's word");
  x64 runs audio_x80.inc. `audio_mixer.x80soft 1` makes Win32 run the software path too.
- Every body transcribes its x87 instructions onto the unit in the original's order and operand order, register stack
  included: the resamplers `fninit; fldcw 0x0f7f` and push what they leave (4 at the count exit, 5 at the end exit, 2 for
  the stereo pair); renderChannelSingle's fade (renderChannelSingle+0x169..+0x203) pushes, compares (`fcom/fcomp`, the
  condition codes `fnstsw` reads) and pops on top of them; renderChannelStreamed's fade and renderChannelVoice's fade / rate
  reset likewise, calling `__ftol` through the dispatcher (it pops st(0) off the unit).
- The MMX renderers make every register valid with TOP 0 and empty them at `emms`.
- `X87Unmodelled` (stack overflow / underflow, extended exponent range, unmasked exceptions) stops a call (U-1432).

## The harness (port/test/test_audio_mixer.py, over S4.2's sound_replay.py without editing it)

- `use_mirror()` points sound_replay at the S4.4 mirror and golden file and installs `MixerLivePort` (LivePort subclass).
- Per vector the unit is loaded with the FPU the ORIGINAL arm found (`KeyRun._arm` wrapper records Unicorn's FPCW / FPSW /
  FPTAG / FP0..7 before and after), and the unit the port leaves is compared with the FPU the original left: control word,
  TOP, C0..C3, registers in use and their 80 bits (`x87_diff`); for a double / float return the caller's `fstp` is applied
  to the port's image first (`pop_image`). After the port arm Unicorn's FPU is put back to the original's exit state.
- A host call must not change Unicorn's FPU (checked; the script host replays memory only).
- The frame block's pages are mapped (and marked dirty, so the fixture VM learns them) in the emulator before the port arm.
- At key start the emulator's eight `valid` tags (Unicorn never ran an `fninit`) are emptied; the emulated original's
  digests of all 55 keys are identical with and without (log/roadmap/S4/4_tags_check.tsv, U-1431).
- `GREEN_PENDING`: every vector equal and every fallback call reaches a function another step owns (Mixer_ctor ->
  Sound_setSourceType, S4.7; U-1436). The bundle keeps the call, the replay serves it (`PASS_PENDING`).
- Bundles `port/corpus/audio/<key>.json.z` carry `x87` (the entry image) per vector; goldens carry `x87_exit` digests.

## Keys (57)

- The 47 registry keys of the S4.4 rows (sound_replay.keys_for("S4.4"); mixRatedStereo16Set's emu-only row has its own
  key, mixResampleG).
- The six MMX renderers (emu-only: installed only by an unreferenced constructor) as DERIVED keys: they take the rated
  mixers' arguments and read the same voice fields, so each borrows the rated mixer of its format and add / store kind
  (c3do_mixR / c3do_mixRG fixtures: loops, chains, eof, fades, rings); the stereo pair twice (both fixtures). Their
  original arm runs on a zeroed emulator stack: on a count of 1 they read two frame slots no instruction wrote (U-1433).
- Two CHAINED keys (`renderChannelSingle@mix`, `renderChannelVoice@mix`): the renderer fixtures stand in their mixers, so
  these revert the stubs in the fixture VM (`eval` of `Interceptor.revert`) and give every channel / voice a real sample
  record over a fixture-owned region (0x6D000000); the renderer runs the real port mixers through the dispatcher and its
  fade on top of their registers (v3..v20 of the first: the unit leaves 2 or 4 registers at 0x0f7f, C0 / C3 set by the
  fade's compares; all equal to the original).

## Gate numbers (2026-10-24)

- ORACLE (log/roadmap/S4/4_oracle_update.tsv / .log): 57 keys, 2,001 vectors; port == emulated original on the return text,
  every region byte AND the x87 exit state, layouts A, B and C; 56 GREEN, 1 GREEN_PENDING (Mixer_ctor), 0 RED; fallbacks 0
  except Mixer_ctor's 36 (S4.7). Census tiers of the registry keys: EQUAL_PI 47; the 10 derived keys EMU_ONLY.
- DEFAULT replay from bundles: x64 57 / 57 PASS (4_replay_x64.log), Win32 57 / 57 PASS on the x87 instructions
  (4_replay_w32.log), Win32 with the software path 57 / 57 (4_replay_w32soft.log).
- x87 operations against Unicorn (log/roadmap/S4/4_x80probe.py, seeds 1..3, 60,000 records, eight control words, add / sub /
  mul / div / fst qword / fist word / dword / qword / fcom / fld qword / fild qword): 0 mismatches except the signaling-NaN
  moves (53 / 58 / 70, U-1430), identical on x64, the Win32 instructions and the Win32 software path, so the Win32
  instructions equal the software path on all 60,000 (log/roadmap/S4/4_x80probe_seeds.log). The default test's Win32 probe
  found one NaN-tie record where the software path picked the first operand; the hardware and Unicorn pick the positive one,
  the software path does now (U-1434, test_x87_nan_ties_equal_the_emulator).
- pytest port/test/test_audio_mixer.py: x64 60 passed / 57 skipped (the live oracle per key, SG_EMU_ORACLE=1),
  --backend win32 60 passed (4_pytest_default.log, 4_pytest_w32.log).
- Negative controls (live oracle, layout A): mode 1 of all 38 functions and of `audio_mixer.crt.ftol` RED (39 / 39, 0 blind;
  4_negctl_m1.tsv); mode 2, the c3fh long-double control (every resampler intermediate a 53-bit double rounded to
  nearest), RED on all 8 resamplers (28..44 of 32..46 vectors per function, 4_negctl_m2.tsv).
- Branch coverage of the emulated original (4_coverage.tsv, layout A, all keys): 852 conditional jumps in the 38 bodies,
  753 with both sides taken, 88 one side, 11 none (the dead second stage of destroyMixerVoices, mixStereoGainAdd's
  faulting arm, mixRatedStereo16Set's uninitialised-eof arm, renderChannel's zero-fill arm, the MMX stereo pair's loop
  clip). The one-sided ones are mostly the MMX renderers' loop / eof handlers (the borrowed vectors reach them on one side)
  and renderChannelStreamed's successor NULL tests (the NULL side faults).
- Segment gate on the mirror: x64 205 / 205 PASS, 0 digests differ from log/p5/segs_x64_loc.tsv; Win32 205 / 205, 0 differ
  from log/p5/segs_win32_loc.tsv and from x64 (4_segments_mirror*.tsv / .log).
- Boot gate on the mirror (4_boot_gate_mirror.log, 67 scenarios now: siblings added six since BASELINE): 62 PASS / 2 KNOWN
  (cov_a_golfer, cov_b_holes18: U-1062) / 1 NO_FRAMES (cov_b_location) / 2 FAIL (cov_r_bio: sg_run_boot -11, as on S4.2's
  mirror; cov_r_flic_input: record 1 g_cachedCameraTileX). Both FAILs are the same on a HEAD-only build
  (`4_mirror.py --base`, 4_boot_base_check.log), so neither comes from this step.
- test_static: test_every_module_file_is_listed fails only on the unlisted modules (audio_core's and audio_mixer's lines are
  merge requests; siblings' audio_* files show there too); the address test passes.

## Reproduce

```
~/bin/py -3.12 log/roadmap/S4/4_mirror.py --w32
~/bin/py -3.12 -m port.test.test_audio_mixer oracle [--update] [--layouts ABC] [--coverage <tsv>] --out <tsv>
~/bin/py -3.12 -m port.test.test_audio_mixer replay [--w32] [--soft]
~/bin/py -3.12 -m port.test.test_audio_mixer negctl --mode 1|2 --out <tsv>
~/bin/py -3.12 -m pytest port/test/test_audio_mixer.py [--backend win32]     # default: replay + x87 probes
SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_mixer.py
~/bin/py -3.12 log/roadmap/S4/4_x80probe.py --n 20000 [--w32 [--soft]]
~/bin/py -3.12 log/roadmap/S4/4_tags_check.py raw; ... norm; ... compare
~/bin/py -3.12 log/roadmap/S4/4_debug.py <key> [<vector> | summary]
```

## Findings (the original, from the instructions; the port reproduces them)

- The MMX renderers store the second output frame of a pass also when the count ran out after the first (one frame past
  the count), from lane-B slots of the previous pass or, on the first pass, from slots never written (U-1433). Their
  interpolation multiplies by (fraction - 0x8000) as a signed word (`sub dx, 0x8000` before `pmulhw`), and the mono-16 pair's
  lane-B loop handlers load a dword (two samples) where the other lanes duplicate one sample (Voice_renderStream16b+0x2ea for
  lane B's cur, +0x33e for its next), the lane-B next handler at esi + 4 (+0x32b) although the frames are 2 bytes apart.
- mixRenderRatedC / Voice_renderStream32 count frames into +0x16c in mode 1 where every other renderer counts the
  integer part of the position.
- renderChannelSingle's fade compares the 64-bit d left in st(0) after `fst qword` (the stored double is rounded, the
  compared value is not) and, after a mixer ran, computes at 0x0f7f on top of its registers.

## For the next S4 workers

- x87 code: use the unit (`x87::unit(ctx)`, the x87:: operations); x87 helpers under the caller's word (S4.5) inherit the
  mixers' 0x0f7f this way. A function that passes a stack local to a callee: the frame block.
- Judge x87 keys with test_audio_mixer's harness (`use_mirror()`), which also checks the exit state.

## For the core consolidation (S4.2b)

Lead's note of 2026-10-24: S4.6 (frame_cell in the image's tail padding) and S4.8 (SndLocals + the harness's entry esp, frame
block 0xFEB00000) handle stack locals differently; S4.2b puts one mechanism into audio.h. In this step:

- STACK LOCALS sit behind ONE helper: the RAII class `Frame` in port/src/sim/audio_mixer.cpp (`Frame fr(c, bytes); fr.va`),
  over `x87::frame_push / frame_pop` (portsim/sim/audio_mixer.h; block at kFrameVa 0x6E000000, 64 KB). Its three users are
  renderChannelSingle (4 bytes, the sample cursor), renderChannelVoice (4 bytes, the same) and renderChannelStreamed (0x18
  bytes, the six DsBuffer cells, zeroed as the original does). Migration: replace Frame's body with audio.h's mechanism (for
  S4.8's SndLocals the class maps one to one), drop frame_push / frame_pop and kFrameVa, and drop the frame-page mapping in
  MixerLivePort.__call__. The callees' fixture stubs log these pointers as 0, so the address chosen does not reach a digest.
- PER-VECTOR X87 STATE is one harness feature, a superset of S4.6's per-vector fpcw: the whole unit (control word, TOP,
  C0..C3, tags, eight registers) is loaded from the FPU the original arm found and compared with the FPU it left. The port
  side is two config entries (`audio_mixer.x87load` reading `$audio/x87`, `audio_mixer.x87save` writing `$audio/x87out`);
  a function that only needs the word reads `x87::unit(ctx).cw`. S4.5's caller-word helpers should take their word from the
  unit, so that what a mixer left (0x0f7f) carries over, rather than from a per-call fpcw.
- sNaN: the same rule as S4.6's U-1451 (the CPU quiets through fld / fst, Unicorn does not). Here it never reaches a vector
  (all 57 keys match the emulator exactly, no forgiveness needed); U-1430 is that row's x87-unit counterpart and can be
  merged into U-1451.

Runner features this step uses, all in port/test/test_audio_mixer.py (to fold into sound_replay.py):
1. x87 entry / exit image per vector: `KeyRun._arm` wrapper (Unicorn FPCW / FPSW / FPTAG / FP0..7 before and after the
   original arm), load before the port call, compare after (`x87_diff`; the caller's fstp applied for double / float returns,
   `pop_image`), Unicorn's FPU restored to the original's exit state after the port arm; bundles carry `x87` per vector,
   goldens `x87_exit` digests.
2. Tags emptied at key start (`KeyRun.setup` wrapper; proven neutral on all keys, U-1431).
3. A host call must leave Unicorn's FPU unchanged (checked in `MixerLivePort._host`).
4. Frame-block pages mapped and marked dirty in the emulator before the port arm (goes away with item 1 of S4.2b's mechanism).
5. DERIVED keys with a zeroed emulator stack (`ZERO_STACK`) and CHAINED keys with a fixture patch (`KeyRun.run` wrapper,
   `PATCHES`: stubs reverted through the VM's `eval`, sample records in a fixture-owned region at 0x6D000000).
6. GREEN_PENDING / PASS_PENDING: fallbacks allowed when every fallback rva is another step's row (`pending_of`).
7. `--coverage`: jcc-side coverage of the emulated original per function (a Unicorn code hook over the body's ranges).
8. A-only runs accept the census tier A_ONLY.

## Open

- U-1436 Mixer_ctor: re-run after S4.7. U-1430 SNaN moves. U-1433 uninitialised MMX slots. U-1435 frame block placement
  (S4.10).
