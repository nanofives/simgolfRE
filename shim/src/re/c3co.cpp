// C3 batch c3co (round 22, 2026-10-09) of golf_clean.exe: the low-address half (0x00403f10-0x00405840) of the
// C++ static-initialization families that the CRT runs before WinMain. Three shapes, all read from the disassembly
// (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list):
//
//   dynInit_*          a slot of the CRT initializer table 0x004c1000 (run by __cinit 0x004a50db):
//                      `call <init>` then `jmp <atexitRegister>`, so it returns the registrar's eax.
//   initGlobalArray_*  constructs one or more fixed global arrays: per array `push dtor; push ctor; push count;
//                      push size; push first; call 0x004a55d2` (the eh vector constructor iterator, __stdcall,
//                      `ret 0x14` at 0x004a5639, which calls ctor with ecx = each element at 0x004a560d), and per
//                      single global object `mov ecx, obj; call ctor` (the last one may be a tail `jmp`).
//   atexitRegister_*   `push <dtor>; call _atexit 0x004a56d2; pop ecx; ret`: returns _atexit's result (0 when
//                      _onexit 0x004a5654 returned the pointer, -1 when it returned NULL: 0x004a56db..0x004a56e2).
//
// Every callee runs through its original address. __thiscall constructors are called as __fastcall (ecx = this,
// edx unused). Within the batch: each dynInit_initGlobalArray_* calls this batch's initGlobalArray_* and
// atexitRegister_* hooks; each dynInit_staticInit_* calls staticInit_* (not hooked) and this batch's
// atexitRegister_*.
#include "hooks.h"

namespace {

// 0x004a55d2 eh vector constructor iterator: (first, size, count, ctor, dtor), callee pops 0x14 (0x004a5639).
typedef void(__stdcall* VecCtorIter_t)(void* first, unsigned size, int count, void* ctor, void* dtor);
const VecCtorIter_t kVecCtorIter = reinterpret_cast<VecCtorIter_t>(0x004a55d2);
// 0x004a56d2 _atexit: cdecl, one argument (the caller pops it: `pop ecx` after every call in this batch).
typedef int(__cdecl* Atexit_t)(void* fn);
const Atexit_t kAtexit = reinterpret_cast<Atexit_t>(0x004a56d2);
// A constructor called with ecx = the object; it returns the object in eax (e.g. 0x00473ab0 `mov eax, ecx`).
typedef void*(__fastcall* Ctor_t)(void* ecx, void* edx);
// The init and register routines a dynInit_* stub reaches (argument-free; the init's eax is discarded because
// the stub tail-jumps to the registrar, whose eax is the stub's result).
typedef void(__cdecl* Init_t)();
typedef int(__cdecl* Register_t)();

// One step of an initGlobalArray_* body: a vector construction (count > 0) or one object (count == 0).
struct InitStep {
    unsigned at;      // address of the `call` (or tail `jmp`) instruction
    unsigned first;   // the array's first element, or the object
    unsigned size;    // element size (vector steps)
    int count;        // element count (vector steps); 0 = single object
    unsigned ctor;    // constructor
    unsigned dtor;    // destructor handed to the iterator for unwinding (vector steps)
};

// Runs the steps in order; returns the eax of the last single-object constructor (a tail `jmp` returns it).
void* runSteps(const InitStep* s, int n) {
    void* last = 0;
    for (int i = 0; i < n; ++i) {
        if (s[i].count)
            kVecCtorIter(reinterpret_cast<void*>(s[i].first), s[i].size, s[i].count,
                         reinterpret_cast<void*>(s[i].ctor), reinterpret_cast<void*>(s[i].dtor));
        else
            last = reinterpret_cast<Ctor_t>(s[i].ctor)(reinterpret_cast<void*>(s[i].first), 0);
    }
    return last;
}

}  // namespace

// ---------------------------------------------------------------------------------------- atexitRegister_*
// Each pushes its own static destructor and returns _atexit's result. Template parameter: the destructor.
namespace {
template <unsigned Dtor> struct AtexitRegister {
    static int(__cdecl* orig)();
    static int __cdecl re() { return kAtexit(reinterpret_cast<void*>(Dtor)); }
};
template <unsigned Dtor> int(__cdecl* AtexitRegister<Dtor>::orig)() = 0;
}  // namespace

// 0x00403f40  atexitRegister_00403f40: push 0x00403f50 (0x00403f40), call _atexit (0x00403f45)
SG_HOOK("golf_clean.exe", 0x00403f40, atexitRegister_00403f40, AtexitRegister<0x00403f50>::re, AtexitRegister<0x00403f50>::orig);

// 0x00403fa0  atexitRegister_00403fa0: push 0x00403fb0 (0x00403fa0), call _atexit (0x00403fa5)
SG_HOOK("golf_clean.exe", 0x00403fa0, atexitRegister_00403fa0, AtexitRegister<0x00403fb0>::re, AtexitRegister<0x00403fb0>::orig);

// 0x00404000  atexitRegister_00404000: push 0x00404010 (0x00404000), call _atexit (0x00404005)
SG_HOOK("golf_clean.exe", 0x00404000, atexitRegister_00404000, AtexitRegister<0x00404010>::re, AtexitRegister<0x00404010>::orig);

// 0x00404060  atexitRegister_00404060: push 0x00404070 (0x00404060), call _atexit (0x00404065)
SG_HOOK("golf_clean.exe", 0x00404060, atexitRegister_00404060, AtexitRegister<0x00404070>::re, AtexitRegister<0x00404070>::orig);

// 0x00404330  atexitRegister_00404330: push 0x00404340 (0x00404330), call _atexit (0x00404335)
SG_HOOK("golf_clean.exe", 0x00404330, atexitRegister_00404330, AtexitRegister<0x00404340>::re, AtexitRegister<0x00404340>::orig);

// 0x004043c0  atexitRegister_004043c0: push 0x004043d0 (0x004043c0), call _atexit (0x004043c5)
SG_HOOK("golf_clean.exe", 0x004043c0, atexitRegister_004043c0, AtexitRegister<0x004043d0>::re, AtexitRegister<0x004043d0>::orig);

// 0x00404400  atexitRegister_00404400: push 0x00404410 (0x00404400), call _atexit (0x00404405)
SG_HOOK("golf_clean.exe", 0x00404400, atexitRegister_00404400, AtexitRegister<0x00404410>::re, AtexitRegister<0x00404410>::orig);

// 0x00404450  atexitRegister_00404450: push 0x00404460 (0x00404450), call _atexit (0x00404455)
SG_HOOK("golf_clean.exe", 0x00404450, atexitRegister_00404450, AtexitRegister<0x00404460>::re, AtexitRegister<0x00404460>::orig);

// 0x004044a0  atexitRegister_004044a0: push 0x004044b0 (0x004044a0), call _atexit (0x004044a5)
SG_HOOK("golf_clean.exe", 0x004044a0, atexitRegister_004044a0, AtexitRegister<0x004044b0>::re, AtexitRegister<0x004044b0>::orig);

// 0x004044f0  atexitRegister_004044f0: push 0x00404500 (0x004044f0), call _atexit (0x004044f5)
SG_HOOK("golf_clean.exe", 0x004044f0, atexitRegister_004044f0, AtexitRegister<0x00404500>::re, AtexitRegister<0x00404500>::orig);

// 0x00404540  atexitRegister_00404540: push 0x00404550 (0x00404540), call _atexit (0x00404545)
SG_HOOK("golf_clean.exe", 0x00404540, atexitRegister_00404540, AtexitRegister<0x00404550>::re, AtexitRegister<0x00404550>::orig);

// 0x00404590  atexitRegister_00404590: push 0x004045a0 (0x00404590), call _atexit (0x00404595)
SG_HOOK("golf_clean.exe", 0x00404590, atexitRegister_00404590, AtexitRegister<0x004045a0>::re, AtexitRegister<0x004045a0>::orig);

// 0x004045e0  atexitRegister_004045e0: push 0x004045f0 (0x004045e0), call _atexit (0x004045e5)
SG_HOOK("golf_clean.exe", 0x004045e0, atexitRegister_004045e0, AtexitRegister<0x004045f0>::re, AtexitRegister<0x004045f0>::orig);

// 0x00404630  atexitRegister_00404630: push 0x00404640 (0x00404630), call _atexit (0x00404635)
SG_HOOK("golf_clean.exe", 0x00404630, atexitRegister_00404630, AtexitRegister<0x00404640>::re, AtexitRegister<0x00404640>::orig);

// 0x00404680  atexitRegister_00404680: push 0x00404690 (0x00404680), call _atexit (0x00404685)
SG_HOOK("golf_clean.exe", 0x00404680, atexitRegister_00404680, AtexitRegister<0x00404690>::re, AtexitRegister<0x00404690>::orig);

// 0x004046d0  atexitRegister_004046d0: push 0x004046e0 (0x004046d0), call _atexit (0x004046d5)
SG_HOOK("golf_clean.exe", 0x004046d0, atexitRegister_004046d0, AtexitRegister<0x004046e0>::re, AtexitRegister<0x004046e0>::orig);

// 0x00404710  atexitRegister_00404710: push 0x00404720 (0x00404710), call _atexit (0x00404715)
SG_HOOK("golf_clean.exe", 0x00404710, atexitRegister_00404710, AtexitRegister<0x00404720>::re, AtexitRegister<0x00404720>::orig);

// 0x00404760  atexitRegister_00404760: push 0x00404770 (0x00404760), call _atexit (0x00404765)
SG_HOOK("golf_clean.exe", 0x00404760, atexitRegister_00404760, AtexitRegister<0x00404770>::re, AtexitRegister<0x00404770>::orig);

// 0x004047b0  atexitRegister_004047b0: push 0x004047c0 (0x004047b0), call _atexit (0x004047b5)
SG_HOOK("golf_clean.exe", 0x004047b0, atexitRegister_004047b0, AtexitRegister<0x004047c0>::re, AtexitRegister<0x004047c0>::orig);

// 0x00404800  atexitRegister_00404800: push 0x00404810 (0x00404800), call _atexit (0x00404805)
SG_HOOK("golf_clean.exe", 0x00404800, atexitRegister_00404800, AtexitRegister<0x00404810>::re, AtexitRegister<0x00404810>::orig);

// 0x00404850  atexitRegister_00404850: push 0x00404860 (0x00404850), call _atexit (0x00404855)
SG_HOOK("golf_clean.exe", 0x00404850, atexitRegister_00404850, AtexitRegister<0x00404860>::re, AtexitRegister<0x00404860>::orig);

// 0x00404cf0  atexitRegister_00404cf0: push 0x00404d00 (0x00404cf0), call _atexit (0x00404cf5)
SG_HOOK("golf_clean.exe", 0x00404cf0, atexitRegister_00404cf0, AtexitRegister<0x00404d00>::re, AtexitRegister<0x00404d00>::orig);

// 0x00404e90  atexitRegister_00404e90: push 0x00404ea0 (0x00404e90), call _atexit (0x00404e95)
SG_HOOK("golf_clean.exe", 0x00404e90, atexitRegister_00404e90, AtexitRegister<0x00404ea0>::re, AtexitRegister<0x00404ea0>::orig);

// 0x00405000  atexitRegister_00405000: push 0x00405010 (0x00405000), call _atexit (0x00405005)
SG_HOOK("golf_clean.exe", 0x00405000, atexitRegister_00405000, AtexitRegister<0x00405010>::re, AtexitRegister<0x00405010>::orig);

// 0x00405150  atexitRegister_00405150: push 0x00405160 (0x00405150), call _atexit (0x00405155)
SG_HOOK("golf_clean.exe", 0x00405150, atexitRegister_00405150, AtexitRegister<0x00405160>::re, AtexitRegister<0x00405160>::orig);

// 0x004052d0  atexitRegister_004052d0: push 0x004052e0 (0x004052d0), call _atexit (0x004052d5)
SG_HOOK("golf_clean.exe", 0x004052d0, atexitRegister_004052d0, AtexitRegister<0x004052e0>::re, AtexitRegister<0x004052e0>::orig);

// 0x00405440  atexitRegister_00405440: push 0x00405450 (0x00405440), call _atexit (0x00405445)
SG_HOOK("golf_clean.exe", 0x00405440, atexitRegister_00405440, AtexitRegister<0x00405450>::re, AtexitRegister<0x00405450>::orig);

// 0x00405580  atexitRegister_00405580: push 0x00405590 (0x00405580), call _atexit (0x00405585)
SG_HOOK("golf_clean.exe", 0x00405580, atexitRegister_00405580, AtexitRegister<0x00405590>::re, AtexitRegister<0x00405590>::orig);

// 0x004056c0  atexitRegister_004056c0: push 0x004056d0 (0x004056c0), call _atexit (0x004056c5)
SG_HOOK("golf_clean.exe", 0x004056c0, atexitRegister_004056c0, AtexitRegister<0x004056d0>::re, AtexitRegister<0x004056d0>::orig);

// 0x00405830  atexitRegister_00405830: push 0x00405840 (0x00405830), call _atexit (0x00405835)
SG_HOOK("golf_clean.exe", 0x00405830, atexitRegister_00405830, AtexitRegister<0x00405840>::re, AtexitRegister<0x00405840>::orig);

// --------------------------------------------------------------------------------------- initGlobalArray_*
// One step table per function, in instruction order. Constructor / destructor names from hooks.csv.

// 0x00403f20  initGlobalArray_00403f20: 1 step(s)
namespace {
const InitStep kSteps_00403f20[] = {
    {0x00403f39, 0x004f65c8, 0x84, 0x24f, 0x00481b50, 0x00404280},  // Flic::ctor481b50 / R2C404280_dtor
};
void(__cdecl* InitGlobalArray_00403f20_orig)();
void __cdecl InitGlobalArray_00403f20_re() { runSteps(kSteps_00403f20, 1); }  // `ret` at 0x00403f3e
}  // namespace
SG_HOOK("golf_clean.exe", 0x00403f20, initGlobalArray_00403f20, InitGlobalArray_00403f20_re, InitGlobalArray_00403f20_orig);

// 0x00403f80  initGlobalArray_00403f80: 1 step(s)
namespace {
const InitStep kSteps_00403f80[] = {
    {0x00403f96, 0x0050fc20, 0x44, 0x24f, 0x00482b60, 0x00404200},  // Flic::ctorBase482b60 / C404200_dtor
};
void(__cdecl* InitGlobalArray_00403f80_orig)();
void __cdecl InitGlobalArray_00403f80_re() { runSteps(kSteps_00403f80, 1); }  // `ret` at 0x00403f9b
}  // namespace
SG_HOOK("golf_clean.exe", 0x00403f80, initGlobalArray_00403f80, InitGlobalArray_00403f80_re, InitGlobalArray_00403f80_orig);

// 0x00403fe0  initGlobalArray_00403fe0: 1 step(s)
namespace {
const InitStep kSteps_00403fe0[] = {
    {0x00403ff6, 0x00509688, 0x2c, 0x24f, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
};
void(__cdecl* InitGlobalArray_00403fe0_orig)();
void __cdecl InitGlobalArray_00403fe0_re() { runSteps(kSteps_00403fe0, 1); }  // `ret` at 0x00403ffb
}  // namespace
SG_HOOK("golf_clean.exe", 0x00403fe0, initGlobalArray_00403fe0, InitGlobalArray_00403fe0_re, InitGlobalArray_00403fe0_orig);

// 0x00404040  initGlobalArray_00404040: 1 step(s)
namespace {
const InitStep kSteps_00404040[] = {
    {0x00404056, 0x004e9aa0, 0x58, 0x24f, 0x00482fd0, 0x00483010},  // Cache482fd0::ctor / Cache483010::dtor
};
void(__cdecl* InitGlobalArray_00404040_orig)();
void __cdecl InitGlobalArray_00404040_re() { runSteps(kSteps_00404040, 1); }  // `ret` at 0x0040405b
}  // namespace
SG_HOOK("golf_clean.exe", 0x00404040, initGlobalArray_00404040, InitGlobalArray_00404040_re, InitGlobalArray_00404040_orig);

// 0x00404c20  initGlobalArray_00404c20: 13 step(s), the last a tail `jmp` whose eax is returned
namespace {
const InitStep kSteps_00404c20[] = {
    {0x00404c33, 0x0059ca58, 0x2c, 0x50, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00404c3d, 0x0059b740, 0, 0, 0x00473ab0, 0},  // mov ecx at 0x00404c38; Surface_ctor
    {0x00404c55, 0x0056a250, 0x2c, 0x10, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00404c6d, 0x00540950, 0x2c, 0x2, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00404c85, 0x005439e0, 0x2c, 0x10, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00404c8f, 0x005787a0, 0, 0, 0x00473ab0, 0},  // mov ecx at 0x00404c8a; Surface_ctor
    {0x00404ca7, 0x0055e5b0, 0x2c, 0x14, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00404cb1, 0x0058b650, 0, 0, 0x00473ab0, 0},  // mov ecx at 0x00404cac; Surface_ctor
    {0x00404cbb, 0x005791c8, 0, 0, 0x00473ab0, 0},  // mov ecx at 0x00404cb6; Surface_ctor
    {0x00404cc5, 0x00567b08, 0, 0, 0x00473ab0, 0},  // mov ecx at 0x00404cc0; Surface_ctor
    {0x00404ccf, 0x005787d0, 0, 0, 0x00473ab0, 0},  // mov ecx at 0x00404cca; Surface_ctor
    {0x00404cd9, 0x005a4100, 0, 0, 0x00473ab0, 0},  // mov ecx at 0x00404cd4; Surface_ctor
    {0x00404ce3, 0x005423e8, 0, 0, 0x00473ab0, 0},  // mov ecx at 0x00404cde; Surface_ctor
};
void*(__cdecl* InitGlobalArray_00404c20_orig)();
void* __cdecl InitGlobalArray_00404c20_re() { return runSteps(kSteps_00404c20, 13); }
}  // namespace
SG_HOOK("golf_clean.exe", 0x00404c20, initGlobalArray_00404c20, InitGlobalArray_00404c20_re, InitGlobalArray_00404c20_orig);

// 0x00404e00  initGlobalArray_00404e00: 8 step(s), the last a tail `jmp` whose eax is returned
namespace {
const InitStep kSteps_00404e00[] = {
    {0x00404e13, 0x00562368, 0x2c, 0x20, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00404e2b, 0x0059d920, 0x2c, 0x20, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00404e35, 0x0058b358, 0, 0, 0x00473ab0, 0},  // mov ecx at 0x00404e30; Surface_ctor
    {0x00404e3f, 0x0056a760, 0, 0, 0x00473ab0, 0},  // mov ecx at 0x00404e3a; Surface_ctor
    {0x00404e49, 0x0056c7b8, 0, 0, 0x00473ab0, 0},  // mov ecx at 0x00404e44; Surface_ctor
    {0x00404e61, 0x005a48e8, 0x2c, 0x4, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00404e79, 0x0059abb8, 0x2c, 0x10, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00404e83, 0x0059bf98, 0, 0, 0x00473ab0, 0},  // mov ecx at 0x00404e7e; Surface_ctor
};
void*(__cdecl* InitGlobalArray_00404e00_orig)();
void* __cdecl InitGlobalArray_00404e00_re() { return runSteps(kSteps_00404e00, 8); }
}  // namespace
SG_HOOK("golf_clean.exe", 0x00404e00, initGlobalArray_00404e00, InitGlobalArray_00404e00_re, InitGlobalArray_00404e00_orig);

// 0x00404f50  initGlobalArray_00404f50: 7 step(s)
namespace {
const InitStep kSteps_00404f50[] = {
    {0x00404f63, 0x00540d98, 0x2c, 0x20, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00404f7b, 0x0056a550, 0x2c, 0xc, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00404f93, 0x0059b050, 0x2c, 0x28, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00404fab, 0x0053ba48, 0x2c, 0x8, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00404fc6, 0x005628e8, 0x2c, 0x1b0, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00404fde, 0x0059e7c0, 0x2c, 0x78, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00404ff6, 0x0058a538, 0x2c, 0x48, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
};
void(__cdecl* InitGlobalArray_00404f50_orig)();
void __cdecl InitGlobalArray_00404f50_re() { runSteps(kSteps_00404f50, 7); }  // `ret` at 0x00404ffb
}  // namespace
SG_HOOK("golf_clean.exe", 0x00404f50, initGlobalArray_00404f50, InitGlobalArray_00404f50_re, InitGlobalArray_00404f50_orig);

// 0x004050b0  initGlobalArray_004050b0: 6 step(s)
namespace {
const InitStep kSteps_004050b0[] = {
    {0x004050c3, 0x00568258, 0x2c, 0x14, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x004050de, 0x00587ea8, 0x2c, 0xe0, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x004050f6, 0x005678b8, 0x2c, 0x8, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x0040510e, 0x0056a7b8, 0x2c, 0x8, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00405126, 0x0055e930, 0x2c, 0xc, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x0040513e, 0x00542e50, 0x2c, 0x4, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
};
void(__cdecl* InitGlobalArray_004050b0_orig)();
void __cdecl InitGlobalArray_004050b0_re() { runSteps(kSteps_004050b0, 6); }  // `ret` at 0x00405143
}  // namespace
SG_HOOK("golf_clean.exe", 0x004050b0, initGlobalArray_004050b0, InitGlobalArray_004050b0_re, InitGlobalArray_004050b0_orig);

// 0x004051f0  initGlobalArray_004051f0: 9 step(s)
namespace {
const InitStep kSteps_004051f0[] = {
    {0x00405203, 0x00541a18, 0x2c, 0x10, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x0040521b, 0x0058b390, 0x2c, 0x10, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00405233, 0x00570cb8, 0x2c, 0x60, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x0040524b, 0x005873a8, 0x2c, 0x2a, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00405263, 0x00568fc8, 0x2c, 0x1c, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x0040527b, 0x005a7c08, 0x2c, 0x18, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00405293, 0x0059ba68, 0x2c, 0x1e, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x004052ab, 0x00583a80, 0x2c, 0x2c, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x004052c3, 0x00586f50, 0x2c, 0x2, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
};
void(__cdecl* InitGlobalArray_004051f0_orig)();
void __cdecl InitGlobalArray_004051f0_re() { runSteps(kSteps_004051f0, 9); }  // `ret` at 0x004052c8
}  // namespace
SG_HOOK("golf_clean.exe", 0x004051f0, initGlobalArray_004051f0, InitGlobalArray_004051f0_re, InitGlobalArray_004051f0_orig);

// 0x004053a0  initGlobalArray_004053a0: 6 step(s)
namespace {
const InitStep kSteps_004053a0[] = {
    {0x004053b3, 0x005a8c90, 0x2c, 0x28, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x004053cb, 0x0058b1a0, 0x2c, 0xa, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x004053e3, 0x00571d40, 0x2c, 0xf, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x004053fb, 0x0059b778, 0x2c, 0x11, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00405413, 0x0053fd48, 0x2c, 0x46, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x0040542b, 0x005a34f8, 0x2c, 0x46, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
};
void(__cdecl* InitGlobalArray_004053a0_orig)();
void __cdecl InitGlobalArray_004053a0_re() { runSteps(kSteps_004053a0, 6); }  // `ret` at 0x00405430
}  // namespace
SG_HOOK("golf_clean.exe", 0x004053a0, initGlobalArray_004053a0, InitGlobalArray_004053a0_re, InitGlobalArray_004053a0_orig);

// 0x004054e0  initGlobalArray_004054e0: 6 step(s)
namespace {
const InitStep kSteps_004054e0[] = {
    {0x004054f3, 0x0053c570, 0x2c, 0x20, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x0040550b, 0x005a4138, 0x2c, 0x20, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00405523, 0x00542f28, 0x2c, 0x4, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x0040553b, 0x0059ab00, 0x2c, 0x4, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00405553, 0x00520590, 0x2c, 0x4, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x0040556b, 0x0059df20, 0x2c, 0x32, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
};
void(__cdecl* InitGlobalArray_004054e0_orig)();
void __cdecl InitGlobalArray_004054e0_re() { runSteps(kSteps_004054e0, 6); }  // `ret` at 0x00405570
}  // namespace
SG_HOOK("golf_clean.exe", 0x004054e0, initGlobalArray_004054e0, InitGlobalArray_004054e0_re, InitGlobalArray_004054e0_orig);

// 0x00405620  initGlobalArray_00405620: 6 step(s)
namespace {
const InitStep kSteps_00405620[] = {
    {0x00405633, 0x0056a948, 0x2c, 0x1e, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x0040564b, 0x00567390, 0x2c, 0x1e, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00405663, 0x005a53c8, 0x2c, 0x24, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x0040567b, 0x0058b680, 0x2c, 0x24, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00405693, 0x005612e8, 0x2c, 0x27, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x004056ab, 0x0058ccb8, 0x2c, 0x27, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
};
void(__cdecl* InitGlobalArray_00405620_orig)();
void __cdecl InitGlobalArray_00405620_re() { runSteps(kSteps_00405620, 6); }  // `ret` at 0x004056b0
}  // namespace
SG_HOOK("golf_clean.exe", 0x00405620, initGlobalArray_00405620, InitGlobalArray_00405620_re, InitGlobalArray_00405620_orig);

// 0x00405760  initGlobalArray_00405760: 8 step(s)
namespace {
const InitStep kSteps_00405760[] = {
    {0x00405773, 0x00561260, 0x2c, 0x3, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x0040578b, 0x00541710, 0x2c, 0x10, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x004057a3, 0x005791f8, 0x2c, 0x10, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x004057bb, 0x0053df58, 0x2c, 0x28, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x004057d3, 0x00541580, 0x2c, 0x9, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x004057eb, 0x00567a20, 0x2c, 0x5, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x00405803, 0x0053de78, 0x2c, 0x5, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
    {0x0040581b, 0x0053b7e0, 0x2c, 0xe, 0x00473ab0, 0x004041f0},  // Surface_ctor / unwindFree_4041f0
};
void(__cdecl* InitGlobalArray_00405760_orig)();
void __cdecl InitGlobalArray_00405760_re() { runSteps(kSteps_00405760, 8); }  // `ret` at 0x00405820
}  // namespace
SG_HOOK("golf_clean.exe", 0x00405760, initGlobalArray_00405760, InitGlobalArray_00405760_re, InitGlobalArray_00405760_orig);

// ----------------------------------------------------------------------------------------------- dynInit_*
// `call Init` then `jmp Register`. Template parameters: the two targets.
namespace {
template <unsigned Init, unsigned Register> struct DynInit {
    static int(__cdecl* orig)();
    static int __cdecl re() {
        reinterpret_cast<Init_t>(Init)();
        return reinterpret_cast<Register_t>(Register)();
    }
};
template <unsigned Init, unsigned Register> int(__cdecl* DynInit<Init, Register>::orig)() = 0;
}  // namespace

// 0x00403f10  dynInit_initGlobalArray_00403f20: call 0x00403f20 (0x00403f10), jmp 0x00403f40 (0x00403f15); table: slot 1 (+0x4) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00403f10, dynInit_initGlobalArray_00403f20, (DynInit<0x00403f20, 0x00403f40>::re), (DynInit<0x00403f20, 0x00403f40>::orig));

// 0x00403f70  dynInit_initGlobalArray_00403f80: call 0x00403f80 (0x00403f70), jmp 0x00403fa0 (0x00403f75); table: slot 2 (+0x8) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00403f70, dynInit_initGlobalArray_00403f80, (DynInit<0x00403f80, 0x00403fa0>::re), (DynInit<0x00403f80, 0x00403fa0>::orig));

// 0x00403fd0  dynInit_initGlobalArray_00403fe0: call 0x00403fe0 (0x00403fd0), jmp 0x00404000 (0x00403fd5); table: slot 3 (+0xc) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00403fd0, dynInit_initGlobalArray_00403fe0, (DynInit<0x00403fe0, 0x00404000>::re), (DynInit<0x00403fe0, 0x00404000>::orig));

// 0x00404030  dynInit_initGlobalArray_00404040: call 0x00404040 (0x00404030), jmp 0x00404060 (0x00404035); table: slot 4 (+0x10) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00404030, dynInit_initGlobalArray_00404040, (DynInit<0x00404040, 0x00404060>::re), (DynInit<0x00404040, 0x00404060>::orig));

// 0x00404310  dynInit_staticInit_00404320: call 0x00404320 (0x00404310), jmp 0x00404330 (0x00404315); table: slot 5 (+0x14) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00404310, dynInit_staticInit_00404320, (DynInit<0x00404320, 0x00404330>::re), (DynInit<0x00404320, 0x00404330>::orig));

// 0x004043a0  dynInit_staticInit_004043b0: call 0x004043b0 (0x004043a0), jmp 0x004043c0 (0x004043a5); table: slot 6 (+0x18) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x004043a0, dynInit_staticInit_004043b0, (DynInit<0x004043b0, 0x004043c0>::re), (DynInit<0x004043b0, 0x004043c0>::orig));

// 0x004043e0  dynInit_staticInit_004043f0: call 0x004043f0 (0x004043e0), jmp 0x00404400 (0x004043e5); table: slot 7 (+0x1c) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x004043e0, dynInit_staticInit_004043f0, (DynInit<0x004043f0, 0x00404400>::re), (DynInit<0x004043f0, 0x00404400>::orig));

// 0x00404430  dynInit_staticInit_00404440: call 0x00404440 (0x00404430), jmp 0x00404450 (0x00404435); table: slot 8 (+0x20) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00404430, dynInit_staticInit_00404440, (DynInit<0x00404440, 0x00404450>::re), (DynInit<0x00404440, 0x00404450>::orig));

// 0x00404480  dynInit_staticInit_00404490: call 0x00404490 (0x00404480), jmp 0x004044a0 (0x00404485); table: slot 9 (+0x24) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00404480, dynInit_staticInit_00404490, (DynInit<0x00404490, 0x004044a0>::re), (DynInit<0x00404490, 0x004044a0>::orig));

// 0x004044d0  dynInit_staticInit_004044e0: call 0x004044e0 (0x004044d0), jmp 0x004044f0 (0x004044d5); table: slot 10 (+0x28) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x004044d0, dynInit_staticInit_004044e0, (DynInit<0x004044e0, 0x004044f0>::re), (DynInit<0x004044e0, 0x004044f0>::orig));

// 0x00404520  dynInit_staticInit_00404530: call 0x00404530 (0x00404520), jmp 0x00404540 (0x00404525); table: slot 11 (+0x2c) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00404520, dynInit_staticInit_00404530, (DynInit<0x00404530, 0x00404540>::re), (DynInit<0x00404530, 0x00404540>::orig));

// 0x00404570  dynInit_staticInit_00404580: call 0x00404580 (0x00404570), jmp 0x00404590 (0x00404575); table: slot 12 (+0x30) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00404570, dynInit_staticInit_00404580, (DynInit<0x00404580, 0x00404590>::re), (DynInit<0x00404580, 0x00404590>::orig));

// 0x004045c0  dynInit_staticInit_004045d0: call 0x004045d0 (0x004045c0), jmp 0x004045e0 (0x004045c5); table: slot 13 (+0x34) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x004045c0, dynInit_staticInit_004045d0, (DynInit<0x004045d0, 0x004045e0>::re), (DynInit<0x004045d0, 0x004045e0>::orig));

// 0x00404610  dynInit_staticInit_00404620: call 0x00404620 (0x00404610), jmp 0x00404630 (0x00404615); table: slot 14 (+0x38) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00404610, dynInit_staticInit_00404620, (DynInit<0x00404620, 0x00404630>::re), (DynInit<0x00404620, 0x00404630>::orig));

// 0x00404660  dynInit_staticInit_00404670: call 0x00404670 (0x00404660), jmp 0x00404680 (0x00404665); table: slot 15 (+0x3c) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00404660, dynInit_staticInit_00404670, (DynInit<0x00404670, 0x00404680>::re), (DynInit<0x00404670, 0x00404680>::orig));

// 0x004046b0  dynInit_staticInit_004046c0: call 0x004046c0 (0x004046b0), jmp 0x004046d0 (0x004046b5); table: slot 16 (+0x40) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x004046b0, dynInit_staticInit_004046c0, (DynInit<0x004046c0, 0x004046d0>::re), (DynInit<0x004046c0, 0x004046d0>::orig));

// 0x004046f0  dynInit_staticInit_00404700: call 0x00404700 (0x004046f0), jmp 0x00404710 (0x004046f5); table: slot 17 (+0x44) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x004046f0, dynInit_staticInit_00404700, (DynInit<0x00404700, 0x00404710>::re), (DynInit<0x00404700, 0x00404710>::orig));

// 0x00404740  dynInit_staticInit_00404750: call 0x00404750 (0x00404740), jmp 0x00404760 (0x00404745); table: slot 18 (+0x48) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00404740, dynInit_staticInit_00404750, (DynInit<0x00404750, 0x00404760>::re), (DynInit<0x00404750, 0x00404760>::orig));

// 0x00404790  dynInit_staticInit_004047a0: call 0x004047a0 (0x00404790), jmp 0x004047b0 (0x00404795); table: slot 19 (+0x4c) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00404790, dynInit_staticInit_004047a0, (DynInit<0x004047a0, 0x004047b0>::re), (DynInit<0x004047a0, 0x004047b0>::orig));

// 0x004047e0  dynInit_staticInit_004047f0: call 0x004047f0 (0x004047e0), jmp 0x00404800 (0x004047e5); table: slot 20 (+0x50) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x004047e0, dynInit_staticInit_004047f0, (DynInit<0x004047f0, 0x00404800>::re), (DynInit<0x004047f0, 0x00404800>::orig));

// 0x00404830  dynInit_staticInit_00404840: call 0x00404840 (0x00404830), jmp 0x00404850 (0x00404835); table: slot 21 (+0x54) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00404830, dynInit_staticInit_00404840, (DynInit<0x00404840, 0x00404850>::re), (DynInit<0x00404840, 0x00404850>::orig));

// 0x00404c10  dynInit_initGlobalArray_00404c20: call 0x00404c20 (0x00404c10), jmp 0x00404cf0 (0x00404c15); table: slot 22 (+0x58) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00404c10, dynInit_initGlobalArray_00404c20, (DynInit<0x00404c20, 0x00404cf0>::re), (DynInit<0x00404c20, 0x00404cf0>::orig));

// 0x00404df0  dynInit_initGlobalArray_00404e00: call 0x00404e00 (0x00404df0), jmp 0x00404e90 (0x00404df5); table: slot 23 (+0x5c) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00404df0, dynInit_initGlobalArray_00404e00, (DynInit<0x00404e00, 0x00404e90>::re), (DynInit<0x00404e00, 0x00404e90>::orig));

// 0x00404f40  dynInit_initGlobalArray_00404f50: call 0x00404f50 (0x00404f40), jmp 0x00405000 (0x00404f45); table: slot 24 (+0x60) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00404f40, dynInit_initGlobalArray_00404f50, (DynInit<0x00404f50, 0x00405000>::re), (DynInit<0x00404f50, 0x00405000>::orig));

// 0x004050a0  dynInit_initGlobalArray_004050b0: call 0x004050b0 (0x004050a0), jmp 0x00405150 (0x004050a5); table: slot 25 (+0x64) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x004050a0, dynInit_initGlobalArray_004050b0, (DynInit<0x004050b0, 0x00405150>::re), (DynInit<0x004050b0, 0x00405150>::orig));

// 0x004051e0  dynInit_initGlobalArray_004051f0: call 0x004051f0 (0x004051e0), jmp 0x004052d0 (0x004051e5); table: slot 26 (+0x68) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x004051e0, dynInit_initGlobalArray_004051f0, (DynInit<0x004051f0, 0x004052d0>::re), (DynInit<0x004051f0, 0x004052d0>::orig));

// 0x00405390  dynInit_initGlobalArray_004053a0: call 0x004053a0 (0x00405390), jmp 0x00405440 (0x00405395); table: slot 27 (+0x6c) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00405390, dynInit_initGlobalArray_004053a0, (DynInit<0x004053a0, 0x00405440>::re), (DynInit<0x004053a0, 0x00405440>::orig));

// 0x004054d0  dynInit_initGlobalArray_004054e0: call 0x004054e0 (0x004054d0), jmp 0x00405580 (0x004054d5); table: slot 28 (+0x70) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x004054d0, dynInit_initGlobalArray_004054e0, (DynInit<0x004054e0, 0x00405580>::re), (DynInit<0x004054e0, 0x00405580>::orig));

// 0x00405610  dynInit_initGlobalArray_00405620: call 0x00405620 (0x00405610), jmp 0x004056c0 (0x00405615); table: slot 29 (+0x74) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00405610, dynInit_initGlobalArray_00405620, (DynInit<0x00405620, 0x004056c0>::re), (DynInit<0x00405620, 0x004056c0>::orig));

// 0x00405750  dynInit_initGlobalArray_00405760: call 0x00405760 (0x00405750), jmp 0x00405830 (0x00405755); table: slot 30 (+0x78) of the table at 0x004c1000
SG_HOOK("golf_clean.exe", 0x00405750, dynInit_initGlobalArray_00405760, (DynInit<0x00405760, 0x00405830>::re), (DynInit<0x00405760, 0x00405830>::orig));
