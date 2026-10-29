# S4.9b notes: loadSoundFile (module audio_soundfile)

Step S4.9b of the roadmap orchestrator (worker brief log/roadmap/WORKER_BRIEF.md; row S4.9b of re/roadmap/S4_steps.tsv). The row S4.9 left
open (U-1487): sound.dll's `loadSoundFile`, rva 0x41d0, 3,762 bytes, slot 4 (+0x10) of the Sound vtable rva 0x5b000 that Sound::ctor
installs, __thiscall (this, name), `ret 4` (0x507f), an SEH frame. No hook source, no path-1 row: the oracle is the ORIGINAL in Unicorn
(decision G1). Uncertainty ids U-1490..U-1494 (log/roadmap/S4/9b_uncertain.tsv).

Status: DONE (gates in section 3; the segment and boot gates on the mirror in section 3 too).

## 1. What was built

- `port/src/sim/audio_soundfile.cpp`: the body, hand-written from the instructions (the disassembly is log/roadmap/S4/9_lsf_dis.txt; its
  stack-offset column is wrong after every stdcall `call ebx`, so every frame offset was recomputed from the pushes; section 2). Registered
  as `snd.loadSoundFile` (the naming rule of log/roadmap/merges/S4-3.md). Every call goes through the dispatcher by the original's thunk
  rva (snd_call / snd_vcall / snd_import), 31 callees; the six WINMM imports (mmioStringToFOURCCA, mmioOpenA, mmioRead, mmioGetInfo,
  mmioSeek, mmioClose) through their IAT slots. The locals the original passes by address live behind ONE helper, `SfFrame` (a wrapper
  over S4.8's SndLocals, for S4.2b to replace), at the original's offsets below the entry esp.
- Harness (log/roadmap/S4/): `9b_mirror.py` (the private build, frozen base: 9b_merge.md), `9b_snap/` (frozen copies of sound_replay /
  sound_emu, S4.2b rewrites the tree's), `9b_replay.py` (copy of 9_replay.py: snapshot import, golden, mirror, fixtures, `orig` command),
  `9b_keys.py` (25 keys), `9b_fixtures.js` (fixtures), `9b_cover.py` (branch coverage of the original), `9b_negctl.py` (all modes, bundles or
  live), `9b_dev.py` (one key with per-vector diffs).
- `port/test/test_audio_soundfile.py`, `port/test/golden_audio_soundfile.json`.

### Fixtures (9b_fixtures.js)

One generator `lsf(options)` per key: Sound objects built by the real Sound::ctor (rva 0x3f30) in a 0xb00-byte block each (create_sound
allocates 0xa60 for this class), their vtable pointer replaced by a fixture copy of the Sound vtable whose slot +0x14 is a recording stub
(answer `busy`); then the heap stand-in (operator new / malloc carve from a 0x40000 arena, delete / free only record, `failMask` refuses
chosen calls), the mmio stand-in (0x20000-byte block, op log 0x4000, import log 0x4000) serving the key's files, g_openMmioCount (rva
0xb4a08) swapped, the shared sound list (rva 0xb4fe8 .. +0xc, the list Sound_findByName / Sound_lookupOrCreate / Sound_reapIdle walk) swapped
for fixture entries, the module string at rva 0x69988 written for the prefix keys, the +0x218 range list of each Sound pre-built for the
success tail, pumpWaveMessages stood in when bit 6 of +0x214 is set. State regions (9b_keys.py): every Sound, the arena, the free log, the mmio
block and its import log, the stub log, the swapped words and every fixture node and record.

Files: synthetic bank files built from the layouts in section 2 (per layout, since a 'trak' payload carries a vtable address); MIDI images
of 1-3 tracks; the user's own files Sounds/Interface/Text Box.wav, Sounds/vssver.scc and Sounds/Effects/Sheep 2.wav, read from original\ by
9b_replay.py at run time (no file bytes in the repo; U-1492: the install holds no file in this format, these three take the unknown-tag exit).

Keys (25, 74 vectors): basic (11 files: plain MIDI, every chunk kind, unknown tags below and above 'ctrl', a kmap whose size cuts its record
loop, 0- / 1- / 3-track lists, a missing file), busy7 / busy0 (bit 0 of +0xac; slot +0x14 answering 7 / 0; busy0 also bit 6 of +0x214),
seekfail (mmioSeek stood in to -1), seeknomove (mmioSeek reporting success without moving, so Midi_Device::loadFromMemory reads the dword
after 'MThd' and returns 0x12), nopre (bit 0 of +0x58 cleared: Sound::ctor sets it), list (the shared list holding the kmap names), pre_g0..g5
(the path-prefix arms: own names with and without backslashes, the module string unset / equal / different at an even and an odd index /
equal over an odd length), oom0..2 and nullA..C / nullU / sfnull(U) / snmnull(U) (allocation failures: NULL track objects in each list drain,
a NULL NodeB000, a NULL VoiceSlot), user.

## 2. The function (from the instructions)

Frame: entry esp E; `push -1; push handler; push fs:[0]` (0x41d0-0x41dd), `sub esp, 0x40c` (0x41e5), four pushes: the chunk loop runs at
E - 0x428. Locals (offset below E, `[esp + n]` at E - 0x428): MMIOINFO 0x54 (+0x3d4), prgm path buffer 0x154 (+0x2d4), kmap path buffer
0x254 (+0x1d4), name buffer 0x358 (+0xd0), CtrlObject3 0x390 (+0x98), ChanRecord 0x3b8 (+0x70), kmap record dwords 0x3c0 / 0x3bc, the
'MThd' four-character code 0x3c4, the kmap trailing dword 0x3c8, the tag 0x3cc, the 12-byte glbl local 0x3d8, kmap count 0x3dc, prgm
length 0x3e0, Seq_listGetAt's out slot 0x3e4, the prgm flag byte 0x3e8 (passed to Seq_setupChannel as a whole dword, 0x4c3d), glbl length
0x3ec, kmap record size 0x3f0, &this+0x948 0x3f4, new-result / kmap index 0x3f8, consumed 0x3fc, VoiceSlot 0x400, MixBuffer 0x404, chunk
size 0x408, the local list {head, tail, +8, count} 0x418.

Flow: fourcc("MThd") (0x41f9, the string at rva 0x63d90); CtrlObject3_ctor / ChanRecord_init / the 0x2e3c0 initializer on the locals; bit 6
of +0x214 pumps the sound window (0x425f); bit 0 of +0xac asks slot +0x14 and a non-zero answer is returned (0x427d); the name is copied to
+0x948 and opened with mmioOpenA(name, NULL, 0) (0x4334): NULL returns 8, else g_openMmioCount += 1 and the loop reads a tag (0x43e8):
- 'MThd': mmioSeek(h, -4, SEEK_CUR) (0x4c8d); -1 returns 8 without closing the file or lowering the count; else
  Midi_loadMemoryAndPreRoll(h, name) (0x4df9): non-zero closes, lowers the count, clears +0xa48 and the 0x100 name bytes, returns 0x12;
  zero closes, lowers, clears +0xa48, pops the track list applying Seq_applyTrackFlag and slot 0 (1) to each object until a NULL one,
  processWaveChunks, walks the +0x218 records marking track+0x458 of each index in [rec+8, rec+0xc] that Seq_listGetAt finds, returns 0.
- 'ctrl': Wave_Device::readFmtChunk (S4.9). 'chnl': size + bytes over the CtrlObject3 local, Seq_copyChannelParams, mmioGetInfo.
- 'trak': a Ctrl5b668 (0x34) read over from +0 with the chunk size (vtable slot included, 0x459d) and appended in a {0, 0, obj} node.
- 'glbl': size, n, n bytes over the 12-byte local, Voice_setPlayModeBits(local+8), a NodeB000 read local+4 times (decremented in place to
  0xffffffff) for Seq_buildTrack14, freed.
- 'kmap': a MixBuffer (0x21c): +0x208 / +0x210 / +0x20c, a name (Voice_setName), a count, the record size when +0x208 & 6, a VoiceSlot
  (0x110) refilled per record ({d1, d2}: +0 = max(d1, d2) unsigned, +4 = d1; or record-size bytes), each record's name (optionally
  prefixed), the slot's name copied to +0xc, allocVoiceSlot unless Sound_findByName finds the MixBuffer's name; the loop stops when the
  consumed count (from 0xd, the count dword not counted) reaches the size; a trailing dword to +0x218 when 4 bytes are left; slot +0xd8
  (Sound_lookupOrCreate), the MixBuffer's slot 0 (1), Sound_reapIdle.
- 'prgm': a size of 0x1c reads n, 0x18 bytes over the ChanRecord (its +4 low byte |= 0x10, &= ~8) and n name bytes and sets the flag byte;
  any other size reads 0x1c bytes and a NUL-ended name (after mmioGetInfo); the name is optionally prefixed; Sound_setName; an alternate
  name when consumed < size (Sound_setAltName); Seq_setupChannel(rec, flag dword), Seq_updateChannel, both names cleared, 0x28 bytes zeroed.
- anything else: mmioClose, the count lowered, the list drained, 8 (+0xa48 keeps the closed handle).
The path prefix (bit 0 of +0x58, 0x480f / 0x4ac5): the Sound's own name cut after its last backslash (none: empty); a name with a backslash
whose prefix differs from the string at rva 0x69988 gets prefix + the name from its backslash on, else prefix + the whole name.

## 3. Gate numbers (mirror port/build/S4_9b_mirror)

| Gate | Result | Log |
|---|---|---|
| live oracle, 3 layouts | 25 keys, 25 GREEN (74 vectors, B and C 148), 0 errors; fallbacks only to rows no step owns (owner `unlisted` 0x2e3c0 / 0x5f40 / 0x2cf50, CRT `-`) | 9b_oracle_final.log / .tsv |
| default replay x64 / Win32 | 25 / 25 PASS / 25 / 25 PASS | 9b_replay_x64.log, 9b_replay_w32.log |
| negative controls vs goldens (bundles) | 10 / 10 RED (vectors differing: 74, 7, 23, 5, 5, 5, 35, 9, 24, 10) | 9b_negctl_replay.log / .tsv |
| negative controls, live oracle (layout A) | 10 / 10 RED (74, 7, 23, 5, 5, 5, 35, 8, 24, 10) | 9b_negctl_live.log / .tsv |
| sinklog (mmio import commands vs the original's import logs) | 25 / 25 EQUAL, 3,744 import commands, 0 records from host-run code | 9b_sinklog.log |
| branch coverage of the original | 1,111 / 1,156 instructions (96.1 %); 101 jcc: 88 both sides, 7 one side, 6 never (all structurally unreachable, section 4) | 9b_coverage.log / .tsv |
| pytest port/test/test_audio_soundfile.py | 41 passed, 25 skipped (the SG_EMU_ORACLE keys); port/test/test_static.py: 5 passed, 1 failed (test_every_module_file_is_listed: the shared tree holds every sibling's unmerged module file, audio_* among them; not this step's check) | 9b_pytest_default.log |
| segment gate x64 / Win32 | 205 / 205 PASS x64 and Win32; every digest equal to S4.9's mirror runs (9_segments_mirror*.tsv), course_season/04 included (S4.9's x64 row was its one transient font-read failure; here PASS with ebfabc27a284c0c0, the digest S4.9's re-run gave) | 9b_segments_mirror*.log / .tsv |
| boot gate | 70 scenarios: 63 PASS, 2 KNOWN, 2 NO_FRAMES, 3 FAIL; the 67 S4.9 ran have the same status as in 9_boot_gate_mirror.tsv (62 / 2 / 1 / 2: cov_r_bio, cov_r_flic_input); the 3 new shared-corpus recordings are cov_a_quit PASS, cov_a_quit_title NO_FRAMES and cov_r_s2_screens FAIL (an S2 recording that stops at the unported showBuyLandScreen) | 9b_boot_gate_mirror.log / .tsv |

Equivalent mutant found (blind, replaced): mode 3 first left the local list count (E - 0x40c) unraised: 0 / 74 vectors differ, because
that count is only ever decremented (the inline pops at 0x42b8, 0x4cd7, 0x4d75, 0x4e6f, 0x4f32, 0x4ffc ..., and listPopFront at
0x5c39, which receives the whole header) and never tested; it lives in the frame, outside every compared region, so no observable can
judge it. Mode 3 is now
"the new node holds NULL instead of the object" (23 / 74 RED).

## 4. Coverage: the sides no key reaches, and why

- 0x428d / 0x4296 / 0x42bf / 0x42cc and 0x4356 / 0x435f / 0x4388 / 0x4395 (spans 0x428f-0x42cc, 0x4358-0x4395): the list drains of the
  busy exit (0x427d) and the open-failure exit (0x4342) run before the chunk loop, when the local list is always empty (only 'trak' fills it).
- 0x481f / 0x4ad5: the pointer at E - 0x3f4 is this + 0x948 (0x4330), never NULL.
- 0x4627 (0x4634) / 0x49cd: a NULL MixBuffer: the next mmioRead targets address 0x208 and Voice_setName writes NULL + 0x204, so the
  original faults before the other side.
- 0x4fad: the +0x220 cursor is written at 0x4f5c and 0x4fb4 only and the record loop runs only with it non-NULL.

## 5. Findings (the original; the port keeps them)

- mmioRead results are never tested: a file that ends inside the chunk loop re-runs its last chunk forever (U-1490).
- A 'trak' chunk reads over the whole new object, vtable pointer included (0x459d), and the success path calls through it (0x4f49): the
  format stores a pointer that is valid only for the build and base that wrote it.
- The seek-failure exit (0x4c96) returns 8 with the file still open and g_openMmioCount raised; the unknown-tag exit leaves the closed handle
  in +0xa48.
- A 'prgm' of size 0x1c with a name of fewer than 4 bytes always reads an alternate name (consumed n + 0x18 < 0x1c), whatever follows.
- The kmap consumed count starts at 0xd and never counts the record-count dword.
- glbl's record count is decremented in place and stays 0xffffffff in the local afterwards: a later glbl with n < 8 runs its record
  loop 2^32 times (the keys always give n = 12).
- Sound_findByName is called with ecx = this (0x4956) but never reads it (it is registered stdcall, 1 slot).

## 6. Open items

- U-1491: the type-3 'ctrl' arm under loadSoundFile (a callee's uninitialised frame dword) waits for nested frames at the original's offsets.
- U-1493: four unnamed callees run as development fallbacks (merge request section 3).
- U-1494: production mmio names from GameFiles is the AudioSink host's (S4.10).
- After S4.2b: point 9b_replay.py at port/test/sound_replay.py, replace SfFrame, re-run `oracle --update` and the gates.

## 7. Reproduce

```
~/bin/py -3.12 log/roadmap/S4/9b_mirror.py --w32
SG_EMU_ORACLE=1 ~/bin/py -3.12 log/roadmap/S4/9b_replay.py oracle --update --out log/roadmap/S4/9b_oracle_final.tsv
~/bin/py -3.12 log/roadmap/S4/9b_replay.py replay [--w32]
~/bin/py -3.12 log/roadmap/S4/9b_negctl.py --out log/roadmap/S4/9b_negctl_replay.tsv
SG_EMU_ORACLE=1 ~/bin/py -3.12 log/roadmap/S4/9b_negctl.py --live --out log/roadmap/S4/9b_negctl_live.tsv
~/bin/py -3.12 log/roadmap/S4/9b_replay.py sinklog
~/bin/py -3.12 log/roadmap/S4/9b_cover.py --out log/roadmap/S4/9b_coverage.tsv
~/bin/py -3.12 -m pytest port/test/test_audio_soundfile.py
~/bin/py -3.12 log/roadmap/S4/9b_replay.py orig [--keys k ...]      # development: the original alone
~/bin/py -3.12 log/roadmap/S4/9b_dev.py <key>                        # development: one key with per-vector diffs
sh log/roadmap/S4/9b_gates.sh                                         # segment (x64, Win32) and boot gates on the mirror
```

## 8. CLAUDE.md paragraph (for the lead to merge)

Step S4.9b (2026-10-24, sound.dll's loadSoundFile, log/roadmap/S4/9b_notes.md, U-1490..U-1494): module audio_soundfile, rva 0x41d0 (Sound
vtable 0x5b000 slot 4), registered as `snd.loadSoundFile`, written from the instructions (no hook source, no path-1 row) and judged against
the original in Unicorn on 25 keys / 74 vectors (synthetic bank files with every chunk kind 'ctrl' 'trak' 'glbl' 'chnl' 'kmap' 'prgm' and
'MThd', allocation failures through the heap stand-in's failMask, stood-in mmioSeek results, the user's own Sounds files): GREEN on three
layouts, replay 25 / 25 x64 and Win32, 10 / 10 negative controls RED, mmio import commands equal to the original's logs, 88 of 101 jcc both
sides (the 13 others structurally unreachable). The user's install holds no file in this format (every sound file is RIFF / FORM; U-1492).
Harness pitfall: a callee's uninitialised frame dword (readFmtChunk's type-3 record) differs when the port's nested frame is not at the
original's stack address (U-1491).
