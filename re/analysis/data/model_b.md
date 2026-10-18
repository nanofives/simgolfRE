# Data model, stream B: simulation state (phase 1, 2026-10-17/18)

Stream B of `re/PHASE1_PLAN.md`: the golfer, walker / employee, golfer-type, statistics, story, shot-track, phrase,
course, finance, history, ranking, date, RNG, flag-word, bulletin and shot / path scratch state of golf_clean.exe.
Rules: `re/model/STREAM_BRIEF.md`. NO-GUESSING: every claim below or in the group sections cites instruction addresses
(or a value in a named save file); `not determined` plus a U row where the code stops.

## 1. Files

| file | content |
|---|---|
| `re/model/include/sections/b.h` | C99 header: 21 structs with `SGB_STATIC_ASSERT` sizes, `SGB_` constants and enums, no original addresses |
| `re/model/b_addresses.tsv` | 164 rows (symbol, va, size, element type, count, stride, sign, .sve field, clusters, evidence, confidence, other_stream) |
| `re/model/b_types.tsv` | the same ranges as `re/tools/model_check.py` input (structs read from b.h through `cstruct:`) |
| `re/names/model_b_globals.tsv` | 122 new global names (addresses no other names file had) |
| `re/model/b_uncertain.tsv` | 64 open questions, U-0110..U-0191 |
| `log/p1/b/` (not committed) | per-group scratch: `<g>/notes.md` drafts, scans, `ctx.py` (references of a range with context), `hookcheck.py` (hook cross-check), `merge.py`, `assemble_h.py`, `coverage.py`, `sve_table.py` |

Method: the lead split stream B's clusters into six groups (ga golfer records; gb1 people tables; gb2 stories, shot
tracks and the big save blocks; gb3 economy and course records; gb4 date, RNG, flags and buffers; gb5 simulation odds and
ends and initialised data) worked in parallel under one brief (`log/p1/b/SUBBRIEF.md`), then merged, re-checked and
cross-checked them against the other streams' finished maps. Sections 3..8 are the group write-ups (ga 3, gb1 4, gb2 5, gb3 6, gb4 7, gb5 8); a group's `section k` is `N.k` here, and the file names a group section mentions (types.tsv, check.tsv, hooks.tsv ...) are its draft files under `log/p1/b/<group>/`, superseded by the merged files of section 1.

## 2. Results

- **Typed**: 164 global objects (21 struct types, the rest scalars and scalar arrays) covering 493 of the 549
  stream-A addresses of the 178 clusters stream B owns now; every other address is an element of an object another
  stream typed (section 2.3), an end marker or a biased base.
- **model_check** over `re/model/b_types.tsv`: **0 errors**, 313 warnings (width_narrow 129, stride 111, width_block 28,
  sign 28, base_outside 17). Each group section explains its warnings: byte tests of flag dwords and words, dword copies
  of byte arrays, the checker losing a multiplier chain (`lea`/`shl` sequences for x37, x0x388, x0x2c, x0x134), biased
  indexes (record i - 1, hole - 1, mainLoop's 1-based golfer selection), and zero-extended bytes compared with `jl`.
- **Header**: compiles with MSVC 2022 at /W4 as C11 and C++17, alone and together with c.h, d.h, e1.h, e2.h, f.h
  (no name collision); a deliberately wrong size assert fails, so the asserts are evaluated.
- **Hook cross-check** (`log/p1/b/hookcheck.py` over `shim/src/re/*.cpp`, negctl sources excluded): 1,621 distinct
  typed literal accesses (`G<T>(addr)`, `ref<T>`, `at<T>`, `Rec<T>(i, addr)`, `reinterpret_cast<T*>(addr + i * S)`,
  `(T*)addr`, inline-asm `byte/word/dword ptr [.. addr ..]`) fall in 128 of the typed ranges: **0 disagree** in width,
  alignment or stride; 82 read one byte or word of a wider field, as the original instructions they transcribe do
  (byte tests of g_flags, g_date, g_preferences, golfer flags) or form a `char*` record base. Accesses that use a
  base 8 bytes below a range (0x004c1570, 0x0059ae78) are other globals, not biased indexes of the range.

### 2.1 The 86 save globals (`re/tools/sve.py` FIELDS) in stream B

57 of the 89 save blocks (86 fields + tail A + the two tail B blocks) lie in stream B's clusters or in an object B
typed. 42 of them had no name before this stream (exe_*_globals.tsv or sve.NAMES); all 42 have one now. Fields 21
(0x5a4998) and 58 (0x56d1b8) moved to stream C by the parent's ownership overrides of 2026-10-17/18 and are typed there.

| field | va | size | stream A owner | name before | B symbol |
|---|---|---|---|---|---|
| 4 | 0x005a34e0 | 0x1 | B | course_type | g_courseType |
| 6 | 0x00541cd8 | 0x4 | B | skill_rating_x100 | g_skillRatingX100 |
| 10 | 0x005a9cac | 0x4 | B | - | g_awardHoleHonours |
| 11 | 0x005a9cb0 | 0x4 | B | - | g_awardLots |
| 12 | 0x005787cc | 0x4 | B | - | g_awardTournamentPoints |
| 13 | 0x00561258 | 0x4 | B | - | g_awardStories |
| 14 | 0x00571fd4 | 0x4 | B | g_cash_div100 | g_cash_div100 |
| 24 | 0x004c284c | 0x4 | B | - | g_objectOfferCount |
| 26 | 0x005794b8 | 0x9c00 | B | g_golfers | g_golfers |
| 27 | 0x00585850 | 0x1300 | B | g_walkers | g_walkers |
| 28 | 0x00834170 | 0x4 | B | g_date | g_date |
| 30 | 0x0053a450 | 0x4 | B | - | g_landPurchaseCount |
| 31 | 0x00822c88 | 0x4 | B | - | g_difficulty |
| 32 | 0x005409ac | 0x3e8 | B | - | g_skillHistory |
| 33 | 0x0051b388 | 0x3e8 | B | - | g_cashHistory |
| 34 | 0x0053e63c | 0x3e8 | B | - | g_funHistory |
| 35 | 0x004c2850 | 0x4 | B | - | g_greensFee |
| 36 | 0x0059b730 | 0x4 | B | - | g_challengeLevel |
| 37 | 0x0059ae7c | 0x4 | B | - | g_nextGolferSlot |
| 38 | 0x0059dea0 | 0x80 | B | - | g_golferClassSlotCounts |
| 39 | 0x0059c08c | 0x4 | B | - | g_thoughtSeenLo |
| 40 | 0x00568600 | 0x3e8 | B | - | g_eventHistory |
| 41 | 0x00822c70 | 0x4 | B | - | g_freeUnlockMask |
| 43 | 0x005a5a24 | 0x4 | B | - | g_lastAllocatedGolfer |
| 44 | 0x00584210 | 0x7d0 | B | - | g_financeYears |
| 45 | 0x00543cfc | 0x4 | B | - | g_amenityUnlockMask |
| 47 | 0x00572cac | 0x4 | B | - | g_visitor60Count |
| 48 | 0x0059aaf8 | 0x4 | B | - | g_ratingMilestone |
| 49 | 0x0056ae70 | 0x1700 | B | g_golferStats | g_golferStats |
| 50 | 0x005422f8 | 0xf0 | B | - | g_courseRanking |
| 51 | 0x0059ae80 | 0x1c8 | B | - | g_holePoints |
| 52 | 0x0059b048 | 0x4 | B | - | g_lastStoryAdvanceDate |
| 53 | 0x005736b0 | 0x2400 | B,C | - | g_ballRecords |
| 54 | 0x0059fc60 | 0x3880 | B | g_storyPairs | g_storyPairs |
| 55 | 0x005849e0 | 0xe70 | B | g_golferTypeState | g_golferTypeState |
| 56 | 0x005a59f8 | 0x4 | B | - | g_playerGolfer |
| 57 | 0x005a5a04 | 0x20 | B | - | g_playerSkills |
| 59 | 0x004c2c94 | 0x12 | B | - | g_skillMaskBlock |
| 60 | 0x005689e8 | 0x320 | B | - | g_scenicDedications |
| 62 | 0x00572cb0 | 0xa00 | B | - | g_habitatSlots |
| 63 | 0x004d6088 | 0xa640 | B | golfer_types | g_golferTypes |
| 64 | 0x00543d10 | 0x19a28 | B,E | - | g_golferPhrases |
| 65 | 0x00567b04 | 0x4 | B | - | g_tournamentPurse |
| 68 | 0x0053ea24 | 0x9c4 | B | g_ballStopCount | g_ballStopCount |
| 70 | 0x0059e7b8 | 0x4 | B | flags | g_flags |
| 71 | 0x004c1578 | 0x420 | B,E | challenge_text | g_bulletinEvents |
| 72 | 0x00571fd8 | 0x30e | B,C | g_courseTable | g_courseTable |
| 73 | 0x00520640 | 0x3e8 | B | - | g_memberTypeHistory |
| 75 | 0x00571d38 | 0x4 | B | - | g_thoughtSeenHi |
| 79 | 0x0056a51c | 0x4 | B | - | g_nextArrivalOrder |
| 80 | 0x005a6374 | 0x4 | B | - | g_debtYears |
| 81 | 0x0056a524 | 0x28 | B | records | g_bestRounds |
| 82 | 0x005a47e0 | 0x4 | B | - | g_tournamentPrep |
| 85 | 0x005a5a00 | 0x4 | B | - | g_preferences |
| tailA | 0x004e06c8 | 0x8c0 | F | - | g_golferTypes (+0xa640) |
| tailB0 | 0x0058f338 | 0xb7c0 | B | - | g_golferTypesBackup |
| tailB1 | 0x00520a28 | 0x19a28 | B | - | g_golferPhrasesBackup |

### 2.2 Decisions taken while merging

- **Typed elsewhere, not repeated** (another stream's map already has the object; the group's evidence is kept in its
  section): e1 g_rootWindow (0x519d30 / 0x519d40 are members), d g_sheetCols 0x53f3e8 (gb5 found the same table; gb3's
  0x53fc20 divisor table is its elements 526..533), c corner_level 0x5a4998, e2 g_seedTime 0x51b380, the stream C tile
  word grid 0x53caf0 (0x53ca8a is a biased displacement into it), e2's Surface arrays g_sprites5612e8 / 5628e8 /
  58a538 / 59df20 / 59e7c0, d g_surfaces59ca58, d g_images 0x81ca10, e1 g_spriteRef_53b7e0, c noise_source 0x838aa0,
  c tile_type_attr (0x578732 = record 20 +0x22), c animals 0x56d1b8, c tile_byte (0x56988b = biased row - 1), e2 g_sve5a6364.
- **Kept, overlapping another stream's row (for stream G)**:
  - g_golferTypes 0x4d6088 (84 x 0x230, gb1 section 2) contains F's f_buf_4e06c8 and f_w_4e06e8: those are bytes of
    golfer-type records 76.. (tail A = records 76..79; customizeCharacter writes record 76's name and look bytes there).
  - g_objectOfferCount 0x4c284c and g_greensFee 0x4c2850 (save fields 24, 35) lie inside F's f_w_4c2848 (int32[4]);
    the four dwords are separate globals with different readers (gb5 section 2).
  - g_surfaces587ea8 (Surface[224], count from its ehVectorConstructorIterator call, gb4 section 7) contains D's single
    g_surface587fdc (element 7).
  - e2's g_sprites5791f8 (16 x 0x2c ending at 0x5794b8) is the range mainLoop's biased golfer reads land in
    (0x005793b8, 0x005793bc, 0x005793d9, 0x0057946e, 0x00579470): those reads are golfer record sel - 1 (section 3), not
    sprite accesses.
- **Animal record**: c.h `Animal` and gb5's agree byte for byte on offsets; gb5 adds meanings stream C left open:
  +0x38 heading 0..7 and +0x39 walk steps left (set to 0x10), +0x20 the flic id base from the species table, +0x22 the
  4x4 walk-map index (gb5 section 5). Stream G should merge them into C's struct.
- **HabitatSlot corrected after stream C's field map** (model_c.md section 5; the habitats stay B's): gb3's draft
  called +0x10 unaddressed because drawWorldObjects reaches the slots through `mov ebp, 0x572cbe` (slot + 0x0e) and
  `add ebp, 0x14`; read through that pointer, +0x08 is a state word (negative = waiting, bits 0 / 1), +0x0c a link to
  another slot (-1 none), +0x0e the animation base, +0x10 the frame ((frame + 1) % g_sheetCols[kind + anim],
  0x004303b2..0x004303f1), +0x11 the direction (section 6.5). Only +0x13 has no access. C and B now agree.
- **Renamed in the merge**: gb3 named 0x5a5a24 `g_playerGolfer`, the name gb4 gave 0x5a59f8. 0x5a5a24 holds the index
  allocateGolfer 0x00421bc0 returned last (customizeCharacter 0x0040f25c..0x0040f263, mainLoop's arrival path
  0x0040fe86..0x0040fea3), and mainLoop scans the golfers from it + 0x98 modulo 0x98 (0x0040fd75..0x0040fd95): it is
  `g_lastAllocatedGolfer`. 0x5a59f8 keeps `g_playerGolfer` (the golfer customizeCharacter marks with flag 0x200,
  0x0040f581..0x0040f584).
- **Resolved by the lead (U-0173 withdrawn)**: the day of the month is decoded from g_date by saveGameDialog
  0x00405b91..0x00405bbf: `and eax, 0x800003ff` with the sign fix-up (date % 1024), `* 30` (`lea` x3, `lea` x5,
  `shl 1`), signed `/ 1024` (`cdq; and edx, 0x3ff; add; sar 0xa`), `inc` = day 1..30, passed to __itoa 0x004ad425.
  saveGame 0x0040b5b9 does the same for the save header. So `day = (date % 1024) * 30 / 1024 + 1` holds as
  re/tools/sve.py states; gb4's note that no decode exists is corrected here.
- **0x53df54** (stream E's cluster 563) is a signed selection word: golfer index + 1 when > 0 (`lea edx, [eax + 1]`
  0x0041ef48, 0x0040d946, 0x0041efc9), minus a hole number when < 0 (`and eax, 0x1f; neg eax` 0x0041ed7a..0x0041ed86),
  0 none. Section 3 uses it.

### 2.3 Coverage

The parent moved clusters 776, 777 (animals), 818 (tile-type attribute byte), 1034 (corner levels) and 1640 (noise
grid) to stream C during the phase; stream B owns 178 clusters (549 addresses, 1,068 referencing-function weight).

| status | clusters | addresses | weighted by referencing functions |
|---|---:|---:|---:|
| typed by B | 148 | 470 | 937 |
| typed by B except an end marker or a biased base (166, 750, 875, 1088) | 4 | 24 | 35 |
| typed partly by B, partly an element of another stream's object (680, 879) | 2 | 6 | 31 |
| element of an object another stream typed (section 2.2) | 24 | 49 | 65 |
| untyped | 0 | 0 | 0 |
| total | 178 | 549 | 1,068 |

Rows of b_addresses.tsv whose content belongs to another stream carry `other_stream` (C 9, D 20, E 20, F 3): B typed
them because they sat in B's clusters and no other map had them; the parent lists them for their owners:
- C: g_levelFalloffKernel 0x004c2f68, g_cameraGoalX 0x004c2f9c, g_heightRelief 0x004c2fa0, g_tileWallMask 0x005619a0, unk_0058dd34 0x0058dd34, g_holePoints 0x0059ae80, g_courseType 0x005a34e0, g_objectTypeLevel 0x005a8c38, unk_005a9cd4 0x005a9cd4
- D: g_landingAnimFrame 0x004c2e28, g_soundVariationCycle 0x004c2fa4, g_jpegRowCursor 0x00543cf0, g_cachedCameraTileY 0x00543d00, g_cachedCameraTileX 0x00543d04, g_tileScreenX 0x0055eb40, g_ballTrailX 0x00567b34, g_ballTrailY 0x00567d94, unk_0056a510 0x0056a510, g_floatLabelY 0x0056c794, g_surfaces587ea8 0x00587ea8, g_nameLabelCount 0x0058bcb0, g_ballScreenXs 0x005a47e8, g_ballScreenYs 0x005a4868, g_lastCameraRot 0x005a59fc, unk_005a9cbc 0x005a9cbc, unk_005a9cc0 0x005a9cc0, unk_005a9cc8 0x005a9cc8, unk_005a9cd8 0x005a9cd8, unk_005a9cdc 0x005a9cdc
- E: g_cursorTileX 0x00543d08, g_cursorTileY 0x00543d0c, unk_0055e5b0 0x0055e5b0, g_surfaces567390 0x00567390, g_tutorialPage 0x00567a18, g_golferStripSlot 0x00567ff4, g_surfaces568258 0x00568258, unk_0056a514 0x0056a514, g_surfaces56a948 0x0056a948, g_value587ea0 0x00587ea0, unk_0059b734 0x0059b734, unk_005a6d40 0x005a6d40, unk_005a9ccc 0x005a9ccc, unk_005a9cd0 0x005a9cd0, g_infoPanelHoverFrames 0x00823768, g_infoPanelTab 0x00824130, g_infoPanelNumBuf 0x00824134, g_infoPanelToggle 0x00824144, g_bulletinAnimTick 0x00839338, g_staticGuard839340 0x00839340
- F: g_maskSpriteTable 0x004c2f28, g_unreadText569528 0x00569528, g_preferences 0x005a5a00

Two scalars of cluster 1640 (now C) are story state and stay here: g_storyReplyChapter 0x838a94 and
g_namedStoryIndex 0x838a9c (gb1 section 7).

### 2.4 Corrections to older notes and names rows (collected; details in the group sections)

- `g_storyPairs` 0x59fc60 (exe_6_globals.tsv: story pairs, 10 entries): **16 ball-flight tracks** of 0x388 bytes (golfer,
  partner, point count, 32 x/y/z points, two whole golfer-record snapshots); `addGolferPair` / `updatePairSnapshot` open a
  track and append a point (gb2 section 1).
- `g_storyDefs` 0x53a454: **story file names**, a 50-byte view of listMatchingFiles' 100-byte slots, so file k is story id
  2k (gb1 section 6); defect: g_namedStoryIndex mixes the two strides (U-0117).
- `g_animals` 0x56d1d8 (exe names row, placeAnimal note): the record starts at **0x56d1b8**; 0x56d1d8 is field +0x20
  (gb5 section 5).
- `g_courseTable` / seedCourseTable: **16** seeded slots of 17, not 12; the plan's 0x571ff4 is field +0x1c of slot 0;
  +0x2c / +0x2d hold the cash's low two bytes (gb3 section 1).
- golfer_record.md: +0x16 is `int16_t` (section 3); golfer types reach **76..79** (+0xb6), the type table has **84**
  records (gb1 section 2); +0xb8's high byte is Random::range(0x80) - 1 for types with demographics bit 7 (gb1);
  +0x1e's three frame tables are elements 60 / 70 / 220 of g_sheetCols (gb5 section 7); records 152 / 153 are also
  narrateShot's replay copies of a shot track's snapshots (gb2).
- record_tables.md g_walkers: no ambient walkers; the table holds the player's character (kind -6, record 0) and the
  staff (kinds -2..-5); every field now has a meaning (gb1 section 1).
- exe_1_globals.tsv `gRngState` 0x4ba800 is the constant double 1/32768; the RNG state is 0x822d9c; Random::range takes
  one argument and returns 0..n-1 (gb4 section 2).
- re/analysis/sim/mainLoop_structure.md: customizeCharacter runs under g_flags 0x4000, not 0x400000; 0x1000000 is free
  money; 0x200000 is set when a tournament field is built (gb4 section 3).
- 0x5a4998 is the terrain corner-level grid (stream C), not a level table (gb3 section 5).
- `buildGolferBioText` 0x004315e0 is the new-game state reset (clears about 20 tables) (gb3, gb5).
- re/analysis/save/0040afa0_save_serializer.md open question: `top10.sve` (1,560 bytes) is read whole into
  0x541ce0..0x5422f7 (10 x 0x9c) by loadTopScores 0x0040ad60 (0x0040af31) (gb3 section 8; the object is stream C's
  TopScore).
- Tail B of the save is the **pre-tournament backup** (golfer types and phrases), gated by the tournament bit 0x200000
  (gb2 section 4).

### 2.5 Defects of the original found (catalogued, not fixed)

**golfer record 156**: showBulletinBoard writes a type word and the traits word at 0x0058316e / 0x00583170
(0x0046f0f9, 0x0046f103), which are +0xb6 / +0xb8 of record 156, one past g_golfers' 156 records, and then calls
skillPointDialog 0x0045f0f0 with `push 0x9c` (golfer index 156, 0x0046f0e7); the bytes land in stream C's 50 x 50 grid at
0x005830b8 (c_addresses.tsv tile_build_code, model_c.md section 12); g_golfers stays 156 records (block clear, save size, swap copy
all say 0x9c00 bytes);
shotPower's cache key ignores its third argument (gb1, gb2); spawnWalker's free-slot search has no bound (gb1);
g_namedStoryIndex 100-byte vs 50-byte stride (gb1, U-0117); hiring writes money back unchanged (gb1); the financial
report writes the column sum back into the stored total (gb3); month histories and the finance ledger store only the
cash's low word, wrapping above $3,276,700 (gb3); ball-screen lists of 32 with no bound check (gb2, U-0136); dead
globals that are only ever written 0 or -1 (0x543cf4, 0x4c2e20, 0x54131c, 0x5689e8) and read-only ones never written
(0x59b04c, 0x56a518, 0x4c2fc8) (gb2, gb3, gb5); more in each group's defects list.

## 3. Golfer records (group ga, clusters 826..837)

Typed as `Golfer g_golfers[156]` at 0x005794b8, stride 0x100: the struct of golfer_record.md section 4, snake_case,
one change of type (below). Field evidence: golfer_record.md section 2 (not repeated).

**+0x16 skill_mask is `int16_t`, not `uint16_t`.** The 8 `movsx` sites stream A listed all widen the word to an int
before a bit test or before passing it as an int argument; none compares it as a number:
- narrateShot 0x0040890a saves the whole word in a frame slot, then stores 0xffff (-1, every skill) at 0x00408915
  and later restores the saved value (0x004089ab); 0x00408c07 `movsx edx, word [...]; test eax, edx`.
- customizeCharacter 0x0040f550, mainLoop 0x0041b914 and pairGolfersWithStory 0x0045ed63: `movsx; mov edx, 1;
  shl edx, cl; test edx, eax` with cl < 0xc (loop bound `cmp ecx, 0xc` at 0x004089a0 in the same family).
- updateGolfers 0x0042bf3b / 0x0042bf62 and drawGolferInfoPanel 0x0045c591: `movsx eax, word [...]; push eax` as an
  argument of skillPointDialog 0x0045f0f0 / the text builder after buildGolferName 0x004676e0.
VC6 emits `movsx` when a `short` is promoted to int, so the source type is a signed short used as a bit mask; the
only negative value any writer stores is narrateShot's -1. The model declares `int16_t skill_mask` (bits 0..11).

**mainLoop's one-record-low indexes are a 1-based golfer selection, not a second table.** All five sites read the
selection dword 0x0053df54 (stream E's cluster 563) and index `[sel * 0x100 + (field - 0x100) + 0x5794b8]`:
- 0x00415a44..0x00415a60: `mov eax, [0x53df54]; test eax, eax; jle`; `shl eax, 8`; reads `[eax + 0x5793d9]` (=
  +0x21 hole of record sel - 1), `[eax + 0x5793bc]` (+0x04 y) and `[eax + 0x5793b8]` (+0x00 x) and passes x, y to
  0x00407c60.
- 0x0041d92f..0x0041d975: `lea ecx, [eax - 1]; push ecx` (the 0-based golfer index) with `movsx edx, word [eax +
  0x57946e]` (+0xb6 type) to showCharacterEditor 0x004385d0, then writes `[eax + 0x579470]` (+0xb8 traits) from the
  type record's byte at 0x004d60a9 + type * 0x230, the same source updateMembership uses for +0xb8 (0x00421ed2).
- 0x0041f059..0x0041f065: `cmp byte [eax + 0x5793d9], 0xff` (+0x21 hole == -1).
Writers of 0x0053df54 confirm the encoding: `lea edx, [eax + 1]; mov [0x53df54], edx` after a golfer lookup at
0x0041ef48 / 0x0041ef4e (and 0x0040d946 / 0x0040d949, 0x0041efc9 / 0x0041efce); 0 = no golfer. It also holds minus a
hole number at 0x0041ed82..0x0041ed86 (`and eax, 0x1f; neg eax`), so the dword is a signed selection: > 0 golfer
index + 1, < 0 a hole, 0 none. These are not out-of-bounds accesses: for sel >= 1 they land inside record sel - 1.

**model_check** (log/p1/b/ga/types.tsv, check.tsv): 0 errors, 98 warnings. width_narrow 72 (byte tests of the flag
dword +0x10, of +0xb8 traits, of the +0x88 thought words' high byte, of +0x16, and byte reads of the low byte of
+0xb6 / +0xb0 / +0xa4 / +0xb2 / +0xb4 / +0xae that the code copies into the per-hole byte arrays), width_block 11
(dword copies of skill[12] and file_tail[4]), base_outside 7 (the 1-based selection above), sign 1 (prepareShot
0x00426163 `xor edx, edx; mov dl, byte [skill 8]` then `jl`: a zero-extended byte in a signed int compare, consistent
with uint8_t). With the previous `uint16_t` for +0x16 there were 8 more sign warnings.

**Hook cross-check** (log/p1/b/hookcheck.py, log/p1/b/ga/hooks.tsv): 349 distinct typed literal accesses in the
non-negctl hook sources fall in the golfer table; 0 disagree in width, alignment or stride; 13 + 3 read one byte of a
dword / word field (the original's own byte tests).

Uncertainties: unchanged (U-0009..U-0017 cover the open bytes); no new row.

## 4. People tables next to the golfer records (group gb1)

Files: `gb1.h` (structs), `types.tsv` (model_check input), `check.tsv` (model_check output), `hooks.tsv`
(log/p1/b/hookcheck.py output), `addresses.tsv`, `names.tsv`, `uncertain.tsv` (U-0110..U-0120). Working files:
`fn.py` / `rd.py` (disassembly / image reads), `walkers.tsv`, `types.tsv_raw`, `typestate.tsv_raw`, `stats.tsv_raw`
(record_fields.py output), `decomp_walkers.c`, `hookscan.py` / `hookscan.txt`, `savefield.py`.

Offsets are record-relative. Golfer offsets follow golfer_record.md (base 0x5794b8). No string of the exe is
quoted: strings are cited by address.

### 4.1 g_walkers 0x00585850: 64 x 0x4c, the player's character and the staff (clusters 857)

Base / count unchanged from record_tables.md (clear 0x004421c5..0x004421cf, save 0x0040b116, loop ends 0x0042774c,
0x0041796d, 0x00403ee6). Stream A's class (record array, 64 x 0x4c) is right.

**Who lives in it.** Only spawnWalker 0x00402970 creates records, and it has two callers: hireEmployee
(`push -2 - choice / 3`, 0x0040ab56..0x0040ab66, choice 0..11 from showHireEmployeeDialog, bounds 0x0040aa8e..0x0040aa99)
and generateRandomCourse (`push -6`, 0x004720b5). So the kind byte +0x13 is -6 for the player's own character and
-2..-5 for the four staff kinds; no instruction stores a kind >= 0 (U-0110). The -6 record is record 0 (the first free
slot when generateRandomCourse runs): updateGolfers copies the flag-0x200 golfer's x, y and facing into record 0
(0x00428e7e..0x00428e9b), customizeCharacter and loadGolfPro read record 0's x, y, facing (0x0040f57b..0x0040f596,
0x0046cf9b..0x0046cfa7), mainLoop tests `[0x585863] == 0xfa` (0x0041632f), and appendWalkerName names a -6 record from
golfer type 0's +0x10 name (0x004676a4). An employee is a record with kind < 0 and kind != -6: the wage loop
(0x004178a7..0x004178b3), drawEmployeeDetailPanel's list and showRoutingMapScreen's count (0x00456c21..0x00456c33)
all use exactly that test.

**Staff kinds** (kind = -2 - choice / 3, expert = choice % 3 == 2, 0x0040ac09..0x0040ac18). appendWalkerName's default
name index is (flags bit 3) - (2 * kind + 4) = 0..7 into the two string-pointer tables 0x4c148c / 0x4c1464
(0x00467623..0x00467674); entries 2k / 2k+1 are the normal / expert title of kind -2 - k. Behaviour in updateWalkers
(decompiled 0x00402a40, switch 0x00403273):

| kind | target | effect | evidence |
|---|---|---|---|
| -2 | golfer with hole > 0, state > 6, last thought not 0x22 / 0x3a | golfer wait -= 0x10, golfer faces the walker, state 0xb; at timer 4 thought 0x22, or 0x3a for a non-expert when meters +0xa6 / +0xa8 >= 0x11 or +0xaa >= 0xa1 | 0x00403039..0x0040304e, 0x00403273.., 0x00402aab..0x00402b18 |
| -3 | golfer without flag 0x4000 and strokes <= 1 | sets golfer flag 0x4000; expert on a golfer with flag 0x20000000: mood -11 (0xfff5), wait 0, action 0xf9 | 0x00402ff7..0x00403039, 0x004032f7..0x00403322 |
| -4 | tile within +-16 of its post with tile flag 0x800, else (expert) a placed-object record at 0x5736b0 with +8 != -1 | sets tile flag 0x4000 and a per-tile counter (0x578804), clears 0x4800 when the counter falls below 2; object target: its +8 = -1, action 0xfa | 0x00402d14..0x00402f85, 0x004033c8..0x0040349f |
| -5 | golfer with meter +0xa8 > 4 (> 0 for an expert) | golfer wait -0x20, state 0xb; at timer 4 thought 0x19 with argument 0x14, money + 2 (0x571fd4), income table + 2, meter +0xa8 = 0, pointsPopup | 0x00403053..0x00403073, 0x00402b1e..0x00402b9a |
| -6 | any of the above roles; walks to the clicked tile 0x543d08 / 0x543d0c when 0x575cb8 == 0 | | 0x0040311b.., 0x004037e5 |

Wages: mainLoop's pass over the table (0x00417892..0x0041796d) subtracts the signed byte 0x4c2e2c[same index as the
name table] from money 0x571fd4 and the income table, and adds it to +0x24, when Random::range(4 - [0x822c88]) is at most
a value derived from [0x5685f0] - 1 and flag 0x4000000 of 0x59e7b8 is clear. Firing (handleEmployeeAction 0x00436d56..)
clears +0x12 and subtracts 25. hireEmployee's cost lines read and write back the same values (0x0040ab25..0x0040ab4e:
money and the income-table word are stored unchanged), so hiring itself changes no money.

| off | width | sign | meaning | evidence |
|---|---|---|---|---|
| +0x00 | 4 | s | world x | spawnWalker (tile << 10) + 0x600 from the word at 0x58bcba (0x004029b5..0x004029d1); updateWalkers moves it by dirX[facing] * speed (0x00403707..) |
| +0x04 | 4 | s | world y | 0x004029bc..0x004029e8 |
| +0x08 | 4 | s | screen x, -1 off screen | walker draw (drawFlyingBall 0x004038f0's walker loop) stores -1 (0x00403a97) then worldToScreen's x (0x00403ae4); mainLoop skips -1 and measures from the cursor (0x0041f006..0x0041f023) |
| +0x0c | 4 | s | screen y | 0x00403ae7, 0x0041f014 |
| +0x10 | 1 | s | post tile x, -1 none | spawnWalker -1 (0x00402a24); hireEmployee clubhouse tile + direction or [0x578150] +-3 (0x0040ab79..0x0040ac03); mainLoop's move-employee click stores the clicked tile (0x0041e54d..0x0041e568); updateWalkers searches +-16 tiles around it (0x00402d23..); drawEmployeeDetailPanel draws a circle there (movsx 0x004370fc) |
| +0x11 | 1 | s | post tile y | same sites (0x0040ac03, 0x0041e568, 0x004370f5) |
| +0x12 | 1 | u | flags: 0 = free slot, 0x01 active (spawnWalker 0x004029f5), 0x02 (matchUpdate 0x00427744, U-0110), 0x08 expert (0x0040ac18), 0x10 leaving (tested 0x00403503, no setter, U-0111) | handleEmployeeAction 0 (0x00436d90); updateWalkers 0 (0x00402a6f, 0x0040358d) |
| +0x13 | 1 | s | kind (WALKER_KIND_*) | spawnWalker argument (0x00402a02); compares 0xfa (0x0040311b, 0x00403208, 0x0046763e) |
| +0x14 | 1 | s | action code of the current action: 0xfe / 0xfd / 0xfc / 0xfb / 0xfa / 0xf9, 0x23 | writes 0x0040327a..0x00403561; reads 0x00402aab, 0x00402b2b; mainLoop indexes table 0x421740 with v + 7 (0x00416eac, U-0112) |
| +0x15 | 1 | u | action timer: 7 at start (0x0040325c), -1 when (g_date & 3) == 0 (0x00402a84..0x00402aa3), effects at 4 (0x00402b1e); mainLoop draws while 1..7 (0x00416e5a..0x00416e99) | |
| +0x16 | 1 | s | facing octant 0..7 (`and 7`) | spawnWalker (0x004029f2..0x004029fc), angleFixed octant (0x0040ac55..0x0040ac66), dirX / dirY tables 0x4c2878 / 0x4c2898 (0x0040ac6c, 0x0040ac97) |
| +0x17 | 1 | | no access | |
| +0x18 | 2 | s | steps left in the current move: 0x18 - sub-tile or sub-tile + 8, capped 0x10 for odd facing, -1 per step | 0x004031f2, 0x00403639..0x00403658, 0x0040386a, 0x0040388c |
| +0x1a | 2 | s | wait ticks: skips the record while non-zero (decrement 0x00402bbe..0x00402bcb); Random::range + 1 / + 4 / + 0x40, halved for experts (0x004034b1..0x004034d7) | 14 writes in updateWalkers; mainLoop 0 on a move order (0x0041e56e) |
| +0x1c | 2 | s | animation frame counter: the walker draw increments it, takes it modulo the frame count 0x53f3e8[seq] and stores it back (0x00403bdb..0x00403bfd, 0x00403eda); updateWalkers zeroes it at an action start (0x004032b8) | |
| +0x1e | 2 | s | animation sequence: 0xb idle (spawn 0x00402a0f), 0xc action, 7..10 walking cycle (0x00403893..0x004038ae); the draw picks the frame base from it (0x00403b9a..0x00403bd1) and returns 0xc to 0xb at the end of the cycle (0x00403c0e..0x00403c15) | |
| +0x20 | 2 | s | golfer index of the current action | 0x00403266; read 0x00402abd.. as golfer index |
| +0x22 | 2 | s | hire month, g_date / 1024 (signed division) | hireEmployee 0x0040ac9e..0x0040acc0; drawEmployeeDetailPanel passes v << 10 to appendDate (0x00437410) |
| +0x24 | 2 | s | wages paid so far | mainLoop `add [rec + 0x24], di` (0x00417960); drawEmployeeDetailPanel prints it after string 0x4c8494 (0x00437450) |
| +0x26 | 2 | s | completed actions | updateWalkers `inc` after a started action (0x004034aa); drawEmployeeDetailPanel prints it after a per-kind label (strings 0x4c8480..0x4c83f0, 0x0043759e) |
| +0x28 | 2 | | no access | |
| +0x2a | char[34] | | name, empty = default | handleEmployeeAction strcpy (0x00436d2f..0x00436d47, input limit 0x1f at 0x00436d09); appendWalkerName (0x0046760f) |

### 4.2 g_golferTypes 0x004d6088: 84 x 0x230 (clusters 419, 422)

New name. Stream A's base 0x4d6088 is record 0's start: the 0x230-byte fread / fwrite and rep movsd moves all start at
type * 0x230 + 0x4d6088 (addresses.tsv), and record 0 +0x10 (0x4d6098) is the name appendWalkerName and
drawEmployeeDetailPanel use. The reads at 0x4d60a8.. are fields +0x20.. of the same record. **Count 84, not 76**
(gb2's finding agrees): loadGolfPro backs up 0x2df0 dwords = 84 records to 0x58f338 (0x0046c98c..0x0046c996),
resetGolferState restores them (0x0046d0f3..0x0046d102), and 0x4d6088 + 84 * 0x230 = 0x4e1848 is where loadGolferRoster's
own data starts (0x00465ee9). Saved as field 63 (records 0..75, 0xa640) plus tail A (flag 0x40000000, 0x0040b44c..0x0040b45d:
records 76..79 at 0x4e06c8, 0x8c0 bytes; stream A gave 0x4e06c8 to F, it is part of this table). Initialised data: the
image holds names and bytes +0x20..+0x22 for all 84 records and zeros from +0x23 on (+0x30.. all zero).

Record roles: 0..75 the golfer types (loops end at 76, golfer type +0xb6); 76 customizeCharacter's character (writes
+0x00, +0x10, +0x20, +0x21, +0x23..+0x25, +0x27 at 0x0040f348..0x0040f400, golfer type 0x4c at 0x0040f2e1); 77 / 78 / 79
the story characters pairGolfersWithStory gives golfer kinds 0x40 / 0x80 / 0x60 (types 0x4d / 0x4e / 0x4f at 0x0045eace,
0x0045ec53, 0x0045e91e); 80..82 not determined (U-0113); 83 showCharacterEditor's undo copy (0x8c dwords to 0x4e1618 on
entry 0x00438633..0x00438648, back on cancel 0x00439d78..0x00439d88) = cluster 422.

| off | width | sign | meaning | evidence |
|---|---|---|---|---|
| +0x00 | char[16] | | type title text | drawGolferInfoPanel / buildGolferComment / showGolferPairSelect copy record + 0 as a string (0x0045c6a7, 0x0046c139, 0x00459bc0); customizeCharacter strcpy into record 76 (0x0040f3f8..0x0040f400) |
| +0x10 | char[16] | | person's name | appendWalkerName (0x004676a4), buildGolferBioText starts the bio with it (0x00431849), loadGolferRoster copies record 76's into a pro record (0x004662da..0x004662ff), showMembershipRoster compares names (0x00454f9a) |
| +0x20 | 1 | u | personality bits 0..4 | editor `xor` 1 / 2 / 4 / 8 / 0x10 (0x00439812..0x00439852); drawGolferInfoPanel lists the set bits with labels from the 5-pointer table 0x4c2864 (0x0045c88f..0x0045c93b); pairGolfersWithStory compares bits of both golfers (0x0045e1a5..0x0045e681); updateMembership derives golfer +0x19 from bits 0x11 / 0x0c (0x00421d39..0x00421d64) |
| +0x21 | 1 | u | demographics: bits 0..2 one-hot age group (editor cycles 1 / 2 / 4, 0x0043a163..0x0043a1a0; golferAge input), bits 3..6 one-hot marital status (editor cycles 0x08..0x40, 0x0043a10e..0x0043a14e), bit 7 sex (U-0114: editor `xor 0x80` 0x00439d2b, typeBit7Clear 0x0046c95d..0x0046c968) | updateMembership puts the sign-extended byte into golfer +0xb8 (`movsx cx` 0x00421eff, `add` 0x00421f07): for a type with bit 7 set the golfer's high byte is Random::range(0x80) - 1, not the plain random value golfer_record.md states |
| +0x22 | 1 | s | body model 0..18: composeGolferSprite indexes the 0x44-byte part table 0x4d55e8 / 0x4d5b38 with it (+0x14 when typeBit7Clear is 0) and falls back to type % 4 for >= 0x13 (0x00462092..0x004620de); editor accepts < 0x13 (0x00439bae) and loads +0x27 / +0x28 defaults from the table (0x00439bd3..0x00439bef) | saves: max 18 over records 0..75 (&AutoSave1.sve) |
| +0x23 | 1 | u | look: low nibble 0..9, high nibble 0..7 (U-0115) | editor stores (v << 4) or low nibble (0x00439b41..0x00439b57); composeGolferSprite `and 0xf` (0x004624d6); loadGolferRoster splits it into pro +0x20 (high, `sar 4`) and +0x22 (low) (0x00466303..0x00466323) |
| +0x24 | 1 | s | look 0..9 (editor mod 10, 0x00439bfa..0x00439c26) | composeGolferSprite movsx 0x004624ac |
| +0x25 | 1 | s | look 0..9 (editor mod 10, 0x00439c3f..0x00439c6b) | 0x004624b3 |
| +0x26 | 1 | s | look, composeGolferSprite only (0x004624cb, written back 0x00462580) | |
| +0x27 | 1 | s | look 0..3 (editor mod 4, 0x00439cc9..0x00439cfc) | 0x004624ba |
| +0x28 | 1 | s | look 0..4 (editor mod 5, 0x00439c84..0x00439cb0) | 0x004624d9 |
| +0x29 | 3 | | no access | |
| +0x2c | 4 | flags | custom: 0 = not edited; bit 7 set by every editor change (0x00439b63, 0x00439c32 ...); bits 0..2 = the preview golfer's +0x19 & 7 (0x00439db1..0x00439dbf), copied into golfer +0x19 by updateMembership when the dword is non-zero (0x00421d80..0x00421d94); bit 3 toggled (0x00439b7b), U-0116; composeGolferSprite uses +0x23..+0x28 only when non-zero, else stores table defaults into them (0x004624a2..0x00462586) | |
| +0x30 | char[512] | | bio text: buildGolferBioText builds it in 0x51a068 and strcpy's it to record + 0x30 for records 0..75 (loop end 0x4e06f8 = record 76 + 0x30, 0x00431ad7..0x00431af3; another template from record 57, 0x004319d3) | showCharacterEditor reads it (0x00439423) |

### 4.3 g_golferTypeState 0x005849e0: 84 x 0x2c, 76 used (cluster 856)

Existing name. Clear 0x39c dwords = 0xe70 (0x00431693..0x0043169d) confirms 84 slots. New field meanings:

| off | width | sign | meaning | evidence |
|---|---|---|---|---|
| +0x00 | 1 | u | lowest round stroke total of a golfer of this type: matchUpdate at round end sums holeStrokes (+5 per unplayed hole) into [esp+0x20] (0x0042835a..0x004283b6) and stores it when it is lower than the zero-extended byte or the byte is 0 (0x00428648..0x0042865e) | &AutoSave1.sve type 2: 78 |
| +0x01 | 1 | s | running average of round score minus par: first value stored as is, then (movsx old + v) / 2 (0x00428664..0x00428688); v = sum of (strokes - par) (0x0042836d..0x0042838a) | &AutoSave1.sve type 2: 11 |
| +0x02 | 1 | u | membership level bits 0..2 (U-0003) | unchanged |
| +0x03 | u8[19] | u | per-hole flags; matchUpdate sums `& 3` of holes 1..h at round end (0x00428394..0x004283a4) | unchanged otherwise |
| +0x16 | s8[19] | s | per-hole strokes (unchanged) | |
| +0x29 | 1 | u | 0xff = banned (U-0003) | |
| +0x2a | 2 | s | rounds started: pairGolfersWithStory increments it for each of the two paired golfers whose kind +0x18 is 0 (0x0045ef0b..0x0045ef41); membershipReport / showMembershipRoster count types with a non-zero value (0x00406a66..0x00406a7e, 0x00454c68); matchUpdate uses it in the stage update (0x00428942) | saves: 1 for every visited type |

### 4.4 g_golferStats 0x0056ae70: 32 x 0xb8 (clusters 768, 769)

Confirmed: clear 0x5c0 dwords (0x00442225..0x0044222f), save 0x1700, 24 references in 6 functions, +0x18..+0x1f have no
access (record_fields.py, all functions and gaps), cluster 769 0x56aedc is +0x6c of record 0. Field names from the
exe_2_globals.tsv row; +0x0c has two writers (U-0119).

### 4.5 g_golfPros 0x0058dd50: 100 x 0x38, the golf pro roster (cluster 922)

New name. Golfer +0xbb indexes it. Base 0x58dd50: buildGolferName / drawGolferActionPanel / showHistographScreen take
`[idx * 0x38 + 0x58dd50]` as a string (0x00467806, 0x0043698c, 0x00456642). loadGolferRoster's first pass marks every
record free (+0x20 = 0xff, 0x00465f43..0x00465f53) and then parses one roster text line per record (fgets 0x00465f64,
tokens through strtok 0x4a678b):

| off | width | sign | meaning | evidence |
|---|---|---|---|---|
| +0x00 | char[32] | | name | strcpy of token 1 (0x00465fde..0x00466004) |
| +0x20 | 1 | s | look, (digit) % 8; -1 = free record (tested 0x0040fff1, 0x0046cbf0); = type look_23 >> 4 on the .pro path (0x00466317) | 0x0046602d..0x0046604a |
| +0x21 | 1 | s | look, (digit) % 4; = type +0x27 (0x00466320) | 0x00466074..0x00466091 |
| +0x22 | 1 | s | look, digit % 10; = type +0x23 & 0xf (0x00466323) | 0x004660c2..0x004660d4 |
| +0x23 | 1 | s | look, digit % 10; = type +0x24 | 0x00466105..0x00466117 |
| +0x24 | 1 | s | look, digit % 10; = type +0x25 | 0x00466149..0x0046615b |
| +0x25 | u8[12] | u | skills: ten characters, digit % 10 or letter % 10 + 10 (0..19) (0x00466188..0x004661d7); loadGolfPro copies 12 bytes as three dwords into golfer +0xf0 (0x0046ceb6..0x0046cece); mainLoop takes the maximum over 12 bytes (0x0041017d..0x00410190) | bytes 10..11 are only read with the block |
| +0x31 | 5 | | no access | |
| +0x36 | 2 | s | skill sum of the ten parsed skills (0x004661e1), read by loadGolfPro (0x0046cb9d) | |

composeGolferSprite reads +0x21..+0x24 for a golfer of kind 0x20 into the same locals it fills from the type record's
+0x27, +0x23 low, +0x24, +0x25 (0x004624ec..0x0046253d), which is how the five look bytes line up with the type record.

Defect (already in CLAUDE.md, round 33): the .pro-file path advances ebp by 0x38 before storing skills and their sum, so
+0x25.. and +0x36 go into the next record (0x0046630b, 0x0046633b, 0x00466354).

The 23 bytes before it, 0x58dd34..0x58dd4a, are a separate map (section 7).

### 4.6 g_storyDefs 0x0053a454: 100 x 50, story file names (cluster 525)

Correction to record_tables.md and the exe_2_globals.tsv row: the table holds story FILE NAMES, not definitions.
loadGolferRoster calls listMatchingFiles 0x0043d2a0 (0x00465a8c, fallback pattern 0x00465aa7), which clears 0x9c4 dwords
at 0x80b130 and writes each matching file name into a 0x64-byte slot (`add ebp, 0x64`, 0x0043d3e3); loadGolferRoster then
copies the first 0x4e2 dwords = 5000 bytes (50 slots) into g_storyDefs (0x00465abe..0x00465acd). Readers use a 50-byte
stride (0x0045e001..0x0045e007, 0x00466c7d..0x00466c90), so file k is story id 2k and odd ids hold bytes 50..99 of a
slot (zero for a short name; pairGolfersWithStory rejects ids whose byte 0 is 0, 0x0045e00e). loadStoryStrings opens
directory + separator 0x4c84e4 + the name (0x00466c90..0x00466cbe, fopen 0x4a614d). The 8-letter requirement code
record_tables.md describes is therefore the file name's first 8 characters. At most 50 story files are reachable.

Defect: g_namedStoryIndex (0x838a9c) is found by walking the 0x64-byte slots (`add esi, 0x64`, 0x00465ae8) but compared
with and stored as a 50-byte story id (0x0045e012, 0x0045e6f8). Every save fixture holds story id 3 (an odd id) in
golfer +0xb0 (all eight saves), which only that store can produce; what loadStoryStrings opens for id 3 is U-0117.

### 4.7 Small globals (clusters 718..722, 743, 1610, 1640, 1644, 1645, 526)

- **0x567b34 / 0x567d94 / 0x567ff4** (718 / 719 / 720): three int32[152] arrays back to back (0x260 apart).
  0x567b34 and 0x567d94 are the ball trail's last screen point per golfer (x, and y minus ballZ scaled by [0x4c2e00] *
  [0x4c2844]), -1 = none (0x00415652..0x00415827): rendering state, other_stream=D. 0x567ff4 maps the slots of a
  horizontal label strip (x from 0xcc, 16 px each, labels drawn until x > 0x310) to golfer indexes for click hit tests
  (0x00418d24..0x00418ed2, 0x0041ef07..0x0041ef3d): UI state, other_stream=E. Stream A's counts (152, 1, 152) become
  152 for all three: the clears are 0x98 dwords and the arrays abut.
- **0x5685c8** (722): int32[10], second key of shotPower's 10-entry result cache; neighbours 0x5a47b8 (first key),
  0x53fd20 (results), 0x5a9ce0 (cursor) belong to other clusters. Defect: the cache key leaves out shotPower's third
  argument, which selects 0x4223f0 or 0x4223c0 (0x004224c5..0x004224e3), so a hit can return the other mode's result.
- **g_shotStartDate 0x56962c** (743): int[152] confirmed (existing row). check.tsv's two warnings at 0x0041161e are
  `[edi + 0x56988b]`, record 151 + 3 = byte -1 of the next table 0x56988c (an index - 1 read of that grid), not this array.
- **0x823768** (1610): drawGolferInfoPanel's hover dwell counter, UI, other_stream=E.
- **Cluster 1640** is three objects: 0x838a94 g_storyReplyChapter, 0x838a9c g_namedStoryIndex (both story state, B),
  and 0x838aa0 g_noiseSeedGrid int8[19][19] filled by genNoise (terrain noise, other_stream=C; 0x838c1c g_sampleGrid
  follows). Stream A's stride 0x13 / count 2 come from the grid.
- **0x838d9c** (1644): char[12] _itoa buffer of buildGolferComment.
- **0x838da8** (1645): int32[65] last argument per thought kind (U-0118 on the count).
- **0x53b7e0** (526) and **0x568258** (721): SgSurfaceObj[14] and SgSurfaceObj[20] built by ehVectorConstructorIterator
  with ctor 0x473ab0 / dtor 0x4041f0 (stream D's g_surfaces* rows use the same type): C++ objects, other_stream=E (D
  holds the other Surface arrays). Stream A's `record array 0x2c x 1` was the element size only.
- **0x58dd34** (part of 922): uint8[23] map tile type -> slot 0..15 (-1 none) built from the 16-byte table 0x4c2d28
  (0x0040f939..0x0040f974), tested by showRoutingMapScreen against the tile-type grid 0x5722e8 (0x00456d18..0x00456d25):
  course data, other_stream=C (U-0120). pathStep's `cmp ecx, 0x58dd34` (0x0042ec11) is the end of a 50 x 50 grid that ends
  here, not an access to the map.

### 4.8 model_check (check.tsv): 0 errors, 70 warnings

Two errors in the first run were fixed in the types: GolfPro's three-dword skill copy read past a 10-byte array (now
skill[12], 0x0046ceb6..0x0046cece) and mainLoop's dword fill of the 23-byte map (now a struct with one uchar[23]).
Warnings, all explained:
- stride (31): multipliers the tracker loses: walkers `idx*0x13 << 2` (hireEmployee 0x0040ab7f.., mainLoop 0x0041e553..),
  pros `idx*7*8` (0x0043698c ...), types `(8i - i)*5 << 4` read as 0x50 (loadThemeFile 0x004380c4..0x004380d4, mainLoop
  0x00414ab8), typeState `idx*11*4` (0x004068ff, 0x004317ff).
- sign (23): golferTypes +0x20 / +0x21 / +0x23 read with movsx (personality bits < 0x20 are unaffected; the +0x21 movsx
  is the sign extension into golfer +0xb8 noted in section 2); pros 0x00410184 is a zero-extended max loop with a signed
  compare; shotStartDate see section 7.
- width_narrow (7): byte tests of the custom dword +0x2c (bit 3, bit 7) and the shotStartDate case.
- width_block (9): the pros' 12-byte skill copy and the map fill.

### 4.9 Hook cross-check (hooks.tsv)

log/p1/b/hookcheck.py over types.tsv: 377 literal-address uses in shim/src/re/*.cpp (226 outside negctl), every one `ok`,
0 DISAGREE. Notes: c3et.cpp:1204 reads 0x58dd34 (the map) as a byte (reported against g_golfPros' lower margin);
c3eo.cpp:590 0x569628 and c3ag.cpp:244 0x53a450 are the globals before those tables. hookscan.txt (my own wider regex)
lists 616 lines including c3fy.cpp's macros; spot checks of c3fy / c3fb / c3ec agree with the field widths
(walker +0x00 int, +0x16 byte; type +0x22 signed char, +0x2c unsigned).

### 4.10 Corrections to older notes and rows

- record_tables.md, g_walkers: every field now has a meaning (section 1); `which value marks an employee`: kind < 0 and
  != -6; the table holds no ambient walkers, only the player's character and staff.
- record_tables.md / exe_2_globals.tsv g_storyDefs: story file names (section 6).
- record_tables.md g_golferTypeState +0x00 / +0x01 / +0x2a: section 3.
- golfer_record.md +0xb6: golfer types reach 0x4c..0x4f (76..79), not only 0..75 (0x0040f2e1, 0x0045e91e, 0x0045eace,
  0x0045ec53); +0xb8: high byte is Random::range(0x80) - 1 for types with demographics bit 7 (section 2); +0xbb: index
  into g_golfPros, whose +0x20 is the byte showCharacterEditor reads (0x00438a3d).
- exe_0_globals.tsv g_walkers row: `ambient walker` wording (already corrected once) can drop `walkers`: staff + player.
- Stream A classes: 1640 (one record array) is three objects; 526 / 721 are C++ object arrays; 718 / 719 / 720 counts
  are all 152; 422 is record 83 of 419; 922 is a 23-byte map plus a 100-record table.

### 4.11 Coverage

| cluster | typed as | stream |
|---|---|---|
| 857 | g_walkers (Walker[64]) | B |
| 856 | g_golferTypeState (GolferTypeState[84]) | B |
| 768, 769 | g_golferStats (GolferStats[32]) | B |
| 525 | g_storyDefs (StorySlot[100]) | B |
| 526 | g_surfaces53b7e0 (SgSurfaceObj[14]) | other_stream=E |
| 419 | g_golferTypes (GolferType[84]) | B |
| 422 | g_golferTypes record 83 | B |
| 922 | g_golfPros (GolfPro[100]) + unk_0058dd34 (uint8[23]) | B + other_stream=C |
| 718 | g_ballTrailX (int32[152]) | other_stream=D |
| 719 | g_ballTrailY (int32[152]) | other_stream=D |
| 720 | g_golferStripSlot (int32[152]) | other_stream=E |
| 721 | g_surfaces568258 (SgSurfaceObj[20]) | other_stream=E |
| 722 | g_shotPowerCacheArg2 (int32[10]) | B |
| 743 | g_shotStartDate (int32[152]) | B |
| 1640 | g_storyReplyChapter, g_namedStoryIndex (int32), g_noiseSeedGrid (int8[361]) | B + other_stream=C |
| 1644 | g_numberTextBuf (char[12]) | B |
| 1645 | g_thoughtArgByKind (int32[65]) | B |
| 1610 | g_infoPanelHoverFrames (int32) | other_stream=E |

19 of 19 clusters typed; none left untyped. Fields left `not determined`: walker kinds >= 0 and flags 0x02 / 0x10
(U-0110, U-0111), action-code drawing (U-0112), type records 80..82 (U-0113), the look bytes (U-0115), custom bit 3
(U-0116), story id 3 (U-0117).

## 5. Shot tracks, phrase tables, celebrity roster, tournament backups, visitor counters (group gb2)

Stream B helper gb2, 2026-10-17. Static evidence only (disassembly from `log/p1/b/dis.pkl`, data pointers searched with
`log/p1/b/gb2/dwsearch.py`, save values with `log/p1/b/gb2/savevals.py` over the 5 distinct saves: `&AutoSave1`, `&AutoSave2`,
`mood5`, `stories5`, `stories9`; the fixture copies are byte-identical in these fields). Header `gb2.h`, map `addresses.tsv`,
checker input `types.tsv`, output `check.tsv`, new names `names.tsv`, open points `uncertain.tsv` (U-0125..U-0136).

### 5.1 g_storyPairs 0x0059fc60: 16 ball-flight tracks of 0x388 bytes (clusters 996-1000)

**Count 16, not 10.** Every whole-table loop steps 0x388 and stops at `cmp r, 0x5a34e0`: addGolferPair 0x00409a04,
trialShot 0x004226d2, updateGolfers 0x00428aa9, buildGolferBioText 0x00431b3d (clear), mainLoop 0x0041d908 (clear);
(0x5a34e0 - 0x59fc60) / 0x388 = 16; the save block is 0x3880 (serializeCourse 0x0040b284). In all 5 saves the 16 golfer words
hold golfer indices or -1, e.g. stories5 entries 0..11 = golfer 19 / partner 18. 0x5a34e0 is the next global (course type,
save field 4). The shim agrees: c3ea.cpp and c3ec.cpp loop `k < 16`, c3e.cpp uses the 0x388 stride and the word / dword widths.

**It is not a story-pair table.** An entry is opened when a golfer's shot starts and the golfer is on screen
(updateGolfers 0x0042c507..0x0042c517: screen x +0x08 != -1, then addGolferPair), filled with ball positions during the flight
(appendPairTrackPoint calls from trialShot 0x00422831 / 0x00422854 / 0x00422d37 and updateGolfers 0x0042c749 / 0x0042c76b /
0x0042cf2f, the 0x0042c76b one only when `g_date & 7 == 0`), replayed as a dotted track by narrateShot 0x00407e00 (worldToScreen
of every point, height subtracted as z * zoom / 64, 0x00407f69..0x004080c9) and by mainLoop's shot preview (golfer 0x99,
0x0041d3a3..0x0041d60c), and released (golfer word = -1) by updateGolfers when the golfer's flag +0x10 bit 0x40000 is clear
(0x00428a95..0x00428ad6). Several entries can hold the same golfer (stories5: 12 entries for golfer 19).
The partner word and the partner snapshot are copies taken at registration; the only consumer of the pair is narrateShot, which
copies both snapshots into golfer records 152 / 153 (0x00407f19..0x00407f43) so the replay draws them.

| off | width | sign | meaning | evidence |
|---|---|---|---|---|
| +0x000 | 2 | s (`movsx` 0x0040996e) | golfer index; -1 = free (`cmp word [ecx], -1` 0x004099f7); mainLoop stores 0x98 after its preview (0x0041d60c) | w 0x00409a26, 0x00428ad6; r 0x00407e28, 0x00409653 |
| +0x002 | 2 | s | partner index copied from Golfer +0xa2 (0x00409a33..0x00409a3a) | w only; read through the snapshot |
| +0x004 | 2 | s (`movsx` 0x00409967) | points stored: 0 at registration (0x00409a41); appendPairTrackPoint writes slot `count` and increments only while count < 0x1f (`cmp esi, 0x1f; jge` 0x004099d0..0x004099dc), so slot 31 is rewritten and never read (readers loop i < count, 0x00407fd2, 0x0041d600; mainLoop reads count - 1, 0x0041d426..0x0041d433) | w 0x004099dc; r 0x00407f50 |
| +0x006 | 2 | | no access | |
| +0x008 | 4 x 32 | s | point x (world): point 0 = Golfer +0xc4 shot start, later points Golfer +0xd4 ball x (0x00409975..0x004099a5) | r 0x00407f81 |
| +0x088 | 4 x 32 | s | point y: Golfer +0xc8 / +0xd8 (0x004099ab..0x004099bd) | r 0x00407f7a |
| +0x108 | 4 x 32 | s | point z: 0 for point 0, else Golfer +0xdc ball height (0x004099c3..0x004099d3) | r 0x00408032 (`imul` by zoom) |
| +0x188 | 0x100 | | whole Golfer record of +0x000 at registration (`rep movsd` 0x40 dwords 0x00409a4f..0x00409a63) | r 0x00407f19..0x00407f31 |
| +0x288 | 0x100 | | whole Golfer record of the partner (0x00409a65..0x00409a80) | r 0x00407f33..0x00407f43 |

Index arithmetic: `lea eax,[ecx*8]; sub eax,ecx; shl eax,4; add eax,ecx; shl eax,3` = idx * 0x388 (0x00409956..0x00409964);
point slot = (idx * 0xe2 + count) * 4 (0x0040998e..0x004099a0).

**Correction** for `re/names/exe_6_globals.tsv` g_storyPairs: `10 entries ... event counter @+4` -> 16 ShotTrack entries,
+4 is the track point count. The name was kept (the brief forbids renaming); a descriptive name would be g_shotTracks.
exe_6.tsv's `updatePairSnapshot` / `addGolferPair` names describe the same misreading (they append a track point / open a
track); function names are left to the names owner.

### 5.2 The 0x543ca0 block (clusters 650-653)

These are separate globals, not one record:

| address | type | meaning | evidence |
|---|---|---|---|
| 0x543ca0 | int32[20] | g_activeObjectLevel: per placed-object kind, max(obj +8 + 1) clamped to 99 over objects of that kind whose byte +7 has bit 0x40; 0 = none | cleared 0x14 dwords 0x0040fc44..0x0040fc4e and 0x004315f3..0x004315fd; loop over g_placed_objects 0x0040fc6e..0x0040fce7, kinds < 6 skipped (0x0040fc76); rebuildTileCoverage sets bit 0x40 when the footprint covers a g_tileFlags-0x40 tile (0x0042f464..0x0042f46e) or unconditionally for kind >= 0x11 and in one flag state (0x0042f3fd, 0x0042f47b..0x0042f490); 0x005a8c38 is the same maximum over all objects (not gb2's); fixed readers kind 6 (0x00543cb8: prepareShot 0x00424bf7, updateGolfers 0x0042a578), 8 (0x00543cc0), 9 (0x00543cc4: matchUpdate 0x004286bc), 10 (0x00543cc8: rateTile 0x00422630), 11 (0x00543ccc), 12 (0x00543cd0: rateLot), 13 (0x00543cd4), 14 (0x00543cd8) |
| 0x543cf0 | uint16_t* | g_jpegRowCursor (stream D) | saveCourseJpg 0x00431e70..0x00431e88, FUN_00431be0 0x00431cfb..0x00431d12 |
| 0x543cf4 | int32 | always 0: the only store is 0 (0x0040fb7c / 0x0040fb89); the `== 2` arms of matchUpdate (0x00427471, doubles [0x004c2850]) and emitGolferThought (0x00468563) never run | dwsearch: 3 code references, no data pointer |
| 0x543cf8 | uint32 | ORed into [0x0056c7b4] then overwritten with edi when g_date == [0x0056a518] + 0x10 (0x0041c150..0x0041c175); only writer | U-0125 |
| 0x543cfc | uint32 | g_amenityUnlockMask (save field 45): 0x3fff at a new game (0x0040fc0c, 0x0042142f), bits set by patronEvent, courseActionMenu (after `sub` of the price from cash 0x00470849..0x00470857), announceLandmark | U-0130; all 5 saves hold 0x3fff |
| 0x543d00 / 0x543d04 | int32 | g_cachedCameraTileY / X: the camera of the frame caches (stream D) | 0x0041054e..0x004105be |
| 0x543d08 / 0x543d0c | int32 | g_cursorTileX / Y: the tile under the mouse (stream E input; also read by updateWalkers and mainLoop's tools) | 0x004199e5..0x00419a1a, 0x00403137..0x00403148 |
| 0x543d10 | GolferTypePhrases[84] | g_golferPhrases (save field 64) | section 3 |

Stream A's cluster 650 (`scalar array`, count 20, stride 4) is right for 0x543ca0..0x543cef only; 0x543cf0..0x543cf8 are three
unrelated scalars. Cluster 652 (`scalar`, 4) is two pairs (camera cache, cursor tile). Byte reads of the cursor words
(0x0041e54d, 0x0041e559) are low-byte reads (model_check width_narrow warnings).

### 5.3 g_golferPhrases 0x543d10 and g_golferPhrasesBackup 0x520a28 (clusters 653, 523): 84 x 25 x 50 bytes

Record = golfer type * 0x4e2; slot = i * 50 for i the position of a thought kind in the byte table 0x004c2d10 (20 kinds, then
0xff): buildGolferComment walks the table (0x00469be6..0x00469c25), tests the slot's first byte (0x00469c0c) and appends the
text (0x00469c27..0x00469c6e); showCharacterEditor edits slots (0x00439485..0x00439491, 0x0043a035..0x0043a0a9, label pointers
0x004c2cc0). 0x4e2 = 25 * 50, but only 20 slots are reachable (U-0133); in every save records 76..79 alone have text, slots 0..19.
Count 84 from the whole-table instructions (`mov ecx, 0x668a` clear 0x00442335, copies 0x0046c998 / 0x0046d104):
0x668a * 4 = 0x19a28 = 84 * 0x4e2. 84 is also the golfer-type table's record count (0x2df0 dwords = 84 * 0x230, section 4).
Record 76 is the player's custom character (customizeCharacter clears 0x55b028 = record 76, 0x0040f33e..0x0040f35d, and sets
byte 0x004e06e9 = type record 76 + 0x21). A record travels with its 0x230 type record through the .chr file (fwrite pair
0x00437c62..0x00437c93, fread pair 0x004380d1..0x004380fc).

| off | width | sign | meaning | evidence |
|---|---|---|---|---|
| +0x000 + 50 i | 50 | | phrase text for thought kind table[i], i < 20, NUL-terminated, empty = none | r 0x00469c0c, 0x00469c46; w 0x0043a09d..0x0043a0a9 |
| +0x3e8..+0x4e1 | 250 | | slots 20..24: no access except whole-record clears / copies | U-0133 |

**Why 0x520a28 is a copy.** loadGolfPro 0x0046c970 (callers startNewCourse 0x0046de1e and mainLoop 0x0041ceec) copies the
golfer-type table 0x4d6088 -> 0x58f338 (0x0046c985..0x0046c996) and 0x543d10 -> 0x520a28 (0x0046c998..0x0046c9ac), then loads
the tournament's pros and sets [0x0059e7b8] bit 0x200000 (0x0046d009 / 0x0046d024). showTournamentResults clears the bit
(0x0045adf7) and calls resetGolferState (0x0045ae01), which copies both back (0x0046d0f3..0x0046d115). So bit 0x200000 is
`tournament in progress`, and tail B saves the two pre-tournament copies so a save made mid-tournament can still restore the
course's own types and phrases. narrateShot (0x00408929) and mainLoop (0x0041e146) also test the bit. The adjacency confirms the
sizes: 0x520a28 + 0x19a28 = 0x53a450 (g_landPurchaseCount), then g_storyDefs 0x53a454 (the base_outside warnings in check.tsv
are g_storyDefs accesses inside the margin).

### 5.4 g_golferTypesBackup 0x58f338 (cluster 924) and g_playerShotStyle 0x58f330 (cluster 923)

0x58f338 is 0xb7c0 = 84 golfer-type records of 0x230 (the struct belongs to the owner of cluster 419, 0x4d6088). Finding for
that owner: the golfer-type table at 0x4d6088 has 84 records, not 76: the save field 63 covers 0..75 (0xa640), tail A
(0x004e06c8, 0x8c0, bit 0x40000000) covers 76..79, and record 83 (0x004e1618) is showCharacterEditor's undo copy of the type it
edits (0x00438605..0x00438648, restored 0x00439d74..0x00439d88); 80..82 have no fixed reference (U-0135). The table ends at
0x004e1848, where the next string starts.
0x58f330 is a scalar, not part of the backup: the 0x38-byte record table at 0x0058dd50 ends exactly there
(0x0058dd50 + 100 * 0x38; its loop end `cmp eax, 0x58f350` at 0x00465f4e walks field +0x20), and 0x58f330..0x58f337 hold
g_playerShotStyle plus 4 bytes with no access. Values in addresses.tsv; meanings U-0128.

### 5.5 g_celebrities 0x55d738 (clusters 655, 656): 100 records of 0x25

| off | width | sign | meaning | evidence |
|---|---|---|---|---|
| +0x00 | 32 | | name, token 1 of a roster line, strcpy with no bound | w 0x00465ca1..0x00465cc7; r 0x00417b5d, 0x0044ddd9, 0x00456671, 0x00469549, 0x0046aa4b |
| +0x20 | 1 | s (`movsx` 0x00417adb, 0x0040120a) | category = (token 2's first char - 'A') % 11 (`idiv` 0x00465cf7..0x00465d09); 0xff = empty (cleared 0x00465c0f, tested 0x00417a89); indexes the string-pointer table 0x004c2ba8 (0x00417b01) | spawnObject 0x0040120a (c3bs.cpp agrees: signed byte at k*0x25) |
| +0x21 | 1 | s | token 3: 1..3 if the char is '1'..'3', else 0 (0x00465d36..0x00465d4a) | r composeGolferSprite 0x00462152 |
| +0x22 | 1 | s | token 4 digit % 10 (0x00465d83..0x00465d95) | r 0x00462159 (+5) |
| +0x23 | 1 | s | token 5 digit % 10 (0x00465dc6..0x00465dd8) | r 0x00462167 |
| +0x24 | 1 | s | token 6 digit % 10 (0x00465e08..0x00465e20) | r 0x0046216e |

Lines whose first byte is 0x2a or shorter than 10 characters are skipped (0x00465c3c..0x00465c58). mainLoop picks a random valid record
(Random::range(100) until +0x20 != 0xff, 0x00417a70..0x00417a91) and builds a text from it. 0x55d713 (cluster 655) is not its
own object: mainLoop indexes `[k + 9k*4 + 0x55d713]` with a 1-based k taken from a placed object's +8 (0x00415cb6..0x00415cc8),
i.e. record k - 1 (model_check reports it as a stride warning on g_golferPhrases' last record because of the margin).
The fill loop's bound `cmp ebp, 0x55e5cd` (0x00465e1a) runs after `add ebp, 0x25`, so at most 100 records are written.
The file is the theme's celebrity file (path built from 0x004e18ac), falling back to the standard one (string 0x004e188c).

Cluster 657: 0x55e5ac is g_lastTipDate (B); 0x55e5b0 is a Surface[20] array of 0x2c-byte objects built by the eh vector
constructor (0x00404c94..0x00404ca7) and destroyed at 0x00404d66..0x00404d74: stream E (`other_stream=E`). 0x55e5cc / 0x55e5cd
(stream A end markers) are the celebrity loops' ends.

### 5.6 Scalars

| address | symbol | sve | meaning (evidence in addresses.tsv) |
|---|---|---|---|
| 0x53a450 | g_landPurchaseCount | 30 | count of land tracts bought; the kind-0x40 visitor's number (Golfer +0x18 = 0x40 + v) and roman-numeral suffix (buildGolferName v & 0x7f <= 8); bit 7 cleared by patronEvent (U-0134). All saves 0. |
| 0x572cac | g_visitor60Count | 47 | count 0..8 of kind-0x60 visitors (type 0x4f, Golfer +0xb6 at 0x0045e91e); bit 7 = visit pending, set with the increment (0x0045e9fe), cleared by patronEvent / loadGolfPro. The other `0x572cac` immediates (markTileKind11 0x0042f329, rebuildHeightfield 0x0042f8f1, showRoutingMapScreen 0x00456d82, generateRandomCourse 0x004715a5) are row-end compares of g_tile_type 0x5722e8 + 0x9c4, not references to this global; generateRandomCourse's is `jle`, the known one-row overrun. |
| 0x5689e8 | g_scenicDedications | 60 | 100 x {tile x, tile y, golfer type, pad}; no writer except -1 (U-0126); every save all 0xff. Tile x / y: mainLoop's per-tile pass pushes ebx = [esp+0x24] (outer index, x of x*50 + y at 0x0041236c..0x0041237b) and ecx = [esp+0x34] (0x004129f3..0x00412a02). c3ae.cpp reads the same 8-byte layout. |
| 0x56a51c | g_nextArrivalOrder | 79 | next Golfer +0xbe stamp; saves: v == max(+0xbe) + 1 in all 5 saves (e.g. stories5 21 -> 22). |
| 0x56a520 | unk_0056a520 | - | write-only (0x0041951d) (U-0136). |
| 0x5409a8 | g_thoughtOnPartner | - | buildGolferComment's `partner speaks` flag: 0 on entry, 1 on eight arms; emitGolferThought then puts the bubble on Golfer +0xa2's record and sets bit 7 of its +0x85 (the golfer_record.md `+0x85 bit 7 from [0x005409a8]`). |
| 0x5a47b8 | g_shotPowerCacheDist | - | int32[10], key 1 of shotPower's ring cache (key 2 0x005685c8, value 0x0053fd20, cursor 0x005a9ce0, not gb2's). Finding: the cache key omits shotPower's third argument (0x004224c5..0x004224e3 picks decaySum2 or decaySum by it), so a later call with the same (distance, lie) and the other mode returns the first mode's value; the zeroed .bss also answers (0, 0) with 0 before any store. c3a.cpp reproduces the cache. |
| 0x5a47e0 | g_tournamentPrep | 82 | -1 pending, -2 ask, then the prep dialog's result bits (1, 2 read by prepareShot), 0 after the results (U-0129). All saves 0. Type int32 with bit tests (prepareShot's byte `test` is a width_narrow warning). |
| 0x5a47e8 / 0x5a4868 | g_ballScreenXs / Ys | - | per-frame screen positions of drawn balls (stream D), count [0x0058bcb0] reset at 0x0041495a; U-0131. |
| 0x58f330 | g_playerShotStyle | - | section 4. |
| 0x55e5ac | g_lastTipDate | - | section 5. |

### 5.7 model_check

`types.tsv` (27 ranges): **0 errors, 24 warnings** (`check.tsv`).
- stride (13): the 0x388 (ShotTrack) and 0x25 (CelebrityRecord) multipliers are built with `lea` / `sub` / `shl` chains that the
  scanner does not track (`lea r,[k+k*8]; [k+r*4]` = k*37; 0x00409a0e..0x00409a21 = k*0x388); not type errors. One of them is the
  1-based celebrity access 0x00415cc8 (section 5).
- base_outside (6): g_storyDefs 0x53a454 accesses within the 0x100 margin after g_golferPhrasesBackup's end; they belong to the
  next global.
- width_narrow (5): low-byte reads / tests of int32 globals (0x0045eaa4, 0x0045e966 for the visitor counters' `add al, 0x40/0x60`,
  0x0041e54d / 0x0041e559 cursor bytes, 0x00426433 bit test). Instructive: the visitor counters are read as bytes exactly where
  they become the low 5 bits of Golfer +0x18.
No loop_end or index_bound finding: the 16-entry and 100-entry loops end exactly at the table ends.

### 5.8 Hook cross-check (shim/src/re/*.cpp)

Agree: c3e.cpp (ShotTrack words +0/+2/+4 as `short`, point slots as `int`, stride 0x388, end 0x5a34e0), c3ea.cpp / c3ec.cpp
(16 entries, signed word +0), c3dy.cpp (`g * 904`), c3ae.cpp (dedication records `short a, b, idx, pad` x 100), c3bs.cpp
(celebrity +0x20 signed byte, stride 0x25), c3dz.cpp (celebrity names at `arg * 37`), c3a.cpp (shotPower cache `int` x 10).
No disagreement found. c3e.cpp's comment calls the table g_storyPairs and its entries pair snapshots (same misreading as the
names row).

### 5.9 Corrections to older notes

- exe_6_globals.tsv g_storyPairs: count 16, layout ShotTrack (section 1).
- golfer_record.md `+0x85 bit 7 from [0x005409a8]`: the bit marks a bubble written by the partner path (section 6).
- golfer_record.md record table: record 152 / 153 are also narrateShot's replay copies of a ShotTrack's two snapshots
  (already listed for 152; 153 too: 0x00407f33..0x00407f43).
- re/tools/sve.py TAIL_B: (0x58f338, 0xb7c0) is the pre-tournament golfer-type copy, (0x520a28, 0x19a28) the pre-tournament
  phrase copy; bit 0x200000 of g_flags = tournament in progress (set 0x0046d009, cleared 0x0045adf7).
- Cluster 419's owner: the golfer-type table has 84 records (section 4).
- loadGolfPro's names row (`loads a hired Golf Pro employee from a .pro file`) does not mention that it saves the two tables
  and starts the tournament state (0x0046c985..0x0046c9ac, 0x0046d009..0x0046d024); left to the names owner.

### 5.10 Cluster coverage

| cluster | typed as | other stream |
|---|---|---|
| 996 | g_storyPairs (ShotTrack[16]) | - |
| 997 | g_storyPairs field +0x88 (point_y) | - |
| 998 | g_storyPairs field +0x108 (point_z) | - |
| 999 | g_storyPairs field +0x188 (golfer_snapshot) | - |
| 1000 | g_storyPairs field +0x288 (partner_snapshot) | - |
| 650 | g_activeObjectLevel[20], g_jpegRowCursor, unk_00543cf4, unk_00543cf8 | 0x543cf0 D |
| 651 | g_amenityUnlockMask | - |
| 652 | g_cachedCameraTileY / X, g_cursorTileX / Y | camera cache D, cursor E |
| 653 | g_golferPhrases (GolferTypePhrases[84]) | - |
| 655 | g_celebrities (record k - 1 access) | - |
| 656 | g_celebrities (CelebrityRecord[100]) | - |
| 657 | g_lastTipDate; Surface[20] at 0x55e5b0 | 0x55e5b0 E |
| 523 | g_golferPhrasesBackup | - |
| 924 | g_golferTypesBackup (84 x 0x230) | - |
| 923 | g_playerShotStyle | - |
| 524 | g_landPurchaseCount | - |
| 797 | g_visitor60Count | - |
| 728 | g_scenicDedications (ScenicDedication[100]) | - |
| 748 | g_nextArrivalOrder | - |
| 749 | unk_0056a520 (write-only) | - |
| 1028 | g_tournamentPrep | - |
| 1027 | g_shotPowerCacheDist[10] | - |
| 1030 | g_ballScreenXs[32] | D |
| 1031 | g_ballScreenYs[32] | D |
| 607 | g_thoughtOnPartner | - |

25 / 25 clusters typed, 54 / 54 stream-A addresses of these clusters covered (end markers 0x55e5cc / 0x55e5cd / 0x58f350
explained as loop ends of neighbouring tables). All 8 gb2 .sve globals named (fields 30, 45, 47, 60, 64, 79, 82, tail B0 / B1;
field 54 keeps g_storyPairs).

## 6. Economy, finance, course records, rankings and the economy scalars (group gb3)

Method: `log/p1/b/ctx.py` over every cluster, `re/tools/record_fields.py` for each table (scan TSVs `t*.tsv`,
`fin_year.tsv`, `rank.tsv` here), the disassembly of the readers and writers (`d.py` here prints the cached
function disassembly), the save fixtures `tests/fixtures/stories/{stories5,mood5}.sve` and
`original/saved games/&AutoSave1.sve` read with `re/tools/sve.py`, and `model_check.py` on `types.tsv`.
Strings are cited by address only. Header: `gb3.h`; address map: `addresses.tsv`; new names: `names.tsv`;
open questions: `uncertain.tsv` (U-0140..U-0159).

### 6.1 Course records `g_courseTable` 0x00571fd8 (17 x 0x2e, CourseRecord)

Base and count. seedCourseTable 0x0046f2b0 clears 0xc3 dwords plus one word = 0x30e bytes from 0x571fd8
(0x0046f2b5..0x0046f2c3); the save writes the same 0x30e bytes (0x0040b384..0x0040b389) = 17 records of 0x2e.
Slots 0..15 are used: the first loop (ebp from 0x57221d = slot 12 +0x1d, to < 0x5722d5) seeds slots 12..15 with
distinct location bytes +0x20 (0x0046f2db..0x0046f3f3); the second loop (ebx 0..11, `cmp ebx, 0xc` at
0x0046f538) seeds slots 0..11 with distinct location ids +0x1c (0x0046f3f9..0x0046f53f); every whole-table loop
ends at 0x5722d4 = slot 16 +0x1c (0x0046f2d4, 0x0046f42a, courseActionMenu 0x0046f745); tierPrice 0x0046f1d0 has
16 cases. Slot 16 is cleared and saved but no instruction addresses it (all-zero in the three saves).
Index: `[g_currentCourse 0x0059bf90] * 0x2e` (`lea ecx,[eax+eax*2]; shl ecx,3; sub ecx,eax; [ecx*2+...]`).
The plan's 0x571ff4 is field +0x1c of slot 0 (the most-read byte), and cluster 793 (0x57221d / 0x572220) is
slot 12 +0x1d / +0x20 used as loop pointers by the first seeding loop.
Source of the seeded values: the location table 0x004c1e90 (stride 0x82, index Random::range(16) at
0x0046f2e9..0x0046f2f0 / 0x0046f3ff..0x0046f406; stream F). courseActionMenu reads the same field layout from
0x004c262e + 0x82 * k (ebx = entry + 0x41, 0x0046f9bd) and draws relief / setting / type labels from it.

| off | width | sign | meaning | evidence |
|---|---|---|---|---|
| +0x00 | char[24] | | course name, copied from the location entry +0x00 | strcpy 0x0046f4da..0x0046f53f; strlen+copy for display 0x00418093, 0x0041cde1, 0x0045a3ef, 0x00470791 |
| +0x18 | 2 | s | map x, copied from location +0x3a | w 0x0046f3b5, 0x0046f4fa; no reader of the copy (U-0140); the menu reads location +0x3a movsx and draws at 0xc8 - x (0x00470033..0x00470045) |
| +0x1a | 2 | s | map y, copied from location +0x3c | w 0x0046f3b9, 0x0046f504 (U-0140) |
| +0x1c | 1 | s (movsx 0x0040d547) | location id 0..15, 0xff unseeded (0x0046f2ce) | indexes the location table 0x004c1ea8 / 0x004c1ea9 (+0x18 byte, +0x19 string) in drawWindow 0x0040d556, appendCourseTitle 0x0040dad1; compared with 0xd / 0xa (decorationName 0x004078be, 0x004079fe; mainLoop 0x00412f4d, 0x00412fb1) and 3 (pairGolfersWithStory 0x0045edab) |
| +0x1d | 1 | u (`xor edx,edx; mov dl` 0x0046fe19..0x0046fe22) | acres: printed by _itoa after the buy label 0x004e3fc8 and before 0x004e3fbc (0x0046fb8a..0x0046fbc1); generateRandomCourse uses v * 10 (0x004720e8..0x004720f6) | seed: (slot + 4) * 10 +/- 20 or 10 by setting, 0xfa with flag 0x1000000 (0x0046f331..0x0046f388; U-0159); 250 in all three saves |
| +0x1e | 1 | s (movsx 0x00470a9a) | relief 0 flat / 1 rolling / 2 hilly, location +0x40 | w 0x0046f4e0; the menu prints [ebx-1] = location +0x40 with labels 0x004e3fa4 / 0x004e3fac / 0x004e3fb4 (0x0046fbe7..0x0046fc0b); heightBlend tests == 2 (0x0040c251), generateRandomCourse switches on it (0x00470a9a..0x00470ab2) |
| +0x1f | 1 | s | setting 0 inland / 1 coastal / 2 island, location +0x3f | w 0x0046f4ce; menu labels 0x004e3f80 / 0x004e3f8c / 0x004e3f98 for [ebx-2] (0x0046fc5d..0x0046fc84); clearTile and generateRandomCourse test 2 / 1 / 0 (0x00470a26, 0x00470dce, 0x00470ff1, 0x004721c5) |
| +0x20 | 1 | s (movsx 0x00470850) | course type 0..3 (Park / Desert / Tropical / Links), location +0x3e | copied into 0x005a34e0 (generateRandomCourse 0x00470a8e..0x00470aa7, FUN_00407d30 0x00407d93..0x00407d9a, mainLoop 0x0040fbbd..0x0040fbc4); 0xff unseeded (0x0046f2ca) |
| +0x21 | 1 | | not addressed (U-0140) | cleared only |
| +0x22 | 1 | s (movsx 0x0046feb8) | year the course was bought (years since 2001), -1 = not owned | w g_year at generateRandomCourse 0x00470a6a..0x00470a94; printed + 0x7d1 (0x0046feb8..0x0046fed0); `cmp 0xff` gates buying and the label 0x004e3f1c (0x0046fa2d, 0x0046fb4d, 0x0047000d, 0x00470663) |
| +0x23 | 1 | s (movsx 0x0046ff3b) | year the player left the course, -1 = never | w g_year in FUN_00407d30 (course switch, 0x00407d63..0x00407d82); printed + 0x7d1 after 0x004e3f38, or 0x004e3f30 when -1 (0x0046ff2f..0x0046ff58) |
| +0x24 | 1 | | g_awardHoleHonours snapshot | w 0x00418c5d (mainLoop award panel block 0x00418bc7..0x00418ca8) |
| +0x25 | 1 | | g_awardLots snapshot | w 0x00418c63 |
| +0x26 | 1 | | always 0 | w 0x00418c6f |
| +0x27 | 1 | | g_awardTournamentPoints snapshot | w 0x00418c76 |
| +0x28 | 1 | | g_awardStories snapshot | w 0x00418c7e |
| +0x29 | 3 | | not addressed (U-0140) | |
| +0x2c | 1 | | g_cash_div100 / 256 (signed, cdq / and 0xff / sar 8) | w 0x00418c84..0x00418c90; saves: 39 with +0x2d 16 = 10000 = the saves' cash |
| +0x2d | 1 | | g_cash_div100 % 256 (signed remainder) | w 0x00418c96..0x00418ca8 |

Correction to `re/names/exe_*_globals.tsv` row g_courseTable (byte[], 12-entry table): the table is 17 records of
0x2e (16 used), and seedCourseTable seeds 16 of them, not 12 (slots 12..15 come from the first loop). The names
row of seedCourseTable 0x0046f2b0 (12 records) has the same error.

### 6.2 Finance: `g_cash_div100` 0x00571fd4 and `g_financeYears` 0x00584210 (100 x 0x14, FinanceYear)

g_cash_div100 (existing row): int32, signed (test / jge at 0x0044e02b), $100 units. Note: the month history and
the finance row copy only its low word (0x00417995, 0x0040fc16), so values above 32767 ($3,276,700) wrap there.

g_financeYears: rep stosd 0x1f4 dwords (0x00442219..0x00442223), save 2000 bytes; row = g_year % 100 (idiv 0x64 at
0x00407db8, 0x00427f9a, 0x0045aaed). showFinancialReport 0x0044f6b0 walks rows max(0, year - 8) .. year
(clamp 0x0044f796..0x0044f7b8, row pointer from 0x584210 step 0x14 at 0x0044f9af) and 9 columns: the label
pointers 0x004d2054..0x004d2074 (`cmp ebp, 0x4d2074; jle`, 0x0044f78e), each value movsx * 100 (0x0044f8b2..0x0044f8b6).

| off | width | sign | column (label pointer slot) | writers |
|---|---|---|---|---|
| +0x00 | 2 | s | 0 greens fees | matchUpdate add (0x004276da) |
| +0x02 | 2 | s | 1 home sites | mainLoop add / sub (0x0041fbc7, 0x0041fe2a), clearObjectFootprint sub (0x0040e4a2) |
| +0x04 | 2 | s | 2 food / drink | updateGolfers add (0x0042aa9f, 0x0042abdd, 0x0042ac95, 0x0042ad4c), updateWalkers add (0x00402bb6) |
| +0x06 | 2 | s | 3 build course | demolishObject add (0x0040a7d8), mainLoop sub (0x00420876, 0x00420a97) |
| +0x08 | 2 | s | 4 facilities | mainLoop sub (0x0041fb6f, 0x0041fea7) |
| +0x0a | 2 | s | 5 salaries | hireEmployee (0x0040ab46..0x0040ab4e), mainLoop sub (0x00417935), handleEmployeeAction add -0x19 (0x00436dc8) |
| +0x0c | 2 | s | 6 maintenance / interest | mainLoop dec / add (0x0041787c, 0x00417ef9) |
| +0x0e | 2 | s | 7 other | starting cash at a new course (0x0040fc1d, FUN_00407d30 0x00407dc3), matchUpdate (0x00427ab2, 0x00427d1b, 0x00427ecb, 0x00427fa2), patronEvent (0x00426877), showBuyLandScreen sub (0x00459349), showTournamentResults (0x0045aafd) |
| +0x10 | 2 | s | 8 total: the report sums columns 0..7 and writes the sum back into +0x10 of each drawn row (0x0044f8f0..0x0044f911) | showFinancialReport only |
| +0x12 | 2 | | never addressed (U-0141) | |

stories5.sve year 0: 349, 0, 12, -138, 0, 0, -57, 10000 (the sandbox start cash in other), 0, 0.

### 6.3 Monthly histories (int16[500] x 5) and the history index

All five are indexed by `(g_date / 1024) % 500` (cdq / and 0x3ff / add / sar 0xa, idiv 0x1f4): g_date's low 10 bits
are the day, bits 10..12 the month (8 months per year), so date / 1024 counts months; 500 slots = 62.5 years before
the ring wraps. Writers: mainLoop at the month change (0x00417973..0x004179e3 and 0x00417c8c..0x00417cd5) and
logTick 0x0040c6f0. Readers: showEndOfYearScreen 0x0044cff0 and showHistographScreen 0x00455f00 (title
0x004d3354, labels 0x004d334c skill, 0x004d2120 fun, 0x004d3344 event, 0x004c88e8 cash). Clears: loadInterfaceArt
0x004421d6..0x0044220b, buildGolferBioText 0x004316fa (event ring).

| symbol | element | content | evidence |
|---|---|---|---|
| g_cashHistory 0x0051b388 | int16 | low word of g_cash_div100 | 0x004179b4, 0x00417cce; stories5.sve 9999, 9998, 9996, 9995 ... |
| g_memberTypeHistory 0x00520640 | int16 | number of golfer types 0..75 with ban byte (+0x29) != 0xff and membership (+0x02 & 7) > 1 | loop 0x005849e2..0x005856f2 step 0x2c, 0x004179c1..0x004179e3 |
| g_funHistory 0x0053e63c | int16 | low word of the fun rating 0x0059ae78 | 0x004179ad, 0x00417cd5 |
| g_skillHistory 0x005409ac | int16 | low word of g_skillRatingX100 | 0x004179a0, 0x00417cc1; negative values in stories5.sve |
| g_eventHistory 0x00568600 | uint16 | logTick(kind, arg) = kind OR arg, kind = n << 5 (0x20..0x160), arg < 32 | callers in 11 functions (U-0144 for the kind list); stories5.sve 0x21..0x25 (announceBuilding kind 0x20) |

### 6.4 Ranking `g_courseRanking` 0x005422f8 (10 x 0x18, CourseRanking) and the award counters

showCourseRanking 0x0045f870 (Top 10 title 0x004d3ca4) fills record 9 from the award globals, sums the five into
+0x14, then repeatedly picks the largest unused total over records 0..9 (0x0045f9bc..0x0045f9e3) and draws each
component as that many icons: +0x00 image 0x0059b28c, +0x04 0x0059b33c, +0x08 0x0059b368, +0x0c 0x0059b394,
+0x10 0x0059b578 (0x0045fa73..0x0045fb58). Rival names: string pointers 0x004c2a60[record] (0x0045f9f3); record 9
prints the course title (appendCourseTitle, 0x0045fa25). Rivals 0..8 are seeded by loadInterfaceArt: pick k (k = 0..8)
takes a random record Random::range(9) whose total is still 0 and gives it k + 1 increments, each to a component
Random::range(4) and to the total (0x004422b1..0x0044231c) and each year mainLoop adds one
(0x004182a8..0x004182e4), so their totals in the saves are a permutation of 1..9 (stories5.sve, mood5.sve).

| off | meaning (record 9 source) |
|---|---|
| +0x00 | g_awardHoleHonours 0x005a9cac |
| +0x04 | g_awardLots 0x005a9cb0 |
| +0x08 | g_awardCashThousands 0x005a9cb4 = cash_div100 / 1000 |
| +0x0c | g_rankComponent3 0x005a9cb8, always 0 for the player |
| +0x10 | g_awardStories 0x00561258 |
| +0x14 | sum of the five (0x0045f9a0..0x0045f9b0) |

Award block in mainLoop (0x00418bc7..0x00418ca8, run before the panel drawn at 0x00419040..0x004193b0): recomputes
0x005a9cac (bits 1 and 2 of hole flag dword +0x200, holes 1..18; holeMagazineEvent sets them at 0x0042e5e8 /
0x0042e726), 0x005a9cb0 (placed objects kind 5 with +0x08 != 0, U-0155), 0x005a9cb4, 0x005a9cb8 = 0, and copies
them with g_awardTournamentPoints and g_awardStories into the current course record +0x24..+0x2d. The panel draws
four icon rows: 0x005a9cac, 0x005a9cb0, 0x005787cc (image 0x0059b5fc), 0x00561258; with 20 or more icons in total it
prints numbers instead (0x0041907f..0x00419082, 0x0041916d..).

### 6.5 Other record tables saved in the .sve

`g_holePoints` 0x0059ae80 (19 x 0x18, HolePoints, **stream C**): per hole number (1..18 used; record 0 has no
writer), two world points (+0x00/+0x04, +0x10/+0x14), -1 = none; +0x08..+0x0f not addressed. Meanings: U-0142.
0x0059ae78 (fun rating) sits just before it and 0x0059b048 / 0x0059b04c just after (separate scalars, see 7).

`g_habitatSlots` 0x00572cb0 (128 x 0x14, HabitatSlot): animal habitat spawn slots. addHabitatSlot 0x00405970 takes the
first slot whose +0x12 is 0xff and writes x = (tile x << 10) + 0x200 (+0x00), y (+0x04), +0x08 = -20, +0x0c = -1,
+0x0e = 0x10c, +0x11 = Random::range(4), +0x12 = its third argument (0x0040598d..0x004059e8); placeHabitat
0x00405e30 writes +0x0c and scans +0x12 of slot runs (0x00405fbc); drawWorldObjects walks the table with `mov ebp, 0x572cbe` (= slot + 0x0e) and `add ebp, 0x14` up to 0x5736be (0x0043036f, 0x00430b09..0x00430b0d), so its `[ebp + k]` operands are fields +0x0e + k: +0x00 / +0x04 position ([ebp - 0xe] / [ebp - 0xa], moved at 0x004304cf, 0x004304f0); +0x08 state ([ebp - 6]: `test eax, eax; jge` 0x00430394..0x00430399, negative = waiting with animation 0x103; bit 0 set / cleared 0x00430894..0x004308b4, bit 1 tested 0x0043048c); +0x0c link ([ebp - 2]: -1 = none, else the index of another slot whose bit 1 of +0x08 is tested, 0x00430492..0x004304a2); +0x0e animation base 0x103 / 0x10c / 0x115 / 0x11e ([ebp], 0x0043039b, 0x00430838, 0x00430886, 0x004308ab); +0x10 frame ([ebp + 2]: (frame + 1) % g_sheetCols[kind + anim], 0x004303b2..0x004303f1, stored 0x004308bb); +0x11 direction ([ebp + 3], rewritten 0x0043047b, 0x0043062e, 0x0043074a); +0x12 kind ([ebp + 4], 0xff frees the slot 0x00430488, 0x00430b01). +0x13 is not addressed. The lead corrected this paragraph after stream C's field map (model_c.md section 5): the first draft missed the ebp-relative accesses and called +0x10 unaddressed.

`g_ballRecords` 0x005736b0 (256 x 0x24, BallRecord): addGolferToTable copies a golfer's ball state (golfer +0xd4 x,
+0xd8 y, +0xdc height, +0xe0 direction, +0xe4 speed / 2, +0xe8 vertical speed / 3, golfer_record.md) and g_date into
the first record with +0x08 == -1 (0x00409b4e..0x00409be1). updateRollingStats 0x00409bf0 each tick: +0x08 +=
foldRange(+0x14, +0x18 >> 4), +0x0c -= absDiff(+0x14, +0x18 >> 4), +0x10 += +0x1c >> 5; at +0x10 <= 0 it zeroes
+0x10 / +0x18 / +0x1c and frees the record (+0x08 = -1) once g_date > +0x20 + 0x400 (one month); +0x18 -= +0x18 >> 4;
+0x1c -= 0x40 while airborne (0x00409bfd..0x00409c98). freeAtTile 0x00402930 frees the records whose +0x00/+0x04
tile matches; updateWalkers and mainLoop 0x00415847 read them.
Why clusters 800 / 801 (0x005758a8..0x005758c4, 0x00575aa8, inside this block) are stream C: every access there is
`[reg + disp]` with reg = hole * 0x208 (holeMagazineEvent 0x0042e374..0x0042e3ce, buildGolferComment
0x0046ac5f..0x0046ac90): the hole table 0x00575ab0 addressed at record hole - 1 (0x00575ab0 - 0x208 = 0x005758a8)
and hole - 1 +0x1f8. They are not ball-record fields; model_check reports them only as warnings (width_narrow /
stride inside g_ballRecords).

`g_golferClassSlotCounts` 0x0059dea0 (int32[4][8]): counts of golfers per (class +0xba, slot stored in golfer +0x19).
updateMembership picks the least-used slot, skipping slots 0..2 and 4 under a condition (0x00421c58..0x00421c60), and
increments it. Saves: only class 0 slots 3, 5, 6, 7 are non-zero (stories5.sve 12, 10, 11, 8).

`cornerLevels` 0x005a4998 (uint8[51][51], **stream C**): the plan called it a level table for stream B. It is the
terrain corner height grid: row * 51 + col, stepCourseEditQueue raises / lowers single bytes (0x00409e9c inc,
0x00409eb4 dec), heightBlend returns a byte of it (0x0040c203), stampCourseTiles copies bytes (0x0040e0a0..0x0040e0b0),
generateRandomCourse writes it; mainLoop resets 50 bytes of each of the first 50 rows to 4 (0x0041ca4b..0x0041ca74).
Not named here (stream C owns it).

`g_bestRounds` 0x0056a524 (int32[10]): the course's best round stroke totals, ascending, 0 = empty. matchUpdate sums
the golfer's per-hole strokes (golfer +0x23 + hole, golfer_record.md; 0x0042835a..0x00428366) over holes 1..hole count when every
hole was played, then inserts (0x004283ff..0x0042848a), shifting the name string slots 0x14 + i (0x0045b9f0 /
0x0045b8b0 at 0x00428462..0x00428469). .srf `Record=%d by %s` prints entry 0 (writeCourseInfo 0x0043209b).
0x0056a510..0x0056a518 (cluster 747) are unrelated scalars (section 7).

### 6.6 Economy scalars

| symbol | addr | type | meaning | sve |
|---|---|---|---|---|
| g_awardHoleHonours | 0x005a9cac | int32 | section 4 | 10 |
| g_awardLots | 0x005a9cb0 | int32 | section 4; byte read 0x00418c48 | 11 |
| g_awardCashThousands | 0x005a9cb4 | int32 | cash_div100 / 1000 | - |
| g_rankComponent3 | 0x005a9cb8 | int32 | always 0 | - |
| g_awardTournamentPoints | 0x005787cc | int32 | +4 - rank for a top-3 finish, +1 otherwise, +1 in matchUpdate | 12 |
| g_awardStories | 0x00561258 | int32 | +1 in advanceStory next to logTick(0x120, story) (0x00466774..0x00466787) | 13 |
| g_skillRatingX100 | 0x00541cd8 | int32 s | holeMagazineEvent's per-hole sum; .srf SkillRating = v * 0.01; signed (-26 in &AutoSave1.sve) | 6 |
| g_ratingMilestone | 0x0059aaf8 | int32 | count of rating milestones passed: next threshold (n + 1)^2 * 25 | 48 |
| g_debtYears | 0x005a6374 | int32 | consecutive year ends with negative cash; 3 ends the career (warnings 0x004d27f0, 0x004d2824, 0x004d285c) | 80 |
| g_tournamentPurse | 0x00567b04 | int32 | tournament purse, $1,000 units (credited v * 10 in $100 units), decays to 2/3 after a win (U-0154) | 65 |
| g_buildingsUnlocked | 0x005a6364 | int32 | number of building kinds available (6 new course, 17 sandbox) | 29 |
| g_buildingKindMask | 0x005a6370 | flags32 | kinds >= 6 present with flag 0x40 | - |
| g_freeUnlockMask | 0x00822c70 | flags32 | items shown with the free marker 0x004c82c0 | 41 |
| g_difficulty | 0x00822c88 | int32 s | -1 menu, 0..3 Easy..Impossible (labels 0x004c6210.. ), 5 / 6 special (U-0152); word read 0x004733e4 | 31 |
| g_year | 0x005a6d3c | int16 s | g_date >> 13 (years since 2001), written only by appendDate | - |
| g_nextGolferSlot | 0x0059ae7c | int32 | round-robin golfer record cursor (mod 152) | 37 |
| g_lastAllocatedGolfer (gb3 wrote g_playerGolfer; renamed, section 2.2) | 0x005a5a24 | int32 | golfer record of the created character | 43 |
| g_challengeLevel | 0x0059b730 | int32 | famous-golfer challenge level (wager scale), U-0158 | 36 |
| g_lastStoryAdvanceDate | 0x0059b048 | int32 | g_date of the last story advance | 52 |
| g_thoughtSeenLo / Hi | 0x0059c08c / 0x00571d38 | flags32 | 64-bit thought-kind seen mask | 39 / 75 |
| g_demolishPendingY | 0x005a6368 | int32 | y of the tile awaiting a confirming click (x in 0x004c2e24) | - |
| g_saveFd | 0x00568d08 | int32 | CRT handle of the open save / top-10 file | - |
| g_shotPowerRingIndex | 0x005a9ce0 | int32 | ring index 0..9 of shotPower's three history arrays | - |

### 6.7 Cluster 1110 (0x005a9cb4..0x005a9ce8) and the leftovers

Cluster 1110 is 14 unrelated dwords plus one byte, not one object: 0x005a9cb4 / 0x005a9cb8 (awards), 0x005a9cbc /
0x005a9cc0 (view projection flags, D, U-0147), 0x005a9cc4 (shot simulation mode, U-0148), 0x005a9cc8 (result of
0x0043d6f0 in ~90 mainLoop sites, D, U-0156), 0x005a9ccc (message hold counter, E), 0x005a9cd0 (int8, status text
cache, E), 0x005a9cd4 (edit queue count, bound 0x100, C), 0x005a9cd8 / 0x005a9cdc (playSoundAt flag and counter, D),
0x005a9ce0 (shot power ring index), 0x005a9ce4 / 0x005a9ce8 (shot target flags, U-0149). Stream A's class `scalar`
with count 14 is right as a count of addresses but there is no record.
Cluster 1069: 0x005a6d3c is g_year (int16); 0x005a6d40 is the message text buffer startMessage copies into (char,
upper bound 0x400 to the next referenced address 0x005a7140; stream E). Cluster 507: 0x00519d30 / 0x00519d40 are
members +0x5c / +0x6c of the object at 0x00519cd4 (stream E). Cluster 586: 0x0053fc20 is a divisor table without a
direct writer next to buildSceneSprites' frame-count divisors (stream D, U-0153). Cluster 747: 0x0056a510 the JPEG
scanline buffer of saveCourseJpg (D), 0x0056a514 an index drawn by mainLoop (E, U-0157), 0x0056a518 never written
(U-0146). Cluster 945: 0x0059b734 system-menu request flag (U-0151), 0x0059b738 / 0x0059b73c shot / golfer scratch
(U-0150; 0x0059b73c is written and never read). 0x0059b04c (cluster 935) is never written (U-0145).

### 6.8 Corrections and findings for other owners

- Top-10 high scores: 0x00541ce0..0x005422f7 is a table of 10 x 0x9c = 0x618 bytes, read whole from `top10.sve` by
  loadTopScores (`push 0x618; push 0x541ce0; ... _read` at 0x0040af31..0x0040af41) and walked by addScore
  (0x00473314..0x00473357, loop end 0x0054237c). original/top10.sve is 1,560 = 0x618 bytes. This closes the open
  question in `re/analysis/save/0040afa0_save_serializer.md` (top10.sve reader not located). Not in gb3's clusters.
- buildGolferBioText 0x004315e0 resets the whole course state (golfer block, histories, award counters, ranking
  cursors 0x00431b44..0x00431ba1); its name describes only part of it (finding for the names owner).
- `seedCourseTable` names row: 16 records, not 12 (section 1).
- The plan's level table 0x005a4998 for stream B is the terrain corner grid of stream C (section 5).
- sve.py NAMES gives 0x00541cd8 `skill_rating_x100`; no globals row existed, now g_skillRatingX100.

### 6.9 model_check and hook cross-check

`model_check.py types.tsv`: **0 errors, 90 warnings** (`check.tsv`). Warnings:
- width_narrow 26: byte / word reads of dword scalars the original really makes (cash low word 0x00407dba,
  0x0040fc16, 0x00417995, 0x00417cb3; skill low word 0x0041798f, 0x00417cad; g_awardLots / g_awardTournamentPoints /
  g_awardStories low bytes 0x00418c48 / 0x00418c56 / 0x00418c69 for the course snapshot; g_challengeLevel byte
  0x0040f437; g_year byte 0x00407d63, 0x00470a6a; g_difficulty word 0x004733e4; 0x0056a514 byte 0x0041f849); the
  HabitatSlot +0x08 byte test of bit 1 (0x004304a2, 0x0043078e); and the hole-table accesses inside g_ballRecords
  (section 5).
- sign 5: cornerLevels zero-extended bytes compared with jle (0x0041da64, 0x0041dc04: `xor ecx,ecx; mov cl`, benign);
  three movsx of 0x005722b6 inside g_courseTable slot 15 +0x2c (scanTileLine 0x0042224e, trialShot 0x00422897,
  updateGolfers 0x0042c7ae): these are the tile-type grid 0x005722e8 addressed at row - 1 (0x005722e8 - 0x32), the
  same biased-index pattern, not course-record reads.
- stride 59: the tracker sees the `lea`/`shl` chains (i * 23 * 2 = 0x2e, i * 5 * 4 = 0x14, i * 9 * 4 = 0x24,
  i * 3 * 8 = 0x18) as strides 2 / 4 / 8; the ranking's `[eax*4 + 0x5422f8]` with eax = record * 6 + component
  (0x004182c1..0x004182c6, 0x004422ef..0x004422f1) is a dword index into the whole table.
Hook cross-check (`hookwidth.py`, `hookwidth.txt`; literal addresses in shim/src/re/*.cpp): 883 accessor uses land in
gb3's typed ranges, 16 have a width different from the typed field, and all 16 copy one of the original's narrow
instructions listed above (c3fy.cpp 592 / 5877 / 5878 / 5993 / 5994 / 6353 / 6354 / 6357 / 9723, c3ah.cpp 101,
c3ed.cpp 237, c3fb.cpp 471, c3fv.cpp 2879) or a biased hole-table access (c3ac.cpp 196). No disagreement in stride.

### 6.10 Cluster table

| cluster | typed as / not typed |
|---|---|
| 791 | g_cash_div100 (existing name) |
| 792 | g_courseTable (CourseRecord[17]) |
| 793 | inside g_courseTable (slot 12 +0x1d / +0x20 loop pointers) |
| 516 | g_cashHistory |
| 522 | g_memberTypeHistory |
| 567 | g_funHistory |
| 608 | g_skillHistory |
| 727 | g_eventHistory |
| 632 | g_courseRanking |
| 633 | g_courseRanking record 9 |
| 933 | g_holePoints (other_stream C) |
| 855 | g_financeYears |
| 798 | g_habitatSlots |
| 799 | g_ballRecords (clusters 800 / 801 inside the block are hole-table accesses, stream C) |
| 978 | g_golferClassSlotCounts |
| 1034 | cornerLevels uint8[51][51] (other_stream C, not named) |
| 1108 | g_awardHoleHonours |
| 1109 | g_awardLots |
| 1110 | 15 scalars (section 7): g_awardCashThousands, g_rankComponent3, g_shotPowerRingIndex named; 0x005a9cd4 C; 0x005a9cbc / cc0 / cc8 / cd8 / cdc D; 0x005a9ccc / cd0 E; cc4 / ce4 / ce8 typed, meaning open |
| 750 | g_bestRounds |
| 747 | 0x0056a510 (D), 0x0056a514 (E), 0x0056a518 typed, unnamed |
| 1069 | g_year; 0x005a6d40 message buffer (E, unnamed) |
| 507 | 0x00519d30 / 0x00519d40 members of the 0x00519cd4 object (E, unnamed) |
| 586 | 0x0053fc20 int32[8] divisor table (D, unnamed) |
| 925 | g_ratingMilestone |
| 932 | g_nextGolferSlot |
| 934 | g_lastStoryAdvanceDate |
| 935 | 0x0059b04c, typed, never written (U-0145) |
| 944 | g_challengeLevel |
| 945 | 0x0059b734 / 0x0059b738 / 0x0059b73c typed, unnamed (U-0150, U-0151) |
| 963 | g_thoughtSeenLo |
| 1067 | g_debtYears |
| 1066 | g_buildingKindMask |
| 1064 | g_buildingsUnlocked (0x005a6364) and g_demolishPendingY (0x005a6368) |
| 785 | g_thoughtSeenHi |
| 821 | g_awardTournamentPoints |
| 669 | g_awardStories |
| 629 | g_skillRatingX100 |
| 716 | g_tournamentPurse |
| 1060 | g_lastAllocatedGolfer |
| 729 | g_saveFd |
| 1601 | g_freeUnlockMask |
| 1604 | g_difficulty |

All 43 clusters are typed (width, count, sign); 36 new names (61 address rows); 23 addresses typed without a name because their
meaning is open or another stream owns them.

## 7. Date, RNG, session flags, preferences, player skills, message and text state (group gb4)

Method: `log/p1/b/allrefs.txt` / `ctx.py` for every reference of the 52 clusters, the disassembly windows around
them (`log/p1/b/gb4/win.py`), a per-bit scan of the flag word (`flagbits.py` + `flagstores.py` + `bittable.py`, raw
output `flags_raw.tsv`, `flags_extra.tsv` for register-built masks read by hand), and a scan of every
`ehVectorConstructorIterator` 0x004a55d2 call with its (base, size, count) pushes (`ehvec.py` -> `ehvec.tsv`).
Save values from `tests/fixtures/stories/{mood5,stories5,stories9}.sve` and `original/saved games/&AutoSave{1,2}.sve`
through `re/tools/sve.py dump`. Files: `gb4.h`, `types.tsv`, `check.tsv`, `addresses.tsv`, `names.tsv`,
`uncertain.tsv` (U-0160..U-0174), `flags_table.md` (the g_flags table, copied into section 3).

### 7.1 g_date 0x00834170 (cluster 1633), int32, save field 28

| off | width | sign | meaning | evidence |
|---|---|---|---|---|
| +0 | 4 | s (`cdq; and edx, 0x3ff; add; sar 0xa` 0x0040c6ff..0x0040c70e, the signed-division idiom) | game date in ticks: month serial = d / 1024, month = serial % 8 (index 0 = the third calendar month .. 7 = the tenth, pointer table 0x004c2908), year = d / 8192, shown + 2001 (`add ebx, 0x7d1` 0x0040d7fc) | appendDate 0x0040d7b9..0x0040d7e8 |

- Tick: mainLoop adds 1 per frame (0x0041762f..0x0041763e); with g_flags bit 3 an odd result gets `+ (d & 1)`
  (0x00417643..0x0041764c), so the date steps by 2.
- Writers: 0x2001 when the course opens and the date is <= 0x2000 (0x0040fefc..0x0040ff1b), 0x2c00 by startNewCourse
  (0x0046ddfa), 0 at the title menu (0x00420e4a), saved and restored around a call by FUN_00407d30 (0x00407d6a /
  0x00407d89 / 0x00407ddd), scenario dates from the 0x30-byte table 0x004c15a0 by showBulletinBoard (0x0046eaf0..0x0046eafc,
  0x0046ebef). Save values: 0x0924 (mood5), 0x17c3 (stories5), 0x0b91 (stories9), 0x1200 / 0x1000 (AutoSaves).
- Month-serial users: history tables indexed by serial % 500 (`idiv 0x1f4`: logTick 0x0040c70e..0x0040c715, mainLoop
  0x00417973..0x004179b4, 0x00417c8c..0x00417cbf, showEndOfYearScreen 0x0044d257..0x0044d294); hireEmployee stores the
  serial (0x0040acb4..0x0040acc0). The low bits gate per-frame work (`test byte [d], 3 / 7 / 0x7f`: 0x00402a92,
  0x00428ae8, 0x0040fe4e; 18 byte reads, the model_check `width_narrow` warnings).
- The day-of-month decode is in saveGameDialog 0x00405b91..0x00405bbf (section 2.2; U-0173 withdrawn).
- **Correction** (re/names/exe_10_globals.tsv row and re/analysis/systems/exe_10.md): logTick 0x0040c6f0 does not write
  g_date; it reads it (0x0040c6fa) and writes the word table 0x568600 at (d / 1024) % 500.

### 7.2 The RNG (cluster 1608) and 0x0051b380

`Random` = `{ uint32_t state; }` (4 bytes). g_rng 0x00822d9c is the only instance: 317 `mov ecx, 0x822d9c` before
`call 0x45c1e0`. Random::next (0x0045c1a0, thiscall): `state = state * 0x41c64e6d + 0x3039` (0x0045c1ad..0x0045c1b8),
returns `((state >> 16) & 0x7fff) * [0x4ba800]` as a double, 0x4ba800 = 3.0517578125e-05 = 1/32768. Random::range(n)
(0x0045c1e0, `ret 4`, one argument): `__ftol(next() * (n & 0xffff))`, a value in 0..n-1. seedFromTime 0x00467110:
`t = timeGetTime()` (import slot 0x004ba21c), `g_rngSeedTime = t` (0x00467119), `state = t * 37`
(`lea ecx, [eax + eax*8]; lea edx, [eax + ecx*4]` 0x00467116..0x00467121). FUN_0045c190 stores 0.

Corrections: re/names/exe_1_globals.tsv names 0x004ba800 `gRngState`, `read and written by Random::next`: it is the
read-only double constant 1/32768 (`fmul qword [0x4ba800]` 0x0045c1ca; no write); the state is [ecx] = 0x822d9c.
re/names/exe_1.tsv's Random::range row says `[lo, hi]`: the function takes one argument (`ret 4`) and returns 0..n-1.
0x0051b380 has no reader in game logic (one write).

### 7.3 g_flags 0x0059e7b8 (cluster 992), flags32, save field 70

237 references in 58 functions (0x59e7ba is its third byte: `test byte [0x59e7ba], 0x20` = bit 21). Whole-word writes:
the title menu 0x40000000 (0x00420e50), startNewCourse 0x4000000 then 0 when pickAProScreen returns 0
(0x0046de04 / 0x0046de17), loadGameBrowserScreen saves the word on its stack (0x0043b656..0x0043b660) and restores it
(0x0043c8ff, 0x0043caaa), loadGame keeps bit 2, clears bits 2..3 and sets bit 30 (0x0040bae7..0x0040bb08), and
deserializeCourse reads it from the save (0x0040be86). Save values: 0x41040000 (mood5, stories5, stories9: bits 30, 24,
18) and 0x41000000 (both AutoSaves).

| bit | mask | set | clear | toggle | tests | meaning / evidence |
|---|---|---|---|---|---|---|
| 0 | 0x00000001 | drawWorldObjects | drawWorldObjects | - | 2 (slopeX, slopeY) | set by drawWorldObjects on entry (0x00430b24) and cleared before its return (0x00430e34); slopeX / slopeY test it (0x0040c3a0, 0x0040c2f8) and heightBlend (0x0040c179): `drawing world objects` |
| 1 | 0x00000002 | clickBuildTool433d30, mainLoop | FUN_0040d890, clickBuildTool433d30, mainLoop | - | 4 (mainLoop) | set at the frame head when [0x55e928] != 0 and bit 27 is clear (0x0040fd40..0x0040fd54) and by clickBuildTool433d30 when a tool is picked (0x00433e33); cleared on cancel (0x00433d56) [U-0163] |
| 2 | 0x00000004 | - | loadGame | mainLoop | 19 (drawFlyingBall, drawWorldObjects, mainLoop, placeAnimal, showMainRadialMenu) | PAUSED: toggled by key code 0x50 (0x0041d020; case 0x47 of byte table 0x0042191c); when set, mainLoop skips updateGolfers / updateWalkers / 0x409bf0 (0x00417601..0x00417613); kept across loadGame (0x0040baec..0x0040baf4) |
| 3 | 0x00000008 | - | loadGame | - | 3 (mainLoop) | after the tick increment, date += date & 1 (0x0041763b..0x0041764c); mainLoop tests 0x00414f8d, 0x00415067; no setter in game logic, loadGame clears it (0x0040baef) [U-0160] |
| 4 | 0x00000010 | - | - | - | 1 (updateGolfers) | tested by updateGolfers 0x0042bc3a only; no setter [U-0160] |
| 5 | 0x00000020 | - | - | - | 11 (buildSceneSprites, drawFlyingBall, drawQueuedSprite, drawWorldObjects, mainLoop, prepareShot, updateGolfers) | tested at 11 sites (drawWorldObjects 0x00430a82 then __itoa into g_textBuf, drawQueuedSprite 0x00462d73, prepareShot 0x00424162, updateGolfers 0x0042917d); no setter [U-0160] |
| 6 | 0x00000040 | - | - | - | 7 (announceBuilding, mainLoop, showCourseReport, updateMembership) | tested by announceBuilding 0x0040e74f, updateMembership 0x00421c51, showCourseReport 0x0044fe73; no setter [U-0160] |
| 7 | 0x00000080 | - | - | - | 1 (drawFlyingBall) | tested by drawFlyingBall 0x00403d41 only; no setter [U-0160] |
| 8 | 0x00000100 | updateWalkers | updateWalkers | - | 3 (pathStep) | updateWalkers sets it around one call and clears it after (0x004035b4 / 0x004035d6); pathStep reads it as a 0 / 0x5a term (0x0042e94f..0x0042e95f) |
| 9 | 0x00000200 | - | - | - | 0 | no reference in game logic |
| 10 | 0x00000400 | - | - | mainLoop | 1 (mainLoop) | toggled by key code 0x54 (0x0041d02c); tested once (0x0041408c) [U-0160] |
| 11 | 0x00000800 | clearObjectFootprint, mainLoop | mainLoop | - | 1 (mainLoop) | set by clearObjectFootprint when the removed type is 0xf (0x0040e4cc..0x0040e4df) and by mainLoop 0x0041ca8b; cleared 0x0041f8be; tested 0x00419785 [U-0163] |
| 12 | 0x00001000 | - | updateGolfers | - | 1 (pathStep) | cleared by updateGolfers 0x0042b38e; tested by pathStep 0x0042eb6a; no setter [U-0163] |
| 13 | 0x00002000 | mainLoop, showCourseReport | clickGolferPanel, customizeCharacter | - | 2 (clickGolferPanel, drawGolferActionPanel) | the argument passed to customizeCharacter with bit 14 (0x0040fd0d..0x0040fd13); set by mainLoop 0x00410361 and showCourseReport (0x402000, 0x00452fb7), cleared by clickGolferPanel 0x004361d6 and customizeCharacter 0x0040f4a5; what customizeCharacter does with 1 vs 0 is not determined [U-0162] |
| 14 | 0x00004000 | clickGolferPanel | customizeCharacter | - | 1 (mainLoop) | CUSTOMIZE_REQUEST: set by clickGolferPanel 0x004361da / 0x0043621f; the frame head then calls customizeCharacter 0x0040f190 with argument (flags >> 13) & 1 (0x0040fcfd..0x0040fd14), which clears bits 13 and 14 (0x0040f4a5) |
| 15 | 0x00008000 | - | mainLoop, showTutorialText | - | 2 (mainLoop, tickMessage) | cleared by showTutorialText together with page := 0 (0x00460d58..0x00460d5b) and mainLoop 0x0041c13d; while set tickMessage does not count down (0x0040d72a); no setter [U-0160] |
| 16 | 0x00010000 | - | - | - | 0 | no reference in game logic |
| 17 | 0x00020000 | - | - | - | 3 (mainLoop, updateGolfers) | tested by mainLoop 0x0041e668 (frame wait 0xaf vs 0xfa) and updateGolfers 0x0042ba19; no setter [U-0160] |
| 18 | 0x00040000 | mainLoop | rebuildHeightfield | - | 1 (mainLoop) | set by mainLoop after the 4-iteration block 0x00413908; cleared by rebuildHeightfield 0x0042f929; tested 0x00413682 [U-0163] |
| 19 | 0x00080000 | pathStep | updateGolfers | - | 1 (updateGolfers) | set by pathStep 0x0042ee1c; cleared by updateGolfers 0x0042b367; while set the golfer step +0x14 is 3 (0x0042b45f..0x0042b466) [U-0163] |
| 20 | 0x00100000 | - | - | - | 1 (mainLoop) | tested by mainLoop 0x0040fe07 only; no setter [U-0160] |
| 21 | 0x00200000 | loadGolfPro | mainLoop, showTournamentResults | - | 31 (clickGolferPanel, demolishObject, drawFlyingBall, drawMatchScoreboard, drawQueuedSprite, mainLoop, matchUpdate, narrateShot, placeAnimal, prepareShot, serializeCourse, updateGolfers, updateWalkers) | TOURNAMENT: set by loadGolfPro after it builds the tournament field (0x0046d009); cleared by showTournamentResults 0x0045adf7 and mainLoop 0x0041c8c3; gates save tail B (0x0040b465) and the membership points (re/analysis/golfers/00406670_membership.md); 31 tests |
| 22 | 0x00400000 | mainLoop, showCourseReport | mainLoop, showCourseReport | - | 3 (clickGolferPanel, drawGolferActionPanel, mainLoop) | set by mainLoop 0x004181cb and showCourseReport 0x00452fb7; cleared 0x004181fc, 0x0041c8c3, showCourseReport 0x00452d65; mainLoop picks the string 0x004c675c when set (0x0041661c..0x00416628), clickGolferPanel 0x00436118 and drawGolferActionPanel 0x00436774 test it [U-0163] |
| 23 | 0x00800000 | chooseShotTarget | chooseShotTarget | - | 1 (prepareShot) | set at chooseShotTarget's start (0x00422fdf) and cleared at its end (0x004240bd); prepareShot tests it 0x004250a2 [U-0163] |
| 24 | 0x01000000 | mainLoop | - | - | 18 (buildScenarioIntro, canAfford, courseActionMenu, drawMoneyLabels, generateRandomCourse, mainLoop, pairGolfersWithStory, pointsPopup, rebuildTileCoverage, seedCourseTable, showEndOfYearScreen) | FREE_MONEY: the frame head stores 0x2710 into the purse [0x571fd4] (0x0040fc57..0x0040fc64), canAfford returns 1 (0x00406c48), drawMoneyLabels / pointsPopup skip; set on two menu paths (0x00420b36 after showPopup returns 0, 0x0042141b with purse 0x186a0); never cleared except by whole-word writes; set in all five saves read |
| 25 | 0x02000000 | - | stepCourseEditQueue | - | 2 (emitGolferThought, mainLoop) | cleared by stepCourseEditQueue 0x00409d7c; while set emitGolferThought returns at once (0x00467a31) and mainLoop calls stepCourseEditQueue (0x0041c587..0x0041c593) [U-0163] |
| 26 | 0x04000000 | mainLoop, startNewCourse | mainLoop | - | 16 (FUN_00407d30, buildTournamentPrep, clickGolferPanel, emitGolferThought, loadGolfPro, loadThemeFile, mainLoop, matchUpdate, postEvent, showTournamentResults, updateGolfers) | set on the menu path that stores course type -1 (0x00421546..0x00421557) and by startNewCourse (0x0046de04); cleared 0x0040fb2e, 0x00420e15; FUN_00407d30 returns at once when set (0x00407d36); 16 tests, five of them as 0x4200000 with bit 21 [U-0161] |
| 27 | 0x08000000 | - | mainLoop | - | 4 (drawBuildingPanel, drawTerrainToolbar, mainLoop) | cleared with bit 1 (0x0041cffd, 0x0041c9db) and at 0x00415bd1; tested 0x0040fd4d, drawTerrainToolbar 0x00433ec0, drawBuildingPanel 0x004346c7 [U-0163] |
| 28 | 0x10000000 | loadGolferRoster | loadGolferRoster | - | 1 (loadStoryStrings) | STORY_FALLBACK: loadGolferRoster clears it (0x004659fe) and sets it when the theme lists no text files (0x00465ab4); loadStoryStrings then takes the standard theme's path (0x00466c1b..0x00466c25) |
| 29 | 0x20000000 | mainLoop | - | - | 0 (-) | set with purse += 100 by key code 0x2b (0x0041d03b..0x0041d054); never tested |
| 30 | 0x40000000 | loadGame, mainLoop | - | - | 1 (serializeCourse) | SAVE_TAIL_A: the title menu resets the word to exactly this bit (0x00420e50), loadGame sets it (0x0040baf9); serializeCourse writes tail A (0x004e06c8, 0x8c0 bytes) when set (0x0040b444) |
| 31 | 0x80000000 | - | - | - | 0 | no reference in game logic |

Bits 9, 16 and 31 have no reference. Bits with `no setter` are reached only through the whole-word writes above.
**Corrections**: re/analysis/sim/mainLoop_structure.md (Globals list) says the frame head calls customizeCharacter
under 0x400000; the test is `test ah, 0x40` = **0x4000** (0x0040fd08), and the argument is bit 13 (0x0040fd0d..0x0040fd13).
The same list calls 0x1000000 `championship` and 0x200000 `story advance enabled`: what the code shows for 0x1000000 is
the free-money behaviour (row 24; set in all five saves read), and 0x200000 is set by
loadGolfPro after it builds a tournament field (row 21). Which title-menu item sets which bit is left to U-0161.

### 7.4 Other flag and state words

#### g_preferences 0x005a5a00 (cluster 1058), flags32, save field 85 (other_stream=F: user options)
showPreferencesMenu 0x00432570..0x00432608 copies the 6 option strings of the pointer table 0x004c2ca8..0x004c2cc0 into
the text buffer 0x51a068, calls showPopup(0x190, 0xc8, g_preferences, ...) and stores the result back. -1 (all set) at
0x0040f8a5; key code 0x4e toggles bit 0 (`xor [0x5a5a00], ebx` 0x0041cc7f, ebx = 1 from 0x0041c54e). -1 in all five saves.

| bit | test site | effect shown by the code |
|---|---|---|
| 0x01 | mainLoop 0x00412615, drawQueuedSprite 0x00462dff | when set mainLoop skips a draw block (`jne 0x41264f`) |
| 0x02 | narrateShot 0x00409111 | when clear a local result is forced to 0 (0x00409115) |
| 0x04 | emitGolferThought 0x004697eb | the thought text is drawn only when set |
| 0x08 | membershipReport 0x00406bd8 | a tail block runs only when set |
| 0x10 | mainLoop 0x00416d43 | a draw call runs only when set |
| 0x20 | placeHabitat 0x00405ec0, drawWorldObjects 0x0043037f | placeHabitat spawns only when set |

Which option line is which bit, and the in-game meaning: U-0166.

#### g_shotFlags5a7140 0x005a7140 and g_messageTimer 0x005a7144 (cluster 1070)
| addr | width | sign | meaning | evidence |
|---|---|---|---|---|
| 0x5a7140 | 4 | flags | bits 0..1 = 0 / 1 / 2 chosen by chooseShotTarget from a distance compare with 0x4b; bit 2 ORed when its local [esp+0x9c] != 0; prepareShot zeroes it, clears bits 0..1 and branches on non-zero | 0x00424005..0x00424031, 0x004240a1..0x004240a8, 0x00424565, 0x00425937..0x0042594c [U-0165] |
| 0x5a7144 | 4 | s (`jg` 0x0040d74d) | message display countdown: startMessage stores len + 0x10; tickMessage decrements on dates with d % 3 != 0 while g_flags bit 15 is clear and clears the showing byte 0x569498 at <= 0; a key case and buildScenarioIntro store 0x80 | 0x0040cba6, 0x0040d72f..0x0040d74f, 0x0041d988, 0x004603db |

#### g_playerGolfer 0x005a59f8 (1056) and g_lastCameraRot 0x005a59fc (1057)
0x5a59f8: int32 golfer index, -1 = none (signed: `cmp [0x5a59f8], -1`, `jle` 0x0041c714). customizeCharacter stores the golfer
it gives record flag +0x10 |= 0x200 (0x0040f575..0x0040f590); 81 reads in 15 functions compare golfer indices with it
(narrateShot, scanShotArea, prepareShot, matchUpdate, updateGolfers, mainLoop far code 0x0041b2a8..0x0041c05c). -1 writes:
buildGolferBioText 0x00431b9c, mainLoop 0x0041c821 / 0x0041c8d8, patronEvent 0x00427365, matchUpdate 0x0042807c,
updateGolfers 0x0042983d and 0x00429991 (when the golfer being removed is it). updateGolfers also stores its loop golfer
(0x0042bfac) [U-0164]. The brief's reading (golfer index with record flag 0x200) holds for the customizeCharacter writer.
0x5a59fc: the previous frame's g_cameraRot [0x5685f4], one of the four camera words mainLoop compares before clearing the
tile caches (0x0041056c..0x004105c9) -> other_stream=D.

#### g_playerSkills 0x005a5a04 (cluster 1059), int8[2][16], save field 57 (0x20 bytes)
| off | width | sign | meaning | evidence |
|---|---|---|---|---|
| +0x00..+0x0b | 1 each | s (`movsx` 0x0043af19, 0x0046f08d; `cmp al, 0xa; jge` 0x0045f78a) | the player's 12 skill levels 0..10; bit i of the skill mask is set when byte i != 0 (0x0040f1b0..0x0040f1c8, 0x0041cec5); loadGolfPro copies them into golfer 1's +0xf0 + i (0x0046cf52..0x0046cf70); skillPointDialog raises one by 1 (0x0045f79f..0x0045f7db); showBulletinBoard sums the 12 (0x0046f08d..0x0046f09a) | as cited |
| +0x0c..+0x0f | 1 each | | written only by the whole-row copies (loadThemeFile 4 dwords 0x00438156..0x0043816c from a theme record, read back by promptAndSaveFile 0x00437ce0..0x00437cf5) | not determined |
| +0x10..+0x1f | 1 each | s | row 1, read by narrateShot with row (a != b) << 4 (0x0040894f..0x00408983) | [U-0167] |

Whole-block: 8-dword clear at the title menu (0x00420e64..0x00420e71), 8-dword copy into skillPointDialog's frame
(0x0045f116..0x0045f124). The 4-dword row writes are why model_check reported 8 `width_cross` errors on a plain char[32];
the struct form (two char[16] fields) turns them into `width_block` warnings (inline row copies).

#### g_courseType 0x005a34e0 (cluster 1001), int8, save field 4 (other_stream=C: course)
From the course record byte +4 (0x00571ff8 + 0x2e * n: generateRandomCourse 0x00470a8e..0x00470aa7, FUN_00407d30
0x00407d93..0x00407d9a); 0 at the title (0x0040f8f0), -1 on the menu path that sets g_flags bit 26 (0x0042154b),
restored by loadGameBrowserScreen (0x0043c8f7, 0x0043cab8). Signed: 45 `movsx`. Values 0..3 index 4-entry jump tables
after `cmp 3; ja` (inRange 0x004070fc / 0x00407270, loadWorldAssets 0x0043e84a / 0x0043e910). Saves: 0 (mood5, stories5,
AutoSaves), 3 (stories9). A shim comment (loadWorldAssets' reimplementation) names the four types; the header keeps
numeric constants. Hooks read it as both `G<unsigned char>` and `G<signed char>`: only the -1 case distinguishes them, and
the binary reads it with `movsx`.

#### g_tutorialPage 0x00567a18 and g_eventFlags567a1c 0x00567a1c (cluster 710)
0x567a18: tutorial page, int32 (showTutorialText switches on page - 1 over 29 cases, table 0x00460d70, `ja` 0x0046050f;
increments it 0x00460b90..0x00460ce3; 0 at 0x00460d5b together with clearing g_flags bit 15) -> other_stream=E.
0x567a1c: bit 0 set by patronEvent (0x00426b98), tested and reset to 0 by mainLoop (0x0041e2a0, 0x0041e366) [U-0168].

### 7.5 Text and number buffers, comment tone

| symbol | addr | type | evidence |
|---|---|---|---|
| g_textBuf | 0x0058a528 | char[16] | 81 `push 0x58a528` in 19 functions, each the destination of __itoa 0x004ad425 (radix 0xa pushed first, e.g. appendDate 0x0040d81a..0x0040d850), whose text is then appended to the shared buffer 0x51a068; the next object (Surface[72]) starts at 0x0058a538 |
| g_infoPanelNumBuf | 0x00824134 | char[16] | the same __itoa role (0x0045c7ea..0x0045c7f2) in 8 functions; the next referenced dword is 0x00824144 |
| g_itoaBuf4e9a70 | 0x004e9a70 | char[16] | placeAnimal's __itoa destination (0x0040191d, 0x0040193e); next referenced address 0x004e9a80 |
| g_commentTone | 0x0058b198 | uint32 | buildGolferComment picks one of 0x80006318 / 0x800023e8 / 0x80007d08 (60 stores); showCharacterEditor, showHoleStatsScreen (maps 0x800023e8 to 0x80001284) and showPlayerCommentsReport read it as the colour word of the comment line |

**Correction to stream A**: cluster 879 is not a `record array 0x2c x 7`: 0x58a528 is a 16-byte buffer and 0x58a538 starts
an unrelated Surface[72] (the 0x2c stride stream A saw). The names row `g_textBuf` is right in role; its size is 16 bytes.
The 16-byte sizes rest on the next referenced address, not on a bound inside __itoa (a radix-10 int fits in 12 bytes).

### 7.6 Per-frame tables and counters

| symbol | addr | type | meaning | evidence |
|---|---|---|---|---|
| g_objectTypeLevel | 0x005a8c38 | int32[20] | per placed-object type t: clamp(old, object dword +8 + 1, 0x63) over every placed object with t >= 6, t != 5; 0 for types with none | 20-dword clears 0x0040fc36..0x0040fc42 (each frame), 0x004315ec; loop 0x0040fc6e..0x0040fcde over 0x58bcb8 step 0x10; readers pickBuilding, drawBuildingPanel (loop end 0x5a8c74), stampCourseTiles, objectAt, updateGolfers, buildSceneSprites, buildGolferComment; biased base 0x5a8c34 (type - 1) 0x00413146 -> other_stream=C |
| g_frameMs | 0x005a8c88 | int32 | elapsed ms of the frame: timeGetTime minus the frame-start time | 0x0041e639..0x0041e64a, frame start 0x0040fc29..0x0040fc2f |
| g_nameLabelCount | 0x0058bcb0 | int32 | number of (x, y) entries appended to 0x5a47e8 / 0x5a4868 during the golfer pass | 0 at 0x0041495a, store after `inc edi` 0x004157ce..0x004157d5, loop bound 0x00418b4d..0x00418b56 -> other_stream=D |
| g_bulletinAnimTick | 0x00839338 | int32 | showBulletinBoard step counter, loops while < 20 | 0x0046e9a1..0x0046e9af, 0x0046f199, 0x0046f1b4, 0x0046e7ec -> other_stream=E |
| g_staticGuard839340 | 0x00839340 | uint8 | bit 0: the object 0x839348 was already destroyed (atexit stub FUN_00473a90) | 0x00473a90..0x00473aa9 -> other_stream=E |
| g_infoPanelTab | 0x00824130 | int32 | drawGolferInfoPanel's tab 0..4 (-1 reset by mainLoop 0x0041ea9b) | 0x0045d843..0x0045d8ff, jump tables 0x0045ddf4 / 0x0045de0c -> other_stream=E |
| g_infoPanelToggle | 0x00824144 | int32 | toggled by a mainLoop key case, gates a drawGolferInfoPanel block | 0x0041e70b..0x0041e718, 0x0045c5b9 [U-0170] -> other_stream=E |

**Correction to stream A**: cluster 1088 is not `int[16] with an end marker`: 0x5a8c38 is int32[20] (the 0x14-dword clears),
0x5a8c34 is its biased base, 0x5a8c74 is a loop end inside it (index 15), and 0x5a8c88 is a separate scalar one past the end.

### 7.7 Surface-object arrays (the `record array 0x2c` and `0x84` / `0x134` clusters): stream D / E

Every one is an array of 0x2c-byte objects built at static-init time by `ehVectorConstructorIterator(base, 0x2c, count,
Surface_ctor 0x00473ab0, unwindFree_4041f0)`; the count is the third push (ehvec.tsv). Stream D's opaque `SgSurfaceObj`
(d.h) is the element type. The odd-looking strides are index arithmetic over groups of Surfaces:

| array | count | clusters | how my clusters land in it |
|---|---|---|---|
| 0x00567390 | 30 | 699..707 | drawGolferInfoPanel: `lea [state*0x2c + 0x567390 + 3k*0x2c]` for k = 0, 2..9, state = (tab == k) from `sete` (0x0045d5b4..0x0045d5d5, 0x0045d94c..0x0045d96e); the source of Surface_stretchBlit |
| 0x0056a948 | 30 | 759..767 | the same indices, passed as `this` of Surface_stretchBlit 0x00473f60 (0x0045d5db..0x0045d5e2) |
| 0x005628e8 | 432 | 689, 690, 691 | index * 0x84 (3 Surfaces): 0x562914 / 0x562940 are elements 1 / 2; 0x562918 is element 1 + 4 (a dword read, 0x00437d9e); loop end 0x5632e0 (element 132) 0x00445088 |
| 0x00587ea8 | 224 | 873, 875, 876, 877 | index * 0x134 (7 Surfaces): 0x587f00 / 0x587f84 elements 2 / 5; 0x587e7c and 0x587c14 are biased bases (elements -1 and -15) |
| 0x0058a538 | 72 | 879 (0x58a538) | indexed by mainLoop 0x004122ee |
| 0x0059df20 | 50 | 979 | 0x59df4c element 1; ends exactly at g_flags |
| 0x0059e7c0 | 120 | 993 | ends exactly at g_storyPairs 0x59fc60 |
| 0x0059ca58 | 80 (stream D row) | 967, 968 | 0x59cab0 element 2, 0x59cd18 element 16 |
| 0x005612e8 | 39 | 680 (0x561970) | 0x561970 element 38, drawPanelS2's `this`; 0x56199c is the loop end loadInterfaceArt compares (0x00446246) |

The two families the brief asked about (0x567390.. and 0x56a948.., spaced 0x84) are therefore **parallel Surface arrays of
30, used as 10 button slots of 3 Surfaces each** by drawGolferInfoPanel: stream A's single-address clusters are the slots 3k
it addresses with a constant displacement. Which Surface of a 3-group is which visual state: U-0174 (stream E).
0x587ea0 (in cluster 875) is not a Surface: a dword drawWindow writes (0x0040d3a5) and mainLoop reads (0x0041f318) [U-0172].

### 7.8 Clusters that are other streams' data

- 1340, 1341, 1454: elements 0, 1 and 140 of stream D's g_images (0x81ca10, SgCache58[190]); fields +0 / +4 accessed with
  `idx*8` index forms (model_check `stride` warnings, 9, in composeGolferSprite / drawQueuedSprite / buildSceneSprites:
  the index register is pre-multiplied by 11).
- 680 0x5619a0: g_tileWallMask, int8 at x * 50 + y with (1 << k) bit tests (wallHeight 0x0044933e..0x00449356) -> C. Byte 0
  also takes bit-2 / bit-3 writes (U-0171).
- 665 0x55eb40: g_tileScreenX int16[2500], the x half of stream D's tileToScreen cache (g_tileScreenY 0x55fec8 follows,
  0x1388 bytes later) -> D.
- 559 0x53ca8a: not an object: `mov ax, [ebp*2 + 0x53ca8a]` (0x00412838) is the (x-1, y-1) neighbour in the tile-word grid
  0x53caf0 (0x66 bytes = 51 words before it) -> C.
- 1057 0x5a59fc (camera cache) -> D; 1612 (info panel), 710 0x567a18 (tutorial), 1651 -> E; 1058 preferences -> F;
  1001 course type and 1088's g_objectTypeLevel -> C.

### 7.9 model_check

`types.tsv` (39 ranges, including the Surface arrays as `0:uint[11]` stride 0x2c, g_images as `0:uint[22]` stride 0x58,
and stream C's grid 0x53caf0 for the biased access): **0 errors, 128 warnings** (`check.tsv`).
- 81 `width_narrow` on g_tileWords53caf0_C: byte tests of stream C's tile words (not mine).
- 18 `width_narrow` on g_date: `test byte [0x834170], k` and `mov al, [0x834170]` reads of the low tick bits; correct.
- 8 `width_block` + 2 `stride` on g_playerSkills: the 4-dword row copies and the `<< 4` row index (section 4).
- 9 `stride` on g_images (section 8).
- 10 `base_outside` on the Surface arrays: biased bases and neighbouring arrays (0x587e7c = element -1, 0x561260 =
  element -3 of 0x5612e8 in showPopup 0x0046daaa, 0x59e708 = element 46 of 0x59df20 attributed to the next range by the
  margin, and 0x567390 / 0x5678b8 / 0x59e7c0 / 0x58a538 being the start of the next array). No out-of-bounds access found in
  my ranges.
The first run (g_playerSkills as char[32]) had 8 `width_cross` errors, fixed by the two-row struct.

### 7.10 Hook cross-check (shim/src/re/*.cpp literal addresses)

Widths agree everywhere: g_date `G<int>` (46) plus `G<unsigned char>` (20) for the low-byte tests; g_flags `G<unsigned>` /
`G<int>` / `G32` and `G<unsigned char>` on its bytes; g_rng passed as a pointer (`t<void*>`, 37); g_courseType 8-bit
(`G<unsigned char>` 16, `G<signed char>` 12, `G<char>` 3: signedness mixed, the binary is signed); g_playerSkills
`t<signed char*>` (11); g_playerGolfer `G<int>` (60); g_preferences `G<unsigned char>` (byte tests) and `G<int>`;
g_shotFlags5a7140 / g_messageTimer `G<int>`; g_textBuf / g_infoPanelNumBuf `t<char*>`; g_infoPanelTab `G<int>` (81);
g_commentTone `G<unsigned>` / `G<int>`; g_objectTypeLevel `G<int>` / `t<int*>`; g_tileWallMask `G<unsigned char>` and
`G<signed char>` (the binary uses `movsx` at 20 of 27 reads). No disagreement in width or stride.

### 7.11 Cluster coverage

| cluster | typed as | note |
|---|---|---|
| 1633 | g_date | |
| 1608 | g_rng (Random) | plus g_rngSeedTime 0x51b380 (no cluster of mine) |
| 992 | g_flags | |
| 1058 | g_preferences | other_stream=F |
| 1070 | g_shotFlags5a7140, g_messageTimer | |
| 1056 | g_playerGolfer | |
| 1057 | g_lastCameraRot | other_stream=D |
| 1059 | g_playerSkills | |
| 879 | g_textBuf, g_surfaces58a538 | Surface array other_stream=D |
| 1340, 1341, 1454 | g_images (stream D row) | other_stream=D |
| 1612 | g_infoPanelTab, g_infoPanelNumBuf, g_infoPanelToggle | other_stream=E |
| 1001 | g_courseType | other_stream=C |
| 1088 | g_objectTypeLevel, g_frameMs | g_objectTypeLevel other_stream=C |
| 882 | g_commentTone | |
| 680 | g_surfaces5612e8 (0x561970), g_value56199c, g_tileWallMask | grid other_stream=C |
| 1651 | g_bulletinAnimTick, g_staticGuard839340 | other_stream=E |
| 914 | g_nameLabelCount | other_stream=D |
| 873, 875, 876, 877 | g_surfaces587ea8 (+ g_value587ea0) | other_stream=D / E |
| 979 | g_surfaces59df20 | other_stream=D |
| 993 | g_surfaces59e7c0 | other_stream=D |
| 967, 968 | g_surfaces59ca58 (stream D row) | other_stream=D |
| 699..707 | g_surfaces567390 | other_stream=E |
| 759..767 | g_surfaces56a948 | other_stream=E |
| 710 | g_tutorialPage, g_eventFlags567a1c | page other_stream=E |
| 492 | g_itoaBuf4e9a70 (existing name) | |
| 559 | biased base of stream C's grid 0x53caf0 | not an object, other_stream=C |
| 665 | g_tileScreenX | other_stream=D |
| 689, 690, 691 | g_surfaces5628e8 | other_stream=E |

All 52 clusters are typed (none left untyped); 40 of them lie wholly and 4 (879, 1088, 680, 710) partly in another stream's object (Surface arrays and g_images to
D / E, grids to C), typed here with the counts the constructor calls give.

## 8. Simulation odds and ends and initialised-data globals (group gb5)

Files: `gb5.h` (structs BulletinEvent, SkillMaskBlock, Animal and constants), `types.tsv` (34 typed ranges), `check.tsv`
(model_check output), `addresses.tsv` (38 rows), `names.tsv` (26 new names), `uncertain.tsv` (U-0185..U-0191). Scratch
tools: `rng.py` (disassembly by range), `imm.py` (every instruction holding a constant in a range), `calls.py` (call sites
with context), `dd.py` (initial data), `animals.tsv` (record_fields.py output).

model_check: **0 errors, 11 warnings** (all explained in section 9).

### 8.1 g_bulletinEvents 0x004c1578: 22 x 0x30 (save field 71)

Stream A: scalar array, stride 4, count 264. sve.py calls the field `challenge_text` from its initial data. It is a record
table: save block (0x4c1578, 0x420) pushed at 0x0040b377; the new-game reset clears 0x108 dwords from 0x4c1578
(0x0043163b..0x00431645); postEvent 0x0046e7b0 addresses record `id` as `lea eax, [ecx + ecx*2]; shl eax, 4` = id * 0x30
plus 0x4c15a0 / 0x4c15a4 (0x0046e7c0..0x0046e7f6); showCharacterIntro and showBulletinBoard walk +0x28 from 0x4c15a0 in
steps of 0x30 to 0x4c19c0 (0x00406626..0x0040662e, 0x0046ea70..0x0046ea7a): (0x4c19c0 - 0x4c15a0) / 0x30 = 22, and
0x4c1578 + 22 * 0x30 = 0x4c1998 = the save block end. The callers push ids 0..0x15 (0..21; showTournamentResults adds 0x11 to
a value at 0x0045aba7). So the record starts with the text: the save block and the clear both start at 0x4c1578, 0x28 bytes
before the first date.

| off | width | sign | meaning | evidence |
|---|---|---|---|---|
| +0x00 | char[0x28] | | file-default text (22 NUL-terminated strings in the image, 14..35 characters). No game-logic instruction reads it; the new-game reset zeroes it, so after a new game it stays zero (all 8 saves read hold zero text) | clear 0x00431645; no other reference in 0x4c1578..0x4c1997 (ctx.py over the range) |
| +0x28 | 4 | s | g_date when the event was first posted, 0 = not posted. postEvent posts only when 0 and flag 0x4000000 of 0x59e7b8 is clear | r 0x0046e7c6, w 0x0046e7e0; showBulletinBoard picks the oldest non-zero (0x0046ea5b..0x0046ea6e) and copies it into g_date to format it (0x0046eaf0..0x0046eafc) |
| +0x2c | 4 | s | value of 0x59bf90 at posting; showBulletinBoard puts it back into 0x59bf90 before appendCourseTitle 0x0040daa0 (0x0046eaf6..0x0046eb08), so it is the course whose title the entry shows | w 0x0046e7f6, r 0x0046eaf6 |

postEvent also stores the id in 0x4e3db8 and its arguments in 0x4e3dbc / 0x8392a4 (not mine). Save evidence: stories9.sve
has event 6 posted (date 2632, course 6); the other 7 saves have none.

### 8.2 Initialised scalars written by the game (clusters 134, 135, 161, 166)

| addr | symbol | width | file default | meaning | evidence |
|---|---|---|---|---|---|
| 0x4c284c | g_objectOfferCount | int | 1 | count of accepted kind-5 object offers: mainLoop offers a spawnObject at a placed object of type 5 once its counter +0xc exceeds (300 * n + 400) * ([0x822c88] + 2) and adds 1 after startMessage returns non-zero | 0x00417a9e..0x00417abf, 0x00417bc8..0x00417bfb; reset to 0 at 0x00431b71 (save field 24; 0 in all 8 saves) |
| 0x4c2850 | g_greensFee | int | 5 | fee charged when a golfer finishes a hole, in units of 100: base golfer word +0xa4, doubled when [0x543cf4] == 2, +2 per hole flag bit 1 / 2, + [0x543cd8], +2 / +5 by the golfer type's level; added to the cash global 0x571fd4 (cash_div100) and the hole record +0x1f4; formatted times 100 | 0x00427465..0x004274f4, 0x004276a8..0x004276c0, 0x0042757e..0x0042759a; holeMagazineEvent +1 per featured hole 0x0042e5e2..0x0042e5fb / 0x0042e720..0x0042e736 (save field 35; 6..10 in the 8 saves) |
| 0x4c2e1c | g_shotGolferPrefs | uint32 | 7 | byte +0x19 (preference bits) of the golfer whose shot chooseShotTarget plans; 7 again when it returns | 0x00422fdb (xor ecx), 0x00422ff3 (mov cl), 0x00423001; 0x004240d8; trialShot tests bit 2 at 0x00422a9a / 0x00422b28 / 0x00422df3 |
| 0x4c2e20 | g_panelSelectPending | int | -1 | moved into the mainLoop frame slot F+0x14 and reset; every store writes -1 (U-0188) | 0x0041988c..0x00419892; 0x0040d9b5..0x0040d9cc |
| 0x4c2e24 | g_demolishConfirmX | int | -1 | tile x of the pending demolish confirmation (y: 0x5a6368). A first click queues a label and stores the tile; a second click on the same tile calls demolishObject 0x0040a4e0 and clears it | 0x0041f1bc..0x0041f201, 0x0041f223..0x0041f284 |
| 0x4c2e28 | g_landingAnimFrame | int | -1 | frame of the one-shot animation drawn where a ball came to rest on a tile whose typeAtPos is 0x11 (U-0190): 0 at the landing, sprite cellAt(0xf8, n, 0) at 0x5787fc / 0x578800, +1 per frame, -1 at n >= g_sheetCols[0xf8] (0x53f7c8) | 0x0042d0c2..0x0042d0fd; 0x00415973..0x004159ed; -1 at new game 0x00431b97 |
| 0x4c2f9c | g_cameraGoalX | int | -1 | camera goal tile x (goal y 0x5685fc): while not -1 the camera 0x4c2ba0 / 0x4c2ba4 moves one sign() step per call toward it; set to the midpoint of a new hole's end tiles; -1 when the edit queue ends | 0x00409cf0..0x00409d50, 0x0040a05f..0x0040a0db, 0x00409d87 |
| 0x4c2fa0 | g_heightRelief | int | 16 | terrain relief amplitude: generateRandomCourse picks 0x30 / 0x20 / 0x10 by a switch, adds half, adds 0x10 under flag 0x1000000; heightBlend multiplies (16 - d) by it | 0x00470aaf..0x00470b35; 0x0040c290..0x0040c2a9 (stream C) |
| 0x4c2fc8 | g_debugShotGolfer | int | -1 | golfer id compared with the planning golfer; on a match chooseShotTarget formats numbers into 0x51a068. No instruction writes it, so the branch never runs in this build | 0x00423c9e, 0x0042404b |
| 0x4c2fcc | g_shotRedrawGolfer | int | -1 | prepareShot sets -1, or [0x53df54] - 1 when flag 0x20 of 0x59e7b8 is set; when the shooting golfer equals it, the window is refreshed and redrawn before the shot | 0x00424167..0x0042417f, 0x0042452f..0x0042454a |

Stream A's cluster 161 class (scalar, 4 addresses) is right. Cluster 166 (scalar array, stride 4, count 28, end marker) is
wrong: 0x4c2f28..0x4c2fcf holds seven objects (this table plus the three constants of section 3).

### 8.3 Constant tables inside cluster 166 (never written; other streams)

| addr | symbol | type | meaning | evidence | stream |
|---|---|---|---|---|---|
| 0x4c2f28 | g_maskSpriteTable | {int16 flic; int16 variant}[16] | sprite for a 4-bit neighbour mask, the mask rotated right by camera rotation 0x5685f4 / 2; flic ids 0x1a2..0x1a7, variants -1..3 | 0x0041272d..0x00412752, 0x00413324..0x00413356 | F |
| 0x4c2f68 | g_levelFalloffKernel | int16[5][5] | FUN_00406f20 raises each cell of the 5x5 block of the level grid 0x5a4998 (row stride 0x33) to at least centre - k, FUN_00406f90 lowers it to at most centre + k; centre entry 0 | 0x00406f48..0x00406f7f, 0x00406fb8..0x00406fed; end 0x4c2f9a | C |
| 0x4c2fa4 | g_soundVariationCycle | int8[36] | playSoundAt's per-call variation (v - 5) * 100, index = counter 0x5a9cdc mod 36 | 0x0040c661..0x0040c688 (U-0189) | D |

### 8.4 g_skillMaskBlock 0x004c2c94 (save field 59, 0x12 bytes)

Only +8 is addressed (19 references, all `word ptr [0x4c2c9c]`). g_playerSkillMask = the skill mask of the player's golf
pro: bit i set iff skill byte [0x5a5a04 + i] != 0 for i < 12 (customizeCharacter 0x0040f1a5..0x0040f1cd, mainLoop
0x0041cebd..0x0041cedf, loadGolfPro 0x0046cf50..0x0046cf80), copied into golfer record +0x16 (0x0040f51b..0x0040f547; record
1 at 0x0046cf8c), bits set / cleared by skillPointDialog 0x0045f7b3 / 0x0045f7fa and returned sign-extended 0x0045f844;
narrateShot ORs a bit 0x004090bd; the new-game reset stores 0 (0x004316e6). The other 16 bytes (U-0185) are unreferenced and
hold their file defaults in all 8 saves read.

### 8.5 g_animals 0x0056d1b8: 100 x 0x3c (save field 58), clusters 776 / 777

The 0x20 bytes from 0x56d1b8 are **part of record 0**, not a header: spawnObject 0x004011e0 computes `i*3*5*4` (0x004011ff,
0x00401207, 0x00401213) and stores the dword `[i*0x3c + 0x56d1b8] = 0` (0x0040121b..0x00401221) for the slot it fills;
the save block is exactly 100 * 0x3c = 6000 bytes from 0x56d1b8; the slot scans run a pointer from 0x56d1d8 (= +0x20) to
0x56e948 = 0x56d1b8 + 100 * 0x3c + 0x20 (0x004011f1, 0x0040175d, 0x00401c1c) and from 0x56d1e0 (+0x28) to 0x56e950
(0x004011d6). The record base is 0x56d1b8; the names row g_animals 0x0056d1d8 names field +0x20 (correction for the names
row and for re/analysis/sim/004017d0_placeAnimal.md, which use 0x56d1d8 as the base; offsets there are 0x20 lower).
Stream A's stride 0x14 for cluster 776 comes from a partial multiplier: its tracker sees `lea edi, [eax + eax*4]; shl edi, 2`
(5 * 4) and misses the `lea eax, [eax + eax*2]` before it; model_check prints the same stride 0x14 warning (section 9).

| off | width | sign | meaning | evidence |
|---|---|---|---|---|
| +0x00 | 4 | | zeroed when the slot is filled, never read (U-0186) | w 0x00401221 |
| +0x04..+0x1f | | | not addressed (U-0186) | |
| +0x20 | 2 | s | flic id base: the signed byte of the species table 0x55d758 (stride 0x25) for the species index; -1 = free slot | w 0x00401232 (from 0x0040120a), 0xffff at 0x00401755 (initRecords) and 0x004011cd (freeRecordAtPos); tests 0x004011e7, 0x00401c09 |
| +0x22 | 2 | s | walk-map id: first spawnObject argument (mainLoop passes Random::range(6), 0x00417bf9..0x00417c17); selects the 0x74-byte map in 0x4e6d70 | w 0x0040122b; r 0x00401af0 |
| +0x24 | 2 | | not addressed | |
| +0x26 | 2 | s | tile x (third argument) | w 0x0040123e; r 0x004017eb, 0x00401861, freeRecordAtPos 0x004011be |
| +0x28 | 2 | s | tile y (fourth argument) | w 0x0040124c; r 0x00401803, 0x0040186e, 0x004011c6 |
| +0x2a | 2 | s | wait countdown: >= 0x80 hides the sprite (0x0040183a jge); off screen a 1-in-500 roll sets 0x1f4 (0x004019d4..0x004019e8); decremented each tick (0x00401a11); when 0 at frame 0 outside idle / walk it gets Random::range(64) + 64 (0x00401a3d..0x00401a55) | 0x0040183a, 0x004019e8, 0x004019f1, 0x00401a11, 0x00401a55 |
| +0x2c | 2 | s | animation offset added to +0x20 for the flic id: 0x140 idle (spawn 0x00401255, 0x00401a00, 0x00401a44, 0x00401be4), 0x14d after the call sound (0x00401b5a, playSoundAt(+0x20 + 0x82) at 0x00401b40..0x00401b55), 0x15a walking (0x00401b77) | r 0x0040180a, 0x00401a2a, 0x00401a6a |
| +0x2e | 2 | | not addressed | |
| +0x30 | 4 | s | x offset inside the tile, 1/1024 tile: spawn (k * 1024 + 512) / 2 for a random cell k < 4 (0x0040129f..0x004012ae); world x = (+0x26 << 10) + it (0x004017fe..0x00401801); (v * 2) >> 10 is the 0..3 cell of the 4x4 walk map (0x00401a9a..0x00401aa8); a walk step adds dx[heading] << 10 >> 5 (0x00401b80..0x00401ba2) | |
| +0x34 | 4 | s | y offset, same rules | 0x004012c3, 0x00401814, 0x00401a94, 0x00401bca |
| +0x38 | 1 | s | heading 0..7: turned by Random::range(3) - 1 and `and 7` (0x00401aab..0x00401ac8); indexes the direction tables 0x4c2878 / 0x4c2898 (0x00401ace, 0x00401b87); also the sprite rotation (camera 0x5685f4 - heading - 2) & 7 (0x00401884..0x004018a4) | |
| +0x39 | 1 | | walk steps left: 0x10 when the next map cell is 0xff (0x00401b13), -1 per step, back to idle at <= 0 (0x00401bd0..0x00401be4) | |
| +0x3a | 1 | s | frame: (frame) mod g_sheetCols[flic] (0x00401892..0x004018ad), +1 per drawn tick and 0 at the frame count (0x004018ed..0x00401909); 0 when the sound animation starts (0x00401b66) | |
| +0x3b | 1 | | not addressed | |

All 8 saves read hold no live record (every +0x20 is -1).

### 8.6 Path finding (clusters 921, 1072, 1083) and the two shot-target scalars (1084, 1086)

| symbol | type | evidence |
|---|---|---|
| g_pathGrid 0x58d370 | uint8[50][50], index x * 50 + y, cost / visited byte | clear 0x271 dwords 0x0042e7f3..0x0042e7ff; start cell 1 at 0x0042e93f; byte reads movzx-style (xor edx; mov dl) 0x0042e99e..0x0042e9a4; stores 0x0042eb10; loop end 0x58dd34 at 0x0042ec11 |
| g_pathQueueX 0x5a7404 | int16[0x400] ring | movsx reads 0x0042e972 with index `and eax, 0x3ff` 0x0042e987; stores 0x0042e90d, 0x0042eb16 |
| g_pathQueueY 0x5a8028 | int16[0x400] ring | 0x0042e918, 0x0042e97a, 0x0042eb1e; ends at 0x5a8828 |

0x5a8828 / 0x5a8830 are **not** path-finding state (the brief listed them with pathStep): they are the planned shot target in
world units (1/1024 tile, +0x200 = tile centre), g_shotTargetX / g_shotTargetY: chooseShotTarget copies 0x56a78c / 0x56a790
(0x00423c5e..0x00423c82), prepareShot stores golfer +0xcc / +0xd0 << 10 + 0x200 (0x00424557..0x00424589), and mainLoop's
probe reads both after prepareShot (0x004136aa / 0x004136af). The dword between them, 0x5a882c, is save field 7
(length_skill_x100), unrelated.

### 8.7 g_sheetCols 0x53f3e8 and the mainLoop frame-sequence `tables` (clusters 571..578, 587)

Stream D already names 0x53f3e8 `g_sheetCols` (re/names/model_d_globals.tsv): int32[0x24e], cleared by `mov ecx, 0x24e; rep
stosd` (0x00442358..0x00442364), filled by loadFlic passing &[id] to flicOpenStream (0x0043d8a6..0x0043d8b8, 0x0043d95f),
bound-checked by cellAt (0x0043d706..0x0043d713). Every address of clusters 571 (0x53f4d8..0x53f500), 572 (0x53f5a0), 573
(0x53f758), 574 (0x53f7c8..0x53f7dc), 575 (0x53f8a4..0x53f8ac), 578 (0x53fa0c..0x53fa6c) and 587 (0x53fcc4..0x53fd0c) is an
element of it: ids 60, 70, 110, 220, 248..253, 303..305, 393..417 and 567..585, all below 590. Stream A's int[25] for
0x53fa0c..0x53fa6c is a span of elements 393..417, not an array of its own. Correction for golfer_record.md +0x1e: `wrapped
with tables 0x0053f500 / 0x0053f4d8 / 0x0053f758 and capped at dword 0x0053f3e8[base + k] - 1` are all one table:
`[ebx*4 + 0x53f500]` = g_sheetCols[70 + ebx], `[ebx*4 + 0x53f4d8]` = g_sheetCols[60 + ebx], `[ebx*4 + 0x53f758]` =
g_sheetCols[220 + ebx] (0x0041516f, 0x0041519b, 0x00415292), i.e. the frame counts of the golfer animation sheets. One
narrow read: updateWalkers takes the low byte of g_sheetCols[400] minus 3 into a byte (0x00403455..0x0040345e).
Owned by D for typing; the row in addresses.tsv repeats D's type for these clusters.

### 8.8 Other sim scalars and arrays

| addr | symbol | type | meaning | evidence |
|---|---|---|---|---|
| 0x53ea24 | g_ballStopCount (existing) | uint8[2500] | per-tile count of balls that stopped there (+1 below 0xff, tile not blocked); mainLoop decays it b - (b >> 3) with the 0x5a6378 / 0x56c7e4 grids; spreadAllShade sums it; save field 68 | 0x0042d57d..0x0042d5a0, 0x00418280..0x0041828e, 0x004616f6..0x00461713; clears 0x004316ec, 0x0044226e, 0x00470bf9 |
| 0x53fd20 | g_shotPowerCache | int32[10] | value side of shotPower's 10-entry cache (keys 0x5a47b8 / 0x5685c8, ring index 0x5a9ce0 mod 10); starts right after g_sheetCols' last element | 0x0042243b..0x004224d9, 0x00422505..0x00422520 |
| 0x54131c | g_panelListIds | int32[<=153] | read as a golfer index (i < [0x56199c]) and as a hole by FUN_0040d890; written by nothing (U-0187) | 0x0040d908..0x0040d99a |
| 0x541a10 | g_probeType1Strokes | int32 | mainLoop's hole probe (synthetic golfer record 154, four passes with byte +0x19 = 6 / 5 / 3 / 7, five strokes each): strokes + 3 when the probe ball first lies on tile type 1, else 0; read only to test 0 | 0x004135a0, 0x0041387c..0x004138a0 |
| 0x542dfc / 0x542e24 | g_probeShotX / g_probeShotY | int32[10] each | tile of each probe stroke's target; pointer from [pass * 4 + base], +4 per stroke, so slots overlap across passes (pass + stroke <= 3 + 4 = 7); written when flag 0x40000 of 0x59e7b8 is clear, replayed when set | 0x00413674..0x004136fc, loop 0x004138b2..0x004138fc |
| 0x568d0c | g_nearTileDist (existing) | int32 | running minimum distance; nearestPlaced seeds 0xffff, findNearestTargetTile seeds maxDist << 10 | 0x0040ddcb..0x0040de3e, 0x0040de91..0x0040df34 |
| 0x56a91c / 0x56a920 | g_nearTileX / g_nearTileY (existing) | int32 | best tile (x = -1 when none) | 0x0040de97, 0x0040df39, 0x0040df3f; updateGolfers 0x0042a31b / 0x0042a328 |
| 0x56c794 | g_floatLabelY | int32[8] | world y of the 8 floating labels (x 0x56c770, colour 0x56a794, ttl 0x56a924, text 0x56c570 [8][0x40]; re/match/golf_small11.cpp declares them) | 0x0040c73e, 0x0040c7bc, 0x0040c872 (stream D) |
| 0x56c7b4 | g_forcedEventBits | flags32 | one-shot requests ORed from 0x543cf8 when g_date == [0x56a518] + 0x10 (0x0041c150..0x0041c175); each consumer acts and clears its bit: 0x01 addGolferToTable emits thought 0x18 (0x00409af6..0x00409b19); 0x02 holeMagazineEvent forces an event (0x0042e2a2..0x0042e2c3); 0x04 mainLoop advances a story pair and sets golfer flag 0x200000 (0x0041c3c6..0x0041c541); 0x08 mainLoop sets the nearest golfer's mood to -1 and emits thought 8 (0x0041c388..0x0041c3b8); 0x10 scanShotArea narrates (0x004098fe..0x00409936); 0x40 mainLoop sets prefs 7, skill rating 0x1f4 and g_date >= 0x2001 (0x0040fef3..0x0041038e); 0x80 mainLoop sets a kind-5 object counter to 0x9c4 (0x00417a58..0x00417a6b) | |
| 0x569528 | g_unreadText569528 | char[0x80] | copy of string 0x4c6bf0 made at mainLoop entry, never read (U-0191) | 0x0040f9a0..0x0040f9c0 |

### 8.9 model_check (types.tsv -> check.tsv): 0 errors, 11 warnings

- 9 `stride` warnings on g_animals in spawnObject: the tracker reports multiplier 0x14 where the code multiplies by 3, 5
  and 4 (0x004011ff, 0x00401207, 0x00401213) = 0x3c. Same mis-tracking as stream A's stride 0x14 vote; type kept.
- 1 `width_narrow` on g_sheetCols at 0x00403455 (updateWalkers reads the low byte of element 400): a truncation in the
  original, the int32 type stands.
- 1 `sign` on g_skillMaskBlock.player_skill_mask at 0x0045f844 (skillPointDialog returns it with movsx); every other
  access is a mask operation and the bits run 0..11, so unsigned stays.
Ranges declared const (g_maskSpriteTable, g_levelFalloffKernel, g_soundVariationCycle, g_debugShotGolfer,
g_panelListIds) raised no const_write.

### 8.10 Hook cross-check (shim/src/re/*.cpp literals inside these ranges)

Widths agree everywhere: 0x4c2c9c is `G16` / `G<unsigned short>` (c3em, c3fb, c3fy, c3em_negctl), 0x4c2e1c / 0x4c2e2x /
0x4c284c / 0x4c2f9c / 0x4c2fcc / 0x5a8828 / 0x5a8830 / 0x541a10 are 32-bit (c3ea, c3ed, c3fy), 0x53ea24 bytes (c3ec, c3fy),
0x578732 `unsigned char` (c3ea), 0x568d0c / 0x56a91c `int` (c3a, c3j, c3ec). Two descriptions differ from this model, not
in width: c3em / c3q comment the bulletin table as 22 x 0x30 records at 0x4c15a0 starting with the date (the same bytes;
the save and clear extents put the record start at 0x4c1578, text first), and c3q / the placeAnimal note use 0x56d1d8 as the
animal record base (section 5).

### 8.11 Corrections to older notes and names

- names row `g_animals` 0x0056d1d8: the record base is 0x0056d1b8 (section 5); its field list (+0 type, +2 map, +6 / +8 tile)
  is offset by 0x20. Not renamed here (rule); the lead or stream G moves it.
- sve.py `NAMES[0x4c1578] = challenge_text`: the field is the 22 bulletin-event records; the text part is never read and is
  zeroed by the new-game reset.
- `buildGolferBioText` 0x004315e0 is the new-game state reset: one run of `rep stosd` clears ~20 tables (golfers, walkers,
  placed objects, bulletin events, ball-stop counts ...) and sets defaults (0x00431621..0x00431ba1); the name does not fit
  (for the names owner).
- golfer_record.md +0x1e `tables 0x0053f500 / 0x0053f4d8 / 0x0053f758`: elements of g_sheetCols (section 7).
- the brief's path-finding list held 0x5a8828 / 0x5a8830: shot target (section 6).
- the brief's `int table with an end marker` 0x4c2f28..0x4c2fcc: seven separate objects (sections 2 and 3); the
  showTitleScreen compare `cmp eax, 0x4c2852` (0x0043d24b) is the end of the empty loop over the 20 x 0x14 amenity table
  0x4c26b0 (start 0x4c26c2 at 0x0043d242), not a marker inside g_greensFee.

### 8.12 Defects and dead paths found (catalogued, not fixed)

- g_debugShotGolfer is never written: chooseShotTarget's formatting branch (0x00423cb3..) never runs.
- g_panelSelectPending only ever receives -1; g_panelListIds and its bound 0x56199c are never written, so FUN_0040d890's
  list branch reads zero (U-0187, U-0188).
- holeMagazineEvent's +1 on g_greensFee is overwritten by the next matchUpdate computation before any read but the save.
- g_probeShotX / Y slots overlap between probe passes (slot = pass + stroke).
- the bulletin texts are file data that the new-game reset erases; nothing reads them.

### 8.13 Cluster coverage

| cluster | typed as | note |
|---|---|---|
| 121 | g_bulletinEvents (BulletinEvent[22]) | |
| 134 | g_objectOfferCount | |
| 135 | g_greensFee | |
| 151 | g_skillMaskBlock (+ g_playerSkillMask at +8) | 16 bytes U-0185 |
| 161 | g_shotGolferPrefs, g_panelSelectPending, g_demolishConfirmX, g_landingAnimFrame | |
| 166 | g_maskSpriteTable (F), g_levelFalloffKernel (C), g_cameraGoalX (C), g_heightRelief (C), g_soundVariationCycle (D), g_debugShotGolfer, g_shotRedrawGolfer | |
| 568 | g_ballStopCount | existing name |
| 571, 572, 573, 574, 575, 578, 587 | elements of g_sheetCols (D's name and type) | other_stream=D |
| 588 | g_shotPowerCache | |
| 611 | g_panelListIds | count upper bound, U-0187 |
| 616 | g_probeType1Strokes | |
| 637 | g_probeShotX, g_probeShotY | |
| 730 | g_nearTileDist | existing name |
| 740 | g_unreadText569528 | other_stream=F (text buffer) |
| 744 | not typed separately: tile_byte[x][y-1] read of the save grid 0x56988c | other_stream=C |
| 756, 757 | g_nearTileX, g_nearTileY | existing names |
| 772 | g_floatLabelY (D), g_forcedEventBits | two objects |
| 776, 777 | g_animals (Animal[100] at 0x56d1b8) | one object |
| 818 | not typed separately: g_typeAttr[20] byte (0x578372 + 20 * 0x30), owned by C (ownership override) | other_stream=C |
| 921, 1072, 1083 | g_pathGrid, g_pathQueueX, g_pathQueueY | existing names |
| 1084, 1086 | g_shotTargetX, g_shotTargetY | |

32 of 32 clusters accounted for: 30 typed in types.tsv, 2 (744, 818) left to stream C as parts of its grids / tables.
