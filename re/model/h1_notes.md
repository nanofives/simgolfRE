# Stream H chunk h1 notes (2026-10-18)

Chunk h1 holds 71 rows of re/model/h_candidates.tsv; 0x00402180 appears twice, so there are 70 distinct functions. All 70 are named, none deferred.

| address | old name | new name | subsystem | reach | size | group | evidence (short) |
|---|---|---|---|---|---|---|---|
| 0x00402180 | FUN_00402180 | return0Pop4_402180 | ui | live | 5 | vslot-default | `xor eax, eax` 0x00402180, `ret 4` 0x00402182; stored in slots 0, 10, 36, 37, 50 of the SgRecList / SgRootWindow / SgView tables (e1_vtables.tsv) |
| 0x00402190 | FUN_00402190 | retPop4_402190 | ui |  | 3 | vslot-default | `ret 4` 0x00402190; stored in slots 1, 51-53, 59, 60, 64, 66 of 16 window-class tables (e1_vtables.tsv, 96 slot rows) |
| 0x00402720 | FUN_00402720 | deletingDtor_adj1c_402720 | crt | live | 99 | deleting-dtor | `lea esi, [ecx-0x1c]` 0x00402721; stores its vtables, calls 0x004026a0 List402_clear, stores 0 at +0x14, resets the HeapObject vtable 0x004ba278 and the heap global 0x... |
| 0x00402790 | FUN_00402790 | deletingDtor_adj1c_402790 | crt | live | 99 | deleting-dtor | `lea esi, [ecx-0x1c]` 0x00402791; stores its vtables, calls 0x004026a0 List402_clear, stores 0 at +0x14, resets the HeapObject vtable 0x004ba278 and the heap global 0x... |
| 0x00402800 | FUN_00402800 | deletingDtor_adj1c_402800 | crt | live | 99 | deleting-dtor | `lea esi, [ecx-0x1c]` 0x00402801; stores its vtables, calls 0x004026a0 List402_clear, stores 0 at +0x14, resets the HeapObject vtable 0x004ba278 and the heap global 0x... |
| 0x00402870 | FUN_00402870 | deletingDtor_401c30_adj14_402870 | crt | live | 63 | deleting-dtor | same steps as deletingDtor_401c30 0x00401c30 (store of vtable 0x004ba278, 0x00839650 restored from [this+4], `call 0x004a5007` _free when the flag bit is set) with the... |
| 0x004028b0 | FUN_004028b0 | deletingDtor_401c30_adj10_4028b0 | crt | live | 63 | deleting-dtor | same steps as deletingDtor_401c30 0x00401c30 (store of vtable 0x004ba278, 0x00839650 restored from [this+4], `call 0x004a5007` _free when the flag bit is set) with the... |
| 0x004028f0 | FUN_004028f0 | deletingDtor_401c30_adj8_4028f0 | crt | live | 63 | deleting-dtor | same steps as deletingDtor_401c30 0x00401c30 (store of vtable 0x004ba278, 0x00839650 restored from [this+4], `call 0x004a5007` _free when the flag bit is set) with the... |
| 0x00403f50 | FUN_00403f50 | exitDtorArray_flicObjs_403f50 | crt | live | 26 | exit-dtor | registered by atexit through the stub at 0x00403f40 (`push 0x00403f50`, `call 0x004a56d2`); pushes element destructor 0x00404280, count 0x24f, element size 0x84, array... |
| 0x00403fb0 | FUN_00403fb0 | exitDtorArray_flicStreams_403fb0 | crt | live | 23 | exit-dtor | registered by atexit through the stub at 0x00403fa0 (`push 0x00403fb0`, `call 0x004a56d2`); pushes element destructor 0x00404200, count 0x24f, element size 0x44, array... |
| 0x00404010 | FUN_00404010 | exitDtorArray_animSprites_404010 | crt | live | 23 | exit-dtor | registered by atexit through the stub at 0x00404000 (`push 0x00404010`, `call 0x004a56d2`); pushes element destructor 0x004041f0, count 0x24f, element size 0x2c, array... |
| 0x00404070 | FUN_00404070 | exitDtorArray_animPalettes_404070 | crt | live | 23 | exit-dtor | registered by atexit through the stub at 0x00404060 (`push 0x00404070`, `call 0x004a56d2`); pushes element destructor 0x00483010, count 0x24f, element size 0x58, array... |
| 0x00404340 | FUN_00404340 | exitDtor_rootWindow_404340 | crt | live | 10 | exit-dtor | registered by atexit through the stub at 0x00404330 (`push 0x00404340`, `call 0x004a56d2`); loads the object address then `jmp 0x00404350` (0x00404345) |
| 0x004043d0 | FUN_004043d0 | exitDtor_image51b068_4043d0 | crt | live | 10 | exit-dtor | registered by atexit through the stub at 0x004043c0 (`push 0x004043d0`, `call 0x004a56d2`); loads the object address then `jmp 0x00474c40` Image_dtor (0x004043d5) |
| 0x00404410 | FUN_00404410 | exitDtor_font519928_404410 | crt | live | 20 | exit-dtor | registered by atexit through the stub at 0x00404400 (`push 0x00404410`, `call 0x004a56d2`); stores vtable 0x004ba468 into 0x00519928 and tail-jumps to FontRef::release... |
| 0x00404460 | FUN_00404460 | exitDtor_font519fd8_404460 | crt | live | 20 | exit-dtor | registered by atexit through the stub at 0x00404450 (`push 0x00404460`, `call 0x004a56d2`); stores vtable 0x004ba468 into 0x00519fd8 and tail-jumps to FontRef::release... |
| 0x004044b0 | FUN_004044b0 | exitDtor_font51b320_4044b0 | crt | live | 20 | exit-dtor | registered by atexit through the stub at 0x004044a0 (`push 0x004044b0`, `call 0x004a56d2`); stores vtable 0x004ba468 into 0x0051b320 and tail-jumps to FontRef::release... |
| 0x00404500 | FUN_00404500 | exitDtor_font51b360_404500 | crt | live | 20 | exit-dtor | registered by atexit through the stub at 0x004044f0 (`push 0x00404500`, `call 0x004a56d2`); stores vtable 0x004ba468 into 0x0051b360 and tail-jumps to FontRef::release... |
| 0x00404550 | FUN_00404550 | exitDtor_font519948_404550 | crt | live | 20 | exit-dtor | registered by atexit through the stub at 0x00404540 (`push 0x00404550`, `call 0x004a56d2`); stores vtable 0x004ba468 into 0x00519948 and tail-jumps to FontRef::release... |
| 0x004045a0 | FUN_004045a0 | exitDtor_font519a40_4045a0 | crt | live | 20 | exit-dtor | registered by atexit through the stub at 0x00404590 (`push 0x004045a0`, `call 0x004a56d2`); stores vtable 0x004ba468 into 0x00519a40 and tail-jumps to FontRef::release... |
| 0x004045f0 | FUN_004045f0 | exitDtor_font51a028_4045f0 | crt | live | 20 | exit-dtor | registered by atexit through the stub at 0x004045e0 (`push 0x004045f0`, `call 0x004a56d2`); stores vtable 0x004ba468 into 0x0051a028 and tail-jumps to FontRef::release... |
| 0x00404640 | FUN_00404640 | exitDtor_font51b340_404640 | crt | live | 20 | exit-dtor | registered by atexit through the stub at 0x00404630 (`push 0x00404640`, `call 0x004a56d2`); stores vtable 0x004ba468 into 0x0051b340 and tail-jumps to FontRef::release... |
| 0x00404690 | FUN_00404690 | exitDtor_font519968_404690 | crt | live | 20 | exit-dtor | registered by atexit through the stub at 0x00404680 (`push 0x00404690`, `call 0x004a56d2`); stores vtable 0x004ba468 into 0x00519968 and tail-jumps to FontRef::release... |
| 0x004046e0 | FUN_004046e0 | exitDtor_palette5199c8_4046e0 | crt | live | 10 | exit-dtor | registered by atexit through the stub at 0x004046d0 (`push 0x004046e0`, `call 0x004a56d2`); loads the object address then `jmp 0x00483010` PaletteCache::dtor (0x004046... |
| 0x00404720 | FUN_00404720 | exitDtor_font519a20_404720 | crt | live | 20 | exit-dtor | registered by atexit through the stub at 0x00404710 (`push 0x00404720`, `call 0x004a56d2`); stores vtable 0x004ba468 into 0x00519a20 and tail-jumps to FontRef::release... |
| 0x00404770 | FUN_00404770 | exitDtor_font51a008_404770 | crt | live | 20 | exit-dtor | registered by atexit through the stub at 0x00404760 (`push 0x00404770`, `call 0x004a56d2`); stores vtable 0x004ba468 into 0x0051a008 and tail-jumps to FontRef::release... |
| 0x004047c0 | FUN_004047c0 | exitDtor_font519988_4047c0 | crt | live | 20 | exit-dtor | registered by atexit through the stub at 0x004047b0 (`push 0x004047c0`, `call 0x004a56d2`); stores vtable 0x004ba468 into 0x00519988 and tail-jumps to FontRef::release... |
| 0x00404810 | FUN_00404810 | exitDtor_font5199a8_404810 | crt | live | 20 | exit-dtor | registered by atexit through the stub at 0x00404800 (`push 0x00404810`, `call 0x004a56d2`); stores vtable 0x004ba468 into 0x005199a8 and tail-jumps to FontRef::release... |
| 0x00404860 | FUN_00404860 | exitDtor_font51a048_404860 | crt | live | 20 | exit-dtor | registered by atexit through the stub at 0x00404850 (`push 0x00404860`, `call 0x004a56d2`); stores vtable 0x004ba468 into 0x0051a048 and tail-jumps to FontRef::release... |
| 0x00404880 | FUN_00404880 | return0Pop12_404880 | ui | live | 5 | vslot-default | `xor eax, eax` 0x00404880, `ret 0xc` 0x00404882; 150 slot rows (slot 5, 12-20 ...) in e1_vtables.tsv |
| 0x00404890 | FUN_00404890 | returnArg1Pop4_404890 | ui | live | 7 | vslot-default | `mov eax, [esp+4]` 0x00404890, `ret 4` 0x00404894; slots 7 and 9 of 15 window-class tables |
| 0x004048a0 | FUN_004048a0 | return0Pop16_4048a0 | ui | live | 5 | vslot-default | `xor eax, eax` 0x004048a0, `ret 0x10` 0x004048a2; slot 11 of 15 window-class tables |
| 0x004048b0 | FUN_004048b0 | retPop12_4048b0 | ui | live | 3 | vslot-default | `ret 0xc` 0x004048b0; slots 21, 25, 55, 100 of 15 window-class tables |
| 0x004048c0 | FUN_004048c0 | return1_4048c0 | ui | live | 6 | vslot-default | `mov eax, 1` 0x004048c0, `ret` 0x004048c5; slots 69 and 101 of 15 window-class tables |
| 0x004048d0 | FUN_004048d0 | Surface_createFull_fwd0 | render | live | 32 | surface-fwd | calls 0x00474dd0 Surface_createFull at 0x004048e8 with the four caller arguments in order and two zero pushes (0x004048d8, 0x004048da); `ret 0x10`; slot 0 of the +0x27... |
| 0x004048f0 | FUN_004048f0 | Image_recreateSurface_fwd | render | live | 23 | surface-fwd | calls 0x00474e70 Image_recreateSurface at 0x004048ff with the same three arguments; `ret 0xc`; slot 1 of the +0x274 sub-object tables |
| 0x00404910 | FUN_00404910 | Image_releaseAll_fwd | render | live | 5 | surface-fwd | `jmp 0x00474cb0` Image_releaseAll at 0x00404910; slot 2 of the +0x274 sub-object tables |
| 0x00404920 | FUN_00404920 | Window::refreshFull_fwd0 | ui | live | 8 | surface-fwd | `push 0` 0x00404920, `call 0x00480c80` Window::refreshFull 0x00404922; slot 73 of 14 window-class tables |
| 0x00404930 | FUN_00404930 | return1Pop8_404930 | ui | live | 8 | vslot-default | `mov eax, 1` 0x00404930, `ret 8` 0x00404935; slots 87 and 88 of 14 window-class tables |
| 0x00404940 | FUN_00404940 | thisIdentity_404940 | ui | live | 3 | vslot-default | `mov eax, ecx` 0x00404940, `ret` 0x00404942; slot 71 of 14 window-class tables |
| 0x00404960 | FUN_00404960 | thisIdentity_404940_adj274 | ui | live | 11 | vslot-default | `sub ecx, 0x274` 0x00404960, `jmp 0x00404940` 0x00404966; slot 4 of the +0x274 sub-object tables (0x004ba2e8 ...) |
| 0x00404d00 | FUN_00404d00 | exitDtorSurfaces_404d00 | crt | live | 231 | exit-dtor | registered by atexit through the stub at 0x00404cf0 (`push 0x00404d00`, `call 0x004a56d2`); destroys 8 single Surface objects (vtable 0x004ba2d8 stored, then `call 0x0... |
| 0x00404ea0 | FUN_00404ea0 | exitDtorSurfaces_404ea0 | crt | live | 148 | exit-dtor | registered by atexit through the stub at 0x00404e90 (`push 0x00404ea0`, `call 0x004a56d2`); destroys 4 single Surface objects (vtable 0x004ba2d8 stored, then `call 0x0... |
| 0x00405010 | FUN_00405010 | exitDtorSurfaces_405010 | crt | live | 137 | exit-dtor | registered by atexit through the stub at 0x00405000 (`push 0x00405010`, `call 0x004a56d2`); destroys 7 Surface arrays (element size 0x2c, element destructor 0x004041f0... |
| 0x00405160 | FUN_00405160 | exitDtorSurfaces_405160 | crt | live | 118 | exit-dtor | registered by atexit through the stub at 0x00405150 (`push 0x00405160`, `call 0x004a56d2`); destroys 6 Surface arrays (element size 0x2c, element destructor 0x004041f0... |
| 0x004052e0 | FUN_004052e0 | exitDtorSurfaces_4052e0 | crt | live | 172 | exit-dtor | registered by atexit through the stub at 0x004052d0 (`push 0x004052e0`, `call 0x004a56d2`); destroys 9 Surface arrays (element size 0x2c, element destructor 0x004041f0... |
| 0x00405450 | FUN_00405450 | exitDtorSurfaces_405450 | crt | live | 115 | exit-dtor | registered by atexit through the stub at 0x00405440 (`push 0x00405450`, `call 0x004a56d2`); destroys 6 Surface arrays (element size 0x2c, element destructor 0x004041f0... |
| 0x00405590 | FUN_00405590 | exitDtorSurfaces_405590 | crt | live | 115 | exit-dtor | registered by atexit through the stub at 0x00405580 (`push 0x00405590`, `call 0x004a56d2`); destroys 6 Surface arrays (element size 0x2c, element destructor 0x004041f0... |
| 0x004056d0 | FUN_004056d0 | exitDtorSurfaces_4056d0 | crt | live | 115 | exit-dtor | registered by atexit through the stub at 0x004056c0 (`push 0x004056d0`, `call 0x004a56d2`); destroys 6 Surface arrays (element size 0x2c, element destructor 0x004041f0... |
| 0x00405840 | FUN_00405840 | exitDtorSurfaces_405840 | crt | live | 153 | exit-dtor | registered by atexit through the stub at 0x00405830 (`push 0x00405840`, `call 0x004a56d2`); destroys 8 Surface arrays (element size 0x2c, element destructor 0x004041f0... |
| 0x00405910 | FUN_00405910 | exitDtor_image5a7148_405910 | crt | live | 10 | exit-dtor | registered by atexit through the stub at 0x00405900 (`push 0x00405910`, `call 0x004a56d2`); loads the object address then `jmp 0x00474c40` Image_dtor (0x00405915) |
| 0x004061d0 | FUN_004061d0 | exitDtor_image568d10_4061d0 | crt | live | 10 | exit-dtor | registered by atexit through the stub at 0x004061c0 (`push 0x004061d0`, `call 0x004a56d2`); loads the object address then `jmp 0x00474c40` Image_dtor (0x004061d5) |
| 0x00406210 | FUN_00406210 | exitDtor_image587ae8_406210 | crt | live | 10 | exit-dtor | registered by atexit through the stub at 0x00406200 (`push 0x00406210`, `call 0x004a56d2`); loads the object address then `jmp 0x00474c40` Image_dtor (0x00406215) |
| 0x00406f20 | FUN_00406f20 | raiseCornerLevelsAround | terrain | live | 102 | terrain-level | reads g_cornerLevel 0x005a4998 at [51*x + y] (0x00406f38), walks a 5x5 window from index - 0x68 (`lea esi, [eax-0x68]` 0x00406f4d, two rows and two columns back; row s... |
| 0x00406f90 | FUN_00406f90 | lowerCornerLevelsAround | terrain | live | 100 | terrain-level | same window and kernel as raiseCornerLevelsAround; `dec al` stores a neighbour that is above the centre level plus the kernel entry (0x00406fca..0x00406fd8); caller ma... |
| 0x00407d30 | FUN_00407d30 | switchToCourse | course | live | 203 | course-flow | skips when g_flags 0x0059e7b8 bit 0x4000000 is set (0x00407d36); calls courseActionMenu through thunk 0x00470a00 (0x00407d46); on a result other than -1 stores g_year ... |
| 0x0040b680 | FUN_0040b680 | saveCourse | save | live | 434 | save-course | copies g_textBuffer to a 0x64-byte local as the file name (0x0040b68e..0x0040b6b4), opens it with _open flags 0x8301 (0x0040b6d3, `call 0x004a5d48`) into g_saveFd 0x00... |
| 0x0040d760 | FUN_0040d760 | shotStyleButtonCode | ui | live | 46 | ui-panel | `add eax, -3; cmp eax, 4` 0x0040d764..0x0040d767, jump table 0x0040d790: index 0 returns 0x800043f0, 1 returns 0, 2 returns 0x8000421f, 3 returns 0x80007ff0, 4 returns... |
| 0x0040d890 | FUN_0040d890 | panelListSelect | ui | live | 506 | ui-panel | clears bit 2 of g_flags 0x0059e7b8 (0x0040d89b); switches on the word 0x00567afc: 3 sets g_playerShotStyle 0x0058f330 to 0, -1, 1, 3 or 4 by the argument 3..7 (jump ta... |
| 0x00421b30 | FUN_00421b30 | Surface_createFull_fwd1 | render | live | 35 | surface-fwd | calls 0x00474dd0 Surface_createFull at 0x00421b4b with five caller arguments and a 0 pushed as third (0x00421b47); `ret 0x14`; caller mainLoop |
| 0x00431be0 | FUN_00431be0 | saveCourseJpg_scanline | save | live | 320 | save-course | function pointer stored at 0x00431ea4 inside saveCourseJpg 0x00431d20; reads RGB555 pixels from g_jpegRowCursor 0x00543cf0 with pitch g_jpegSrcPitch 0x0056125c (shr 7 ... |
| 0x004321a0 | FUN_004321a0 | setPendingCourseInfo | course | live | 41 | save-course | strlen then `rep movsd` / `rep movsb` copy of its string argument into g_pendingCourseInfo 0x00587da0 (0x004321a6..0x004321c4); sole caller saveCourse 0x0040b6c5 |
| 0x00437900 | FUN_00437900 | exitDtor_palette5aa560_437900 | crt | live | 10 | exit-dtor | registered by atexit through the stub at 0x004378f0 (`push 0x00437900`, `call 0x004a56d2`); loads the object address then `jmp 0x00483010` PaletteCache::dtor (0x004379... |
| 0x004385c0 | FUN_004385c0 | exitDtor_characterEditorEditBox_4385c0 | crt | live | 10 | exit-dtor | registered by atexit through the stub at 0x004385b0 (`push 0x004385c0`, `call 0x004a56d2`); loads the object address then `jmp 0x0043cb50` EditBox::dtor (0x004385c5) |
| 0x0043cbc0 | FUN_0043cbc0 | retPop8_43cbc0 | ui | live | 3 | vslot-default | `ret 8` 0x0043cbc0; 363 slot rows (slots 23, 27-34, 38-40 ...) in e1_vtables.tsv, the most shared default |
| 0x0043cc40 | FUN_0043cc40 | exitDtorArray_images_43cc40 | crt | live | 23 | exit-dtor | registered by atexit through the stub at 0x0043cc30 (`push 0x0043cc40`, `call 0x004a56d2`); pushes element destructor 0x00483010, count 0xbe, element size 0x58, array ... |
| 0x0043cc90 | FUN_0043cc90 | exitDtor_palette5aa6f0_43cc90 | crt | live | 10 | exit-dtor | registered by atexit through the stub at 0x0043cc80 (`push 0x0043cc90`, `call 0x004a56d2`); loads the object address then `jmp 0x00483010` PaletteCache::dtor (0x0043cc... |
| 0x0043ccd0 | FUN_0043ccd0 | exitDtor_bink_43ccd0 | crt | live | 10 | exit-dtor | registered by atexit through the stub at 0x0043ccc0 (`push 0x0043ccd0`, `call 0x004a56d2`); loads the object address then `jmp 0x00487040` BinkPlayer::dtor (0x0043ccd5) |
| 0x0043d4a0 | FUN_0043d4a0 | exitDtorSurfaces_43d4a0 | crt | live | 43 | exit-dtor | registered by atexit through the stub at 0x0043d490 (`push 0x0043d4a0`); stores vtable 0x004ba2d8 into g_sprite5aa748 0x005aa748 and calls Surface_free 0x00473ae0 (0x0... |
| 0x0043d500 | FUN_0043d500 | exitDtor_surface820e90_43d500 | crt | live | 20 | exit-dtor | registered by atexit through the stub at 0x0043d4f0 (`push 0x0043d500`); `mov [0x00820e90], 0x004ba2d8` 0x0043d500 then `jmp 0x00473ae0` Surface_free 0x0043d50f |
| 0x0043d660 | FUN_0043d660 | exitDtor_image5aa778_43d660 | crt | live | 10 | exit-dtor | registered by atexit through the stub at 0x0043d650 (`push 0x0043d660`, `call 0x004a56d2`); loads the object address then `jmp 0x00474c40` Image_dtor (0x0043d665) |

## Groups

- exit-dtor: 41. atexit destructors registered by the registerExit_ / atexitRegister_ stubs (each push confirmed by scanning every push of the address in .text): FontRef globals, Image / Palette / Bink / EditBox singletons, Flic / Sprite / Palette / Image arrays, groups of Surface globals.
- vslot-default: 11. default bodies stored in the virtual tables of the window classes (return a constant, return an argument or this, or only pop the arguments).
- deleting-dtor: 6. scalar deleting destructors reached through sub-objects (this - 0x1c SgRecList, this - 0x14 / 0x10 / 8 HeapObject).
- surface-fwd: 5. forwarders that adapt arguments to Surface_createFull / Image_recreateSurface / Image_releaseAll / Window::refreshFull (the +0x274 Surface sub-object slots 0-2, slot 73, and one mainLoop helper).
- save-course: 3. saveCourse, its string setter and the JPEG scanline callback.
- terrain-level: 2. the two 5x5 neighbourhood relaxers over g_cornerLevel with g_levelFalloffKernel.
- ui-panel: 2. panelListSelect and shotStyleButtonCode.
- course-flow: 1. switchToCourse.

## Counts

- named: 71 (distinct functions); deferred: 0.
- reach: all 70 are `live` in h_candidates.tsv; 0 callers in the call graph for every function that is stored in a vtable, an atexit stub or a function pointer.

## Defects found in the original (instruction addresses, not fixed)

- raiseCornerLevelsAround 0x00406f20 / lowerCornerLevelsAround 0x00406f90: the 5x5 window starts at index - 0x68 and has no bounds test, so a centre within two of an edge reads and writes bytes outside the 51 x 51 grid (0x00406f4d, 0x00406fbd; rows wrap into the neighbouring row at the column edges).
- saveCourse 0x0040b680: copies the g_textBuffer file name into a 0x64-byte stack local with no length bound (0x0040b693..0x0040b6b4), stores the _open result without testing it (0x0040b6e0) and then writes to whatever handle that is.
- setPendingCourseInfo 0x004321a0: unbounded string copy into the 0xdc-byte g_pendingCourseInfo (0x004321bd..0x004321c4).
- panelListSelect 0x0040d890: mode 1 and the default arm index g_panelListIds / 0x0059bfc4 by the argument with no bound (0x0040d98b, 0x0040d9fb); only mode 2 tests it against 0x0056199c (0x0040d913).
- saveCourseJpg_scanline 0x00431be0: divides by n*n with n read from 0x004c3074 (0x00431cb1, 0x00431ccb); a zero scale faults.

## Model corrections noticed

- g_ui_567afc 0x00567afc (e1 symbol) is read by panelListSelect 0x0040d89f..0x0040d903 as a panel mode selector (3 shot style, 2 golfer list, 1 hole list, other theme slot), not only by drawFlyingBall: a mode word.
- f_w_4c2848 0x004c2848 receives the theme slot taken from 0x0059bfc4[arg] (0x0040d9e1..0x0040da02) and 0x004c2854 receives the hole from g_panelListIds (0x0040d99a); both are selections, not an undifferentiated dword array.
- g_value56199c 0x0056199c is the length bound of the golfer-id list g_panelListIds in mode 2 (0x0040d913).
- deletingDtor_adj1c_402720 and _402800 are slot 0 of the SgRecList +0x28 tables 0x004ba26c and 0x004ba2c0 (e1_vtables.tsv); the third, 0x00402790, stores vtables 0x004ba2ac / 0x004ba2a8 (0x00402797, 0x004027a3) which e1_vtables.tsv has no rows for: the table 0x004ba2a8 is missing from the map. All three are deleting destructors (this - 0x1c), not constructors.
- 0x00404350 (target of exitDtor_rootWindow_404340) and 0x00404d00-family neighbours were outside this chunk; their names were not touched.
