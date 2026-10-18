# Stream F (p1f): initialised constant tables and configuration words

Phase 1 of `re/PORT_PLAN.md`, stream F of `re/model/STREAM_BRIEF.md` (2026-10-18). Scope: the 366 clusters of `re/model/ownership.tsv`
with `stream = F` (3,487 addresses in `.rdata` and initialised `.data`, 0x4ba000-0x4e7000). Per decision 1 the files below hold
DESCRIPTORS only (symbol, element type, count, stride, file offset in `original\golf_clean.exe`, pointer flag, readers): no table
bytes and no string text are copied. File offset = va - section va + raw pointer: `.rdata` 0x4ba000 -> 0xba000, `.data` 0x4c1000 -> 0xc1000.

## 1. What was typed

| file | content |
|---|---|
| `re/model/f_descriptors.tsv` | 374 objects: symbol, file_offset, size, element_type, count, stride, has_pointers, kind, readers (+ va, access, confidence, clusters, note) |
| `re/model/f_addresses.tsv` | the same objects in the stream address-map format (module, va, size, C type, count, stride, signed, sve_field, cluster, evidence, confidence, access) |
| `re/model/f_strings.tsv` | 3114 referenced strings: va, file offset, length, kind (text / asset_path / filename / format), pool; no text |
| `re/model/f_imports.tsv` | the 3 import-table spans: 74 slots, DLL and API of each (72 import slots, 2 zero separators), readers |
| `re/model/f_gamemain_defaults.tsv` | the 38 stores gameMain makes into `.data` / bss (value, source register, owning object, other readers) |
| `re/model/include/sections/f.h` | C99 structs with `SG_STATIC_ASSERT` sizes; compiled clean with cl 12 /W4 /TC |
| `re/names/model_f_globals.tsv` | 372 names (`f_<kind>_<va>`; two addresses already named elsewhere are not renamed) |
| `re/model/f_uncertain.tsv` | U-0500..U-0514 |

| kind | objects | bytes | elements |
|---|---|---|---|
| string_pool | 293 | 81246 | 3116 |
| record_table | 6 | 5947 | 100 |
| pointer_table | 21 | 1780 | 445 |
| const_table | 20 | 1501 | 423 |
| ui_style_words | 9 | 448 | 112 |
| import_address_table | 3 | 296 | 74 |
| state_table | 1 | 80 | 20 |
| const_scalar | 8 | 74 | 11 |
| state_words | 5 | 64 | 16 |
| pointer_to_state | 3 | 40 | 10 |
| state_buffer | 1 | 32 | 2 |
| state_bytes | 1 | 17 | 17 |
| config_words | 1 | 8 | 2 |
| state_word | 1 | 4 | 1 |
| string_word | 1 | 2 | 1 |

Confidence: 331 high, 40 medium, 3 low. Every cluster has at least one object (366 / 366); 3,486 of 3,487 addresses lie inside an object
(the missing one is the loop-end address 0x4e45dc, one past the pointer table 0x4e4584).

## 2. Surprise first: the clusters called sprite, rect and layout tables are mostly string pools

The big clusters the brief names (0x4c80d0 span 0x9fa4 with 1,602 addresses, 0x4d21a4, 0x4e28e0, 0x4c73cc, 0x4c48cc) are **string pools**: every
one of the 3,051 pool addresses starts a NUL-terminated printable string (checked on the file bytes); with the 63 addresses of the 52 one-string clusters of other classes that is 3,114 referenced strings. The largest pools are asset
PATHS (1,498 strings with a backslash: the image / flic / PCX names `loadImage`, `loadInterfaceArt` and friends open), then UI text (1,560),
plain file names (32) and printf-style formats (24; most format text has no `%` because the game builds numbers separately).
Real layout data (rectangles, hit-test points, sprite offsets) is a handful of small int16-pair tables (section 5).

Largest pools (file offset, bytes, referenced strings, kind mix, readers):

| symbol | file offset | bytes | strings | kinds | readers |
|---|---|---|---|---|---|
| `f_strpool_4cd6a4` | 0xcd6a4 | 18847 | 718 | asset_path:718 | loadInterfaceArt@442180, loadSoundTable@448220 |
| `f_strpool_4c95f4` | 0xc95f4 | 16557 | 608 | asset_path:571,filename:3,text:34 | loadWorldAssets@43dbe0, loadInterfaceArt@442180, courseActionMenu@46f550 |
| `f_strpool_4e28e0` | 0xe28e0 | 5316 | 204 | text:204 | emitGolferThought@467a00, appendOpinion@469a20, buildGolferComment@469b00 +1 |
| `f_strpool_4c8a28` | 0xc8a28 | 3015 | 119 | asset_path:110,filename:2,text:7 | listMatchingFiles@43d2a0, loadFlic@43d740, loadWorldAssets@43dbe0 |
| `f_strpool_4d21a4` | 0xd21a4 | 2283 | 74 | asset_path:35,filename:3,text:36 | showCreditsScreen@44b9c0, loadInfoScreenArt@44bde0, showEndOfYearScreen@44cff0 +4 |
| `f_strpool_4d2c28` | 0xd2c28 | 1302 | 76 | asset_path:2,format:1,text:73 | showShortcutsScreen@44e770, showFinancialReport@44f6b0, showCourseReport@44fb30 +2 |
| `f_strpool_4c4970` | 0xc4970 | 1251 | 77 | asset_path:1,filename:1,text:75 | buildStatusText@406d50, appendHoleName@407280, landmarkName@4074a0 +11 |
| `f_strpool_4c64a8` | 0xc64a8 | 1073 | 37 | format:1,text:36 | mainLoop@40f5c0, showHoleStatsScreen@453330 |
| `f_strpool_4c80d0` | 0xc80d0 | 1041 | 59 | filename:1,text:58 | pickBuilding@434140, drawBuildingPanel@434350, drawAmenitiesPanel@434cf0 +6 |
| `f_strpool_4c4f98` | 0xc4f98 | 756 | 53 | text:53 | narrateShot@407e00, buildToolLabel@40a160, demolishObject@40a4e0 +3 |
| `f_strpool_4c73cc` | 0xc73cc | 729 | 22 | text:22 | matchUpdate@427380, updateGolfers@4289e0 |
| `f_strpool_4c84e8` | 0xc84e8 | 681 | 53 | asset_path:9,filename:2,text:42 | promptAndSaveFile@437910, loadThemeFile@437fa0, showCharacterEditor@4385d0 +14 |
| `f_strpool_4c44ac` | 0xc44ac | 615 | 13 | asset_path:12,text:1 | showLoadingScreen@406250, showCharacterIntro@4065c0 |
| `f_strpool_4c6ecc` | 0xc6ecc | 599 | 20 | text:20 | patronEvent@4266b0, matchUpdate@427380, pairGolfersWithStory@45de80 |

All 293 pools are in `f_descriptors.tsv`; `f_strings.tsv` indexes each referenced string. The pool extent runs from the first to the end of
the last REFERENCED string: strings between them that no instruction addresses (walked by a pointer or a length loop) belong to the pool.
The one case with proof is the pool at 0x4e4680, widened to 0x4e4700 because `expandTextMarkup` walks a pointer up to that address
(`cmp esi, 0x4e4700` at 0x00494be2, found by the checker as `loop_end`). 52 clusters stream A classed as scalar / const table / pointer table are
one-string clusters (code loads the address into edi and runs `repne scasb`, or reads the bytes); they were retyped as strings.

## 3. Import tables (3 clusters, 74 slots)

Spans 0x4ba024 (3 slots), 0x4ba090 (23) and 0x4ba1a4 (48) of the import address table; names come from the PE import directory, not from code.
Counts per DLL over the spans: KERNEL32 26, USER32 24, binkw32 16, WINMM 6. Five slots in the spans have no reader in the game-logic range
(GetCommandLineA, SetEndOfFile, SetFilePointer, InitializeCriticalSection, GetCurrentThreadId: CRT use). `Terrain.dll` and GDI imports sit
in other spans (not stream F). Each slot is 4 bytes read by `call dword ptr [slot]`; the file holds the loader image of the slot (unbound),
the port never reads these bytes.

## 4. Pointer tables (21 tables of `char *`)

Every entry points at NUL-terminated text or at the zeroed bss word 0x4e9a84, which the original uses as the shared EMPTY STRING
(it lies past the raw end of `.data`, in bss). Tables are bounded by the next F cluster; clusters that stream A kept apart because of an
inner loop bound (0x4c2960 / 0x4c2998, 0x4c1464 / 0x4c1484 / 0x4c148c) are one table with sub-runs.

| symbol | file offset | entries | readers |
|---|---|---|---|
| `f_ptrtab_4c1464` | 0xc1464 | 20 | showHireEmployeeDialog@459400, appendWalkerName@467600 |
| `f_ptrtab_4c2858` | 0xc2858 | 8 | showCharacterEditor@4385d0, showGolferPairSelect@459850 +1 |
| `f_ptrtab_4c28b8` | 0xc28b8 | 20 | mainLoop@40f5c0, drawGolferInfoPanel@45c560 |
| `f_ptrtab_4c2908` | 0xc2908 | 22 | appendDate@40d7b0, showEndOfYearScreen@44cff0 |
| `f_ptrtab_4c2960` | 0xc2960 | 46 | drawGolferInfoPanel@45c560 |
| `f_ptrtab_4c2a18` | 0xc2a18 | 28 | narrateShot@407e00, appendUpgradeText@40e5f0 +2 |
| `f_ptrtab_4c2a88` | 0xc2a88 | 3 | membershipReport@406670 |
| `f_ptrtab_4c2a94` | 0xc2a94 | 67 | membershipReport@406670 |
| `f_ptrtab_4c2ba8` | 0xc2ba8 | 11 | mainLoop@40f5c0 |
| `f_ptrtab_4c2bd4` | 0xc2bd4 | 12 | buildGolferName@4676e0 |
| `f_ptrtab_4c2c10` | 0xc2c10 | 11 | showEndOfYearScreen@44cff0, showHistographScreen@455ed0 +1 |
| `f_ptrtab_4c2c3c` | 0xc2c3c | 22 | narrateShot@407e00, mainLoop@40f5c0 +4 |
| `f_ptrtab_4c2ca8` | 0xc2ca8 | 26 | showPreferencesMenu@432560, showCharacterEditor@4385d0 |
| `f_ptrtab_4c2e38` | 0xc2e38 | 20 | appendHoleName@407280 |
| `f_ptrtab_4c2e88` | 0xc2e88 | 20 | appendHoleName@407280 |
| `f_ptrtab_4c2ed8` | 0xc2ed8 | 20 | appendHoleName@407280 |
| `f_ptrtab_4c2fd4` | 0xc2fd4 | 40 | buildGolferBioText@4315e0 |
| `f_ptrtab_4c3078` | 0xc3078 | 4 | writeCourseInfo@431fa0 |
| `f_ptrtab_4d2054` | 0xd2054 | 22 | showFinancialReport@44f6b0, showCourseReport@44fb30 |
| `f_ptrtab_4d3888` | 0xd3888 | 1 | gameMain@45baf0 |
| `f_ptrtab_4e4584` | 0xe4584 | 22 | matchDirective@48cd80 |

Pointers to STATE (not text): `f_stateptr_4c1570` (two dwords, file values 0x519cd4 and 0x51b068, read ~510 times, never written: the current text
target and the Surface pointer the draw routines use), `f_stateptr_4e4a10` (seven pointers to static view-field objects in bss, read by the
four viewField constructors / cleanups) and `f_stateptr_4e42ec` (a pointer variable stored by `Window::onKeyChar` 0x47ef30: STATE).

## 5. Record tables and numeric tables

| symbol | file offset | count x stride | layout | evidence (instruction addresses) |
|---|---|---|---|---|
| `g_recordDefs` | 0xc11e0 | 15 x 0x27 | `0:uchar[6],6:char,7:char[32]` | readers placeRecord 0x401040 (lea eax, [edx + 0x4c11e0] at 0x00401069) and drawRecords 0x4012d0 (movsx ebx, byte [eax + 0x4c11e6] at 0x0040157a) |
| `f_tab_animals_4c1998` | 0xc1998 | 9 x 0x12 | `0:char[16],16:char,17:char` | placeHabitat 0x405e30: repne scasb on [edx*2 + 0x4c1998] at 0x00406030, cmp cl, [eax + 0x4c19a8] at 0x00405f04, movsx edx, [eax + 0x4c19a9] at 0x00405f0c |
| `f_tab_locations_4c1e90` | 0xc1e90 | 16 x 0x82 | `0:char[24],24:char,25:char[33],58:short,60:short,62:uchar,63:uchar,64:uchar,65:char[65]` | seedCourseTable 0x46f2b0: mov dx, [ecx + 0x4c1eca] 0x0046f3a4, mov ax, [ecx + 0x4c1ecc] 0x0046f3ae, mov cl, [ecx*2 + 0x4c1ece] 0x0046f31a |
| `f_tab_buildItems_4c26b0` | 0xc26b0 | 20 x 0x14 | `0:char[16],16:char,17:uchar,18:short` | buildToolLabel 0x40a160 lea edi, [ecx*4 + 0x4c26b0] |
| `f_tab_quotes_4d55e8` | 0xd55e8 | 20 x 0x44 | `0:char[4],4:char[64]` | composeGolferSprite 0x462020 movsx eax, byte [ebx*4 + 0x4d55e8] 0x004620ae |
| `f_tab_quotes_4d5b38` | 0xd5b38 | 20 x 0x44 | `0:char[4],4:char[64]` | composeGolferSprite 0x462020 movsx eax, byte [ebx*4 + 0x4d5b38] 0x004620c7 |

- **Build items** (`f_tab_buildItems_4c26b0`, 20 records): stream A had cluster 130 (0x4c27ec) as a separate scalar table; it is the +0x10 word of record 15,
  and cluster 128 (0x4c2728, +0x12 of record 6) is part of the same table. `loadWorldAssets` 0x43dbe0 copies three strings over the names of records 7, 9 and 14
  and one over record 12 (rep movsd at 0x00441179, 0x004411c3, 0x0044119e, 0x00441645): the table is STATE (4 name fields), the other fields are constants.
- **Course sites** (`f_tab_locations_4c1e90`): exactly 16 records, 0x4c1e90 + 16 * 0x82 = 0x4c26b0 (the build-item table).
- **Record definitions** (`g_recordDefs`, existing name): fields as in `exe_0_globals.tsv`; 15 valid records.
- **Quote tables** (`f_tab_quotes_4d55e8` / `4d5b38`): two tables of 20 records of 0x44 bytes (the last record of the first is all zero); the region
  starting 0x4d6088 is a 42,560-byte SAVE field (`re/tools/sve.py` FIELDS), not a third quote table.

Numeric tables (read-only except where marked):

| symbol | file offset | count | element | readers / note |
|---|---|---|---|---|
| `f_tab_layoutInts_4ba62c` | 0xba62c | 105 | int32 | FUN_449190@449190, loadInfoScreenArt@44bde0 +2 |
| `f_tab_dirDx_4c2878` | 0xc2878 | 8 | int32 | placeAnimal@4017d0, updateWalkers@402a40 +22 |
| `f_tab_dirDy_4c2898` | 0xc2898 | 8 | int32 | placeAnimal@4017d0, updateWalkers@402a40 +22 |
| `f_tab_bytes_4c2d10` | 0xc2d10 | 21 | int8 | mainLoop@40f5c0, showCharacterEditor@4385d0 +1 |
| `f_tab_bytes_4c2d28` | 0xc2d28 | 16 | int8 | mainLoop@40f5c0, showCharacterEditor@4385d0 +1 |
| `f_tab_point16_4c2d38` | 0xc2d38 | 50 | int16 pair | loadInterfaceArt@442180, showBulletinBoard@46e810 |
| `f_tab_bytes_4c2e2c` | 0xc2e2c | 8 | int8 | mainLoop@40f5c0, showHireEmployeeDialog@459400 |
| `f_tab_point16_4c7930` | 0xc7930 | 16 | int16 pair | nearestMenuSpot@4326a0, showMainRadialMenu@432ba0 |
| `f_tab_point16_4c7970` | 0xc7970 | 9 | int16 pair | showMainRadialMenu@432ba0, hitTestSlot432f90@432f90 +2 |
| `f_tab_bytes_4c7994` | 0xc7994 | 12 | int8 | showMainRadialMenu@432ba0, hitTestSlot432f90@432f90 +2 |
| `f_tab_bytes_4c79a0` | 0xc79a0 | 20 | int8 | showMainRadialMenu@432ba0, hitTestSlot432f90@432f90 +2 |
| `f_tab_point16_4c79b4` (state) | 0xc79b4 | 20 | int16 pair | showMainRadialMenu@432ba0, hitTestSlot432f90@432f90 +2 |
| `f_tab_rec24_4c7a2c` | 0xc7a2c | 18 | int32 | showInfoCursorPanel@433190 |
| `f_tab_point16_4c7a88` | 0xc7a88 | 14 | int16 pair | drawTerrainToolbar@433e50, drawAmenitiesPanel@434cf0 |
| `f_tab_point16_4c7af0` | 0xc7af0 | 8 | int16 pair | hitEmployee436b00@436b00, drawEmployeeDetailPanel@436e50 |
| `f_tab_point16_4c7b38` | 0xc7b38 | 22 | int16 pair | nearestCharControl@438260 |
| `f_tab_point16_4c7b90` | 0xc7b90 | 20 | int16 pair | showCharacterEditor@4385d0 |
| `f_tab_pair32_4c7be0` | 0xc7be0 | 10 | int32 pair | charEditorZone@4382f0, pickCharEditor@438390 |
| `f_tab_point16_4e3dc0` | 0xe3dc0 | 16 | int16 pair | courseActionMenu@46f550, drawScoreSummary@473470 |
| `f_tab_dwords_4e3e00` | 0xe3e00 | 10 | int32 | courseActionMenu@46f550, drawScoreSummary@473470 |
| `f_tab_dwords_4e432c` | 0xe432c | 32 | int32 | rebuildFramePieces@47d610, hitTestTree@47f340 |

Direction tables `f_tab_dirDx_4c2878` / `f_tab_dirDy_4c2898`: two 8-entry int32 tables indexed together by 24 functions (path steps, animal and walker
movement, flood fills, shot scans); the hooks index them as `int` with stride 4. `f_tab_point16_4c79b4` is STATE: `showInfoCursorPanel` rewrites
entries 13..15 from the 0x4c7a2c records (stores at 0x004331f0 / 0x004331f8).

`.rdata` constants: `f_k_dbl_*` are 8-byte x87 operands (read by `fmul` / `fadd`): three doubles at 0x4ba478, one each at 0x4ba620 / 0x4ba800 / 0x4ba818 /
0x4bacf0 and two at 0x4bba48. **0x4ba800 is the scale of `Random::next` 0x45c1a0 (`fmul qword ptr [0x4ba800]` at 0x0045c1ca): see section 8.**

## 6. Option and flag words, and what gameMain sets

The words in 0x4e42c0-0x4e4a28 are the DEFAULT FIELD VALUES of the UI widget classes: constructors copy them into each new object
(the c3di / c3ef batches copy `0x4e42c8..` into the window object at +0x16c..+0x180 and `0x4e4764..` into the control at +0x57c..+0x5a0). Blocks
by reader function:

| symbol | file offset | words | read by | written by |
|---|---|---|---|---|
| `f_cfg_4e4210` | 0xe4210 | 2 | Image_ctor@474ae0, Image_releaseAll@474cb0 | - |
| `f_cfg_tooltip_4e42c0` | 0xe42c0 | 2 | gameMain@45baf0, View_ctor@479c40, layoutNode@479f30 +2 | HotList::showTip@492bd0 |
| `f_cfg_View_4e42c8` | 0xe42c8 | 9 | gameMain@45baf0, View_ctor@479c40, layoutNode@479f30 +2 | gameMain@45baf0 |
| `f_cfg_EditBox_4e43f0` | 0xe43f0 | 8 | gameMain@45baf0, VoiceRx::ctor@4852e0, EditBox::resetFields@485ff0 +1 | gameMain@45baf0 |
| `f_cfg_Button_4e443c` | 0xe443c | 19 | gameMain@45baf0, Reader::open@487fb0, Button::ctor@488500 +14 | gameMain@45baf0 |
| `f_cfg_ListModel_4e4488` | 0xe4488 | 18 | gameMain@45baf0, Reader::open@487fb0, Button::ctor@488500 +14 | gameMain@45baf0 |
| `f_cfg_ListBox_4e44d0` | 0xe44d0 | 4 | gameMain@45baf0, Reader::open@487fb0, Button::ctor@488500 +14 | gameMain@45baf0 |
| `f_cfg_TextViewBuild_4e44e0` | 0xe44e0 | 1 | gameMain@45baf0, Reader::open@487fb0, Button::ctor@488500 +14 | - |
| `f_cfg_TextView_4e44e4` | 0xe44e4 | 40 | gameMain@45baf0, Reader::open@487fb0, Button::ctor@488500 +14 | gameMain@45baf0;MsgBox::setColorA@490cc0;MsgBox::setColorB@4 |
| `f_cfg_Control_4e4764` | 0xe4764 | 11 | buildLabeledControl@496030, controlInitBase@4961d0 | - |

`gameMain` 0x45baf0 stores 32 defaults into these blocks and bss at 0x0045bc48..0x0045bd64 (`re/model/f_gamemain_defaults.tsv`; register values
from the straight-line constant tracking of the function, with esi = 0 at 0x45bb53, edi = 1 at 0x45bb5a, ebp = 7 at 0x45bc63, ebx = 0xf8 at 0x45bc85,
eax = 0xff at 0x45bca9, ecx = 2 at 0x45bca4): values 0, 1, 2, 7, 8, 0xf8, 0xff and -1. Because gameMain overwrites them and `MsgBox::setColorA/B`,
`HotList::showTip` and `customizeCharacter` write others, **these blocks are state initialised from the file, not constants** (listed in section 7).
Whether a word is a palette index (0xf7..0xff), a font id or a flag is not determined (U-0509). The bss targets the same function sets
(0x83ab6c, 0x83ab70, 0x83afd8..0x83afe0, 0x83b604, 0x83b608, 0x83b9d0, 0x83b9f0, 0x83ad44, 0x83abdc, 0x822c74, 0x822c8c / 0x822c90 = screen size 0x320 x 0x258)
are bss, owned by other streams; their stored values are in the tsv.

## 7. Initialised objects the game WRITES (report to the parent: these are state)

| symbol | va | writers | .sve |
|---|---|---|---|
| `f_tab_buildItems_4c26b0` | 0x4c26b0 | buildToolLabel 0x40a160 lea edi, [ecx*4 + 0x4c26b0] | - |
| `f_w_4c2848` | 0x4c2848 | stepCourseEditQueue@409cf0;loadCourse@40b840;loadGame@40b9b0;FUN_40d890@40d890;announceBui | 24,35 |
| `f_w_4c2e00` | 0x4c2e00 | showCharacterIntro@4065c0;narrateShot@407e00;stepCourseEditQueue@409cf0;demolishObject@40a | - |
| `f_w_4c2e14` | 0x4c2e14 | customizeCharacter@40f190;mainLoop@40f5c0 | 46 |
| `f_w_4c3074` | 0x4c3074 | saveCourseJpg@431d20 | - |
| `f_tab_point16_4c79b4` | 0x4c79b4 | word reads [0x4c79b4] (7) / [0x4c79b6] (6): hitTestSlot432f90 movsx edx, word ptr [ecx - 2 | - |
| `f_buf_4e06c8` | 0x4e06c8 | customizeCharacter 0x40f190 (state, not a constant); inside save tail block A (re/tools/sv | tailA |
| `f_w_4e06e8` | 0x4e06e8 | byte stores [0x4e06e8] 0x0040f3c9, [0x4e06e9] 0x0040f348, [0x4e06eb] 0x0040f38e, [0x4e06ec | tailA |
| `f_w_4e3da4` | 0x4e3da4 | runScrollText@46dea0 | - |
| `f_w_4e3db8` | 0x4e3db8 | postEvent@46e7b0;showBulletinBoard@46e810;FUN_46f180@46f180 | - |
| `f_cfg_tooltip_4e42c0` | 0x4e42c0 | HotList::showTip@492bd0 | - |
| `f_cfg_View_4e42c8` | 0x4e42c8 | gameMain@45baf0 | - |
| `f_stateptr_4e42ec` | 0x4e42ec | holds bss address 0x83aba4, loaded at 0x47eeee, stored by 0x47ef30 | - |
| `f_cfg_EditBox_4e43f0` | 0x4e43f0 | gameMain@45baf0 | - |
| `f_cfg_Button_4e443c` | 0x4e443c | gameMain@45baf0 | - |
| `f_cfg_ListModel_4e4488` | 0x4e4488 | gameMain@45baf0 | - |
| `f_cfg_ListBox_4e44d0` | 0x4e44d0 | gameMain@45baf0 | - |
| `f_cfg_TextView_4e44e4` | 0x4e44e4 | gameMain@45baf0;MsgBox::setColorA@490cc0;MsgBox::setColorB@490d20 | - |

Save coverage (`re/tools/sve.py` FIELDS indices 24 and 35 are the dwords at 0x4c284c / 0x4c2850 inside `f_w_4c2848`, 46 is 0x4c2e14; tail block A = 0x4e06c8 + 0x8c0
covers the two buffers and the byte block written by `customizeCharacter`). The 0x4c1578 record table (22 x 0x30, 1,056 bytes, a save field) lies before
the animal table and is stream B / E (U-0513).

## 8. Corrections to stream A and to older notes

- **`gRngState` 0x4ba800 is wrong** (`re/names/exe_1_globals.tsv`: type `unsigned (LCG state)`, evidence `read and written by 0x45c1a0`). The address is in `.rdata`;
  `Random::next` 0x45c1a0 keeps its LCG state at `[ecx]` (instance, `mov [ecx], eax` at 0x0045c1b8) and only READS 0x4ba800 as an 8-byte x87 scale
  (`fmul qword ptr [0x4ba800]`, 0x0045c1ca). No writer exists. The row should be withdrawn and 0x4ba800 named a double constant (this file keeps `f_k_dbl_4ba800`).
- `g_recordDefs` (0x4c11e0) is right; it gains the name field at +0x07 and 15 records (6 footprint bytes, size byte, name[32]).
- Stream A class errors: 52 const / scalar / pointer clusters were single strings (retyped); clusters 128 and 130 are inside the build-item table; clusters 154..158
  (`pointer table` / `const lookup table`) are one int16-pair table, not pointers; 0x4c1464 etc. are tables of `char *`, not nested pools.
- The brief called 0x4c80d0 a sprite / rectangle table: it is an asset-path pool (section 2).

## 9. Checker, hook cross-check

`re/tools/model_check.py log/p1/f/model_f_check.tsv` (input generated from f_descriptors; same result with `--all-functions`): **8 errors, 27 warnings**.
All 8 errors are the same idiom: the compiler reads a constant string by dwords / words for an inline copy (a 4-byte read of `0x4d32c4..0x4d32d4`
in showMembershipRoster 0x00454f24..0x00454f4c, of 0x4e43c8 / 0x4e43cc in VoiceRx::ctor 0x0048530c / 0x00485312, a 2-byte read of the one-character string 0x4c3f70 in
drawMessageBuffer 0x0045b0d3): `width_cross` on a `char` array, explained (the original copies the string, not a number). The 27 warnings: `stride` (12 on the
build-item table: the index is `idx * 5` scaled by 4, the checker tracks only the 4), `width_narrow` (byte reads of the low byte of the -1 / 0 / 1 dwords of the
direction tables and of style words), one `sign` on a location byte. Fixed from earlier runs: build-item names written by loadWorldAssets (const -> state), the radial-menu
table written by showInfoCursorPanel, bound 0x13 of the two list tables at 0x4c79a0 / 0x4c79b4 (20 entries each), signed bytes (movsx).

Hook cross-check (`shim/src/re/*.cpp`, `*_negctl.cpp` excluded): 5,256 literal addresses in 0x4ba000..0x4e7000 across 147 sources (negctl copies included in this count), 4,158 inside F objects (the rest belong
to streams B-E: vtables, bss pointers). The 753 accesses with an explicit `G<T>` / `reinterpret_cast<T*>` type were checked against element width, alignment and struct field
offset: **0 disagreements**; the 105 `addr + k * N` expressions use the strides of the types (4, 8, 18, 20, 39, 68, 130): **0 disagreements**. Observations: the hooks type
0x4c1570 as `void**` (a Surface pointer or the current text object) and swap it in fixtures; 0x4c27ec is read as `signed char` and 0x4c1ea8 as `signed char` with stride 130, matching the signed bytes above.

## 10. Coverage

| measure | done / total |
|---|---|
| clusters | 366 / 366 |
| addresses | 3,486 / 3,487 |
| addresses weighted by referencing functions | 4,246 / 4,247 (99.98%) |
| objects by confidence | 331 high, 40 medium, 3 low |

Left untyped: nothing by cluster. Weak spots (U rows): roles of the int16-pair tables (U-0502), the 105-dword table at 0x4ba62c (U-0501), 0x4c7a2c record extent (U-0503),
the byte lists (U-0511), the meaning of every UI default word (U-0509), the quote header bytes (U-0508).

Clusters that belong to another stream (for the parent): none among the 366; but the neighbours named above are not F: the 0x4c1578 record table (B / E), the 0x4d6088
save region (42,560 bytes) and everything at 0x4e9a84 and above (bss; 0x4e9a84 itself is the shared zero word used as empty string).

## 11. Open ranges

See `re/model/f_uncertain.tsv` (U-0500..U-0514).
