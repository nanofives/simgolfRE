# undisc2_exe: the composite-view vtordisp layer of golf_clean.exe (2026-10-09)

Second batch of golf_clean.exe code Ghidra never made into functions. 87 entries, listed in `log/n2_input.tsv`
and created in the Ghidra master before this batch: nine two-level switch dispatchers on `[ecx-8]`, three C++
dynamic-initializer stubs, 49 vtordisp this-adjustor thunks and the 26 methods those thunks jump to.

Everything below is read from the image with `re/tools/pointer_callers.py`, `re/tools/xref.py`, capstone, a
dword-level dump of the `.rdata` vtable region and headless decompilations of the created functions
(`re/tools/decomp.py`, after `scripts/ghidra_pool.sh sync`).

## 1. Six classes, four vtables each

Searching the code sections for literal dwords inside 0x004bb200-0x004bc400 gives, for every vtable in the
region, the functions that store it. Six of those classes own all 49 thunks of this batch. Each one installs
three consecutive tables, and every thunk of this batch sits in table A or table C:

| class (named after its constructor) | table A (3 slots) | table B (5 slots) | table C (91 slots) | installers |
|---|---|---|---|---|
| ListBox | 0x004bb240 | 0x004bb24c | 0x004bb260 | ListBox::ctor 0x00489cb0, ListBox::dtor 0x00489de0 |
| viewFieldCtor49d5a0 (`listPanel*`) | 0x004bb724 | 0x004bb730 | 0x004bb744 | viewFieldCtor49d5a0 0x0049d5a0, Dialog::viewDtor 0x00491500, Dialog::childDtor 0x00491680, listPanelDeletingDtor 0x00491ad0 |
| viewCtor4a0740 (`view*`) | 0x004bb8b0 | 0x004bb8bc | 0x004bb8d0 | viewCtor4a0740 0x004a0740, Dialog::viewDtor 0x00491500 |
| viewFieldCtor49ebb0 (`optionDialog*`) | 0x004bbe90 | 0x004bbe9c | 0x004bbeb0 | viewFieldCtor49ebb0 0x0049ebb0, viewFieldDtor49ec80 0x0049ec80 |
| viewFieldCtor4a2250 (`panelRow*`) | 0x004bc088 | 0x004bc094 | 0x004bc0a8 | viewFieldCtor4a2250 0x004a2250, viewFieldDtor4a2330 0x004a2330 |
| panelSubCtor4a3110 (`panelSub*`) | 0x004bc224 | 0x004bc230 | 0x004bc244 | panelSubCtor4a3110 0x004a3110, panelSubDtor4a3480 0x004a3480 |

The 91-slot table C is the Window-family table agent N1 numbered: slot 3 is `Window::createBackingSurface`
0x00480b30, slot 26 `Window::fwdMouseEvent238` 0x0047eff0, slot 67 `Window::runModal` 0x0047bab0, slots 81-86
`Window::onCommand` .. `Window::onReleaseInner158`, slot 74 `Container::cycleSelection` 0x0047af60. Checking those
five anchors against the ListBox table 0x004bb260 and the 0x004bb744 table reproduces every slot index N1 already
filed for the thunks in the same tables (`re/names/undisc_exe.tsv`: 0x004bb7b0 is slot 27, 0x004bc198 slot 60, and
so on), so the slot numbers in this batch use the same origin. Slot 0 of table C is the scalar deleting
destructor in all six classes; slot 1 of table A is the class's build method.

Every constructor also stores a virtual-base table, which is what the thunks and the methods read: for ListBox it
is 0x004bb3d0 (`{-4, 0x58, 0x5d0}`, stored at 0x00489ce4), reached through the dword at `this-0x58`, and its +4
entry 0x58 is the displacement added to `this` before each member access. The `sub ecx, dword ptr [ecx-4]` of
every thunk reads the per-object vtordisp field, not that table.

## 2. The this-adjustor thunks (49 rows, `<target>_vtordisp`)

All 49 have one shape, `sub ecx, dword ptr [ecx-4]; jmp <target>`, followed by `int3` padding, and each is
reached only from a vtable slot. Naming follows N1: `<target name>_vtordisp`. Two of them share a target
(0x00491a30 and 0x00491a40 both reach 0x004a0ed0 from slots 57 and 58), so those carry the slot in the name:
`viewDispatchOnScroll_vtordisp57` / `_vtordisp58`.

Eight thunks jump to functions that were already in the listing but are still unnamed (0x00487af0, 0x0049e7b0,
0x0049e990, 0x0049fc30, 0x0049fd80, 0x0049fe20, 0x004a4340, 0x004a4870). Those thunks are named after the class
and the slot instead: `ListBox::Slot60_vtordisp`, `listPanelSlot28_vtordisp`, `listPanelSlot60_vtordisp`,
`optionDialogSlot28_vtordisp`, `optionDialogSlot38_vtordisp`, `optionDialogSlot60_vtordisp`,
`panelSubSlot27_vtordisp`, `panelSubSlot60_vtordisp`. Naming those eight targets is left for a later batch.

## 3. The type tag at this-8 and the nineteen `viewDispatch*` bodies

The class whose tables viewCtor4a0740 0x004a0740 installs has nineteen table-C/table-A entries that are not
plain methods: they read a 32-bit tag (at `this-8`, or `this-0x580` for the table-A entry) and select one of five
this-adjustments, 1 -> -0x168, 2 -> -0x1a0, 4 -> -0x4, 8 -> -0x8c, 0x10 -> -0x180. The nine with five or four
live cases are the two-level VC6 switches (`mov eax,[ecx-8]; dec eax; cmp eax,0xf; ja default; movzx edx,byte
[eax+<bytemap>]; jmp dword ptr [edx*4+<jumptable>]`); the ten others are plain `cmp` chains over one or two tags.

Each tag maps onto one of the other five classes, and the mapping is fixed by the targets, not assumed: for the
same slot number, the case bodies call exactly the method that class's own table holds at that slot. Slot 30
(0x004a0b50) is the clearest instance, with all four of its targets already named by N1 as slot-30 entries:

| tag | this-adjust | class | slot 27 | slot 30 | slot 36 | slot 38 |
|---|---|---|---|---|---|---|
| 1 | -0x168 | viewFieldCtor49ebb0 | optionDialogOnMouse 0x0049fda0 | optionDialogUpdate 0x0049f9a0 | optionDialogOnKey 0x0049fa90 | 0x0049fc30 |
| 2 | -0x1a0 | ListBox | ListBox::onDrag 0x0048c8a0 | ListBox::onPress 0x0048aa00 | ListBox::typeAhead 0x0048aea0 | ListBox::fireSelectChanged 0x0048c560 |
| 4 | -0x04 | viewFieldCtor4a2250 | - | - | - | - |
| 8 | -0x8c | panelSubCtor4a3110 | 0x004a4340 | hotHitCb0 0x004a43f0 | panelOnKey 0x004a4680 | hotHitCb5 0x004a4530 |
| 0x10 | -0x180 | viewFieldCtor49d5a0 | listPanelReadValue 0x0049e7d0 | listPanelOnScroll 0x0049e230 | listPanelRefresh 0x0049e450 | listSelectionChanged 0x0049e640 |

The nineteen entries and their names:

| addr | table/slot | name | what the cases do |
|---|---|---|---|
| 0x004a0ef0 | A slot 1 | viewDispatchBuild | tail-calls the five build methods; default returns 7 |
| 0x004a1130 | C slot 6 | viewDispatchSlot6 | tag 2 only: inline copy of ListBox::fwdInnerVfn20 0x0048cbf0 |
| 0x004a1170 | C slot 7 | viewDispatchSlot7 | tag 2 only: inline copy of ListBox::fwdInnerVfn24 0x0048cc30 |
| 0x004a0d30 | C slot 22 | viewDispatchDraw | inline copies of four classes' slot-22 bodies; tag 4 calls panelDrawRow 0x004a2d50 |
| 0x004a13c0 | C slot 25 | viewDispatchScrollBy | tag 2 only: ListBox::scrollBy 0x0048caf0 |
| 0x004a0ab0 | C slot 27 | viewDispatchSlot27 | four tags, two arguments |
| 0x004a0a10 | C slot 28 | viewDispatchSlot28 | four tags, ListBox::clearSelection 0x0048cac0 for tag 2 |
| 0x004a0db0 | C slot 29 | viewDispatchSlot29 | tags 2 and 8: ListBox::forwardClick 0x0048c610, panelOnPick 0x004a4350 |
| 0x004a0b50 | C slot 30 | viewDispatchSlot30 | four tags, two arguments |
| 0x004a0e60 | C slot 31 | viewDispatchSlot31 | tests 4 then 8; acts on tag 8 (hotHitCb1 0x004a4430) |
| 0x004a0eb0 | C slot 32 | viewDispatchSlot32 | tag 8 only: hotHitCb2 0x004a4470 |
| 0x004a0e20 | C slot 33 | viewDispatchSlot33 | tag 8 only: hotHitCb3 0x004a44b0 |
| 0x004a0e90 | C slot 34 | viewDispatchSlot34 | tag 8 only: hotHitCb4 0x004a44f0 |
| 0x004a0bf0 | C slot 36 | viewDispatchSlot36 | four tags, returns the callee's value; default returns 0 |
| 0x004a0c90 | C slot 38 | viewDispatchSelectChanged | four tags; two of the four are already named selection handlers |
| 0x004a0e40 | C slot 39 | viewDispatchSlot39 | tag 8 only: hotHitCb6 0x004a4570 |
| 0x004a0ed0 | C slots 57 and 58 | viewDispatchOnScroll | tag 2 only: ListBox::onScroll 0x0048c420 |
| 0x004a0ff0 | C slot 60 | viewDispatchSlot60 | five tags; tag 4 is widgetShow 0x004a2fa0 |
| 0x004a1090 | C slot 74 | viewDispatchCycleSelection | four tags call Container::cycleSelection 0x0047af60; tag 4 is an inline copy of panelRowCycleSelection 0x004a2ff0 |

Ten of the nineteen are named after their slot number rather than a role. For those slots the already-named
targets do not agree on one role (slot 36 holds three key handlers and `listPanelRefresh`; slot 30 holds
`optionDialogUpdate`, `ListBox::onPress`, `hotHitCb0` and `listPanelOnScroll`), so the role is **not
determined** and the slot index is the firm fact. N1's reading of the same table from other classes gives slot 27
as the pointer-move virtual, slot 30 as the press virtual and slots 29/31 as the release virtuals; that reading
and the targets above have not been reconciled, and no name here depends on it.

`viewDispatchDraw` 0x004a0d30 and `viewDispatchCycleSelection` 0x004a1090 show the bodies are inline copies, not
calls: case 2 of 0x004a0d30 is `ListBox::drawContents` 0x0048c130 instruction for instruction with every offset
shifted by -0x1a0, and case 4 of 0x004a1090 is `panelRowCycleSelection` 0x004a2ff0 shifted by -0x4.

## 4. The 26 target methods

Seven belong to ListBox, five are the deleting destructors of the other five classes, and the rest are slot-22
and slot-74 bodies plus the nineteen dispatchers of section 3 (ten of which arrive in this batch as
vtordisp-target rows rather than switch dispatchers).

- **Scalar deleting destructors** (slot 0 of each table C): `ListBox::deletingDtor` 0x0048cba0,
  `viewDeletingDtor` 0x00491900, `listPanelDeletingDtor` 0x00491ad0, `optionDialogDeletingDtor` 0x0049ff60,
  `panelRowDeletingDtor` 0x004a3050, `panelSubDeletingDtor` 0x004a4980. All six run the class destructor,
  `ListModel::dtor` 0x00489370 on `this+0x578`, `Base4805a0::dtor` 0x004805a0 on `this`, and call
  `operatorDelete_4a4ffc` 0x004a4ffc on the object base when bit 0 of the byte argument is set.
  `listPanelDeletingDtor` is the only one that first writes the three base vtables 0x004bb744 / 0x004bb730 /
  0x004bb724 back into the object.
- **Contents drawing** (slot 22): `ListBox::drawContents` 0x0048c130, `listPanelDrawContents` 0x0049e680,
  `optionDialogDrawContents` 0x0049fc60, `panelSubDrawContents` 0x004a45b0. Each sets `g_curWidget` 0x0083ab2c
  from its child pointer, blits the backing surface through `Surface_blitThunk` 0x00475c60 using the extents the
  child's virtuals +0xd8 / +0xdc return, and then draws its rows (`listPanelDraw` 0x0049dab0,
  `optionListDraw` 0x0049f370, `panelDraw4a3f10` 0x004a3f10).
- **ListBox forwarders**: `ListBox::fwdInnerVfn20` 0x0048cbf0 and `ListBox::fwdInnerVfn24` 0x0048cc30 both guard
  on bit 0x40 of the flag byte at +0x44 and on the child pointer at +0xd8, then return that child's virtual +0x20
  / +0x24. `ListBox::fwdOnScroll` 0x0048c4f0 (slot 57) forwards to the object's own slot 58,
  `ListBox::onScroll` 0x0048c420.
- **The 0x0083b654 pair**: `ListBox::onPressMarked` 0x0048a9c0 (slot 33) and
  `ListBox::fireSelectChangedMarked` 0x0048c520 (slot 39) set the global 0x0083b654 to 1, call the object's own
  slot 30 / slot 38 virtual, and clear it. That global is read in exactly two other places,
  `ListBox::setSelection` 0x00489f50 (0x0048a0c0) and `ListBox::fireSelectChanged` 0x0048c560 (0x0048c5c2), both
  of which pass it as the **second** argument of the owner's virtual +0xfc / +0x104 next to the selected id. What
  the receiver does with it is not determined here; the flag is 1 only while these two wrappers' inner call runs.
  The global is left unnamed.
- `panelRowCycleSelection` 0x004a2ff0 (slot 74 of 0x004bc0a8) tail-calls the virtual +0x128 of the object at
  `[+0xb0]+4` when +0xb4 and +0xac are both non-zero, and otherwise falls back to `Container::cycleSelection`
  0x0047af60.

## 5. The three C++ initializer stubs

The two slots N1 recorded as still missing from the listing, plus the body one of them reaches:

- `dynInit_gSurfaceCtor_839a68` 0x00474a80 — slot 0x004c1114. `call gSurfaceCtor_839a68 0x00474a90` (which builds
  the file-scope Surface at 0x00839a68) then `jmp atexitThunk_474aa0 0x00474aa0`, which registers the destructor
  0x00474ab0 with `atexit` 0x004a56d2. Same two-instruction shape as the 86 `dynInit_*` rows of
  `re/names/undisc_exe.tsv`.
- `storeUiEventTable_thunk` 0x004799c0 — slot 0x004c1118, a single 5-byte `jmp 0x004799d0`.
- `storeUiEventTable` 0x004799d0 — one instruction, `mov dword ptr [0x0083a4f8], 0x004ba87c`, then `ret`. It
  publishes the 13-entry UI event table whose slots N1 named (`redrawRegion` 0x00495770 at slot 1, `uiKeyEvent`
  0x00495910 at 3, `uiKeyEvent148` 0x00495990 at 4, `uiPointerMove` 0x004959b0 at 5, `Timer_fire` 0x00495cb0 at
  12). N1's rows already cite the initializer 0x004799d0 by address, so this names it.

## 6. Everything in the input is a real function start

Unlike N1's batch, none of the 87 entries is a switch case block. The nine entries the input calls switch
dispatchers are precisely the enclosing functions N1 identified as the real missing entries of its section 7:
each starts at the `mov eax, dword ptr [ecx-8]` that opens the dispatcher, each is the sole target of exactly one
vtordisp thunk sitting in one vtable slot, and the 23 case blocks N1 had to leave unnamed are inside them. The
49 thunks are `int3`-padded 5-byte bodies in the middle of each class's thunk run, and the 26 targets all have a
prologue or a `this` load as their first instruction.

## 7. Left open

- The eight thunk targets of section 2 are still `FUN_*`. Seven of them are the slot-28, slot-38 and slot-60
  entries of four of the classes, so the five slot-60 bodies (0x00487af0, 0x0049e7b0, 0x0049fd80, 0x004a4870 and
  `widgetShow` 0x004a2fa0) are one coherent candidate group.
- The role of vtable-C slots 6, 7, 25, 27, 28, 29, 30, 31, 32, 33, 34, 36, 39 and 60 is not determined, and
  neither is the meaning of the 0x0083b654 flag.
- The base-class implementations that fill the unoverridden slots of table C (0x00404880, 0x004048b0, 0x00402180,
  0x00402190, 0x0043cbc0, 0x0047e5e0 and friends) are unnamed stubs; naming them would fix several slot roles at
  once.
