# S3.4c merge: what S3 wave 2's re-merge needs (log/roadmap/S3/MERGE_w2.md + this file)

S3.4c changes no shared file and no port source. Its files (4c_files.txt) are test-side only and join files_w2.txt:

```
port/test/test_menu.py
port/test/golden_menu.json
port/test/test_terrain_live.py
port/test/terrainlive.py
```

(test_menu.py, golden_menu.json, test_terrain_live.py and terrainlive.py are tracked files owned by 38j / S3.2; S3.4c's edits to them are
listed in 4c_notes.md sections 2 and 3. golden_menu.json was re-pinned with SG_EMU_ORACLE=update only after every oracle case of test_menu
was GREEN, 4c_menu_update.log.)

## 1. Lines to append to MERGE_w2.md's "Gate" section

```
- S3.4c (log/roadmap/S3/4c_notes.md), tree = HEAD bbc9072f + S3 wave 2 + S3 wave 4 (log/roadmap/S3/4c_mirror.py, builds S3_4c / S3_4c_w32):
  - port/test/test_menu.py 70 passed with SG_EMU_ORACLE=update (4c_menu_update.log), 24 passed + 47 skipped without the oracle in the suite:
    the emulated original now faults on loadFlic's Image_draw of a NULL stream (Image_draw+0x7 reads [ecx + 8]), so the three stock tropical
    cases (tropical_from_parkland, tropical_boot_difficulty, lia_tropical) are judged against the original's Fault at flic.treeAliasNames = 1
    (port SG_E_FAULT, data image and recorded calls at the fault equal); three new `_alias` cases (the install with the spaced copies) run the
    port at 0 and complete equal. Golden digests: the 3 stock cases changed, 3 alias cases new, 41 unchanged (4c_golden_diff.log). New
    control ("flic.load", 5) RED; all 11 digest controls RED.
  - port/test/test_terrain_live.py 398 passed in the x64 suite: the hawaii recording (made with the alias copies) runs at
    flic.treeAliasNames = 0 (4 cases PASS), new test_hawaii_at_the_default_switch_faults_like_the_stock_game (sg_run_boot -3 at 1);
    test_x64_equals_win32_terrain_pixels PASS (I6's RunnerDied was a transient runner start: the same test passes on I6's own tree).
  - test_flic 10 passed, test_defects_flic 8 passed (x64 suite).
  - port suite x64, one file per process (4c_suite_by_file.sh): 66 files, 64 PASS; test_boot 94 + 1 failed (cov_r_s2_screens, BASELINE's
    known failure); test_gfx_dialogs 31 + 1 failed (test_dialog_bodies_handled: `Surface_drawSprite: an alpha`), which S3 wave 4's own tree
    port/build/S3_6b_src fails the same way without any S3.4c file (4c_dialogs_on_S3_6b.log) and I6's tree without wave 4 passes: a wave 4 item.
    repo static 614 passed / 2 skipped.
  - segments x64 205 / 205 and Win32 205 / 205, 0 rows differ from log/roadmap/merges/I6/segs_*.tsv (status, frames, digest).
  - boot: 70 rows, 0 differ from log/roadmap/merges/I6/boot_x64.tsv; HEAD's 61 at 58 PASS / 2 KNOWN / 1 NO_FRAMES (4c_boot_compare.log).
  - Win32: test_defects_flic 8, test_menu golden digests + the flic.load control pass (4c_pytest_w32.log).
```

## 2. CLAUDE.md paragraph (MERGE_w2.md section 6): append one sentence

```
test_menu's emulated original faults like the stock game (Image_draw on loadFlic's NULL stream reads 0x8): its stock tropical cases are
pinned against that Fault at flic.treeAliasNames = 1 and `_alias` cases (the spaced copies) run the port at 0; test_terrain_live runs
S3.2's hawaii recording (made with the copies) at 0 and checks the Fault at 1 (roadmap S3.4c).
```
