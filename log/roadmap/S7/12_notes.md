# S7.12 the course-sfx alias and the app's defect profile (2026-10-30, claude2 opus; row S7.12 of re/roadmap/S7_steps.tsv)

Decision G15 (Mariano, 2026-10-30, re/roadmap/DECISIONS_PENDING.md): the port finds the course effects under both folder names,
behind a defect switch in profile `fixed`, used by the app; install files untouched; simulation unchanged. U-1607, U-1608,
U-1627, U-1628, U-1638 (log/roadmap/S7/12_uncertain.tsv). Mirror log/roadmap/S7/12_mirror.py: port/build/S7_12_root =
`git archive 37e58e96 port re/model/include` + S7.10 / S7.11 / S7.12's owned files, with junctions to original, extracted, log,
scripts, port/corpus, tests/golden, every re/* directory but re/model, every re/model entry but include (re/port included, so
the TSV rows are read from the shared tree), copies of re's top-level files and pytest.ini. x64 in port/build/x64, Win32 app in
port/build/w32, the Win32 runner (port/test/build.py --arch Win32) in port/build/Win32, all inside the mirror; /W4 /WX clean.

## 1. The facts
- golf_clean.exe holds 18 course-effect names: 17 `sounds\Golf sfx\<file>` at 0x004d1da0 .. 0x004d2020 and
  `sounds\golf sfx\Cart Away.wav` at 0x004d104c (pefile over original\golf_clean.exe; list in test_defects_golfsfx.py SFX).
  original\sounds\Golf_Sfx holds 26 files, all 18 among them; there is no `Golf sfx` directory.
- The CD's InstallShield header iso\data1.hdr names the directory `Golf Sfx` (one occurrence, file offset 586212; no `Golf_Sfx`).
  unshield's extraction (extracted\cab\Program_Files_(ENGLISH)\Sounds\Golf_Sfx, copied to original\) writes spaces in directory
  names as underscores. G15's wording "the stock install has Golf_Sfx" holds for this extracted install; an InstallShield
  install would hold `Golf Sfx`, which the exe's name matches on NTFS (U-1607, not run).
- Where a sound file name meets GameFiles: only the device sink's mmioOpenA (audio_exe.cpp DeviceSink::mmio) and deviceStep 3's
  slot check (`present`). The app's DsSink sends every command to that model first, so the app reaches the same lookup. Nothing
  else in the sound path opens a file by name (DeviceSink answers only the 12 WINMM mmio imports with names; loadSoundTable
  pushes the paths from its own instructions; the sim's context runs the device-less path, g_soundReady 0 in every segment).

## 2. What was built
- port/src/sim/audio_exe.cpp: `golf_sfx_name(files, name)`: at switch 1 the name unchanged; at 0, when the name is absent and is
  `sounds/<Golf sfx>/<rest>` (both components compared case-insensitively), `sounds/Golf_Sfx/<rest>` if it exists, else the
  install's own spelling found through GameFiles::list (a case-sensitive host). mmioOpenA opens the result; its line keeps the
  exe's name and adds ` (alias <name>)` only when they differ, so every line at switch 1 is unchanged. DefectDef
  `sound.golfSfxAlias` (default 1) on kModule_audio_exe; probe entry `audio_exe.golfSfxName` ("$audio_exe/sfxname" ->
  "$audio_exe/sfxname.out", eax 1 when the file exists) with negctl g_neg_golfSfx: 1 = the directory compared case-sensitively,
  2 = deviceStep 3's slot check ignores the alias. DeviceModel counts file_present / file_absent / file_aliased (audio_exe.h).
- port/app/audio_out.*: AudioOutStats files_present / files_absent / files_aliased from the model at the thread's end;
  main.cpp writes them into audio.json.
- port/app/main.cpp: `--profile original|fixed` (default fixed) applied before any run (every switch reset to 1, then the
  members); `--profile-list <p>` prints every switch with the value p gives; kFixedProfile is the compiled copy of
  re/port/defect_profiles.tsv (10 members with this one); every session.json (live, live-trace, shell / present-trace) gets
  `"profile"` and `"defects"` (every switch and its value). Control `--negctl profile-ignored` records the profile, sets nothing.
- port/test/app_replay.py: `session_switches` / `apply_switches`; gate_one replays under the switches the recording's
  session.json holds (none = every switch 1, which is what the app ran before S7.12, so every existing recording replays as
  before), `switches=` overrides (the control), every switch back at 1 afterwards.
- re/port/defect_profiles.tsv row `sound.golfSfxAlias 0 G15`; re/port/defect_decisions.tsv row D54 (wrong-value, all effect
  columns no, recommendation fix, decision `G15 fix (S7.12)`) from log/roadmap/S7/12_decisions.py --apply.

## 3. Gate (light part, on the mirror; log/roadmap/S7/12_test_defects_golfsfx.log, 51 passed in 242 s)
- Device run (audio_stream.device_run, sandbox_basic/00, 300 plays, 40 ticks): switch 1 = golden $device 4b6ed61a, 13,020 lines,
  211 mmioOpenA (one absent: Ball Tree Leaves at line 500, U-1627), x64 and Win32. Switch 0: sha256 fcbc91953c989668...593ea,
  14,309 lines, 229 mmioOpenA, 18 aliased, 0 absent, x64 = Win32 (pinned DEVICE0). Every switch-0 line equals the run at switch 1
  over a scratch install with the directory named `Golf sfx` (S7.11's 11_sfx_install.py layout, junctions and hard links into
  original\, removed afterwards; 14,309 lines, a64a9003...) once the ` (alias ...)` notes are removed: the alias opens exactly
  what an install with the exe's name opens (log/roadmap/S7/12_device_x64.log; S7.11 measured the same 229 / 14,309).
- Name lookup: the 18 names (incl. the lower-case one) unchanged and absent at 1, opened as `sounds/Golf_Sfx/<file>` at 0; other
  names (another directory, a missing file in Golf sfx, the install's own name, a path without `sounds/`, a trailing slash, a
  `music/` path) untouched; an exe-spelled file that exists (put in the overlay) wins.
- App: `--profile-list original|fixed` equals every library switch at 1 / re/port/defect_profiles.tsv applied; `--profile bogus`
  exits 2. The tropical study trace log/p5/terrain/s32t_cov_b_s34_hawaii/trace.trace (`--live-trace`, headless, audio off):
  under the default (fixed) code 0, stop host, 40 records, session.json profile fixed with defects equal to the TSV members;
  under `--profile original` and under `--negctl profile-ignored` code -3 `fault at va 8` (the stock game's Image_draw on
  loadFlic's NULL stream, U-1330) with every switch 1 recorded. app_replay.gate_one over the fixed recording: PASS under its
  recorded switches, FAIL (code -3) with switches={} (control); every switch back at 1 after the replay.
- App audio (course_season corpus trace, --audio model, log/roadmap/S7/12_app_course_season.log): fixed 35 files present / 29
  absent / 13 aliased, original 22 / 116 / 0 (U-1628); 259 requests, failed 0, g_soundReady 1 in both; trace.trace
  (37f5c904...) and framehash.bin (549da51f...) identical: the switch does not reach the sim.
- Light regressions after the final build: see section 6.

## 4. Controls
- golfSfxName mode 1 (directory compared case-sensitively): the lower-case `golf sfx\Cart Away.wav` stays absent at 0: RED.
- golfSfxName mode 2 (deviceStep 3's slot check ignores the alias): the switch-0 device run plays fewer slots, its digest is
  neither DEVICE0 nor DEVICE1 and it makes fewer than 229 opens: RED.
- app `--negctl profile-ignored`: the tropical trace faults: RED. app_replay with every switch 1: FAIL: RED.
- 4 / 4 RED, 0 blind.

## 5. Findings
- The failed opens belong to the extracted install, not to an InstallShield install of the CD (section 1, U-1607).
- The same unshield rename hides six music files (`Desert Music`, `Links Music`, `Tropical Music` written with underscores) and
  seven more names differ in the CD's own data (U-1608, log/roadmap/S7/12_absent_names.log). Not covered by G15.
- The engine retries a failed sound's open on later plays (138 opens against 64 over course_season, U-1628).
