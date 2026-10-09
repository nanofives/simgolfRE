# undisc_exe: golf_clean.exe code Ghidra never made into functions (2026-10-09)

`re/tools/undiscovered.py` found 236 entry points whose calls sit outside every function of the Ghidra listing;
`ghidra/scripts/CreateFunctions.java` created them and `log/undisc_created.tsv` records each entry, its first
instructions, the dwords that point at it and the tracked functions it calls. 206 of the 236 are in
golf_clean.exe. 180 are named here; the remaining 26 are **not function starts** (last section).

Everything below is read from the image with `re/tools/xref.py`, `re/tools/pointer_callers.py` and capstone, plus
headless decompilations of the newly created functions (the pool was refreshed from the master first, otherwise
every address decompiles as `NO FUNCTION`).

## 1. The C++ dynamic-initializer table (86 rows, `dynInit_*`)

`__cinit` 0x004a50db loads 0x004c1000 at 0x004a50fb and hands the table to `_initterm`. The table runs
0x004c1004 to 0x004c1184 (97 slots) between the NULL terminators 0x004c1000 and 0x004c1188; 86 of its slots were
not functions in the listing. Every one of them is two instructions:

```
00403f10  call 0x403f20          ; the file-scope object's constructor
00403f15  jmp  0x403f40          ; push <destructor>; call atexit 0x004a56d2; pop ecx; ret
```

0x004a56d2 is `atexit`: it forwards to `__onexit` 0x004a5654 and converts the result to 0 / -1. So each row's
purpose names the constructor it calls and the destructor the second target registers. The slot order is the link
order, so the names stay keyed to the address, not to any ordering assumption.

Two initializer-table slots are **still missing from the listing** and are not in the 236: slot 0x004c1114 and
slot 0x004c1118 -> 0x004799c0, a `jmp 0x4799d0` whose body is the single store
`mov dword ptr [0x0083a4f8], 0x004ba87c` (section 5). `undiscovered.py` does not see them because their call
targets are not functions of the listing either.

## 2. atexit destructor stubs (13 rows, `atexitDtor_<global>`)

The companion of section 1: the address pushed by the `push imm32` inside each registration stub. Twelve are
`mov ecx, <global>; jmp <destructor>` (0x0044adf0 and friends, globals 0x00821070..0x008224c8, destructors
0x0044ae00 / 0x0044afb0 / 0x0044b1b0 / 0x0044b3e0 / 0x0044b4f0 / 0x0044b690 / 0x0044b790). The thirteenth,
`atexitDtor_83aca8` 0x00482f20, is a real body: it guards on bit 0 of 0x0083ac28, calls `Cache482f60::dtor`
0x004835f0 on 0x0083aca8, writes the base vtable 0x004ba278 back and copies 0x0083acac into 0x00839650.

## 3. this-adjustor thunks (36 rows)

Four shapes, all ending in a tail `jmp` to the real method, all reached only from a vtable slot:

| shape | suffix | count |
|---|---|---|
| `sub ecx, 0x274; jmp X` | `_adj274` | 8 |
| `sub ecx, 0x5c; jmp X` | `_adj5c` | 1 |
| `sub ecx, dword ptr [ecx-4]; jmp X` | `_vtordisp` | 26 |
| `sub ecx, dword ptr [ecx-4]; sub ecx, 0x578; jmp X` | `_vtordisp_adj578` | 1 |

The fixed 0x274 adjustment is the offset of the Surface sub-object every Window-family class embeds: each class has
a small secondary vtable immediately before its main one (0x004bb094 before 0x004bb0a8 for Button, 0x004ba494
before 0x004ba4a8 for EditBox, and so on), and slot 3 of the secondary table holds the `_adj274` thunk of that
class's deleting destructor. The same 0x274 appears as the `this` operand in
`Window::createBackingSurface` 0x00480b30, which builds the Surface at `this+0x274`.

## 4. The Window class hierarchy

`pointer_callers.py` resolves each vtable to the constructor that stores it, which gives the class names used in
the evidence column:

| vtable | installed by | class |
|---|---|---|
| 0x004ba2fc | staticInit_404360 0x00404360 | Flic |
| 0x004ba4a8 | EditBox::ctor 0x00486070 | EditBox |
| 0x004ba8b4 | View_ctor 0x00479c40 | View |
| 0x004baa30 | View4804a0::ctor 0x004804a0 | View4804a0 |
| 0x004bb0a8 | Button::ctor 0x00488500 | Button |
| 0x004bb260 | ListBox::ctor 0x00489cb0 | ListBox |
| 0x004bb3f8 | TextView::ctor 0x0048ce00 | TextView |
| 0x004bb5b4 | LabelButton::dtor 0x00491410 | LabelButton |
| 0x004bbac8 | buildLabeledControl 0x00496030 | the combo control |
| 0x004bbeb0, 0x004bc0a8, 0x004bc244 | viewFieldCtor49ebb0 / 4a2250 / panelSubCtor4a3110 | the panel field views |

Eight of the created functions sit at the same slot of fourteen or fifteen of these tables, so they are base-class
(Window) implementations: slot 3 `Window::createBackingSurface`, slot 26 `Window::fwdMouseEvent238`, slot 67
`Window::runModal`, slot 81 `Window::onCommand`, slot 82 `Window::onKeyChar`, slot 83 `Window::onPressHot`,
slot 84 `Window::onReleaseInner150`, slot 86 `Window::onReleaseInner158`. The remaining slots repeat across
classes and fix the meaning of a few numbers: slot 22 is the contents-drawing virtual (`TextView::drawContents`
0x00490d60, `LabelButton::drawContents` 0x004a1580), slot 27 is the pointer-move virtual
(`EditBox::updateCaretDrag`, `LabelButton::onMouseMove`), slot 30 is the press virtual
(`EditBox::beginCaretDrag`, `Button::beginPress`), and slots 29 / 31 are the matching release virtuals.

The names of the press/release handlers come from what the code does, not from the message that reaches them: they
claim and release the drag owner `g_scrollDragX` 0x0083ab30 / `g_scrollDragY` 0x0083ab34 and raise and clear the
pressed flag at `this+0x614`. Note that `g_scrollDragX` / `g_scrollDragY` are typed `int drag anchor` in
`re/names/exe_1_globals.tsv`; every function in this batch uses them as `Window*`.

`Button::timerCallback` 0x00488fb0 and `HotList::tipTimerCallback` 0x00492bb0 are the only two reached from a
store rather than a vtable: five and one `push <addr>` sites hand them to `Timer::init` 0x00486d20 /
`Timer::initStartRepeat` 0x00486d90.

## 5. The UI event table 0x004ba87c (3 rows)

Thirteen consecutive function pointers, 0x00495750 to 0x00495d20, whose address the initializer 0x004799d0
stores in 0x0083a4f8. Slots 3, 4 and 5 were missing from the listing: `uiKeyEvent` 0x00495910 (ORs
0x10000 / 0x20000 / 0x40000 into the key code from `GetKeyState` of VK_SHIFT / VK_CONTROL / VK_MENU, then the
focus owner's virtual +0x144), `uiKeyEvent148` 0x00495990 (the focus owner's virtual +0x148) and `uiPointerMove`
0x004959b0 (`hitTestScreen` 0x0047f700, `drawCursor` 0x0047d130, enter/leave through the virtuals +0x70 and
+0x68). Slot 1 is the already-named `redrawRegion` 0x00495770 and slot 12 `Timer_fire` 0x00495cb0, which is what
fixes the table as the engine's event sink.

## 6. IJG libjpeg 6a (18 rows)

Every libjpeg function reached only through a method pointer that a `jinit_*` routine stores was missing. All
eighteen were identified by compiling the vendored release source and letting
`re/tools/match_autoname.py` pair each address with an obj symbol at **100%**:

```
py -3.12 re/tools/match_autoname.py re/match/golf_jpeg_memmgr.cpp  --module golf_clean.exe 0x004af540 0x004af730 0x004afa20
py -3.12 re/tools/match_autoname.py re/match/golf_jpeg_cmarker.cpp --module golf_clean.exe 0x004b0060
py -3.12 re/tools/match_autoname.py re/match/golf_jpeg_ccoefct.cpp --module golf_clean.exe 0x004b08b0 0x004b0b20
py -3.12 re/tools/match_autoname.py re/match/golf_jpeg_chuff.cpp   --module golf_clean.exe 0x004b12f0 0x004b1480 0x004b1ca0 0x004b1e20
py -3.12 re/tools/match_autoname.py re/match/golf_jpeg_cphuff.cpp  --module golf_clean.exe 0x004b1f70 0x004b2140 0x004b28c0 0x004b2d30 0x004b2eb0 0x004b33c0
py -3.12 re/tools/match_autoname.py re/match/golf_jpeg_cprepct.cpp --module golf_clean.exe 0x004b3b80 0x004b3d40
```

giving `access_virt_sarray`, `access_virt_barray`, `self_destruct` (filed as `self_destruct_jmemmgr` so the name
stays unique per module), `write_scan_header`, `compress_data`, `compress_first_pass`, `start_pass_huff`,
`encode_mcu_huff`, `encode_mcu_gather`, `finish_pass_gather`, `start_pass_phuff`, `encode_mcu_DC_first`,
`encode_mcu_AC_first`, `encode_mcu_DC_refine`, `encode_mcu_AC_refine`, `finish_pass_gather_phuff`,
`pre_process_data` and `pre_process_context`. The matching `// MATCH:` lines are not added to the
`re/match/golf_jpeg_*.cpp` files here, to keep this batch to the names files.

## 7. Entries that are NOT function starts (26)

26 of the 206 golf_clean.exe entries are switch case blocks inside a larger function. `undiscovered.py` takes
the nearest preceding referenced address as the entry, and a `switch` jump-table slot is such a reference.

- **25** already carry Ghidra's own switch label in the re-exported listing, which is decisive on its own:
  0x004867df and 0x00486835 (`switchD_004867d8::caseD_26` / `caseD_28`), and the twenty-three cases of the nine
  indirect jumps at 0x004a0a21, 0x004a0ac1, 0x004a0b61, 0x004a0c01, 0x004a0ca1, 0x004a0d41, 0x004a0dc1,
  0x004a1001 and 0x004a10a1 (`switchD_004a0a21::caseD_2`, `switchD_004a0ac1::caseD_1/2/10`, and so on).
  Each dispatcher is `mov eax, [ecx-8]; dec eax; cmp eax, 0xf; ja default; movzx edx, byte [eax+<bytemap>];
  jmp dword ptr [edx*4+<jumptable>]`, i.e. VC6's two-level switch (jump table followed by a byte index map such
  as the `00 01 04 04 ...` at 0x004a0b3c), and every case body adjusts `this` by a different negative offset and
  forwards to a different handler. The real missing entries are the enclosing functions, each starting at the
  `mov eax, [ecx-8]` that opens the dispatcher; 0x004a0a10 is one of them and
  `xref.py golf_clean.exe 0x004a0a10` reports that it is not a function entry in the module.
- **0x004115ef** has no switch label but the same evidence: its only reference 0x0042163c sits in a dense run of
  code addresses at 0x00421618..0x0042167c, several of them repeated (0x0041094f three times) and several
  pointing *into* the range the new function covers (0x004115dc, 0x00411cf9), i.e. a jump table for a switch
  inside `mainLoop` 0x0040f5c0, whose ranges end at 0x00421614. The block also reads `edx` before writing it
  (`push edx` at 0x004115f9) and indexes `[esp+0x4c]`, so it cannot be a function entry under any convention.

These 26 are left unnamed. They are already functions in the master, which is harmless, but they should not be
promoted or hooked.
