# S7.12 merge notes

Base: HEAD 37e58e96 + S7.10 / S7.11's files (log/roadmap/S7/10_files.txt, 11_files.txt). Every edit is in a file of the row's
files_owned (log/roadmap/S7/12_files.txt); no shared file was edited. Mirror: log/roadmap/S7/12_mirror.py (port/build/S7_12_root).

- port/src/sim/modules.inc, tick.cpp, commands.cpp, port/CMakeLists.txt: no change. audio_exe.cpp's ModuleDef now carries a
  DefectDef table (switch `sound.golfSfxAlias`) and one more configuration entry (`audio_exe.golfSfxName`) in its own table;
  touching registry.cpp is not needed (modules.inc unchanged).
- Goldens: none changed. golden_audio_exe.json `$device` (4b6ed61a, 13,020 lines) is the switch-1 run; the switch-0 digest
  (fcbc9195..., 14,309 lines) is pinned in port/test/test_defects_golfsfx.py.
- re/port/defect_profiles.tsv: one row (sound.golfSfxAlias 0 G15). re/port/defect_decisions.tsv: one row D54 after D53, written
  by log/roadmap/S7/12_decisions.py --apply from the fragment log/roadmap/S7/12_decisions.tsv. A later
  `log/roadmap/S6/merge_decisions.py --write` drops D54 unless it reads the fragment: patch below.
- port/app/main.cpp's kFixedProfile is a compiled copy of defect_profiles.tsv (the LAN build ships no repo file);
  test_defects_golfsfx.py::test_app_profile_list_equals_the_table fails when the two differ: a new `fixed` row must be added in
  both places.

## Shared-file patches (the integrator applies them)

1. port/test/integrator.py, cmd ledger's `tests` map (section "5. compatibility switches"), after the flic.treeAliasNames entry:

```
                 # roadmap S7.12 (D54, G15): the device sink opens the 18 course effects through the alias at 0, line for line
                 # an install with the exe's directory name; the switch-1 run is golden $device
                 "sound.golfSfxAlias": "port/test/test_defects_golfsfx.py::test_device_run_switch_zero_opens_the_course_effects, "
                                       "test_device_run_switch_zero_equals_a_renamed_install (device sink)",
```

2. log/roadmap/S6/merge_decisions.py, after the S3 wave 2 block (`d5_s3 = {r["id"] for r in d}`):

```
    # roadmap S7.12 (G15, Mariano 2026-10-30): the course-sfx directory alias
    h7, g = read(ROOT / "log" / "roadmap" / "S7" / "12_decisions.tsv")
    assert h7 == h6, h7
    for r in g:
        assert r["id"] not in out, r["id"]
        out[r["id"]] = dict(r, verified="device-sink run (S7.12)")
    g15 = {r["id"] for r in g}
```
   and in the `for r in rows` loop, before `elif r["recommendation"] == "fix":` add
   `elif r["id"] in g15: r["decision"] = "G15 fix (S7.12)"`. Output then equals the committed table (D54 included).

3. UNCERTAINTIES.md: the five rows of log/roadmap/S7/12_uncertain.tsv (U-1607, U-1608, U-1627, U-1628, U-1638), and on U-1499
   append: `S7.12: switch sound.golfSfxAlias (profile fixed, the app's default) opens them from Golf_Sfx (D54).`

4. LAN staging: no change. With the app's default profile the extracted install's `Golf_Sfx` plays as it is (S7.11's staging
   rename was dropped by Mariano's decision; it is not needed now).

5. Suggested CLAUDE.md paragraph (after the S7.9 one):

Roadmap S7.12, the course-sfx alias and the app's defect profile (2026-10-30, notes log/roadmap/S7/12_notes.md, U-1607 / U-1608 /
U-1627 / U-1628 / U-1638): switch `sound.golfSfxAlias` (audio_exe.cpp golf_sfx_name, decision G15, D54): at 1 the device sink's
mmioOpenA of the exe's `sounds\Golf sfx\` names fails on the extracted install's `Golf_Sfx` as the original's does (the CD header
iso\data1.hdr names it `Golf Sfx`; unshield wrote spaces as underscores), at 0 the install's Golf_Sfx file opens. Device run:
switch 1 = golden $device, switch 0 fcbc9195 (229 opens against 211) = an install with the exe's directory name line for line, x64
= Win32. `simgolf_port --profile original|fixed` (default fixed, compiled copy of defect_profiles.tsv, `--profile-list`); every
session.json records `profile` and every switch's value, and app_replay.gate_one replays under them. The tropical study trace runs
under fixed and faults under original (va 8, the stock game's). The same unshield rename hides six music files (U-1608).
