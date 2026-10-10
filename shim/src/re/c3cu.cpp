// C3 batch c3cu (round 23, 2026-10-09) of golf_clean.exe: the composite-view layer from 0x004a0c90 up
// (re/analysis/systems/undisc2_exe.md).
//
//   panelSub hot-list callbacks (slots 29..39 of table 0x004bc244):
//     hotHitCb0 0x004a43f0 .. hotHitCb6 0x004a4570 and their vtordisp thunks 0x004a49f0 .. 0x004a4a60,
//     panelSubSlot60_vtordisp 0x004a4a70
//   panelRow (table 0x004bc0a8): widgetShow 0x004a2fa0, panelRowCycleSelection 0x004a2ff0 and their thunks
//     0x004a30a0 / 0x004a30b0
//   the view class's tag dispatchers (viewCtor4a0740's table 0x004bb8d0): viewDispatchSelectChanged 0x004a0c90,
//     viewDispatchSlot29 0x004a0db0, viewDispatchSlot33 0x004a0e20, viewDispatchSlot39 0x004a0e40,
//     viewDispatchSlot31 0x004a0e60, viewDispatchSlot34 0x004a0e90, viewDispatchSlot32 0x004a0eb0,
//     viewDispatchSlot60 0x004a0ff0, viewDispatchCycleSelection 0x004a1090, viewDispatchSlot6 0x004a1130,
//     viewDispatchScrollBy 0x004a13c0
//   the destructors of seven file-scope Surface aggregates (atexit routines, render): gSurfaceDtor_44ae00,
//     gSurfaceDtor_44afb0, gSurfaceDtor_44b1b0, gSurfaceDtor_44b3e0, gSurfaceDtor_44b4f0, gSurfaceDtor_44b690,
//     gSurfaceDtor_44b790
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list)
// and cites the address of each instruction it reproduces. __thiscall is emulated with __fastcall (ecx = this,
// edx unused). Callees run through their original addresses:
//   hotHitCb0..6           -> HotList::hitTest 0x00492a90 (C3, c3d)
//   viewDispatchSlot29     -> ListBox::forwardClick 0x0048c610 (C3, c3cr), panelOnPick 0x004a4350 (C2)
//   viewDispatchSelectChanged -> 0x0049fc30 (no row), ListBox::fireSelectChanged 0x0048c560 (C3, c3cr),
//                             hotHitCb5 (this batch), listSelectionChanged 0x0049e640 (C2)
//   viewDispatchSlot31..39 -> hotHitCb1/2/3/4/6 (this batch)
//   viewDispatchSlot60     -> 0x0049fd80, 0x00487af0, widgetShow (this batch), 0x004a4870, 0x0049e7b0 (no rows)
//   viewDispatchCycleSelection -> Container::cycleSelection 0x0047af60 (C3, c3cr), panelRowCycleSelection (this batch)
//   viewDispatchScrollBy   -> ListBox::scrollBy 0x0048caf0 (C3, c3cr)
//   panelRowCycleSelection -> Container::cycleSelection 0x0047af60 (C3, c3cr)
//   each vtordisp thunk    -> its body
//   gSurfaceDtor_*         -> Surface_free 0x00473ae0 (C2, no hook), the CRT vector destructor iterator 0x004a5713
//                             with element destructor unwindFree_4041f0 0x004041f0 (C2, no hook)
//
// The view object: the dispatchers receive the class's table-C sub-object D and read a 32-bit tag at D-8. Each tag
// selects one of the other five classes' sub-objects by a fixed displacement (undisc2_exe.md section 3):
//   tag 1    D-0x168  optionDialog      tag 2  D-0x1a0  ListBox        tag 4  D-0x4  panelRow
//   tag 8    D-0x8c   panelSub          tag 0x10 D-0x180 listPanel
// The two-level switches (`dec eax; cmp eax, 0xf; ja; movzx edx, byte [eax+map]; jmp [edx*4+table]`) were decoded
// from their byte maps and jump tables in the image (map / table): 0x004a0d1c / 0x004a0d08 (selectChanged),
// 0x004a0e08 / 0x004a0df8 (slot 29), 0x004a107c / 0x004a1064 (slot 60), 0x004a1120 / 0x004a1108 (cycleSelection).
// A tag whose byte selects the last (default) case, or a tag outside 1..16, returns with eax = tag - 1, the value
// `dec eax` left.
#include "hooks.h"

namespace {

static inline char* B(void* p) { return reinterpret_cast<char*>(p); }
static inline int& I(void* base, int off) { return *reinterpret_cast<int*>(B(base) + off); }
static inline void* Pp(void* base, int off) { return *reinterpret_cast<void**>(B(base) + off); }
// `mov eax, [obj]; call [eax+off]`: slot off of the object's own vtable.
template <typename T> static inline T vslot(void* obj, int off) {
    return *reinterpret_cast<T*>(*reinterpret_cast<char**>(obj) + off);
}
// `sub ecx, dword ptr [ecx - 4]`: the vtordisp adjustment every thunk of this batch makes before its jmp.
static inline void* vtordispThis(void* p) { return B(p) - *reinterpret_cast<int*>(B(p) - 4); }

typedef int(__fastcall* Vfn0_t)(void* ecx, void* edx);
typedef int(__fastcall* Vfn1_t)(void* ecx, void* edx, int a);
typedef int(__fastcall* M0_t)(void* ecx, void* edx);
typedef int(__fastcall* M1_t)(void* ecx, void* edx, int a);
typedef void(__fastcall* M1v_t)(void* ecx, void* edx, int a);
typedef int(__fastcall* M2_t)(void* ecx, void* edx, int a, int b);
typedef void(__fastcall* M2v_t)(void* ecx, void* edx, int a, int b);
typedef int(__fastcall* M3_t)(void* ecx, void* edx, int a, int b, int c);

typedef int(__fastcall* HitTest_t)(void* ecx, void* edx, int x, int y, int* aOut, int* bOut);  // ret 0x10
const HitTest_t kHitTest = reinterpret_cast<HitTest_t>(0x00492a90);
const M2_t kForwardClick = reinterpret_cast<M2_t>(0x0048c610);          // ListBox::forwardClick, ret 8
const M2_t kPanelOnPick = reinterpret_cast<M2_t>(0x004a4350);           // panelOnPick, ret 8
const M2_t kOptionSel = reinterpret_cast<M2_t>(0x0049fc30);             // optionDialog slot 38, ret 8
const M2_t kFireSelectChanged = reinterpret_cast<M2_t>(0x0048c560);     // ListBox::fireSelectChanged, ret 8
const M2_t kListSelectionChanged = reinterpret_cast<M2_t>(0x0049e640);  // listSelectionChanged, ret 8
const M3_t kScrollBy = reinterpret_cast<M3_t>(0x0048caf0);              // ListBox::scrollBy, ret 0xc
const M0_t kCycleSelection = reinterpret_cast<M0_t>(0x0047af60);        // Container::cycleSelection, ret
const M1_t kOptionSlot60 = reinterpret_cast<M1_t>(0x0049fd80);          // ret 4
const M1v_t kListBoxSlot60 = reinterpret_cast<M1v_t>(0x00487af0);       // the single instruction `ret 4`
const M1_t kPanelSubSlot60 = reinterpret_cast<M1_t>(0x004a4870);        // ret 4
const M1_t kListPanelSlot60 = reinterpret_cast<M1_t>(0x0049e7b0);       // ret 4
const M1_t kWidgetShow = reinterpret_cast<M1_t>(0x004a2fa0);            // widgetShow, ret 4
const M0_t kPanelRowCycleSelection = reinterpret_cast<M0_t>(0x004a2ff0);
const M2_t kHotHitCb1 = reinterpret_cast<M2_t>(0x004a4430);
const M2_t kHotHitCb2 = reinterpret_cast<M2_t>(0x004a4470);
const M2_t kHotHitCb3 = reinterpret_cast<M2_t>(0x004a44b0);
const M2_t kHotHitCb4 = reinterpret_cast<M2_t>(0x004a44f0);
const M2_t kHotHitCb5 = reinterpret_cast<M2_t>(0x004a4530);
const M2_t kHotHitCb6 = reinterpret_cast<M2_t>(0x004a4570);

// ================================================================================================================
// hotHitCb0..6 (panelSub, this = P)
// ================================================================================================================
// All seven are the same 0x40-byte body and differ only in the callback slot they read:
//   hotHitCb0 0x004a43f0  [P-0x20] (0x004a4414)     hotHitCb4 0x004a44f0  [P-0x10] (0x004a4514)
//   hotHitCb1 0x004a4430  [P-0x1c] (0x004a4454)     hotHitCb5 0x004a4530  [P-0x0c] (0x004a4554)
//   hotHitCb2 0x004a4470  [P-0x18] (0x004a4494)     hotHitCb6 0x004a4570  [P-0x08] (0x004a4594)
//   hotHitCb3 0x004a44b0  [P-0x14] (0x004a44d4)
// (x, y) are the two stack arguments (`ret 8`). The HotList at P-0xd0 (`lea ecx, [esi-0xd0]`, 0x004a4404) is hit-
// tested with (x, y, aOut = the address of y's own stack slot, bOut = 0) (pushes 0x004a43ff..0x004a4403, call
// 0x004a440a), so on a hit HotList::hitTest overwrites y with the entry's +0x18 dword.
//  - miss (hitTest returned -1, 0x004a440f / 0x004a4412): returns -1;
//  - hit and a null callback (0x004a4417 / 0x004a4419): returns 0, the callback value just loaded;
//  - hit and a callback: calls it, cdecl, with the (overwritten) y slot (0x004a441b..0x004a4422) and returns its eax.
typedef int(__cdecl* HitCallback_t)(int v);
static int hotHitCommon(void* self, int x, int y, int cbOff) {
    int slot = y;                                                       // the y stack slot (0x004a43f7 / 0x004a43fb)
    int r = kHitTest(B(self) - 0xd0, 0, x, y, &slot, 0);                // 0x004a43ff..0x004a440a
    if (r == -1) {                                                      // 0x004a440f / 0x004a4412
        return -1;
    }
    HitCallback_t cb = reinterpret_cast<HitCallback_t>(Pp(self, cbOff));   // 0x004a4414
    if (cb == 0) {                                                      // 0x004a4417 / 0x004a4419
        return 0;
    }
    return cb(slot);                                                    // 0x004a441b..0x004a4420
}

// 0x004a43f0  hotHitCb0: callback at P-0x20 (0x004a4414).
M2_t HotHitCb0_orig;
int __fastcall HotHitCb0_re(void* self, void*, int x, int y) { return hotHitCommon(self, x, y, -0x20); }
// 0x004a4430  hotHitCb1: callback at P-0x1c (0x004a4454).
M2_t HotHitCb1_orig;
int __fastcall HotHitCb1_re(void* self, void*, int x, int y) { return hotHitCommon(self, x, y, -0x1c); }
// 0x004a4470  hotHitCb2: callback at P-0x18 (0x004a4494).
M2_t HotHitCb2_orig;
int __fastcall HotHitCb2_re(void* self, void*, int x, int y) { return hotHitCommon(self, x, y, -0x18); }
// 0x004a44b0  hotHitCb3: callback at P-0x14 (0x004a44d4).
M2_t HotHitCb3_orig;
int __fastcall HotHitCb3_re(void* self, void*, int x, int y) { return hotHitCommon(self, x, y, -0x14); }
// 0x004a44f0  hotHitCb4: callback at P-0x10 (0x004a4514).
M2_t HotHitCb4_orig;
int __fastcall HotHitCb4_re(void* self, void*, int x, int y) { return hotHitCommon(self, x, y, -0x10); }
// 0x004a4530  hotHitCb5: callback at P-0xc (0x004a4554).
M2_t HotHitCb5_orig;
int __fastcall HotHitCb5_re(void* self, void*, int x, int y) { return hotHitCommon(self, x, y, -0x0c); }
// 0x004a4570  hotHitCb6: callback at P-0x8 (0x004a4594).
M2_t HotHitCb6_orig;
int __fastcall HotHitCb6_re(void* self, void*, int x, int y) { return hotHitCommon(self, x, y, -0x08); }

// ================================================================================================================
// panelSub vtordisp thunks (table 0x004bc244): `sub ecx, dword ptr [ecx-4]; jmp <body>`, arguments untouched.
// ================================================================================================================
// 0x004a49f0  hotHitCb0_vtordisp: 0x004a49f0 sub, 0x004a49f3 jmp 0x004a43f0 (slot 30).
M2_t HotHitCb0Vd_orig;
int __fastcall HotHitCb0Vd_re(void* p, void*, int x, int y) {
    return reinterpret_cast<M2_t>(0x004a43f0)(vtordispThis(p), 0, x, y);
}
// 0x004a4a00  hotHitCb1_vtordisp: 0x004a4a00 sub, 0x004a4a03 jmp 0x004a4430 (slot 31).
M2_t HotHitCb1Vd_orig;
int __fastcall HotHitCb1Vd_re(void* p, void*, int x, int y) { return kHotHitCb1(vtordispThis(p), 0, x, y); }
// 0x004a4a10  hotHitCb2_vtordisp: 0x004a4a10 sub, 0x004a4a13 jmp 0x004a4470 (slot 32).
M2_t HotHitCb2Vd_orig;
int __fastcall HotHitCb2Vd_re(void* p, void*, int x, int y) { return kHotHitCb2(vtordispThis(p), 0, x, y); }
// 0x004a4a20  hotHitCb3_vtordisp: 0x004a4a20 sub, 0x004a4a23 jmp 0x004a44b0 (slot 33).
M2_t HotHitCb3Vd_orig;
int __fastcall HotHitCb3Vd_re(void* p, void*, int x, int y) { return kHotHitCb3(vtordispThis(p), 0, x, y); }
// 0x004a4a30  hotHitCb4_vtordisp: 0x004a4a30 sub, 0x004a4a33 jmp 0x004a44f0 (slot 34).
M2_t HotHitCb4Vd_orig;
int __fastcall HotHitCb4Vd_re(void* p, void*, int x, int y) { return kHotHitCb4(vtordispThis(p), 0, x, y); }
// 0x004a4a50  hotHitCb5_vtordisp: 0x004a4a50 sub, 0x004a4a53 jmp 0x004a4530 (slot 38).
M2_t HotHitCb5Vd_orig;
int __fastcall HotHitCb5Vd_re(void* p, void*, int x, int y) { return kHotHitCb5(vtordispThis(p), 0, x, y); }
// 0x004a4a60  hotHitCb6_vtordisp: 0x004a4a60 sub, 0x004a4a63 jmp 0x004a4570 (slot 39).
M2_t HotHitCb6Vd_orig;
int __fastcall HotHitCb6Vd_re(void* p, void*, int x, int y) { return kHotHitCb6(vtordispThis(p), 0, x, y); }
// 0x004a4a70  panelSubSlot60_vtordisp: 0x004a4a70 sub, 0x004a4a73 jmp 0x004a4870 (slot 60). The body 0x004a4870
// reads the vbtable at this-0xe0, adds its entry +4 and calls slot +0x120 of the base found there (0x004a4870..
// 0x004a4887); its one stack argument is not read (`ret 4` at 0x004a488d).
M1_t PanelSubSlot60Vd_orig;
int __fastcall PanelSubSlot60Vd_re(void* p, void*, int a) { return kPanelSubSlot60(vtordispThis(p), 0, a); }

// ================================================================================================================
// panelRow (this = R)
// ================================================================================================================
// 0x004a2fa0  widgetShow(this, on): the ten child pointers R-0x88 .. R-0x64 (`lea esi, [ecx-0x88]`, count 0xa at
// 0x004a2fc6, step 4 at 0x004a2fdb).
//  - on != 0 (0x004a2fa0 / 0x004a2fa4 / 0x004a2fa6): only the first child [R-0x88] (0x004a2fa8); when it is set
//    (0x004a2fae / 0x004a2fb0) its slot +0xf0 gets `on` and that result is returned (0x004a2fb4 / 0x004a2fb5);
//    when it is null eax still holds `on` (0x004a2fe3).
//  - on == 0: every non-null child (0x004a2fcb..0x004a2fcf) gets slot +0xf0 with 0 (0x004a2fd3 / 0x004a2fd5);
//    eax is the last call's result, or 0 (the `on` loaded at 0x004a2fa0) when no child is set.
typedef int(__fastcall* FocusVfn_t)(void* ecx, void* edx, int on);
M1_t WidgetShow_orig;
int __fastcall WidgetShow_re(void* self, void*, int on) {
    if (on != 0) {                                                      // 0x004a2fa4 / 0x004a2fa6
        void* c = Pp(self, -0x88);                                      // 0x004a2fa8
        if (c == 0) {                                                   // 0x004a2fae / 0x004a2fb0
            return on;
        }
        return vslot<FocusVfn_t>(c, 0xf0)(c, 0, on);                    // 0x004a2fb2..0x004a2fb5
    }
    int r = on;                                                         // eax = 0 from 0x004a2fa0
    for (int i = 0; i < 10; ++i) {                                      // 0x004a2fc6 / 0x004a2fde / 0x004a2fdf
        void* c = Pp(self, -0x88 + 4 * i);                              // 0x004a2fcb
        if (c != 0) {                                                   // 0x004a2fcd / 0x004a2fcf
            r = vslot<FocusVfn_t>(c, 0xf0)(c, 0, 0);                    // 0x004a2fd1..0x004a2fd5
        }
    }
    return r;
}

// 0x004a2ff0  panelRowCycleSelection(this): the vbtable at R-0x8c, its entry +4 = d (0x004a2ff0 / 0x004a2ff6) and
// X = R + d (0x004a2ff9). When [X+0xb4] (0x004a2ffc..0x004a3005) and [X+0xac] (0x004a3007..0x004a300f) are both
// non-zero and the object Z = [[X+0xb0]+4] (0x004a3011 / 0x004a3017) is not null (0x004a301a / 0x004a301c), it tail-
// jumps to Z's slot +0x128 (0x004a301e / 0x004a3020). Otherwise it tail-jumps to Container::cycleSelection
// 0x0047af60 with this = X-0x8c (0x004a3026 / 0x004a302c).
M0_t PanelRowCycleSelection_orig;
int __fastcall PanelRowCycleSelection_re(void* self, void*) {
    int d = (*reinterpret_cast<int**>(B(self) - 0x8c))[1];              // 0x004a2ff0 / 0x004a2ff6
    char* x = B(self) + d;                                              // 0x004a2ff9
    if (I(x, 0xb4) != 0 && I(x, 0xac) != 0) {                           // 0x004a2ffc..0x004a300f
        void* z = Pp(Pp(x, 0xb0), 4);                                   // 0x004a3011 / 0x004a3017
        if (z != 0) {                                                   // 0x004a301a / 0x004a301c
            return vslot<Vfn0_t>(z, 0x128)(z, 0);                       // 0x004a301e / 0x004a3020
        }
    }
    return kCycleSelection(x - 0x8c, 0);                                // 0x004a3026 / 0x004a302c
}

// 0x004a30a0  widgetShow_vtordisp: 0x004a30a0 sub, 0x004a30a3 jmp 0x004a2fa0 (slot 60 of 0x004bc0a8).
M1_t WidgetShowVd_orig;
int __fastcall WidgetShowVd_re(void* p, void*, int on) { return kWidgetShow(vtordispThis(p), 0, on); }
// 0x004a30b0  panelRowCycleSelection_vtordisp: 0x004a30b0 sub, 0x004a30b3 jmp 0x004a2ff0 (slot 74 of 0x004bc0a8).
M0_t PanelRowCycleSelectionVd_orig;
int __fastcall PanelRowCycleSelectionVd_re(void* p, void*) { return kPanelRowCycleSelection(vtordispThis(p), 0); }

// ================================================================================================================
// The view class's tag dispatchers (this = D, tag = [D-8])
// ================================================================================================================
static inline int tagOf(void* d) { return I(d, -8); }

// 0x004a0c90  viewDispatchSelectChanged(this, a, b) (`ret 8`): u = tag - 1 (0x004a0c90 / 0x004a0c93); u above 15
// unsigned (0x004a0c94 / 0x004a0c97) or a byte-map entry 4 returns u (0x004a0d05). The byte map 0x004a0d1c sends
// tag 1 -> 0x004a0cd8: 0x0049fc30 on D-0x168 (0x004a0ce2 / 0x004a0ce8); tag 2 -> 0x004a0cc0: ListBox::fireSelectChanged
// on D-0x1a0 (0x004a0cca / 0x004a0cd0); tag 8 -> 0x004a0cf0: hotHitCb5 on D-0x8c (0x004a0cfa / 0x004a0d00);
// tag 0x10 -> 0x004a0ca8: listSelectionChanged on D-0x180 (0x004a0cb2 / 0x004a0cb8). Each passes (a, b) in order
// (pushes of [esp+8] then [esp+4]) and returns the callee's eax.
M2_t SelectChanged_orig;
int __fastcall SelectChanged_re(void* self, void*, int a, int b) {
    int tag = tagOf(self);                                              // 0x004a0c90
    switch (tag) {
    case 1:    return kOptionSel(B(self) - 0x168, 0, a, b);             // 0x004a0cd8..0x004a0ce8
    case 2:    return kFireSelectChanged(B(self) - 0x1a0, 0, a, b);     // 0x004a0cc0..0x004a0cd0
    case 8:    return kHotHitCb5(B(self) - 0x8c, 0, a, b);              // 0x004a0cf0..0x004a0d00
    case 0x10: return kListSelectionChanged(B(self) - 0x180, 0, a, b);  // 0x004a0ca8..0x004a0cb8
    default:   return tag - 1;                                          // 0x004a0c93, ret at 0x004a0d05
    }
}

// 0x004a0db0  viewDispatchSlot29(this, a, b) (`ret 8`): byte map 0x004a0e08, jump table 0x004a0df8. Tag 2 ->
// 0x004a0dc8: ListBox::forwardClick on D-0x1a0 (0x004a0dd2 / 0x004a0dd8); tag 8 -> 0x004a0de0: panelOnPick on
// D-0x8c (0x004a0dea / 0x004a0df0); every other tag (map entries 0 and 3 both point at the `ret 8` 0x004a0df5)
// returns tag - 1 (0x004a0db3).
M2_t Slot29_orig;
int __fastcall Slot29_re(void* self, void*, int a, int b) {
    int tag = tagOf(self);                                              // 0x004a0db0
    if (tag == 2) return kForwardClick(B(self) - 0x1a0, 0, a, b);       // 0x004a0dc8..0x004a0dd8
    if (tag == 8) return kPanelOnPick(B(self) - 0x8c, 0, a, b);         // 0x004a0de0..0x004a0df0
    return tag - 1;                                                     // 0x004a0db3, ret at 0x004a0df5
}

// 0x004a0e60  viewDispatchSlot31(this, a, b) (`ret 8`): eax = tag (0x004a0e60); tag 4 returns it (0x004a0e63 /
// 0x004a0e66), any tag other than 8 returns it (0x004a0e68 / 0x004a0e6b), tag 8 calls hotHitCb1 on D-0x8c with
// (a, b) (0x004a0e6d..0x004a0e7d) and returns its eax.
M2_t Slot31_orig;
int __fastcall Slot31_re(void* self, void*, int a, int b) {
    int tag = tagOf(self);                                              // 0x004a0e60
    if (tag == 4) return tag;                                           // 0x004a0e63 / 0x004a0e66
    if (tag != 8) return tag;                                           // 0x004a0e68 / 0x004a0e6b
    return kHotHitCb1(B(self) - 0x8c, 0, a, b);                         // 0x004a0e77 / 0x004a0e7d
}

// The four `cmp dword ptr [ecx-8], 8; jne <ret 8>` dispatchers. Their other path leaves eax as the caller left it,
// so they are modelled (and A/B'd) as returning nothing; the tag-8 path calls the slot's hotHitCb on D-0x8c with
// (a, b).
// 0x004a0eb0  viewDispatchSlot32: 0x004a0eb0 / 0x004a0eb4, hotHitCb2 at 0x004a0ec0 / 0x004a0ec6.
M2v_t Slot32_orig;
void __fastcall Slot32_re(void* self, void*, int a, int b) {
    if (tagOf(self) == 8) kHotHitCb2(B(self) - 0x8c, 0, a, b);
}
// 0x004a0e20  viewDispatchSlot33: 0x004a0e20 / 0x004a0e24, hotHitCb3 at 0x004a0e30 / 0x004a0e36.
M2v_t Slot33_orig;
void __fastcall Slot33_re(void* self, void*, int a, int b) {
    if (tagOf(self) == 8) kHotHitCb3(B(self) - 0x8c, 0, a, b);
}
// 0x004a0e90  viewDispatchSlot34: 0x004a0e90 / 0x004a0e94, hotHitCb4 at 0x004a0ea0 / 0x004a0ea6.
M2v_t Slot34_orig;
void __fastcall Slot34_re(void* self, void*, int a, int b) {
    if (tagOf(self) == 8) kHotHitCb4(B(self) - 0x8c, 0, a, b);
}
// 0x004a0e40  viewDispatchSlot39: 0x004a0e40 / 0x004a0e44, hotHitCb6 at 0x004a0e50 / 0x004a0e56.
M2v_t Slot39_orig;
void __fastcall Slot39_re(void* self, void*, int a, int b) {
    if (tagOf(self) == 8) kHotHitCb6(B(self) - 0x8c, 0, a, b);
}

// 0x004a0ff0  viewDispatchSlot60(this, a) (`ret 4`): byte map 0x004a107c, jump table 0x004a1064.
// tag 1 -> 0x004a102e: 0x0049fd80 on D-0x168; tag 2 -> 0x004a101b: 0x00487af0 on D-0x1a0, a bare `ret 4` that
// leaves eax = u = 1 from 0x004a0ff3; tag 4 -> 0x004a1054: widgetShow on D-4; tag 8 -> 0x004a1041: 0x004a4870 on
// D-0x8c; tag 0x10 -> 0x004a1008: 0x0049e7b0 on D-0x180; anything else returns tag - 1 (0x004a1061).
M1_t Slot60_orig;
int __fastcall Slot60_re(void* self, void*, int a) {
    int tag = tagOf(self);                                              // 0x004a0ff0
    int u = tag - 1;                                                    // 0x004a0ff3
    switch (tag) {
    case 1:    return kOptionSlot60(B(self) - 0x168, 0, a);             // 0x004a1032 / 0x004a1039
    case 2:    kListBoxSlot60(B(self) - 0x1a0, 0, a); return u;         // 0x004a101f / 0x004a1026
    case 4:    return kWidgetShow(B(self) - 4, 0, a);                   // 0x004a1058 / 0x004a105c
    case 8:    return kPanelSubSlot60(B(self) - 0x8c, 0, a);            // 0x004a1045 / 0x004a104c
    case 0x10: return kListPanelSlot60(B(self) - 0x180, 0, a);          // 0x004a100c / 0x004a1013
    default:   return u;                                                // 0x004a1061
    }
}

// 0x004a1090  viewDispatchCycleSelection(this) (`ret`): byte map 0x004a1120, jump table 0x004a1108. Tags 1, 2, 8 and
// 0x10 go to four identical blocks (0x004a10d2, 0x004a10bd, 0x004a10e7, 0x004a10a8): the vbtable at D-0x1f8, its
// entry +4 = d, tail jump to Container::cycleSelection 0x0047af60 with this = D - 0x1f8 + d. Tag 4 -> 0x004a10fc:
// tail jump to panelRowCycleSelection on D-4. Anything else returns tag - 1 (0x004a1104).
M0_t ViewCycleSelection_orig;
int __fastcall ViewCycleSelection_re(void* self, void*) {
    int tag = tagOf(self);                                              // 0x004a1090
    switch (tag) {
    case 1: case 2: case 8: case 0x10: {
        int d = (*reinterpret_cast<int**>(B(self) - 0x1f8))[1];         // 0x004a10a8 / 0x004a10ae
        return kCycleSelection(B(self) - 0x1f8 + d, 0);                 // 0x004a10b1 / 0x004a10b8
    }
    case 4:  return kPanelRowCycleSelection(B(self) - 4, 0);            // 0x004a10fc / 0x004a10ff
    default: return tag - 1;                                            // 0x004a1104
    }
}

// 0x004a1130  viewDispatchSlot6(this) (`ret`): any tag other than 2 returns 0 (0x004a1133 / 0x004a1136 /
// 0x004a1164). Tag 2 is ListBox::fwdInnerVfn20 0x0048cbf0 written in place with offsets shifted by -0x1a0: the
// vbtable at D-0x1f8, entry +4 = d (0x004a1138 / 0x004a113e); bit 0x40 of the byte at D+d-0x15c (0x004a1148 /
// 0x004a114f) and the pointer at D+d-0x1a0+0xd8 (0x004a1141 / 0x004a1154) must be set (0x004a1152, 0x004a115a /
// 0x004a115c), and then that object's slot +0x20 is called and its eax returned (0x004a115e / 0x004a1160);
// otherwise 0.
M0_t Slot6_orig;
int __fastcall Slot6_re(void* self, void*) {
    if (tagOf(self) != 2) {                                             // 0x004a1133 / 0x004a1136
        return 0;                                                       // 0x004a1164
    }
    int d = (*reinterpret_cast<int**>(B(self) - 0x1f8))[1];             // 0x004a1138 / 0x004a113e
    if ((*reinterpret_cast<unsigned char*>(B(self) + d - 0x15c) & 0x40) == 0) {   // 0x004a1148 / 0x004a114f
        return 0;
    }
    void* c = Pp(B(self) + d - 0x1a0, 0xd8);                            // 0x004a1141 / 0x004a1154
    if (c == 0) {                                                       // 0x004a115a / 0x004a115c
        return 0;
    }
    return vslot<Vfn0_t>(c, 0x20)(c, 0);                                // 0x004a115e / 0x004a1160
}

// 0x004a13c0  viewDispatchScrollBy(this, a, b, c) (`ret 0xc`): any tag other than 2 returns tag - 2 (0x004a13c3 /
// 0x004a13c6 / 0x004a13e2). Tag 2 calls ListBox::scrollBy on D-0x1a0 with (a, b, c) in the same order (pushes at
// 0x004a13d0 / 0x004a13d5 / 0x004a13d6, call 0x004a13dd) and returns its eax.
M3_t ScrollBy_orig;
int __fastcall ScrollBy_re(void* self, void*, int a, int b, int c) {
    int tag = tagOf(self);                                              // 0x004a13c0
    if (tag != 2) {                                                     // 0x004a13c3 / 0x004a13c6
        return tag - 2;
    }
    return kScrollBy(B(self) - 0x1a0, 0, a, b, c);                      // 0x004a13d7 / 0x004a13dd
}

// ================================================================================================================
// Destructors of file-scope Surface aggregates (this = the aggregate; `ret` without arguments)
// ================================================================================================================
// Each is a C++ destructor with an SEH frame (push -1 / push <handler> / fs:[0] at entry, the state byte at [esp+0x10]
// lowered before each member, fs:[0] restored at exit). Its members are 0x2c-byte Surfaces destroyed in reverse
// declaration order:
//   a single member at +off: `lea ecx, [esi+off]; mov dword ptr [ecx], 0x4ba2d8; call 0x473ae0` (the Surface vtable
//     0x004ba2d8 stored back, then Surface_free 0x00473ae0 on it);
//   an array member of n Surfaces at +off: `push 0x4041f0; push n; push 0x2c; push esi+off; call 0x4a5713`, the CRT
//     vector destructor iterator (stdcall, `ret 0x10` at 0x004a5778), which runs unwindFree_4041f0 0x004041f0 (the
//     same two steps) on each element from the last to the first (0x004a574a..0x004a5758).
// The member tables below are read off each listing; the state bytes and the SEH frame are not modelled (they live
// in the stack frame only).
struct SurfMember { int off; int count; };   // count 0 = a single Surface
typedef void(__fastcall* SurfaceFree_t)(void* ecx, void* edx);
const SurfaceFree_t kSurfaceFree = reinterpret_cast<SurfaceFree_t>(0x00473ae0);
typedef void(__stdcall* EhVecDtor_t)(void* base, unsigned size, int count, void* dtor);
const EhVecDtor_t kEhVecDtor = reinterpret_cast<EhVecDtor_t>(0x004a5713);
void* const kUnwindFree = reinterpret_cast<void*>(0x004041f0);

static void destroySurfaces(void* self, const SurfMember* m, int n) {
    for (int i = 0; i < n; ++i) {
        char* p = B(self) + m[i].off;
        if (m[i].count == 0) {
            *reinterpret_cast<unsigned*>(p) = 0x004ba2d8;
            kSurfaceFree(p, 0);
        } else {
            kEhVecDtor(p, 0x2c, m[i].count, kUnwindFree);
        }
    }
}

typedef void(__fastcall* Dtor_t)(void* ecx, void* edx);

// 0x0044ae00  gSurfaceDtor_44ae00: +0x2c (0x0044ae1d..0x0044ae2e), +0 (0x0044ae33..0x0044ae43).
const SurfMember kDtor44ae00[] = {{0x2c, 0}, {0, 0}};
Dtor_t Dtor44ae00_orig;
void __fastcall Dtor44ae00_re(void* self, void*) { destroySurfaces(self, kDtor44ae00, 2); }

// 0x0044afb0  gSurfaceDtor_44afb0: +0xdc (0x0044afcd), +0xb0 (0x0044afe6), +0x84 (0x0044affc), +0x58 (0x0044b012),
// +0x2c (0x0044b025), +0 (0x0044b038), each followed by its Surface_free call.
const SurfMember kDtor44afb0[] = {{0xdc, 0}, {0xb0, 0}, {0x84, 0}, {0x58, 0}, {0x2c, 0}, {0, 0}};
Dtor_t Dtor44afb0_orig;
void __fastcall Dtor44afb0_re(void* self, void*) { destroySurfaces(self, kDtor44afb0, 6); }

// 0x0044b1b0  gSurfaceDtor_44b1b0: array of 0x24 at +0x2c0 (0x0044b1cd..0x0044b1e5), array of 8 at +0x160
// (0x0044b1ea..0x0044b1ff), then +0x134 (0x0044b204), +0x108 (0x0044b21a), +0xdc (0x0044b230), +0xb0 (0x0044b246),
// +0x84 (0x0044b25c), +0x58 (0x0044b272), +0x2c (0x0044b285), +0 (0x0044b298).
const SurfMember kDtor44b1b0[] = {{0x2c0, 0x24}, {0x160, 8}, {0x134, 0}, {0x108, 0}, {0xdc, 0},
                                  {0xb0, 0},     {0x84, 0},  {0x58, 0},  {0x2c, 0},  {0, 0}};
Dtor_t Dtor44b1b0_orig;
void __fastcall Dtor44b1b0_re(void* self, void*) { destroySurfaces(self, kDtor44b1b0, 10); }

// 0x0044b3e0  gSurfaceDtor_44b3e0: array of 0x12 at +0x58 (0x0044b3fd..0x0044b412), +0x2c (0x0044b417), +0
// (0x0044b42a).
const SurfMember kDtor44b3e0[] = {{0x58, 0x12}, {0x2c, 0}, {0, 0}};
Dtor_t Dtor44b3e0_orig;
void __fastcall Dtor44b3e0_re(void* self, void*) { destroySurfaces(self, kDtor44b3e0, 3); }

// 0x0044b4f0  gSurfaceDtor_44b4f0: array of 0x15 at +0x318 (0x0044b50d..0x0044b525), +0x2ec (0x0044b52a), array of
// 0xc at +0xdc (0x0044b540..0x0044b555), array of 4 at +0x2c (0x0044b55a..0x0044b56c), +0 (0x0044b571).
const SurfMember kDtor44b4f0[] = {{0x318, 0x15}, {0x2ec, 0}, {0xdc, 0xc}, {0x2c, 4}, {0, 0}};
Dtor_t Dtor44b4f0_orig;
void __fastcall Dtor44b4f0_re(void* self, void*) { destroySurfaces(self, kDtor44b4f0, 5); }

// 0x0044b690  gSurfaceDtor_44b690: array of 4 at +0x2c (0x0044b6ad..0x0044b6c2), +0 (0x0044b6c7).
const SurfMember kDtor44b690[] = {{0x2c, 4}, {0, 0}};
Dtor_t Dtor44b690_orig;
void __fastcall Dtor44b690_re(void* self, void*) { destroySurfaces(self, kDtor44b690, 2); }

// 0x0044b790  gSurfaceDtor_44b790: eleven singles from +0x1e4 down to +0x2c in steps of 0x2c (0x0044b7ad ..
// 0x0044b897), then +0 (0x0044b89c).
const SurfMember kDtor44b790[] = {{0x1e4, 0}, {0x1b8, 0}, {0x18c, 0}, {0x160, 0}, {0x134, 0}, {0x108, 0},
                                  {0xdc, 0},  {0xb0, 0},  {0x84, 0},  {0x58, 0},  {0x2c, 0},  {0, 0}};
Dtor_t Dtor44b790_orig;
void __fastcall Dtor44b790_re(void* self, void*) { destroySurfaces(self, kDtor44b790, 12); }

}  // namespace

// 0x004a43f0  hotHitCb0
SG_HOOK("golf_clean.exe", 0x004a43f0, hotHitCb0, HotHitCb0_re, HotHitCb0_orig);
// 0x004a4430  hotHitCb1
SG_HOOK("golf_clean.exe", 0x004a4430, hotHitCb1, HotHitCb1_re, HotHitCb1_orig);
// 0x004a4470  hotHitCb2
SG_HOOK("golf_clean.exe", 0x004a4470, hotHitCb2, HotHitCb2_re, HotHitCb2_orig);
// 0x004a44b0  hotHitCb3
SG_HOOK("golf_clean.exe", 0x004a44b0, hotHitCb3, HotHitCb3_re, HotHitCb3_orig);
// 0x004a44f0  hotHitCb4
SG_HOOK("golf_clean.exe", 0x004a44f0, hotHitCb4, HotHitCb4_re, HotHitCb4_orig);
// 0x004a4530  hotHitCb5
SG_HOOK("golf_clean.exe", 0x004a4530, hotHitCb5, HotHitCb5_re, HotHitCb5_orig);
// 0x004a4570  hotHitCb6
SG_HOOK("golf_clean.exe", 0x004a4570, hotHitCb6, HotHitCb6_re, HotHitCb6_orig);
// 0x004a49f0  hotHitCb0_vtordisp
SG_HOOK("golf_clean.exe", 0x004a49f0, hotHitCb0_vtordisp, HotHitCb0Vd_re, HotHitCb0Vd_orig);
// 0x004a4a00  hotHitCb1_vtordisp
SG_HOOK("golf_clean.exe", 0x004a4a00, hotHitCb1_vtordisp, HotHitCb1Vd_re, HotHitCb1Vd_orig);
// 0x004a4a10  hotHitCb2_vtordisp
SG_HOOK("golf_clean.exe", 0x004a4a10, hotHitCb2_vtordisp, HotHitCb2Vd_re, HotHitCb2Vd_orig);
// 0x004a4a20  hotHitCb3_vtordisp
SG_HOOK("golf_clean.exe", 0x004a4a20, hotHitCb3_vtordisp, HotHitCb3Vd_re, HotHitCb3Vd_orig);
// 0x004a4a30  hotHitCb4_vtordisp
SG_HOOK("golf_clean.exe", 0x004a4a30, hotHitCb4_vtordisp, HotHitCb4Vd_re, HotHitCb4Vd_orig);
// 0x004a4a50  hotHitCb5_vtordisp
SG_HOOK("golf_clean.exe", 0x004a4a50, hotHitCb5_vtordisp, HotHitCb5Vd_re, HotHitCb5Vd_orig);
// 0x004a4a60  hotHitCb6_vtordisp
SG_HOOK("golf_clean.exe", 0x004a4a60, hotHitCb6_vtordisp, HotHitCb6Vd_re, HotHitCb6Vd_orig);
// 0x004a4a70  panelSubSlot60_vtordisp
SG_HOOK("golf_clean.exe", 0x004a4a70, panelSubSlot60_vtordisp, PanelSubSlot60Vd_re, PanelSubSlot60Vd_orig);
// 0x004a2fa0  widgetShow
SG_HOOK("golf_clean.exe", 0x004a2fa0, widgetShow, WidgetShow_re, WidgetShow_orig);
// 0x004a2ff0  panelRowCycleSelection
SG_HOOK("golf_clean.exe", 0x004a2ff0, panelRowCycleSelection, PanelRowCycleSelection_re, PanelRowCycleSelection_orig);
// 0x004a30a0  widgetShow_vtordisp
SG_HOOK("golf_clean.exe", 0x004a30a0, widgetShow_vtordisp, WidgetShowVd_re, WidgetShowVd_orig);
// 0x004a30b0  panelRowCycleSelection_vtordisp
SG_HOOK("golf_clean.exe", 0x004a30b0, panelRowCycleSelection_vtordisp, PanelRowCycleSelectionVd_re,
        PanelRowCycleSelectionVd_orig);
// 0x004a0c90  viewDispatchSelectChanged
SG_HOOK("golf_clean.exe", 0x004a0c90, viewDispatchSelectChanged, SelectChanged_re, SelectChanged_orig);
// 0x004a0db0  viewDispatchSlot29
SG_HOOK("golf_clean.exe", 0x004a0db0, viewDispatchSlot29, Slot29_re, Slot29_orig);
// 0x004a0e60  viewDispatchSlot31
SG_HOOK("golf_clean.exe", 0x004a0e60, viewDispatchSlot31, Slot31_re, Slot31_orig);
// 0x004a0eb0  viewDispatchSlot32
SG_HOOK("golf_clean.exe", 0x004a0eb0, viewDispatchSlot32, Slot32_re, Slot32_orig);
// 0x004a0e20  viewDispatchSlot33
SG_HOOK("golf_clean.exe", 0x004a0e20, viewDispatchSlot33, Slot33_re, Slot33_orig);
// 0x004a0e90  viewDispatchSlot34
SG_HOOK("golf_clean.exe", 0x004a0e90, viewDispatchSlot34, Slot34_re, Slot34_orig);
// 0x004a0e40  viewDispatchSlot39
SG_HOOK("golf_clean.exe", 0x004a0e40, viewDispatchSlot39, Slot39_re, Slot39_orig);
// 0x004a0ff0  viewDispatchSlot60
SG_HOOK("golf_clean.exe", 0x004a0ff0, viewDispatchSlot60, Slot60_re, Slot60_orig);
// 0x004a1090  viewDispatchCycleSelection
SG_HOOK("golf_clean.exe", 0x004a1090, viewDispatchCycleSelection, ViewCycleSelection_re, ViewCycleSelection_orig);
// 0x004a1130  viewDispatchSlot6
SG_HOOK("golf_clean.exe", 0x004a1130, viewDispatchSlot6, Slot6_re, Slot6_orig);
// 0x004a13c0  viewDispatchScrollBy
SG_HOOK("golf_clean.exe", 0x004a13c0, viewDispatchScrollBy, ScrollBy_re, ScrollBy_orig);
// 0x0044ae00  gSurfaceDtor_44ae00
SG_HOOK("golf_clean.exe", 0x0044ae00, gSurfaceDtor_44ae00, Dtor44ae00_re, Dtor44ae00_orig);
// 0x0044afb0  gSurfaceDtor_44afb0
SG_HOOK("golf_clean.exe", 0x0044afb0, gSurfaceDtor_44afb0, Dtor44afb0_re, Dtor44afb0_orig);
// 0x0044b1b0  gSurfaceDtor_44b1b0
SG_HOOK("golf_clean.exe", 0x0044b1b0, gSurfaceDtor_44b1b0, Dtor44b1b0_re, Dtor44b1b0_orig);
// 0x0044b3e0  gSurfaceDtor_44b3e0
SG_HOOK("golf_clean.exe", 0x0044b3e0, gSurfaceDtor_44b3e0, Dtor44b3e0_re, Dtor44b3e0_orig);
// 0x0044b4f0  gSurfaceDtor_44b4f0
SG_HOOK("golf_clean.exe", 0x0044b4f0, gSurfaceDtor_44b4f0, Dtor44b4f0_re, Dtor44b4f0_orig);
// 0x0044b690  gSurfaceDtor_44b690
SG_HOOK("golf_clean.exe", 0x0044b690, gSurfaceDtor_44b690, Dtor44b690_re, Dtor44b690_orig);
// 0x0044b790  gSurfaceDtor_44b790
SG_HOOK("golf_clean.exe", 0x0044b790, gSurfaceDtor_44b790, Dtor44b790_re, Dtor44b790_orig);
