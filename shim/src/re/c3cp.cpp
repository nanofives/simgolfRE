// C3 batch c3cp (round 22, 2026-10-09) of golf_clean.exe: the C++ static-initialization families at the high
// addresses (0x004058e0 .. 0x0049eb50; agent c3co has the low half). Every body is hand-written from the disassembly
// (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and cites the instructions it reproduces.
//
// The families, from the disassembly:
//   dynInit_*          a slot of the CRT initializer table 0x004c1000 that __cinit 0x004a50db walks: `call X` then
//                      `jmp Y`. X constructs a file-scope object at a fixed address (a constructor reached with ecx
//                      loaded, or the eh vector constructor iterator 0x004a55d2 over an array); Y is a 12-byte
//                      `push <exit routine>; call _atexit 0x004a56d2; pop ecx; ret`, so the dynInit returns
//                      _atexit's result.
//   atexitRegister_*   three of those Y's (the others are not in this batch).
//   atexitDtor_*       exit routines registered by a Y: `mov ecx, <object>; jmp <destructor>`.
//   atexitDtor_83aca8  the one exit routine with a body: a once-only guard bit at 0x0083ac28.
//   storeUiEventTable  slot 70 of the table: one store.
//
// Callees run through their original addresses (X, Y, _atexit, the destructors). __thiscall destructors are called
// through __fastcall (ecx = this, edx unused). These hooks are installed in the shim's DllMain, before the exe's
// __cinit runs, so every dynInit / storeUiEventTable reimplementation here also runs once in each boot's real static
// initialization.
#include "hooks.h"

namespace {

typedef int(__cdecl* Init_t)();                   // X and Y: cdecl, no arguments; Y returns _atexit's result
typedef int(__cdecl* Atexit_t)(void(__cdecl*)()); // _atexit 0x004a56d2
typedef void(__fastcall* Dtor_t)(void*, void*);   // a __thiscall destructor (ecx = this)
typedef void(__cdecl* Exit_t)();                  // an exit routine as _atexit stores it

inline Init_t Init(unsigned va) { return reinterpret_cast<Init_t>(va); }
inline Dtor_t Dtor(unsigned va) { return reinterpret_cast<Dtor_t>(va); }
inline void* Obj(unsigned va) { return reinterpret_cast<void*>(va); }
inline Atexit_t Atexit() { return reinterpret_cast<Atexit_t>(0x004a56d2); }

// 0x004058e0  dynInit_staticInit_004058f0: slot 31 (+0x7c) of the initializer table 0x004c1000. The call at
// 0x004058e0 runs staticInit_004058f0 0x004058f0 (constructs the object at 0x005a7148); the jmp at 0x004058e5 tail-jumps to
// atexitRegister_00405900 0x00405900, which pushes 0x00405910 (0x00405900) and calls _atexit (0x00405905).
Init_t dynInit_staticInit_004058f0_orig;
int __cdecl dynInit_staticInit_004058f0_re() {
    Init(0x004058f0)();
    return Init(0x00405900)();
}

// 0x00405900  atexitRegister_00405900: `push 0x00405910` (0x00405900), `call _atexit 0x004a56d2` (0x00405905), `pop ecx` (0x0040590a),
// `ret` (0x0040590b) with _atexit's eax. The routine pushed: 0x00405910 loads ecx = 0x005a7148 and tail-jumps to Image_dtor 0x00474c40.
Init_t atexitRegister_00405900_orig;
int __cdecl atexitRegister_00405900_re() {
    return Atexit()(reinterpret_cast<Exit_t>(0x00405910));
}

// 0x004061a0  dynInit_staticInit_004061b0: slot 32 (+0x80) of the initializer table 0x004c1000. The call at
// 0x004061a0 runs staticInit_004061b0 0x004061b0 (constructs the object at 0x00568d10); the jmp at 0x004061a5 tail-jumps to
// atexitRegister_004061c0 0x004061c0, which pushes 0x004061d0 (0x004061c0) and calls _atexit (0x004061c5).
Init_t dynInit_staticInit_004061b0_orig;
int __cdecl dynInit_staticInit_004061b0_re() {
    Init(0x004061b0)();
    return Init(0x004061c0)();
}

// 0x004061c0  atexitRegister_004061c0: `push 0x004061d0` (0x004061c0), `call _atexit 0x004a56d2` (0x004061c5), `pop ecx` (0x004061ca),
// `ret` (0x004061cb) with _atexit's eax. The routine pushed: 0x004061d0 loads ecx = 0x00568d10 and tail-jumps to Image_dtor 0x00474c40.
Init_t atexitRegister_004061c0_orig;
int __cdecl atexitRegister_004061c0_re() {
    return Atexit()(reinterpret_cast<Exit_t>(0x004061d0));
}

// 0x004061e0  dynInit_staticInit_004061f0: slot 33 (+0x84) of the initializer table 0x004c1000. The call at
// 0x004061e0 runs staticInit_004061f0 0x004061f0 (constructs the object at 0x00587ae8); the jmp at 0x004061e5 tail-jumps to
// atexitRegister_00406200 0x00406200, which pushes 0x00406210 (0x00406200) and calls _atexit (0x00406205).
Init_t dynInit_staticInit_004061f0_orig;
int __cdecl dynInit_staticInit_004061f0_re() {
    Init(0x004061f0)();
    return Init(0x00406200)();
}

// 0x00406200  atexitRegister_00406200: `push 0x00406210` (0x00406200), `call _atexit 0x004a56d2` (0x00406205), `pop ecx` (0x0040620a),
// `ret` (0x0040620b) with _atexit's eax. The routine pushed: 0x00406210 loads ecx = 0x00587ae8 and tail-jumps to Image_dtor 0x00474c40.
Init_t atexitRegister_00406200_orig;
int __cdecl atexitRegister_00406200_re() {
    return Atexit()(reinterpret_cast<Exit_t>(0x00406210));
}

// 0x004378d0  dynInit_staticCtor_4378e0: slot 34 (+0x88) of the initializer table 0x004c1000. The call at
// 0x004378d0 runs staticCtor_4378e0 0x004378e0 (constructs the object at 0x005aa560); the jmp at 0x004378d5 tail-jumps to
// registerExit_4378f0 0x004378f0, which pushes 0x00437900 (0x004378f0) and calls _atexit (0x004378f5).
Init_t dynInit_staticCtor_4378e0_orig;
int __cdecl dynInit_staticCtor_4378e0_re() {
    Init(0x004378e0)();
    return Init(0x004378f0)();
}

// 0x00438590  dynInit_staticCtor_4385a0: slot 35 (+0x8c) of the initializer table 0x004c1000. The call at
// 0x00438590 runs staticCtor_4385a0 0x004385a0 (constructs the object at 0x005a9f78); the jmp at 0x00438595 tail-jumps to
// registerExit_4385b0 0x004385b0, which pushes 0x004385c0 (0x004385b0) and calls _atexit (0x004385b5).
Init_t dynInit_staticCtor_4385a0_orig;
int __cdecl dynInit_staticCtor_4385a0_re() {
    Init(0x004385a0)();
    return Init(0x004385b0)();
}

// 0x0043cc00  dynInit_arrayStaticInit_43cc10: slot 36 (+0x90) of the initializer table 0x004c1000. The call at
// 0x0043cc00 runs arrayStaticInit_43cc10 0x0043cc10 (constructs the object at 0x0081ca10); the jmp at 0x0043cc05 tail-jumps to
// registerExit_43cc30 0x0043cc30, which pushes 0x0043cc40 (0x0043cc30) and calls _atexit (0x0043cc35).
Init_t dynInit_arrayStaticInit_43cc10_orig;
int __cdecl dynInit_arrayStaticInit_43cc10_re() {
    Init(0x0043cc10)();
    return Init(0x0043cc30)();
}

// 0x0043cc60  dynInit_staticCtor_43cc70: slot 37 (+0x94) of the initializer table 0x004c1000. The call at
// 0x0043cc60 runs staticCtor_43cc70 0x0043cc70 (constructs the object at 0x005aa6f0); the jmp at 0x0043cc65 tail-jumps to
// registerExit_43cc80 0x0043cc80, which pushes 0x0043cc90 (0x0043cc80) and calls _atexit (0x0043cc85).
Init_t dynInit_staticCtor_43cc70_orig;
int __cdecl dynInit_staticCtor_43cc70_re() {
    Init(0x0043cc70)();
    return Init(0x0043cc80)();
}

// 0x0043cca0  dynInit_staticCtor_43ccb0: slot 38 (+0x98) of the initializer table 0x004c1000. The call at
// 0x0043cca0 runs staticCtor_43ccb0 0x0043ccb0 (constructs the object at 0x005aa6d0); the jmp at 0x0043cca5 tail-jumps to
// registerExit_43ccc0 0x0043ccc0, which pushes 0x0043ccd0 (0x0043ccc0) and calls _atexit (0x0043ccc5).
Init_t dynInit_staticCtor_43ccb0_orig;
int __cdecl dynInit_staticCtor_43ccb0_re() {
    Init(0x0043ccb0)();
    return Init(0x0043ccc0)();
}

// 0x0043d4d0  dynInit_staticCtor_43d4e0: slot 40 (+0xa0) of the initializer table 0x004c1000. The call at
// 0x0043d4d0 runs staticCtor_43d4e0 0x0043d4e0 (constructs the object at 0x00820e90); the jmp at 0x0043d4d5 tail-jumps to
// registerExit_43d4f0 0x0043d4f0, which pushes 0x0043d500 (0x0043d4f0) and calls _atexit (0x0043d4f5).
Init_t dynInit_staticCtor_43d4e0_orig;
int __cdecl dynInit_staticCtor_43d4e0_re() {
    Init(0x0043d4e0)();
    return Init(0x0043d4f0)();
}

// 0x0043d630  dynInit_staticCtor_43d640: slot 41 (+0xa4) of the initializer table 0x004c1000. The call at
// 0x0043d630 runs staticCtor_43d640 0x0043d640 (constructs the object at 0x005aa778); the jmp at 0x0043d635 tail-jumps to
// registerExit_43d650 0x0043d650, which pushes 0x0043d660 (0x0043d650) and calls _atexit (0x0043d655).
Init_t dynInit_staticCtor_43d640_orig;
int __cdecl dynInit_staticCtor_43d640_re() {
    Init(0x0043d640)();
    return Init(0x0043d650)();
}

// 0x00448150  dynInit_arrayStaticInit_448160: slot 42 (+0xa8) of the initializer table 0x004c1000. The call at
// 0x00448150 runs arrayStaticInit_448160 0x00448160 (constructs the object at 0x0080d840); the jmp at 0x00448155 tail-jumps to
// registerExit_448180 0x00448180, which pushes 0x00448190 (0x00448180) and calls _atexit (0x00448185).
Init_t dynInit_arrayStaticInit_448160_orig;
int __cdecl dynInit_arrayStaticInit_448160_re() {
    Init(0x00448160)();
    return Init(0x00448180)();
}

// 0x004491b0  dynInit_staticCtor_4491c0: slot 45 (+0xb4) of the initializer table 0x004c1000. The call at
// 0x004491b0 runs staticCtor_4491c0 0x004491c0 (constructs the object at 0x00820ed8); the jmp at 0x004491b5 tail-jumps to
// registerExit_4491d0 0x004491d0, which pushes 0x004491e0 (0x004491d0) and calls _atexit (0x004491d5).
Init_t dynInit_staticCtor_4491c0_orig;
int __cdecl dynInit_staticCtor_4491c0_re() {
    Init(0x004491c0)();
    return Init(0x004491d0)();
}

// 0x00449200  dynInit_allocTileBuffer: slot 46 (+0xb8) of the initializer table 0x004c1000. The call at
// 0x00449200 runs allocTileBuffer 0x00449210 (constructs the object at 0x00820f1c); the jmp at 0x00449205 tail-jumps to
// registerExit_449240 0x00449240, which pushes 0x00449250 (0x00449240) and calls _atexit (0x00449245).
Init_t dynInit_allocTileBuffer_orig;
int __cdecl dynInit_allocTileBuffer_re() {
    Init(0x00449210)();
    return Init(0x00449240)();
}

// 0x0044ac50  dynInit_initPanelState: slot 48 (+0xc0) of the initializer table 0x004c1000. The call at
// 0x0044ac50 runs initPanelState 0x0044ac60 (constructs the object at 0x00821020); the jmp at 0x0044ac55 tail-jumps to
// registerExit_44aca0 0x0044aca0, which pushes 0x0044acb0 (0x0044aca0) and calls _atexit (0x0044aca5).
Init_t dynInit_initPanelState_orig;
int __cdecl dynInit_initPanelState_re() {
    Init(0x0044ac60)();
    return Init(0x0044aca0)();
}

// 0x0044ad10  dynInit_arrayStaticInit_44ad20: slot 49 (+0xc4) of the initializer table 0x004c1000. The call at
// 0x0044ad10 runs arrayStaticInit_44ad20 0x0044ad20 (constructs the object at 0x00820f40); the jmp at 0x0044ad15 tail-jumps to
// registerExit_44ad40 0x0044ad40, which pushes 0x0044ad50 (0x0044ad40) and calls _atexit (0x0044ad45).
Init_t dynInit_arrayStaticInit_44ad20_orig;
int __cdecl dynInit_arrayStaticInit_44ad20_re() {
    Init(0x0044ad20)();
    return Init(0x0044ad40)();
}

// 0x0044ad70  dynInit_staticCtor_44ad80: slot 50 (+0xc8) of the initializer table 0x004c1000. The call at
// 0x0044ad70 runs staticCtor_44ad80 0x0044ad80 (constructs the object at 0x00821040); the jmp at 0x0044ad75 tail-jumps to
// registerExit_44ad90 0x0044ad90, which pushes 0x0044ada0 (0x0044ad90) and calls _atexit (0x0044ad95).
Init_t dynInit_staticCtor_44ad80_orig;
int __cdecl dynInit_staticCtor_44ad80_re() {
    Init(0x0044ad80)();
    return Init(0x0044ad90)();
}

// 0x0044adc0  dynInit_staticCtor_44add0: slot 51 (+0xcc) of the initializer table 0x004c1000. The call at
// 0x0044adc0 runs staticCtor_44add0 0x0044add0 (constructs the object at 0x008210c8); the jmp at 0x0044adc5 tail-jumps to
// registerExit_44ade0 0x0044ade0, which pushes 0x0044adf0 (0x0044ade0) and calls _atexit (0x0044ade5).
Init_t dynInit_staticCtor_44add0_orig;
int __cdecl dynInit_staticCtor_44add0_re() {
    Init(0x0044add0)();
    return Init(0x0044ade0)();
}

// 0x0044adf0  atexitDtor_8210c8: `mov ecx, 0x008210c8` (0x0044adf0) and a tail jump to gSurfaceDtor_44ae00 0x0044ae00 (0x0044adf5).
// Registered by registerExit_44ade0 0x0044ade0 (`push 0x0044adf0`).
Exit_t atexitDtor_8210c8_orig;
void __cdecl atexitDtor_8210c8_re() {
    Dtor(0x0044ae00)(Obj(0x008210c8), 0);
}

// 0x0044aeb0  dynInit_staticCtor_44aec0: slot 52 (+0xd0) of the initializer table 0x004c1000. The call at
// 0x0044aeb0 runs staticCtor_44aec0 0x0044aec0 (constructs the object at 0x00821c60); the jmp at 0x0044aeb5 tail-jumps to
// registerExit_44aed0 0x0044aed0, which pushes 0x0044aee0 (0x0044aed0) and calls _atexit (0x0044aed5).
Init_t dynInit_staticCtor_44aec0_orig;
int __cdecl dynInit_staticCtor_44aec0_re() {
    Init(0x0044aec0)();
    return Init(0x0044aed0)();
}

// 0x0044aee0  atexitDtor_821c60: `mov ecx, 0x00821c60` (0x0044aee0) and a tail jump to gSurfaceDtor_44ae00 0x0044ae00 (0x0044aee5).
// Registered by registerExit_44aed0 0x0044aed0 (`push 0x0044aee0`).
Exit_t atexitDtor_821c60_orig;
void __cdecl atexitDtor_821c60_re() {
    Dtor(0x0044ae00)(Obj(0x00821c60), 0);
}

// 0x0044aef0  dynInit_staticCtor_44af00: slot 53 (+0xd4) of the initializer table 0x004c1000. The call at
// 0x0044aef0 runs staticCtor_44af00 0x0044af00 (constructs the object at 0x00821bf8); the jmp at 0x0044aef5 tail-jumps to
// registerExit_44af10 0x0044af10, which pushes 0x0044af20 (0x0044af10) and calls _atexit (0x0044af15).
Init_t dynInit_staticCtor_44af00_orig;
int __cdecl dynInit_staticCtor_44af00_re() {
    Init(0x0044af00)();
    return Init(0x0044af10)();
}

// 0x0044af20  atexitDtor_821bf8: `mov ecx, 0x00821bf8` (0x0044af20) and a tail jump to gSurfaceDtor_44ae00 0x0044ae00 (0x0044af25).
// Registered by registerExit_44af10 0x0044af10 (`push 0x0044af20`).
Exit_t atexitDtor_821bf8_orig;
void __cdecl atexitDtor_821bf8_re() {
    Dtor(0x0044ae00)(Obj(0x00821bf8), 0);
}

// 0x0044af30  dynInit_staticCtor_44af40: slot 54 (+0xd8) of the initializer table 0x004c1000. The call at
// 0x0044af30 runs staticCtor_44af40 0x0044af40 (constructs the object at 0x00821070); the jmp at 0x0044af35 tail-jumps to
// registerExit_44af50 0x0044af50, which pushes 0x0044af60 (0x0044af50) and calls _atexit (0x0044af55).
Init_t dynInit_staticCtor_44af40_orig;
int __cdecl dynInit_staticCtor_44af40_re() {
    Init(0x0044af40)();
    return Init(0x0044af50)();
}

// 0x0044af60  atexitDtor_821070: `mov ecx, 0x00821070` (0x0044af60) and a tail jump to gSurfaceDtor_44ae00 0x0044ae00 (0x0044af65).
// Registered by registerExit_44af50 0x0044af50 (`push 0x0044af60`).
Exit_t atexitDtor_821070_orig;
void __cdecl atexitDtor_821070_re() {
    Dtor(0x0044ae00)(Obj(0x00821070), 0);
}

// 0x0044af70  dynInit_staticCtor_44af80: slot 55 (+0xdc) of the initializer table 0x004c1000. The call at
// 0x0044af70 runs staticCtor_44af80 0x0044af80 (constructs the object at 0x00821200); the jmp at 0x0044af75 tail-jumps to
// registerExit_44af90 0x0044af90, which pushes 0x0044afa0 (0x0044af90) and calls _atexit (0x0044af95).
Init_t dynInit_staticCtor_44af80_orig;
int __cdecl dynInit_staticCtor_44af80_re() {
    Init(0x0044af80)();
    return Init(0x0044af90)();
}

// 0x0044afa0  atexitDtor_821200: `mov ecx, 0x00821200` (0x0044afa0) and a tail jump to gSurfaceDtor_44afb0 0x0044afb0 (0x0044afa5).
// Registered by registerExit_44af90 0x0044af90 (`push 0x0044afa0`).
Exit_t atexitDtor_821200_orig;
void __cdecl atexitDtor_821200_re() {
    Dtor(0x0044afb0)(Obj(0x00821200), 0);
}

// 0x0044b0f0  dynInit_staticCtor_44b100: slot 56 (+0xe0) of the initializer table 0x004c1000. The call at
// 0x0044b0f0 runs staticCtor_44b100 0x0044b100 (constructs the object at 0x008223c0); the jmp at 0x0044b0f5 tail-jumps to
// registerExit_44b110 0x0044b110, which pushes 0x0044b120 (0x0044b110) and calls _atexit (0x0044b115).
Init_t dynInit_staticCtor_44b100_orig;
int __cdecl dynInit_staticCtor_44b100_re() {
    Init(0x0044b100)();
    return Init(0x0044b110)();
}

// 0x0044b120  atexitDtor_8223c0: `mov ecx, 0x008223c0` (0x0044b120) and a tail jump to gSurfaceDtor_44afb0 0x0044afb0 (0x0044b125).
// Registered by registerExit_44b110 0x0044b110 (`push 0x0044b120`).
Exit_t atexitDtor_8223c0_orig;
void __cdecl atexitDtor_8223c0_re() {
    Dtor(0x0044afb0)(Obj(0x008223c0), 0);
}

// 0x0044b130  dynInit_staticCtor_44b140: slot 57 (+0xe4) of the initializer table 0x004c1000. The call at
// 0x0044b130 runs staticCtor_44b140 0x0044b140 (constructs the object at 0x008222b8); the jmp at 0x0044b135 tail-jumps to
// registerExit_44b150 0x0044b150, which pushes 0x0044b160 (0x0044b150) and calls _atexit (0x0044b155).
Init_t dynInit_staticCtor_44b140_orig;
int __cdecl dynInit_staticCtor_44b140_re() {
    Init(0x0044b140)();
    return Init(0x0044b150)();
}

// 0x0044b160  atexitDtor_8222b8: `mov ecx, 0x008222b8` (0x0044b160) and a tail jump to gSurfaceDtor_44afb0 0x0044afb0 (0x0044b165).
// Registered by registerExit_44b150 0x0044b150 (`push 0x0044b160`).
Exit_t atexitDtor_8222b8_orig;
void __cdecl atexitDtor_8222b8_re() {
    Dtor(0x0044afb0)(Obj(0x008222b8), 0);
}

// 0x0044b170  dynInit_staticCtor_44b180: slot 58 (+0xe8) of the initializer table 0x004c1000. The call at
// 0x0044b170 runs staticCtor_44b180 0x0044b180 (constructs the object at 0x00821308); the jmp at 0x0044b175 tail-jumps to
// registerExit_44b190 0x0044b190, which pushes 0x0044b1a0 (0x0044b190) and calls _atexit (0x0044b195).
Init_t dynInit_staticCtor_44b180_orig;
int __cdecl dynInit_staticCtor_44b180_re() {
    Init(0x0044b180)();
    return Init(0x0044b190)();
}

// 0x0044b1a0  atexitDtor_821308: `mov ecx, 0x00821308` (0x0044b1a0) and a tail jump to gSurfaceDtor_44b1b0 0x0044b1b0 (0x0044b1a5).
// Registered by registerExit_44b190 0x0044b190 (`push 0x0044b1a0`).
Exit_t atexitDtor_821308_orig;
void __cdecl atexitDtor_821308_re() {
    Dtor(0x0044b1b0)(Obj(0x00821308), 0);
}

// 0x0044b3a0  dynInit_staticCtor_44b3b0: slot 59 (+0xec) of the initializer table 0x004c1000. The call at
// 0x0044b3a0 runs staticCtor_44b3b0 0x0044b3b0 (constructs the object at 0x00821f48); the jmp at 0x0044b3a5 tail-jumps to
// registerExit_44b3c0 0x0044b3c0, which pushes 0x0044b3d0 (0x0044b3c0) and calls _atexit (0x0044b3c5).
Init_t dynInit_staticCtor_44b3b0_orig;
int __cdecl dynInit_staticCtor_44b3b0_re() {
    Init(0x0044b3b0)();
    return Init(0x0044b3c0)();
}

// 0x0044b3d0  atexitDtor_821f48: `mov ecx, 0x00821f48` (0x0044b3d0) and a tail jump to gSurfaceDtor_44b3e0 0x0044b3e0 (0x0044b3d5).
// Registered by registerExit_44b3c0 0x0044b3c0 (`push 0x0044b3d0`).
Exit_t atexitDtor_821f48_orig;
void __cdecl atexitDtor_821f48_re() {
    Dtor(0x0044b3e0)(Obj(0x00821f48), 0);
}

// 0x0044b4b0  dynInit_staticCtor_44b4c0: slot 60 (+0xf0) of the initializer table 0x004c1000. The call at
// 0x0044b4b0 runs staticCtor_44b4c0 0x0044b4c0 (constructs the object at 0x008224c8); the jmp at 0x0044b4b5 tail-jumps to
// registerExit_44b4d0 0x0044b4d0, which pushes 0x0044b4e0 (0x0044b4d0) and calls _atexit (0x0044b4d5).
Init_t dynInit_staticCtor_44b4c0_orig;
int __cdecl dynInit_staticCtor_44b4c0_re() {
    Init(0x0044b4c0)();
    return Init(0x0044b4d0)();
}

// 0x0044b4e0  atexitDtor_8224c8: `mov ecx, 0x008224c8` (0x0044b4e0) and a tail jump to gSurfaceDtor_44b4f0 0x0044b4f0 (0x0044b4e5).
// Registered by registerExit_44b4d0 0x0044b4d0 (`push 0x0044b4e0`).
Exit_t atexitDtor_8224c8_orig;
void __cdecl atexitDtor_8224c8_re() {
    Dtor(0x0044b4f0)(Obj(0x008224c8), 0);
}

// 0x0044b650  dynInit_staticCtor_44b660: slot 61 (+0xf4) of the initializer table 0x004c1000. The call at
// 0x0044b650 runs staticCtor_44b660 0x0044b660 (constructs the object at 0x00821120); the jmp at 0x0044b655 tail-jumps to
// registerExit_44b670 0x0044b670, which pushes 0x0044b680 (0x0044b670) and calls _atexit (0x0044b675).
Init_t dynInit_staticCtor_44b660_orig;
int __cdecl dynInit_staticCtor_44b660_re() {
    Init(0x0044b660)();
    return Init(0x0044b670)();
}

// 0x0044b680  atexitDtor_821120: `mov ecx, 0x00821120` (0x0044b680) and a tail jump to gSurfaceDtor_44b690 0x0044b690 (0x0044b685).
// Registered by registerExit_44b670 0x0044b670 (`push 0x0044b680`).
Exit_t atexitDtor_821120_orig;
void __cdecl atexitDtor_821120_re() {
    Dtor(0x0044b690)(Obj(0x00821120), 0);
}

// 0x0044b750  dynInit_staticCtor_44b760: slot 62 (+0xf8) of the initializer table 0x004c1000. The call at
// 0x0044b750 runs staticCtor_44b760 0x0044b760 (constructs the object at 0x00821cb8); the jmp at 0x0044b755 tail-jumps to
// registerExit_44b770 0x0044b770, which pushes 0x0044b780 (0x0044b770) and calls _atexit (0x0044b775).
Init_t dynInit_staticCtor_44b760_orig;
int __cdecl dynInit_staticCtor_44b760_re() {
    Init(0x0044b760)();
    return Init(0x0044b770)();
}

// 0x0044b780  atexitDtor_821cb8: `mov ecx, 0x00821cb8` (0x0044b780) and a tail jump to gSurfaceDtor_44b790 0x0044b790 (0x0044b785).
// Registered by registerExit_44b770 0x0044b770 (`push 0x0044b780`).
Exit_t atexitDtor_821cb8_orig;
void __cdecl atexitDtor_821cb8_re() {
    Dtor(0x0044b790)(Obj(0x00821cb8), 0);
}

// 0x0045ba50  dynInit_staticCtor_45ba60: slot 63 (+0xfc) of the initializer table 0x004c1000. The call at
// 0x0045ba50 runs staticCtor_45ba60 0x0045ba60 (constructs the object at 0x00822cc8); the jmp at 0x0045ba55 tail-jumps to
// registerExit_45ba70 0x0045ba70, which pushes 0x0045ba80 (0x0045ba70) and calls _atexit (0x0045ba75).
Init_t dynInit_staticCtor_45ba60_orig;
int __cdecl dynInit_staticCtor_45ba60_re() {
    Init(0x0045ba60)();
    return Init(0x0045ba70)();
}

// 0x0045baa0  dynInit_staticCtor_45bab0: slot 64 (+0x100) of the initializer table 0x004c1000. The call at
// 0x0045baa0 runs staticCtor_45bab0 0x0045bab0 (constructs the object at 0x00822c98); the jmp at 0x0045baa5 tail-jumps to
// registerExit_45bac0 0x0045bac0, which pushes 0x0045bad0 (0x0045bac0) and calls _atexit (0x0045bac5).
Init_t dynInit_staticCtor_45bab0_orig;
int __cdecl dynInit_staticCtor_45bab0_re() {
    Init(0x0045bab0)();
    return Init(0x0045bac0)();
}

// 0x0046de30  dynInit_gImageCtor_838f98: slot 67 (+0x10c) of the initializer table 0x004c1000. The call at
// 0x0046de30 runs gImageCtor_838f98 0x0046de40 (constructs the object at 0x00838f98); the jmp at 0x0046de35 tail-jumps to
// atexitThunk_46de50 0x0046de50, which pushes 0x0046de60 (0x0046de50) and calls _atexit (0x0046de55).
Init_t dynInit_gImageCtor_838f98_orig;
int __cdecl dynInit_gImageCtor_838f98_re() {
    Init(0x0046de40)();
    return Init(0x0046de50)();
}

// 0x00473a60  dynInit_initGlobalImage: slot 68 (+0x110) of the initializer table 0x004c1000. The call at
// 0x00473a60 runs initGlobalImage 0x00473a70 (constructs the object at 0x00839348); the jmp at 0x00473a65 tail-jumps to
// atexitThunk_473a80 0x00473a80, which pushes 0x00473a90 (0x00473a80) and calls _atexit (0x00473a85).
Init_t dynInit_initGlobalImage_orig;
int __cdecl dynInit_initGlobalImage_re() {
    Init(0x00473a70)();
    return Init(0x00473a80)();
}

// 0x00474a80  dynInit_gSurfaceCtor_839a68: slot 69 (+0x114) of the initializer table 0x004c1000. The call at
// 0x00474a80 runs gSurfaceCtor_839a68 0x00474a90 (constructs the object at 0x00839a68); the jmp at 0x00474a85 tail-jumps to
// atexitThunk_474aa0 0x00474aa0, which pushes 0x00474ab0 (0x00474aa0) and calls _atexit (0x00474aa5).
Init_t dynInit_gSurfaceCtor_839a68_orig;
int __cdecl dynInit_gSurfaceCtor_839a68_re() {
    Init(0x00474a90)();
    return Init(0x00474aa0)();
}

// 0x004799d0  storeUiEventTable: slot 70 (+0x118) of the initializer table 0x004c1000. `mov dword ptr [0x0083a4f8],
// 0x004ba87c` (0x004799d0) and `ret` (0x004799da): no read, no call.
Exit_t storeUiEventTable_orig;
void __cdecl storeUiEventTable_re() {
    *reinterpret_cast<unsigned*>(0x0083a4f8) = 0x004ba87c;
}

// 0x004799e0  dynInit_gImageCtor_83a500: slot 71 (+0x11c) of the initializer table 0x004c1000. The call at
// 0x004799e0 runs gImageCtor_83a500 0x004799f0 (constructs the object at 0x0083a500); the jmp at 0x004799e5 tail-jumps to
// atexitThunk_479a00 0x00479a00, which pushes 0x00479a10 (0x00479a00) and calls _atexit (0x00479a05).
Init_t dynInit_gImageCtor_83a500_orig;
int __cdecl dynInit_gImageCtor_83a500_re() {
    Init(0x004799f0)();
    return Init(0x00479a00)();
}

// 0x00479a30  dynInit_gImageCtor_83a7b8: slot 72 (+0x120) of the initializer table 0x004c1000. The call at
// 0x00479a30 runs gImageCtor_83a7b8 0x00479a40 (constructs the object at 0x0083a7b8); the jmp at 0x00479a35 tail-jumps to
// atexitThunk_479a50 0x00479a50, which pushes 0x00479a60 (0x00479a50) and calls _atexit (0x00479a55).
Init_t dynInit_gImageCtor_83a7b8_orig;
int __cdecl dynInit_gImageCtor_83a7b8_re() {
    Init(0x00479a40)();
    return Init(0x00479a50)();
}

// 0x00482e70  dynInit_staticInit_g83acb0: slot 73 (+0x124) of the initializer table 0x004c1000. The call at
// 0x00482e70 runs staticInit_g83acb0 0x00482e80 (constructs the object at 0x0083acb0); the jmp at 0x00482e75 tail-jumps to
// atexit_482ea0 0x00482e90, which pushes 0x00482ea0 (0x00482e90) and calls _atexit (0x00482e95).
Init_t dynInit_staticInit_g83acb0_orig;
int __cdecl dynInit_staticInit_g83acb0_re() {
    Init(0x00482e80)();
    return Init(0x00482e90)();
}

// 0x00482eb0  dynInit_staticInit_g83ac30: slot 74 (+0x128) of the initializer table 0x004c1000. The call at
// 0x00482eb0 runs staticInit_g83ac30 0x00482ec0 (constructs the object at 0x0083ac30); the jmp at 0x00482eb5 tail-jumps to
// atexit_482ee0 0x00482ed0, which pushes 0x00482ee0 (0x00482ed0) and calls _atexit (0x00482ed5).
Init_t dynInit_staticInit_g83ac30_orig;
int __cdecl dynInit_staticInit_g83ac30_re() {
    Init(0x00482ec0)();
    return Init(0x00482ed0)();
}

// 0x00482ef0  dynInit_staticInit_g83ac88: slot 75 (+0x12c) of the initializer table 0x004c1000. The call at
// 0x00482ef0 runs staticInit_g83ac88 0x00482f00 (constructs the object at 0x0083ac88); the jmp at 0x00482ef5 tail-jumps to
// atexit_482f20 0x00482f10, which pushes 0x00482f20 (0x00482f10) and calls _atexit (0x00482f15).
Init_t dynInit_staticInit_g83ac88_orig;
int __cdecl dynInit_staticInit_g83ac88_re() {
    Init(0x00482f00)();
    return Init(0x00482f10)();
}

// 0x00482f20  atexitDtor_83aca8: the exit routine registered by atexit_482f20 0x00482f10 (`push 0x00482f20`) for the
// object constructed by staticInit_g83ac88 0x00482f00 (Cache482f60::ctor 0x00482f60 on 0x0083ac88 with 1). Bit 0 of
// the byte 0x0083ac28 is a run-once guard: read at 0x00482f20, tested at 0x00482f28 (`test al, cl` with al = 1 from
// 0x00482f26); when it is set the routine returns at once (`jne 0x00482f52`, 0x00482f2a). Otherwise the bit is set
// (`or cl, al` 0x00482f2c, stored 0x00482f2e), Cache482f60::dtor 0x004835f0 runs with ecx = 0x0083aca8 (0x00482f34 /
// 0x00482f39), the dword 0x0083acac is read (0x00482f3e), 0x004ba278 is stored at 0x0083aca8 (0x00482f43) and the
// dword read is stored at 0x00839650 (0x00482f4d).
Exit_t atexitDtor_83aca8_orig;
void __cdecl atexitDtor_83aca8_re() {
    unsigned char* guard = reinterpret_cast<unsigned char*>(0x0083ac28);
    if (*guard & 1) return;
    *guard = static_cast<unsigned char>(*guard | 1);
    Dtor(0x004835f0)(Obj(0x0083aca8), 0);
    unsigned saved = *reinterpret_cast<unsigned*>(0x0083acac);
    *reinterpret_cast<unsigned*>(0x0083aca8) = 0x004ba278;
    *reinterpret_cast<unsigned*>(0x00839650) = saved;
}

// 0x004837a0  dynInit_screenMgrCtor4837b0: slot 76 (+0x130) of the initializer table 0x004c1000. The call at
// 0x004837a0 runs screenMgrCtor4837b0 0x004837b0 (constructs the object at 0x0083ad20); the jmp at 0x004837a5 tail-jumps to
// atexit_4837d0 0x004837c0, which pushes 0x004837d0 (0x004837c0) and calls _atexit (0x004837c5).
Init_t dynInit_screenMgrCtor4837b0_orig;
int __cdecl dynInit_screenMgrCtor4837b0_re() {
    Init(0x004837b0)();
    return Init(0x004837c0)();
}

// 0x00483dd0  dynInit_staticInit_g83ad80: slot 77 (+0x134) of the initializer table 0x004c1000. The call at
// 0x00483dd0 runs staticInit_g83ad80 0x00483de0 (constructs the object at 0x0083ad80); the jmp at 0x00483dd5 tail-jumps to
// atexit_483e00 0x00483df0, which pushes 0x00483e00 (0x00483df0) and calls _atexit (0x00483df5).
Init_t dynInit_staticInit_g83ad80_orig;
int __cdecl dynInit_staticInit_g83ad80_re() {
    Init(0x00483de0)();
    return Init(0x00483df0)();
}

// 0x00483e10  dynInit_staticInit_g83ad58: slot 78 (+0x138) of the initializer table 0x004c1000. The call at
// 0x00483e10 runs staticInit_g83ad58 0x00483e20 (constructs the object at 0x0083ad58); the jmp at 0x00483e15 tail-jumps to
// atexit_483e40 0x00483e30, which pushes 0x00483e40 (0x00483e30) and calls _atexit (0x00483e35).
Init_t dynInit_staticInit_g83ad58_orig;
int __cdecl dynInit_staticInit_g83ad58_re() {
    Init(0x00483e20)();
    return Init(0x00483e30)();
}

// 0x00483e50  dynInit_staticInit_g83af98: slot 79 (+0x13c) of the initializer table 0x004c1000. The call at
// 0x00483e50 runs staticInit_g83af98 0x00483e60 (constructs the object at 0x0083af98); the jmp at 0x00483e55 tail-jumps to
// atexit_483e80 0x00483e70, which pushes 0x00483e80 (0x00483e70) and calls _atexit (0x00483e75).
Init_t dynInit_staticInit_g83af98_orig;
int __cdecl dynInit_staticInit_g83af98_re() {
    Init(0x00483e60)();
    return Init(0x00483e70)();
}

// 0x00487d70  dynInit_initGlobalReader: slot 80 (+0x140) of the initializer table 0x004c1000. The call at
// 0x00487d70 runs initGlobalReader 0x00487d80 (constructs the object at 0x0083b000); the jmp at 0x00487d75 tail-jumps to
// registerReaderAtexit 0x00487d90, which pushes 0x00487da0 (0x00487d90) and calls _atexit (0x00487d95).
Init_t dynInit_initGlobalReader_orig;
int __cdecl dynInit_initGlobalReader_re() {
    Init(0x00487d80)();
    return Init(0x00487d90)();
}

// 0x00487e00  dynInit_initKeyTableArray: slot 83 (+0x14c) of the initializer table 0x004c1000. The call at
// 0x00487e00 runs initKeyTableArray 0x00487e10 (constructs the object at 0x0083b170); the jmp at 0x00487e05 tail-jumps to
// registerKeyTableAtexit 0x00487e30, which pushes 0x00487e40 (0x00487e30) and calls _atexit (0x00487e35).
Init_t dynInit_initKeyTableArray_orig;
int __cdecl dynInit_initKeyTableArray_re() {
    Init(0x00487e10)();
    return Init(0x00487e30)();
}

// 0x0049bfe0  dynInit_initGlobalObj_49c020: slot 86 (+0x158) of the initializer table 0x004c1000. The call at
// 0x0049bfe0 runs initGlobalObj_49c020 0x0049bff0 (constructs the object at 0x008400c8); the jmp at 0x0049bfe5 tail-jumps to
// regAtexit_49c000 0x0049c000, which pushes 0x0049c010 (0x0049c000) and calls _atexit (0x0049c005).
Init_t dynInit_initGlobalObj_49c020_orig;
int __cdecl dynInit_initGlobalObj_49c020_re() {
    Init(0x0049bff0)();
    return Init(0x0049c000)();
}

// 0x0049d000  dynInit_initPairArrayGlobal: slot 87 (+0x15c) of the initializer table 0x004c1000. The call at
// 0x0049d000 runs initPairArrayGlobal 0x0049d010 (constructs the object at 0x008406e8); the jmp at 0x0049d005 tail-jumps to
// regAtexit_49d020 0x0049d020, which pushes 0x0049d030 (0x0049d020) and calls _atexit (0x0049d025).
Init_t dynInit_initPairArrayGlobal_orig;
int __cdecl dynInit_initPairArrayGlobal_re() {
    Init(0x0049d010)();
    return Init(0x0049d020)();
}

// 0x0049d150  dynInit_initGfxGlobal_840710: slot 88 (+0x160) of the initializer table 0x004c1000. The call at
// 0x0049d150 runs initGfxGlobal_840710 0x0049d160 (constructs the object at 0x00840710); the jmp at 0x0049d155 tail-jumps to
// regAtexit_49d170 0x0049d170, which pushes 0x0049d180 (0x0049d170) and calls _atexit (0x0049d175).
Init_t dynInit_initGfxGlobal_840710_orig;
int __cdecl dynInit_initGfxGlobal_840710_re() {
    Init(0x0049d160)();
    return Init(0x0049d170)();
}

// 0x0049d290  dynInit_initGfxGlobal_8407c0: slot 89 (+0x164) of the initializer table 0x004c1000. The call at
// 0x0049d290 runs initGfxGlobal_8407c0 0x0049d2a0 (constructs the object at 0x008407c0); the jmp at 0x0049d295 tail-jumps to
// regAtexit_49d2b0 0x0049d2b0, which pushes 0x0049d2c0 (0x0049d2b0) and calls _atexit (0x0049d2b5).
Init_t dynInit_initGfxGlobal_8407c0_orig;
int __cdecl dynInit_initGfxGlobal_8407c0_re() {
    Init(0x0049d2a0)();
    return Init(0x0049d2b0)();
}

// 0x0049d2f0  dynInit_initGfxGlobal_840790: slot 90 (+0x168) of the initializer table 0x004c1000. The call at
// 0x0049d2f0 runs initGfxGlobal_840790 0x0049d300 (constructs the object at 0x00840790); the jmp at 0x0049d2f5 tail-jumps to
// regAtexit_49d310 0x0049d310, which pushes 0x0049d320 (0x0049d310) and calls _atexit (0x0049d315).
Init_t dynInit_initGfxGlobal_840790_orig;
int __cdecl dynInit_initGfxGlobal_840790_re() {
    Init(0x0049d300)();
    return Init(0x0049d310)();
}

// 0x0049d350  dynInit_initGfxGlobal_8407f0: slot 91 (+0x16c) of the initializer table 0x004c1000. The call at
// 0x0049d350 runs initGfxGlobal_8407f0 0x0049d360 (constructs the object at 0x008407f0); the jmp at 0x0049d355 tail-jumps to
// regAtexit_49d370 0x0049d370, which pushes 0x0049d380 (0x0049d370) and calls _atexit (0x0049d375).
Init_t dynInit_initGfxGlobal_8407f0_orig;
int __cdecl dynInit_initGfxGlobal_8407f0_re() {
    Init(0x0049d360)();
    return Init(0x0049d370)();
}

// 0x0049d480  dynInit_initGfxGlobal_840830: slot 92 (+0x170) of the initializer table 0x004c1000. The call at
// 0x0049d480 runs initGfxGlobal_840830 0x0049d490 (constructs the object at 0x00840830); the jmp at 0x0049d485 tail-jumps to
// regAtexit_49d4a0 0x0049d4a0, which pushes 0x0049d4b0 (0x0049d4a0) and calls _atexit (0x0049d4a5).
Init_t dynInit_initGfxGlobal_840830_orig;
int __cdecl dynInit_initGfxGlobal_840830_re() {
    Init(0x0049d490)();
    return Init(0x0049d4a0)();
}

// 0x0049d4e0  dynInit_initGfxGlobal_840890: slot 93 (+0x174) of the initializer table 0x004c1000. The call at
// 0x0049d4e0 runs initGfxGlobal_840890 0x0049d4f0 (constructs the object at 0x00840890); the jmp at 0x0049d4e5 tail-jumps to
// regAtexit_49d500 0x0049d500, which pushes 0x0049d510 (0x0049d500) and calls _atexit (0x0049d505).
Init_t dynInit_initGfxGlobal_840890_orig;
int __cdecl dynInit_initGfxGlobal_840890_re() {
    Init(0x0049d4f0)();
    return Init(0x0049d500)();
}

// 0x0049d540  dynInit_initGfxGlobal_840860: slot 94 (+0x178) of the initializer table 0x004c1000. The call at
// 0x0049d540 runs initGfxGlobal_840860 0x0049d550 (constructs the object at 0x00840860); the jmp at 0x0049d545 tail-jumps to
// regAtexit_49d560 0x0049d560, which pushes 0x0049d570 (0x0049d560) and calls _atexit (0x0049d565).
Init_t dynInit_initGfxGlobal_840860_orig;
int __cdecl dynInit_initGfxGlobal_840860_re() {
    Init(0x0049d550)();
    return Init(0x0049d560)();
}

// 0x0049eaf0  dynInit_initGfxGlobal_8408c8: slot 95 (+0x17c) of the initializer table 0x004c1000. The call at
// 0x0049eaf0 runs initGfxGlobal_8408c8 0x0049eb00 (constructs the object at 0x008408c8); the jmp at 0x0049eaf5 tail-jumps to
// regAtexit_49eb10 0x0049eb10, which pushes 0x0049eb20 (0x0049eb10) and calls _atexit (0x0049eb15).
Init_t dynInit_initGfxGlobal_8408c8_orig;
int __cdecl dynInit_initGfxGlobal_8408c8_re() {
    Init(0x0049eb00)();
    return Init(0x0049eb10)();
}

// 0x0049eb50  dynInit_initGfxGlobal_8408f8: slot 96 (+0x180) of the initializer table 0x004c1000. The call at
// 0x0049eb50 runs initGfxGlobal_8408f8 0x0049eb60 (constructs the object at 0x008408f8); the jmp at 0x0049eb55 tail-jumps to
// regAtexit_49eb70 0x0049eb70, which pushes 0x0049eb80 (0x0049eb70) and calls _atexit (0x0049eb75).
Init_t dynInit_initGfxGlobal_8408f8_orig;
int __cdecl dynInit_initGfxGlobal_8408f8_re() {
    Init(0x0049eb60)();
    return Init(0x0049eb70)();
}

}  // namespace

// 0x004058e0  dynInit_staticInit_004058f0
SG_HOOK("golf_clean.exe", 0x004058e0, dynInit_staticInit_004058f0, dynInit_staticInit_004058f0_re, dynInit_staticInit_004058f0_orig);
// 0x00405900  atexitRegister_00405900
SG_HOOK("golf_clean.exe", 0x00405900, atexitRegister_00405900, atexitRegister_00405900_re, atexitRegister_00405900_orig);
// 0x004061a0  dynInit_staticInit_004061b0
SG_HOOK("golf_clean.exe", 0x004061a0, dynInit_staticInit_004061b0, dynInit_staticInit_004061b0_re, dynInit_staticInit_004061b0_orig);
// 0x004061c0  atexitRegister_004061c0
SG_HOOK("golf_clean.exe", 0x004061c0, atexitRegister_004061c0, atexitRegister_004061c0_re, atexitRegister_004061c0_orig);
// 0x004061e0  dynInit_staticInit_004061f0
SG_HOOK("golf_clean.exe", 0x004061e0, dynInit_staticInit_004061f0, dynInit_staticInit_004061f0_re, dynInit_staticInit_004061f0_orig);
// 0x00406200  atexitRegister_00406200
SG_HOOK("golf_clean.exe", 0x00406200, atexitRegister_00406200, atexitRegister_00406200_re, atexitRegister_00406200_orig);
// 0x004378d0  dynInit_staticCtor_4378e0
SG_HOOK("golf_clean.exe", 0x004378d0, dynInit_staticCtor_4378e0, dynInit_staticCtor_4378e0_re, dynInit_staticCtor_4378e0_orig);
// 0x00438590  dynInit_staticCtor_4385a0
SG_HOOK("golf_clean.exe", 0x00438590, dynInit_staticCtor_4385a0, dynInit_staticCtor_4385a0_re, dynInit_staticCtor_4385a0_orig);
// 0x0043cc00  dynInit_arrayStaticInit_43cc10
SG_HOOK("golf_clean.exe", 0x0043cc00, dynInit_arrayStaticInit_43cc10, dynInit_arrayStaticInit_43cc10_re, dynInit_arrayStaticInit_43cc10_orig);
// 0x0043cc60  dynInit_staticCtor_43cc70
SG_HOOK("golf_clean.exe", 0x0043cc60, dynInit_staticCtor_43cc70, dynInit_staticCtor_43cc70_re, dynInit_staticCtor_43cc70_orig);
// 0x0043cca0  dynInit_staticCtor_43ccb0
SG_HOOK("golf_clean.exe", 0x0043cca0, dynInit_staticCtor_43ccb0, dynInit_staticCtor_43ccb0_re, dynInit_staticCtor_43ccb0_orig);
// 0x0043d4d0  dynInit_staticCtor_43d4e0
SG_HOOK("golf_clean.exe", 0x0043d4d0, dynInit_staticCtor_43d4e0, dynInit_staticCtor_43d4e0_re, dynInit_staticCtor_43d4e0_orig);
// 0x0043d630  dynInit_staticCtor_43d640
SG_HOOK("golf_clean.exe", 0x0043d630, dynInit_staticCtor_43d640, dynInit_staticCtor_43d640_re, dynInit_staticCtor_43d640_orig);
// 0x00448150  dynInit_arrayStaticInit_448160
SG_HOOK("golf_clean.exe", 0x00448150, dynInit_arrayStaticInit_448160, dynInit_arrayStaticInit_448160_re, dynInit_arrayStaticInit_448160_orig);
// 0x004491b0  dynInit_staticCtor_4491c0
SG_HOOK("golf_clean.exe", 0x004491b0, dynInit_staticCtor_4491c0, dynInit_staticCtor_4491c0_re, dynInit_staticCtor_4491c0_orig);
// 0x00449200  dynInit_allocTileBuffer
SG_HOOK("golf_clean.exe", 0x00449200, dynInit_allocTileBuffer, dynInit_allocTileBuffer_re, dynInit_allocTileBuffer_orig);
// 0x0044ac50  dynInit_initPanelState
SG_HOOK("golf_clean.exe", 0x0044ac50, dynInit_initPanelState, dynInit_initPanelState_re, dynInit_initPanelState_orig);
// 0x0044ad10  dynInit_arrayStaticInit_44ad20
SG_HOOK("golf_clean.exe", 0x0044ad10, dynInit_arrayStaticInit_44ad20, dynInit_arrayStaticInit_44ad20_re, dynInit_arrayStaticInit_44ad20_orig);
// 0x0044ad70  dynInit_staticCtor_44ad80
SG_HOOK("golf_clean.exe", 0x0044ad70, dynInit_staticCtor_44ad80, dynInit_staticCtor_44ad80_re, dynInit_staticCtor_44ad80_orig);
// 0x0044adc0  dynInit_staticCtor_44add0
SG_HOOK("golf_clean.exe", 0x0044adc0, dynInit_staticCtor_44add0, dynInit_staticCtor_44add0_re, dynInit_staticCtor_44add0_orig);
// 0x0044adf0  atexitDtor_8210c8
SG_HOOK("golf_clean.exe", 0x0044adf0, atexitDtor_8210c8, atexitDtor_8210c8_re, atexitDtor_8210c8_orig);
// 0x0044aeb0  dynInit_staticCtor_44aec0
SG_HOOK("golf_clean.exe", 0x0044aeb0, dynInit_staticCtor_44aec0, dynInit_staticCtor_44aec0_re, dynInit_staticCtor_44aec0_orig);
// 0x0044aee0  atexitDtor_821c60
SG_HOOK("golf_clean.exe", 0x0044aee0, atexitDtor_821c60, atexitDtor_821c60_re, atexitDtor_821c60_orig);
// 0x0044aef0  dynInit_staticCtor_44af00
SG_HOOK("golf_clean.exe", 0x0044aef0, dynInit_staticCtor_44af00, dynInit_staticCtor_44af00_re, dynInit_staticCtor_44af00_orig);
// 0x0044af20  atexitDtor_821bf8
SG_HOOK("golf_clean.exe", 0x0044af20, atexitDtor_821bf8, atexitDtor_821bf8_re, atexitDtor_821bf8_orig);
// 0x0044af30  dynInit_staticCtor_44af40
SG_HOOK("golf_clean.exe", 0x0044af30, dynInit_staticCtor_44af40, dynInit_staticCtor_44af40_re, dynInit_staticCtor_44af40_orig);
// 0x0044af60  atexitDtor_821070
SG_HOOK("golf_clean.exe", 0x0044af60, atexitDtor_821070, atexitDtor_821070_re, atexitDtor_821070_orig);
// 0x0044af70  dynInit_staticCtor_44af80
SG_HOOK("golf_clean.exe", 0x0044af70, dynInit_staticCtor_44af80, dynInit_staticCtor_44af80_re, dynInit_staticCtor_44af80_orig);
// 0x0044afa0  atexitDtor_821200
SG_HOOK("golf_clean.exe", 0x0044afa0, atexitDtor_821200, atexitDtor_821200_re, atexitDtor_821200_orig);
// 0x0044b0f0  dynInit_staticCtor_44b100
SG_HOOK("golf_clean.exe", 0x0044b0f0, dynInit_staticCtor_44b100, dynInit_staticCtor_44b100_re, dynInit_staticCtor_44b100_orig);
// 0x0044b120  atexitDtor_8223c0
SG_HOOK("golf_clean.exe", 0x0044b120, atexitDtor_8223c0, atexitDtor_8223c0_re, atexitDtor_8223c0_orig);
// 0x0044b130  dynInit_staticCtor_44b140
SG_HOOK("golf_clean.exe", 0x0044b130, dynInit_staticCtor_44b140, dynInit_staticCtor_44b140_re, dynInit_staticCtor_44b140_orig);
// 0x0044b160  atexitDtor_8222b8
SG_HOOK("golf_clean.exe", 0x0044b160, atexitDtor_8222b8, atexitDtor_8222b8_re, atexitDtor_8222b8_orig);
// 0x0044b170  dynInit_staticCtor_44b180
SG_HOOK("golf_clean.exe", 0x0044b170, dynInit_staticCtor_44b180, dynInit_staticCtor_44b180_re, dynInit_staticCtor_44b180_orig);
// 0x0044b1a0  atexitDtor_821308
SG_HOOK("golf_clean.exe", 0x0044b1a0, atexitDtor_821308, atexitDtor_821308_re, atexitDtor_821308_orig);
// 0x0044b3a0  dynInit_staticCtor_44b3b0
SG_HOOK("golf_clean.exe", 0x0044b3a0, dynInit_staticCtor_44b3b0, dynInit_staticCtor_44b3b0_re, dynInit_staticCtor_44b3b0_orig);
// 0x0044b3d0  atexitDtor_821f48
SG_HOOK("golf_clean.exe", 0x0044b3d0, atexitDtor_821f48, atexitDtor_821f48_re, atexitDtor_821f48_orig);
// 0x0044b4b0  dynInit_staticCtor_44b4c0
SG_HOOK("golf_clean.exe", 0x0044b4b0, dynInit_staticCtor_44b4c0, dynInit_staticCtor_44b4c0_re, dynInit_staticCtor_44b4c0_orig);
// 0x0044b4e0  atexitDtor_8224c8
SG_HOOK("golf_clean.exe", 0x0044b4e0, atexitDtor_8224c8, atexitDtor_8224c8_re, atexitDtor_8224c8_orig);
// 0x0044b650  dynInit_staticCtor_44b660
SG_HOOK("golf_clean.exe", 0x0044b650, dynInit_staticCtor_44b660, dynInit_staticCtor_44b660_re, dynInit_staticCtor_44b660_orig);
// 0x0044b680  atexitDtor_821120
SG_HOOK("golf_clean.exe", 0x0044b680, atexitDtor_821120, atexitDtor_821120_re, atexitDtor_821120_orig);
// 0x0044b750  dynInit_staticCtor_44b760
SG_HOOK("golf_clean.exe", 0x0044b750, dynInit_staticCtor_44b760, dynInit_staticCtor_44b760_re, dynInit_staticCtor_44b760_orig);
// 0x0044b780  atexitDtor_821cb8
SG_HOOK("golf_clean.exe", 0x0044b780, atexitDtor_821cb8, atexitDtor_821cb8_re, atexitDtor_821cb8_orig);
// 0x0045ba50  dynInit_staticCtor_45ba60
SG_HOOK("golf_clean.exe", 0x0045ba50, dynInit_staticCtor_45ba60, dynInit_staticCtor_45ba60_re, dynInit_staticCtor_45ba60_orig);
// 0x0045baa0  dynInit_staticCtor_45bab0
SG_HOOK("golf_clean.exe", 0x0045baa0, dynInit_staticCtor_45bab0, dynInit_staticCtor_45bab0_re, dynInit_staticCtor_45bab0_orig);
// 0x0046de30  dynInit_gImageCtor_838f98
SG_HOOK("golf_clean.exe", 0x0046de30, dynInit_gImageCtor_838f98, dynInit_gImageCtor_838f98_re, dynInit_gImageCtor_838f98_orig);
// 0x00473a60  dynInit_initGlobalImage
SG_HOOK("golf_clean.exe", 0x00473a60, dynInit_initGlobalImage, dynInit_initGlobalImage_re, dynInit_initGlobalImage_orig);
// 0x00474a80  dynInit_gSurfaceCtor_839a68
SG_HOOK("golf_clean.exe", 0x00474a80, dynInit_gSurfaceCtor_839a68, dynInit_gSurfaceCtor_839a68_re, dynInit_gSurfaceCtor_839a68_orig);
// 0x004799d0  storeUiEventTable
SG_HOOK("golf_clean.exe", 0x004799d0, storeUiEventTable, storeUiEventTable_re, storeUiEventTable_orig);
// 0x004799e0  dynInit_gImageCtor_83a500
SG_HOOK("golf_clean.exe", 0x004799e0, dynInit_gImageCtor_83a500, dynInit_gImageCtor_83a500_re, dynInit_gImageCtor_83a500_orig);
// 0x00479a30  dynInit_gImageCtor_83a7b8
SG_HOOK("golf_clean.exe", 0x00479a30, dynInit_gImageCtor_83a7b8, dynInit_gImageCtor_83a7b8_re, dynInit_gImageCtor_83a7b8_orig);
// 0x00482e70  dynInit_staticInit_g83acb0
SG_HOOK("golf_clean.exe", 0x00482e70, dynInit_staticInit_g83acb0, dynInit_staticInit_g83acb0_re, dynInit_staticInit_g83acb0_orig);
// 0x00482eb0  dynInit_staticInit_g83ac30
SG_HOOK("golf_clean.exe", 0x00482eb0, dynInit_staticInit_g83ac30, dynInit_staticInit_g83ac30_re, dynInit_staticInit_g83ac30_orig);
// 0x00482ef0  dynInit_staticInit_g83ac88
SG_HOOK("golf_clean.exe", 0x00482ef0, dynInit_staticInit_g83ac88, dynInit_staticInit_g83ac88_re, dynInit_staticInit_g83ac88_orig);
// 0x00482f20  atexitDtor_83aca8
SG_HOOK("golf_clean.exe", 0x00482f20, atexitDtor_83aca8, atexitDtor_83aca8_re, atexitDtor_83aca8_orig);
// 0x004837a0  dynInit_screenMgrCtor4837b0
SG_HOOK("golf_clean.exe", 0x004837a0, dynInit_screenMgrCtor4837b0, dynInit_screenMgrCtor4837b0_re, dynInit_screenMgrCtor4837b0_orig);
// 0x00483dd0  dynInit_staticInit_g83ad80
SG_HOOK("golf_clean.exe", 0x00483dd0, dynInit_staticInit_g83ad80, dynInit_staticInit_g83ad80_re, dynInit_staticInit_g83ad80_orig);
// 0x00483e10  dynInit_staticInit_g83ad58
SG_HOOK("golf_clean.exe", 0x00483e10, dynInit_staticInit_g83ad58, dynInit_staticInit_g83ad58_re, dynInit_staticInit_g83ad58_orig);
// 0x00483e50  dynInit_staticInit_g83af98
SG_HOOK("golf_clean.exe", 0x00483e50, dynInit_staticInit_g83af98, dynInit_staticInit_g83af98_re, dynInit_staticInit_g83af98_orig);
// 0x00487d70  dynInit_initGlobalReader
SG_HOOK("golf_clean.exe", 0x00487d70, dynInit_initGlobalReader, dynInit_initGlobalReader_re, dynInit_initGlobalReader_orig);
// 0x00487e00  dynInit_initKeyTableArray
SG_HOOK("golf_clean.exe", 0x00487e00, dynInit_initKeyTableArray, dynInit_initKeyTableArray_re, dynInit_initKeyTableArray_orig);
// 0x0049bfe0  dynInit_initGlobalObj_49c020
SG_HOOK("golf_clean.exe", 0x0049bfe0, dynInit_initGlobalObj_49c020, dynInit_initGlobalObj_49c020_re, dynInit_initGlobalObj_49c020_orig);
// 0x0049d000  dynInit_initPairArrayGlobal
SG_HOOK("golf_clean.exe", 0x0049d000, dynInit_initPairArrayGlobal, dynInit_initPairArrayGlobal_re, dynInit_initPairArrayGlobal_orig);
// 0x0049d150  dynInit_initGfxGlobal_840710
SG_HOOK("golf_clean.exe", 0x0049d150, dynInit_initGfxGlobal_840710, dynInit_initGfxGlobal_840710_re, dynInit_initGfxGlobal_840710_orig);
// 0x0049d290  dynInit_initGfxGlobal_8407c0
SG_HOOK("golf_clean.exe", 0x0049d290, dynInit_initGfxGlobal_8407c0, dynInit_initGfxGlobal_8407c0_re, dynInit_initGfxGlobal_8407c0_orig);
// 0x0049d2f0  dynInit_initGfxGlobal_840790
SG_HOOK("golf_clean.exe", 0x0049d2f0, dynInit_initGfxGlobal_840790, dynInit_initGfxGlobal_840790_re, dynInit_initGfxGlobal_840790_orig);
// 0x0049d350  dynInit_initGfxGlobal_8407f0
SG_HOOK("golf_clean.exe", 0x0049d350, dynInit_initGfxGlobal_8407f0, dynInit_initGfxGlobal_8407f0_re, dynInit_initGfxGlobal_8407f0_orig);
// 0x0049d480  dynInit_initGfxGlobal_840830
SG_HOOK("golf_clean.exe", 0x0049d480, dynInit_initGfxGlobal_840830, dynInit_initGfxGlobal_840830_re, dynInit_initGfxGlobal_840830_orig);
// 0x0049d4e0  dynInit_initGfxGlobal_840890
SG_HOOK("golf_clean.exe", 0x0049d4e0, dynInit_initGfxGlobal_840890, dynInit_initGfxGlobal_840890_re, dynInit_initGfxGlobal_840890_orig);
// 0x0049d540  dynInit_initGfxGlobal_840860
SG_HOOK("golf_clean.exe", 0x0049d540, dynInit_initGfxGlobal_840860, dynInit_initGfxGlobal_840860_re, dynInit_initGfxGlobal_840860_orig);
// 0x0049eaf0  dynInit_initGfxGlobal_8408c8
SG_HOOK("golf_clean.exe", 0x0049eaf0, dynInit_initGfxGlobal_8408c8, dynInit_initGfxGlobal_8408c8_re, dynInit_initGfxGlobal_8408c8_orig);
// 0x0049eb50  dynInit_initGfxGlobal_8408f8
SG_HOOK("golf_clean.exe", 0x0049eb50, dynInit_initGfxGlobal_8408f8, dynInit_initGfxGlobal_8408f8_re, dynInit_initGfxGlobal_8408f8_orig);
