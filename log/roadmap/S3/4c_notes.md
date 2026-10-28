# S3.4c notes: S3 wave 2 re-merge fix (the tropical cases of test_menu and test_terrain_live)

Worker session cmvo1wvng8w23o11588itengj (claude2), lead cmvmns8sn3fsbo115g4dgm1nf. Step row S3.4c of re/roadmap/S3_steps.tsv.
Why: integrator I6 (log/roadmap/merges/I6.md lines 13-27) refused S3 wave 2 (FLIC decoder + defect switch flic.treeAliasNames,
MERGE_w2.md) because two port test files failed with it: test_menu's golden digests of three tropical cases and test_terrain_live's
four hawaii boots, both hitting the switch's default-1 Fault at the first `Tree Cerc` load; plus one RunnerDied in
test_x64_equals_win32_terrain_pixels.

## 1. Tree

`log/roadmap/S3/4c_mirror.py` builds port/build/S3_4c_src = `git archive` of HEAD bbc9072f, the junctions of I6's mirror (original,
log, port/corpus, tests/golden, tests/fixtures, re/versions, tools/vc6, shim/build, re/replay, extracted, instances), then:
- every port/ file of port/build/S3_6b_src/port (S3.6b's tree = I6's port/ with all four waves + S3 wave 4) that differs from HEAD's
  (CR / LF ignored). Checked before (diff --strip-trailing-cr): exactly wave 2's flic.cpp, flic.h, test_flic.py,
  test_defects_flic.py, menu.cpp (edits 2a-2c), modules.inc (SG_MODULE(flic)) and wave 4's gfx_hud / gfx_pixel files,
  commands_layout.inc, gfx.cpp, gfx_body.h, gfx_families.inc, tick.cpp; every other port file equals HEAD (17 files copied);
- re/port/defect_profiles.tsv / defect_decisions.tsv from port/build/I6 (HEAD's plus the flic line / the D53 row, as I6's
  apply_shared.py with prepared_s3w2 wrote them), re/tools/flic_cells_rec.py, re/frida/js/flic_cells.js;
- this step's files from the shared tree (`--own-only` recopies only them).
Builds: port/build/S3_4c (x64), port/build/S3_4c_w32 (Win32), 0 errors (4c_build.log, 4c_build_w32.log).

## 2. test_menu: what each tropical case models

The three failing cases read the stock install (original/ has Tree_Cerc / Tree_Drac only). The old emulated original could not
fault: its flicOpenStream stub returned 0 for every open and loadFlic's Image_draw was a drop, so a missing file ran on with 0 columns
(the run-on port pinned against it). What the original does there (U-1330): loadFlic stores flicOpenStream's result in
g_flicFrameImage, `mov ecx, eax`, and calls Image_draw (thiscall) whose first read is `mov eax, [esi + 8]` with esi = ecx
(Image_draw+0x7); with the NULL stream that reads 0x8, the stock game's recorded death (log/roadmap/S3/4_rec_hawaii_stock.log, c3fyb's
exception report). The same pattern follows flicNextFrame at the two frame draws (checked in the disassembly of loadFlic).

test_menu.py's oracle now models that literally:
- flicOpenStream returns a token (0x7E000000 + 0x10 * slot, never dereferenced: every consumer is a drop) for a file whose converted
  FLIC header opens, 0 otherwise, and stores the column count as before; flicNextFrame (a drop before) returns the slot's token;
- Image_draw (a drop before) faults with `Fault read 0x8: Image_draw on a NULL frame Surface` when ecx is 0, else returns as the drop
  (its own `ret N`). g_flicFrameImage, which holds the tokens, was already masked.
- compare_lwa / compare_lia share compare_entry: when the original stops on that fault the port must give SG_E_FAULT, and the data
  image at the fault (MASK) and the recorded calls must be equal, as for a completing case.

Cases (decision per case):
- `tropical_from_parkland`, `tropical_boot_difficulty`, `lia_tropical` model the STOCK install: switch at its default 1, both sides
  fault at the first Tree Cerc load (4c_probe.log: original err = the NULL-draw fault, port code -3, 427 sheets with columns > 0 on
  both sides, images equal, showLoadingScreen calls 0 / 2 / 0 equal).
- New `tropical_from_parkland_alias`, `tropical_boot_difficulty_alias`, `lia_tropical_alias` model the install with copies under the
  spaced names (the recording's alias, 4_rec_hawaii_alias.log): the oracle's paths map `Tree Cerc` / `Tree Drac` to the underscore
  directories when the spaced file is absent, the port runs at flic.treeAliasNames = 0; both complete, 436 sheets with columns,
  images equal.
- Oracle (SG_EMU_ORACLE=update, 4c_menu_update.log): all 70 tests of test_menu PASS, i.e. every one of the 15 + 5 world / interface
  cases, the 16 pool cases and the 11 screen cases GREEN against the changed oracle (the token / Image_draw model changes no
  non-tropical case), then pinned. Digest changes (4c_golden_diff.log, before copy 4c_golden_menu.before.json): exactly the three stock
  cases changed (run-on -> Fault), the three alias cases new, the other 41 unchanged (47 keys).
- Control: NEGCTL gains ("flic.load", 5): the alias applied at switch 1, so the stock cases complete; RED (4c_menu_negctl.log: 18
  passed = golden + 11 digest controls + 4 tick.menu controls + 2 vector controls). All 11 digest controls RED.

## 3. test_terrain_live: the hawaii recording

s32t_cov_b_s34_hawaii was recorded on instances\s3_4 with the tree-alias copies, so the port runs it at flic.treeAliasNames = 0:
terrainlive.port_frames takes `defects={name: on}` (set before, defect_reset after), test_terrain_live.py has SWITCHES (hawaii -> the
switch off) used by `_port` (the cache key includes the switches). New test
test_hawaii_at_the_default_switch_faults_like_the_stock_game: at the default the trace run stops with `sg_run_boot -3`, at 0 it runs
(this also reads RED if the switch stops acting). 4c_tl_hawaii.log: 4 hawaii course-view cases + the new test PASS.

## 4. The RunnerDied (test_x64_equals_win32_terrain_pixels)

The I6 failure (log/roadmap/merges/I6/suite/test_terrain_live.txt) is the Win32 runner's stdout closing at OP_HELLO with
`proc.poll()` None, in the lib32 fixture, before any port function runs, so the flic switch is not on its path. Separated:
- on I6's own unchanged tree and builds (port/build/I6, default lib32 fixture, no rebuild: runner32_stale False, bytecode writes off)
  the test PASSES (4c_tl_w32eq_I6tree.log);
- on this step's tree with SG_PORTSIM_DLL32 / RUNNER32 = port/build/S3_4c_w32 it PASSES (4c_tl_w32eq.log);
- in the full suite below (default fixture) see section 5.
So the RunnerDied was a transient runner start failure in I6's run (another workload at 07:19), not caused by S3 wave 2.

## 5. Gates (tree of section 1, builds S3_4c / S3_4c_w32)

- port suite x64, one file per process (`4c_suite_by_file.sh`, outputs log/roadmap/S3/4c_suite/<file>.txt; the background shell hit its 2 h
  limit after file 61, so files 62-66 ran as a second pass, 4c_suite_x64_part2.log): 66 files, 64 PASS. test_menu 24 + 47 skipped,
  test_terrain_live 398 (hawaii and x64 = Win32 included, default lib32 fixture), test_flic 10, test_defects_flic 8, test_tick 250,
  test_vectors 298 + 478 skipped (I6 474: +4 = wave 4's gfx.family.hud / pixel registry entries without vector files). Failures:
  - test_boot 94 + 1 failed: cov_r_s2_screens at record 1,156 (showBuyLandScreen), BASELINE's known failure.
  - test_gfx_dialogs 31 + 1 failed: test_dialog_bodies_handled finds `unsupported Surface_drawSprite: an alpha outside drawAlpha's quarter
    band` in cov_a_keys's run. NOT from this step or wave 2: S3.6b's own tree (port/build/S3_6b_src = I6 + wave 4, its S3_6b build, none of
    S3.4c's files) fails the same assertion (4c_dialogs_on_S3_6b.log), while I6's tree (waves 2 / 3 + S2 wave 5, no wave 4) passed 32
    (log/roadmap/merges/I6/suite/test_gfx_dialogs.txt). S3.6b's gate list did not run test_gfx_dialogs. Open for wave 4's owner.
  - repo static 614 passed / 2 skipped.
- Win32: test_defects_flic 8, test_menu test_golden_digests and the flic.load control (4c_pytest_w32.log, SG_BACKEND=win32), i.e. Win32
  gives the pinned digests.
- segments x64 205 / 205 and Win32 205 / 205, 0 rows differ in status / frames / digest from log/roadmap/merges/I6/segs_x64.tsv /
  segs_win32.tsv (4c_compare.py; 4c_segs_*.tsv / .log).
- boot (`port.test.boot --all --jobs 3`): 70 rows, 0 differ from log/roadmap/merges/I6/boot_x64.tsv; HEAD's 61 at 58 PASS / 2 KNOWN /
  1 NO_FRAMES (4c_boot_compare.log).
- Controls: ("flic.load", 5) on test_menu's digests RED (x64 and Win32); test_hawaii_at_the_default_switch_faults_like_the_stock_game is
  the switch's own check on the recording (Fault at 1, RAN at 0). 0 blind.

## 6. Open

- test_gfx_dialogs::test_dialog_bodies_handled with S3 wave 4 (section 5): which wave 4 drawing call reaches Surface_drawSprite with an
  alpha outside the quarter band in cov_a_keys was not traced here (wave 4's step).
- No new U-rows.

## Reproduce

```
~/bin/py -3.12 log/roadmap/S3/4c_mirror.py --w32           # tree + x64 build (+ Win32 with --w32); --own-only recopies this step's files
cd port/build/S3_4c_src
export SG_PORTSIM_DLL=C:/Users/maria/Desktop/Proyectos/SimGolf/port/build/S3_4c/Release/portsim_c.dll
SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_menu.py -q -p no:cacheprovider      # =update re-pins after the oracle
~/bin/py -3.12 log/roadmap/S3/4c_probe.py
~/bin/py -3.12 -m pytest port/test/test_terrain_live.py -q -p no:cacheprovider -k hawaii
cd ../../..; sh log/roadmap/S3/4c_suite_by_file.sh            # SG_PORTSIM_DLL set; one file per process
```
