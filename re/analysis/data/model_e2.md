# Data model stream E2: object layouts of the engine classes (phase 1, 2026-10-17/18)

Stream E2 of `re/PHASE1_PLAN.md` (brief `re/model/STREAM_BRIEF.md`, ownership `re/model/ownership_e.tsv`, 277 clusters
and 403 addresses with stream E2). The engine classes the exe builds as global objects or on the heap, their vtables,
and the API boundary the port needs to replace jgld.dll, Terrain.dll and sound.dll. NO-GUESSING applies: a field whose
meaning no instruction shows has a neutral name (`f_XX`) and a U row; a field nothing touches is `unk_XX`.

## 1. Files

| file | content |
|---|---|
| `re/model/include/sections/e2.h` | 29 structs, C99, pointers as `uint32_t`, `sizeof` and `offsetof` asserts for every member |
| `re/model/e2_classes.tsv` | the same layouts as rows (class, size, offset, type, count, field, meaning and evidence) |
| `re/model/e2_addresses.tsv` | 153 typed globals (symbol, va, size, element type, count, stride, sign, .sve field, clusters, evidence, confidence) |
| `re/model/e2_vtables.tsv` | 975 slot rows: 44 exe vtables / vbtables of the E2 clusters (557 rows) and 10 jgld vtables the exe calls (418 rows) |
| `re/model/e2_dll_api.tsv` | 223 rows: jgld Display (86) and Surface (60) slots, jgld's export, the exe's callback table handed to jgld (14), sound.dll's 12 exports and the 25 Sound-object slots the exe calls, the 26 Terrain.dll imports |
| `re/model/e2_uncertain.tsv` | U-0450..U-0487 (37 rows; U-0484 withdrawn with HotList) |
| `re/names/model_e2_globals.tsv` | 130 new global names (existing names kept, section 7) |
| `log/p1/e2/` | scratch: `e2scan.py` (objects, allocations, `this` field scans), `vcalls.py` (virtual calls by object kind), `gen_tables.py` (vtables + API), `gen_model.py` (one spec -> header, address map, names, U rows, model_check input), `hookcheck.py`, `coverage.py`, decompilations and scan outputs |

Regenerate: `~/bin/py -3.12 log/p1/e2/e2scan.py objs`, `log/p1/e2/vcalls.py > log/p1/e2/vcalls.tsv`,
`log/p1/e2/gen_tables.py`, `log/p1/e2/gen_model.py`; check with `re/tools/model_check.py log/p1/e2/check_e2.tsv`,
`log/p1/e2/hookcheck.py`, `log/p1/e2/coverage.py`.

## 2. Method

- **Global objects** (`e2scan.py objs`): every `mov ecx, imm` / `lea ecx, [imm]` followed by a `call` or tail `jmp`
  (thiscall on a fixed address), and every `ehVectorConstructorIterator` 0x004a55d2 call with its five pushed arguments
  (base, element size, count, constructor, destructor). 66 such arrays exist; 60 are `Surface_ctor` 0x00473ab0 arrays
  of 0x2c-byte elements, the others are the 591-entry animation arrays (Cache482fd0 0x58, Flic 0x84, Flic base 0x44,
  Surface 0x2c), Snd[300] at 0x0080d840, Cache482fd0[190] at 0x0081ca10, KeyTable[4] at 0x0083b170 and the
  56,640-element SpriteRef area 0x005aaa30 (ends exactly at 0x0080b130).
- **Object sizes**: the element size of those arrays, `push N; call operator_new 0x004a57f1` followed by the result in
  ecx at a constructor call (`e2scan.py allocs`: Image 0x2b8, Cache482fd0 0x58, Timer 0x30, MappedFile 0x14, ...),
  `heapAlloc` sizes (S3List nodes 0x1c / 0x18 / 0x64), and the spacing of consecutive statically constructed globals.
- **Fields** (`e2scan.py fields <methods>`): straight-line tracking of `this` (ecx at entry, register copies, `lea`
  moves) over every method of a class; per offset the access widths, reads / writes, movsx / movzx, stored immediates
  (vtable installs) and virtual calls through `this` or through a pointer member. Meanings come from the constructors
  and the decompilations of the methods (log/p1/e2/decomp_ctors.c), and from the re/names purposes, each re-read
  against the instruction it cites.
- **Virtual calls** (`vcalls.py`): register kinds (`this`, `*0x...` a global's value, `K[0x..]` a member) followed to
  `call [vptr + K]`; 920 indirect call sites (436 through a vtable slot), 132 through the display global 0x0083ad50.

## 3. Classes

| class | size | size from | vtable(s) | instances |
|---|---|---|---|---|
| SpriteRef | 0x2c | 60 ehVector arrays of 0x2c, `Surface_ctor` | 0x004ba2d8 | 39 arrays (incl. the 5 cross-owned ones) and 17 single globals typed here, 1,574 elements; 2 SpritePair, 1 SpriteRow5 |
| Image | 0x2b8 | operator new 0x2b8 + Image_ctor (layoutViewTree, Window::create) | 0x004ba858 (0x004ba86c during member construction) | 8 globals; one inside every window at +0x274 (E1) |
| FontRef | 0x20 | 11 statics 0x20 apart | 0x004ba468 | 11 globals |
| PaletteCache | 0x58 | operator new 0x58 + Cache482fd0::ctor; ehVector 591 / 190 | 0x004babbc | g_animPalettes[591], 4 globals |
| PaletteCycleSlot | 0x10 | Cache482fd0::ctor loop of 5 | - | inside PaletteCache |
| HotList (stream E1) | 0x60 | Image_ctor builds two at +0xb4 / +0x254 | 0x004bba74 (E1) | embedded in Image as opaque 0x60-byte members; E1 owns the layout |
| Timer | 0x30 | operator new 0x30 + Timer::ctor (allocColorRange) | 0x004baea0 | in PaletteCycleSlot, HotList |
| FlicFrame | 0x44 | ehVector 591 x 0x44 (D: g_flicStreams) | 0x004ba2dc | Flic base |
| Flic | 0x84 | ehVector 591 x 0x84 (D: g_flicObjs) | 0x004ba2e0 | - |
| Snd | 0x6c | ehVector 300 x 0x6c (D: g_sounds) | 0x004badec -> 0x004bac68 -> 0x004bacf8 | D's g_sounds |
| VoiceRxNode | 0x64 | heapAlloc 100 in S3List_addVoiceRx | 0x004bbc3c, vbtable 0x004bbcd4, HeapObject 0x004bbc38 | heap |
| BinkPlayer | 0x1c | g_bink 0x005aa6d0 is followed by a PaletteCache at 0x005aa6f0 | 0x004baea8 | g_bink |
| StrList (NodeListB / NodeListC) | 0x20 | g_sfxDevice 0x0083ad58 .. next object | 0x004baeac / 0x004bafb0 / 0x004bb014 | g_sfxDevice, g_voiceDevice |
| NodeList / NodeListBucket | 0x1e4 / 0x1c | NodeList::ctor ehVector(this + 0x24, 0x1c, 16) | 0x004baf00 / 0x004bafac | D's g_sound_device_music 0x0083ad80 |
| Reader | 0x164 | Reader::ctor fields to +0x160; KeyTable array at 0x0083b170 | 0x004bb084 | g_sectionReader |
| KeyTable / Buf / PairArray | 0x120 / 0x18 / 0x1c | ehVector 4 x 0x120; Buf at KeyTable+0x108 | 0x004bb088 / 0x004ba84c / 0x004bbe6c | KeyTable[4] (other stream's cluster), g_pairArray |
| MappedFile | 0x14 | operator new 0x14 (Flic::open) | 0x004bba78 | heap |
| Logger | 0xc (U-0467) | loggerCtor / logWrite | 0x004bc034 | g_logFile |
| ScreenObjList | 0x28 | g_screenObjList 0x0083ac88 .. g_screenMgrA 0x0083acb0 | 0x004babac, vbtable 0x004babb4, HeapObject +0x20 | g_screenObjList |
| HeapObject | 0x8 | virtual base installed through vbtables | 0x004ba278 and copies | in S3ListNode, ListNode10, ScreenObjList, VoiceRxNode |
| S3List / S3ListNode / ListNode10 | 0x1c / 0x1c / 0x10 | S3List_add's heapAlloc sizes; Decoder482ae0 / 482b20 ctors are the node ctors | node vbtables 0x004ba28c / 0x004ba2b8 | heap (lists embedded in the E1 model classes) |
| StdList | 0xc | initList820f18 | - | gTileBuffer |
| SpritePair / SpriteRow5 | 0x58 / 0xdc | gSurfaceCtor_44ae60 / gSurfaceCtor_44b6f0 | - | 0x00821070, 0x008210c8 / 0x00821120 |
| StoryFileRec | 0x10c | stride 0x10c, loop end 0x00838a88 = 8 records | - | g_storyFiles[8] |

Notes on the main ones (full field lists with evidence in e2.h / e2_classes.tsv):

- **SpriteRef** (the class re/names calls Surface_* / Image_draw / Sprite_draw): +4 is a jgld **Sprite**: created
  through Display slot 32 (`createSprite`, Surface_create 0x00474550, Surface_init2, Surface_copyFrom, cropSprite) and
  freed through slot 37 (`destroySprite`, Surface_free 0x00473afa). +8 is the malloc'd file name of a deferred load
  (Surface_open 0x00473b50; Image_draw 0x00473e67 loads it through g_loadImage when set), +0xc a PaletteCache, +0x10 /
  +0x14 the size (stored twice, +0x18 / +0x1c, U-0451), +0x20 / +0x24 the origin added to every destination. +0x28 is
  never accessed (U-0450). Surface_create and Surface_init2 pass g_screenMgrA's palette (0x0083acb4) to the new Sprite.
- **Image** (0x2b8): +4 a jgld **Surface** (slot 31 `createSurface` in Surface_createFull 0x00474dd0, released through
  slot 35 in Image_releaseAll 0x00474cb8), +8 the depth mode, +0x5c the text font (a holder whose +4 must be set:
  setDrawTarget 0x004762d0), +0x60..+0x68 three text parameters (U-0457), +0x6c a 4 x 4 colour table (row r at
  +0x6c + 0x10 r, column q written by Widget_setQuad / _70 / _74 / _78), +0xac a PaletteCache, two HotLists at +0xb4 /
  +0x254 with 40 ids (-2 = free) and 40 malloc'd buffers between them. Button::setColorNormal / Hover / Pressed
  (re/names) write a colour quad through `this + 0x274`, the window's Surface sub-object (CLAUDE.md, M36WinSurface):
  whether that sub-object is a complete embedded Image is stream E1's to settle (Window::create also allocates a
  separate 0x2b8 Image at 0x0048077b).
- **FontRef** (methods named Surface::createSized / createFromId / releaseBacking): +4 is made by Display slot 29 and
  freed by slot 34 (jgld calls them Factory::createFont / destroyFont, U-0453).
- **PaletteCache** (Cache482fd0 / Cache483010 / Cache483030 / Cache483060 / Cache483070 / Table483 in re/names are one
  class): +4 the jgld palette from Display slot 30, five colour-cycle slots of {id (-1 free), Timer*, 2 bytes, buffer}
  that allocColorRange 0x00483190 / startColorCycle 0x004832e0 fill and start.
- **Snd** equals stream D's `SgSnd` field for field (D typed g_sounds first); E2 adds the base-class split (0x34-byte
  SndRoot vtable 0x004badec, 0x58-byte base vtable 0x004bac68 with 33 slots, 61-slot most-derived vtable 0x004bacf8),
  the prev / next direction (+0x48 prev: a null prev moves the head 0x0083af68, Snd::unlinkFromList 0x004844e9) and the
  float +0x60 = 1.0 written by Snd::ctorDerived 0x00484850.
- **Flic / FlicFrame**: FlicFrame embeds a SpriteRef at +8; Image::seek returns `frame + 8`. The fields Image::seek /
  Image::step read (+0x48 chunk table, +0x58 header with ushort +0x60 / +0x62 and the mask +0x70) are Flic's (Flic::open
  writes +0x58 at 0x0048236d), so those two functions take a Flic, not an Image (U-0460).
- **Net**: g_netSession 0x008400b8 has four references, all reads (Window::onCommand 0x0047ee78, 0x00495d22,
  gfxFlushA / B); g_netState 0x008400b0 is stored once, with 0 (netSendShutdown 0x00497d78). Both are pointers to
  objects called through vtables (the interface: slot 2 with 1 push, slot 4 with 1, slot 21 with 4, slot 26 with 6, the
  object pushed as first argument). No instruction in golf_clean.exe stores a nonzero value in either, so statically the
  network layer 0x00497b40..0x0049bec0 never runs with a session; the session object's layout (fields up to +0x780 from
  netSendShutdown) is listed under U-0469 and not typed as a struct.

## 4. Vtables (`e2_vtables.tsv`)

The 44 exe vtable / vbtable clusters of E2 are dumped slot by slot with the current best names (re/names first; the
7-byte stub 0x004a6ab8 `push 0x19; call; pop` is written `_purecall`: all three slots of Image's construction vtable
0x004ba86c point to it). vbtable rows give the virtual-base offset instead of a function. Slot counts are stream A's
(the run of function entries up to the next installed start). The root-window (0x004ba2e8 / 0x004ba2fc) and the
106-slot dialog vtable (0x004bb3e4 / 0x004bb3f8) are in E2's clusters and dumped here, but the window layouts are
stream E1's. The 10 jgld vtables (Display / DisplayBase 86, Surface / SurfaceBase 60, Sprite / SpriteBase 49, Palette /
PaletteBase 9, Font / FontBase 5) are dumped as the API contract.

## 5. DLL API boundary (`e2_dll_api.tsv`)

- **jgld.dll**: one export, `get_graphsy_object_ptr` (ordinal 1); everything else goes through the Display object
  (g_graphicsDevice 0x0083ad50, D's address) and the objects its factory slots return. Exe call sites reach 28 of the
  86 Display slots and 15 of the 60 Surface slots through straight-line tracked pointers (`exe_call_sites` /
  `exe_callers`; calls through spilled pointers are not attributed, so 0 does not mean unused). Stack argument counts
  come from each jgld body's `ret N` (the push counts at the exe sites are listed too; they are upper bounds, since a
  push for a later call can precede the virtual call). Factory pairing as the exe uses it:
  slot 29 / 34 FontRef, 30 / 36 PaletteCache, 31 / 35 Image, 32 / 37 SpriteRef; slots 35 and 36 have identical bodies
  (U-0454). The exe also hands jgld a 14-entry callback table 0x004ba87c (address stored in 0x0083a4f8 by
  storeUiEventTable 0x004799d0): redrawRegion, uiKeyEvent, uiKeyEvent148, uiPointerMove, Timer_fire and 9 unnamed
  handlers.
- **Terrain.dll**: the exe imports 26 of the 38 exports; their decorated names give the argument types (int, float,
  bool, `Tile*`, `HDC`, `int*`), all thiscall on the Terrain singleton except the static `getInstance`. Callers are the
  exe functions that call each import thunk.
- **sound.dll**: 12 exports. Ordinals 1..11 are loaded by soundVersionCheck 0x00483fc0 into g_soundFns 0x0083af6c[i] =
  GetProcAddress(hSound 0x0083afc0, i + 1); the version check wants `(get_sound_version() & 0xff00) == 0xc00`
  (0x00484021..0x00484026). create_sound(Sound** out, const char* name, unsigned kind) returns 0 or 3 and builds kind 1
  MultiSound (0x1450 bytes, vtable 0x1005c7e8) or kind 2 Sound5b6a8 (0x948 bytes, vtable 0x1005b6a8) (decompilation of
  0x100093e0); the 25 slots the exe calls on Snd+0x40 are listed with both classes' bodies. The device create /
  delete exports (decorated `SNDERR (__cdecl static)(Device**, unsigned long)` / `()`) are called by openHookA/B/C and
  closeHookA/B/C through the table. Ordinal 12 (the waveIn callback) is not called by the exe.

## 6. Global instances (`e2_addresses.tsv`)

153 rows: 56 SpriteRef (39 arrays, 17 singles), 11 FontRef, 8 Image, 5 PaletteCache (incl. g_animPalettes[591] at the base
of cluster 494), BinkPlayer, ScreenObjList, 2 StrList, Reader, PairArray, Logger, StdList, 2 SpritePair, 1 SpriteRow5,
g_storyFiles[8], and 61 scalars / buffers. `.sve` fields typed here: g_holesPlusOne 0x005685f0, g_theme 0x00567328
(char[100]), g_courseIndex 0x0059bf90, and 0x00561250 / 0x005a6364 (meaning U-0473). g_newObjectHeap 0x00839650 is the
heap the next HeapObject is placed on: every HeapObject installer copies it into the object's heap field and clears it
(0x00482b04, S3List_add 0x00401dd1; 138 dword references).

## 7. Corrections to older notes and to stream A

- **Declared types in re/names that the model replaces** (names kept, types differ): g_blitSaveBuf 0x0083a7b8 `void*`
  -> Image object; gTileBuffer 0x00820f18 `void*` -> std::list header; g_pairArray 0x008406e8 `int-pair array ptr` ->
  PairArray object; g_logFile 0x00840930 `FILE*` -> Logger object; g_screenMgrA 0x0083acb0 `ScreenMgr` -> PaletteCache;
  g_screenObjList 0x0083ac88 `List` -> ScreenObjList; g_sfxDevice / g_voiceDevice `SoundDevice` -> StrList objects
  (NodeListB / NodeListC vtables) holding the sound.dll device at +0x14; g_pcxSurface / g_cursorSurfA..C `gfx surface
  object` -> SpriteRef; **g_netState 0x008400b0 `int (net state; nonzero)` -> an interface pointer, never stored
  nonzero** (E2 proposes g_netInterface; the names-file owner decides).
- **Class names in re/names that describe another class**: Image::seek / Image::step take a Flic (U-0460); the
  Surface:: methods (createSized, createFromId, releaseBacking, ctorVtbl483b90) belong to FontRef, not to the
  0x2c-byte Surface_* class; Decoder482ae0::ctor / Decoder482b20::ctor construct S3List nodes, not decoders; the
  Cache48xxxx / Table483 names are one PaletteCache class.
- **jgld names**: Factory::destroyPalette / destroySurface (slots 35 / 36) have identical bodies and the exe pairs them
  crosswise with createSurface / createPalette (U-0454).
- **Stream A**: cluster 1689 (0x0083aca8, `object`) is the HeapObject sub-object at +0x20 of g_screenObjList; the
  175 sound-table `object` clusters are elements of Snd[300]; the rows of objects 0x2c apart (A's 0x00540110 ...) are
  SpriteRef arrays; cluster 494's base 0x004e9aa0 is the PaletteCache[591] array; 0x0058ccba..0x0058ccc0,
  0x00520593, 0x005787c2 / 0x005787c3, 0x005423fc are loop-end bounds of the tables below (`cmp reg, imm`), not
  members of the SpriteRef arrays they fall in (U-0485); the 0x00838204 cluster mixes four scalars with the
  StoryFileRec[8] table.
- **D's placeholders** (`SgSurfaceObj`, `SgFlicObj`, `SgFlicStream`, `SgCache58`) are SpriteRef, Flic, FlicFrame and
  PaletteCache; `SgSnd` = Snd; `SgSndGroupDevice` (0x1e4) has NodeList's size and bucket layout. For stream G.
- 0x0083ab00 / 0x0083aafc / 0x0083ab0c / 0x0083ab10 / 0x0083ab14 hold callback pointers (the C3 hooks call them through
  function-pointer types: c3dv.cpp, c3dw.cpp, c3ek.cpp); E2 types them as dwords of g_ui83aae4 (U-0477).

## 8. Original defects catalogued (not repaired)

- loadStoryStrings reads 0x00838220 as a dword at 0x004670db (then truncates to 16 bits) while every writer stores 16
  bits: the upper half is two bytes nothing writes (0 in .bss). The C3 hook transcribes the same dword read
  (c3dz.cpp:172).
- The PaletteCache's palette is created through Display slot 30 and destroyed through slot 36, the Image's surface
  created through 31 and destroyed through 35 (harmless only because the two destroy bodies are identical, U-0454).

## 9. model_check (`log/p1/e2/check_e2.tsv`, all functions in game logic, margin 0x100): 2 errors, 75 warnings

- error `width_cross` g_story838220: the dword read above (a defect of the original, section 8).
- error `loop_end` g_sprites53df58: loadWorldAssets 0x0043ebb0 compares the loop pointer with 0x0053e690 after
  `add esi, 0xb0`; the loop touches esi - 0x2c, esi, esi + 0x2c with esi from 0x0053dfb0, so its last access is
  element 39 (0x0053e60c) of the 40: the checker's rule assumes the compared pointer itself is accessed. No access
  leaves the array.
- warnings: `stride` 42 and `base_outside` 20 are neighbouring arrays (an indexed `lea` whose base is the next array, or
  a scale-4 `lea` after an unseen `* 11` multiplier: 0x2c = 4 x 11); `width_narrow` 13 come from the neighbours' biased
  accesses (golfer records at displacement - 0x100 in mainLoop, the text buffer at 0x0051a067 = g_textBuffer - 1 in
  appendCents / textInputBox, the 0x0053caf0 flag grid) and the word read of g_eoy58b388. None names a field of an E2
  type that the instruction contradicts.

## 10. Hook cross-check (`log/p1/e2/hookcheck.tsv`)

2,669 literal addresses in shim/src/re/*.cpp code (comments stripped) fall inside E2 ranges; 1,099 of them carry a
width (`G<T>(addr [+ k] [+ K * i])`, G8 / G16 / G32, D). 1,095 agree with the model's field width and element stride;
2 disagree, both the dword read of 0x00838220 that the original makes (c3dz.cpp:172, c3dz_negctl.cpp:195); 2 are
narrower (G<short> of g_eoy58b388, the original's `mov ax, word ptr` at 0x0044d904). Object fields reached through
`this` helpers (I / U / At / F) are not address literals and were not checked against the class layouts.

## 11. Handoffs for the parent

- SpriteRef arrays typed here whose base cluster another stream owns: 0x0053df58 (40, base cluster 564 E1), 0x005628e8
  (432, 689 B), 0x0058a538 (72, 879 B), 0x0059df20 (50, 979 B), 0x0059e7c0 (120, 993 B).
- **Overlaps with other address maps found at the end of the run (2026-10-18, log/p1/e2/overlap.py)**:
  - `re/model/e1_addresses.tsv` repeats 30 of E2's global rows with E2's own symbols (g_sprites53c570 ...,
    g_infoSprites821070 / 8210c8 / 821120, g_font51a048, g_sprites53df58, g_sprites59df20) typed `GfxSurface` /
    `SurfaceRef`; it also has g_gfxSurface_8224c8 (0x8224c8, 0x6b4) covering g_sprite822b50. One stream must drop them;
    the E2 rows carry the ehVectorConstructorIterator / constructor evidence and the E2 class names.
  - D's g_surface58baa0 and g_surface5a57e8 are element 24 of g_sprites58b680 and of g_sprites5a53c8 (loadInterfaceArt
    steps a pointer from there): one array row each, not separate objects.
  - D's g_credits_text_ptrs 0x0083b15c (4 pointers) overlaps g_sectionReader's +0x15c / +0x160 (Reader::allocBuffers'
    two line buffers, Reader::ctor clears them); 0x0083b164 / 0x0083b168 are separate globals copied from them by
    FUN_00487dd0 / FUN_00487df0 (no `this`): D's row should start at 0x0083b164 with 2 pointers.
  - C's tile_anim_frame 0x00578804 (50 x 50 bytes, committed 8a180799) covers the byte E2 had at 0x00578804; E2
    dropped its row and its name g_bytes578804 (withdrawn: it is in no committed names file, nothing for stream G to
    remove). The rest of cluster 822 (the SpriteRef at 0x005787d0, 0x005787fc, 0x00578800) stays E2's.
  - Terrain boundary objects are C's (c.h: TerrainObject 0x164ad0, TerrainTile 0x248, TerrainTypeInfo): the
    Terrain.dll rows of e2_dll_api.tsv name them instead of retyping them. g_holesPlusOne 0x005685f0 (cluster 723, E2)
    stays typed here; C documents it without a row.
- Clusters inside other streams' typed ranges (48, not re-typed): Snd[300] g_sounds (D): 1300..1333; D's SpriteRef
  arrays g_surfaces53ba48 (541..545), g_surfaces5439e0 (649), g_surfaces59b050 (937..943); 1113 (0x005a9f78, an EditBox
  built by staticCtor_4385a0) is in e1_addresses.tsv.
- 0x0051b380 seed time (B's RNG word) and the .sve fields 0x00561250, 0x005685f0, 0x0059bf90, 0x005a6364, 0x00567328
  sit in E2 clusters by vote; typed by width here, meanings for B.
- Agreed with stream E1 (2026-10-18, session message): E1 owns the window family layouts (RootWindow, TextView /
  Dialog), HotList, the EditBox at 0x005a9f78 and the SpriteRef array at 0x008224c8 (gSurfaceCtor_44b5a0, 39 elements);
  E2 dropped its single row at 0x00822b50 (element 38 of that array) and its HotList structs. E1 was asked to drop its
  30 duplicate rows of E2 globals (list in log/p1/e2/overlap.py output).
- Items the parent listed for E (g_textBuffer, 0x0083ad44, the 0x0083aa78..0x0083ab28 words, the two device objects)
  are all typed now: g_textBuffer, the devices and 0x0083aa98..0x0083aaa0 / 0x0083aae0..0x0083ab28 here; 0x0083ad44 and
  the other 0x0083aa78.. words by E1 and D (no gap). D's g_screenMgrB 0x0083ac30 (0x58) and the 190 image slots at
  0x0081ca10 have Cache482fd0::ctor as element constructor: they are PaletteCache objects.

## 12. Coverage

| measure | done | total | note |
|---|---:|---:|---|
| clusters typed by E2 (all addresses in an E2 range, an E2 vtable row, or an E2 range end bound) | 228 | 277 | 1 more is mixed: 1598, whose base object is E1's array |
| clusters inside another stream's typed range | 48 | 277 | D: g_sounds, three Surface arrays; C: 0x00578804; E1: 1113 |
| clusters left untyped | 0 | 277 | |
| addresses (incl. end markers): E2 range 322, vtable 45, end bound 4 | 371 | 420 | other stream 49 |
| weighted by referencing functions | 1,104 | 1,164 | other stream 57, mixed 3 |

U rows: 37 (U-0450..U-0487, U-0484 withdrawn), mostly field meanings of constructor-only fields and of window-system dwords that are
stream E1's.
