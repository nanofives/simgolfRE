# Phase 1 name changes (stream G1, 2026-10-18)

Corrections applied to re/names/*.tsv by `log/p1/g1/apply_names.py` (scratch; backup of the files before it in
log/p1/g1/names_backup_pre_g1/). names_sanitize, names_validate (every touched id) and names_check_evidence pass.
hooks.csv followed through `scripts/re_classify.py retag log/p1/g1/retag.tsv` (24 function rows, levels unchanged,
one CHANGELOG line each). ApplyNames was NOT run and the Ghidra master is untouched: the later step applies these
with `ApplyNames.java <re/names> overwrite` and deletes the withdrawn DATA labels with `DeleteLabels.java` (ApplyNames
resets only functions listed in withdrawn.tsv). Registry keys, fixtures and SG_HOOK name strings keep the old
spellings (rows are looked up by address).

## Function renames (class prefixes follow the model) (24)

| file | address | old | new | evidence | status |
|---|---|---|---|---|---|
| exe_4.tsv | 0x004914d0 | Dialog::subDtor | CompositeView::subDtor | calls CompositeView's destructor 0x00491500 on this+0x1fc (0x004914d4..0x004914dc), ListModel::dtor on this+0x774 (0x004914e1..0x004914e7) and the Window destructor 0x004805a0 on this+0x1fc; no Dialog class exists (DECISIONS 2026-10-18: vtable 0x004bb3f8 is TextView) | applied |
| exe_4.tsv | 0x00491500 | Dialog::viewDtor | CompositeView::dtor | model_e1.md 4.4: CompositeView's destructor (stores the CompositeView vtables at this-0x5c / this-0x1fc) | applied |
| exe_4.tsv | 0x004916f0 | Dialog::scalarDtor | TextView::scalarDtor | slot 0 of TextView's vtable 0x004bb3f8 (re/model/e1_vtables.tsv); calls 0x00491710 | applied |
| exe_4.tsv | 0x00491710 | Dialog::dtor | TextView::dtor | stores TextView's vtables 0x004bb3f8 / 0x004bb3e4 and tears down its members (model_e1.md 4.4; DECISIONS 2026-10-18) | applied |
| undisc_exe.tsv | 0x004918d0 | Dialog::scalarDtor_adj274 | TextView::scalarDtor_adj274 | slot 3 of TextView's Image-base vtable 0x004bb3e4 (re/model/e1_vtables.tsv), adjustor to TextView::scalarDtor 0x004916f0 | applied |
| exe_3.tsv | 0x004804a0 | View4804a0::ctor | Window::ctor | model_e1.md 4.1: the 91-slot vtable 0x004baa30 class is Window (View + Image base at +0x274, stored at 0x004804c4) | applied |
| exe_3.tsv | 0x00480580 | View4804a0::scalarDtor | Window::scalarDtor | slot 0 of Window's vtable 0x004baa30 (re/model/e1_vtables.tsv) | applied |
| exe_3.tsv | 0x004805a0 | Base4805a0::dtor | Window::dtor | model_e1.md 6: Window's destructor (installs vtables 0x004baa30 / 0x004baa1c) | applied |
| undisc_exe.tsv | 0x00481b40 | View4804a0::scalarDtor_adj274 | Window::scalarDtor_adj274 | slot 3 of Window's Image-base vtable 0x004baa1c, adjustor to Window::scalarDtor 0x00480580 (re/model/e1_vtables.tsv) | applied |
| exe_3.tsv | 0x00482ae0 | Decoder482ae0::ctor | S3ListNode::ctor | model_e2.md 7: constructs the S3List node (vbtable 0x004ba28c, HeapObject at +0x14) S3List_add allocates; no decoder | applied |
| exe_3.tsv | 0x00482b20 | Decoder482b20::ctor | ListNode10::ctor | model_e2.md 7: constructs S3List_add2's 0x10-byte node (vbtable 0x004ba2b8 at 0x00482b31, HeapObject at +8) | applied |
| exe_3.tsv | 0x00482fd0 | Cache482fd0::ctor | PaletteCache::ctor | model_e2.md 3 / 7: PaletteCache (0x58 bytes, vtable 0x004babbc): 5 cycle slots set to id -1 (0x00482fe6..0x00482ff5) | applied |
| exe_3.tsv | 0x00483010 | Cache483010::dtor | PaletteCache::dtor | stores PaletteCache's vtable 0x004babbc and tail-jumps to the release body 0x00483070 (row evidence) | applied |
| exe_3.tsv | 0x00483030 | Cache483030::attach | PaletteCache::attach | creates the PaletteCache palette through Display slot 30 into +4 (model_e2.md 3) | applied |
| exe_3.tsv | 0x00483060 | Cache483060::detach | PaletteCache::detach | releases PaletteCache +4 through the display (row purpose) | applied |
| exe_3.tsv | 0x00483070 | Cache483070::reset | PaletteCache::reset | releases +4 through Display slot 36 and the 5 cycle slots (model_e2.md 3) | applied |
| exe_3.tsv | 0x004833f0 | Table483::find | PaletteCache::findCycleSlot | searches the 5 PaletteCycleSlot ids of a PaletteCache (model_e2.md 3) | applied |
| exe_3.tsv | 0x004837f0 | C4837f0::ctor | FontRef::ctor | stores FontRef's vtable 0x004ba468 (0x004837f9); constructor of the 12 g_font* objects (re/names/model_e2_globals.tsv) and of 0x00519a20 | applied |
| exe_3.tsv | 0x00483800 | Surface::createFromId | FontRef::createFromId | model_e2.md 7: a FontRef method (FontRef +4 from Display slot 29), not the 0x2c-byte SpriteRef class | applied |
| exe_3.tsv | 0x00483850 | Surface::createSized | FontRef::createSized | model_e2.md 3 / 7: creates FontRef +4 through Display slot 29 | applied |
| exe_3.tsv | 0x004838b0 | Surface::releaseBacking | FontRef::release | model_e2.md 3 / 7: frees FontRef +4 through Display slot 34; the FontRef atexit routine 0x00404720 tail-calls it | applied |
| exe_3.tsv | 0x00483b90 | Surface::ctorVtbl483b90 | FontRef::dtor | stores FontRef's vtable 0x004ba468 and calls FontRef::release (row evidence), the destructor shape of PaletteCache::dtor | applied |
| exe_4.tsv | 0x00487770 | HashTable::bucketEmpty | SndGroup_isDisabled | model_d.md 2.3: true when group index > 0xf or byte [this + i * 0x1c + 0x24] is 0 (NodeListBucket.enabled, set to 1 by 0x004877ee); Snd::play stops on it | applied |
| exe_4.tsv | 0x004877a0 | HashTable::bucketSet | SndGroup_isMuted | model_d.md 2.3: true when group index <= 0xf and byte [this + i * 0x1c + 0x25] (NodeListBucket.muted) is nonzero | applied |

## Function purpose corrections (2)

| file | address | old | new | evidence | status |
|---|---|---|---|---|---|
| exe_0.tsv | 0x0040a130 | stores grid spacing/offset globals (no live callers) | Stores one tile's build-tool code into tile_build_code 0x005830b8 and its refund amount into tile_refund 0x0059c090 (setGrids(x, y, code, value)); mainLoop calls it 8 times (0x0041f544..0x00420a0f). | model_c.md 13 (the old purpose `no live callers` is wrong) | applied |
| exe_1.tsv | 0x004315e0 | Composes a golfer's bio text (favorite color, likes) into the text buffer. | Resets the new-game state (one run of rep stosd clears about 20 tables: golfers, walkers, histories, award counters, ranking) and composes the bio text of golfer types 0..75 (built in 0x51a068, strcpy'd to record + 0x30, loop end 0x4e06f8 at 0x00431ad7..0x00431af3). | model_b.md 2.4 / 4.2 / 8.11 (the name covers only the second half; the function name is left to stream H) | applied |

## Global labels withdrawn (rows moved to re/names/withdrawn.tsv) (5)

| file | address | old | new | evidence | status |
|---|---|---|---|---|---|
| exe_1_globals.tsv | 0x004ba800 | gRngState (unsigned (LCG state)) | - | gRngState: .rdata double 1/32768, read by Random::next as `fmul qword ptr [0x4ba800]` (0x0045c1ca) and never written; the LCG state is the instance word [ecx] (0x0045c1b8), 0x822d9c for the global RNG (model_f.md 8, model_b.md 7.2) | applied |
| exe_1_globals.tsv | 0x0059d91c | gTextBuffer2Index (int) | - | gTextBuffer2Index: the only reference is the loop end `cmp ecx, 0x59d91c` of appendToBuffer45b8b0 (0x0045b928) one past gTextBuffer2 (0x80 words); nothing reads or writes the dword | applied |
| exe_11_globals.tsv | 0x0056d1d8 | g_animals (struct[100] (0x3c stride: short type, map, ..., +6 / +8 tile x / y, +0xa wait, +0xc state, int +0x10 / +0x14 sub-tile, byte +0x18 heading, +0x19 steps, +0x1a frame)) | - | g_animals: field +0x20 of record 0 of g_animalRecords 0x0056d1b8 (spawnObject stores [i * 0x3c + 0x56d1b8] at 0x0040121b; save field 58 is 100 x 0x3c from 0x56d1b8; model_b.md 8.5, model_c.md 5) | applied |
| exe_0_globals.tsv | 0x00578372 | g_typeAttr (per-tile-type attribute table, stride 0x30 (attr @+0, kind @+4 at 0x00578376)) | - | g_typeAttr: field +0x22 of record 0 of g_tileTypeAttrs 0x00578350 (23 x 0x30, copied from 0x4c1a40 by `rep movsd` at 0x0040f5f7; model_c.md 2, model_inventory.md 5) | applied |
| exe_0_globals.tsv | 0x004e6d70 | g_recordOccupancy (byte[6x6] per-tile occupancy within a bank) | - | g_recordOccupancy: field +0x50 (int8_t[36] occupancy) of bank 0 of g_recordBanks 0x004e6d20 (6 banks of 0x74, initRecords 0x00401750; model_d.md 9) | applied |

## Global labels renamed (11)

| file | address | old | new | evidence | status |
|---|---|---|---|---|---|
| exe_1_globals.tsv | 0x00587da0 | gCourseInfoDirty (int) | g_pendingCourseInfo (char[0xdc]) | FUN_004321a0 copies a string into it (0x004321b5..0x004321c4); flushCourseInfo 0x00432170 tests byte 0, passes it to writeCourseInfo 0x431fa0 and saveThumbAndFull 0x431ee0 (0x00432179..0x00432188) and clears byte 0 (0x00432190) | applied |
| exe_4_globals.tsv | 0x0083d34c | g_polyFillColor (ushort) | g_polyFill (SgPolyFill (0x48 bytes, re/model/include/sections/d.h): fill colour dword at +0) | convex polygon rasteriser state of fillConvexPoly8 / 16, polyEdgeStep, fillSpanDown / Up 0x00493000 / 0x00493080 (model_d.md 4.3); the colour is stored as a dword and read as a word or a byte | applied |
| exe_5_globals.tsv | 0x008400b0 | g_netState (int (net state; nonzero)) | g_netInterface (interface pointer (never stored nonzero by the game logic)) | loaded and called through its vtable (inventory ptruse vtblval); netBeginOp0/1 0x00497c20 / 0x00497c70 test it; netSendShutdown stores 0 at 0x00497d7a; netSendMessages passes it as ecx (model_e2.md 7) | applied |
| exe_5_globals.tsv | 0x0083ab30 | g_scrollDragX (int drag anchor) | g_windowSlotA (Window*) | layoutNode 0x00479f78 and Window::detach 0x0047b880 compare it with the window being removed, store edi into it and call that window's slot +0x10 (0x00479f80..0x00479f8a, 0x0047b887..0x0047b891) (model_e1.md 6) | applied |
| exe_5_globals.tsv | 0x0083ab34 | g_scrollDragY (int drag anchor) | g_windowSlotB (Window*) | layoutNode 0x00479f8d / 0x00479f95 and Window::detach 0x0047b894 compare it with the window being removed and clear it (model_e1.md 6) | applied |
| model_f_globals.tsv | 0x004c1a40 | f_strpool_4c1a40 (char[5]) | f_tab_tileTypeAttrInit_4c1a40 (const TileTypeAttr[23] (0x30 each)) | file image of g_tileTypeAttrs: mainLoop and loadWorldAssets copy 0x114 dwords from it to 0x578350 (rep movsd at 0x0040f5f7, 0x00440f9a, 0x004411d6, 0x004412cf, 0x004414a6) | applied |
| model_e2_globals.tsv | 0x00561250 | g_sve561250 (int32_t) | g_courseReportScore (int32_t (save field 25)) | showCourseReport stores the report score at 0x00452dc1 (`mov [0x561250], ebx`) only when > 0 (0x00452cc8); no reader outside the serializers (0x0040b0f8 / 0x0040bd54) (re/analysis/data/sve_field_status.md 3) | applied |
| model_e2_globals.tsv | 0x005a6364 | g_sve5a6364 (int32_t) | g_buildingUnlockLevel (int32_t (save field 29)) | 6 at a new game (0x0040fbe7), 0x11 with the amenity mask 0x3fff (0x0040fc02, 0x00421420), +1 in announceBuilding (0x0040efef..0x0040eff0); bound of pickBuilding (`cmp eax, [0x5a6364]; jl` 0x004342ef) and drawBuildingPanel 0x00434703 (sve_field_status.md 3) | applied |
| model_f_globals.tsv | 0x004c2e14 | f_w_4c2e14 (int32_t[2]) | g_proChallenger (int32_t (save field 46; index into g_golfPros, -1 = none)) | set at 0x0041037d after the prompt 0x0040cb00 accepts, -1 at 0x0040f48b; tested against -1 at 0x0040fffe, 0x004360e7, 0x004361fc, 0x00436748; no instruction references 0x4c2e18 (sve_field_status.md 3) | applied |
| exe_1_globals.tsv | 0x0056fcb0 | gTextBuffer (char[]) | g_namePool (char[0x1002] (save field 76)) | string pool: clearBuffers zeroes it (0x0045b881..0x0045b88f); appendToBuffer45b8b0 bounds it at 0x1002 (0x0045b992) and copies strings in; appendString 0x0045b9f0 reads pool + offset; slot 0 course name, 1..18 hole names, 20..29 best-round holders (sve_field_status.md 3) | applied |
| exe_1_globals.tsv | 0x0059d81c | gTextBuffer2 (char[]) | g_namePoolOffset (int16_t[128] (save field 77; -1 = free)) | clearBuffers fills -1 (0x0045b891..0x0045b89e); appendToBuffer45b8b0 reads [slot * 2] (0x0045b8bd), clears it (0x0045b907), lowers the offsets above (loop end 0x59d91c, 0x0045b928) and stores the new one (0x0045b9bb) | applied |

## Global declared types corrected (name kept) (26)

| file | address | old | new | evidence | status |
|---|---|---|---|---|---|
| exe_0_globals.tsv | 0x004e6d20 | g_recordBanks (record banks, 0x74 bytes/bank, 16 slots (id short @+0, x @+0x40, y @+0x50, animCounter @+0x60)) | g_recordBanks (SgRecordBank[6] (0x74 each: int16 id[16] @+0, int8 x[16] @+0x20, y[16] @+0x30, anim[16] @+0x40, occupancy[36] @+0x50)) | written by placeRecord 0x00401040, read by drawRecords 0x004012d0; six banks reset by initRecords 0x00401750; offsets model_d.md 9 (re/model/include/sections/d.h SgRecordBank) | applied |
| exe_0_globals.tsv | 0x0053caf0 | g_tileFlags (byte[] tile flags, stride 0x32 per row (bit 0x40=visited, 0x20, 0x800)) | g_tileFlags (uint16_t[50][50] tile flag words (TF_* bits in re/model/include/sections/c.h), row stride 0x64 bytes) | word index (x * 50 + y) * 2 (floodMark 0x0042f120, mainLoop 0x00413dbb); save field 19 (0x0053caf0, 5000 bytes) 0x0040b09c | applied |
| exe_2_globals.tsv | 0x00838c1c | g_sampleGrid (char[]) | g_sampleGrid (int8_t[19][19] (17 x 17 used)) | 0x13-stride signed grid bilinearly sampled by bilinearSample 0x004674c0; genNoise writes rows 1..16 through the biased base 0x838c1b (0x0046746e); model_c.md 6 / 13 | applied |
| exe_0_globals.tsv | 0x0058bcb8 | g_placed_objects (struct{short type;short x;short y;short pad;int variant}[256] (0x10 stride)) | g_placed_objects (PlacedObject[256] (0x10 each, re/model/include/sections/c.h)) | read by objectAt 0x0040df80 (type @+0, x @+2, y @+4) and holeQuadrants 0x00407000; layout model_c.md 5 | applied |
| exe_3_globals.tsv | 0x0083ad80 | g_musicDevice (SoundDevice) | g_musicDevice (NodeList (0x1e4 bytes: StrList, 16 NodeListBucket sound groups at +0x24)) | constructed by staticInit_g83ad80 (0x483de0), loaded by initSound (0x483e90); group bytes read by SndGroup_isDisabled 0x00487770 / SndGroup_isMuted 0x004877a0, gain by Snd::setVolume (0x83ada8 + group * 0x1c); model_d.md 2.3, model_e2.md 3 | applied |
| exe_3_globals.tsv | 0x0083ad58 | g_sfxDevice (SoundDevice) | g_sfxDevice (StrList (0x20 bytes; sound.dll device at +0x14)) | constructed by staticInit_g83ad58 (0x483e20), loaded by initSound (0x483e90); NodeListB vtable (model_e2.md 7) | applied |
| exe_3_globals.tsv | 0x0083af98 | g_voiceDevice (SoundDevice) | g_voiceDevice (StrList (0x20 bytes; sound.dll device at +0x14)) | constructed by staticInit_g83af98 (0x483e60), loaded by initSound (0x483e90); NodeListC vtable (model_e2.md 7) | applied |
| exe_3_globals.tsv | 0x0083a7b8 | g_blitSaveBuf (void*) | g_blitSaveBuf (Image (0x2b8)) | the Image gImageCtor_83a7b8 0x00479a40 constructs; blitted by addDirtyRect (0x47d060) and compositeScreen (0x47fab0) (model_inventory.md 4.2, model_e2.md 7) | applied |
| exe_1_globals.tsv | 0x00820f18 | gTileBuffer (void*) | gTileBuffer (StdList (0xc: std::list header)) | constructed by initList820f18 0x00449210; read by rebuildTileData 0x0044a5b0 (model_e2.md 7) | applied |
| exe_5_globals.tsv | 0x008406e8 | g_pairArray (int-pair array ptr) | g_pairArray (PairArray (0x1c)) | initialized by initPairArrayGlobal 0x0049d010 via PairArray::ctor 0x0049d050 (model_e2.md 7) | applied |
| exe_5_globals.tsv | 0x00840930 | g_logFile (FILE* log handle) | g_logFile (Logger (0xc)) | passed as `this` by initLogFile 0x004a0130 (0x004a0135, 0x004a0339); used by logWrite 0x004a0280 (model_inventory.md 4.2, model_e2.md 7) | applied |
| exe_3_globals.tsv | 0x0083acb0 | g_screenMgrA (ScreenMgr) | g_screenMgrA (PaletteCache (0x58)) | constructed by staticInit_g83acb0 (0x482e80) with PaletteCache::ctor; its palette is passed to every Sprite create (model_e2.md 3) | applied |
| exe_3_globals.tsv | 0x0083ac88 | g_screenObjList (List) | g_screenObjList (ScreenObjList (0x28; HeapObject at +0x20)) | cleared by clearScreenObjects (0x483340), initialized by staticInit_g83ac88 (0x482f00) (model_e2.md 7) | applied |
| exe_5_globals.tsv | 0x00840710 | g_pcxSurface (gfx surface object) | g_pcxSurface (SpriteRef (0x2c)) | written by loadPcxSurface 0x0049d1b0 and initGfxGlobal_840710 0x0049d160 (model_e2.md 7) | applied |
| exe_5_globals.tsv | 0x00840830 | g_cursorSurfA (cursor gfx surface) | g_cursorSurfA (SpriteRef (0x2c)) | loadCursors 0x0049e9d0 / freeCursors 0x0049ead0 (model_e2.md 7) | applied |
| exe_5_globals.tsv | 0x00840860 | g_cursorSurfB (cursor gfx surface) | g_cursorSurfB (SpriteRef (0x2c)) | loadCursors 0x0049e9d0 / freeCursors 0x0049ead0 (model_e2.md 7) | applied |
| exe_5_globals.tsv | 0x00840890 | g_cursorSurfC (cursor gfx surface) | g_cursorSurfC (SpriteRef (0x2c)) | loadCursors 0x0049e9d0 / freeCursors 0x0049ead0 (model_e2.md 7) | applied |
| exe_5_globals.tsv | 0x0083fe78 | g_widgetTable (0x60-entry table) | g_widgetTable (uint32_t[38] callback words) | copied into WidgetBase +4..+0x98 by initWidgetTable 0x00495d38..0x00495e9d; reset by resetWidgetTable 0x00495eb0 (model_e1.md 6) | applied |
| exe_12_globals.tsv | 0x00839348 | g_loadImage (Image) | g_loadImage (Image (0x2b8 bytes, vtable 0x004ba858)) | the Image that Image_draw 0x00473e60 and Surface_drawScaled 0x00473cb0 load into with loadImage; size from operator new 0x2b8 at 0x0048077b (model_e1.md 6) | applied |
| exe_6_globals.tsv | 0x0059fc60 | g_storyPairs (story-pair table, 10 entries of 0x388 bytes (golfer idx @+0, partner idx @+2, event counter @+4)) | g_storyPairs (ShotTrack[16] (0x388 each, re/model/include/sections/b.h)) | save field 54 (0x3880 bytes); addGolferPair 0x004099f0 opens a track, appendPairTrackPoint appends a point (model_b.md 5.1) | applied |
| exe_2_globals.tsv | 0x00571fd8 | g_courseTable (byte[]) | g_courseTable (CourseRecord[17] (0x2e each; 16 seeded)) | seedCourseTable 0x0046f2b0 seeds 16 slots; save field 72 is 0x30e = 17 x 0x2e (model_b.md 6.1) | applied |
| exe_4_globals.tsv | 0x0083b170 | g_keyTables (KeyTable[16]) | g_keyTables (KeyTable[4] (0x120 each)) | initKeyTableArray pushes count 4 to ehVectorConstructorIterator (0x00487e1a); KeyTable::lookup ends at 0x83b5f0 = 0x83b170 + 4 * 0x120 (0x0048844d) | applied |
| exe_1_globals.tsv | 0x0059d81c | gTextBuffer2 (char[]) | gTextBuffer2 (int16_t[0x80] offsets into gTextBuffer (-1 = empty)) | save field 77; clearBuffers fills it with -1 (0x0045b891..0x0045b89e); appendToBuffer45b8b0 reads [k * 2 + 0x59d81c] (0x0045b8bd) and rewrites the offsets (0x0045b916..0x0045b92e) | applied |
| exe_1_globals.tsv | 0x0056fcb0 | gTextBuffer (char[]) | gTextBuffer (char[0x1002]) | save field 76 (0x1002 bytes); clearBuffers zeroes 0x400 dwords + 1 word (0x0045b881..0x0045b88f); appendString 0x45b9f0 writes it | applied |
| exe_3_globals.tsv | 0x0083ac10 | g_resampleBuf (void*) | g_resampleBuf (int16_t*) | malloc'd by allocResampleTable (0x004818c0), words written / read through it (0x00481a8b, 0x00481ab5); freed by freeResampleTable 0x481b20 | applied |
| exe_4_globals.tsv | 0x0083b9f4 | g_sinTable (int[]) | g_sinTable (int32_t[256] quarter-wave sine table (entry 255 = sin(pi/2) * 65535)) | initSinTable 0x00491c10 fills 256 dwords (esi to 0x83bdf4); sinScaled 0x00491c70 reads [i] and [i + 1] for i = 0..255 (0x00491ca0 / 0x00491ca7) | applied |

## New global names (re/names/model_g1_globals.tsv) (12)

| file | address | old | new | evidence | status |
|---|---|---|---|---|---|
| model_g1_globals.tsv | 0x004ba800 | - | f_k_dbl_4ba800 (double (1/32768)) | Random::next 0x0045c1a0 scales by `fmul qword ptr [0x4ba800]` (0x0045c1ca); .rdata, no writer (model_f.md 8) | applied |
| model_g1_globals.tsv | 0x004ba2a0 | - | f_k_dbl_4ba2a0 (double (1.0)) | allocResampleTable fsub / fdivr / fld qword [0x4ba2a0] (0x00481906, 0x00481919, 0x00481931); bytes 0 / 0x3ff00000 | applied |
| model_g1_globals.tsv | 0x00509688 | - | g_animSprites (SpriteRef[591]) | initGlobalArray_00403fe0 ehVectorConstructorIterator(0x509688, 0x2c, 0x24f, Surface_ctor 0x473ab0) (0x00403fe0..0x00403ff6); FUN_00404010 destroys it | applied |
| model_g1_globals.tsv | 0x00540d98 | - | g_sprites540d98 (SpriteRef[32]) | initGlobalArray_00404f50 ehVectorConstructorIterator(0x540d98, 0x2c, 0x20, Surface_ctor 0x473ab0) (0x00404f50..0x00404f63) | applied |
| model_g1_globals.tsv | 0x0053e638 | - | g_saveIoRead (int32_t (nonzero = read)) | saveFieldIo 0x0040af70 calls __read 0x4a583a when nonzero, __write 0x4a5b58 when zero (0x0040af7f / 0x0040af88); serializeCourse 0x0040afab / deserializeCourse 0x0040bbfb store their argument | applied |
| model_g1_globals.tsv | 0x0056125c | - | g_jpegSrcPitch (int32_t (row pitch in pixels)) | saveCourseJpg stores the source Surface's slot +0xe0 result at 0x00431dda; FUN_00431be0 advances by 2 * value per 16-bit row (0x00431c42, 0x00431c4e) | applied |
| model_g1_globals.tsv | 0x0083be04 | - | g_dataDirA (char[0x100] (never written)) | resolveDataPath tests byte 0 at 0x00491f53 and copies it (0x00491f71); the literals 0x00491f54 / 0x00491f61 are the only ones in the image | applied |
| model_g1_globals.tsv | 0x0083bf04 | - | g_dataDirB (char[0x100] (never written)) | resolveDataPath tests byte 0 at 0x00491e38 and copies it (0x00491e5c); literals only at 0x00491e39 / 0x00491e4c | applied |
| model_g1_globals.tsv | 0x0083c104 | - | g_dataDirC (char[] (never written)) | resolveDataPath copies it (mov edi, 0x83c104 0x00491ecf; repne scasb 0x00491ee0); the literal 0x00491ed0 is the only one in the image | applied |
| model_g1_globals.tsv | 0x00542fd8 | - | g_moneyLabelX (int32_t[8]) | pointsPopup stores [ring * 4 + 0x542fd8] (0x0040c8cf), ring & 7 (0x0040c8f4); drawMoneyLabels reads it (0x0040c94e) | applied |
| model_g1_globals.tsv | 0x00542ff8 | - | g_moneyLabelY (int32_t[8]) | pointsPopup stores [ring * 4 + 0x542ff8] (0x0040c8da); drawMoneyLabels reads it (0x0040c936) | applied |
| model_g1_globals.tsv | 0x005a46b8 | - (map symbol g_flagWords5a46b8) | g_namePoolLength (int16_t[128] (save field 78)) | appendToBuffer45b8b0 stores strlen + 1 (`mov word ptr [ebp*2 + 0x5a46b8], dx` 0x0045b9ca) and takes the pool end as max(offset + length) (`movsx edx, word ptr [eax*2 + 0x5a46b8]` 0x0045b949..0x0045b954) | applied |

## Address-map symbols replaced by the names-file label (55)

The map takes the label at an object's start (names files are the one naming source); nothing changes in re/names.

| va | stream | stream symbol | map symbol |
|---|---|---|---|
| 0x004c2844 | c | zoom_scale | g_zoomScale |
| 0x004c2ba0 | c | camera_x | gCameraTileX |
| 0x004c2ba4 | c | camera_y | gCameraTileY |
| 0x004c2e14 | f | f_w_4c2e14 | g_proChallenger |
| 0x004d2044 | c | terrain_init_pending | g_terrainInitPending |
| 0x004d2048 | c | terrain_last_cam_x | g_terrainLastCamX |
| 0x004d204c | c | terrain_last_cam_y | g_terrainLastCamY |
| 0x004d2050 | c | terrain_last_zoom | g_terrainLastZoom |
| 0x0051b770 | c | corner_cache | g_cornerCache |
| 0x0053bbac | c | tile_path_cost | g_tilePathCost |
| 0x0053caf0 | c | tile_flags | g_tileFlags |
| 0x00541318 | c | hole_dir_mask | g_holeDirMask |
| 0x00541ce0 | c | top_scores | g_topScores |
| 0x00542414 | c | tile_type_saved | g_tileTypeSaved |
| 0x00543018 | c | tile_level | g_tileLevel |
| 0x00561250 | e2 | g_sve561250 | g_courseReportScore |
| 0x005685f4 | c | camera_rot | g_cameraRot |
| 0x005685f8 | c | hole_notice_mask | g_holeNoticeMask |
| 0x005685fc | c | camera_target_y | g_cameraTargetY |
| 0x0056949c | c | accuracy_skill | g_accuracySkillX100 |
| 0x0056988c | c | tile_variant | g_tileVariant |
| 0x0056c7e4 | c | tile_mood_down | g_tileMoodDown |
| 0x0056d1b8 | c | animals | g_animalRecords |
| 0x005722e8 | c | tile_type | g_tileType |
| 0x00575ab0 | c | holes | g_holes |
| 0x00578350 | c | tile_type_attr | g_tileTypeAttrs |
| 0x005830b8 | c | tile_build_code | g_tileBuildCode |
| 0x00586b50 | c | edit_queue_x | g_editQueueX |
| 0x00586fa8 | c | edit_queue_y | g_editQueueY |
| 0x0058bcb8 | c | placed_objects | g_placed_objects |
| 0x0058d36c | c | course_length | g_courseYards |
| 0x0059aafc | c | course_par | g_coursePar |
| 0x0059ae78 | c | fun_rating | g_funRating |
| 0x0059c090 | c | tile_refund | g_tileRefund |
| 0x005a46b8 | d | g_flagWords5a46b8 | g_namePoolLength |
| 0x005a47e4 | c | last_edit_tile_y | g_lastEditTileY |
| 0x005a4998 | c | corner_level | g_cornerLevel |
| 0x005a6364 | e2 | g_sve5a6364 | g_buildingUnlockLevel |
| 0x005a636c | c | imagination | g_imaginationX100 |
| 0x005a6378 | c | tile_mood_up | g_tileMoodUp |
| 0x005a882c | c | length_skill | g_lengthSkillX100 |
| 0x005a8834 | c | edit_queue_mode | g_editQueueMode |
| 0x00820ed0 | c | terrain | gTerrain |
| 0x00820ed8 | c | surface_820ed8 | g_surface820ed8 |
| 0x00820f14 | c | terrain_list_cursor | g_terrainListCursor |
| 0x00822c78 | c | course_id | g_courseId |
| 0x00822da0 | c | tile_shade_a | g_tileShadeA |
| 0x0082376c | c | tile_shade_b | g_tileShadeB |
| 0x00838aa0 | c | noise_source | g_noiseSource |
| 0x00838c1c | c | sample_grid | g_sampleGrid |
| 0x00838eac | c | itoa_scratch | g_itoaScratch838eac |
| 0x0083ab30 | e1 | g_scrollDragX | g_windowSlotA |
| 0x0083ab34 | e1 | g_scrollDragY | g_windowSlotB |
| 0x0083ad80 | d | g_sound_device_music | g_musicDevice |
| 0x008400b0 | e2 | g_netState | g_netInterface |

## Conflicts checked with no change

- Same global name at two addresses, a global named like a function, two labels at one address: none (log/p1/g1/names_scan.py).
- Interior labels kept because their type is the field itself: gThoughtTable 0x579540 (golfer +0x88), g_golferTypeSlot
  0x57956e (golfer +0xb6), g_polyVerts 0x83d358 / g_polyVertCount 0x83d378 (g_polyFill +0x0c / +0x2c), g_playerSkillMask
  0x4c2c9c (g_skillMaskBlock +8), g_resCloseHook1..3 0x83af80 / 88 / 90 (g_soundFns elements 5 / 7 / 9),
  g_objectOfferCount / g_greensFee (f_w_4c2848 elements 1 / 2), F's suffix strings and record-76 tail-A labels.
- 0x822c74 (g_halfBrightLut: 0x10000 words, a 64K halving table) and 0x822c78 (g_courseId, a random course id) are named
  correctly in model_d / model_c_globals; the wrong descriptions are prose: CLAUDE.md's b2 paragraph calls 0x822c78 the
  `title ticker` and re/PHASE1_PLAN.md stream D calls 0x822c74 a `32K conversion table` (for the parent to edit).
- Left to stream H (function naming, evidence in the stream notes): buildGolferBioText 0x4315e0 (purpose corrected; it
  also resets the new-game state), Image::seek / Image::step 0x482490 / 0x482420 (take a Flic, U-0460 open),
  Factory::destroyPalette / destroySurface (U-0454 open), HashTable::remove 0x4876c0 (not reported by a stream).

Stream G3 (2026-10-18) renames and new labels: re/analysis/data/model_corrections.md section 8.
