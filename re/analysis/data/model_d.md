# Stream D data model: rendering and audio state (golf_clean.exe, phase 1)

Files: `re/model/include/sections/d.h` (structs, enums, constants), `re/model/d_addresses.tsv` (109 typed ranges, symbol,
va, size, element type, count, stride, sign, .sve field, clusters, evidence, confidence), `re/names/model_d_globals.tsv`
(62 new names), `re/model/d_uncertain.tsv` (U-0301..U-0330), and two descriptor tables that hold addresses and file
offsets only, never the strings: `re/model/d_sound_slots.tsv` (219 loadSoundTable calls) and `re/model/d_image_slots.tsv`
(289 loadImage calls). Scratch and generators: `log/p1/d/` (`spec.py` is the typed-range source, `build_d.py` writes the
files, `hookcheck.py` / `hookrest.py` / `ctorcheck.py` are the hook cross-checks).

Method. For every D row of `re/model/ownership.tsv` (446 clusters, 529 inventory addresses; the brief's 450 / 548 were
the numbers before the parent moved clusters 814-817 to stream C) the referencing functions were decompiled with
`re/tools/decomp.py`, the access widths and signs were read from `re/model/inventory_addresses.tsv`, element counts were taken
from constructor loops and loop bounds (not from stream A's counts), and every typed range was checked with
`re/tools/model_check.py` over all accesses (section 6). Evidence cells cite instruction addresses; the automatic suffix of
each evidence cell in `d_addresses.tsv` is the first read and first write instruction the scanner found.

## 1. What was typed

| group | clusters | addresses | main ranges |
|---|---|---|---|
| sound table and Snd objects | 176 | 176 | `g_sounds` 300 x 0x6c |
| sound device, list, entry points | 8 | 18 | music device 0x1e4, list head / tail, 11 entry points, dll handle, ready flag |
| sprite queue | 24 | 26 | 24 parallel arrays of 1024 and 4 scalars |
| screen, colour tables, blit, cursor, draw list | 19 | 26 | screen size, two colour tables, blit and cursor state, draw list |
| polygon fill and combo defaults | 3 | 16 | `SgPolyFill` 0x48, four combo default words |
| asset sheets, cells, images | 187 | 216 | 590-sheet tables, 0xdd40-cell pool, 190 image slots |
| flic players and movie | 5 | 7 | 591 Flic objects, 591 stream objects, movie id list |
| Surface object arrays and singles | 13 | 17 | 9 constructor arrays, singles |
| other typed objects and scalars | 11 | 27 | sky actors, record banks, tile screen cache, misc scalars |
| total | 446 | 529 | 109 typed ranges, 0 addresses untyped |

By confidence of the covering range: high 493 addresses (807 of 869 referencing-function weight), medium 21, low 15.

## 2. Sound (clusters 1124-1299 for the slots, 1696-1705, 1709)

### 2.1 `g_sounds`: 300 Snd objects of 0x6c bytes at 0x80d840

| evidence | address | what it shows |
|---|---|---|
| construct | arrayStaticInit_448160 0x448160: `ehVectorConstructorIterator(0x80d840, 0x6c, 300, Snd::ctorDerived)` (call at 0x448180) | 300 elements of 0x6c |
| destroy | FUN_00448190 0x448190: the vector destructor iterator with the same base, size and count | same |
| index | playSound 0x4481b0: `this = 0x80d840 + slot * 0x6c` after `slot != -1`; startSoundSlot 0x448200 the same | slot = element index |
| end | the table ends at 0x80d840 + 300 * 0x6c = 0x8156d0, the first byte of the asset path table | no gap |

Stream A listed 175 independent `object` clusters (`this` at 0x80d8ac, 0x80d918 ...) plus a `record array stride 0x6c n=1`: all
of them are element starts of this one array. `loadSoundTable` 0x448220 makes 219 calls `Snd::applyMode484e30(&slot, path,
mode)` on 210 distinct slots (indexes 0..269; slots 270..299 are never configured, U-0316): mode argument 4 in 201 calls and
0x14 in 18 calls (the 18 are the music files). `d_sound_slots.tsv` lists slot, slot address, call order, path VA, path file
offset, path length, mode and whether a later call overwrites the slot (9 do, section 5).

### 2.2 Snd layout (`SgSnd`, 0x6c bytes)

Derived from the methods that take `this` (0x484150 ctorBase, 0x484820 ctorDerived, 0x485260 ctor485260, 0x484260 setMode, 0x4847f0
setPan, 0x485140 setVolume, 0x484f40 setPitch, 0x4846b0 setField34, 0x4846d0 setField38, 0x484940 play, 0x484a40 configure,
0x484b30 busy, 0x484b80 setName, 0x4843e0 unlinkSound, 0x4844e0 unlinkFromList, 0x484ff0 flags, 0x484c80 open, 0x484d60 reopen,
0x484e70 start, 0x4842f0 SndMusic::setFile).

| offset | field | evidence |
|---|---|---|
| +0x00 | vptr | stores 0x4badec (0x485260), 0x4bac68 (0x484150), 0x4bacf8 (0x484820); the derived store wins |
| +0x04 | volume (0..0x7f) | 0x485260 stores 0x7f; setVolume masks `& 0x7f` (0x485147) |
| +0x08 | pan (-0x40..0x3f) | setPan clamps `< -0x40` and `> 0x3f` (0x4847f0) |
| +0x0c..+0x2f | 9 dwords, zeroed | 0x485260 clears 9 dwords from param_1 + 3; no reader (U-0320) |
| +0x30 | loop | `Snd::flags` maps it to result bit 2; `Snd::start` / `setFile` call the voice slot +0x48 when it is nonzero |
| +0x34 | field_34 | setField34 stores it and calls voice slot +0x4c (U-0320) |
| +0x38 | field_38 | default 1000 (0x484150 calls setField38(1000)), zero refused with return 10, voice slot +0x04 (U-0320) |
| +0x3c | group | ctorDerived stores 0x10; setVolume tests `< 0x10` and indexes the group table; play calls bucketEmpty / bucketSet with it |
| +0x40 | voice | created by `createSound4840e0(&this+0x40, path, 1)` (g_soundFns[0]); every setter forwards to `[[voice]+slot]` when nonzero |
| +0x44 | state bits | see the enum in d.h: setMode ORs 4 / 8 / 0x10 / 0x28 / 0x100 / 0x80; start and setFile OR 1; unlinkSound tests 2 |
| +0x48 / +0x4c | prev / next | unlinkSound 0x4843e0 splices them and updates g_soundHead / g_soundTail |
| +0x50 | path | setName allocates strlen+1 and copies; shutdownSound frees it |
| +0x54 | mode | setMode stores its argument |
| +0x58 | options | configure builds it from the option argument (bit map in d.h) |
| +0x5c | pitch (-0x4b0..0x4b0) | setPitch clamps (0x484f40) |
| +0x60 | unk_60 | ctorDerived stores 0x3f800000; no reader (U-0320) |
| +0x64 | length_ms | open stores the result of voice slot +0xc8; `busy` compares `timeGetTime() - [+0x68] < [+0x64]` |
| +0x68 | started_at | play stores timeGetTime() on the deferred path |

### 2.3 Music device, list, sound.dll entry points

* `g_sound_device_music` 0x83ad80 is an object of 0x1e4 bytes: a 0x24-byte header (vptr at +0, a device object pointer at +0x14
  that resourceLoad 0x487c00 calls through) followed by 16 group records of 0x1c bytes at +0x24. Evidence: bucketEmpty 0x487770
  reads the byte at `this + i * 0x1c + 0x24` and bucketSet 0x4877a0 the byte at `+ 0x25` with `i <= 0xf`; setVolume 0x485140
  reads the dword at `0x83ada8 + group * 0x1c` (that is group +4); the 16 records end at 0x83af64 = `g_soundTail`. Stream A's
  `record array stride 0x1c n=2` for cluster 1696 undercounts. The method names `HashTable::bucketEmpty` / `bucketSet` are not
  supported by the code: they test the two flag bytes of a sound group (play proceeds only when byte 0 is nonzero and byte 1 is
  zero). Suggested names for stream H: `SndGroup_isDisabled` / `SndGroup_isMuted`.
* `g_soundFns`: 11 pointers. soundVersionCheck 0x483fc0 stores `GetProcAddress(sound.dll, ordinal i+1)` for i = 0..10 (loop
  compare `0x83af98`, the next global); clearSoundPtrs 0x484130 clears 0xb dwords. Slot 0 creates a voice (0x4840e0), slot 1
  releases one (0x484110), slot 2 is `callSoundHook74`, slot 3 is called by shutdownSound, slot 4 by openHookA, slots 9 and
  10 are `closeHookC`'s hook (0x83af90) and `callSoundHook94` (0x83af94). `soundVersionCheck` compares the high byte of slot 10's
  result with 0xc and shows a warning when it differs.
* `g_soundDll` (HMODULE) and `g_soundReady` as named in the master; `g_soundHead` / `g_soundTail` bound the doubly linked list
  of Snd objects.
* 0x83afcc (cluster 1705) is written by the network code and read by pathStep / pumpAndRedraw / runScrollText: not sound state
  (U-0301). 0x83b15c..0x83b168 are four string pointers used by showCreditsScreen (U-0302).

## 3. Sprite queue (clusters 1613-1638 range)

Arrays of 1024 elements. `enqueueSprite` 0x462a30 refuses when `count > 0x3fe` (cmp at its entry), so the largest valid count
is 0x3ff and index 0x3ff of every array is never written by it. Nothing outside it grows the count.

| address | symbol | element | role (evidence) |
|---|---|---|---|
| 0x82415c | g_spriteSortKey | int32 | param 3 (depth key); the insertion walk keeps the list sorted by it (signed `jle`, 0x462a30..) |
| 0x82515c | g_spriteSnapNext | int32 | snapshot of the link array |
| 0x82615c / 0x82715c | g_spriteSnapAnchorX / Y | int32 | snapshots of anchor x / y |
| 0x82815c | g_spriteFlags | uint32 | param 8; -1 = plain blit, bit 0x100 golfer (low byte index), 0x200 palette-copy draw (low 9 bits index the image table), 0x400 / 0x800 scaled draws (drawQueuedSprite 0x462be0) |
| 0x82915c | g_spriteShade | int32 | param 7; sign selects the branch, shade * zoom goes to display slot +0x9c |
| 0x82a15c | g_spriteObj | ptr | param 6; `this` of Surface_blit2 / Sprite_draw / Sprite_drawScaled |
| 0x82b15c | g_spriteSnapCount | int32 | snapshotTables stores the count first |
| 0x82b160 / 0x82c164 | g_spriteAnchorX / Y | int32 | params 1 and 2; drawCenteredText coordinates for the golfer label |
| 0x82c160 | g_spriteQueueHead | int32 | -1 when empty |
| 0x82d164 / 0x82e164 / 0x82f164 | snapshots of key, flags, shade | int32 | snapshotTables copies |
| 0x830164 / 0x831164 | g_spriteX / Y | int32 | scaled position written by addSprite / enqueueSprite |
| 0x832164 / 0x833164 | snapshots of x, y | int32 | |
| 0x834164 | g_golferIdText | char[12] | `__itoa(id, 0x834164, 10)` in drawQueuedSprite; the next global is g_date |
| 0x834174 | g_spriteQueueAux | int32 | written only (U-0303) |
| 0x8351f4 | g_spriteSnapObj | ptr | snapshot of the object array |
| 0x8361f4 | g_spriteQueueNext | int32 | link, -1 terminates (renderSpriteQueue 0x463100) |
| 0x8371f4 | g_spriteSnapHead | int32 | snapshot of the head |
| 0x8371f8 | g_spriteFlag8371f8 | int32 | read once, no writer (U-0304) |
| 0x8371fc | g_spriteSnapAux | int32 | snapshot of 0x834174 |
| 0x838200 | g_spriteQueueCount | int32 | |

snapshotTables 0x462800 copies ten arrays of 0x400 dwords each. The ten destination arrays are clusters 1615-1617, 1625-1627,
1630, 1631, 1635 and the array at 0x8371fc (inside cluster 1637); stream A classified nine of them `low` (indexed, width 4, no
stride evidence) and could not name them. Gaps: 0x835174..0x8351f3 (U-0318) and 0x8381fc (U-0319).

## 4. Screen, colour tables, blit, cursor, assets

### 4.1 Colour tables
* `g_halfBrightLut` 0x822c74 (pointer): gameMain 0x45baf0 allocates `malloc(0x20000)` and fills 0x10000 words with
  `(((i >> 10 & 0x1e) << 5 | (i >> 5 & 0x1e)) << 4) | ((i >> 1) & 0xf)` (a halving of each 5-bit channel). The brief and the
  plan call it a 32K conversion table; it has 64K entries of 16 bits (0x20000 bytes).
* `g_color16Lut` 0x824148 (pointer): FUN_004617f0 stores `operator_new(0x50000)`; build16BitColorTable 0x461830 fills five tables
  of 0x10000 bytes, each indexed by a packed 15-bit (display slot +0xb4 == 1 selects the 16-bit 5-6-5 packing) colour, so the
  allocation is 5 x 0x8000 words.

### 4.2 Screen and blit
`g_screenWidth` / `g_screenHeight` 0x822c8c / 0x822c90: gameMain stores 800 / 600 (the Display mode call follows at
0x45bb2a..0x45bb42); the dirty flags 0x822b98 / 0x822b9c and 0x4d3834 (pending redraw) are plain dwords (flushRedraw 0x45ae70).
`g_blitX` / `g_blitW` are two fields of the cursor save record whose other fields belong to stream E (cluster 1667). The blit
context words 0x83c310..0x83c330 are five scalars/pointers (blitError, a cleared word, context, object, surface). `g_tileScreenY`
0x55fec8 is the y half of the 50 x 50 int16 cache `tileToScreen` fills (the x half is 0x55eb40, stream B's cluster 665; -99 in
the x half marks an off-screen tile).

### 4.3 Polygon fill (`SgPolyFill`, 0x83d34c..0x83d393)
fillConvexPoly8 0x493100 / fillConvexPoly16 0x4932d0 fill it; polyEdgeStep 0x492ed0, fillSpanDown 0x493000 and fillSpanUp 0x493080
read it. Fields: fill colour +0x00 (written as a dword, read as a byte or a word), bottom vertex index +0x04, vertex pointer +0x0c
(pairs of dwords, x then y), clip left +0x10 and top +0x14, vertex count +0x2c, locked 16-bit pixel base +0x30 (display slot
+0x20), locked 8-bit base +0x34 (slot +0x18), current scan line +0x38, pitch +0x3c (x2 on the 16-bit path), clip right-1 +0x40,
clip bottom-1 +0x44. Unreferenced bytes: +0x08 and +0x18..+0x2b (U-0323). Four default words for the combo box popup follow at
0x83d3a0 (U-0308).

### 4.4 Asset sheets ("flics") and the cell pool
All arrays are indexed by sheet id (0..589; id 0x24e is the shadow stream slot, so the stream arrays have 591 elements).

| address | symbol | element | evidence |
|---|---|---|---|
| 0x53f3e8 | g_sheetCols | int32[590] | cellAt 0x43d6f0: `if cols[id] <= col return default cell`; loadFlic 0x43d740 passes `&cols[id]` to flicOpenStream as the frame-count output |
| 0x5a5a2c | g_sheetRows | int32[590] | loadFlic stores its parameter 3 here; freeTable 0x43d670 multiplies cols * rows; the array ends at 0x5a6364 (an .sve global) |
| 0x5a9370 | g_sheetFirstCell | int32[590] | loadFlic stores allocBlock's result (-1 = none); cellAt returns the default cell when -1 |
| 0x8156d0 | g_sheetPath | char[590][0x32] | loadFlic copies the path to `0x8156d0 + (id + base) * 0x32`; the table ends at 0x81ca0c |
| 0x5aaa30 | g_cellPool | Surface[0xdd40] x 0x2c | arrayStaticInit_43d460: vector constructor (0x5aaa30, 0x2c, 0xdd40); cellAt: `0x5aaa30 + (cols * row + first + col) * 0x2c`; loadFlic reports an error when `first + cols * rows >= 0xdd40` |
| 0x820b70 / 0x820d00 | g_cellFreeStart / Len | int32[100] | allocBlock 0x43d5d0 and freeBlock 0x43d520 scan 100 entries (`cmp 99`); freeBlock merges adjacent runs; -1 marks an unused entry |
| 0x820b6c | g_sheetBase | int32 | added to loadFlic's id; set to 0 by the loaders |
| 0x820ec0 | g_cellCursor | int32 | next free cell during a load |
| 0x820ec8 / 0x820ecc | g_flicFrameImage / g_flicLoadCount | ptr / int32 | Image_draw argument; loadFlic counts calls and shows progress every 100th |

Stream A's clusters 569-585, 1061-1062, 1104-1107 and 1334-1339 (fixed-id reads such as `cellAt(0x1bd, ...)`, whose cols
element is 0x53fadc, and the resets of id ranges 0x190.., 0x1b6.., 0x22e.. by loadWorldAssets) are elements of these arrays.

### 4.5 Image slots
`g_images` 0x81ca10: 190 Cache482fd0 objects of 0x58 bytes. arrayStaticInit_43cc10 0x43cc10 (call at 0x43cc26) constructs 0xbe of
them with element constructor 0x482fd0; the same constructor builds the screen manager object at 0x83ac30, which is where the 0x58
size of that object comes from (U-0306). loadInterfaceArt / loadWorldAssets pass 163 distinct addresses (289 calls) to
loadImage, all on the 0x58 grid, indexes 0..188; index 189 is constructed and never loaded. Four calls choose the path at run
time. The 157 stream-A `buffer` clusters at 0x81cac0..0x820ab0 (and 3 neighbours: one record array, one scalar array, one scalar) are slots of this array.

### 4.6 Flic players and Surface arrays
* `g_flicObjs` 0x4f65c8 (591 x 0x84, ctor 0x481b50) and `g_flicStreams` 0x50fc20 (591 x 0x44, ctor 0x482b60); the per-id 0x58-byte
  record table at 0x4e9aa0 (591 x 0x58, ends at 0x4f65c8) is stream E's cluster 494 and is not typed here.
* Nine Surface arrays with constructor counts: 0x59ca58 x 0x50, 0x56a250 x 0x10, 0x540950 x 2, 0x5439e0 x 0x10, 0x5a48e8 x 4,
  0x59b050 x 0x28, 0x53ba48 x 8, 0x5678b8 x 8, 0x541580 x 9 (initGlobalArray_00404c20 / 00404e00 / 00404f50 / 004050b0 / 00405760).
  Singles: 0x822c98, 0x839a68, 0x820e90, 0x840790, 0x8407c0, 0x8407f0 (each a static constructor calling Surface_ctor), and the
  Cursor object 0x822cc8.

### 4.7 Remaining objects
`g_skyActors` (4 x 0x10, .sve field 61), `g_recordBanks` (6 x 0x74), the movie list (serviceMovie 0x49c8e0), `g_fileWinBuf`, the guard
words (bit 0 / bit 1 run-once flags before static destructors) and the scalars in section 1's last row; each has its evidence in
`d_addresses.tsv`.

## 5. Original defects catalogued (not repaired)

1. **Nine sound slots are configured twice** (loadSoundTable 0x448220, later call wins): slot 8 (calls 16 and 63), 16 (47, 172), 17
   (21, 176), 18 (48, 180), 19 (22, 184), 21 (23, 188), 22 (50, 192), 23 (24, 196), 24 (104, 200). The first path of each pair is
   never played (`d_sound_slots.tsv`, column `overwritten_by_later_call`).
2. **resetSpriteQueue 0x4627d0 loops 0x400 times storing -1 to one dword** (0x8371f4) instead of clearing the link array; the
   link array is rebuilt by enqueueSprite so the stale values are never read, but the snapshot head is overwritten.
3. **playSound / startSoundSlot index the 300-slot array with no bound test** (only `-1` is refused, 0x4481b0): a slot number >= 300 reads
   past the array.
4. **collectDrawList 0x47e330 appends without a capacity test** (U-0325) and **serviceMovie 0x49c8e0 has no bound on its id list** (U-0326).
5. **updateWalkers reads one byte of a sheet's frame count** (0x403455 reads the low byte of `cols[400]`): correct only while the
   count is below 256 (U-0328).
6. **addSprite 0x4628d0 draws immediately and has no capacity test**: it writes element `count` of the queue arrays without the 0x3fe guard enqueueSprite has.

## 6. Checker result

`re/tools/model_check.py log/p1/d/d_check.tsv` (109 ranges; the music device is checked as two ranges because the tool has no nested
structs): **0 errors, 34 warnings**, identical with `--all-functions`. Controls (a short at g_screenWidth, 512 elements at
g_spriteSortKey, a const g_spriteX) produce 28 errors. Warnings, all explained:

| warning | count | explanation |
|---|---|---|
| `g_images` stride | 9 | `[idx*8 + base]` forms: the index is premultiplied by 11 before the scale (11 * 8 = 0x58) |
| `g_surfaces5a48e8` stride + width_narrow | 4 + 7 | byte accesses at 0x5a4930 / 0x5a4997 in FUN_00406f20, FUN_00406f90 and generateRandomCourse use a biased base into the level table that starts at 0x5a4998, the byte after this array; they are not Surface fields |
| `base_outside` | 10 | `lea [x + 0x541710]`, `[0x59b778]`, `[0x820f40]`, `[0x567834]`: the next or previous Surface array of another cluster |
| `g_polyFill` width_narrow | 2 | the fill colour is stored as a dword and read as a word (16-bit path) or a byte (8-bit path) |
| `g_sheetCols` width_narrow | 1 | defect 5 above |
| `g_sheetPath` stride | 1 | a byte index inside one path row |

## 7. Hook cross-check

`log/p1/d/hookcheck.py` extracts every `G<T>(0xADDR + K*i)`, `F<T>`, `At<T>`, `Arr(...)` and `Qa(...)` access of the non-negctl
sources in `shim/src/re/*.cpp` that lies in a typed range and compares width, stride and, for `SgRecordBank`, `SgSkyActor` and
`SgPolyFill`, the field offset: **312 accesses in 50 ranges, 0 disagreements**. `hookrest.py` lists 768 more literals in other
forms (casts, tables of `{addr, size, count}`): the constructor-iterator tables of c3co.cpp agree with every Surface array count
and with the Flic arrays (`ctorcheck.py`: 11 of 11 OK); c3cq.cpp line 706 gave the image array count 0xbe (the count 189 first
derived from the highest loaded slot was one short). The hook sources use `G<unsigned char>(0x81b490 + 50 * v)` for the first byte
of path rows, which is the `char[590][50]` view.

## 8. Not typed / belongs to another stream

* Brief names g_textBuffer 0x51a068 (cluster 511), 0x83ad44 (1692), the blit state words 0x83aa78..0x83ab28 (1666, 1667, 1675), the
  `Image` / Display / Surface object layouts and the sfx / voice device objects 0x83ad58 / 0x83af98 (1695, 1703): all assigned to
  stream E in `ownership.tsv` (U-0327).
* 0x55eb40 (the x half of the tile screen cache) is stream B's cluster 665.
* Clusters 814-817 moved to stream C during the work. What this stream read there, for stream C: they are fields of `g_typeAttr`
  (0x578372, stride 0x30): two 16-byte name fields at +0x0e and +0x1e of records 3, 4, 9, 10, 12, 13, 14, 17 that loadWorldAssets
  overwrites per course type (0x440fc7, 0x440fec, 0x441036, 0x441011, 0x4410a5, 0x4410ca, 0x44105b, 0x441080, 0x4410ef, 0x441114,
  0x441139 and their second copies), single bytes at record 4 +0, 5 +4, 18 +6, a dword at 18 +0x0a, a read of record 16 +1
  (signed byte, drawAmenitiesPanel 0x4353a4), a write of record 17 +0 by prepareShot 0x424154 / 0x426619, and 0x578462 / 0x5784c2
  (starts of records 5 and 7) used as loop bounds by applyCourseType 0x449423 / 0x44942a.
* Four building catalog name fields at 0x4c273c, 0x4c2764, 0x4c27a0, 0x4c27c8 are typed here as `char[16]` but are +4 of 0x14-byte
  catalog records stream F owns (U-0313).

## 9. Corrections

Stream A: (1) the 175 `object` clusters 1125-1299 and the 0x6c record array are one array of 300 (section 2.1); (2) the `buffer`
clusters 1342-1502 are elements of one 190-element object array (section 4.5); (3) clusters 569-585 and neighbours are elements of
590-element arrays; (4) cluster 1696 is a 0x1e4-byte object with 16 group records; (5) stream A's element counts were weak, as it
said: the constructor counts above replace its `n=` column for every array typed here.
Older notes: `re/names/exe_0_globals.tsv` describes g_recordBanks with x at +0x40, y at +0x50 and the animation counter at +0x60;
the offsets are +0x20, +0x30 and +0x40, and the occupancy grid (`g_recordOccupancy` 0x4e6d70) is +0x50 of the same 0x74-byte bank, six
banks in all (initRecords 0x401750). The names `HashTable::bucketEmpty` / `bucketSet` and `g_musicDevice` do not describe what the
code does (section 2.3). The brief's 32K conversion table at 0x822c74 is 64K entries (section 4.1).

## 10. Coverage

| measure | done | total |
|---|---|---|
| clusters | 446 | 446 |
| addresses | 529 | 529 |
| referencing-function weight | 869 | 869 |

Every D address lies inside a typed range, but confidence differs: 493 high, 21 medium, 15 low addresses. Ranges whose extent or
meaning is not decided are the 30 U rows. Not done: no run against live memory (static evidence only; the model_check controls and the
hook tables are the validation), and the Snd fields marked U-0320 need sound.dll's voice object.
