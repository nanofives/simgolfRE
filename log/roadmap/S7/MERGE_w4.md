# S7 wave 4 merge request: AUDIO OUT (steps S7.10, S7.11, S7.12)

Base: HEAD 37e58e96. Files: log/roadmap/S7/files_w4.txt (union of 10_ / 11_ / 12_files.txt plus the lead's logs). Every port
file of the list is the copy in port/build/S7_12_root, which the final gate ran on (byte-equal, checked by the lead). The shared
tree also holds OTHER sessions' uncommitted edits of port/app/host.cpp / host.h, port/test/test_app_{ingame,live,present}.py,
terrain_live.*, gl_raster.*, gfx_terrain.cpp (they undo parts of 37e58e96): none of them belongs to this merge, commit S7's
files by explicit path only.

## What it does
- `simgolf_port.exe` plays the game's sound. The sim context keeps the device-less sound path of the pinned corpus (GameState,
  trace and framehash unchanged); audio_exe's requests (Req depth 0: playSound, startSoundSlot, Snd::status, loadSoundTable,
  arrayStaticInit_448160) are copied to a SHADOW context on an audio thread (port/app/audio_out.*), which runs S4's
  device-present path (initSound(1, 0x8a), the timer client at its 30 ms period, THREAD_PRIORITY_TIME_CRITICAL as winmm's timer
  thread) into port/app/ds_sink.* (S4's DeviceSink answers, then real dsound.dll buffers, or a software capture).
- `--audio on|off|model|capture` (default on). No output device: the engine is told what a PC without one says (Initialize
  DSERR_NODRIVER ...), so its own initSound fails as the original's does (eax 3, g_soundReady 0) and the audio thread ends.
- `--profile original|fixed` (default fixed): the app applies re/port/defect_profiles.tsv (10 members with the new
  `sound.golfSfxAlias`, D54, Mariano's G15), records `profile` and every switch in session.json, and app_replay replays under
  the recorded switches. With fixed the app also runs tropical courses (flic.treeAliasNames 0); under original it faults as the
  stock game.

## Apply order (shared files; nothing else needs editing)
1. Copy the port files of files_w4.txt (all are S7-owned or step-owned: port/app/*, audio_exe.cpp / .h, port/test/app_replay.py,
   test_app_audio.py, test_defects_golfsfx.py, re/port/defect_profiles.tsv / defect_decisions.tsv (one row each: D54)).
2. port/test/integrator.py, ledger `tests` map, after the flic.treeAliasNames entry: the `sound.golfSfxAlias` entry in
   log/roadmap/S7/12_merge.md "Shared-file patches" 1 (exact text there).
3. log/roadmap/S6/merge_decisions.py: 12_merge.md patch 2 (reads log/roadmap/S7/12_decisions.tsv so a regeneration keeps D54).
4. UNCERTAINTIES.md: the rows of log/roadmap/S7/10_uncertain.tsv (U-1686..U-1692), 11_uncertain.tsv (U-1693..U-1699),
   12_uncertain.tsv (U-1607, U-1608, U-1627, U-1628, U-1638); append to U-1499 the sentence in 12_merge.md patch 3.
5. CLAUDE.md: the three paragraphs below, after "Roadmap S7.9".
6. Integrator builds: the new target `simgolf_audio_check` (port/app/CMakeLists.txt, library portapp_audio links dsound.lib /
   winmm.lib, both Windows system libraries) on x64 and Win32; test_app_audio.py finds it at port/build/<arch>/app/Release.
7. LAN staging: no change (U-1499 is handled by the switch; the staging rename of 11_merge.md was dropped).

## CLAUDE.md paragraphs (after "Roadmap S7.9", in this order)

Roadmap S7.10, audio out (2026-10-29, notes log/roadmap/S7/10_notes.md, U-1686..U-1692): `simgolf_port --audio on|off|model|capture`
(default on since S7.11) keeps the sim's device-less sound path (GameState unchanged: --live-trace sandbox_basic trace.trace and
framehash.bin equal with and without audio) and copies every request of audio_exe (Req depth 0, the new
`audio_exe_set_observer`) to a SHADOW context on an audio thread (port/app/audio_out.*): the exe's initialised data plus the
device-holder constructors (`audio_exe_construct_holders`), gameMain's initSound(1, 0x8a) before the first non-CRT request,
shutdownSoundSystem at the end, the timer client called at its 30 ms period on that thread. Its sink (port/app/ds_sink.*)
answers through S4's DeviceSink model (`DeviceModel`, `make_device_model`, `audio_exe_device_fixture`) and drives real
dsound.dll buffers (DSound) or a software capture (Capture). Gate (a): audio_stream.device_run's sequence through the app
sink gives $device 4b6ed61a x64 and Win32 (`simgolf_audio_check device`). Gate (c): a scripted title session's capture
against the install's WAVs (port/test/test_app_audio.py): clicks at volume / 127 and hard pan, static buffers at unity gain
then faded; controls unlock-skip and volume-ignored RED, pan / frequency equivalent (the engine sends no SetPan, SetFrequency
only at 22050 Hz). Finding: S4's model answers SetWindowLongA 0, so SoundWindowObj_subclass never kept the window and every
completion message went to hwnd 0 (a slot played once); the app sink answers a previous-procedure token and delivers the
messages to soundWindowSubclassProc. The real device was not available (DirectSoundCreate DSERR_NODRIVER, TV off, U-1691).

Roadmap S7.11, audio out edge cases (2026-10-30, notes log/roadmap/S7/11_notes.md, U-1693..U-1699): `--audio` defaults to
`on`. NO DEVICE: when DirectSoundCreate fails (or `--audio-no-device` / audio_check `--no-device 1` forces it) the sink answers
the engine's device probes as a PC without an output device does (scenario_baseline.md section 1: IDirectSound::Initialize
DSERR_NODRIVER, its Release 0, midiOutOpen / waveOutOpen errors), so the shadow's initSound takes the original's own failure
path (eax 3, g_soundReady 0, after exactly the measured probe order), its thread ends and the observer drops every later
request; the sim's trace.trace and framehash.bin equal `--audio off`'s. SHUTDOWN: the window session's WM_CLOSE stops the
shadow before DestroyWindow (gameMain's order); no thread id is left (Toolhelp32) and the handle count stops growing after a
process's first DirectSoundCreate. Pause (g_flags bit 2, key 0x50 at 0x0041d020) makes no sound call and focus muting is
DirectSound's own (the engine's buffers carry no GLOBALFOCUS / STICKYFOCUS, level DSSCL_PRIORITY), so neither needs code.

Roadmap S7.12, the course-sfx alias and the app's defect profile (2026-10-30, notes log/roadmap/S7/12_notes.md, U-1607 / U-1608 /
U-1627 / U-1628 / U-1638): switch `sound.golfSfxAlias` (audio_exe.cpp golf_sfx_name, decision G15, D54): at 1 the device sink's
mmioOpenA of the exe's `sounds\Golf sfx\` names fails on the extracted install's `Golf_Sfx` as the original's does (the CD header
iso\data1.hdr names it `Golf Sfx`; unshield wrote spaces as underscores), at 0 the install's Golf_Sfx file opens. Device run:
switch 1 = golden $device, switch 0 fcbc9195 (229 opens against 211) = an install with the exe's directory name line for line, x64
= Win32. `simgolf_port --profile original|fixed` (default fixed, compiled copy of defect_profiles.tsv, `--profile-list`); every
session.json records `profile` and every switch's value, and app_replay.gate_one replays under them. The tropical study trace runs
under fixed and faults under original (va 8, the stock game's). The same unshield rename hides six music files (U-1608).

## Rebase onto I19 (orchestrator 2026-10-30: base = c7412150 + S3 wave 7, the tree in port/build/I19)
- port/src/sim/audio_exe.cpp: P1 (c7412150) renamed 43 `snd.FUN_<rva>` keys. Ours rebased by a 3-way merge (base 37e58e96,
  ours, theirs c7412150; log/roadmap/S7/w4_rebase/): no conflict, the merged file differs from c7412150's by exactly our 210
  changed lines (sorted line sets equal), none of them holds a FUN_ name. The shared tree's audio_exe.cpp now holds that merge.
  I19's audio_exe.cpp / .h equal c7412150's.
- port/app/main.cpp: I19 (S3.20, renderer selection) changed it. 3-way merge onto I19's file, two conflicts, both resolved
  by keeping both sides (the usage comment: I19's `--gl` sentence, then S7.12's `--profile` lines; the option parser: S7's
  `--audio-no-device` / `--profile` / `--audio` branches, then I19's `--gl` branch). The result
  log/roadmap/S7/w4_rebase/main_merged.cpp differs from I19's by exactly our 217 changed lines. USE THAT FILE for
  port/app/main.cpp: the shared tree's main.cpp still holds ours on 37e58e96 (left untouched, I19 commits its own edits there).
- Every other file of ours is unchanged by c7412150 and I19 (compared with line endings normalised). test_app_ingame.py /
  test_app_live.py are not S7 wave 4 files (I19's versions are used as they are).
- Mirror: log/roadmap/S7/w4_mirror.py -> port/build/S7_w4_root = I19's port/ and re/model/include + our files; `diff -rq`
  against port/build/I19 shows only our 12 files. Builds x64 and Win32 (runner included): 0 warnings, 0 errors.

## Gate results
Pass 1, on port/build/S7_12_root (37e58e96 + S7.10..S7.12), log/roadmap/S7/12_g_gates.log, tables in 12_out/:
- (a) device run through the app sink in model mode = golden $device 4b6ed61a (13,020 lines) x64 and Win32 (test_app_audio);
  at sound.golfSfxAlias 0: fcbc9195... (14,309 lines, 229 mmioOpenA), equal line for line to an install with the exe's name.
- (b) segments 205 / 205 x64 and Win32 at switch 1 (0 digests changed against log/roadmap/S0/1_segs_x64.tsv) and 205 / 205
  at switch 0 (12_lead_segs_sw0.py); boot 66 / 2 / 2 x64 and Win32, 0 rows differ from log/roadmap/merges/I15; live
  equivalence 68 + 2 Win32, x64 67 + 2 + 1 FAIL (mood5_fixture: `missing: text font file` at its first record; alone it
  passes 245 / 245, 12_out/equiv_x64_mood5_rerun.tsv); app replay 3 / 3 x64 and Win32.
- app files, one per process, x64 / Win32: ingame 13 / 13, input 16 / 16, live 13 / 13, present 6 of 7 / 7, replay 14 / 14,
  shell 18 / 18, audio 28 / 28 x64 and 27 of 28 Win32 (a one-run handle count 3 above the audio-free reference; the
  assertion now allows HANDLE_SLACK 4, the leak proofs are the thread ids and the constant count over 4 cycles), defects
  (test_defects_golfsfx) 51 / 51.
- (c) title capture judged against the install's WAVs (volume / 127, hard pan, unity-gain music then faded), x64 = Win32 byte
  for byte; controls RED: unlock copy skipped, volume ignored, timer without user word, request slot dropped, constructHolders,
  no-device keep-thread (tool, app), leak-thread (tool, app), close-late, golfSfxName modes 1 / 2, profile-ignored, replay
  with every switch 1; blind: pan and frequency ignored (the engine sends no SetPan, SetFrequency only at 22050 Hz), an
  11 ms tick clock step.
- test_app_present::test_long_session_memory_bounded x64 reads 4,427,776 / 4,354,048 B against its 4 MB limit (private
  bytes after frame 2,000 over course_season_6y). Not audio: the same app run measured outside pytest grows 3,403,776 B
  with --audio off and 3,297,280 B with --audio on, and HEAD's own app (port/build/I15, no wave-4 code) 3,362,816 /
  3,334,144 B (log/roadmap/S7/11_lead_mem, 12_lead_mem_head). The limit was not changed; the test on plain I19 is in pass 2.

Pass 2, the merge gate, on port/build/S7_w4_root (I19 tree + S7 wave 4), log/roadmap/S7/w4_g_gates.log, tables in w4_out/:
- test_defects_golfsfx 51 / 51; app files x64 / Win32: ingame 14 / 14 (I19 added one), input 16 / 16, live 13 / 13, present
  7 / 7 (the memory test passes here: growth 3,457,024 B), replay 14 / 14, shell 18 / 18, audio 28 / 28, on both backends.
- segments 205 / 205 x64 and Win32 at switch 1 (0 digests changed) and at switch 0; boot 66 / 2 / 2 x64 and Win32 at switch 1
  and at switch 0 (12_lead_boot_sw0.py), 0 rows differ from I15's tables; app replay 3 / 3 (hole, report, saveload) x64 and
  Win32. Live equivalence was not re-run on I19 (the live core is not touched by wave 4; pass 1 ran it).
- test_long_session_memory_bounded is load-dependent: 4.43 / 4.35 MB under three heavy jobs (pass 1), 3.46 MB in pass 2,
  3.3-3.4 MB in single app runs of ours and of HEAD's own app. On PLAIN I19 (port/build/I19, no wave-4 code) the same test
  FAILS: 4,337,664 B (log/roadmap/S7/w4_head_present.log, w4_out/head_facts): a failure of the merge base under load, not
  of this merge. The limit was not changed (the orchestrator decides).

Commands (from port/build/S7_w4_root, after `~/bin/py -3.12 log/roadmap/S7/w4_mirror.py --build x64` and `--build Win32
--target portsim_c --target portsim_runner32 --target simgolf_port --target simgolf_audio_check`):
    sh log/roadmap/S7/w4_gates.sh defects_x64 app_<f>_x64 app_<f>_win32 ... segs_x64 segs_win32 segs0_x64 segs0_win32 \
       boot_x64 boot_win32 boot0_x64 boot0_win32 replay_x64 replay_win32      (f in ingame input live present replay shell audio)
Expected: the numbers above. The real-device listening check (gate d) is Mariano's: `simgolf_port.exe --install <dir>`
(audio on and profile fixed by default); with no output device the game runs silent, as the original does.

## S7.13 folded in (decision G16, 2026-10-30; notes log/roadmap/S7/13_notes.md, files 13_files.txt, already in files_w4.txt)
- The same switch sound.golfSfxAlias also opens the six theme music tracks the exe names under `sounds\music\Desert Music\`,
  `Links Music\`, `Tropical Music\` from the install's Desert_Music / Links_Music / Tropical_Music (audio_exe.cpp kDirAliases);
  the seven names wrong on the CD itself stay absent. port/src/sim/audio_exe.cpp and port/test/test_defects_golfsfx.py in
  the shared tree equal the S7_w4_root copies the gate ran on (line endings normalised); D54 (re/port/defect_decisions.tsv and
  its fragment log/roadmap/S7/12_decisions.tsv) and the defect_profiles.tsv reason carry G16; U-1608 resolved for the music.
- Device run at switch 0 re-pinned: 14,833 lines, 07f83f3f..., 235 mmioOpenA (24 through the alias), x64 = Win32; switch 1
  unchanged ($device 4b6ed61a). Gate (13_g_gates.log): test_defects_golfsfx 65 / 65, test_app_audio 28 / 28 x64 and Win32,
  segments at switch 0 205 / 205 and boot at switch 0 66 / 2 / 2 on x64 and Win32, 0 rows differing from I15. Control
  golfSfxName mode 3 (music aliases ignored) RED.
- CLAUDE.md: append to the S7.12 paragraph: `S7.13 (G16): the same switch opens the six theme music tracks under
  sounds\music\Desert Music / Links Music / Tropical Music from the install's underscore folders (kDirAliases); the seven names
  wrong on the CD stay absent; switch-0 device run 07f83f3f, 235 opens (24 aliased), x64 = Win32.`
- UNCERTAINTIES.md: U-1608 from log/roadmap/S7/12_uncertain.tsv now carries its S7.13 resolution.
