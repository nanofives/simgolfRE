# Data model stream E1: object layouts of the UI toolkit (phase 1, 2026-10-18)

Stream E1 of `re/PHASE1_PLAN.md` (brief `re/model/STREAM_BRIEF.md`): the C++ classes of golf_clean.exe's window / widget
family, their vtables and vbtables, the statically constructed instances and the class statics in the 410 clusters
`re/model/ownership_e.tsv` gives E1. NO-GUESSING applies: a field gets a meaning only where the cited code shows it,
otherwise a neutral `f_<offset>` name with the type its accesses give.

## 1. Files

| file | content |
|---|---|
| `re/model/include/sections/e1.h` | 26 structs (C99, `uint32_t` pointers, size asserts), compiles as C and C++ with MSVC x86 and as C with MSVC x64 (`log/p1/e1/cc/build.bat`, /W4) |
| `re/model/e1_fields.tsv` | 557 rows: every field and sub-object of every struct with width, C type, the access widths, r/w, signedness and EVERY function and instruction that touches it |
| `re/model/e1_vtables.tsv` | 1,522 rows: every slot of the 51 vtables and every entry of the 12 vbtables of these classes (owner stream, class, `this` origin the slot receives, target, name, vtordisp / adjustor target) |
| `re/model/e1_addresses.tsv` | 209 typed ranges: 29 constructed objects, 59 vtables / vbtables, 121 statics (30 objects stream E2 types are left to `re/model/e2_addresses.tsv`, list `log/p1/e1/dropped_to_e2.txt`) |
| `re/names/model_e1_globals.tsv` | 62 new global names (existing names are reused, not repeated) |
| `re/model/e1_uncertain.tsv` | U-0400..U-0418 (U-0410 resolved during the stream, see 4.4) |
| `log/p1/e1/*.py` | the tools: `this_scan.py`, `classes.py`, `aggregate.py`, `global_this.py`, `gctors.py`, `globals.py`, `sites.py`, `gen_header.py`, `gen_vtables.py`, `gen_addresses.py`, `flat.py`, `assemble.py`, `hookcheck.py`, `coverage.py`, `mkcheck.sh` |

Regenerate: `this_scan.py`, `global_this.py`, `aggregate.py`, `gctors.py`, `globals.py`, `sites.py`, then `gen_header.py`,
`assemble.py`, `gen_vtables.py`, `sh log/p1/e1/mkcheck.sh` (addresses, names, model_check), `hookcheck.py`, `coverage.py`.

## 2. Method

**`this` scan (`this_scan.py`).** Every function of `re/functions_ghidra.tsv` (all ranges), capstone, straight-line
register tracking: ecx = this at entry; `mov`, `lea r, [this + k]`, `add / sub r, imm` keep a register as this + off;
`mov r, [this + k]` makes r the field k; a call clobbers eax / ecx / edx; at `jmp` / `ret` only esi / edi / ebx / ebp
written once in the function survive. Recorded: field accesses (width, r / w, movsx / movzx, x87), direct calls with ecx
= this + k (sub-objects and bases), calls with ecx = a loaded field (pointer members), virtual calls through the own or
a member's vtable, and `mov [this + k], imm` vtable stores. **Virtual bases**: `mov r, [this + V]` (the vbptr), `mov r2,
[r + 4k]` (vbtable entry k), then `[r2 + this + d]` or `add r2, this` give an access to virtual base k at offset
`t + d - V` (t = the this offset of the register): ListBox::onPress 0x0048aa00 reads [vbt[1] + esi + 0xd8] at
0x0048aa0c, which is Window +0x130.

**Classes (`classes.py`).** The class table was read from the constructors (section 3). Seeds: each vtable maps to
(class, the offset of the sub-object its slots receive as this); vtordisp thunks (`sub ecx, [ecx-4]; jmp T`) and
adjustor thunks pass the seed to T; constructors / destructors are the functions that store a seeded vtable at this+0
(or, for the virtual-base constructors, the six read by hand). Propagation (`aggregate.py`): a call with ecx = this + k
gives the callee (class, origin + k); a function seen with several classes resolves to the most-derived class common to
all its views (a shared Window method resolves to Window). Functions named `Window::`, `View_`, `ListModel::`,
`ListBox::`, `Button::`, `HotList::`, `TextView::`, `EditBox::` in re/names seed only functions nothing else reached.
Four overrides with their reason (`OVERRIDE` in aggregate.py): ListBox::dtor 0x00489de0 and Dialog::viewDtor
0x00491500 receive the Window part (they store vtables at this-0x5c / this-0x1fc), ListModel::destroyNodes 0x00489b30
and List401c70_dtor 0x00401c70 receive interior pointers of the record list, the entry destructor 0x00492660 a hot-list
entry. Result: **545 functions** with a class (Window 54, View 49, RootWindow 49, CompositeView 47, ListBox 46, PanelSub
35, TextView 33, EditBox 31, OptionPart 29, Button 27, ListPanelPart 23, ...), 4 left in conflict (the shared stubs
FUN_00402180 / FUN_00402190 `ret`, `_free`, `operator new`). Each access is then located in the innermost sub-object
that holds its offset (`locate`), so a Button method's access at +0x130 lands in View, at +0x300 in Image.

**Globals.** `global_this.py`: calls / tail jumps with ecx = an immediate data address. `gctors.py`: static construction
(eh-vector constructor ??_L 0x004a55d2 calls with their pushed address / size / count / constructor, and direct
constructor calls), giving 59 objects that hold E1 clusters (`globals.py`). The rest of the E1 clusters (window-tree
state, class default blocks, panel words) were typed by hand in `log/p1/e1/gspec.tsv` from the inventory widths and
every instruction site (`sites.py`, 8,437 references).

## 3. Class table (sizes and how each is known)

| struct | size | how the size is known | layout |
|---|---|---|---|
| SgWidgetBase | 0x9c | View_ctor's first own store is +0x9c; initWidgetTable 0x00495d30 writes +0x0..+0x98 | vtbl + 38 callback words copied from g_widgetTable |
| SgHotList | 0x60 | View +0xbc -> next field +0x11c; Image +0xb4 -> next store +0x114; TextView +0x1f04 -> +0x1f64 | vtbl 0x004bba74, Timer at +0x20, entries / capacity / count / tip delay |
| SgHotListEntry | 0x20 | eh-vector stride 0x20 (HotList::grow 0x004926eb, HotList::alloc 0x00492977) | vtbl 0x004bba70, +0x1c tip text |
| SgViewList | 0x18 | View +0x134 -> next store +0x14c | child list of a View (nodes +4 child, +0xc next, +0x10 prev) |
| SgRecList | 0x30 | vbtable 0x004bb234 {-4, 0x24}: virtual base HeapObject at +0x28 (8 bytes); ListModel +0xc0..+0xf0 | record list with copy / free slots |
| SgView | 0x274 | Image base at +0x274 in View4804a0::ctor 0x004804c4 | WidgetBase, flags, HotList +0xbc, parent +0x130, ViewList +0x134, rects, handlers, scrollbars |
| SgWindow | 0x574 | ListBox vbtable 0x004bb3d0: vbases 0x58 / 0x5d0 from the vbptr at +4 -> Window part 0x5c..0x5d0 minus the vtordisp = 0x574; EditBox / Button / DebugWindow first own fields +0x574 | View at 0, Image at +0x274 (a second base: Window's constructor replaces its vtable, 0x004804e1), own +0x52c..+0x570 |
| SgRootWindow | 0x574 | staticInit_404360 0x00404360 adds only the vtables 0x004ba2fc / 0x004ba2e8; g_rootWindow 0x00519a60..0x00519fd4, the next object at 0x00519fd8 | Window |
| SgEditBox | 0x5dc | operator new 0x5dc at 0x004a2660 | Window, text / capacity, caret Timer at +0x5ac |
| SgButton | 0x630 | LabelButton's first own store +0x630 (ctorObj4a13f0 0x004a140b); last Button store +0x62c (0x004885b0) | Window, Timers at +0x57c / +0x5b4 |
| SgLabelButton | 0x6d0 | ScrollCombo holds two at +0x60c / +0xcdc (stride 0x6d0) | Button + per-state int[3] tables |
| SgScrollCombo | 0x13b4 | operator new 0x13b4 at 0x0047a978 / 0x0047aa24 | Window, state triples, two LabelButtons |
| SgDebugWindow | 0x5c8 | Timer at +0x598 (ctorGlobalObj49c020 0x0049c055); its instance 0x008400c8 ends at 0x00840690 | Window, FontRef +0x574, +0x594, Timer |
| SgListModel | **0xfc** | listPanelSlot28 writes ListModel +0xf8 (0x0049e9a6); TextView holds a CompositeView at +0x1460 whose ListModel ends at +0x1cd0, TextView's next member (rep movsd target 0x0048cfa2) | vtbl 0x004bb21c, Buf +4, RecList +0xc0, +0xf0..+0xf8 |
| SgListBox | 0x6d0 | vbtable 0x004bb3d0 {-4, 0x58, 0x5d0} + ListModel 0xfc | ListBoxNv (vtbl, vbptr, 0x58), vtordisp, Window +0x5c, vtordisp, ListModel +0x5d4 |
| SgListPanelPart / SgOptionPart / SgPanelSub / SgPanelRow | 0x698 / 0x690 / 0x754 / 0x700 | vbtables 0x004bbe7c / 0x004bc01c / 0x004bc3c4 / 0x004bc214 + Window 0x574 + ListModel 0xfc | non-virtual part 0x20 / 0x18 / 0xdc / 0x88 then the two virtual bases |
| SgCompositeView | 0x870 | viewCtor4a0740 0x004a0740: five non-virtual parts at 0, 0x58, 0x78, 0x90, 0x16c, vtordisp 0x1f8, Window 0x1fc, vtordisp 0x770, ListModel 0x774 | the five parts over ONE Window and ONE ListModel |
| SgTextView | 0x2114 | operator new 0x2114 at 0x0049132b | Window, Buf, two LabelButtons, SpriteRef, two RecLists, CompositeView, HotList, tables |
| SgCursor | 0x14 | Cursor::ctor 0x00488490 writes +0x0..+0x10 | vtbl 0x004ba7f4 |

Classes of stream E2 embedded as opaque byte arrays (names and sizes agreed with E2, re/model/include/sections/e2.h):
Image 0x2b8 (operator new 0x2b8 at 0x0048077b), SpriteRef 0x2c (Surface_ctor 0x00473ab0, eh-vector stride 0x2c), Timer
0x30 (operator new 0x30 at 0x0048326d), Buf 0x18, HeapObject 8, FontRef 0x20 (C4837f0::ctor 0x004837f0). E2 removed its
HotList structs: HotList / HotListEntry are E1's, and E2's Image embeds them as two 0x60-byte arrays at +0xb4 / +0x254.

## 4. Notes per class

### 4.1 View / Window (the 0x274 sub-object, the root window)
- The class the names call `View` (View_ctor 0x00479c40, vtable 0x004ba8b4, 87 slots) is the 0x274-byte base. The class
  the names call `View4804a0` / `Base4805a0` (vtable 0x004baa30, 91 slots) is **Window**: View + Image. The 0x274
  sub-object is a full **Image** base (5-slot vtable 0x004baa1c with the adjustor thunk View4804a0::scalarDtor_adj274
  in slot 3), not a bare Surface: CLAUDE.md's `0x274 is the Surface sub-object of every Window class` describes it by
  its use. Every Window-derived class rewrites both vtable words (+0 and +0x274) in its constructor.
- Fields with a meaning: +0x9c flags and +0xa0 state (bit tests in View_attach / Window::show; also read as bytes),
  +0xb0 self (0x00479ca2), +0x130 parent View* (View_unlinkFromParent 0x0047ada1 walks the parent's child list),
  +0x134 children (ViewList), +0x1ac / +0x1bc two x, y, w, h rects, +0x238..+0x258 nine handler words each read by the
  window method named after its offset, +0x26c / +0x270 two ScrollCombo pointers (deref +0x5a0, this of
  setWidgetScalar / orderPair). Window: +0x52c.. frame Image pointers (drawPanel4810d0), +0x568 an owned object released
  through slot +0xc (0x004806ab), +0x56c the list cookie from 0x0083ac18.
- RootWindow (g_rootWindow 0x00519a60) is a Window with its own vtables; slots 29..39 are the rootWin_* input handlers.

### 4.2 Widgets
- **Button** (92-slot vtable 0x004bb0a8; slot 91 Button::resetFields) adds two Timers and 0x40 bytes of state; +0x5f8 is
  the tooltip text (Button::setTooltip 0x00488a25..0x00488a85 frees and replaces it). **LabelButton** (ctorObj4a13f0)
  adds per-state int[3] tables: +0x638 = -1, +0x644 from 0x00840948, +0x650 = 0 and nine rows from 0x00840950 + 0xc * k.
  The default rows overlap: row 0 is read at 0x00840948 + 4 * i and row 1 at 0x00840950 + 4 * i, so row 0's third word
  is row 1's first (0x004a143f / 0x004a1451); ScrollCombo's defaults do the same (0x0083ff24 / 0x0083ff2c).
- **EditBox**: +0x574 text (char*), +0x578 capacity (EditBox::resizeBuffer compares and replaces both at
  0x00486258..0x004862a4), caret Timer at +0x5ac.
- **ScrollCombo** (vtable 0x004bbac8; buildLabeledControl, comboDtor): one class answers both names the older notes
  used. Its slots 22, 30, 31, 35, 53 are the scrollbar* functions, comboDtor tears it down, and it embeds two
  LabelButtons; the struct keeps the neutral name.
- **HotList** (+0x50 entries of 0x20 bytes, +0x54 capacity, +0x58 count, +0x5c 1000 ms tip delay) sits inside every
  View and twice inside every Image.

### 4.3 ListModel, ListBox and the record list
- ListModel (0xfc): vtbl 0x004bb21c, Buf at +4, defaults from g_listModelDefaults 0x0083b61c (ListModel::ctor
  0x004891ff..0x0048930a), a RecList at +0xc0 whose virtual base HeapObject (+0xe8) takes the heap word 0x00839650
  (0x0048917c). **Correction:** the A/B fixture helper `re/frida/js/uitree.js` allocates ListModel 0xf8, ListBox 0x6cc,
  the four parts 0x694 / 0x68c / 0x750 / 0x6fc and the Composite 0x86c, each 4 bytes short: listPanelSlot28's write to
  ListModel +0xf8 (0x0049e9a6) lands past the arena block in those fixtures. The game itself never allocates these
  classes alone; its only complete instances sit in TextView, which has the room.
- ListBox and the four composite parts use virtual inheritance: a non-virtual part (vbptr first), then vtordisp,
  Window, vtordisp, ListModel. Their methods are reached through vtordisp thunks and receive this = the Window part of
  the STANDALONE layout (ListBox +0x5c): non-virtual fields are at negative offsets (the vbptr at this-0x58, as the
  brief says), virtual-base fields go through the vbtable.

### 4.4 CompositeView and TextView
- CompositeView (viewCtor4a0740) holds the five non-virtual parts (ListBox, list panel, option part, panel sub,
  panel row) over one Window (+0x1fc) and one ListModel (+0x774). +0x1f4 is the part mask the view dispatchers test at
  [ecx - 8]: each this-adjustment lands on a part's non-virtual start plus its standalone Window offset, so bit 1
  (-0x168) = option part (0x78 + 0x1c), 2 (-0x1a0) = ListBox (0 + 0x5c), 4 (-0x4) = panel row (0x16c + 0x8c), 8 (-0x8c)
  = panel sub (0x90 + 0xe0), 0x10 (-0x180) = list panel (0x58 + 0x24).
- TextView (0x2114) is the class the names call both TextView (TextView::ctor 0x0048ce00 and 20 more TextView methods in re/names) and Dialog
  (Dialog::dtor 0x00491710 stores TextView's vtables 0x004bb3f8 / 0x004bb3e4 and tears down its members in order).
  Dialog::viewDtor 0x00491500 is CompositeView's destructor. Stream E2 records vtable 0x004bb3f8 as `Dialog`: the
  parent should pick one name (21 functions in re/names carry the TextView prefix). MsgBox is not a class: MsgBox::setLabelA / setButtonA /
  initDefaults write class statics (g_msgBox*, g_textViewDefaults) that TextView::ctor and TextView::build copy.

## 5. Global objects and statics

- **29 constructed objects** typed here: g_rootWindow, g_debugWindow 0x008400c8 (DebugWindow; Window::onKeyChar
  references it at 0x0047ef52..0x0047ef6e when its key code matches), g_characterEditorEditBox 0x005a9f78 (every use
  is in showCharacterEditor), 17 SpriteRef runs (eh-vector constructed, counts from the constructor calls, e.g.
  g_spriteRef_8224c8 x 39 from gSurfaceCtor_44b5a0), 8 FontRefs, the Image 0x00838f98. Another 30 objects that hold E1
  clusters (29 SpriteRef runs and g_font51a048) are typed by stream E2 under the same symbols (agreed 2026-10-18).
- **Window-tree state** (0x0083a2c8..0x0083ab94): g_tipRect, g_zlist (Window*[0x80], the bound compare at 0x0047a74c),
  g_dirtyRect, the **modal stack** g_modalStack 0x0083a4e8 / g_modalSaved 0x0083aa88 (Window::runModal stores
  [depth * 4 + base] at 0x0047bad2 / 0x0047bad9, the pop 0x0047bbb0 reads them back) with g_modalDepth 0x0083ab58,
  g_modalTop 0x0083ab54 and g_modalPrev 0x0083ab50, g_clipRect 0x0083aa78 (Surface_clip), the tooltip words
  (g_tipCallback 0x0083aabc is called at 0x0047cdb9), focus / capture / drag globals and g_viewDefaults 0x0083ab64
  (copied into every View at 0x00479d31..0x00479dc1).
- **Class default blocks** read by the constructors: g_widgetTable (38 callback words), g_buttonDefaults,
  g_defaultButtonFont +4 / +8, g_listModelDefaults, g_listBoxDefaults, g_editBoxDefaults, g_textViewTableA / B (0x46
  dwords each, rep movsd), g_textViewDefaults / Row / Rows, g_msgBox* blocks, g_scrollComboDefaults / States,
  g_labelButtonDefaults, g_panelRowDefault, g_panelSubDefault.
- **UI panel words** of the in-game panels (employee / golfer / amenity / building panels, label queue, money labels,
  scroll text, HUD text tables): typed by width with the reading functions; 28 keep a neutral `g_ui_<addr>` name
  (U-0418).

## 6. Corrections to stream A and older notes

| item | was | is (evidence) |
|---|---|---|
| ListModel / ListBox / parts / Composite sizes (uitree.js KINDS) | 0xf8 / 0x6cc / ... / 0x86c | 0xfc / 0x6d0 / +4 each / 0x870 (section 4.3) |
| g_widgetTable 0x0083fe78 (exe_5_globals) | 0x60-entry table | 38 callback words copied into WidgetBase +4..+0x98 (initWidgetTable 0x00495d38..0x00495e9d) |
| g_scrollDragX / g_scrollDragY 0x0083ab30 / 0x0083ab34 | int drag anchor | Window* drag captures: loaded and called through the vtable (inventory ptruse vtblval); uitree.js layoutNode notes 0x00479f78 / 0x00479f8d |
| g_loadImage 0x00839348 (exe_12_globals) | Image of 0x2c0 bytes | Image is 0x2b8 (operator new 0x2b8 at 0x0048077b) |
| Window's +0x274 | Surface sub-object | Image base class with its own vtable |
| names View4804a0 / Base4805a0 | separate classes | Window's constructor / destructor |
| names Dialog::dtor / Dialog::viewDtor | Dialog class | TextView's / CompositeView's destructors |
| stream A objects 0x2c apart | separate `this` objects | elements of SpriteRef runs built by eh-vector constructors (count from the call, not the span) |
| ownership_e.tsv | 31 objects split between E1 and E2 (base cluster E2, elements E1); RootWindow and TextView vtables E2 | agreed with E2: E2 types 30 of them (the SpriteRef runs and g_font51a048, log/p1/e1/dropped_to_e2.txt), E1 the EditBox 0x005a9f78 and the run 0x008224c8 x 39; the vtable rows stay E2's, slot maps are in e1_vtables.tsv too |
| cluster 507 (B) 0x00519d30..0x00519d40 | stream B scalar | bytes of g_rootWindow's Image base (+0x2d0..+0x2e0) |
| clusters 122, 654, 839, 1123 (E1) and 0x0056e948 / 0x0056e950 | E1 | elements of other streams' objects (U-0412; 1123 is covered by E2's row 0x0080b130) |

**G3 corrections (2026-10-18, re/analysis/data/model_corrections.md):** the RecList override tables 0x004bc3b0 / 0x004bc3b4
/ 0x004bc3bc are stored only by panelSubCtor4a3110, so e1_vtables.tsv now lists them as class `SgPanelSub` (this_origin
+0xa0 / +0x78; map symbols vt_PanelSub_4bc3b0 / 4bc3b4 / 4bc3bc); 0x004bb22c keeps `SgRecList` because ListModel::ctor,
TextView::ctor and netSendMessages all store it. g_ui_83ff6c is g_netPeerTable (pointer to 16 SgNetPeerRecord of 0x58
bytes, new struct), g_titleBarH is g_hscrollHeight (GetSystemMetrics(3) at 0x00496028), g_ui_567afc is labelled
g_panelListMode. The slot function names of e1_vtables.tsv were refreshed from re/names (1,099 rows).

## 7. Checks

**model_check** (`log/p1/e1/check_types.tsv`, 200 ranges; flattened structs for the three window objects, SpriteRef
dword rows, element types for arrays, vtables const): **0 errors, 45 warnings** (`log/p1/e1/violations.tsv`). Before the
SpriteRef runs went to E2 the only error was a checker false positive on g_sprites53df58: loadWorldAssets' loop
(0x0043eb3e..0x0043ebb6) walks four Surfaces per iteration (esi += 0xb0, accesses esi-0x58 .. esi+0x2c) and stops at
`cmp esi, 0x53e690`, its last iteration touching element 39 of 40, while the checker assumes a one-element step (E2
should expect the same report). Warnings: 19 stride and 15 base_outside on the SpriteRef runs (indexed accesses at
strides of the arrays around them, biased displacements into neighbouring grids), 4 width_narrow (byte reads of dwords
the code also reads whole: 0x005a34f0 at 0x0040e345, 0x00839264 at 0x0046df28), 4 width_block (dword reads inside the
opaque Image bytes of g_rootWindow, e.g. 0x00519cd8 = Image +4 at 0x00449826), 3 sign (g_ui_4d20dc).

**Hook cross-check** (`log/p1/e1/hookcheck.py`, shim/src/re/*.cpp without negative controls, comments stripped):
2,176 literal addresses in E1 ranges; 884 with a typed access agree with the model's width, 1,261 are address-only
uses, 29 fall in opaque E2 bytes, **2 disagree**, both explained: c3dx.cpp:1589 and c3el.cpp:267 read the low byte of a
dword exactly as the original does (0x0040e345 `mov cl, byte ptr [0x5a34f0]`, 0x0046df28 `mov cl, byte ptr [0x839264]`).
The cross-check covers literal addresses only; `this`-relative field offsets in the hooks are not compared.

**Header**: compiles with MSVC 19 x86 (C and C++) and x64 (C), /W4, every `SGE1_STATIC_ASSERT` holds.

## 8. Coverage

| measure | done / total |
|---|---|
| E1 clusters typed (e1 + E2's agreed rows) | 406 / 410 (e1_addresses.tsv alone 254; the 4 others are parts of B / C objects, U-0412) |
| E1 addresses typed (incl. end markers) | 704 / 710 (e1 alone 528) |
| weighted by referencing functions | 1,340 / 1,347 = 99.5% (e1 alone 79.8%) |
| window-family classes with a constructor given a struct | 22 classes + 4 non-virtual parts |
| fields typed in the structs | 557 rows in e1_fields.tsv; 354 keep a neutral name (U-0409); 10 unk ranges (U-0400..U-0404) |
| functions given a `this` class | 545 (U-0413: stack-passed windows are not attributed) |

Untyped E1 clusters for the parent: 122 (0x004c15a0, in B's .sve field challenge_text), 654 (0x0055b028, in B's record
array 0x00543d10), 778 part (0x0056e948 / 0x0056e950, the record table after C's animals), 839 (0x0058316e, in C's grid
0x005830b8).
