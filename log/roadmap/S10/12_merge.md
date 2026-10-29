# S10.12 merge notes (nothing here is applied; every edit below is to a shared file or a decision)

## 1. re/match/boundaries.tsv: one new row (U-1912)

Append this line (tab-separated; columns module, entry, size, reason):

```
golf_clean.exe	0x004a55d2	130	ehVectorConstructorIterator body 0x4a55d2..0x4a563c plus its contiguous __finally funclet 0x4a563c..0x4a5654 (VC6 emits both in one COMDAT)
```

Then: `git mv re/match/wip/s10_12_ehvec.cpp re/match/smsg_hand_ehvec.cpp` and run
`~/bin/py -3.12 re/tools/match.py re/match/smsg_hand_ehvec.cpp` (expected 100.0%, 44/44; the same number was measured with a
monkeypatched extent: log/roadmap/S10/12_ehvec_extent130.log), `~/bin/py -3.12 -m pytest tests/test_match.py -q -k smsg_hand`. Without
the row the file reads 81.8% and must not sit in re/match/. Evidence for it: `match.py <file>` (the row 0x004a55d2 is already C4 by
scenario, so no promote). The row changes the extent for every caller of ghidra_size at that address; the funclet entry 0x004a563c stays
a function of the master.

## 2. Decision: re/match/smsg_hand_toolbar_sp5.cpp (U-1910)

Already in the tree (new file, canonical, 100% under `// TOOLCHAIN sp5`; tests: test_match -k smsg_hand 1 passed, test_match_tools +
test_smsg_import 18 passed). The lead decides whether an SP5 line counts as C4 evidence for the exe; if not, `git mv` it to re/match/wip/.
Commands to give it evidence (writes log/diff/00434cf0__RenderBuildingToolbar__YAXHH_Z.match.csv and
00436e50__RenderEmployeePanel__YAXHH_Z.match.csv; delete both to retract):
`~/bin/py -3.12 re/tools/match.py re/match/smsg_hand_toolbar_sp5.cpp`; the promote line is in 12_promote_cmds.txt.

## 3. Wave-1 interactions

- No existing file was edited. wip/smsg_golf_Toolbar.cpp and wip/smsg_golf_GolferShot.cpp still hold their misses; once the decision
  in section 2 is taken, drop their two Toolbar lines (and 0x00427380 stays).
- New wip files (read by test_match_tools' entry test, all at function entries): s10_12_Toolbar.cpp, s10_12_GolferShot.cpp,
  s10_12_ehvec.cpp, s10_12_expfield.cpp.
