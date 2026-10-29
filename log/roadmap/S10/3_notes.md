# S10.3 notes: jackal.lib, jpeg.lib, binkw32.lib from SMSG sources (2026-10-29)

Status: DONE. Commands: `~/bin/py -3.12 re/tools/smsg_import.py project golf/jackal.lib/jackal.dsp` (65 sources), then
`file <src> --evidence` for the 49 sources with a canonical file (3_jackal_evidence.log, list 3_evidence_files.txt), and
`project golf/jpeg.lib/jpeg.dsp --evidence` (47 sources, 3_jpeg_run.log).

## jackal.lib (counts per source in 3_counts.md)

65 files, 1,234 procedures: 358 new 100 %, 870 already ours, 4 folded (window.cpp 2, Dialogs.cpp 1, SpriteBox.cpp 1), 2 miss,
0 nosym, 0 compile errors. Canonical files re/match/smsg_golf_jackal_<stem>.cpp (46 after dedupe; 49 written), wip files: BasePop (resolved, below),
multidebug. Names carry `jackal_` because their sources are archived members of jackal.lib; the PDB module is `<stem>.obj` with
lib `..\jackal.lib\Release\jackal.lib`.

Misses (3_miss_diffs.log):
1. BasePop::exec 0x0048e900 (99.9 %, 1418/1419): `test dword ptr [ebp + 0x1f68], 0x800000`, the immediate-in-image-range
   masking of S10.2 cause 1 (U-1855). With the working tree's match.py after S10.5's `literal_values` edit it reads 100 %
   (smsg_golf_jackal_BasePop.cpp now has 28 new lines, verified 28/28) and its wip file is gone.
2. MultiDebug::~MultiDebug 0x0049c780 (92.9 %, 26/28): the original's unwind state numbers are 1 then 0 (`mov [esp+0x10], 1`
   before `call 0x49c0a0`, no byte store before `call 0x486ce0`); our compile of multidebug.cpp numbers them 2, 1, 0
   (`mov [esp+0x10], 2` and an extra `mov byte [esp+0x10], 1`). One member of the class has an unwind state in our compile that
   the original lacks. Not resolved (U-1860); no hand file.

## jpeg.lib (3_jpeg_counts.md)

47 files (CreateJPG.c and the 46 jpeg-6a sources), 151 procedures: 26 new 100 %, 125 already ours, 0 folded / miss / nosym /
error. The 26 new lines are in re/match/smsg_golf_jpeg_*.cpp (12 files after dedupe; 13 written); the existing golf_jpeg_*.cpp wrappers keep their lines
(no file edited). The 24 libjpeg statics of 8_jpeg_autoname.log are all in the 26 (comm of the two address lists: nothing of the
log is missing). The two extra lines: 0x004ae020 `CreateJPG` (CreateJPG.c) and 0x004a0890 `jpeg_mem_term`, an empty `ret` that
the linker folded with ten other empty functions (see "One line per address"). No jpeg row is a C4 candidate (inventory
c4_candidate = no for all 151: each is already C4 by match or has no C3 row), so 3_promote_cmds.txt has no jpeg commands.

## One line per address (found in this step, applies to S10.2 and S10.4 too)

smsg_import.py skips only addresses held by non-smsg files (so a re-run is idempotent), hence an ICF-folded address got a MATCH
line in every smsg file with a procedure there: 28 duplicate lines over 12 addresses (0x004a0890 in 11 files, 0x00479820 in 5,
0x00402180 / 0x00402190 / 0x0047e5e0 / 0x0043cbc0 in 3, six more in 2). Every one read 100 % (identical bytes), so no wrong
evidence, but the rule is one canonical line per address. New tool `re/tools/smsg_dedupe.py <prefix> [--write]` keeps the line in
the first file by name (S10.1's Geometry / Plot / InputPoll first, never rewritten) and turns the others into `// ALSO 100%,
already in <file>:` comments; run on prefix smsg_golf_ it rewrote 28 lines and removed the four files that held only a folded
line (jackal DataFile, Dialogs, EngineInit; jpeg jmemnobs: the match tier requires a MATCH line per file); a second run reports 0.
The run summary rows of those four still read new_100 = 1. Run it after every bulk run:
re-running smsg_import.py on a file puts its folded lines back. The extra evidence CSVs of the rewritten lines stay in log/diff
(all GREEN).

## Interaction with S10.5

Midway the working tree's re/tools/match.py gained `literal_values` (S10.5's fix for the immediate masking): two evidence
runs failed with `NameError: name 'literal_values' is not defined` while the file was half edited (sprite.cpp and ScratchSprite.cpp;
re-run, both verified). With the new match.py the 0x800000 / 0x402000 misses read 100 %: Golfer, InfoScreens and BasePop lost
their wip files, GolferShot kept 1 miss (ProcessHoleCompletion) and Toolbar 2 (RenderBuildingToolbar, RenderEmployeePanel).
These runs depend on S10.5's uncommitted match.py: if the lead rejects it, 5 canonical lines (AdvanceStoryChapter,
CalculateBestShot, PlanGolferShot, RenderHoleInfoOverlay, BasePop::exec) go back to miss.

## binkw32.lib

binkw32.dsp builds `binkw32.c`, a stub DLL (every function `return 0` or empty, `__declspec(dllexport)`) used to link the
import library; golf.pdb holds no procedure from it (module `binkw32.dll` has 4 import-table records, 0 code procedures) and
golf_clean.exe holds only the `jmp [IAT]` thunks of its imports. smsg_import.py has no PROJECTS entry for it by design: nothing
to match (U-1861).

## C4 candidates and gates

77 jackal.lib rows (C3, no MATCH of ours, status 100 in the inventory): exact commands with CSV names in
3_promote_cmds.txt (made by log/roadmap/S10/promote_cmds.py golf jackal.lib/src; the orchestrator runs them, I ran none).
Total after S10.2 + S10.3: 25 + 77 = 102 of the inventory's 103 candidates; the 103rd is 0x00434cf0 (miss).
Regenerate the CSVs first (log/diff is gitignored): the loops in 2_notes.md / 3_evidence_files.txt.
- `~/bin/py -3.12 -m pytest tests/test_match.py tests/test_match_tools.py -q`: 3_match_tier.log (final run after dedupe, shared by S10.2-4): 473 passed, 0 failed. Earlier runs:
  3_match_tier_second.log 472 passed, 1 failed:
  test_every_match_annotation_is_a_function_entry fails only on smsg_sound_Cmix.cpp / smsg_sound_ambiencemgr.cpp (S10.6's files,
  not this step's); none of the S10.2 / S10.3 files is in its list. A first run (2_match_tier.log) also failed
  test_queue_skips_matched_and_non_game_functions because a wip file of mine was deleted by a parallel `file` run while the test
  read it: not a defect of the files (re-run passed).
- Controls: S10.1's (Course.cpp under release flags reads miss; overload mapping). New control for the dedupe: a second run of
  smsg_dedupe.py reports 0.

## Uncertainties (3_uncertain.tsv, U-1860 .. U-1864)

U-1860 MultiDebug dtor unwind states, U-1861 binkw32 has nothing to match, U-1862 duplicate lines across smsg files (smsg_import.py
skips only non-smsg holders; fix in the tool belongs to S10.5's owner), U-1863 evidence for S10.2 / S10.3 files was written
with a match.py under edit (two runs failed and were repeated; the CSVs are from the final tool), U-1864 the 0x004a0890 empty
function has 11 sources (which one the original linker kept is not determined; same as U-1850).
