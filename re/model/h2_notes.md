# Stream H chunk h2 notes (functions 0x00448190..0x00485080, 71 rows)

All 71 functions are `live` by pointer (CRT initializer table, atexit registrations, vtable slots); callers column of the candidate list is 0 except 0x0046f180 (mainLoop) and 0x00483020.

| address | old name | new name | subsystem | reach | group | evidence |
|---|---|---|---|---|---|---|
| 0x00448190 | FUN_00448190 | gSoundsDtor_448190 | audio | operand | atexit destructors | pushes dtor 0x004848a0 (0x00448190), count 0x12c (0x00448195), element size 0x6c (0x0044819a), base 0x0080d840 g_sounds (0x0044819c), calls the vector |
| 0x00449160 | FUN_00449160 | dynInit_staticInitGTerrain | boot | vtable/table | dynInit stubs | 5-byte jmp 0x00449170 (0x00449160) |
| 0x00449170 | FUN_00449170 | staticInitGTerrain | terrain | tailjmp from stub | static init bodies | calls 0x004a4f10, the Terrain.dll import thunk (jmp [0x004ba168] = Terrain::getInstance), and stores eax in gTerrain 0x00820ed0 (0x00449175) |
| 0x00449180 | FUN_00449180 | dynInit_staticInit_820f04 | boot | vtable/table | dynInit stubs | 5-byte jmp 0x00449190 (0x00449180) |
| 0x00449190 | FUN_00449190 | staticInit_820f04 | terrain | tailjmp from stub | static init bodies | fld qword [0x004ba620] (0x00449190), fabs (0x00449196), __ftol 0x004a6030 (0x00449198), store eax at static_820f04 0x00820f04 (0x0044919d) |
| 0x004491e0 | FUN_004491e0 | gSurfaceDtor_4491e0 | render | operand | atexit destructors | stores vtable 0x004ba2d8 at 0x00820ed8 (0x004491e0), ecx = 0x00820ed8 and jmp Surface_free 0x00473ae0 (0x004491ea/0x004491ef) |
| 0x00449250 | FUN_00449250 | destroyList820f18 | util | operand | atexit destructors | walks the circular list whose head pointer is 0x00820f1c (0x00449252..0x00449289), unlinks each node and frees it with operatorDelete_4a4ffc 0x004a4ff |
| 0x004492b0 | FUN_004492b0 | dynInit_emptyStaticInit_4492c0 | boot | vtable/table | dynInit stubs | 5-byte jmp 0x004492c0 (0x004492b0) |
| 0x004492c0 | FUN_004492c0 | emptyStaticInit_4492c0 | boot | tailjmp from stub | static init bodies | the whole body is one ret (0x004492c0) |
| 0x0044acb0 | FUN_0044acb0 | fontRefsDtor_44acb0 | ui | operand | atexit destructors | for the five objects 0x00821ee8, 0x00821f28, 0x00821ec8, 0x00821f08, 0x00821020 stores vtable 0x004ba468 and calls FontRef::release 0x004838b0 (0x0044 |
| 0x0044ad50 | FUN_0044ad50 | spriteRefArrayDtor_44ad50 | ui | operand | atexit destructors | pushes dtor 0x004041f0, count 5, element size 0x2c, base 0x00820f40 g_spriteRef_820f40 and calls the vector destructor iterator 0x004a5713 (0x0044ad50 |
| 0x0044ada0 | FUN_0044ada0 | gSurfaceDtor_44ada0 | render | operand | atexit destructors | stores vtable 0x004ba2d8 at 0x00821040 (0x0044ada0), ecx = 0x00821040 and jmp Surface_free 0x00473ae0 (0x0044adaf) |
| 0x0045ba80 | FUN_0045ba80 | gCursorDtor_45ba80 | render | operand | atexit destructors | stores vtable 0x004ba7f4 at 0x00822cc8 (0x0045ba80), ecx = 0x00822cc8 and jmp Cursor::destroy 0x004884b0 (0x0045ba8f) |
| 0x0045bad0 | FUN_0045bad0 | gSurfaceDtor_45bad0 | render | operand | atexit destructors | stores vtable 0x004ba2d8 at 0x00822c98 (0x0045bad0), ecx = 0x00822c98 and jmp Surface_free 0x00473ae0 (0x0045badf) |
| 0x0045c180 | FUN_0045c180 | dynInit_staticInit_gRngZero | boot | vtable/table | dynInit stubs | 5-byte jmp 0x0045c190 (0x0045c180) |
| 0x0045c190 | FUN_0045c190 | staticInit_gRngZero | sim | tailjmp from stub | static init bodies | mov dword [0x00822d9c], 0 (0x0045c190) |
| 0x004617e0 | FUN_004617e0 | dynInit_staticInit_gColor16Lut | boot | vtable/table | dynInit stubs | 5-byte jmp 0x004617f0 (0x004617e0) |
| 0x004617f0 | FUN_004617f0 | staticInit_gColor16Lut | render | tailjmp from stub | static init bodies | push 0x50000, call operator_new 0x004a57f1 (0x004617f0/0x004617f5), store eax at g_color16Lut 0x00824148 (0x004617fd) |
| 0x004658a0 | FUN_004658a0 | RootWindow::exitProcess | frontend | vtable/table | virtual overrides | slot 24 of vtable 0x004ba2fc (RootWindow): push 3 |
| 0x0046de60 | FUN_0046de60 | gImageDtor_46de60 | render | operand | atexit destructors | ecx = 0x00838f98 (gImage_838f98) and jmp Image_dtor 0x00474c40 (0x0046de60/0x0046de65) |
| 0x0046f180 | FUN_0046f180 | runBulletinBoardWithSound | frontend | live (call from mainLoop) | screens | only caller mainLoop 0x0040f5c0 |
| 0x00473a90 | FUN_00473a90 | gLoadImageDtor_473a90 | render | operand | atexit destructors | tests bit 0 of the guard byte 0x00839340 and sets it (0x00473a90..0x00473a9e), then ecx = 0x00839348 (g_loadImage) and jmp Image_dtor 0x00474c40 (0x00 |
| 0x00474700 | FUN_00474700 | recordFreeStr_474700 | util | vtable/table | small virtuals | slot 1 of vtable 0x004bc3b4 (installed by panelSubCtor4a3110 |
| 0x00474720 | FUN_00474720 | SpriteRef::scalarDtor | render | vtable/table | virtual destructors | slot 0 of vtable 0x004ba2d8 (class SpriteRef, e2_classes.tsv) |
| 0x00474ab0 | FUN_00474ab0 | gSurfaceDtor_474ab0 | render | operand | atexit destructors | tests and sets bit 0 of guard 0x00839a58 (0x00474ab0..0x00474abc), stores vtable 0x004ba2d8 at 0x00839a68 (0x00474abe) and jmp Surface_free 0x00473ae0 |
| 0x00479820 | FUN_00479820 | return0_479820 | util | vtable/table | small virtuals | xor eax, eax |
| 0x00479a10 | FUN_00479a10 | gImageDtor_479a10 | render | operand | atexit destructors | tests and sets bit 0 of the guard byte 0x0083a2c0 (0x00479a10..0x00479a1e), ecx = 0x0083a500 (g_image83a500) and jmp Image_dtor 0x00474c40 (0x00479a29 |
| 0x00479a60 | FUN_00479a60 | gImageDtor_479a60 | render | operand | atexit destructors | tests and sets bit 1 of the guard byte 0x0083a2c0 (0x00479a60..0x00479a6e), ecx = 0x0083a7b8 (g_blitSaveBuf) and jmp Image_dtor 0x00474c40 (0x00479a79 |
| 0x0047bbb0 | FUN_0047bbb0 | Window::popModal | ui | vtable/table | window virtuals | slot 68 of every Window-family vtable |
| 0x0047e110 | FUN_0047e110 | return0Pop8_47e110 | ui | vtable/table | small virtuals | xor eax, eax |
| 0x0047e5e0 | FUN_0047e5e0 | emptyVirtual_47e5e0 | ui | vtable/table | small virtuals | the whole body is one ret (0x0047e5e0) |
| 0x0047e770 | FUN_0047e770 | DEFERRED | ui | vtable/table | window virtuals | slot 78 of the View vtable 0x004ba8b4 |
| 0x0047ea30 | FUN_0047ea30 | View::drawEdges | ui | vtable/table | window virtuals | slot 79 of the View and Window vtables |
| 0x00480f80 | FUN_00480f80 | Window::resizeSurfaces | ui | vtable/table | window virtuals | slot 90 of the Window vtables |
| 0x00482ea0 | FUN_00482ea0 | gScreenMgrADtor_482ea0 | ui | operand | atexit destructors | ecx = 0x0083acb0 (g_screenMgrA) and jmp PaletteCache::dtor 0x00483010 (0x00482ea0/0x00482ea5) |
| 0x00482ee0 | FUN_00482ee0 | gScreenMgrBDtor_482ee0 | ui | operand | atexit destructors | ecx = 0x0083ac30 (g_screenMgrB) and jmp PaletteCache::dtor 0x00483010 (0x00482ee0/0x00482ee5) |
| 0x00483020 | FUN_00483020 | returnThis_483020 | util | live (1 caller) | small virtuals | mov eax, ecx |
| 0x004837d0 | FUN_004837d0 | gFontRefDtor_4837d0 | ui | operand | atexit destructors | stores vtable 0x004ba468 at 0x0083ad20 (0x004837d0), ecx = 0x0083ad20 and jmp FontRef::release 0x004838b0 (0x004837df) |
| 0x00483b30 | FUN_00483b30 | FontRef::scalarDtor | ui | vtable/table | virtual destructors | slot 0 of vtable 0x004ba468 (class FontRef 0x20 bytes, e2_classes.tsv) |
| 0x00483e00 | FUN_00483e00 | gMusicDeviceDtor_483e00 | audio | operand | atexit destructors | ecx = 0x0083ad80 (g_musicDevice) and jmp NodeList::dtor 0x00487390 (0x00483e00/0x00483e05) |
| 0x00483e40 | FUN_00483e40 | gSfxDeviceDtor_483e40 | audio | operand | atexit destructors | ecx = 0x0083ad58 (g_sfxDevice) and jmp NodeListB::dtor 0x00487a60 (0x00483e40/0x00483e45) |
| 0x00483e80 | FUN_00483e80 | gVoiceDeviceDtor_483e80 | audio | operand | atexit destructors | ecx = 0x0083af98 (g_voiceDevice) and jmp NodeListC::dtor 0x00487b80 (0x00483e80/0x00483e85) |
| 0x00484440 | FUN_00484440 | Snd::isPlaying | audio | vtable/table | Snd virtuals | slot 24 of SndBase/VoiceRx |
| 0x00484450 | FUN_00484450 | Snd::linkIntoList | audio | vtable/table | Snd virtuals | slot 31 |
| 0x004844b0 | FUN_004844b0 | Snd::getNext | audio | vtable/table | Snd accessors | mov eax, [ecx+0x4c] |
| 0x004844c0 | FUN_004844c0 | Snd::getPrev | audio | vtable/table | Snd accessors | mov eax, [ecx+0x48] |
| 0x004844d0 | FUN_004844d0 | Snd::getMode | audio | vtable/table | Snd accessors | mov eax, [ecx+0x54] |
| 0x00484540 | FUN_00484540 | returnB_484540 | audio | vtable/table | Snd virtuals | mov eax, 0xb |
| 0x00484590 | FUN_00484590 | Snd::fwdVoice1c | audio | vtable/table | Snd voice forwarders | slot 7 |
| 0x004845b0 | FUN_004845b0 | Snd::fwdVoice18 | audio | vtable/table | Snd voice forwarders | slot 6 |
| 0x00484600 | FUN_00484600 | Snd::fwdVoice38 | audio | vtable/table | Snd voice forwarders | slot 14 |
| 0x00484620 | FUN_00484620 | Snd::setVolumeBase | audio | vtable/table | Snd virtuals | slot 16 of SndBase/VoiceRx |
| 0x00484690 | FUN_00484690 | Snd::setLoop | audio | vtable/table | Snd virtuals | slot 18 |
| 0x00484700 | FUN_00484700 | Snd::setField38Voice54 | audio | vtable/table | Snd virtuals | slot 21 |
| 0x00484730 | FUN_00484730 | Snd::applyField38Voice28 | audio | vtable/table | Snd voice forwarders | slot 10 |
| 0x00484770 | FUN_00484770 | Snd::applyField38Voice30 | audio | vtable/table | Snd voice forwarders | slot 12 |
| 0x00484790 | FUN_00484790 | Snd::fwdVoice34 | audio | vtable/table | Snd voice forwarders | slot 13, ret 0xc |
| 0x004847d0 | FUN_004847d0 | Snd::fwdVoice58 | audio | vtable/table | Snd voice forwarders | slot 22 |
| 0x004847e0 | FUN_004847e0 | Snd::fwdVoice74 | audio | vtable/table | Snd voice forwarders | slot 29 |
| 0x00484870 | FUN_00484870 | Snd::fwdVoice60 | audio | vtable/table | Snd voice forwarders | Snd slot 24 |
| 0x00484920 | FUN_00484920 | Snd::fwdVoice98 | audio | vtable/table | Snd voice forwarders | Snd slot 38 |
| 0x004849e0 | FUN_004849e0 | Snd::fwdVoice94 | audio | vtable/table | Snd voice forwarders | Snd slot 37 |
| 0x00484a00 | FUN_00484a00 | Snd::fwdVoiceD0 | audio | vtable/table | Snd voice forwarders | Snd slot 52 |
| 0x00484a20 | FUN_00484a20 | Snd::fwdVoiceD4 | audio | vtable/table | Snd voice forwarders | Snd slot 53 |
| 0x00484b00 | FUN_00484b00 | Snd::fwdVoiceCC | audio | vtable/table | Snd voice forwarders | Snd slot 51 |
| 0x00484b20 | FUN_00484b20 | Snd::getLengthMs | audio | vtable/table | Snd accessors | Snd slot 50 |
| 0x00484bf0 | FUN_00484bf0 | Snd::fwdVoiceF0 | audio | vtable/table | Snd voice forwarders | Snd slot 60, ret 8 |
| 0x00484f80 | FUN_00484f80 | Snd::setFlags | audio | vtable/table | Snd virtuals | Snd slot 27, inverse of Snd::flags 0x00484ff0: bit 2 sets +0x30 (0x00484f8d), bits 1, 4, 0x40, 0x80, 0x10, 0x100, 0x400 or bits 1, 2, 8, 0x10, 4, 0x20 |
| 0x00485040 | FUN_00485040 | Snd::fwdVoiceB8 | audio | vtable/table | Snd voice forwarders | Snd slot 46 |
| 0x00485060 | FUN_00485060 | Snd::fwdVoiceBC | audio | vtable/table | Snd voice forwarders | Snd slot 47 |
| 0x00485080 | FUN_00485080 | Snd::fwdVoice3c | audio | vtable/table | Snd voice forwarders | Snd slot 15 |

## Groups
- atexit destructors: 19
- Snd voice forwarders: 18
- Snd virtuals: 7
- dynInit stubs: 5
- static init bodies: 5
- small virtuals: 5
- window virtuals: 4
- Snd accessors: 4
- virtual destructors: 2
- virtual overrides: 1
- screens: 1

## Counts
- named: 70
- deferred: 1 (0x0047e770: visual role not established; evidence in the table is literal rectangle fills only)

## Defects found (not fixed)
- 0x0047e770: two `idiv` (0x0047e8e1 by [esp+0x4c], 0x0047e8f7 by ebp) take their divisor from child slots +0xd8/+0xdc with no zero test; a zero extent faults.
- 0x00484620 Snd::setVolumeBase: when the group index is 0x10 or more the volume forwarded to the voice is the unscaled masked value; no defect, recorded as a behaviour difference from groups below 0x10.
- 0x00480f80 Window::resizeSurfaces: the early return for argument 1 (0x00480f8a) skips the visible-redraw call at 0x004810b2.

## Model corrections noticed
- re/model/e2_vtables.tsv names the class at vtable 0x004ba2d8 GfxSurface and the one at 0x004ba468 SurfaceRef, while e2_classes.tsv and the installers (0x00473ab7, 0x004837f9) name them SpriteRef (0x2c bytes) and FontRef (0x20 bytes). The slot-0 deleting destructors were named SpriteRef::scalarDtor / FontRef::scalarDtor.
- vtable 0x004bc3b4 is typed SgRecList in e1_vtables.tsv but is installed by panelSubCtor4a3110 and its slot 0 is recordCopyStr20 (a string record copier); the class name is not supported by the slots.
- Snd slots 6, 7, 10, 12-15, 22, 24, 29, 37, 38, 46, 47, 50-53, 60 are plain forwarders to the sound.dll voice object at +0x40; their voice slot offsets are in the names (Snd::fwdVoiceNN), the semantics of those voice slots are not determined.
- 0x00449190 loads the double at 0x004ba620 and stores an integer into static_820f04; the value of the constant was not read here.
