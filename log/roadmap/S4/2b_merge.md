# S4.2b merge request: the resubmitted S4 wave (S4.1 .. S4.11)

One request for the whole section: S4.1, S4.2, S4.2b, S4.3, S4.4, S4.5, S4.6, S4.7, S4.8, S4.9, S4.9b, S4.10, S4.11. It folds every
step's merge request (MERGE.md for wave 1, 3_ .. 11_merge.md) into one deduplicated list, in apply order. Every shared-file edit is
made by ONE script, and the mirror build the gates ran on takes its shared files from that same script:

```
~/bin/py -3.12 log/roadmap/S4/2b_apply_shared.py <root>      # apply to a tree (refuses if an edit is already present)
~/bin/py -3.12 log/roadmap/S4/2b_apply_shared.py --prove     # apply to a scratch copy of the tree's shared files (each unmodified
                                                             # against HEAD b3d61e41), compare with the mirror build's copies:
                                                             # modules.inc, portsim_c.h, portsim_c.cpp, tick.cpp, tick_boot.inc EQUAL
```

The whole-tree mirror `port/build/S4_2b_full` (log/roadmap/S4/2b_fulltree.py --fresh: `git archive HEAD` + the S0 export junctions
plus shim/build, re/replay and extracted + the wave's files + this script) is where the port suite, repo static, segments and the
boot gate ran.

## 1. Shared-file edits, deduplicated, in apply order (all made by 2b_apply_shared.py)

1. Wave 1 (S4.1 + S4.2): I3's `log/roadmap/merges/S4-3/apply_shared.py`, run as is except one check: HEAD b3d61e41 put the S2 / S3
   paragraphs between S0's and "Phase 7 defect decisions" (S6), the anchor I3 inserts before, so its assertion that the paragraph
   before the anchor is S0's also accepts S2's / S3's. The S4 paragraph still lands in section order (S0, S2, S3, S4, S6). It makes:
   - port/src/sim/modules.inc `SG_MODULE(audio_core)`, port/src/registry.cpp touched;
   - port/include/portsim_c.h: 2_cabi.h.txt before `sg_build_info`;
   - port/src/portsim_c.cpp: `#include <cstddef>`, `#include "portsim/sim/audio.h"`, sg_state_free with `audio_release`, 2_cabi.cpp.txt;
   - UNCERTAINTIES.md U-1400 .. U-1417; CLAUDE.md "Section S4, audio engine model"; port/CORE.md section 8.
2. port/src/sim/modules.inc after audio_core, in this order, then registry.cpp touched:
   ```
   SG_MODULE(audio_mixer)
   SG_MODULE(audio_chan)
   SG_MODULE(audio_ds)
   SG_MODULE(audio_dev)
   SG_MODULE(audio_codec)
   SG_MODULE(audio_seq)
   SG_MODULE(audio_sound)
   SG_MODULE(audio_wave)
   SG_MODULE(audio_soundfile)
   SG_MODULE(audio_crt)
   SG_MODULE(audio_exe)
   ```
3. C ABI and sg_state_free: nothing beyond wave 1 (the only per-context sound state is the core's; `audio_release` frees the x87
   unit). port/CMakeLists.txt: no change (it globs src/sim/*.cpp).
4. port/src/sim/tick.cpp, kDrops: S4.10's three lines removed (`playSound`, `startSoundSlot`, `Snd::status`).
5. port/src/sim/tick_boot.inc (with 4, never alone: 10_merge.md 2b): `cc("arrayStaticInit_448160")` after
   `cc("staticInit_820f04")` in BootCrt, `cc("loadSoundTable")` after `cc("showTitleScreen")` in BootGameMain, the `ML+0x2f playSound`
   comment replaced by `cc("playSound", 0x80, 0x64, 0, 0, 0)` in BootSetup, and BootGameMain's comment `loadSoundTable (gameMain+0x42e)
   and the colour cycle are not the boot's.` -> `the colour cycle is not the boot's.`.
6. re/roadmap/S4_functions.tsv (the lead's table, a wave file): S4.11's nine rows `-` -> `S4.11` (operator_delete, strstr,
   __ArrayUnwind, ehVectorConstructorIterator, heap_alloc, free, crtRealloc, msize, _unlock) and its nineteen new rows (11_merge.md
   section 4, texts in the script).
7. UNCERTAINTIES.md: 70 rows U-1418 .. U-1544 appended in this order: 2b_, 3_, 4_, 5_, 6_, 7_, 8_, 9_, 9b_, 10_, 11_uncertain.tsv
   (four-column files 9_ / 9b_ / 11_: empty path-to-resolution, status `open`; U-1493's `|=` escaped). Status by the wave:
   - resolved: U-1420, U-1425, U-1435, U-1444, U-1450, U-1452, U-1470, U-1472 (S4.2b), U-1493 (S4.11), U-1494 (S4.10), and the
     wave-1 rows U-1411, U-1415, U-1416 (S4.10's texts, written over their `open`);
   - partial: U-1440, U-1460 (one frame API, cells not at the original's offsets), U-1462 (clock VM in sound_replay);
   - U-1421 stays open with S4.10's call-graph text added to its path-to-resolution.
8. CLAUDE.md step paragraphs: not applied by the script (the lead chooses). Each step's merge request has one (3_ .. 11_merge.md);
   S4.2b's (2b_notes.md, end) supersedes what the S4.4 / S4.6 / S4.8 ones say about frames, the x87 unit's location and the runners.

## 2. Edits of the per-step requests that are in the wave's own files (done in the tree, not shared)

- 10_merge.md section 3 and 11_merge.md section 2 (port/src/sim/audio_core.cpp): `audio_crt_function_name` and
  `audio_exe_sound_name` declared and consulted in `audio_crt_name`.
- 10_merge.md section 4 (port/src/sim/audio_wave.cpp), applied with one correction. setRegionMs gets the stacked 0xe (and 8) as its
  second argument, as requested. But dropping the pushed word from the slot-0x40 call and ecx from flushSoundList's five calls made
  six S4.9 keys RED (closeOutput_all / _part, restart_ok / _ok0, parseRiffWave_big, writeWaveFile_empty: the fixtures' recording
  stand-ins log the stack slots and ecx the original leaves there, 0x0e / 0x08 / the writer object). The words go back, inside the
  core's new SndResidue scope (audio.h: a port callee of a smaller arity is entered anyway and ignores them; outside a scope an arity
  mismatch still throws), so the stand-ins see them and the device-present run's arity check passes. 13 sites; the six keys GREEN.
- 11_merge.md section 3 (port/test/sound_replay.py): the two `sound_keys_crt.install` lines at the end.
- S4.10's audio_exe.cpp, now owned by S4.2b for the merge: the image size constants written as shifts (`0x445u << 12`,
  `(0x44u << 16) | 0x414eu`) and the comment's image base by name (test_static's address rule); its 74 sound.dll functions keep
  `snd.FUN_<va>` under the rule below.
- Other modules' tests the kDrops removal touches (the full suite found them; S4.10's gates did not run them): test_commands,
  test_dialogs, test_chareditor, test_chareditor_bio, test_reports, test_events and port/test/liveraw.py set the new test-only
  switch `audio_exe.stubRequests` on every state they create (their oracles stub playSound / startSoundSlot / Snd::status);
  test_bridge attaches original/ for the real playSound; test_framegate's kDrops table check updated. audio_exe.cpp:
  SndGroup_isDisabled / isMuted registered ret `uint8` (the corpus and registry interface). Goldens unchanged.
- NAME RULE for an unnamed sound.dll function (U-1498): registered `snd.FUN_<va>` where FUN_<va> is exactly its name in the master's
  listing re/functions_ghidra_sound.dll.tsv (Ghidra's default, va = 0x10000000 + rva), allowed only while hooks.csv, re/names and
  S4_functions.tsv have no name at that rva; once it gets one, the FUN_ form fails port/test/test_audio_names.py (controls: a FUN_
  name for GlblHeader::init 0x2e3c0 and for an rva that is not a function entry are both refused).
- NOT applied, replaced by S4.2b: 4_merge.md sections 2 / 3, 8_merge.md section 2; done by S4.2b: 3_merge.md 3a / 3b, 5_ / 6_ /
  7_merge.md section 3, 8_merge.md sections 3-4, 9_merge.md section 3, 9b_merge.md section 3.
- Retired runners (their tables are in sound_replay.py): 3_run.py, 5_run.py, 6_run.py, 7_run.py, 8_replay.py, 9_replay.py,
  11_replay.py; 9b_replay.py forwards to `sound_replay --step S4.9b`. Never re-run 3_ / 6_ / 7_assemble.py. S4.10's
  port/test/audio_stream.py stays the exe side's runner (its own keys, streams and device run).

## 3. Goldens and bundles

11_merge.md section 5's re-pin was done on the final build by `2b_gates2.sh oracle` (every step `oracle --update`, S4.11 included),
not by copying 11_goldens/. 2b_golden_compare.py checks every golden file (twelve: core, chan, ds, dev, mixer, codec, seq, sound,
wave, soundfile, crt, exe) against the accepted copies in 2b_accepted/: digests, tiers and regions equal key for key; only the
`fallbacks` / `pending` fields change. Bundles (port/corpus/audio, audio_exe; gitignored, format 2) are regenerated by
`oracle --update`.

## 4. Defaults to flip at the merge

port/test/sound_replay.py `MIRROR` (port/build/S4_2b_mirror) and port/test/audio_stream.py `MIRROR` (port/build/S4_10_mirror): once
the modules are in the integrator's build, delete the mirrors or set SG_S4_MIRROR to an empty directory; both then load
port/build/x64 (the whole-tree run used exactly that).

## 5. Tracker (lead's, not run by this wave)

11_merge.md section 6 (re/names/sound_19.tsv batch to C1 / C2, ApplyNames on sound.dll, the c2_note refresh) as written there.

## 6. Files

The union of the non-log paths of log/roadmap/S4/{files,1_ .. 11_,2b_}files.txt (2b_fulltree.py copies exactly that set).

## 7. Gate command lines and numbers

2b_notes.md "Reproduce" and "Phase 3 gate numbers".
