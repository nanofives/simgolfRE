# Stream H chunk h4 notes (2026-10-18)

Chunk h4 = the rows with chunk = h4 in re/model/h_candidates.tsv: 68 functions, 0x00489ae0..0x004a4ac0, 9,495 bytes. Names are in `re/names/model_h4.tsv` (evidence column cites instruction addresses). All 68 are reached through vtable or table slots, stored pointers or the C++ initializer / atexit tables (no direct call), except 0x0049c0a0, 0x004a2980 and 0x004a4340, which have recorded callers.

| address | old name | new name | subsystem | reach | evidence (short) | group |
|---|---|---|---|---|---|---|
| 0x00489ae0 | FUN_00489ae0 | ListModel::deletingDtor_adj28 | ui | live (0 callers) | slot 0 of the table 0x004bb228 (list model at +0x28) | B list deleting dtors |
| 0x0048b3f0 | FUN_0048b3f0 | ListBox::drawItem | ui | live (0 callers) | slot 0 of the table 0x004bb3cc installed by ListBox::ctor 0x00489cb0 (also slot 0 of 0x004bba3c, viewCtor4a0740) | C ListBox drawing |
| 0x0048cd30 | FUN_0048cd30 | ListBox::deletingDtor_adj274 | ui | live (0 callers) | slot 3 (+0xc) of the table 0x004bb24c (ListBox sub-object at +0x2d0) | A this-adjustor thunks |
| 0x0048cd50 | FUN_0048cd50 | ListBox::deletingDtor_vtordisp_adj578 | ui | live (0 callers) | slot 0 (+0x0) of the table 0x004bb240 (sub-object at +0x5d4) | A this-adjustor thunks |
| 0x0048cd70 | FUN_0048cd70 | ListBox::drawContents_vtordisp_adj578 | ui | live (0 callers) | slot 2 (+0x8) of the table 0x004bb240 | A this-adjustor thunks |
| 0x00490ac0 | FUN_00490ac0 | TextView::isNotFlag4000 | ui | live (0 callers) | slot 35 (+0x8c) of the TextView table 0x004bb3f8 | E TextView slots |
| 0x00490ae0 | FUN_00490ae0 | TextView::hitTest | ui | live (0 callers) | slot 80 (+0x140) of the TextView table 0x004bb3f8 | E TextView slots |
| 0x00490b10 | FUN_00490b10 | TextView::isNotFlag4000Arg1 | ui | live (0 callers) | slot 37 (+0x94) of the TextView table 0x004bb3f8 | E TextView slots |
| 0x00490b30 | FUN_00490b30 | textViewSelectionSyncCb | ui | live (0 callers) | address stored into [esi+0x14b4] by TextView::render 0x0048fe60 at 0x0049034e and 0x00490368 | E TextView slots |
| 0x00490d50 | FUN_00490d50 | TextView::isGlobal83b8ecNull | ui | live (0 callers) | slot 69 (+0x114) of the TextView table 0x004bb3f8 | E TextView slots |
| 0x00491370 | FUN_00491370 | TextView::onMouseDown | ui | live (0 callers) | slot 30 (+0x78) of the TextView table 0x004bb3f8 (slot 30 is scrollbarOnDown 0x00496c30 in the scrollbar table 0x004bbac8) | E TextView slots |
| 0x004913c0 | FUN_004913c0 | TextView::setField14bcTriple | ui | live (0 callers) | slot 92 (+0x170) of the TextView table 0x004bb3f8 | E TextView slots |
| 0x004913f0 | FUN_004913f0 | TextView::setField14d0 | ui | live (0 callers) | slot 93 (+0x174) of the TextView table 0x004bb3f8 | E TextView slots |
| 0x00491400 | FUN_00491400 | TextView::setField14d4 | ui | live (0 callers) | slot 94 (+0x178) of the TextView table 0x004bb3f8 | E TextView slots |
| 0x004916e0 | FUN_004916e0 | TextView::alwaysFalse | ui | live (0 callers) | slot 105 (+0x1a4) of the TextView table 0x004bb3f8 | E TextView slots |
| 0x00491a70 | FUN_00491a70 | viewDeletingDtor_adj274 | ui | live (0 callers) | slot 3 (+0xc) of the table 0x004bb8bc (composite view sub-object at +0x470) | A this-adjustor thunks |
| 0x00491a90 | FUN_00491a90 | viewDeletingDtor_vtordisp_adj578 | ui | live (0 callers) | slot 0 (+0x0) of the table 0x004bb8b0 (sub-object at +0x774) | A this-adjustor thunks |
| 0x00491ab0 | FUN_00491ab0 | viewDispatchDraw_vtordisp_adj578 | ui | live (0 callers) | slot 2 (+0x8) of the table 0x004bb8b0 | A this-adjustor thunks |
| 0x00491bc0 | FUN_00491bc0 | listPanelDeletingDtor_adj274 | ui | live (0 callers) | slot 3 (+0xc) of the table 0x004bb730 (list panel part at +0x298) | A this-adjustor thunks |
| 0x00491be0 | FUN_00491be0 | listPanelDeletingDtor_vtordisp_adj578 | ui | live (0 callers) | slot 0 (+0x0) of the table 0x004bb724 (sub-object at +0x59c) | A this-adjustor thunks |
| 0x00491c00 | FUN_00491c00 | listPanelDrawContents_vtordisp_adj578 | ui | live (0 callers) | slot 2 (+0x8) of the table 0x004bb724 | A this-adjustor thunks |
| 0x00492430 | FUN_00492430 | dynInit_clearGlobal83c314 | boot | live (0 callers) | 5-byte `jmp 0x00492440` at 0x00492430 | K static init |
| 0x00492440 | FUN_00492440 | initClearGlobal83c314 | boot | live (0 callers) | `mov dword ptr [0x83c314], 0` 0x00492440, `ret` 0x0049244a | K static init |
| 0x00492650 | FUN_00492650 | HotListEntry::ctor | ui | live (0 callers) | stores the HotListEntry vtable 0x004bba70 at [eax] (0x00492652) and clears [eax+0x1c] (0x00492658) | F HotList |
| 0x004927a0 | FUN_004927a0 | HotListEntry::deletingDtor | ui | live (0 callers) | slot 0 of the table 0x004bba70 | F HotList |
| 0x00492d70 | FUN_00492d70 | HotListEntry::dtor | ui | live (0 callers) | `mov [ecx], 0x4bba70` 0x00492d70 then `jmp 0x00492660` (HotList::freeEntryTip) 0x00492d76 | F HotList |
| 0x00495750 | FUN_00495750 | uiEventForwardRootSlot3 | ui | live (0 callers) | slot 0 (+0x0) of the UiEventTable 0x004ba87c stored by storeUiEventTable 0x004799d0 | G UI event table |
| 0x00495790 | FUN_00495790 | uiPointerRegionCode | ui | live (0 callers) | slot 2 (+0x8) of the UiEventTable 0x004ba87c | G UI event table |
| 0x00495a50 | FUN_00495a50 | uiScrollEvent | ui | live (0 callers) | slot 6 (+0x18) of the UiEventTable 0x004ba87c | G UI event table |
| 0x00495aa0 | FUN_00495aa0 | uiMousePress1 | ui | live (0 callers) | slot 7 (+0x1c) of the UiEventTable 0x004ba87c | G UI event table |
| 0x00495b90 | FUN_00495b90 | uiMouseRelease1 | ui | live (0 callers) | slot 8 (+0x20) of the UiEventTable 0x004ba87c | G UI event table |
| 0x00495bd0 | FUN_00495bd0 | uiMousePress2 | ui | live (0 callers) | slot 9 (+0x24) of the UiEventTable 0x004ba87c | G UI event table |
| 0x00495c30 | FUN_00495c30 | uiMouseRelease2 | ui | live (0 callers) | slot 10 (+0x28) of the UiEventTable 0x004ba87c | G UI event table |
| 0x00495c70 | FUN_00495c70 | uiActivateEvent | ui | live (0 callers) | slot 11 (+0x2c) of the UiEventTable 0x004ba87c | G UI event table |
| 0x00495d20 | FUN_00495d20 | uiIdleNetPoll | net | live (0 callers) | slot 13 (+0x34) of the UiEventTable 0x004ba87c | G UI event table |
| 0x00496010 | FUN_00496010 | dynInit_hscrollHeight | boot | live (0 callers) | 5-byte `jmp 0x00496020` at 0x00496010 | K static init |
| 0x00496020 | FUN_00496020 | initHscrollHeight | boot | live (0 callers) | `push 3` 0x00496020, `call [0x4ba1c8]` 0x00496022 (USER32 GetSystemMetrics import slot, index 3), `mov [0x83ff10], eax` 0x00496028 | K static init |
| 0x00496860 | FUN_00496860 | scrollbarOnMouseMove | ui | live (0 callers) | slot 27 (+0x6c) of the scrollbar table 0x004bbac8 (slot 27 is the one LabelButton::onMouseMove 0x004a2010 calls at 0x004a21f0) | H scrollbar |
| 0x00496d60 | FUN_00496d60 | scrollbarOnRelease | ui | live (0 callers) | slot 29 (+0x74) of the scrollbar table 0x004bbac8 (slot 29 is Button::endPress 0x00488eb0) | H scrollbar |
| 0x00497170 | FUN_00497170 | scrollbarSetSpan | ui | live (0 callers) | slot 23 (+0x5c) of the scrollbar table 0x004bbac8 | H scrollbar |
| 0x00497260 | FUN_00497260 | scrollbarArrowRepeat | ui | live (0 callers) | slot 50 (+0xc8) of the scrollbar table 0x004bbac8 | H scrollbar |
| 0x00497f60 | FUN_00497f60 | netMarkRecordCb | net | live (0 callers) | address stored by netService 0x00497fc0 at 0x00498443 | J net |
| 0x0049c010 | FUN_0049c010 | gDebugWindowDtor_49c010 | boot | live (0 callers) | address pushed by regAtexit_49c000 0x0049c000 (store 0x0049c001) | K static exit |
| 0x0049c0a0 | FUN_0049c0a0 | DebugWindow::clearField594 | ui | live (1 callers) | `mov dword ptr [ecx + 0x594], 0` 0x0049c0a0 | D debug window |
| 0x0049c0b0 | FUN_0049c0b0 | DebugWindow::drawContents | ui | live (0 callers) | slot 22 (+0x58) of the SgDebugWindow table 0x004bbcf4 (slot 22 is drawContents in ListBox::drawContents 0x0048c130) | D debug window |
| 0x0049d030 | FUN_0049d030 | gPairArrayDtor_49d030 | boot | live (0 callers) | address pushed by regAtexit_49d020 0x0049d020 (store 0x0049d021) | K static exit |
| 0x0049d180 | FUN_0049d180 | gSurfaceDtor_49d180 | boot | live (0 callers) | atexit target of regAtexit_49d170 0x0049d170 | K static exit |
| 0x0049d2c0 | FUN_0049d2c0 | gSurfaceDtor_49d2c0 | boot | live (0 callers) | atexit target of regAtexit_49d2b0 0x0049d2b0 | K static exit |
| 0x0049d320 | FUN_0049d320 | gSurfaceDtor_49d320 | boot | live (0 callers) | atexit target of regAtexit_49d310 0x0049d310 | K static exit |
| 0x0049d380 | FUN_0049d380 | gSurfaceDtor_49d380 | boot | live (0 callers) | atexit target of regAtexit_49d370 0x0049d370 | K static exit |
| 0x0049d4b0 | FUN_0049d4b0 | gSurfaceDtor_49d4b0 | boot | live (0 callers) | atexit target of regAtexit_49d4a0 0x0049d4a0 | K static exit |
| 0x0049d510 | FUN_0049d510 | gSurfaceDtor_49d510 | boot | live (0 callers) | atexit target of regAtexit_49d500 0x0049d500 | K static exit |
| 0x0049d570 | FUN_0049d570 | gSurfaceDtor_49d570 | boot | live (0 callers) | atexit target of regAtexit_49d560 0x0049d560 | K static exit |
| 0x0049eb20 | FUN_0049eb20 | gSurfaceDtor_49eb20 | boot | live (0 callers) | atexit target of regAtexit_49eb10 0x0049eb10 | K static exit |
| 0x0049eb80 | FUN_0049eb80 | gSurfaceDtor_49eb80 | boot | live (0 callers) | atexit target of regAtexit_49eb70 0x0049eb70 | K static exit |
| 0x004a0010 | FUN_004a0010 | optionDialogDeletingDtor_adj274 | ui | live (0 callers) | slot 3 (+0xc) of the table 0x004bbe9c (option part at +0x290) | A this-adjustor thunks |
| 0x004a0030 | FUN_004a0030 | optionDialogDeletingDtor_vtordisp_adj578 | ui | live (0 callers) | slot 0 (+0x0) of the table 0x004bbe90 (sub-object at +0x594) | A this-adjustor thunks |
| 0x004a0050 | FUN_004a0050 | optionDialogDrawContents_vtordisp_adj578 | ui | live (0 callers) | slot 2 (+0x8) of the table 0x004bbe90 | A this-adjustor thunks |
| 0x004a0150 | FUN_004a0150 | gLoggerDtor_4a0150 | boot | live (0 callers) | address pushed by regAtexit_4a0140 0x004a0140 (store 0x004a0141) | K static exit |
| 0x004a21c0 | FUN_004a21c0 | LabelButton::endHover | ui | live (0 callers) | slot 28 (+0x70) of the LabelButton table 0x004bb5b4 | I label button |
| 0x004a2980 | FUN_004a2980 | slotArrayFieldAt574 | ui | live (1 callers) | `mov eax, [ecx + eax*4 + 4]` 0x004a2984, returns 0 when the entry is NULL (0x004a298a), else `mov eax, [eax + 0x574]` 0x004a298c | J misc accessors |
| 0x004a30c0 | FUN_004a30c0 | panelRowDeletingDtor_adj274 | ui | live (0 callers) | slot 3 (+0xc) of the table 0x004bc094 (panel row at +0x300) | A this-adjustor thunks |
| 0x004a30e0 | FUN_004a30e0 | panelRowDeletingDtor_vtordisp_adj578 | ui | live (0 callers) | slot 0 (+0x0) of the table 0x004bc088 (sub-object at +0x604) | A this-adjustor thunks |
| 0x004a3390 | FUN_004a3390 | panelSubList_deletingDtor_adj28 | ui | live (0 callers) | slot 0 of the table 0x004bc3b0 (list at +0x28) | B list deleting dtors |
| 0x004a4340 | FUN_004a4340 | TextView::endPressNop | ui | live (3 callers) | slot 29 (+0x74) of the TextView table 0x004bb3f8 (slot 29 is Button::endPress 0x00488eb0 and scrollbarOnRelease 0x00496d60) | E TextView slots |
| 0x004a4a80 | FUN_004a4a80 | panelSubDeletingDtor_adj274 | ui | live (0 callers) | slot 3 (+0xc) of the table 0x004bc230 (panel sub-view at +0x354) | A this-adjustor thunks |
| 0x004a4aa0 | FUN_004a4aa0 | panelSubDeletingDtor_vtordisp_adj578 | ui | live (0 callers) | slot 0 (+0x0) of the table 0x004bc224 (sub-object at +0x658) | A this-adjustor thunks |
| 0x004a4ac0 | FUN_004a4ac0 | panelSubDrawContents_vtordisp_adj578 | ui | live (0 callers) | slot 2 (+0x8) of the table 0x004bc224 | A this-adjustor thunks |

## Groups (roles found)

- A this-adjustor thunks: 17
- B list deleting dtors: 2
- C ListBox drawing: 1
- D debug window: 2
- E TextView slots: 11
- F HotList: 3
- G UI event table: 9
- H scrollbar: 4
- I label button: 1
- J misc accessors: 1
- J net: 1
- K static exit: 12
- K static init: 4

## Counts

- named: 68 of 68
- deferred: 0 (every function had an instruction-level role; the weakest names describe the mechanics only: slotArrayFieldAt574, TextView::isGlobal83b8ecNull, TextView::setField14bcTriple / setField14d0 / setField14d4, because what the fields mean is not determined)
- defects found in the original: none new in this chunk's code. Observations that are not defects: uiMouseRelease1 / uiMouseRelease2 clear the capture word 0x0083ab20 before the hit test, while uiMousePress1 sets it only for border codes 2 and 10..17 (0x00495b26..0x00495b32).

## Findings that change or extend older notes

- The ListBox / list-panel / option-dialog / panel-row / panel-sub / composite-view sub-objects each carry the same three-thunk set (deleting destructor at +0x274 adjust, vtordisp deleting destructor, vtordisp draw slot): 16 thunks in this chunk follow one pattern (see group A), so the 0x578 constant is the distance from the vtordisp field to the object start in all five classes.
- Slots 27 / 29 / 30 / 31 of the Window-family tables are the pointer slots: 27 mouse move (LabelButton::onMouseMove calls it, scrollbarOnMouseMove 0x00496860 fills it), 29 end of press (Button::endPress, scrollbarOnRelease 0x00496d60, TextView::endPressNop 0x004a4340), 30 mouse down (scrollbarOnDown, TextView::onMouseDown 0x00491370), 31 drag (scrollbarOnDrag). Slot 28 of LabelButton is the pointer-exit handler (0x004a21c0).
- UiEventTable 0x004ba87c (14 entries, e2_vtables.tsv): entries 7 / 8 / 9 / 10 are first-button press, first-button release, second-button press, second-button release (they call window slots 83, 84, 85, 86 and the hook words 0x0083ab04 / 0x0083ab08 / 0x0083aafc); entry 6 is the scroll event (window slot 25), entry 2 the frame region classifier, entry 11 the activate / deactivate event (ClipCursor), entry 13 the idle network poll. Entries 1, 3, 4, 5 and 12 were already named.
- Model correction (lead, not applied): g_titleBarH 0x0083ff10 is written once at startup by initHscrollHeight 0x00496020 with GetSystemMetrics(3), the value the Win32 header calls the horizontal scrollbar height (SM_CYHSCROLL); the name in model_e1 describes the readers' use (0x0047cc10 family), the store shows the value's source. Not renamed: the readers were not re-checked.
- Model correction (lead): g_ui83aa9c 0x0083aa9c is a pointer to the root window object (slot +0xc is called with four arguments by uiEventForwardRootSlot3, flag 2 of [obj+0xa0] and the rectangle at [obj+0x1bc..0x1c8] are read by uiPointerRegionCode); the model types it int32_t[2]. The word 0x0083ab20 is the pointer-capture window (cleared by the two release entries, tested by uiPointerRegionCode and uiPointerMove).
- netMarkRecordCb 0x00497f60 takes five arguments and returns 1 (the shape of an enumeration callback); the table it scans (pointer 0x0083ff6c, 16 records of 0x58 bytes, first dword = key, byte +0x14 flags, dword +8 mark) is not typed in the model (g_ui_83ff6c is int32_t).
- Static stubs: 0x00492430 / 0x00496010 are slots 84 / 85 of the initializer table 0x004c1000; the nine gSurfaceDtor_* functions guard on bits of three bytes (0x00840708, 0x00840740, 0x00840828, 0x008408c0) like the earlier gSurfaceDtor_44b* family and set the vtable 0x004ba2d8 before Surface_free.
