"""Writes log/roadmap/S6/6_decisions.tsv (schema SCHEMA.md) from the verified rows below. Run: ~/bin/py -3.12 make_decisions.py"""
import pathlib

OUT = pathlib.Path(__file__).resolve().parents[1] / "6_decisions.tsv"
COLS = ["id", "source", "function", "summary", "class", "changes_save", "changes_balance", "changes_sim", "changes_frame", "hang",
        "fault", "effect_note", "recommendation", "switch", "switch_state", "evidence", "uncertain"]
SRC = "model_defects.md"

R = []


def row(id, function, summary, cls, save, bal, sim, frame, hang, fault, note, rec, switch, state, evidence, unc="", src=None):
    R.append([id, src or SRC, function, summary, cls, save, bal, sim, frame, hang, fault, note, rec, switch, state, evidence, unc])


row("D1", "showBulletinBoard 0x0046f0e7, stores 0x0046f0f9 / 0x0046f103",
    "stores 0 and 0x0a as two words at 0x0058316e / 0x00583170 (cells 182..185 of tile_build_code) every time the board runs, then skillPointDialog(0x9c)",
    "overrun-silent", "yes", "unknown", "yes", "unknown", "no", "no",
    "read: ebx is 0 (xor ebx, ebx at 0x0046ee56, no later write) and cx = movsx byte [0x004d60a9] = 0x0a (exe constant), so the stores set cells 182,183 = 0, 184 = 0x0a, 185 = 0 (tile x 3, y 32..35; the fill value is 0xff = no code). Readers of the grid: buildToolLabel 0x0040a248, demolishObject 0x0040a602 / 0x0040a62f / 0x0040a7c4, serializeCourse 0x0040b41f (saved), buildGolferBioText 0x004316b6 (fill). Demolishing those four tiles reads code 0 / 10 instead of 0xff: balance effect depends on tile_refund there, not measured. A fix changes a saved grid",
    "ask", "showBulletinBoard.recordPastTable", "to-implement",
    "disassembly of 0x0046e810..0x0046f179 (6_probes/d01_listing.txt); grid readers by 6_probes/plib.py refs()", "U-1589")

row("D2", "playSound 0x004481b0 / startSoundSlot 0x00448200",
    "300 Snd slots indexed with no bound, only -1 refused (0x004481b4)", "overrun-silent", "no", "no", "no", "no", "no", "unknown",
    "read: slot = 0x0080d840 + id * 0x6c, no compare other than id == -1. Of the 95 call sites 88 pass an immediate id (24..267, all below 300); 7 pass a computed id (playSoundAt 0x0040c645 / 0x0040c6d0, matchUpdate 0x004281b7 / 0x004281f6, showCharacterEditor 0x0043866d / 0x0043a2f3, advanceStory 0x004665e2); their value ranges were not traced",
    "keep", "none", "not-needed", "6_probes/scans.py (scans.log)", "U-1585")

row("D3", "collectDrawList 0x0047e330, serviceMovie 0x0049c8e0",
    "draw list (512 entries) append not located; the movie id list has no writer at all", "overrun-silent", "no", "no", "no", "no", "no", "unknown",
    "movie half: g_movieCount 0x008406e4 and g_movieIds 0x00840698 are referenced only by serviceMovie (reads at 0x0049c8e0 / 0x0049c8f9 / 0x0049c8ed), no instruction in the image stores to either, so the list is always empty: dead state, no overflow possible. Draw-list half: collectDrawList reads the window children (this + 0x224 / 0x22c) and has no capacity compare of its own; the instruction that appends to the 512-entry list was not located here",
    "keep", "none", "not-needed", "6_probes/scans.py (scans.log)", "U-1589")

row("D4", "addSprite 0x004628d0 (enqueueSprite 0x00462a30)",
    "addSprite writes the sprite queue arrays without enqueueSprite's count compare", "overrun-silent", "no", "no", "no", "unknown", "no", "unknown",
    "read: enqueueSprite compares ebp with 0x3ff (0x00462a37) and 0x400 (0x00462bc4); addSprite's only compares are 0x3e8 / 0x4b0 / test ah, 1 / test ecx (0x004628d8..0x00462908), none against the queue count. Queue is render state, not compared by the segment gate",
    "keep", "none", "not-needed", "instruction scan of both functions (scans.log)", "U-1552")

row("D7", "stampCourseTiles 0x0040e2f1..0x0040e30f",
    "free-slot scan over the 256 placed objects reads past the table, then returns 0 (read only)", "harmless", "no", "no", "no", "no", "no", "no",
    "the scan (0x0040e2f6 add eax, 0x10; 0x0040e2fa cmp word [eax], -1) only reads; index >= 0x100 returns 0 at 0x0040e30f with no write to any record. Unicorn: full 256-record table on sandbox_basic and course_season images completes with eax 0, scan stops at the first word 0xffff at stride 0x10 (0x0058dd48), writes only the 4 footprint bytes that precede the scan; with every 0xffff word of .data removed the scan runs on into .rsrc and stops at 0x008431f8, still completes. A bounded scan changes no run (return 0 either way). S6.1's `fix refuses the placement` does not change the original: it already refuses",
    "keep", "none", "not-needed", "6_probes/d07_stamp.py (d07_stamp.log)", "")

row("D8", "placeHabitat 0x00405fbc",
    "reads +0x12 of slot + 1; slot 127 reads 0x005736c2 (ball record 0, byte 2 of height)", "overrun-silent", "no", "no", "no", "no", "yes", "no",
    "a read of a defined global. With the byte != 0xff the cluster loop ends (probe (a), completes); with 0xff it never ends (D40a). Treating slot 128 as taken changes no run that completes, so the fix is the D40a fix",
    "fix", "placeHabitat.clusterLoopBound", "to-implement", "6_probes/d40_habitat.py (d40_habitat.log) arms (a) and (b)", "U-1581")

row("D11", "spawnWalker 0x00402970",
    "free-slot search over 64 walker records has no bound: with 64 live walkers it returns 64 and initialises a record at 0x00586b50 (g_editQueueX)", "overrun-silent",
    "unknown", "no", "yes", "unknown", "no", "no",
    "Unicorn on sandbox_basic and course_season images, flags byte +0x12 of all 64 records nonzero: completes, eax = 0x40, writes the new record over g_editQueueX[0..18] (0x00586b51 / 0x00586b55 / 0x00586b60 / 0x00586b62 / 0x00586b63 / 0x00586b66 / 0x00586b6e) and the RNG words; the scan stops because the byte at 0x00586b62 is 0 in both images. With every stride-0x4c byte to the end of .data nonzero it returns 0x937d and writes a record at the end of .data, still no fault. No hang and no fault: S6.1's `hang` is not supported. A fix (refuse) changes sim state on a run the original completes",
    "ask", "spawnWalker.fullTable", "to-implement", "6_probes/d11_spawnwalker.py (d11_spawnwalker.log)", "U-1580")

row("D12", "mainLoop 0x004157c0 / 0x004157c7, count 0x0058bcb0",
    "g_ballScreenXs / Ys (32 entries each) appended with no bound; count is g_nameLabelCount, reset at 0x0041495a", "overrun-silent", "no", "no", "unknown", "yes", "no", "unknown",
    "read: one append per on-screen golfer label that has a ball (0x0041557f test of +0x58c), loop over the golfer records (152), no compare of edi (0x0041577b..0x004157d5). Entry 32 of Xs lands in Ys[0] (0x005a4868), entry 32 of Ys lands in g_surfaces5a48e8 (a constructed static SpriteRef array whose only code references are its constructor 0x00404e5c and exit destructor 0x00404ed2). The reader at 0x00418b4d..0x00418bbd draws labels from the same arrays, so a fix changes the frame once the count passes 32. Number of golfers on screen with a ball in one frame not measured",
    "ask", "ballScreenList.overflow", "to-implement", "instruction reading 0x00415540..0x004157d5; refs scan of 0x005a47e8 / 0x005a4868 / 0x005a48e8", "U-1587")

row("D13", "showMembershipRoster 0x00454fb6",
    "strcpy of the type name (first field of a 0x230-byte record) into a 19-byte buffer holding the sentinel string", "harmless", "no", "no", "no", "no", "no", "no",
    "S6.1 / model note read it as a copy of the 0x230-byte slot; the instructions copy to the NUL (repne scasb at 0x00454fc3, rep movsd / movsb at 0x00454fd2 / 0x00454fd9) from the record's first field at 0x004d6098 + t * 0x230. Destination esp + 0x2c holds the 19-byte constant `zzzzzzzzzzzzzzzzzz` + NUL (0x004d32c4..0x004d32d6). The 76 shipped type names are at most 10 characters (Jacqueline); the table is an exe constant, the start images show the same names. The overrun needs a name of 19 or more characters, which no shipped data holds",
    "keep", "none", "not-needed", "6_probes/scans.py (scans.log)", "")

row("D22", "rebuildTerrainGrid 0x00449736",
    "inline tileAt tests the row-offset register ebp instead of x (edi) for the lower bound", "harmless", "no", "no", "no", "no", "no", "no",
    "read 0x00449711..0x00449785: the loop starts at edi = 0x31, ebp = 0x992 = 49 * 50 and each pass does `dec edi` / `sub ebp, 0x32` (0x00449781 / 0x00449782), so ebp == edi * 50 on every pass and the loop exits (jns 0x00449787) before either goes negative. The test is equivalent to the intended one at this site; no x below 0 reaches it",
    "keep", "none", "not-needed", "disassembly 0x00449700..0x0044978b", "")

row("D36", "netQueueMessage 0x0049b6e1..0x0049b718",
    "peer name copied (strcat form) into the 0x43-byte field at record + 0x185 with no bound; the source is a string of a polled network message", "overrun-silent", "no", "no", "no", "no", "no", "unknown",
    "read: record = this + 0x170 + slot * 0x58 (0x0049b6bf..0x0049b6ca), name at +0x185 (0x0049b6e1), repne scasb on the source at 0x0049b6ec, rep movsd / movsb at 0x0049b70d / 0x0049b718, no length compare. Callers: netPoll 0x0049b834 (source = polled record + 4) and 0x0049b8e4. The size of the polled record's name field was not read. Network code is outside the offline port",
    "keep", "none", "not-needed", "disassembly 0x0049b690..0x0049b740", "U-1589")

row("D40a", "placeHabitat 0x00405fbc..0x00406019",
    "cluster loop never advances; with slot 127 first and the byte 0x005736c2 == 0xff it never ends", "hang", "no", "no", "no", "no", "yes", "no",
    "Unicorn (course_season and sandbox_basic images, slots 0..126 used, 127 free, rng scripted to reach the arm): byte 0x00 completes; byte 0xff exhausts 3 M and 30 M instructions (status hang, eip 0x00405978 in addHabitatSlot's scan, 7,469 / 74,633 Random::range draws, the state repeats). Slots 5 and 6 free (picked 5): completes. The loop ends whenever the next slot gets claimed, which holds for every slot but 127, so a precheck `slot == 127 and the table full` changes no completing run",
    "fix", "placeHabitat.clusterLoopBound", "to-implement", "6_probes/d40_habitat.py (d40_habitat.log)", "U-1581", src=SRC + " 40")

row("D40b", "addHabitatSlot -1 link write 0x00406011 (placeHabitat)",
    "full table: the link word is written 0x14 bytes before the table (0x00572ca8, tile_type cell 2496)", "overrun-silent", "no", "no", "no", "no", "yes", "no",
    "reached only inside the D40a hang: the second addHabitatSlot call happens when flag(slot + 1) == 0xff, and that call returns -1 only when no slot is free, i.e. only for slot 127 with byte 0x005736c2 == 0xff. Probe (b) shows the write (0x00572ca8 <- 0x7f) and then the loop. No run that completes performs it. S6.2's `keep` is replaced by the D40a fix",
    "fix", "placeHabitat.clusterLoopBound", "to-implement", "6_probes/d40_habitat.py arm (b)", "U-1581", src=SRC + " 40")

row("D41", "spawnObject 0x004011e0, search 0x00401264..0x0040129d",
    "claims the animal record, then draws Random::range(4) twice until the bank cell [lot * 0x74 + si * 6 + cx + 0x004e6d70] is 0xff; with all 16 cells of rows / columns 0..3 occupied it never ends", "hang",
    "no", "no", "no", "no", "yes", "no",
    "Unicorn (lot 0 and 3, both images, animal slot free): all 16 cells nonzero exhausts 2 M instructions with 249,997 draws (record writes already made: 0x0056d1d8 / 0x0056d1e4); one cell free completes after 4 draws. The record is claimed before the search (0x00401221..0x00401255), so a precheck for a free cell in the 16 changes no completing run (the RNG draws of a run that finds a cell are unchanged)",
    "fix", "spawnObject.cellSearchBound", "to-implement", "6_probes/d41_spawnobject.py (d41_spawnobject.log)", "U-1582")

row("D42a", "loadGolfPro 0x0046cbab, 0x0046cc63",
    "idiv by (d * 5 + 0x14) * 2 (#DE at d = -4) and by 4 - d (#DE at d = 4), d = g_difficulty 0x00822c88", "fault", "no", "no", "no", "no", "no", "yes",
    "Unicorn (course_season image, pro records 1..99 filled): d = 4 faults at 0x0046cc63, d = -4 at 0x0046cbab (UC_ERR_EXCEPTION); d = 0..3 and -1 pass both divisions. The game writes d in 0..3, -1 (menu), 5 and 6 (mainLoop 0x00421503 / 0x004215f7), never 4 or -4, so no completing run has a divisor of 0",
    "fix", "loadGolfPro.divideAndRejectGuard", "to-implement", "6_probes/d42_golfpro.py (d42_golfpro.log)", "", src=SRC + " 42")

row("D42b", "loadGolfPro 0x0046cb70..0x0046cc6f",
    "rejection loop (Random::range(100) until a filled pro record 1..99 whose thresholds fit) never ends with no filled record", "hang", "no", "no", "no", "no", "yes", "no",
    "Unicorn (course_season image, d = 2): no record filled and only record 0 filled exhaust 3 M and 30 M instructions; one record filled (any of 1..99) completes. The `used` test is dropped after 0x320 passes (0x0046cc05..0x0046cc21) and the acceptance window widens with the pass count for d in 0..3, so any filled record 1..99 ends the loop. A precheck `no filled record 1..99` changes no completing run",
    "fix", "loadGolfPro.divideAndRejectGuard", "to-implement", "6_probes/d42_golfpro.py", "", src=SRC + " 42")

row("D42c", "loadGolfPro 0x0046cc53..0x0046cc8e",
    "with d = 5 or 6 the window bound uses 4 - d < 0, shrinks with the pass count and the loop does not end even with every pro record filled", "hang", "no", "no", "no", "no", "yes", "no",
    "Unicorn (course_season image, records 1..99 filled with skill 10, d = 5 and d = 6): both exhaust 3 M instructions in the loop (eip 0x0046cba4 / 0x0046cbb3); d = 0..3 complete. Whether loadGolfPro runs with g_difficulty 5 or 6 (values mainLoop writes at 0x00421503 / 0x004215f7 before showThemePackSelect) was not settled: startNewCourse 0x0046ddd0 restores d from the value it saved at entry. A fix would change which pro is chosen on a run that completes only if the window rule changes",
    "ask", "loadGolfPro.windowForDifficultyFiveSix", "to-implement", "6_probes/d42_golfpro.py", "U-1583", src=SRC + " 42")

row("D43", "placeAnimal 0x0040189a",
    "idiv by the sprite frame count dword [esi * 4 + 0x0053f3e8]; a count of 0 raises #DE", "fault", "no", "no", "no", "no", "no", "yes",
    "Unicorn (both images, animal record 0 on screen via a return-1 stand-in for the visibility call 0x0042fb90): count 0 faults at 0x0040189a with zero bytes written before it; count 6 completes. A fix that returns before the idiv leaves no partial state",
    "fix", "placeAnimal.zeroFrameCount", "to-implement", "6_probes/d43_placeanimal.py (d43_placeanimal.log)", "")

row("D46", "replaceInText 0x0045b7c0",
    "saves the tail of g_textBuffer from the match on into a 0x200-byte stack buffer at esp + 0x10 with no bound; the return address follows the buffer", "fault", "no", "no", "no", "no", "no", "yes",
    "Unicorn: tail strlen 0x100 / 0x1ff completes; strlen 0x200 writes the NUL over the low byte of the return address; strlen 0x204 returns to 0x62626262 (fetch fault), 0x210 faults at 0x0045b81b reading 0x62626262. g_textBuffer holds 0x1000 bytes, so the arm needs a text of more than 0x1ff characters after the match. A clamp or heap buffer changes no run whose tail is below 0x200",
    "fix", "replaceInText.tailLimit", "to-implement", "6_probes/d46_replace.py (d46_replace.log)", "U-1562")

row("D47a", "loadStoryStrings 0x00467054..0x0046707f (0x00466ee1..0x00466f1e)",
    "appends the saved indented line from an uninitialised stack buffer ([esp + 0x160]) when no indented line was read before", "wrong-value", "no", "no", "unknown", "unknown", "no", "no",
    "read: the only writer of the saved line is the copy in the space-line branch (0x00466f8a..0x00466fb5); the two appends (0x00466ee1, 0x00467054) read it. The appended text is stack garbage up to a NUL, set by earlier call frames. Not run (needs the CRT file stand-ins). Port appends an empty string",
    "keep", "none", "not-needed", "disassembly 0x00466b70..0x00467102 and C3 notes c3dz.cpp:120..170", "U-0901")

row("D47b", "loadStoryStrings 0x004670a1..0x004670cf",
    "cache store strcpy's the whole g_textBuffer into a 0x100-byte entry text field", "overrun-silent", "no", "no", "yes", "unknown", "no", "no",
    "read: cache = 8 entries of 0x10c bytes from 0x00838228 (3 header dwords, text at +0xc, 0x100 bytes), strcpy at 0x004670a1..0x004670cf with no length compare. The text is the selected header line plus one reply appended to whatever g_textBuffer already holds; the longest shipped header + reply pair over the 40 story files under original/Themes is 155 bytes (Firaxis/CMxLxMxxCuttingEdge.txt), so the arm needs 101 or more bytes already in the buffer. fgets caps a line at 0xfa",
    "ask", "loadStoryStrings.cacheEntryLimit", "to-implement", "disassembly + story-file scan (scans.log)", "U-1584")

row("D48a", "buildGolferComment 0x0046c47d..0x0046c6bf",
    "three stack slots filled from g_textBuffer with no length test; the slots are 0x38 bytes (esp + 0x24), 0x80 bytes (esp + 0x5c) and the one at esp + 0xdc", "overrun-silent", "unknown", "no", "unknown", "unknown", "no", "unknown",
    "read: copies at 0x0046c49b (to esp + 0xdc), 0x0046c4cf (to esp + 0x24) and 0x0046c6bf (to esp + 0x5c), each preceded by a repne scasb on g_textBuffer and no compare. Slot spacing 0x5c - 0x24 = 0x38, so S6.2's `0x40-byte slots` is replaced by 0x38 and 0x80. The first two hold a golfer name built by 0x004676e0 after g_textBuffer was cleared (0x0046c4a4); a name of 0x38 or more characters overruns the next slot",
    "ask", "buildGolferComment.slotLimit", "to-implement", "disassembly 0x0046c440..0x0046c6d0", "U-1563")

row("D48b", "computeStoryReply 0x00466a73..0x00466a85",
    "calls clamp(mood [+0xa4], counter [+0xb4], 0x63): the story counter is the lower bound", "wrong-value", "no", "no", "yes", "no", "no", "no",
    "read: movsx eax, word [ecx + 0x57956c] (+0xb4) pushed second, movsx ecx, word [ecx + 0x57955c] (+0xa4) pushed last; clamp 0x00467130 is clamp(x, lo, hi) (cmp x, lo / jge at 0x00467138; cmp x, hi at 0x00467142). Field names from golfer_record.md rows +0xa4 (mood) and +0xb4 (story counter). The result drives the story chapter",
    "ask", "computeStoryReply.clampLowerBound", "to-implement", "disassembly 0x00466a73..0x00466a99 and 0x00467130", "")

row("D49a", "postEvent 0x0046e7b0",
    "the id indexes the 0x30-byte ring at 0x004c15a0 without a bound; id 22 or more writes outside it", "overrun-silent", "unknown", "no", "yes", "no", "no", "unknown",
    "read: 21 of 22 call sites push an immediate id (values 0 to 0x10 and 0x15), the 22nd (showTournamentResults 0x0045abad) pushes 0x11 + the course-type byte 0x005a34e0 (0x0045aba7). Shipped course types are 0..3 (ids 0x11..0x14) and 4 gives 0x15; type 5 or more gives 22 or more. The writes are conditional on the slot's first dword being 0 (0x0046e7c6..0x0046e7ce)",
    "ask", "postEvent.idBound", "to-implement", "call-site scan of 0x0046e7b0 (scans.log)", "U-1588")

row("D49b", "patronEvent 0x00427114..0x00427124",
    "the fourth block runs only when (byte & 0xe0) == 0x100", "dead-state", "no", "no", "no", "no", "no", "no",
    "read: `and eax, 0xe0` at 0x00427114 then `cmp eax, 0x100` / `jne 0x00427345` at 0x00427119 / 0x0042711e; a value masked with 0xe0 never equals 0x100, so the block at 0x00427124 is unreachable code",
    "keep", "none", "not-needed", "disassembly 0x004270c0..0x00427160", "")

row("D50", "rebuildHeightfield 0x0042f88a..0x0042f8fb",
    "relax / raise pass repeats until a pass changes nothing; a type with flags bits 0, 1 and 2 makes relaxTile and raiseFromNeighbours undo each other", "harmless", "no", "no", "no", "no", "no", "no",
    "the type flags dword (0x0057837c + type * 0x30) is copied from the exe constant table at 0x004c1a40 by mainLoop (0x0040f5fc..0x0040f60e) and no other instruction writes it; no shipped type has bits 0 + 1 (relax) and the only two types with bit 0 and bit 2 are 21 and 22 (flags 5). Unicorn: shipped flags with grids of types 21 / 22 / 2 / 4 / 0 / 1 (6 grids per image) all complete; forcing type 21 flags to 7 hangs on 1 of 3 grids (eip 0x0040bf64), so the hang arm exists only for a table value no shipped data has. S6.2's `fix` is not needed on shipped data",
    "keep", "none", "not-needed", "6_probes/d50_heightfield.py (d50_heightfield.log)", "U-0806")

row("D51", "buildGolferBioText 0x00431719..0x0043180a",
    "twelve picks draw Random::range(0x4c) until the type has the residue, class and an unused flag; with none left it never ends", "harmless", "no", "no", "no", "no", "no", "no",
    "the type table (0x004d60a8 + t * 0x230) is an exe constant and the used flags (0x005849e2) are cleared at 0x00431698, so each pick i needs a type with t % 3 == i % 3 and class [5, 3, 6, 7][i / 3]; every one of the 12 pools holds 5..8 types, one is consumed per pick, none is exhausted. Unicorn with a draw cycle over 0..0x4b reaches the end of the picks (0x00431810) and completes",
    "keep", "none", "not-needed", "6_probes/d51_bio.py (d51_bio.log)", "")

row("D52a", "generateRandomCourse 0x00470b3b..0x00470c1d, read at 0x00470b91",
    "base tile type [esp + 0x10] is read without a write for course type 4 (n < 8, or g_flags bit 24 set) and for course types above 4", "wrong-value", "no", "no", "unknown", "unknown", "unknown", "no",
    "read: jump table 0x00472208 maps types 0..3 to 0x00470b4e (stores 4), type 4 to 0x00470b54 (fills the grid with 0x0e, stores nothing in [esp + 0x10]), above 4 to 0x00470b65; the n >= 8 / n >= 12 arms (0x00470b69..0x00470b8d) set it only when g_flags bit 24 is clear. Unicorn with a pre-filled stack: the first tile type byte equals the fill pattern (0x00 -> 0, 0x55 -> 0x55) for type 4 / n = 5 and for type 4 / flag set; n = 9 type 4 without the flag stores 5 and completes. With fill 0x00 and 0x55 these runs did not finish in 400 M instructions (the control, type 3, finishes below 200 M). Shipped courses are types 0..3 (0x00571ff8 + n * 0x2e, n = 0..16), so the arm needs a user theme with type 4 or more. The real stack contents are not reproducible offline",
    "ask", "generateRandomCourse.uninitBaseType", "to-implement", "6_probes/d52a_basetype.py (d52a_basetype.log)", "U-1588")

row("D52b1", "generateRandomCourse 0x0047184a..0x00471885",
    "layout kind 1: draws a cell (x 10..39, y 0..29) until its tile type is 0x11; never ends when the window holds none", "hang", "no", "no", "no", "no", "yes", "no",
    "Unicorn (sandbox_basic image, type 3, n = 9, kind forced to 1 at 0x0047165c): a type-0x11 cell in the window completes; none exhausts 400 M and 1.2 G instructions with eip inside the loop (0x00471871 / 0x00471883). 32 seeds over course types 0..3 (n = 9) all complete, so a normal grid has such a cell. A precheck for a 0x11 cell in the window changes no completing run (draws unchanged)",
    "fix", "generateRandomCourse.searchBound", "to-implement", "6_probes/d52b_search.py, d52b_seeds.py (d52b_search.log, d52b_run1.log)", "U-1586", src=SRC + " 52")

row("D52b2", "generateRandomCourse 0x004715b9..0x00471616",
    "clubhouse search: draws (15 + range(17), 15 + range(17)) until the tile type equals the base type and evalPlacementArea accepts; never ends when no cell of the window has the base type", "hang",
    "no", "no", "no", "no", "yes", "no",
    "Unicorn (same state, window of 17 x 17 cells rewritten to type 3 at the loop entry): control completes, the rewritten grid exhausts 500 M instructions with eip in Random::range (0x0045c1ba). The search has no counter (jne 0x004715b9 at 0x004715fd, je 0x004715b9 at 0x00471616)",
    "fix", "generateRandomCourse.searchBound", "to-implement", "6_probes/d52b_clubhouse.py (d52b_clubhouse.log)", "U-1586", src=SRC + " 52")

row("D52b3", "generateRandomCourse 0x0047172e..0x0047176f and the other evalPlacementArea loops",
    "random-cell searches gated by evalPlacementArea (-1 = reject) with no counter", "hang", "no", "no", "no", "no", "unknown", "no",
    "read: `je 0x0047172e` at 0x0047176f repeats the draws of rand(0x28) + 5 pairs while 0x0040db90 returns -1; not run on a constructed grid, so the hang arm is structural only",
    "ask", "generateRandomCourse.searchBound", "to-implement", "disassembly 0x0047172e..0x0047178c", "U-1586", src=SRC + " 52")

with OUT.open("w", encoding="utf-8", newline="\n") as f:
    f.write("\t".join(COLS) + "\n")
    for r in R:
        assert len(r) == len(COLS), r[0]
        for c in r:
            assert "\t" not in c and "\n" not in c
        f.write("\t".join(r) + "\n")
print("wrote", OUT, len(R), "rows")
