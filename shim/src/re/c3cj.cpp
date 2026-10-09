// Batch c3cj: the jgld.dll Display / DisplayBase vtable methods.
//
// Candidate list log/c3/c3cj_candidates.tsv (45 rows, every one of them a slot of the Display vtable at
// VA 0x1011d640 or of the DisplayBase vtable at VA 0x1011d7dc, installed by Display::Display 0x100654b0 /
// FUN_10065670 and by FUN_10065630 / DisplayBase::dtor 0x10066d20). Most of the list is refused by the
// round's rules - a slot whose whole body is one Win32 import (ShowWindow, SetWindowPos, GetWindowRect,
// InvalidateRect, GetCursorPos, EnumDisplaySettingsA, ChangeDisplaySettingsA, MessageBoxA, BitBlt), a
// `new`/`delete` factory, or a DisplayBase default whose result cannot vary. The reasons for each are in
// log/c3/c3cj_notes.txt. What this batch takes:
//
//   Display::findMode        0x00065c30   slot 20 (+0x50)
//   Display::setMode         0x00065da0   slot 24 (+0x60)
//   Display::pickMode        0x00065ea0   slot 22 (+0x58)
//   Display::pickModeFreq    0x00066040   slot 23 (+0x5c)
//   Display::getField138     0x00065bf0   slot 15 (+0x3c)
//   Display::getField12c130  0x000666b0   slot 13 (+0x34)
//   Display::getHwnd         0x00067560   slot 8  (+0x20)
//   Display::getInstance     0x000675a0   slot 9  (+0x24)
//   Display::getWidth        0x00067760   slot 42 (+0xa8)
//   Display::getHeight       0x000677a0   slot 43 (+0xac)
//   Display::getBpp          0x000677e0   slot 44 (+0xb0)
//   Display::setScale        0x00067d50   slot 39 (+0x9c)
//   Display::getScale        0x00067da0   slot 40 (+0xa0)
//   Factory::destroyFont     0x00067b50   slot 34 (+0x88)
//   Factory::destroyPalette  0x00067bd0   slot 35 (+0x8c)
//   Factory::destroySurface  0x00067c50   slot 36 (+0x90)
//   Factory::destroySprite   0x00067cd0   slot 37 (+0x94)
//
// SG_HOOK addresses are RVAs; every instruction cited in a comment is a VA (0x10000000 + RVA), the form
// the debug build's disassembly prints. jgld.dll is LoadLibrary'd after the shim starts, so these hooks
// install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds them by
// (module, rva) through SimGolfShim_FindHookM. __thiscall is emulated with __fastcall: ecx = this, edx
// unused; a __thiscall callee is called the same way (ecx, an unused dword, then its stack arguments).
//
// Nothing live is handed to these bodies. The A/B never touches the module's Display pointer 0x10128420
// nor the Lib pointer 0x1012872c: every Display, Mode, Array<ModeInfo>, mode table and destroyable object
// is allocated by the fixture (re/frida/js/fixtures.d/c3cj.js). The four mode methods dispatch through
// slots +0x58 / +0x60 / +0x64 / +0x68 of the object's OWN vtable, and the real bodies behind those slots
// call EnumDisplaySettingsA (0x10065a90), ChangeDisplaySettingsA and MessageBoxA (0x100662a0) and write the
// module arrays g_modes 0x101286c8 / g_devmodes 0x101286f8, so the fixture's objects carry a synthetic
// vtable of recording stubs instead: what each slot receives, in which order, IS the evidence.
//
// The only module globals in this batch are the three scale words 0x10122dc0 / 0x10122dc4 / 0x10122dc8 that
// Display::setScale writes and Display::getScale reads. About forty live sprite blitters read them (c3bm,
// c3bp, c3bu, c3by), so both keys run with the game's first thread suspended and the game's values written
// back before it is resumed (c3cd's and c3av's pattern).
#include "hooks.h"

namespace {

// jgld.dll is relocatable, so every address the code holds is resolved against the running base.
unsigned char* JgldBase() {
    static unsigned char* base = 0;
    if (!base) base = reinterpret_cast<unsigned char*>(GetModuleHandleA("jgld.dll"));
    return base;
}
void* JgldVA(unsigned va) { return JgldBase() + (va - 0x10000000u); }

template <class T> T* F(void* p, unsigned off) {
    return reinterpret_cast<T*>(static_cast<char*>(p) + off);
}
inline void* P32R(const void* p, unsigned off) {
    return *reinterpret_cast<void* const*>(static_cast<const char*>(p) + off);
}
// A virtual dispatch is [[obj] + slot]: the vtable pointer sits at [obj] and the slot inside the table.
template <class T> T Vfn(void* obj, unsigned slot) {
    void* vtbl = *reinterpret_cast<void**>(obj);
    return reinterpret_cast<T>(*reinterpret_cast<void**>(static_cast<char*>(vtbl) + slot));
}

// ---------------------------------------------------------------- the mode list the four mode methods walk
//
// Display::enumModes 0x10065a90 returns Array<ModeInfo>* (the module array g_modes 0x101286c8). The three
// searchers read the element buffer from [array+4] and step it by 0x14 (`imul edx,edx,0x14` at 0x10065cb5,
// 0x10065f08, 0x100660dd), and read the element count through Array::count 0x10066a80, which returns the
// int at [array+0xc] (0x10066aa0). The four fields they compare are at +0x00, +0x04, +0x0c and +0x10; the
// dword at +0x08 is never read by any of them.
struct ModeInfo { int w; int h; int w2; int bpp; int freq; };

// Array<ModeInfo>::count 0x10066a80, called through the incremental-link thunk 0x100013ac (0x10065ca8,
// 0x10065ef7, 0x100660cc). Called at its body address, as the thunk's `jmp 0x10066a80` resolves to.
typedef int(__fastcall* ArrayCount_t)(void*, void*);
int ArrayCount(void* arr) {
    return reinterpret_cast<ArrayCount_t>(JgldVA(0x10066a80))(arr, 0);
}
const ModeInfo* ModeAt(void* arr, int i) {
    return reinterpret_cast<const ModeInfo*>(P32R(arr, 4)) + i;
}

// The four vtable slots the mode methods dispatch to, in the shape the pushes give them. The bodies the
// REAL Display vtable 0x1011d640 holds in those slots were read out of the anchored binary with
// log/c3/alt/c3cj_vt.py (each slot holds an incremental-link thunk; the script follows the `jmp`):
//   +0x58 slot 22 = 0x10065ea0 Display::pickMode        +0x64 slot 25 = 0x100662a0 Display::setModeIndex
//   +0x60 slot 24 = 0x10065da0 Display::setMode         +0x68 slot 26 = 0x10065a90 Display::enumModes
// so the four methods of this batch call one another through the table: setMode's zero arm calls pickMode,
// and pickModeFreq's non-zero arm calls setMode. Those real bodies reach EnumDisplaySettingsA (0x10065b11
// in enumModes, 0x100662da in setModeIndex), ChangeDisplaySettingsA (0x10066382) and MessageBoxA
// (0x1006631c), and refill the module arrays g_modes 0x101286c8 and g_devmodes 0x101286f8 (the adds at
// 0x10065b66 and 0x10065b85), which is why the A/B's objects carry a vtable of recording stubs instead.
typedef void*(__fastcall* VEnumModes_t)(void*, void*);                             // +0x68
typedef int(__fastcall* VSetModeIndex_t)(void*, void*, int);                       // +0x64
typedef void(__fastcall* VPickMode_t)(void*, void*, int, int, int);                // +0x58
typedef int(__fastcall* VSetMode_t)(void*, void*, int, int, int, int);             // +0x60
typedef void(__fastcall* VModeSet_t)(void*, void*, int, int, int, int);            // Mode slot +0x00

// -------------------------------------------------------------------- Display::findMode 0x00065c30
//
// __thiscall(int w, int h, int bpp), `ret 0xc` at 0x10065d4d. It dispatches slot +0x68 with no argument
// (0x10065c86) to get the mode list, scans it for the entry whose +0x00, +0x04 and +0x0c equal w, h and bpp
// and whose +0x10 is the largest seen, and hands that index to slot +0x64 (0x10065d31). The two running
// values start at -1 (0x10065c6e, 0x10065c75), so an empty or non-matching list passes -1 on. The frequency
// test is SIGNED (`jle 0x10065d03`). The return is the `xor eax,eax` at 0x10065d3b: always 0, whatever the
// scan found - the result of the scan leaves this function only through the slot +0x64 call.
// Three locals the body fills are never read again: [ebp-8] = 0 (0x10065c4d) and the rectangle
// {0, w, 0, h} at [ebp-0x10]..[ebp-0x14] (0x10065c5b-0x10065c6b).
typedef int(__fastcall* FindMode_t)(void*, void*, int, int, int);
FindMode_t findMode_orig;

int __fastcall findMode_re(void* self, void*, int w, int h, int bpp) {
    void* modes = Vfn<VEnumModes_t>(self, 0x68)(self, 0);     // 0x10065c86
    int best = -1;                                            // 0x10065c6e
    int bestFreq = -1;                                        // 0x10065c75
    for (int i = 0; i < ArrayCount(modes); i++) {             // 0x10065ca8 / jge 0x10065cb0
        const ModeInfo* m = ModeAt(modes, i);
        if (m->w != w) continue;                              // jne 0x10065cc4
        if (m->h != h) continue;                              // jne 0x10065cd9
        if (m->bpp != bpp) continue;                          // jne 0x10065cee
        if (m->freq <= bestFreq) continue;                    // jle 0x10065d03
        bestFreq = m->freq;                                   // 0x10065d15
        best = i;                                             // 0x10065d1b
    }
    Vfn<VSetModeIndex_t>(self, 0x64)(self, 0, best);          // 0x10065d31
    return 0;                                                 // 0x10065d3b
}

// --------------------------------------------------------------------- Display::setMode 0x00065da0
//
// __thiscall(int w, int h, int bpp, int apply), `ret 0x10` at 0x10065e64. The fourth argument decides on its
// LOW BYTE only (`and eax,0xff` at 0x10065dc0, `jne 0x10065dc7`):
//   apply byte 0: slot +0x58 (Display::pickMode 0x10065ea0 in the real table) is called with
//                 (w, h, [this+0x134]) - the object's CURRENT depth, not the bpp argument, which is
//                 dropped on this arm (pushes at 0x10065dd4, 0x10065dd8, 0x10065ddc).
//   apply byte non-zero: [this+0x12c] = w (0x10065df7), [this+0x130] = h (0x10065e03), [this+0x134] = bpp
//                 (0x10065e0f), then slot +0x00 of the object at [this+0x148] is called with the three
//                 fields READ BACK (0x10065e1c, 0x10065e26, 0x10065e30) and a literal 1 (0x10065e17), with
//                 that object in ecx (0x10065e3a).
// No return value (the epilogue at 0x10065e54 does not load eax).
typedef void(__fastcall* SetMode_t)(void*, void*, int, int, int, int);
SetMode_t setMode_orig;

void __fastcall setMode_re(void* self, void*, int w, int h, int bpp, int apply) {
    if ((apply & 0xff) == 0) {                                            // 0x10065dc0 / jne 0x10065dc7
        Vfn<VPickMode_t>(self, 0x58)(self, 0, w, h, *F<int>(self, 0x134));   // 0x10065de5
        return;
    }
    *F<int>(self, 0x12c) = w;                                             // 0x10065df7
    *F<int>(self, 0x130) = h;                                             // 0x10065e03
    *F<int>(self, 0x134) = bpp;                                           // 0x10065e0f
    void* mode = P32R(self, 0x148);                                       // 0x10065e3a / 0x10065e43
    Vfn<VModeSet_t>(mode, 0x00)(mode, 0,                                  // 0x10065e4b
                                *F<int>(self, 0x12c),                     // 0x10065e30
                                *F<int>(self, 0x130),                     // 0x10065e26
                                *F<int>(self, 0x134),                     // 0x10065e1c
                                1);                                       // 0x10065e17
}

// -------------------------------------------------------------------- Display::pickMode 0x00065ea0
//
// __thiscall(int w, int h, int bpp), `ret 0xc` at 0x10065fe2. The same scan as findMode over the list slot
// +0x68 returns (0x10065ed5), with one extra test inside the loop: once a matching entry's frequency is
// greater than 0x4b (`cmp ...,0x4b` / `jle 0x10065f6d` at 0x10065f68) AND a candidate is already held
// (`cmp [ebp-0x10],-1` / `jle 0x10065f73`), the scan STOPS and slot +0x64 is called with the PREVIOUS
// candidate (0x10065f83), not with the entry that triggered it. Otherwise the loop runs out and slot +0x64
// is called with the best index if one was found (`jl 0x10065fb1`, call 0x10065fc1); with none, the return
// is the `mov eax,1` at 0x10065fcd. On both calling paths the slot's eax is the return value: nothing
// between the call and the epilogue touches eax (0x10065f8d, 0x10065fcb).
typedef int(__fastcall* PickMode_t)(void*, void*, int, int, int);
PickMode_t pickMode_orig;

int __fastcall pickMode_re(void* self, void*, int w, int h, int bpp) {
    int best = -1;                                            // 0x10065ebd
    int bestFreq = -1;                                        // 0x10065ec4
    void* modes = Vfn<VEnumModes_t>(self, 0x68)(self, 0);     // 0x10065ed5
    for (int i = 0; i < ArrayCount(modes); i++) {             // 0x10065ef7 / jge 0x10065eff
        const ModeInfo* m = ModeAt(modes, i);
        if (m->w != w) continue;                              // jne 0x10065f17
        if (m->h != h) continue;                              // jne 0x10065f30
        if (m->bpp != bpp) continue;                          // jne 0x10065f45
        if (m->freq <= bestFreq) continue;                    // jle 0x10065f5a
        if (m->freq > 0x4b && best > -1)                      // jle 0x10065f6d / jle 0x10065f73
            return Vfn<VSetModeIndex_t>(self, 0x64)(self, 0, best);   // 0x10065f83
        bestFreq = m->freq;                                   // 0x10065f9f
        best = i;                                             // 0x10065fa5
    }
    if (best >= 0)                                            // jl 0x10065fb1
        return Vfn<VSetModeIndex_t>(self, 0x64)(self, 0, best);       // 0x10065fc1
    return 1;                                                 // 0x10065fcd
}

// ---------------------------------------------------------------- Display::pickModeFreq 0x00066040
//
// __thiscall(int w, int h, int bpp, int apply, int freq), `ret 0x14` at 0x1006619d.
// With the low byte of `apply` non-zero (`and eax,0xff` at 0x1006606e, `je 0x10066075`) the whole body is
// one dispatch of slot +0x60 (Display::setMode 0x10065da0 in the real table) with (w, h, bpp, X), and that
// call's eax is returned (0x1006609b jumps to the
// epilogue). X is NOT the argument: 0x10066079 writes the low byte of [ebp+0x14] into CL of an ecx that
// still holds `this` (popped at 0x10066059 and not written since), and 0x1006607c pushes the whole
// register, so the fourth value the slot receives is (this & 0xffffff00) | (apply & 0xff) - three bytes of
// the object's own address.
// Otherwise it scans the list from slot +0x68 (0x100660aa) for w/h/bpp as pickMode does, but with a
// two-way frequency test: an entry whose +0x10 EQUALS the fifth argument wins immediately and ends the scan
// (`jne 0x1006612b`, `mov [ebp-0x10],edx` at 0x10066130, `jmp 0x10066168`); otherwise the largest frequency
// seen is kept (`jle 0x10066148`). There is no 0x4b test on this path. Then slot +0x64 with the index
// (0x1006617c) when one was found, else `mov eax,1` at 0x10066188.
typedef int(__fastcall* PickModeFreq_t)(void*, void*, int, int, int, int, int);
PickModeFreq_t pickModeFreq_orig;

int __fastcall pickModeFreq_re(void* self, void*, int w, int h, int bpp, int apply, int freq) {
    int best = -1;                                            // 0x1006605d
    int bestFreq = -1;                                        // 0x10066064
    if ((apply & 0xff) != 0) {                                // 0x1006606e / je 0x10066075
        // ecx holds `this` here, and only its low byte is replaced (0x10066079).
        unsigned mixed = (reinterpret_cast<unsigned>(self) & 0xffffff00u)
                       | (static_cast<unsigned>(apply) & 0xffu);
        return Vfn<VSetMode_t>(self, 0x60)(self, 0, w, h, bpp, static_cast<int>(mixed));  // 0x10066091
    }
    void* modes = Vfn<VEnumModes_t>(self, 0x68)(self, 0);     // 0x100660aa
    for (int i = 0; i < ArrayCount(modes); i++) {             // 0x100660cc / jge 0x100660d4
        const ModeInfo* m = ModeAt(modes, i);
        if (m->w != w) continue;                              // jne 0x100660ec
        if (m->h != h) continue;                              // jne 0x10066101
        if (m->bpp != bpp) continue;                          // jne 0x10066116
        if (m->freq == freq) {                                // jne 0x1006612b
            best = i;                                         // 0x10066130
            break;                                            // 0x10066133
        }
        if (m->freq <= bestFreq) continue;                    // jle 0x10066148
        bestFreq = m->freq;                                   // 0x1006615a
        best = i;                                             // 0x10066160
    }
    if (best >= 0)                                            // jl 0x1006616c
        return Vfn<VSetModeIndex_t>(self, 0x64)(self, 0, best);       // 0x1006617c
    return 1;                                                 // 0x10066188
}

// ----------------------------------------------------------------- Display::getField138 0x00065bf0
//
// __thiscall(), plain `ret` at 0x10065c1c: the whole body is `mov eax,[this+0x138]` (0x10065c10). That is
// the device context Display::createWindow 0x100657e0 stores from GetDC (import called at 0x10065988,
// store at 0x10065998) and Display::present 0x10066470 reads at 0x10066531 and 0x100665ab for its two
// BitBlt calls (0x10066538, 0x100665b2). No branch, no write.
typedef unsigned(__fastcall* GetDword_t)(void*, void*);
GetDword_t getField138_orig;

unsigned __fastcall getField138_re(void* self, void*) {
    return *F<unsigned>(self, 0x138);                         // 0x10065c10
}

// -------------------------------------------------------------- Display::getField12c130 0x000666b0
//
// __thiscall(int* a, int* b), `ret 8` at 0x100666ef: writes [this+0x12c] through the FIRST pointer
// (0x100666d3 / 0x100666d9) and [this+0x130] through the second (0x100666e1 / 0x100666e7). Neither pointer
// is tested, so a NULL argument faults in the original; no vector passes one. No branch, no read of the
// destinations.
typedef void(__fastcall* GetTwo_t)(void*, void*, int*, int*);
GetTwo_t getField12c130_orig;

void __fastcall getField12c130_re(void* self, void*, int* a, int* b) {
    *a = *F<int>(self, 0x12c);                                // 0x100666d3 / 0x100666d9
    *b = *F<int>(self, 0x130);                                // 0x100666e1 / 0x100666e7
}

// -------------------------------------------------------------------- Display::getHwnd 0x00067560
//
// __thiscall(), plain `ret` at 0x1006758c: `mov eax,[this+0x144]` (0x10067580), the window handle
// Display::createWindow 0x100657e0 stores at 0x1006591f from CreateWindowExA (0x1006590f). Slot 8 (+0x20)
// of the Display
// vtable; the DisplayBase body at the same slot (0x10066d60) returns a constant 0 instead.
GetDword_t getHwnd_orig;

unsigned __fastcall getHwnd_re(void* self, void*) {
    return *F<unsigned>(self, 0x144);                         // 0x10067580
}

// ---------------------------------------------------------------- Display::getInstance 0x000675a0
//
// __thiscall(), plain `ret` at 0x100675cc: `mov eax,[this+0x140]` (0x100675c0), the module handle
// Display::createWindow stores at 0x10065811 from GetModuleHandleA (0x10065801) and Display::shutdown
// 0x100656c0 passes to UnregisterClassA (0x10065771). Slot 9 (+0x24); DisplayBase 0x10066d90 returns 0.
GetDword_t getInstance_orig;

unsigned __fastcall getInstance_re(void* self, void*) {
    return *F<unsigned>(self, 0x140);                         // 0x100675c0
}

// ------------------------------------------------------------------- Display::getWidth 0x00067760
//
// __thiscall(), plain `ret` at 0x1006778c: `mov eax,[this+0x12c]` (0x10067780). That field is written by
// Display::createWindow (0x1006594f, from GetSystemMetrics at 0x1006593f) and by Display::setMode
// 0x10065da0 (0x10065df7).
GetDword_t getWidth_orig;

int __fastcall getWidth_re(void* self, void*) {
    return *F<int>(self, 0x12c);                              // 0x10067780
}

// ------------------------------------------------------------------ Display::getHeight 0x000677a0
//
// __thiscall(), plain `ret` at 0x100677cc: `mov eax,[this+0x130]` (0x100677c0), written by
// Display::createWindow at 0x10065969 and by Display::setMode at 0x10065e03.
GetDword_t getHeight_orig;

int __fastcall getHeight_re(void* self, void*) {
    return *F<int>(self, 0x130);                              // 0x100677c0
}

// --------------------------------------------------------------------- Display::getBpp 0x000677e0
//
// __thiscall(), plain `ret` at 0x1006780c: `mov eax,[this+0x134]` (0x10067800), the depth setMode writes
// at 0x10065e0f and reads back on its other arm at 0x10065dce.
GetDword_t getBpp_orig;

int __fastcall getBpp_re(void* self, void*) {
    return *F<int>(self, 0x134);                              // 0x10067800
}

// ------------------------------------------------------------------- Display::setScale 0x00067d50
//
// __thiscall(int a, int b, int c), `ret 0xc` at 0x10067d8d: three stores into the module globals
// 0x10122dc0 (0x10067d70), 0x10122dc4 (0x10067d78) and 0x10122dc8 (0x10067d81), in argument order. `this`
// is spilled to the /GZ frame slot at 0x10067d6a and never read. No branch, no return value.
// Those three dwords are the words about forty sprite blitters read to pick their body and their mirror
// quadrant (re/names/jgld_1.tsv), so the A/B key suspends the game's first thread and the fixture writes
// the game's values back before resuming it.
typedef void(__fastcall* SetScale_t)(void*, void*, int, int, int);
SetScale_t setScale_orig;

void __fastcall setScale_re(void*, void*, int a, int b, int c) {
    *reinterpret_cast<int*>(JgldVA(0x10122dc0)) = a;          // 0x10067d70
    *reinterpret_cast<int*>(JgldVA(0x10122dc4)) = b;          // 0x10067d78
    *reinterpret_cast<int*>(JgldVA(0x10122dc8)) = c;          // 0x10067d81
}

// ------------------------------------------------------------------- Display::getScale 0x00067da0
//
// __thiscall(int* a, int* b, int* c), `ret 0xc` at 0x10067de3: the three globals read back through the
// three pointers, in the same order (0x10067dc0/0x10067dc6, 0x10067dcb/0x10067dd0, 0x10067dd5/0x10067ddb).
// No pointer is tested and `this` is not read. No branch, no return value.
typedef void(__fastcall* GetScale_t)(void*, void*, int*, int*, int*);
GetScale_t getScale_orig;

void __fastcall getScale_re(void*, void*, int* a, int* b, int* c) {
    *a = *reinterpret_cast<int*>(JgldVA(0x10122dc0));         // 0x10067dc0 / 0x10067dc6
    *b = *reinterpret_cast<int*>(JgldVA(0x10122dc4));         // 0x10067dcb / 0x10067dd0
    *c = *reinterpret_cast<int*>(JgldVA(0x10122dc8));         // 0x10067dd5 / 0x10067ddb
}

// ------------------------------------------------- the four Factory destroy slots (34, 35, 36, 37)
//
// `delete p` on a polymorphic object, as VC6 /Od generates it: copy the argument into two frame slots, test
// it against zero and, when it is not zero, call SLOT 0 OF THE ARGUMENT'S OWN VTABLE with a literal 1 - the
// scalar deleting destructor's "also free me" flag. The four bodies are the same twenty-two instructions at
// the same offsets and differ only in their addresses:
//
//                            copy      test / je     push 1     call [vtbl]   ret 4
//   destroyFont    0x67b50   0x10067b6d  0x10067b79/7d  0x10067b81  0x10067b8b  0x10067bb0
//   destroyPalette 0x67bd0   0x10067bed  0x10067bf9/fd  0x10067c01  0x10067c0b  0x10067c30
//   destroySurface 0x67c50   0x10067c6d  0x10067c79/7d  0x10067c81  0x10067c8b  0x10067cb0
//   destroySprite  0x67cd0   0x10067ced  0x10067cf9/fd  0x10067d01  0x10067d0b  0x10067d30
//
// They return nothing: the slot's eax is stored into the frame slot at [ebp-0x10] (0x10067b94) and never
// loaded back, and the NULL arm only writes a 0 there (0x10067b99), so the epilogue leaves eax as the slot
// left it (or, on the NULL arm, as the argument load at 0x10067b6d left it: the NULL pointer itself).
// The call goes through the CALLER'S object, so the A/B can stand a counting stub in that slot and no
// memory is ever freed - the same escape c3bw used for LinkedList::clear and that the direct
// `call operator delete` of a deleting destructor does not offer (c3cd).
typedef void(__fastcall* Destroy_t)(void*, void*, void*);
typedef int(__fastcall* VDeletingDtor_t)(void*, void*, int);

void Destroy(void* p) {
    if (p == 0) return;                                       // je 0x10067b7d
    Vfn<VDeletingDtor_t>(p, 0x00)(p, 0, 1);                   // 0x10067b8b, flag pushed at 0x10067b81
}

// 0x00067b50  Factory::destroyFont
Destroy_t destroyFont_orig;
void __fastcall destroyFont_re(void*, void*, void* p) { Destroy(p); }

// 0x00067bd0  Factory::destroyPalette
Destroy_t destroyPalette_orig;
void __fastcall destroyPalette_re(void*, void*, void* p) { Destroy(p); }

// 0x00067c50  Factory::destroySurface
Destroy_t destroySurface_orig;
void __fastcall destroySurface_re(void*, void*, void* p) { Destroy(p); }

// 0x00067cd0  Factory::destroySprite
Destroy_t destroySprite_orig;
void __fastcall destroySprite_re(void*, void*, void* p) { Destroy(p); }

}  // namespace

// The four mode methods: each dispatches the display-mode slots of its own vtable.
// 0x00065c30  Display::findMode
SG_HOOK("jgld.dll", 0x00065C30, Display_findMode, findMode_re, findMode_orig);
// 0x00065da0  Display::setMode
SG_HOOK("jgld.dll", 0x00065DA0, Display_setMode, setMode_re, setMode_orig);
// 0x00065ea0  Display::pickMode
SG_HOOK("jgld.dll", 0x00065EA0, Display_pickMode, pickMode_re, pickMode_orig);
// 0x00066040  Display::pickModeFreq
SG_HOOK("jgld.dll", 0x00066040, Display_pickModeFreq, pickModeFreq_re, pickModeFreq_orig);
// The field accessors of the Display vtable.
// 0x00065bf0  Display::getField138
SG_HOOK("jgld.dll", 0x00065BF0, Display_getField138, getField138_re, getField138_orig);
// 0x000666b0  Display::getField12c130
SG_HOOK("jgld.dll", 0x000666B0, Display_getField12c130, getField12c130_re, getField12c130_orig);
// 0x00067560  Display::getHwnd
SG_HOOK("jgld.dll", 0x00067560, Display_getHwnd, getHwnd_re, getHwnd_orig);
// 0x000675a0  Display::getInstance
SG_HOOK("jgld.dll", 0x000675A0, Display_getInstance, getInstance_re, getInstance_orig);
// 0x00067760  Display::getWidth
SG_HOOK("jgld.dll", 0x00067760, Display_getWidth, getWidth_re, getWidth_orig);
// 0x000677a0  Display::getHeight
SG_HOOK("jgld.dll", 0x000677A0, Display_getHeight, getHeight_re, getHeight_orig);
// 0x000677e0  Display::getBpp
SG_HOOK("jgld.dll", 0x000677E0, Display_getBpp, getBpp_re, getBpp_orig);
// The two scale slots: the only module globals this batch touches.
// 0x00067d50  Display::setScale
SG_HOOK("jgld.dll", 0x00067D50, Display_setScale, setScale_re, setScale_orig);
// 0x00067da0  Display::getScale
SG_HOOK("jgld.dll", 0x00067DA0, Display_getScale, getScale_re, getScale_orig);
// The four Factory destroy slots: `delete p` through slot 0 of the argument's own vtable.
// 0x00067b50  Factory::destroyFont
SG_HOOK("jgld.dll", 0x00067B50, Factory_destroyFont, destroyFont_re, destroyFont_orig);
// 0x00067bd0  Factory::destroyPalette
SG_HOOK("jgld.dll", 0x00067BD0, Factory_destroyPalette, destroyPalette_re, destroyPalette_orig);
// 0x00067c50  Factory::destroySurface
SG_HOOK("jgld.dll", 0x00067C50, Factory_destroySurface, destroySurface_re, destroySurface_orig);
// 0x00067cd0  Factory::destroySprite
SG_HOOK("jgld.dll", 0x00067CD0, Factory_destroySprite, destroySprite_re, destroySprite_orig);
