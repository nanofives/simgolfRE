# Phase 1 stream G3: the model corrections reported by stream H (2026-10-18)

Stream G3 applies the corrections listed in re/model/DECISIONS.md (section `corrections reported by stream H`), each
re-read in the disassembly of original\golf_clean.exe before it was applied, then regenerates every derived artefact.
NO-GUESSING applies: where a reading from stream H is not complete in the instructions, the symbol keeps a neutral name
and the open part is a U row in re/model/g3_uncertain.tsv (U-0600..U-0603). Scratch and tools: log/p1/g3/ (sgdis.py
disassembler, refs.py literal finder, access.py memory-operand scan, refresh_slot_names.py, live_probe.py, backup/).

## 1. Vtables 0x004ba2d8 / 0x004ba468: SpriteRef and FontRef

Evidence: Surface_ctor stores 0x004ba2d8 at +0 (0x00473ab7); slot 0 of 0x004ba2d8 (0x00474720, SpriteRef::scalarDtor)
runs the vector destructor iterator with element size 0x2c (`push 0x2c` 0x0047473a, `call 0x4a5713` 0x0047473d).
FontRef::ctor stores 0x004ba468 (0x004837f9); slot 0 of 0x004ba468 (0x00483b30, FontRef::scalarDtor) passes element size
0x20 (`push 0x20` 0x00483b4a). Both agree with e2_classes.tsv (SpriteRef 0x2c, FontRef 0x20).

Changes: e2_vtables.tsv class GfxSurface -> SpriteRef and SurfaceRef -> FontRef (evidence text extended); map symbols
vt_GfxSurface_4ba2d8 -> vt_SpriteRef_4ba2d8 and vt_SurfaceRef_4ba468 -> vt_FontRef_4ba468 (generated from the class);
re/tools/model_merge.py CANONICAL_TYPE mapped `SurfaceRef` to SpriteRef, now FontRef; e2.h's FontRef vptr comment cites
FontRef::ctor (was C4837f0::ctor). No names file carried a vt_GfxSurface / vt_SurfaceRef label, so no names row changed.

## 2. Vtable 0x004bc3b4 (was SgRecList)

Evidence: panelSubCtor4a3110 builds a RecList member at this + 0x78: vbtable 0x004bc3bc at +0x7c (0x004a3180), the
HeapObject at +0xa0 (0x004a3187), the RecList base table 0x004ba2c4 at +0x78 (0x004a31ac), then overwrites it with
0x004bc3b4 (0x004a31e7) and the HeapObject table with 0x004bc3b0 (0x004a31f1). The dword 0x004bc3b4 / 0x004bc3b0 occurs
nowhere else in the image. Slots: 0 recordCopyStr20 0x00474650 (copies this + 0x1c / + 0x20 (string) / + 0x24 into a
record), 1 recordFreeStr_474700 (frees the record's string when this + 0x18 is 0); 0x004bc3b0 slot 0 is
panelSubList_deletingDtor_adj28. The base tables 0x004ba270 / 0x004ba2c4 hold return0Pop4_402180 / retPop4_402190, so
0x004bc3b4 is SgPanelSub's override of its RecList member, not the RecList class's own table.

Changes: e1_vtables.tsv class SgRecList -> SgPanelSub for 0x004bc3b0 (this_origin +0x28 -> +0xa0), 0x004bc3b4 (+0x0 ->
+0x78) and 0x004bc3bc, in E1's own convention (class = owning class, this_origin = sub-object offset, as for SgPanelSub
+0xe0); e1_addresses.tsv symbols vt_RecList_4bc3b0 / 4bc3b4 / 4bc3bc -> vt_PanelSub_4bc3b0 / 4bc3b4 / 4bc3bc; e1.h
SgRecList vtbl comment lists the base and override tables. The struct type SgRecList stays (the 0x30-byte layout is the
same in all its users). Not changed: 0x004bb22c (recordCopyStr1c, bufferFree) keeps SgRecList, because three functions of different classes install
it (ListModel::ctor `mov [esi + 0xc0], 0x4bb22c` 0x004891ca, TextView::ctor `mov ecx, 0x4bb22c` 0x0048ceaa, netSendMessages
`mov ebx, 0x4bb22c` 0x00499adc): its own class name is not determined.

## 3. Table 0x004ba2a8

Evidence: 0x004ba2a8 holds one slot (0x00402790); the next dword starts the table 0x004ba2ac (return0Pop4_402180,
retPop4_402190), which the same four functions store: Cache482f60::ctor 0x00482f9a (0x004ba2ac at 0x00482f91),
C4021e0_dtor 0x004021f3, deletingDtor_adj1c_402790 0x004027a3, Cache482f60::dtor 0x004836bc.

Result: the map already covered both tables through E2 (vt_Cache482f60_HeapObject_4ba2a8, 1 slot;
vt_Cache482f60_base_4ba2ac, 2 slots) and e1_vtables.tsv needs no rows for them; what was missing was the slot names
(FUN_ placeholders). e2_vtables.tsv's row now names deletingDtor_adj1c_402790 and lists the four installers. The pair has
the shape of the RecList base pair 0x004ba2c4 / 0x004ba2c0 (same two slots; HeapObject slot a deleting destructor that
calls List402_clear 0x004026a0), recorded here only: the E2 class labels (Cache482f60.base / .HeapObject) stay.

Slot names everywhere: log/p1/g3/refresh_slot_names.py refreshed the function-name columns of e1_vtables.tsv (1,099
cells, name and thunk_name) and e2_vtables.tsv (340) from the function rows of re/names (fallback: the function index);
1,430 replaced a FUN_ placeholder, 9 followed G1's class renames (View4804a0 -> Window, Dialog -> TextView, Cache483030 ->
PaletteCache); list in log/p1/g3/slot_name_changes.tsv. The function index re/functions_ghidra.tsv still lists 0x00402180
as FUN_00402180 although re/names/model_h1.tsv names it return0Pop4_402180 (5-byte body): the only names-file function the
index does not carry (for the parent's next ApplyNames run).

## 4. 0x0083aa9c and 0x0083ab20

0x0083aa9c: read as a Window* by six instructions: uiEventForwardRootSlot3 calls its slot +0xc (0x00495750..0x0049576a),
uiPointerRegionCode tests bit 2 of +0xa0 (0x00495790, 0x004957b2) and reads it again at 0x004957d7, layoutViewTree
stores it as a new view's parent at +0x130 (0x0047a6f9..0x0047a71c), View_toLocal ends the parent walk at it (0x0047ab82),
uiMousePress1 compares the hit window with it (0x00495abe). No instruction writes it and the dword 0x0083aa9c occurs in
the image only as these six read operands (no push, lea or data pointer), so it holds 0 for the whole run; the live probe
(log/p1/g3/live_probe.tsv: menu, sandbox, a built hole, 49 s of season at x32) read 0 at every sample. Stream H's reading
"pointer to the root window object" is the role its readers give it; that the root window g_rootWindow 0x00519a60 is ever
stored there is not shown, so the name is the role: **g_viewTreeRoot** (SgWindow *). The word after it, 0x0083aaa0, is a
separate variable (10 stores in addDirtyRect / drawCursor, reads in drawTooltip / redrawDrawList): neutral **g_ui83aaa0**
(U-0477). E2's row g_ui83aa9c (int32_t[2]) was split into these two. Defect entry 34 (model_defects.md).

0x0083ab20: uiMousePress1 stores the window hitTestScreen 0x0047f700 returned (0x00495b2c) when its slot +0x140 returns 2
or 10..17 (0x00495b0f..0x00495b1c), together with that code at 0x0083ab1c (0x00495b26) and the press point at 0x0083ab24
/ 0x0083ab28 (0x00495b33, 0x00495b38); uiMouseRelease1 / 2 clear it (0x00495b9a, 0x00495c3a); while it is set
uiPointerMove forwards the pointer to compositeScreen (0x004959b0..0x004959c5), which reads the code (0x0047fbd8) and
reads and rewrites the point (0x0047faea, 0x00480014, 0x00480079; 0x0047faf4, 0x0047fdf1, 0x0047fe1e, 0x0047ff64,
0x00480047). E2's row g_ui83aae4 (18 dwords to 0x0083ab2c) was cut to 14 dwords (0x0083aae4..0x0083ab18) and
**g_captureCode** (int32), **g_captureWindow** (SgWindow *) and **g_capturePoint** (int32[2]) were added.
Note: shim/src/re/c3di.cpp line 45 names its local alias of 0x0083aa98 (g_cursorClipOwner) `g_captureWindow`; that is a
variable name in a hook source, not a model symbol (not changed: shim sources are outside this stream).

## 5. The 16-record table at the pointer 0x0083ff6c

Evidence: netService is a method (`mov ebp, ecx` 0x00497fe1); it clears +0x08 of 16 records of 0x58 bytes at
this + 0x178 (0x0049830f..0x00498329) and stores this + 0x170 into 0x0083ff6c (0x0049832b, 0x00498331), the pointer's only
store. So the table is a member array (this + 0x170 .. + 0x6f0) of the net session object, not a global table; 0x0083ff6c
points at it. Record fields from every accessor found (indexed `[base + i*0x58*8 + k]` and pointer walks, net code
0x00497b40..0x0049bec0): +0x00 id, 0 = free (netQueueMessage 0x0049b6a1..0x0049b6d1, netNameOf 0x0049aa3d,
netMarkRecordCb 0x00497f79, 0x0049b923); +0x08 marked (cleared 0x0049831f, set 0x00497fa1, tested 0x004986bd, 0x00498792);
+0x0c reply_time (read 0x004985c7, rewritten 0x004986da as (old + timeGetTime() - send time + 0x14) >> 1, timeGetTime =
import slot 0x004ba21c); +0x10 stamp (record 0 only: timeGetTime stored by netServiceControl 0x0049ac7c, `mov ebp,
[0x4ba21c]` 0x0049ab54); +0x14 flags byte (0x0049b6db, 0x0049b944; bit 2 tested 0x00497f99); +0x15 name, 0x43 bytes
(0x0049b6e1..0x0049b718, netNameOf returns its address 0x0049aa5b). No access to +0x04..+0x07 was found (U-0603).

Changes: new struct **SgNetPeerRecord** (0x58, static_assert) in e1.h; e1_addresses.tsv g_ui_83ff6c (int32_t) ->
**g_netPeerTable** (SgNetPeerRecord *). The net session class itself is not typed (other net methods use this + 0xe8,
+0xf0, +0xf4, +0x6f8, +0x700, +0x77c, +0x780). Defect entry 36: netQueueMessage's name copy has no bound.

## 6. Documentation corrections

- **g_ui_567afc 0x00567afc**: panelListSelect 0x0040d890 dispatches on it: 3 (0x0040d89f) maps the argument 3..7 to
  g_playerShotStyle (0, -1, 1, 3, 4; jump table 0x0040da8c), 2 (0x0040d903) takes a golfer index from g_panelListIds
  below g_panelListCount and moves the camera to it (0x0040d91e..0x0040d96c), 1 (0x0040d97f) stores a list id into
  0x004c2854, any other value (0x0040d9dd) stores the selection into 0x004c2848. It is stored 32 times in 12 functions
  (16 immediates 0..5, 16 registers; access.py). The role in panelListSelect is complete, the meaning of 0, 4 and 5 is not:
  labelled **g_panelListMode** (new names row), U-0601. Live: 0 at the menu, 5 in the sandbox, 0 after the hole build.
- **0x004c2848 / 0x004c2854**: not renamed. 0x004c2848 receives the argument or g_themeSlots59bfc4[arg]
  (0x0040d9e1..0x0040da02), then indexes the tile-type attribute table (field +0x26 of the 0x30-byte record at
  0x578350, compared with 0xd at 0x0040da27), so "theme slot" is not complete (U-0600). 0x004c2854 receives
  g_panelListIds[arg] or -1 (0x0040d98b..0x0040d9af); g_panelListIds and g_panelListCount have no writer (U-0169), so
  "hole selection" is not shown (U-0602). Both read 0xffffffff in the live probe.
- **g_value56199c 0x0056199c** -> **g_panelListCount**: its one reader bounds the g_panelListIds index in mode 2
  (0x0040d90c..0x0040d913); no instruction writes it (live 0). Defect entry 35.
- **g_titleBarH 0x0083ff10** -> **g_hscrollHeight**: its only store is initHscrollHeight's `push 3; call
  [0x4ba1c8]` (USER32 GetSystemMetrics, SM_CYHSCROLL) at 0x00496020..0x00496028; 20 reads (layoutViewTree, Window::*Size*
  / grow / shrinkRect, Window::create, ListBox::measure / addItem, scrollbarCreateA/B_impl). Live value 0x11.
- **Snd slots 41-45**: 0x00485120 `mov eax, 0xb; ret` (slot 41) and 0x00485130 `mov eax, 0xb; ret 0xc` (slots 42..45):
  recorded in e2.h's Snd vptr comment; slot names refreshed (returnB_485120, returnBPop12_485130).
- **Snd +0x40 / VoiceRx +0x40 / NodeList +0x14**: already typed (Snd.sound, StrList.device); comments extended: the
  Snd::fwdVoiceNN slots forward through +0x40, VoiceRxNode.snd_base holds it at +0x40, the NodeList::fwdChildNN /
  NodeListB::callChildNN slots call the vtable of +0x14 (0x004876a0..0x004876a9). Stream H's `child pointer` is this
  device object.

## 7. Files changed

re/model: e1_vtables.tsv, e2_vtables.tsv, e1_addresses.tsv, e2_addresses.tsv, b_addresses.tsv, b_types.tsv,
include/sections/e1.h, include/sections/e2.h, addresses.tsv, include/simgolf_model.h, model_ctypes.py (regenerated),
g3_uncertain.tsv (new), DECISIONS.md (one line). re/names: model_e2_globals.tsv, model_b_globals.tsv, exe_3_globals.tsv,
model_g3_globals.tsv (new); re/analysis/systems/model_g3.md (new stub). re/tools/model_merge.py (CANONICAL_TYPE, MAP_CHECK).
re/analysis/data: model_corrections.md (this file), model_defects.md (entries 34-36), model_e1.md, model_e2.md,
model_name_changes.md (pointer). sve_fields.tsv regenerated unchanged.

## 8. Names (old -> new)

| address | old | new | file |
|---|---|---|---|
| 0x004ba2d8 | vt_GfxSurface_4ba2d8 (class GfxSurface) | vt_SpriteRef_4ba2d8 (SpriteRef) | e2_vtables.tsv (map symbol) |
| 0x004ba468 | vt_SurfaceRef_4ba468 (class SurfaceRef) | vt_FontRef_4ba468 (FontRef) | e2_vtables.tsv (map symbol) |
| 0x004bc3b0 / b4 / bc | vt_RecList_4bc3b0 / 4bc3b4 / 4bc3bc (SgRecList) | vt_PanelSub_4bc3b0 / 4bc3b4 / 4bc3bc (SgPanelSub) | e1_vtables.tsv, e1_addresses.tsv |
| 0x0083aa9c | g_ui83aa9c int32_t[2] | g_viewTreeRoot SgWindow * | e2_addresses.tsv, model_e2_globals.tsv |
| 0x0083aaa0 | (in g_ui83aa9c) | g_ui83aaa0 int32_t | e2_addresses.tsv, model_g3_globals.tsv |
| 0x0083aae4 | g_ui83aae4 int32_t[18] | g_ui83aae4 int32_t[14] | e2_addresses.tsv, model_e2_globals.tsv |
| 0x0083ab1c / 20 / 24 | (in g_ui83aae4) | g_captureCode, g_captureWindow, g_capturePoint | e2_addresses.tsv, model_g3_globals.tsv |
| 0x0083ff6c | g_ui_83ff6c int32_t | g_netPeerTable SgNetPeerRecord * | e1_addresses.tsv, model_g3_globals.tsv |
| 0x00567afc | g_ui_567afc (map symbol) | g_panelListMode (label; stream symbol kept) | model_g3_globals.tsv |
| 0x0056199c | g_value56199c | g_panelListCount | b_addresses.tsv, b_types.tsv, model_b_globals.tsv |
| 0x0083ff10 | g_titleBarH | g_hscrollHeight | e1_addresses.tsv, exe_3_globals.tsv |

No function was renamed, so hooks.csv needs no `re_classify.py retag`. ApplyNames was not run and the Ghidra master was not
touched: the data labels above are for the parent's next ApplyNames run (renames at 0x0083aa9c, 0x0056199c, 0x0083ff10;
new labels at 0x0083aaa0, 0x0083ab1c, 0x0083ab20, 0x0083ab24, 0x0083ff6c, 0x00567afc).

## 9. Check results

- names_sanitize (touched files): 0 replaced; names_validate model_g3 model_e2 model_b exe_3: bad 0, cross-file duplicates
  0; names_check_evidence: 439 citations, 0 bad.
- model_merge merge: 1,123 rows (was 1,119), 0 overlaps, 0 duplicate symbols, 0 struct size disagreements; coverage 1,767 /
  1,767 clusters and 5,949 / 5,949 addresses; the map diff is exactly the rows of section 8.
- header: simgolf_model.h 2,479 lines (SgNetPeerRecord added, comments).
- model_check over 1,106 ranges and all functions: 13 errors, 770 warnings, the same violation set as G1's run (diff of
  log/p1/g1/viol_all.tsv and log/p1/g3/viol_all.tsv empty); none on a G3 row.
- hook cross-check: 15,797 literal uses, ok 7,625, narrow 242, untyped 7,876, wide 51, stride 3 (G1's figures); every use
  of a G3 row is ok or address-only.
- model_ctypes_gen: 105 structs, 274 static asserts, 1,120 symbols typed; model_sve_fields: 91 rows, all named, unchanged.
- `pytest -m static`: 507 passed, 2 skipped.
