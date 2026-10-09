// C3 batch c3cr (round 22, 2026-10-09) of golf_clean.exe: UI rows below 0x491b70 reached through vtable slots,
// a stored callback pointer or a tail jump.
//
//   ListBox, the half of the class that reaches its two virtual bases through the vbtable pointer at this-0x58:
//     clearSelection 0x0048cac0        fwdOnScroll 0x0048c4f0           onNavKeyMarked 0x0048a9c0
//     fireSelectChangedMarked 0x0048c520  fireSelectChanged 0x0048c560  forwardClick 0x0048c610
//     fwdInnerVfn20 0x0048cbf0          fwdInnerVfn24 0x0048cc30         scrollBy 0x0048caf0
//   and the nine vtordisp thunks of table 0x004bb260 that enter them (`sub ecx, [ecx-4]; jmp <body>`):
//     0x0048cc90 0x0048cd00 0x0048ccc0 0x0048ccf0 0x0048cce0 0x0048cca0 0x0048cbe0 0x0048cc20 0x0048cc70
//   Window::isFocusChild 0x0047d7d0, Container::cycleSelection 0x0047af60,
//   EditBox::cycleSelectionIfOwned 0x00486b20, Button::timerCallback 0x00488fb0.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list)
// and cites the address of each instruction it reproduces. __thiscall is emulated with __fastcall (ecx = this,
// edx unused). Callees run through their original addresses:
//   scrollBy           -> Window::fwdScroll26c 0x0047b9f0 (C3, c3bx)
//   fireSelectChanged  -> ListModel::selectedId 0x00489950 (C3, c3u)
//   cycleSelection     -> Window::visible 0x004801f0 (C3, c3c)
//   cycleSelectionIfOwned -> Container::cycleSelection 0x0047af60 (this batch)
//   timerCallback      -> Button::onTimer 0x00488fd0 (C2, hooked by no batch)
//   each vtordisp thunk -> its body (this batch)
//
// The ListBox layout used below, read off the instructions (no source is matched): the dword at this-0x58 points
// at a vbtable whose entry [1] (+4) and entry [2] (+8) are the offsets, from this-0x58, of two virtual bases.
//   W = this - 0x58 + vbt[1]  the Window base: its vtable at W+0 (dispatched as `[edx+ecx-0x58]`, e.g. 0x0048cad7),
//                             the flag byte at W+0x9c (addressed as this+vbt[1]+0x44, 0x0048cbf9 / 0x0048caf9) and the
//                             parent window at W+0x130 (addressed as this+vbt[1]+0xd8, 0x0048c616).
//   M = this - 0x58 + vbt[2]  the list-model base: the selection field at M+0xf4 (this+vbt[2]+0x9c, 0x0048cac6),
//                             the row count at M+0xd0 (this+vbt[2]+0x78) and the visible rows at M+0x5c
//                             (this+vbt[2]+4, 0x0048cb3f / 0x0048cb43); ListModel::selectedId runs on M (0x0048c584).
//   this-0x34 is the top row (0x0048cb08) and this-8 a cdecl selection callback (0x0048c577 / 0x0048c58e).
// The code below keeps exactly the original's address arithmetic (this + vbt[k] + disp) instead of W and M, so a
// different reading of the layout cannot hide a difference.
//
// Globals: 0x0083b654 (the "marked" flag; its only references are 0x0048a0c0 and the five stores / loads in
// onNavKeyMarked, fireSelectChangedMarked and fireSelectChanged), 0x0083ab2c (written by fireSelectChanged at
// 0x0048c571; the running game rewrites it from its own dispatch, so that key freezes the game's first thread)
// and 0x0083ab98 (cycleSelection; its other two references are in setFocusItem 0x0047ae80).
#include "hooks.h"

namespace {

static inline char* B(void* p) { return reinterpret_cast<char*>(p); }
static inline int& I(void* base, int off) { return *reinterpret_cast<int*>(B(base) + off); }
static inline void* Pp(void* base, int off) { return *reinterpret_cast<void**>(B(base) + off); }
// The vbtable pointer at this-0x58 and its entry k (k = 1 or 2), read afresh at every use as the original does.
static inline int* vbt(void* self) { return *reinterpret_cast<int**>(B(self) - 0x58); }
static inline int vbo(void* self, int k) { return vbt(self)[k]; }
// `mov eax, [obj]; call [eax+off]`: slot off of the object's own vtable.
template <typename T> static inline T vslot(void* obj, int off) {
    return *reinterpret_cast<T*>(*reinterpret_cast<char**>(obj) + off);
}

typedef int(__fastcall* Vfn0_t)(void* ecx, void* edx);
typedef int(__fastcall* Vfn1_t)(void* ecx, void* edx, int a);
typedef int(__fastcall* Vfn2_t)(void* ecx, void* edx, int a, int b);

int& g_marked = *reinterpret_cast<int*>(0x0083b654);
void*& g_curWidget = *reinterpret_cast<void**>(0x0083ab2c);
void*& g_prevFocus = *reinterpret_cast<void**>(0x0083ab98);

typedef int(__fastcall* FwdScroll26c_t)(void* ecx, void* edx, int v);          // Window::fwdScroll26c, ret 4
const FwdScroll26c_t kFwdScroll26c = reinterpret_cast<FwdScroll26c_t>(0x0047b9f0);
typedef int(__fastcall* SelectedId_t)(void* ecx, void* edx);                    // ListModel::selectedId, ret
const SelectedId_t kSelectedId = reinterpret_cast<SelectedId_t>(0x00489950);
typedef int(__fastcall* Visible_t)(void* ecx, void* edx);                       // Window::visible, ret
const Visible_t kVisible = reinterpret_cast<Visible_t>(0x004801f0);
typedef int(__fastcall* OnTimer_t)(void* ecx, void* edx, int which);            // Button::onTimer, ret 4
const OnTimer_t kOnTimer = reinterpret_cast<OnTimer_t>(0x00488fd0);
typedef int(__fastcall* CycleSelection_t)(void* ecx, void* edx);                // Container::cycleSelection, ret
const CycleSelection_t kCycleSelection = reinterpret_cast<CycleSelection_t>(0x0047af60);

// ================================================================================================================
// ListBox bodies
// ================================================================================================================

// 0x0048cac0  ListBox::clearSelection(this, a, b): both stack arguments are unused (`ret 8` at 0x0048cae5).
// Writes -1 to the dword at this + vbt[2] + 0x9c (0x0048cac0 / 0x0048cac3 / 0x0048cac6), then calls slot +0x120 of
// the Window base this - 0x58 + vbt[1] with no argument (0x0048cad1..0x0048cadf); eax is that call's result.
typedef int(__fastcall* ClearSelection_t)(void* ecx, void* edx, int a, int b);
ClearSelection_t ClearSelection_orig;
int __fastcall ClearSelection_re(void* self, void*, int, int) {
    I(self, vbo(self, 2) + 0x9c) = -1;                                  // 0x0048cac6
    void* w = B(self) + vbo(self, 1) - 0x58;                            // 0x0048cad4 / 0x0048cadb
    return vslot<Vfn0_t>(w, 0x120)(w, 0);                               // 0x0048cadf
}

// 0x0048c4f0  ListBox::fwdOnScroll(this, a, b): calls slot +0xe8 of the Window base with (a, b) in the same order
// (pushes at 0x0048c502 / 0x0048c507, call at 0x0048c508) and returns its result.
typedef int(__fastcall* Fwd2_t)(void* ecx, void* edx, int a, int b);
Fwd2_t FwdOnScroll_orig;
int __fastcall FwdOnScroll_re(void* self, void*, int a, int b) {
    void* w = B(self) + vbo(self, 1) - 0x58;                            // 0x0048c4f3 / 0x0048c4fa
    return vslot<Vfn2_t>(w, 0xe8)(w, 0, a, b);                          // 0x0048c508
}

// 0x0048a9c0  ListBox::onNavKeyMarked(this, a, b): sets 0x0083b654 to 1 (0x0048a9c0), calls slot +0x78 of the
// Window base with (a, b) (0x0048a9e2), sets 0x0083b654 back to 0 (0x0048a9e5) and returns the call's result.
Fwd2_t OnNavKeyMarked_orig;
int __fastcall OnNavKeyMarked_re(void* self, void*, int a, int b) {
    g_marked = 1;                                                       // 0x0048a9c0
    void* w = B(self) + vbo(self, 1) - 0x58;                            // 0x0048a9cd / 0x0048a9d4
    int r = vslot<Vfn2_t>(w, 0x78)(w, 0, a, b);                         // 0x0048a9e2
    g_marked = 0;                                                       // 0x0048a9e5
    return r;
}

// 0x0048c520  ListBox::fireSelectChangedMarked(this, a, b): the same bracket around slot +0x98 (0x0048c520,
// 0x0048c542, 0x0048c548).
Fwd2_t FireSelectChangedMarked_orig;
int __fastcall FireSelectChangedMarked_re(void* self, void*, int a, int b) {
    g_marked = 1;                                                       // 0x0048c520
    void* w = B(self) + vbo(self, 1) - 0x58;                            // 0x0048c52d / 0x0048c534
    int r = vslot<Vfn2_t>(w, 0x98)(w, 0, a, b);                         // 0x0048c542
    g_marked = 0;                                                       // 0x0048c548
    return r;
}

// 0x0048c560  ListBox::fireSelectChanged(this, a, b): both stack arguments are unused (`ret 8` at 0x0048c607).
//  1. 0x0083ab2c = the parent at this + vbt[1] + 0xd8 (0x0048c56a / 0x0048c571).
//  2. When the cdecl callback at this-8 is set (0x0048c577 / 0x0048c57c), it is called with the result of
//     ListModel::selectedId on this - 0x58 + vbt[2] (0x0048c584..0x0048c58e).
//  3. The parent is read again (0x0048c59a); null ends the function with eax = the vbtable pointer (0x0048c5a3).
//     Otherwise its vtable is read first (0x0048c5a9), selectedId runs again and the parent's slot +0x108 gets
//     that id (0x0048c5af..0x0048c5b7).
//  4. 0x0083b654 is loaded (0x0048c5c0), the parent and its vtable are read again (0x0048c5cd / 0x0048c5d8),
//     selectedId runs a third time and slot +0x104 gets (id, the loaded flag) (0x0048c5da..0x0048c5e2).
//  5. The parent is read a fourth time (0x0048c5ef); when its +0x98 child is set (0x0048c5f6 / 0x0048c5fe) that
//     child's slot +0x1c is called and its result returned, else eax is the parent.
typedef int(__cdecl* SelCallback_t)(int id);
Fwd2_t FireSelectChanged_orig;
int __fastcall FireSelectChanged_re(void* self, void*, int, int) {
    g_curWidget = Pp(self, vbo(self, 1) + 0xd8);                        // 0x0048c56a / 0x0048c571
    SelCallback_t cb = reinterpret_cast<SelCallback_t>(Pp(self, -8));   // 0x0048c577
    if (cb != 0) {                                                      // 0x0048c57c
        void* m = B(self) + vbo(self, 2) - 0x58;                        // 0x0048c581 / 0x0048c584
        cb(kSelectedId(m, 0));                                          // 0x0048c588 / 0x0048c58e
    }
    void* parent = Pp(self, vbo(self, 1) + 0xd8);                       // 0x0048c59a
    if (parent == 0) {                                                  // 0x0048c5a1 / 0x0048c5a3
        return reinterpret_cast<int>(vbt(self));
    }
    Vfn1_t f108 = vslot<Vfn1_t>(parent, 0x108);                         // 0x0048c5a9 (vtable read first)
    int id = kSelectedId(B(self) + vbo(self, 2) - 0x58, 0);             // 0x0048c5ab / 0x0048c5af
    f108(parent, 0, id);                                                // 0x0048c5b7
    int marked = g_marked;                                              // 0x0048c5c0
    void* p2 = Pp(self, vbo(self, 1) + 0xd8);                           // 0x0048c5cd
    Vfn2_t f104 = vslot<Vfn2_t>(p2, 0x104);                             // 0x0048c5d8
    int id2 = kSelectedId(B(self) + vbo(self, 2) - 0x58, 0);            // 0x0048c5d4 / 0x0048c5da
    f104(p2, 0, id2, marked);                                           // 0x0048c5e2
    void* p3 = Pp(self, vbo(self, 1) + 0xd8);                           // 0x0048c5ef
    void* child = Pp(p3, 0x98);                                         // 0x0048c5f6
    if (child == 0) {                                                   // 0x0048c5fc / 0x0048c5fe
        return reinterpret_cast<int>(p3);
    }
    return vslot<Vfn0_t>(child, 0x1c)(child, 0);                        // 0x0048c602
}

// 0x0048c610  ListBox::forwardClick(this, a, b): the parent at this + vbt[1] + 0xd8 (0x0048c616); when it is set
// (0x0048c61d / 0x0048c61f) its slot +0x74 gets (a, b) (0x0048c621..0x0048c62d) and eax is that result; when it is
// null eax still holds the vbtable pointer loaded at 0x0048c610.
Fwd2_t ForwardClick_orig;
int __fastcall ForwardClick_re(void* self, void*, int a, int b) {
    int* t = vbt(self);                                                 // 0x0048c610
    void* parent = Pp(self, t[1] + 0xd8);                               // 0x0048c613 / 0x0048c616
    if (parent == 0) {                                                  // 0x0048c61d / 0x0048c61f
        return reinterpret_cast<int>(t);
    }
    return vslot<Vfn2_t>(parent, 0x74)(parent, 0, a, b);                // 0x0048c62d
}

// 0x0048cbf0  ListBox::fwdInnerVfn20(this): when bit 0x40 of the byte at this + vbt[1] + 0x44 is set (0x0048cbf9 /
// 0x0048cbfd / 0x0048cc00) and the parent at this + vbt[1] + 0xd8 is not null (0x0048cc02 / 0x0048cc08 /
// 0x0048cc0a), tail-jumps to the parent's slot +0x20 (0x0048cc0e); otherwise returns 0 (0x0048cc11).
typedef int(__fastcall* Fwd0_t)(void* ecx, void* edx);
Fwd0_t FwdInnerVfn20_orig;
int __fastcall FwdInnerVfn20_re(void* self, void*) {
    int d = vbo(self, 1);                                               // 0x0048cbf3
    if ((*reinterpret_cast<unsigned char*>(B(self) + d + 0x44) & 0x40) != 0) {   // 0x0048cbf9 / 0x0048cbfd
        void* parent = Pp(self, d + 0xd8);                              // 0x0048cc02
        if (parent != 0) {                                              // 0x0048cc08 / 0x0048cc0a
            return vslot<Vfn0_t>(parent, 0x20)(parent, 0);              // 0x0048cc0e
        }
    }
    return 0;                                                           // 0x0048cc11
}

// 0x0048cc30  ListBox::fwdInnerVfn24(this, a): the same two tests (0x0048cc39 / 0x0048cc3d / 0x0048cc40,
// 0x0048cc42 / 0x0048cc48 / 0x0048cc4a); when both pass the parent's slot +0x24 gets a (0x0048cc53) and its result
// is returned, otherwise a itself is returned (0x0048cc59).
typedef int(__fastcall* Fwd1_t)(void* ecx, void* edx, int a);
Fwd1_t FwdInnerVfn24_orig;
int __fastcall FwdInnerVfn24_re(void* self, void*, int a) {
    int d = vbo(self, 1);                                               // 0x0048cc33
    if ((*reinterpret_cast<unsigned char*>(B(self) + d + 0x44) & 0x40) != 0) {   // 0x0048cc39 / 0x0048cc3d
        void* parent = Pp(self, d + 0xd8);                              // 0x0048cc42
        if (parent != 0) {                                              // 0x0048cc48 / 0x0048cc4a
            return vslot<Vfn1_t>(parent, 0x24)(parent, 0, a);           // 0x0048cc53
        }
    }
    return a;                                                           // 0x0048cc59
}

// 0x0048caf0  ListBox::scrollBy(this, dir, b, c): only dir is read (`ret 0xc` at 0x0048cb38 / 0x0048cb7f).
// Nothing happens unless bit 3 of the byte at this + vbt[1] + 0x44 is set (0x0048caf9 / 0x0048cafe).
// dir > 0 (signed, 0x0048cb06): when the top row at this-0x34 is positive (0x0048cb0b / 0x0048cb0d) it is
//   decremented and stored (0x0048cb0f / 0x0048cb10).
// dir <= 0: when the top row is below the dword at this + vbt[2] + 0x78 minus the dword at this + vbt[2] + 4
//   (0x0048cb3f..0x0048cb52, signed jge) it is incremented and stored (0x0048cb54 / 0x0048cb57).
// After a change the new top goes to Window::fwdScroll26c on the Window base (0x0048cb16..0x0048cb1e /
// 0x0048cb5a..0x0048cb65) and the Window base's slot +0x120 is called with no argument (0x0048cb31 / 0x0048cb78).
// eax on return: the slot's result after a change; otherwise the vbtable pointer (flag clear), the top row read at
// 0x0048cb08 (dir > 0, top <= 0) or this + vbt[2] (dir <= 0, at the bottom, 0x0048cb47).
typedef int(__fastcall* ScrollBy_t)(void* ecx, void* edx, int dir, int b, int c);
ScrollBy_t ScrollBy_orig;
int __fastcall ScrollBy_re(void* self, void*, int dir, int, int) {
    int* t = vbt(self);                                                 // 0x0048caf3
    if ((*reinterpret_cast<unsigned char*>(B(self) + t[1] + 0x44) & 8) == 0) {   // 0x0048caf6 / 0x0048caf9
        return reinterpret_cast<int>(t);                                // 0x0048cafe
    }
    if (dir > 0) {                                                      // 0x0048cb04 / 0x0048cb06
        int top = I(self, -0x34);                                       // 0x0048cb08
        if (top <= 0) {                                                 // 0x0048cb0b / 0x0048cb0d
            return top;
        }
        top -= 1;                                                       // 0x0048cb0f
        I(self, -0x34) = top;                                           // 0x0048cb10
        kFwdScroll26c(B(self) + vbo(self, 1) - 0x58, 0, top);           // 0x0048cb13..0x0048cb1e
        void* w = B(self) + vbo(self, 1) - 0x58;                        // 0x0048cb26 / 0x0048cb2d
        return vslot<Vfn0_t>(w, 0x120)(w, 0);                           // 0x0048cb31
    }
    int d2 = t[2];                                                      // 0x0048cb3b
    int limit = I(self, d2 + 0x78) - I(self, d2 + 4);                   // 0x0048cb3f / 0x0048cb43 / 0x0048cb4d
    int top = I(self, -0x34);                                           // 0x0048cb4a
    if (top >= limit) {                                                 // 0x0048cb50 / 0x0048cb52
        return reinterpret_cast<int>(B(self) + d2);                     // eax = lea [ecx+esi] at 0x0048cb47
    }
    top += 1;                                                           // 0x0048cb54
    I(self, -0x34) = top;                                               // 0x0048cb57
    kFwdScroll26c(B(self) + vbo(self, 1) - 0x58, 0, top);               // 0x0048cb5a..0x0048cb65
    void* w = B(self) + vbo(self, 1) - 0x58;                            // 0x0048cb6d / 0x0048cb74
    return vslot<Vfn0_t>(w, 0x120)(w, 0);                               // 0x0048cb78
}

// ================================================================================================================
// The vtordisp thunks of table 0x004bb260: `sub ecx, dword ptr [ecx - 4]` then a jump to the body. The thunk
// receives the Window base and the displacement stored just below it gives the ListBox this. Each passes its
// stack arguments through untouched (a jmp), so it has its body's signature.
// ================================================================================================================
static inline void* vtordispThis(void* p) { return B(p) - *reinterpret_cast<int*>(B(p) - 4); }

typedef int(__fastcall* Body2_t)(void* ecx, void* edx, int a, int b);
typedef int(__fastcall* Body0_t)(void* ecx, void* edx);
typedef int(__fastcall* Body1_t)(void* ecx, void* edx, int a);

// 0x0048cc90  ListBox::clearSelection_vtordisp: 0x0048cc90 sub, 0x0048cc93 jmp 0x0048cac0 (slot 28 of 0x004bb260).
Fwd2_t ClearSelectionVd_orig;
int __fastcall ClearSelectionVd_re(void* p, void*, int a, int b) {
    return reinterpret_cast<Body2_t>(0x0048cac0)(vtordispThis(p), 0, a, b);
}
// 0x0048cd00  ListBox::fwdOnScroll_vtordisp: 0x0048cd00 sub, 0x0048cd03 jmp 0x0048c4f0 (slot 57).
Fwd2_t FwdOnScrollVd_orig;
int __fastcall FwdOnScrollVd_re(void* p, void*, int a, int b) {
    return reinterpret_cast<Body2_t>(0x0048c4f0)(vtordispThis(p), 0, a, b);
}
// 0x0048ccc0  ListBox::onNavKeyMarked_vtordisp: 0x0048ccc0 sub, 0x0048ccc3 jmp 0x0048a9c0 (slot 33).
Fwd2_t OnNavKeyMarkedVd_orig;
int __fastcall OnNavKeyMarkedVd_re(void* p, void*, int a, int b) {
    return reinterpret_cast<Body2_t>(0x0048a9c0)(vtordispThis(p), 0, a, b);
}
// 0x0048ccf0  ListBox::fireSelectChangedMarked_vtordisp: 0x0048ccf0 sub, 0x0048ccf3 jmp 0x0048c520 (slot 39).
Fwd2_t FireSelectChangedMarkedVd_orig;
int __fastcall FireSelectChangedMarkedVd_re(void* p, void*, int a, int b) {
    return reinterpret_cast<Body2_t>(0x0048c520)(vtordispThis(p), 0, a, b);
}
// 0x0048cce0  ListBox::fireSelectChanged_vtordisp: 0x0048cce0 sub, 0x0048cce3 jmp 0x0048c560 (slot 38).
Fwd2_t FireSelectChangedVd_orig;
int __fastcall FireSelectChangedVd_re(void* p, void*, int a, int b) {
    return reinterpret_cast<Body2_t>(0x0048c560)(vtordispThis(p), 0, a, b);
}
// 0x0048cca0  ListBox::forwardClick_vtordisp: 0x0048cca0 sub, 0x0048cca3 jmp 0x0048c610 (slot 29).
Fwd2_t ForwardClickVd_orig;
int __fastcall ForwardClickVd_re(void* p, void*, int a, int b) {
    return reinterpret_cast<Body2_t>(0x0048c610)(vtordispThis(p), 0, a, b);
}
// 0x0048cbe0  ListBox::fwdInnerVfn20_vtordisp: 0x0048cbe0 sub, 0x0048cbe3 jmp 0x0048cbf0 (slot 6).
Fwd0_t FwdInnerVfn20Vd_orig;
int __fastcall FwdInnerVfn20Vd_re(void* p, void*) {
    return reinterpret_cast<Body0_t>(0x0048cbf0)(vtordispThis(p), 0);
}
// 0x0048cc20  ListBox::fwdInnerVfn24_vtordisp: 0x0048cc20 sub, 0x0048cc23 jmp 0x0048cc30 (slot 7).
Fwd1_t FwdInnerVfn24Vd_orig;
int __fastcall FwdInnerVfn24Vd_re(void* p, void*, int a) {
    return reinterpret_cast<Body1_t>(0x0048cc30)(vtordispThis(p), 0, a);
}
// 0x0048cc70  ListBox::scrollBy_vtordisp: 0x0048cc70 sub, 0x0048cc73 jmp 0x0048caf0 (slot 25).
ScrollBy_t ScrollByVd_orig;
int __fastcall ScrollByVd_re(void* p, void*, int dir, int b, int c) {
    return reinterpret_cast<ScrollBy_t>(0x0048caf0)(vtordispThis(p), 0, dir, b, c);
}

// ================================================================================================================
// Window / Container / EditBox / Button
// ================================================================================================================

// 0x0047d7d0  Window::isFocusChild(this): 1 when bit 12 of the flag dword at this+0x9c is set (`test ah, 0x10` at
// 0x0047d7d6 / 0x0047d7d9 / 0x0047d7db). Otherwise 0 without a parent at this+0x130 (0x0047d7e1 / 0x0047d7e9);
// with one, the parent's focused item is the +4 of the node its cursor +0x13c points at, or none when its list head
// +0x138 is null (0x0047d7eb..0x0047d800), and the result is 1 exactly when that item is this (0x0047d802 /
// 0x0047d804 / 0x0047d806), else 0 (0x0047d80c).
typedef int(__fastcall* IsFocusChild_t)(void* ecx, void* edx);
IsFocusChild_t IsFocusChild_orig;
int __fastcall IsFocusChild_re(void* self, void*) {
    if ((I(self, 0x9c) & 0x1000) != 0) {                                // 0x0047d7d0 / 0x0047d7d6
        return 1;                                                       // 0x0047d7db
    }
    void* parent = Pp(self, 0x130);                                     // 0x0047d7e1
    if (parent == 0) {                                                  // 0x0047d7e9
        return 0;                                                       // 0x0047d80c
    }
    void* cur = 0;                                                      // 0x0047d800
    if (Pp(parent, 0x138) != 0) {                                       // 0x0047d7eb / 0x0047d7f3
        cur = Pp(Pp(parent, 0x13c), 4);                                 // 0x0047d7f5 / 0x0047d7fb
    }
    return cur == self ? 1 : 0;                                         // 0x0047d802 / 0x0047d804
}

// 0x0047af60  Container::cycleSelection(this): move the parent's focus to the next visible child.
// p = this+0x130. Nothing happens without a parent (0x0047af6b) or when the parent's child count +0x140 is not
// above 1 (0x0047af71 / 0x0047af78, signed); eax is then the parent (or 0).
// The current item (cursor node +0x13c's +4, or none when the head +0x138 is null, 0x0047af7e..0x0047af93) is
// saved in 0x0083ab98 (0x0047af95). The loop (0x0047afad..0x0047b019) runs at most count times: each pass, when
// the head is set, steps the cursor to the node's +0xc, increments the index +0x144 and wraps it to 0 when it
// reaches the count (0x0047afb7..0x0047afdd); it then tests the cursor node's item with Window::visible
// (0x0047afe7..0x0047b003) and stops at the first visible one (0x0047b00a). With a null head that item is
// NULL and Window::visible reads [0xa0]: the original faults, so the fixtures never pass a null head here.
// When the item now under the cursor equals the saved one (0x0047b01b..0x0047b03e) the function returns with
// eax = that item. Otherwise this's own slot +0xf0 is called with 0 (0x0047b040..0x0047b046), the parent's head
// and cursor are read again (0x0047b04c..0x0047b05f) and, when both the head and the cursor node's item are set
// (0x0047b05d / 0x0047b067), that item's slot +0xf0 is called with 1 (0x0047b06b / 0x0047b06d).
typedef int(__fastcall* FocusVfn_t)(void* ecx, void* edx, int on);
CycleSelection_t CycleSelection_orig;
int __fastcall CycleSelection_re(void* self, void*) {
    void* p = Pp(self, 0x130);                                          // 0x0047af63
    if (p == 0) {                                                       // 0x0047af69 / 0x0047af6b
        return 0;
    }
    if (I(p, 0x140) <= 1) {                                             // 0x0047af71 / 0x0047af78
        return reinterpret_cast<int>(p);
    }
    void* saved = 0;                                                    // 0x0047af93
    if (Pp(p, 0x138) != 0) {                                            // 0x0047af7e / 0x0047af86
        saved = Pp(Pp(p, 0x13c), 4);                                    // 0x0047af88 / 0x0047af8e
    }
    g_prevFocus = saved;                                                // 0x0047af95
    p = Pp(self, 0x130);                                                // 0x0047af9a
    if (I(p, 0x140) > 0) {                                              // 0x0047afa3 / 0x0047afab
        for (int i = 0;;) {
            if (Pp(p, 0x138) != 0) {                                    // 0x0047afad / 0x0047afb5
                void* next = Pp(Pp(p, 0x13c), 0xc);                     // 0x0047afb7 / 0x0047afbd
                int idx = I(p, 0x144) + 1;                              // 0x0047afc0 / 0x0047afd2
                *reinterpret_cast<void**>(B(p) + 0x13c) = next;         // 0x0047afc6
                I(p, 0x144) = idx;                                      // 0x0047afd5
                if (idx == I(p, 0x140)) {                               // 0x0047afcc / 0x0047afd3 / 0x0047afdb
                    I(p, 0x144) = 0;                                    // 0x0047afdd
                }
            }
            void* q = Pp(self, 0x130);                                  // 0x0047afe7
            void* item = 0;                                             // 0x0047b001
            if (Pp(q, 0x138) != 0) {                                    // 0x0047aff2 / 0x0047aff7
                item = Pp(Pp(q, 0x13c), 4);                             // 0x0047aff9 / 0x0047affc
            }
            if (kVisible(item, 0) != 0) {                               // 0x0047b003 / 0x0047b00a
                break;
            }
            p = Pp(self, 0x130);                                        // 0x0047b00c
            ++i;                                                        // 0x0047b012
            if (i >= I(p, 0x140)) {                                     // 0x0047b013 / 0x0047b019
                break;
            }
        }
    }
    void* q = Pp(self, 0x130);                                          // 0x0047b01b
    void* now = 0;                                                      // 0x0047b036
    if (Pp(q, 0x138) != 0) {                                            // 0x0047b027 / 0x0047b02c
        now = Pp(Pp(q, 0x13c), 4);                                      // 0x0047b02e / 0x0047b031
    }
    if (now == g_prevFocus) {                                           // 0x0047b038 / 0x0047b03e
        return reinterpret_cast<int>(now);
    }
    vslot<FocusVfn_t>(self, 0xf0)(self, 0, 0);                          // 0x0047b040..0x0047b046
    void* r = Pp(self, 0x130);                                          // 0x0047b04c
    void* head = Pp(r, 0x138);                                          // 0x0047b058
    if (head == 0) {                                                    // 0x0047b05b / 0x0047b05d
        return 0;
    }
    void* node = Pp(r, 0x13c);                                          // 0x0047b05f
    void* it = Pp(node, 4);                                             // 0x0047b062
    if (it == 0) {                                                      // 0x0047b065 / 0x0047b067
        return reinterpret_cast<int>(node);
    }
    return vslot<FocusVfn_t>(it, 0xf0)(it, 0, 1);                       // 0x0047b06b / 0x0047b06d
}

// 0x00486b20  EditBox::cycleSelectionIfOwned(this): with a parent at this+0x130 (0x00486b20 / 0x00486b26 /
// 0x00486b28) tail-jumps to Container::cycleSelection 0x0047af60 with the same this (0x00486b2a); without one
// returns with eax = 0 (the parent just loaded, 0x00486b2f).
CycleSelection_t CycleSelectionIfOwned_orig;
int __fastcall CycleSelectionIfOwned_re(void* self, void*) {
    if (Pp(self, 0x130) == 0) {                                         // 0x00486b20 / 0x00486b28
        return 0;                                                       // 0x00486b2f
    }
    return kCycleSelection(self, 0);                                    // 0x00486b2a
}

// 0x00488fb0  Button::timerCallback(which, button), cdecl: the callback Button::onTimer stores with
// `push 0x488fb0` at 0x004890af for Timer::initStartOnce. With a non-null button (0x00488fb0 / 0x00488fb4 /
// 0x00488fb6) it calls Button::onTimer 0x00488fd0 on that button with `which` (0x00488fb8..0x00488fbd).
typedef int(__cdecl* TimerCallback_t)(int which, void* button);
TimerCallback_t TimerCallback_orig;
int __cdecl TimerCallback_re(int which, void* button) {
    if (button != 0) {                                                  // 0x00488fb4 / 0x00488fb6
        return kOnTimer(button, 0, which);                              // 0x00488fbd
    }
    return 0;
}

}  // namespace

// 0x0048cac0  ListBox::clearSelection
SG_HOOK("golf_clean.exe", 0x0048cac0, ListBox_clearSelection, ClearSelection_re, ClearSelection_orig);
// 0x0048c4f0  ListBox::fwdOnScroll
SG_HOOK("golf_clean.exe", 0x0048c4f0, ListBox_fwdOnScroll, FwdOnScroll_re, FwdOnScroll_orig);
// 0x0048a9c0  ListBox::onNavKeyMarked
SG_HOOK("golf_clean.exe", 0x0048a9c0, ListBox_onNavKeyMarked, OnNavKeyMarked_re, OnNavKeyMarked_orig);
// 0x0048c520  ListBox::fireSelectChangedMarked
SG_HOOK("golf_clean.exe", 0x0048c520, ListBox_fireSelectChangedMarked, FireSelectChangedMarked_re,
        FireSelectChangedMarked_orig);
// 0x0048c560  ListBox::fireSelectChanged
SG_HOOK("golf_clean.exe", 0x0048c560, ListBox_fireSelectChanged, FireSelectChanged_re, FireSelectChanged_orig);
// 0x0048c610  ListBox::forwardClick
SG_HOOK("golf_clean.exe", 0x0048c610, ListBox_forwardClick, ForwardClick_re, ForwardClick_orig);
// 0x0048cbf0  ListBox::fwdInnerVfn20
SG_HOOK("golf_clean.exe", 0x0048cbf0, ListBox_fwdInnerVfn20, FwdInnerVfn20_re, FwdInnerVfn20_orig);
// 0x0048cc30  ListBox::fwdInnerVfn24
SG_HOOK("golf_clean.exe", 0x0048cc30, ListBox_fwdInnerVfn24, FwdInnerVfn24_re, FwdInnerVfn24_orig);
// 0x0048caf0  ListBox::scrollBy
SG_HOOK("golf_clean.exe", 0x0048caf0, ListBox_scrollBy, ScrollBy_re, ScrollBy_orig);
// 0x0048cc90  ListBox::clearSelection_vtordisp
SG_HOOK("golf_clean.exe", 0x0048cc90, ListBox_clearSelection_vtordisp, ClearSelectionVd_re, ClearSelectionVd_orig);
// 0x0048cd00  ListBox::fwdOnScroll_vtordisp
SG_HOOK("golf_clean.exe", 0x0048cd00, ListBox_fwdOnScroll_vtordisp, FwdOnScrollVd_re, FwdOnScrollVd_orig);
// 0x0048ccc0  ListBox::onNavKeyMarked_vtordisp
SG_HOOK("golf_clean.exe", 0x0048ccc0, ListBox_onNavKeyMarked_vtordisp, OnNavKeyMarkedVd_re, OnNavKeyMarkedVd_orig);
// 0x0048ccf0  ListBox::fireSelectChangedMarked_vtordisp
SG_HOOK("golf_clean.exe", 0x0048ccf0, ListBox_fireSelectChangedMarked_vtordisp, FireSelectChangedMarkedVd_re,
        FireSelectChangedMarkedVd_orig);
// 0x0048cce0  ListBox::fireSelectChanged_vtordisp
SG_HOOK("golf_clean.exe", 0x0048cce0, ListBox_fireSelectChanged_vtordisp, FireSelectChangedVd_re,
        FireSelectChangedVd_orig);
// 0x0048cca0  ListBox::forwardClick_vtordisp
SG_HOOK("golf_clean.exe", 0x0048cca0, ListBox_forwardClick_vtordisp, ForwardClickVd_re, ForwardClickVd_orig);
// 0x0048cbe0  ListBox::fwdInnerVfn20_vtordisp
SG_HOOK("golf_clean.exe", 0x0048cbe0, ListBox_fwdInnerVfn20_vtordisp, FwdInnerVfn20Vd_re, FwdInnerVfn20Vd_orig);
// 0x0048cc20  ListBox::fwdInnerVfn24_vtordisp
SG_HOOK("golf_clean.exe", 0x0048cc20, ListBox_fwdInnerVfn24_vtordisp, FwdInnerVfn24Vd_re, FwdInnerVfn24Vd_orig);
// 0x0048cc70  ListBox::scrollBy_vtordisp
SG_HOOK("golf_clean.exe", 0x0048cc70, ListBox_scrollBy_vtordisp, ScrollByVd_re, ScrollByVd_orig);
// 0x0047d7d0  Window::isFocusChild
SG_HOOK("golf_clean.exe", 0x0047d7d0, Window_isFocusChild, IsFocusChild_re, IsFocusChild_orig);
// 0x0047af60  Container::cycleSelection
SG_HOOK("golf_clean.exe", 0x0047af60, Container_cycleSelection, CycleSelection_re, CycleSelection_orig);
// 0x00486b20  EditBox::cycleSelectionIfOwned
SG_HOOK("golf_clean.exe", 0x00486b20, EditBox_cycleSelectionIfOwned, CycleSelectionIfOwned_re,
        CycleSelectionIfOwned_orig);
// 0x00488fb0  Button::timerCallback
SG_HOOK("golf_clean.exe", 0x00488fb0, Button_timerCallback, TimerCallback_re, TimerCallback_orig);
