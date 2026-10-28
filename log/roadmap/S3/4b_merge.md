# S3.4b merge: defect switch flic.treeAliasNames

Worker S3.4b, 2026-10-24. Notes: log/roadmap/S3/4b_notes.md. Builds on log/roadmap/S3/4_merge.md (apply that first).

## 1. re/port/defect_profiles.tsv (S6's file): one line (tab separated, after the last row)

```
flic.treeAliasNames	0	D5	loadWorldAssets' tropical `Tree Cerc` / `Tree Drac` loads (the install holds Tree_Cerc / Tree_Drac) fault in the original: load the install's names instead
```

test_defects_flic.py::test_switch_is_in_the_fixed_profile validates the table plus this line (a temp copy) until it is applied,
then reads the real table.

## 2. re/port/defect_decisions.tsv (S6's file): one row

Values for the row (columns as the file's header): id = the D-number the S6 owner assigns; subject `loadWorldAssets: Tree Cerc / Tree Drac names`;
class `fault` (the original's flicOpenStream returns NULL and loadFlic's Image_draw dies; stock run port/corpus/flic/cov_b_s34_hawaii/stock,
log/roadmap/S3/4_rec_hawaii_stock.log, instance s3_4); effects: save no, balance no, sim yes on tropical courses only (the sheet's
columns decide later cell allocations; before this switch the port ran on with 0 columns), frame yes (the trees draw), hang no,
fault yes; recommendation `fix` (orchestrator D5 option a); switch `flic.treeAliasNames` (default 1 = Fault, 0 = load Tree_Cerc /
Tree_Drac). The divergences.tsv line for tropical courses (U-1331) becomes: original profile = Fault, as the original.

## 3. port/src/sim/menu.cpp (not S3's): two edits, the column path, no gfx backend needed (decision of the S3 lead)

The original dies on a tropical course whether or not anything is drawn, and the alias changes sim state (columns -> cell
allocations), so the switch acts in load_flic's column path. flic.cpp exports `std::string flic::resolve_name(const GameFiles &, const
std::string &name)` (flic.h): a name holding `Tree Cerc` / `Tree Drac` whose file is missing raises `Fault{8, 4}` at switch 1 and returns
the underscore name at 0; every other name is returned unchanged. Edits (applied in the mirror source copy by 4b_mirror.py):

(a) include, after `#include "portsim/sim/course.h"`:

```cpp
#include "portsim/sim/flic.h"
```

(b) in `load_flic`, the line

```cpp
  const int32_t cols = flic_cols(ctx, read_str(ctx.mem, text));
```

becomes

```cpp
  const int32_t cols = flic_cols(ctx, flic::resolve_name(ctx.files(), read_str(ctx.mem, text)));
```

The 4_merge.md section 2 hook-up (sheet_to_gfx) is unchanged: with the switch at 0 it decodes the same underscore file through flic::load, at 1 it
never runs because load_flic has already faulted.

## 4. UNCERTAINTIES.md: append log/roadmap/S3/4b_uncertain.tsv (U-1336..U-1339)

## 5. Gate results (mirror port/build/S3_head + S3.4 files + this step's flic.cpp / flic.h / tests + the menu.cpp edit; builds S3_4b x64, S3_4b_w32)

- test_defects_flic.py 8 passed x64 (4b_pytest_defects_x64.log) and 8 passed Win32 (4b_pytest_defects_w32.log, runner 4b_run_w32.py). Two of the eight run
  loadWorldAssets on a tropical course (type 2) with NO gfx backend: switch 1 gives SG_E_FAULT; switch 0 on the stock install gives a GameState byte-equal to the
  switch-1 run over the install plus spaced copies (the original's completing run), with non-zero tropical sheet columns; negctl 5 turns the Fault into a completing run.
- test_flic.py 9 passed, 1 skipped (gfx_body.h missing in the mirror); test_static.py 6 passed; test_profiles.py 19 passed (first build).
- Segments, with the menu.cpp edit: x64 205/205 PASS, Win32 205/205 PASS, 0 digest or status differences against log/roadmap/merges/S6-1/segs_x64.tsv / segs_win32.tsv (4b_segs_compare.log).
- Boot gate: HEAD's 61 scenarios 58 PASS / 2 KNOWN / 1 NO_FRAMES, 0 rows differ from log/roadmap/S0/1_boot_gate.tsv (4b_boot_compare.log). Extra rows of the shared corpus (other
  workers' recordings, U-1102): cov_r_bio, cov_r_flic_input FAIL exactly as in S3.4's own boot gate before this edit (4_boot_gate.tsv), cov_r_s2_screens FAIL (stops in showBuyLandScreen, not flic).
