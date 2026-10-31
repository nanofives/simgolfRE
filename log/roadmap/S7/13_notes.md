# S7.13 the music directory aliases (2026-10-30, the S7 lead; decision G16, U-1608)

Mariano's answer G16 (re/roadmap/DECISIONS_PENDING.md): the course-sfx alias switch `sound.golfSfxAlias` also finds the
six theme music tracks the exe asks for under `sounds\music\Desert Music\`, `Links Music\`, `Tropical Music\`, which the
extracted install holds as `Desert_Music` / `Links_Music` / `Tropical_Music` (the unshield rename of Golf_Sfx; facts in
12_uncertain.tsv U-1608 and 12_absent_names.log). Same switch, profile `fixed`, app only, sim unchanged. The seven names that
are wrong on the CD itself (Alligator, lake 1, Marshal Male, PLS Sid mix, mGREETING, forest2, wind 1) stay absent.

## Built (on port/build/S7_w4_root = I19 tree + wave 4, log/roadmap/S7/w4_mirror.py)
- port/src/sim/audio_exe.cpp: golf_sfx_name now walks a table kDirAliases (parent, exe spelling, install spelling):
  sounds / `Golf sfx` / Golf_Sfx (S7.12, unchanged behaviour) and sounds/music / `Desert Music`, `Links Music`,
  `Tropical Music` / the underscore names. Same rules as before: only at switch 0, only when the exe's name is absent,
  directory and file compared case-insensitively, the install's own spelling found through GameFiles::list on a
  case-sensitive host. Negctl audio_exe.golfSfxName mode 3 = the music aliases ignored. DefectDef text extended.
- port/test/test_defects_golfsfx.py: MUSIC (the six tracks); at 1 each stays absent, at 0 each opens the underscore file;
  the CD-wrong names and a music name outside `sounds\music` stay alone; control 3 RED; DEVICE0 re-pinned; the switch-0
  device-run test now splits the alias notes into 18 effects and 6 tracks.
- log/roadmap/S7/11_sfx_install.py: the scratch install with the exe's names now renames the three music directories too,
  so test_device_run_switch_zero_equals_a_renamed_install still proves the alias opens exactly what such an install opens.
- re/port/defect_decisions.tsv D54 and log/roadmap/S7/12_decisions.tsv (the merge_decisions.py fragment): source, site,
  description, effect text and U column extended (G16); re/port/defect_profiles.tsv's sound.golfSfxAlias reason extended.
  12_uncertain.tsv U-1608: resolved for the music, the seven CD names kept.

## Numbers (log/roadmap/S7/13_device_x64.json, 13_device_w32.json; 13_g_gates.log)
- Device run (audio_stream.device_run, sandbox_basic/00): switch 1 unchanged = golden $device 4b6ed61a, 13,020 lines,
  211 mmioOpenA; switch 0 14,833 lines, sha256 07f83f3fce538b03..., 235 mmioOpenA, 24 through the alias (18 effects +
  6 tracks), 0 absent; x64 = Win32 (the two JSON files are byte-identical). S7.12's switch-0 run was 229 / 14,309.
- Builds x64 and Win32 on S7_w4_root: 0 warnings, 0 errors (13_build_x64.log, 13_build_w32.log).
- test_defects_golfsfx 65 / 65 (x64 and the Win32 device-run cases), test_app_audio x64 28 / 28; the rest in 13_g_gates.log.
- Gate on S7_w4_root (13_g_gates.log, tables 13_out/): test_defects_golfsfx 65 / 65, test_app_audio 28 / 28 x64 and Win32,
  segments at switch 0 205 / 205 x64 and Win32, boot at switch 0 66 / 2 / 2 x64 and Win32 with 0 rows differing from I15's
  tables. Switch 1 is unchanged by construction (golf_sfx_name returns at its first test) and the device run at 1 is the
  golden stream; pass 2's switch-1 rows of segments / boot / app files stand.
- Controls: golfSfxName mode 3 (music aliases ignored) RED in test_control_music_aliases_ignored_red; modes 1 / 2 still RED.

## Files
log/roadmap/S7/13_files.txt. No shared file edited.
