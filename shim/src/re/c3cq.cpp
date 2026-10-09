// C3 batch c3cq (round 22, 2026-10-09) of golf_clean.exe: the statically linked MSVC CRT's static-initialisation
// rows and a few CRT helpers.
//   - 54 exit-routine registrations: `push <fn>; call atexit 0x004a56d2; pop ecx; ret`.
//   - 40 static constructors of single global objects: `mov ecx, <global>; jmp <constructor>`.
//   - 3 static constructors of global arrays through the eh vector constructor iterator 0x004a55d2.
//   - errnoPtr / doserrnoPtr (__getptd 0x004a8e53 + 8 / + 0xc), initFpControl (the six float-conversion
//     pointers 0x004e4b00..0x004e4b14), the heap-scope guard's restore 0x004021a0 and conditional free 0x004021b0,
//     and the global Reader 0x0083b000's constructor / close wrappers and the Cache482f60 static constructor.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list)
// and cites the address of each instruction it reproduces; the uniform families are emitted by log/c3/c3cq_gen.py,
// which re-reads each function's bytes from original/golf_clean.exe and refuses a row whose shape differs.
// __thiscall is emulated with __fastcall (ecx = this, edx unused). Callees run through their original addresses.
#include "hooks.h"

namespace {

typedef void(__cdecl* ExitFn_t)();
typedef int(__cdecl* Atexit_t)(ExitFn_t fn);
const Atexit_t kAtexit = reinterpret_cast<Atexit_t>(0x004a56d2);             // atexit (FidDb name _atexit)

// __ehvec_ctor-style iterator 0x004a55d2: (base, size, count, ctor, dtor), `ret 0x14` at 0x004a5639; it calls
// the constructor with ecx = each element (0x004a560d) while the signed counter < count (jge at 0x004a5606).
typedef void(__stdcall* EhVectorCtor_t)(void* base, unsigned size, int count, void* ctor, void* dtor);
const EhVectorCtor_t kEhVectorCtor = reinterpret_cast<EhVectorCtor_t>(0x004a55d2);

// A constructor with no stack argument, called with ecx = the object; it returns the object in eax.
typedef void*(__fastcall* Ctor_t)(void* ecx, void* edx);
inline void* CtorAt(unsigned ctor, unsigned obj) {
    return reinterpret_cast<Ctor_t>(ctor)(reinterpret_cast<void*>(obj), 0);
}
// The same with one stack argument (`ret 4`).
typedef void*(__fastcall* Ctor1_t)(void* ecx, void* edx, int arg);
inline void* Ctor1At(unsigned ctor, unsigned obj, int arg) {
    return reinterpret_cast<Ctor1_t>(ctor)(reinterpret_cast<void*>(obj), 0, arg);
}

typedef int(__cdecl* Fn0_t)();
typedef void*(__cdecl* PFn0_t)();
typedef void(__cdecl* Fn0v_t)();

// 0x004378f0  registerExit_4378f0(): registers the exit routine 0x00437900
// with atexit 0x004a56d2: `push 0x00437900` at 0x004378f0, `call 0x004a56d2` at 0x004378f5, and atexit's result
// returned unchanged (`pop ecx` 0x004378fa, `ret` 0x004378fb).
Fn0_t registerExit_4378f0_orig;
int __cdecl registerExit_4378f0_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x00437900)); }
SG_HOOK("golf_clean.exe", 0x004378f0, registerExit_4378f0, registerExit_4378f0_re, registerExit_4378f0_orig);

// 0x004385b0  registerExit_4385b0(): registers the exit routine 0x004385c0
// with atexit 0x004a56d2: `push 0x004385c0` at 0x004385b0, `call 0x004a56d2` at 0x004385b5, and atexit's result
// returned unchanged (`pop ecx` 0x004385ba, `ret` 0x004385bb).
Fn0_t registerExit_4385b0_orig;
int __cdecl registerExit_4385b0_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x004385c0)); }
SG_HOOK("golf_clean.exe", 0x004385b0, registerExit_4385b0, registerExit_4385b0_re, registerExit_4385b0_orig);

// 0x0043cc30  registerExit_43cc30(): registers the exit routine 0x0043cc40
// with atexit 0x004a56d2: `push 0x0043cc40` at 0x0043cc30, `call 0x004a56d2` at 0x0043cc35, and atexit's result
// returned unchanged (`pop ecx` 0x0043cc3a, `ret` 0x0043cc3b).
Fn0_t registerExit_43cc30_orig;
int __cdecl registerExit_43cc30_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0043cc40)); }
SG_HOOK("golf_clean.exe", 0x0043cc30, registerExit_43cc30, registerExit_43cc30_re, registerExit_43cc30_orig);

// 0x0043cc80  registerExit_43cc80(): registers the exit routine 0x0043cc90
// with atexit 0x004a56d2: `push 0x0043cc90` at 0x0043cc80, `call 0x004a56d2` at 0x0043cc85, and atexit's result
// returned unchanged (`pop ecx` 0x0043cc8a, `ret` 0x0043cc8b).
Fn0_t registerExit_43cc80_orig;
int __cdecl registerExit_43cc80_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0043cc90)); }
SG_HOOK("golf_clean.exe", 0x0043cc80, registerExit_43cc80, registerExit_43cc80_re, registerExit_43cc80_orig);

// 0x0043ccc0  registerExit_43ccc0(): registers the exit routine 0x0043ccd0
// with atexit 0x004a56d2: `push 0x0043ccd0` at 0x0043ccc0, `call 0x004a56d2` at 0x0043ccc5, and atexit's result
// returned unchanged (`pop ecx` 0x0043ccca, `ret` 0x0043cccb).
Fn0_t registerExit_43ccc0_orig;
int __cdecl registerExit_43ccc0_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0043ccd0)); }
SG_HOOK("golf_clean.exe", 0x0043ccc0, registerExit_43ccc0, registerExit_43ccc0_re, registerExit_43ccc0_orig);

// 0x0043d490  registerExit_43d490(): registers the exit routine 0x0043d4a0
// with atexit 0x004a56d2: `push 0x0043d4a0` at 0x0043d490, `call 0x004a56d2` at 0x0043d495, and atexit's result
// returned unchanged (`pop ecx` 0x0043d49a, `ret` 0x0043d49b).
Fn0_t registerExit_43d490_orig;
int __cdecl registerExit_43d490_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0043d4a0)); }
SG_HOOK("golf_clean.exe", 0x0043d490, registerExit_43d490, registerExit_43d490_re, registerExit_43d490_orig);

// 0x0043d4f0  registerExit_43d4f0(): registers the exit routine 0x0043d500
// with atexit 0x004a56d2: `push 0x0043d500` at 0x0043d4f0, `call 0x004a56d2` at 0x0043d4f5, and atexit's result
// returned unchanged (`pop ecx` 0x0043d4fa, `ret` 0x0043d4fb).
Fn0_t registerExit_43d4f0_orig;
int __cdecl registerExit_43d4f0_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0043d500)); }
SG_HOOK("golf_clean.exe", 0x0043d4f0, registerExit_43d4f0, registerExit_43d4f0_re, registerExit_43d4f0_orig);

// 0x0043d650  registerExit_43d650(): registers the exit routine 0x0043d660
// with atexit 0x004a56d2: `push 0x0043d660` at 0x0043d650, `call 0x004a56d2` at 0x0043d655, and atexit's result
// returned unchanged (`pop ecx` 0x0043d65a, `ret` 0x0043d65b).
Fn0_t registerExit_43d650_orig;
int __cdecl registerExit_43d650_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0043d660)); }
SG_HOOK("golf_clean.exe", 0x0043d650, registerExit_43d650, registerExit_43d650_re, registerExit_43d650_orig);

// 0x00448180  registerExit_448180(): registers the exit routine 0x00448190
// with atexit 0x004a56d2: `push 0x00448190` at 0x00448180, `call 0x004a56d2` at 0x00448185, and atexit's result
// returned unchanged (`pop ecx` 0x0044818a, `ret` 0x0044818b).
Fn0_t registerExit_448180_orig;
int __cdecl registerExit_448180_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x00448190)); }
SG_HOOK("golf_clean.exe", 0x00448180, registerExit_448180, registerExit_448180_re, registerExit_448180_orig);

// 0x004491d0  registerExit_4491d0(): registers the exit routine 0x004491e0
// with atexit 0x004a56d2: `push 0x004491e0` at 0x004491d0, `call 0x004a56d2` at 0x004491d5, and atexit's result
// returned unchanged (`pop ecx` 0x004491da, `ret` 0x004491db).
Fn0_t registerExit_4491d0_orig;
int __cdecl registerExit_4491d0_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x004491e0)); }
SG_HOOK("golf_clean.exe", 0x004491d0, registerExit_4491d0, registerExit_4491d0_re, registerExit_4491d0_orig);

// 0x00449240  registerExit_449240(): registers the exit routine 0x00449250
// with atexit 0x004a56d2: `push 0x00449250` at 0x00449240, `call 0x004a56d2` at 0x00449245, and atexit's result
// returned unchanged (`pop ecx` 0x0044924a, `ret` 0x0044924b).
Fn0_t registerExit_449240_orig;
int __cdecl registerExit_449240_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x00449250)); }
SG_HOOK("golf_clean.exe", 0x00449240, registerExit_449240, registerExit_449240_re, registerExit_449240_orig);

// 0x0044aca0  registerExit_44aca0(): registers the exit routine 0x0044acb0
// with atexit 0x004a56d2: `push 0x0044acb0` at 0x0044aca0, `call 0x004a56d2` at 0x0044aca5, and atexit's result
// returned unchanged (`pop ecx` 0x0044acaa, `ret` 0x0044acab).
Fn0_t registerExit_44aca0_orig;
int __cdecl registerExit_44aca0_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0044acb0)); }
SG_HOOK("golf_clean.exe", 0x0044aca0, registerExit_44aca0, registerExit_44aca0_re, registerExit_44aca0_orig);

// 0x0044ad40  registerExit_44ad40(): registers the exit routine 0x0044ad50
// with atexit 0x004a56d2: `push 0x0044ad50` at 0x0044ad40, `call 0x004a56d2` at 0x0044ad45, and atexit's result
// returned unchanged (`pop ecx` 0x0044ad4a, `ret` 0x0044ad4b).
Fn0_t registerExit_44ad40_orig;
int __cdecl registerExit_44ad40_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0044ad50)); }
SG_HOOK("golf_clean.exe", 0x0044ad40, registerExit_44ad40, registerExit_44ad40_re, registerExit_44ad40_orig);

// 0x0044ad90  registerExit_44ad90(): registers the exit routine 0x0044ada0
// with atexit 0x004a56d2: `push 0x0044ada0` at 0x0044ad90, `call 0x004a56d2` at 0x0044ad95, and atexit's result
// returned unchanged (`pop ecx` 0x0044ad9a, `ret` 0x0044ad9b).
Fn0_t registerExit_44ad90_orig;
int __cdecl registerExit_44ad90_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0044ada0)); }
SG_HOOK("golf_clean.exe", 0x0044ad90, registerExit_44ad90, registerExit_44ad90_re, registerExit_44ad90_orig);

// 0x0044ade0  registerExit_44ade0(): registers the exit routine 0x0044adf0 (atexitDtor_8210c8)
// with atexit 0x004a56d2: `push 0x0044adf0` at 0x0044ade0, `call 0x004a56d2` at 0x0044ade5, and atexit's result
// returned unchanged (`pop ecx` 0x0044adea, `ret` 0x0044adeb).
Fn0_t registerExit_44ade0_orig;
int __cdecl registerExit_44ade0_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0044adf0)); }
SG_HOOK("golf_clean.exe", 0x0044ade0, registerExit_44ade0, registerExit_44ade0_re, registerExit_44ade0_orig);

// 0x0044aed0  registerExit_44aed0(): registers the exit routine 0x0044aee0 (atexitDtor_821c60)
// with atexit 0x004a56d2: `push 0x0044aee0` at 0x0044aed0, `call 0x004a56d2` at 0x0044aed5, and atexit's result
// returned unchanged (`pop ecx` 0x0044aeda, `ret` 0x0044aedb).
Fn0_t registerExit_44aed0_orig;
int __cdecl registerExit_44aed0_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0044aee0)); }
SG_HOOK("golf_clean.exe", 0x0044aed0, registerExit_44aed0, registerExit_44aed0_re, registerExit_44aed0_orig);

// 0x0044af10  registerExit_44af10(): registers the exit routine 0x0044af20 (atexitDtor_821bf8)
// with atexit 0x004a56d2: `push 0x0044af20` at 0x0044af10, `call 0x004a56d2` at 0x0044af15, and atexit's result
// returned unchanged (`pop ecx` 0x0044af1a, `ret` 0x0044af1b).
Fn0_t registerExit_44af10_orig;
int __cdecl registerExit_44af10_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0044af20)); }
SG_HOOK("golf_clean.exe", 0x0044af10, registerExit_44af10, registerExit_44af10_re, registerExit_44af10_orig);

// 0x0044af50  registerExit_44af50(): registers the exit routine 0x0044af60 (atexitDtor_821070)
// with atexit 0x004a56d2: `push 0x0044af60` at 0x0044af50, `call 0x004a56d2` at 0x0044af55, and atexit's result
// returned unchanged (`pop ecx` 0x0044af5a, `ret` 0x0044af5b).
Fn0_t registerExit_44af50_orig;
int __cdecl registerExit_44af50_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0044af60)); }
SG_HOOK("golf_clean.exe", 0x0044af50, registerExit_44af50, registerExit_44af50_re, registerExit_44af50_orig);

// 0x0044af90  registerExit_44af90(): registers the exit routine 0x0044afa0 (atexitDtor_821200)
// with atexit 0x004a56d2: `push 0x0044afa0` at 0x0044af90, `call 0x004a56d2` at 0x0044af95, and atexit's result
// returned unchanged (`pop ecx` 0x0044af9a, `ret` 0x0044af9b).
Fn0_t registerExit_44af90_orig;
int __cdecl registerExit_44af90_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0044afa0)); }
SG_HOOK("golf_clean.exe", 0x0044af90, registerExit_44af90, registerExit_44af90_re, registerExit_44af90_orig);

// 0x0044b110  registerExit_44b110(): registers the exit routine 0x0044b120 (atexitDtor_8223c0)
// with atexit 0x004a56d2: `push 0x0044b120` at 0x0044b110, `call 0x004a56d2` at 0x0044b115, and atexit's result
// returned unchanged (`pop ecx` 0x0044b11a, `ret` 0x0044b11b).
Fn0_t registerExit_44b110_orig;
int __cdecl registerExit_44b110_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0044b120)); }
SG_HOOK("golf_clean.exe", 0x0044b110, registerExit_44b110, registerExit_44b110_re, registerExit_44b110_orig);

// 0x0044b150  registerExit_44b150(): registers the exit routine 0x0044b160 (atexitDtor_8222b8)
// with atexit 0x004a56d2: `push 0x0044b160` at 0x0044b150, `call 0x004a56d2` at 0x0044b155, and atexit's result
// returned unchanged (`pop ecx` 0x0044b15a, `ret` 0x0044b15b).
Fn0_t registerExit_44b150_orig;
int __cdecl registerExit_44b150_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0044b160)); }
SG_HOOK("golf_clean.exe", 0x0044b150, registerExit_44b150, registerExit_44b150_re, registerExit_44b150_orig);

// 0x0044b190  registerExit_44b190(): registers the exit routine 0x0044b1a0 (atexitDtor_821308)
// with atexit 0x004a56d2: `push 0x0044b1a0` at 0x0044b190, `call 0x004a56d2` at 0x0044b195, and atexit's result
// returned unchanged (`pop ecx` 0x0044b19a, `ret` 0x0044b19b).
Fn0_t registerExit_44b190_orig;
int __cdecl registerExit_44b190_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0044b1a0)); }
SG_HOOK("golf_clean.exe", 0x0044b190, registerExit_44b190, registerExit_44b190_re, registerExit_44b190_orig);

// 0x0044b3c0  registerExit_44b3c0(): registers the exit routine 0x0044b3d0 (atexitDtor_821f48)
// with atexit 0x004a56d2: `push 0x0044b3d0` at 0x0044b3c0, `call 0x004a56d2` at 0x0044b3c5, and atexit's result
// returned unchanged (`pop ecx` 0x0044b3ca, `ret` 0x0044b3cb).
Fn0_t registerExit_44b3c0_orig;
int __cdecl registerExit_44b3c0_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0044b3d0)); }
SG_HOOK("golf_clean.exe", 0x0044b3c0, registerExit_44b3c0, registerExit_44b3c0_re, registerExit_44b3c0_orig);

// 0x0044b4d0  registerExit_44b4d0(): registers the exit routine 0x0044b4e0 (atexitDtor_8224c8)
// with atexit 0x004a56d2: `push 0x0044b4e0` at 0x0044b4d0, `call 0x004a56d2` at 0x0044b4d5, and atexit's result
// returned unchanged (`pop ecx` 0x0044b4da, `ret` 0x0044b4db).
Fn0_t registerExit_44b4d0_orig;
int __cdecl registerExit_44b4d0_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0044b4e0)); }
SG_HOOK("golf_clean.exe", 0x0044b4d0, registerExit_44b4d0, registerExit_44b4d0_re, registerExit_44b4d0_orig);

// 0x0044b670  registerExit_44b670(): registers the exit routine 0x0044b680 (atexitDtor_821120)
// with atexit 0x004a56d2: `push 0x0044b680` at 0x0044b670, `call 0x004a56d2` at 0x0044b675, and atexit's result
// returned unchanged (`pop ecx` 0x0044b67a, `ret` 0x0044b67b).
Fn0_t registerExit_44b670_orig;
int __cdecl registerExit_44b670_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0044b680)); }
SG_HOOK("golf_clean.exe", 0x0044b670, registerExit_44b670, registerExit_44b670_re, registerExit_44b670_orig);

// 0x0044b770  registerExit_44b770(): registers the exit routine 0x0044b780 (atexitDtor_821cb8)
// with atexit 0x004a56d2: `push 0x0044b780` at 0x0044b770, `call 0x004a56d2` at 0x0044b775, and atexit's result
// returned unchanged (`pop ecx` 0x0044b77a, `ret` 0x0044b77b).
Fn0_t registerExit_44b770_orig;
int __cdecl registerExit_44b770_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0044b780)); }
SG_HOOK("golf_clean.exe", 0x0044b770, registerExit_44b770, registerExit_44b770_re, registerExit_44b770_orig);

// 0x0045ba70  registerExit_45ba70(): registers the exit routine 0x0045ba80
// with atexit 0x004a56d2: `push 0x0045ba80` at 0x0045ba70, `call 0x004a56d2` at 0x0045ba75, and atexit's result
// returned unchanged (`pop ecx` 0x0045ba7a, `ret` 0x0045ba7b).
Fn0_t registerExit_45ba70_orig;
int __cdecl registerExit_45ba70_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0045ba80)); }
SG_HOOK("golf_clean.exe", 0x0045ba70, registerExit_45ba70, registerExit_45ba70_re, registerExit_45ba70_orig);

// 0x0045bac0  registerExit_45bac0(): registers the exit routine 0x0045bad0
// with atexit 0x004a56d2: `push 0x0045bad0` at 0x0045bac0, `call 0x004a56d2` at 0x0045bac5, and atexit's result
// returned unchanged (`pop ecx` 0x0045baca, `ret` 0x0045bacb).
Fn0_t registerExit_45bac0_orig;
int __cdecl registerExit_45bac0_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0045bad0)); }
SG_HOOK("golf_clean.exe", 0x0045bac0, registerExit_45bac0, registerExit_45bac0_re, registerExit_45bac0_orig);

// 0x0046de50  atexitThunk_46de50(): registers the exit routine 0x0046de60
// with atexit 0x004a56d2: `push 0x0046de60` at 0x0046de50, `call 0x004a56d2` at 0x0046de55, and atexit's result
// returned unchanged (`pop ecx` 0x0046de5a, `ret` 0x0046de5b).
Fn0_t atexitThunk_46de50_orig;
int __cdecl atexitThunk_46de50_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0046de60)); }
SG_HOOK("golf_clean.exe", 0x0046de50, atexitThunk_46de50, atexitThunk_46de50_re, atexitThunk_46de50_orig);

// 0x00473a80  atexitThunk_473a80(): registers the exit routine 0x00473a90
// with atexit 0x004a56d2: `push 0x00473a90` at 0x00473a80, `call 0x004a56d2` at 0x00473a85, and atexit's result
// returned unchanged (`pop ecx` 0x00473a8a, `ret` 0x00473a8b).
Fn0_t atexitThunk_473a80_orig;
int __cdecl atexitThunk_473a80_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x00473a90)); }
SG_HOOK("golf_clean.exe", 0x00473a80, atexitThunk_473a80, atexitThunk_473a80_re, atexitThunk_473a80_orig);

// 0x00474aa0  atexitThunk_474aa0(): registers the exit routine 0x00474ab0
// with atexit 0x004a56d2: `push 0x00474ab0` at 0x00474aa0, `call 0x004a56d2` at 0x00474aa5, and atexit's result
// returned unchanged (`pop ecx` 0x00474aaa, `ret` 0x00474aab).
Fn0_t atexitThunk_474aa0_orig;
int __cdecl atexitThunk_474aa0_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x00474ab0)); }
SG_HOOK("golf_clean.exe", 0x00474aa0, atexitThunk_474aa0, atexitThunk_474aa0_re, atexitThunk_474aa0_orig);

// 0x00479a00  atexitThunk_479a00(): registers the exit routine 0x00479a10
// with atexit 0x004a56d2: `push 0x00479a10` at 0x00479a00, `call 0x004a56d2` at 0x00479a05, and atexit's result
// returned unchanged (`pop ecx` 0x00479a0a, `ret` 0x00479a0b).
Fn0_t atexitThunk_479a00_orig;
int __cdecl atexitThunk_479a00_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x00479a10)); }
SG_HOOK("golf_clean.exe", 0x00479a00, atexitThunk_479a00, atexitThunk_479a00_re, atexitThunk_479a00_orig);

// 0x00479a50  atexitThunk_479a50(): registers the exit routine 0x00479a60
// with atexit 0x004a56d2: `push 0x00479a60` at 0x00479a50, `call 0x004a56d2` at 0x00479a55, and atexit's result
// returned unchanged (`pop ecx` 0x00479a5a, `ret` 0x00479a5b).
Fn0_t atexitThunk_479a50_orig;
int __cdecl atexitThunk_479a50_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x00479a60)); }
SG_HOOK("golf_clean.exe", 0x00479a50, atexitThunk_479a50, atexitThunk_479a50_re, atexitThunk_479a50_orig);

// 0x00482e90  atexit_482ea0(): registers the exit routine 0x00482ea0
// with atexit 0x004a56d2: `push 0x00482ea0` at 0x00482e90, `call 0x004a56d2` at 0x00482e95, and atexit's result
// returned unchanged (`pop ecx` 0x00482e9a, `ret` 0x00482e9b).
Fn0_t atexit_482ea0_orig;
int __cdecl atexit_482ea0_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x00482ea0)); }
SG_HOOK("golf_clean.exe", 0x00482e90, atexit_482ea0, atexit_482ea0_re, atexit_482ea0_orig);

// 0x00482ed0  atexit_482ee0(): registers the exit routine 0x00482ee0
// with atexit 0x004a56d2: `push 0x00482ee0` at 0x00482ed0, `call 0x004a56d2` at 0x00482ed5, and atexit's result
// returned unchanged (`pop ecx` 0x00482eda, `ret` 0x00482edb).
Fn0_t atexit_482ee0_orig;
int __cdecl atexit_482ee0_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x00482ee0)); }
SG_HOOK("golf_clean.exe", 0x00482ed0, atexit_482ee0, atexit_482ee0_re, atexit_482ee0_orig);

// 0x00482f10  atexit_482f20(): registers the exit routine 0x00482f20 (atexitDtor_83aca8)
// with atexit 0x004a56d2: `push 0x00482f20` at 0x00482f10, `call 0x004a56d2` at 0x00482f15, and atexit's result
// returned unchanged (`pop ecx` 0x00482f1a, `ret` 0x00482f1b).
Fn0_t atexit_482f20_orig;
int __cdecl atexit_482f20_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x00482f20)); }
SG_HOOK("golf_clean.exe", 0x00482f10, atexit_482f20, atexit_482f20_re, atexit_482f20_orig);

// 0x004837c0  atexit_4837d0(): registers the exit routine 0x004837d0
// with atexit 0x004a56d2: `push 0x004837d0` at 0x004837c0, `call 0x004a56d2` at 0x004837c5, and atexit's result
// returned unchanged (`pop ecx` 0x004837ca, `ret` 0x004837cb).
Fn0_t atexit_4837d0_orig;
int __cdecl atexit_4837d0_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x004837d0)); }
SG_HOOK("golf_clean.exe", 0x004837c0, atexit_4837d0, atexit_4837d0_re, atexit_4837d0_orig);

// 0x00483df0  atexit_483e00(): registers the exit routine 0x00483e00
// with atexit 0x004a56d2: `push 0x00483e00` at 0x00483df0, `call 0x004a56d2` at 0x00483df5, and atexit's result
// returned unchanged (`pop ecx` 0x00483dfa, `ret` 0x00483dfb).
Fn0_t atexit_483e00_orig;
int __cdecl atexit_483e00_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x00483e00)); }
SG_HOOK("golf_clean.exe", 0x00483df0, atexit_483e00, atexit_483e00_re, atexit_483e00_orig);

// 0x00483e30  atexit_483e40(): registers the exit routine 0x00483e40
// with atexit 0x004a56d2: `push 0x00483e40` at 0x00483e30, `call 0x004a56d2` at 0x00483e35, and atexit's result
// returned unchanged (`pop ecx` 0x00483e3a, `ret` 0x00483e3b).
Fn0_t atexit_483e40_orig;
int __cdecl atexit_483e40_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x00483e40)); }
SG_HOOK("golf_clean.exe", 0x00483e30, atexit_483e40, atexit_483e40_re, atexit_483e40_orig);

// 0x00483e70  atexit_483e80(): registers the exit routine 0x00483e80
// with atexit 0x004a56d2: `push 0x00483e80` at 0x00483e70, `call 0x004a56d2` at 0x00483e75, and atexit's result
// returned unchanged (`pop ecx` 0x00483e7a, `ret` 0x00483e7b).
Fn0_t atexit_483e80_orig;
int __cdecl atexit_483e80_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x00483e80)); }
SG_HOOK("golf_clean.exe", 0x00483e70, atexit_483e80, atexit_483e80_re, atexit_483e80_orig);

// 0x00487d90  registerReaderAtexit(): registers the exit routine 0x00487da0
// with atexit 0x004a56d2: `push 0x00487da0` at 0x00487d90, `call 0x004a56d2` at 0x00487d95, and atexit's result
// returned unchanged (`pop ecx` 0x00487d9a, `ret` 0x00487d9b).
Fn0_t registerReaderAtexit_orig;
int __cdecl registerReaderAtexit_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x00487da0)); }
SG_HOOK("golf_clean.exe", 0x00487d90, registerReaderAtexit, registerReaderAtexit_re, registerReaderAtexit_orig);

// 0x00487e30  registerKeyTableAtexit(): registers the exit routine 0x00487e40
// with atexit 0x004a56d2: `push 0x00487e40` at 0x00487e30, `call 0x004a56d2` at 0x00487e35, and atexit's result
// returned unchanged (`pop ecx` 0x00487e3a, `ret` 0x00487e3b).
Fn0_t registerKeyTableAtexit_orig;
int __cdecl registerKeyTableAtexit_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x00487e40)); }
SG_HOOK("golf_clean.exe", 0x00487e30, registerKeyTableAtexit, registerKeyTableAtexit_re, registerKeyTableAtexit_orig);

// 0x0049c000  regAtexit_49c000(): registers the exit routine 0x0049c010
// with atexit 0x004a56d2: `push 0x0049c010` at 0x0049c000, `call 0x004a56d2` at 0x0049c005, and atexit's result
// returned unchanged (`pop ecx` 0x0049c00a, `ret` 0x0049c00b).
Fn0_t regAtexit_49c000_orig;
int __cdecl regAtexit_49c000_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0049c010)); }
SG_HOOK("golf_clean.exe", 0x0049c000, regAtexit_49c000, regAtexit_49c000_re, regAtexit_49c000_orig);

// 0x0049d020  regAtexit_49d020(): registers the exit routine 0x0049d030
// with atexit 0x004a56d2: `push 0x0049d030` at 0x0049d020, `call 0x004a56d2` at 0x0049d025, and atexit's result
// returned unchanged (`pop ecx` 0x0049d02a, `ret` 0x0049d02b).
Fn0_t regAtexit_49d020_orig;
int __cdecl regAtexit_49d020_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0049d030)); }
SG_HOOK("golf_clean.exe", 0x0049d020, regAtexit_49d020, regAtexit_49d020_re, regAtexit_49d020_orig);

// 0x0049d170  regAtexit_49d170(): registers the exit routine 0x0049d180
// with atexit 0x004a56d2: `push 0x0049d180` at 0x0049d170, `call 0x004a56d2` at 0x0049d175, and atexit's result
// returned unchanged (`pop ecx` 0x0049d17a, `ret` 0x0049d17b).
Fn0_t regAtexit_49d170_orig;
int __cdecl regAtexit_49d170_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0049d180)); }
SG_HOOK("golf_clean.exe", 0x0049d170, regAtexit_49d170, regAtexit_49d170_re, regAtexit_49d170_orig);

// 0x0049d2b0  regAtexit_49d2b0(): registers the exit routine 0x0049d2c0
// with atexit 0x004a56d2: `push 0x0049d2c0` at 0x0049d2b0, `call 0x004a56d2` at 0x0049d2b5, and atexit's result
// returned unchanged (`pop ecx` 0x0049d2ba, `ret` 0x0049d2bb).
Fn0_t regAtexit_49d2b0_orig;
int __cdecl regAtexit_49d2b0_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0049d2c0)); }
SG_HOOK("golf_clean.exe", 0x0049d2b0, regAtexit_49d2b0, regAtexit_49d2b0_re, regAtexit_49d2b0_orig);

// 0x0049d310  regAtexit_49d310(): registers the exit routine 0x0049d320
// with atexit 0x004a56d2: `push 0x0049d320` at 0x0049d310, `call 0x004a56d2` at 0x0049d315, and atexit's result
// returned unchanged (`pop ecx` 0x0049d31a, `ret` 0x0049d31b).
Fn0_t regAtexit_49d310_orig;
int __cdecl regAtexit_49d310_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0049d320)); }
SG_HOOK("golf_clean.exe", 0x0049d310, regAtexit_49d310, regAtexit_49d310_re, regAtexit_49d310_orig);

// 0x0049d370  regAtexit_49d370(): registers the exit routine 0x0049d380
// with atexit 0x004a56d2: `push 0x0049d380` at 0x0049d370, `call 0x004a56d2` at 0x0049d375, and atexit's result
// returned unchanged (`pop ecx` 0x0049d37a, `ret` 0x0049d37b).
Fn0_t regAtexit_49d370_orig;
int __cdecl regAtexit_49d370_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0049d380)); }
SG_HOOK("golf_clean.exe", 0x0049d370, regAtexit_49d370, regAtexit_49d370_re, regAtexit_49d370_orig);

// 0x0049d4a0  regAtexit_49d4a0(): registers the exit routine 0x0049d4b0
// with atexit 0x004a56d2: `push 0x0049d4b0` at 0x0049d4a0, `call 0x004a56d2` at 0x0049d4a5, and atexit's result
// returned unchanged (`pop ecx` 0x0049d4aa, `ret` 0x0049d4ab).
Fn0_t regAtexit_49d4a0_orig;
int __cdecl regAtexit_49d4a0_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0049d4b0)); }
SG_HOOK("golf_clean.exe", 0x0049d4a0, regAtexit_49d4a0, regAtexit_49d4a0_re, regAtexit_49d4a0_orig);

// 0x0049d500  regAtexit_49d500(): registers the exit routine 0x0049d510
// with atexit 0x004a56d2: `push 0x0049d510` at 0x0049d500, `call 0x004a56d2` at 0x0049d505, and atexit's result
// returned unchanged (`pop ecx` 0x0049d50a, `ret` 0x0049d50b).
Fn0_t regAtexit_49d500_orig;
int __cdecl regAtexit_49d500_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0049d510)); }
SG_HOOK("golf_clean.exe", 0x0049d500, regAtexit_49d500, regAtexit_49d500_re, regAtexit_49d500_orig);

// 0x0049d560  regAtexit_49d560(): registers the exit routine 0x0049d570
// with atexit 0x004a56d2: `push 0x0049d570` at 0x0049d560, `call 0x004a56d2` at 0x0049d565, and atexit's result
// returned unchanged (`pop ecx` 0x0049d56a, `ret` 0x0049d56b).
Fn0_t regAtexit_49d560_orig;
int __cdecl regAtexit_49d560_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0049d570)); }
SG_HOOK("golf_clean.exe", 0x0049d560, regAtexit_49d560, regAtexit_49d560_re, regAtexit_49d560_orig);

// 0x0049eb10  regAtexit_49eb10(): registers the exit routine 0x0049eb20
// with atexit 0x004a56d2: `push 0x0049eb20` at 0x0049eb10, `call 0x004a56d2` at 0x0049eb15, and atexit's result
// returned unchanged (`pop ecx` 0x0049eb1a, `ret` 0x0049eb1b).
Fn0_t regAtexit_49eb10_orig;
int __cdecl regAtexit_49eb10_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0049eb20)); }
SG_HOOK("golf_clean.exe", 0x0049eb10, regAtexit_49eb10, regAtexit_49eb10_re, regAtexit_49eb10_orig);

// 0x0049eb70  regAtexit_49eb70(): registers the exit routine 0x0049eb80
// with atexit 0x004a56d2: `push 0x0049eb80` at 0x0049eb70, `call 0x004a56d2` at 0x0049eb75, and atexit's result
// returned unchanged (`pop ecx` 0x0049eb7a, `ret` 0x0049eb7b).
Fn0_t regAtexit_49eb70_orig;
int __cdecl regAtexit_49eb70_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x0049eb80)); }
SG_HOOK("golf_clean.exe", 0x0049eb70, regAtexit_49eb70, regAtexit_49eb70_re, regAtexit_49eb70_orig);

// 0x004a0140  regAtexit_4a0140(): registers the exit routine 0x004a0150
// with atexit 0x004a56d2: `push 0x004a0150` at 0x004a0140, `call 0x004a56d2` at 0x004a0145, and atexit's result
// returned unchanged (`pop ecx` 0x004a014a, `ret` 0x004a014b).
Fn0_t regAtexit_4a0140_orig;
int __cdecl regAtexit_4a0140_re() { return kAtexit(reinterpret_cast<ExitFn_t>(0x004a0150)); }
SG_HOOK("golf_clean.exe", 0x004a0140, regAtexit_4a0140, regAtexit_4a0140_re, regAtexit_4a0140_orig);

// 0x004043f0  staticInit_004043f0(): constructs the global object at 0x00519928 in place,
// with `mov ecx, 0x00519928` at 0x004043f0 and a tail `jmp 0x004837f0` (C4837f0::ctor) at 0x004043f5, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_004043f0_orig;
void* __cdecl staticInit_004043f0_re() { return CtorAt(0x004837f0, 0x00519928); }
SG_HOOK("golf_clean.exe", 0x004043f0, staticInit_004043f0, staticInit_004043f0_re, staticInit_004043f0_orig);

// 0x00404440  staticInit_00404440(): constructs the global object at 0x00519fd8 in place,
// with `mov ecx, 0x00519fd8` at 0x00404440 and a tail `jmp 0x004837f0` (C4837f0::ctor) at 0x00404445, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_00404440_orig;
void* __cdecl staticInit_00404440_re() { return CtorAt(0x004837f0, 0x00519fd8); }
SG_HOOK("golf_clean.exe", 0x00404440, staticInit_00404440, staticInit_00404440_re, staticInit_00404440_orig);

// 0x00404490  staticInit_00404490(): constructs the global object at 0x0051b320 in place,
// with `mov ecx, 0x0051b320` at 0x00404490 and a tail `jmp 0x004837f0` (C4837f0::ctor) at 0x00404495, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_00404490_orig;
void* __cdecl staticInit_00404490_re() { return CtorAt(0x004837f0, 0x0051b320); }
SG_HOOK("golf_clean.exe", 0x00404490, staticInit_00404490, staticInit_00404490_re, staticInit_00404490_orig);

// 0x004044e0  staticInit_004044e0(): constructs the global object at 0x0051b360 in place,
// with `mov ecx, 0x0051b360` at 0x004044e0 and a tail `jmp 0x004837f0` (C4837f0::ctor) at 0x004044e5, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_004044e0_orig;
void* __cdecl staticInit_004044e0_re() { return CtorAt(0x004837f0, 0x0051b360); }
SG_HOOK("golf_clean.exe", 0x004044e0, staticInit_004044e0, staticInit_004044e0_re, staticInit_004044e0_orig);

// 0x00404530  staticInit_00404530(): constructs the global object at 0x00519948 in place,
// with `mov ecx, 0x00519948` at 0x00404530 and a tail `jmp 0x004837f0` (C4837f0::ctor) at 0x00404535, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_00404530_orig;
void* __cdecl staticInit_00404530_re() { return CtorAt(0x004837f0, 0x00519948); }
SG_HOOK("golf_clean.exe", 0x00404530, staticInit_00404530, staticInit_00404530_re, staticInit_00404530_orig);

// 0x00404580  staticInit_00404580(): constructs the global object at 0x00519a40 in place,
// with `mov ecx, 0x00519a40` at 0x00404580 and a tail `jmp 0x004837f0` (C4837f0::ctor) at 0x00404585, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_00404580_orig;
void* __cdecl staticInit_00404580_re() { return CtorAt(0x004837f0, 0x00519a40); }
SG_HOOK("golf_clean.exe", 0x00404580, staticInit_00404580, staticInit_00404580_re, staticInit_00404580_orig);

// 0x004045d0  staticInit_004045d0(): constructs the global object at 0x0051a028 in place,
// with `mov ecx, 0x0051a028` at 0x004045d0 and a tail `jmp 0x004837f0` (C4837f0::ctor) at 0x004045d5, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_004045d0_orig;
void* __cdecl staticInit_004045d0_re() { return CtorAt(0x004837f0, 0x0051a028); }
SG_HOOK("golf_clean.exe", 0x004045d0, staticInit_004045d0, staticInit_004045d0_re, staticInit_004045d0_orig);

// 0x00404620  staticInit_00404620(): constructs the global object at 0x0051b340 in place,
// with `mov ecx, 0x0051b340` at 0x00404620 and a tail `jmp 0x004837f0` (C4837f0::ctor) at 0x00404625, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_00404620_orig;
void* __cdecl staticInit_00404620_re() { return CtorAt(0x004837f0, 0x0051b340); }
SG_HOOK("golf_clean.exe", 0x00404620, staticInit_00404620, staticInit_00404620_re, staticInit_00404620_orig);

// 0x00404670  staticInit_00404670(): constructs the global object at 0x00519968 in place,
// with `mov ecx, 0x00519968` at 0x00404670 and a tail `jmp 0x004837f0` (C4837f0::ctor) at 0x00404675, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_00404670_orig;
void* __cdecl staticInit_00404670_re() { return CtorAt(0x004837f0, 0x00519968); }
SG_HOOK("golf_clean.exe", 0x00404670, staticInit_00404670, staticInit_00404670_re, staticInit_00404670_orig);

// 0x004046c0  staticInit_004046c0(): constructs the global object at 0x005199c8 in place,
// with `mov ecx, 0x005199c8` at 0x004046c0 and a tail `jmp 0x00482fd0` (Cache482fd0::ctor) at 0x004046c5, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_004046c0_orig;
void* __cdecl staticInit_004046c0_re() { return CtorAt(0x00482fd0, 0x005199c8); }
SG_HOOK("golf_clean.exe", 0x004046c0, staticInit_004046c0, staticInit_004046c0_re, staticInit_004046c0_orig);

// 0x00404700  staticInit_00404700(): constructs the global object at 0x00519a20 in place,
// with `mov ecx, 0x00519a20` at 0x00404700 and a tail `jmp 0x004837f0` (C4837f0::ctor) at 0x00404705, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_00404700_orig;
void* __cdecl staticInit_00404700_re() { return CtorAt(0x004837f0, 0x00519a20); }
SG_HOOK("golf_clean.exe", 0x00404700, staticInit_00404700, staticInit_00404700_re, staticInit_00404700_orig);

// 0x00404750  staticInit_00404750(): constructs the global object at 0x0051a008 in place,
// with `mov ecx, 0x0051a008` at 0x00404750 and a tail `jmp 0x004837f0` (C4837f0::ctor) at 0x00404755, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_00404750_orig;
void* __cdecl staticInit_00404750_re() { return CtorAt(0x004837f0, 0x0051a008); }
SG_HOOK("golf_clean.exe", 0x00404750, staticInit_00404750, staticInit_00404750_re, staticInit_00404750_orig);

// 0x004047a0  staticInit_004047a0(): constructs the global object at 0x00519988 in place,
// with `mov ecx, 0x00519988` at 0x004047a0 and a tail `jmp 0x004837f0` (C4837f0::ctor) at 0x004047a5, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_004047a0_orig;
void* __cdecl staticInit_004047a0_re() { return CtorAt(0x004837f0, 0x00519988); }
SG_HOOK("golf_clean.exe", 0x004047a0, staticInit_004047a0, staticInit_004047a0_re, staticInit_004047a0_orig);

// 0x004047f0  staticInit_004047f0(): constructs the global object at 0x005199a8 in place,
// with `mov ecx, 0x005199a8` at 0x004047f0 and a tail `jmp 0x004837f0` (C4837f0::ctor) at 0x004047f5, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_004047f0_orig;
void* __cdecl staticInit_004047f0_re() { return CtorAt(0x004837f0, 0x005199a8); }
SG_HOOK("golf_clean.exe", 0x004047f0, staticInit_004047f0, staticInit_004047f0_re, staticInit_004047f0_orig);

// 0x00404840  staticInit_00404840(): constructs the global object at 0x0051a048 in place,
// with `mov ecx, 0x0051a048` at 0x00404840 and a tail `jmp 0x004837f0` (C4837f0::ctor) at 0x00404845, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_00404840_orig;
void* __cdecl staticInit_00404840_re() { return CtorAt(0x004837f0, 0x0051a048); }
SG_HOOK("golf_clean.exe", 0x00404840, staticInit_00404840, staticInit_00404840_re, staticInit_00404840_orig);

// 0x004378e0  staticCtor_4378e0(): constructs the global object at 0x005aa560 in place,
// with `mov ecx, 0x005aa560` at 0x004378e0 and a tail `jmp 0x00482fd0` (Cache482fd0::ctor) at 0x004378e5, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_4378e0_orig;
void* __cdecl staticCtor_4378e0_re() { return CtorAt(0x00482fd0, 0x005aa560); }
SG_HOOK("golf_clean.exe", 0x004378e0, staticCtor_4378e0, staticCtor_4378e0_re, staticCtor_4378e0_orig);

// 0x0043cc70  staticCtor_43cc70(): constructs the global object at 0x005aa6f0 in place,
// with `mov ecx, 0x005aa6f0` at 0x0043cc70 and a tail `jmp 0x00482fd0` (Cache482fd0::ctor) at 0x0043cc75, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_43cc70_orig;
void* __cdecl staticCtor_43cc70_re() { return CtorAt(0x00482fd0, 0x005aa6f0); }
SG_HOOK("golf_clean.exe", 0x0043cc70, staticCtor_43cc70, staticCtor_43cc70_re, staticCtor_43cc70_orig);

// 0x0043ccb0  staticCtor_43ccb0(): constructs the global object at 0x005aa6d0 in place,
// with `mov ecx, 0x005aa6d0` at 0x0043ccb0 and a tail `jmp 0x00487000` (BinkPlayer::ctor) at 0x0043ccb5, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_43ccb0_orig;
void* __cdecl staticCtor_43ccb0_re() { return CtorAt(0x00487000, 0x005aa6d0); }
SG_HOOK("golf_clean.exe", 0x0043ccb0, staticCtor_43ccb0, staticCtor_43ccb0_re, staticCtor_43ccb0_orig);

// 0x0043d4e0  staticCtor_43d4e0(): constructs the global object at 0x00820e90 in place,
// with `mov ecx, 0x00820e90` at 0x0043d4e0 and a tail `jmp 0x00473ab0` (Surface_ctor) at 0x0043d4e5, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_43d4e0_orig;
void* __cdecl staticCtor_43d4e0_re() { return CtorAt(0x00473ab0, 0x00820e90); }
SG_HOOK("golf_clean.exe", 0x0043d4e0, staticCtor_43d4e0, staticCtor_43d4e0_re, staticCtor_43d4e0_orig);

// 0x004491c0  staticCtor_4491c0(): constructs the global object at 0x00820ed8 in place,
// with `mov ecx, 0x00820ed8` at 0x004491c0 and a tail `jmp 0x00473ab0` (Surface_ctor) at 0x004491c5, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_4491c0_orig;
void* __cdecl staticCtor_4491c0_re() { return CtorAt(0x00473ab0, 0x00820ed8); }
SG_HOOK("golf_clean.exe", 0x004491c0, staticCtor_4491c0, staticCtor_4491c0_re, staticCtor_4491c0_orig);

// 0x0044ad80  staticCtor_44ad80(): constructs the global object at 0x00821040 in place,
// with `mov ecx, 0x00821040` at 0x0044ad80 and a tail `jmp 0x00473ab0` (Surface_ctor) at 0x0044ad85, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_44ad80_orig;
void* __cdecl staticCtor_44ad80_re() { return CtorAt(0x00473ab0, 0x00821040); }
SG_HOOK("golf_clean.exe", 0x0044ad80, staticCtor_44ad80, staticCtor_44ad80_re, staticCtor_44ad80_orig);

// 0x0044add0  staticCtor_44add0(): constructs the global object at 0x008210c8 in place,
// with `mov ecx, 0x008210c8` at 0x0044add0 and a tail `jmp 0x0044ae60` (gSurfaceCtor_44ae60) at 0x0044add5, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_44add0_orig;
void* __cdecl staticCtor_44add0_re() { return CtorAt(0x0044ae60, 0x008210c8); }
SG_HOOK("golf_clean.exe", 0x0044add0, staticCtor_44add0, staticCtor_44add0_re, staticCtor_44add0_orig);

// 0x0044aec0  staticCtor_44aec0(): constructs the global object at 0x00821c60 in place,
// with `mov ecx, 0x00821c60` at 0x0044aec0 and a tail `jmp 0x0044ae60` (gSurfaceCtor_44ae60) at 0x0044aec5, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_44aec0_orig;
void* __cdecl staticCtor_44aec0_re() { return CtorAt(0x0044ae60, 0x00821c60); }
SG_HOOK("golf_clean.exe", 0x0044aec0, staticCtor_44aec0, staticCtor_44aec0_re, staticCtor_44aec0_orig);

// 0x0044af00  staticCtor_44af00(): constructs the global object at 0x00821bf8 in place,
// with `mov ecx, 0x00821bf8` at 0x0044af00 and a tail `jmp 0x0044ae60` (gSurfaceCtor_44ae60) at 0x0044af05, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_44af00_orig;
void* __cdecl staticCtor_44af00_re() { return CtorAt(0x0044ae60, 0x00821bf8); }
SG_HOOK("golf_clean.exe", 0x0044af00, staticCtor_44af00, staticCtor_44af00_re, staticCtor_44af00_orig);

// 0x0044af40  staticCtor_44af40(): constructs the global object at 0x00821070 in place,
// with `mov ecx, 0x00821070` at 0x0044af40 and a tail `jmp 0x0044ae60` (gSurfaceCtor_44ae60) at 0x0044af45, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_44af40_orig;
void* __cdecl staticCtor_44af40_re() { return CtorAt(0x0044ae60, 0x00821070); }
SG_HOOK("golf_clean.exe", 0x0044af40, staticCtor_44af40, staticCtor_44af40_re, staticCtor_44af40_orig);

// 0x0044af80  staticCtor_44af80(): constructs the global object at 0x00821200 in place,
// with `mov ecx, 0x00821200` at 0x0044af80 and a tail `jmp 0x0044b060` (gSurfaceCtor_44b060) at 0x0044af85, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_44af80_orig;
void* __cdecl staticCtor_44af80_re() { return CtorAt(0x0044b060, 0x00821200); }
SG_HOOK("golf_clean.exe", 0x0044af80, staticCtor_44af80, staticCtor_44af80_re, staticCtor_44af80_orig);

// 0x0044b100  staticCtor_44b100(): constructs the global object at 0x008223c0 in place,
// with `mov ecx, 0x008223c0` at 0x0044b100 and a tail `jmp 0x0044b060` (gSurfaceCtor_44b060) at 0x0044b105, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_44b100_orig;
void* __cdecl staticCtor_44b100_re() { return CtorAt(0x0044b060, 0x008223c0); }
SG_HOOK("golf_clean.exe", 0x0044b100, staticCtor_44b100, staticCtor_44b100_re, staticCtor_44b100_orig);

// 0x0044b140  staticCtor_44b140(): constructs the global object at 0x008222b8 in place,
// with `mov ecx, 0x008222b8` at 0x0044b140 and a tail `jmp 0x0044b060` (gSurfaceCtor_44b060) at 0x0044b145, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_44b140_orig;
void* __cdecl staticCtor_44b140_re() { return CtorAt(0x0044b060, 0x008222b8); }
SG_HOOK("golf_clean.exe", 0x0044b140, staticCtor_44b140, staticCtor_44b140_re, staticCtor_44b140_orig);

// 0x0044b180  staticCtor_44b180(): constructs the global object at 0x00821308 in place,
// with `mov ecx, 0x00821308` at 0x0044b180 and a tail `jmp 0x0044b2c0` (gSurfaceCtor_44b2c0) at 0x0044b185, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_44b180_orig;
void* __cdecl staticCtor_44b180_re() { return CtorAt(0x0044b2c0, 0x00821308); }
SG_HOOK("golf_clean.exe", 0x0044b180, staticCtor_44b180, staticCtor_44b180_re, staticCtor_44b180_orig);

// 0x0044b3b0  staticCtor_44b3b0(): constructs the global object at 0x00821f48 in place,
// with `mov ecx, 0x00821f48` at 0x0044b3b0 and a tail `jmp 0x0044b450` (gSurfaceCtor_44b450) at 0x0044b3b5, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_44b3b0_orig;
void* __cdecl staticCtor_44b3b0_re() { return CtorAt(0x0044b450, 0x00821f48); }
SG_HOOK("golf_clean.exe", 0x0044b3b0, staticCtor_44b3b0, staticCtor_44b3b0_re, staticCtor_44b3b0_orig);

// 0x0044b4c0  staticCtor_44b4c0(): constructs the global object at 0x008224c8 in place,
// with `mov ecx, 0x008224c8` at 0x0044b4c0 and a tail `jmp 0x0044b5a0` (gSurfaceCtor_44b5a0) at 0x0044b4c5, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_44b4c0_orig;
void* __cdecl staticCtor_44b4c0_re() { return CtorAt(0x0044b5a0, 0x008224c8); }
SG_HOOK("golf_clean.exe", 0x0044b4c0, staticCtor_44b4c0, staticCtor_44b4c0_re, staticCtor_44b4c0_orig);

// 0x0044b660  staticCtor_44b660(): constructs the global object at 0x00821120 in place,
// with `mov ecx, 0x00821120` at 0x0044b660 and a tail `jmp 0x0044b6f0` (gSurfaceCtor_44b6f0) at 0x0044b665, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_44b660_orig;
void* __cdecl staticCtor_44b660_re() { return CtorAt(0x0044b6f0, 0x00821120); }
SG_HOOK("golf_clean.exe", 0x0044b660, staticCtor_44b660, staticCtor_44b660_re, staticCtor_44b660_orig);

// 0x0044b760  staticCtor_44b760(): constructs the global object at 0x00821cb8 in place,
// with `mov ecx, 0x00821cb8` at 0x0044b760 and a tail `jmp 0x0044b8d0` (gSurfaceCtor_44b8d0) at 0x0044b765, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_44b760_orig;
void* __cdecl staticCtor_44b760_re() { return CtorAt(0x0044b8d0, 0x00821cb8); }
SG_HOOK("golf_clean.exe", 0x0044b760, staticCtor_44b760, staticCtor_44b760_re, staticCtor_44b760_orig);

// 0x0045ba60  staticCtor_45ba60(): constructs the global object at 0x00822cc8 in place,
// with `mov ecx, 0x00822cc8` at 0x0045ba60 and a tail `jmp 0x00488490` (Cursor::ctor) at 0x0045ba65, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_45ba60_orig;
void* __cdecl staticCtor_45ba60_re() { return CtorAt(0x00488490, 0x00822cc8); }
SG_HOOK("golf_clean.exe", 0x0045ba60, staticCtor_45ba60, staticCtor_45ba60_re, staticCtor_45ba60_orig);

// 0x0045bab0  staticCtor_45bab0(): constructs the global object at 0x00822c98 in place,
// with `mov ecx, 0x00822c98` at 0x0045bab0 and a tail `jmp 0x00473ab0` (Surface_ctor) at 0x0045bab5, so the constructor's return (this)
// is the function's return.
PFn0_t staticCtor_45bab0_orig;
void* __cdecl staticCtor_45bab0_re() { return CtorAt(0x00473ab0, 0x00822c98); }
SG_HOOK("golf_clean.exe", 0x0045bab0, staticCtor_45bab0, staticCtor_45bab0_re, staticCtor_45bab0_orig);

// 0x00482e80  staticInit_g83acb0(): constructs the global object at 0x0083acb0 in place,
// with `mov ecx, 0x0083acb0` at 0x00482e80 and a tail `jmp 0x00482fd0` (Cache482fd0::ctor) at 0x00482e85, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_g83acb0_orig;
void* __cdecl staticInit_g83acb0_re() { return CtorAt(0x00482fd0, 0x0083acb0); }
SG_HOOK("golf_clean.exe", 0x00482e80, staticInit_g83acb0, staticInit_g83acb0_re, staticInit_g83acb0_orig);

// 0x00482ec0  staticInit_g83ac30(): constructs the global object at 0x0083ac30 in place,
// with `mov ecx, 0x0083ac30` at 0x00482ec0 and a tail `jmp 0x00482fd0` (Cache482fd0::ctor) at 0x00482ec5, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_g83ac30_orig;
void* __cdecl staticInit_g83ac30_re() { return CtorAt(0x00482fd0, 0x0083ac30); }
SG_HOOK("golf_clean.exe", 0x00482ec0, staticInit_g83ac30, staticInit_g83ac30_re, staticInit_g83ac30_orig);

// 0x00483de0  staticInit_g83ad80(): constructs the global object at 0x0083ad80 in place,
// with `mov ecx, 0x0083ad80` at 0x00483de0 and a tail `jmp 0x00487280` (NodeList::ctor) at 0x00483de5, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_g83ad80_orig;
void* __cdecl staticInit_g83ad80_re() { return CtorAt(0x00487280, 0x0083ad80); }
SG_HOOK("golf_clean.exe", 0x00483de0, staticInit_g83ad80, staticInit_g83ad80_re, staticInit_g83ad80_orig);

// 0x00483e20  staticInit_g83ad58(): constructs the global object at 0x0083ad58 in place,
// with `mov ecx, 0x0083ad58` at 0x00483e20 and a tail `jmp 0x00487a20` (NodeListB::ctor) at 0x00483e25, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_g83ad58_orig;
void* __cdecl staticInit_g83ad58_re() { return CtorAt(0x00487a20, 0x0083ad58); }
SG_HOOK("golf_clean.exe", 0x00483e20, staticInit_g83ad58, staticInit_g83ad58_re, staticInit_g83ad58_orig);

// 0x00483e60  staticInit_g83af98(): constructs the global object at 0x0083af98 in place,
// with `mov ecx, 0x0083af98` at 0x00483e60 and a tail `jmp 0x00487b40` (NodeListC::ctor) at 0x00483e65, so the constructor's return (this)
// is the function's return.
PFn0_t staticInit_g83af98_orig;
void* __cdecl staticInit_g83af98_re() { return CtorAt(0x00487b40, 0x0083af98); }
SG_HOOK("golf_clean.exe", 0x00483e60, staticInit_g83af98, staticInit_g83af98_re, staticInit_g83af98_orig);

// 0x0043cc10  arrayStaticInit_43cc10(): constructs the global array of 0xbe objects of 0x58 bytes
// at 0x0081ca10 with the eh vector constructor iterator 0x004a55d2 (__stdcall: base, size, count, ctor,
// dtor pushed in reverse at 0x0043cc10..; call at 0x0043cc26): element constructor 0x00482fd0,
// unwind destructor 0x00483010.
Fn0v_t arrayStaticInit_43cc10_orig;
void __cdecl arrayStaticInit_43cc10_re() {
    kEhVectorCtor(reinterpret_cast<void*>(0x0081ca10), 0x58, 0xbe,
                  reinterpret_cast<void*>(0x00482fd0), reinterpret_cast<void*>(0x00483010));
}
SG_HOOK("golf_clean.exe", 0x0043cc10, arrayStaticInit_43cc10, arrayStaticInit_43cc10_re, arrayStaticInit_43cc10_orig);

// 0x0044ad20  arrayStaticInit_44ad20(): constructs the global array of 0x5 objects of 0x2c bytes
// at 0x00820f40 with the eh vector constructor iterator 0x004a55d2 (__stdcall: base, size, count, ctor,
// dtor pushed in reverse at 0x0044ad20..; call at 0x0044ad33): element constructor 0x00473ab0,
// unwind destructor 0x004041f0.
Fn0v_t arrayStaticInit_44ad20_orig;
void __cdecl arrayStaticInit_44ad20_re() {
    kEhVectorCtor(reinterpret_cast<void*>(0x00820f40), 0x2c, 0x5,
                  reinterpret_cast<void*>(0x00473ab0), reinterpret_cast<void*>(0x004041f0));
}
SG_HOOK("golf_clean.exe", 0x0044ad20, arrayStaticInit_44ad20, arrayStaticInit_44ad20_re, arrayStaticInit_44ad20_orig);

// 0x00487e10  initKeyTableArray(): constructs the global array of 0x4 objects of 0x120 bytes
// at 0x0083b170 with the eh vector constructor iterator 0x004a55d2 (__stdcall: base, size, count, ctor,
// dtor pushed in reverse at 0x00487e10..; call at 0x00487e26): element constructor 0x004882a0,
// unwind destructor 0x004882f0.
Fn0v_t initKeyTableArray_orig;
void __cdecl initKeyTableArray_re() {
    kEhVectorCtor(reinterpret_cast<void*>(0x0083b170), 0x120, 0x4,
                  reinterpret_cast<void*>(0x004882a0), reinterpret_cast<void*>(0x004882f0));
}
SG_HOOK("golf_clean.exe", 0x00487e10, initKeyTableArray, initKeyTableArray_re, initKeyTableArray_orig);

// ---------------------------------------------------------------- hand-written CRT helpers

// __getptd 0x004a8e53 (FidDb name): the calling thread's per-thread data block, allocated with calloc(1, 0x74)
// 0x004a8e73 and stored with TlsSetValue 0x004a8e87 the first time a thread asks.
typedef char*(__cdecl* Getptd_t)();
const Getptd_t kGetptd = reinterpret_cast<Getptd_t>(0x004a8e53);

// 0x004a910c  errnoPtr(): the address of the calling thread's errno: `call 0x004a8e53` (0x004a910c) and `add eax, 8`
// (0x004a9111), returned at 0x004a9114.
PFn0_t errnoPtr_orig;
void* __cdecl errnoPtr_re() { return kGetptd() + 8; }
SG_HOOK("golf_clean.exe", 0x004a910c, errnoPtr, errnoPtr_re, errnoPtr_orig);

// 0x004a9115  doserrnoPtr(): the same block + 0xc (`add eax, 0xc` at 0x004a911a), the thread's _doserrno.
PFn0_t doserrnoPtr_orig;
void* __cdecl doserrnoPtr_re() { return kGetptd() + 0xc; }
SG_HOOK("golf_clean.exe", 0x004a9115, doserrnoPtr, doserrnoPtr_re, doserrnoPtr_orig);

// 0x004a4fc4  initFpControl(): fills the six-slot table of floating-point conversion routines at 0x004e4b00, the
// table the printf/scanf float paths call through (0x004aa144, 0x004aa165, 0x004aa17c, 0x004b6a8e read it). Store
// order as in the original; eax holds 0x004a78d7 (__cfltcvt) from 0x004a4fc4 for slots 0 and 5.
inline void store32(unsigned at, unsigned v) { *reinterpret_cast<volatile unsigned*>(at) = v; }
Fn0v_t initFpControl_orig;
void __cdecl initFpControl_re() {
    store32(0x004e4b04, 0x004a7581);   // 0x004a4fc9
    store32(0x004e4b00, 0x004a78d7);   // 0x004a4fd3 (__cfltcvt)
    store32(0x004e4b08, 0x004a75e7);   // 0x004a4fd8 (__fassign)
    store32(0x004e4b0c, 0x004a7527);   // 0x004a4fe2 (__forcdecpt)
    store32(0x004e4b10, 0x004a75cf);   // 0x004a4fec
    store32(0x004e4b14, 0x004a78d7);   // 0x004a4ff6 (__cfltcvt)
}
SG_HOOK("golf_clean.exe", 0x004a4fc4, initFpControl, initFpControl_re, initFpControl_orig);

// The heap-selection global 0x00839650: Cache482f60::ctor 0x00482f60 saves it into its guard sub-object
// (+0x24, 0x00482f85, the guard's vtable 0x004ba278 at +0x20, 0x00482f78) and then clears it (0x00482f88).
volatile unsigned* const g_heap839650 = reinterpret_cast<volatile unsigned*>(0x00839650);

// 0x004021a0  unwindCleanup_4021a0(this): the guard's restore: reads the saved value at this+4 (0x004021a0), stores
// the vtable 0x004ba278 at this+0 (0x004021a3) and writes the saved value back to 0x00839650 (0x004021a9).
typedef void(__fastcall* Method0_t)(void* ecx, void* edx);
Method0_t unwindCleanup_4021a0_orig;
void __fastcall unwindCleanup_4021a0_re(void* thisp, void*) {
    unsigned* self = static_cast<unsigned*>(thisp);
    unsigned saved = self[1];          // 0x004021a0
    self[0] = 0x004ba278;              // 0x004021a3
    *g_heap839650 = saved;             // 0x004021a9
}
SG_HOOK("golf_clean.exe", 0x004021a0, unwindCleanup_4021a0, unwindCleanup_4021a0_re, unwindCleanup_4021a0_orig);

// 0x004021b0  unwindFree_4021b0(p): a null p returns at once (0x004021b6); otherwise p goes to the CRT free
// 0x004a5007 (0x004021c3) only while 0x00839650 is 0 (0x004021c0), and 0x00839650 is cleared either way
// (0x004021cb). Callers pass a second argument (0x004b7cf9) that the body never reads.
typedef void(__cdecl* Free_t)(void* p);
const Free_t kFree = reinterpret_cast<Free_t>(0x004a5007);
typedef void(__cdecl* FreeFn_t)(void* p);
FreeFn_t unwindFree_4021b0_orig;
void __cdecl unwindFree_4021b0_re(void* p) {
    if (p == 0) return;                // 0x004021b4 / 0x004021b6
    if (*g_heap839650 == 0)            // 0x004021b8 / 0x004021c0
        kFree(p);                      // 0x004021c3
    *g_heap839650 = 0;                 // 0x004021cb
}
SG_HOOK("golf_clean.exe", 0x004021b0, unwindFree_4021b0, unwindFree_4021b0_re, unwindFree_4021b0_orig);

// The global Reader at 0x0083b000 (vtable 0x004bb084, stored by Reader::ctor at 0x00487ec5).
const unsigned kReader = 0x0083b000;

// 0x00487e60  destroyGlobalReader(): `mov ecx, 0x0083b000` (0x00487e60) and a tail `jmp 0x00487f80`
// (Reader::closeFile, 0x00487e65): fclose 0x004a609f of the FILE at +0x158 when it is not null, then +0x158 = 0.
Fn0v_t destroyGlobalReader_orig;
void __cdecl destroyGlobalReader_re() { CtorAt(0x00487f80, kReader); }
SG_HOOK("golf_clean.exe", 0x00487e60, destroyGlobalReader, destroyGlobalReader_re, destroyGlobalReader_orig);

// 0x00487d80  initGlobalReader(): Reader::ctor 0x00487ea0 on 0x0083b000 with the buffer size 0x200 (`push 0x200`
// 0x00487d80, `mov ecx` 0x00487d85, `call` 0x00487d8a); the constructor's return (this, 0x00487ed0) is returned.
PFn0_t initGlobalReader_orig;
void* __cdecl initGlobalReader_re() { return Ctor1At(0x00487ea0, kReader, 0x200); }
SG_HOOK("golf_clean.exe", 0x00487d80, initGlobalReader, initGlobalReader_re, initGlobalReader_orig);

// 0x00482f00  staticInit_g83ac88(): Cache482f60::ctor 0x00482f60 on 0x0083ac88 with the flag 1 (`push 1` 0x00482f00,
// `mov ecx` 0x00482f02, `call` 0x00482f07); the constructor's eax (this, 0x00482f65) is returned.
PFn0_t staticInit_g83ac88_orig;
void* __cdecl staticInit_g83ac88_re() { return Ctor1At(0x00482f60, 0x0083ac88, 1); }
SG_HOOK("golf_clean.exe", 0x00482f00, staticInit_g83ac88, staticInit_g83ac88_re, staticInit_g83ac88_orig);

}  // namespace
