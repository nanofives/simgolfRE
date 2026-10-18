# Stream H chunk h3 notes (0x00485090..0x00488fa0, 71 functions, 2026-10-18)

Every function of the chunk is a vtable slot body, a timer callback or a static-init/exit stub: all reach `live` through .rdata, a pushed callback or the initializer table, and none has a direct caller. Forwarders are named by the slot index of the TARGET vtable: `fwdDevN` / `callDevN` for the device object at this+0x40 of Snd and VoiceRx, `fwdChildN` / `callChildN` for the child object at this+0x14 of the NodeList classes. `fwd` returns the target result, `call` drops it and returns 0.

| address | old name | new name | subsystem | reach | evidence | group |
|---|---|---|---|---|---|---|
| 0x00485090 | FUN_00485090 | Snd::fwdDev48 | audio | live | slot 48 (+0xc0) of the vtable 0x004bacf8; loads this+0x40 and tests it (0x00485090), calls the device vtable +0xc0 (tail jmp or return of it | Snd device forwarders |
| 0x004850b0 | FUN_004850b0 | Snd::fwdDev49 | audio | live | slot 49 (+0xc4) of the vtable 0x004bacf8; loads this+0x40 (0x004850b0), pushes three arguments and calls device vtable +0xc4 (0x004850c8); n | Snd device forwarders |
| 0x004850f0 | FUN_004850f0 | Snd::setField60 | audio | live | slot 59 (+0xec) of the vtable 0x004bacf8; stores argument 1 in this+0x60 (0x004850f4), calls device vtable +0xec (0x00485101); no device ret | Snd setters |
| 0x00485120 | FUN_00485120 | returnB_485120 | audio | live | `mov eax, 0xb; ret` 0x00485120 (6 bytes); slot 41 (+0xa4) of the vtable 0x004bacf8 | constant returners |
| 0x00485130 | FUN_00485130 | returnBPop12_485130 | audio | live | `mov eax, 0xb; ret 0xc` 0x00485130 (8 bytes); slots 42..45 (+0xa8..+0xb4) of the vtable 0x004bacf8 | constant returners |
| 0x004851b0 | FUN_004851b0 | Snd::fwdDev54 | audio | live | slot 54 (+0xd8) of the vtable 0x004bacf8; loads this+0x40 and tests it (0x004851b0), calls the device vtable +0xd8 (tail jmp or return of it | Snd device forwarders |
| 0x004851e0 | FUN_004851e0 | Snd::fwdDev55 | audio | live | slot 55 (+0xdc) of the vtable 0x004bacf8; loads this+0x40 and tests it (0x004851e0), calls the device vtable +0xdc (tail jmp or return of it | Snd device forwarders |
| 0x00485200 | FUN_00485200 | Snd::fwdDev56 | audio | live | slot 56 (+0xe0) of the vtable 0x004bacf8; loads this+0x40 and tests it (0x00485200), calls the device vtable +0xe0 (tail jmp or return of it | Snd device forwarders |
| 0x00485220 | FUN_00485220 | Snd::fwdDev57 | audio | live | slot 57 (+0xe4) of the vtable 0x004bacf8; loads this+0x40 and tests it (0x00485220), calls the device vtable +0xe4 (tail jmp or return of it | Snd device forwarders |
| 0x00485240 | FUN_00485240 | Snd::fwdDev58 | audio | live | slot 58 (+0xe8) of the vtable 0x004bacf8; loads this+0x40 and tests it (0x00485240), calls the device vtable +0xe8 (tail jmp or return of it | Snd device forwarders |
| 0x00485290 | FUN_00485290 | SndBase::setField38 | audio | live | slot 2 (+0x8) of the vtables 0x004bac68, 0x004bacf8 and 0x004badf8 (slot 1 of 0x004badec); returns 0xa when argument 1 is 0 (0x00485298), el | Snd setters |
| 0x00485380 | FUN_00485380 | returnBPop4_485380 | audio | live | `mov eax, 0xb; ret 4` 0x00485380 (8 bytes); slot 27 (+0x6c) of the vtables 0x004badf8 and 0x004bbc3c | constant returners |
| 0x00485390 | FUN_00485390 | VoiceRx::fwdDev37 | audio | live | slot 37 (+0x94) of the vtable 0x004badf8 and 0x004bbc3c; loads this+0x40 and tests it (0x00485390), calls the device vtable +0x94 (tail jmp  | VoiceRx device forwarders |
| 0x00485450 | FUN_00485450 | VoiceRx::start | audio | live | slot 33 (+0x84) of the vtables 0x004badf8 and 0x004bbc3c (Snd::start is slot 33 of 0x004bacf8); creates the device with createSound4840e0 0x | VoiceRx |
| 0x00485500 | FUN_00485500 | VoiceRx::callDev34 | audio | live | slot 34 (+0x88) of the vtable 0x004badf8 and 0x004bbc3c; loads this+0x40 and tests it (0x00485500), calls the device vtable +0x88 (result dr | VoiceRx device forwarders |
| 0x00485520 | FUN_00485520 | VoiceRx::callDev8 | audio | live | slot 8 (+0x20) of the vtable 0x004badf8 and 0x004bbc3c; loads this+0x40 and tests it (0x00485520), calls the device vtable +0x20 (result dro | VoiceRx device forwarders |
| 0x00485530 | FUN_00485530 | VoiceRx::fwdDev5 | audio | live | slot 5 (+0x14) of the vtable 0x004badf8 and 0x004bbc3c; loads this+0x40 and tests it (0x00485530), calls the device vtable +0x14 (tail jmp o | VoiceRx device forwarders |
| 0x00485550 | FUN_00485550 | VoiceRx::fwdDev35 | audio | live | slot 35 (+0x8c) of the vtable 0x004badf8 and 0x004bbc3c; loads this+0x40 and tests it (0x00485550), calls the device vtable +0x8c (tail jmp  | VoiceRx device forwarders |
| 0x00485570 | FUN_00485570 | VoiceRx::fwdDev36 | audio | live | slot 36 (+0x90) of the vtable 0x004badf8 and 0x004bbc3c; loads this+0x40 and tests it (0x00485570), calls the device vtable +0x90 (tail jmp  | VoiceRx device forwarders |
| 0x004864f0 | FUN_004864f0 | EditBox::drawContents | ui | live | slot 22 (+0x58) of the SgEditBox vtable 0x004ba4a8; paints through Window::paint 0x004808c0 (0x00486531), insets the rect once per border st | EditBox |
| 0x00486af0 | FUN_00486af0 | EditBox::relayoutSlot57 | ui | live | slot 57 (+0xe4) of the SgEditBox vtable 0x004ba4a8; `mov eax,[ecx]; call [eax+0x120]; ret 8` 0x00486af0..0x00486af8, vtable +0x120 is Window | EditBox |
| 0x00486b00 | FUN_00486b00 | EditBox::fwdSlot57From58 | ui | live | slot 58 (+0xe8) of the SgEditBox vtable 0x004ba4a8; pushes its two arguments and calls this vtable +0xe4 (slot 57, EditBox::relayoutSlot57)  | EditBox |
| 0x00486b70 | FUN_00486b70 | EditBox::caretBlinkTick | ui | live | pushed at 0x004861de before the call to Timer::init 0x00486cf0 on this+0x5ac (0x004861e3..0x004861e9, interval 0x1f4, resolution 5); toggles | EditBox |
| 0x00486f40 | FUN_00486f40 | Timer::windowTimerProc | util | live | pushed at 0x00486e10 and 0x00486e84 before the SetTimer call [0x004ba1b8] inside Timer::startMmPeriodic 0x00486dc0 / Timer::startMmOneShot 0 | Timer |
| 0x00487490 | FUN_00487490 | NodeList::fwdChild19 | util | live | slot 19 (+0x4c) of the vtable 0x004baf00; loads this+0x14 (0x00487490), tail-jumps to the child vtable +0x4c; no child: bare `ret` | NodeList child forwarders |
| 0x004874a0 | FUN_004874a0 | NodeList::fwdChild20 | util | live | slot 20 (+0x50) of the vtable 0x004baf00; loads this+0x14 (0x004874a0), tail-jumps to the child vtable +0x50; no child: bare `ret` | NodeList child forwarders |
| 0x004874b0 | FUN_004874b0 | NodeList::fwdChild14 | util | live | slot 14 (+0x38) of the vtable 0x004baf00; loads this+0x14 (0x004874b0), tail-jumps to the child vtable +0x38; no child: bare `ret` | NodeList child forwarders |
| 0x004874c0 | FUN_004874c0 | NodeList::fwdChild6 | util | live | slot 6 (+0x18) of the vtable 0x004baf00; loads this+0x14 and tests it (0x004874c0), calls the child vtable +0x18 (tail jmp or return of its  | NodeList child forwarders |
| 0x004874d0 | FUN_004874d0 | NodeList::fwdChild28 | util | live | slot 28 (+0x70) of the vtable 0x004baf00; loads this+0x14 and tests it (0x004874d0), calls the child vtable +0x70 (tail jmp or return of its | NodeList child forwarders |
| 0x004874f0 | FUN_004874f0 | NodeList::fwdChild23 | util | live | slot 23 (+0x5c) of the vtable 0x004baf00; loads this+0x14 and tests it (0x004874f0), calls the child vtable +0x5c (tail jmp or return of its | NodeList child forwarders |
| 0x00487500 | FUN_00487500 | NodeList::playChildSuspendingSounds | util | live | slot 7 (+0x1c) of the vtable 0x004baf00 (Snd::play is slot 7 of 0x004bacf8); returns 2 when this+0x14 is NULL (0x00487513); walks the sound  | NodeList |
| 0x004875c0 | FUN_004875c0 | NodeList::callChild8 | util | live | slot 8 (+0x20) of the vtable 0x004baf00; loads this+0x14 and tests it (0x004875c0), calls the child vtable +0x20 (result dropped, returns 0) | NodeList child forwarders |
| 0x004875e0 | FUN_004875e0 | NodeList::fwdChild15 | util | live | slot 15 (+0x3c) of the vtable 0x004baf00; loads this+0x14 (0x004875e0), calls the child vtable +0x3c with one argument and returns its resul | NodeList child forwarders |
| 0x00487600 | FUN_00487600 | NodeList::fwdChild16 | util | live | slot 16 (+0x40) of the vtable 0x004baf00; loads this+0x14 and tests it (0x00487600), calls the child vtable +0x40 (tail jmp or return of its | NodeList child forwarders |
| 0x00487610 | FUN_00487610 | NodeList::fwdChild9 | util | live | slot 9 (+0x24) of the vtable 0x004baf00; loads this+0x14 (0x00487610), calls the child vtable +0x24 with one argument and returns its result | NodeList child forwarders |
| 0x00487640 | FUN_00487640 | NodeList::callChild25 | util | live | slot 25 (+0x64) of the vtable 0x004baf00; loads this+0x14 and tests it (0x00487640), calls the child vtable +0x64 (result dropped, returns 0 | NodeList child forwarders |
| 0x00487650 | FUN_00487650 | NodeList::callChild26 | util | live | slot 26 (+0x68) of the vtable 0x004baf00; loads this+0x14 and tests it (0x00487650), calls the child vtable +0x68 (result dropped, returns 0 | NodeList child forwarders |
| 0x00487660 | FUN_00487660 | NodeList::fwdChild27 | util | live | slot 27 (+0x6c) of the vtable 0x004baf00; loads this+0x14 and tests it (0x00487660), calls the child vtable +0x6c (tail jmp or return of its | NodeList child forwarders |
| 0x00487680 | FUN_00487680 | NodeList::fwdChild21 | util | live | slot 21 (+0x54) of the vtable 0x004baf00; loads this+0x14 and tests it (0x00487680), calls the child vtable +0x54 (tail jmp or return of its | NodeList child forwarders |
| 0x004876a0 | FUN_004876a0 | NodeList::fwdChild22 | util | live | slot 22 (+0x58) of the vtable 0x004baf00; loads this+0x14 and tests it (0x004876a0), calls the child vtable +0x58 (tail jmp or return of its | NodeList child forwarders |
| 0x004877d0 | FUN_004877d0 | NodeListBucket::ctor | util | live | re/model/e2_classes.tsv NodeListBucket; stores vtable 0x004bafac at +8 (0x004877d4), zeroes +0xc..+0x18 (0x004877db..0x004877e4), +4 = 0x7f  | NodeList |
| 0x00487800 | FUN_00487800 | NodeList::fwdChild30 | util | live | slot 30 (+0x78) of the vtable 0x004baf00; loads this+0x14 and tests it (0x00487800), calls the child vtable +0x78 (tail jmp or return of its | NodeList child forwarders |
| 0x00487810 | FUN_00487810 | NodeList::fwdChild32 | util | live | slot 32 (+0x80) of the vtable 0x004baf00; loads this+0x14 and tests it (0x00487810), calls the child vtable +0x80 (tail jmp or return of its | NodeList child forwarders |
| 0x00487830 | FUN_00487830 | NodeList::fwdChild31 | util | live | slot 31 (+0x7c) of the vtable 0x004baf00; loads this+0x14 and tests it (0x00487830), calls the child vtable +0x7c (tail jmp or return of its | NodeList child forwarders |
| 0x00487850 | FUN_00487850 | NodeList::fwdChild33 | util | live | slot 33 (+0x84) of the vtable 0x004baf00; loads this+0x14 and tests it (0x00487850), calls the child vtable +0x84 (tail jmp or return of its | NodeList child forwarders |
| 0x00487870 | FUN_00487870 | NodeList::fwdChild34 | util | live | slot 34 (+0x88) of the vtable 0x004baf00; loads this+0x14 and tests it (0x00487870), calls the child vtable +0x88 (tail jmp or return of its | NodeList child forwarders |
| 0x00487890 | FUN_00487890 | NodeList::fwdChild35 | util | live | slot 35 (+0x8c) of the vtable 0x004baf00; loads this+0x14 and tests it (0x00487890), calls the child vtable +0x8c (tail jmp or return of its | NodeList child forwarders |
| 0x004878c0 | FUN_004878c0 | NodeList::fwdChild36 | util | live | slot 36 (+0x90) of the vtable 0x004baf00; loads this+0x14 and tests it (0x004878c0), calls the child vtable +0x90 (tail jmp or return of its | NodeList child forwarders |
| 0x004878f0 | FUN_004878f0 | NodeList::fwdChild37 | util | live | slot 37 (+0x94) of the vtable 0x004baf00; loads this+0x14 and tests it (0x004878f0), calls the child vtable +0x94 (tail jmp or return of its | NodeList child forwarders |
| 0x00487910 | FUN_00487910 | NodeList::fwdChild38 | util | live | slot 38 (+0x98) of the vtable 0x004baf00; loads this+0x14 and tests it (0x00487910), calls the child vtable +0x98 (tail jmp or return of its | NodeList child forwarders |
| 0x00487930 | FUN_00487930 | NodeList::fwdChild39 | util | live | slot 39 (+0x9c) of the vtable 0x004baf00; loads this+0x14 and tests it (0x00487930), calls the child vtable +0x9c (tail jmp or return of its | NodeList child forwarders |
| 0x00487950 | FUN_00487950 | NodeList::fwdChild40 | util | live | slot 40 (+0xa0) of the vtable 0x004baf00; loads this+0x14 and tests it (0x00487950), calls the child vtable +0xa0 (tail jmp or return of its | NodeList child forwarders |
| 0x00487970 | FUN_00487970 | NodeList::fwdChild41 | util | live | slot 41 (+0xa4) of the vtable 0x004baf00; loads this+0x14 and tests it (0x00487970), calls the child vtable +0xa4 (tail jmp or return of its | NodeList child forwarders |
| 0x00487990 | FUN_00487990 | NodeList::fwdChild42 | util | live | slot 42 (+0xa8) of the vtable 0x004baf00; loads this+0x14 and tests it (0x00487990), calls the child vtable +0xa8 (tail jmp or return of its | NodeList child forwarders |
| 0x00487b10 | FUN_00487b10 | NodeListB::callChild22 | util | live | slot 22 (+0x58) of the vtable 0x004bafb0; loads this+0x14 (0x00487b10), calls the child vtable +0x58 with the result dropped (0x00487b19), r | NodeList child forwarders |
| 0x00487b20 | FUN_00487b20 | NodeListB::fwdChild24 | util | live | slot 24 (+0x60) of the vtable 0x004bafb0; loads this+0x14 (0x00487b20), tail-jumps to the child vtable +0x60 (0x00487b29); no child returns  | NodeList child forwarders |
| 0x00487c70 | FUN_00487c70 | NodeListC::fwdChild25 | util | live | slot 25 (+0x64) of the vtable 0x004bb014; loads this+0x14 (0x00487c70), calls the child vtable +0x64 with one argument (0x00487c7e); no chil | NodeList child forwarders |
| 0x00487c90 | FUN_00487c90 | NodeListC::fwdChild26 | util | live | slot 26 (+0x68) of the vtable 0x004bb014; loads this+0x14 (0x00487c90), calls the child vtable +0x68 with one argument (0x00487c9e); no chil | NodeList child forwarders |
| 0x00487cb0 | FUN_00487cb0 | returnZeroPop12_487cb0 | util | caller | `xor eax, eax; ret 0xc` 0x00487cb0 (5 bytes); slot 8 (+0x20) of the vtables 0x004bafb0 and 0x004bb014 | constant returners |
| 0x00487cc0 | FUN_00487cc0 | NodeListB::callChild23 | util | live | slot 23 (+0x5c) of the vtables 0x004bafb0 and 0x004bb014; loads this+0x14 (0x00487cc0), calls the child vtable +0x5c with the result dropped | NodeList child forwarders |
| 0x00487cd0 | FUN_00487cd0 | NodeListC::callChild24 | util | live | slot 24 (+0x60) of the vtable 0x004bb014; loads this+0x14 (0x00487cd0), calls the child vtable +0x60 with the result dropped (0x00487cd9), r | NodeList child forwarders |
| 0x00487da0 | FUN_00487da0 | atexitDtor_83b000 | boot | live | dword 0x00487da0 pushed at 0x00487d90 before the call to 0x004a56d2 inside the registration stub 0x00487d90; sets the vtable 0x004bb084 at 0 | static init and exit stubs |
| 0x00487dc0 | FUN_00487dc0 | dynInit_initCreditsPtr0_487dc0 | boot | live | `jmp 0x00487dd0` 0x00487dc0 (5 bytes); the dword 0x00487dc0 sits in slot 0x004c1144 of the dynamic-initializer table 0x004c1004-0x004c1184 ( | static init and exit stubs |
| 0x00487dd0 | FUN_00487dd0 | initCreditsPtr0_487dd0 | boot | live | copies the dword at 0x0083b15c to 0x0083b164 (0x00487dd0..0x00487dd5); g_credits_text_ptrs 0x0083b164 is read by showCreditsScreen 0x0044bb0 | static init and exit stubs |
| 0x00487de0 | FUN_00487de0 | dynInit_initCreditsPtr1_487de0 | boot | live | `jmp 0x00487df0` 0x00487de0 (5 bytes); the dword 0x00487de0 sits in slot 0x004c1148 of the dynamic-initializer table 0x004c1004-0x004c1184 ( | static init and exit stubs |
| 0x00487df0 | FUN_00487df0 | initCreditsPtr1_487df0 | boot | live | copies the dword at 0x0083b160 to 0x0083b168 (0x00487df0..0x00487df5); second slot of g_credits_text_ptrs 0x0083b164 (stride 4, count 2); en | static init and exit stubs |
| 0x00487e40 | FUN_00487e40 | atexitDtor_83b170 | boot | live | dword 0x00487e40 pushed at 0x00487e30 before the call to 0x004a56d2 inside the registration stub 0x00487e30; pushes 0x004882f0 (KeyTable::dt | static init and exit stubs |
| 0x00488bf0 | FUN_00488bf0 | Button::endCapture | ui | live | slot 31 (+0x7c) of the SgButton vtable 0x004bb0a8 and SgLabelButton 0x004bb5b4 (EditBox::endCaretDrag is slot 31 of 0x004ba4a8); clears g_wi | Button |
| 0x00488cb0 | FUN_00488cb0 | returnZeroPop8_488cb0 | ui | live | `xor eax, eax; ret 8` 0x00488cb0 (5 bytes); slot 35 (+0x8c) of the vtables 0x004bb0a8 and 0x004bb5b4 | constant returners |
| 0x00488cc0 | FUN_00488cc0 | Button::moveIfFocusedPressed | ui | live | slot 28 (+0x70) of the SgButton vtable 0x004bb0a8; takes g_focusWin 0x0083ab40, else g_focusWin2 0x0083ab44 (0x00488cc0..0x00488cc9), requir | Button |
| 0x00488fa0 | FUN_00488fa0 | Button::relayoutSlot60 | ui | live | slot 60 (+0xf0) of the vtables 0x004bb0a8 and 0x004bb5b4; `mov eax,[ecx]; call [eax+0x120]; ret 4` 0x00488fa0..0x00488fa8, vtable +0x120 is  | Button |

## Counts

Named 71 of 71, deferred 0.

Groups: NodeList child forwarders 34, Snd device forwarders 7, VoiceRx device forwarders 6, static init and exit stubs 6, constant returners 5, EditBox 4, Button 3, Snd setters 2, NodeList 2, VoiceRx 1, Timer 1.


Deferred reasons: none (no function deferred; the largest tail of functions is one-line forwarders whose slot and default value are read from the disassembly).

## Defects found (instruction addresses, not fixed)

- NodeList::fwdChild15 0x004875e0 and NodeList::fwdChild9 0x00487610 return without setting eax when this+0x14 is NULL (0x004875f1, 0x00487621): the result is the caller's stale eax.
- Button::endCapture 0x00488bf0 loads this+0x620 and tests it at 0x00488c8a, then never branches on it (the flags are overwritten by the call at 0x00488c8f).
- EditBox::drawContents 0x004864f0 divides by the value of Widget_value at 0x0048661e (idiv ecx, ecx from 0x0048660a): a zero font value faults.
- VoiceRx::start 0x00485450 returns the createSound4840e0 result unchanged when that result is nonzero (0x0048546e jumps to 0x004854af with eax = that result); the slot 31 call at 0x004854a9 is skipped on that path.

## Model corrections noticed

- e2_vtables.tsv slots 42..45 of Snd (0x004bacf8) all point at 0x00485130 and slot 41 at 0x00485120: both are constant returners (0xb), so those slots are unimplemented stubs, not device forwarders.
- Offset 0x40 is the sound device pointer of Snd and VoiceRx; offset 0x14 is the child object of the NodeList / NodeListB / NodeListC classes (their forwarders all load it first). If the model header names these members, they are the device pointer and the child pointer.
- Timer offsets: +0x24 pending flag (written by 0x00486f77), +0x8 flags, as already used by Timer_fire.
- The EditBox fields touched by drawContents: +0x57c first visible line, +0x584 background argument to Window::paint, +0x590/+0x594 border start values, +0x598 border step count, +0x59c inner inset, +0x5a4 caret byte position, +0x5a8 caret-visible flag, +0x2e0 caret colour; the timer object is at +0x5ac.
