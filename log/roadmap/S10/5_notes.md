# S10.5 notes: match.py toolchain selector, sound.dll near misses with SP5, U-1886 (2026-10-29)

Status: DONE. Final gate: 487 passed, 0 failed (log/roadmap/S10/5_match_tier2.log).

## Process rules followed

Every file write at 60 lines or fewer (longer files written in appended parts); runs over 2 minutes in the background
teed to log/roadmap/S10/5_*.log; `~/bin/py -3.12` only; at most 2 compile jobs at a time; no `--evidence` anywhere (no
CSV written to log/diff by this step); no port/, shim/, original\ or tracker edit.

## 1. The selector

`re/tools/match.py`:
- `// TOOLCHAIN <id>` per file (`file_toolchain`), default `8168`. `compare(..., toolchain=)` and the CLI
  `--toolchain` override the file's line (used to re-score a file with another compiler).
- `TOOLCHAINS`: `8168` = tools/vc6/vc98/bin (unchanged: same PATH string, same command line, so every existing file
  compiles exactly as before); `sp5` = tools/vc6sp5/VC98/BIN (cl 12.00.8804, c1 12.00.8867, c1xx 12.00.8964, c2
  12.00.8966, file versions read from the DLLs) with tools/vc6sp5/Common/MSDev98/Bin on PATH (MSPDB60.DLL lives there,
  not in BIN); `sp5c2` = RTM cl / c1 / c1xx 8168 with SP5's back end through cl's `/B2<path to C2.DLL>`.
- The evidence CSV's meta line names the toolchain (`compiler=cl 12.00.8168 (VC6)` unchanged for the default).
- Headers stay RTM's for every toolchain. Reason: SMSG-decomp's build.bat builds sound.dll with SP5's msdev over
  `%VC6%\VC98\INCLUDE` (build.bat line 96: INCLUDE=%DXSDK%\include;%VC6%\VC98\INCLUDE;...; line 98 `call :msdev
  "%VC6SP5%"`). SP5's include directory differs from RTM's in 5 of 956 common files (fpieee.h, fstream, oleauto.h,
  winnt.h, xtree: xtree drops the `_Lockit` locals, fstream swaps fwrite's size / count); `// INCLUDE $VCSP5` places
  it for a file that needs it (no current file does).

Which C2 ran (log/roadmap/S10/5_compid.log, `5_tools/compid.py`): the obj's `@comp.id` symbol is prodid << 16 | build.

| toolchain | cl banner | @comp.id C++ obj | @comp.id C obj |
|---|---|---|---|
| 8168 | 12.00.8168 | 11 / 8168 | 10 / 8168 |
| sp5 | 12.00.8804 | 11 / 8966 | 10 / 8966 |
| sp5c2 | 12.00.8168 | 11 / 8966 | 10 / 8966 |

8966 is only C2.DLL's build in the SP5 tree, and the hybrid (RTM front end, SP5 C2) writes 8966 too, so @comp.id is the
back end's build. sound.dll's Rich header holds (11, 8966) x34 = 34 C++ objects from that C2, which equals the 34
sound.dll sources the bulk run compiles (section 3). The Rich header therefore cannot say which front end ran (U-1870).

## 2. sound.dll near misses in re/match/wip/ re-run with SP5

`5_tools/nearmiss.py` scores every sound.dll line of re/match/wip/*.cpp with `8168`, `sp5` and `sp5c2`: the 121 `// MATCH:`
lines of 23 files (m36b_hand_*, m36c_*) and the 28 `// WIP:` lines of m36a's 12 files (scored as MATCH lines of a scratch
copy `_s105_<file>` beside the original, deleted after each compile; none left). Table: log/roadmap/S10/5_nearmiss.tsv
(file, addr, symbol, ins, score per toolchain), summary 5_nearmiss_summary.txt. The 22 wip files the bulk run of
section 3 wrote are in the glob too and read `err: no Ghidra size` under every toolchain (U-1871).

- Lines at 100% under all three: 86 (older copies of functions since matched in canonical files).
- Lines below 100%: 63, on 54 distinct addresses. SP5 changes the score of ONE: m36a_g2_wip.cpp 0x1000a670
  (Midi_Device::dispatchCommand) 99.6% with 8168, 100.0% with sp5 and sp5c2. That address is already 100% in
  re/match/m36a_h1_midi.cpp with 8168 (another spelling) and C4, so it gives no new canonical file. The other 62 lines
  score the same under all three toolchains.
- The 23 MATCH files compile to byte-identical .text under 8168 and sp5 (sha1 over all .text sections: 23 / 23). The
  12 m36a WIP files were not hashed; m36a_g2_wip.cpp's code differs (its score does).
- Where the 54 addresses stand now: 36 are 100% from SMSG-decomp's sources under sp5 (re/match/smsg_sound_*.cpp,
  section 3), 17 were already 100% in our own canonical files, 1 is matched by neither: 0x10048268 (m36c_48268.cpp,
  static CRT, /O1, 42.9%; sound.pdb has no CRT procedures).

So the near misses were source differences, not compiler differences: SP5's C2 moves 1 of 54. No smsg_soundnm_*.cpp file
was written (none qualifies: the only SP5-only 100% is an address our canonical file already matches). The wip originals
are untouched.

## 3. Bulk smoke run (no --evidence)

`~/bin/py -3.12 re/tools/smsg_import.py project sound/sound.dll/sound.dsp` (log/roadmap/S10/5_sound_bulk.log, every
wrapper with `// TOOLCHAIN sp5`): 34 sources, 1,196 procedures, all mapped; new_100 489, already_100 564, miss 0, nosym
0, folded 0, error 143, verified 489 / 489. All 143 errors are `no Ghidra size for sound.dll 0x...` (U-1871). Outputs:
28 canonical re/match/smsg_sound_*.cpp and 22 wip files (moved to log/roadmap/S10/5_sound_wip/, section 5), runs/sound_*.tsv, summary.tsv lines.
Re-scored with each toolchain (`nearmiss.py --glob 'smsg_sound_*.cpp'`, 5_smsg_sound_tc.tsv): 488 of the 489 new lines
are 100% under 8168, sp5 and sp5c2; one is 100% only with SP5's C2: CDSB::get_emulated_write_info 0x10010920 (99.4%,
157 / 158, with 8168: its `jae` after `cmp ecx, 0x10000` goes to body offset 0xb4 instead of the original's 0xac; U-1872).
Our row for it is DsBuffer::calibratePosition (C2, status dead): no tracker step follows from a match. S10.6 analyses
the rest.

## 4. U-1886: exe literals inside the image range

golf_clean.exe has no base relocations, so match.py took an original operand as an address whenever its value lies
between the first section and SizeOfImage, while the recompiled side masks only relocated fields. A plain constant in that
range (rgb_ycc_start's `mov dword ptr [esp + 0x1c], 0x807fff` = CBCR_OFFSET + ONE_HALF - 1) was masked on one side only.
New rule (score_function, exe only): `literal_values()` collects every immediate / displacement of the obj function that
carries no relocation; an original operand whose value is in that set is compared as the literal. The rule can only turn a
pair equal when both sides encode the same value (with no relocations the original's bytes cannot tell a constant from an
address); DLL scores do not change (their mask is the base relocation table, untouched).

Effect over every exe MATCH line of re/match/*.cpp and re/match/wip/*.cpp (`5_tools/literal_rule.py`, one compile per file,
old rule = literal set empty; 5_literal_rule.tsv, 2,043 rows): 6 lines move, all up, all to 100%, no line moves down.

| addr | file | old | differing pair (old rule) | tracker |
|---|---|---|---|---|
| 0x00466370 | smsg_golf_Golfer.cpp (AdvanceStoryChapter) | 99.5 | `or ebp, 0x800000`, `or ecx, 0x800000` | advanceStory C4 |
| 0x00422fb0 | smsg_golf_GolferShot.cpp (CalculateBestShot) | 99.92 | `or ebx, 0x800000` | chooseShotTarget C4 |
| 0x00424120 | smsg_golf_GolferShot.cpp | 99.96 | `test dword ptr [<addr>], 0x800000` | prepareShot C4 |
| 0x0044fb30 | smsg_golf_InfoScreens.cpp | 99.98 | `or eax, 0x402000` | showCourseReport C4 |
| 0x0048e900 | smsg_golf_jackal_BasePop.cpp (BasePop::exec) | 99.9 | `test dword ptr [ebp + 0x1f68], 0x800000` | TextView::build C3 |
| 0x004b4ca0 | smsg_golf_jpeg_jccolor.cpp (rgb_ycc_start) | 98.2 | `mov dword ptr [esp + 0x1c], 0x807fff` | no row (smsg_exe_1.tsv) |

Every pair is the same flag constant on both sides (log/roadmap/S10/5_literal_moves.log). The five smsg_golf files are a
sibling's (S10.2 bulk; untracked at the start of this step, all rewritten at 15:43:19, after this rule went into the
shared match.py): with the old rule these canonical lines read below 100%, so test_match needs the new rule for them. HEAD's match.py gives 98.2% for rgb_ycc_start (checked with `git show HEAD:re/tools/match.py`).

## 5. Gate

`~/bin/py -3.12 -m pytest tests/test_match.py tests/test_match_tools.py tests/test_match_toolchain.py
tests/test_smsg_import.py -q` (log/roadmap/S10/5_match_tier.log): 481 passed, 6 failed. None of the 6 involves the
selector or the literal rule:
- test_match `every_matched_source_is_100_percent` for smsg_golf_jackal_DataFile, _Dialogs, _EngineInit and
  smsg_golf_jpeg_jmemnobs, and test_smsg_import `canonical_smsg_files_name_their_source` (DataFile): sibling golf
  files from 15:43 that hold only `// ALSO 100%` comments and no MATCH line (smsg_import writes a canonical file when
  every 100% line is already ours). For S10.1 / S10.2: do not write a canonical file with no MATCH line.
- test_match_tools `every_match_annotation_is_a_function_entry`: the wip files of this step's bulk sound run carry the 143
  `no Ghidra size` procedures as `// MATCH:` lines, and the test reads re/match/wip too (rglob). Fix: U-1871 (create the
  functions), or smsg_import writes error rows without `// MATCH:`; either belongs to S10.1 / S10.6 (merge file).
  Resolved for now: the 22 wip files (only those error rows; miss 0, nosym 0, so runs/sound_*.tsv holds the same rows)
  were moved to log/roadmap/S10/5_sound_wip/. After the move, and with the four sibling files gone (a sibling removed
  them), the re-run is in 5_match_tier2.log (numbers in the Status line).
Every existing (non-smsg) re/match file passes test_match with the default toolchain; the default compile is the same
command line and environment as before; the only score changes under the default are the 6 lines of section 4.

tests/test_match_toolchain.py (new, static, 7 tests, 1.6 s): toolchain line default / sp5 / unknown; cl banner and obj
@comp.id per toolchain (8168 -> 11/8168, sp5 -> 11/8966, sp5c2 -> 11/8966); CONTROL: smsg_sound_dsb.cpp 0x10010920 is
100% with its `// TOOLCHAIN sp5` line, 99.4% (157 / 158) when re-scored with 8168 (RED), 100% with sp5c2; U-1886:
rgb_ycc_start 100% with the rule and, CONTROL, 98.2% with the literal set patched empty (RED). Controls 2 / 2 RED, 0 blind.
The file skips when tools/vc6 or tools/vc6sp5 is missing; smsg_sound_dsb.cpp is generated by smsg_import (the control
test skips if it is absent).

## Reproduce

```
~/bin/py -3.12 log/roadmap/S10/5_tools/compid.py                         # banners and @comp.id
~/bin/py -3.12 log/roadmap/S10/5_tools/nearmiss.py                       # wip near misses x 3 toolchains
~/bin/py -3.12 log/roadmap/S10/5_tools/nearmiss.py --glob 'smsg_sound_*.cpp' --out log/roadmap/S10/5_smsg_sound_tc.tsv
~/bin/py -3.12 log/roadmap/S10/5_tools/literal_rule.py                   # U-1886 old vs new rule, exe lines
~/bin/py -3.12 re/tools/match.py <file> --no-evidence --toolchain sp5    # one file with another toolchain
```

## Open

U-1870 (front end of sound.dll not determined), U-1871 (143 sound procedures without a Ghidra function), U-1872 (what
SP5's C2 changes in get_emulated_write_info). smsg_import.py was edited only to emit `// TOOLCHAIN <id>` (was
`// SMSG-TOOLCHAIN`) and to pass the file's toolchain to its own compile (S10.1 accepted; docstring updated).

Status: DONE. Final gate (5_match_tier2.log): test_match + test_match_tools + test_match_toolchain + test_smsg_import 487 passed.
