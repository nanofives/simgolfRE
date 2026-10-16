# Golfer record (golf_clean.exe `g_golfers` 0x005794b8)

Phase 1 of `re/PORT_PLAN.md`: the first typed data-model deliverable. Offsets are from the record start
**0x005794b8 + index * 0x100**. Every row cites the instructions it rests on; `not determined` means the code was
read and its effect is stated, but no name follows from it.

Method: `re/tools/record_fields.py --base 0x5794b8 --stride 0x100 --count 0x9c --tsv log/data/golfers.tsv`
lists every instruction of every function range in `re/functions_ghidra.tsv` whose memory displacement or immediate
falls inside the block, plus a linear sweep of the code outside every function range (3,120 references: 2,676 in
60 functions, 444 in code no function covers), grouped by `(address - 0x5794b8) mod 0x100`, with width,
access, and signedness from `movsx` / `movzx` or the conditional jump after a `cmp`. The disassembly windows
around each reference (`log/data/golfers_ctx.txt`, not committed) were read field by field, together with the
reimplementation comments of the C3/C4 hooks that touch the record (c3dz, c3ea, c3ec, c3ah, c3fb, c3fr,
golf_course.cpp) and the saves in `original\saved games\`.

## 1. Base: 0x005794b8, 156 records of 0x100 bytes

The record starts at **0x005794b8**. 0x005794c0 is field +0x08 of record 0, not a record start.

| evidence | address | what it shows |
|---|---|---|
| whole-record swap | swapTableEntry 0x0045de30: `shl eax, 8; mov ecx, 0x40; lea eax, [eax + 0x5794b8]; rep movsd` to 0x00582cb8, then record b to record a, then 0x00582cb8 to record b (0x0045de35..0x0045de71) | a record is the 0x40 dwords from 0x5794b8 + k * 0x100 |
| whole-record copy | trialShot 0x004226a0: `shl ebp, 8; mov ecx, 0x40; mov edi, 0x582fb8; lea esi, [ebp + 0x5794b8]; rep movsd` (0x004226df..0x004226fb), copied back from 0x582fb8 at 0x00422f9d..0x00422fa7 | the same start; record 155 is trialShot's scratch copy |
| pair snapshot | addGolferPair 0x004099f0: `movsx esi, word [ebp + 0x57955a]; shl esi, 8; add esi, 0x5794b8; mov ecx, 0x40; rep movsd` into 0x0059fee8 (0x00409a65..0x00409a80); narrateShot copies 0x0059fde8 / 0x0059fee8 back into records 152 / 153 (0x00407f19..0x00407f43) | the partner's whole record starts at 0x5794b8 + partner * 0x100 |
| block clear | buildGolferBioText 0x004315e0 `mov ecx, 0x2700; mov edi, 0x5794b8; rep stosd` (0x0043160b..0x00431615); loadInterfaceArt 0x00442180 the same (0x004421b9..0x004421c3) | 0x2700 dwords = 0x9c00 bytes = 156 records |
| save | serializeCourse 0x0040afa0 pushes (0x005794b8, 0x9c00) (0x0040b107; `re/tools/sve.py` FIELDS) | the save block is the 156 records exactly |
| next object | 0x005830b8 = 0x5794b8 + 0x9c00 is a separate byte grid of 0x9c4 bytes: buildGolferBioText fills 0x271 dwords from it with -1 (0x004316ae..0x004316bb); setGrids / demolishObject / buildToolLabel index it as `[x + 0x5830b8]` (0x0040a149, 0x0040a602, 0x0040a248); the save writes it as its own field (0x0040b41f) | with base 0x5794c0, record 155 would run 8 bytes into this grid |
| saves | in `stories5.sve`, `mood5.sve`, `stories9.sve` and `&AutoSave1.sve`, every record read from 0x5794b8 has world x, y at +0x00 / +0x04 whose `>> 10` lies in 0..49 (the 50 x 50 tile grid), e.g. stories5 record 0: 26602, 16319 = tile (25, 15) | the 8 bytes before 0x5794c0 are record 0's x and y |

**The 8 bytes between 0x5794b8 and 0x5794c0** are fields +0x00 (x) and +0x04 (y) of record 0. There is no header
and no record -1: no instruction addresses 0x5794b8..0x5794bf other than as `index * 0x100 + 0x5794b8` /
`+ 0x5794bc` (record-relative x / y, 126 and 125 `disp+idx` references) or through the whole-record copies above.

**Why two conventions exist.** The shim sources golf_course.cpp, c3fa.cpp, c3fy.cpp, parts of c3ec.cpp and
`log/c3/c3fx_deps.tsv` write `0x005794c0 + i * 0x100` and offsets 8 lower than this table (their `+0xb2` is +0xba
here, their `+0xae` is +0xb6, their `+0xcc` / `+0xd0` are +0xd4 / +0xd8). Both address the same bytes, so the
reimplementations are not wrong, but the offsets cannot be compared across files. c3dz.cpp, c3ea.cpp,
re/match/golf_story.cpp, U-0003 and this document use 0x5794b8.

**Record roles.** Loops over golfers run i = 0..151: `cmp esi, 0x582cd9` (record 152, field +0x21) at 0x0042d359,
0x00429a69, 0x0040f225, 0x004586f6, 0x0046d04d; `cmp eax, 0x582d5a` (record 152, +0xa2) at 0x00431710 and
0x0046c9bf; `cmp eax, 0x582cd0` at 0x0040feec. Records 152..155 are fixed-purpose:

| record | address | role (evidence) |
|---|---|---|
| 0..151 | 0x005794b8.. | golfers on or waiting for the course; partner pairs are i and i ^ 1 (initGolferState 0x0046d0a4) |
| 152 | 0x00582cb8 | swap / replay scratch: swapTableEntry's temporary (0x0045de44), narrateShot's replayed golfer, partner set to 0x99 (0x00407f47) |
| 153 | 0x00582db8 | preview golfer: showCharacterEditor writes +0x18, +0x19, +0xb6, +0xb8, +0xba (0x004386a3..0x00439dab), drawFlyingBall and skillPointDialog write it too (0x00403b55..0x00403ea2, 0x0045f20a) |
| 154 | 0x00582eb8 | a synthetic golfer built by mainLoop's far code: cleared (`mov edi, 0x582eb8; rep stosd`, 0x00413594..0x004135a6), +0x19 set to 6 / 5 / 3 / 7 (0x004135b1..0x004135ed), +0xba = 1 (0x00413601), position +0x00 / +0x04 and +0xd4 / +0xd8, shot start +0xc4 / +0xc8, target +0xcc / +0xd0, hole +0x21 written (0x00413635..0x00413876), strokes +0x22 read back (0x0041370e..0x004138b7); which game action runs this block is [UNCERTAIN U-0017] |
| 155 | 0x00582fb8 | trialShot's saved copy of the golfer being simulated (0x004226e7, 0x00422fa2) |

Record 1 has fixed-address writes in loadGolfPro (0x005795b8.., 0x0046cf42..0x0046cfbd), and record 76
(+0xf0 = 0x0057e1a8) receives the skill block of every `.pro` file loadGolferRoster reads
(`loadThemeFile(name, 0x4c, 0x4c)`, 0x004662d0..0x004662d5).

## 2. Field map

Width in bytes; `s` / `u` with the instruction that shows the signedness; `w` / `r` give one writer and one reader.
Arrays are indexed by the hole byte +0x21 unless stated. Holes run 1..18 (drawGolferInfoPanel's scorecard loop,
x from 0x10a step 0x12 while < 0x24e, 0x0045d1a2, and its `cmp dl, 0x13` guard at 0x0045cf56), so an array of 19
entries has an unused hole-0 slot that no fixed-address access reaches.

| off | width | sign | meaning | evidence | writers | readers |
|---|---|---|---|---|---|---|
| +0x00 | 4 | s (`sar edx, 0xa` 0x0042be76) | world x; tile = v >> 10, tile centre = (t << 10) + 0x200 | w 0x00428a4a ((short [0x58bcba] << 10) + 0x600), 0x0042baf2 (x += dirX[+0x1a] * step), 0x004151f7 (snap to the tile centre: `and 0xfffffc00; add 0x200`); r 0x00415371 (worldToScreen 0x0042fb90 argument) | updateGolfers, stepCourseEditQueue, customizeCharacter, loadGolfPro (record 1, 0x0046cfac), mainLoop far code; whole-record copies | 15 functions plus mainLoop far code |
| +0x04 | 4 | s (`sar ecx, 0xa` 0x0042be79) | world y, same units | w 0x00428a50, 0x0042bb11, 0x0041520d; r 0x0041535d | as +0x00, plus updateMembership (0x00421c23) | as +0x00, plus drawGolferActionPanel |
| +0x08 | 4 | s (`cmp 0x32; jle`, `cmp 0x2ee; jge` 0x00428c6a..0x00428c78) | screen x: worldToScreen(+0x00, +0x04, &sx, &sy, 0)'s sx, stored only when the call returns non-zero (0x0041537a..0x00415395); -1 is stored first (0x00414969) and tested at 0x00429930, 0x0042be5d, 0x0042c507, 0x004356c4 | w 0x00415395; r 0x00428c64 | mainLoop far code only | updateGolfers (pair gate), emitGolferThought (0x004688e7), narrateShot (0x00407e46..0x00407e74), clickEmployeeView, mainLoop far code |
| +0x0c | 4 | s (`cmp 0x1c2; jge` 0x00428c84..0x00428c8e) | screen y, from the same call | w 0x0041539b; r 0x00428c84 | mainLoop far code only | updateGolfers, emitGolferThought, narrateShot, mainLoop far code |
| +0x10 | 4 | flags | flag dword, bits in section 3; byte accesses to +0x10 and the byte `or [+0x13], 0x10` (0x0041b331) are parts of it | section 3 | 14 functions | 22 functions |
| +0x14 | 2 | s (`jle` 0x00428d26, `jg` 0x0042b2d4) | movement left in the current step: the step that scales dirX / dirY is subtracted (`sub [+0x14], ax` 0x0042bb1b); set to 0x18 - k or k + 8 (0x0042b404..0x0042b42e), capped at 0x10 for odd +0x1a (0x0042b435..0x0042b44c), 3 when [0x59e7b8] & 0x80000 (0x0042b466) | w 0x0042b404; r 0x00428d26 | updateGolfers, advanceStory (0 for both of a pair, 0x004668b6 / 0x004668bd) | updateGolfers |
| +0x16 | 2 | | skill mask: bit i set when skill byte +0xf0 + i != 0 (loadGolfPro starts from 0xe0 and ORs per byte, 0x0046cf10..0x0046cf2d); rateTile tests bit 0 before +0xf0 (0x004225cc), prepareShot bit 4 before +0xf4 (0x00424c1c), trialShot bit 9 (0x200), chooseShotTarget 0x20 / 0x40; narrateShot sets -1 and restores it (0x00408915, 0x00408fae); 2nd argument of skillPointDialog 0x0045f0f0 (0x0042bf3b) | w 0x0046cf2d; r 0x004225cc | customizeCharacter, loadGolfPro, narrateShot, pairGolfersWithStory | narrateShot, prepareShot, rateTile, trialShot, chooseShotTarget, updateGolfers, drawGolferInfoPanel, customizeCharacter, pairGolfersWithStory |
| +0x18 | 1 | u (`and 0xe0`, `and 0x1f`) | golfer kind: bits 5..7 a class, bits 0..4 a sub-kind; buildGolferName picks the name source from it (0x00467723: 0x20 = the 0x38-byte record +0xbb, class 0 = the golfer-type name); patronEvent stops on 0x60 / 0x40 / 0x80 (0x004266c5, 0x00426987, 0x00426c98); matchUpdate lets only kind 0 join (0x004286c7) | w 0x0045e978 (0x60 + [0x572cac]), 0x0045eaaf (0x40 + [0x53a450]), 0x0045ec14 (0x80) | pairGolfersWithStory, customizeCharacter (0x20 at 0x0040f2aa, 0x24 at 0x0040f4ae), loadGolfPro, clickGolferPanel (record 152), drawFlyingBall / showCharacterEditor (record 153), mainLoop far code | 20 functions |
| +0x19 | 1 | u (`and 7`, `and 0xf0`) | bits 0..2 the golfer's preferences, drawn by drawGolferInfoPanel from the 3 strings of table 0x004c2858 (0x0045c9dc..0x0045ca7e) and toggled by showCharacterEditor (0x004397ea / 0x004397f6 / 0x00439802); the g_golferStats index term (record_tables.md); bits 4..7: [0x59b730] << 4 + 7 (0x0040f437..0x0040f44c), 0x47 (0x0040f540, 0x0046cf42), tested non-zero by matchUpdate (0x004277c3) and updateGolfers (0x0042c51f); their meaning is open [UNCERTAIN U-0010] | w 0x00421d94; r 0x0045c9dc | customizeCharacter, loadGolfPro, updateMembership (0x00421d17..0x00421d94), pairGolfersWithStory, mainLoop, clickGolferPanel, showCharacterEditor, drawFlyingBall, mainLoop far code | 10 functions |
| +0x1a | 1 | s (`movsx` 0x0042badf) | facing octant 0..7 (`and al, 7` 0x0042ae6e): index into the step tables 0x004c2878 (dx) and 0x004c2898 (dy), x += dirX[d] * step (0x0042badf..0x0042baf2); derived from the ball direction +0xe0 (0x0042bff5..0x0042c011) | w 0x0042afe2; r 0x0042badf | updateGolfers, updateWalkers, advanceStory, customizeCharacter, loadGolfPro, narrateShot, updateMembership, mainLoop far code | updateGolfers, drawFlyingBall, narrateShot, mainLoop far code |
| +0x1b | 1 | s (`movsx` 0x00425a71, `jg`) | shot value set by prepareShot: acc / (n + 3), +4, 0 on a tile byte 1, max 10 (0x004249ee..0x00424a3b), 0 / 10 / 20 / 6 (0x00425aac, 0x00425b52, 0x00425b5a); scanShotArea uses 200 / v (0x00409849); updateGolfers compares it with difficulty + 3 (0x0042d6b3) [UNCERTAIN U-0011] | w 0x00424a3b; r 0x00409849 | prepareShot | narrateShot, scanShotArea, prepareShot, updateGolfers |
| +0x1c | 1 | s (`movsx` 0x00414b40) | club 0..13: appendClubName 0x0040a9a0 switches on 0..0xd (`cmp eax, 0xd; ja` 0x0040a9a5); mainLoop far code clamps it to 0..0xb and prints it after string 0x004c6138 (0x0041b5fc..0x0041b65b); prepareShot sets 5 and 0xd (0x00424b38, 0x00424b51); bit (1 << club) goes into +0xa0 | w 0x00424b51; r 0x00425de6 | prepareShot, matchUpdate (0, 0x004277b1), updateGolfers, mainLoop far code | prepareShot, narrateShot, updateGolfers, mainLoop far code (`cmp 0xd` 0x00414b72) |
| +0x1d | 1 | s (`movsx` 0x0042bcd3; `cmp 7; jl` 0x00428bc6) | animation / activity state: values 0, 7, 0xb, 0xc, 0xd, 0xe, 0x10..0x13 written, clamp(+0x20 - 3, 0, 6) at 0x0042c428, a 7..0xa cycle at 0x0042bc58..0x0042bc6e; mainLoop far code picks the frame-table base from it (0x00414aef, 0x00414b90..0x0041529d); the pair gate wants >= 7 and != 0x10 (0x00428bbd..0x00428be7) [UNCERTAIN U-0012] | w 0x0042c428; r 0x0042bcd3 | updateGolfers, advanceStory, emitGolferThought, narrateShot, resetGolfer, updateMembership, updateWalkers, loadGolfPro, drawFlyingBall, mainLoop far code | updateGolfers, drawQueuedSprite (`cmp 6`), updateWalkers, mainLoop far code |
| +0x1e | 1 | u (`jbe` 0x00414bb2) | frame counter in the current sequence: `inc` (0x00414c25, 0x0041518d), wrapped with tables 0x0053f500 / 0x0053f4d8 / 0x0053f758 and capped at dword 0x0053f3e8[base + k] - 1 (0x00414c44..0x00414c56); reset when +0x1f changes (0x00415330) | w 0x00414c25; r 0x00414bb2 | updateGolfers (0 at 0x0042c086), mainLoop far code | mainLoop far code; showTournamentResults (record 1, 0x0045aba1) |
| +0x1f | 1 | u | frame-table base last chosen (0xa, 0x28, 0x32, ... 0xdc); on a change +0x1e = 0 and +0x1f = base (0x00415326..0x00415337) | w 0x00415337; r 0x00415326 | mainLoop far code only | mainLoop far code only |
| +0x20 | 1 | s (`cmp 0x78; jle` 0x0042c43e) | counter: 1 (0x0042af93, 0x0042c0b6), incremented up to 0x78 (0x0042c415..0x0042c440), drives +0x1d; at 0x20 with flag 0x4000 it sets flag 0x400 and returns to 0 (0x0042c584..0x0042c5a1); mainLoop far code decrements it when +0x1d is 4 or 5 (0x00414c88, 0x00414c9c) [UNCERTAIN U-0012] | w 0x0042af93; r 0x0042c43e | updateGolfers, mainLoop far code | updateGolfers, mainLoop far code |
| +0x21 | 1 | s (`movsx`, 106 reads; `cmp dl, 0xff` 0x0045ce95) | current hole, index into the hole records 0x00575ab0 + h * 0x208; -1 when allocated (0x00421f31), 1 when a pair starts (0x0045de97, 0x0040fe3c), `inc` at hole end (0x00428713), 1 when the next hole's par byte is 0 (0x0042873a), 0x13 (19) when the golfer leaves (0x004297e8, 0x0042734f, 0x0041e609), 0 at 0x004298d8 / 0x00429940 / 0x0042a8ff | w 0x00428713; r 0x0045ce8f | 13 functions incl. mainLoop far code | about 25 functions |
| +0x22 | 1 | s (`movsx` 0x0045d114; `cmp 8; jge` 0x0042d9f5) | strokes on the current hole: `inc` per shot (0x0042bd98, 0x0042cd3b, 0x0042d51d), copied into +0x23[hole] then zeroed by matchUpdate (0x0042745e, 0x00427763); indexes the stroke-name string table 0x004c28b8 in drawGolferInfoPanel (0x0045cef9). U-0003's gate `byte +0x22 == 0` therefore means no stroke yet on this hole | w 0x0042bd98; r 0x0045d114 | updateGolfers, chooseShotTarget (+1 / -1, 0x004231b2 / 0x004231e8), matchUpdate, resetGolfer, stepCourseEditQueue, mainLoop far code | 9 functions |
| +0x23 | s8[19], index hole | s (`movsx` 0x00427833) | per-hole strokes, holes 1..18 at +0x24..+0x35: matchUpdate stores +0x22 at [+0x23 + hole] (0x0042745e); drawGolferInfoPanel's scorecard reads holes 1..18 (0x0045cfff); showTournamentResults walks from +0x24 (0x0045a89e); matchUpdate sums it (0x0042835a); slot 0 is read at 0x00428f43 | w 0x0042745e; r 0x0045cfff | matchUpdate | matchUpdate, updateGolfers, drawGolferInfoPanel, drawMatchScoreboard, buildGolferComment, emitGolferThought, showTournamentResults |
| +0x36 | 1 | s (`movsx` 0x004288fe; `test al, al; jl` 0x004095ca) | not determined: a signed value matchUpdate steps one unit toward 0 at hole end (`x -= sign(x)`, sign 0x00467150, 0x004288fe..0x00428916); emitGolferThought sets it to 1 for thought kinds 2 / 3 / 0xc / 0xd (0x00468519) and otherwise adds the thought delta or zeroes it, only when byte +0x18 != 0 (0x004684b2..0x004684d9); rateTile adds clamp(x, 0, 3) * v / 24 when x > 0 (0x004225f5); prepareShot reads it at 0x00424c6b, 0x00424d13, 0x00424f86 [UNCERTAIN U-0013] | w 0x0042982b / 0x0042b7f7 (0xfd = -3), 0x00468519; r 0x004225f5 | matchUpdate, updateGolfers, narrateShot, emitGolferThought | rateTile, prepareShot, matchUpdate, emitGolferThought, narrateShot |
| +0x37 | u8[19], index hole +0x21 | | per-hole story counter byte: computeStoryReply writes (chapter << 3) + 1 (0x00466b29, 0x00466b48) and reads v >> 3 (0x00466a0c); matchUpdate writes the low byte of counter +0xb4 when flag 0x100000 is set (0x004288c6..0x004288d3); updateGolfers writes 0x63 (99) into the partner's slot (0x00428cbd); pairGolfersWithStory writes compat * 2 into slot 1 (0x0045ede8 / 0x0045edf2); drawGolferInfoPanel draws hole k's story lines when slot k != 0 (0x0045d36e); mainLoop's far code writes it (value or 0x63) after testing bits 0x80 / 0x40 of +0x89 (0x0041c430..0x0041c4ec, 0x0041e874..0x0041e9af), next to the force=1 story advances U-0003 lists | see meaning | computeStoryReply, matchUpdate, updateGolfers, pairGolfersWithStory | drawGolferInfoPanel, computeStoryReply |
| +0x4a | u8[19], index hole | u (`mov al` after `xor eax, eax`, 0x00466910) | per-hole chapter (`holeStory` in re/match/golf_story.cpp): advanceStory stores the low byte of stage +0xb2 (0x00466409, the partner's at 0x00466800); storyText reads its own or the partner's slot (0x00466910 / 0x00466929) | w 0x00466409; r 0x00466910 | advanceStory | storyText |
| +0x5d | u8[18], addressed as +0x5c + hole | | per-hole mood (low byte of +0xa4) for holes 1..18 (+0x5d..+0x6e; a hole-0 write would land on +0x5c, the last +0x4a entry): matchUpdate writes [+0x5c + hole] (0x004288ea..0x004288f7), pairGolfersWithStory writes +0x5d after setting hole = 1 (0x0045eef9 / 0x0045ef05); drawGolferInfoPanel writes [+0x5d + hole] for hole < 0x13 (0x0045ce8f..0x0045cea1, 0x0045cf56..0x0045cf67), one slot higher than matchUpdate; no instruction reads the array | w 0x004288f7, 0x0045cf67 | matchUpdate, pairGolfersWithStory, drawGolferInfoPanel | none |
| +0x6f | 1 | | written only by drawGolferInfoPanel's [+0x5d + hole] for hole 18 (0x0045cf67) | | drawGolferInfoPanel | none |
| +0x70 | u8[10] | u (`mov cl` after `xor ecx, ecx`, 0x0042d5ac) | thought-kind history, slot 0 newest: emitGolferThought shifts slots 0..8 to 1..9 (0x00467b94..0x00467bbd) and stores the new kind in slot 0 (0x00467bbf); bit 7 is masked off by readers (`and 0x7f`, 0x00428d46); updateGolfers tests slots 0 and 1 against 0xb (0x00428d40 / 0x00428d50) and slot 0 against 0x24 (0x0042b1f2); computeStoryReply skips kind 0x13 (0x00466a50..0x00466a69) | w 0x00467bbf | emitGolferThought | emitGolferThought, updateGolfers, drawGolferInfoPanel, computeStoryReply, narrateShot |
| +0x7a | u8[10] | u | place of each history thought: hole * 11 + byte +0x22, plus 1 when kind >= 4 (0x00467bc5..0x00467bf1); drawGolferInfoPanel divides by 11 (magic 0x2e8ba2e9, 0x0045db42); prepareShot compares slot 0 before and after a shot (0x004254ea, 0x00425b72..0x00425e80) | w 0x00467bf1 | emitGolferThought | prepareShot, drawGolferInfoPanel |
| +0x84 | 1 | u (`test dl, dl; ja` 0x00468656) | thought countdown: emitGolferThought sets 7 on the target record (0x00467af4); updateGolfers decrements it when (g_date & 7) == 0 and calls thoughtFlag at 1 (0x00428ade..0x00428af5); advanceStory adds 3 / 1 (0x00466501, 0x004665ea), pairGolfersWithStory 10 / 8 (0x0045efb1 / 0x0045efbf), emitGolferThought 2 (0x00468687); prepareShot, matchUpdate (0x004276f3) and drawQueuedSprite (0x00462e10) act only when it is non-zero | see meaning | emitGolferThought, updateGolfers, advanceStory, pairGolfersWithStory, prepareShot | updateGolfers, prepareShot, matchUpdate, drawQueuedSprite, advanceStory, emitGolferThought |
| +0x85 | 1 | u (`and 0x7f`) | kind of the thought just emitted; bit 7 set when [0x005409a8] != 0 (0x00467afb / 0x00467b27); updateGolfers tests 0x32 (0x00428b2b), 7 (0x0042b300), 0xa (0x0042b50d), 0x2b (0x0042b561) | w 0x00467b27 | emitGolferThought | updateGolfers |
| +0x86 | 1 | u | place of the thought just emitted, hole * 11 + byte +0x22 (0x00467b01..0x00467b0f) | w 0x00467b0f | emitGolferThought | none |
| +0x87 | 1 | | no access | | | |
| +0x88 | u16[10] | u (`and 0x3fff`, `and 0xc000`) | thought history argument and outcome, shifted with +0x70 / +0x7a (0x00467bb3..0x00467bb6): bits 0..13 the thought's argument (0x00467be8; drawGolferInfoPanel `and 0x3fff` 0x0045dc58); bit 14 (byte +0x89 bit 6) set when the delta was > 0 (0x004687c0); bits 14 and 15 set when the final delta < 0 (0x004687cf, 0x004687da). thoughtBalance 0x0045c420 counts 0x4000 / 0xc000 over slots 0..4 (0x0045c42a); thoughtFlag reads slot 0 bit 15 (0x004675d7) | w 0x00467be8 | emitGolferThought | thoughtBalance, thoughtFlag, updateGolfers, drawGolferInfoPanel, computeStoryReply, emitGolferThought |
| +0x9c | 2 | | argument of the thought just emitted (0x00467b1e) | w 0x00467b1e | emitGolferThought | none |
| +0x9e | 2 | s (`cmp word, 0; jl` 0x0042b80b) | wait counter: updateGolfers skips the golfer's step while it is < 0 (0x0042b803) and increments it on odd dates (0x00429103..0x00429119); stores include 0xff9d (-99) (0x004297d6), 0xfff8 (-8) (0x004286fe), 0xffd0 (-48) (0x0042aa3b), -6 - Random::range(6) (0x0045def4, 0x0040fec0), (0xfffc - stage) << 2 (0x0046681f) | w 0x004297d6; r 0x0042b80b | 11 functions (updateWalkers, narrateShot, stepCourseEditQueue, customizeCharacter, mainLoop, matchUpdate, updateGolfers, pairGolfersWithStory, advanceStory, emitGolferThought, loadGolfPro) | updateWalkers (0x00402c6c), updateGolfers, advanceStory (0x00466830) |
| +0xa0 | 2 | s (`movsx` 0x00425dc5) | clubs-used bitmask: bit k for club byte +0x1c (`shl edx, cl; or`, 0x00425de6..0x00425df5); drawGolferInfoPanel names bits 0..13 from table 0x004c2960 (0x0045cdf5); prepareShot emits thought 0x36 for a club not used before (0x00425dd8) | w 0x00425df5 | prepareShot | prepareShot, updateGolfers, drawGolferInfoPanel |
| +0xa2 | 2 | s (`movsx`, 91 refs) | partner golfer index, -1 = none: buildGolferBioText and loadGolfPro store 0xffff in records 0..151 (0x00431701..0x00431715, 0x0046c9a7..0x0046c9c4); initGolferState stores index ^ 1 (0x0046d0a4); narrateShot stores 0x99 in record 152 (0x00407f47); `holeStory` reads the partner's slot through it (golf_story.cpp) | w 0x0045de9e; r 0x00428b6a | pairGolfersWithStory, customizeCharacter, initGolferState, loadGolfPro, mainLoop, narrateShot, stepCourseEditQueue, buildGolferBioText | 21 functions (updateGolfers 43 refs, advanceStory 15) |
| +0xa4 | 2 | s (`movsx` 0x00466a7a) | mood (U-0003): emitGolferThought adds the delta and clamps to -10..10 (0x0046868c..0x004686a7); drawGolferInfoPanel draws (8 - v) * 10 beside label string 0x004d2120 (0x0045cb4c); updateWalkers stores 0xfff5 (-11), outside that clamp (0x004032f7); loadGolfPro stores Random + 3 / 5 / [0x543cc4] + 4 (0x0046cb20..0x0046cb57); byte reads copy the low byte into +0x5c[] (0x004288f1, 0x0045cf61) | w 0x004686a7; r 0x00466a7a | 9 functions | 13 functions |
| +0xa6 | 2 | s (`movsx` 0x0042a1ef) | meter drawn beside label string 0x004d39ec (0x0045ccdc), clamp(v * 5 / 2, 0, 0x50); updateGolfers increments it (0x00429033) and at 0x10 resets it or emits thought 0xf (0x00429041..0x0042907c), zeroes it at 0x0042aa34; updateWalkers calls thoughtFlag when v > 0x10 (0x00402ad7) | w 0x00429033; r 0x00402ad7 | updateGolfers | updateGolfers, updateWalkers, emitGolferThought, drawGolferInfoPanel, drawEmployeeViewPanel |
| +0xa8 | 2 | s (`movsx` 0x0045cd6d) | meter drawn beside label string 0x004d39e4 (0x0045cd68), same scaling; updateGolfers increments it (0x00429087) with the 0x10 / thought 0xe step (0x004290ad..0x004290cc) and zeroes it at 0x0042aa2d; updateWalkers sets 0x63 (0x00402b4b) and 0 (0x00402b93) | w 0x00429087, 0x00402b4b; r 0x00402ace | updateGolfers, updateWalkers | updateGolfers, updateWalkers, emitGolferThought, drawGolferInfoPanel, drawEmployeeViewPanel |
| +0xaa | 2 | s (`movsx` 0x0042a918) | meter drawn beside label string 0x004d39f4 (0x0045cc4f), clamp(v / 4, 0, 0x50); updateGolfers increments it per walking step (0x0042b2c0), caps it at 0x28 (0x0042b9ea), sets 0xa0 for class 0x20 (0x0042bc31) and 0 (0x00429824); updateWalkers tests v > 0xa0 (0x00402ae0) | w 0x0042b2c0; r 0x00402ae0 | updateGolfers | updateGolfers, updateWalkers, emitGolferThought, drawGolferInfoPanel, drawEmployeeViewPanel |
| +0xac | 2 | | shot kind set by prepareShot: 0 (0x00425209), 4 chip branch (0x004253d4), 3 lob branch (0x004254b1), 1 (0x00425f8e) or 0xffff (-1) (0x00426081) from a frame slot; narrateShot pairs 1 / -1 / 3 with bits 0x20 / 0x40 / 0x80 of byte +0x16 (0x00408b0a..0x00408b62) | w 0x00425209; r 0x00408b32 | prepareShot | prepareShot, narrateShot |
| +0xae | 2 | s (`movsx` 0x0046a11b, then unsigned `ja` 5) | pair relation class: pairGolfersWithStory stores 0 (0x0045df12), 2 when compat <= 1 and typeBit7Clear agrees (0x0045ee6e), 3 when compat == 5 (0x0045ee8e), 1 when compat >= 4, typeBit7Clear differs and both +0xb8 have bit 0x10 (0x0045eee1); narrateShot and buildGolferComment switch on 0..5 (tables 0x004095dc, 0x0046c874); values 4 and 5 have readers and no writer found [UNCERTAIN U-0014] | w 0x0045ee6e; r 0x0046a11b | pairGolfersWithStory, customizeCharacter (0x0040f4fb), showCharacterEditor (record 153, 0x004394ad) | buildGolferComment, narrateShot, emitGolferThought, updateGolfers |
| +0xb0 | 2 | s (`movsx` 0x0046644c; `cmp word, -1` 0x0045c5ab) | story id, -1 = none (U-0003): an index into g_storyDefs 0x0053a454 stored by pairGolfersWithStory (0x0045e6cd), which rejects ids held by any golfer 0..151 with hole > 0 (0x0045e692..0x0045e6b4); customizeCharacter (0x0040f4ed) and loadGolfPro (0x0046cac2) store -1; advanceStory copies its low byte into the type record's +0x29 (0x004667a2) | w 0x0045e6cd; r 0x0046644c | pairGolfersWithStory, customizeCharacter, loadGolfPro | advanceStory, drawGolferInfoPanel, drawEmployeeViewPanel, buildGolferComment, pairGolfersWithStory |
| +0xb2 | 2 | s (`movsx` 0x004663c7) | story stage (U-0003): advanceStory increments it (0x004663f4) or lowers it (0x004666df); computeStoryReply sets the chapter (0x00466b15 / 0x00466b34); updateGolfers uses 0x3000 / (v + 6) (0x00428c38); low byte copied to +0x4a[hole] | w 0x004663f4; r 0x004663c7 | advanceStory, computeStoryReply | advanceStory, updateGolfers, drawGolferInfoPanel, drawEmployeeViewPanel |
| +0xb4 | 2 | s (`movsx` 0x00428892) | story counter (U-0003): matchUpdate sets c * 7 / 8 + mood when paired, else mood (0x00428892..0x004288e3); pairGolfersWithStory sets compat * 2 (0x0045edcc); advanceStory writes it on rollback (0x00466700); low byte copied to +0x37[hole] | w 0x004288e3; r 0x004663d5 | matchUpdate, pairGolfersWithStory, advanceStory | advanceStory, computeStoryReply, matchUpdate |
| +0xb6 | 2 | s (`movsx` 0x0046c947) | golfer type 0..75: index into the 0x230-byte records at 0x004d6088 (typeBit7Clear 0x0046c947..0x0046c95a) and into g_golferTypeState 0x005849e0 (0x00429676, 0x004667a2); golferScore uses type % 10 (0x0045326d) | w 0x0045e91e; r 0x0046c947 | updateMembership, pairGolfersWithStory, customizeCharacter, loadGolfPro, showCharacterEditor, skillPointDialog, pickAProScreen, clickGolferPanel, drawFlyingBall | 22 functions |
| +0xb8 | 2 | | flag word: low byte = the type record's byte 0x004d60a9 + type * 0x230, high byte = Random::range(0x80) (0x00421ed2..0x00421f09, 0x0046caf0..0x0046caff); bits 1 / 2 / 4 enter golferScore (0x0045327f), which drawGolferInfoPanel prints after string 0x004d3a0c (`, age `) (0x0045c7e0..0x0045c81e); bits 0x08 / 0x10 / 0x20 / 0x40 select the marital-status string (0x0045c73b..0x0045c7b9) and are the bits story code letters test (0x0045e11b..0x0045e157); no reader of the high byte found | w 0x00421f09; r 0x0045c73b | updateMembership, loadGolfPro, customizeCharacter, showCharacterEditor, skillPointDialog, drawFlyingBall | golferScore, drawGolferInfoPanel, showGolferPairSelect, showCharacterEditor, pairGolfersWithStory |
| +0xba | 1 | s (`movsx` 0x00422561) | class 0..2: updateMembership stores type % 3 (`idiv 3`, 0x00421f1d..0x00421f25), loadGolfPro (0x0046ca93) and customizeCharacter (0x0040f539) store 2; drawGolferInfoPanel appends one or two `+` for 1 / 2 (0x0045ca84 / 0x0045caba); the g_golferStats index term (record_tables.md); rateTile adds v * 50 / 3 (0x00422561) | w 0x00421f25; r 0x00422561 | updateMembership, loadGolfPro, customizeCharacter, showCharacterEditor, drawFlyingBall | updateGolfers, matchUpdate, rateTile, updateMembership, drawGolferInfoPanel |
| +0xbb | 1 | u (`and eax, 0xff` 0x00428062) | index into the 0x38-byte records at 0x0058dd50: buildGolferName takes text from it (0x004677f5..0x00467806), composeGolferSprite reads its +0x22 (0x00462519), showCharacterEditor its +0x20 (0x00438a3d) | w 0x0040f2b1, 0x0046ccc3; r 0x00428062 | customizeCharacter, loadGolfPro | buildGolferName, composeGolferSprite, matchUpdate, showCharacterEditor |
| +0xbc | 1 | u | bitmask, not determined: on a type-0x11 tile with tile flags 0x20 and 0x100, updateGolfers emits thought 7, clears bit Random::range(5) and sets bit (tile flags & 7) (0x0042b333..0x0042b34f); no other access [UNCERTAIN U-0015] | rw 0x0042b34f | updateGolfers | updateGolfers |
| +0xbd | 1 | | no access | | | |
| +0xbe | 2 | s (`movsx` 0x004299ff) | order stamp from the counter 0x0056a51c: initGolferState (0x0046d095), pairGolfersWithStory (0x0045ded0 / 0x0045ded8), loadGolfPro (0x0046cb2c); matchUpdate adds 2 * [0x005685f0] (0x0042874a); updateGolfers compares counter - v between golfers on one hole (0x004299ff..0x00429a17) | w 0x0046d095; r 0x004299ff | initGolferState, pairGolfersWithStory, loadGolfPro, matchUpdate | updateGolfers |
| +0xc0 | 4 | s | date stamp: a copy of g_date 0x00834170. matchUpdate adds (g_date - [+0xc0]) / 2 to the dword at 0x00575c9c + 0x208 * hole (hole = +0x21) and stores g_date back (0x0042776a..0x00427799) | w 0x0040f4de, 0x00421f3f, 0x0045debf, 0x0046cb12 (each from [0x00834170]); r 0x0042776a, `cmp; jle` 0x00427770 | customizeCharacter, updateMembership, pairGolfersWithStory, loadGolfPro, matchUpdate | matchUpdate |
| +0xc4, +0xc8 | 4 + 4 | s (`sar 0xa` 0x0042ca6d / 0x0042ca7c) | shot start position (world x, y): copied from +0xd4/+0xd8 at 0x0042c08d..0x0042c09f; out of bounds puts the ball back here (0x0042d7df..0x0042d7f4); stroke distance = distance(+0xd4/+0xd8 minus +0xc4/+0xc8) (0x0042d452..0x0042d46d) | w 0x0042c093 / 0x0042c09f; r 0x00409690, 0x00409987, 0x0042d7df | updateGolfers | narrateShot, scanShotArea, updatePairSnapshot, updateGolfers |
| +0xcc, +0xd0 | 4 + 4 | | shot target tile (x, y in tile units): the hole's tile [0x575ac8 / 0x575acc + 0x208*h] (0x004241d7 / 0x004241dd), chooseShotTarget's best cell (0x00424083 / 0x00424090) or an explicit target `sar 0xa` (0x0042425b / 0x00424261); +0xcc = -1 (0xffffffff) marks a player's shot pending (w 0x0042be88, tested 0x0042bf95 and drawGolferActionPanel 0x0043644a) | see meaning | chooseShotTarget, prepareShot, updateGolfers | chooseShotTarget, prepareShot, updateGolfers, drawGolferActionPanel |
| +0xd4, +0xd8 | 4 + 4 | s (`sar edx, 0xa` 0x00422bd6) | ball position (world x, y; tile = v >> 10, sub-tile (v >> 6) & 15 at 0x0042286c); moved each step along +0xe0 by +0xe4 / 16 (0x004227cb..0x004227de, 0x0042c6e1..0x0042c701); +0xd4 = 0 means no ball, and updateWalkers then uses +0x00/+0x04 (0x00402bf9); stepCourseEditQueue sets it with +0x00/+0x04 to tile << 10 + 0x200 (0x00409f36 / 0x00409f51) | w 0x004227d0 / 0x004227de, 0x0042c6f3 / 0x0042c701; r 0x0042273d, 0x00402bed, 0x00422594 | stepCourseEditQueue, trialShot, updateGolfers; +0xd4 also patronEvent (0x00427345), matchUpdate (0x00427759) | 11 functions (updateWalkers, narrateShot, scanShotArea, updatePairSnapshot, addGolferToTable, chooseShotTarget, prepareShot, trialShot, rateTile, updateGolfers, drawWorldObjects) |
| +0xdc | 4 | s (`test; jg` 0x00422c62) | ball height: h += vz / 32 per step (0x004227e4..0x004227fb); bounce when h <= 0 and vz < 0 (0x00422c5c..0x00422c72), then h = 0 (0x00422d2b); drawWorldObjects acts only on h < 0x96 (150) (0x004305b0) | w 0x004227fb, 0x0042c71e, 0x0042afad (0); r 0x004099c5, 0x00409b77 | trialShot, updateGolfers | updatePairSnapshot, addGolferToTable, trialShot, updateGolfers, drawWorldObjects |
| +0xe0 | 4 | s (`sar 0x1c` 0x00422934) | ball direction, a 32-bit full circle from angleFixed (0x004242a8, 0x00423198); octant (((d >> 28) + 1) >> 1) & 7 indexes the step tables 0x004c2878 / 0x004c2898; `xor 0x80000000` reverses it (0x00422d4e..0x00422d5a); updateGolfers derives facing +0x1a from it (0x0042bff5..0x0042c011) | w 0x004242a8, 0x0042298c, 0x0042cac6; r 0x0042278f, 0x00409b9c | chooseShotTarget, prepareShot, trialShot, updateGolfers | addGolferToTable, chooseShotTarget, prepareShot, trialShot, updateGolfers |
| +0xe4 | 4 | s (`cmp 0x40; jge` 0x00422f4c) | horizontal ball speed; step = speed / 16 (0x00422789); set by shotPower 0x00422430 through prepareShot (0x00424bdd); the ball is at rest when speed < 0x40, +0xdc == 0 and +0xe8 == 0 (0x0042d361..0x0042d384); trialShot's loop runs while it is non-zero (0x00422f92) | w 0x00424ba0, 0x00422bca, 0x0042c88d, 0x0042d38c (0); r 0x00422783, 0x00409bae | prepareShot, trialShot, updateGolfers | addGolferToTable, prepareShot, trialShot, updateGolfers, drawWorldObjects |
| +0xe8 | 4 | s (`cmp 0xffffff40` (-192); `jge` 0x0042ced8) | vertical ball speed: -0x40 (-64) per airborne step (0x00422817, 0x0042c751); bounce: clamp(-0x40 - b * vz / 12, 0, 9999), 0 if < 0x80 (0x00422ce9..0x00422d21); prepareShot sets sp / 8 + 0x200 (0x00424ba6..0x00424bb4) | w 0x00424bb4, 0x00422d19, 0x0042afa1 (0); r 0x004227e4, 0x00409bb5 | prepareShot, trialShot, updateGolfers | addGolferToTable, prepareShot, trialShot, updateGolfers |
| +0xec | 4 | s (`test; jle` 0x00425f29) | spin: added to +0xe0 each airborne step, whole in trialShot (0x0042297d..0x0042298c), halved in updateGolfers (0x0042c893..0x0042c8ae); trialShot halves it on entry (0x0042270b..0x00422719); built in prepareShot from the skill bytes (0x00424cd3..0x004254d8) | w 0x00424cd3, 0x00422719, 0x0042cb62; r 0x00425170, 0x0042c893 | prepareShot, trialShot, updateGolfers | prepareShot, trialShot, updateGolfers |
| +0xf0 | u8[12] | u (`movzx` 0x0042d1ec; `cmp cl, 0xa; jae` 0x0043a204) | skill levels, one byte per skill; bit i of word +0x16 is set iff byte i != 0 (loop i < 0xc at 0x0046cf1b..0x0046cf38 and 0x0040f45d..0x0040f485); showCharacterEditor shows and clamps 10 of them to 0..10 (0x00438fd4..0x0043912d, 0x0043a204..0x0043a21a; label pointers 0x004c2c3c + 4i). Per-byte readers: [0] [1] rateTile (0x004225d2, 0x004225e6), [2] [3] [5] [6] [7] prepareShot spin divisors (0x00424fd2, 0x00424fa5, 0x0042500d, 0x0042503f, 0x004254c2), [4] prepareShot accuracy (0x00424c27), [8] prepareShot lie test (0x00426163), [9] trialShot / updateGolfers tree and bounce terms (0x00422a0b, 0x00422dad, 0x0042c92e, 0x0042d1ec) | dword writes: loadGolfPro 0x0046cec2 / 0x0046cece / 0x0046ced8 (12 bytes); byte writes: pairGolfersWithStory rand(8) + 1 per set bit (0x0045ed7c), showCharacterEditor +/-1 (0x0043a20b / 0x0043a21a), narrateShot (0x00408f6f) | customizeCharacter, loadGolfPro, pairGolfersWithStory, showCharacterEditor, narrateShot, loadThemeFile | rateTile, prepareShot, trialShot, updateGolfers, scanShotArea (sum of 10, 0x0040971e..0x0040972d), skillPointDialog (0x0045f2e9), loadGolferRoster (0x00466335), promptAndSaveFile |
| +0xfc | 4 | | [UNCERTAIN U-0016] moved only as bytes 12..15 of the 16-byte .chr / .pro skill block: loadThemeFile stores the 16 bytes read at 0x0043810b into +0xf0..+0xff (0x00438145..0x00438151); promptAndSaveFile writes +0xf0..+0xff out (0x00437cb9..0x00437cd8) | 0x00438151 | loadThemeFile | promptAndSaveFile |

Every byte of the record is accounted for: +0x87 and +0xbd have no access at all; +0x01..+0x03, +0x05..+0x07,
+0x09..+0x0b, +0x0d..+0x0f, +0x11..+0x13, +0x15, +0x17 are the inner bytes of the dword / word fields above;
+0x25..+0x35, +0x38..+0x49, +0x4b..+0x5b, +0x5e..+0x6e, +0x71..+0x79, +0x7b..+0x83, +0x8a..+0x9b and +0xf1..+0xfb are
reached only through the indexed accesses of their arrays.

## 3. Flag dword +0x10

Set / clear / test give instruction addresses; a meaning is written only where the cited code shows it.
Everything marked `not determined` is [UNCERTAIN U-0009].

| bit | set | clear | test | meaning |
|---|---|---|---|---|
| 0x1 | 0x0042d9e4 | 0x00426611, 0x0042da20 | chooseShotTarget 0x004234f0, prepareShot 0x00424147, buildGolferComment 0x0046c1ae | not determined |
| 0x2 | 0x0042c9f7 | 0x00425edf | 0x00408a78, 0x0042c8ce, 0x0042d6b7 | not determined |
| 0x4 / 0x8 / 0x10 | 0x0042acd1 / 0x0042ac1f / 0x0042ab70 | all three by `and 0xffffffe3` in customizeCharacter / loadGolfPro | 0x0042a63c / 0x0042a475 / 0x0042a555 | not determined |
| 0x20 | 0x00425c52, 0x0042a6e0 | 0x00425b69, 0x0042a6eb | 0x0042daee, 0x00414d7e | not determined |
| 0x40 | 0x00425c7f | 0x0042a456 | 0x0042db08, 0x00414da2 | not determined |
| 0x80 | 0x00425478 | 0x00422de8, 0x0042cf83 | 0x00422dc8, 0x00425bde | not determined |
| 0x100 | 0x0042cf85 | 0x00422d66, 0x0042a607 | 0x00422d4a, 0x0042cf3d | not determined |
| 0x200 | customizeCharacter 0x0040f581 (the same block stores the index in 0x005a59f8, 0x0040f584), loadGolfPro 0x0046cf7d | | patronEvent 0x0042735f and updateGolfers 0x00429838 reset 0x005a59f8 to -1 when it is set; drawQueuedSprite 0x00462dc9 | marks the golfer whose index is in 0x005a59f8 |
| 0x400 | 0x0042c598, 0x0042d996 | | | not determined |
| 0x800 | 0x0042a79d, 0x0042c3fd | 0x0042bcaf | | not determined |
| 0x1000 | 0x0042b5cf | 0x0042a952, 0x0042bcaf | 0x0042a2d1 | not determined |
| 0x2000 | 0x0042a26e | 0x0042a360 | | not determined |
| 0x4000 | 0x0042b61b, updateWalkers 0x00403312 | | 0x00462d3d | not determined |
| 0x8000 | 0x0042b941 | 0x00429158 | | not determined |
| 0x10000 | updateMembership 0x00421f82 | 0x00429800, 0x0042b7db | 0x0042b551, 0x0046b5fe | not determined |
| 0x20000 | 0x00409569, 0x0045a05c, 0x004298a8 | 0x0042c3c8 | `xor` at 0x00415008 / 0x004150e2; tested 0x00467c64, 0x0045ee0e | not determined |
| 0x40000 | 0x0042c501 | 0x004290f9, 0x0042cd59, 0x0042d3a5 | 0x00428ab8, 0x00468535 | not determined |
| 0x80000 | 0x0042bac5 | 0x0042bacd | 0x00414e62 | not determined |
| 0x100000 | pairGolfersWithStory 0x0045e6db / 0x0045e70b (story accepted) | customizeCharacter 0x0040f515 | storyText 0x00466902, 0x00428b3c, drawGolferInfoPanel 0x0045d387 | paired with a story partner (re/match/golf_story.cpp, U-0003) |
| 0x200000 | 0x00428cd6 (golfer), 0x00428cf8 (partner) | matchUpdate 0x004277a5 | 0x004277b8 | not determined |
| 0x400000 | 0x004250fd, 0x00426178 | 0x00424dee | 0x0042c47a, 0x00414c09 | not determined |
| 0x800000 | advanceStory 0x00466416 / 0x00466438 | | | not determined |
| 0x1000000 | no setter found | | 0x0042a1f9; part of mask 0x21002000 at 0x0042b042 | not determined |
| 0x2000000 | 0x0042a350 | 0x0042a390 | 0x0042a2fd | not determined |
| 0x4000000 | 0x0042aae3 | 0x0042d3ea, 0x004277a5 | 0x00424dad, 0x0042a3dc | not determined |
| 0x8000000 | mainLoop far code 0x00414c10 | 0x00414bbe, 0x00415353 | | not determined |
| 0x10000000 | prepareShot 0x00424434; byte `or [+0x13], 0x10` at 0x0041b331 | 0x004242bb | 0x004291c6, 0x0041b376 | not determined |
| 0x20000000 | 0x00429806 (after hole = 0x13 at 0x004297e8), 0x0042b7e4 | | 0x004293a2, updateWalkers 0x0040300a, drawEmployeeViewPanel 0x004359aa, emitGolferThought 0x004686e9 | set when the golfer leaves the course |
| 0x40000000 | 0x0042b528 | 0x0042b539 | 0x0042b506 | not determined |
| 0x80000000 | matchUpdate 0x004286f3 | membershipReport 0x0040668f | 0x00429135, which then calls membershipReport | membership join / upgrade request (`re/analysis/golfers/00406670_membership.md`) |

## 4. C struct

Offsets are exact; names are the meanings above, `uNN` where none is established. MSVC packs this with no padding
(every field sits at a multiple of its own size).

```c
#include <stdint.h>

typedef struct Golfer {        /* 0x100 bytes, g_golfers 0x005794b8 */
    int32_t  x;                /* +0x00 world, tile = x >> 10 */
    int32_t  y;                /* +0x04 */
    int32_t  screenX;          /* +0x08 -1 when worldToScreen returned 0 */
    int32_t  screenY;          /* +0x0c */
    uint32_t flags;            /* +0x10 section 3 */
    int16_t  stepLeft;         /* +0x14 */
    uint16_t skillMask;        /* +0x16 bit i = skill[i] != 0 */
    uint8_t  kind;             /* +0x18 class bits 5..7, sub-kind 0..4 */
    uint8_t  prefs;            /* +0x19 bits 0..2 preferences, 4..7 [U-0010] */
    int8_t   facing;           /* +0x1a 0..7 */
    int8_t   u1b;              /* +0x1b [U-0011] */
    int8_t   club;             /* +0x1c 0..13 */
    int8_t   state;            /* +0x1d [U-0012] */
    uint8_t  frame;            /* +0x1e */
    uint8_t  frameBase;        /* +0x1f */
    int8_t   u20;              /* +0x20 [U-0012] */
    int8_t   hole;             /* +0x21 -1 new, 1..18, 0x13 leaving */
    int8_t   strokes;          /* +0x22 on the current hole */
    int8_t   holeStrokes[19];  /* +0x23 [hole] */
    int8_t   u36;              /* +0x36 signed, stepped toward 0 per hole [U-0013] */
    uint8_t  holeCounter[19];  /* +0x37 [hole]: story counter low byte / (chapter << 3) + 1 */
    uint8_t  holeChapter[19];  /* +0x4a [hole]: story stage (golf_story.cpp holeStory); [18] is +0x5c */
    uint8_t  holeMood[18];     /* +0x5d: code writes +0x5c + hole (hole 1..18), so holeMood[hole - 1];
                                  hole 0 would land on holeChapter[18]; drawGolferInfoPanel writes one slot higher */
    uint8_t  pad6f;            /* +0x6f only reached by drawGolferInfoPanel's [+0x5d + 18] */
    uint8_t  thoughtKind[10];  /* +0x70 history, [0] newest; bit 7 masked by readers */
    uint8_t  thoughtPlace[10]; /* +0x7a hole * 11 + byte +0x22 (+1 for kind >= 4) */
    uint8_t  thoughtTimer;     /* +0x84 countdown */
    uint8_t  lastThoughtKind;  /* +0x85 bit 7 from [0x005409a8] */
    uint8_t  lastThoughtPlace; /* +0x86 */
    uint8_t  pad87;            /* +0x87 no access */
    uint16_t thoughtArg[10];   /* +0x88 bits 0..13 argument, 14 / 15 outcome */
    uint16_t lastThoughtArg;   /* +0x9c */
    int16_t  wait;             /* +0x9e < 0 = skip steps */
    int16_t  clubsUsed;        /* +0xa0 bit per club +0x1c */
    int16_t  partner;          /* +0xa2 -1 = none */
    int16_t  mood;             /* +0xa4 -10..10 (updateWalkers stores -11) */
    int16_t  meterA6;          /* +0xa6 meter beside string 0x004d39ec */
    int16_t  meterA8;          /* +0xa8 meter beside string 0x004d39e4 */
    int16_t  meterAA;          /* +0xaa meter beside string 0x004d39f4 */
    int16_t  shotKind;         /* +0xac 0, 1, -1, 3, 4 */
    int16_t  pairClass;        /* +0xae 0..3 written, 0..5 read [U-0014] */
    int16_t  storyId;          /* +0xb0 index into g_storyDefs, -1 = none */
    int16_t  storyStage;       /* +0xb2 */
    int16_t  storyCounter;     /* +0xb4 */
    int16_t  type;             /* +0xb6 0..75 */
    uint16_t traits;           /* +0xb8 low byte from the type record, high byte random (unread) */
    int8_t   cls;              /* +0xba 0..2 */
    uint8_t  nameRec;          /* +0xbb index into the 0x38-byte records at 0x0058dd50 */
    uint8_t  maskBC;           /* +0xbc bitmask [U-0015] */
    uint8_t  padBD;            /* +0xbd no access */
    int16_t  order;            /* +0xbe stamp from 0x0056a51c */
    int32_t  dateStamp;        /* +0xc0 copy of g_date 0x00834170 */
    int32_t  shotStartX;       /* +0xc4 world */
    int32_t  shotStartY;       /* +0xc8 */
    int32_t  targetX;          /* +0xcc tile; -1 = player's shot pending */
    int32_t  targetY;          /* +0xd0 tile */
    int32_t  ballX;            /* +0xd4 world; 0 = no ball */
    int32_t  ballY;            /* +0xd8 */
    int32_t  ballZ;            /* +0xdc height */
    uint32_t ballDir;          /* +0xe0 full circle = 2^32 */
    int32_t  ballSpeed;        /* +0xe4 horizontal */
    int32_t  ballVz;           /* +0xe8 */
    int32_t  spin;             /* +0xec */
    uint8_t  skill[12];        /* +0xf0 bit i of +0x16 set iff skill[i] != 0 */
    uint8_t  fileTail[4];      /* +0xfc only copied with the .chr / .pro block [U-0016] */
} Golfer;                      /* sizeof == 0x100: static_assert(sizeof(Golfer) == 0x100) */

extern Golfer g_golfers[156]; /* 0x005794b8; 0..151 golfers, 152 scratch, 153 preview, 155 trialShot copy */
```

## 5. Open byte ranges

| range | entry |
|---|---|
| +0x10 bits marked `not determined` | U-0009 |
| +0x19 bits 4..7 | U-0010 |
| +0x1b | U-0011 |
| +0x1d, +0x20 | U-0012 |
| +0x36 | U-0013 |
| +0xae values 4 and 5 | U-0014 |
| +0xbc | U-0015 |
| +0xfc..+0xff | U-0016 |
| record 154 | U-0017 |

Not open but unused: +0x87 and +0xbd (no access), the high byte of +0xb8 (written, never read), +0x6f (written only
by drawGolferInfoPanel's off-by-one store for hole 18).

## 6. Findings and corrections

- **Coverage of the function index.** 444 of the 3,120 references lie in code outside every range of
  `re/functions_ghidra.tsv`: 441 in the block from 0x004115dc (mainLoop's per-golfer screen-position and
  frame-sequence pass 0x00414969..0x004155e1, the story calls 0x0041c430..0x0041c4ec / 0x0041e874..0x0041e9af and
  more) and 3 after appendDate (0x0040d92a..0x0040d95a). They are the only writers of +0x08 / +0x0c and the only
  accessors of +0x1f. `record_fields.py` sweeps such gaps by default and names them `(no function)`; xref.py and
  the C2 notes do not see them until the code is made a function (CLAUDE.md already notes that updateGolfers'
  call 0x00417609 lies outside mainLoop's recorded ranges).
- **exe_2_globals.tsv g_golferTypeSlot 0x0057956e** said "offset 0xae": that is the 0x5794c0-based offset; from the
  record start it is +0xb6. Row corrected.
- **exe_1_globals.tsv gThoughtTable 0x00579540** was typed `int[]`: it is +0x88 of record 0, ten 16-bit slots
  (every access is a word or one of its bytes). Row corrected.
- **exe_0_globals.tsv g_walkers** described 0x00585862 as a parallel array; it is field +0x12 of walker record 0,
  and the table also holds the employees (record_tables.md). Row corrected.
- **New rows** in exe_2_globals.tsv: g_golfers 0x005794b8, g_golferTypeState 0x005849e0, g_storyDefs 0x0053a454.
- **U-0003** reads `byte +0x22 == 0` as a gate; +0x22 is the stroke count on the current hole, so the gate is
  `no stroke yet on this hole`.
- **golf_course.cpp / c3fa / c3fy / c3fx_deps.tsv** use base 0x5794c0 (section 1). golf_course.cpp's comment on
  rateTile (its +0xcc / +0xd0) names the ball position +0xd4 / +0xd8, not the target tile +0xcc / +0xd0.
- **exe_6.tsv updatePairSnapshot** says it copies from `table 0x00579594`: that address is +0xdc (ball height) of
  record 0; the function reads +0xc4 / +0xc8 or +0xd4 / +0xd8 / +0xdc (0x00409975..0x004099d3).
- **golferScore 0x00453260**: drawGolferInfoPanel prints its result after the string 0x004d3a0c (`, age `)
  (0x0045c7e0..0x0045c81e); the name does not describe that use. Function names are left to the names owner.
- **pickGolferName 0x00467600** reads walker records, not golfer records (record_tables.md).
- **Per-hole mood base**: matchUpdate and pairGolfersWithStory write +0x5c + hole, drawGolferInfoPanel
  +0x5d + hole; nothing reads the array.
- **updateWalkers stores mood -11** (0x004032f7), outside emitGolferThought's -10..10 clamp.
- **drawGolferActionPanel** passes the ball's x (+0xd4) with the golfer's y (+0x04) to typeAtPos
  (0x00436457..0x00436465), the same mixed pair as updateGolfers' water sound at 0x0042d7ff.
- **loadGolferRoster** reads every `.pro` file's skill block into record 76 (0x0057e1a8) as a scratch buffer
  (0x004662d0..0x004662d5), so record 76 is overwritten while the roster loads.

## 7. Neighbouring tables

Walker / employee records 0x00585850, per-class statistics 0x0056ae70, per-type state 0x005849e0 and story
definitions 0x0053a454: `record_tables.md`.
