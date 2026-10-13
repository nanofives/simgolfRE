// C3 batch c3ev (round 32, 2026-10-13): golf_clean.exe rows of the display / live / other drop classes.
//
// 56 bodies, every one A/B'd GREEN on instances\c3ev (re/frida/registry.d/c3ev.py, re/frida/js/fixtures.d/c3ev.js;
// Purposes and branch tables in log/c3/c3ev_purpose.md, controls in log/c3/c3ev_notes.txt). Each body was written from
// the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and cites the instructions it
// reproduces.
//
// __thiscall is emulated with __fastcall (ecx = this, edx unused). Callees run through their original addresses, so a
// callee another batch hooks runs its reimplementation (the verifier re-runs the keys with those hooks off).
#include "hooks.h"
#include <string.h>

namespace {

static inline char* B(void* p) { return reinterpret_cast<char*>(p); }
template <typename T> static inline T& G(unsigned addr) { return *reinterpret_cast<T*>(addr); }

// `sub ecx, dword ptr [ecx - 4]`: the vtordisp thunks below.
static inline void* VtordispThis(void* p) { return B(p) - *reinterpret_cast<int*>(B(p) - 4); }

typedef int(__fastcall* F0_t)(void* ecx, void* edx);
typedef int(__fastcall* F1_t)(void* ecx, void* edx, int a);
typedef int(__fastcall* F2_t)(void* ecx, void* edx, int a, int b);
typedef int(__fastcall* F4_t)(void* ecx, void* edx, int a, int b, int c, int d);
typedef void(__fastcall* V0_t)(void* ecx, void* edx);
typedef int(__cdecl* C0_t)();
typedef void(__cdecl* Del_t)(void* p);

// operator delete 0x004a4ffc and free 0x004a5007 (cdecl, the caller pops: `add esp, 4` after each call below)
static inline void OperatorDelete(void* p) { reinterpret_cast<Del_t>(0x004a4ffc)(p); }
static inline void CrtFree(void* p) { reinterpret_cast<Del_t>(0x004a5007)(p); }

// The scalar deleting destructor shape (0x00488630, 0x004916f0, 0x0049c800, 0x0043cbd0, 0x00404260):
//   push esi; mov esi, ecx; call <dtor>; test byte [esp+8], 1; je skip; push esi; call 0x004a4ffc; add esp, 4;
//   skip: mov eax, esi; pop esi; ret 4
template <unsigned Dtor> static void* ScalarDeleting(void* self, int flags) {
    reinterpret_cast<V0_t>(Dtor)(self, 0);
    if (flags & 1) OperatorDelete(self);
    return self;
}

}  // namespace

// =====================================================================================================================
// vtordisp thunks: `sub ecx, dword ptr [ecx - 4]` (3 bytes) then `jmp <body>`; the stack arguments reach the body
// untouched and the body pops them. Argument counts of the Build / OnScroll bodies: the stand-ins c3ej gave them
// (log/c3/alt/c3ej_fx/40_disp.js: 0x0048c420 two, 0x0048a5d0 / 0x0049d6d0 / 0x004a2400 / 0x004a3880 four); the deleting
// destructors end `ret 4` (0x004a308b, 0x004a49bb).
// =====================================================================================================================
// 0x0048cd10  ListBox::onScroll_vtordisp -> 0x0048c420 (jmp at 0x0048cd13)
F2_t ListBoxOnScrollVd_orig;
int __fastcall ListBoxOnScrollVd_re(void* p, void*, int a, int b) { return reinterpret_cast<F2_t>(0x0048c420)(VtordispThis(p), 0, a, b); }
// 0x0048cd60  ListBox::build_vtordisp -> 0x0048a5d0 (jmp at 0x0048cd63)
F4_t ListBoxBuildVd_orig;
int __fastcall ListBoxBuildVd_re(void* p, void*, int a, int b, int c, int d) { return reinterpret_cast<F4_t>(0x0048a5d0)(VtordispThis(p), 0, a, b, c, d); }
// 0x00491bf0  panelBuildRow_vtordisp -> 0x0049d6d0 (jmp at 0x00491bf3)
F4_t PanelBuildRowVd_orig;
int __fastcall PanelBuildRowVd_re(void* p, void*, int a, int b, int c, int d) { return reinterpret_cast<F4_t>(0x0049d6d0)(VtordispThis(p), 0, a, b, c, d); }
// 0x004a3040  panelRowDeletingDtor_vtordisp -> 0x004a3050 (jmp at 0x004a3043)
F1_t PanelRowDeletingDtorVd_orig;
int __fastcall PanelRowDeletingDtorVd_re(void* p, void*, int a) { return reinterpret_cast<F1_t>(0x004a3050)(VtordispThis(p), 0, a); }
// 0x004a30f0  panelCreate4a2400_vtordisp -> 0x004a2400 (jmp at 0x004a30f3)
F4_t PanelCreate4a2400Vd_orig;
int __fastcall PanelCreate4a2400Vd_re(void* p, void*, int a, int b, int c, int d) { return reinterpret_cast<F4_t>(0x004a2400)(VtordispThis(p), 0, a, b, c, d); }
// 0x004a4970  panelSubDeletingDtor_vtordisp -> 0x004a4980 (jmp at 0x004a4973)
F1_t PanelSubDeletingDtorVd_orig;
int __fastcall PanelSubDeletingDtorVd_re(void* p, void*, int a) { return reinterpret_cast<F1_t>(0x004a4980)(VtordispThis(p), 0, a); }
// 0x004a4ab0  panelBuild4a3880_vtordisp -> 0x004a3880 (jmp at 0x004a4ab3)
F4_t PanelBuild4a3880Vd_orig;
int __fastcall PanelBuild4a3880Vd_re(void* p, void*, int a, int b, int c, int d) { return reinterpret_cast<F4_t>(0x004a3880)(VtordispThis(p), 0, a, b, c, d); }

// =====================================================================================================================
// Fixed this-adjustors: `sub ecx, <imm>` then `jmp <deleting destructor>` (each target ends `ret 4`).
// =====================================================================================================================
// 0x0049bfd0  screenObj_deleteDtor_adj5c: sub ecx, 0x5c (0x0049bfd0), jmp 0x0049bf70 (0x0049bfd3)
F1_t ScreenObjDeleteDtorAdj5c_orig;
int __fastcall ScreenObjDeleteDtorAdj5c_re(void* p, void*, int a) { return reinterpret_cast<F1_t>(0x0049bf70)(B(p) - 0x5c, 0, a); }
// 0x0043cbf0  editBoxTimer_deleteDtor_adj274: sub ecx, 0x274 (0x0043cbf0), jmp 0x0043cbd0 (0x0043cbf6)
F1_t EditBoxTimerDeleteDtorAdj274_orig;
int __fastcall EditBoxTimerDeleteDtorAdj274_re(void* p, void*, int a) { return reinterpret_cast<F1_t>(0x0043cbd0)(B(p) - 0x274, 0, a); }
// 0x00489140  Button::scalarDtor_adj274: sub ecx, 0x274 (0x00489140), jmp 0x00488630 (0x00489146)
F1_t ButtonScalarDtorAdj274_orig;
int __fastcall ButtonScalarDtorAdj274_re(void* p, void*, int a) { return reinterpret_cast<F1_t>(0x00488630)(B(p) - 0x274, 0, a); }
// 0x004918d0  Dialog::scalarDtor_adj274: sub ecx, 0x274 (0x004918d0), jmp 0x004916f0 (0x004918d6)
F1_t DialogScalarDtorAdj274_orig;
int __fastcall DialogScalarDtorAdj274_re(void* p, void*, int a) { return reinterpret_cast<F1_t>(0x004916f0)(B(p) - 0x274, 0, a); }
// 0x0049c820  globalObjDeleteDtor49c800_adj274: sub ecx, 0x274 (0x0049c820), jmp 0x0049c800 (0x0049c826)
F1_t GlobalObjDeleteDtorAdj274_orig;
int __fastcall GlobalObjDeleteDtorAdj274_re(void* p, void*, int a) { return reinterpret_cast<F1_t>(0x0049c800)(B(p) - 0x274, 0, a); }

// =====================================================================================================================
// Scalar deleting destructors (shape above): the destructor, then operator delete 0x004a4ffc when bit 0 of the flag
// argument is set; eax = this in both cases.
// =====================================================================================================================
// 0x00488630  Button::scalarDtor: call 0x00488650 (0x00488633), flag test 0x00488638, delete 0x00488640
F1_t ButtonScalarDtor_orig;
int __fastcall ButtonScalarDtor_re(void* self, void*, int flags) { return reinterpret_cast<int>(ScalarDeleting<0x00488650>(self, flags)); }
// 0x004916f0  Dialog::scalarDtor: call 0x00491710 (0x004916f3), flag test 0x004916f8, delete 0x00491700
F1_t DialogScalarDtor_orig;
int __fastcall DialogScalarDtor_re(void* self, void*, int flags) { return reinterpret_cast<int>(ScalarDeleting<0x00491710>(self, flags)); }
// 0x0049c800  globalObjDeleteDtor49c800: call 0x0049c780 (0x0049c803), flag test 0x0049c808, delete 0x0049c810
F1_t GlobalObjDeleteDtor_orig;
int __fastcall GlobalObjDeleteDtor_re(void* self, void*, int flags) { return reinterpret_cast<int>(ScalarDeleting<0x0049c780>(self, flags)); }
// 0x0043cbd0  editBoxTimer_deleteDtor: call 0x0043cb50 (0x0043cbd3), flag test 0x0043cbd8, delete 0x0043cbe0
F1_t EditBoxTimerDeleteDtor_orig;
int __fastcall EditBoxTimerDeleteDtor_re(void* self, void*, int flags) { return reinterpret_cast<int>(ScalarDeleting<0x0043cb50>(self, flags)); }
// 0x00404260  deletingDtor_404260: call 0x00404200 (0x00404263), flag test 0x00404268, delete 0x00404270
F1_t DeletingDtor404260_orig;
int __fastcall DeletingDtor404260_re(void* self, void*, int flags) { return reinterpret_cast<int>(ScalarDeleting<0x00404200>(self, flags)); }

// 0x0049bf70  screenObj_deleteDtor: call 0x0049bfc0 (0x0049bf73); eax = [this+0x60] (0x0049bf7c); [this+0x5c] =
// 0x004ba278 (0x0049bf7f); the list cookie [0x00839650] = eax (0x0049bf86). When bit 0 of the flag is set (0x0049bf8b)
// and this != 0 (0x0049bf90): free 0x004a5007 (CRT free, not operator delete) only when the saved +0x60 was 0
// (0x0049bf94..0x0049bf99), and [0x00839650] = 0 either way (0x0049bfa1). eax = this (0x0049bfab).
F1_t ScreenObjDeleteDtor_orig;
int __fastcall ScreenObjDeleteDtor_re(void* self, void*, int flags) {
    reinterpret_cast<V0_t>(0x0049bfc0)(self, 0);
    const unsigned saved = *reinterpret_cast<unsigned*>(B(self) + 0x60);
    *reinterpret_cast<unsigned*>(B(self) + 0x5c) = 0x004ba278u;
    G<unsigned>(0x00839650) = saved;
    if ((flags & 1) && self != 0) {
        if (saved == 0) CrtFree(self);
        G<unsigned>(0x00839650) = 0;
    }
    return reinterpret_cast<int>(self);
}

// 0x004a3050  panelRowDeletingDtor: `this` is the Window sub-object; the full object is this - 0x8c (0x004a3051).
// viewFieldDtor4a2330 on this (0x004a3060), ListModel::dtor 0x00489370 on full + 0x604 (0x004a3065..0x004a306b),
// Base4805a0::dtor 0x004805a0 on this (0x004a3072), operator delete of the full object when bit 0 of the flag is set
// (0x004a3077..0x004a307f); eax = the full object (0x004a3087).
F1_t PanelRowDeletingDtor_orig;
int __fastcall PanelRowDeletingDtor_re(void* self, void*, int flags) {
    char* full = B(self) - 0x8c;
    reinterpret_cast<V0_t>(0x004a2330)(self, 0);
    reinterpret_cast<V0_t>(0x00489370)(full + 0x604, 0);
    reinterpret_cast<V0_t>(0x004805a0)(self, 0);
    if (flags & 1) OperatorDelete(full);
    return reinterpret_cast<int>(full);
}

// 0x004a4980  panelSubDeletingDtor: full object = this - 0xe0 (0x004a4981); panelSubDtor4a3480 on this (0x004a4990),
// ListModel::dtor on full + 0x658 (0x004a4995..0x004a499b), Base4805a0::dtor on this (0x004a49a2), operator delete of
// the full object when bit 0 of the flag is set (0x004a49a7..0x004a49af); eax = the full object (0x004a49b7).
F1_t PanelSubDeletingDtor_orig;
int __fastcall PanelSubDeletingDtor_re(void* self, void*, int flags) {
    char* full = B(self) - 0xe0;
    reinterpret_cast<V0_t>(0x004a3480)(self, 0);
    reinterpret_cast<V0_t>(0x00489370)(full + 0x658, 0);
    reinterpret_cast<V0_t>(0x004805a0)(self, 0);
    if (flags & 1) OperatorDelete(full);
    return reinterpret_cast<int>(full);
}

// =====================================================================================================================
// Static initialisation of the global root window 0x00519a60.
// =====================================================================================================================
// 0x00404360  staticInit_404360: View4804a0::ctor 0x004804a0 on this (0x00404363), then the root-window vtables:
// [this] = 0x004ba2fc (0x00404368, the table whose slots 29..39 are the rootWin_* input handlers, exe_13.tsv) and
// [this+0x274] = 0x004ba2e8 (0x0040436e); eax = this (0x00404378). No stack argument (`ret` 0x0040437b).
F0_t StaticInit404360_orig;
int __fastcall StaticInit404360_re(void* self, void*) {
    reinterpret_cast<F0_t>(0x004804a0)(self, 0);
    *reinterpret_cast<unsigned*>(B(self)) = 0x004ba2fcu;
    *reinterpret_cast<unsigned*>(B(self) + 0x274) = 0x004ba2e8u;
    return reinterpret_cast<int>(self);
}
// 0x00404320  staticInit_00404320: mov ecx, 0x00519a60 (0x00404320); jmp 0x00404360 (0x00404325): its eax is
// staticInit_404360's.
C0_t StaticInit404320_orig;
int __cdecl StaticInit404320_re() { return reinterpret_cast<F0_t>(0x00404360)(reinterpret_cast<void*>(0x00519a60), 0); }

// Dynamic-initializer stubs: `call <init>` then `jmp <register>` (eax = the register function's).
// 0x0043d450  dynInit_arrayStaticInit_43d460: call 0x0043d460 (0x0043d450), jmp 0x0043d490 (0x0043d455)
C0_t DynInitArrayStaticInit43d460_orig;
int __cdecl DynInitArrayStaticInit43d460_re() {
    reinterpret_cast<C0_t>(0x0043d460)();
    return reinterpret_cast<C0_t>(0x0043d490)();
}
// 0x004a0120  dynInit_initLogFile: call 0x004a0130 (0x004a0120), jmp 0x004a0140 (0x004a0125)
C0_t DynInitInitLogFile_orig;
int __cdecl DynInitInitLogFile_re() {
    reinterpret_cast<C0_t>(0x004a0130)();
    return reinterpret_cast<C0_t>(0x004a0140)();
}
// 0x004a0130  initLogFile: push 0x004e4a44 (0x004a0130), ecx = 0x00840930 (0x004a0135), call 0x004a0180 (0x004a013a,
// loggerCtor: thiscall, it pops the one argument: no `add esp` follows), ret with its eax (0x004a013f).
C0_t InitLogFile_orig;
int __cdecl InitLogFile_re() { return reinterpret_cast<F1_t>(0x004a0180)(reinterpret_cast<void*>(0x00840930), 0, 0x004e4a44); }

// =====================================================================================================================
// Root-window input handlers (slots of the vtable 0x004ba2fc, exe_13.tsv), the three bodies without a callee.
// =====================================================================================================================
// 0x00465740  rootWin_onUnpoint2: [0x00822d8c] = -1 (0x00465740), `ret 8` (0x0046574a) with eax untouched (void).
typedef void(__fastcall* V2_t)(void* ecx, void* edx, int a, int b);
V2_t RootWinOnUnpoint2_orig;
void __fastcall RootWinOnUnpoint2_re(void*, void*, int, int) { G<int>(0x00822d8c) = -1; }
// 0x004657d0  rootWin_onChar0: [0x0083821c] = 0 (0x004657d0), eax = 0 (0x004657da), `ret 4`.
F1_t RootWinOnChar0_orig;
int __fastcall RootWinOnChar0_re(void*, void*, int) { G<int>(0x0083821c) = 0; return 0; }
// 0x004657b0  rootWin_onChar: the argument (0x004657b0) is stored to [0x00822b98] (0x004657b4) and [0x0083821c]
// (0x004657b9); eax = 0 (0x004657be), `ret 4`.
F1_t RootWinOnChar_orig;
int __fastcall RootWinOnChar_re(void*, void*, int ch) {
    G<int>(0x00822b98) = ch;
    G<int>(0x0083821c) = ch;
    return 0;
}

// The two press handlers share one body except the value stored to [0x00822d68]. scaleX 0x00404970 (cdecl, one
// argument) is called with 2 twice (0x004657e2 / 0x004657ef, the `add esp, 8` at 0x00465807 pops both).
typedef int(__cdecl* ScaleX_t)(int v);
static int RootWinPress(int x, int y, int mode) {
    const int sx = reinterpret_cast<ScaleX_t>(0x00404970)(2);       // 0x004657e4
    G<int>(0x00822d58) = (x + x) / sx;                               // lea / cdq / idiv, 0x004657f1..0x004657f7
    const int sy = reinterpret_cast<ScaleX_t>(0x00404970)(2);       // 0x004657fc
    G<int>(0x00822d60) = x;                                          // 0x0046580a
    G<int>(0x00822d64) = y;                                          // 0x00465813
    G<int>(0x00822d5c) = (y + y) / sy;                               // 0x00465810..0x0046581e
    G<int>(0x00822d68) = mode;                                       // 0x00465828 (1) / 0x0046587d (2)
    G<int>(0x00822d78) = 1;                                          // 0x0046582d
    G<int>(0x00822d74) = 1;                                          // 0x00465832
    return 1;                                                        // 0x00465823
}
// 0x004657e0  rootWin_onPress1b: RootWinPress with [0x00822d68] = 1, `ret 8` (0x00465837).
F2_t RootWinOnPress1b_orig;
int __fastcall RootWinOnPress1b_re(void*, void*, int x, int y) { return RootWinPress(x, y, 1); }
// 0x00465840  rootWin_onPress2b: RootWinPress with [0x00822d68] = 2 (0x0046587d), `ret 8` (0x0046589c).
F2_t RootWinOnPress2b_orig;
int __fastcall RootWinOnPress2b_re(void*, void*, int x, int y) { return RootWinPress(x, y, 2); }

// 0x00465750  rootWin_onKey (a, code), `ret 8`: [0x00822b9c] = 0 when 0x60 <= code <= 0x69 (signed, 0x00465754..
// 0x0046575e), else a (0x0046576e); [0x00822b98] = code (0x00465776); [0x00838208] = (code == 0x1000d) (0x0046577b /
// 0x00465785 / 0x0046578a). When [0x005aa774] is 0 (0x00465790), or timeGetTime (IAT 0x004ba21c, 0x00465792) is above it
// unsigned (re-read at 0x00465798, `jbe` 0x0046579e), the byte [0x0083ab9c] = 0 (0x004657a0). Returns 1 (0x004657a7).
typedef unsigned(__stdcall* TimeGetTime_t)();
F2_t RootWinOnKey_orig;
int __fastcall RootWinOnKey_re(void*, void*, int a, int code) {
    G<int>(0x00822b9c) = (code >= 0x60 && code <= 0x69) ? 0 : a;
    G<int>(0x00822b98) = code;
    G<int>(0x00838208) = code == 0x1000d ? 1 : 0;
    if (G<unsigned>(0x005aa774) == 0 || (*reinterpret_cast<TimeGetTime_t*>(0x004ba21c))() > G<unsigned>(0x005aa774))
        G<unsigned char>(0x0083ab9c) = 0;
    return 1;
}

// =====================================================================================================================
// 0x00401c00  tickAnimals: walks the 0x3c-byte records from 0x0056d1d8 (0x00401c04) to 0x0056e948 (0x00401c1c, 100
// records) with an index from 0 (0x00401c02, 0x00401c1b); for every record whose first word is not 0xffff (0x00401c09)
// it calls placeAnimal 0x004017d0 (cdecl) with the index (0x00401c0f..0x00401c15). No result (eax is placeAnimal's last
// value or the caller's: void).
// =====================================================================================================================
typedef void(__cdecl* PlaceAnimal_t)(int i);
C0_t TickAnimals_orig;
void __cdecl TickAnimals_re() {
    int i = 0;
    for (unsigned rec = 0x0056d1d8; rec < 0x0056e948; rec += 0x3c, ++i)
        if (*reinterpret_cast<unsigned short*>(rec) != 0xffff) reinterpret_cast<PlaceAnimal_t>(0x004017d0)(i);
}

// 0x004041c0  resetPair (cdecl, one argument i): Stream482dd0::dtor 0x00482dd0 on 0x0050fc20 + i * 0x44 (0x004041c5..
// 0x004041d3) and Flic::reset 0x00481ba0 on 0x004f65c8 + i * 0x84 (0x004041d8..0x004041e6), which leaves eax.
typedef int(__cdecl* C1_t)(int a);
C1_t ResetPair_orig;
int __cdecl ResetPair_re(int i) {
    reinterpret_cast<V0_t>(0x00482dd0)(reinterpret_cast<void*>(0x0050fc20 + i * 0x44), 0);
    return reinterpret_cast<F0_t>(0x00481ba0)(reinterpret_cast<void*>(0x004f65c8 + i * 0x84), 0);
}

// 0x00491c10  initSinTable: for i = 0 .. 255 (the stores run from 0x0083b9f4 to 0x0083bdf4, 0x00491c40) g_sinTable[i] =
// __ftol(sin(i * [0x004bba50]) * [0x004bba48]) (fild / fmul / fsin / fmul / call 0x004a6030, 0x00491c1f..0x00491c31);
// then sinScaled 0x00491c70 (0x40000000, 0x64) and cosScaled 0x00491d80 (0xa02d82d8, 0x64) (cdecl, popped together at
// 0x00491c64). The x87 steps are reproduced with the same instructions (fsin is not affected by the precision control,
// so a C sin() would not be the same computation); __ftol is the exe's own. eax at the `ret` (0x00491c69) is cosScaled's
// result (only pops follow the call), so that is the return value.
C0_t InitSinTable_orig;
int __cdecl InitSinTable_re() {
    for (int i = 0; i < 256; ++i) {
        int v;
        int idx = i;
        __asm {
            fild dword ptr [idx]
            fmul qword ptr ds:[0x004bba50]
            fsin
            fmul qword ptr ds:[0x004bba48]
            mov eax, 0x004a6030
            call eax
            mov v, eax
        }
        reinterpret_cast<int*>(0x0083b9f4)[i] = v;
    }
    typedef int(__cdecl* C2_t)(int a, int b);
    reinterpret_cast<C2_t>(0x00491c70)(0x40000000, 0x64);
    return reinterpret_cast<C2_t>(0x00491d80)(static_cast<int>(0xa02d82d8u), 0x64);
}

// =====================================================================================================================
// 0x00482e20  Link482::resolve: eax = this (0x00482e20); when [this+4] is 0 (0x00482e25) return 0 (0x00482e29);
// otherwise Image::step 0x00482420 with ecx = [this+4] and the stack arguments (this, 0) (pushes 0x00482e2c /
// 0x00482e2e, the callee pops), returning its eax (0x00482e34). No stack argument of its own (`ret`).
// =====================================================================================================================
F0_t Link482Resolve_orig;
int __fastcall Link482Resolve_re(void* self, void*) {
    void* img = *reinterpret_cast<void**>(B(self) + 4);
    if (img == 0) return 0;
    return reinterpret_cast<F2_t>(0x00482420)(img, 0, reinterpret_cast<int>(self), 0);
}

// =====================================================================================================================
// Second tranche (same session).
// =====================================================================================================================
// Scalar deleting destructors of the Snd / Screen classes (shape of ScalarDeleting above):
// 0x004841c0  Snd::scalarDtor4841c0: call 0x004841e0 (0x004841c3), flag test 0x004841c8, delete 0x004841d0
F1_t SndScalarDtor4841c0_orig;
int __fastcall SndScalarDtor4841c0_re(void* self, void*, int flags) { return reinterpret_cast<int>(ScalarDeleting<0x004841e0>(self, flags)); }
// 0x00484880  SndMusic::scalarDtor: call 0x004848a0 (0x00484883), flag test 0x00484888, delete 0x00484890
F1_t SndMusicScalarDtor_orig;
int __fastcall SndMusicScalarDtor_re(void* self, void*, int flags) { return reinterpret_cast<int>(ScalarDeleting<0x004848a0>(self, flags)); }
// 0x004852b0  Snd::scalarDtor4852b0: call 0x004852d0 (0x004852b3), flag test 0x004852b8, delete 0x004852c0
F1_t SndScalarDtor4852b0_orig;
int __fastcall SndScalarDtor4852b0_re(void* self, void*, int flags) { return reinterpret_cast<int>(ScalarDeleting<0x004852d0>(self, flags)); }
// 0x004853b0  Screen::scalarDtor: call 0x004853d0 (0x004853b3), flag test 0x004853b8, delete 0x004853c0
F1_t ScreenScalarDtor_orig;
int __fastcall ScreenScalarDtor_re(void* self, void*, int flags) { return reinterpret_cast<int>(ScalarDeleting<0x004853d0>(self, flags)); }

// 0x00404200  C404200_dtor (SEH frame 0x00404200..0x0040420e, state -1 at the end): [this] = 0x004ba2dc (0x0040421d),
// Stream482dd0::dtor 0x00482dd0 on this (0x0040422b), [this+8] = 0x004ba2d8 (0x0040423b, Surface's vtable) and
// Surface_free 0x00473ae0 on this + 8 (0x00404230 / 0x00404241), whose eax is left (no later eax write).
F0_t C404200Dtor_orig;
int __fastcall C404200Dtor_re(void* self, void*) {
    *reinterpret_cast<unsigned*>(B(self)) = 0x004ba2dcu;
    reinterpret_cast<V0_t>(0x00482dd0)(self, 0);
    *reinterpret_cast<unsigned*>(B(self) + 8) = 0x004ba2d8u;
    return reinterpret_cast<F0_t>(0x00473ae0)(B(self) + 8, 0);
}

// 0x004914d0  Dialog::subDtor: Dialog::viewDtor 0x00491500 on this + 0x1fc (0x004914d4..0x004914dc), ListModel::dtor
// 0x00489370 on this + 0x774 (0x004914e1..0x004914e7), Base4805a0::dtor 0x004805a0 on this + 0x1fc (0x004914ee), whose
// eax is left.
F0_t DialogSubDtor_orig;
int __fastcall DialogSubDtor_re(void* self, void*) {
    reinterpret_cast<V0_t>(0x00491500)(B(self) + 0x1fc, 0);
    reinterpret_cast<V0_t>(0x00489370)(B(self) + 0x774, 0);
    return reinterpret_cast<F0_t>(0x004805a0)(B(self) + 0x1fc, 0);
}

// The scrollbar creators. None of the four reads ecx, so the incoming ecx reaches scrollbarCreate 0x00496330 (7 stack
// arguments, `ret 0x1c`) unchanged: the reimplementations forward it.
typedef int(__fastcall* F5_t)(void* ecx, void* edx, int a, int b, int c, int d, int e);
typedef int(__fastcall* F7_t)(void* ecx, void* edx, int a, int b, int c, int d, int e, int f, int g);
// 0x00496690  scrollbarCreateA_impl (a1..a5, `ret 0x14`): 3 when a4 == 0 (0x00496694) or a3 == 0 (0x0049669c), else
// scrollbarCreate(a1, a2, [0x0083ff10], a3, a4, a5, 0) (pushes 0x004966a5..0x004966b9, call 0x004966ba).
F5_t ScrollbarCreateAImpl_orig;
int __fastcall ScrollbarCreateAImpl_re(void* ecx, void*, int a1, int a2, int a3, int a4, int a5) {
    if (a4 == 0 || a3 == 0) return 3;
    return reinterpret_cast<F7_t>(0x00496330)(ecx, 0, a1, a2, G<int>(0x0083ff10), a3, a4, a5, 0);
}
// 0x004966d0  scrollbarCreateB_impl: the same tests (0x004966d4, 0x004966dc), then scrollbarCreate(a1, a2, a3,
// [0x0083ff10], a4, a5, 0) (pushes 0x004966e5..0x004966fa, call 0x004966fb): the global moves one place right.
F5_t ScrollbarCreateBImpl_orig;
int __fastcall ScrollbarCreateBImpl_re(void* ecx, void*, int a1, int a2, int a3, int a4, int a5) {
    if (a4 == 0 || a3 == 0) return 3;
    return reinterpret_cast<F7_t>(0x00496330)(ecx, 0, a1, a2, a3, G<int>(0x0083ff10), a4, a5, 0);
}
// 0x00496710  scrollbarCreateA: [0x0083ff60] = 1 (0x00496729), then scrollbarCreateA_impl with the five arguments in
// order (0x00496710..0x00496733) and its eax.
F5_t ScrollbarCreateA_orig;
int __fastcall ScrollbarCreateA_re(void* ecx, void*, int a1, int a2, int a3, int a4, int a5) {
    G<int>(0x0083ff60) = 1;
    return reinterpret_cast<F5_t>(0x00496690)(ecx, 0, a1, a2, a3, a4, a5);
}
// 0x00496740  scrollbarCreateB: [0x0083ff60] = 1 (0x00496759), then scrollbarCreateB_impl (0x00496763).
F5_t ScrollbarCreateB_orig;
int __fastcall ScrollbarCreateB_re(void* ecx, void*, int a1, int a2, int a3, int a4, int a5) {
    G<int>(0x0083ff60) = 1;
    return reinterpret_cast<F5_t>(0x004966d0)(ecx, 0, a1, a2, a3, a4, a5);
}

// 0x00482e40  Link482b::send (a1, a2, a3; `ret 0xc`): 0 when [this+4] is 0 (0x00482e45..0x00482e4b); otherwise
// Image::seek 0x00482490 on [this+4] with (a1, a2, this, a3) (pushes 0x00482e52..0x00482e5d) and its eax.
typedef int(__fastcall* F3_t)(void* ecx, void* edx, int a, int b, int c);
F3_t Link482bSend_orig;
int __fastcall Link482bSend_re(void* self, void*, int a1, int a2, int a3) {
    void* img = *reinterpret_cast<void**>(B(self) + 4);
    if (img == 0) return 0;
    return reinterpret_cast<F4_t>(0x00482490)(img, 0, a1, a2, reinterpret_cast<int>(self), a3);
}

// 0x00482420  Image::step (link, a2; `ret 8`): the link's frame counter +0x3c is incremented and stored (0x0048242d /
// 0x00482434 / 0x00482438) before decodeImageChunks 0x00482570 (this, args: the link's old chunk pointer +0x38, link, a2;
// 0x0048242c..0x0048243b). Then, when the counter equals the word +0x62 of the header [this+0x58] (0x00482440..
// 0x0048244e), the chunk pointer restarts at frame [link+0x40] of the table [this+0x48] (0x00482450..0x0048245a) and
// the counter is 0 (0x00482461); in both arms the chunk pointer then advances by the dword it points at (0x0048245f /
// 0x00482468 and 0x00482474..0x0048247f). Returns link + 8 (0x0048246a / 0x0048247c).
F2_t ImageStep_orig;
int __fastcall ImageStep_re(void* self, void*, int linkv, int a2) {
    char* link = reinterpret_cast<char*>(linkv);
    const unsigned chunk = *reinterpret_cast<unsigned*>(link + 0x38);
    *reinterpret_cast<int*>(link + 0x3c) += 1;
    reinterpret_cast<F3_t>(0x00482570)(self, 0, static_cast<int>(chunk), linkv, a2);
    char* hdr = *reinterpret_cast<char**>(B(self) + 0x58);
    unsigned p;
    if (*reinterpret_cast<int*>(link + 0x3c) == static_cast<int>(*reinterpret_cast<unsigned short*>(hdr + 0x62))) {
        char* table = *reinterpret_cast<char**>(B(self) + 0x48);
        p = *reinterpret_cast<unsigned*>(table + *reinterpret_cast<int*>(link + 0x40) * 4);
        *reinterpret_cast<unsigned*>(link + 0x38) = p;
        *reinterpret_cast<int*>(link + 0x3c) = 0;
    } else {
        p = *reinterpret_cast<unsigned*>(link + 0x38);
    }
    *reinterpret_cast<unsigned*>(link + 0x38) = p + *reinterpret_cast<unsigned*>(p);
    return reinterpret_cast<int>(link + 8);
}

// 0x00482490  Image::seek (frame, pos, link, a4; `ret 0x10`). hdr = [this+0x58]; 0 when it is NULL (0x00482499). pos ==
// -1 (0x004824aa) selects frame slot `frame` at position 0. Otherwise, with the mask m = [hdr+0x70] != 0 (0x004824b7),
// 0 when bit (frame & 31) of m is clear (0x004824c9..0x004824cd), else the slot is the count of set bits of m below
// `frame` (0x004824d8..0x004824f5, the loop runs only for frame > 0); with m == 0 the slot is frame % word [hdr+0x60]
// (signed idiv, 0x004824f9..0x00482506). The target step is pos % word [hdr+0x62] (0x00482508..0x00482515). When the
// link's slot +0x40 differs (0x00482517), the link restarts there: +0x40 = slot, +0x3c = 0, +0x38 = [[this+0x48] +
// slot*4] (0x00482522..0x00482534), decodeImageChunks (this; that pointer, link, a4) (0x0048253a), then the chunk pointer
// advances by the dword it points at (0x0048253f..0x00482546). Then Image::step (link, 0) runs until +0x3c equals the
// target step (0x00482549..0x0048255b). Returns link + 8 (0x0048255d).
F4_t ImageSeek_orig;
int __fastcall ImageSeek_re(void* self, void*, int frame, int pos, int linkv, int a4) {
    char* hdr = *reinterpret_cast<char**>(B(self) + 0x58);
    if (hdr == 0) return 0;
    int slot;
    int p = pos;
    if (pos == -1) {
        slot = frame;
        p = 0;
    } else {
        const unsigned m = *reinterpret_cast<unsigned*>(hdr + 0x70);
        if (m != 0) {
            if ((m & (1u << (frame & 31))) == 0) return 0;
            int n = 0;
            for (int i = 0; i < frame; ++i)
                if (m & (1u << (i & 31))) ++n;
            slot = n;
        } else {
            slot = frame % static_cast<int>(*reinterpret_cast<unsigned short*>(hdr + 0x60));
        }
    }
    const int target = p % static_cast<int>(*reinterpret_cast<unsigned short*>(hdr + 0x62));
    char* link = reinterpret_cast<char*>(linkv);
    if (*reinterpret_cast<int*>(link + 0x40) != slot) {
        *reinterpret_cast<int*>(link + 0x40) = slot;
        *reinterpret_cast<int*>(link + 0x3c) = 0;
        char* table = *reinterpret_cast<char**>(B(self) + 0x48);
        const unsigned c = *reinterpret_cast<unsigned*>(table + slot * 4);
        *reinterpret_cast<unsigned*>(link + 0x38) = c;
        reinterpret_cast<F3_t>(0x00482570)(self, 0, static_cast<int>(c), linkv, a4);
        const unsigned q = *reinterpret_cast<unsigned*>(link + 0x38);
        *reinterpret_cast<unsigned*>(link + 0x38) = q + *reinterpret_cast<unsigned*>(q);
    }
    while (*reinterpret_cast<int*>(link + 0x3c) != target) reinterpret_cast<F2_t>(0x00482420)(self, 0, linkv, 0);
    return reinterpret_cast<int>(link + 8);
}

// 0x0043d670  freeTable (cdecl, i). base = [0x005a9370 + i*4] (0x0043d675); nothing when it is -1 (0x0043d683, eax = i).
// Else freeBlock 0x0043d520 (base, [0x0053f3e8 + i*4] * [0x005a5a2c + i*4]) (0x0043d699..0x0043d6a0), Surface_free
// 0x00473ae0 on the 0x2c-byte Surfaces 0x005aaa30 + ([base slot] + k) * 0x2c for k below the same product, both words
// and the base re-read every pass (0x0043d6b3..0x0043d6d2), then [0x0053f3e8 + i*4] = 0 (0x0043d6d4) and the base slot
// = -1 (0x0043d6dc). eax is the last callee's.
typedef int(__cdecl* C2i_t)(int a, int b);
C1_t FreeTable_orig;
int __cdecl FreeTable_re(int i) {
    int* base = reinterpret_cast<int*>(0x005a9370) + i;
    if (*base == -1) return i;
    int* w = reinterpret_cast<int*>(0x0053f3e8) + i;
    int* h = reinterpret_cast<int*>(0x005a5a2c) + i;
    int r = reinterpret_cast<C2i_t>(0x0043d520)(*base, *w * *h);
    for (int k = 0; k < *w * *h; ++k)
        r = reinterpret_cast<F0_t>(0x00473ae0)(reinterpret_cast<void*>(0x005aaa30 + (*base + k) * 0x2c), 0);
    *w = 0;
    *base = -1;
    return r;
}

// 0x0040d6a0  tickMessage (no argument). While the delay [0x005694a4] is non-zero it is decremented and returned
// (0x0040d6a0..0x0040d6af). Otherwise the text 0x005a6d40 is copied to the message buffer 0x0051a068 (0x0040d6b2..
// 0x0040d6d8), then with mode byte [0x00569498] == 1 (0x0040d6e1) the counter [0x005a9ccc] counts up to 3 (signed,
// 0x0040d700..0x0040d70c) and the drawn value is the new count, or -1 when it was already >= 3; with another mode a
// non-zero counter is drawn and decremented (0x0040d6ed..0x0040d6f6; the `je` at 0x0040d6fc never jumps, eax is the
// non-zero old count), a zero one draws -1. drawWindow 0x0040d320 (cdecl) gets (0xf0, -8, [0x005a34ec], value)
// (0x0040d70e..0x0040d71d). Then nothing more when bit 15 of [0x0059e7b8] is set (0x0040d72a, eax = that word) or when
// g_date [0x00834170] % 3 == 0 (signed, 0x0040d72f..0x0040d73e, eax = the quotient); else [0x005a7144] is decremented
// (0x0040d740..0x0040d748, eax = the new value) and when it is <= 0 the mode byte is cleared (0x0040d74f).
typedef void(__cdecl* DrawWindow_t)(int a, int b, int c, int d);
C0_t TickMessage_orig;
int __cdecl TickMessage_re() {
    int& delay = G<int>(0x005694a4);
    if (delay != 0) return --delay;
    strcpy(reinterpret_cast<char*>(0x0051a068), reinterpret_cast<const char*>(0x005a6d40));
    int& count = G<int>(0x005a9ccc);
    int v = -1;
    if (G<unsigned char>(0x00569498) == 1) {
        if (count < 3) v = ++count;
    } else if (count != 0) {
        v = count--;
    }
    reinterpret_cast<DrawWindow_t>(0x0040d320)(0xf0, -8, G<int>(0x005a34ec), v);
    const int flags = G<int>(0x0059e7b8);
    if (flags & 0x8000) return flags;
    const int date = G<int>(0x00834170);
    if (date % 3 == 0) return date / 3;
    const int left = --G<int>(0x005a7144);
    if (left <= 0) G<unsigned char>(0x00569498) = 0;
    return left;
}

// 0x0043d460  arrayStaticInit_43d460: the eh vector constructor 0x004a55d2 (stdcall, 5 arguments: no `add esp` follows)
// over 0xdd40 Surfaces of 0x2c bytes at 0x005aaa30 with Surface_ctor 0x00473ab0 and the destructor 0x004041f0 (pushes
// 0x0043d460..0x0043d471, call 0x0043d476), then a tail jump into Surface_ctor with ecx = 0x005aa748 (0x0043d47b /
// 0x0043d480), whose eax is returned.
typedef void(__stdcall* VecCtor_t)(void* p, int size, int count, void* ctor, void* dtor);
C0_t ArrayStaticInit43d460_orig;
int __cdecl ArrayStaticInit43d460_re() {
    reinterpret_cast<VecCtor_t>(0x004a55d2)(reinterpret_cast<void*>(0x005aaa30), 0x2c, 0xdd40, reinterpret_cast<void*>(0x00473ab0),
                                            reinterpret_cast<void*>(0x004041f0));
    return reinterpret_cast<F0_t>(0x00473ab0)(reinterpret_cast<void*>(0x005aa748), 0);
}

// =====================================================================================================================
// Third tranche: Snd destructor / stop, the movie service trio.
// =====================================================================================================================
// 0x004841e0  Snd::dtor (SEH frame 0x004841e0..0x004841ee): [this] = 0x004bac68 (0x004841fd); a non-NULL [this+0x50] goes
// to operator delete 0x004a4ffc and the field is cleared (0x00484203..0x0048421b); a non-NULL [this+0x40] goes to
// releaseSoundObj 0x00484110 (cdecl) and is cleared (0x00484222..0x00484232); then Snd::unlinkFromList 0x004844e0 on this
// (0x0048423b) and Snd::setVtbl4852d0 0x004852d0 (0x0048424a), which only stores 0x004badec at [this] and leaves eax, so
// eax is unlinkFromList's.
F0_t SndDtor_orig;
int __fastcall SndDtor_re(void* self, void*) {
    *reinterpret_cast<unsigned*>(B(self)) = 0x004bac68u;
    void*& buf = *reinterpret_cast<void**>(B(self) + 0x50);
    if (buf != 0) { OperatorDelete(buf); buf = 0; }
    void*& obj = *reinterpret_cast<void**>(B(self) + 0x40);
    if (obj != 0) { reinterpret_cast<Del_t>(0x00484110)(obj); obj = 0; }
    const int r = reinterpret_cast<F0_t>(0x004844e0)(self, 0);
    reinterpret_cast<V0_t>(0x004852d0)(self, 0);
    return r;
}

// 0x00484f00  Snd::stop484f00: result 0 (0x00484f04) unless [this+0x40] is non-NULL (0x00484f09), in which case it is
// that object's slot +0x14 (`call [eax + 0x14]` 0x00484f0f); [this+0x40] = 0 (0x00484f17); unless bit 1 of the byte
// [this+0x58] is set (0x00484f1e / 0x00484f20) this's own slot +0x80 runs (0x00484f26); bit 0 of [this+0x44] is cleared
// (0x00484f2c..0x00484f31).
F0_t SndStop484f00_orig;
int __fastcall SndStop484f00_re(void* self, void*) {
    int r = 0;
    void* obj = *reinterpret_cast<void**>(B(self) + 0x40);
    if (obj != 0) r = (*reinterpret_cast<F0_t*>(B(*reinterpret_cast<void**>(obj)) + 0x14))(obj, 0);
    *reinterpret_cast<void**>(B(self) + 0x40) = 0;
    if ((*reinterpret_cast<unsigned char*>(B(self) + 0x58) & 2) == 0)
        (*reinterpret_cast<V0_t*>(B(*reinterpret_cast<void**>(self)) + 0x80))(self, 0);
    *reinterpret_cast<unsigned*>(B(self) + 0x44) &= 0xfffffffeu;
    return r;
}

// The movie service functions (thiscall, no stack argument). logLine 0x004a0320 is cdecl with five arguments here.
typedef void(__cdecl* LogLine_t)(int fmt, int tag, unsigned t, int a, int b);
static inline unsigned TimeNow() { return (*reinterpret_cast<TimeGetTime_t*>(0x004ba21c))(); }
// 0x0049cae0  movieDecodeStart: logLine (0x004e493c, 0x004e4954, timeGetTime(), 0, 0) (0x0049cae1..0x0049caf8, the clock
// through IAT 0x004ba21c); 0 when bit 4 of the byte [this+0xb8] is set (0x0049cb06 / 0x0049cb08); otherwise moviePlayFrame
// 0x0049ccc0 (0x0049cb10, result unused) and movieStepReturn1 0x0049cb80 (this, 0) (0x0049cb15 / 0x0049cb19), whose eax is
// returned.
F0_t MovieDecodeStart_orig;
int __fastcall MovieDecodeStart_re(void* self, void*) {
    reinterpret_cast<LogLine_t>(0x004a0320)(0x004e493c, 0x004e4954, TimeNow(), 0, 0);
    if (*reinterpret_cast<unsigned char*>(B(self) + 0xb8) & 0x10) return 0;
    reinterpret_cast<F0_t>(0x0049ccc0)(self, 0);
    return reinterpret_cast<F1_t>(0x0049cb80)(self, 0, 0);
}
// 0x0049cb20  movieReadProcess: logLine (0x004e4960, 0x004e4954, timeGetTime(), 0, 0) (0x0049cb21..0x0049cb38). Unless bit
// 4 of [this+0xb8] is set (0x0049cb46 / 0x0049cb48): when moviePlayFrame answers non-zero (0x0049cb4c / 0x0049cb53) it
// repeats movieStepReturn1 (this, 0) then moviePlayFrame while both answer non-zero (0x0049cb55..0x0049cb6b). Then
// movieReadFrame 0x0049cb90 (0x0049cb6f), whose eax is returned.
F0_t MovieReadProcess_orig;
int __fastcall MovieReadProcess_re(void* self, void*) {
    reinterpret_cast<LogLine_t>(0x004a0320)(0x004e4960, 0x004e4954, TimeNow(), 0, 0);
    if ((*reinterpret_cast<unsigned char*>(B(self) + 0xb8) & 0x10) == 0 && reinterpret_cast<F0_t>(0x0049ccc0)(self, 0) != 0) {
        while (reinterpret_cast<F1_t>(0x0049cb80)(self, 0, 0) != 0 && reinterpret_cast<F0_t>(0x0049ccc0)(self, 0) != 0) {
        }
    }
    return reinterpret_cast<F0_t>(0x0049cb90)(self, 0);
}
// 0x0049c910  serviceMovieFrame: eax = [this+0xb8] (0x0049c913); nothing more, eax being that word, unless bit 15 is set
// (0x0049c919 / 0x0049c91c). With bit 4 set (0x0049c91e / 0x0049c920) it goes straight to movieReadProcess 0x0049cb20
// (0x0049c938); otherwise moviePlayFrame 0x0049ccc0 (0x0049c922) and then movieDecodeStart 0x0049cae0 (0x0049c92d) return
// a non-zero result at once (0x0049c929, 0x0049c934), and only when both answer 0 movieReadProcess runs. eax is the last
// callee's.
F0_t ServiceMovieFrame_orig;
int __fastcall ServiceMovieFrame_re(void* self, void*) {
    const unsigned flags = *reinterpret_cast<unsigned*>(B(self) + 0xb8);
    if ((flags & 0x8000) == 0) return static_cast<int>(flags);
    if ((flags & 0x10) == 0) {
        int r = reinterpret_cast<F0_t>(0x0049ccc0)(self, 0);
        if (r != 0) return r;
        r = reinterpret_cast<F0_t>(0x0049cae0)(self, 0);
        if (r != 0) return r;
    }
    return reinterpret_cast<F0_t>(0x0049cb20)(self, 0);
}

// =====================================================================================================================
// Fourth tranche: KeyTable::find, jinit_compress_master, loadGraphsyObject, renderSpriteQueue.
// =====================================================================================================================
typedef char*(__cdecl* StrChr_t)(const char* s, int c);
typedef int(__cdecl* StrCmpI_t)(const char* a, const char* b);
// 0x00488310  KeyTable::find (thiscall, `ret 8`: name, key). name is copied to a 0x200-byte local (0x00488322..
// 0x00488344); when _strchr 0x004a6170 finds no '.' in it (0x0048834b / 0x00488355) the extension 0x004e4418 (".txt")
// is appended (0x00488357..0x00488381). -1 unless __strcmpi 0x004ad4b0 (local, this+4) is 0 (0x0048838c / 0x00488396).
// A leading '#' of key is skipped (0x0048839f..0x004883a5). The entries at [this+0x110] are packed {dword value; char
// name[]}; the first one is compared before the count [this+0x104] is looked at (0x004883a6..0x004883c7); after each
// mismatch the name is skipped up to its NUL and one more byte (0x004883c9..0x004883da), the count is decremented and
// -1 returned when it reaches 0 (0x004883db..0x004883e0), else the next entry is compared (0x004883e2..0x004883f3). On a
// match (__strcmpi 0) the entry's value dword is returned (0x004883f5).
F2_t KeyTableFind_orig;
int __fastcall KeyTableFind_re(void* self, void*, int nameArg, int keyArg) {
    char buf[0x200];
    strcpy(buf, reinterpret_cast<const char*>(nameArg));
    if (reinterpret_cast<StrChr_t>(0x004a6170)(buf, 0x2e) == 0) strcat(buf, reinterpret_cast<const char*>(0x004e4418));
    const StrCmpI_t cmpi = reinterpret_cast<StrCmpI_t>(0x004ad4b0);
    if (cmpi(buf, B(self) + 4) != 0) return -1;
    const char* key = reinterpret_cast<const char*>(keyArg);
    if (*key == '#') ++key;
    char* entry = *reinterpret_cast<char**>(B(self) + 0x110);
    int count = *reinterpret_cast<int*>(B(self) + 0x104);
    char* name = entry + 4;
    while (cmpi(key, name) != 0) {
        while (*name != 0) ++name;
        ++name;
        if (--count == 0) return -1;
        entry = name;
        name = entry + 4;
    }
    return *reinterpret_cast<int*>(entry);
}

// 0x004b03a0  jinit_compress_master (IJG 6a jcinit.c; cdecl, one argument cinfo; every callee cdecl). jinit_c_master_control
// 0x004b51e0 (cinfo, 0) (0x004b03a8); unless raw_data_in [+0xa8] (0x004b03b8): jinit_color_converter 0x004b4a60,
// jinit_downsampler 0x004b4040, jinit_c_prep_controller 0x004b3a80 (cinfo, 0) (0x004b03bb..0x004b03c9);
// jinit_forward_dct 0x004b34b0 (0x004b03d2). With arith_code [+0xac] set (0x004b03e2) err->msg_code [err+0x14] = 1 and
// err->error_exit [err+0] (cinfo) (0x004b03e4..0x004b03f0); else jinit_phuff_encoder 0x004b1f30 when progressive_mode
// [+0xdc] is set, jinit_huff_encoder 0x004b12b0 when not (0x004b03f4..0x004b0406). jinit_c_coef_controller 0x004b06f0
// (cinfo, num_scans [+0xa0] > 1 (signed `jg`) || optimize_coding [+0xb0] != 0) (0x004b040b..0x004b042e),
// jinit_c_main_controller 0x004b0560 (cinfo, 0) (0x004b0436), jinit_marker_writer 0x004afaf0 (0x004b043c), then
// mem->realize_virt_arrays [[cinfo+4]+0x18] (0x004b0445) and marker->write_file_header [[cinfo+0x154]+4] (0x004b044f);
// eax is the last call's.
typedef int(__cdecl* J1_t)(void* cinfo);
typedef int(__cdecl* J2_t)(void* cinfo, int b);
static inline J1_t JSlot(void* cinfo, int field, int slot) {
    return *reinterpret_cast<J1_t*>(B(*reinterpret_cast<void**>(B(cinfo) + field)) + slot);
}
J1_t JinitCompressMaster_orig;
int __cdecl JinitCompressMaster_re(void* cinfo) {
    reinterpret_cast<J2_t>(0x004b51e0)(cinfo, 0);
    if (*reinterpret_cast<int*>(B(cinfo) + 0xa8) == 0) {
        reinterpret_cast<J1_t>(0x004b4a60)(cinfo);
        reinterpret_cast<J1_t>(0x004b4040)(cinfo);
        reinterpret_cast<J2_t>(0x004b3a80)(cinfo, 0);
    }
    reinterpret_cast<J1_t>(0x004b34b0)(cinfo);
    if (*reinterpret_cast<int*>(B(cinfo) + 0xac) != 0) {
        void* err = *reinterpret_cast<void**>(cinfo);
        *reinterpret_cast<int*>(B(err) + 0x14) = 1;
        JSlot(cinfo, 0, 0)(cinfo);
    } else if (*reinterpret_cast<int*>(B(cinfo) + 0xdc) != 0) {
        reinterpret_cast<J1_t>(0x004b1f30)(cinfo);
    } else {
        reinterpret_cast<J1_t>(0x004b12b0)(cinfo);
    }
    const int multi = (*reinterpret_cast<int*>(B(cinfo) + 0xa0) > 1 || *reinterpret_cast<int*>(B(cinfo) + 0xb0) != 0) ? 1 : 0;
    reinterpret_cast<J2_t>(0x004b06f0)(cinfo, multi);
    reinterpret_cast<J2_t>(0x004b0560)(cinfo, 0);
    reinterpret_cast<J1_t>(0x004afaf0)(cinfo);
    JSlot(cinfo, 4, 0x18)(cinfo);
    return JSlot(cinfo, 0x154, 4)(cinfo);
}

// 0x004a00a0  loadGraphsyObject (cdecl, one argument: the DLL path). NULL path: return it (0x004a00a4 / 0x004a00a8).
// LoadLibraryA through IAT 0x004ba0a8 (0x004a00aa), its handle stored in [0x0084092c] (0x004a00b2) and returned when 0
// (0x004a00b9). GetProcAddress (IAT 0x004ba0a4) for 0x004e4a2c ("get_graphsy_object_ptr") (0x004a00ba..0x004a00c0):
// NULL -> freeGraphsyObject 0x004a0060, return 0 (0x004a00ca / 0x004a00cf). Otherwise the export is called with no
// argument (0x004a00d3); a NULL result runs freeGraphsyObject (0x004a00db); [0x0083ad50] = the result either way
// (0x004a00e0), which is returned.
typedef HMODULE(__stdcall* LoadLibraryA_t)(const char* name);
typedef void*(__stdcall* GetProcAddress_t)(HMODULE h, const char* name);
C1_t LoadGraphsyObject_orig;
int __cdecl LoadGraphsyObject_re(int path) {
    if (path == 0) return 0;
    HMODULE h = (*reinterpret_cast<LoadLibraryA_t*>(0x004ba0a8))(reinterpret_cast<const char*>(path));
    G<HMODULE>(0x0084092c) = h;
    if (h == 0) return 0;
    C0_t factory = reinterpret_cast<C0_t>((*reinterpret_cast<GetProcAddress_t*>(0x004ba0a4))(h, reinterpret_cast<const char*>(0x004e4a2c)));
    if (factory == 0) { reinterpret_cast<C0_t>(0x004a0060)(); return 0; }
    const int obj = factory();
    if (obj == 0) reinterpret_cast<C0_t>(0x004a0060)();
    G<int>(0x0083ad50) = obj;
    return obj;
}

// 0x00463100  renderSpriteQueue (no argument; SEH frame 0x00463100..0x0046310e). A 0x58-byte local cache is built by
// Cache482fd0::ctor 0x00482fd0 (0x0046311d) and destroyed by Cache483010::dtor 0x00483010 (0x00463156), nothing else
// touching it. In between, from the head [0x0082c160] (0x00463122) until -1 (0x00463130 / 0x00463145):
// drawQueuedSprite 0x00462be0 (cdecl, the index) (0x00463136), next index [0x008361f4 + 4 * i] (0x0046313b). eax is
// the destructor's (Cache483070::reset 0x00483070 on a fresh cache leaves eax = 0).
typedef void(__cdecl* DrawQueued_t)(int i);
C0_t RenderSpriteQueue_orig;
int __cdecl RenderSpriteQueue_re() {
    char cache[0x58];
    reinterpret_cast<F0_t>(0x00482fd0)(cache, 0);
    for (int i = G<int>(0x0082c160); i != -1; i = reinterpret_cast<int*>(0x008361f4)[i])
        reinterpret_cast<DrawQueued_t>(0x00462be0)(i);
    return reinterpret_cast<F0_t>(0x00483010)(cache, 0);
}

// 0x0049d6d0  panelBuildRow (thiscall, `ret 0x10`: owner, a2, a3, style; `this` is a sub-object 0x59c into the panel,
// whose vbtable [this-0x59c] gives the Window base at +4 and a virtual base at +8). A NULL owner returns 3 (0x0049d6d7..
// 0x0049d6e5). panelLayoutText 0x0049d770 on this-0x59c (0x0049d6f0) returns a non-zero result at once (0x0049d6f7).
// Else a2 / a3 are stored at +0x28 / +0x2c of the virtual base (0x0049d6fc..0x0049d713), style gets bit 20 unless bit 12
// of [owner+0x9c] is set (`test ah, 0x10` 0x0049d720, 0x0049d725), and Window::create 0x004806c0 runs on the Window base
// with (a2, a3, [vbase+0x60], [vbase+0x64], 0, style, owner, 0, 0) (0x0049d72d..0x0049d761); its eax is returned.
typedef int(__fastcall* F9_t)(void* ecx, void* edx, int a, int b, int c, int d, int e, int f, int g, int h, int i);
F4_t PanelBuildRow_orig;
int __fastcall PanelBuildRow_re(void* self, void*, int owner, int a2, int a3, int style) {
    if (owner == 0) return 3;
    char* full = B(self) - 0x59c;
    const int r = reinterpret_cast<F0_t>(0x0049d770)(full, 0);
    if (r != 0) return r;
    const int* vbt = *reinterpret_cast<int**>(full);
    char* vb = full + vbt[2];
    *reinterpret_cast<int*>(vb + 0x28) = a2;
    *reinterpret_cast<int*>(vb + 0x2c) = a3;
    if ((*reinterpret_cast<unsigned*>(owner + 0x9c) & 0x1000) == 0) style |= 0x100000;
    return reinterpret_cast<F9_t>(0x004806c0)(full + vbt[1], 0, a2, a3, *reinterpret_cast<int*>(vb + 0x60),
                                              *reinterpret_cast<int*>(vb + 0x64), 0, style, owner, 0, 0);
}

// 0x0049cb90  movieReadFrame (thiscall, no stack argument; one local, the buffer pointer listPush writes). With no frame
// size [this+0xd8] (0x0049cb9e): listPush 0x004a4db0 on this+0x668 (&buf, 0, [this+0x940], 1) (0x0049cba0..0x0049cbb6);
// non-zero -> 0 (0x0049cbbd); else logLine (0x004e498c, 0x004e49b4, [this+0x940], [this+0x674], 0) (0x0049cbc6..
// 0x0049cbdb / 0x0049cc89). With a size n: listPush (&buf, n, [this+0x940], 0) (0x0049cbe0..0x0049cbf5); 0x12 -> 0
// (0x0049cbfa); a NULL buf -> logLine (0x004e498c, 0x004e499c, timeGetTime(), 0, 0) and 0 (0x0049cc06..0x0049cc2b).
// Then [this+0xd8] is read again (0x0049cc32) and read in pieces of at most 0x10000 (unsigned `ja` 0x0049cc4d) with
// mmioRead (IAT 0x004ba20c) ([this+0xbc], buf, piece) (0x0049cc51..0x0049cc5e), buf and the remaining count moved by
// each result (0x0049cc64..0x0049cc72); a 0 result clears bit 15 of [this+0xb8] and returns 0 (0x0049cca7..0x0049ccb8).
// After the read: logLine (0x004e498c, 0x004e4974, [this+0x940], [this+0x674], 0) (0x0049cc74..0x0049cc8e). Both
// success paths end with movieAdvanceFrame 0x0049c940 (0x0049cc98) and return 1.
typedef LONG(__stdcall* MmioRead_t)(void* h, char* p, LONG n);
F0_t MovieReadFrame_orig;
int __fastcall MovieReadFrame_re(void* self, void*) {
    char* p = B(self);
    char* buf = 0;
    const int n = *reinterpret_cast<int*>(p + 0xd8);
    if (n == 0) {
        if (reinterpret_cast<F4_t>(0x004a4db0)(p + 0x668, 0, reinterpret_cast<int>(&buf), 0, *reinterpret_cast<int*>(p + 0x940), 1) != 0)
            return 0;
        reinterpret_cast<LogLine_t>(0x004a0320)(0x004e498c, 0x004e49b4, *reinterpret_cast<unsigned*>(p + 0x940),
                                                *reinterpret_cast<int*>(p + 0x674), 0);
    } else {
        if (reinterpret_cast<F4_t>(0x004a4db0)(p + 0x668, 0, reinterpret_cast<int>(&buf), n, *reinterpret_cast<int*>(p + 0x940), 0) == 0x12)
            return 0;
        if (buf == 0) {
            reinterpret_cast<LogLine_t>(0x004a0320)(0x004e498c, 0x004e499c, TimeNow(), 0, 0);
            return 0;
        }
        unsigned left = *reinterpret_cast<unsigned*>(p + 0xd8);
        while (left != 0) {
            const LONG got = (*reinterpret_cast<MmioRead_t*>(0x004ba20c))(*reinterpret_cast<void**>(p + 0xbc), buf,
                                                                          left > 0x10000 ? 0x10000 : static_cast<LONG>(left));
            if (got == 0) {
                *reinterpret_cast<unsigned*>(p + 0xb8) &= 0xffff7fffu;
                return 0;
            }
            left -= got;
            buf += got;
        }
        reinterpret_cast<LogLine_t>(0x004a0320)(0x004e498c, 0x004e4974, *reinterpret_cast<unsigned*>(p + 0x940),
                                                *reinterpret_cast<int*>(p + 0x674), 0);
    }
    reinterpret_cast<V0_t>(0x0049c940)(self, 0);
    return 1;
}

// 0x0049ccc0  moviePlayFrame (thiscall, no stack argument; ebp = 0xffffefff, the bit-12 clear mask, 0x0049ccc7). With
// bit 4 of [this+0xb8] set it goes straight to the tail (0x0049ccd4). The target frame t is the clock object's slot
// +0x94 (on this+4, 0x0049cce4) when bit 0 is clear, else ((unsigned)([this+0x13c] * (timeGetTime() - [this+0x93c])) /
// [this+0x138]) / 1000 (`imul`, `div`, magic 0x10624dd3 `shr 6`: 0x0049ccee..0x0049cd1a). Against the current frame
// c = [this+0x944] (signed): t < c -> 0 (0x0049cd25); t == c without bit 12 -> 0 (0x0049cd2f..0x0049cd3a). logLine
// (0x004e49e4, 0x004e4954, timeGetTime(), t - c, 0) (0x0049cd44..0x0049cd62); t <= c (re-read) -> tail (0x0049cd72).
// t == c + 1: [this+0x674] != 0 -> clear bit 12 and step (0x0049cd85); else movieReadFrame 0x0049cb90, clear bit 12,
// tail (0x0049cd8b..0x0049cd98). Otherwise s = select 0x0049cf50 (t) (0x0049cda0); s == -1 with c < t: repeat {clear
// bit 12; movieStepReturn1 0x0049cb80 (0); a 0 answer -> movieReadFrame and another step (0)} while c < t
// (0x0049cdb4..0x0049cde5). logLine (0x004e49e4, 0x004e49d4, t, c, s) (0x0049cde7..0x0049cdfa). With c the current
// frame (0x0049cdff): t - c < s - t (signed, 0x0049ce10) -> t' = t - 1; when c < t' repeat {clear bit 12; step (1); a 0
// answer -> movieReadFrame} while frame < t' (0x0049ce14..0x0049ce41); clear bit 12; step (0) non-zero -> tail, else
// movieReadFrame and step (0) (0x0049ce43..0x0049ce69). Else when s > [this+0x940] - 1 (0x0049ce74): listTrim 0x004a4d30
// on this+0x668 (0x28), movieAdvanceFrame 0x0049c940 while [this+0x940] < s, movieReadFrame (0x0049ce76..0x0049cea1);
// when not: a local k = 0, listPeek 0x004a4ea0 (&k, 0) and listRemoveHead 0x004a4ed0 then listPeek again while k < s
// (0x0049cea3..0x0049cede). Both end with clear bit 12 and step (0) (0x0049cee0..0x0049ceea). Tail (0x0049ceef):
// frame == [this+0x938] - 1 clears bit 15, bit 12 is cleared, logLine (0x004e49e4, 0x004e49cc, timeGetTime(), 0, 0),
// return 1.
// a + b with the 32-bit wrap of `add` / `sub` / `inc` / `dec`
static inline int Wrap(int a, int b) { return static_cast<int>(static_cast<unsigned>(a) + static_cast<unsigned>(b)); }
F0_t MoviePlayFrame_orig;
int __fastcall MoviePlayFrame_re(void* self, void*) {
    char* p = B(self);
    unsigned& flags = *reinterpret_cast<unsigned*>(p + 0xb8);
    int& cur = *reinterpret_cast<int*>(p + 0x944);
    void* q = p + 0x668;
    const F0_t readFrame = reinterpret_cast<F0_t>(0x0049cb90);
    const F1_t step = reinterpret_cast<F1_t>(0x0049cb80);
    enum { kClearStep, kStep, kTail } tail = kTail;
    if ((flags & 0x10) == 0) {
        int t;
        if ((flags & 1) == 0) {
            void* clk = p + 4;
            t = (*reinterpret_cast<F0_t*>(B(*reinterpret_cast<void**>(clk)) + 0x94))(clk, 0);
        } else {
            const unsigned prod = *reinterpret_cast<unsigned*>(p + 0x13c) * (TimeNow() - *reinterpret_cast<unsigned*>(p + 0x93c));
            t = static_cast<int>(prod / *reinterpret_cast<unsigned*>(p + 0x138) / 1000u);
        }
        if (t < cur) return 0;
        if (t == cur && (flags & 0x1000) == 0) return 0;
        reinterpret_cast<LogLine_t>(0x004a0320)(0x004e49e4, 0x004e4954, TimeNow(), Wrap(t, -cur), 0);
        if (t <= cur) {
            tail = kTail;
        } else if (t == Wrap(cur, 1)) {
            if (*reinterpret_cast<int*>(p + 0x674) != 0) {
                tail = kClearStep;
            } else {
                readFrame(self, 0);
                flags &= 0xffffefffu;
                tail = kTail;
            }
        } else {
            const int s = reinterpret_cast<F1_t>(0x0049cf50)(self, 0, t);
            if (s == -1 && cur < t) {
                do {
                    flags &= 0xffffefffu;
                    if (step(self, 0, 0) == 0) { readFrame(self, 0); step(self, 0, 0); }
                } while (cur < t);
            }
            reinterpret_cast<LogLine_t>(0x004a0320)(0x004e49e4, 0x004e49d4, static_cast<unsigned>(t), cur, s);
            const int c = cur;
            if (Wrap(t, -c) < Wrap(s, -t)) {
                const int t1 = Wrap(t, -1);
                if (c < t1) {
                    do {
                        flags &= 0xffffefffu;
                        if (step(self, 0, 1) == 0) readFrame(self, 0);
                    } while (cur < t1);
                }
                flags &= 0xffffefffu;
                if (step(self, 0, 0) != 0) {
                    tail = kTail;
                } else {
                    readFrame(self, 0);
                    tail = kStep;
                }
            } else if (s > Wrap(*reinterpret_cast<int*>(p + 0x940), -1)) {
                reinterpret_cast<F1_t>(0x004a4d30)(q, 0, 0x28);
                while (*reinterpret_cast<int*>(p + 0x940) < s) reinterpret_cast<V0_t>(0x0049c940)(self, 0);
                readFrame(self, 0);
                tail = kClearStep;
            } else {
                int k = 0;
                reinterpret_cast<F2_t>(0x004a4ea0)(q, 0, reinterpret_cast<int>(&k), 0);
                while (k < s) {
                    reinterpret_cast<F0_t>(0x004a4ed0)(q, 0);
                    reinterpret_cast<F2_t>(0x004a4ea0)(q, 0, reinterpret_cast<int>(&k), 0);
                }
                tail = kClearStep;
            }
        }
        if (tail == kClearStep) flags &= 0xffffefffu;
        if (tail != kTail) step(self, 0, 0);
    }
    if (cur == Wrap(*reinterpret_cast<int*>(p + 0x938), -1)) flags &= 0xffff7fffu;
    flags &= 0xffffefffu;
    reinterpret_cast<LogLine_t>(0x004a0320)(0x004e49e4, 0x004e49cc, TimeNow(), 0, 0);
    return 1;
}

// 0x0044a380  applyTileWalls (cdecl: tile, x, y; returns a byte). For each side s = 0, 2, 4, 6 (0x0044a396, 0x0044a3f2..
// 0x0044a3f8): when wallHeight 0x00449330 (x, y, s) is non-zero and the byte [tile + 0x234 + s] is 0 (0x0044a3a3..
// 0x0044a3b0), wallHeight is called again and Terrain::setWall (the exe's import thunk 0x004a4f40, thiscall on the
// Terrain object [0x00820ed0]) gets (tile, s, that height, 1): the `push 1` at 0x0044a3b2 is left on the stack by the
// cdecl call (`add esp, 0xc` 0x0044a3bc) and becomes setWall's last argument. Otherwise wallHeight is called (again,
// 0x0044a3c5) and when it is 0 and the byte is non-zero setWall gets (tile, s, 0, 0) (0x0044a3cd..0x0044a3e8). The result
// (0x0044a3fa) is 1 when any setWall ran (0x0044a3ed), else 0.
typedef int(__cdecl* C3_t)(int a, int b, int c);
C3_t ApplyTileWalls_orig;
int __cdecl ApplyTileWalls_re(int tile, int x, int y) {
    const C3_t wallHeight = reinterpret_cast<C3_t>(0x00449330);
    int any = 0;
    for (int s = 0; s < 8; s += 2) {
        const unsigned char wall = *reinterpret_cast<unsigned char*>(tile + 0x234 + s);
        int h, last;
        if (wallHeight(x, y, s) != 0 && wall == 0) {
            h = wallHeight(x, y, s);
            last = 1;
        } else {
            if (wallHeight(x, y, s) != 0 || *reinterpret_cast<unsigned char*>(tile + 0x234 + s) == 0) continue;
            h = 0;
            last = 0;
        }
        reinterpret_cast<F4_t>(0x004a4f40)(G<void*>(0x00820ed0), 0, tile, s, h, last);
        any = 1;
    }
    return any;
}

// 0x0046ddd0  startNewCourse (no argument). [0x00822c88] is read first (0x0046ddd1). pickAProScreen 0x0043b610 (1, 1)
// (0x0046dddb): 0 is returned as it is (0x0046dde5). Otherwise [0x00822c88] gets the value read before (0x0046dde9),
// [0x00571fd4] = 1000 (0x0046ddf4), g_date [0x00834170] = 0x2c00 (0x0046ddfa), [0x0059e7b8] = 0x4000000 (0x0046de04),
// loadGameBrowserScreen 0x0043a8c0 (0x0046de0e): 0 -> [0x0059e7b8] = 0 and return 0 (0x0046de17); else loadGolfPro
// 0x0046c970 (0x0046de1e), [0x00571fd4] = 1000 again (0x0046de23), return 1.
typedef int(__cdecl* C2a_t)(int a, int b);
C0_t StartNewCourse_orig;
int __cdecl StartNewCourse_re() {
    const int saved = G<int>(0x00822c88);
    const int r = reinterpret_cast<C2a_t>(0x0043b610)(1, 1);
    if (r == 0) return r;
    G<int>(0x00822c88) = saved;
    G<int>(0x00571fd4) = 1000;
    G<int>(0x00834170) = 0x2c00;
    G<int>(0x0059e7b8) = 0x4000000;
    const int b = reinterpret_cast<C0_t>(0x0043a8c0)();
    if (b == 0) { G<int>(0x0059e7b8) = b; return b; }
    reinterpret_cast<C0_t>(0x0046c970)();
    G<int>(0x00571fd4) = 1000;
    return 1;
}

SG_HOOK("golf_clean.exe", 0x0044a380, applyTileWalls, ApplyTileWalls_re, ApplyTileWalls_orig);
SG_HOOK("golf_clean.exe", 0x0046ddd0, startNewCourse, StartNewCourse_re, StartNewCourse_orig);
SG_HOOK("golf_clean.exe", 0x0049ccc0, moviePlayFrame, MoviePlayFrame_re, MoviePlayFrame_orig);
SG_HOOK("golf_clean.exe", 0x0049d6d0, panelBuildRow, PanelBuildRow_re, PanelBuildRow_orig);
SG_HOOK("golf_clean.exe", 0x0049cb90, movieReadFrame, MovieReadFrame_re, MovieReadFrame_orig);
SG_HOOK("golf_clean.exe", 0x00488310, KeyTable_find, KeyTableFind_re, KeyTableFind_orig);
SG_HOOK("golf_clean.exe", 0x004b03a0, jinit_compress_master, JinitCompressMaster_re, JinitCompressMaster_orig);
SG_HOOK("golf_clean.exe", 0x004a00a0, loadGraphsyObject, LoadGraphsyObject_re, LoadGraphsyObject_orig);
SG_HOOK("golf_clean.exe", 0x00463100, renderSpriteQueue, RenderSpriteQueue_re, RenderSpriteQueue_orig);

SG_HOOK("golf_clean.exe", 0x004841e0, Snd_dtor, SndDtor_re, SndDtor_orig);
SG_HOOK("golf_clean.exe", 0x00484f00, Snd_stop484f00, SndStop484f00_re, SndStop484f00_orig);
SG_HOOK("golf_clean.exe", 0x0049cae0, movieDecodeStart, MovieDecodeStart_re, MovieDecodeStart_orig);
SG_HOOK("golf_clean.exe", 0x0049cb20, movieReadProcess, MovieReadProcess_re, MovieReadProcess_orig);
SG_HOOK("golf_clean.exe", 0x0049c910, serviceMovieFrame, ServiceMovieFrame_re, ServiceMovieFrame_orig);

SG_HOOK("golf_clean.exe", 0x004841c0, Snd_scalarDtor4841c0, SndScalarDtor4841c0_re, SndScalarDtor4841c0_orig);
SG_HOOK("golf_clean.exe", 0x00484880, SndMusic_scalarDtor, SndMusicScalarDtor_re, SndMusicScalarDtor_orig);
SG_HOOK("golf_clean.exe", 0x004852b0, Snd_scalarDtor4852b0, SndScalarDtor4852b0_re, SndScalarDtor4852b0_orig);
SG_HOOK("golf_clean.exe", 0x004853b0, Screen_scalarDtor, ScreenScalarDtor_re, ScreenScalarDtor_orig);
SG_HOOK("golf_clean.exe", 0x00404200, C404200_dtor, C404200Dtor_re, C404200Dtor_orig);
SG_HOOK("golf_clean.exe", 0x004914d0, Dialog_subDtor, DialogSubDtor_re, DialogSubDtor_orig);
SG_HOOK("golf_clean.exe", 0x00496690, scrollbarCreateA_impl, ScrollbarCreateAImpl_re, ScrollbarCreateAImpl_orig);
SG_HOOK("golf_clean.exe", 0x004966d0, scrollbarCreateB_impl, ScrollbarCreateBImpl_re, ScrollbarCreateBImpl_orig);
SG_HOOK("golf_clean.exe", 0x00496710, scrollbarCreateA, ScrollbarCreateA_re, ScrollbarCreateA_orig);
SG_HOOK("golf_clean.exe", 0x00496740, scrollbarCreateB, ScrollbarCreateB_re, ScrollbarCreateB_orig);
SG_HOOK("golf_clean.exe", 0x00482e40, Link482b_send, Link482bSend_re, Link482bSend_orig);
SG_HOOK("golf_clean.exe", 0x00482420, Image_step, ImageStep_re, ImageStep_orig);
SG_HOOK("golf_clean.exe", 0x00482490, Image_seek, ImageSeek_re, ImageSeek_orig);
SG_HOOK("golf_clean.exe", 0x0043d670, freeTable, FreeTable_re, FreeTable_orig);
SG_HOOK("golf_clean.exe", 0x0040d6a0, tickMessage, TickMessage_re, TickMessage_orig);
SG_HOOK("golf_clean.exe", 0x0043d460, arrayStaticInit_43d460, ArrayStaticInit43d460_re, ArrayStaticInit43d460_orig);

SG_HOOK("golf_clean.exe", 0x0048cd10, ListBox_onScroll_vtordisp, ListBoxOnScrollVd_re, ListBoxOnScrollVd_orig);
SG_HOOK("golf_clean.exe", 0x0048cd60, ListBox_build_vtordisp, ListBoxBuildVd_re, ListBoxBuildVd_orig);
SG_HOOK("golf_clean.exe", 0x00491bf0, panelBuildRow_vtordisp, PanelBuildRowVd_re, PanelBuildRowVd_orig);
SG_HOOK("golf_clean.exe", 0x004a3040, panelRowDeletingDtor_vtordisp, PanelRowDeletingDtorVd_re, PanelRowDeletingDtorVd_orig);
SG_HOOK("golf_clean.exe", 0x004a30f0, panelCreate4a2400_vtordisp, PanelCreate4a2400Vd_re, PanelCreate4a2400Vd_orig);
SG_HOOK("golf_clean.exe", 0x004a4970, panelSubDeletingDtor_vtordisp, PanelSubDeletingDtorVd_re, PanelSubDeletingDtorVd_orig);
SG_HOOK("golf_clean.exe", 0x004a4ab0, panelBuild4a3880_vtordisp, PanelBuild4a3880Vd_re, PanelBuild4a3880Vd_orig);
SG_HOOK("golf_clean.exe", 0x0049bfd0, screenObj_deleteDtor_adj5c, ScreenObjDeleteDtorAdj5c_re, ScreenObjDeleteDtorAdj5c_orig);
SG_HOOK("golf_clean.exe", 0x0043cbf0, editBoxTimer_deleteDtor_adj274, EditBoxTimerDeleteDtorAdj274_re, EditBoxTimerDeleteDtorAdj274_orig);
SG_HOOK("golf_clean.exe", 0x00489140, Button_scalarDtor_adj274, ButtonScalarDtorAdj274_re, ButtonScalarDtorAdj274_orig);
SG_HOOK("golf_clean.exe", 0x004918d0, Dialog_scalarDtor_adj274, DialogScalarDtorAdj274_re, DialogScalarDtorAdj274_orig);
SG_HOOK("golf_clean.exe", 0x0049c820, globalObjDeleteDtor49c800_adj274, GlobalObjDeleteDtorAdj274_re, GlobalObjDeleteDtorAdj274_orig);
SG_HOOK("golf_clean.exe", 0x00488630, Button_scalarDtor, ButtonScalarDtor_re, ButtonScalarDtor_orig);
SG_HOOK("golf_clean.exe", 0x004916f0, Dialog_scalarDtor, DialogScalarDtor_re, DialogScalarDtor_orig);
SG_HOOK("golf_clean.exe", 0x0049c800, globalObjDeleteDtor49c800, GlobalObjDeleteDtor_re, GlobalObjDeleteDtor_orig);
SG_HOOK("golf_clean.exe", 0x0043cbd0, editBoxTimer_deleteDtor, EditBoxTimerDeleteDtor_re, EditBoxTimerDeleteDtor_orig);
SG_HOOK("golf_clean.exe", 0x00404260, deletingDtor_404260, DeletingDtor404260_re, DeletingDtor404260_orig);
SG_HOOK("golf_clean.exe", 0x0049bf70, screenObj_deleteDtor, ScreenObjDeleteDtor_re, ScreenObjDeleteDtor_orig);
SG_HOOK("golf_clean.exe", 0x004a3050, panelRowDeletingDtor, PanelRowDeletingDtor_re, PanelRowDeletingDtor_orig);
SG_HOOK("golf_clean.exe", 0x004a4980, panelSubDeletingDtor, PanelSubDeletingDtor_re, PanelSubDeletingDtor_orig);
SG_HOOK("golf_clean.exe", 0x00404360, staticInit_404360, StaticInit404360_re, StaticInit404360_orig);
SG_HOOK("golf_clean.exe", 0x00404320, staticInit_00404320, StaticInit404320_re, StaticInit404320_orig);
SG_HOOK("golf_clean.exe", 0x0043d450, dynInit_arrayStaticInit_43d460, DynInitArrayStaticInit43d460_re, DynInitArrayStaticInit43d460_orig);
SG_HOOK("golf_clean.exe", 0x004a0120, dynInit_initLogFile, DynInitInitLogFile_re, DynInitInitLogFile_orig);
SG_HOOK("golf_clean.exe", 0x004a0130, initLogFile, InitLogFile_re, InitLogFile_orig);
SG_HOOK("golf_clean.exe", 0x00465740, rootWin_onUnpoint2, RootWinOnUnpoint2_re, RootWinOnUnpoint2_orig);
SG_HOOK("golf_clean.exe", 0x004657d0, rootWin_onChar0, RootWinOnChar0_re, RootWinOnChar0_orig);
SG_HOOK("golf_clean.exe", 0x004657b0, rootWin_onChar, RootWinOnChar_re, RootWinOnChar_orig);
SG_HOOK("golf_clean.exe", 0x00482e20, Link482_resolve, Link482Resolve_re, Link482Resolve_orig);
SG_HOOK("golf_clean.exe", 0x004657e0, rootWin_onPress1b, RootWinOnPress1b_re, RootWinOnPress1b_orig);
SG_HOOK("golf_clean.exe", 0x00465840, rootWin_onPress2b, RootWinOnPress2b_re, RootWinOnPress2b_orig);
SG_HOOK("golf_clean.exe", 0x00465750, rootWin_onKey, RootWinOnKey_re, RootWinOnKey_orig);
SG_HOOK("golf_clean.exe", 0x00401c00, tickAnimals, TickAnimals_re, TickAnimals_orig);
SG_HOOK("golf_clean.exe", 0x004041c0, resetPair, ResetPair_re, ResetPair_orig);
SG_HOOK("golf_clean.exe", 0x00491c10, initSinTable, InitSinTable_re, InitSinTable_orig);
