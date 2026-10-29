# S4 merge request: the resubmitted wave (steps S4.1 .. S4.11, with S4.2b and S4.9b), 2026-10-25, re-submitted 2026-10-26, third time 2026-10-29 (after I11)

Third resubmission, after I11's refusal (log/roadmap/merges/I11.md): S7 (d66aa755) split port/src/portsim_c.cpp into two
`extern "C"` blocks, so wave 1's end-of-file anchor `}  // extern "C"` occurred twice. 2b_apply_shared.py's i3_script() now relaxes
that one check of I3's script to sg_build_info's tail (`#endif`, `}`, blank line, the file's last `}  // extern "C"`) occurring once.
S4's 12-line C ABI block (sg_snd_set_host) therefore goes where it went on 78ffb06d and b3d61e41: after sg_build_info, at the end
of the file's last block (S7's second block, which also holds sg_raster_* and sg_build_info; the block choice changes nothing for
linkage). The proof was redone on HEAD e179bd24 by step S4.12 ("Resubmission proof" below); --prove reads SAME on all 8 files on
01d84188, e179bd24 and 6e6df6e7. Mirror port/build/S4r2; delete it, port/build/S4r and port/build/S4rx after the merge.

Second resubmission, after I8's refusal (log/roadmap/merges/I8.md, I8/buyland_win32.md). The three fixes I8 named:
1. port/test/test_buyland.py (new in this wave's files: S2 is done, S4 owns the edit) sets `audio_exe.stubRequests` on its states,
   as its oracle stubs playSound and as the seven other UI oracle files already do.
2. sg_state_free -> audio_release now calls the new audio_exe_release (audio_exe.cpp / audio_exe.h): the context's ExeCtx and
   DeviceSink are erased, so a context allocated at a freed context's address starts from no state. With the stub hidden,
   log/roadmap/S4/resub/buyland_reset_probe.py gives 81 of 232 differing in 4 of 4 Win32 runs and on x64 (I8 saw 0..81 on Win32
   and 0 on x64: x64 passed on inherited state). With the stub: 0 differing; test_buyland Win32 13 passed / 3 skipped in 6 of 6 runs.
3. 2b_apply_shared.py works on 78ffb06d: I3's script reads the wave-1 sections from log/roadmap/S4/wave1/ (I8's rebuild, copied),
   port/src/sim/modules.inc is edited as LF and written back with the checkout's CRLF, and MERGE.md's `S4 wave 2` CLAUDE.md
   paragraph is inserted by the script (I8's apply.py did it by hand). `--prove` now checks with log/roadmap/S4/resub/edit_equiv.py
   (I8's tool) that every edit equals the one S4 made on b3d61e41, line for line: all eight files SAME on 78ffb06d.
Proof: mirror port/build/S4r (log/roadmap/S4/resub/mirror.py = I8's, 86 paths), gates log/roadmap/S4/resub/gates.sh (I8's 23 steps
plus the 13 audio test files run whole on Win32); numbers in "Gate" below.

Supersedes the refused wave 1 (log/roadmap/merges/S4-3.md: NodeList_clear name collision). Every sound.dll function is now
registered as `snd.<hooks.csv name>`; port/test/test_audio_names.py refuses a collision with another registered name or an exe
vector stem. One step is held: S4.10g (U-1421, see "Open").

Files: log/roadmap/S4/files.txt (86 repo paths + the log/roadmap/S4/ directory; log/* is gitignored, so use `git add -f`).
port/corpus/audio and port/corpus/audio_exe hold the local, gitignored bundles. Regenerate them with `oracle --update` (Node + unicorn).

## Shared-file edits

All of them are made by ONE script, `log/roadmap/S4/2b_apply_shared.py <root>`. The authoritative list, deduplicated and in
apply order, is log/roadmap/S4/2b_merge.md section 1. The script refuses to run when an edit is already present.
`~/bin/py -3.12 log/roadmap/S4/2b_apply_shared.py --prove` applies the edits to a scratch copy of the tree's shared files and
checks each edit against S4's on b3d61e41 (port/build/S4_2b_full). Re-run 2026-10-26 on HEAD 78ffb06d and 2026-10-28/29 on
01d84188, e179bd24 and 6e6df6e7 (log/roadmap/S4/resub2/prove*.log): modules.inc, portsim_c.h,
portsim_c.cpp, tick.cpp, tick_boot.inc, UNCERTAINTIES.md (71 rows U-1418 .. U-1545), CLAUDE.md (plus the wave-2 paragraph) and
port/CORE.md all SAME; modules.inc keeps CRLF.
In short:
1. Wave 1 through I3's log/roadmap/merges/S4-3/apply_shared.py (one placement check relaxed): SG_MODULE(audio_core), the
   sg_snd_set_host C ABI, sg_state_free -> audio_release, UNCERTAINTIES U-1400..U-1417, the CLAUDE.md "Section S4" paragraph,
   port/CORE.md section 8.
2. modules.inc: audio_mixer, audio_chan, audio_ds, audio_dev, audio_codec, audio_seq, audio_sound, audio_wave, audio_soundfile,
   audio_crt, audio_exe; then touch registry.cpp.
3. tick.cpp kDrops: playSound, startSoundSlot and Snd::status removed. tick_boot.inc gets arrayStaticInit_448160, loadSoundTable and
   playSound(0x80, 0x64, 0, 0, 0) at the original's places. Steps 3 and 4 must go in TOGETHER: without step 4, every boot fails at record 0.
4. re/roadmap/S4_functions.tsv: S4.11's rows (a section file, in files.txt).
5. UNCERTAINTIES.md: 71 rows U-1418 .. U-1545 with statuses (2b_merge.md section 1 item 7; U-1545 from log/roadmap/S4/resub_uncertain.tsv).
6. CLAUDE.md: the `S4 wave 2` paragraph below, after the "Section S4" paragraph of step 1.
After the merge, delete port/build/S4_2b_mirror and port/build/S4_10_mirror in the shared tree, or set SG_S4_MIRROR to an empty
directory, so that sound_replay.py and audio_stream.py load port/build/x64 (2b_merge.md section 4).
Tracker (lead / orchestrator): 11_merge.md section 6 (re/names/sound_19.tsv to C1 / C2, ApplyNames on sound.dll, c2_note refresh).

## Gate (decision G1) and the numbers to expect

Per ported sound.dll function and per key: the fixture is replayed in S4.1's fixture VM, the ORIGINAL runs in Unicorn over the
user's sound.dll, and the port must equal it on the return, every compared region and the x87 exit state. The emulated original
is also tied to the path-1 CSV on every address-independent field. Default runs check golden digests.
Numbers (2b_notes.md "Phase 3 gate numbers", whole-tree mirror port/build/S4_2b_full on HEAD b3d61e41):
- sound_replay oracle: 1,087 keys / 16,859 vectors GREEN in layouts A / B / C, with 0 fallback calls and 0 GREEN_PENDING.
- audio_exe (S4.10): 182 keys / 1,861 vectors, 178 GREEN and 4 GREEN_PENDING on exe callees other modules own (return0_479820,
  NodeList / B / C::dtor).
- Goldens: twelve files equal the accepted ones key for key.
- Replay: x64 and Win32, 0 failures in every step.
- Controls: every mode-1 control RED (S4.2 61, S4.3 42, S4.4 38, S4.5 80, S4.6 101, S4.7 66, S4.8 116, S4.9 100, S4.9b 10, S4.11 28,
  S4.10 114). Sound5b6a8_ctor mode 2 and setVolume mode 3 stay BLIND, as at acceptance.
- Sink logs: 5 + 31 + 83 + 25 EQUAL.
- AudioSink streams: 205 / 205 segments, 4,964 requests, x64 = Win32. The device-present run gives 181 requests and 0 fallbacks,
  with the stream sha256 equal to the pinned one.
- Port suite: one file per process, 70 files. The 13 audio files are new; every other file equals I5's counts.
- Repo static 630 / 2. Segments 205 / 205 on x64 and Win32 with 0 digests changed. Boot: 0 baseline rows changed.
Resubmission proof on HEAD e179bd24 (2026-10-28/29, step S4.12, notes log/roadmap/S4/12_notes.md; I11 refused the apply on 01d84188,
log/roadmap/merges/I11.md). Mirror port/build/S4r2 = the e179bd24 tree + the 86 paths + `2b_apply_shared.py`: built as git archive of
01d84188 (log/roadmap/S4/resub2/mirror.py --dst S4r2) with port/build/I13's gfx_terrain.cpp, because 01d84188's copy did not compile
against its gl_raster.h (U-1546). e179bd24 commits that same file (md5 with CR stripped e88dfcab... in e179bd24, port/build/I13 and the
mirror) and changes nothing else under port, shim, scripts, tests, re or tools, so the mirror equals e179bd24 + S4
(port/build/S4r2/HEAD_COMMIT.note). Gates: log/roadmap/S4/resub2/gates.sh (I13's steps
plus resub's s4_* steps), the suites through resub2/suite_by_file.sh. Baselines: I13 (log/roadmap/merges/I13/), and I12 for the gates
I13 did not run (chains, defects, ledger, freetype).
- --prove: SAME on all 8 files on 01d84188 (the nine shared files are equal in e179bd24) and on 6e6df6e7; edit_equiv.py on S4r2: SAME
  on all 8.
- repo static 630 / 2 (HEAD 614 / 2 + S4's 16), 0 failed.
- segments 205 / 205 on x64, Win32 and freetype x64; 0 rows differ from I13 / I12; x64 = Win32 = freetype digests.
- boot x64 / Win32 / freetype x64: 70 rows 66 / 2 KNOWN / 2 NO_FRAMES each, 0 rows differ from I13 / I12 (no low-memory re-run needed).
- chains 342 / 342, defects 15 switches, ledger 30 lines with 1 row without a cause (showRoutingMapScreen.exitRemapReadsFrame): 0 rows
  differ from I12. gfx.check 0 duplicates / 131 bodies on both backends.
- frame census 698 rows PASS 100 (FAIL 537, UNSUPPORTED 51, NOT_RUN 10): none lost, none gained, total_px equal in all 688 judged rows.
  S3 terrain 527 PASS / 75 NOT_MEASURED, 0 verdicts differ.
- S7: live equivalence 68 PASS + 2 NO_FRAMES on x64 and Win32; app replay hole 340 / report 205 / saveload 266 on both; 0 rows differ
  from I13.
- port suite x64, one file per process with `-m ""`: 95 files (HEAD's 82 + 13 audio), 94 PASS; the failure is test_fttext with 1 of the
  2 U-1393 host GDI metric tests (I13: both). All six test_app_* files PASS (app at Normal priority). 79 files equal I13's counts; the
  13 audio files equal the 78ffb06d proof's; test_vectors (310 passed / 2,236 skipped) and test_win32 (185 / 1,116) are I13's plus S4's
  own additions (test_vectors' skips are exactly I13's 538 names plus S4's 1,698, each a registered name with no vector file).
- port suite Win32 on I13's 66-file list: 20 failed ids, every one in I10's 22 known ids, 0 new; the other 2 known ids (test_fttext's
  U-1393 pair) passed in this run. The 13 audio files under SG_BACKEND=win32: all PASS, counts equal to the 78ffb06d proof's.
  test_buyland alone on Win32: 13 passed / 3 skipped in 6 of 6 runs.
- G1: sound_replay oracle 1,087 keys / 16,859 vectors GREEN, 0 pending, 0 fallbacks (17 + 1 quirk-forgiven vectors); audio_exe 182 keys /
  1,861 vectors, 178 GREEN + 4 GREEN_PENDING; every per-key row equal to the 78ffb06d proof's. Twelve goldens EQUAL to 2b_accepted and
  to the shipped copies. Replay 0 failures in every step on x64 and Win32 (+ S4.4 Win32 soft). Controls: 669 functions, 667 RED, BLIND
  only Sound5b6a8_ctor m2 and DsBuffer::setVolume m3 (as accepted), every row equal to the 78ffb06d proof's; S4.10's 114 controls
  (test_audio_exe) all RED. Sink logs 5 + 31 + 83 + 25 EQUAL, the two sink controls DIFF 3 / 6. AudioSink streams 205 / 205, 4,964
  requests, the per-context counter = the segment's requests in 205 / 205 rows, x64 = Win32, 0 rows differ from the 78ffb06d proof.
  Device-present run 181 requests, 0 fallbacks, sha256 4b6ed61a... (= I8's and the 78ffb06d proof's). Win32 audio selections and the
  setVolume grid pass, every count equal to the 78ffb06d proof's.
- HEAD moved during the run: e179bd24 replaced gfx_terrain.cpp with I13's file (= the mirror's, so U-1546 is resolved), and 6e6df6e7
  added scripts/lan_release and 10 CLAUDE.md lines, which no gate reads. --prove on 6e6df6e7 is SAME on all 8 files, so the proof
  stands for 6e6df6e7 + S4.
- U-1398: the tracked log/roadmap/S7/8_*facts.json were restored after every app test run; `git status log/roadmap/S7/` clean at the end.
The earlier proof on 78ffb06d (mirror port/build/S4r) is kept in log/roadmap/S4/resub2/MERGE.old_proof_78ffb06d.md.
Command lines (2b_notes.md "Reproduce"):
```
~/bin/py -3.12 log/roadmap/S4/2b_apply_shared.py --prove
~/bin/py -3.12 -m pytest tests/test_fixvm.py                                  # 16
~/bin/py -3.12 -m pytest port/test/test_audio_names.py                        # 11
sh log/roadmap/S4/2b_suite_by_file.sh                                         # port suite, one file per process
bash log/roadmap/S4/2b_gates2.sh all                                          # every oracle / replay / control / sinklog
~/bin/py -3.12 log/roadmap/S4/2b_golden_compare.py
~/bin/py -3.12 -m port.test.integrator segments [--backend win32] --jobs 3 --out <tsv>
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out <tsv>
~/bin/py -3.12 log/roadmap/S4/resub2/mirror.py --dst S4r2          # mirror port/build/S4r2 (86 paths); on 01d84188 also cp port/build/I13/port/src/sim/gfx_terrain.cpp (U-1546; not needed from e179bd24 on)
~/bin/py -3.12 log/roadmap/S4/2b_apply_shared.py port/build/S4r2   # then build x64 + Win32 there (SG_FREETYPE_ROOT set, port/test/build.py)
~/bin/py -3.12 log/roadmap/S4/resub2/edit_equiv.py S4r2             # every edit = S4's on b3d61e41 (+ wave-2 paragraph, U-1545)
sh log/roadmap/S4/resub2/gates.sh <step>...                         # steps: see the file; suites: resub2/suite_by_file.sh
~/bin/py -3.12 log/roadmap/S4/resub2/compare.py segs|boot <be>|ledger|census|s7|terrain|streams|suite x64
~/bin/py -3.12 log/roadmap/S4/resub2/s4_totals.py; ~/bin/py -3.12 log/roadmap/S4/resub2/win32_compare.py
```
```
Lead's re-runs at acceptance: test_audio_core 143 (wave 1), chan 264, mixer 60 (x64 and Win32), seq 199, ds + dev 300, sound 189,
codec 111, soundfile 41, exe 302, crt 96, test_fixvm 16. The --prove re-run on 2026-10-25 is above.

## CLAUDE.md paragraph (after the "Section S4" paragraph that step 1 inserts)

```
S4 wave 2 (2026-10-24/25, steps S4.2b..S4.11, notes log/roadmap/S4/<k>_notes.md, U-1418..U-1499 and U-1540..U-1549): sound.dll's
engine is ported in eleven modules over the audio core: audio_mixer (the Voice_mix* leaves, renderers and streamRender at the
original's x87 word, Win32 real x87 and an x64 software 80-bit path that equals it on every vector and 60,000 probe operations),
audio_chan (channels, voices, Voice::update), audio_seq (sequencer, MIDI, tracks), audio_sound (Sound / MultiSound /
SoundGroup, create_sound, Engine_playSound), audio_ds (DirectSound as AudioSink commands, whose logs equal the import stand-in's)
and audio_dev (devices, Ctrl, command queues, SoundTimer, the GetProcAddress exports), audio_wave (wave-out / wave-in devices,
mmio RIFF readers over GameFiles, stream readers, decode rings), audio_soundfile (loadSoundFile, the Sound vtable's slot 4: no
user file uses its chunk format, so every path but the unknown-tag exit is judged on synthetic banks, U-1492), audio_codec (the
LPC-10 speech codec), audio_crt (the CRT helpers, so 0 development fallbacks remain) and audio_exe (the exe's Snd layer,
playSound / startSoundSlot / initSound / loadSoundTable on the port's engine). S4.2b consolidated the core: ONE stack mechanism
(SndFrame: a function's locals at the original's offsets below its entry esp), the virtual x87 unit as the core FPU model,
unclaimed `jmp [iat]` stubs routed as imports, SndResidue for words a caller pushes that its callee never reads, `snd.<name>`
registration (an unnamed function keeps Ghidra's FUN_<va> until it is named), and one front end, port/test/sound_replay.py
`--step S4.k`. Oracle (G1): 1,087 keys / 16,859 vectors equal to the emulated original with 0 fallbacks, and the twelve golden
files pinned. Segments run with the audio module attached and write an AudioSink request stream (205 / 205, 4,964 requests, every
digest unchanged). All 205 start images hold g_soundReady 0, so the requests are recorded at the exe's sound layer; a device-present
fixture run drives the engine end to end (181 requests, 0 fallbacks). Test-only switch `audio_exe.stubRequests` stands in for the
audio calls the UI oracles stub; sg_state_free (audio_release -> audio_exe_release) drops audio_exe's per-context state, which a
context allocated at a freed context's address used to inherit (I8: test_buyland's Win32 digests varied run to run). Unicorn rules folded into the oracle: the x87 exception flags are not set (searched), and a
signaling NaN copied through fld / fst is quieted by the CPU, not by Unicorn (U-1451, 17 + 1 vectors forgiven on the quiet bit
only). U-1421: the codec's caller control word is not measured. Three live runs with an audio device never entered the codec:
all 319 install sounds are PCM and openMmioSound / loadWaveResource are the network voice path (D6). The port keeps the codec at
0x027f. Findings in the original: renderChannelStreamed hangs on an underrun at the read limit; create_sound maps `.aif` to the
default; the MIDI loaders never check `MTrk`; loadSoundFile never leaves its chunk loop on a truncated file (U-1490); both
create_device exports report success after a failed allocation; Midi_Device::shutdown skips every second track.
```

## Open
- U-1421: settling it needs a recorded network voice session with a virtual wave-in device. Network play is a later section (D6),
  so S4.10g is held.
- U-1440 / U-1460 are partial: one frame API, but some cells are not at the original's offsets.
- audio_exe's 4 GREEN_PENDING keys wait on exe callees outside S4 (return0_479820, NodeList / B / C::dtor).
- U-1545: cov_r_flic_input's Win32 boot failed at record 0 (SG_E_NO_RESOURCE) in 2 of 42 runs of the resubmission build, both under
  8-10 GB of free commit; 205 later runs passed (that build 40, an instrumented copy 120, I8's mirror 45) and the re-run Win32 boot
  gate is clean. The missing name was not caught: port/build/S4rx (throwaway) logs it to log/roadmap/S4/resub/s4rx_missing.txt.
- U-1545 did not recur in the e179bd24 proof (boot Win32 66 / 2 / 2 on the first run).
- U-1546 (log/roadmap/S4/12_uncertain.tsv, status resolved): 01d84188 did not build from git archive (gfx_terrain.cpp held S3.10's
  rasteriser-thread calls); e179bd24 fixed it. Kept in S4's notes, not appended to UNCERTAINTIES.md (2b_apply_shared.py's rows are
  unchanged, so --prove stays line-for-line equal to S4's edits).
