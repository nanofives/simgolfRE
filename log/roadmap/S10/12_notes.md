# S10.12 notes: residual match misses (2026-10-29)

Status: DONE (table complete; 2 new 100% lines under SP5, 1 under a corrected extent, 3 left as measured misses).
Rules followed: every file write at 60 lines or fewer; runs over 2 minutes in the background teed to log/roadmap/S10/12_*.log;
at most 2 compile jobs (permuter `--jobs 2`, one at a time); no `--evidence` anywhere (no CSV written to log/diff); no edit of
any existing file (re/match, re/tools, wave-1 notes untouched); `~/bin/py -3.12` only.

## 1. The residual list

`12_tools/residual.py` (log: 12_residual.log, raw: 12_residual_raw.tsv) collects every `// MATCH:` / `// WIP:` address of
re/match/wip/*.cpp (191 distinct addresses, exe and sound.dll; no wip line names jgld.dll or Terrain.dll), compiles every canonical
re/match/*.cpp that names one of them (53 files, default compile, no evidence) and keeps the addresses no canonical file scores
100%: 5 addresses.

| module | addr | wip source | stands |
|---|---|---|---|
| golf_clean.exe | 0x00427380 | smsg_golf_GolferShot.cpp | S10.2 miss (ProcessHoleCompletion) |
| golf_clean.exe | 0x00434cf0 | smsg_golf_Toolbar.cpp | S10.2 miss (RenderBuildingToolbar = drawAmenitiesPanel) |
| golf_clean.exe | 0x00436e50 | smsg_golf_Toolbar.cpp | S10.2 miss (RenderEmployeePanel) |
| golf_clean.exe | 0x004a55d2 | m36d_wip_ehvec.cpp | m36d near miss, 81.8% |
| sound.dll | 0x10048268 | m36c_48268.cpp | m36c near miss, 42.9% |

The other 186 addresses are 100% in a canonical file (older wip copies since matched, SMSG sources of S10.2 / S10.4, m36 hand files).
MultiDebug::~MultiDebug 0x0049c780 (S10.3's miss, wip/smsg_golf_jackal_multidebug.cpp, 92.9%) is not in the 5: our own
re/match/golf_hand_04.cpp:243 matches it at 100% (symbol C49c780), so the address has its canonical line. It is a row of the
table because the SMSG source still misses it. Final table: **12_residual.tsv** (6 rows, one per address).
The tracker rows behind them: 0x00427380 matchUpdate C4 (scenario), 0x00434cf0 drawAmenitiesPanel C3, 0x00436e50
drawEmployeeDetailPanel C4 (scenario), 0x0049c780 globalObjDtor49c780 C4, 0x004a55d2 ehVectorConstructorIterator C4; 0x10048268 has no
hooks.csv row (it is a callee of CIsqrt / CIlog, both C2 `dead`).

## 2. Per address

### 0x00434cf0 RenderBuildingToolbar and 0x00436e50 RenderEmployeePanel: SP5 reads 100%
- Copies for the permuter: re/match/wip/s10_12_Toolbar.cpp (header of the wrapper + Toolbar.cpp + the MATCH lines moved above their
  functions, because match_permute finds a function by the `{` after its MATCH line; line 1696 of the copy carries my
  operand-swap experiment) and re/match/wip/s10_12_GolferShot.cpp (same recipe).
- The a / b order of match.py's diff is (recompiled | original). Measured diff of 0x00436e50: recompile `add ecx, eax; cmp ecx, 0x14`,
  original `add eax, ecx; cmp eax, 0x14` (S10.2's notes had it the other way round) at the second `count + scroll < 0x14` of
  Toolbar.cpp (the hire branch, call 0x0040aa80); the first occurrence (`>= 0x14`, call 0x00473cb0 before it) matches. The loads are
  equal: eax = [0x005aa6c0], ecx = [0x005aa558].
- 0x00434cf0: original `mov esi, [esp + 0x28]` right after `call 0x00432620`, then `mov edi, [esp + 0x24]`; the compile loads edi first
  (one instruction pair swapped, score 681/682).
- match_permute (kinds commute, move, swap, cmp, neg, incform, initsplit, tern, ifelse, plus a 25-step walk): 0x00436e50 151 + 211
  compiles, 0x00434cf0 145 compiles (rounds 3, walk 25), no round improved (log/roadmap/S10/12_permute_*.log). Hand variants of the
  second compare (12_tools/variants.py + v436e50.json, 12_variants_436e50.log): operand order, literal on the left, `<= 0x13`,
  `a < 0x14 - b`: 99.7, 99.7, 99.6, 97.8 and a subtraction form 92.5.
- **`--toolchain sp5` (cl 12.00.8804 with c2 8966): both read 100.0%** on the UNMODIFIED third-party Toolbar.cpp (682/682 and
  759/759); sp5c2 (RTM front end, SP5 back end) reads 99.9 / 99.7 as the default does (12_exe_toolchains.log). So the difference is in SP5's
  front end (c1xx 8964), not in its back end. ProcessHoleCompletion reads 99.9% under all three toolchains.
- New canonical file re/match/smsg_hand_toolbar_sp5.cpp: wrapper with `// TOOLCHAIN sp5`, `// COMPILE` of the unmodified Toolbar.cpp and
  the two MATCH lines; `pytest tests/test_match.py -k smsg_hand` 1 passed, test_match_tools + test_smsg_import 18 passed.
  **No evidence CSV was written** (see section 3).

## 3. Why the SP5 result for the exe is not taken as C4 evidence (U-1910)

- golf_clean.exe's Rich header (12_tools/rich.py, 12_rich.log) lists 152 C objects and 109 C++ objects at build 8168 and no object
  at 8966 (jgld.dll and Terrain.dll the same). sound.dll's Rich header is the one with 34 C++ objects at 8966 (S10.5).
- Every canonical exe line of the 90 re/match/smsg_golf_*.cpp files (609 lines) re-scored with `--toolchain sp5`
  (12_tools/exe_sp5.py, 12_exe_sp5.log / 12_exe_sp5.tsv): 602 stay at 100%, 7 drop (MainLoop 91.45, RenderGolferInfoPanel 94.73,
  SaveCourseToFile 97.40, RenderFinancialReportScreen 98.84, ClassList<JackalVoiceRx>::add 99.47, ResetAllGameState 99.72, OpenHoleForPlay
  99.77), none rises. So SP5's compiler and the original's agree on most functions and differ on both sides of these.
- Reading: the original was compiled by 8168, a function whose 8168 compile differs from the original is a source difference, and SP5
  reaching 100% on the two Toolbar functions is a coincidence of SP5's front end with the unknown original source, not a proof of the
  source. The file is canonical because the match tier is mechanical (100% against the anchors under the file's declared toolchain, as for
  sound.dll since S10.5), but nobody should promote on it without the lead's decision. Hence: no `--evidence` run, and the promote
  command for 0x00434cf0 (C3, the only row it could lift; 0x00436e50 is already C4 by scenario) is in 12_promote_cmds.txt behind a comment.

## 4. 0x00427380 ProcessHoleCompletion

99.9% (1815/1816): `mov esi, [esp + 0x38]` appears one instruction earlier in the original (S10.2 / S10.3 diff, 2_miss_diffs.log).
match_permute over re/match/wip/s10_12_GolferShot.cpp (4,743 lines, one compile of the copy is several seconds): 3 rounds with
the default kinds, round 1 of 756 variants found nothing, then a 20-step walk: 796 compiles, best 99.9% (regs-renamed 99.9, in place 99.9),
12_permute_427380.log. sp5 and sp5c2: 99.9. Class: scheduling (a reload of a callee-saved register placed after the next instruction).
Not closed.

## 5. 0x0049c780 MultiDebug::~MultiDebug

Original unwind states: `mov [esp+0x10], 1` before `call 0x0049c0a0`, then 0; no byte store before `call 0x00486ce0`. The SMSG source's
`MultiDebug` keeps `Time m_timer`, whose own destructor is a third state (2, 1, 0 in the compile). Our golf_hand_04.cpp line 234 records
the same finding for the same function: the 0x00486ce0 call is made from the body, "a member the compiler destroys would add an EH state
the original lacks", and its C49c780 reads 100% (the address is canonical there). Not re-tried on their source: it would need their
header's Time dtor removed (a layout-changing edit of third-party headers) and the address already has its canonical line.
Class: EH states.

## 6. 0x004a55d2 eh vector constructor iterator

re/match/wip/m36d_wip_ehvec.cpp reads 81.8% (36/44) because the recompiled COMDAT holds the `__finally` funclet after the body while Ghidra's
extent of 0x004a55d2 is the body only (106 bytes, re/functions_ghidra.tsv line 1872) and the funclet is the contiguous entry 0x004a563c
(24 bytes, line 1873). With the original extent 130 bytes (0x004a55d2..0x004a5654) the same source reads **100.0% (44/44)**
(12_tools/extent_try.py monkeypatches match.ghidra_size for one run; 12_ehvec_extent130.log). The source is re/match/wip/s10_12_ehvec.cpp
(unchanged code, header rewritten). It is not canonical until `re/match/boundaries.tsv` carries the row in 12_merge.md (a shared file; wave 1
is merging, so it is not edited here). The funclet row 0x004a563c (ehVectorCtor_finally, C2, held) is the other half of that extent.

## 7. sound.dll 0x10048268

The static CRT's exponent-field helper (names row sound_8 `doubleExponentField`). Original (match.py --show): `mov eax,[esp+8]; and eax,0x7ff00000;
cmp eax,0x7ff00000; je L; ret; L: mov eax,[esp+8]; ret`. Every C spelling compiled so far keeps the mask in ecx (`mov ecx, 0x7ff00000`, `jne`)
at /O1: `==` + early return, `!=`, ternary (25.0), signed (42.9), xor (37.5); /O2 /Ox /Od /Os give 42.9 / 42.9 / 14.3 / 15.4 (the last two have a
frame). Toolchains 8168 / sp5 / sp5c2 all read 42.9. match_permute: 34 compiles, no change (12_permute_48268.log, 12_variants_48268.log). The
shape (immediate operands repeated, a bare `ret` after the `je`) is the shape of hand-written assembler; an `__asm` body is a transcription, which
the project does not count (CLAUDE.md, round 36 evidence decisions). Class: different sequence. Not closed.
