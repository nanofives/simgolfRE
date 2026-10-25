# S1.2 notes: U-1012, chooseShotTarget.strictFlagSticky on constructed states (2026-10-24)

Result: DONE. A family of constructed GameStates reaches a search in which the original's strict flag stays 1 after a landing
that stored it and a LATER landing fails the condition. On them the port with the switch ON equals the original (Unicorn)
byte for byte in eax and the whole GameState image, and with the switch OFF it differs. U-1012 is resolved by this evidence.

## What the defect is (cited)
chooseShotTarget 0x00422fb0 keeps the strict flag in the frame dword [esp+0xb4]. The only store is `mov [esp+0xb4], 1` at
0x00423b0e. It is reached only from the chain at 0x00423acd..0x00423b0c: test byte [ebp+0x5794d1], 4 (golfer prefs bit 2, 0x00423acd),
`jg` on the landing tile's attribute byte at +0x578372 (0x00423aef), `cmp ecx, 0x32; jl` (hole distance >= 50, 0x00423af1), and
the `idiv` / `cmp eax, 4; jl` (0x00423b07..0x00423b0c: (reach - t4) * 60 / (reach * 3) >= 4). Every failing test jumps to
0x00423b19 and falls into the read `mov edx, [esp+0xb4]` at 0x00423b20, which is pushed as scanTileLine's seventh argument at
0x00423b2e..0x00423b50. Nothing clears the word, so a second landing in the same search that fails the chain reads the 1 an
earlier landing stored. Gate to reach this code (the port's `trials == 4` pass, the second pass): prefs bit 2,
unk_005a9cc4 != 2, outOfReach == 0, landing level <= 0 (port/src/sim/shots.cpp lines 1256..1270).
scanTileLine with strict 1 scores the last cells of the line only when they are collar type (port/src/sim/terrain.cpp:
`(i >= steps - lead && strict == 0) || is13(t)`), so the score of every later strict scan changes, and with it the best cell,
the aim returned and g_shotTargetX / Y.

## The states (port/test/test_shots_strict.py, `_case(seed)`)
Deterministic family: a random 50x50 tile grid of legal types (one of three mixes), one golfer (index 5, slope preference
byte +0x19 in {4,5,6,7}, kind 0 or 1, ten skill bytes 8..29, ball and golfer at a tile centre), hole 0 at 3..13 tiles from the
ball, g_difficulty 0..2, a seeded g_rng; the base image is the user's initialised .data with the tile-type table copied from its
init copy and the sine table built by the port's initSinTable. Sweep over seeds 0..699 with the port, switch ON vs OFF: 42 of 700
differ (6 of the first 100, 36 of the next 600), 0 faults. Pinned seeds: 4, 35, 66, 156, 182, 239, 394, 526 (eax changes 0 -> 1,
1 -> -1, -1 -> 0 on 239, 394, 526; image-only changes on the rest).

## Evidence
- Default suite (no original run): `test_the_switch_changes_the_search` 8, `test_golden_digests_of_the_original` 8 (the port ON equals
  SHA-256 digests of the original's result, the port OFF does not), `test_golden_digests_see_negctl` 4,
  `test_negctl_blind_spots_are_known` 1, `test_sticky_witness_has_no_false_positive` 1: 22 passed, 25 skipped, 12 s
  (log/roadmap/S1/2_default.log).
- Opt-in `SG_EMU_ORACLE=1` (needs unicorn and original/golf_clean.exe): the original `chooseShotTarget(5)` at 0x00422fb0 runs in
  Unicorn (vector_emu's image, flat GDT and fake TEB; own lean runner in the test file, no per-access hooks, 8-21 s per state):
  `test_original_equals_port_with_the_flag_sticky` 8 / 8 (eax and all 0x381174 bytes equal), `test_original_differs_from_the_fixed_behaviour` 8 / 8
  (OFF is RED), `test_original_scans_strict_after_a_failing_landing` 8 / 8: code hooks on 0x00423b0e (store) and 0x00423b20 (read)
  show every state has a scan that read 1 with no store at its own landing, and every scan before the first store read 0
  (log/roadmap/S1/2_oracle.log has the 8 + 8 + the witness unit test; its 4 scans failures are the first version of the
  first-read assertion, which wrongly required the first read to be 0 although the first qualifying landing reads its own store;
  fixed and re-run, log/roadmap/S1/2_oracle_scans.log 8 passed).
- Regeneration of the pinned digests: `SG_EMU_ORACLE=update pytest port/test/test_shots_strict.py -k update -s`
  (log/roadmap/S1/2_golden_update.log; 156 s).
- Reproduce: build `cmake -S port -B port/build/S1_2 -A x64` and `--build ... --config Release --target portsim_c`, then
  `SG_PORTSIM_DLL=port/build/S1_2/Release/portsim_c.dll ~/bin/py -3.12 -m pytest port/test/test_shots_strict.py` (add
  `SG_EMU_ORACLE=1` for the original; about 12 minutes).

## Controls
- The switch OFF (the fixed behaviour) reads RED against the original on 8 / 8 states (oracle and digests): the main control.
- chooseShotTarget negctl modes 1 (1 trial per cell), 2 (variant 1 never tried), 3 (hole-distance divisor 4), 4 (landing weight 4)
  change a pinned digest: RED. Modes 3 and 4 were blind on the vector corpus; these states see them.
- Negctl mode 5 (prune tolerance 0x40) is BLIND on the pinned states (recorded, asserted by `test_negctl_blind_spots_are_known`);
  it would need a search whose second pass keeps cells scoring between best + 0x40 and best + 0x80 (U-1162).
Total: 5 RED of 6, 1 blind.

## Oracle limits (both arms alike)
- The original's frame word starts at 0 (zero-filled emulated stack); the live value is stack residue (U-1160, U-0880).
- pumpInput483bd0 (0x00483bd0, called at 0x00423fb2) is patched to `ret` in the emulated original, as the port drops it: with no
  display object the real pump faults at 0x00483bc5 (U-1161).
- The states are synthetic; whether a played course produces them is not measured (U-1162).

## Files
Created: port/test/test_shots_strict.py, log/roadmap/S1/2_notes.md, 2_files.txt, 2_uncertain.tsv, 2_merge.md, 2_default.log, 2_oracle.log,
2_oracle_scans.log, 2_golden_update.log, 2_build.log. shots.cpp and every other source untouched. Private build dir
port/build/S1_2 (not committed).

## Open
U-1160 (live stack residue), U-1161 (pump with a fake display), U-1162 (reach in played courses, negctl 5 blind).
