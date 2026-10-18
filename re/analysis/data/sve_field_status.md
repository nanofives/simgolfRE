# .sve field status (phase 1 stream G2, 2026-10-18)

Status of every field of the save format after the phase-1 streams, from `re/model/sve_fields.tsv` (built by
`log/p1/g2/build_sve_fields.py`, scratch, from `re/tools/sve.py` FIELDS / TAIL_A / TAIL_B and the six
`re/model/*_addresses.tsv`). Fields are mapped onto model rows by address, not through the streams' `sve_field` column
(that column has four spellings: stream B and C write the 0-based FIELDS index, B also `tailB0` / `tailB1`, E2 writes
the VA, F writes `24,35` and `tailA`). Report and round-trip results: `re/analysis/data/model_sve.md`.

## 1. Counts

The format has 89 serialized globals: 86 written unconditionally by FUN_0040afa0 (0x0040afa0..0x0040b492), tail A
(flag 0x40000000 of field 70) and the two tail-B blocks (flag 0x00200000), plus the 100-byte header (two for
championship files). Before phase 1, `re/tools/sve.py` NAMES named 20 of the 89 and **69 were unnamed** (U-0004).

| status now | fields | of the 69 unnamed |
|---|---|---|
| named: one model row covers the field exactly (84) or the field is a whole-record slice of an array row (5) | 83 | 63 |
| placeholder: a row exists but its name is its own address (`g_sve561250`, `g_sve5a6364`, `f_w_4c2e14`, `g_flagWords5a46b8`) | 4 | 4 |
| unnamed: no row in any stream (clusters 779 and 969, stream X in `re/model/ownership.tsv`) | 2 | 2 |

So 63 of the 69 are now named; the 6 left are listed in section 3, each with a proposed row for stream G1. All 20
fields sve.py named keep a model row; four of them changed name, not meaning: `tile_byte` -> `tile_variant` (C),
`challenge_text` -> `g_bulletinEvents` (B: 22 records of 0x30, the initial text is record 0), `golfer_types` ->
`g_golferTypes[0:76]` (B: the table has 84 records; the field saves 0..75), `records` -> `g_bestRounds` (B).

Slices (field = elements k..n-1 of a larger model array, element boundaries checked):

| field | model symbol | why |
|---|---|---|
| H0, H1 | `g_textBuffer[0:100]` (E2, char[0x1000]) | FUN_0040b4a0 writes the first 100 bytes of 0x0051a068 whole (0x0040b654) |
| 46 | `f_w_4c2e14[0:1]` (F, int32[2]) | only the first dword is saved (section 3) |
| 63 | `g_golferTypes[0:76]` (B, GolferType[84]) | 0x4d6088 + 0xa640 = 76 x 0x230 |
| tailA | `g_golferTypes[76:80]` | 0x4e06c8 = record 76, 0x8c0 = 4 x 0x230; records 80..83 (0x4e0f88..0x4e1848) are never saved |

## 2. The 63 fields named since U-0004

| field | va | size | model symbol | stream | element type |
|---|---|---|---|---|---|
| 0 | 0x00822c78 | 0x4 | `course_id` | c | int32_t (0..0xffff) |
| 10 | 0x005a9cac | 0x4 | `g_awardHoleHonours` | b | int32_t |
| 11 | 0x005a9cb0 | 0x4 | `g_awardLots` | b | int32_t |
| 12 | 0x005787cc | 0x4 | `g_awardTournamentPoints` | b | int32_t |
| 13 | 0x00561258 | 0x4 | `g_awardStories` | b | int32_t |
| 15 | 0x0059bf90 | 0x4 | `g_courseIndex` | e2 | int32_t |
| 16 | 0x00575ab0 | 0x28a0 | `holes` | c | HoleRecord |
| 19 | 0x0053caf0 | 0x1388 | `tile_flags` | c | uint16_t [50][50] (TF_*) |
| 20 | 0x0053bbac | 0x9c4 | `tile_path_cost` | c | int8_t [50][50] |
| 21 | 0x005a4998 | 0xa29 | `corner_level` | c | uint8_t [51][51] |
| 22 | 0x00838c1c | 0x169 | `sample_grid` | c | int8_t [19][19] (17x17 used) |
| 24 | 0x004c284c | 0x4 | `g_objectOfferCount` | b | int32 |
| 27 | 0x00585850 | 0x1300 | `g_walkers` | b | Walker |
| 30 | 0x0053a450 | 0x4 | `g_landPurchaseCount` | b | int32 |
| 31 | 0x00822c88 | 0x4 | `g_difficulty` | b | int32_t |
| 32 | 0x005409ac | 0x3e8 | `g_skillHistory` | b | int16_t |
| 33 | 0x0051b388 | 0x3e8 | `g_cashHistory` | b | int16_t |
| 34 | 0x0053e63c | 0x3e8 | `g_funHistory` | b | int16_t |
| 35 | 0x004c2850 | 0x4 | `g_greensFee` | b | int32 |
| 36 | 0x0059b730 | 0x4 | `g_challengeLevel` | b | int32_t |
| 37 | 0x0059ae7c | 0x4 | `g_nextGolferSlot` | b | int32_t |
| 38 | 0x0059dea0 | 0x80 | `g_golferClassSlotCounts` | b | int32_t |
| 39 | 0x0059c08c | 0x4 | `g_thoughtSeenLo` | b | flags32 |
| 40 | 0x00568600 | 0x3e8 | `g_eventHistory` | b | uint16_t |
| 41 | 0x00822c70 | 0x4 | `g_freeUnlockMask` | b | flags32 |
| 42 | 0x005685f8 | 0x4 | `hole_notice_mask` | c | uint32_t |
| 43 | 0x005a5a24 | 0x4 | `g_lastAllocatedGolfer` | b | int32_t |
| 44 | 0x00584210 | 0x7d0 | `g_financeYears` | b | FinanceYear |
| 45 | 0x00543cfc | 0x4 | `g_amenityUnlockMask` | b | uint32 |
| 47 | 0x00572cac | 0x4 | `g_visitor60Count` | b | int32 |
| 48 | 0x0059aaf8 | 0x4 | `g_ratingMilestone` | b | int32_t |
| 49 | 0x0056ae70 | 0x1700 | `g_golferStats` | b | GolferStats |
| 50 | 0x005422f8 | 0xf0 | `g_courseRanking` | b | CourseRanking |
| 51 | 0x0059ae80 | 0x1c8 | `g_holePoints` | b | HolePoints |
| 52 | 0x0059b048 | 0x4 | `g_lastStoryAdvanceDate` | b | int32_t |
| 53 | 0x005736b0 | 0x2400 | `g_ballRecords` | b | BallRecord |
| 54 | 0x0059fc60 | 0x3880 | `g_storyPairs` | b | ShotTrack |
| 55 | 0x005849e0 | 0xe70 | `g_golferTypeState` | b | GolferTypeState |
| 56 | 0x005a59f8 | 0x4 | `g_playerGolfer` | b | int32 |
| 57 | 0x005a5a04 | 0x20 | `g_playerSkills` | b | int8[2][16] |
| 58 | 0x0056d1b8 | 0x1770 | `animals` | c | Animal |
| 59 | 0x004c2c94 | 0x12 | `g_skillMaskBlock` | b | SkillMaskBlock |
| 60 | 0x005689e8 | 0x320 | `g_scenicDedications` | b | ScenicDedication |
| 61 | 0x005419d0 | 0x40 | `g_skyActors` | d | SgSkyActor |
| 62 | 0x00572cb0 | 0xa00 | `g_habitatSlots` | b | HabitatSlot |
| 64 | 0x00543d10 | 0x19a28 | `g_golferPhrases` | b | GolferTypePhrases |
| 65 | 0x00567b04 | 0x4 | `g_tournamentPurse` | b | int32_t |
| 66 | 0x005a6378 | 0x9c4 | `tile_mood_up` | c | uint8_t [50][50] |
| 67 | 0x0056c7e4 | 0x9c4 | `tile_mood_down` | c | uint8_t [50][50] |
| 68 | 0x0053ea24 | 0x9c4 | `g_ballStopCount` | b | uint8 |
| 69 | 0x00542414 | 0x9c4 | `tile_type_saved` | c | int8_t [50][50] (enum TileType) |
| 72 | 0x00571fd8 | 0x30e | `g_courseTable` | b | CourseRecord |
| 73 | 0x00520640 | 0x3e8 | `g_memberTypeHistory` | b | int16_t |
| 75 | 0x00571d38 | 0x4 | `g_thoughtSeenHi` | b | flags32 |
| 79 | 0x0056a51c | 0x4 | `g_nextArrivalOrder` | b | int32 |
| 80 | 0x005a6374 | 0x4 | `g_debtYears` | b | int32_t |
| 82 | 0x005a47e0 | 0x4 | `g_tournamentPrep` | b | int32 |
| 83 | 0x005830b8 | 0x9c4 | `tile_build_code` | c | uint8_t [50][50] |
| 84 | 0x0059c090 | 0x9c4 | `tile_refund` | c | int8_t [50][50] |
| 85 | 0x005a5a00 | 0x4 | `g_preferences` | b | flags32 |
| tailA | 0x004e06c8 | 0x8c0 | `g_golferTypes[76:80]` | b | GolferType |
| tailB0 | 0x0058f338 | 0xb7c0 | `g_golferTypesBackup` | b | GolferType |
| tailB1 | 0x00520a28 | 0x19a28 | `g_golferPhrasesBackup` | b | GolferTypePhrases |

## 3. The 6 fields left, with the row proposed for stream G1

Stream G2 does not edit the stream files; these rows are for G1's address-map merge (columns of the streams' maps).
Each was read from the instructions (log/p1/g2/g2_refs.py lists every instruction of the listed functions whose operand
falls in a range).

### Field 25: 0x00561250, int32 (E2 placeholder `g_sve561250`, U-0473)
Readers and writers in game code: one store, `mov [0x561250], ebx` at 0x00452dc1 in showCourseReport; otherwise only
the two serializers (0x0040b0f8 save, 0x0040bd54 load). ebx is the report's score: loaded from the frame at 0x00452cbd,
stored only when `test ebx, ebx; jg` (0x00452cc8) passes, converted with `_itoa` and followed by the suffix string at
0x004d2f88 (0x00452d79..0x00452db3); the same value compared with 100 selects postEvent 0x15 (0x00452c4f..0x00452c81).
The labels sit at 0x004d2fb0 (string pushed at 0x00452ca9).
Proposed row: `g_courseReportScore	golf_clean.exe	0x00561250	4	int32_t	1	4	s	25	667	store 0x00452dc1 in showCourseReport when > 0 (0x00452cc8); printed with the suffix at 0x004d2f88; compared with 100 at 0x00452c4f; no other reader	medium`.
Not determined: whether a reader outside the save path exists through a computed address (none in the listed functions).

### Field 29: 0x005a6364, int32 (E2 placeholder `g_sve5a6364`, U-0473)
mainLoop stores 6 at a new game (0x0040fbe7) and 0x11 together with cash 0x2710 / 0x186a0 and the amenity unlock mask
0x3fff (0x0040fc02..0x0040fc0c, 0x00421411..0x0042142f); announceBuilding increments it (`inc edx; mov [0x5a6364], edx`
0x0040efef..0x0040eff0) after comparing the building index with value - 5 (0x0040ee2f..0x0040ee3a); pickBuilding
compares the selection with it (`cmp eax, [0x5a6364]; jl` 0x004342ef); drawBuildingPanel reads it per row
(0x00434703, 0x004347dd); courseActionMenu compares it with 0x11 (0x004706bc..0x004706c6); FUN_0040d890 compares a
table value with it (0x0040d992).
Proposed row: `g_buildingUnlockLevel	golf_clean.exe	0x005a6364	4	int32_t	1	4	s	29	1063	6 at a new game 0x0040fbe7; 0x11 with the amenity mask 0x3fff 0x0040fc02 / 0x00421420; +1 in announceBuilding 0x0040eff0; bound of pickBuilding 0x004342ef and drawBuildingPanel 0x00434703	medium`.

### Field 46: 0x004c2e14, int32 (F placeholder `f_w_4c2e14`, int32[2])
A golf-pro index, -1 = none. mainLoop reads the g_golfPros record (0x0058dd50, stride 0x38) of a frame index and tests
its byte +0x20 against 0xff (0x0040ffe1..0x0040fff8), continues only when [0x4c2e14] == -1 (0x0040fffe), builds a prompt
in 0x0051a068 with the pro's name (0x0041000e..0x00410347) and, when the prompt 0x0040cb00 returns non-zero (0x00410351),
stores the same frame index (`mov [0x4c2e14], eax` 0x0041037d; [esp+0x30] after five pushes = [esp+0x1c] of 0x0040ffe1).
customizeCharacter resets it to -1 (0x0040f48b); clickGolferPanel (0x004360e7, 0x004361fc) and drawGolferActionPanel
(0x00436748) test it against -1; mainLoop reads it at 0x0040febb. No instruction references 0x004c2e18, the second dword
of F's row, and only 4 bytes are saved.
Proposed row: `g_proChallenger	golf_clean.exe	0x004c2e14	4	int32_t	1	4	s	46	160	index into g_golfPros; set at 0x0041037d after the prompt 0x0040cb00 accepts, -1 at 0x0040f48b; tested at 0x0040fffe, 0x004360e7, 0x004361fc, 0x00436748	medium`, and F's `f_w_4c2e14` reduced to the dword at 0x004c2e18 or dropped.

### Fields 76, 77, 78: the name-string pool 0x0056fcb0 / 0x0059d81c / 0x005a46b8
One structure in three globals (re/names exe_1_globals.tsv calls the first two `gTextBuffer` / `gTextBuffer2`; D calls
the third `g_flagWords5a46b8`, U-0311):
- clearBuffers 0x0045b880 zero-fills 0x400 dwords + 1 word at 0x0056fcb0 (= 0x1002 bytes, 0x0045b881..0x0045b88f) and
  fills 0x40 dwords at 0x0059d81c with -1 (= 128 int16, 0x0045b891..0x0045b89e).
- appendToBuffer45b8b0(slot) 0x0045b8b0: when slot != -1 and [0x59d81c + slot*2] != -1 (0x0045b8bd..0x0045b8c9) it
  removes that string: strlen at pool + offset, moves the rest of the 0x1002-byte pool down (0x0045b8e4..0x0045b901),
  sets the slot's offset to -1 (0x0045b907) and lowers every offset above it (0x0045b911..0x0045b92e, loop end
  0x0059d91c = 128 words). It then takes the end of the used pool as max(offset + [0x5a46b8 + i*2]) over the slots in
  use (0x0045b93b..0x0045b95a); a call with slot -1 takes the first free index above 0x20 (0x0045b962..0x0045b96c). If
  strlen(0x0051a068) + 1 + end < 0x1002 (0x0045b992) it stores the offset (0x0045b9bb), the length + 1 into 0x5a46b8
  (0x0045b9ca) and copies the text, returning the slot (0x0045b9d5).
- appendString 0x0045b9f0(slot) appends pool + [0x59d81c + slot*2] to 0x0051a068 (0x0045b9f9..0x0045ba3d).
- Fixed slots: 0 = the course name (appendCourseTitle pushes 0 at 0x0040daa2; mainLoop stores slot 0 at 0x0041f143 after
  the check 0x00405ac0), 1..18 = hole names (appendHoleName pushes the hole index at 0x0040729a), 20..29 = best-round
  holders (matchUpdate stores g_bestRounds[i] at 0x0042848a and then slot i + 0x14 at 0x004284a2; writeCourseInfo reads
  slot 0x14 at 0x0043208d).
Proposed rows (one struct is also possible: 0x1002 + gap + ... are not adjacent, so three rows):
`g_namePool	golf_clean.exe	0x0056fcb0	0x1002	char	4098	1	-	76	779	clearBuffers 0x0045b881..0x0045b88f; appendToBuffer45b8b0 bound 0x1002 at 0x0045b8e4 / 0x0045b992; appendString 0x0045ba0c	high`,
`g_namePoolOffset	golf_clean.exe	0x0059d81c	0x100	int16_t	128	2	s	77	969	-1 = free, clearBuffers 0x0045b891..0x0045b89e; read 0x0045b8bd / 0x0045b93b / 0x0045b9f9; loop end 0x0059d91c at 0x0045b928	high`,
`g_namePoolLength	golf_clean.exe	0x005a46b8	0x100	int16_t	128	2	s	78	1026	strlen + 1 stored at 0x0045b9ca; end = offset + length at 0x0045b949..0x0045b954	high`.
These answer U-0311 (role of 0x5a46b8) and, with fields 25 / 29, U-0473; both rows stay open in UNCERTAINTIES.md until
G1 takes the rows.

## 4. Other findings for G1 (overlaps and naming, not edited here)

- F's `f_w_4c2848` (int32[4], 0x004c2848..0x004c2857) contains B's `g_objectOfferCount` 0x004c284c (field 24) and
  `g_greensFee` 0x004c2850 (field 35); B's section 2.2 says they are separate globals with different readers. The field
  table maps fields 24 / 35 onto B's rows (exact matches).
- F's `f_buf_4e06c8` (char[2][16]) and `f_w_4e06e8` (int8_t[17]) are fields +0x00 / +0x20 of golfer-type record 76
  inside B's `g_golferTypes`; tail A maps onto `g_golferTypes[76:80]`.
- `g_surfaces5678b8` is defined by both D and E1 (the mirror keeps D's row).
- The streams name in two styles: C without a prefix (`course_length`, `tile_type`, `holes`), the others with `g_`.
- `g_year` 0x005a6d3c (B) is NOT saved and is written only by appendDate 0x0040d7b0 (0x0040d7da); the save header
  calls appendDate (0x0040b649), so saving refreshes it. Finance code indexes g_financeYears by g_year % 100 (e.g.
  `movsx eax, [0x5a6d3c]; cdq; mov ecx, 0x64; idiv` at 0x00417922): measured, a save made at the championship's
  tournament_start checkpoint changed two int16 cells (+0x0c of rows 0 and 1) of g_financeYears by the next checkpoint (tour_0060)
  (control run without the save: identical to the pinned reference). A port that loads a save must recompute g_year from
  g_date the way appendDate does before finance code runs.
