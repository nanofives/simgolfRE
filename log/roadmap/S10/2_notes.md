# S10.2 notes: golf.exe/src from SMSG sources (2026-10-29)

Status: DONE. `~/bin/py -3.12 re/tools/smsg_import.py project golf/golf.exe/golf.dsp` over all 34 sources (jgldefs.cpp has no
procedures), evidence run on the canonical files only, inventory re-run, C4 candidates listed. Tool: re/tools/smsg_import.py
(S10.1, unchanged), re/tools/smsg_hand.py (new, hand-fix wrapper maker; no hand file survived, see "Hand fixes").

## Numbers (latest run per source, log/roadmap/S10/runs/summary.tsv; table in 2_counts.md)

34 files, 722 procedures: 252 new at 100 % (22 of them S10.1's three pilot files), 460 already ours at 100 %, 7 folded
(ICF, the address is already 100 % from another procedure), 3 miss, 0 nosym, 0 compile errors, 716 mapped by name or public.
The first full run (before S10.5's match.py fix, see "Misses") read 248 new and 7 miss; the numbers here are the final ones.
Every canonical file re-verified by match.compare right after writing (verified=n/n in every row). golf.pdb has 2,107
procedures, 722 in golf.exe/src, the rest in jackal.lib (S10.3). Inventory: `smsg_100` 708, `our_match` 1,366 (was 1,346).

Canonical files (new): smsg_golf_{Employee,EmployeeRender,FlicChannels,Globals,SpriteAssets,MainLoop,GolferSpawn,GolferShot,
WildlifeUpdate,Toolbar,SetupScreens,FlicBanks,TerrainView,InfoScreens,StringPool,GameMain,InputWait,GolferPanel,GolferPairing,
TerrainInfluence,ShadeTable,RenderQueue,Screen,ThemeLoader,Golfer,GolferDialogue,Tournament,ListDialog,WorldMap}.cpp. Wip files
(misses): re/match/wip/smsg_golf_{Golfer,GolferShot,InfoScreens,Toolbar}.cpp. Evidence: 29 files re-run with `--evidence`
(2_golf_evidence.log, list 2_evidence_files.txt), 247 `*.match.csv` written, all from 100 % lines.

## Misses (7 in the first run, 3 left; diffs of the first run in 2_miss_diffs.log), grouped by cause

1. Immediate in the image range read as an address (5): AdvanceStoryChapter 0x00466370 (99.5 %, `or ebp, 0x800000` and
   `or ecx, 0x800000`), CalculateBestShot 0x00422fb0 (99.9 %, `or ebx, 0x800000`), PlanGolferShot 0x00424120 (100.0 %,
   2649/2650, `test dword ptr [0x59e7b8], 0x800000`), RenderHoleInfoOverlay 0x0044fb30 (100.0 %, 4714/4715,
   `or eax, 0x402000`). The instruction text is identical on both sides; match.py masks the original operand because it lies in
   the exe's range 0x400000..0x8xxxxx and the exe has no relocations, while the recompiled obj holds a literal. This is U-1886
   (rgb_ycc_start 0x807fff). The fix is S10.5's (match.py ownership): `0x800000` and `0x402000` are flag constants (g_flags bit 23).
   RESOLVED during this step: S10.5's working-tree match.py (`literal_values`) reads all five at 100 %; they are canonical lines now
   (Golfer, GolferShot, InfoScreens, Toolbar and BasePop wip files re-run with --evidence). Depends on that match.py being accepted.
2. Placement of one load (2): ProcessHoleCompletion 0x00427380 (99.9 %) and RenderBuildingToolbar 0x00434cf0 (99.9 %): the
   original loads `mov esi, [esp + N]` (0x38 and 0x28) one instruction earlier than we compile it (VC6 /O2 scheduling).
3. Operand order of a sum (1): RenderEmployeePanel 0x00436e50 (99.7 %): the original computes `g_employeeListCount +
   g_employeeListScroll` after the Draw call at 0x00473cb0 as `add ecx, eax; cmp ecx, 0x14` (eax = [0x5aa6c0], ecx = [0x5aa558]) and we emit `add eax,
   ecx; cmp eax, 0x14`. Swapping the operands in the source (line 1681 of Toolbar.cpp) left the score at 99.7 %: VC6 orders the
   two loads by variable, not by source position.

## Hand fixes

Tried for cause 3 only: a copy of Toolbar.cpp (re/match/smsg_hand_src/, wrapper smsg_golf_hand_01.cpp via smsg_hand.py) with
the operands swapped: 99.7 % unchanged; a second variant (`(a + b) - 0x14 < 0`) changed the branch. Both removed. Cause 1 needs
no hand file (the text is equal). Cause 2 was not attempted with match_permute.py (the permuter rewrites the function text in
its source file, which here is a third-party file compiled through `// COMPILE`); U-1856.

## C4 candidates (inventory column c4_candidate = yes, golf.exe/src)

26 C3 rows have no MATCH of ours and a source here: 25 are 100 % (promote commands in 2_promote_cmds.txt, produced by
log/roadmap/S10/promote_cmds.py from the inventory, CSV names from log/diff), 1 is a miss (0x00434cf0, drawAmenitiesPanel =
their RenderBuildingToolbar, cause 2). The 25 include gameMain 0x0045baf0, loadWorldAssets 0x0043dbe0, loadInterfaceArt
0x00442180 (their name ResetAllGameState: the contradiction S10.8 settled), loadInfoScreenArt 0x0044bde0, drawWorldObjects
0x00430360, showCharacterEditor 0x004385d0 and the browser / report screens. The two S10.1 rows (drawRecords 0x004012d0,
appendWalkerName 0x00467600) are in that list too. 77 further C4 candidates sit in jackal.lib (S10.3). `re_classify.py` is run
by the orchestrator; I ran none. Regenerate the CSVs before promoting (log/diff is gitignored):
`for s in $(cat log/roadmap/S10/2_evidence_files.txt); do ~/bin/py -3.12 re/tools/smsg_import.py file $s --evidence; done`.

## Finding: mainLoop

`MainLoop.cpp` gives 0x0040f5c0 `MainLoop` (75,108 bytes, the whole body incl. its 27 switch tables) at 100 % from their source,
plus AssetSurface::Create 0x00421b30, SpriteObject::BlitScaled 0x00421b90 and OnFrameIdle 0x004a0890. mainLoop is C2 (hold
NO_PATH1: no path-1 A/B can exist for a once-per-process function), so the inventory does not list it as a C4 candidate and a
match is not a C2 -> C3 step (U-1853). The 100 % line is in re/match/smsg_golf_MainLoop.cpp with a match CSV
(log/diff/0040f5c0__MainLoop__YAXXZ.match.csv) for the orchestrator to weigh against the c3fy hold.

## Gates

- `~/bin/py -3.12 -m pytest tests/test_match.py tests/test_match_tools.py -q`: log/roadmap/S10/2_match_tier.log (final run after dedupe: 473 passed, 0 failed; the first run, 2_match_tier_first.log, had 2 failures on S10.6's sound files and a transient wip file of this step).
- Controls: S10.1's tests/test_smsg_import.py controls stand (Course.cpp under release flags reads miss). Miss rows read 99.5 ..
  100.0 % and never reach a canonical file: the 3 left stay in wip. Wip files now: GolferShot (ProcessHoleCompletion), Toolbar (RenderBuildingToolbar, RenderEmployeePanel).

## Uncertainties (2_uncertain.tsv)

U-1855 (immediate masking, S10.5), U-1856 (load placement in ProcessHoleCompletion / RenderBuildingToolbar), U-1857 (sum operand
order in RenderEmployeePanel), U-1858 (mainLoop 100 % vs the NO_PATH1 hold), U-1859 (folded rows: 7 rows share an address with
a procedure that already scored 100 %).

## One line per address

smsg_import.py leaves duplicate MATCH lines for ICF-folded addresses across smsg files (U-1862, found in S10.3: 28 lines, 12
addresses, e.g. 0x004a0890 in 11 files). `re/tools/smsg_dedupe.py smsg_golf_ --write` turned the extra ones into `// ALSO` comments
(four files that held only a folded line were removed, see 3_notes.md; MainLoop.cpp's OnFrameIdle line is now one of them, kept in smsg_golf_Golfer.cpp as OnTraitMatched). S10.1's three files are never
rewritten. Run it after any re-run of `file` / `project`.
