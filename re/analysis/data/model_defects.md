# Original defects found by the phase-1 data model (index, stream G1, 2026-10-18)

Accesses and computations of the original golf_clean.exe that leave a typed object, read memory nothing writes, or
lose data. They are catalogued, never repaired in the model (decision 3); the port's keep-or-fix list (re/PORT_PLAN.md)
decides each. Every entry cites the instructions; the full evidence is in the stream write-up named in the last column.
`checker` = the entry explains a re/tools/model_check.py error over the merged map (log/p1/g1/viol_all.tsv).

## 1. Accesses outside a typed object

| # | defect | instructions | source |
|---|---|---|---|
| 1 | showBulletinBoard stores a type word and the traits word into golfer record 156, one past the 156 records: the bytes land in tile_build_code (g_tileBuildCode 0x005830b8, cells 182 / 184..185); then skillPointDialog runs on golfer index 156 (`push 0x9c`) (checker: 2 width_cross) | 0x0046f0f9 / 0x0046f103 (stores to 0x0058316e / 0x00583170), 0x0046f0e7 | model_b.md 2.5, model_c.md 10 / 12 |
| 2 | playSound / startSoundSlot index the 300 Snd slots with no bound test (only -1 is refused) | 0x004481b0 | model_d.md 5.3 |
| 3 | collectDrawList appends to the 512-entry draw list with no capacity test (U-0325); serviceMovie has no bound on its movie id list (U-0326) | collectDrawList 0x47e330, serviceMovie 0x49c8e0 | model_d.md 5.4 |
| 4 | addSprite draws immediately and writes element `count` of the sprite queue arrays without enqueueSprite's 0x3fe guard | addSprite 0x4628d0 | model_d.md 5.6 |
| 5 | edge reads outside the 50 x 50 tile grids: mainLoop's neighbour tests read tile_type at x * 50 + y - 1 / + 1 / +- 0x32 / + 0x33 (x = 0 reads the course table, x = 49 reads 0x572cac and the habitat table), through the biased base 0x5722e7 | mainLoop 0x00411611..0x00411710, 0x004126c7, 0x00412838, 0x0041161e (tile_variant at 0x0056988b); scanTileLine 0x004222bc; trialShot 0x004228c2; updateGolfers 0x0042c7db / 0x0042cbbe | model_c.md 12, re/tools/model_merge.py MARKS |
| 6 | heightBlend reads one row past the grid at x = 49; canStep reads before its bound check; generateRandomCourse's river and smoothing loops step one row outside (`jle` one past 0x572cac); floodMark reads AND writes outside tile_flags | heightBlend 0x0040c225, canStep 0x00407466, generateRandomCourse 0x00470fbb, floodMark 0x0042f182 / 0x0042f150 | model_c.md 12 (reach U-0207) |
| 7 | stampCourseTiles' free-slot scan has no upper bound and reads past the 256 placed objects when the table is full; a full table returns 0, the same as slot 0 | 0x0040e2f6..0x0040e30f | model_c.md 12 |
| 8 | placeHabitat reads +0x12 of the next habitat record (slot 127 reads 0x005736c2, outside the table) | 0x00405fbc | model_c.md 12 |
| 9 | hole records 20..31 are reachable through the 5-bit hole field (20 records exist) | U-0208 | model_c.md 12 |
| 10 | sinScaled interpolates between table entries i and i + 1 for i = (x & 0x3fffffff) >> 22 = 0..255, but initSinTable fills 256 entries (0x83b9f4..0x83bdf4, entry 255 = sin(pi / 2)): at i = 255 it reads the dword at 0x83bdf4, which no instruction in the image writes (bss 0) | initSinTable 0x00491c1a..0x00491c4a; sinScaled 0x00491ca7, 0x00491cf9, 0x00491d43 | this file (G1) |
| 11 | spawnWalker's free-slot search over the 64 walker records has no bound | spawnWalker 0x00402970 | model_b.md 2.5 / 4.1 |
| 12 | the 32-entry ball-screen lists g_ballScreenXs / Ys are appended with no bound check (count 0x0058bcb0 reset at 0x0041495a) | 0x005a47e8 / 0x005a4868 | model_b.md 2.5 / 5.6 |
| 13 | showMembershipRoster strcpy's a 0x230-byte slot into a 19-byte buffer | 0x00454fb6 | CLAUDE.md round 31 |
| 36 | netQueueMessage copies a peer name into SgNetPeerRecord.name (0x43 bytes at +0x15) with the length strlen + 1 of its argument and no bound (`repne scasb` 0x0049b6ec, `rep movsd` / `rep movsb` 0x0049b70d / 0x0049b718): a name of 0x43 characters or more overruns into the next record (or past this + 0x6f0 for record 15) | 0x0049b6e1..0x0049b718 | model_corrections.md 5 (G3) |

## 2. Wrong width, mixed strides, uninitialised or never-written data

| # | defect | instructions | source |
|---|---|---|---|
| 14 | loadStoryStrings reads 0x00838220 as a dword and truncates it to 16 bits while every writer stores 16 bits: the upper half is two bytes nothing writes (0 in bss); the C3 hook transcribes the same read (checker: 1 width_cross) | 0x004670db; hook c3dz.cpp:172 | model_e2.md 8 |
| 15 | resetSpriteQueue loops 0x400 times storing -1 into one dword (0x8371f4) instead of the link array | resetSpriteQueue 0x4627d0 | model_d.md 5.2 |
| 16 | updateWalkers reads one byte of a sheet's frame count (`cols[400]` low byte): correct only below 256 frames (U-0328) | 0x00403455 | model_d.md 5.5 |
| 17 | g_namedStoryIndex is found by walking 0x64-byte slots but compared with and stored as a 50-byte story id (U-0117) | 0x00465ae8; 0x0045e012, 0x0045e6f8 | model_b.md 4.6 |
| 18 | shotPower's 10-entry result cache key leaves out its third argument, which selects 0x4223f0 or 0x4223c0: a hit can return the other mode's result | 0x004224c5..0x004224e3 | model_b.md 4.7 |
| 19 | loadGolferRoster's .pro path advances ebp by 0x38 before storing the skills and their sum: +0x25.. and +0x36 go into the next record | 0x0046630b, 0x0046633b, 0x00466354 | model_b.md 4.5 (CLAUDE.md round 33) |
| 20 | genNoise computes an 8-neighbour sum and stores the centre byte instead; it reads 0x00838c08, which nothing writes | 0x00467436..0x0046746e | model_c.md 12 |
| 21 | corner_level: unclamped inc / dec in stepCourseEditQueue; mainLoop's reset writes 50 of 51 bytes of rows 0..49 only; byte x * 51 + 50 is both vertex (x, 50) and (x + 1, -1) (U-0209) | mainLoop 0x0041ca44..0x0041ca74 | model_c.md 12 |
| 22 | the exe's inline tileAt tests the row-offset register instead of x for the lower bound | 0x00449736 | model_c.md 12 |
| 23 | the month histories and the finance ledger store only the cash's low word: values above 32767 ($3,276,700) wrap | 0x00417995, 0x0040fc16 | model_b.md 6.2 / 6.3 |
| 24 | the financial report sums columns 0..7 and writes the sum back into +0x10 of each drawn year row | 0x0044f8f0..0x0044f911 | model_b.md 6.2 |
| 25 | hireEmployee's cost lines read and write back the same values | 0x0040ab25..0x0040ab4e | model_b.md 4.1 |
| 26 | tile_mood_up / tile_mood_down increments wrap 255 -> 0 | see model_c.md 12 | model_c.md 12 |
| 27 | holeMagazineEvent's +1 on g_greensFee is overwritten by the next matchUpdate computation before any read but the save | 0x0042e5e2..0x0042e5fb | model_b.md 8.12 |
| 28 | Nine sound slots are configured twice by loadSoundTable; the first path of each pair is never played (slots 8, 16, 17, 18, 19, 21, 22, 23, 24) | loadSoundTable 0x448220 (calls listed in re/model/d_sound_slots.tsv, column overwritten_by_later_call) | model_d.md 5.1 |
| 29 | The PaletteCache's palette is created through Display slot 30 and destroyed through slot 36, the Image's surface created through 31 and destroyed through 35: harmless only because the two jgld destroy bodies are identical (U-0454) | PaletteCache::attach 0x00483030, PaletteCache::reset 0x0048308b, Surface_createFull 0x00474dd0, Image_releaseAll 0x00474cb8 | model_e2.md 8 |

## 3. Dead state (written but never read, or read but never written)

| # | defect | instructions | source |
|---|---|---|---|
| 30 | resolveDataPath tests and copies three path prefixes g_dataDirA / B / C (0x83be04, 0x83bf04, 0x83c104) that no instruction in the image writes: their branches always see the empty string | 0x00491f53, 0x00491e38, 0x00491ecf (the only literals: 0x00491f54 / 0x00491f61, 0x00491e39 / 0x00491e4c, 0x00491ed0) | this file (G1) |
| 31 | globals only ever written 0 or -1 (0x543cf4, 0x4c2e20, 0x54131c, 0x5689e8) and read-only globals never written (0x59b04c, 0x56a518, 0x4c2fc8); g_debugShotGolfer is never written, so chooseShotTarget's formatting branch never runs; g_panelSelectPending only receives -1 and g_panelListIds is never written (U-0187, U-0188) | chooseShotTarget 0x00423cb3.. | model_b.md 2.5 / 8.12 |
| 32 | g_probeShotX / Y slots overlap between probe passes (slot = pass + stroke) | 0x004136aa..0x004136fc | model_b.md 8.12 |
| 34 | g_viewTreeRoot 0x83aa9c is read as a Window* by six instructions (layoutViewTree, View_toLocal, uiEventForwardRootSlot3, uiPointerRegionCode x2, uiMousePress1) and written by none; its address is never taken, so every reader takes its NULL branch (a live read stayed 0 through menu, sandbox, a hole build and a season at x32) | 0x0047a6f9, 0x0047ab82, 0x00495750, 0x00495790, 0x004957d7, 0x00495abe | model_corrections.md 4 (G3) |
| 35 | g_panelListCount 0x56199c has no writer either (U-0169), so panelListSelect mode 2 (golfer list) always takes its out-of-range arm and only clears 0x53df54 | 0x0040d90c..0x0040d913, 0x0040d972 | model_corrections.md 6 (G3) |

## 3b. State the save routine changes (port requirement)

| # | defect | instructions | source |
|---|---|---|---|
| 33 | g_year 0x5a6d3c is not in the save; only appendDate writes it (0x0040d7da), and the save header calls appendDate (0x0040b649), so SAVING refreshes it from g_date. Finance code indexes g_financeYears by g_year % 100 (`movsx eax, word ptr [0x5a6d3c]` at 0x00417922, then `cdq; idiv ecx` with ecx = 0x64): measured by stream G2, a save at the championship's tournament_start checkpoint changed two int16 cells (+0x0c of rows 0 and 1) of g_financeYears by the next checkpoint, a run without the save stayed identical to the pinned reference. **Port requirement:** after loading a save, recompute g_year from g_date the way appendDate does before any finance code runs (and treat saving as a state-changing act, or recompute g_year where the original would) | 0x0040d7da, 0x0040b649, 0x00417922 | re/analysis/data/sve_field_status.md 4 (G2) |

## 4. Checker errors that are not defects

model_check over the merged map reports 13 errors (log/p1/g1/viol_all.tsv): the 3 above (entries 1 and 14) and 10 that
are correct code: 8 `width_cross` where the compiler copies a constant string by dwords / words (showMembershipRoster
0x00454f24..0x00454f4c over 0x4d32c4..0x4d32d4, VoiceRx::ctor 0x0048530c / 0x00485312 over 0x4e43c8 / 0x4e43cc,
drawMessageBuffer 0x0045b0d3 over the one-character string 0x4c3f70; the type `char` is right), and 2 `loop_end` where the
compared pointer is not the accessed one (rebuildHeightfield 0x0042f87e: pointer from byte 3 of record 0, last write
0x0052058f inside corner_cache; loadWorldAssets 0x0043ebb0: four Surfaces per step, last access element 39 of 40).
