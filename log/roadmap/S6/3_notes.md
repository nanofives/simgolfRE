# S6.3 notes: defect profile mechanism

Done (Python only, over the existing sg_defect ABI):
- `re/port/defect_profiles.tsv`: header `switch value decision_id reason`, documented comment block, empty body (`fixed` == `original`).
- `port/test/profiles.py`: `load_table` (header, 0/1 values, no duplicates, required columns), `members`, `validate` (switch names absent
  from the library are an error, all listed), `apply(lib, profile, path=None)` (reset to all 1, then members; a refused profile changes
  nothing), `state`, `current`, CLI `~/bin/py -3.12 -m port.test.profiles validate [--dll] [--file]`. `original` takes no member list
  (`members("original", path)` raises "not editable"); `EDITABLE = ("fixed",)`.
- `port/test/test_profiles.py`: 19 tests, one file (5 static, 14 port), no skips here.
- No C++ (no profiles.cpp). Optional native ABI text is in 3_merge.md.

Gate (build dir port/build/S6_3, `SG_PORTSIM_DLL=port/build/S6_3/Release/portsim_c.dll`):
`~/bin/py -3.12 -m pytest port/test/test_profiles.py -q` -> 19 passed. `test_shots.py -k defect` (the existing switch tests) 5 passed.
Original profile vs no profile: switch state all 1, three segments (sandbox_basic/00, course_season/00, cov_a_flags/05) give identical
status, frames, divergence, noncausal text and run detail; shotPower vectors GREEN. The other existing switch tests (core, shots x3, one
vector-blind) are untouched code; only shots' were re-run.
Controls: a fixture profile `shotPower.cacheIgnoresPutt = 0` makes shotPower RED on exactly the putt vectors (same check as
test_defect_shot_power_putt), and `original` restores GREEN: 1/1 RED. An unknown switch refused (state unchanged), bad table forms
refused (5 parametrised), 0 blind.
Not run: full segment gate (200+ segments) and `integrator defects`; the original profile is by construction the default state
(lib.defect_reset), and the sample covers three scenarios.

Open: the lead writes the real member list after S6.1/S6.2; run `profiles validate` then. `current()` returns `custom` for any other
switch state. Win32 backend not exercised (lib32 has the same defect interface; not tested). U-1570..U-1579: none filed.
