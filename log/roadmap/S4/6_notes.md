# S4.6 notes: channels and voices (module audio_chan)

Status: DONE (2026-10-24), all 101 rows of S4.6 ported and judged. Merge request: log/roadmap/S4/6_merge.md. Uncertainties U-1450..U-1455:
log/roadmap/S4/6_uncertain.tsv. Files: log/roadmap/S4/6_files.txt. Builds on S4.2 (log/roadmap/S4/2_notes.md, 2_merge.md,
re/analysis/data/sound_replay.md); S4.2's files (audio.h, audio_core.cpp, sound_replay.py, snd_hook2port.py) are used as they are.

## What was built

| file | what |
|---|---|
| `port/src/sim/audio_chan.cpp` | module `audio_chan`: 101 functions of the S4.6 rows (Channel_*, Voice_*, Ch5*, SubChannel, ChannelTable, ChanRecord, VoiceSlot, the voice / channel constructors and destructors, the five source openers, the x87 rate / fade / reset functions, Voice::update). GENERATED from the parts below by `log/roadmap/S4/6_assemble.py` (the .cpp is the deliverable; re-assembling reproduces it) |
| `port/include/portsim/sim/audio_chan.h` | the module's header (no API: documents the x87 functions, `kAudioChanX87Functions`) |
| `port/test/test_audio_chan.py` | 550 tests (264 default, 286 need SG_EMU_ORACLE=1 or are judged on Win32) |
| `port/test/golden_audio_chan.json` | golden digests: 168 keys, 3,344 vectors, tier, regions, x87 control words, quirk vectors |
| `log/roadmap/S4/6_run.py` | the runner (wraps S4.2's sound_replay.py without editing it): own mirror build and golden file, derived keys of the emu-only rows, Voice::update's synthetic keys, the x87 word of each vector, the sNaN quirk filter, Win32 replay with the word applied, x87 controls |
| `log/roadmap/S4/6_mirror.py`, `6_cycle.sh` | the private build `port/build/S4_6_mirror` (HEAD + S4.2's files + audio_chan; x64 and Win32 + runner) and the assemble + build + diagnostics cycle |
| `log/roadmap/S4/6_assemble.py`, `6_parts/head.cpp`, `hand.cpp`, `hand_update.cpp`, `6_conv/audio_chan.cpp` | the converter's output (snd_hook2port.py --step S4.6) and the hand blocks that replace or add to it; head = helpers, X80 |
| `log/roadmap/S4/6_fixtures.js` | the fixture of Voice::update (34 rows, 9 registry-list variants) injected into S4.1's fixture VM |
| `log/roadmap/S4/6_cover.py`, `6_diff.py`, `6_disasm.py`, `6_keys.py` | original-instruction coverage per function, where does the port differ, disassembler, key list |

## Method

1. `re/tools/snd_hook2port.py --step S4.6 --module audio_chan --compile` converted the hook sources (c3an, c3ar, c3au, c3az, c3ca, c3cl, c3cx,
   c3dh, c3dj, c3dt, c3ew, c3ex, c3fd, c3fe, c3fh, c3fk, c3fl, c3fq): 93 blocks (it refused two functions with a hook source, Channel_ctorB and
   serviceChannelPlayback: `unbalanced (` in a call it could not split), 101 compile errors in the output (memory pointers dereferenced as host
   pointers, `c3ex_vslot`, `void**`, the x87 helpers, a local named `st`, array members through `snd_obj`). 43 of the 93 blocks were rewritten by hand
   (`6_parts/hand.cpp`, from the hook source and the instructions); 50 are kept as converted (their provenance line says `converted from`); 8 functions
   are hand-written from the start (the two refused, five without a hook source, Voice::update).
2. Port rules kept: every call outside the function goes through the dispatcher (snd_call by rva, snd_vcall by slot, snd_import); module
   globals through snd_va / snd_obj; wrapping arithmetic through arith.h (sub32, udiv32, div32, neg32); no image-range literal (the quiet
   bit masks are written as shifts).
3. Five functions have no hook source and were written from the instructions: Channel_propagateFlag0 (0x2ddb0, 39 bytes), Channel_set3DBufferStub
   (0x37c00), and the three one-instruction stubs stubReturnBC (0x11160), Channel_stubReturnB (0x35f60), VoiceSlot_dtorEmpty (0x37df0); and
   Voice::update (0x1f8c0, 1,338 bytes, 416 instructions, x87) from its disassembly, line by line with addresses in the comments.

## Judge

- Oracle (decision G1): S4.1's fixture VM rebuilds each key, the original runs in Unicorn over the user's sound.dll, the port runs as the second
  arm on the same memory in layouts A / B / C, and calls the port makes to code it does not own (stand-ins of the fixture, other steps'
  functions) reach the emulator as host calls. The 156 registry keys of the 97 rows with a C3 path-1 key (Channel_propagateFlag0 and Channel_set3DBufferStub have keys), the 3 derived keys of the rows without one
  (stubReturnBC, Channel_stubReturnB, VoiceSlot_dtorEmpty borrow a twin's fixture: same argument slots, their own address) and 9 synthetic keys of
  Voice::update (own fixture file) = 168 keys, 3,344 vectors.
- Default replay (no emulator, no Node): bundles (port/corpus/audio, local) + golden digests, x64 and Win32.
- Negative controls: mode 1 of every function (a changed return, or one flipped dword the function owns), mode 2 of the x87 functions (every
  x87 operation as a plain double operation), modes 2..8 of Voice::update (named in hand_update.cpp: equality of X and [this+0x720], the
  round limit, `found`, the registry count, the +0x4c update, a NaN in [this+0x728], the unlink bit).
- x87 (port/CORE.md section 7): the five functions Channel_setRate, Channel_recomputeVoiceGains, Channel_setReverseRate, Channel_stepVolumeFade and
  Voice_resetDefaults ran in the C3 fixtures at two control words, 0x027f and 0x0f7f (17 keys, 133 vectors). On Win32 their bodies execute the
  original's x87 instructions (X80 helpers: one instruction group per call, 10-byte registers) under the word of the call; on x64 X80 is a double.
  The oracle reads the word from Unicorn's FPCW per vector and stores it in the bundle; the Win32 replay applies it with sg_set_fpcw.

## Gate numbers (2026-10-24)

- ORACLE (`6_run.py oracle --update`, log/roadmap/S4/6_oracle_final.tsv / .log): 168 keys, 3,344 vectors in three layouts; 156 GREEN (port equals
  the emulated original on the return text and every region byte, A 100%, B and C 100%), 12 `X87_WIN32` (the keys at word 0x0f7f where the x64 port
  differs on the vectors the word changes: 118 vectors, 77 equal on x64; judged on Win32), 0 RED, 0 errors, 0 fallbacks to S4.2 / S4.6 functions.
  Tiers of the emulated original against the C3 CSVs (S4.1 compare): EXACT 19, EQUAL_PI 137, EMU_ONLY 12.
- sNaN quirk (U-1451): 17 vectors in 7 keys (updateVoiceRate_22050 / 44100 / max, Channel_startVoice_22050 / 44100 / null, serviceChannelPlayback_dev)
  differ from the emulator only in the quiet bit of a NaN the original copies through fld / fst; the port follows the CPU. Their golden digests pin the
  port's regions.
- DEFAULT replay from bundles: x64 168 keys, 151 PASS and 17 SKIP (every vector at 0x0f7f: 133 vectors, `SG_E_UNSUPPORTED` for the word), 0 FAIL
  (log/roadmap/S4/6_replay_x64.log); Win32 168 / 168 PASS with the 133 vectors run under 0x0f7f (6_replay_w32.log).
- Negative controls (`6_run.py negctl`, 6_negctl_r1.tsv): mode 1 of all 101 functions RED, 0 blind. x87 mode 2 (`6_run.py x87ctl`, Win32, 6_x87ctl.tsv):
  RED at the 0x0f7f vectors the word changes for all five functions (setRate 3 keys 1 vector each, recomputeVoiceGains and setReverseRate 3 keys 5
  vectors each, stepVolumeFade 2 vectors, resetDefaults r3 / r999 3 vectors each), equal at 0x027f (an equivalent mutant there: SSE2 doubles are exact
  at that word; recorded), the correct bodies PASS on all 40 keys. Voice::update modes 1..8 (6_negctl_vu_m*.tsv): 8 of 8 RED.
- Coverage of the ORIGINAL by the keys that judge the port (`6_cover.py`, 6_coverage.tsv): 4,956 of 5,035 instructions (98.4%), 415 of 459
  conditional jumps taken both ways; 88 of 101 functions fully executed; Voice::update 416 / 416 instructions, 51 / 54 jumps both ways (the
  three others are the redundant `cmp eax, ebx; je` after the registry search at 0x1fbc2 / 0x1fcbd / 0x1fd4a: eax cannot be 0 there). The 13
  functions with unexecuted instructions and the one-sided jumps are listed under "Thin" below.
- pytest: default `~/bin/py -3.12 -m pytest port/test/test_audio_chan.py` 264 passed, 286 skipped (168 + 101 oracle tests, 17 x64 keys judged on
  Win32) in 573 s (6_pytest_default.log); SG_EMU_ORACLE=1: 533 passed, 17 skipped (the 17 x64 keys judged on Win32) in 1,676 s (6_pytest_oracle.log).
- Segment gate x64 on the mirror (`integrator segments --dll port/build/S4_6_mirror/x64/Release/portsim_c.dll --jobs 3`, 6_segments_mirror.tsv / .log):
  205 / 205 PASS in 185 s, `integrator compare` 0 digests differ from log/p5/segs_x64_loc.tsv and from S4.2's 2_segments_mirror.tsv. (Win32 segment gate
  not re-run: the module is not reached by the sim; the Win32 build compiles it.)
- Boot gate on the mirror (`SG_PORTSIM_DLL=<mirror dll> port.test.boot --all --jobs 3`, 6_boot_gate_mirror.log): 66 scenarios, 61 PASS, 2 KNOWN, 1
  NO_FRAMES, 2 FAIL: cov_r_bio (-11 after 579 of 687 records, the same row as BASELINE / S1 / S4.2) and cov_r_flic_input, a scenario another step added to
  the shared corpus today (port/corpus/traces/cov_r_flic_input.trace, 09:48): it fails identically (record 1, g_cachedCameraTileX) on S4.2's mirror DLL, which
  has no audio_chan, so it is not this step's.
- test_static: `test_no_original_addresses_in_port_code` has no
  finding in audio_chan.cpp / audio_chan.h (it lists a sibling's audio_mixer.cpp); `test_every_module_file_is_listed` fails on audio_chan exactly as on
  audio_core until the merge request's one modules.inc line is applied.

## Callees owned by other steps (S4.2's development fallback, counted)

Layout A, oracle run of the final build: 337 fallback calls over the 168 keys (355 on the Win32 replay, which also runs the 0x0f7f keys). By callee:
Sound_setSourceType 0x2ad10 (S4.7) 46; CommandQueue::push 0xa240 (S4.8) 48; DsBuffer::setVolume 0xf1c0 28, DsBuffer::ctor 0xeef0 10, DsBuffer::stop 0xf760 8,
Ds3DBuffer::setPosition 0x11000 21, setPositionX 0x11070 24, setPositionY 0x110c0 24, setPositionZ 0x11110 24, CtrlObject3_ctor 0x1caf0 12 (all S4.8);
Mixer_ctor 0x2c9f0 (S4.4) 12; codecAllocDecoderContext 0x3aea0 (S4.3) 18; ehVectorConstructorIterator 0x42902 (no step row) 24 and the eh vector destructor
iterator 0x4280e (no row) 56. Gate: 0 fallbacks to S4.2 or S4.6 functions. Once an owner step registers its function the call is served by the port
(re-run the oracle then; the golden digests do not change because the host call and the port function return the same values by their own gates).

## Thin (coverage of the original, `6_coverage.tsv`)

Unexecuted instructions: ChannelTable_loadRange 0x1afd6 (the NULL result of operator new for a member: the original and the port both store through
NULL, the fixture never fails an allocation here), SubChannel_ctor 0x2b549-0x2b550, Ch5be88_ctor 0x2d4b9-0x2d4c0, Ch5c178_ctor 0x2e999-0x2e9a0,
buildVoiceRxName 0x2edbb-0x2edc2, buildVoiceTxName 0x2f28b-0x2f292 (the virtual call on the object just cleared at +0x40: dead, as the comments of the
hook sources say), Ch5bb90_dtor 0x2b6b7-0x2b6c0 and 0x2b6dc-0x2b717, Ch5be88_dtorA / dtorB 0x2d5c8-0x2d5e4 / 0x2e6c8-0x2e6e4, Ch5c178_dtor
0x2ea92-0x2ea9b, Ch5c234_dtor 0x2efa2-0x2efbe, Ch5c2ec_dtor 0x2f455-0x2f471 (the second unlink / delete blocks the compiler repeats after the first: dead
because the first cleared the fields), allocVoiceSlot 0x2d149-0x2d151 (the allocation failure arm). Jumps with one side only: the loops of
Voice_notifyChannelNotes, ChannelTable_loadRange / releaseObject, Voice_allNotesOff, Channel_closeAndFree, Channel_releaseBuffers, Channel_releaseVoices,
Channel_stopVoices, Channel_findStreamNode, Channel_stepVolumeFade and serviceChannelPlayback (one jump each at a count of 0 or the first element, the
full list is the last column of the TSV). None of these is reachable from the vectors; U-1455 records that the controls change results, not branches.

## Findings

- Voice::update computes `__ftol(X - [this+0x720])` into ecx on its timed path and never reads it there (ecx is read only when bit 1 of [this+0x58] is
  clear): a pure instruction group with no effect; the port keeps it (0x1f926-0x1f933).
- Unicorn keeps a signaling NaN signaling through fld / fst; the CPU and the live C3 A/B quiet it (U-1451).
- Unicorn's FPCW reads 0x023f where the fixture set 0x027f (bit 6 is reserved): the runner normalises it.
- Voice::update's playEvent call zeroes the [esp+0x14] slot through a `mov` that follows the two argument pushes (0x1fa6d: the address is the slot of
  the caller's frame, not the flag at [esp+0x1c]); a transcription that reads the offset against the wrong esp stores the zero into `found`.
- serviceChannelPlayback passes the address of its local `b` to duplicateBuffer and freeBufferById (U-1450); Channel_ctorB, VoiceBase_ctor and
  VoiceBase_dtor run the eh vector constructor / destructor iterators (0x42902 / 0x4280e) over their element arrays: no row owns them (fallback table above).

## Reproduce

```
~/bin/py -3.12 re/tools/snd_hook2port.py --step S4.6 --module audio_chan --compile --out log/roadmap/S4/6_conv/audio_chan.cpp
~/bin/py -3.12 log/roadmap/S4/6_assemble.py                                  # writes port/src/sim/audio_chan.cpp
log/roadmap/S4/6_cycle.sh --w32                                              # assemble + build x64 and Win32 in port/build/S4_6_mirror
SG_EMU_ORACLE=1 ~/bin/py -3.12 log/roadmap/S4/6_run.py oracle --update --out log/roadmap/S4/6_oracle_final.tsv
SG_EMU_ORACLE=1 ~/bin/py -3.12 log/roadmap/S4/6_run.py negctl --mode 1 --out log/roadmap/S4/6_negctl_r1.tsv
~/bin/py -3.12 log/roadmap/S4/6_run.py replay                                # x64 from bundles
~/bin/py -3.12 log/roadmap/S4/6_run.py replay --w32                          # Win32, x87 words applied
~/bin/py -3.12 log/roadmap/S4/6_run.py x87ctl --out log/roadmap/S4/6_x87ctl.tsv
SG_EMU_ORACLE=1 ~/bin/py -3.12 log/roadmap/S4/6_cover.py --out log/roadmap/S4/6_coverage.tsv
~/bin/py -3.12 -m pytest port/test/test_audio_chan.py                         # default, 573 s
SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_chan.py        # + the live oracle and its controls
~/bin/py -3.12 -m port.test.integrator segments --dll port/build/S4_6_mirror/x64/Release/portsim_c.dll --jobs 3 --out <tsv>
```
After the merge the mirror is unnecessary: `6_run.py` reads `DLL64` (port/build/S4_6_mirror/x64) and falls back to nothing, so point it at the
integrator's build by editing MIR at the top of the file (one line) or set SG_PORTSIM_DLL for the oracle / negctl commands.

## Open

- U-1450 frame cells (S4.10 decides the production frame), U-1451 emulator sNaN quirk, U-1452 Win32 live oracle, U-1453 Voice::update's fixture lives
  under log/ (a C3 round would move it under re/frida/fixtures.d), U-1454 fallbacks until the owner steps land, U-1455 controls vs branches.
- Voice::update's golden digests come from a fixture written for this step (no C3 path-1 evidence exists for it).

## CLAUDE.md paragraph (after the S4.2 paragraph)

```
Step S4.6, channels and voices (2026-10-24, module audio_chan, notes log/roadmap/S4/6_notes.md, U-1450..U-1455): the 101 rows of S4.6 ported against
S4.2's audio core (portsim/sim/audio.h): Channel_*, Voice_*, Ch5*, SubChannel, ChannelTable, ChanRecord, VoiceSlot, the source openers, the x87 rate /
fade / reset functions and Voice::update (written from its disassembly, judged on a fixture of this step: 34 rows x 9 registry-list variants, 416 / 416
instructions of the original executed). port/src/sim/audio_chan.cpp is assembled by log/roadmap/S4/6_assemble.py from the converter's output
(snd_hook2port.py --step S4.6) and hand blocks. Judge: log/roadmap/S4/6_run.py over sound_replay (own golden port/test/golden_audio_chan.json): 168 keys,
3,344 vectors, 156 GREEN + 12 X87_WIN32 + 0 RED in three layouts; mode 1 of all 101 functions RED. The five c3fh caller-word functions (x87 control word
0x027f / 0x0f7f) run their x87 instructions under the word of the call on Win32 (X80 helpers) and are judged there: the oracle stores Unicorn's FPCW per
vector in the bundle, the Win32 replay applies it (168 / 168 PASS, 133 vectors at 0x0f7f; mode 2, plain doubles, RED at 0x0f7f). Unicorn keeps a signaling
NaN signaling through fld / fst (the CPU and the live C3 A/B quiet it): 17 vectors are forgiven when the only difference is that quiet bit (U-1451).
Locals the original passes by address live in 16-byte cells in the image's tail padding (U-1450). Callees other steps own run through the development
fallback (337 calls by owner in the notes); 0 fallbacks to S4.2 / S4.6 functions. Coverage of the original by the judging keys: 98.4% of instructions.
```
