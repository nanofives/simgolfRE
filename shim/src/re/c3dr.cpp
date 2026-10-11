// C3 batch c3dr (round 27, 2026-10-11) of golf_clean.exe, non-drawing rows (log/c3/c3dr_candidates.tsv).
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and
// cites the address of each instruction it reproduces. __thiscall is emulated with __fastcall (ecx = this, edx
// unused). Callees run through their original addresses (a hooked callee runs its reimplementation on both arms).
//
//   Part 1  static-initialisation stubs `mov ecx, <global>; jmp <constructor>` (22 functions, generated section at the
//           end of this file by log/c3/alt/c3dr_gen.py): Image / EditBox / GlobalObj / C4837f0 / Surface constructors
//           over the game's global objects, and initPanelState (five C4837f0 constructions).
//   Part 2  vtordisp thunks `sub ecx, dword ptr [ecx - 4]; jmp <body>` of the composite view's table-C slots (11).
//   Part 3  small logic rows: blitError, setDirtyFlagA/B, matchInputCode, wobble, Cache482fd0::ctor, KeyTable::ctor,
//           Flic::ctorBase482b60, Flic::ctor481b50, Decoder482ae0::ctor, Decoder482b20::ctor,
//           Window::releaseScrollbars, zPush.
//   Part 4  viewDispatchSlot7 and its thunk, Reader::scalarDtor (callees Reader::free 0x00487f30, operator delete 0x004a4ffc).
//
// Hooked callees of other batches (re-run these keys with SIMGOLF_HOOKS_OFF so the original arm uses original callees):
//   Part 1: Image_ctor 0x00474ae0 (C3), EditBox::ctor 0x00486070 (C3), ctorGlobalObj49c020 0x0049c020 (C3),
//           C4837f0::ctor 0x004837f0 (C3), Surface_ctor 0x00473ab0 (C3), and what they call.
//   Part 2: viewDispatchSlot6 0x004a1130, viewDispatchScrollBy 0x004a13c0, viewDispatchSlot29 0x004a0db0,
//           viewDispatchSlot31 0x004a0e60, viewDispatchSlot32 0x004a0eb0, viewDispatchSlot33 0x004a0e20,
//           viewDispatchSlot34 0x004a0e90, viewDispatchSlot39 0x004a0e40, viewDispatchSelectChanged 0x004a0c90,
//           viewDispatchSlot60 0x004a0ff0, viewDispatchCycleSelection 0x004a1090 (all C3, c3cu).
//   Part 3: Random::range 0x0045c1e0 / Random::next 0x0045c1a0 and sign 0x00467150 (wobble), Buf_ctor 0x004747a0
//           (KeyTable::ctor), Surface_ctor 0x00473ab0 (Flic::ctorBase482b60), and inside this batch
//           Flic::ctorBase482b60 (Flic::ctor481b50).
#include "hooks.h"

namespace {

static inline char* B(void* p) { return reinterpret_cast<char*>(p); }
template <typename T> static inline T& At(void* p, int off) { return *reinterpret_cast<T*>(B(p) + off); }
template <typename T> static inline T& Abs(unsigned a) { return *reinterpret_cast<T*>(a); }

typedef void*(__fastcall* Ctor_t)(void* ecx, void* edx);      // thiscall, no stack arguments, returns this
typedef void*(__cdecl* Init_t)();                             // the static-initialisation stubs
inline void* CtorAt(unsigned ctor, unsigned obj) {
    return reinterpret_cast<Ctor_t>(ctor)(reinterpret_cast<void*>(obj), 0);
}

typedef int(__fastcall* M0_t)(void* ecx, void* edx);
typedef int(__fastcall* M1_t)(void* ecx, void* edx, int a);
typedef int(__fastcall* M2_t)(void* ecx, void* edx, int a, int b);
typedef int(__fastcall* M3_t)(void* ecx, void* edx, int a, int b, int c);
typedef int(__cdecl* Fn0_t)();

// =====================================================================================================================
// Part 1 - static-initialisation stubs: generated section (see the end of this file)
// =====================================================================================================================

// =====================================================================================================================
// Part 2 - vtordisp thunks of the composite view's table-C slots (view class of viewCtor4a0740, table 0x004bb8d0).
// Each is `sub ecx, dword ptr [ecx - 4]` (the thunk's this minus the displacement stored just below it) followed by
// `jmp <dispatcher>`; the arguments stay on the stack untouched and the dispatcher's eax is the thunk's eax.
// =====================================================================================================================
static inline void* vtordispThis(void* p) { return B(p) - At<int>(p, -4); }

// 0x00491940  viewDispatchSlot6_vtordisp: 0x00491940 sub, 0x00491943 jmp 0x004a1130 (viewDispatchSlot6, no arguments).
M0_t ViewSlot6Vd_orig;
int __fastcall ViewSlot6Vd_re(void* p, void*) { return reinterpret_cast<M0_t>(0x004a1130)(vtordispThis(p), 0); }

// 0x00491970  viewDispatchScrollBy_vtordisp: 0x00491970 sub, 0x00491973 jmp 0x004a13c0 (viewDispatchScrollBy, ret 0xc).
M3_t ViewScrollByVd_orig;
int __fastcall ViewScrollByVd_re(void* p, void*, int a, int b, int c) {
    return reinterpret_cast<M3_t>(0x004a13c0)(vtordispThis(p), 0, a, b, c);
}
SG_HOOK("golf_clean.exe", 0x00491970, viewDispatchScrollBy_vtordisp, ViewScrollByVd_re, ViewScrollByVd_orig);
SG_HOOK("golf_clean.exe", 0x00491940, viewDispatchSlot6_vtordisp, ViewSlot6Vd_re, ViewSlot6Vd_orig);

// 0x004919a0  viewDispatchSlot29_vtordisp: 0x004919a0 sub, 0x004919a3 jmp 0x004a0db0 (viewDispatchSlot29, ret 8).
M2_t ViewSlot29Vd_orig;
int __fastcall ViewSlot29Vd_re(void* p, void*, int a, int b) {
    return reinterpret_cast<M2_t>(0x004a0db0)(vtordispThis(p), 0, a, b);
}
SG_HOOK("golf_clean.exe", 0x004919a0, viewDispatchSlot29_vtordisp, ViewSlot29Vd_re, ViewSlot29Vd_orig);

// 0x004919c0  viewDispatchSlot31_vtordisp: 0x004919c0 sub, 0x004919c3 jmp 0x004a0e60 (viewDispatchSlot31, ret 8).
M2_t ViewSlot31Vd_orig;
int __fastcall ViewSlot31Vd_re(void* p, void*, int a, int b) {
    return reinterpret_cast<M2_t>(0x004a0e60)(vtordispThis(p), 0, a, b);
}
SG_HOOK("golf_clean.exe", 0x004919c0, viewDispatchSlot31_vtordisp, ViewSlot31Vd_re, ViewSlot31Vd_orig);

// 0x004919d0  viewDispatchSlot32_vtordisp: 0x004919d0 sub, 0x004919d3 jmp 0x004a0eb0 (viewDispatchSlot32, ret 8).
M2_t ViewSlot32Vd_orig;
int __fastcall ViewSlot32Vd_re(void* p, void*, int a, int b) {
    return reinterpret_cast<M2_t>(0x004a0eb0)(vtordispThis(p), 0, a, b);
}
SG_HOOK("golf_clean.exe", 0x004919d0, viewDispatchSlot32_vtordisp, ViewSlot32Vd_re, ViewSlot32Vd_orig);

// 0x004919e0  viewDispatchSlot33_vtordisp: 0x004919e0 sub, 0x004919e3 jmp 0x004a0e20 (viewDispatchSlot33, ret 8).
M2_t ViewSlot33Vd_orig;
int __fastcall ViewSlot33Vd_re(void* p, void*, int a, int b) {
    return reinterpret_cast<M2_t>(0x004a0e20)(vtordispThis(p), 0, a, b);
}
SG_HOOK("golf_clean.exe", 0x004919e0, viewDispatchSlot33_vtordisp, ViewSlot33Vd_re, ViewSlot33Vd_orig);

// 0x004919f0  viewDispatchSlot34_vtordisp: 0x004919f0 sub, 0x004919f3 jmp 0x004a0e90 (viewDispatchSlot34, ret 8).
M2_t ViewSlot34Vd_orig;
int __fastcall ViewSlot34Vd_re(void* p, void*, int a, int b) {
    return reinterpret_cast<M2_t>(0x004a0e90)(vtordispThis(p), 0, a, b);
}
SG_HOOK("golf_clean.exe", 0x004919f0, viewDispatchSlot34_vtordisp, ViewSlot34Vd_re, ViewSlot34Vd_orig);

// 0x00491a10  viewDispatchSelectChanged_vtordisp: 0x00491a10 sub, 0x00491a13 jmp 0x004a0c90 (viewDispatchSelectChanged, ret 8).
M2_t ViewSelectChangedVd_orig;
int __fastcall ViewSelectChangedVd_re(void* p, void*, int a, int b) {
    return reinterpret_cast<M2_t>(0x004a0c90)(vtordispThis(p), 0, a, b);
}
SG_HOOK("golf_clean.exe", 0x00491a10, viewDispatchSelectChanged_vtordisp, ViewSelectChangedVd_re, ViewSelectChangedVd_orig);

// 0x00491a20  viewDispatchSlot39_vtordisp: 0x00491a20 sub, 0x00491a23 jmp 0x004a0e40 (viewDispatchSlot39, ret 8).
M2_t ViewSlot39Vd_orig;
int __fastcall ViewSlot39Vd_re(void* p, void*, int a, int b) {
    return reinterpret_cast<M2_t>(0x004a0e40)(vtordispThis(p), 0, a, b);
}
SG_HOOK("golf_clean.exe", 0x00491a20, viewDispatchSlot39_vtordisp, ViewSlot39Vd_re, ViewSlot39Vd_orig);

// 0x00491a50  viewDispatchSlot60_vtordisp: 0x00491a50 sub, 0x00491a53 jmp 0x004a0ff0 (viewDispatchSlot60, ret 4).
M1_t ViewSlot60Vd_orig;
int __fastcall ViewSlot60Vd_re(void* p, void*, int a) {
    return reinterpret_cast<M1_t>(0x004a0ff0)(vtordispThis(p), 0, a);
}
SG_HOOK("golf_clean.exe", 0x00491a50, viewDispatchSlot60_vtordisp, ViewSlot60Vd_re, ViewSlot60Vd_orig);

// 0x00491a60  viewDispatchCycleSelection_vtordisp: 0x00491a60 sub, 0x00491a63 jmp 0x004a1090 (no arguments).
M0_t ViewCycleSelectionVd_orig;
int __fastcall ViewCycleSelectionVd_re(void* p, void*) {
    return reinterpret_cast<M0_t>(0x004a1090)(vtordispThis(p), 0);
}
SG_HOOK("golf_clean.exe", 0x00491a60, viewDispatchCycleSelection_vtordisp, ViewCycleSelectionVd_re, ViewCycleSelectionVd_orig);

// =====================================================================================================================
// Part 3 - small logic rows
// =====================================================================================================================

// 0x00492460  blitError(): eax = the dword at 0x0083c310 (0x00492460); when it is 0 (test 0x00492465, jne 0x00492467
// not taken) eax = 8 (0x00492469); `ret` at 0x0049246e. No stack arguments.
Fn0_t BlitError_orig;
int __cdecl BlitError_re() {
    int v = Abs<int>(0x0083c310);       // 0x00492460
    if (v == 0) {                       // 0x00492465 / 0x00492467
        v = 8;                          // 0x00492469
    }
    return v;                           // 0x0049246e
}
SG_HOOK("golf_clean.exe", 0x00492460, blitError, BlitError_re, BlitError_orig);

// 0x0045ae30  setDirtyFlagA(): eax = the dword at 0x00822b98 (0x0045ae30); when it is non-zero (test 0x0045ae35, jne
// 0x0045ae37) the dword is cleared (0x0045ae3a); eax keeps the value read (`ret` at 0x0045ae39 / 0x0045ae44).
Fn0_t SetDirtyFlagA_orig;
int __cdecl SetDirtyFlagA_re() {
    int v = Abs<int>(0x00822b98);       // 0x0045ae30
    if (v != 0) {                       // 0x0045ae35 / 0x0045ae37
        Abs<int>(0x00822b98) = 0;       // 0x0045ae3a
    }
    return v;                           // 0x0045ae39 / 0x0045ae44
}
SG_HOOK("golf_clean.exe", 0x0045ae30, setDirtyFlagA, SetDirtyFlagA_re, SetDirtyFlagA_orig);

// 0x0045ae50  setDirtyFlagB(): the same over the dword at 0x00822b9c (0x0045ae50, 0x0045ae55, 0x0045ae57, 0x0045ae5a).
Fn0_t SetDirtyFlagB_orig;
int __cdecl SetDirtyFlagB_re() {
    int v = Abs<int>(0x00822b9c);       // 0x0045ae50
    if (v != 0) {                       // 0x0045ae55 / 0x0045ae57
        Abs<int>(0x00822b9c) = 0;       // 0x0045ae5a
    }
    return v;                           // 0x0045ae59 / 0x0045ae64
}
SG_HOOK("golf_clean.exe", 0x0045ae50, setDirtyFlagB, SetDirtyFlagB_re, SetDirtyFlagB_orig);

// matchInputCode 0x0047eee0 is hooked by c3e.cpp (round 2, linked in the main build); this batch only adds its
// keys matchInputCode_p* (re/frida/registry.d/c3dr.py), which exercise that hook. Body removed 2026-10-11 (duplicate).

// 0x00405920  wobble(): r = Random::range(0x00822d9c, 0x65) & 0xffff (push 0x65 0x00405921, mov ecx 0x00405923, call
// 0x0045c1e0 at 0x00405928, mov si, ax / and esi, 0xffff at 0x0040592d / 0x00405930); s = r - 0x32 (0x00405936);
// sg = sign(s) (call 0x00467150 at 0x0040593a, kept in ecx 0x0040593f); m = |s| (cdq / xor / sub, 0x00405943-0x00405949).
//   m < 0x14 (cmp 0x0040594b, jge 0x0040594f): m >> 1 (sar at 0x00405954);
//   0x14 <= m < 0x28 (0x00405958, jge 0x0040595b): m - 10 (0x0040595d);
//   m >= 0x28: 2m - 0x32 (lea at 0x00405962).
// The result is negated (neg at 0x0040596b) unless sign(s) == 1 (cmp ecx, 1 / je at 0x00405966 / 0x00405969).
typedef int(__fastcall* RandomRange_t)(void* ecx, void* edx, int n);        // ret 4
typedef int(__cdecl* Sign_t)(int v);
Fn0_t Wobble_orig;
int __cdecl Wobble_re() {
    int r = reinterpret_cast<RandomRange_t>(0x0045c1e0)(reinterpret_cast<void*>(0x00822d9c), 0, 0x65) & 0xffff;
    int s = r - 0x32;                                                        // 0x00405936
    int sg = reinterpret_cast<Sign_t>(0x00467150)(s);                        // 0x0040593a
    int m = s < 0 ? -s : s;                                                  // 0x00405943-0x00405949
    if (m < 0x14) {                                                          // 0x0040594b / 0x0040594f
        m = m >> 1;                                                          // 0x00405954
    } else if (m < 0x28) {                                                   // 0x00405958 / 0x0040595b
        m = m - 0xa;                                                         // 0x0040595d
    } else {
        m = m + m - 0x32;                                                    // 0x00405962
    }
    if (sg != 1) {                                                           // 0x00405966 / 0x00405969
        m = -m;                                                              // 0x0040596b
    }
    return m;                                                                // 0x0040596d
}
SG_HOOK("golf_clean.exe", 0x00405920, wobble, Wobble_re, Wobble_orig);

// 0x00482fd0  Cache482fd0::ctor(this): [this + 4] = 0 (0x00482fda) and the vtable 0x004babbc at [this] (0x00482fdd);
// then five entries of 0x10 bytes from this + 8 (edx = this + 0xc at 0x00482fe3, ecx = 5 at 0x00482fd5): the dword at
// edx - 4 gets -1 (0x00482fe6), the dword at edx 0 (0x00482fed), the bytes at edx + 4 and edx + 5 0 (0x00482fef /
// 0x00482ff2), the dword at edx + 8 0 (0x00482ff5); edx += 0x10 (0x00482ff8), loop on ecx (0x00482ffb / 0x00482ffc);
// [this + 4] = 0 again (0x00482ffe). Returns this (mov eax, ecx at 0x00482fd1).
Ctor_t Cache482fd0Ctor_orig;
void* __fastcall Cache482fd0Ctor_re(void* self, void*) {
    At<int>(self, 4) = 0;                                                    // 0x00482fda
    At<int>(self, 0) = 0x004babbc;                                           // 0x00482fdd
    char* e = B(self) + 0xc;                                                 // 0x00482fe3
    for (int i = 0; i < 5; ++i) {                                            // 0x00482fd5 / 0x00482ffb / 0x00482ffc
        *reinterpret_cast<int*>(e - 4) = -1;                                 // 0x00482fe6
        *reinterpret_cast<int*>(e) = 0;                                      // 0x00482fed
        e[4] = 0;                                                            // 0x00482fef
        e[5] = 0;                                                            // 0x00482ff2
        *reinterpret_cast<int*>(e + 8) = 0;                                  // 0x00482ff5
        e += 0x10;                                                           // 0x00482ff8
    }
    At<int>(self, 4) = 0;                                                    // 0x00482ffe
    return self;                                                             // 0x00483002
}
SG_HOOK("golf_clean.exe", 0x00482fd0, Cache482fd0_ctor, Cache482fd0Ctor_re, Cache482fd0Ctor_orig);

// 0x004882a0  KeyTable::ctor(this): Buf_ctor 0x004747a0 on this + 0x108 (lea 0x004882a3, call 0x004882a9); the vtable
// 0x004bb088 at [this] (0x004882b0); [this + 0x104] = 0 (0x004882b6); the byte at this + 4 = 0 (0x004882bc); returns this
// (mov eax, esi at 0x004882bf).
Ctor_t KeyTableCtor_orig;
void* __fastcall KeyTableCtor_re(void* self, void*) {
    reinterpret_cast<Ctor_t>(0x004747a0)(B(self) + 0x108, 0);               // 0x004882a9
    At<int>(self, 0) = 0x004bb088;                                           // 0x004882b0
    At<int>(self, 0x104) = 0;                                                // 0x004882b6
    At<char>(self, 4) = 0;                                                   // 0x004882bc
    return self;                                                             // 0x004882bf
}
SG_HOOK("golf_clean.exe", 0x004882a0, KeyTable_ctor, KeyTableCtor_re, KeyTableCtor_orig);

// 0x00482b60  Flic::ctorBase482b60(this): Surface_ctor 0x00473ab0 on this + 8 (lea 0x00482b63, call 0x00482b66); the
// vtable 0x004ba2dc at [this] (0x00482b6d); zeros at +4 (0x00482b73), +0x38 (0x00482b76), +0x3c (0x00482b79), +0x34
// (0x00482b7c); -1 at +0x40 (0x00482b7f); returns this (0x00482b86).
Ctor_t FlicCtorBase_orig;
void* __fastcall FlicCtorBase_re(void* self, void*) {
    reinterpret_cast<Ctor_t>(0x00473ab0)(B(self) + 8, 0);                   // 0x00482b66
    At<int>(self, 0) = 0x004ba2dc;                                           // 0x00482b6d
    At<int>(self, 4) = 0;                                                    // 0x00482b73
    At<int>(self, 0x38) = 0;                                                 // 0x00482b76
    At<int>(self, 0x3c) = 0;                                                 // 0x00482b79
    At<int>(self, 0x34) = 0;                                                 // 0x00482b7c
    At<int>(self, 0x40) = -1;                                                // 0x00482b7f
    return self;                                                             // 0x00482b86
}
SG_HOOK("golf_clean.exe", 0x00482b60, Flic_ctorBase482b60, FlicCtorBase_re, FlicCtorBase_orig);

// 0x00481b50  Flic::ctor481b50(this): Flic::ctorBase482b60 (call 0x00481b53), the vtable 0x004ba2e0 at [this]
// (0x00481b5a), then zeros at +0x50 (0x00481b60), +0x54, +0x6c, +0x70, +0x5c, +0x60, +0x64, +0x68, +0x74, +0x78, +0x44,
// +0x58, +0x7c, +0x48, +0x4c (0x00481b63-0x00481b8a); returns this (0x00481b8d).
Ctor_t FlicCtor_orig;
void* __fastcall FlicCtor_re(void* self, void*) {
    reinterpret_cast<Ctor_t>(0x00482b60)(self, 0);                          // 0x00481b53
    At<int>(self, 0) = 0x004ba2e0;                                           // 0x00481b5a
    static const int zeroOffs[] = {0x50, 0x54, 0x6c, 0x70, 0x5c, 0x60, 0x64, 0x68, 0x74, 0x78, 0x44, 0x58, 0x7c, 0x48, 0x4c};
    for (int i = 0; i < 15; ++i) {                                           // 0x00481b60-0x00481b8a
        At<int>(self, zeroOffs[i]) = 0;
    }
    return self;                                                             // 0x00481b8d
}
SG_HOOK("golf_clean.exe", 0x00481b50, Flic_ctor481b50, FlicCtor_re, FlicCtor_orig);

// 0x00482ae0  Decoder482ae0::ctor(this, flag), ret 4: a non-zero flag (cmp 0x00482ae9, je 0x00482aef) stores the vtable
// 0x004ba28c at [this] (0x00482af1) and 0x004ba278 at +0x14 (0x00482af7), copies the dword at 0x00839650 to +0x18
// (0x00482afe / 0x00482b04) and clears 0x00839650 (0x00482b07). Then the vtable at [this] is read (0x00482b0d), its
// dword +4 (0x00482b0f) is the offset at which 0x004ba288 is stored (0x00482b12). Returns this (eax = ecx, 0x00482ae7).
typedef void*(__fastcall* Ctor1_t)(void* ecx, void* edx, int flag);
Ctor1_t Decoder482ae0Ctor_orig;
void* __fastcall Decoder482ae0Ctor_re(void* self, void*, int flag) {
    if (flag != 0) {                                                         // 0x00482ae9 / 0x00482aeb
        At<int>(self, 0) = 0x004ba28c;                                       // 0x00482af1
        At<int>(self, 0x14) = 0x004ba278;                                    // 0x00482af7
        At<int>(self, 0x18) = Abs<int>(0x00839650);                          // 0x00482afe / 0x00482b04
        Abs<int>(0x00839650) = 0;                                            // 0x00482b07
    }
    char* vt = At<char*>(self, 0);                                           // 0x00482b0d
    int d = *reinterpret_cast<int*>(vt + 4);                                 // 0x00482b0f
    At<int>(self, d) = 0x004ba288;                                           // 0x00482b12
    return self;                                                             // 0x00482b1a
}
SG_HOOK("golf_clean.exe", 0x00482ae0, Decoder482ae0_ctor, Decoder482ae0Ctor_re, Decoder482ae0Ctor_orig);

// 0x00482b20  Decoder482b20::ctor(this, flag), ret 4: the same shape with the vtable 0x004ba2b8 (0x00482b31), 0x004ba278
// at +8 (0x00482b37), the dword at 0x00839650 copied to +0xc (0x00482b3e / 0x00482b44) and cleared (0x00482b47); the final
// store (0x00482b52) writes 0x004ba2b4 at the vtable's +4 offset.
Ctor1_t Decoder482b20Ctor_orig;
void* __fastcall Decoder482b20Ctor_re(void* self, void*, int flag) {
    if (flag != 0) {                                                         // 0x00482b29 / 0x00482b2b
        At<int>(self, 0) = 0x004ba2b8;                                       // 0x00482b31
        At<int>(self, 8) = 0x004ba278;                                       // 0x00482b37
        At<int>(self, 0xc) = Abs<int>(0x00839650);                           // 0x00482b3e / 0x00482b44
        Abs<int>(0x00839650) = 0;                                            // 0x00482b47
    }
    char* vt = At<char*>(self, 0);                                           // 0x00482b4d
    int d = *reinterpret_cast<int*>(vt + 4);                                 // 0x00482b4f
    At<int>(self, d) = 0x004ba2b4;                                           // 0x00482b52
    return self;                                                             // 0x00482b5a
}
SG_HOOK("golf_clean.exe", 0x00482b20, Decoder482b20_ctor, Decoder482b20Ctor_re, Decoder482b20Ctor_orig);

// 0x0047d510  Window::releaseScrollbars(this): for each of the child pointers at +0x150 (0x0047d513), +0x154
// (0x0047d52d) and +0x158 (0x0047d547): when it is non-null (test / je at 0x0047d519 / 0x0047d535 / 0x0047d54f) its
// slot 0 (the deleting destructor: mov eax, [ecx] / push 1 / call [eax] at 0x0047d51d-0x0047d521, 0x0047d537-0x0047d53b,
// 0x0047d551-0x0047d555) is called with 1 and the pointer is cleared (0x0047d523, 0x0047d53d, 0x0047d557). eax is the
// last call's result (no caller reads it: layoutViewTree overwrites eax at 0x0047aab7).
M0_t ReleaseScrollbars_orig;
int __fastcall ReleaseScrollbars_re(void* self, void*) {
    typedef int(__fastcall* DelDtor_t)(void* ecx, void* edx, int flags);     // ret 4
    int r = 0;
    for (int off = 0x150; off <= 0x158; off += 4) {                          // 0x0047d513 / 0x0047d52d / 0x0047d547
        void* c = At<void*>(self, off);
        if (c != 0) {                                                        // 0x0047d519 / 0x0047d535 / 0x0047d54f
            void** vt = At<void**>(c, 0);                                    // 0x0047d51d / 0x0047d537 / 0x0047d551
            r = reinterpret_cast<DelDtor_t>(vt[0])(c, 0, 1);                 // 0x0047d521 / 0x0047d53b / 0x0047d555
            At<void*>(self, off) = 0;                                        // 0x0047d523 / 0x0047d53d / 0x0047d557
        }
    }
    return r;
}
SG_HOOK("golf_clean.exe", 0x0047d510, Window_releaseScrollbars, ReleaseScrollbars_re, ReleaseScrollbars_orig);

// 0x0047e4c0  zPush(win), cdecl: a null window (test 0x0047e4c5, je 0x0047e4c7) returns at once. The window's flag dword
// at +0x9c (0x0047e4c9) and the count at 0x0083ab94 (0x0047e4cf) are read. With bit 0x2000000 set (test 0x0047e4d5, je
// 0x0047e4d9 not taken) the window is appended at 0x0083a2d8[count] (0x0047e4dc) and the count grows (0x0047e4e3 /
// 0x0047e4e4). Otherwise, when count > 0 (signed test 0x0047e4ec, jle 0x0047e4ee) entries 0..count-1 move up one place
// from the top (0x0047e4f0-0x0047e503), then count + 1 (0x0047e506), the window lands at slot 0 (0x0047e507) and the
// count is stored (0x0047e50d). eax at exit: the flag dword (appended case, and the count <= 0 case), 0x0083a2d8 after
// a shift; no caller reads it (layoutViewTree overwrites eax at 0x0047a783).
typedef int(__cdecl* ZPush_t)(void* win);
ZPush_t ZPush_orig;
int __cdecl ZPush_re(void* win) {
    if (win == 0) {                                                          // 0x0047e4c5 / 0x0047e4c7
        return 0;
    }
    int flags = At<int>(win, 0x9c);                                          // 0x0047e4c9
    int count = Abs<int>(0x0083ab94);                                        // 0x0047e4cf
    void** list = reinterpret_cast<void**>(0x0083a2d8);
    if (flags & 0x2000000) {                                                 // 0x0047e4d5 / 0x0047e4d9
        list[count] = win;                                                   // 0x0047e4dc
        Abs<int>(0x0083ab94) = count + 1;                                    // 0x0047e4e3 / 0x0047e4e4
        return flags;                                                        // 0x0047e4eb
    }
    int ret = flags;
    if (count > 0) {                                                         // 0x0047e4ec / 0x0047e4ee
        for (int i = count; i > 0; --i) {                                    // 0x0047e4f0-0x0047e503
            list[i] = list[i - 1];
        }
        ret = 0x0083a2d8;                                                    // eax after the loop (0x0047e4ff)
    }
    list[0] = win;                                                           // 0x0047e507
    Abs<int>(0x0083ab94) = count + 1;                                        // 0x0047e506 / 0x0047e50d
    return ret;
}
SG_HOOK("golf_clean.exe", 0x0047e4c0, zPush, ZPush_re, ZPush_orig);

// =====================================================================================================================
// Part 4 - viewDispatchSlot7 (+ thunk) and Reader::scalarDtor
// =====================================================================================================================

// 0x004a1170  viewDispatchSlot7(this = D, a), ret 4: reads the 32-bit tag at D - 8 (0x004a1170); tag 2 (sub 0x004a1173,
// jne 0x004a1176 not taken) selects the ListBox part: the vbtable at D - 0x1f8 (0x004a1178), its entry +4 (0x004a117e) gives
// e; base = D + e - 0x1a0 (lea 0x004a1181) and the byte at D + e - 0x15c (0x004a1188) must have bit 0x40 (test 0x004a118f,
// je 0x004a1192 not taken) and the dword at base + 0xd8 (0x004a1194) must be non-null (test 0x004a119a, je 0x004a119c not
// taken); then slot +0x24 of that object's vtable runs with a (0x004a119e-0x004a11a5) and its eax is the result. Every
// other path returns a (mov eax, [esp + 4] at 0x004a11ab).
M1_t ViewSlot7_orig;
int __fastcall ViewSlot7_re(void* d, void*, int a) {
    if (At<int>(d, -8) == 2) {                                               // 0x004a1170 / 0x004a1173 / 0x004a1176
        char* vb = At<char*>(d, -0x1f8);                                     // 0x004a1178
        int e = *reinterpret_cast<int*>(vb + 4);                             // 0x004a117e
        char* base = B(d) + e - 0x1a0;                                       // 0x004a1181
        unsigned char fl = *reinterpret_cast<unsigned char*>(B(d) + e - 0x15c);   // 0x004a1188
        if (fl & 0x40) {                                                     // 0x004a118f / 0x004a1192
            void* child = *reinterpret_cast<void**>(base + 0xd8);            // 0x004a1194
            if (child != 0) {                                                // 0x004a119a / 0x004a119c
                void** vt = *reinterpret_cast<void***>(child);               // 0x004a11a2
                return reinterpret_cast<M1_t>(vt[0x24 / 4])(child, 0, a);    // 0x004a11a4 / 0x004a11a5
            }
        }
    }
    return a;                                                                // 0x004a11ab
}
SG_HOOK("golf_clean.exe", 0x004a1170, viewDispatchSlot7, ViewSlot7_re, ViewSlot7_orig);

// 0x00491950  viewDispatchSlot7_vtordisp: 0x00491950 sub, 0x00491953 jmp 0x004a1170 (viewDispatchSlot7, ret 4).
M1_t ViewSlot7Vd_orig;
int __fastcall ViewSlot7Vd_re(void* p, void*, int a) {
    return reinterpret_cast<M1_t>(0x004a1170)(vtordispThis(p), 0, a);
}
SG_HOOK("golf_clean.exe", 0x00491950, viewDispatchSlot7_vtordisp, ViewSlot7Vd_re, ViewSlot7Vd_orig);

// 0x00488460  Reader::scalarDtor(this, flags), ret 4: the vtable 0x004bb084 at [this] (0x00488463), Reader::free 0x00487f30
// (call 0x00488469), then, when bit 0 of the flags byte is set (test 0x0048846e, je 0x00488473 not taken), this goes to
// operator delete 0x004a4ffc (call 0x00488476); returns this (mov eax, esi 0x0048847e, ret 4 0x00488481).
typedef void*(__fastcall* ScalarDtor_t)(void* ecx, void* edx, unsigned flags);
ScalarDtor_t ReaderScalarDtor_orig;
void* __fastcall ReaderScalarDtor_re(void* self, void*, unsigned flags) {
    At<int>(self, 0) = 0x004bb084;                                           // 0x00488463
    reinterpret_cast<Ctor_t>(0x00487f30)(self, 0);                           // 0x00488469
    if (flags & 1) {                                                         // 0x0048846e / 0x00488473
        reinterpret_cast<void(__cdecl*)(void*)>(0x004a4ffc)(self);           // 0x00488476
    }
    return self;                                                             // 0x0048847e
}
SG_HOOK("golf_clean.exe", 0x00488460, Reader_scalarDtor, ReaderScalarDtor_re, ReaderScalarDtor_orig);

// 0x004043b0  staticInit_004043b0(): constructs the global object at 0x0051b068 in place: `mov ecx, 0x0051b068` at 0x004043b0 and a tail
// `jmp 0x00474ae0` (Image_ctor) at 0x004043b5, so the constructor's return (this) is the function's return.
Init_t staticInit_004043b0_orig;
void* __cdecl staticInit_004043b0_re() { return CtorAt(0x00474ae0, 0x0051b068); }
SG_HOOK("golf_clean.exe", 0x004043b0, staticInit_004043b0, staticInit_004043b0_re, staticInit_004043b0_orig);

// 0x004058f0  staticInit_004058f0(): constructs the global object at 0x005a7148 in place: `mov ecx, 0x005a7148` at 0x004058f0 and a tail
// `jmp 0x00474ae0` (Image_ctor) at 0x004058f5, so the constructor's return (this) is the function's return.
Init_t staticInit_004058f0_orig;
void* __cdecl staticInit_004058f0_re() { return CtorAt(0x00474ae0, 0x005a7148); }
SG_HOOK("golf_clean.exe", 0x004058f0, staticInit_004058f0, staticInit_004058f0_re, staticInit_004058f0_orig);

// 0x004061b0  staticInit_004061b0(): constructs the global object at 0x00568d10 in place: `mov ecx, 0x00568d10` at 0x004061b0 and a tail
// `jmp 0x00474ae0` (Image_ctor) at 0x004061b5, so the constructor's return (this) is the function's return.
Init_t staticInit_004061b0_orig;
void* __cdecl staticInit_004061b0_re() { return CtorAt(0x00474ae0, 0x00568d10); }
SG_HOOK("golf_clean.exe", 0x004061b0, staticInit_004061b0, staticInit_004061b0_re, staticInit_004061b0_orig);

// 0x004061f0  staticInit_004061f0(): constructs the global object at 0x00587ae8 in place: `mov ecx, 0x00587ae8` at 0x004061f0 and a tail
// `jmp 0x00474ae0` (Image_ctor) at 0x004061f5, so the constructor's return (this) is the function's return.
Init_t staticInit_004061f0_orig;
void* __cdecl staticInit_004061f0_re() { return CtorAt(0x00474ae0, 0x00587ae8); }
SG_HOOK("golf_clean.exe", 0x004061f0, staticInit_004061f0, staticInit_004061f0_re, staticInit_004061f0_orig);

// 0x0043d640  staticCtor_43d640(): constructs the global object at 0x005aa778 in place: `mov ecx, 0x005aa778` at 0x0043d640 and a tail
// `jmp 0x00474ae0` (Image_ctor) at 0x0043d645, so the constructor's return (this) is the function's return.
Init_t staticCtor_43d640_orig;
void* __cdecl staticCtor_43d640_re() { return CtorAt(0x00474ae0, 0x005aa778); }
SG_HOOK("golf_clean.exe", 0x0043d640, staticCtor_43d640, staticCtor_43d640_re, staticCtor_43d640_orig);

// 0x0046de40  gImageCtor_838f98(): constructs the global object at 0x00838f98 in place: `mov ecx, 0x00838f98` at 0x0046de40 and a tail
// `jmp 0x00474ae0` (Image_ctor) at 0x0046de45, so the constructor's return (this) is the function's return.
Init_t gImageCtor_838f98_orig;
void* __cdecl gImageCtor_838f98_re() { return CtorAt(0x00474ae0, 0x00838f98); }
SG_HOOK("golf_clean.exe", 0x0046de40, gImageCtor_838f98, gImageCtor_838f98_re, gImageCtor_838f98_orig);

// 0x00473a70  initGlobalImage(): constructs the global object at 0x00839348 in place: `mov ecx, 0x00839348` at 0x00473a70 and a tail
// `jmp 0x00474ae0` (Image_ctor) at 0x00473a75, so the constructor's return (this) is the function's return.
Init_t initGlobalImage_orig;
void* __cdecl initGlobalImage_re() { return CtorAt(0x00474ae0, 0x00839348); }
SG_HOOK("golf_clean.exe", 0x00473a70, initGlobalImage, initGlobalImage_re, initGlobalImage_orig);

// 0x004799f0  gImageCtor_83a500(): constructs the global object at 0x0083a500 in place: `mov ecx, 0x0083a500` at 0x004799f0 and a tail
// `jmp 0x00474ae0` (Image_ctor) at 0x004799f5, so the constructor's return (this) is the function's return.
Init_t gImageCtor_83a500_orig;
void* __cdecl gImageCtor_83a500_re() { return CtorAt(0x00474ae0, 0x0083a500); }
SG_HOOK("golf_clean.exe", 0x004799f0, gImageCtor_83a500, gImageCtor_83a500_re, gImageCtor_83a500_orig);

// 0x00479a40  gImageCtor_83a7b8(): constructs the global object at 0x0083a7b8 in place: `mov ecx, 0x0083a7b8` at 0x00479a40 and a tail
// `jmp 0x00474ae0` (Image_ctor) at 0x00479a45, so the constructor's return (this) is the function's return.
Init_t gImageCtor_83a7b8_orig;
void* __cdecl gImageCtor_83a7b8_re() { return CtorAt(0x00474ae0, 0x0083a7b8); }
SG_HOOK("golf_clean.exe", 0x00479a40, gImageCtor_83a7b8, gImageCtor_83a7b8_re, gImageCtor_83a7b8_orig);

// 0x004385a0  staticCtor_4385a0(): constructs the global object at 0x005a9f78 in place: `mov ecx, 0x005a9f78` at 0x004385a0 and a tail
// `jmp 0x00486070` (EditBox::ctor) at 0x004385a5, so the constructor's return (this) is the function's return.
Init_t staticCtor_4385a0_orig;
void* __cdecl staticCtor_4385a0_re() { return CtorAt(0x00486070, 0x005a9f78); }
SG_HOOK("golf_clean.exe", 0x004385a0, staticCtor_4385a0, staticCtor_4385a0_re, staticCtor_4385a0_orig);

// 0x0049bff0  initGlobalObj_49c020(): constructs the global object at 0x008400c8 in place: `mov ecx, 0x008400c8` at 0x0049bff0 and a tail
// `jmp 0x0049c020` (ctorGlobalObj49c020) at 0x0049bff5, so the constructor's return (this) is the function's return.
Init_t initGlobalObj_49c020_orig;
void* __cdecl initGlobalObj_49c020_re() { return CtorAt(0x0049c020, 0x008400c8); }
SG_HOOK("golf_clean.exe", 0x0049bff0, initGlobalObj_49c020, initGlobalObj_49c020_re, initGlobalObj_49c020_orig);

// 0x004837b0  screenMgrCtor4837b0(): constructs the global object at 0x0083ad20 in place: `mov ecx, 0x0083ad20` at 0x004837b0 and a tail
// `jmp 0x004837f0` (C4837f0::ctor) at 0x004837b5, so the constructor's return (this) is the function's return.
Init_t screenMgrCtor4837b0_orig;
void* __cdecl screenMgrCtor4837b0_re() { return CtorAt(0x004837f0, 0x0083ad20); }
SG_HOOK("golf_clean.exe", 0x004837b0, screenMgrCtor4837b0, screenMgrCtor4837b0_re, screenMgrCtor4837b0_orig);

// 0x0049d160  initGfxGlobal_840710(): constructs the global object at 0x00840710 in place: `mov ecx, 0x00840710` at 0x0049d160 and a tail
// `jmp 0x00473ab0` (Surface_ctor) at 0x0049d165, so the constructor's return (this) is the function's return.
Init_t initGfxGlobal_840710_orig;
void* __cdecl initGfxGlobal_840710_re() { return CtorAt(0x00473ab0, 0x00840710); }
SG_HOOK("golf_clean.exe", 0x0049d160, initGfxGlobal_840710, initGfxGlobal_840710_re, initGfxGlobal_840710_orig);

// 0x0049d2a0  initGfxGlobal_8407c0(): constructs the global object at 0x008407c0 in place: `mov ecx, 0x008407c0` at 0x0049d2a0 and a tail
// `jmp 0x00473ab0` (Surface_ctor) at 0x0049d2a5, so the constructor's return (this) is the function's return.
Init_t initGfxGlobal_8407c0_orig;
void* __cdecl initGfxGlobal_8407c0_re() { return CtorAt(0x00473ab0, 0x008407c0); }
SG_HOOK("golf_clean.exe", 0x0049d2a0, initGfxGlobal_8407c0, initGfxGlobal_8407c0_re, initGfxGlobal_8407c0_orig);

// 0x0049d300  initGfxGlobal_840790(): constructs the global object at 0x00840790 in place: `mov ecx, 0x00840790` at 0x0049d300 and a tail
// `jmp 0x00473ab0` (Surface_ctor) at 0x0049d305, so the constructor's return (this) is the function's return.
Init_t initGfxGlobal_840790_orig;
void* __cdecl initGfxGlobal_840790_re() { return CtorAt(0x00473ab0, 0x00840790); }
SG_HOOK("golf_clean.exe", 0x0049d300, initGfxGlobal_840790, initGfxGlobal_840790_re, initGfxGlobal_840790_orig);

// 0x0049d360  initGfxGlobal_8407f0(): constructs the global object at 0x008407f0 in place: `mov ecx, 0x008407f0` at 0x0049d360 and a tail
// `jmp 0x00473ab0` (Surface_ctor) at 0x0049d365, so the constructor's return (this) is the function's return.
Init_t initGfxGlobal_8407f0_orig;
void* __cdecl initGfxGlobal_8407f0_re() { return CtorAt(0x00473ab0, 0x008407f0); }
SG_HOOK("golf_clean.exe", 0x0049d360, initGfxGlobal_8407f0, initGfxGlobal_8407f0_re, initGfxGlobal_8407f0_orig);

// 0x0049d490  initGfxGlobal_840830(): constructs the global object at 0x00840830 in place: `mov ecx, 0x00840830` at 0x0049d490 and a tail
// `jmp 0x00473ab0` (Surface_ctor) at 0x0049d495, so the constructor's return (this) is the function's return.
Init_t initGfxGlobal_840830_orig;
void* __cdecl initGfxGlobal_840830_re() { return CtorAt(0x00473ab0, 0x00840830); }
SG_HOOK("golf_clean.exe", 0x0049d490, initGfxGlobal_840830, initGfxGlobal_840830_re, initGfxGlobal_840830_orig);

// 0x0049d4f0  initGfxGlobal_840890(): constructs the global object at 0x00840890 in place: `mov ecx, 0x00840890` at 0x0049d4f0 and a tail
// `jmp 0x00473ab0` (Surface_ctor) at 0x0049d4f5, so the constructor's return (this) is the function's return.
Init_t initGfxGlobal_840890_orig;
void* __cdecl initGfxGlobal_840890_re() { return CtorAt(0x00473ab0, 0x00840890); }
SG_HOOK("golf_clean.exe", 0x0049d4f0, initGfxGlobal_840890, initGfxGlobal_840890_re, initGfxGlobal_840890_orig);

// 0x0049d550  initGfxGlobal_840860(): constructs the global object at 0x00840860 in place: `mov ecx, 0x00840860` at 0x0049d550 and a tail
// `jmp 0x00473ab0` (Surface_ctor) at 0x0049d555, so the constructor's return (this) is the function's return.
Init_t initGfxGlobal_840860_orig;
void* __cdecl initGfxGlobal_840860_re() { return CtorAt(0x00473ab0, 0x00840860); }
SG_HOOK("golf_clean.exe", 0x0049d550, initGfxGlobal_840860, initGfxGlobal_840860_re, initGfxGlobal_840860_orig);

// 0x0049eb00  initGfxGlobal_8408c8(): constructs the global object at 0x008408c8 in place: `mov ecx, 0x008408c8` at 0x0049eb00 and a tail
// `jmp 0x00473ab0` (Surface_ctor) at 0x0049eb05, so the constructor's return (this) is the function's return.
Init_t initGfxGlobal_8408c8_orig;
void* __cdecl initGfxGlobal_8408c8_re() { return CtorAt(0x00473ab0, 0x008408c8); }
SG_HOOK("golf_clean.exe", 0x0049eb00, initGfxGlobal_8408c8, initGfxGlobal_8408c8_re, initGfxGlobal_8408c8_orig);

// 0x0049eb60  initGfxGlobal_8408f8(): constructs the global object at 0x008408f8 in place: `mov ecx, 0x008408f8` at 0x0049eb60 and a tail
// `jmp 0x00473ab0` (Surface_ctor) at 0x0049eb65, so the constructor's return (this) is the function's return.
Init_t initGfxGlobal_8408f8_orig;
void* __cdecl initGfxGlobal_8408f8_re() { return CtorAt(0x00473ab0, 0x008408f8); }
SG_HOOK("golf_clean.exe", 0x0049eb60, initGfxGlobal_8408f8, initGfxGlobal_8408f8_re, initGfxGlobal_8408f8_orig);

// 0x0044ac60  initPanelState(): C4837f0::ctor 0x004837f0 on the global objects 0x00821020 (mov ecx at 0x0044ac60, call
// 0x0044ac65), 0x00821f08 (0x0044ac6a / 0x0044ac6f), 0x00821ec8 (0x0044ac74 / 0x0044ac79), 0x00821f28 (0x0044ac7e /
// 0x0044ac83) and, as a tail `jmp` at 0x0044ac8d, 0x00821ee8 (0x0044ac88): the return is the last constructor's.
Init_t initPanelState_orig;
void* __cdecl initPanelState_re() {
    CtorAt(0x004837f0, 0x00821020);
    CtorAt(0x004837f0, 0x00821f08);
    CtorAt(0x004837f0, 0x00821ec8);
    CtorAt(0x004837f0, 0x00821f28);
    return CtorAt(0x004837f0, 0x00821ee8);
}
SG_HOOK("golf_clean.exe", 0x0044ac60, initPanelState, initPanelState_re, initPanelState_orig);


}  // namespace
