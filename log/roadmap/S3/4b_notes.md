# S3.4b: defect switch flic.treeAliasNames (2026-10-24)

Orchestrator decision D5 option a (U-1331): the `original` profile faults where the original dies, a defect switch loads the
install's names and joins `fixed`.

## Measurement the switch rests on
Stock install, scenario cov_b_s34_hawaii on instance s3_4: loadWorldAssets' first `Tree Cerc` open returns NULL and the game dies in
loadFlic's Image_draw after 491 loads (recording port/corpus/flic/cov_b_s34_hawaii/stock, log/roadmap/S3/4_rec_hawaii_stock.log).
The same scenario with the underscore files copied to the exe's names records 500 loads (4_rec_hawaii_alias.log,
port/corpus/flic/cov_b_s34_hawaii/flic); the 114 tropical loads all decode equal in the port. The install holds
Flics/Trees/Tropic/Tree_Cerc and Tree_Drac.

## Change
- port/src/sim/flic.cpp: `g_defect_tree_alias` (default 1), DefectDef `flic.treeAliasNames` in kModule_flic, `tree_underscore_path`.
  In `flic::load`: a path naming Tree Cerc / Tree Drac whose main file is absent: switch 1 throws `Fault{8, 4}` (SG_E_FAULT through
  flic.load / flic.toGfx); switch 0 retries the path with the underscore spelling and decodes it (the shadow name derives from the
  same path). A present file takes the old route at both values. Negative control 5 of g_negctl_flic applies the alias at switch 1.
- port/test/test_flic.py: test_tree_cerc_names_fail_like_the_original expects SG_E_FAULT at the default (it expected status 1).
- port/test/test_defects_flic.py (new, 6 tests): switch exists and defaults 1; fixed-profile membership (real table, or table plus
  the merge line); fault probe ON and cells equal to the underscore load OFF; every distinct tropical world load of the alias
  recording equal to the original with the switch 0 on the stock install (no alias copies); completing control (sandbox_basic, 120
  loads, no tree name) identical at 1 and 0; wrong body (negctl 5) no longer faults.
- flic.h: resolve_name added (round 2).

## Round 2 (lead's three items)
1. flic.cpp exports `flic::resolve_name(files, name)` (declared in flic.h): the backend-free entry. menu.cpp's load_flic calls it around flic_cols
   (exact edit in 4b_merge.md section 3, applied in the mirror source copy by 4b_mirror.py, never in menu.cpp). flic::load shares the helper.
2. The Fault is `Fault{8, 4}`: the stock death is an access violation reading 0x8 at 0x00473e67 (Image_draw on the NULL frame Surface), from 0x0043d990
   called from 0x00441e18 (U-1330). U-1337 closed.
3. Measured: segments x64 205/205 and Win32 205/205, 0 differences against S6-1 (4b_segs_compare.log); boot gate HEAD-61 58/2/1, 0 rows differ from S0
   (4b_boot_compare.log). Tropical case with no backend: test_defects_flic.py loadWorldAssets (type 2) faults at 1 and at 0 equals the original-with-aliases
   GameState byte for byte; wrong body (negctl 5) completes instead of faulting. No boot trace of cov_b_s34_hawaii exists for the port, hence the constructed call.

## Numbers
test_defects_flic 8/8 x64 and 8/8 Win32; test_flic 9 passed 1 skipped; static 6; segments and boot as above. Controls: 2/2 RED (negctl 5 on the flic load and on
loadWorldAssets), 0 blind. Open: U-1338 (scope, documented). Merge: log/roadmap/S3/4b_merge.md.

## Reproduce
```
~/bin/py -3.12 log/roadmap/S3/4b_mirror.py --w32
cd port/build/S3_head
SG_PORTSIM_DLL=<S3_4b x64 dll> ~/bin/py -3.12 -m pytest port/test/test_defects_flic.py
SG_BACKEND=win32 ~/bin/py -3.12 ../../../log/roadmap/S3/4b_run_w32.py port/test/test_defects_flic.py
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --dll $SG_PORTSIM_DLL --jobs 8 --out log/roadmap/S3/4b_segs_x64.tsv   (and win32 as in 4_notes.md)
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out log/roadmap/S3/4b_boot_gate.tsv ; ~/bin/py -3.12 log/roadmap/S3/4b_boot_compare.py   (from the repo root)
~/bin/py -3.12 log/roadmap/S3/4b_segs_compare.py
```
