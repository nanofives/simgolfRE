// C3 batch c3ej (round 30, 2026-10-12, Sonnet 5.5): the panel bodies and the view dispatchers of golf_clean.exe on REAL
// panels. The composite viewCtor4a0740 0x004a0740 (built at TextView+0x1460 by TextView::ctor) holds a ListBox part, the
// listPanel part (viewFieldCtor49d5a0), the optionDialog part (viewFieldCtor49ebb0), a panel (panelSubCtor4a3110) and the
// field part 4a2250 over ONE Window base (+0x1fc) and ONE ListModel base (+0x774). Its Window vtable 0x004bb8d0 and its
// ListModel vtable 0x004bb8b0 hold the two-level switch dispatchers below: each reads the tag dword at (Window base - 8)
// = composite +0x1f4 (ListModel dispatchers: ListModel base - 0x580, the same dword) and runs the matching part's body
// on `this` moved by a fixed amount: tag 1 -0x168 (optionDialog part), 2 -0x1a0 (ListBox), 4 -4 (part 4a2250), 8 -0x8c
// (panel), 0x10 -0x180 (listPanel part). Index table (bytes at 0x004a0aa0 etc., the same in all dispatchers): tag-1 ->
// case 0/1/-/2/-/-/-/3/.../4 for tags 1/2/(3)/4/.../8/.../0x10, every other value the default.
//
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and cites the
// instructions it reproduces. __thiscall is emulated with __fastcall (ecx = this, edx unused). Callees run through their
// original addresses, so a callee another batch hooks runs its reimplementation (the verifier re-runs the keys with those
// hooks off; list in log/c3/c3ej_notes.txt).
#include "hooks.h"
#include <string.h>

namespace {

static inline char* B(void* p) { return reinterpret_cast<char*>(p); }
static inline void*& P(void* base, int off) { return *reinterpret_cast<void**>(B(base) + off); }
static inline int& I(void* base, int off) { return *reinterpret_cast<int*>(B(base) + off); }
static inline unsigned& U(void* base, int off) { return *reinterpret_cast<unsigned*>(B(base) + off); }
template <typename T> static inline T& G(unsigned addr) { return *reinterpret_cast<T*>(addr); }

// `sub ecx, dword ptr [ecx - 4]`: the vtordisp thunks of every table below.
static inline void* VtordispThis(void* p) { return B(p) - *reinterpret_cast<int*>(B(p) - 4); }

// Call fn with `self` in ecx and the stack arguments args[0..n-1] (args[0] pushed last, as the originals push their
// copies), eax = eaxv and edx = edxv at the call: the dispatcher arms leave their own values there and a target that is a
// bare `ret N` (0x004a4340) returns that eax. The callee pops its arguments (every target here is `ret 4*n`).
static int __declspec(noinline) CallE(void* fn, void* self, int n, const int* args, int eaxv, int edxv) {
    int r;
    __asm {
        mov ecx, n
        mov edx, args
    again:
        test ecx, ecx
        jz pushed
        push dword ptr [edx + ecx*4 - 4]
        dec ecx
        jmp again
    pushed:
        mov eax, eaxv
        mov edx, edxv
        mov ecx, self
        call fn
        mov r, eax
    }
    return r;
}

typedef int(__fastcall* F0_t)(void* ecx, void* edx);
typedef int(__fastcall* F1_t)(void* ecx, void* edx, int a);
typedef int(__fastcall* F2_t)(void* ecx, void* edx, int a, int b);
typedef int(__fastcall* F4_t)(void* ecx, void* edx, int a, int b, int c, int d);

// Index of tag in the 16-entry byte table every dispatcher shares (0x004a0aa0, 0x004a0be0, 0x004a0c74, 0x004a0d98,
// 0x004a0fd8): case 0 = tag 1, 1 = tag 2, 2 = tag 4, 3 = tag 8, 4 = tag 0x10, 5 = default. `tag - 1` above 15 unsigned
// (`ja`) is the default as well. Returns the case 0..5.
static inline int TagCase(int tag) {
    unsigned i = static_cast<unsigned>(tag) - 1;                         // dec eax; cmp eax, 0xf; ja
    if (i > 15) return 5;
    static const unsigned char kMap[16] = {0, 1, 5, 2, 5, 5, 5, 3, 5, 5, 5, 5, 5, 5, 5, 4};
    return kMap[i];
}

// ---------------------------------------------------------------------------------------------------------------------
// viewDispatchSlot28 0x004a0a10 (`ret 8`, two arguments a, b), slot 28 of 0x004bb8d0. tag = [V-8] (0x004a0a10). The arms
// (jump table 0x004a0a88) are `mov eax, [esp+8]; mov edx, [esp+4]; push eax; push edx; add ecx, k; call target; ret 8`:
//   tag 1  optionDialogSlot28 0x0049fe20 on V-0x168 (0x004a0a58..0x004a0a68)
//   tag 2  ListBox::clearSelection 0x0048cac0 on V-0x1a0 (0x004a0a40..0x004a0a50)
//   tag 4  no call: `ret 8` at 0x004a0a85, eax = tag - 1
//   tag 8  0x004a4340 (a bare `ret 8`) on V-0x8c (0x004a0a70..0x004a0a80), so eax = b (the register holds the second argument)
//   tag 0x10 listPanelSlot28 0x0049e990 on V-0x180 (0x004a0a28..0x004a0a38)
// Every other tag returns eax = tag - 1 (0x004a0a85).
// 0x004a0a10  viewDispatchSlot28
F2_t ViewDispatchSlot28_orig;
int __fastcall ViewDispatchSlot28_re(void* V, void*, int a, int b) {
    int tag = I(V, -8);                                                  // 0x004a0a10
    int args[2] = {a, b};
    switch (TagCase(tag)) {
    case 0: return CallE(reinterpret_cast<void*>(0x0049fe20), B(V) - 0x168, 2, args, b, a);   // 0x004a0a58
    case 1: return CallE(reinterpret_cast<void*>(0x0048cac0), B(V) - 0x1a0, 2, args, b, a);   // 0x004a0a40
    case 3: return CallE(reinterpret_cast<void*>(0x004a4340), B(V) - 0x8c, 2, args, b, a);    // 0x004a0a70
    case 4: return CallE(reinterpret_cast<void*>(0x0049e990), B(V) - 0x180, 2, args, b, a);   // 0x004a0a28
    default: return tag - 1;                                              // 0x004a0a85 (cases 2 and 5)
    }
}

// viewDispatchSlot30 0x004a0b50 (`ret 8`, a, b), slot 30. Arms (jump table 0x004a0bc8), same shape:
//   tag 1 optionDialogUpdate 0x0049f9a0 on V-0x168 (0x004a0b98..0x004a0ba8), tag 2 ListBox::onNavKey 0x0048aa00 on V-0x1a0
//   (0x004a0b80..0x004a0b90), tag 8 0x004a43f0 on V-0x8c (0x004a0bb0..0x004a0bc0), tag 0x10 listPanelOnScroll 0x0049e230
//   on V-0x180 (0x004a0b68..0x004a0b78); tag 4 and every other tag `ret 8` with eax = tag - 1 (0x004a0bc5).
// 0x004a0b50  viewDispatchSlot30
F2_t ViewDispatchSlot30_orig;
int __fastcall ViewDispatchSlot30_re(void* V, void*, int a, int b) {
    int tag = I(V, -8);                                                  // 0x004a0b50
    int args[2] = {a, b};
    switch (TagCase(tag)) {
    case 0: return CallE(reinterpret_cast<void*>(0x0049f9a0), B(V) - 0x168, 2, args, b, a);
    case 1: return CallE(reinterpret_cast<void*>(0x0048aa00), B(V) - 0x1a0, 2, args, b, a);
    case 3: return CallE(reinterpret_cast<void*>(0x004a43f0), B(V) - 0x8c, 2, args, b, a);
    case 4: return CallE(reinterpret_cast<void*>(0x0049e230), B(V) - 0x180, 2, args, b, a);
    default: return tag - 1;                                              // 0x004a0bc5
    }
}

// viewDispatchSlot36 0x004a0bf0 (`ret 4`, one argument a), slot 36. Arms (jump table 0x004a0c5c) are `mov eax|edx, [esp+4];
// add ecx, k; push; call target; ret 4`: tag 1 optionDialogOnKey 0x0049fa90 on V-0x168 (0x004a0c2e..0x004a0c38, eax = a),
// tag 2 0x0048aea0 on V-0x1a0 (0x004a0c1b..0x004a0c25, edx = a), tag 8 0x004a4680 on V-0x8c (0x004a0c41..0x004a0c4b, edx =
// a), tag 0x10 listPanelRefresh 0x0049e450 on V-0x180 (0x004a0c08..0x004a0c13, eax = a). Tag 4 and every other tag: `xor
// eax, eax; ret 4` (0x004a0c54).
// 0x004a0bf0  viewDispatchSlot36
F1_t ViewDispatchSlot36_orig;
int __fastcall ViewDispatchSlot36_re(void* V, void*, int a) {
    int tag = I(V, -8);                                                  // 0x004a0bf0
    int args[1] = {a};
    switch (TagCase(tag)) {
    case 0: return CallE(reinterpret_cast<void*>(0x0049fa90), B(V) - 0x168, 1, args, a, 0);
    case 1: return CallE(reinterpret_cast<void*>(0x0048aea0), B(V) - 0x1a0, 1, args, 1, a);
    case 3: return CallE(reinterpret_cast<void*>(0x004a4680), B(V) - 0x8c, 1, args, 7, a);
    case 4: return CallE(reinterpret_cast<void*>(0x0049e450), B(V) - 0x180, 1, args, a, 4);
    default: return 0;                                                    // 0x004a0c54
    }
}

// viewDispatchDraw 0x004a0d30 (plain `ret`), slot 22. Arms (jump table 0x004a0d80) are `add ecx, k; jmp target`:
// tag 1 optionDialogDrawContents 0x0049fc60 on V-0x168 (0x004a0d5e), tag 2 ListBox::drawContents 0x0048c130 on V-0x1a0
// (0x004a0d53), tag 4 panelDrawRow 0x004a2d50 on V-4 (0x004a0d74), tag 8 panelSubDrawContents 0x004a45b0 on V-0x8c
// (0x004a0d69), tag 0x10 listPanelDrawContents 0x0049e680 on V-0x180 (0x004a0d48); every other tag `ret` (0x004a0d7c)
// with eax = tag - 1. The jumps leave eax = tag - 1 and edx = the case index in the target.
// 0x004a0d30  viewDispatchDraw
F0_t ViewDispatchDraw_orig;
int __fastcall ViewDispatchDraw_re(void* V, void*) {
    int tag = I(V, -8);                                                  // 0x004a0d30
    int e = tag - 1;                                                     // dec eax
    switch (TagCase(tag)) {
    case 0: return CallE(reinterpret_cast<void*>(0x0049fc60), B(V) - 0x168, 0, 0, e, 0);
    case 1: return CallE(reinterpret_cast<void*>(0x0048c130), B(V) - 0x1a0, 0, 0, e, 1);
    case 2: return CallE(reinterpret_cast<void*>(0x004a2d50), B(V) - 4, 0, 0, e, 2);
    case 3: return CallE(reinterpret_cast<void*>(0x004a45b0), B(V) - 0x8c, 0, 0, e, 3);
    case 4: return CallE(reinterpret_cast<void*>(0x0049e680), B(V) - 0x180, 0, 0, e, 4);
    default: return e;                                                    // 0x004a0d7c
    }
}

// viewDispatchOnScroll 0x004a0ed0 (`ret 8`, a, b), slots 57 and 58. eax = tag - 2 (0x004a0ed3); a tag other than 2 returns
// it (`jne 0x004a0eed`, `ret 8`); tag 2 calls ListBox::onScroll 0x0048c420 on V-0x1a0 with (a, b) (0x004a0ed8..0x004a0ee8)
// and returns its eax.
// 0x004a0ed0  viewDispatchOnScroll
F2_t ViewDispatchOnScroll_orig;
int __fastcall ViewDispatchOnScroll_re(void* V, void*, int a, int b) {
    int tag = I(V, -8);                                                  // 0x004a0ed0
    if (tag - 2 != 0) return tag - 2;                                    // 0x004a0ed3 / 0x004a0ed6
    int args[2] = {a, b};
    return CallE(reinterpret_cast<void*>(0x0048c420), B(V) - 0x1a0, 2, args, b, a);
}

// viewDispatchBuild 0x004a0ef0 (`ret 0x10`, a, b, c, d), slot of the ListModel table 0x004bb8b0: the tag is [V-0x580]
// (0x004a0ef0). Arms (jump table 0x004a0fc0): tag 1 optionDialogBuild 0x0049ed20 on V-0x168 (0x004a0f53..0x004a0f6d), tag 2
// 0x0048a5d0 on V-0x1a0 (0x004a0f31..0x004a0f4b), tag 4 0x004a2400 on V-4 (0x004a0f97..0x004a0fae), tag 8 0x004a3880 on
// V-0x8c (0x004a0f75..0x004a0f8f), tag 0x10 panelBuildRow 0x0049d6d0 on V-0x180 (0x004a0f0f..0x004a0f29); the default is
// `mov eax, 7; ret 0x10` (0x004a0fb6). Each arm re-pushes the four arguments in their original order.
// 0x004a0ef0  viewDispatchBuild
F4_t ViewDispatchBuild_orig;
int __fastcall ViewDispatchBuild_re(void* V, void*, int a, int b, int c, int d) {
    int tag = I(V, -0x580);                                              // 0x004a0ef0
    int args[4] = {a, b, c, d};
    switch (TagCase(tag)) {
    case 0: return CallE(reinterpret_cast<void*>(0x0049ed20), B(V) - 0x168, 4, args, b, a);
    case 1: return CallE(reinterpret_cast<void*>(0x0048a5d0), B(V) - 0x1a0, 4, args, b, a);
    case 2: return CallE(reinterpret_cast<void*>(0x004a2400), B(V) - 4, 4, args, b, a);
    case 3: return CallE(reinterpret_cast<void*>(0x004a3880), B(V) - 0x8c, 4, args, b, a);
    case 4: return CallE(reinterpret_cast<void*>(0x0049d6d0), B(V) - 0x180, 4, args, b, a);
    default: return 7;                                                    // 0x004a0fb6
    }
}

// viewDispatchOpen 0x004a1250 (`ret 4`, k): this = the composite c. The five parts are visited in order; the walk stops at
// the first non-zero result (`jne 0x004a12d4`) and eax is the last setKey result.
//   0x49d690(c+0x58); ListModel::setKey on [c+0x58]'s ListModel base ([vbt+8] + c+0x58) with k
//   ListBox::resetFields 0x00489e40(c); setKey on the ListModel base [vbt+8] + c + 4 (vbt = [c+4])
//   0x49ece0(c+0x78); setKey on [vbt+8] + c+0x78
//   panelOpenReset4a3790(c+0x90, k)
//   0x4a23a0(c+0x16c); setKey on [vbt+8] + c+0x16c
// 0x004a1250  viewDispatchOpen
F1_t ViewDispatchOpen_orig;
int __fastcall ViewDispatchOpen_re(void* c, void*, int k) {
    F0_t f490 = reinterpret_cast<F0_t>(0x0049d690);
    F1_t setKey = reinterpret_cast<F1_t>(0x004896b0);
    char* p58 = B(c) + 0x58;
    f490(p58, 0);                                                        // 0x004a125a
    int r = setKey(p58 + reinterpret_cast<int*>(P(p58, 0))[2], 0, k);    // 0x004a1261..0x004a126b
    if (r != 0) return r;                                                // 0x004a1272
    reinterpret_cast<F0_t>(0x00489e40)(c, 0);                            // 0x004a1276
    char* t = B(c) + reinterpret_cast<int*>(P(c, 4))[2] + 4;             // 0x004a127b..0x004a1282
    r = setKey(t, 0, k);                                                 // 0x004a1286
    if (r != 0) return r;                                                // 0x004a128d
    char* p78 = B(c) + 0x78;
    reinterpret_cast<F0_t>(0x0049ece0)(p78, 0);                          // 0x004a1294
    r = setKey(p78 + reinterpret_cast<int*>(P(p78, 0))[2], 0, k);        // 0x004a1299..0x004a12a1
    if (r != 0) return r;                                                // 0x004a12a8
    r = reinterpret_cast<F1_t>(0x004a3790)(B(c) + 0x90, 0, k);           // 0x004a12ab..0x004a12b1
    if (r != 0) return r;                                                // 0x004a12b8
    char* p16c = B(c) + 0x16c;
    reinterpret_cast<F0_t>(0x004a23a0)(p16c, 0);                         // 0x004a12c2
    return setKey(p16c + reinterpret_cast<int*>(P(p16c, 0))[2], 0, k);   // 0x004a12c7..0x004a12cf
}

// viewDispatchReset 0x004a12e0 (`ret 4`, k): the same walk with ListModel::reset 0x004897f0 instead of setKey and
// panelReset4a3860 (0x004a1341) instead of panelOpenReset4a3790.
// 0x004a12e0  viewDispatchReset
F1_t ViewDispatchReset_orig;
int __fastcall ViewDispatchReset_re(void* c, void*, int k) {
    F0_t f490 = reinterpret_cast<F0_t>(0x0049d690);
    F1_t reset = reinterpret_cast<F1_t>(0x004897f0);
    char* p58 = B(c) + 0x58;
    f490(p58, 0);                                                        // 0x004a12ea
    int r = reset(p58 + reinterpret_cast<int*>(P(p58, 0))[2], 0, k);     // 0x004a12ef..0x004a12fb
    if (r != 0) return r;                                                // 0x004a1302
    reinterpret_cast<F0_t>(0x00489e40)(c, 0);                            // 0x004a1306
    char* t = B(c) + reinterpret_cast<int*>(P(c, 4))[2] + 4;             // 0x004a130b..0x004a1312
    r = reset(t, 0, k);                                                  // 0x004a1316
    if (r != 0) return r;                                                // 0x004a131d
    char* p78 = B(c) + 0x78;
    reinterpret_cast<F0_t>(0x0049ece0)(p78, 0);                          // 0x004a1324
    r = reset(p78 + reinterpret_cast<int*>(P(p78, 0))[2], 0, k);         // 0x004a1329..0x004a1331
    if (r != 0) return r;                                                // 0x004a1338
    r = reinterpret_cast<F1_t>(0x004a3860)(B(c) + 0x90, 0, k);           // 0x004a133a..0x004a1341
    if (r != 0) return r;                                                // 0x004a1348
    char* p16c = B(c) + 0x16c;
    reinterpret_cast<F0_t>(0x004a23a0)(p16c, 0);                         // 0x004a1352
    return reset(p16c + reinterpret_cast<int*>(P(p16c, 0))[2], 0, k);    // 0x004a1357..0x004a135f
}

// listPanelSlot28 0x0049e990 (`ret 8`, a, b unused): the unnamed body behind listPanelSlot28_vtordisp. Y = this; the
// selected-row word [Y + vbt[2] + 0xd0] (= L+0xf4) and pressed-row word [Y + vbt[2] + 0xd4] (= L+0xf8) become -1 (0x0049e990..0x0049e9a6, vbt = [Y-0x24]),
// then slot +0x120 of the Window base Y - 0x24 + vbt[1] runs and its eax is returned (0x0049e9ad..0x0049e9bb).
// 0x0049e990  listPanelSlot28
F2_t ListPanelSlot28_orig;
int __fastcall ListPanelSlot28_re(void* Y, void*, int, int) {
    int* vbt = *reinterpret_cast<int**>(B(Y) - 0x24);                    // 0x0049e990
    I(Y, vbt[2] + 0xd0) = -1;                                            // 0x0049e999
    vbt = *reinterpret_cast<int**>(B(Y) - 0x24);
    I(Y, vbt[2] + 0xd4) = -1;                                            // 0x0049e9a6
    vbt = *reinterpret_cast<int**>(B(Y) - 0x24);
    void* w = B(Y) - 0x24 + vbt[1];                                      // 0x0049e9ad..0x0049e9b7
    typedef int(__fastcall * V0_t)(void*, void*);
    return (*reinterpret_cast<V0_t*>(B(P(w, 0)) + 0x120))(w, 0);         // 0x0049e9bb
}

// ---------------------------------------------------------------------------------------------------------------------
// The vtordisp thunks: `sub ecx, dword ptr [ecx - 4]` then a jmp, so the stack arguments reach the body untouched.
// ---------------------------------------------------------------------------------------------------------------------
// 0x00491960 viewDispatchDraw_vtordisp -> 0x004a0d30 (slot 22 of 0x004bb8d0)
F0_t ViewDispatchDrawVd_orig;
int __fastcall ViewDispatchDrawVd_re(void* p, void*) { return reinterpret_cast<F0_t>(0x004a0d30)(VtordispThis(p), 0); }
// 0x00491990 viewDispatchSlot28_vtordisp -> 0x004a0a10
F2_t ViewDispatchSlot28Vd_orig;
int __fastcall ViewDispatchSlot28Vd_re(void* p, void*, int a, int b) { return reinterpret_cast<F2_t>(0x004a0a10)(VtordispThis(p), 0, a, b); }
// 0x004919b0 viewDispatchSlot30_vtordisp -> 0x004a0b50
F2_t ViewDispatchSlot30Vd_orig;
int __fastcall ViewDispatchSlot30Vd_re(void* p, void*, int a, int b) { return reinterpret_cast<F2_t>(0x004a0b50)(VtordispThis(p), 0, a, b); }
// 0x00491a00 viewDispatchSlot36_vtordisp -> 0x004a0bf0
F1_t ViewDispatchSlot36Vd_orig;
int __fastcall ViewDispatchSlot36Vd_re(void* p, void*, int a) { return reinterpret_cast<F1_t>(0x004a0bf0)(VtordispThis(p), 0, a); }
// 0x00491a30 viewDispatchOnScroll_vtordisp57 -> 0x004a0ed0 (slot 57 of 0x004bb8d0)
F2_t ViewDispatchOnScrollVd57_orig;
int __fastcall ViewDispatchOnScrollVd57_re(void* p, void*, int a, int b) { return reinterpret_cast<F2_t>(0x004a0ed0)(VtordispThis(p), 0, a, b); }
// 0x00491a40 viewDispatchOnScroll_vtordisp58 -> 0x004a0ed0 (slot 58 of 0x004bb8d0)
F2_t ViewDispatchOnScrollVd58_orig;
int __fastcall ViewDispatchOnScrollVd58_re(void* p, void*, int a, int b) { return reinterpret_cast<F2_t>(0x004a0ed0)(VtordispThis(p), 0, a, b); }
// 0x00491aa0 viewDispatchBuild_vtordisp -> 0x004a0ef0
F4_t ViewDispatchBuildVd_orig;
int __fastcall ViewDispatchBuildVd_re(void* p, void*, int a, int b, int c, int d) { return reinterpret_cast<F4_t>(0x004a0ef0)(VtordispThis(p), 0, a, b, c, d); }
// 0x00491b70 listPanelSlot28_vtordisp -> 0x0049e990
F2_t ListPanelSlot28Vd_orig;
int __fastcall ListPanelSlot28Vd_re(void* p, void*, int a, int b) { return reinterpret_cast<F2_t>(0x0049e990)(VtordispThis(p), 0, a, b); }

// =====================================================================================================================
// The listPanel part (viewFieldCtor49d5a0, class with the vbptr at part+0 and a Window base + a ListModel base reached
// through its vbtable: W = part + vbt[1] (the View with the parent pointer at +0x130 and the embedded drawing object S =
// W+0x274), L = part + vbt[2] (the ListModel base). Bodies that take `Y` (= part + 0x24) read the same fields 0x24 lower.
// L fields (offsets from L): +0x28 / +0x2c offsets of the cell inside its blit, +0x38 text indent, +0x44 / +0x48 / +0x5c /
// +0x68 / +0x6c cell origin x / spacing / columns / cell width / cell height, +0x74..+0x7c the draw target for S, +0x80..
// +0xac the three quad colour sets, +0xbc a frame colour (-1 none), +0xc0 the row list {head +0xc8, cursor +0xcc, count
// +0xd0, cursor index +0xd4}, +0xf0 / +0xf4 / +0xf8 the cursor, selected and pressed rows. A list node is {+0xc next, +0x10
// previous, +8 entry}; an entry is {+4 text, +8 line count}. Part fields: +4 / +8 / +0xc the three row images, +0x18 / +0x1c
// the quad colours of the selected / pressed row, Y-0x10 / Y-0x14 the two cdecl callbacks.
// =====================================================================================================================
typedef int(__fastcall* F3_t)(void* ecx, void* edx, int a, int b, int c);
typedef int(__fastcall* F7_t)(void* ecx, void* edx, int a, int b, int c, int d, int e, int f, int g);
typedef int(__cdecl* Cdecl1_t)(int a);
typedef char*(__cdecl* Strchr_t)(const char* s, int c);

const F0_t kWidgetValue = reinterpret_cast<F0_t>(0x00477580);           // Widget_value(S), thiscall
const F4_t kSetQuad = reinterpret_cast<F4_t>(0x00476310);                // ret 0x10
const F4_t kSetQuad70 = reinterpret_cast<F4_t>(0x00476340);
const F4_t kSetQuad74 = reinterpret_cast<F4_t>(0x00476370);
const F4_t kSetDrawTarget = reinterpret_cast<F4_t>(0x004762d0);          // ret 0x10
const F7_t kBlitThunk = reinterpret_cast<F7_t>(0x00475c60);              // Surface_blitThunk, ret 0x1c
const F4_t kImageDraw = reinterpret_cast<F4_t>(0x00473e60);              // Image_draw(this = image; S, x, y, 0), ret 0x10
const F1_t kIdAt = reinterpret_cast<F1_t>(0x00489a30);                   // ListModel::idAt(L, index), ret 4
const F3_t kDrawCentered = reinterpret_cast<F3_t>(0x00477cd0);           // drawCentered(S; text, &x, len), ret 0xc
const F3_t kDrawRight2 = reinterpret_cast<F3_t>(0x004781f0);             // drawRight2(S; text, &x, len), ret 0xc
const F4_t kDrawTextLines = reinterpret_cast<F4_t>(0x004782d0);          // drawTextLines(S; text, &x, 0, 0), ret 0x10
const F2_t kDrawBoxR = reinterpret_cast<F2_t>(0x00479670);               // Widget_drawBoxR(S; &x, colour), ret 8
const F0_t kSelectedId = reinterpret_cast<F0_t>(0x00489950);             // ListModel::selectedId(L)
const F0_t kWindowInvalidate = reinterpret_cast<F0_t>(0x00480ce0);       // Window::invalidate(W)
const Strchr_t kStrchr = reinterpret_cast<Strchr_t>(0x004a6170);         // CRT strchr

// The cursor of a node list: seek to the last node. 0x0049dbb0..0x0049dbe8 (listPanelDraw), 0x0049e25a..0x0049e292
// (listPanelOnScroll). n = the count [L+0xd0]; with n - 1 >= -1 the cursor becomes head, then one step to the previous node
// (the constant |-1| is 1: `or eax, -1; cdq; xor; sub`), and the index [L+0xd4] becomes n - 1 when 1 <= n.
static void SeekLast(char* L) {
    int n = I(L, 0xd0);
    if (n - 1 < -1) return;                                              // jl
    P(L, 0xcc) = P(L, 0xc8);
    int k = 1;                                                           // abs(-1)
    if (k > n) return;                                                   // jg
    do { P(L, 0xcc) = P(P(L, 0xcc), 0x10); } while (--k != 0);           // cursor = cursor->previous
    I(L, 0xd4) = n - 1;
}

// Advance the cursor one node (circular), wrapping the index: 0x0049dc07..0x0049dc2b (and 0x0049e2d7..0x0049e2f7).
static void AdvanceCursor(char* L) {
    if (I(L, 0xc8) == 0) return;                                         // test head
    void* cur = P(L, 0xcc);
    int ix = I(L, 0xd4) + 1;
    int cnt = I(L, 0xd0);
    P(L, 0xcc) = P(cur, 0xc);
    I(L, 0xd4) = ix;
    if (ix == cnt) I(L, 0xd4) = 0;
}

// The line count of the cursor's entry: 0 with an empty list (0x0049dc53..0x0049dc72 and its twins).
static int CursorLines(char* L) {
    if (I(L, 0xc8) == 0) return 0;
    return I(P(P(L, 0xcc), 8), 8);
}
static char* CursorText(char* L) {
    if (I(L, 0xc8) == 0) return 0;
    return reinterpret_cast<char*>(P(P(P(L, 0xcc), 8), 4));
}

// The row text of listPanelDraw (0x0049dfde..0x0049e1d3) and of optionListDraw (0x0049f76b..0x0049f98f): R0 moves right by the
// indent [L+0x38]; with a one-line entry (line count 1) the text before a '|' is drawn centred and the part after it right-
// aligned (drawCentered / drawRight2); with any other count the text goes to drawTextLines (and the part after '|' to
// drawRight2). Text pointers and lengths are read from the cursor entry each time, as the original reads them.
static void RowText(char* L, char* S, int* R) {
    R[0] += I(L, 0x38);                                                  // 0x0049dfe7..0x0049dfed / 0x0049f785..0x0049f78f
    int head = I(L, 0xc8);
    char* text = CursorText(L);
    int lines2 = head != 0 ? I(P(P(L, 0xcc), 8), 8) : 0;
    char* pipe;
    if (head != 0 && lines2 == 1) {                                      // 0x0049e016 / 0x0049e019
        pipe = kStrchr(text, '|');                                       // 0x0049e033
        if (pipe == 0) {
            char* t = CursorText(L);                                     // 0x0049e041..0x0049e06d
            if (t != 0) kDrawCentered(S, 0, reinterpret_cast<int>(t), reinterpret_cast<int>(R), static_cast<int>(strlen(t)));   // 0x0049e08a
        } else {
            *pipe = 0;                                                   // 0x0049e094
            char* t = CursorText(L);
            if (t != 0) kDrawCentered(S, 0, reinterpret_cast<int>(t), reinterpret_cast<int>(R), static_cast<int>(strlen(t)));   // 0x0049e0d8
            *pipe = '|';                                                 // 0x0049e0dd
            pipe++;
            kDrawRight2(S, 0, reinterpret_cast<int>(pipe), reinterpret_cast<int>(R), static_cast<int>(strlen(pipe)));   // 0x0049e1d3
        }
    } else {                                                             // 0x0049e10b / 0x0049e11a
        pipe = kStrchr(text, '|');                                       // 0x0049e11f
        if (pipe == 0) {
            kDrawTextLines(S, 0, reinterpret_cast<int>(CursorText(L)), reinterpret_cast<int>(R), 0, 0);   // 0x0049e165
        } else {
            *pipe = 0;                                                   // 0x0049e16c
            kDrawTextLines(S, 0, reinterpret_cast<int>(CursorText(L)), reinterpret_cast<int>(R), 0, 0);   // 0x0049e1a8
            *pipe = '|';                                                 // 0x0049e1ad
            pipe++;
            kDrawRight2(S, 0, reinterpret_cast<int>(pipe), reinterpret_cast<int>(R), static_cast<int>(strlen(pipe)));   // 0x0049e1d3
        }
    }
}

// 0x0049dab0  listPanelDraw(part, index, flag): `ret 8`. Draws row `index` of the panel (flag 0: normal, selected when
// [L+0xf4] == index; flag != 0: pressed look). Nothing happens when W has no parent [W+0x130] (0x0049dac7 / 0x0049dacd).
// Otherwise: the display's slot +0x9c (1, 1, 1) when the display exists (0x0049dad3..0x0049dae5); S gets the three quad
// sets from L+0x80.. (0x49db18, 0x49db4b, 0x49db7e) and the draw target (L+0x74, +0x78, +0x7c, 0) (0x49dba2). The vertical
// offset A = 0 grows by (max(H*lines, rowH) - rowH) for each of the `index` rows passed from the last node on whose entry
// has more than one line (H = Widget_value(S)); the cell origin is X = (cellW + spacing) * (index / cols) + L[0x44] and
// Y = A + (index % cols) * (rowH + spacing) + (spacing >> 1). Rows beyond the count return (0x0049dd3d); the cursor is
// seeked to the row (negative rows count from the end). The cell height is the larger of rowH and H * lines of that row.
// The parent's slot +0x2c is called with (W, idAt(index), state, &rect): state 2 when flag, else (selected == index); a
// non-zero result ends the draw. Otherwise S is blitted onto the parent's surface (Surface_blitThunk), the row image is
// drawn in the cell (flag: image +8 and quad [part+0x1c]; selected: image +0xc and quad [part+0x18]; else image +4) and the
// text goes through drawCentered (one line) or drawTextLines (more), split at '|' with drawRight2 for the right part.
// The frame (L+0xbc) is drawn last when flag is set.
// 0x0049dab0  listPanelDraw
typedef void(__fastcall* LpDraw_t)(void* part, void* edx, int index, int flag);
LpDraw_t ListPanelDraw_orig;
void __fastcall ListPanelDraw_re(void* part, void*, int index, int flag) {
    int* vbt = *reinterpret_cast<int**>(part);                           // 0x0049dabb
    char* W = B(part) + vbt[1];
    void* ps = P(W, 0x130);                                              // 0x0049dac0
    if (ps == 0) return;                                                 // 0x0049dacd
    void* disp = G<void*>(0x0083ad50);
    if (disp != 0) {                                                     // 0x0049dad9
        typedef int(__fastcall * D3_t)(void*, void*, int, int, int);
        (*reinterpret_cast<D3_t*>(B(P(disp, 0)) + 0x9c))(disp, 0, 1, 1, 1);   // 0x0049dae5
    }
    char* L = B(part) + vbt[2];                                          // 0x0049daed
    char* S = W + 0x274;
    kSetQuad(S, 0, I(L, 0x80), I(L, 0x8c), I(L, 0x98), I(L, 0xa4));     // 0x0049db18
    kSetQuad70(S, 0, I(L, 0x84), I(L, 0x90), I(L, 0x9c), I(L, 0xa8));    // 0x0049db4b
    kSetQuad74(S, 0, I(L, 0x88), I(L, 0x94), I(L, 0xa0), I(L, 0xac));    // 0x0049db7e
    kSetDrawTarget(S, 0, I(L, 0x74), I(L, 0x78), I(L, 0x7c), 0);         // 0x0049dba2
    int A = 0;                                                           // [esp+0x10]
    SeekLast(L);                                                         // 0x0049dbb7..0x0049dbe8
    for (int cnt = index; cnt > 0; cnt--) {                              // 0x0049dbeb..0x0049dcea (count = index > 0)
        void* head = P(L, 0xc8);
        if (head != 0) AdvanceCursor(L);                                 // 0x0049dc0e
        void* cur = P(L, 0xcc);
        if (cur == 0) continue;                                          // 0x0049dc33
        void* entry = head != 0 ? P(cur, 8) : 0;                         // 0x0049dc39..0x0049dc42
        if (I(entry, 8) <= 1) continue;                                  // 0x0049dc44 / 0x0049dc48
        int lines = CursorLines(L);                                      // 0x0049dc4e..0x0049dc78
        int rowH = I(L, 0x6c);                                           // 0x0049dc80
        int prod = kWidgetValue(S, 0) * lines;                           // 0x0049dc8e / 0x0049dc93
        int v;
        if (rowH > prod) v = rowH;                                       // 0x0049dc98 / 0x0049dc9a
        else v = kWidgetValue(S, 0) * CursorLines(L);                    // 0x0049dca0..0x0049dcc9
        A += v - I(L, 0x6c);                                             // 0x0049dccc..0x0049dcdd
    }
    int cols = I(L, 0x5c), sp = I(L, 0x48);
    int q = index / cols;                                                // 0x0049dd02 idiv (faults on 0)
    int X = (I(L, 0x68) + sp) * q + I(L, 0x44);                          // 0x0049dd0a..0x0049dd12
    int Y = A + (index - q * cols) * (I(L, 0x6c) + sp) + (sp >> 1);      // 0x0049dd1f..0x0049dd32
    int n = I(L, 0xd0);
    if (index > n - 1) return;                                           // 0x0049dd3d
    P(L, 0xcc) = P(L, 0xc8);                                             // 0x0049dd49
    int dest;
    if (index < 0) {                                                     // 0x0049dd53
        int k = index < 0 ? -index : index;                              // xor / sub abs
        if (k > n) return;                                               // 0x0049dd5e
        for (int i = k; i > 0; i--) P(L, 0xcc) = P(P(L, 0xcc), 0x10);    // 0x0049dd68..0x0049dd78
        dest = n + index;                                                // 0x0049dd7a
    } else {
        dest = index;
        for (int i = index; i > 0; i--) P(L, 0xcc) = P(P(L, 0xcc), 0xc); // 0x0049dd81..0x0049dd91
    }
    I(L, 0xd4) = dest;                                                   // 0x0049dd93
    int lines = CursorLines(L);                                          // 0x0049dd99..0x0049ddc7
    int rowH = I(L, 0x6c);
    int cellH;
    if (rowH > kWidgetValue(S, 0) * lines) cellH = rowH;                 // 0x0049dde1..0x0049ddf1
    else cellH = kWidgetValue(S, 0) * CursorLines(L);                    // 0x0049ddf7..0x0049de22
    int R[4] = {X, Y, X + I(L, 0x68), Y + cellH};                        // 0x0049de32..0x0049de4c
    int state = flag ? 2 : (I(L, 0xf4) == index ? 1 : 0);                // 0x0049de4e / 0x0049de55..0x0049de67
    int id = kIdAt(L, 0, index);                                         // 0x0049de95
    typedef int(__fastcall * Row_t)(void*, void*, int, int, int, int);
    int handled = (*reinterpret_cast<Row_t*>(B(P(ps, 0)) + 0x2c))(ps, 0, reinterpret_cast<int>(W), id, state,
                                                                  reinterpret_cast<int>(R));   // 0x0049dea8
    if (handled == 0) {                                                  // 0x0049dead
        kBlitThunk(B(ps) + 0x274, 0, reinterpret_cast<int>(S), X + I(L, 0x28), Y + I(L, 0x2c), X, Y, I(L, 0x68), cellH);   // 0x0049deed
        void* imgA = P(part, 4);
        void* imgB = P(part, 8);
        void* imgC = P(part, 0xc);
        if (imgB != 0 && imgA != 0) {                                    // 0x0049def5 / 0x0049defd
            if (flag != 0) {                                             // 0x0049df0c
                kImageDraw(imgB, 0, reinterpret_cast<int>(S), X, Y + ((R[3] - I(imgB, 0x1c) - R[1]) >> 1), 0);   // 0x0049df3e
                kSetQuad(S, 0, I(part, 0x1c), -1, 2, 2);                 // 0x0049dfd9
            } else if (I(L, 0xf4) != index) {                            // 0x0049df57 / 0x0049df5e
                kImageDraw(imgA, 0, reinterpret_cast<int>(S), X, Y + ((R[3] - I(imgA, 0x1c) - R[1]) >> 1), 0);   // 0x0049df8a
            } else {
                kImageDraw(imgC, 0, reinterpret_cast<int>(S), X, Y + ((R[3] - I(imgC, 0x1c) - R[1]) >> 1), 0);   // 0x0049dfbe
                kSetQuad(S, 0, I(part, 0x18), -1, 2, 2);                 // 0x0049dfd9
            }
        }
    }
    RowText(L, S, R);
    if (I(L, 0xbc) != -1 && flag != 0) {                                 // 0x0049e1dd / 0x0049e1eb
        R[0] -= 2;                                                       // 0x0049e1f3
        kDrawBoxR(S, 0, reinterpret_cast<int>(R), I(L, 0xbc));           // 0x0049e214
    }
}


// 0x0049e680  listPanelDrawContents(Y): plain `ret`. Y = part + 0x24; W = part + vbt[1] (vbt = [part]). child = [W+0x130]
// (the dword at Y + vbt[1] + 0x10c, 0x0049e68f); it goes to the global g_curWidget 0x0083ab2c (0x0049e698) and a NULL
// child returns at once with eax = vbt (the dword read at 0x0049e686). Otherwise child's slot +0x28 gets W (0x0049e6bf); a
// zero result blits W's object S onto the child: the extents are the child-side object's slots +0xdc / +0xd8 (when [W+0x278]
// is set, else 0 / 0) and the call is Surface_blitThunk(child+0x274; S, [L+0x28], [L+0x2c], 0, 0, w, h) (0x0049e74e). Then
// listPanelDraw(part, i, [L+0xf0] == i) runs for i in 0 .. [L+0xd0]-1 (the bound is re-read each round, 0x0049e786) and
// Window::invalidate(W) ends it; eax is that call's.
// 0x0049e680  listPanelDrawContents
F0_t ListPanelDrawContents_orig;
int __fastcall ListPanelDrawContents_re(void* Y, void*) {
    char* part = B(Y) - 0x24;
    int* vbt = *reinterpret_cast<int**>(part);                           // 0x0049e686
    char* W = part + vbt[1];
    void* child = P(W, 0x130);                                           // 0x0049e68f
    G<void*>(0x0083ab2c) = child;                                        // 0x0049e698
    if (child == 0) return reinterpret_cast<int>(vbt);                   // 0x0049e69e
    char* L = part + vbt[2];
    char* S = W + 0x274;
    typedef int(__fastcall * V1_t)(void*, void*, int);
    typedef int(__fastcall * V0_t)(void*, void*);
    int r = (*reinterpret_cast<V1_t*>(B(P(child, 0)) + 0x28))(child, 0, reinterpret_cast<int>(W));   // 0x0049e6bf
    if (r == 0) {                                                        // 0x0049e6c4
        void* o = P(W, 0x274 + 4);                                       // [W+0x278], 0x0049e6f0
        int h = o != 0 ? (*reinterpret_cast<V0_t*>(B(P(o, 0)) + 0xdc))(o, 0) : 0;   // 0x0049e6fd
        void* o2 = P(W, 0x274 + 4);                                      // 0x0049e715
        int w = o2 != 0 ? (*reinterpret_cast<V0_t*>(B(P(o2, 0)) + 0xd8))(o2, 0) : 0;   // 0x0049e722
        kBlitThunk(B(child) + 0x274, 0, reinterpret_cast<int>(S), I(L, 0x28), I(L, 0x2c), 0, 0, w, h);   // 0x0049e74e
    }
    for (int i = 0; I(L, 0xd0) > 0; ) {                                  // 0x0049e75d..0x0049e766
        reinterpret_cast<LpDraw_t>(0x0049dab0)(part, 0, i, I(L, 0xf0) == i ? 1 : 0);   // 0x0049e779
        i++;
        if (!(i < I(L, 0xd0))) break;                                    // 0x0049e786 / 0x0049e78c
    }
    return kWindowInvalidate(W, 0);                                      // 0x0049e797
}

// 0x0049e230  listPanelOnScroll(Y, a, b): `ret 8`. Moves the cursor row [L+0xf0] after a scroll by `a` pixels, consuming
// `b` pixels of rows from the last list node on (see listPanelDraw): the list cursor is seeked to the last node, then while
// b > 0 it advances one node per round (the first round always runs; later rounds stop when [L+0xd4] == count-1) and b
// drops by max(H * lines, rowH + spacing) (H = Widget_value(S), 0x0049e320..0x0049e33b). target = (a / (cellW + spacing)) *
// cols + [L+0xd4]. When target equals [L+0xf0] the cdecl callback at Y-0x10 gets ListModel::selectedId(L) (0x0049e374..
// 0x0049e387); otherwise listPanel row [L+0xf0] is redrawn unselected, [L+0xf0] = target, the callback at Y-0x14 gets it,
// the target row is drawn selected, [L+0xf4] = [L+0xf8] = -1 and Window::invalidate(W) runs. If bit 2 of the byte at L+0x24 is
// set and the child [W+0x130] exists its slot +0xd4 gets -1 (both ends, 0x0049e3b1 / 0x0049e43f). eax is the last call's.
// 0x0049e230  listPanelOnScroll
F2_t ListPanelOnScroll_orig;
int __fastcall ListPanelOnScroll_re(void* Y, void*, int a, int b) {
    char* part = B(Y) - 0x24;
    int* vbt = *reinterpret_cast<int**>(part);                           // 0x0049e23f
    char* W = part + vbt[1];
    G<void*>(0x0083ab2c) = P(W, 0x130);                                  // 0x0049e24f
    char* L = part + vbt[2];
    char* S = W + 0x274;
    SeekLast(L);                                                         // 0x0049e25a..0x0049e292
    int f = 1;                                                           // [esp+0x10]
    int rest = b;                                                        // ebp
    while (rest > 0) {                                                   // 0x0049e29b
        if (f == 0 && I(L, 0xd4) == I(L, 0xd0) - 1) return reinterpret_cast<int>(L + 0x24);   // 0x0049e2a7..0x0049e2c1 (eax = lea [ecx+esi], 0x0049e2b7)
        f = 0;
        AdvanceCursor(L);                                                // 0x0049e2d7..0x0049e2f7 (head test 0x0049e2de)
        int lines = 0;
        if (P(L, 0xcc) != 0) {                                           // 0x0049e2fd
            lines = I(L, 0xc8) != 0 ? I(P(P(L, 0xcc), 8), 8) : I(0, 8);  // 0x0049e301..0x0049e30c
        }
        int cons = kWidgetValue(S, 0) * lines;                           // 0x0049e31b / 0x0049e320
        int minc = I(L, 0x48) + I(L, 0x6c);                              // 0x0049e32e..0x0049e331
        if (minc > cons) cons = minc;                                    // 0x0049e333 / 0x0049e335
        rest -= cons;                                                    // 0x0049e339
    }
    int q = a / (I(L, 0x68) + I(L, 0x48));                               // 0x0049e35f..0x0049e360 idiv
    int target = q * I(L, 0x5c) + I(L, 0xd4);                            // 0x0049e36a..0x0049e36e
    int cur = I(L, 0xf0);
    void* child = P(W, 0x130);
    typedef int(__fastcall * V1_t)(void*, void*, int);
    if (cur == target) {                                                 // 0x0049e370
        Cdecl1_t cb = reinterpret_cast<Cdecl1_t>(P(Y, -0x10));           // 0x0049e374
        int r0 = 0;                                                      // eax = the callback slot just read when it is NULL
        if (cb != 0) r0 = cb(kSelectedId(L, 0));                         // 0x0049e37e..0x0049e384
        if ((*reinterpret_cast<unsigned char*>(L + 0x24) & 4) == 0) return r0;   // 0x0049e38f
        if (child == 0) return r0;                                       // 0x0049e3a3
        {   // the original enters the slot with eax = the child's vtable, edx = vbt[1] (0x0049e399 / 0x0049e3ab): a callee
            // that is a bare `ret 4` returns that eax
            int arg = -1;
            return CallE(*reinterpret_cast<void**>(B(P(child, 0)) + 0xd4), child, 1, &arg, reinterpret_cast<int>(P(child, 0)), vbt[1]);   // 0x0049e3b1
        }
    }
    reinterpret_cast<LpDraw_t>(0x0049dab0)(part, 0, cur, 0);             // 0x0049e3c4
    I(L, 0xf0) = target;                                                 // 0x0049e3ce
    Cdecl1_t cb2 = reinterpret_cast<Cdecl1_t>(P(Y, -0x14));              // 0x0049e3d5
    if (cb2 != 0) cb2(I(L, 0xf0));                                       // 0x0049e3dc..0x0049e3eb
    reinterpret_cast<LpDraw_t>(0x0049dab0)(part, 0, target, 1);          // 0x0049e3f3
    I(L, 0xf4) = -1;                                                     // 0x0049e400
    I(L, 0xf8) = -1;                                                     // 0x0049e40c
    int r = kWindowInvalidate(W, 0);                                     // 0x0049e41c
    if ((*reinterpret_cast<unsigned char*>(L + 0x24) & 4) == 0) return r;   // 0x0049e426
    child = P(W, 0x130);                                                 // 0x0049e436
    if (child == 0) return vbt[1];                                       // 0x0049e438 (eax = [edi+4] = vbt[1], loaded at 0x0049e42c)
    {   // eax = vbt[1] ([edi+4], 0x0049e42c), edx = the child's vtable (0x0049e43a)
        int arg = -1;
        return CallE(*reinterpret_cast<void**>(B(P(child, 0)) + 0xd4), child, 1, &arg, vbt[1], reinterpret_cast<int>(P(child, 0)));   // 0x0049e43f
    }
}

// 0x0049e450  listPanelRefresh(Y, key): `ret 4`. g_curWidget = [W+0x130] (0x0049e467); key - 0x25 above 0x43 returns 0
// (0x0049e5d2). The byte table 0x0049e5f0 maps key 0x25 / 0x64 -> case 0 (cursor [L+0xf0] += cols, undone when it reaches the
// count), 0x26 / 0x68 -> case 1 (cursor - 1, clamped at 0), 0x27 / 0x66 -> case 2 (cursor - cols, +cols when negative), 0x28 /
// 0x62 -> case 3 (cursor + 1, clamped at count - 1); every other key returns 0. Then [L+0xf4] = [L+0xf8] = -1, the previously
// selected row (when not -1) and the old cursor row are redrawn unselected, the callback at Y-0x14 gets the new cursor, the
// new row is drawn selected, Window::invalidate(W) runs and eax = 1.
// 0x0049e450  listPanelRefresh
F1_t ListPanelRefresh_orig;
int __fastcall ListPanelRefresh_re(void* Y, void*, int key) {
    char* part = B(Y) - 0x24;
    int* vbt = *reinterpret_cast<int**>(part);                           // 0x0049e457
    char* W = part + vbt[1];
    G<void*>(0x0083ab2c) = P(W, 0x130);                                  // 0x0049e467
    char* L = part + vbt[2];
    unsigned idx = static_cast<unsigned>(key - 0x25);                    // lea ebp, [edx - 0x25]
    int prev = I(L, 0xf4);                                               // 0x0049e479
    int old = I(L, 0xf0);                                                // 0x0049e486
    static const unsigned char kMap[0x44] = {
        0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 0, 4, 2, 4, 1};   // 0x0049e5f0
    if (idx > 0x43) return 0;                                            // 0x0049e490
    switch (kMap[idx]) {
    case 1:                                                              // 0x0049e4a5
        I(L, 0xf0) = old - 1;
        if (I(L, 0xf0) <= 0) I(L, 0xf0) = 0;                             // setle / dec / and
        break;
    case 3:                                                              // 0x0049e4d0
        I(L, 0xf0) = old + 1;
        {
            int c = I(L, 0xf0), top = I(L, 0xd0) - 1;
            I(L, 0xf0) = c < top ? c : top;                              // 0x0049e4ee..0x0049e4f4
        }
        break;
    case 0:                                                              // 0x0049e4fc
        I(L, 0xf0) = I(L, 0x5c) + old;
        if (I(L, 0xf0) >= I(L, 0xd0)) I(L, 0xf0) = I(L, 0xf0) - I(L, 0x5c);   // 0x0049e51d / 0x0049e521
        break;
    case 2:                                                              // 0x0049e52c
        I(L, 0xf0) = old - I(L, 0x5c);
        if (I(L, 0xf0) < 0) I(L, 0xf0) = I(L, 0x5c) + I(L, 0xf0);        // 0x0049e544 / 0x0049e54b
        break;
    default:
        return 0;                                                        // 0x0049e5d2
    }
    I(L, 0xf4) = -1;                                                     // 0x0049e55b
    I(L, 0xf8) = -1;                                                     // 0x0049e567
    if (prev != -1) reinterpret_cast<LpDraw_t>(0x0049dab0)(part, 0, prev, 0);   // 0x0049e575
    reinterpret_cast<LpDraw_t>(0x0049dab0)(part, 0, old, 0);             // 0x0049e583
    Cdecl1_t cb2 = reinterpret_cast<Cdecl1_t>(P(Y, -0x14));              // 0x0049e588
    if (cb2 != 0) cb2(I(L, 0xf0));                                       // 0x0049e58d..0x0049e59e
    reinterpret_cast<LpDraw_t>(0x0049dab0)(part, 0, I(L, 0xf0), 1);      // 0x0049e5b2
    kWindowInvalidate(W, 0);                                             // 0x0049e5c0
    return 1;                                                            // 0x0049e5c8
}

// The vtordisp thunks of this part: `sub ecx, dword ptr [ecx - 4]; jmp body`.
// 0x00491b50 listPanelDrawContents_vtordisp -> 0x0049e680 (slot 22 of 0x004bb744)
F0_t ListPanelDrawContentsVd_orig;
int __fastcall ListPanelDrawContentsVd_re(void* p, void*) { return reinterpret_cast<F0_t>(0x0049e680)(VtordispThis(p), 0); }
// 0x00491b80 listPanelOnScroll_vtordisp -> 0x0049e230
F2_t ListPanelOnScrollVd_orig;
int __fastcall ListPanelOnScrollVd_re(void* p, void*, int a, int b) { return reinterpret_cast<F2_t>(0x0049e230)(VtordispThis(p), 0, a, b); }
// 0x00491b90 listPanelRefresh_vtordisp -> 0x0049e450
F1_t ListPanelRefreshVd_orig;
int __fastcall ListPanelRefreshVd_re(void* p, void*, int a) { return reinterpret_cast<F1_t>(0x0049e450)(VtordispThis(p), 0, a); }

// =====================================================================================================================
// The optionDialog part (viewFieldCtor49ebb0): vbtable 0x004bc01c standalone, Window base at part+0x1c, ListModel base at
// part+0x594; W = part + vbt[1], L = part + vbt[2] as for the listPanel. Bodies that take X (= part + 0x1c) read the L
// fields 0x1c lower. Part fields: +4 the current row index, +8 / +0xc the two cdecl callbacks (X-0x14 / X-0x10), +0x10 /
// +0x14 the two row images (normal / state). L+0xf0 is the bit mask the bitTest / flagIsSet / bitSet family reads.
// =====================================================================================================================
const F1_t kBitTest = reinterpret_cast<F1_t>(0x0049f030);                // bitTest(part; bit), ret 4
const F1_t kFlagToggleEe90 = reinterpret_cast<F1_t>(0x0049ee90);         // ret 4
const F1_t kFlagToggleEec0 = reinterpret_cast<F1_t>(0x0049eec0);         // ret 4
const F1_t kFlagIsSet = reinterpret_cast<F1_t>(0x0049ef80);              // flagIsSet(part; id), ret 4
const F1_t kFindId = reinterpret_cast<F1_t>(0x004899d0);                 // ListModel::findId(L; id), ret 4
const F0_t kListIterValue = reinterpret_cast<F0_t>(0x00402160);          // listIterValue(list): the cursor entry, or 0
const F0_t kOptionLayout = reinterpret_cast<F0_t>(0x0049f050);           // optionDialogLayout(part)
typedef int(__fastcall* F10_t)(void* ecx, void* edx, int a, int b, int c, int d, int e, int f, int g, int h, int i);

// 0x0049f370  optionListDraw(part, index, state, unused): `ret 0xc`. The third argument is never read. The same draw as
// listPanelDraw (rows seeked from the last node, cell X / Y from L+0x44 / 0x48 / 0x5c / 0x68 / 0x6c, the parent's slot +0x2c
// called with (W, idAt(index), state, &rect)), without the three quad sets and the draw target, with the rows' line-count
// test through listIterValue 0x00402160 (0x0049f43b), the two images at part+0x14 (state != 0) / part+0x10, no quad
// colours, Y advanced by [[W+0x2d0]+0x1c] (the draw target's +0x1c, 0x0049f76f..0x0049f77e) before the text and no frame.
// 0x0049f370  optionListDraw
typedef int(__fastcall* OlDraw_t)(void* part, void* edx, int index, int state, int unused);
OlDraw_t OptionListDraw_orig;
int __fastcall OptionListDraw_re(void* part, void*, int index, int state, int) {
    int* vbt = *reinterpret_cast<int**>(part);                           // 0x0049f37b
    char* W = B(part) + vbt[1];
    void* ps = P(W, 0x130);                                              // 0x0049f380
    if (ps == 0) return 0;                                               // 0x0049f389 / 0x0049f38d (eax = ps = 0)
    void* disp = G<void*>(0x0083ad50);
    if (disp != 0) {                                                     // 0x0049f399
        typedef int(__fastcall * D3_t)(void*, void*, int, int, int);
        (*reinterpret_cast<D3_t*>(B(P(disp, 0)) + 0x9c))(disp, 0, 1, 1, 1);   // 0x0049f3a5
    }
    char* L = B(part) + vbt[2];                                          // 0x0049f3b1
    char* S = W + 0x274;
    int A = 0;
    SeekLast(L);                                                         // 0x0049f3b4..0x0049f3ec
    for (int cnt = index; cnt > 0; cnt--) {                              // 0x0049f3ef..0x0049f4e8
        void* head = P(L, 0xc8);
        if (head != 0) AdvanceCursor(L);                                 // 0x0049f40b..0x0049f42f
        if (P(L, 0xcc) == 0) continue;                                   // 0x0049f432
        int* entry = reinterpret_cast<int*>(kListIterValue(L + 0xc0, 0));   // 0x0049f43b
        if (I(entry, 8) <= 1) continue;                                  // 0x0049f440 / 0x0049f444
        int lines = CursorLines(L);                                      // 0x0049f44a..0x0049f470
        int rowH = I(L, 0x6c);                                           // 0x0049f47c
        int prod = kWidgetValue(S, 0) * lines;                           // 0x0049f48a / 0x0049f48f
        int v;
        if (rowH > prod) v = rowH;                                       // 0x0049f494 / 0x0049f496
        else v = kWidgetValue(S, 0) * CursorLines(L);                    // 0x0049f49c..0x0049f4c5
        A += v - I(L, 0x6c);                                             // 0x0049f4c8..0x0049f4db
    }
    int cols = I(L, 0x5c), sp = I(L, 0x48);
    int q = index / cols;                                                // 0x0049f4ff
    int X = (I(L, 0x68) + sp) * q + I(L, 0x44);                          // 0x0049f507..0x0049f512
    int Y = A + (index - q * cols) * (I(L, 0x6c) + sp) + (sp >> 1);      // 0x0049f51c..0x0049f532
    int n = I(L, 0xd0);
    if (index > n - 1) return n - 1;                                     // 0x0049f537 / 0x0049f53d (eax = n - 1)
    P(L, 0xcc) = P(L, 0xc8);                                             // 0x0049f543
    int dest;
    if (index < 0) {                                                     // 0x0049f553
        int k = index < 0 ? -index : index;
        if (k > n) return k;                                             // 0x0049f55e (eax = |index|)
        for (int i = k; i > 0; i--) P(L, 0xcc) = P(P(L, 0xcc), 0x10);   // 0x0049f568..0x0049f578
        dest = n + index;                                                // 0x0049f57a
    } else {
        dest = index;
        for (int i = index; i > 0; i--) P(L, 0xcc) = P(P(L, 0xcc), 0xc); // 0x0049f581..0x0049f591
    }
    I(L, 0xd4) = dest;                                                   // 0x0049f593
    int lines = CursorLines(L);                                          // 0x0049f599..0x0049f5c7
    int rowH = I(L, 0x6c);
    int cellH;
    if (rowH > kWidgetValue(S, 0) * lines) cellH = rowH;                 // 0x0049f5e1..0x0049f5f1
    else cellH = kWidgetValue(S, 0) * CursorLines(L);                    // 0x0049f5f7..0x0049f622
    int R[4] = {X, Y, X + I(L, 0x68), Y + cellH};                        // 0x0049f632..0x0049f648
    int st = state != 0 ? 2 : (I(L, 0xf4) == index ? 1 : 0);             // 0x0049f64c / 0x0049f655..0x0049f667
    int id = kIdAt(L, 0, index);                                         // 0x0049f695
    typedef int(__fastcall * Row_t)(void*, void*, int, int, int, int);
    int handled = (*reinterpret_cast<Row_t*>(B(P(ps, 0)) + 0x2c))(ps, 0, reinterpret_cast<int>(W), id, st,
                                                                  reinterpret_cast<int>(R));   // 0x0049f6a8
    if (handled == 0) {                                                  // 0x0049f6ad
        kBlitThunk(B(ps) + 0x274, 0, reinterpret_cast<int>(S), X + I(L, 0x28), Y + I(L, 0x2c), X, Y, I(L, 0x68), cellH);   // 0x0049f6ed
        void* imgState = P(part, 0x14);                                  // 0x0049f6f2
        void* imgNorm = P(part, 0x10);                                   // 0x0049f6f9
        if (imgState != 0 && imgNorm != 0) {                             // 0x0049f6f5 / 0x0049f6fc
            void* img = state != 0 ? imgState : imgNorm;                 // 0x0049f704
            kImageDraw(img, 0, reinterpret_cast<int>(S), X, Y + ((R[3] - I(img, 0x1c) - R[1]) >> 1), 0);   // 0x0049f736 / 0x0049f764
        }
    }
    void* tgt = P(W, 0x274 + 0x5c);                                      // [W+0x2d0], 0x0049f772
    R[1] += I(tgt, 0x1c);                                                // 0x0049f779..0x0049f77e
    RowText(L, S, R);                                                    // 0x0049f785..0x0049f98f
    return 0;
}

// 0x0049f9a0  optionDialogUpdate(X, a, b): `ret 8`: a click / move at pixel (a, b). X = part + 0x1c. child [W+0x130] goes to
// g_curWidget (0x0049f9b6). idx = (a / (cellW + spacing)) * cols + b / (rowH + spacing) (L+0x68, 0x48, 0x5c, 0x6c,
// 0x0049f9c8..0x0049f9e9); id = ListModel::idAt(idx); flagToggle49ee90(part, id). When idAt(current row [part+4]) != id the
// old row is redrawn (optionListDraw(cur, bitTest(cur), 0)) and [part+4] = findId(id) (0x0049fa0d..0x0049fa39). The callback at
// part+8 gets [L+0xf0] (0x0049fa43..0x0049fa50); the row of `id` is drawn with (findId(id), flagIsSet(id), 1) and
// Window::invalidate(W) ends it (eax = its result).
// 0x0049f9a0  optionDialogUpdate
F2_t OptionDialogUpdate_orig;
int __fastcall OptionDialogUpdate_re(void* X, void*, int a, int b) {
    char* part = B(X) - 0x1c;
    int* vbt = *reinterpret_cast<int**>(part);                           // 0x0049f9a6
    char* W = part + vbt[1];
    G<void*>(0x0083ab2c) = P(W, 0x130);                                  // 0x0049f9b6
    char* L = part + vbt[2];
    int sp = I(L, 0x48);
    int q = a / (I(L, 0x68) + sp);                                       // 0x0049f9c8..0x0049f9d3
    int idx = q * I(L, 0x5c) + b / (I(L, 0x6c) + sp);                    // 0x0049f9d5..0x0049f9e9
    int id = kIdAt(L, 0, idx);                                           // 0x0049f9ec
    kFlagToggleEe90(part, 0, id);                                        // 0x0049f9f6
    int cur = I(part, 4);                                                // [esi-0x18]
    if (kIdAt(L, 0, cur) != id) {                                        // 0x0049fa08 / 0x0049fa0d
        int bt = kBitTest(part, 0, I(part, 4));                          // 0x0049fa19
        reinterpret_cast<OlDraw_t>(0x0049f370)(part, 0, I(part, 4), bt, 0);   // 0x0049fa25
        I(part, 4) = kFindId(L, 0, id);                                  // 0x0049fa34 / 0x0049fa39
    }
    Cdecl1_t cb = reinterpret_cast<Cdecl1_t>(P(part, 8));                // 0x0049fa3c
    if (cb != 0) cb(I(L, 0xf0));                                         // 0x0049fa43..0x0049fa52
    int fl = kFlagIsSet(part, 0, id);                                    // 0x0049fa5a
    int idx2 = kFindId(L, 0, id);                                        // 0x0049fa6a
    reinterpret_cast<OlDraw_t>(0x0049f370)(part, 0, idx2, fl, 1);        // 0x0049fa72
    return kWindowInvalidate(W, 0);                                      // 0x0049fa80
}

// 0x0049fa90  optionDialogOnKey(X, key): `ret 4`. g_curWidget = [W+0x130] (0x0049faa8); the current row [part+4] is redrawn with
// (cur, bitTest(cur), 0) (0x0049fabe). key - 0x20 above 0x48 returns 0. Byte table 0x0049fbdc: key 0x20 toggles the bit of
// the current row (flagToggle49eec0, 0x0049fb6e); 0x25 / 0x64 cursor + cols (undone at the count); 0x26 / 0x68 cursor - 1
// (clamped at 0); 0x27 / 0x66 cursor - cols (+cols when negative); 0x28 / 0x62 cursor + 1 (clamped at count - 1); every other
// key returns 0 (0x0049fbbd). Then the callback at part+8 gets [L+0xf0], the row is drawn (cur, bitTest(cur), 1),
// Window::invalidate(W) runs and eax = 1.
// 0x0049fa90  optionDialogOnKey
F1_t OptionDialogOnKey_orig;
int __fastcall OptionDialogOnKey_re(void* X, void*, int key) {
    char* part = B(X) - 0x1c;
    int* vbt = *reinterpret_cast<int**>(part);                           // 0x0049fa96
    char* W = part + vbt[1];
    G<void*>(0x0083ab2c) = P(W, 0x130);                                  // 0x0049faa8
    char* L = part + vbt[2];
    int bt = kBitTest(part, 0, I(part, 4));                              // 0x0049fab2
    reinterpret_cast<OlDraw_t>(0x0049f370)(part, 0, I(part, 4), bt, 0);  // 0x0049fabe
    unsigned idx = static_cast<unsigned>(key - 0x20);                    // lea eax, [edx - 0x20]
    static const unsigned char kMap[0x49] = {
        0, 5, 5, 5, 5, 1, 2, 3, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 5, 1, 5, 3, 5, 2};   // 0x0049fbdc
    if (idx > 0x48) return 0;                                            // 0x0049facd
    switch (kMap[idx]) {
    case 2: {                                                            // 0x0049fae2
        int c = I(part, 4) - 1;
        I(part, 4) = c;
        I(part, 4) = c <= 0 ? 0 : c;                                     // setle / dec / and
        break;
    }
    case 4: {                                                            // 0x0049fafa
        int c = I(part, 4) + 1;
        I(part, 4) = c;
        int top = I(L, 0xd0) - 1;
        I(part, 4) = c < top ? c : top;                                  // 0x0049fb0e..0x0049fb14
        break;
    }
    case 1: {                                                            // 0x0049fb19
        int c = I(part, 4) + I(L, 0x5c);
        I(part, 4) = c;
        if (c >= I(L, 0xd0)) I(part, 4) = c - I(L, 0x5c);                // 0x0049fb39 / 0x0049fb3b / 0x0049fb3d
        break;
    }
    case 3: {                                                            // 0x0049fb45
        int c = I(part, 4) - I(L, 0x5c);
        I(part, 4) = c;
        if (c < 0) I(part, 4) = I(L, 0x5c) + c;                          // 0x0049fb56 / 0x0049fb61
        break;
    }
    case 0:                                                              // 0x0049fb68
        kFlagToggleEec0(part, 0, I(part, 4));                            // 0x0049fb6e
        break;
    default:
        return 0;                                                        // 0x0049fbbd
    }
    Cdecl1_t cb = reinterpret_cast<Cdecl1_t>(P(part, 8));                // 0x0049fb73
    if (cb != 0) cb(I(L, 0xf0));                                         // 0x0049fb7a..0x0049fb89
    int bt2 = kBitTest(part, 0, I(part, 4));                             // 0x0049fb94
    reinterpret_cast<OlDraw_t>(0x0049f370)(part, 0, I(part, 4), bt2, 1); // 0x0049fba0
    kWindowInvalidate(W, 0);                                             // 0x0049fbae
    return 1;                                                            // 0x0049fbb4
}

// 0x0049fc60  optionDialogDrawContents(X): plain `ret`. As listPanelDrawContents: child [W+0x130] to g_curWidget; none returns
// with eax = vbt; the child's slot +0x28 gets W, a zero result blits W's S onto the child (extents from the object at
// [W+0x278] slots +0xdc / +0xd8, Surface_blitThunk(child+0x274; S, [L+0x28], [L+0x2c], 0, 0, w, h)); then optionListDraw(i,
// bitTest(i), [part+4] == i) for i in 0 .. [L+0xd0]-1 (the bound is re-read; eax = vbt after the loop, the count when it is
// not positive).
// 0x0049fc60  optionDialogDrawContents
F0_t OptionDialogDrawContents_orig;
int __fastcall OptionDialogDrawContents_re(void* X, void*) {
    char* part = B(X) - 0x1c;
    int* vbt = *reinterpret_cast<int**>(part);                           // 0x0049fc67
    char* W = part + vbt[1];
    void* child = P(W, 0x130);                                           // 0x0049fc70
    if (child == 0) return reinterpret_cast<int>(vbt);                   // 0x0049fc77 / 0x0049fc79
    G<void*>(0x0083ab2c) = child;                                        // 0x0049fc82
    char* L = part + vbt[2];
    char* S = W + 0x274;
    typedef int(__fastcall * V1_t)(void*, void*, int);
    typedef int(__fastcall * V0_t)(void*, void*);
    int r = (*reinterpret_cast<V1_t*>(B(P(child, 0)) + 0x28))(child, 0, reinterpret_cast<int>(W));   // 0x0049fc9e
    if (r == 0) {                                                        // 0x0049fca3
        void* o = P(W, 0x274 + 4);                                       // [W+0x278], 0x0049fccf
        int h = o != 0 ? (*reinterpret_cast<V0_t*>(B(P(o, 0)) + 0xdc))(o, 0) : 0;   // 0x0049fcdc
        void* o2 = P(W, 0x274 + 4);                                      // 0x0049fcf4
        int w = o2 != 0 ? (*reinterpret_cast<V0_t*>(B(P(o2, 0)) + 0xd8))(o2, 0) : 0;   // 0x0049fd01
        kBlitThunk(B(child) + 0x274, 0, reinterpret_cast<int>(S), I(L, 0x28), I(L, 0x2c), 0, 0, w, h);   // 0x0049fd2d
    }
    int count = I(L, 0xd0);                                              // 0x0049fd39
    if (count <= 0) return count;                                        // 0x0049fd42 (eax = the count)
    for (int i = 0;;) {
        int bt = kBitTest(part, 0, i);                                   // 0x0049fd52
        reinterpret_cast<OlDraw_t>(0x0049f370)(part, 0, i, bt, I(part, 4) == i ? 1 : 0);   // 0x0049fd5b (the state flag is pushed first)
        i++;
        if (!(i < I(L, 0xd0))) break;                                    // 0x0049fd66 / 0x0049fd6d
    }
    return reinterpret_cast<int>(vbt);                                   // eax = [edi] (0x0049fd60)
}

// 0x0049ff60  optionDialogDeletingDtor(X, flags): `ret 4`. part = X - 0x1c; viewFieldDtor49ec80(X) (0x0049ff6a),
// ListModel::dtor on part+0x594 (0x0049ff75), Base4805a0::dtor(X) (0x0049ff7c); with bit 0 of flags the block `part` goes to
// operator delete 0x004a4ffc (0x0049ff89). eax = part (0x0049ff91).
// 0x0049ff60  optionDialogDeletingDtor
F1_t OptionDialogDeletingDtor_orig;
int __fastcall OptionDialogDeletingDtor_re(void* X, void*, int flags) {
    char* part = B(X) - 0x1c;
    reinterpret_cast<F0_t>(0x0049ec80)(X, 0);                            // 0x0049ff6a
    reinterpret_cast<F0_t>(0x00489370)(part + 0x594, 0);                 // 0x0049ff75
    reinterpret_cast<F0_t>(0x004805a0)(X, 0);                            // 0x0049ff7c
    if (flags & 1) reinterpret_cast<void(__cdecl*)(void*)>(0x004a4ffc)(part);   // 0x0049ff89
    return reinterpret_cast<int>(part);                                  // 0x0049ff91
}

// 0x0049ed20  optionDialogBuild(T, a, b, c, d): `ret 0x10`; T = part + 0x594 (the ListModel-table `this`). a = parent window
// (NULL returns 3, 0x0049ed27..0x0049ed35), b / c the position, d the flags. optionDialogLayout(part) runs; L+0x28 = b and
// L+0x2c = c (0x0049ed4f / 0x0049ed5f); without bit 12 of [a+0x9c] d gets 0x100000 (0x0049ed66..0x0049ed71). Window::create(W;
// b, c, [L+0x60], [L+0x64], 0, d, a, 0, 0) (0x0049edad); a non-zero result is returned. Otherwise the three quad sets
// L+0x80.. go to W's S (setQuad / 70 / 74), the draw target (L+0x74, 0x78, 0x7c, 0) is set and 0 is returned.
// 0x0049ed20  optionDialogBuild
F4_t OptionDialogBuild_orig;
int __fastcall OptionDialogBuild_re(void* T, void*, int a, int b, int c, int d) {
    if (a == 0) return 3;                                                // 0x0049ed27 / 0x0049ed2f
    char* part = B(T) - 0x594;                                           // 0x0049ed38
    kOptionLayout(part, 0);                                              // 0x0049ed41
    int* vbt = *reinterpret_cast<int**>(part);
    char* L = part + vbt[2];                                             // 0x0049ed46..0x0049ed4c
    I(L, 0x28) = b;                                                      // 0x0049ed4f
    I(L, 0x2c) = c;                                                      // 0x0049ed5f
    if ((I(reinterpret_cast<void*>(a), 0x9c) & 0x1000) == 0) d |= 0x100000;   // 0x0049ed66..0x0049ed71
    char* W = part + vbt[1];                                             // 0x0049ed87
    int r = reinterpret_cast<F10_t>(0x004806c0)(W, 0, b, c, I(L, 0x60), I(L, 0x64), 0, d, a, 0, 0);   // 0x0049edad
    if (r != 0) return r;                                                // 0x0049edb5
    char* S = W + 0x274;
    kSetQuad(S, 0, I(L, 0x80), I(L, 0x8c), I(L, 0x98), I(L, 0xa4));      // 0x0049ede9
    kSetQuad70(S, 0, I(L, 0x84), I(L, 0x90), I(L, 0x9c), I(L, 0xa8));    // 0x0049ee1c
    kSetQuad74(S, 0, I(L, 0x88), I(L, 0x94), I(L, 0xa0), I(L, 0xac));    // 0x0049ee4f
    kSetDrawTarget(S, 0, I(L, 0x74), I(L, 0x78), I(L, 0x7c), 0);         // 0x0049ee7e
    return 0;                                                            // 0x0049ee83
}

// The vtordisp thunks of this part and of the listPanel's deleting destructor.
// 0x0049ffa0 optionDialogDrawContents_vtordisp -> 0x0049fc60
F0_t OptionDialogDrawContentsVd_orig;
int __fastcall OptionDialogDrawContentsVd_re(void* p, void*) { return reinterpret_cast<F0_t>(0x0049fc60)(VtordispThis(p), 0); }
// 0x0049ffd0 optionDialogUpdate_vtordisp -> 0x0049f9a0
F2_t OptionDialogUpdateVd_orig;
int __fastcall OptionDialogUpdateVd_re(void* p, void*, int a, int b) { return reinterpret_cast<F2_t>(0x0049f9a0)(VtordispThis(p), 0, a, b); }
// 0x0049ffe0 optionDialogOnKey_vtordisp -> 0x0049fa90
F1_t OptionDialogOnKeyVd_orig;
int __fastcall OptionDialogOnKeyVd_re(void* p, void*, int a) { return reinterpret_cast<F1_t>(0x0049fa90)(VtordispThis(p), 0, a); }
// 0x004a0040 optionDialogBuild_vtordisp -> 0x0049ed20
F4_t OptionDialogBuildVd_orig;
int __fastcall OptionDialogBuildVd_re(void* p, void*, int a, int b, int c, int d) { return reinterpret_cast<F4_t>(0x0049ed20)(VtordispThis(p), 0, a, b, c, d); }
// 0x0049ff50 optionDialogDeletingDtor_vtordisp -> 0x0049ff60
F1_t OptionDialogDeletingDtorVd_orig;
int __fastcall OptionDialogDeletingDtorVd_re(void* p, void*, int a) { return reinterpret_cast<F1_t>(0x0049ff60)(VtordispThis(p), 0, a); }
// 0x00491ac0 listPanelDeletingDtor_vtordisp -> 0x00491ad0
F1_t ListPanelDeletingDtorVd_orig;
int __fastcall ListPanelDeletingDtorVd_re(void* p, void*, int a) { return reinterpret_cast<F1_t>(0x00491ad0)(VtordispThis(p), 0, a); }

// =====================================================================================================================
// compositeScreen 0x0047fab0 and uiPointerMove 0x004959b0: the pointer-drag path of the window tree.
// =====================================================================================================================
// Globals (data addresses read by the two bodies): g_dragMode 0x0083ab1c (2 = move, 10..17 = the eight resize grips; every
// other value does nothing), g_dragWindow 0x0083ab20 (the window being moved / resized, NULL otherwise), g_dragOriginX / Y
// 0x0083ab24 / 0x0083ab28 (where the pointer last was), g_inDraw 0x0083ab4c (1 around the redraw), g_hitIsGlobal 0x0083ab18
// (hitTestScreen's flag), g_hoverWindow 0x0083aaf0, g_dragCapture 0x0083ab30.
// Window fields: +0xa0 flags (bit 1: client rect form), +0xb0 the object whose slot +0xc takes (w, h, 0, 0), +0x12c a linked
// window, +0x1a4 / +0x1a8 the minimum size, +0x1ac rect (x0, y0, x1, y1) and +0x1bc client rect, +0x1dc the clip list.
const F2_t kFromParent = reinterpret_cast<F2_t>(0x0047b200);              // View::fromParent(&x, &y), ret 8
const F2_t kToGlobal = reinterpret_cast<F2_t>(0x0047b2d0);                // View::toGlobal(&x, &y), ret 8
const F2_t kAdjustSizeOuter = reinterpret_cast<F2_t>(0x0047ca10);         // Window::adjustSizeOuter(&w, &h), ret 8
const F1_t kOffsetRectToParent = reinterpret_cast<F1_t>(0x0047b0d0);      // View::offsetRectToParent(&rect), ret 4
const F1_t kOffsetRectToLocal = reinterpret_cast<F1_t>(0x0047b120);       // View::offsetRectToLocal(&rect), ret 4
typedef int(__cdecl* C2cd_t)(void* a, void* b);
typedef int(__cdecl* C1cd_t)(void* a);
typedef int(__stdcall* Union_t)(void* dst, void* a, void* b);
const C2cd_t kRelayoutClip = reinterpret_cast<C2cd_t>(0x0047e140);        // relayoutClip(window, clip list)
const C2cd_t kRedrawDrawList = reinterpret_cast<C2cd_t>(0x0047f8e0);      // redrawDrawList(&rect, 0)
const C1cd_t kDrawTooltip = reinterpret_cast<C1cd_t>(0x0047cdb0);         // drawTooltip(&rect)

// The call of window slot +0xc of the object at +0xb0: (w, h, 0, 0) pushed as 0, 0, h, w (0x0047fd39..0x0047fd43 and the
// seven twins), the callee pops them.
typedef int(__fastcall* Resize_t)(void* obj, void* edx, int w, int h, int z0, int z1);
static int CallResize(char* self, int w, int h) {
    void* obj = P(self, 0xb0);
    return (*reinterpret_cast<Resize_t*>(B(P(obj, 0)) + 0xc))(obj, 0, w, h, 0, 0);
}

// 0x0047fab0  compositeScreen(self, x, y): `ret 8`. (x, y) is first converted in place by View::toGlobal (flags +0xa0 bit 1)
// or View::fromParent (0x0047fac2..0x0047fae1); dx = x - g_dragOriginX, dy = y - g_dragOriginY; both zero ends the call
// (0x0047fb04). The minimum size [+0x1a4], [+0x1a8] is adjusted by Window::adjustSizeOuter, and the window's rect (+0x1bc when
// bit 1 of +0xa0 is set, else +0x1ac) is turned into a (0, 0, w, h) rect and offset to the parent by View::offsetRectToParent /
// ToLocal: the OLD rectangle R. By g_dragMode (jump table 0x004801b0):
//   2        move: the rect (+0x1ac, or +0x1bc and the linked window's +0x1ac rect, which is also unioned into the dirty box)
//            moves by (dx, dy)
//   10 / 12  left / top grip: client x0 / y0 += dx / dy, clamped so that the size is at least the minimum; 11 / 15 right /
//   bottom grip: x1 / y1 += dx / dy and the origin globals follow (clamped likewise); 13 / 14 / 16 / 17 the corners;
//   the resize modes call slot +0xc of [self+0xb0] with the new (w, h, 0, 0)
//   3..9 and every other value: nothing (0x004801a3).
// For 2 and 10..17 the NEW rectangle is computed the same way as R, R = union(R, new) (and the linked window's old rect for
// mode 2), relayoutClip(self, self + 0x1dc), g_inDraw = 1, redrawDrawList(&R, 0), drawTooltip(&R), g_inDraw = 0 (0x00480169..
// 0x00480199). eax is drawTooltip's, or the offset call's / the conversion call's when the call ends early.
// 0x0047fab0  compositeScreen
typedef int(__fastcall* Cs_t)(void* self, void* edx, int x, int y);
Cs_t CompositeScreen_orig;
int __fastcall CompositeScreen_re(void* self_, void*, int x, int y) {
    char* self = B(self_);
    int last;                                                            // the eax of the last call made
    last = (I(self, 0xa0) & 2) == 0 ? kFromParent(self, 0, reinterpret_cast<int>(&x), reinterpret_cast<int>(&y))
                                    : kToGlobal(self, 0, reinterpret_cast<int>(&x), reinterpret_cast<int>(&y));   // 0x0047face / 0x0047fae1
    int dx = x - G<int>(0x0083ab24);                                     // 0x0047faea..0x0047fafa
    int dy = y - G<int>(0x0083ab28);
    if (dx == 0 && dy == 0) return last;                                 // 0x0047fb00 / 0x0047fb04
    int minW = I(self, 0x1a4), minH = I(self, 0x1a8);                    // 0x0047fb0a..0x0047fb16
    last = kAdjustSizeOuter(self, 0, reinterpret_cast<int>(&minW), reinterpret_cast<int>(&minH));   // 0x0047fb2a
    bool clientForm = (I(self, 0xa0) & 2) != 0;                          // 0x0047fb2f
    int rectOff = clientForm ? 0x1bc : 0x1ac;
    int R[4];
    {   // (0, 0, w, h) of the window rect, offset to the parent (0x0047fb38..0x0047fb82 / 0x0047fb89..0x0047fbd3)
        int* r = reinterpret_cast<int*>(self + rectOff);
        R[0] = r[0] - r[0]; R[1] = r[1] - r[1]; R[2] = r[2] - r[0]; R[3] = r[3] - r[1];
        last = (clientForm ? kOffsetRectToLocal : kOffsetRectToParent)(self, 0, reinterpret_cast<int>(R));
    }
    int* G24 = &G<int>(0x0083ab24);
    int* G28 = &G<int>(0x0083ab28);
    int* c1bc = reinterpret_cast<int*>(self + 0x1bc);                    // x0, y0, x1, y1 of the client rect
    int tmp[4];                                                          // the linked window's old rect (mode 2)
    bool tail = true;
    // After a resize the window object is told the new size (w, h) = (x1 - x0, y1 - y0).
    auto resize = [&]() { last = CallResize(self, c1bc[2] - c1bc[0], c1bc[3] - c1bc[1]); };
    // x1 below the minimum width: pull it back and move the origin global with it (0x00480065..0x00480079)
    auto fixRight = [&]() { int adj = c1bc[0] - c1bc[2] + minW; c1bc[2] += adj; *G24 += adj; };
    auto fixBottom = [&]() { int adj = c1bc[1] - c1bc[3] + minH; c1bc[3] += adj; *G28 += adj; };
    switch (G<int>(0x0083ab1c)) {                                        // 0x0047fbd8..0x0047fbea
    case 2:                                                              // 0x0047fbf1
        if (dx != 0 || dy != 0) {                                        // 0x0047fbf1..0x0047fbf7
            if (!clientForm) {
                int* r = reinterpret_cast<int*>(self + 0x1ac);
                r[0] += dx; r[2] += dx; r[1] += dy; r[3] += dy;          // 0x0047fc06..0x0047fc38
            } else {
                c1bc[0] += dx; c1bc[2] += dx; c1bc[1] += dy; c1bc[3] += dy;   // 0x0047fc43..0x0047fc75
                char* linked = reinterpret_cast<char*>(P(self, 0x12c));  // 0x0047fc7b
                if (linked != 0) {
                    int* lr = reinterpret_cast<int*>(linked + 0x1ac);
                    for (int k = 0; k < 4; k++) tmp[k] = lr[k];          // 0x0047fc8e..0x0047fca7
                    lr[0] += dx; lr[2] += dx; lr[1] += dy; lr[3] += dy;  // 0x0047fcab..0x0047fcc6
                    reinterpret_cast<Union_t>(G<void*>(0x004ba1a4))(tmp, tmp, lr);   // 0x0047fce0 (UnionRect(tmp, tmp, new linked rect))
                }
            }
        }
        break;
    case 10:                                                             // 0x0047fceb: left grip
        if (dx != 0) {
            c1bc[0] += dx;
            if (c1bc[2] - c1bc[0] < minW) c1bc[0] = c1bc[2] - minW;      // 0x0047fd11 / 0x0047fd13
            resize();                                                    // 0x0047fd43
        }
        break;
    case 11:                                                             // 0x0047fd4b: right grip
        if (dx != 0) {
            c1bc[2] += dx; *G24 += dx;
            if (c1bc[2] - c1bc[0] < minW) fixRight();                    // 0x0047fd81 / 0x0047fd83 / 0x00480065
            resize();                                                    // 0x004800a9
        }
        break;
    case 12:                                                             // 0x0047fd8e: top grip
        if (dy != 0) {
            c1bc[1] += dy;
            if (c1bc[3] - c1bc[1] < minH) c1bc[1] = c1bc[3] - minH;      // 0x0047fdb4 / 0x0047fdb6
            resize();                                                    // 0x0047fe4e
        }
        break;
    case 15:                                                             // 0x0047fdd0: bottom grip
        if (dy != 0) {
            c1bc[3] += dy; *G28 += dy;
            if (c1bc[3] - c1bc[1] < minH) fixBottom();                   // 0x0047fe06 / 0x0047fe08..0x0047fe1c
            resize();                                                    // 0x0047fe4e
        }
        break;
    case 13:                                                             // 0x0047fe56: top-left corner
        c1bc[0] += dx; c1bc[1] += dy;
        if (c1bc[3] - c1bc[1] < minH) c1bc[1] = c1bc[3] - minH;          // 0x0047fe84 / 0x0047fe86 / 0x0047fe8c
        if (c1bc[2] - c1bc[0] < minW) c1bc[0] = c1bc[2] - minW;          // 0x0047fea0 / 0x0047fea2 / 0x0047fea8
        resize();                                                        // 0x0047fecc
        break;
    case 14:                                                             // 0x0047fed4: top-right corner
        c1bc[2] += dx; c1bc[1] += dy; *G24 += dx;
        if (c1bc[3] - c1bc[1] < minH) c1bc[1] = c1bc[3] - minH;          // 0x0047ff10 / 0x0047ff12 / 0x0047ff16
        if (c1bc[2] - c1bc[0] < minW) fixRight();                        // 0x0047ff30 / 0x0047ff32 / 0x00480065
        resize();                                                        // 0x004800a9
        break;
    case 16:                                                             // 0x0047ff3d: bottom-left corner
        c1bc[0] += dx; c1bc[3] += dy; *G28 += dy;
        if (c1bc[3] - c1bc[1] < minH) fixBottom();                       // 0x0047ff79 / 0x0047ff7b / 0x0047ff7d..0x0047ff91
        if (c1bc[2] - c1bc[0] < minW) c1bc[0] = c1bc[2] - minW;          // 0x0047ffab / 0x0047ffad / 0x0047ffb3
        resize();                                                        // 0x0047ffdd
        break;
    case 17:                                                             // 0x0047ffe5: bottom-right corner
        c1bc[2] += dx; c1bc[3] += dy; *G24 += dx; *G28 += dy;
        if (c1bc[3] - c1bc[1] < minH) fixBottom();                       // 0x0048002f / 0x00480031 / 0x00480033..0x00480047
        if (c1bc[2] - c1bc[0] < minW) fixRight();                        // 0x00480061 / 0x00480063 / 0x00480065
        resize();                                                        // 0x004800a9
        break;
    default:
        tail = false;                                                    // modes 3..9 and out of range: 0x004801a3
        last = G<int>(0x0083ab1c) - 2;                                   // eax = lea [edx - 2] (0x0047fbde) when the jump leaves
        break;
    }
    if (!tail) return last;
    int R2[4];                                                           // the NEW rect (0x004800ac..0x00480132)
    {
        int* r = reinterpret_cast<int*>(self + rectOff);
        R2[0] = r[0] - r[0]; R2[1] = r[1] - r[1]; R2[2] = r[2] - r[0]; R2[3] = r[3] - r[1];
        (clientForm ? kOffsetRectToLocal : kOffsetRectToParent)(self, 0, reinterpret_cast<int>(R2));
    }
    Union_t un = reinterpret_cast<Union_t>(G<void*>(0x004ba1a4));        // mov edi, [0x4ba1a4] (0x00480137)
    un(R, R, R2);                                                        // 0x0048014c
    if (P(self, 0x12c) != 0) un(R, R, tmp);                              // 0x00480154..0x00480167
    kRelayoutClip(self, self + 0x1dc);                                   // 0x00480171
    G<int>(0x0083ab4c) = 1;                                              // 0x0048017d
    kRedrawDrawList(R, 0);                                               // 0x00480187
    int r = kDrawTooltip(R);                                             // 0x00480191
    G<int>(0x0083ab4c) = 0;                                              // 0x00480199
    return r;
}

// 0x004959b0  uiPointerMove(x, y): `ret 8`. With a drag window in g_dragWindow 0x0083ab20 (0x004959b0) compositeScreen(window,
// x, y) answers (0x004959c5). Otherwise hitTestScreen(&x, &y) finds the window under the pointer (cdecl, 0x004959d8) and
// drawCursor(win, 1, -1, -1) draws its cursor (0x004959e3..0x004959e6). With a window: when g_hoverWindow 0x0083aaf0 is another
// window and no other window holds the drag capture 0x0083ab30 (0x004959f2..0x00495a0b) the old window's slot +0x70 gets
// (x, y) and HotList::cancelTip(old + 0xbc) runs (0x00495a19..0x00495a28); then g_hoverWindow = win and win's slot +0x68 gets
// (x, y, g_hitIsGlobal 0x0083ab18) (0x00495a47). Without a window nothing more happens. eax is the last call's.
// 0x004959b0  uiPointerMove
typedef int(__fastcall* Slot2_t)(void* obj, void* edx, int a, int b);
typedef int(__fastcall* Slot3_t)(void* obj, void* edx, int a, int b, int c);
typedef int(__cdecl* HitTest_t)(int* px, int* py);
typedef int(__cdecl* DrawCursor_t)(void* win, int a1, int x, int y);
const F0_t kCancelTip = reinterpret_cast<F0_t>(0x00492d40);              // HotList::cancelTip(list), plain ret
typedef int(__stdcall* UiPtr_t)(int x, int y);
UiPtr_t UiPointerMove_orig;
int __stdcall UiPointerMove_re(int x, int y) {
    void* g = G<void*>(0x0083ab20);                                      // 0x004959b0
    if (g != 0) return reinterpret_cast<Cs_t>(0x0047fab0)(g, 0, x, y);   // 0x004959b9 / 0x004959c5
    int px = x, py = y;
    char* win = reinterpret_cast<char*>(reinterpret_cast<HitTest_t>(0x0047f700)(&px, &py));   // 0x004959d8
    int r = reinterpret_cast<DrawCursor_t>(0x0047d130)(win, 1, -1, -1);  // 0x004959e6
    if (win == 0) return r;                                              // 0x004959ee / 0x004959f0
    char* cur = G<char*>(0x0083aaf0);                                    // 0x004959f2
    if (cur != 0 && win != cur) {                                        // 0x004959fa / 0x004959fc
        char* drag = G<char*>(0x0083ab30);                               // 0x00495a00
        if (drag == 0 || drag == cur) {                                  // 0x00495a07 / 0x00495a0b
            (*reinterpret_cast<Slot2_t*>(B(P(cur, 0)) + 0x70))(cur, 0, px, py);   // 0x00495a19
            kCancelTip(G<char*>(0x0083aaf0) + 0xbc, 0);                  // 0x00495a28
        }
    }
    int hitGlobal = G<int>(0x0083ab18);                                  // 0x00495a2d
    G<char*>(0x0083aaf0) = win;                                          // 0x00495a3b
    return (*reinterpret_cast<Slot3_t*>(B(P(win, 0)) + 0x68))(win, 0, px, py, hitGlobal);   // 0x00495a47
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x004a0a10, viewDispatchSlot28, ViewDispatchSlot28_re, ViewDispatchSlot28_orig);
SG_HOOK("golf_clean.exe", 0x004a0b50, viewDispatchSlot30, ViewDispatchSlot30_re, ViewDispatchSlot30_orig);
SG_HOOK("golf_clean.exe", 0x004a0bf0, viewDispatchSlot36, ViewDispatchSlot36_re, ViewDispatchSlot36_orig);
SG_HOOK("golf_clean.exe", 0x004a0d30, viewDispatchDraw, ViewDispatchDraw_re, ViewDispatchDraw_orig);
SG_HOOK("golf_clean.exe", 0x004a0ed0, viewDispatchOnScroll, ViewDispatchOnScroll_re, ViewDispatchOnScroll_orig);
SG_HOOK("golf_clean.exe", 0x004a0ef0, viewDispatchBuild, ViewDispatchBuild_re, ViewDispatchBuild_orig);
SG_HOOK("golf_clean.exe", 0x004a1250, viewDispatchOpen, ViewDispatchOpen_re, ViewDispatchOpen_orig);
SG_HOOK("golf_clean.exe", 0x004a12e0, viewDispatchReset, ViewDispatchReset_re, ViewDispatchReset_orig);
SG_HOOK("golf_clean.exe", 0x0049e990, listPanelSlot28, ListPanelSlot28_re, ListPanelSlot28_orig);
SG_HOOK("golf_clean.exe", 0x00491960, viewDispatchDraw_vtordisp, ViewDispatchDrawVd_re, ViewDispatchDrawVd_orig);
SG_HOOK("golf_clean.exe", 0x00491990, viewDispatchSlot28_vtordisp, ViewDispatchSlot28Vd_re, ViewDispatchSlot28Vd_orig);
SG_HOOK("golf_clean.exe", 0x004919b0, viewDispatchSlot30_vtordisp, ViewDispatchSlot30Vd_re, ViewDispatchSlot30Vd_orig);
SG_HOOK("golf_clean.exe", 0x00491a00, viewDispatchSlot36_vtordisp, ViewDispatchSlot36Vd_re, ViewDispatchSlot36Vd_orig);
SG_HOOK("golf_clean.exe", 0x00491a30, viewDispatchOnScroll_vtordisp57, ViewDispatchOnScrollVd57_re, ViewDispatchOnScrollVd57_orig);
SG_HOOK("golf_clean.exe", 0x00491a40, viewDispatchOnScroll_vtordisp58, ViewDispatchOnScrollVd58_re, ViewDispatchOnScrollVd58_orig);
SG_HOOK("golf_clean.exe", 0x00491aa0, viewDispatchBuild_vtordisp, ViewDispatchBuildVd_re, ViewDispatchBuildVd_orig);
SG_HOOK("golf_clean.exe", 0x00491b70, listPanelSlot28_vtordisp, ListPanelSlot28Vd_re, ListPanelSlot28Vd_orig);
SG_HOOK("golf_clean.exe", 0x0049dab0, listPanelDraw, ListPanelDraw_re, ListPanelDraw_orig);
SG_HOOK("golf_clean.exe", 0x0049e230, listPanelOnScroll, ListPanelOnScroll_re, ListPanelOnScroll_orig);
SG_HOOK("golf_clean.exe", 0x0049e450, listPanelRefresh, ListPanelRefresh_re, ListPanelRefresh_orig);
SG_HOOK("golf_clean.exe", 0x0049e680, listPanelDrawContents, ListPanelDrawContents_re, ListPanelDrawContents_orig);
SG_HOOK("golf_clean.exe", 0x00491b50, listPanelDrawContents_vtordisp, ListPanelDrawContentsVd_re, ListPanelDrawContentsVd_orig);
SG_HOOK("golf_clean.exe", 0x00491b80, listPanelOnScroll_vtordisp, ListPanelOnScrollVd_re, ListPanelOnScrollVd_orig);
SG_HOOK("golf_clean.exe", 0x00491b90, listPanelRefresh_vtordisp, ListPanelRefreshVd_re, ListPanelRefreshVd_orig);
SG_HOOK("golf_clean.exe", 0x0049f370, optionListDraw, OptionListDraw_re, OptionListDraw_orig);
SG_HOOK("golf_clean.exe", 0x0049f9a0, optionDialogUpdate, OptionDialogUpdate_re, OptionDialogUpdate_orig);
SG_HOOK("golf_clean.exe", 0x0049fa90, optionDialogOnKey, OptionDialogOnKey_re, OptionDialogOnKey_orig);
SG_HOOK("golf_clean.exe", 0x0049fc60, optionDialogDrawContents, OptionDialogDrawContents_re, OptionDialogDrawContents_orig);
SG_HOOK("golf_clean.exe", 0x0049ff60, optionDialogDeletingDtor, OptionDialogDeletingDtor_re, OptionDialogDeletingDtor_orig);
SG_HOOK("golf_clean.exe", 0x0049ed20, optionDialogBuild, OptionDialogBuild_re, OptionDialogBuild_orig);
SG_HOOK("golf_clean.exe", 0x0049ffa0, optionDialogDrawContents_vtordisp, OptionDialogDrawContentsVd_re, OptionDialogDrawContentsVd_orig);
SG_HOOK("golf_clean.exe", 0x0049ffd0, optionDialogUpdate_vtordisp, OptionDialogUpdateVd_re, OptionDialogUpdateVd_orig);
SG_HOOK("golf_clean.exe", 0x0049ffe0, optionDialogOnKey_vtordisp, OptionDialogOnKeyVd_re, OptionDialogOnKeyVd_orig);
SG_HOOK("golf_clean.exe", 0x004a0040, optionDialogBuild_vtordisp, OptionDialogBuildVd_re, OptionDialogBuildVd_orig);
SG_HOOK("golf_clean.exe", 0x0049ff50, optionDialogDeletingDtor_vtordisp, OptionDialogDeletingDtorVd_re, OptionDialogDeletingDtorVd_orig);
SG_HOOK("golf_clean.exe", 0x00491ac0, listPanelDeletingDtor_vtordisp, ListPanelDeletingDtorVd_re, ListPanelDeletingDtorVd_orig);
SG_HOOK("golf_clean.exe", 0x0047fab0, compositeScreen, CompositeScreen_re, CompositeScreen_orig);
SG_HOOK("golf_clean.exe", 0x004959b0, uiPointerMove, UiPointerMove_re, UiPointerMove_orig);
