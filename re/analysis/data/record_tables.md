# Record tables next to the golfer records (golf_clean.exe)

Phase 1 of `re/PORT_PLAN.md`. Each table was scanned with `re/tools/record_fields.py` (capstone over every function
range in `re/functions_ghidra.tsv`; every memory displacement or immediate inside the table, grouped by
`(address - base) mod stride`, plus a linear sweep of the code outside every function range, where mainLoop's far
blocks 0x004115dc.. lie; those rows are named `(no function)`). Full reference lists: `log/data/<table>.tsv`, per-field summaries
`log/data/<table>.txt`, disassembly windows `log/data/<table>_ctx.txt` (not committed; rerun the commands below).
The golfer records themselves are in `golfer_record.md`.

Base, stride and count are taken from instructions that touch the whole table (block clears, loop bounds, save
calls), not from the scan, which only lists what code addresses directly. A field reached only through a pointer
that is advanced at run time (e.g. `mov ebp, 0x58586e` then `[ebp - 8]`) shows up only as a `reg` row, which follows
registers in straight-line decode order and is a lead, not evidence.

| table | base | stride | count | bytes | evidence for base / count |
|---|---|---|---|---|---|
| walker / employee records `g_walkers` | 0x00585850 | 0x4c | 64 | 0x1300 | `rep stosd` 0x4c0 dwords from 0x585850 (0x004421c5..0x004421cf, loadInterfaceArt); save block (0x00585850, 0x1300) pushed at 0x0040b116; loop end `cmp eax, 0x586b62` = base + 0x12 + 64*0x4c (0x0042774c, matchUpdate) and `cmp ebx, 0x586b63` (0x0043726e) |
| per-golfer-class statistics `g_golferStats` | 0x0056ae70 | 0xb8 | 32 | 0x1700 | save block (0x0056ae70, 0x1700) pushed at 0x0040b23c; index = `(signed char)golfer[+0xba] + 4 * (golfer[+0x19] & 7)` (0x0042d475..0x0042d4a2, 0x0042740d..0x00427433), 4 x 8 = 32 |
| per-type course state `g_golferTypeState` | 0x005849e0 | 0x2c | 84 slots, 76 used | 0xe70 | save block (0x005849e0, 0xe70) pushed at 0x0040b293 (0xe70 / 0x2c = 84, and 0x5849e0 + 0xe70 = 0x585850, the next table); every loop ends after 76 records: `cmp eax, 0x5856f2` (0x0042ded0, 0x00455720), `cmp edx, 0x5856f3` (0x0045564f), `cmp eax, 0x58571a` (0x00406a8e, 0x00454c72) |
| story definitions `g_storyDefs` | 0x0053a454 | 0x32 | 100 | 0x1388 | `rep stosd` 0x4e2 dwords from 0x53a454 (0x004659e8..0x004659f4) and `rep movsd` 0x4e2 dwords from 0x0080b130 (0x00465abe..0x00465acd), both in loadGolferRoster; index = Random::range(100) (`push 0x64`, 0x0045dfd4..0x0045dfe9) times 50 (`lea eax,[edi+edi*4]; lea ecx,[eax+eax*4]; [ecx*2+0x53a454]`, 0x0045e001..0x0045e007) |

## g_walkers 0x00585850 (64 x 0x4c): walkers and employees

The records are created by `spawnWalker` 0x00402970 and moved by `updateWalkers` 0x00402a40. `hireEmployee`
0x0040aa80 calls spawnWalker and then writes the new record's +0x10/+0x11/+0x12/+0x16/+0x22 and +0x00/+0x04
(0x0040aba5..0x0040acc0); `drawEmployeeDetailPanel` 0x00436e50 reads +0x22/+0x24/+0x26; `handleEmployeeAction`
0x00436c00 copies the rename text (`textInputBox` 0x0045b2c0 with limit 0x1f, prompt string 0x004c837c, result in the
text buffer 0x0051a068) into +0x2a (0x00436d1f..0x00436d47). So the table holds the staff as well as the ambient
walkers; which value of +0x12 / +0x13 marks an employee is not determined here.

| off | width | sign | accesses (functions) | meaning from the cited code |
|---|---|---|---|---|
| +0x00 | 4 | | 31 refs: spawnWalker, updateWalkers, hireEmployee (w 0x0040ac84), updateGolfers (w record 0, 0x00428e90), customizeCharacter / loadGolfPro (r record 0) | world x (same units as golfer +0x00; record 0 receives a golfer's x at 0x00428e7e..0x00428e90) |
| +0x04 | 4 | | 28 refs, same functions (w 0x0040acae, 0x00428e96) | world y |
| +0x08 | 4 | | read in mainLoop's far code (0x0041f006, `[esi - 0xb]` with esi = record + 0x13 from `mov esi, 0x585863` at 0x0041eff5); 2 `reg` rows in drawFlyingBall | not determined |
| +0x0c | 4 | | read in mainLoop's far code (0x0041f014); 1 `reg` row in drawFlyingBall | not determined |
| +0x10 | 1 | s (7) | spawnWalker, updateWalkers, hireEmployee (w), stampCourseTiles (w record 0, 0x0040e2a5), announceBuilding (w record 0, 0x0040f07d), drawEmployeeDetailPanel | not determined |
| +0x11 | 1 | s (8) | same set as +0x10 (w 0x0040abb2, 0x0040e2aa, 0x0040f095) | not determined |
| +0x12 | 1 | | 32 refs, 9 fns incl. matchUpdate loop from 0x00585862 | pickGolferName takes bit 3 (`and edx, 8; shr edx, 3`, 0x00467629..0x00467635) into its default-name index; the rest not determined |
| +0x13 | 1 | s | spawnWalker, updateWalkers, drawFlyingBall, matchUpdate, drawEmployeeDetailPanel, showRoutingMapScreen, pickGolferName | compared with 0xfa (-6) at 0x0046763e and 0x00403208; not determined |
| +0x14 | 1 | | updateWalkers (7 w), mainLoop's far code (r 0x00416eac) | not determined |
| +0x15 | 1 | | updateWalkers, mainLoop's far code (r 0x0041633c, `cmp byte [ebx], 0` 0x00416e5a) | not determined |
| +0x16 | 1 | s | 8 fns incl. hireEmployee, customizeCharacter / loadGolfPro (record 0), updateGolfers (w record 0, 0x00428e9b) | not determined |
| +0x18 | 2 | s (1) | updateWalkers only | not determined |
| +0x1a | 2 | | spawnWalker, updateWalkers (14 w) | not determined |
| +0x1c | 2 | | updateWalkers (3 w) | not determined |
| +0x1e | 2 | | spawnWalker, updateWalkers, drawFlyingBall, showRoutingMapScreen (loops from 0x0058586e) | not determined |
| +0x20 | 2 | s (8) | updateWalkers only | not determined |
| +0x22 | 2 | s | hireEmployee (w 0x0040acc0, value from g_date 0x00834170), drawEmployeeDetailPanel (r) | written from the date at hire time (0x0040ac9e..0x0040acc0) |
| +0x24 | 2 | s | drawEmployeeDetailPanel (r) | not determined |
| +0x26 | 2 | s | updateWalkers (w), drawEmployeeDetailPanel (r) | not determined |
| +0x2a | char[0x22] | | handleEmployeeAction (strcpy into it, 0x00436d2f..0x00436d47), pickGolferName (r 0x0046760f; returns it when non-empty, else builds a default from +0x12 / +0x13) | the walker's / employee's name; the copy is bounded by textInputBox's limit 0x1f |

Untouched by any direct reference: +0x17, +0x28..+0x29.

## g_golferStats 0x0056ae70 (32 x 0xb8)

All accesses are in `updateGolfers` 0x004289e0 and `matchUpdate` 0x00427380, both indexing as in the table above:
record = golfer class byte +0xba (signed, 0..3 by the 4x multiplier) + 4 * (golfer +0x19 & 7). Fields +0x00..+0x14
are described in `re/names/exe_2_globals.tsv` (row g_golferStats). The scan adds the per-hole part:

| off | width | accesses | meaning |
|---|---|---|---|
| +0x00 | 4 | updateGolfers rw 0x0042d4a5 / 0x0042d4ad | sum of first-shot distances (names row) |
| +0x04 | 4 | updateGolfers rw 0x0042d4fd / 0x0042d50b | count, names row |
| +0x08 | 4 | updateGolfers r 0x0042d56c | greens in regulation (names row) |
| +0x0c | 4 | updateGolfers 0x0042bdb9 / 0x0042c034 | names row |
| +0x10 | 4 | updateGolfers 0x0042d444 | first shots (names row) |
| +0x14 | 4 | updateGolfers rw 0x0042d4b3 / 0x0042d4bd | longest first shot (names row) |
| +0x20 | int[19] | matchUpdate rw 0x00427436 / 0x0042743e: `[idx*0xb8 + hole*4 + 0x56ae90] += (signed char)golfer[+0x22]` with hole = golfer +0x21 (0x0042741a..0x00427433) | per-hole sum of golfer byte +0x22 |
| +0x6c | int[19] | matchUpdate rw 0x00427444 / 0x0042744b: `[... + 0x56aedc] += 1` | per-hole count (the divisor for +0x20) |

0x20 + 19*4 = 0x6c and 0x6c + 19*4 = 0xb8: the two arrays end the record. +0x18..+0x1f are not addressed.
The extent 19 is the stride arithmetic; the hole byte's range is in `golfer_record.md` (+0x21).

## g_golferTypeState 0x005849e0 (76 used of 84 x 0x2c)

Indexed by the golfer's type word +0xb6 (`movsx eax, word [esi+0x57956e]; lea edx,[eax+eax*4]; lea eax,[eax+edx*2];
[... + eax*4 + base]`, 0x00425ca7..0x00425cbb), so one record per golfer type 0..75. U-0003 already describes +0x02
and +0x29 (membership level, ban byte).

| off | width | accesses | meaning |
|---|---|---|---|
| +0x00 | 1 | matchUpdate, showCharacterEditor, showMembershipRoster | not determined |
| +0x01 | 1 | matchUpdate rw 0x00428664 / 0x00428688, showCharacterEditor r 0x004392fa, showMembershipRoster r 0x00455192 | not determined |
| +0x02 | 1 | 29 refs, 10 fns (updateMembership, membershipReport, holeMagazineEvent, matchUpdate, updateGolfers ...) | membership level, low 3 bits (U-0003, `re/analysis/golfers/00406670_membership.md`); loadThemes writes from 0x584a0e = record 1 +0x02 (0x00465966) |
| +0x03 | u8[19] | narrateShot `or byte [..], 1` (0x004080f5..0x004080fc), updateGolfers `or dl, 4` (0x0042b787..0x0042b798), advanceStory 0x004667ce, showMembershipRoster loop from 0x5849e3 | per-hole flag bytes indexed by golfer hole +0x21; bits 1 and 4 set by the cited code |
| +0x16 | u8[19] | matchUpdate w 0x0042772a (`[type*0x2c + hole + 0x5849f6] = golfer[+0x22]`), prepareShot r 0x00425cbb, buildGolferComment r 0x0046b1fb | per-hole copy of golfer byte +0x22, indexed by golfer hole +0x21 |
| +0x29 | 1 | updateGolfers w 0xff at 0x0042b79a, mainLoop's far code w 0xff at 0x0041e602 and `cmp 0xff` at 0x004179c6, updateMembership, membershipReport, holeMagazineEvent, showCharacterEditor, showMembershipRoster, advanceStory | 0xff = type banned after a golfer of it quit (U-0003) |
| +0x2a | 2 | membershipReport / showMembershipRoster loops from 0x584a0a, matchUpdate, showCharacterEditor, pairGolfersWithStory | not determined |

0x03 + 19 = 0x16 and 0x16 + 19 = 0x29: the two per-hole arrays fill +0x03..+0x28 exactly.

## g_storyDefs 0x0053a454 (100 x 50 bytes)

Not part of the `.sve`: loadGolferRoster 0x004659a0 clears it and later copies 5000 bytes from 0x0080b130 into it.
Each record is a NUL-terminated string: loadStoryStrings 0x00466b70 takes `strlen` of record `index`
(`lea edi, [eax*2+0x53a454]; repne scasb`, 0x00466c90..0x00466c99) and copies it. pairGolfersWithStory 0x0045de80
picks a random index 0..99 whose byte 0 is non-zero (0x0045e007..0x0045e010, retrying up to 100 times) and then
upper-cases bytes 0..7 one by one with `_toupper` 0x004a6650 and switches on each: byte 1 minus 'F' (0x46) through a
14-entry table (0x0045e060..0x0045e077) comparing the two golfers' `typeBit7Clear` 0x0046c940 results; byte 2 minus
'D' (0x44) through 20 entries testing bits 0x08 / 0x10 / 0x20 / 0x40 of golfer byte +0xb8 (0x00579570,
0x0045e0f4..0x0045e15e); byte 3 minus 'A'; bytes 4..7 at 0x0045e311, 0x0045e3ca, 0x0045e46b, 0x0045e4db. So bytes 0..7
are an 8-letter requirement code. announceLandmark 0x004722c0 reads byte 0 (0x0047235e). Story ids in golfer +0xb0
are indexes into this table (`golfer_record.md`).

## Conflicts found

- `g_walkers` (exe_2_globals.tsv) calls the table "ambient walker/critter": hireEmployee, drawEmployeeDetailPanel
  and handleEmployeeAction's rename show employee records in it too. Row updated.
- `pickGolferName` 0x00467600 does not touch golfer records: its argument indexes `g_walkers` and it returns the
  walker's name (+0x2a) or a default. The function name is left to the names owner; the finding is recorded here.
- 0x0056c570, used by queueMessage 0x0040c720 and drawLabels 0x0040c7a0, is the first byte after `g_golferStats`
  (0x56ae70 + 0x1700), a separate global, not record 32.
- 0x0053b7e0 onward (initGlobalArray_00405760, mainLoop, showTitleScreen) is past the 100 story records, so it is
  a separate object array, not story data; likewise 0x0053bbac is the save field `g_0053BBAC` (0x9c4 bytes).
