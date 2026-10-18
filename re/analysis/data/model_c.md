# Data model, stream C: course, terrain and world (phase 1, 2026-10-17)

Stream C of `re/PHASE1_PLAN.md` under `re/model/STREAM_BRIEF.md`. Owned clusters: every `stream = C` row of
`re/model/ownership.tsv` (68 after the parent's 2026-10-17 overrides: animals 776 / 777 and the attribute-table
clusters 814-818 moved here). NO-GUESSING applies; every row cites instructions. Table bytes and strings of the
user's exe are not copied: a meaning taken from a string the code pairs with a value is cited by the string's address.

## 1. Files and method

| file | content |
|---|---|
| `re/model/include/sections/c.h` | C99 header: TileType enum, TileTypeAttr, HoleRecord, PlacedObject, Animal, TopScore, the grid typedefs, TF_* flag bits, the Terrain.dll boundary (TerrainObject, TerrainTile, TerrainTypeInfo). No addresses. Compiles with MSVC 2022 as C, C11 and C++ (`log/p1/c/hdrcheck.bat`), every `static_assert` holds |
| `re/model/c_addresses.tsv` | 50 typed ranges: symbol, va, size, element type, count, stride, sign, `.sve` field index, clusters, evidence, confidence |
| `re/names/model_c_globals.tsv` | 35 new global names (names_sanitize 0 changes, names_validate 0 bad / 0 cross-file duplicates, names_check_evidence 0 bad) |
| `re/model/c_uncertain.tsv` | U-0200..U-0212 |
| scratch `log/p1/c/` | `spec.tsv` (single source; `gen.py` writes the address map, the names file and the checker input `types.tsv`), `refs.tsv` (every model_scan reference with its function), `asm/` (disassembly of the 195 referencing functions), `ev_*.md` (survey notes), `viol.tsv` (checker output), `hookcheck.py` / `hook.tsv`, `coverage.py` |

Method: model_scan's references for each cluster, read against the disassembly of every referencing function
(195 functions, mainLoop's whole body included), the C2 / C3 notes and the C3 / C4 hook comments. Each typed range
was then checked by `re/tools/model_check.py` against every access of the 1,812 game-logic functions, with the
structs read from the header itself (`cstruct:re/model/include/sections/c.h#Name`), and by the hook cross-check.

## 2. Tile grids and tile types

All per-tile arrays are [50][50] indexed x * 50 + y (outer x: canStep 0x00407410 `[esi + eax*2 + 0x5722e8]` with
eax = x * 25; pathStep 0x0042e9f8..0x0042ea05). Every one is cleared with `rep stosd` 0x271 dwords.

| grid | type | content (evidence) |
|---|---|---|
| tile_type 0x5722e8 (.sve 17) | int8 | TileType: the value is the TileTypeAttr index (canStep 0x00407417..0x00407420, ~70 sites) and Terrain.dll's tile type (syncTiles compares it with Tile +0x24 at 0x004494b8 / 0x004494c0). Read with movsx at ~110 sites; every compared or stored immediate is 0..22 (U-0202) |
| tile_variant 0x56988c (.sve 18) | uint8 | passed to Terrain::setType as `rotation` (0x004494fd) and compared with Terrain::getVariation (0x004494cb); the DLL keeps both in Tile +0x28 (terrain_tile.cpp:22). stampCourseTiles writes 0xff (0x0040e166) and the object index over a footprint (0x0040e39d); mainLoop builds wall / fence pieces from direction bits (0x00412100..0x00412122); bit 0x80 tested by prepareShot 0x00424c5e |
| tile_flags 0x53caf0 (.sve 19) | uint16 | section 3 |
| tile_path_cost 0x53bbac (.sve 20) | int8 | pathStep's step cost (movsx 0x0042ea08, +2 at the target); every type stamp copies TileTypeAttr +0x25 into it (stampCourseTiles 0x0040e0d1 / 0x0040e0d7, generateRandomCourse 0x00470bb9..0x00470beb, mainLoop 0x0042089e); updateWalkers subtracts from it (0x004037b4) |
| tile_mood_up 0x5a6378 / tile_mood_down 0x56c7e4 (.sve 66 / 67) | uint8 | emitGolferThought adds the thought's mood change to golfer word +0xa4 (0x0046868c, clamp -10..10) and to HoleRecord.mood_sum (0x004686c1), then increments the up grid when it is positive (0x004687e3..0x0046880d) and the down grid when negative (0x00468811..0x0046883b), with no saturation; mainLoop decays both by b >> 3 (0x00418256..0x00418279); spreadAllShade turns them into shade (0x00461764 / 0x00461791) |
| tile_build_code 0x5830b8 (.sve 83) / tile_refund 0x59c090 (.sve 84) | uint8 / int8 | setGrids(x, y, code, value) 0x0040a130 writes both; mainLoop's build-tool switch calls it with code 0x90 and value = the tool table word [tool * 0x14 + 0x004c26c2] (0x0041f4dc..0x0041f544). 0xff = nothing (fill 0x004316ae..0x004316bb); demolishObject adds the refund to the cash (movsx 0x0040a77c) and resets the code to 0xff (0x0040a7c4) |
| tile_type_saved 0x542414 (.sve 69) | int8 | generateRandomCourse copies tile_type into it (0x0047210c..0x0047211d) before masking unbought land with type 20; showBuyLandScreen restores bought tiles from it (0x004593b4 / 0x004593ba) |
| tile_level 0x543018 (not saved) | int8 | section 6 |
| tile_anim_frame 0x578804 (not saved) | uint8 | frame of the one-shot tile animation that TF_ANIMATING marks: every setter of flag 0x4000 also writes this byte (0x00403444, 0x0046876c ...); mainLoop increments it (0x00413df7) and clears the flag / stores 0xff when it reaches the frame count of the sheet table 0x0053f3e8 (0x0041279a..0x004127d8); buildSceneSprites reads it (0x004632ec ...) |
| tile_shade_a / _b 0x822da0 / 0x82376c | uint8 | spreadShade 0x004615f0 (already named; typed here) |

**Tile types** (enum TileType in the header; identifiers are ours, chosen from what the code does with the value;
the display names live in the attribute record and are read from the user's exe): 0 tee, 1 green, 2 fairway (the
hole tile and its neighbours must be 2 at 0x00413c63), 4 rough (the default fill 0x04040404 at mainLoop 0x0041ca35),
5 deep rough, 7 sand and 9 pot bunker (slopeX / slopeY skip 7 and 9 at 0x0040c31e / 0x0040c322), 10 stream
(updateGolfers 0x0042cb82..0x0042cc14), 11 brush, 12..18 the features mainLoop's switch 0x00421668 dispatches as
type - 12 (0x004124af), 13..16 trees, 17 water (~45 compare sites; Terrain's Tile::edgeKind special-cases 0x11), 20 out
of bounds (typeAtPos returns it off the grid, 0x0040bfbf; tileBlocked 0x0040bf80), 21 / 22 buildings (stampCourseTiles
writes (kind != 5) + 21, 0x0040e06a..0x0040e0cb; canStep excludes both, 0x0040742a / 0x0040742f). 3, 6, 8 and 19 have
no compare or store of their own. Exe and DLL names disagree for 10, 13, 18, 19 (U-0211); only the exe's index use is
modelled.

**TileTypeAttr**, 23 x 0x30 from 0x00578350 (copied from the file data 0x004c1a40, 0x114 dwords, mainLoop SETUP
0x0040f60e, and again by loadWorldAssets, whose jump table 0x00442164 on the course type 0x005a34e0 then renames
records 4, 5, 10, 11, 13, 14, 15, 18 and patches 0x00578432, 0x00578466, 0x005786d8, 0x005786dc):

| off | field | evidence |
|---|---|---|
| +0x00 / +0x10 | name[16], plural[16] | strlen / strcpy by index 0x0040821b, 0x0043331e, 0x00468a68; plural 0x00458fde, 0x0046c634 |
| +0x20 / +0x21 | int8 u20 / u21 | trialShot 0x00422cb0 / 0x00422abc, updateGolfers 0x0042ce3e / 0x0042ca24, decaySum 0x004223c0 (U-0204) |
| +0x22 | int8 walk | the named g_typeAttr; canStep 0x00407426 (<= 0 blocks), clearCost, overlayCharAt 0x00456bd6; written by scanTileLine 0x00421fd7 / 0x004223b4 (record 1) and prepareShot 0x00424154..0x00426623 (records 17, 20) |
| +0x23 | int8 cost | clearCost 0x0042ef15; drawAmenitiesPanel shows v * 100 (0x00435478..0x00435491) |
| +0x24 | int8 u24 | summed over 5 tiles by evalPlacementArea 0x0040dcc0..0x0040dd2d, mainLoop 0x0041ff9f / 0x00420000 (U-0204) |
| +0x25 | int8 path_cost | copied into tile_path_cost (above) |
| +0x26 | int8 collar | passed as int[23] to Terrain::passCollarInfo (0x00449413, 0x004497e3); switch v - 4 at 0x0040777f / 0x00412a5a |
| +0x27 | int8 u27 | mainLoop neighbour compare 0x00414136..0x004141c1, rebuildHeightfield 0x0042f8d4 (== 0x10) (U-0204) |
| +0x28 | int8 wall_height | wallHeight 0x00449373 / 0x004493a8 |
| +0x2c | uint32 flags | bit 1 level fixed, 2 / 4 level = corner-range min / max (rebuildHeightfield 0x0042f81e..0x0042f8c3, cornerHeights 0x0040c057), 8 height 3 (heightAt 0x0042fa5f, worldToScreen 0x0042fe33); 0x100 / 0x200 / 0x400 untested (U-0204) |

All fixed addresses inside the table fit this map (0x00578795 = record 22 +0x25, read by stampCourseTiles 0x0040e0d1;
0x005787c2 / 0x005787c3 are loop ends one past record 22).

## 3. Tile flag word (0x53caf0, uint16, TF_* in the header)

Byte operations on 0x53caf1 act on the high byte; no movsx of the word anywhere. Full setter / clearer / tester lists:
`log/p1/c/ev_flags.md`.

| bits | name | evidence |
|---|---|---|
| 0x001f | field | three uses: the hole number on tee / cup tiles (0x0040a0be, 0x0040a055 with 0x80; read by mainLoop 0x0041ed7a as a HoleRecord index), placed-object kind + 1 on an object's origin tile (stampCourseTiles 0x0040e2cc / 0x0040e2d2; inRange 0x004070e0), a 0..7 phase on water tiles with 0x120 (0x0041f654..0x0041f677; updateGolfers 0x0042b30e) |
| 0x0020 | TF_PATH | buildToolLabel 0x0040a463 pairs it with the remove-path label string 0x004c5128; passed to Terrain::layPath as `on` (0x00449779) |
| 0x0040 | TF_FLOOD_REACHED | set by floodMark 0x0042f14a, cleared over the grid by rebuildTileCoverage 0x0042f363 |
| 0x0080 | TF_CUP | set with the hole number on the HoleRecord cup tile (0x0040a032..0x0040a055, 0x004202cc..0x004202e3) |
| 0x0100 | TF_BIT_0100 | U-0200 |
| 0x0200 | TF_BENCH | 0x0040a3a5 with the remove-bench label 0x004c5118 |
| 0x0400 | TF_COVERED | stampCourseTiles ORs it into every footprint tile (pointer loop 0x0040e3a6..0x0040e3b7, which model_scan does not list); 0x0040a1ef pairs it with the demolish label 0x004c5144 |
| 0x0800 | TF_BIT_0800 | U-0200 |
| 0x1000 | TF_FLOWERS | 0x0040a494 with the remove-flowers label 0x004c5134 |
| 0x2000 | TF_BIT_2000 | U-0200 |
| 0x4000 | TF_ANIMATING | section 2 (tile_anim_frame) |
| 0x8000 | TF_PERMANENT | demolishObject refuses (0x0040a51a); 0x0040a1c0 with the permanent label 0x004c5150; only whole-word wipes clear it |

Whole-word operations: stampCourseTiles clears 0x1300 then ORs its flags argument (0x0040e14d..0x0040e15e); mainLoop
keeps only 0x480 over the grid (0x0041ca60); stepCourseEditQueue xors with the queued mode & 0xff7f (0x00409dd2).

## 4. Hole records: base 0x575ab0, record n = hole n

**Correction to stream A and to the brief** (model_inventory.md section 5 says the index is 1-based from 0x005758a8):
hole n is the record at **0x575ab0 + n * 0x208**, n = 0..19, and the golfer's hole byte +0x21 indexes it directly.
Evidence: showTournamentResults pairs hole 1 with 0x00575cb8 (0x0045a107 / 0x0045a10c, loop end 0x00578148 at
0x0045a137); nearestHole returns 1 for 0x00575cc0 = record 1 +8 (0x00407352 / 0x0040735c). The displacements
0x005758a8.. and 0x00575aa8 are `[n * 0x208 + 0x5758a8]` = record n - 1, the previous hole (holeMagazineEvent
0x0042e3ac..0x0042e3ce / 0x0042e374, guarded by `cmp ebx, 1; jle` at 0x0042e337). Record 0 is showRoutingMapScreen's
swap slot (0x00458440 / 0x004585f4 / 0x00458685); records 1..18 are holes (loops `cmp 0x13`, clamp 1..0x12 at
0x0041840e..0x00458413); record 19 is the golfer hole byte 0x13 slot (0x0042990d..0x00429921), whose tee pair
+0x08 / +0x0c is written at 0x0041f90b / 0x0041f911 next to stampCourseTiles(c, r, 0xf, 0x60). So the plan's
`0x575cb8 x 18` is records 1..18 of this table. Field map: header HoleRecord, evidence in `log/p1/c/ev_holes.md`.
Named here: +0x004 length (25 per tile of tee-to-cup distance: announceBuilding ftol(sqrt(625 (dx^2 + dy^2))),
0x0040e7de..0x0040e833), +0x158 mood_sum (emitGolferThought 0x004686c1), +0x1fc magazine_score
(holeMagazineEvent). 0x00575ec0 + k * 0x410 (pairGolfersWithStory 0x0045e8c2..0x0045e8cc) is record 2k + 2 +0, not a
separate table.

## 5. Placed objects, animals, habitats

**PlacedObject** 256 x 0x10 at 0x58bcb8 (.sve 23). Correction to exe_*_globals (`{short type; short x; short y; short
pad; int variant}`): +6 is a byte orientation 0..3 (stampCourseTiles 0x0040e34e; buildTournamentPrep picks it from
the dominant axis 0x0046d63b..0x0046d675), +7 a flag byte (bit 0x40 recomputed by rebuildTileCoverage
0x0042f407..0x0042f490), +8 a per-type value, +0xc a second value (type 5: mainLoop 0x00417a5a rateLot running
average). Type -1 = free. 0x58bd0a / 0x58bd0c are record 5 x / y (showTutorialText 0x0046083c).

**Animal**: the record base is **0x56d1b8**, not 0x56d1d8. spawnObject computes edi = idx * 0x3c and writes both
[edi + 0x56d1b8] and [edi + 0x56d1d8] (0x004011ff..0x00401232); the .sve block is (0x56d1b8, 0x1770) = 100 x 0x3c; with
base 0x56d1d8 record 99 would end at 0x56e948, 0x20 bytes into the next table (0x56e928, listThemes 0x004724e9). The
named g_animals 0x56d1d8 is field +0x20 (type, -1 free) of record 0. Model_scan's stride 0x14 is the `shl 2` after
`* 15`, not a record size.

**Habitats** 0x572cb0 (128 x 0x14, B clusters 797 / 798): documented for stream B, not typed here: +0 / +4 world
x / y, +8 flags, +0xc head index (-1 = head), +0xe animation state, +0x10 frame, +0x11 direction, +0x12 kind 0..8 (0xff
free) (addHabitatSlot 0x004059ad..0x004059e8, drawWorldObjects). 0x572cac is a counter plus a 0x80 latch of
pairGolfersWithStory / patronEvent (0x0045e9fd, 0x00426782), not a habitat count.

## 6. Terrain heights

| range | type | content |
|---|---|---|
| corner_level 0x5a4998 (.sve 21) | uint8[51][51], x * 51 + y | the vertex level grid: tile (x, y) takes the vertices (x, y-1), (x, y), (x+1, y-1), (x+1, y) (cornerHeights 0x0040c0bb..0x0040c0e8; generateRandomCourse 0x00471b76..0x00471bac); heightBlend reads it when g_date != 0 (0x0040c1fb); stepCourseEditQueue raises / lowers it (0x00409e9c / 0x00409eb4). The plan's `level table 0x5a4998` (stream B) is this grid; B cluster 1034 holds its first addresses |
| tile_level 0x543018 | int8[50][50] | fixed level of tiles whose type has flag 1; rebuildHeightfield stores cornerRange min / max (0x0042f857 / 0x0042f866) |
| corner_cache 0x51b770 | int8[2500][8] | bytes 1, 3, 5, 7 = cornerHeights(x, y, k, 0) (0x0042f7e7..0x0042f80a); 0 = not cached (0x0040c031) |
| sample_grid 0x838c1c (.sve 22) | int8[19][19], 17 x 17 used | genNoise's periodic 16 x 16 noise tile with row / column 16 = row / column 0 (0x00467489..0x004674b0); bilinearSample movsx (0x004674c9..0x0046752d). The parent's `16 x 16, 19-byte rows` is the period; the store is 17 x 17 |
| noise_source 0x838aa0 | int8[19][19] | genNoise's random source, Random::range(16) per byte (0x004673ec..0x0046740f) |

## 7. Course summary, top scores, file records

The course scalars the `.sve` carries and the `.srf` text file (writeCourseInfo 0x00431fa0) prints are computed by
holeMagazineEvent 0x0042dea0 over holes 1..18: course_length 0x58d36c = sum of HoleRecord.length (0x0042df4e..0x0042df68;
the `.srf` prints it after its yards label), course_par 0x59aafc = sum of par, fun_rating 0x59ae78 += 100 * mood_sum /
(+0x24 / 2 + 4 + first_shots), and the three x100 skill ratings (length 0x5a882c, accuracy 0x56949c, imagination
0x5a636c) from per-hole column ratios (0x0042e104..0x0042e22e). **0x822c78 is a course id**, Random::range(0xffff)
(mainLoop 0x0040f8b1..0x0040f8bb, 0x00421522) that addScore uses to keep one high-score row per course
(0x00473343 / 0x004733f1); CLAUDE.md's b2 paragraph calls it the title ticker (the boot-timeline effect stands: its
value comes from the RNG). TopScore 10 x 0x9c at 0x541ce0: designer name (the string writeCourseInfo prints with its
designer label, 0x00432082), course title, fun, skill, cash, level, course id (addScore 0x004732d0).

File records: a championship course is saved with the `.cse` extension string 0x004c5f38 (mainLoop 0x0041cae7 ->
FUN_0040b680) and read back by the `.sve` loader with a 200-byte header (`0040afa0_save_serializer.md`); its body is the
same 86-field `.sve` layout, so its in-memory form is the globals of this model. The `.srf` is text with no record of
its own.

## 8. Camera and projection

camera_x / camera_y 0x4c2ba0 / 0x4c2ba4: tile coordinates, file value 25, clamped 0..0x31 every frame
(clamp stores 0x0041e244 / 0x0041e24e); writers that copy a golfer / ball position (0x0041b539) are unclamped until that
clamp. zoom_scale 0x4c2844: 1, 2 or 4 (file value 4; halved 0x0041c900..0x0041c90e, doubled while < 4
0x0041c92d..0x0041c93d); one tile is 8 * zoom pixels: **0x4c2840 = 8 * [0x4c2844]** at every writer
(0x004105de..0x004105eb, 0x0041c913, 0x0041c942), not a tile count (its file value 16 does not match 8 * 4 until the
first write; D owns it as g_tileStep4c2840). camera_rot 0x5685f4: 0, 2, 4, 6 (+-2 & 7 at 0x0041e042..0x0041e060;
unsigned `ja 6` switches); Terrain gets 90 / 180 / -90 / 0 degrees from it. camera_target_y 0x5685fc with x in
0x4c2f9c (-1 = none) is the pan target of stepCourseEditQueue. hole_notice_mask 0x5685f8 (.sve 42): bit n = hole-type
notice n already shown (holeMagazineEvent 0x0042e2da..0x0042e317). updateTerrainRender keeps the last camera x / y
(0x4d2048 / 0x4d204c, file -1) and zoom (0x4d2050) and takes a strip render for |dx| + |dy| <= 2.

## 9. Terrain.dll boundary

`gTerrain` 0x820ed0 holds the object Terrain::getInstance returns (0x00449175); the exe never runs the constructor
(the DLL's `new Terrain(50, 50)`, operator new(0x164ad0) at Terrain.dll 0x100031f1) but frees it itself: ~Terrain then
the exe's operatorDelete_4a4ffc (freeTerrain 0x00449878..0x00449886). Layout (TerrainObject / TerrainTile in the
header) from the matched sources; the DLL indexes tiles x + y * width, the transpose of the exe's grids. The exe reads
directly only width / height (inline tileAt, 0x00449937..0x00449956), Tile.type (0x004494b3), Tile.corner[odd]
(0x00449fd0..0x0044a1c2) and Tile.wall_on (0x0044a3a7). The 26 imports with their argument types and the exe's
arguments: `log/p1/c/ev_terrain.md` (all thiscall on gTerrain but getInstance; setType(tile, tile_type, tile_variant),
layPath(tile, TF_PATH, TF_FLOOD_REACHED), setWall(tile, side, TileTypeAttr.wall_height, on), passCollarInfo(int[23]
of TileTypeAttr.collar, 23), render / localRender / lowerEdgeCorner angles from camera_rot). CLAUDE.md says the exe
reaches Tile::minHeight / maxHeight inline: no exe instruction does. exe_1.tsv's drawMapLine row says it calls jgld's
drawLine: it calls Terrain::drawLine (0x004493f9).

## 10. Checker result (`log/p1/c/viol.tsv`)

48 typed ranges, every access of the 1,812 game-logic functions: **3 errors, all explained, 273 warnings**.

| error | instruction | explanation |
|---|---|---|
| width_cross x2 in tile_build_code | showBulletinBoard 0x0046f0f9 / 0x0046f103, word stores to 0x0058316e / 0x00583170 | **original defect**: by address they are fields +0xb6 / +0xb8 of a golfer record 156 (0x005794b8 + 156 * 0x100 + 0xb6), the type word and the next word, set to 0 and to the type byte of 0x004d60a9 just before skillPointDialog (0x0046f117); g_golfers has 156 records (0..155), so the stores land in this grid's cells 182 and 184..185 |
| loop_end in corner_cache | rebuildHeightfield 0x0042f87e `cmp edi, 0x520593` | checker limitation: the loop pointer starts at byte 3 of record 0 (0x0051b773) and writes [edi - 2]..[edi + 4]; its last pass writes 0x0052058f, inside the range |

Warnings: holes `stride` 213 (the 0x208 index is formed as (n << 6) + n then `* 8`, the tracker keeps the last factor
8), `width_narrow` 17 (byte tests of the flags dword +0x200, byte reads of the tee / cup tile ints at hireEmployee
0x0040ab8b and generateRandomCourse 0x004720bc), `base_outside` 1 (record n - 1 at 0x0042e374); animals `stride` 9
(the * 15 * 4 composition); tile_type_attr `stride` 13 (* 3 * 16) and `width_narrow` 4 (byte tests of flags +0x2c);
top_scores `stride` 8; camera_rot `width_narrow` 1 (mainLoop 0x004120f5 byte read); corner_level / tile_anim_frame
`sign` 4 (zero-extending `mov cl` / `mov dl` the tracker scores as signed: 0x0041da64, 0x0041dc04, 0x004127bd,
0x0041339c); fun_rating `width_narrow` 2 (mainLoop word reads 0x004179a7 / 0x00417cc8); edit_queue_mode
`width_narrow` 1 (low byte of the mode, 0x00409fda). No warning on tile_type, tile_flags or the other grids.

## 11. Hook cross-check (`log/p1/c/hook.tsv`)

Literal addresses inside the 48 ranges in the code (comments stripped) of every `shim/src/re/*.cpp` except the
negctl copies: 1,544 uses. 1,281 agree in width with the field they land in, 72 are narrower on purpose (byte ops on
the flag word and the hole flags dword, char-pointer bases), 182 have no width (address arithmetic, block tables,
pointer bases), **9 disagree**, all explained: 7 dword loops that copy or clear a whole byte / word range the way the
original's `rep movsd` / `rep stosd` does (c3ah.cpp:382 comment_count clear, c3bs.cpp:165-166 shade clears,
c3eu.cpp:486 top-score clear, c3fv.cpp:1159 and c3fy.cpp:197 the attribute-table copy, c3fy.cpp:8131 the rough fill),
and c3em.cpp:1184-1185, which reproduce the golfer-156 word stores of showBulletinBoard (section 10). No hook reads a
field with a different width than the original.

## 12. Original defects catalogued (not fixed in the model)

- golfer record 156 written into tile_build_code (section 10).
- edge reads outside the grids: mainLoop 0x00411611..0x00411710 (tile_type -1 / +1 / +-0x32 / +0x33, x = 0 reads the
  course table, x = 49 reads 0x572cac and the habitat table), heightBlend 0x0040c225 (x = 49), canStep 0x00407466 (read
  before the bound check), generateRandomCourse's river 0x00470fbb and smoothing loop (`jle` one past 0x572cac),
  floodMark 0x0042f182 / 0x0042f150 (read AND write outside tile_flags), mainLoop 0x004126c7 / 0x00412838, mainLoop
  0x0041161e (tile_variant at 0x0056988b); reach of most is open (U-0207).
- corner_level: unclamped inc / dec in stepCourseEditQueue; mainLoop's reset 0x0041ca44..0x0041ca74 writes 50 of 51
  bytes of rows 0..49 only; byte x * 51 + 50 is both vertex (x, 50) and (x + 1, -1) (U-0209).
- tile_mood_up / _down increments wrap 255 -> 0.
- genNoise computes an 8-neighbour sum and stores the centre byte instead (0x00467436..0x0046746e) and reads 0x00838c08,
  never written.
- stampCourseTiles' free-slot scan has no upper bound and reads past the placed-object table when it is full; a full
  table returns 0, the same as slot 0 (0x0040e2f6..0x0040e30f).
- placeHabitat 0x00405fbc reads +0x12 of the next habitat record (slot 127 reads 0x005736c2, outside).
- hole records 20..31 reachable through the 5-bit flag field (U-0208).
- the exe's inline tileAt at 0x00449736 tests the row-offset register instead of x for the lower bound.

## 13. Corrections to older notes and to stream A

- Hole records: base 0x575ab0 and hole n = record n (section 4), not 1-based from 0x5758a8.
- Animals: base 0x56d1b8; g_animals 0x56d1d8 is field +0x20 of record 0. Tile-type attributes: base 0x578350;
  g_typeAttr 0x578372 is field +0x22 of record 0 (stream A said so; the names rows still say otherwise).
- g_placed_objects' declared struct (+6 byte orientation, +7 flags, +0xc).
- g_tileFlags (exe_0_globals.tsv) says `byte[] ... stride 0x32`: it is uint16[2500], row stride 0x64 bytes.
- g_sampleGrid `char[]`: int8[19][19] with 17 x 17 used.
- 0x822c78 is a random course id, not a ticker (CLAUDE.md b2 paragraph); 0x4c2840 is 8 * zoom, not a tile count
  (mainLoop_structure.md); camera clamp is 0..0x31 (0x34 in audit123 is a bound on writers, not a clamp).
- 0x5830b8 is byte-indexed only (stream A's width-4 classification came from the `rep stosd` fill); setGrids'
  names row (`stores grid spacing/offset globals (no live callers)`) is wrong: it writes tile_build_code / tile_refund
  and mainLoop calls it 8 times (0x0041f544..0x00420a0f).
- mainLoop_structure.md: the switch 0x00421668 is indexed by the tile type - 12 (0x004124af), not by typeAttr - 12.
- Cluster 647: only 0x543018 is terrain; 0x542fd8 / 0x542ff8 are the x[8] / y[8] of the money-popup ring (pointsPopup
  0x0040c8cf / 0x0040c8da) whose value / timer arrays 0x542dd8 / 0x542f00 are stream E's: left untyped for E.
  Cluster 794's 0x5722d4 / 0x5722d5 are loop ends of the course table 0x571fd8 (B). Cluster 1646's 0x838ec0 is a UI
  word of runScrollText, typed low for completeness.

## 14. Overlaps for stream G

| address | here | other stream |
|---|---|---|
| 0x5a4998..0x5a5320 corner_level | typed (C cluster 1033 holds the biased 0x5a4997) | B cluster 1034 (plan claim `level table`); not moved by the parent's override |
| 0x578804..0x5791c8 tile_anim_frame | typed (2500 bytes) | E cluster 822; e2_addresses types 0x578804 as one byte (g_bytes578804): the name was not duplicated |
| 0x838aa0..0x838c09 noise_source | typed | B cluster 1640 (0x838a94 / 0x838a9c are B's own scalars; 0x838aa0 / 0x838ab2 / 0x838ab4 are genNoise's) |
| 0x4c2840 | documented, not typed | D (g_tileStep4c2840) |
| 0x5685f0 next hole index (.sve holes_plus1) | documented, not typed | E2 (g_holesPlusOne) |

## 15. Coverage (`log/p1/c/coverage.py`)

| measure | done | total |
|---|---:|---:|
| clusters (stream C) | 66 (+2 partial) | 68 |
| kept addresses (biased displacements counted with their range) | 215 | 219 |
| weighted by referencing functions | 98.3% | |

Untyped: cluster 647's 0x542fd8 / 0x542ff8 (stream E's popup ring, section 13) and cluster 794's two end markers of
the B course table. Open semantic questions: U-0200..U-0212 in `re/model/c_uncertain.tsv`.
