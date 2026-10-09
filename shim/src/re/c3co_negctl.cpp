// NEGATIVE CONTROL for batch c3co. NEVER list this file in shim/re_batches.txt: every body here is
// DELIBERATELY WRONG in exactly one step, so a diff_hook run proves that c3co's compared state (the pattern-filled
// live regions, the _onexit stand-in's log, the fixture display's counter) catches that step.
//
// Built INSTEAD of c3co.cpp through log/c3/c3co_negctl_batches.txt into instances\c3co (then c3co.cpp is rebuilt);
// driven by log/c3/alt/c3co_negctl.py; CSVs go to log/c3/alt/c3co_negctl, never to log/diff.
//
//   0x00403f20 initGlobalArray_00403f20   count 0x24e instead of 0x24f (0x00403f2a): the last Flic is not built
//   0x00404c20 initGlobalArray_00404c20   the original body plus one stray store at 0x0059b744 (+4 of the
//                                         single object 0x0059b740 that Surface_ctor zeroes, 0x00473ab4)
//   0x00403f40 atexitRegister_00403f40    pushes 0x00403fb0 (another registrar's destructor) instead of 0x00403f50
//   0x00404310 dynInit_staticInit_00404320  skips the View constructor call (0x00404310) and only registers
#include "hooks.h"

namespace {

typedef void(__stdcall* VecCtorIter_t)(void* first, unsigned size, int count, void* ctor, void* dtor);
const VecCtorIter_t kVecCtorIter = reinterpret_cast<VecCtorIter_t>(0x004a55d2);
typedef int(__cdecl* Atexit_t)(void* fn);
const Atexit_t kAtexit = reinterpret_cast<Atexit_t>(0x004a56d2);
typedef void*(__fastcall* Ctor_t)(void* ecx, void* edx);
typedef int(__cdecl* Register_t)();

void(__cdecl* Neg403f20_orig)();
void __cdecl Neg403f20_re() {
    kVecCtorIter(reinterpret_cast<void*>(0x004f65c8), 0x84, 0x24e /* WRONG: 0x24f */,
                 reinterpret_cast<void*>(0x00481b50), reinterpret_cast<void*>(0x00404280));
}

void(__cdecl* Neg404c20_orig)();
void __cdecl Neg404c20_re() {
    // The original body through its trampoline, then ONE store the original never makes: Surface_ctor writes 0 to
    // +4 of the single object 0x0059b740 (0x00473ab4, called from 0x00404c3d); the control leaves 0xdeadbeef there.
    Neg404c20_orig();
    *reinterpret_cast<unsigned*>(0x0059b744) = 0xdeadbeefu;
}

int(__cdecl* Neg403f40_orig)();
int __cdecl Neg403f40_re() { return kAtexit(reinterpret_cast<void*>(0x00403fb0)); /* WRONG: 0x00403f50 */ }

int(__cdecl* Neg404310_orig)();
int __cdecl Neg404310_re() {
    // WRONG: no call to staticInit_00404320 0x00404320
    return reinterpret_cast<Register_t>(0x00404330)();
}

}  // namespace

// 0x00403f20  initGlobalArray_00403f20 (negative control)
SG_HOOK("golf_clean.exe", 0x00403f20, neg_initGlobalArray_00403f20, Neg403f20_re, Neg403f20_orig);
// 0x00404c20  initGlobalArray_00404c20 (negative control)
SG_HOOK("golf_clean.exe", 0x00404c20, neg_initGlobalArray_00404c20, Neg404c20_re, Neg404c20_orig);
// 0x00403f40  atexitRegister_00403f40 (negative control)
SG_HOOK("golf_clean.exe", 0x00403f40, neg_atexitRegister_00403f40, Neg403f40_re, Neg403f40_orig);
// 0x00404310  dynInit_staticInit_00404320 (negative control)
SG_HOOK("golf_clean.exe", 0x00404310, neg_dynInit_staticInit_00404320, Neg404310_re, Neg404310_orig);
