# S10.8 notes: golf.pdb names against ours (2026-10-29, claude2 worker)

RE names work only: no port/, shim/, original\, Ghidra master or tracker write. Every claim below rests on our disassembly,
read with capstone, re/tools/xref.py, pointer_callers.py, match.py, match_autoname.py and log/c3/m36e_lib_match.py. golf.pdb
names were used as leads only.

## 1. Counts

Diff of golf.pdb procedures against re/names, re/functions_ghidra.tsv and hooks.csv: `~/bin/py -3.12
log/roadmap/S10/8_tools/diff.py` writes log/roadmap/S10/8_diff.tsv. Classes:
- named 1,164: both sides have a specific name. Only the five contradictions the investigation listed were reviewed.
- GENERIC 641: our name is mechanical (address suffix, slotN, fwdN, dynInit_ / staticInit_ / exitDtor_ ...).
- lib 147: jpeg.lib, all already named.
- MASTER_ONLY 5: thunks of 5 bytes.
- NEW 1: 0x0047e770.
FUN_ functions without a names row: `8_tools/funs.py` writes 8_norow.tsv (975 rows without a row, 171 FUN_). 42 of the FUN_
rows have a golf.pdb procedure or public at their address.

| item | proposed | accepted | rejected / not taken (reason) |
|---|---|---|---|
| new rows, re/names/smsg_exe_1.tsv | 42 | 42 | 0. jpeg_fdct_islow is accepted through its installer only (U-1885) |
| contradictions | 5 | 4 renames, 1 keep | loadInterfaceArt kept: both names partial, 31 port files use the name |
| VIP homes (0x0056d1b8) | 10 renames | 10 | settled: celebrity vacation-home residents |
| NodeList forwarders | 28 resolved | 10 renames | 15 reach unnamed sound.dll bodies (S10.9), 2 reach constant stubs, 1 NodeListB reaches an unnamed body |
| GENERIC $E initializers | 356 | 0 | compiler-generated names; ours carry the target |
| GENERIC class ctors, dtors, deleting dtors | 71 | 0 | class names have no support in our binary (no RTTI, no class-name string) |
| GENERIC other methods | 214 | 10 (the NodeList rows above) | 204 not evaluated, see U-1889 for the method |
| golfer U-rows U-0009..U-0016 | 8 | U-0010, U-0015, U-0016 resolved, U-0009 partial (4 bits) | U-0011..U-0014 stay open: golf.pdb names only, no deciding code read |

## 2. The five contradictions (details and citations: 8_merge.md section 2)

- 0x004038f0 drawFlyingBall vs RenderEmployees: THEIRS. Our own C3 Purpose already describes the 64-record walker loop
  0x00585850 plus the golfer carried by the mouse (selection >= 0x80, 0x00403901). The string `I'm flying!` 0x004c1564 is the
  carried golfer's label. Proposed name: drawWalkers.
- 0x00442180 loadInterfaceArt vs ResetAllGameState: BOTH PARTIAL. It resets the game tables (rep stosd
  0x004421a9..0x0044227c, golfers at 0x004421be) and, when its argument is non-zero, loads the art. Keep, given the port's use.
- 0x0040e720 announceBuilding vs OpenHoleForPlay: THEIRS. It validates and opens the finished hole (tee / pin tests, yards,
  par, statistics reset); the building text is an optional tail. Proposed: openHole.
- 0x0042dea0 holeMagazineEvent vs CalculateAllHoleStatistics: THEIRS. A per-tick rating pass over the 18 holes; the
  magazine events are one branch. Proposed: rateCourseHoles.
- 0x00422530 rateTile vs CalculateGolferMaxDistance: THEIRS. A golfer's reach in yards (150..330). chooseShotTarget compares
  sqrt(dx*dx + dy*dy) at 25 per tile with reach(g) + reach(g, strokes + 1) at 0x004231b8..0x004231ca. Proposed:
  golferShotReach.

## 3. 0x0056d1b8 records = VIP homes (settled, ours)

mainLoop 0x00417a5a..0x00417c17 picks a celebrity from g_celebrities 0x0055d738, posts the string 0x004c6444 (`has purchased
a vacation home at your golf course!`), and calls spawnObject with Random::range(6) and the type-5 lot's tile. spawnObject
copies the celebrity kind byte (0x0040120a) and places the record in a free cell of g_recordBanks layout a's 6x6 grid. The six
g_recordBanks layouts are the home plots that drawRecords draws on type-5 lots (0x00413f3e). The habitat code (0x572cb0) is
the real animal code and is unaffected. Renames and model rows: 8_merge.md section 2b.

## 4. Golfer fields (8_merge.md section 3)

- U-0010 +0x19: skills byte. Bit 0 adds 50 yards of reach. The high nibble holds g_challengeLevel, set when the challenge
  golfer is set up (0x0040f437..0x0040f44c). Resolved for the writer.
- U-0015 +0xbc: scenic-bridge memory, write-only (0x0042b2de..0x0042b34f; texts 0x0046b60a / 0x0046b63e / 0x0046b672).
  Resolved.
- U-0016 +0xfc..+0xff: padding. Zero in all 46 shipped .pro / .chr copies at file offset 0x71e..0x721. Resolved.
- U-0009 +0x10: bits 0x4 / 0x8 / 0x10 = used a placed object of kind 0xa / 8 / 6 with a fee (0x0042ac9d / 0x0042abed /
  0x0042ab2c). Bit 0x10000 = rides (0x0046b5fe). The rest stay open (U-1887).
- Leads only, still open (golf.pdb Golfer struct, offsets computed from base 0x005794b8):
  - U-0011 +0x1b shotDifficulty;
  - U-0012 +0x1d activity (ACTIVITY_SWING 0, STRIKE 5, FOLLOW_THROUGH 6, IDLE 7..10, WALK 11, PLEASED 12, ANNOYED 13,
    TALK 14, STEP_UP 16, SIT_DOWN 17, GET_UP 18, WASH_BALL 19), which equals our recorded value set, and +0x20 shotStep;
  - U-0013 +0x36 attitude;
  - U-0014 +0xae rapport.
  Their struct places those names exactly at our open offsets (log/roadmap/S10/8_notes.md, this list); the meanings still
  need our own reads.

## 5. Gate (reproduce)

```
~/bin/py -3.12 log/roadmap/S10/8_tools/gen_new.py            # regenerates re/names/smsg_exe_1.tsv from the logs below
~/bin/py -3.12 re/tools/names_sanitize.py re/names/smsg_exe_1.tsv re/analysis/systems/smsg_exe_1.md
~/bin/py -3.12 re/tools/names_validate.py smsg_exe_1
~/bin/py -3.12 re/tools/names_check_evidence.py re/names/smsg_exe_1.tsv
```
Inputs:
- log/roadmap/S10/8_crtlib.log + 8_crtlib/*.csv (`log/c3/m36e_lib_match.py tools/vc6/vc98/lib/libcmt.lib golf_clean.exe ...`);
- 8_jpeg_autoname.log (match_autoname over re/match/golf_jpeg_{datadst,error,cmarker,chuff,cdctmgr,cprepct,csample,ccolor}.cpp
  and 8_tools/jpeg_fdct.cpp);
- 8_jpeg_installers.txt (pointer_callers.py);
- 8_jpeg_probe.log (`match.py 8_tools/jpeg_probe.cpp --show --no-evidence`).
The only match / evidence CSVs written go to log/roadmap/S10/8_crtlib/. log/diff is untouched (match.py ran with
--no-evidence; match_autoname writes no CSV).

## 6. Tools (log/roadmap/S10/8_tools/)

diff.py, funs.py, sgdis.py (capstone over golf_clean.exe .text: ranges or --grep), fwd.py (NodeList forwarders to sound.dll
vtable slots), gen_new.py, jpeg_fdct.cpp / jpeg_probe.cpp (scratch wrappers, never canonical sources), ml417a.txt (mainLoop's VIP
block disassembly).

## 7. Open items

- U-1885..U-1889 (8_uncertain.tsv).
- Every rename of an existing row except the NodeList ones touches names the port uses as registry, vector and golden keys
  (8_merge.md marks them P). The lead decides between port aliases and renaming only re/names + Ghidra.
- 204 generic exe method rows were not evaluated. The fwd.py method extends to Snd / VoiceRx once S10.9 names the sound.dll
  bodies.
- No Ghidra was opened. No tool call was refused.
