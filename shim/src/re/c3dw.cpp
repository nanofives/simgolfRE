// C3 batch c3dw (round 28, 2026-10-11) of golf_clean.exe: rows earlier rounds dropped for heap use, imports, live globals or
// file I/O, retried with the shared stand-ins (re/frida/js/heapstandin.js, importstandin.js, freeze.js) and, for the display
// glue, a synthetic display object with recording slots swapped into [0x0083ad50] under a frozen first thread (c3dq's method).
// Fixtures: re/frida/js/fixtures.d/c3dw.js. Registry: re/frida/registry.d/c3dw.py. Purposes and per-jcc tables:
// log/c3/c3dw_purpose.md.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and cites
// the instructions it reproduces. __thiscall is emulated with __fastcall (ecx = this, edx unused). Callees run through their
// original addresses; an import is called through the IAT slot the original reads, never the shim's own import table.
#include "hooks.h"

namespace {

template <typename T> static inline T& G(unsigned addr) { return *reinterpret_cast<T*>(addr); }
template <typename T> static inline T Imp(unsigned slot) { return *reinterpret_cast<T*>(slot); }
static inline char* B(void* p) { return reinterpret_cast<char*>(p); }
template <typename T> static inline T& At(void* p, unsigned off) { return *reinterpret_cast<T*>(B(p) + off); }

typedef int(__cdecl* C0_t)();
typedef int(__cdecl* C1_t)(int a);
typedef int(__fastcall* T0_t)(void* ecx, void* edx);
typedef int(__fastcall* T1_t)(void* ecx, void* edx, int a);
typedef int(__fastcall* T7_t)(void* ecx, void* edx, int a, int b, int c, int d, int e, int f, int g);

// ---- the pump family's callees (original addresses)
const C0_t kRedrawAll = reinterpret_cast<C0_t>(0x00483c70);        // redrawAll
const C0_t kShowScreen = reinterpret_cast<C0_t>(0x00483bb0);       // showScreen
const C0_t kHideScreen = reinterpret_cast<C0_t>(0x00483c10);       // hideScreen
const C0_t kEnterModal = reinterpret_cast<C0_t>(0x00483d30);       // enterModal483d30
const C0_t kFrameTick = reinterpret_cast<C0_t>(0x00483cd0);        // frameTick483cd0

// gfxFlushA 0x00497b00 / gfxFlushB 0x00497b20 inlined at the tail jumps (c3dq's method): eaxIn is the eax the caller holds at
// the call or jmp. ecx = [0x008400b8]; zero returns eaxIn untouched (0x00497b06/0x00497b08, 0x00497b26/0x00497b28); then
// eax = [0x008400b0], zero returns 0 (0x00497b0f..0x00497b11, 0x00497b2f..0x00497b31); else a tail jmp (0x00497b13 /
// 0x00497b33) to netPump 0x0049aa70 / netServiceControl 0x0049ab40 with ecx = the net object.
const T0_t kNetPump = reinterpret_cast<T0_t>(0x0049aa70);
const T0_t kNetServiceControl = reinterpret_cast<T0_t>(0x0049ab40);
static int flushATail(int eaxIn) {
    void* net = G<void*>(0x008400b8);
    if (net == 0) return eaxIn;
    if (G<int>(0x008400b0) == 0) return 0;
    return kNetPump(net, 0);
}
static int flushBTail(int eaxIn) {
    void* net = G<void*>(0x008400b8);
    if (net == 0) return eaxIn;
    if (G<int>(0x008400b0) == 0) return 0;
    return kNetServiceControl(net, 0);
}
const C0_t kPumpInput483bd0 = reinterpret_cast<C0_t>(0x00483bd0);
const C0_t kPumpInput483c30 = reinterpret_cast<C0_t>(0x00483c30);
const C0_t kDrainAndRedraw = reinterpret_cast<C0_t>(0x0045af00);
const C0_t kPumpMessages = reinterpret_cast<C0_t>(0x0045c030);
const C0_t kFlushRedraw = reinterpret_cast<C0_t>(0x0045ae70);
const C0_t kFrameTickLoop = reinterpret_cast<C0_t>(0x00483cf0);

// ---- imports (golf_clean.exe IAT slots, pefile over original\golf_clean.exe)
typedef int(__stdcall* PeekMessageA_t)(void* msg, void* hwnd, unsigned lo, unsigned hi, unsigned remove);
typedef int(__stdcall* MsgFn_t)(void* msg);
const unsigned kIatPeekMessageA = 0x004ba1d8;
const unsigned kIatDispatchMessageA = 0x004ba1dc;
const unsigned kIatTranslateMessage = 0x004ba1e0;

}  // namespace

// ====================================================================================================================
// Message / redraw pumps
// ====================================================================================================================

// 0x00465560  resetScrollText: enterModal483d30 0x00483d30 (call 0x00465560), then [0x00822d68], [0x00822b9c],
// [0x00822b98] = 0 (xor eax, eax 0x00465565; stores 0x00465567, 0x0046556c, 0x00465571). eax = 0 at the ret (0x00465576).
int __cdecl ResetScrollText_re() {
    kEnterModal();                                                          // 0x00465560
    G<int>(0x00822d68) = 0;                                                 // 0x00465567
    G<int>(0x00822b9c) = 0;                                                 // 0x0046556c
    G<int>(0x00822b98) = 0;                                                 // 0x00465571
    return 0;                                                               // 0x00465565, ret 0x00465576
}
C0_t ResetScrollText_orig;
// 0x00465560
SG_HOOK("golf_clean.exe", 0x00465560, resetScrollText, ResetScrollText_re, ResetScrollText_orig);

// 0x0045af00  drainAndRedraw: redrawAll 0x00483c70 (call 0x0045af00), pumpInput483c30 0x00483c30 (call 0x0045af05); a
// non-zero [0x00822b9c] (0x0045af0a/0x0045af0f/0x0045af11) or [0x00822b98] (0x0045af13/0x0045af18/0x0045af1a) returns 0
// (xor eax, eax 0x0045af22); both zero returns 1 (0x0045af1c).
int __cdecl DrainAndRedraw_re() {
    kRedrawAll();                                                           // 0x0045af00
    kPumpInput483c30();                                                     // 0x0045af05
    if (G<int>(0x00822b9c) != 0) return 0;                                  // 0x0045af0f / 0x0045af11
    if (G<int>(0x00822b98) != 0) return 0;                                  // 0x0045af18 / 0x0045af1a
    return 1;                                                               // 0x0045af1c
}
C0_t DrainAndRedraw_orig;
// 0x0045af00
SG_HOOK("golf_clean.exe", 0x0045af00, drainAndRedraw, DrainAndRedraw_re, DrainAndRedraw_orig);

// 0x0045aed0  waitRedrawReady: drainAndRedraw (call 0x0045aed1); esi = 0 (xor 0x0045aed6). A non-zero result skips the loop
// (jne 0x0045aef1). Otherwise [0x00822b9c] = [0x00822b98] = 0 (0x0045aedc, 0x0045aee2), drainAndRedraw again (0x0045aee8)
// until it returns non-zero (je 0x0045aedc). Then frameTickLoop 0x00483cf0 (call 0x0045aef1), [0x00822d68] = 0
// (0x0045aef6) and eax = the result of 0x00483cf0 at the ret (0x0045aefd).
int __cdecl WaitRedrawReady_re() {
    int r = kDrainAndRedraw();                                              // 0x0045aed1
    if (r == 0) {                                                           // 0x0045aed8 / 0x0045aeda
        do {
            G<int>(0x00822b9c) = 0;                                         // 0x0045aedc
            G<int>(0x00822b98) = 0;                                         // 0x0045aee2
            r = kDrainAndRedraw();                                          // 0x0045aee8
        } while (r == 0);                                                   // 0x0045aeed / 0x0045aeef
    }
    int e = kFrameTickLoop();                                               // 0x0045aef1
    G<int>(0x00822d68) = 0;                                                 // 0x0045aef6
    return e;                                                               // 0x0045aefd
}
C0_t WaitRedrawReady_orig;
// 0x0045aed0
SG_HOOK("golf_clean.exe", 0x0045aed0, waitRedrawReady, WaitRedrawReady_re, WaitRedrawReady_orig);

// 0x0045ae70  flushRedraw: [0x004d3834] non-zero (0x0045ae70/0x0045ae75/0x0045ae77): cleared (0x0045ae79), redrawAll
// (0x0045ae83), [0x00822b9c] = [0x00822b98] = 0 (0x0045ae88, 0x0045ae92); zero: redrawAll (0x0045ae9e). Then
// pumpInput483c30 (0x0045aea3). A non-zero [0x00822b98] (0x0045aea8..0x0045aeaf) or [0x00822b9c] (0x0045aeb1..0x0045aeb8)
// returns 1 with [0x004d3834] = 1 (0x0045aebd, 0x0045aec2, 0x0045aec7); both zero returns 0 (0x0045aeba).
int __cdecl FlushRedraw_re() {
    if (G<int>(0x004d3834) != 0) {                                          // 0x0045ae75 / 0x0045ae77
        G<int>(0x004d3834) = 0;                                             // 0x0045ae79
        kRedrawAll();                                                       // 0x0045ae83
        G<int>(0x00822b9c) = 0;                                             // 0x0045ae88
        G<int>(0x00822b98) = 0;                                             // 0x0045ae92
    } else {
        kRedrawAll();                                                       // 0x0045ae9e
    }
    kPumpInput483c30();                                                     // 0x0045aea3
    if (G<int>(0x00822b98) == 0 && G<int>(0x00822b9c) == 0) return 0;       // 0x0045aeaf / 0x0045aeb6 / 0x0045aeba
    G<int>(0x004d3834) = 1;                                                 // 0x0045aec2
    return 1;                                                               // 0x0045aebd
}
C0_t FlushRedraw_orig;
// 0x0045ae70
SG_HOOK("golf_clean.exe", 0x0045ae70, flushRedraw, FlushRedraw_re, FlushRedraw_orig);

// 0x00483bd0  pumpInput483bd0: [0x0083ad4c] = 0x3f (0x00483bd0); showScreen 0x00483bb0 (0x00483bda); a zero result jumps to
// 0x00483bf6. A non-zero result loops: [0x0083ad4c] = 0x3f (0x00483be3), showScreen (0x00483bed) while non-zero
// (0x00483bf2/0x00483bf4). [0x0083ad4c] = 0 (0x00483bf6), then a tail jmp to gfxFlushA 0x00497b00 (0x00483c00) with eax = 0.
int __cdecl PumpInput483bd0_re() {
    G<int>(0x0083ad4c) = 0x3f;                                              // 0x00483bd0
    if (kShowScreen() != 0) {                                               // 0x00483bda / 0x00483bdf / 0x00483be1
        do {
            G<int>(0x0083ad4c) = 0x3f;                                      // 0x00483be3
        } while (kShowScreen() != 0);                                       // 0x00483bed / 0x00483bf2 / 0x00483bf4
    }
    G<int>(0x0083ad4c) = 0;                                                 // 0x00483bf6
    return flushATail(0);                                                   // 0x00483c00 (eax = 0: the loop exits on a zero result)
}
C0_t PumpInput483bd0_orig;
// 0x00483bd0
SG_HOOK("golf_clean.exe", 0x00483bd0, pumpInput483bd0, PumpInput483bd0_re, PumpInput483bd0_orig);

// 0x00483c30  pumpInput483c30: same shape with [0x0083ad4c] = 0x20 and hideScreen 0x00483c10 (0x00483c3a, 0x00483c4d); after
// [0x0083ad4c] = 0 (0x00483c56) gfxFlushA is called (0x00483c60) and gfxFlushB entered by a tail jmp (0x00483c65), so the
// result is gfxFlushB's with eax as gfxFlushA left it.
int __cdecl PumpInput483c30_re() {
    G<int>(0x0083ad4c) = 0x20;                                              // 0x00483c30
    if (kHideScreen() != 0) {                                               // 0x00483c3a / 0x00483c3f / 0x00483c41
        do {
            G<int>(0x0083ad4c) = 0x20;                                      // 0x00483c43
        } while (kHideScreen() != 0);                                       // 0x00483c4d / 0x00483c52 / 0x00483c54
    }
    G<int>(0x0083ad4c) = 0;                                                 // 0x00483c56
    int a = flushATail(0);                                                  // 0x00483c60 (eax = 0 from the last test)
    return flushBTail(a);                                                   // 0x00483c65
}
C0_t PumpInput483c30_orig;
// 0x00483c30
SG_HOOK("golf_clean.exe", 0x00483c30, pumpInput483c30, PumpInput483c30_re, PumpInput483c30_orig);

// 0x0045c150  pumpLight: [0x00822d68] = 0 (0x0045c150). Loop: pumpInput483bd0 (0x0045c15a), pumpMessages 0x0045c030
// (0x0045c15f); a non-zero [0x00822d68] repeats (0x0045c164..0x0045c16b); flushRedraw 0x0045ae70 (0x0045c16d); a non-zero
// result repeats (0x0045c172/0x0045c174). eax = 0 at the ret (0x0045c176).
int __cdecl PumpLight_re() {
    G<int>(0x00822d68) = 0;                                                 // 0x0045c150
    for (;;) {
        kPumpInput483bd0();                                                 // 0x0045c15a
        kPumpMessages();                                                    // 0x0045c15f
        if (G<int>(0x00822d68) != 0) continue;                              // 0x0045c164 / 0x0045c169 / 0x0045c16b
        if (kFlushRedraw() == 0) break;                                     // 0x0045c16d / 0x0045c172 / 0x0045c174
    }
    return 0;                                                               // eax from the last test, 0
}
C0_t PumpLight_orig;
// 0x0045c150
SG_HOOK("golf_clean.exe", 0x0045c150, pumpLight, PumpLight_re, PumpLight_orig);

// 0x0045c030  pumpMessages: PeekMessageA [0x4ba1d8] (msg, 0, 0x200, 0x200, 1) (pushes 0x0045c03e..0x0045c04d); while it
// returns non-zero (0x0045c04f/0x0045c051, loop test 0x0045c084/0x0045c086) TranslateMessage [0x4ba1e0] (call 0x0045c066)
// and DispatchMessageA [0x4ba1dc] (call 0x0045c06d) on the message, then Peek again (0x0045c082). Then a zero [0x00822d74]
// (0x0045c08a..0x0045c092) clears [0x00822d68] (0x0045c094); frameTick483cd0 0x00483cd0 (0x0045c09e); [0x00822d74] = 0
// (0x0045c0a3); eax = frameTick's result (ret 0x0045c0b0).
int __cdecl PumpMessages_re() {
    char msg[0x1c];                                                         // sub esp, 0x1c (0x0045c030)
    while (Imp<PeekMessageA_t>(kIatPeekMessageA)(msg, 0, 0x200, 0x200, 1) != 0) {   // 0x0045c04d / 0x0045c082
        Imp<MsgFn_t>(kIatTranslateMessage)(msg);                            // 0x0045c066
        Imp<MsgFn_t>(kIatDispatchMessageA)(msg);                            // 0x0045c06d
    }
    if (G<int>(0x00822d74) == 0) G<int>(0x00822d68) = 0;                    // 0x0045c08a..0x0045c094
    int r = kFrameTick();                                                   // 0x0045c09e
    G<int>(0x00822d74) = 0;                                                 // 0x0045c0a3
    return r;                                                               // 0x0045c0b0
}
C0_t PumpMessages_orig;
// 0x0045c030
SG_HOOK("golf_clean.exe", 0x0045c030, pumpMessages, PumpMessages_re, PumpMessages_orig);

// ====================================================================================================================
// Terrain glue (the exe reaches Terrain.dll through `jmp [slot]` import thunks 0x004a4fxx; the Terrain object is [0x00820ed0])
// ====================================================================================================================

namespace {
typedef int(__fastcall* TerPass_t)(void* ecx, void* edx, int* table, int count);
const T7_t kTerDrawLine = reinterpret_cast<T7_t>(0x004a4f16);       // Terrain::drawLine, jmp [0x004ba138]
const T1_t kTerLoadNewCourseType = reinterpret_cast<T1_t>(0x004a4f1c);   // jmp [0x004ba13c]
const TerPass_t kTerPassCollarInfo = reinterpret_cast<TerPass_t>(0x004a4f22);   // jmp [0x004ba140]
const T0_t kTerResetTerrain = reinterpret_cast<T0_t>(0x004a4f34);    // jmp [0x004ba194]
const T0_t kTerDtor = reinterpret_cast<T0_t>(0x004a4f5e);            // jmp [0x004ba178]
const T0_t kTerCloseSystem = reinterpret_cast<T0_t>(0x004a4f64);     // jmp [0x004ba174]
const C0_t kRebuildTerrainGrid = reinterpret_cast<C0_t>(0x00449540);
typedef int(__cdecl* Delete_t)(void* p);
const Delete_t kOperatorDelete = reinterpret_cast<Delete_t>(0x004a4ffc);
}  // namespace

// 0x004493d0  drawMapLine(a1..a7): the seven stack arguments are pushed in the same order (0x004493dc..0x004493f8) behind
// ecx = [0x00820ed0] (0x004493f1) for Terrain::drawLine through the thunk 0x004a4f16 (call 0x004493f9); ret (0x004493fe)
// with the callee's eax.
int __cdecl DrawMapLine_re(int a1, int a2, int a3, int a4, int a5, int a6, int a7) {
    return kTerDrawLine(G<void*>(0x00820ed0), 0, a1, a2, a3, a4, a5, a6, a7);     // 0x004493f9
}
typedef int(__cdecl* C7_t)(int, int, int, int, int, int, int);
C7_t DrawMapLine_orig;
// 0x004493d0
SG_HOOK("golf_clean.exe", 0x004493d0, drawMapLine, DrawMapLine_re, DrawMapLine_orig);

// 0x0040bf20  lineTo(a1..a5): drawMapLine 0x004493d0 (call 0x0040bf48) with (a1, a2, [0x005a412c], [0x005a4130], a3, a4, a5)
// (pushes 0x0040bf36..0x0040bf47: edi = a1, esi = a2 are the last two pushed); then [0x005a412c] = a1 (0x0040bf50) and
// [0x005a4130] = a2 (0x0040bf56). eax = drawMapLine's.
int __cdecl LineTo_re(int a1, int a2, int a3, int a4, int a5) {
    int px = G<int>(0x005a412c), py = G<int>(0x005a4130);                         // 0x0040bf37, 0x0040bf3d
    int r = reinterpret_cast<C7_t>(0x004493d0)(a1, a2, px, py, a3, a4, a5);       // 0x0040bf48
    G<int>(0x005a412c) = a1;                                                      // 0x0040bf50
    G<int>(0x005a4130) = a2;                                                      // 0x0040bf56
    return r;
}
typedef int(__cdecl* C5_t)(int, int, int, int, int);
C5_t LineTo_orig;
// 0x0040bf20
SG_HOOK("golf_clean.exe", 0x0040bf20, lineTo, LineTo_re, LineTo_orig);

// 0x00449520  refreshTerrain: Terrain::resetTerrain through the thunk 0x004a4f34 with ecx = [0x00820ed0] (0x00449520,
// 0x00449526), rebuildTerrainGrid 0x00449540 (call 0x0044952b), [0x00820f2a] = 1 (byte store 0x00449530). eax = rebuild's.
int __cdecl RefreshTerrain_re() {
    kTerResetTerrain(G<void*>(0x00820ed0), 0);                                    // 0x00449526
    int r = kRebuildTerrainGrid();                                                // 0x0044952b
    G<unsigned char>(0x00820f2a) = 1;                                             // 0x00449530
    return r;
}
C0_t RefreshTerrain_orig;
// 0x00449520
SG_HOOK("golf_clean.exe", 0x00449520, refreshTerrain, RefreshTerrain_re, RefreshTerrain_orig);

// 0x00449860  freeTerrain: Terrain::closeSystem through 0x004a4f64 with ecx = [0x00820ed0] (0x00449860, 0x00449867); the
// pointer is read again (0x0044986c); non-NULL: ~Terrain through 0x004a4f5e (ecx = it, 0x00449876/0x00449878), operator
// delete 0x004a4ffc (push 0x0044987d, call 0x0044987e); [0x00820ed0] = 0 (0x00449886). The delete result is not
// meaningful (void), so the registry compares memory only.
void __cdecl FreeTerrain_re() {
    kTerCloseSystem(G<void*>(0x00820ed0), 0);                                     // 0x00449867
    void* t = G<void*>(0x00820ed0);                                               // 0x0044986c
    if (t != 0) {                                                                 // 0x00449872 / 0x00449874
        kTerDtor(t, 0);                                                           // 0x00449878
        kOperatorDelete(t);                                                       // 0x0044987e
    }
    G<int>(0x00820ed0) = 0;                                                       // 0x00449886
}
typedef void(__cdecl* V0_t)();
V0_t FreeTerrain_orig;
// 0x00449860
SG_HOOK("golf_clean.exe", 0x00449860, freeTerrain, FreeTerrain_re, FreeTerrain_orig);

// 0x00449400  applyCourseType(arg): dl = byte [0x005a34e0] (0x00449400). For the 23 records at 0x00578372 + k * 0x30
// (loop 0x00449413..0x00449442, 23 iterations until the address reaches 0x005787c2): table[k] = the signed byte at +4
// (movsx 0x00449413); when dl == 1 (0x00449417) and the signed byte at +0 is > 0 (0x0044941e / 0x00449421 `jle`) and the
// record is neither 0x00578462 nor 0x005784c2 (0x00449423, 0x0044942a) table[k] = 4 (0x00449431). Then
// Terrain::passCollarInfo(table, 0x17) through 0x004a4f22 (pushes 0x0044944e, 0x00449450) and
// Terrain::loadNewCourseType(arg) through 0x004a4f1c (0x0044945a..0x00449461), both with ecx = [0x00820ed0]. eax = the
// last call's.
int __cdecl ApplyCourseType_re(int arg) {
    int table[23];                                                                // sub esp, 0x5c (0x00449406)
    unsigned char dl = G<unsigned char>(0x005a34e0);                              // 0x00449400
    unsigned addr = 0x00578372;                                                   // 0x0044940d
    for (int i = 0; addr < 0x005787c2; i++, addr += 0x30) {                       // 0x00449437..0x00449442
        table[i] = G<signed char>(addr + 4);                                      // 0x00449413
        if (dl == 1 && G<signed char>(addr) > 0 && addr != 0x00578462 && addr != 0x005784c2)   // 0x00449417..0x0044942f
            table[i] = 4;                                                         // 0x00449431
    }
    kTerPassCollarInfo(G<void*>(0x00820ed0), 0, table, 0x17);                     // 0x00449451
    return kTerLoadNewCourseType(G<void*>(0x00820ed0), 0, arg);                   // 0x00449461
}
typedef int(__cdecl* C1x_t)(int);
C1x_t ApplyCourseType_orig;
// 0x00449400
SG_HOOK("golf_clean.exe", 0x00449400, applyCourseType, ApplyCourseType_re, ApplyCourseType_orig);

// ====================================================================================================================
// Window glue (windows are 0x300+ byte objects: +0 vtable, +0x9c / +0xa0 flag words, +0xb0 a link, +0x130 the parent,
// +0x224 / +0x22c a child array and its count; the top-level array is 0x0083a2d8 with its count at 0x0083ab94)
// ====================================================================================================================

namespace {
typedef int(__fastcall* T2_t)(void* ecx, void* edx, int a, int b);
typedef int(__fastcall* T3_t)(void* ecx, void* edx, int a, int b, int c);
typedef int(__fastcall* Tp_t)(void* ecx, void* edx, void* p);
typedef int(__cdecl* Cp_t)(void* p);
typedef int(__stdcall* S1_t)(int a);
const T0_t kVisible = reinterpret_cast<T0_t>(0x004801f0);              // Window::visible
const Cp_t kRelayoutAllP = reinterpret_cast<Cp_t>(0x0047e450);         // plain `ret`, no argument is read
const T0_t kPanelNotify = reinterpret_cast<T0_t>(0x0047e120);
const Tp_t kBringToFront = reinterpret_cast<Tp_t>(0x0047e700);         // ret 4
const Cp_t kZRaise = reinterpret_cast<Cp_t>(0x0047e580);
const T3_t kMouseEvent238 = reinterpret_cast<T3_t>(0x0047be40);        // ret 0xc
const T1_t kNetBeginOp0 = reinterpret_cast<T1_t>(0x00497c20);          // ret 4
const T1_t kNetBeginOp1 = reinterpret_cast<T1_t>(0x00497c70);          // ret 4
const T0_t kNetOp0 = reinterpret_cast<T0_t>(0x00497cc0);               // no stack argument
const T1_t kDispatchCommand = reinterpret_cast<T1_t>(0x0047c970);      // ret 4
const T1_t kDispatchAccel = reinterpret_cast<T1_t>(0x0047c6c0);        // ret 4
const T3_t kWin47c010 = reinterpret_cast<T3_t>(0x0047c010);            // ret 0xc
const T3_t kWin47c430 = reinterpret_cast<T3_t>(0x0047c430);            // ret 0xc
const Tp_t kWin47fa30 = reinterpret_cast<Tp_t>(0x0047fa30);            // ret 4
const Tp_t kWin47b0d0 = reinterpret_cast<Tp_t>(0x0047b0d0);            // ret 4
const Cp_t kDrawTooltip = reinterpret_cast<Cp_t>(0x0047cdb0);          // cdecl (RECT*)
const C0_t kCurrentFocusOwner = reinterpret_cast<C0_t>(0x0047f2f0);
const T1_t kTextWorker = reinterpret_cast<T1_t>(0x00478610);           // ret 4
static inline int kRelayoutAll(int) { return reinterpret_cast<C0_t>(0x0047e450)(); }
static inline int vcall1(void* obj, unsigned off, int a) {
    return reinterpret_cast<T1_t>(*reinterpret_cast<void**>(*reinterpret_cast<char**>(obj) + off))(obj, 0, a);
}
static inline int vcall2(void* obj, unsigned off, int a, int b) {
    return reinterpret_cast<T2_t>(*reinterpret_cast<void**>(*reinterpret_cast<char**>(obj) + off))(obj, 0, a, b);
}
}  // namespace

// 0x0047eff0  Window::fwdMouseEvent238(this, a1, a2, a3) (ret 0xc): the three arguments are pushed in order (0x0047eff4..
// 0x0047effe) and mouseEvent238 0x0047be40 is called with ecx still this (call 0x0047efff); eax = its result.
int __fastcall FwdMouseEvent238_re(void* self, void*, int a1, int a2, int a3) {
    return kMouseEvent238(self, 0, a1, a2, a3);                                   // 0x0047efff
}
typedef int(__fastcall* Fwd3_t)(void*, void*, int, int, int);
Fwd3_t FwdMouseEvent238_orig;
// 0x0047eff0
SG_HOOK("golf_clean.exe", 0x0047eff0, Window_fwdMouseEvent238, FwdMouseEvent238_re, FwdMouseEvent238_orig);

// 0x0047e120  Panel::notify(this): a parent at +0x130 (`mov ecx, [eax + 0x130]` 0x0047e123, `je` 0x0047e12b) takes
// ChildArray::bringToFront 0x0047e700 with this as its argument (push 0x0047e122, call 0x0047e12d: its `ret 4` pops it);
// without a parent zRaise 0x0047e580 gets this on the stack (call 0x0047e133, then `pop ecx`). eax = the callee's.
int __fastcall PanelNotify_re(void* self, void*) {
    if (At<unsigned>(self, 0x9c) & 0x2000000) return reinterpret_cast<int>(self); // callee exits at once (0x0047e713 / 0x0047e593) with eax = this (`mov eax, ecx` 0x0047e120)
    void* parent = At<void*>(self, 0x130);                                        // 0x0047e123
    if (parent != 0) return kBringToFront(parent, 0, self);                       // 0x0047e12d
    return kZRaise(self);                                                         // 0x0047e133
}
T0_t PanelNotify_orig;
// 0x0047e120
SG_HOOK("golf_clean.exe", 0x0047e120, Panel_notify, PanelNotify_re, PanelNotify_orig);

// 0x0047e700  ChildArray::bringToFront(this, win) (ret 4): a NULL win (0x0047e705 / 0x0047e707) or a win whose flag word +0x9c
// has bit 0x2000000 (0x0047e709 / 0x0047e713) leaves at once (`pop edi; ret 4`, eax untouched: the only caller, Panel::notify, enters with eax = win, which this body returns; NULL returns 0). The
// array pointer is +0x224 and the count +0x22c (0x0047e715, 0x0047e722). The index of win in the array is searched from 0
// (loop 0x0047e728..0x0047e732, none when the count is <= 0, 0x0047e71e/0x0047e720); an index below the count
// (0x0047e734/0x0047e736) shifts entries 0..index-1 up by one (0x0047e73c..0x0047e74e, `jle` 0x0047e73a skips it for index 0)
// and stores win at [0] (0x0047e756). relayoutAll 0x0047e450 follows in every non-early path (0x0047e758); eax is its result.
int __fastcall BringToFront_re(void* self, void*, void* win) {
    if (win == 0) return 0;                                                       // 0x0047e707
    if (At<unsigned>(win, 0x9c) & 0x2000000) return reinterpret_cast<int>(win);   // 0x0047e713 (eax = the caller's: this of Panel::notify = win)
    int count = At<int>(self, 0x22c);                                             // 0x0047e715
    int idx = 0;
    if (count > 0) {                                                              // 0x0047e720
        void** arr = At<void**>(self, 0x224);                                     // 0x0047e722
        while (arr[idx] != win) {                                                 // 0x0047e728
            idx++;                                                                // 0x0047e72c
            if (idx >= count) break;                                              // 0x0047e730 / 0x0047e732
        }
    }
    if (idx < count) {                                                            // 0x0047e734 / 0x0047e736
        void** arr = At<void**>(self, 0x224);
        for (int i = idx; i > 0; i--) arr[i] = arr[i - 1];                        // 0x0047e73c..0x0047e74e
        At<void**>(self, 0x224)[0] = win;                                         // 0x0047e756
    }
    return kRelayoutAll(0);                                                       // 0x0047e758
}
Tp_t BringToFront_orig;
// 0x0047e700
SG_HOOK("golf_clean.exe", 0x0047e700, ChildArray_bringToFront, BringToFront_re, BringToFront_orig);

// 0x0047e580  zRaise(win): the same on the top-level array 0x0083a2d8 with count [0x0083ab94] (0x0047e595, 0x0047e5a1): a
// NULL win (0x0047e587) or flag bit 0x2000000 of +0x9c (0x0047e589) exits with eax untouched (its only caller, Panel::notify, enters with eax = win, which this body returns; NULL returns 0). A count
// <= 0 (0x0047e59f) or a win not in the array (loop 0x0047e5a6..0x0047e5b0) goes to relayoutAll 0x0047e450 with the array
// unchanged. A hit at index > 0 shifts the entries below it up (0x0047e5c1..0x0047e5d1) and stores win at [0x0083a2d8]
// (0x0047e5d3); index 0 stores it again. eax is relayoutAll's.
int __cdecl ZRaise_re(void* win) {
    if (win == 0) return 0;                                                       // 0x0047e587
    if (At<unsigned>(win, 0x9c) & 0x2000000) return reinterpret_cast<int>(win);   // 0x0047e589 / 0x0047e593 (eax = the caller's, see bringToFront)
    int count = G<int>(0x0083ab94);                                               // 0x0047e595
    void** arr = reinterpret_cast<void**>(0x0083a2d8);
    int idx = 0;
    if (count > 0) {                                                              // 0x0047e59f
        while (arr[idx] != win) {                                                 // 0x0047e5a6
            idx++;                                                                // 0x0047e5aa
            if (idx >= count) return kRelayoutAll(0);                             // 0x0047e5b0 / 0x0047e5b2
        }
        for (int i = idx; i > 0; i--) arr[i] = arr[i - 1];                        // 0x0047e5c1..0x0047e5d1
        arr[0] = win;                                                             // 0x0047e5d3
    }
    return kRelayoutAll(0);                                                       // 0x0047e5d9
}
Cp_t ZRaise_orig;
// 0x0047e580
SG_HOOK("golf_clean.exe", 0x0047e580, zRaise, ZRaise_re, ZRaise_orig);

// 0x0047ee70  Window::onCommand(this, a, b) (ret 8): net object ecx = [0x008400b8] (0x0047ee76). With it, a == [net + 0x54]
// (0x0047ee80) calls netBeginOp0 0x00497c20 with [net + 0x5c] when b is non-zero (0x0047ee8d..0x0047ee91) and the argument-free
// netOp0 0x00497cc0 when b is zero (0x0047eeb2); a == [net + 0x58] (0x0047ee99) the same with netBeginOp1 0x00497c70 and
// [net + 0x60] (0x0047eea6..0x0047eeaa). Every other case (and no net object): b == 0 (0x0047eebe..0x0047eec0) calls
// dispatchCommand 0x0047c970(a) on this (0x0047eec5); otherwise [0x0083ab88] = a (0x0047eed0) and dispatchAccel 0x0047c6c0(a)
// (0x0047eed5). eax = the callee's.
int __fastcall WindowOnCommand_re(void* self, void*, int a, int b) {
    void* net = G<void*>(0x008400b8);                                             // 0x0047ee76
    if (net != 0) {                                                               // 0x0047ee7c
        if (a == At<int>(net, 0x54)) {                                            // 0x0047ee80
            if (b != 0) return kNetBeginOp0(net, 0, At<int>(net, 0x5c));          // 0x0047ee91
            return kNetOp0(net, 0);                                               // 0x0047eeb2
        }
        if (a == At<int>(net, 0x58)) {                                            // 0x0047ee9c
            if (b != 0) return kNetBeginOp1(net, 0, At<int>(net, 0x60));          // 0x0047eeaa
            return kNetOp0(net, 0);                                               // 0x0047eeb2
        }
    }
    if (b == 0) return kDispatchCommand(self, 0, a);                              // 0x0047eec5
    G<int>(0x0083ab88) = a;                                                       // 0x0047eed0
    return kDispatchAccel(self, 0, a);                                            // 0x0047eed5
}
typedef int(__fastcall* Fwd2_t)(void*, void*, int, int);
Fwd2_t WindowOnCommand_orig;
// 0x0047ee70
SG_HOOK("golf_clean.exe", 0x0047ee70, Window_onCommand, WindowOnCommand_re, WindowOnCommand_orig);

// 0x0047f1b0  Window::setFocus(this, a1, a2, a3, a4) (ret 0x10): Panel::notify 0x0047e120 (call 0x0047f1b6); a parent at +0x130
// (0x0047f1bb..0x0047f1c3) gets its virtual slot +0x12c with this (0x0047f1c9, 0x0047f1ca). a4 != 0 (0x0047f1d4): [0x0083ab44]
// = this and [0x0083ab40] = 0 (0x0047f1d8, 0x0047f1de), else the reverse (0x0047f1e6, 0x0047f1ec); [0x0083ab48] = 0
// (0x0047f1fa). a1 != 0 and bit 0x200 of +0x9c clear (0x0047f200..0x0047f20e, `test ah, 2`) call 0x0047c010(a2, a3, a4)
// (pushes 0x0047f213..0x0047f215, call 0x0047f218); then 0x0047c430(a2, a3, a4) (0x0047f21d..0x0047f222), whose result is
// returned.
int __fastcall WindowSetFocus_re(void* self, void*, int a1, int a2, int a3, int a4) {
    kPanelNotify(self, 0);                                                        // 0x0047f1b6
    void* parent = At<void*>(self, 0x130);                                        // 0x0047f1bb
    if (parent != 0) vcall1(parent, 0x12c, reinterpret_cast<int>(self));          // 0x0047f1ca
    if (a4 != 0) {                                                                // 0x0047f1d4
        G<void*>(0x0083ab44) = self;                                              // 0x0047f1d8
        G<int>(0x0083ab40) = 0;                                                   // 0x0047f1de
    } else {
        G<void*>(0x0083ab40) = self;                                              // 0x0047f1e6
        G<int>(0x0083ab44) = 0;                                                   // 0x0047f1ec
    }
    G<int>(0x0083ab48) = 0;                                                       // 0x0047f1fa
    if (a1 != 0 && (At<unsigned>(self, 0x9c) & 0x200) == 0)                       // 0x0047f200..0x0047f211
        kWin47c010(self, 0, a2, a3, a4);                                          // 0x0047f218
    return kWin47c430(self, 0, a2, a3, a4);                                       // 0x0047f222
}
typedef int(__fastcall* Fwd4_t)(void*, void*, int, int, int, int);
Fwd4_t WindowSetFocus_orig;
// 0x0047f1b0
SG_HOOK("golf_clean.exe", 0x0047f1b0, Window_setFocus, WindowSetFocus_re, WindowSetFocus_orig);

// 0x00480c20  Window::refreshRect(this, a1, a2, a3, a4, a5) (ret 0x14): Window::visible 0x004801f0 (call 0x00480c26); zero
// returns zero (0x00480c2d `je` 0x00480c78). Otherwise a RECT {a1, a2, a1 + a3, a2 + a4} is built on the stack
// (0x00480c2f..0x00480c53; a5 is not read) and passed to 0x0047fa30 (this, rect; call 0x00480c5a), 0x0047b0d0 (this, rect;
// call 0x00480c66) and the plain drawTooltip 0x0047cdb0 (rect; call 0x00480c70), whose result is returned.
int __fastcall WindowRefreshRect_re(void* self, void*, int a1, int a2, int a3, int a4, int a5) {
    int vis = kVisible(self, 0);                                                  // 0x00480c26
    if (vis == 0) return vis;                                                     // 0x00480c2d
    int r[4];
    r[0] = a1;                                                                    // 0x00480c3b
    r[1] = a2;                                                                    // 0x00480c41
    r[2] = static_cast<int>(static_cast<unsigned>(a1) + static_cast<unsigned>(a3));   // 0x00480c3f
    r[3] = static_cast<int>(static_cast<unsigned>(a2) + static_cast<unsigned>(a4));   // 0x00480c4d
    kWin47fa30(self, 0, r);                                                       // 0x00480c5a
    kWin47b0d0(self, 0, r);                                                       // 0x00480c66
    return kDrawTooltip(r);                                                       // 0x00480c70
}
typedef int(__fastcall* Fwd5_t)(void*, void*, int, int, int, int, int);
Fwd5_t WindowRefreshRect_orig;
// 0x00480c20
SG_HOOK("golf_clean.exe", 0x00480c20, Window_refreshRect, WindowRefreshRect_re, WindowRefreshRect_orig);

// 0x0047f2f0  currentFocusOwner(): esi = [0x0083ab54] (0x0047f2f1). Non-zero: [0x0083b990] when non-zero replaces it
// (0x0047f2fb..0x0047f304). Zero: [0x0083ab38], and when that is zero [0x0083ab60] (0x0047f308..0x0047f312). The chosen
// window is stored in [0x0083ab3c] (0x0047f31a). NULL returns 0 (0x0047f320 `je` 0x0047f339 with eax = esi = 0). Otherwise
// esi = [esi + 0xb0] (0x0047f322); NULL returns 0; else Window::visible(esi) (call 0x0047f32e): non-zero returns esi, zero
// returns 0 (the ret at 0x0047f338 with eax = visible's 0).
int __cdecl CurrentFocusOwner_re() {
    void* w = G<void*>(0x0083ab54);                                               // 0x0047f2f1
    if (w != 0) {
        void* alt = G<void*>(0x0083b990);                                         // 0x0047f2fb
        if (alt != 0) w = alt;                                                    // 0x0047f304
    } else {
        w = G<void*>(0x0083ab38);                                                 // 0x0047f308
        if (w == 0) w = G<void*>(0x0083ab60);                                     // 0x0047f312
    }
    G<void*>(0x0083ab3c) = w;                                                     // 0x0047f31a
    if (w == 0) return 0;                                                         // 0x0047f320
    void* target = At<void*>(w, 0xb0);                                            // 0x0047f322
    if (target == 0) return 0;                                                    // 0x0047f328
    if (kVisible(target, 0) == 0) return 0;                                       // 0x0047f32e..0x0047f335
    return reinterpret_cast<int>(target);                                         // 0x0047f339
}
C0_t CurrentFocusOwner_orig;
// 0x0047f2f0
SG_HOOK("golf_clean.exe", 0x0047f2f0, currentFocusOwner, CurrentFocusOwner_re, CurrentFocusOwner_orig);

// 0x00495990  uiKeyEvent148(a1) (ret 4): currentFocusOwner 0x0047f2f0 (call 0x00495990); a non-NULL owner gets its virtual
// slot +0x148 with a1 (`mov edx, [eax]` 0x0049599d, push 0x0049599f, call [edx + 0x148] 0x004959a2); eax = that result, or
// 0 for no owner.
int __stdcall UiKeyEvent148_re(int a1) {
    void* o = reinterpret_cast<void*>(kCurrentFocusOwner());                      // 0x00495990
    if (o == 0) return 0;                                                         // 0x00495997
    return vcall1(o, 0x148, a1);                                                  // 0x004959a2
}
S1_t UiKeyEvent148_orig;
// 0x00495990
SG_HOOK("golf_clean.exe", 0x00495990, uiKeyEvent148, UiKeyEvent148_re, UiKeyEvent148_orig);

// 0x00495910  uiKeyEvent(a1, a2) (ret 8): GetKeyState [0x4ba1a8] for VK 0x10, 0x11, 0x12 (calls 0x0049591a, 0x0049592d,
// 0x0049593c); bit 15 of each (`test ah, 0x80`) adds 0x10000 / 0x20000 / 0x40000 to a1 (0x00495925, 0x00495934,
// 0x00495943). currentFocusOwner (call 0x00495949): an owner gets virtual slot +0x144 with (a1 | modifiers, a2)
// (pushes 0x00495958, 0x00495959, call 0x0049595c). Then, when a2 != 0 (0x0049596b) and the pointer [0x0083ab00] is
// non-NULL (0x00495962), that cdecl callback runs with the modifier-extended a1 (0x0049596f..0x00495970); finally a
// non-NULL [0x0083aafc] (0x00495975..0x0049597b) runs with no arguments and its result is returned, otherwise 0.
typedef int(__stdcall* GetKeyState_t)(int vk);
typedef int(__cdecl* Cb1_t)(int a);
int __stdcall UiKeyEvent_re(int a1, int a2) {
    int f = a1;                                                                   // 0x0049591c
    if (Imp<GetKeyState_t>(0x004ba1a8)(0x10) & 0x8000) f |= 0x10000;              // 0x0049591a..0x00495925
    if (Imp<GetKeyState_t>(0x004ba1a8)(0x11) & 0x8000) f |= 0x20000;              // 0x0049592d..0x00495934
    if (Imp<GetKeyState_t>(0x004ba1a8)(0x12) & 0x8000) f |= 0x40000;              // 0x0049593c..0x00495943
    void* o = reinterpret_cast<void*>(kCurrentFocusOwner());                      // 0x00495949
    if (o != 0) vcall2(o, 0x144, f, a2);                                          // 0x0049595c
    Cb1_t cb = G<Cb1_t>(0x0083ab00);                                              // 0x00495962
    if (cb != 0 && a2 != 0) cb(f);                                                // 0x0049596f
    C0_t cb2 = G<C0_t>(0x0083aafc);                                               // 0x00495975
    if (cb2 == 0) return 0;                                                       // 0x0049597b
    return cb2();                                                                 // 0x00495980
}
typedef int(__stdcall* S2_t)(int, int);
S2_t UiKeyEvent_orig;
// 0x00495910
SG_HOOK("golf_clean.exe", 0x00495910, uiKeyEvent, UiKeyEvent_re, UiKeyEvent_orig);

// 0x00477e60  drawBox(a1, a2, a3, a4, a5, a6) (ret 0x18): a RECT {a2, a3, a2 + a4, a3 + a5} (stores 0x00477e70, 0x00477e7e,
// 0x00477e76, 0x00477e84) and the call 0x00477eb0(a1, &rect, a6) (pushes 0x00477e8c, 0x00477e95, 0x00477e96, call
// 0x00477e97, ret 0xc); eax = its result.
typedef int(__stdcall* DrawRect_t)(int a, int* rect, int c);
int __stdcall DrawBox_re(int a1, int a2, int a3, int a4, int a5, int a6) {
    int r[4];
    r[0] = a2;                                                                    // 0x00477e70
    r[1] = a3;                                                                    // 0x00477e7e
    r[2] = static_cast<int>(static_cast<unsigned>(a2) + static_cast<unsigned>(a4));   // 0x00477e76
    r[3] = static_cast<int>(static_cast<unsigned>(a3) + static_cast<unsigned>(a5));   // 0x00477e84
    return reinterpret_cast<DrawRect_t>(0x00477eb0)(a1, r, a6);                   // 0x00477e97
}
typedef int(__stdcall* S6_t)(int, int, int, int, int, int);
S6_t DrawBox_orig;
// 0x00477e60
SG_HOOK("golf_clean.exe", 0x00477e60, drawBox, DrawBox_re, DrawBox_orig);

// 0x004785e0  TextView_beginMeasure(this, a1, a2) (ret 8): +0x1c = 0 (0x004785e5), +0x18 = +0x14 (0x004785e0, 0x004785e8),
// +0x3c = 0 (0x004785ef), [0x00839aa0] = 0 (0x004785f2), [0x00839aa8] = 0 (0x004785f7), [0x00839aa4] = a2 (0x00478600); then
// the text worker 0x00478610(a1) (push 0x00478606, call 0x00478607, ret 4); eax = its result.
int __fastcall TextViewBeginMeasure_re(void* self, void*, int a1, int a2) {
    At<int>(self, 0x1c) = 0;                                                      // 0x004785e5
    At<int>(self, 0x18) = At<int>(self, 0x14);                                    // 0x004785e8
    At<int>(self, 0x3c) = 0;                                                      // 0x004785ef
    G<int>(0x00839aa0) = 0;                                                       // 0x004785f2
    G<int>(0x00839aa8) = 0;                                                       // 0x004785f7
    G<int>(0x00839aa4) = a2;                                                      // 0x00478600
    return kTextWorker(self, 0, a1);                                              // 0x00478607
}
Fwd2_t TextViewBeginMeasure_orig;
// 0x004785e0
SG_HOOK("golf_clean.exe", 0x004785e0, TextView_beginMeasure, TextViewBeginMeasure_re, TextViewBeginMeasure_orig);

// ====================================================================================================================
// Display glue ([0x0083ad50] is the graphics device object; its virtual slots are read from the instructions)
// ====================================================================================================================

namespace {
static inline int dslot0(void* d, unsigned off) {
    return reinterpret_cast<T0_t>(*reinterpret_cast<void**>(*reinterpret_cast<char**>(d) + off))(d, 0);
}
typedef int(__stdcall* FreeLibrary_t)(void* h);
typedef int(__fastcall* T5_t)(void* ecx, void* edx, int a, int b, int c, int d);
const T0_t kCache483070 = reinterpret_cast<T0_t>(0x00483070);
const C0_t kResetScreenList = reinterpret_cast<C0_t>(0x00483340);
const T5_t kDrawText = reinterpret_cast<T5_t>(0x00477c30);             // drawText(text, x, y, len), ret 0x10
const T0_t kWidgetValue2 = reinterpret_cast<T0_t>(0x00477580);
typedef int(__fastcall* T4_t)(void* ecx, void* edx, int a, int b, int c, int d);
const T4_t kSetDrawTarget = reinterpret_cast<T4_t>(0x004762d0);        // ret 0x10
typedef int(__cdecl* C4_t)(int, int, int, int);
const C4_t kDrawStringAt = reinterpret_cast<C4_t>(0x00404b70);
}  // namespace

// 0x00483b10  clearPrimarySurface: a display (0x00483b10 `mov ecx, [0x0083ad50]`, 0x00483b16 test) takes its virtual slot
// +0xbc (0x00483b1c, no arguments); then [0x0083ad44] = 0 (0x00483b22). eax = the slot's result; without a display eax is
// untouched (the caller's): the naked entry hands the entry eax to the body, so a caller that passes it on sees the same value.
static int __cdecl ClearPrimarySurfaceBody(int eaxIn) {
    int r = eaxIn;
    void* d = G<void*>(0x0083ad50);                                               // 0x00483b10
    if (d != 0) r = dslot0(d, 0xbc);                                              // 0x00483b1c
    G<int>(0x0083ad44) = 0;                                                       // 0x00483b22
    return r;
}
__declspec(naked) int __cdecl ClearPrimarySurface_re() {
    __asm {
        push eax
        call ClearPrimarySurfaceBody
        add esp, 4
        ret
    }
}
C0_t ClearPrimarySurface_orig;
// 0x00483b10
SG_HOOK("golf_clean.exe", 0x00483b10, clearPrimarySurface, ClearPrimarySurface_re, ClearPrimarySurface_orig);

// 0x00483320  resetScreenState: the list reset 0x00483340 (call 0x00483320), then display virtual slot +0xc0 with
// (0x0083acb0, 0x0083ac30) pushed in that order (0x0048332b, 0x00483330, call 0x0048333d: the slot pops them); eax = its
// result. The display is not tested for NULL.
int __cdecl ResetScreenState_re() {
    kResetScreenList();                                                           // 0x00483320
    void* d = G<void*>(0x0083ad50);                                               // 0x00483325
    return reinterpret_cast<T2_t>(*reinterpret_cast<void**>(*reinterpret_cast<char**>(d) + 0xc0))(
        d, 0, 0x0083acb0, 0x0083ac30);                                            // 0x0048333d
}
C0_t ResetScreenState_orig;
// 0x00483320
SG_HOOK("golf_clean.exe", 0x00483320, resetScreenState, ResetScreenState_re, ResetScreenState_orig);

// 0x00483da0  pumpInputDisplay483da0: [0x0083ad4c] |= 0x3f (0x00483da0..0x00483da9); a display takes virtual slot +0x150
// (0x00483dbb, no arguments); [0x0083ad4c] = 0 (0x00483dc1). eax = the slot's result; without a display eax is untouched
// (Timer::stop 0x00486ec0 reaches this function by a tail jump and passes its own eax through): the naked entry hands the entry
// eax to the body.
static int __cdecl PumpInputDisplayBody(int eaxIn) {
    G<unsigned>(0x0083ad4c) |= 0x3f;                                              // 0x00483da9
    int r = eaxIn;
    void* d = G<void*>(0x0083ad50);                                               // 0x00483daf
    if (d != 0) r = dslot0(d, 0x150);                                             // 0x00483dbb
    G<int>(0x0083ad4c) = 0;                                                       // 0x00483dc1
    return r;
}
__declspec(naked) int __cdecl PumpInputDisplay_re() {
    __asm {
        push eax
        call PumpInputDisplayBody
        add esp, 4
        ret
    }
}
C0_t PumpInputDisplay_orig;
// 0x00483da0
SG_HOOK("golf_clean.exe", 0x00483da0, pumpInputDisplay483da0, PumpInputDisplay_re, PumpInputDisplay_orig);

// 0x00483030  Cache483030::attach(this): Cache483070 (call 0x00483033); the display's virtual slot +0x78 with this
// (push 0x0048303e, call 0x00483041; the display is not tested) gives an object stored in +4 (0x00483044), whose virtual
// slot +4 is then called (`mov edx, [eax]` 0x00483047, `call [edx + 4]` 0x0048304b, ecx = the object). eax = 0 at the ret.
int __fastcall CacheAttach_re(void* self, void*) {
    kCache483070(self, 0);                                                        // 0x00483033
    void* d = G<void*>(0x0083ad50);                                               // 0x00483038
    void* o = reinterpret_cast<void*>(vcall1(d, 0x78, reinterpret_cast<int>(self)));   // 0x00483041
    At<void*>(self, 4) = o;                                                       // 0x00483044
    dslot0(o, 4);                                                                 // 0x0048304b
    return 0;                                                                     // 0x0048304e
}
T0_t CacheAttach_orig;
// 0x00483030
SG_HOOK("golf_clean.exe", 0x00483030, Cache483030_attach, CacheAttach_re, CacheAttach_orig);

// 0x004a0060  freeGraphsyObject: a display takes virtual slot +0xc (0x004a006c, no arguments); eax = [0x0084092c]
// (0x004a006f); non-zero: FreeLibrary [0x4ba0ac] (push 0x004a0078, call 0x004a0079) and [0x0084092c] = 0 (0x004a007f);
// [0x0083ad50] = 0 (0x004a0089). eax = FreeLibrary's result, or 0 when the handle was 0.
int __cdecl FreeGraphsyObject_re() {
    void* d = G<void*>(0x0083ad50);                                               // 0x004a0060
    if (d != 0) dslot0(d, 0xc);                                                   // 0x004a006c
    void* h = G<void*>(0x0084092c);                                               // 0x004a006f
    int r = 0;
    if (h != 0) {                                                                 // 0x004a0076
        r = Imp<FreeLibrary_t>(0x004ba0ac)(h);                                    // 0x004a0079
        G<int>(0x0084092c) = 0;                                                   // 0x004a007f
    }
    G<int>(0x0083ad50) = 0;                                                       // 0x004a0089
    return r;
}
C0_t FreeGraphsyObject_orig;
// 0x004a0060
SG_HOOK("golf_clean.exe", 0x004a0060, freeGraphsyObject, FreeGraphsyObject_re, FreeGraphsyObject_orig);

// 0x0048e120  TextView::setFontSize(this, size) (ret 4). The scaled branch needs +0x590 == 0 (0x0048e123..0x0048e12b), bit
// 0x400 of +0x1f68 clear (`test ah, 4` 0x0048e133), a display (0x0048e138..0x0048e13e) whose virtual slot +0xa8 returns >= 0x400
// (signed, 0x0048e144..0x0048e14f): then value = size * 3 / 2, truncating toward zero (0x0048e15b..0x0048e164). Otherwise
// value = size (0x0048e17b). Either way it is stored at [[this + 0x1464] + 8 + this + 0x1494] (0x0048e161..0x0048e166,
// 0x0048e17b..0x0048e17e). eax = the stored value in the scaled branch, [this + 0x1464] in the other.
int __fastcall TextViewSetFontSize_re(void* self, void*, int size) {
    bool scaled = false;
    if (At<int>(self, 0x590) == 0 && (At<unsigned>(self, 0x1f68) & 0x400) == 0) {     // 0x0048e12b, 0x0048e133
        void* d = G<void*>(0x0083ad50);                                           // 0x0048e138
        if (d != 0 && dslot0(d, 0xa8) >= 0x400) scaled = true;                    // 0x0048e144, 0x0048e14a
    }
    void* q = At<void*>(self, 0x1464);                                            // 0x0048e155 / 0x0048e171
    int off = At<int>(q, 8);                                                      // 0x0048e161 / 0x0048e17b
    if (scaled) {
        int v = static_cast<int>(static_cast<unsigned>(size) * 3u);               // 0x0048e15b
        v = v / 2;                                                                // cdq; sub eax, edx; sar eax, 1
        At<int>(self, off + 0x1494) = v;                                          // 0x0048e166
        return v;
    }
    At<int>(self, off + 0x1494) = size;                                           // 0x0048e17e
    return reinterpret_cast<int>(q);                                              // 0x0048e171
}
T1_t TextViewSetFontSize_orig;
// 0x0048e120
SG_HOOK("golf_clean.exe", 0x0048e120, TextView_setFontSize, TextViewSetFontSize_re, TextViewSetFontSize_orig);

// 0x00478970  Text_flush(this): [0x00839658] is a NUL-terminated line buffer (byte 0 tested at 0x00478970/0x00478979). A non-empty
// buffer is measured (repne scasb 0x00478990) and drawn through drawText 0x00477c30(this; buffer, [0x00839a9c], [0x00839aa0],
// length) (pushes 0x0047899b..0x004789a4, call 0x004789ab); then widgetValue 0x00477580 (call 0x004789b3) is added to
// [0x00839aa0] (0x004789b8..0x004789c0) and +0x1c is incremented (0x004789c6..0x004789ca). Always: [0x00839658] = 0
// (0x004789cd), [0x00839aa4] = 0 (0x004789d3), +0x30, +0x38, +0x34, +0x3c = 0 (0x004789d9..0x004789e2). eax = [0x00839aa0].
int __fastcall TextFlush_re(void* self, void*) {
    char* buf = reinterpret_cast<char*>(0x00839658);
    if (buf[0] != 0) {                                                            // 0x00478979 / 0x0047897d
        int len = 0;
        while (buf[len] != 0) len++;                                              // 0x00478990
        kDrawText(self, 0, reinterpret_cast<int>(buf), G<int>(0x00839a9c), G<int>(0x00839aa0), len);   // 0x004789ab
        int w = kWidgetValue2(self, 0);                                           // 0x004789b3
        G<int>(0x00839aa0) = static_cast<int>(static_cast<unsigned>(G<int>(0x00839aa0)) + static_cast<unsigned>(w));   // 0x004789c0
        At<int>(self, 0x1c) = At<int>(self, 0x1c) + 1;                            // 0x004789ca
    }
    buf[0] = 0;                                                                   // 0x004789cd
    G<int>(0x00839aa4) = 0;                                                       // 0x004789d3
    At<int>(self, 0x30) = 0;                                                      // 0x004789d9
    At<int>(self, 0x38) = 0;                                                      // 0x004789dc
    At<int>(self, 0x34) = 0;                                                      // 0x004789df
    At<int>(self, 0x3c) = 0;                                                      // 0x004789e2
    return G<int>(0x00839aa0);                                                    // 0x004789e5
}
T0_t TextFlush_orig;
// 0x00478970
SG_HOOK("golf_clean.exe", 0x00478970, Text_flush, TextFlush_re, TextFlush_orig);

// 0x00432620  drawPanelBackdrop(a1, a2) (cdecl): setDrawTarget 0x004762d0 on ecx = [0x004c1570] with (0x00519fd8, 0, 0, 0)
// (pushes 0x00432628..0x0043262e, call 0x00432633); len = strlen of the text buffer 0x0051a068 (0x00432638..0x0043264a),
// w = len * 3 (0x0043264b); half = a1 raised to w when below it (0x0043264e..0x00432652, signed) and lowered to 0x320 - w when
// above it (0x00432654..0x0043265f, signed); drawMapLine 0x004493d0(half - w, a2, w + half, a2, 0x80000000, 0xa, 5)
// (0x00432665..0x00432679), then drawStringAt 0x00404b70(0x0051a068, half, a2 - 5, 0x80007fff) (0x00432681..0x0043268d, cdecl,
// add esp 0x2c). eax = drawStringAt's.
int __cdecl DrawPanelBackdrop_re(int a1, int a2) {
    kSetDrawTarget(G<void*>(0x004c1570), 0, 0x00519fd8, 0, 0, 0);                 // 0x00432633
    const char* text = reinterpret_cast<const char*>(0x0051a068);
    int len = 0;
    while (text[len] != 0) len++;                                                 // 0x00432646
    int w = len * 3;                                                              // 0x0043264b
    int half = a1;                                                                // 0x00432642
    if (half < w) half = w;                                                       // 0x0043264e / 0x00432650
    int cap = 0x320 - w;                                                          // 0x00432659
    if (half > cap) half = cap;                                                   // 0x0043265b / 0x0043265f
    reinterpret_cast<C7_t>(0x004493d0)(half - w, a2, w + half, a2, static_cast<int>(0x80000000), 0xa, 5);   // 0x00432679
    return kDrawStringAt(0x0051a068, half, a2 - 5, static_cast<int>(0x80007fff));   // 0x0043268d
}
typedef int(__cdecl* C2_t)(int, int);
C2_t DrawPanelBackdrop_orig;
// 0x00432620
SG_HOOK("golf_clean.exe", 0x00432620, drawPanelBackdrop, DrawPanelBackdrop_re, DrawPanelBackdrop_orig);

// ====================================================================================================================
// File / CRT glue (the CRT's stdio and low-level I/O are statically linked into the exe: fgets 0x004a65ee, fprintf 0x004a622c,
// fopen 0x004a614d, fclose 0x004a609f, fwrite 0x004a6268, fflush 0x004a9943, __read 0x004a583a, __write 0x004a5b58)
// ====================================================================================================================

namespace {
typedef char*(__cdecl* Fgets_t)(char* buf, int n, void* fp);
typedef int(__cdecl* Fwrite_t)(const void* buf, int size, int n, void* fp);
typedef int(__cdecl* Fflush_t)(void* fp);
typedef int(__cdecl* Fopen_t)(const char* path, const char* mode);
typedef int(__cdecl* Rw_t)(int fd, void* buf, int n);
typedef int(__cdecl* Cs1_t)(void* p);
typedef int(__cdecl* Format_t)(void* cinfo, char* buf);
typedef int(__cdecl* Fprintf3_t)(void* stream, const char* fmt, const char* arg);
typedef int(__cdecl* Cs2_t)(void* a, void* b);
typedef int(__fastcall* Find_t)(void* ecx, void* edx, int a, int b);
const Fgets_t kFgets = reinterpret_cast<Fgets_t>(0x004a65ee);
const Cs1_t kClean4925d0 = reinterpret_cast<Cs1_t>(0x004925d0);
const Cs1_t kClean4925b0 = reinterpret_cast<Cs1_t>(0x004925b0);
const Fwrite_t kFwrite = reinterpret_cast<Fwrite_t>(0x004a6268);
const Fflush_t kFflush = reinterpret_cast<Fflush_t>(0x004a9943);
typedef int(__fastcall* T2s_t)(void* ecx, void* edx, int a, int b);
typedef void(__cdecl* Free_t)(void*);
const Free_t kFree = reinterpret_cast<Free_t>(0x004a5007);
}  // namespace

// 0x00488230  Reader::readLine(this): the FILE at +0x158 with bit 0x10 set in its flag word +0xc (`test byte ptr [eax + 0xc], 0x10`
// 0x00488239) ends the line buffer +0x15c with a NUL (0x00488246) and returns 0 (0x00488249). Otherwise fgets 0x004a65ee
// (buffer, 0x1ff, FILE) (pushes 0x00488252..0x00488258, call 0x00488259): NULL ends the buffer (0x0048826b) and returns it
// (0x0048826d); a line goes through 0x004925d0 and 0x004925b0 (calls 0x0048827c, 0x00488288), +0x154 takes the buffer pointer
// (0x00488296) and the buffer is returned.
int __fastcall ReaderReadLine_re(void* self, void*) {
    void* fp = At<void*>(self, 0x158);                                            // 0x00488233
    char* buf = At<char*>(self, 0x15c);
    if (At<unsigned char>(fp, 0xc) & 0x10) {                                      // 0x00488239 / 0x0048823d
        buf[0] = 0;                                                               // 0x00488246
        return 0;                                                                 // 0x00488249
    }
    if (kFgets(At<char*>(self, 0x15c), 0x1ff, fp) == 0) {                         // 0x00488259 / 0x00488261
        At<char*>(self, 0x15c)[0] = 0;                                            // 0x0048826b
        return reinterpret_cast<int>(At<char*>(self, 0x15c));                     // 0x0048826d
    }
    kClean4925d0(At<char*>(self, 0x15c));                                         // 0x0048827c
    kClean4925b0(At<char*>(self, 0x15c));                                         // 0x00488288
    At<char*>(self, 0x154) = At<char*>(self, 0x15c);                              // 0x00488296
    return reinterpret_cast<int>(At<char*>(self, 0x15c));                         // 0x0048828d
}
T0_t ReaderReadLine_orig;
// 0x00488230
SG_HOOK("golf_clean.exe", 0x00488230, Reader_readLine, ReaderReadLine_re, ReaderReadLine_orig);

// 0x00487e90  readLineGlobal(): ecx = the global reader 0x0083b000 (0x00487e90) and a tail jmp to Reader::readLine 0x00488230
// (0x00487e95); eax = its result.
int __cdecl ReadLineGlobal_re() {
    return reinterpret_cast<T0_t>(0x00488230)(reinterpret_cast<void*>(0x0083b000), 0);   // 0x00487e95
}
C0_t ReadLineGlobal_orig;
// 0x00487e90
SG_HOOK("golf_clean.exe", 0x00487e90, readLineGlobal, ReadLineGlobal_re, ReadLineGlobal_orig);

// 0x00487e70  readSectionGlobal(a1, a2) (cdecl): Reader::open 0x00487fb0 on the global reader 0x0083b000 with (a1, a2) (pushes
// 0x00487e78, 0x00487e79, ecx 0x00487e7a, call 0x00487e7f, ret 8 in the callee); eax = its result.
int __cdecl ReadSectionGlobal_re(int a1, int a2) {
    return reinterpret_cast<T2s_t>(0x00487fb0)(reinterpret_cast<void*>(0x0083b000), 0, a1, a2);   // 0x00487e7f
}
C2_t ReadSectionGlobal_orig;
// 0x00487e70
SG_HOOK("golf_clean.exe", 0x00487e70, readSectionGlobal, ReadSectionGlobal_re, ReadSectionGlobal_orig);

// 0x004ae590  term_destination(cinfo) (cdecl; the IJG stdio destination): dest = [cinfo + 0x14] (0x004ae59c); the unwritten
// count is 0x1000 - [dest + 4] (0x004ae597, 0x004ae59f); a non-zero count is written with fwrite 0x004a6268 (buffer [dest + 0x18],
// 1, count, FILE [dest + 0x14]) (pushes 0x004ae5aa..0x004ae5ae, call 0x004ae5af); a short count sets err->msg_code ([err + 0x14])
// = 0x24 (0x004ae5be) and calls err->error_exit, the pointer at [err + 0] (0x004ae5c7) with cinfo. fflush 0x004a9943 on the FILE
// (call 0x004ae5d0); a FILE with bit 0x20 of its flag word +0xc (0x004ae5db) sets msg_code = 0x24 again (0x004ae5e4) and calls
// error_exit (0x004ae5ed). eax = fflush's result, or error_exit's when the last test failed.
int __cdecl TermDestination_re(void* cinfo) {
    char* dest = At<char*>(cinfo, 0x14);                                          // 0x004ae59c
    int count = 0x1000 - At<int>(dest, 4);                                        // 0x004ae59f
    typedef int(__cdecl * ErrExit_t)(void*);
    if (count != 0) {                                                             // 0x004ae5a2
        int n = kFwrite(At<void*>(dest, 0x18), 1, count, At<void*>(dest, 0x14));  // 0x004ae5af
        if (n != count) {                                                         // 0x004ae5b7 / 0x004ae5b9
            char* err = At<char*>(cinfo, 0);
            At<int>(err, 0x14) = 0x24;                                            // 0x004ae5be
            At<ErrExit_t>(At<char*>(cinfo, 0), 0)(cinfo);                         // 0x004ae5c7
        }
    }
    int r = kFflush(At<void*>(dest, 0x14));                                       // 0x004ae5d0
    if (At<unsigned char>(At<void*>(dest, 0x14), 0xc) & 0x20) {                   // 0x004ae5db
        At<int>(At<char*>(cinfo, 0), 0x14) = 0x24;                                // 0x004ae5e4
        r = At<ErrExit_t>(At<char*>(cinfo, 0), 0)(cinfo);                         // 0x004ae5ed
    }
    return r;
}
Cp_t TermDestination_orig;
// 0x004ae590
SG_HOOK("golf_clean.exe", 0x004ae590, term_destination, TermDestination_re, TermDestination_orig);

// 0x004aecf0  output_message(cinfo) (cdecl; the IJG message printer): err = [cinfo] (0x004aecfe); err->format_message, the pointer at
// [err + 0xc] (0x004aed02), is called with (cinfo, buffer) (pushes 0x004aed00, 0x004aed01) into a 200-byte stack buffer
// (sub esp, 0xc8 0x004aecf4); fprintf 0x004a622c (0x004e4dc8, 0x004e6cf8, buffer) (pushes 0x004aed09..0x004aed0f, call
// 0x004aed14); eax = fprintf's.
int __cdecl OutputMessage_re(void* cinfo) {
    char buf[200];                                                                // 0x004aecf4
    At<Format_t>(At<char*>(cinfo, 0), 0xc)(cinfo, buf);                           // 0x004aed02
    return reinterpret_cast<Fprintf3_t>(0x004a622c)(reinterpret_cast<void*>(0x004e4dc8), reinterpret_cast<const char*>(0x004e6cf8), buf);   // 0x004aed14
}
Cp_t OutputMessage_orig;
// 0x004aecf0
SG_HOOK("golf_clean.exe", 0x004aecf0, output_message, OutputMessage_re, OutputMessage_orig);

// 0x0040af70  saveFieldIo(buf, n) (cdecl): fd = [0x00568d08] (0x0040af79); a non-zero mode [0x0053e638] (0x0040af70, 0x0040af7f)
// calls __read 0x004a583a(fd, buf, n) (call 0x0040af8a), zero calls __write 0x004a5b58(fd, buf, n) (call 0x0040af95); eax = the call's.
int __cdecl SaveFieldIo_re(void* buf, int n) {
    int fd = G<int>(0x00568d08);                                                  // 0x0040af79
    if (G<int>(0x0053e638) != 0) return reinterpret_cast<Rw_t>(0x004a583a)(fd, buf, n);   // 0x0040af8a
    return reinterpret_cast<Rw_t>(0x004a5b58)(fd, buf, n);                        // 0x0040af93
}
typedef int(__cdecl* Cpi_t)(void*, int);
Cpi_t SaveFieldIo_orig;
// 0x0040af70
SG_HOOK("golf_clean.exe", 0x0040af70, saveFieldIo, SaveFieldIo_re, SaveFieldIo_orig);

// 0x00432170  flushCourseInfo(): a non-zero first byte of the course-info buffer 0x00587da0 (0x00432170, 0x00432175) runs
// writeCourseInfo 0x00431fa0(0x00587da0) (call 0x0043217e) and saveThumbAndFull 0x00431ee0(0x00587da0) (call 0x00432188), then
// clears that byte (0x00432190). Without it nothing runs. (eax at the ret is not defined by the body: the registry compares
// memory only.)
void __cdecl FlushCourseInfo_re() {
    if (G<unsigned char>(0x00587da0) != 0) {                                      // 0x00432177
        reinterpret_cast<Cs1_t>(0x00431fa0)(reinterpret_cast<void*>(0x00587da0)); // 0x0043217e
        reinterpret_cast<Cs1_t>(0x00431ee0)(reinterpret_cast<void*>(0x00587da0)); // 0x00432188
        G<unsigned char>(0x00587da0) = 0;                                         // 0x00432190
    }
}
V0_t FlushCourseInfo_orig;
// 0x00432170
SG_HOOK("golf_clean.exe", 0x00432170, flushCourseInfo, FlushCourseInfo_re, FlushCourseInfo_orig);

// 0x004a00f0  resolveAndOpenFile(path, mode) (cdecl): resolveDataPath 0x00491da0(path) (call 0x004a00f6); a zero result keeps
// the original path (0x004a0102); fopen 0x004a614d(that path, mode) (pushes 0x004a0108, 0x004a0109, call 0x004a010a); eax = its result.
int __cdecl ResolveAndOpenFile_re(const char* path, const char* mode) {
    const char* p = reinterpret_cast<const char*>(reinterpret_cast<Cs1_t>(0x00491da0)(const_cast<char*>(path)));   // 0x004a00f6
    if (p == 0) p = path;                                                         // 0x004a0100 / 0x004a0102
    return reinterpret_cast<Fopen_t>(0x004a614d)(p, mode);                        // 0x004a010a
}
Cs2_t ResolveAndOpenFile_orig;
// 0x004a00f0
SG_HOOK("golf_clean.exe", 0x004a00f0, resolveAndOpenFile, ResolveAndOpenFile_re, ResolveAndOpenFile_orig);

// 0x004a0320  logLine(a1, a2, a3, a4, a5) (cdecl): the five arguments are pushed in order (0x004a032c..0x004a0338) behind
// ecx = the logger 0x00840930 (0x004a0339) for logWrite 0x004a0280 (call 0x004a033e, ret 0x14); eax = its result.
typedef int(__fastcall* T5x_t)(void* ecx, void* edx, int a, int b, int c, int d, int e);
int __cdecl LogLine_re(int a1, int a2, int a3, int a4, int a5) {
    return reinterpret_cast<T5x_t>(0x004a0280)(reinterpret_cast<void*>(0x00840930), 0, a1, a2, a3, a4, a5);   // 0x004a033e
}
typedef int(__cdecl* C5i_t)(int, int, int, int, int);
C5i_t LogLine_orig;
// 0x004a0320
SG_HOOK("golf_clean.exe", 0x004a0320, logLine, LogLine_re, LogLine_orig);

// 0x00488420  KeyTable::lookup(a1, a2) (cdecl): four 0x120-byte tables from 0x0083b170 (loop 0x00488430..0x00488453, end
// 0x0083b5f0). A table whose word +0x104 is non-zero (0x00488430..0x00488438) is asked KeyTable::find 0x00488310(a1, a2) (pushes
// 0x0048843a, 0x0048843b, call 0x0048843e, ret 8 in the callee); a non-negative result (0x00488445 `jge`) is returned.
// No table answers: -1 (0x00488455).
int __cdecl KeyTableLookup_re(int a1, int a2) {
    for (unsigned t = 0x0083b170; t < 0x0083b5f0; t += 0x120) {                   // 0x0048842b / 0x00488447..0x00488453
        if (G<int>(t + 0x104) == 0) continue;                                     // 0x00488436
        int r = reinterpret_cast<T2s_t>(0x00488310)(reinterpret_cast<void*>(t), 0, a1, a2);   // 0x0048843e
        if (r >= 0) return r;                                                     // 0x00488445
    }
    return -1;                                                                    // 0x00488455
}
C2_t KeyTableLookup_orig;
// 0x00488420
SG_HOOK("golf_clean.exe", 0x00488420, KeyTable_lookup, KeyTableLookup_re, KeyTableLookup_orig);

// 0x00490c30  MsgBox::freeLabels(): [0x0083b9b4] = 0 and [0x0083b9c4] = 0 (0x00490c3a, 0x00490c40); a non-zero [0x0083b994]
// (0x00490c30, 0x00490c38) goes to the CRT free 0x004a5007 (call 0x00490c49) and is cleared (0x00490c51); the same for
// [0x0083b998] (0x00490c57..0x00490c69). (eax at the ret is free's: the registry compares memory only.)
void __cdecl MsgBoxFreeLabels_re() {
    G<int>(0x0083b9b4) = 0;                                                       // 0x00490c3a
    G<int>(0x0083b9c4) = 0;                                                       // 0x00490c40
    if (G<void*>(0x0083b994) != 0) {                                              // 0x00490c46
        kFree(G<void*>(0x0083b994));                                              // 0x00490c49
        G<int>(0x0083b994) = 0;                                                   // 0x00490c51
    }
    if (G<void*>(0x0083b998) != 0) {                                              // 0x00490c5e
        kFree(G<void*>(0x0083b998));                                              // 0x00490c61
        G<int>(0x0083b998) = 0;                                                   // 0x00490c69
    }
}
V0_t MsgBoxFreeLabels_orig;
// 0x00490c30
SG_HOOK("golf_clean.exe", 0x00490c30, MsgBox_freeLabels, MsgBoxFreeLabels_re, MsgBoxFreeLabels_orig);

// 0x0046d040  initGolferState(): the byte at +0xd9 of the 152 golfer records (0x100 each from 0x005794d9, loop 0x0046d045..0x0046d052)
// is cleared; [0x0056a51c] = [0x0059ae7c] = 0 (0x0046d059, 0x0046d05e). For the 19 course records 0x00577f40 down to 0x00575ab0
// in steps of 0x208 (0x0046d0ad..0x0046d0b4; bl counts 0x12 down to 0) whose first byte is non-zero (0x0046d06d), twice
// (esi = 2): r = 0x00421bc0(1) (call 0x0046d079); record r: byte +0xd9 = bl (0x0046d08f), word +0x579576 (offset 0x9d)
// = [0x0056a51c] (0x0046d095), [0x0056a51c] += 1 (0x0046d09e), word +0x57955a (offset 0x81) = r ^ 1 (0x0046d0a4).
void __cdecl InitGolferState_re() {
    for (unsigned p = 0x005794d9; p < 0x00582cd9; p += 0x100) G<unsigned char>(p) = 0;   // 0x0046d045..0x0046d052
    G<int>(0x0056a51c) = 0;                                                       // 0x0046d059
    G<int>(0x0059ae7c) = 0;                                                       // 0x0046d05e
    int bl = 0x12;
    for (unsigned edi = 0x00577f40; static_cast<int>(edi) >= 0x00575ab0; edi -= 0x208, bl--) {   // 0x0046d0ad..0x0046d0ba
        if (G<unsigned char>(edi) == 0) continue;                                 // 0x0046d06d / 0x0046d070
        for (int k = 0; k < 2; k++) {                                             // esi = 2 (0x0046d072)
            unsigned r = static_cast<unsigned>(reinterpret_cast<C1_t>(0x00421bc0)(1));   // 0x0046d079
            unsigned base = r << 8;                                               // 0x0046d086
            int n = G<int>(0x0056a51c);                                           // 0x0046d07e
            G<unsigned char>(base + 0x005794d9) = static_cast<unsigned char>(bl); // 0x0046d08f
            G<unsigned short>(base + 0x00579576) = static_cast<unsigned short>(n);// 0x0046d095
            G<int>(0x0056a51c) = n + 1;                                           // 0x0046d09e
            G<unsigned short>(base + 0x0057955a) = static_cast<unsigned short>(r ^ 1);   // 0x0046d0a4
        }
    }
}
V0_t InitGolferState_orig;
// 0x0046d040
SG_HOOK("golf_clean.exe", 0x0046d040, initGolferState, InitGolferState_re, InitGolferState_orig);
