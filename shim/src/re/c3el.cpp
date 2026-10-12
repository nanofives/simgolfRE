// C3 batch c3el (round 30, 2026-10-12) of golf_clean.exe: the modal pilot. The pumps the dialogs loop on, and the dialogs
// themselves, A/B'd through the shared helper re/frida/js/modal.js (scripted input delivered through the root window's own
// handlers, the C3 pumps stood in on the A/B thread by deterministic models, a virtual clock, a bound that ends a runaway
// loop as an ERROR). Fixtures: re/frida/js/fixtures.d/c3el.js. Registry: re/frida/registry.d/c3el.py. Purposes and per-jcc
// tables: log/c3/c3el_purpose.md. Notes and the modal.js contract: log/c3/c3el_notes.txt.
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
typedef int(__cdecl* C2_t)(int, int);
typedef int(__fastcall* T0_t)(void* ecx, void* edx);
typedef int(__fastcall* T1_t)(void* ecx, void* edx, int a);

// virtual slot `off` of obj (thiscall)
template <typename F> static inline F vslot(void* obj, unsigned off) {
    return *reinterpret_cast<F*>(*reinterpret_cast<char**>(obj) + off);
}

// call `fn` (cdecl, no stack arguments) with eax = eaxIn at the call: the callers below reach gfxFlushA 0x00497b00 by a
// tail jmp with a known eax, and gfxFlushA returns that eax untouched when there is no net session.
static int callWithEax(unsigned fn, int eaxIn) {
    int r;
    __asm {
        mov eax, eaxIn
        call fn
        mov r, eax
    }
    return r;
}

const unsigned kGfxFlushA = 0x00497b00;
const T0_t kNetPump = reinterpret_cast<T0_t>(0x0049aa70);
const C0_t kRedrawAll = reinterpret_cast<C0_t>(0x00483c70);
const C0_t kFrameTick = reinterpret_cast<C0_t>(0x00483cd0);
const C0_t kEnterModal = reinterpret_cast<C0_t>(0x00483d30);
const C0_t kScreenFlip = reinterpret_cast<C0_t>(0x00483ba0);
const C2_t kRunScrollText = reinterpret_cast<C2_t>(0x0046dea0);

typedef void*(__stdcall* LoadCursorA_t)(void* inst, unsigned id);
typedef void*(__stdcall* SetCursor_t)(void* cursor);
const unsigned kIatLoadCursorA = 0x004ba1c4;
const unsigned kIatSetCursor = 0x004ba1f8;

}  // namespace

// ====================================================================================================================
// gfxFlushA and the two frame pumps that tail-jump into it
// ====================================================================================================================

// 0x00497b00  gfxFlushA: ecx = [0x008400b8] (0x00497b00); zero returns with eax untouched (test 0x00497b06, je 0x00497b08 ->
// ret 0x00497b18). eax = [0x008400b0] (0x00497b0a); zero returns 0 (0x00497b0f / 0x00497b11). Otherwise a tail jmp to
// netPump 0x0049aa70 (0x00497b13) with ecx = the net object: the result is netPump's. The naked entry hands the entry eax to
// the body (pumpInput483bd0 / 483c30 / 483c90 / 483cf0 jump here with eax = 0).
static int __cdecl GfxFlushABody(int eaxIn) {
    void* net = G<void*>(0x008400b8);                                       // 0x00497b00
    if (net == 0) return eaxIn;                                             // 0x00497b06 / 0x00497b08
    if (G<int>(0x008400b0) == 0) return 0;                                  // 0x00497b0a..0x00497b11
    return kNetPump(net, 0);                                                // 0x00497b13
}
__declspec(naked) int __cdecl GfxFlushA_re() {
    __asm {
        push eax
        call GfxFlushABody
        add esp, 4
        ret
    }
}
C0_t GfxFlushA_orig;
// 0x00497b00
SG_HOOK("golf_clean.exe", 0x00497b00, gfxFlushA, GfxFlushA_re, GfxFlushA_orig);

// 0x00483c90  pumpInput483c90: [0x0083ad4c] = 2 (0x00483c90); redrawAll 0x00483c70 (0x00483c9a); a zero result goes to
// 0x00483cb6 (test 0x00483c9f, je 0x00483ca1). A non-zero result loops: [0x0083ad4c] = 2 (0x00483ca3), redrawAll (0x00483cad)
// while non-zero (0x00483cb2 / 0x00483cb4). [0x0083ad4c] = 0 (0x00483cb6), then a tail jmp to gfxFlushA (0x00483cc0) with
// eax = 0 (the loop leaves on a zero result).
int __cdecl PumpInput483c90_re() {
    G<int>(0x0083ad4c) = 2;                                                 // 0x00483c90
    if (kRedrawAll() != 0) {                                                // 0x00483c9a / 0x00483c9f / 0x00483ca1
        do {
            G<int>(0x0083ad4c) = 2;                                         // 0x00483ca3
        } while (kRedrawAll() != 0);                                        // 0x00483cad / 0x00483cb2 / 0x00483cb4
    }
    G<int>(0x0083ad4c) = 0;                                                 // 0x00483cb6
    return callWithEax(kGfxFlushA, 0);                                      // 0x00483cc0
}
C0_t PumpInput483c90_orig;
// 0x00483c90
SG_HOOK("golf_clean.exe", 0x00483c90, pumpInput483c90, PumpInput483c90_re, PumpInput483c90_orig);

// 0x00483cf0  pumpInput483cf0: the same shape with [0x0083ad4c] = 8 (0x00483cf0, 0x00483d03) and frameTick483cd0 0x00483cd0
// (0x00483cfa, 0x00483d0d; tests 0x00483cff / 0x00483d01 and 0x00483d12 / 0x00483d14); [0x0083ad4c] = 0 (0x00483d16) and a
// tail jmp to gfxFlushA (0x00483d20) with eax = 0.
int __cdecl PumpInput483cf0_re() {
    G<int>(0x0083ad4c) = 8;                                                 // 0x00483cf0
    if (kFrameTick() != 0) {                                                // 0x00483cfa / 0x00483cff / 0x00483d01
        do {
            G<int>(0x0083ad4c) = 8;                                         // 0x00483d03
        } while (kFrameTick() != 0);                                        // 0x00483d0d / 0x00483d12 / 0x00483d14
    }
    G<int>(0x0083ad4c) = 0;                                                 // 0x00483d16
    return callWithEax(kGfxFlushA, 0);                                      // 0x00483d20
}
C0_t PumpInput483cf0_orig;
// 0x00483cf0
SG_HOOK("golf_clean.exe", 0x00483cf0, pumpInput483cf0, PumpInput483cf0_re, PumpInput483cf0_orig);

// ====================================================================================================================
// Window::runModal: the window modal stack and its screenFlip loop
// ====================================================================================================================

// 0x0047bab0  Window::runModal(this, flags, poll, obj): thiscall, `ret 0xc`, returns ebx (0 or 1).
//   The stack depth [0x0083ab58] at 4 or more (cmp 0x0047bab9, jl 0x0047babe) returns 0 (0x0047bac1).
//   Push: [0x0083a4e8 + 4 * depth] = this (0x0047bad2), [0x0083aa88 + 4 * depth] = obj (0x0047bad9), depth + 1 stored
//   (0x0047bae0 / 0x0047bae3); [0x0083ab54] = this (0x0047bae8); [0x0083ab40] = [0x0083ab44] = 0 (0x0047baee / 0x0047baf4).
//   A drag window [0x0083ab30] that is non-zero and not this (0x0047bae1 / 0x0047bafa, 0x0047bafc / 0x0047bafe) gets its
//   slot +0x10 (0x0047bb02) and [0x0083ab30] = 0 (0x0047bb05). [0x0083ab38] = 0 (0x0047bb0d).
//   A non-NULL obj (0x0047bb0b / 0x0047bb13) is stored in [0x0083ab50] (0x0047bb15) and gets its slot +4 with 0 (0x0047bb20).
//   flags & 2 == 0 (0x0047bb23..0x0047bb2a) calls this's slot +4 with 0 (0x0047bb31).
//   enterModal483d30 (0x0047bb34); SetCursor(LoadCursorA(NULL, 0x7f00)) through [0x004ba1c4] / [0x004ba1f8] (0x0047bb3f,
//   0x0047bb46).
//   flags & 2 != 0 (0x0047bb4c / 0x0047bb4e) or [0x0083ab54] != this (0x0047bb50 / 0x0047bb56) returns 0.
//   Loop (0x0047bb5c): this's slot +0x114 returning 0 (0x0047bb60..0x0047bb68) returns 0. A non-NULL poll (0x0047bb6a /
//   0x0047bb6c) is called (0x0047bb6e); a 0 result (0x0047bb70 / 0x0047bb72) calls slot +0x110 of [0x0083ab54] when it is
//   non-NULL (0x0047bb89..0x0047bb95) and returns 1 (0x0047bb9b). Then screenFlip 0x00483ba0 (0x0047bb74); the loop repeats
//   while [0x0083ab54] == this (0x0047bb79 / 0x0047bb7f), else returns 0 (0x0047bb82).
typedef int(__cdecl* Poll_t)();
int __fastcall WindowRunModal_re(void* self, void*, int flags, Poll_t poll, void* obj) {
    int depth = G<int>(0x0083ab58);                                         // 0x0047bab0
    if (depth >= 4) return 0;                                               // 0x0047bab9 / 0x0047babe / 0x0047bac1
    G<void*>(0x0083a4e8 + 4 * depth) = self;                                // 0x0047bad2
    G<void*>(0x0083aa88 + 4 * depth) = obj;                                 // 0x0047bad9
    G<int>(0x0083ab58) = depth + 1;                                         // 0x0047bae0 / 0x0047bae3
    G<void*>(0x0083ab54) = self;                                            // 0x0047bae8
    G<int>(0x0083ab40) = 0;                                                 // 0x0047baee
    G<int>(0x0083ab44) = 0;                                                 // 0x0047baf4
    void* drag = G<void*>(0x0083ab30);                                      // 0x0047bac7
    if (drag != 0 && drag != self) {                                        // 0x0047bafa / 0x0047bafe
        vslot<T0_t>(drag, 0x10)(drag, 0);                                   // 0x0047bb02
        G<int>(0x0083ab30) = 0;                                             // 0x0047bb05
    }
    G<int>(0x0083ab38) = 0;                                                 // 0x0047bb0d
    if (obj != 0) {                                                         // 0x0047bb0b / 0x0047bb13
        G<void*>(0x0083ab50) = obj;                                         // 0x0047bb15
        vslot<T1_t>(obj, 4)(obj, 0, 0);                                     // 0x0047bb20
    }
    const int nonBlocking = flags & 2;                                      // 0x0047bb27
    if (nonBlocking == 0) vslot<T1_t>(self, 4)(self, 0, 0);                 // 0x0047bb2a / 0x0047bb31
    kEnterModal();                                                          // 0x0047bb34
    Imp<SetCursor_t>(kIatSetCursor)(Imp<LoadCursorA_t>(kIatLoadCursorA)(0, 0x7f00));   // 0x0047bb3f / 0x0047bb46
    if (nonBlocking != 0) return 0;                                         // 0x0047bb4c / 0x0047bb4e
    if (G<void*>(0x0083ab54) != self) return 0;                             // 0x0047bb50 / 0x0047bb56
    for (;;) {
        if (vslot<T0_t>(self, 0x114)(self, 0) == 0) return 0;               // 0x0047bb60 / 0x0047bb66 / 0x0047bb68
        if (poll != 0 && poll() == 0) {                                     // 0x0047bb6a..0x0047bb72
            void* top = G<void*>(0x0083ab54);                               // 0x0047bb89
            if (top != 0) vslot<T0_t>(top, 0x110)(top, 0);                  // 0x0047bb8f..0x0047bb95
            return 1;                                                       // 0x0047bb9b
        }
        kScreenFlip();                                                      // 0x0047bb74
        if (G<void*>(0x0083ab54) != self) return 0;                         // 0x0047bb79 / 0x0047bb7f / 0x0047bb82
    }
}
typedef int(__fastcall* RunModal_t)(void*, void*, int, Poll_t, void*);
RunModal_t WindowRunModal_orig;
// 0x0047bab0
SG_HOOK("golf_clean.exe", 0x0047bab0, Window_runModal, WindowRunModal_re, WindowRunModal_orig);

// ====================================================================================================================
// scrollTextWindow
// ====================================================================================================================

// 0x0046de70  scrollTextWindow(text, x, y): cdecl (`ret` 0x0046de9c). [0x00839260] = x (0x0046de7f), [0x00839264] = y
// (0x0046de84), [0x00839278] = 1 (0x0046de8a); returns runScrollText 0x0046dea0 (text, 1) (pushes 0x0046de7c / 0x0046de7e,
// call 0x0046de94, add esp, 8 0x0046de99).
int __cdecl ScrollTextWindow_re(const char* text, int x, int y) {
    G<int>(0x00839260) = x;                                                 // 0x0046de7f
    G<int>(0x00839264) = y;                                                 // 0x0046de84
    G<int>(0x00839278) = 1;                                                 // 0x0046de8a
    return kRunScrollText(reinterpret_cast<int>(text), 1);                  // 0x0046de94
}
typedef int(__cdecl* Stw_t)(const char*, int, int);
Stw_t ScrollTextWindow_orig;
// 0x0046de70
SG_HOOK("golf_clean.exe", 0x0046de70, scrollTextWindow, ScrollTextWindow_re, ScrollTextWindow_orig);

// ====================================================================================================================
// runScrollText: the scroll-text menu loop
// ====================================================================================================================

namespace {
typedef int(__fastcall* SetDrawTarget_t)(void*, void*, int, int, int, int);
typedef int(__fastcall* WidgetValue_t)(void*, void*);
typedef void(__fastcall* ToLocal_t)(void*, void*, int*, int*);
typedef int(__cdecl* C1_t)(int);
const SetDrawTarget_t kSetDrawTarget = reinterpret_cast<SetDrawTarget_t>(0x004762d0);
const WidgetValue_t kWidgetValue = reinterpret_cast<WidgetValue_t>(0x00477580);
const ToLocal_t kViewToLocal = reinterpret_cast<ToLocal_t>(0x0047ab50);
const C1_t kScaleX = reinterpret_cast<C1_t>(0x00404970);
const C2_t kDrawScrollText = reinterpret_cast<C2_t>(0x0046e260);
const C2_t kWaitTicks = reinterpret_cast<C2_t>(0x0045bf80);
const C0_t kPumpMessages = reinterpret_cast<C0_t>(0x0045c030);
const C0_t kFlushRedraw = reinterpret_cast<C0_t>(0x0045ae70);
const C0_t kSetDirtyFlagA = reinterpret_cast<C0_t>(0x0045ae30);
const C0_t kResetScrollText = reinterpret_cast<C0_t>(0x00465560);
}  // namespace

// 0x0046dea0  runScrollText(text, unused): cdecl (`ret` 0x0046e22c), returns the chosen line or -1. Only the first argument
// is read ([esp + 0x428] at 0x0046dead); scrollTextWindow passes 1 as the second.
//   Setup: the text (strlen + 1 bytes, repne scasb 0x0046deb8) is copied into a 0x400-byte frame buffer (rep movs 0x0046decb /
//   0x0046ded2); setDrawTarget 0x004762d0 on the text object [0x004c1570] with (0x519928, 0, 0, 0) (0x0046dee4); sel = [0x004e3da4]
//   unless it is -1, then 0 (0x0046def0..0x0046df0a); [0x00838f80] = [0x00839294] = [0x0083928c] = 0 (0x0046def6..0x0046df02),
//   [0x0083927c] = -1 (0x0046df17), [0x00838eec] = 1 (0x0046df1d); Widget_value 0x00477580 on [0x004c1570] (0x0046df23, result
//   unused); [0x004e3dac] = 7 and the colours [0x0083925c] = 0x80000000, [0x00839250] = 0x80004210, [0x00838ec0] = 0x800003ff,
//   [0x00838f74] = 0x80000018 (0x0046df36..0x0046df59), replaced when bit 0 of the byte [0x00839264] is set (test 0x0046df33,
//   je 0x0046df63) by 0x80007fff, 0x800003ff, 0x800003ff, 0x80006318 (0x0046df65..0x0046df79). done = 0, hover = 1
//   (0x0046df8f / 0x0046df93). drawScrollText 0x0046e260 (buffer, [0x004e3da4]) (0x0046df97); a non-zero [0x00838f70] is
//   cleared and -1 returned (0x0046df9c..0x0046dfba). waitTicks(0x1e, 0) (0x0046dfbe), enterModal483d30 (0x0046dfc6),
//   [0x00822d68] = 0 (0x0046dfcb).
//   Loop (0x0046dfd1): [0x00838eec] = 0; pumpMessages 0x0045c030 (0x0046dfd7). With [0x00822d68] == 0 and hover == 0
//   (0x0046dfe1..0x0046dfe9) only flushRedraw 0x0045ae70 runs (0x0046dfeb): 0 keeps sel, else the key branch. Otherwise hover = 1
//   (0x0046dfff); the point is the cursor (View_toLocal 0x0047ab50 on 0x00519a60, 0x0046e014, each coordinate * 4 / scaleX(4),
//   0x0046e019..0x0046e052) when [0x00822d68] == 0, else the click [0x00822d58] / [0x00822d5c] with [0x00839294] = 1
//   (0x0046e05f..0x0046e06b); [0x00822d68] == 2 sets [0x0083928c] = 1 (0x0046e079..0x0046e07e). The line under the point:
//   sel = (py - [0x00839264] - 4) / [0x004e3dac] (signed, 0x0046e084..0x0046e092) - [0x0083927c] (0x0046e09f), -1 when negative
//   (jns 0x0046e0a1); a set bit sel & 31 of [0x00839280] restores the last drawn line (0x0046e0a6..0x0046e0b7); px above
//   [0x00838f84] or below [0x00839260] (0x0046e0bb..0x0046e0c9) and py above [0x00838f88] or below [0x00839264]
//   (0x0046e0ce..0x0046e0d8) give -1. [0x00839294] != 0 with [0x00822d68] == 0 sets done (0x0046e0dd..0x0046e0ef). Then
//   flushRedraw (0x0046e0f7); 0 skips the key branch (0x0046e0fe).
//   Key branch (0x0046e10b): hover = 0; key = setDirtyFlagA 0x0045ae30 (0x0046e10f); switch on key - 0xd (tables 0x0046e230 /
//   0x0046e244): 0xd and 0x20 set done when bit sel & 31 of [0x00839280] is clear (0x0046e13f..0x0046e14e); 0x1b sets done and
//   sel = -1 (0x0046e154 -> 0x0046e1a8); 0x26 decrements a positive sel (0x0046e138..0x0046e13c); 0x28 increments sel below
//   [0x00838f7c] - 1 (0x0046e12b..0x0046e135); any other key searches the bytes 0x00838ec4[sel + 1 .. 0x1f] for one that is not
//   0xff and whose low 5 bits equal the key's (0x0046e15a..0x0046e181).
//   Then sel is -1 when negative, not below [0x00838f7c], or [0x00839274] / [0x00838ee8] is non-zero (0x0046e18c..0x0046e1a8);
//   a sel that differs from the last drawn one with [0x00839278] != 0 is drawn (drawScrollText 0x0046e1bf) and remembered.
//   done or [0x0083afcc] != 0 (0x0046e1cb..0x0046e1d7) leaves the loop.
//   Exit: sel != -1 waits waitTicks(0x14, 0) (0x0046e1e7), else [0x0083928c] = 0 (0x0046e1f1); [0x004e3da8] = [0x004e3da4] = -1,
//   [0x00839290] = [0x00838ee8] = [0x00839280] = [0x00838ee4] = 0 (0x0046e1f7..0x0046e215); resetScrollText 0x00465560 (0x0046e21b);
//   returns sel (0x0046e220).
int __cdecl RunScrollText_re(const char* text, int) {
    char buf[0x400];
    {
        unsigned n = 0;
        while (text[n] != 0) ++n;                                           // repne scasb 0x0046deb8
        for (unsigned i = 0; i <= n; ++i) buf[i] = text[i];                 // rep movsd / movsb 0x0046decb / 0x0046ded2
    }
    kSetDrawTarget(G<void*>(0x004c1570), 0, 0x519928, 0, 0, 0);             // 0x0046dee4
    int sel = G<int>(0x004e3da4);                                           // 0x0046dee9
    if (sel == -1) sel = 0;                                                 // 0x0046deee / 0x0046def0 / 0x0046df08
    int last = -1;                                                          // 0x0046def2
    G<int>(0x00838f80) = 0;                                                 // 0x0046def6
    G<int>(0x00839294) = 0;                                                 // 0x0046defc
    G<int>(0x0083928c) = 0;                                                 // 0x0046df02
    G<int>(0x0083927c) = -1;                                                // 0x0046df17
    G<int>(0x00838eec) = 1;                                                 // 0x0046df1d
    kWidgetValue(G<void*>(0x004c1570), 0);                                  // 0x0046df23
    const unsigned char ylo = G<unsigned char>(0x00839264);                 // 0x0046df28
    G<int>(0x004e3dac) = 7;                                                 // 0x0046df36
    G<unsigned>(0x0083925c) = 0x80000000u;                                  // 0x0046df40
    G<unsigned>(0x00839250) = 0x80004210u;                                  // 0x0046df4a
    G<unsigned>(0x00838ec0) = 0x800003ffu;                                  // 0x0046df54
    G<unsigned>(0x00838f74) = 0x80000018u;                                  // 0x0046df59
    if (ylo & 1) {                                                          // 0x0046df33 / 0x0046df63
        G<unsigned>(0x0083925c) = 0x80007fffu;                              // 0x0046df65
        G<unsigned>(0x00838f74) = 0x800003ffu;                              // 0x0046df6f
        G<unsigned>(0x00838ec0) = 0x800003ffu;                              // 0x0046df74
        G<unsigned>(0x00839250) = 0x80006318u;                              // 0x0046df79
    }
    int done = 0;                                                           // 0x0046df8f
    int hover = 1;                                                          // 0x0046df93
    kDrawScrollText(reinterpret_cast<int>(buf), G<int>(0x004e3da4));        // 0x0046df97
    if (G<int>(0x00838f70) != 0) {                                          // 0x0046df9c..0x0046dfa6
        G<int>(0x00838f70) = 0;                                             // 0x0046dfa8
        return -1;                                                          // 0x0046dfb0
    }
    kWaitTicks(0x1e, 0);                                                    // 0x0046dfbe
    kEnterModal();                                                          // 0x0046dfc6
    G<int>(0x00822d68) = 0;                                                 // 0x0046dfcb
    for (;;) {
        G<int>(0x00838eec) = 0;                                             // 0x0046dfd1
        kPumpMessages();                                                    // 0x0046dfd7
        int click = G<int>(0x00822d68);                                     // 0x0046dfdc
        bool keyBranch;
        if (click == 0 && hover == 0) {                                     // 0x0046dfe1..0x0046dfe9
            keyBranch = kFlushRedraw() != 0;                                // 0x0046dfeb..0x0046dff8
        } else {
            hover = 1;                                                      // 0x0046dfff
            int px, py;
            if (click == 0) {                                               // 0x0046e003
                kViewToLocal(reinterpret_cast<void*>(0x00519a60), 0, &px, &py);   // 0x0046e014
                px = static_cast<int>(static_cast<unsigned>(px) * 4u) / kScaleX(4);  // 0x0046e019..0x0046e032
                py = static_cast<int>(static_cast<unsigned>(py) * 4u) / kScaleX(4);  // 0x0046e036..0x0046e052
                click = G<int>(0x00822d68);                                 // 0x0046e054
            } else {
                px = G<int>(0x00822d58);                                    // 0x0046e05f
                py = G<int>(0x00822d5c);                                    // 0x0046e065
                G<int>(0x00839294) = 1;                                     // 0x0046e06b
            }
            if (click == 2) G<int>(0x0083928c) = 1;                         // 0x0046e079..0x0046e07e
            const int top = G<int>(0x00839264);                             // 0x0046e084
            sel = (py - top - 4) / G<int>(0x004e3dac) - G<int>(0x0083927c); // 0x0046e08a..0x0046e09f
            if (sel < 0) sel = -1;                                          // jns 0x0046e0a1 / 0x0046e0a3
            if (((1u << (sel & 31)) & G<unsigned>(0x00839280)) != 0) sel = last;   // 0x0046e0a6..0x0046e0b7
            if (px > G<int>(0x00838f84) || px < G<int>(0x00839260)) sel = -1;      // 0x0046e0bb..0x0046e0cb
            if (py > G<int>(0x00838f88) || py < top) sel = -1;                     // 0x0046e0ce..0x0046e0da
            if (G<int>(0x00839294) != 0 && G<int>(0x00822d68) == 0) done = 1;     // 0x0046e0dd..0x0046e0ef
            keyBranch = kFlushRedraw() != 0;                                // 0x0046e0f7..0x0046e0fe
        }
        bool none = false;
        if (keyBranch) {
            hover = 0;                                                      // 0x0046e10b
            const int key = kSetDirtyFlagA();                               // 0x0046e10f
            switch (key) {                                                  // 0x0046e114..0x0046e124
            case 0x0d:
            case 0x20:                                                      // 0x0046e13f
                if (((1u << (sel & 31)) & G<unsigned>(0x00839280)) == 0) done = 1;   // 0x0046e144..0x0046e14e
                break;
            case 0x1b:                                                      // 0x0046e154
                done = 1;
                none = true;                                                // jmp 0x0046e1a8
                break;
            case 0x26:                                                      // 0x0046e138
                if (sel > 0) --sel;                                         // 0x0046e13a / 0x0046e13c
                break;
            case 0x28:                                                      // 0x0046e12b
                if (sel < G<int>(0x00838f7c) - 1) ++sel;                    // 0x0046e130..0x0046e135
                break;
            default:                                                        // 0x0046e15a
                for (int i = sel + 1; i < 0x20; ++i) {                      // 0x0046e15a..0x0046e17d
                    const unsigned char h = G<unsigned char>(0x00838ec4 + i);       // 0x0046e162
                    if (h != 0xff && ((h & 0x1f) ^ (key & 0x1f)) == 0) {    // 0x0046e168..0x0046e177
                        sel = i;                                            // 0x0046e181
                        break;
                    }
                }
                break;
            }
        }
        if (none || sel < 0 || sel >= G<int>(0x00838f7c) ||                 // 0x0046e18c..0x0046e196
            G<int>(0x00839274) != 0 || G<int>(0x00838ee8) != 0)             // 0x0046e198..0x0046e1a6
            sel = -1;                                                       // 0x0046e1a8
        if (sel != last && G<int>(0x00839278) != 0) {                       // 0x0046e1ab..0x0046e1b7
            kDrawScrollText(reinterpret_cast<int>(buf), sel);               // 0x0046e1bf
            last = sel;                                                     // 0x0046e1c7
        }
        if (done != 0 || G<int>(0x0083afcc) != 0) break;                    // 0x0046e1cb..0x0046e1d7
    }
    if (sel != -1) kWaitTicks(0x14, 0);                                     // 0x0046e1e0..0x0046e1e7
    else G<int>(0x0083928c) = 0;                                            // 0x0046e1f1
    G<int>(0x004e3da8) = -1;                                                // 0x0046e1f7
    G<int>(0x004e3da4) = -1;                                                // 0x0046e1fd
    G<int>(0x00839290) = 0;                                                 // 0x0046e203
    G<int>(0x00838ee8) = 0;                                                 // 0x0046e209
    G<int>(0x00839280) = 0;                                                 // 0x0046e20f
    G<int>(0x00838ee4) = 0;                                                 // 0x0046e215
    kResetScrollText();                                                     // 0x0046e21b
    return sel;                                                             // 0x0046e220
}
typedef int(__cdecl* Rst_t)(const char*, int);
Rst_t RunScrollText_orig;
// 0x0046dea0
SG_HOOK("golf_clean.exe", 0x0046dea0, runScrollText, RunScrollText_re, RunScrollText_orig);

// ====================================================================================================================
// textInputBox: the one-line text entry over the shared text buffer 0x0051a068
// ====================================================================================================================

namespace {
typedef int(__fastcall* Fill5_t)(void*, void*, int, int, int, int, unsigned);
typedef int(__fastcall* TextPut_t)(void*, void*, const char*);
typedef int(__fastcall* Refresh_t)(void*, void*, int);
typedef int(__cdecl* DrawText_t)(const char*, int, int, unsigned);
const Fill5_t kWinFillRect = reinterpret_cast<Fill5_t>(0x00478af0);
const Fill5_t kWidgetFillRect = reinterpret_cast<Fill5_t>(0x00478b80);
const Fill5_t kFillWindowRect = reinterpret_cast<Fill5_t>(0x00480b00);
const TextPut_t kTextPut = reinterpret_cast<TextPut_t>(0x00477250);
const Refresh_t kRefreshFull = reinterpret_cast<Refresh_t>(0x00480c80);
const DrawText_t kDrawText = reinterpret_cast<DrawText_t>(0x004049d0);
const C0_t kSetDirtyFlagB = reinterpret_cast<C0_t>(0x0045ae50);
const C1_t kIsPrint = reinterpret_cast<C1_t>(0x004a65c0);
static inline char* TB() { return reinterpret_cast<char*>(0x0051a068); }
static inline unsigned slen(const char* s) { unsigned n = 0; while (s[n] != 0) ++n; return n; }
}  // namespace

// 0x0045b2c0  textInputBox(title, x, y, maxLen): cdecl (`ret` 0x0045b646 / 0x0045b65c); edits the NUL-terminated text at
// 0x0051a068 in place and returns with eax = the last setDirtyFlagA result (both exits leave it untouched).
//   Setup: the title (strlen + 1 bytes, 0x0045b2da..0x0045b300) is copied to a frame buffer; the text is copied to a second
//   one that nothing reads (0x0045b302..0x0045b322); len = strlen(text) (0x0045b324..0x0045b345); cursor = len (0x0045b349);
//   width = maxLen * 12 (lea / shl 0x0045b33e / 0x0045b342).
//   Draw (0x0045b34f): setDrawTarget 0x004762d0 on [0x004c1570] with (0x51b360, 0, 0, 0) (0x0045b360); Win_fillRect 0x00478af0
//   (thiscall on [0x004c1570]) (x - 1, y - 1, width + 0x20, 0x30, 0x80007fff), (x + 1, y + 1, .., 0x80000000), (x, y, ..,
//   0x800035b3) (0x0045b382, 0x0045b39d, 0x0045b3b2); drawText4049d0 (title copy, x + 4, y + 6, 0x80007fff) (0x0045b3c9);
//   Win_fillRect (x + 0x10, y + 0x17, width, 0x16, 0x80007fff) (0x0045b3eb); setDrawTarget (0x519928, 0, 0, 0) (0x0045b401);
//   drawText4049d0 (0x51a068, x + 0x14, y + 0x1c, 0x80000000) (0x0045b418); the byte at the cursor is replaced by 0 while
//   Text_put 0x00477250 measures the head (0x0045b41d..0x0045b44f); Widget_fillRect 0x00478b80 on 0x00519cd4 draws the caret
//   (cx, y + 0x1a, cx, y + 0x2a, 0x80006000) with cx = x + head + 0x14 (0x0045b45b); w = Text_put(text) (0x0045b46b). While
//   w > width - 0x10 (signed, 0x0045b477 / 0x0045b4c6) the last character is cut (0x0045b48e), a cursor above the new length
//   (unsigned jbe 0x0045b49e) is set to it, and w is measured again; len is not updated there. fillWindowRect 0x00480b00 on
//   0x00519a60 (w + x + 0x15, y + 0x29, 8, 2, 0x80004210) (0x0045b4e8); Window::refreshFull 0x00480c80 on 0x00519a60 with 0
//   (0x0045b4f4).
//   Wait (0x0045b4f9): waitTicks(1, 0), pumpMessages; a non-zero [0x00822d68] (0x0045b50f / 0x0045b511) or flushRedraw != 0
//   (0x0045b518 / 0x0045b51a) ends it. ch = setDirtyFlagB 0x0045ae50 (0x0045b51c), code = setDirtyFlagA (0x0045b523).
//   ch == 0x1b: the text is emptied (byte 0, 0x0045b638) and the function returns. ch == 0xd: the byte at len is 0
//   (0x0045b64e) and it returns. ch == 8 with cursor != 0 (0x0045b53f): bytes cursor..len move down one (0x0045b551..0x0045b560),
//   cursor and len drop by one and the byte at len is 0 (0x0045b562..0x0045b568). Any other non-zero ch goes to the insert
//   test. ch == 0 reads code: 0x27 moves the cursor right below len, 0x25 left above 0, 0x23 to len, 0x24 to 0, 0x2e with the
//   cursor below len removes the byte at the cursor (0x0045b5a8..0x0045b5cc).
//   Insert test (0x0045b5d3): a cursor below maxLen - 1 (signed) and _isprint(ch) 0x004a65c0 != 0 (0x0045b5e4) move bytes
//   len..cursor up one (0x0045b5fe..0x0045b60d), store ch at the cursor, cursor + 1, len + 1, a 0 at len and at maxLen
//   (0x0045b616..0x0045b629). Then the draw again.
int __cdecl TextInputBox_re(const char* title, int x, int y, int maxLen) {
    char head[0x80];
    {
        const unsigned n = slen(title);                                     // repne scasb 0x0045b2da
        for (unsigned i = 0; i <= n; ++i) head[i] = title[i];               // 0x0045b2f7 / 0x0045b300
    }
    char* const tb = TB();
    int len = static_cast<int>(slen(tb));                                   // 0x0045b324..0x0045b345
    int cur = len;                                                          // 0x0045b349
    const int width = maxLen * 12;                                          // 0x0045b33e / 0x0045b342
    for (;;) {
        void* to = G<void*>(0x004c1570);
        kSetDrawTarget(to, 0, 0x51b360, 0, 0, 0);                           // 0x0045b360
        kWinFillRect(G<void*>(0x004c1570), 0, x - 1, y - 1, width + 0x20, 0x30, 0x80007fffu);   // 0x0045b382
        kWinFillRect(G<void*>(0x004c1570), 0, x + 1, y + 1, width + 0x20, 0x30, 0x80000000u);   // 0x0045b39d
        kWinFillRect(G<void*>(0x004c1570), 0, x, y, width + 0x20, 0x30, 0x800035b3u);           // 0x0045b3b2
        kDrawText(head, x + 4, y + 6, 0x80007fffu);                                             // 0x0045b3c9
        kWinFillRect(G<void*>(0x004c1570), 0, x + 0x10, y + 0x17, width, 0x16, 0x80007fffu);    // 0x0045b3eb
        kSetDrawTarget(G<void*>(0x004c1570), 0, 0x519928, 0, 0, 0);                             // 0x0045b401
        kDrawText(tb, x + 0x14, y + 0x1c, 0x80000000u);                                         // 0x0045b418
        const char saved = tb[cur];                                         // 0x0045b41d
        tb[cur] = 0;                                                        // 0x0045b42c
        const int cx = x + kTextPut(G<void*>(0x004c1570), 0, tb) + 0x14;    // 0x0045b438 / 0x0045b445
        tb[cur] = saved;                                                    // 0x0045b44f
        kWidgetFillRect(reinterpret_cast<void*>(0x00519cd4), 0, cx, y + 0x1a, cx, y + 0x2a, 0x80006000u);   // 0x0045b45b
        int w = kTextPut(G<void*>(0x004c1570), 0, tb);                      // 0x0045b46b
        while (w > width - 0x10) {                                          // 0x0045b477 / 0x0045b4c6
            tb[slen(tb) - 1] = 0;                                           // 0x0045b485..0x0045b48e
            const unsigned n2 = slen(tb);                                   // 0x0045b497..0x0045b49b
            if (static_cast<unsigned>(cur) > n2) cur = static_cast<int>(n2);        // 0x0045b49c / 0x0045b49e / 0x0045b4ad
            w = kTextPut(G<void*>(0x004c1570), 0, tb);                      // 0x0045b4ba
        }
        kFillWindowRect(reinterpret_cast<void*>(0x00519a60), 0, w + x + 0x15, y + 0x29, 8, 2, 0x80004210u);   // 0x0045b4e8
        kRefreshFull(reinterpret_cast<void*>(0x00519a60), 0, 0);            // 0x0045b4f4
        for (;;) {
            kWaitTicks(1, 0);                                               // 0x0045b4fd
            kPumpMessages();                                                // 0x0045b505
            if (G<int>(0x00822d68) != 0) break;                             // 0x0045b50f / 0x0045b511
            if (kFlushRedraw() != 0) break;                                 // 0x0045b513..0x0045b51a
        }
        const int ch = kSetDirtyFlagB();                                    // 0x0045b51c
        const int code = kSetDirtyFlagA();                                  // 0x0045b523
        if (ch == 0x1b) {                                                   // 0x0045b528 / 0x0045b52b
            tb[0] = 0;                                                      // 0x0045b638
            return code;
        }
        if (ch == 0x0d) {                                                   // 0x0045b531 / 0x0045b534
            tb[len] = 0;                                                    // 0x0045b64e
            return code;
        }
        if (ch == 8) {                                                      // 0x0045b53a / 0x0045b53d
            if (cur != 0) {                                                 // 0x0045b53f / 0x0045b541
                for (int i = cur; i <= len; ++i) tb[i] = tb[i + 1];         // 0x0045b54d..0x0045b560
                --cur;                                                      // 0x0045b562
                --len;                                                      // 0x0045b563
                tb[len] = 0;                                                // 0x0045b568
            }
        } else if (ch == 0) {                                               // 0x0045b571 / 0x0045b573
            if (code == 0x27) {                                             // 0x0045b575
                if (cur < len) ++cur;                                       // 0x0045b57a..0x0045b580
            } else if (code == 0x25) {                                      // 0x0045b583
                if (cur != 0) --cur;                                        // 0x0045b588..0x0045b58c
            } else if (code == 0x23) {                                      // 0x0045b58f
                cur = len;                                                  // 0x0045b594
            } else if (code == 0x24) {                                      // 0x0045b59a
                cur = 0;                                                    // 0x0045b59f
            } else if (code == 0x2e && cur < len) {                         // 0x0045b5a3..0x0045b5ac
                for (int i = cur; i <= len; ++i) tb[i] = tb[i + 1];         // 0x0045b5b0..0x0045b5c3
                --len;                                                      // 0x0045b5c7
                tb[len] = 0;                                                // 0x0045b5cc
            }
        }
        if (cur >= maxLen - 1) continue;                                    // 0x0045b5d3..0x0045b5dd
        if (kIsPrint(ch) == 0) continue;                                    // 0x0045b5e4..0x0045b5ee
        for (int i = len; i >= cur; --i) tb[i + 1] = tb[i];                 // 0x0045b5f4..0x0045b60d
        tb[cur] = static_cast<char>(ch);                                    // 0x0045b616
        ++cur;                                                              // 0x0045b61c
        ++len;                                                              // 0x0045b61d
        tb[len] = 0;                                                        // 0x0045b622
        tb[maxLen] = 0;                                                     // 0x0045b629
    }
}
typedef int(__cdecl* Tib_t)(const char*, int, int, int);
Tib_t TextInputBox_orig;
// 0x0045b2c0
SG_HOOK("golf_clean.exe", 0x0045b2c0, textInputBox, TextInputBox_re, TextInputBox_orig);
