// C3 batch c3eq (round 31, 2026-10-13): golf_clean.exe round-30 leftovers and the Bink video player.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and
// cites the instruction address of each fact. __thiscall is emulated with __fastcall (ecx = this, edx unused). Callees
// run through their ORIGINAL addresses, so a callee another batch hooks runs that batch's reimplementation (the verifier
// re-runs these keys with those hooks off; the list is in log/c3/c3eq_notes.txt). Every Win32 / binkw32 import is called
// through the IAT SLOT the original uses, never through the shim's own import, so the import stand-in
// (re/frida/js/importstandin.js) sees it.
//
// Hooked callees of other batches (the verifier re-runs these keys with those hooks off; the full list is
// log/c3/c3eq_hooks_off.txt, 28 addresses): pumpInput483c90 / 483cf0 0x00483c90 / 0x00483cf0 and BinkPlayer::frame
// 0x00487180 under BinkPlayer::play; fileExists 0x004378a0, BinkPlayer::setTarget 0x00487050, pumpInput483bd0
// 0x00483bd0, Window::paint 0x004808c0, resourceQuery 0x00487630 and BinkPlayer::close 0x00487060 under
// playAudioFile; the twenty measuring and setter callees under TextView::render.
// Hooked in THIS batch and called by this batch: BinkPlayer::play <- playAudioFile.
#include "hooks.h"

namespace {

template <class T>
static inline T& At(void* p, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(p) + off); }
template <class T>
static inline T& G(unsigned va) { return *reinterpret_cast<T*>(va); }
// The import whose IAT slot is at `slot` (`call dword ptr [slot]` in the original).
template <typename T>
static inline T Imp(unsigned slot) { return *reinterpret_cast<T*>(slot); }

// ---- golf_clean.exe IAT slots (pefile over original\golf_clean.exe; the exe has no relocations)
typedef int(__stdcall* Stdcall1_t)(void* a);
typedef int(__stdcall* Stdcall2_t)(void* a, void* b);
typedef int(__stdcall* Stdcall3_t)(void* a, void* b, void* c);
typedef int(__stdcall* Stdcall4_t)(void* a, void* b, void* c, void* d);
typedef int(__stdcall* Stdcall0_t)();
const unsigned kIatShowCursor = 0x004ba200;            // USER32.dll ShowCursor
const unsigned kIatTimeGetTime = 0x004ba21c;           // WINMM.dll timeGetTime
const unsigned kIatBinkOpenDirectSound = 0x004ba244;   // binkw32.dll _BinkOpenDirectSound@4
const unsigned kIatBinkSetSoundSystem = 0x004ba248;    // _BinkSetSoundSystem@8
const unsigned kIatBinkSetIOSize = 0x004ba24c;         // _BinkSetIOSize@4
const unsigned kIatBinkOpen = 0x004ba250;              // _BinkOpen@8
const unsigned kIatBinkBufferOpen = 0x004ba254;        // _BinkBufferOpen@16
const unsigned kIatBinkBufferSetOffset = 0x004ba258;   // _BinkBufferSetOffset@12
const unsigned kIatBinkWait = 0x004ba25c;              // _BinkWait@4

// ---- exe callees (original addresses)
typedef int(__cdecl* Cdecl0_t)();
typedef int(__cdecl* Cdecl1_t)(const char* a);
typedef int(__fastcall* Thi0_t)(void* ecx, void* edx);
typedef int(__fastcall* Thi1_t)(void* ecx, void* edx, unsigned a);
typedef int(__fastcall* Thi4_t)(void* ecx, void* edx, unsigned a, unsigned b, unsigned c, unsigned d);
typedef unsigned char(__fastcall* Thi0b_t)(void* ecx, void* edx);
const Cdecl0_t kPumpInput483c90 = reinterpret_cast<Cdecl0_t>(0x00483c90);   // pumpInput483c90 (c3el)
const Cdecl0_t kPumpInput483cf0 = reinterpret_cast<Cdecl0_t>(0x00483cf0);   // pumpInput483cf0 (c3el)
const Cdecl0_t kPumpInput483bd0 = reinterpret_cast<Cdecl0_t>(0x00483bd0);   // pumpInput483bd0 (c3dw)
const Thi0b_t kBinkPlayerFrame = reinterpret_cast<Thi0b_t>(0x00487180);     // BinkPlayer::frame (c3dl)
const Cdecl1_t kFileExists = reinterpret_cast<Cdecl1_t>(0x004378a0);        // fileExists (c3dw)
const Thi1_t kBinkSetTarget = reinterpret_cast<Thi1_t>(0x00487050);         // BinkPlayer::setTarget (c3bj)
const Thi0_t kBinkClose = reinterpret_cast<Thi0_t>(0x00487060);             // BinkPlayer::close (c3dl)
const Thi0_t kResourceQuery = reinterpret_cast<Thi0_t>(0x00487630);         // resourceQuery (c3dm)
const Thi1_t kWindowPaint = reinterpret_cast<Thi1_t>(0x004808c0);           // Window::paint (c3dq)
const Thi4_t kBinkPlayerPlay = reinterpret_cast<Thi4_t>(0x00487090);        // BinkPlayer::play (this batch)

// Virtual call `mov eax, [obj]; call [eax + slot]` with no stack argument.
static inline Thi0_t VSlot0(void* obj, unsigned slot) {
    return *reinterpret_cast<Thi0_t*>(static_cast<char*>(At<void*>(obj, 0)) + slot);
}

// =====================================================================================================================
// Bink video
// =====================================================================================================================

// 0x00487090  BinkPlayer::play(this, path, offX, offY, soundParam): plays one Bink movie to the end, to a key, or to an
// error, and is the only function that drives the player's loop.
//
// No target window (`mov eax, [esi + 0xc]; test eax, eax; je` 0x00487093 / 0x00487096 / 0x00487098) returns 7
// (`mov eax, 7` 0x0048709a, `ret 0x10` 0x004870a0). A non-zero soundParam ([esp + 0x14], `test eax, eax; je`
// 0x004870a3 / 0x004870a7 / 0x004870a9) calls _BinkSetSoundSystem [0x4ba248] with the ADDRESS held in the
// _BinkOpenDirectSound IAT slot [0x4ba244] as the first argument and soundParam as the second (pushes 0x004870ab /
// 0x004870b1, call 0x004870b2). _BinkSetIOSize [0x4ba24c] gets 0x9c4000 (push 0x004870b8, call 0x004870bd) and
// _BinkOpen [0x4ba250] (path, 0x1000000) (pushes 0x004870c7 / 0x004870cc, call 0x004870cd); the handle is stored at
// this+4 (0x004870d5) and a NULL handle leaves with 1 (`je` 0x004870d8 -> `mov eax, 1` 0x0048716b). A non-zero word at
// handle+0x1c (`mov ecx, [eax + 0x1c]; test ecx, ecx; jne` 0x004870de / 0x004870e1 / 0x004870e3) leaves with 1 as well.
// _BinkBufferOpen [0x4ba254] gets (this+0xc, handle[0], handle[4], 0) (pushes 0x004870f1-0x004870f5, call 0x004870f6),
// the buffer is stored at this+8 (0x004870fe) and a NULL buffer leaves with 1 (`je` 0x00487101). _BinkBufferSetOffset
// [0x4ba258] gets (buffer, offX, offY) (pushes 0x0048710d-0x0048710f, call 0x00487116) and the two offsets are kept at
// this+0x10 / this+0x14 (0x00487110 / 0x00487113).
// ShowCursor [0x4ba200] (0) hides the pointer (push 0x00487122, call 0x0048712b) and the byte 0x0083ab9c is set to 1
// (0x00487124). The loop (0x00487133): _BinkWait [0x4ba25c] on the handle (push 0x00487136, call 0x00487137); a
// non-zero result (`test eax, eax; jne` 0x00487139 / 0x0048713b) runs pumpInput483c90 0x00483c90 (call 0x00487153) and
// pumpInput483cf0 0x00483cf0 (call 0x00487158) and repeats (0x0048715d). A zero result reads the byte 0x0083ab9c
// (0x0048713d): zero ends the loop (`je` 0x00487144), else BinkPlayer::frame 0x00487180 runs on this (call 0x00487148)
// and a false result ends the loop (`test al, al; je` 0x0048714d / 0x0048714f), a true one repeats (0x00487151).
// The end shows the pointer again, ShowCursor(1) (push 0x0048715f, call 0x00487161), and returns 0 (`xor eax, eax`
// 0x00487165, `ret 0x10` 0x00487168). Neither _BinkClose nor _BinkBufferClose is called here: the caller does that
// through BinkPlayer::close 0x00487060.
typedef int(__fastcall* BinkPlay_t)(void* ecx, void* edx, unsigned path, unsigned offX, unsigned offY,
                                    unsigned soundParam);
BinkPlay_t BinkPlayerPlay_orig;
int __fastcall BinkPlayerPlay_re(void* self, void*, unsigned path, unsigned offX, unsigned offY,
                                 unsigned soundParam) {
    if (At<void*>(self, 0xc) == 0) return 7;                                       // 0x00487093 / 0x0048709a
    if (soundParam != 0)                                                           // 0x004870a3 / 0x004870a9
        Imp<Stdcall2_t>(kIatBinkSetSoundSystem)(*reinterpret_cast<void**>(kIatBinkOpenDirectSound),
                                                reinterpret_cast<void*>(soundParam));   // 0x004870b2
    Imp<Stdcall1_t>(kIatBinkSetIOSize)(reinterpret_cast<void*>(0x9c4000));         // 0x004870bd
    void* bink = reinterpret_cast<void*>(
        Imp<Stdcall2_t>(kIatBinkOpen)(reinterpret_cast<void*>(path),
                                      reinterpret_cast<void*>(0x1000000)));        // 0x004870cd
    At<void*>(self, 4) = bink;                                                     // 0x004870d5
    if (bink == 0) return 1;                                                       // 0x004870d8 / 0x0048716b
    if (At<int>(bink, 0x1c) != 0) return 1;                                        // 0x004870e3 / 0x0048716b
    void* buffer = reinterpret_cast<void*>(
        Imp<Stdcall4_t>(kIatBinkBufferOpen)(At<void*>(self, 0xc), At<void*>(bink, 0),
                                            At<void*>(bink, 4), 0));               // 0x004870f6
    At<void*>(self, 8) = buffer;                                                   // 0x004870fe
    if (buffer == 0) return 1;                                                     // 0x00487101 / 0x0048716b
    At<unsigned>(self, 0x10) = offX;                                               // 0x00487110
    At<unsigned>(self, 0x14) = offY;                                               // 0x00487113
    Imp<Stdcall3_t>(kIatBinkBufferSetOffset)(buffer, reinterpret_cast<void*>(offX),
                                             reinterpret_cast<void*>(offY));       // 0x00487116
    G<unsigned char>(0x0083ab9c) = 1;                                              // 0x00487124
    Imp<Stdcall1_t>(kIatShowCursor)(0);                                            // 0x0048712b
    for (;;) {
        if (Imp<Stdcall1_t>(kIatBinkWait)(At<void*>(self, 4)) != 0) {              // 0x00487137 / 0x0048713b
            kPumpInput483c90();                                                    // 0x00487153
            kPumpInput483cf0();                                                    // 0x00487158
            continue;                                                              // 0x0048715d
        }
        if (G<unsigned char>(0x0083ab9c) == 0) break;                              // 0x0048713d / 0x00487144
        if (kBinkPlayerFrame(self, 0) == 0) break;                                 // 0x00487148 / 0x0048714f
    }
    Imp<Stdcall1_t>(kIatShowCursor)(reinterpret_cast<void*>(1));                   // 0x00487161
    return 0;                                                                      // 0x00487165
}

// 0x0043cce0  playAudioFile(path, immediate): plays one movie file end to end on the global BinkPlayer 0x005aa6d0.
//
// fileExists 0x004378a0 on the path (push 0x0043cce5, call 0x0043cce6, `add esp, 4` 0x0043cceb) returning 0 leaves at
// once (`test eax, eax; je` 0x0043ccee / 0x0043ccf0) with that 0 in eax. A zero `immediate` ([esp + 0xc], `test eax,
// eax; jne` 0x0043ccf2 / 0x0043ccf8) sets the deadline 0x005aa774 to timeGetTime [0x4ba21c] + 0x1b58 (call 0x0043ccfa,
// `add eax, 0x1b58` 0x0043cd00, store 0x0043cd05); a non-zero one sets it to 0 (0x0043cd0c). The display 0x0083ad50 is
// asked for its window through vtable slot +0x20 (0x0043cd16-0x0043cd1e) and that result is given to
// BinkPlayer::setTarget 0x00487050 on 0x005aa6d0 (push 0x0043cd21, `mov ecx, 0x5aa6d0` 0x0043cd22, call 0x0043cd27).
// pumpInput483bd0 0x00483bd0 runs once (call 0x0043cd2c), then Window::paint 0x004808c0 on the window 0x00519a60 with
// 0x80007fff (push 0x0043cd31, `mov ecx, 0x519a60` 0x0043cd36, call 0x0043cd3b) and resourceQuery 0x00487630 on
// 0x0083ad80 (`mov ecx, 0x83ad80` 0x0043cd40, call 0x0043cd45). Its result is the fourth argument of BinkPlayer::play
// 0x00487090 (pushes 0x0043cd4a-0x0043cd4f: the path, 0 and 0; `mov ecx, 0x5aa6d0` 0x0043cd50, call 0x0043cd55), so the
// two offsets are always 0. BinkPlayer::close 0x00487060 follows on the same object (0x0043cd5a / 0x0043cd5f) and the
// deadline is cleared (0x0043cd64). The function ends `pop esi; ret` (0x0043cd6e / 0x0043cd6f) without writing eax, so
// it returns BinkPlayer::close's result on the full path and fileExists' 0 on the early exit.
typedef int(__cdecl* PlayAudioFile_t)(const char* path, int immediate);
PlayAudioFile_t PlayAudioFile_orig;
int __cdecl PlayAudioFile_re(const char* path, int immediate) {
    int exists = kFileExists(path);                                                // 0x0043cce6
    if (exists == 0) return exists;                                                // 0x0043ccee / 0x0043ccf0
    if (immediate == 0)                                                            // 0x0043ccf2 / 0x0043ccf8
        G<unsigned>(0x005aa774) = static_cast<unsigned>(Imp<Stdcall0_t>(kIatTimeGetTime)()) + 0x1b58;   // 0x0043cd05
    else
        G<unsigned>(0x005aa774) = 0;                                               // 0x0043cd0c
    void* disp = G<void*>(0x0083ad50);                                             // 0x0043cd16
    int hwnd = VSlot0(disp, 0x20)(disp, 0);                                        // 0x0043cd1e
    kBinkSetTarget(reinterpret_cast<void*>(0x005aa6d0), 0, static_cast<unsigned>(hwnd));   // 0x0043cd27
    kPumpInput483bd0();                                                            // 0x0043cd2c
    kWindowPaint(reinterpret_cast<void*>(0x00519a60), 0, 0x80007fff);              // 0x0043cd3b
    int sound = kResourceQuery(reinterpret_cast<void*>(0x0083ad80), 0);            // 0x0043cd45
    kBinkPlayerPlay(reinterpret_cast<void*>(0x005aa6d0), 0, reinterpret_cast<unsigned>(path), 0, 0,
                    static_cast<unsigned>(sound));                                 // 0x0043cd55
    int r = kBinkClose(reinterpret_cast<void*>(0x005aa6d0), 0);                    // 0x0043cd5f
    G<unsigned>(0x005aa774) = 0;                                                   // 0x0043cd64
    return r;                                                                      // 0x0043cd6f
}

// =====================================================================================================================
// TextView::render 0x0048fe60
// =====================================================================================================================

typedef int(__fastcall* R0_t)(void* ecx, void* edx);
typedef int(__fastcall* R1_t)(void* ecx, void* edx, unsigned a);
typedef int(__fastcall* R2_t)(void* ecx, void* edx, unsigned a, unsigned b);
typedef int(__fastcall* R4_t)(void* ecx, void* edx, unsigned a, unsigned b, unsigned c, unsigned d);

const R4_t kWidgetSetQuad   = reinterpret_cast<R4_t>(0x00476310);   // Widget_setQuad
const R4_t kWidgetSetQuad70 = reinterpret_cast<R4_t>(0x00476340);   // Widget_setQuad70
const R4_t kWidgetSetQuad74 = reinterpret_cast<R4_t>(0x00476370);   // Widget_setQuad74
const R4_t kWidgetSetQuad78 = reinterpret_cast<R4_t>(0x004763a0);   // Widget_setQuad78
const R4_t kSetDrawTarget   = reinterpret_cast<R4_t>(0x004762d0);   // setDrawTarget
const R1_t kSetFontSize     = reinterpret_cast<R1_t>(0x0048e120);   // TextView::setFontSize
const R1_t kSetLineSpacing  = reinterpret_cast<R1_t>(0x0048e0b0);   // TextView::setLineSpacing
const R2_t kSetSize         = reinterpret_cast<R2_t>(0x0048e190);   // TextView::setSize
const R0_t kPanelStop       = reinterpret_cast<R0_t>(0x004a11c0);   // panelStop
const R1_t kIndexOfId       = reinterpret_cast<R1_t>(0x004898d0);   // ListModel::indexOfId
const R1_t kSelectById      = reinterpret_cast<R1_t>(0x00489ed0);   // ListBox::selectById
const R2_t kSetSelection    = reinterpret_cast<R2_t>(0x00489f50);   // ListBox::setSelection
const R1_t kFieldSelect     = reinterpret_cast<R1_t>(0x004a29a0);   // field selector 0x004a29a0
const R1_t kTextPut         = reinterpret_cast<R1_t>(0x00477250);   // Text_put
const R0_t kListCurrentIdx  = reinterpret_cast<R0_t>(0x004941e0);   // listCurrentIndex
const R0_t kWidgetValue     = reinterpret_cast<R0_t>(0x00477580);   // Widget_value
const R4_t kWrapText        = reinterpret_cast<R4_t>(0x00476e20);   // wrapText
const R2_t kBeginMeasure    = reinterpret_cast<R2_t>(0x004785e0);   // TextView_beginMeasure
const R1_t kMeasureText4    = reinterpret_cast<R1_t>(0x00478610);   // measureText4
const R0_t kEndSegment      = reinterpret_cast<R0_t>(0x00478700);   // TextView_endSegment

// 0x0048fe60  TextView::render(this): recomputes the geometry of a TextView from its fields, measures its rows and
// publishes the two layout globals. It takes no stack argument (`ret` 0x0049092f) and reads `this` from ecx
// (`mov esi, ecx` 0x0048fe66).
//
// It opens by pushing the four quad records and the draw target of the OWNER's surface (owner = [this+0x5bc], surface
// = owner+0x274): Widget_setQuad 0x00476310 with ([+0x1fe0], [+0x1ff0], [+0x2000], [+0x2010]) (pushes 0x0048fe7b-
// 0x0048fe8a, call 0x0048fe91), Widget_setQuad70 0x00476340 with the +4 column (call 0x0048febe), Widget_setQuad74
// 0x00476370 with the +8 column (call 0x0048feeb), Widget_setQuad78 0x004763a0 with the +0xc column (call 0x0048ff18)
// and setDrawTarget 0x004762d0 with ([+0x1fd0], [+0x1fd4], [+0x1fd8], [+0x1fdc]) (call 0x0048ff45).
//
// Row count +0x1f70 = [+0x137c] (0x0048ff50), one more when bit 0x40 of the flag word +0x1f68 is set (`test cl, 0x40`
// 0x0048ff5c) and one more when NEITHER bit of 0x10100 is set (`test dword ptr [esi + 0x1f68], 0x10100; jne`
// 0x0048ff68 / 0x0048ff72).
// Spacing: a non-zero [+0x590] (0x0048ff82 / 0x0048ff84) gives +0x1fa0 = 0 (0x0048ff86) and +0x1fa4 = [+0x1454]
// (0x0048ffe8); otherwise, when bit 0x400 of the flags is clear (`test ah, 4` 0x0048ff94), the display 0x0083ad50 is
// not NULL (0x0048ff9f) and its vtable slot +0xa8 answers 0x400 or more (`cmp eax, 0x400; jl` 0x0048ffab /
// 0x0048ffb0), the pair is (3 * [+0x1450]) / 2 and (3 * [+0x1454]) / 2 (0x0048ffb8-0x0048ffd4); else [+0x1450] and
// [+0x1454] (0x0048ffdc / 0x0048ffe8). The two arms at 0x0048fff4 write +0x1fa8 / +0x1fac from [+0x1448] / [+0x144c]
// with the SAME values, so the test of [+0x590] at 0x0048fffa changes nothing.
// +0x1fb0 / +0x1fb4 = [+0x1440] / [+0x1444] (0x00490046 / 0x0049004c). With a record at [+0x13d0] (0x00490040):
// +0x1f98 = ([record+0x10] * [+0x1fc4]) / [+0x1fc8] + [+0x1fa0] + ([+0x1420] != 0 ? [+0x1fa4] : 0) (0x0049006c-
// 0x00490083) and +0x1f9c = ([record+0x14] * [+0x1fc4]) / [+0x1fc8] (0x00490089-0x00490094); without one both are 0
// (0x004900a0 / 0x004900a6).
//
// With [+0x590] non-zero (0x004900bb / 0x004900c1) the three setters run: TextView::setFontSize 0x0048e120 with
// [+0x59c] - 2 * [+0x1fa0] - [+0x1444] - [+0x1440] (call 0x004900e4), TextView::setLineSpacing 0x0048e0b0 with
// [+0x5a0] - [+0x1fac] - [+0x1fa8] - ([+0x1f70] != 0 ? [+0x1f90] : 0) (call 0x00490116) and TextView::setSize
// 0x0048e190 with ([+0x594], [+0x598]) (call 0x0049012b), then bits 0 and 1 are set in the dword at this + vb + 0x1488,
// vb = [[this+0x1464]+8] (0x00490139-0x0049014a). panelStop 0x004a11c0 runs on this+0x1460 (call 0x00490155).
//
// Width: with [+0x590] non-zero (0x00490160 / 0x0049016b) +0x1f7c = [this+vb+0x14c4], or [this+vb+0x1494] when that is
// 0 (0x0049016d-0x00490186), and +0x1f80 = [this+vb+0x14c8] or [this+vb+0x1498] (0x0049018c-0x004901b1). Otherwise
// (0x004901bc) +0x1f7c starts as [this+vb+0x14c4] (or [this+vb+0x1494] when 0, 0x004901cc); a value not above +0x1f98
// (`jle` 0x004901d9) is replaced by +0x1f98 (0x004901eb); a larger one is recomputed from the SAME two fields at
// 0x004901db-0x004901e6 (`[eax + 0x60]` with eax = this + vb + 0x1464 is [this+vb+0x14c4], `[eax + 0x30]` is
// [this+vb+0x1494]), so that arm leaves the value unchanged and the pair is max(width, +0x1f98). A non-zero [+0x164]
// (0x004901f9 / 0x004901fb) runs setDrawTarget on THIS
// object's surface this+0x274 with ([+0x164], 0, 0, 0) (call 0x0049020c), takes the larger of +0x1f7c and Text_put
// 0x00477250 of [+0x5c8] (calls 0x00490220 / 0x00490236, `jle` 0x00490227) and restores the owner's draw target (call
// 0x00490269). When the combo 0x0083fe08 is set and listCurrentIndex 0x004941e0 answers non-zero (0x00490276 /
// 0x00490281), Text_put of that text raised to 200 (`cmp eax, 0xc8; jge` 0x0049029c / 0x004902a1), plus +0x1f98 plus
// 0x32, is a lower bound for +0x1f7c (0x004902c3 / 0x004902cf). Finally [+0x145c] == -1 adds 2 * [+0x1fa0], any other
// value adds [+0x1fa0] + [+0x145c] (0x004902df-0x004902f9).
//
// +0x1f84 = [this+vb+0x14c8] (0x00490316). The switch on [+0x1654] + 1 (`cmp ecx, 9; ja` 0x0049031c / 0x0049031f,
// table 0x00490930): 2 stores 0x00490b30 at +0x14e4 (0x0049032c); 3 runs ListBox::selectById 0x00489ed0 on this+0x1460
// with [+0x5c4] when that is not -1, else ListBox::setSelection 0x00489f50 with (0, 1), and stores 0x00490b30 at
// +0x14b4 (0x0049033b-0x00490372); 5 runs the field selector 0x004a29a0 on this+0x15cc with [+0x5c4] when it is not -1
// (0x00490377-0x0049038d); 9 copies [+0x2020] / [+0x2024] / [+0x2028] to +0x15ac / +0x15c4 / +0x15a8 and [+0x1fc8] /
// [+0x1fc4] to +0x1564 / +0x1560 (0x00490394-0x004903ca); 0 does nothing (0x00490407); every other index runs
// ListModel::indexOfId 0x004898d0 on this+vb+0x1464 with [+0x5c4] when it is not -1, else clears [this+vb+0x1554], and
// stores 0x00490b30 at +0x14cc (0x004903d2-0x004903fd).
//
// Row walk: [+0x1420] >= 1 (`dec ecx; js` 0x0049040d / 0x0049040e) resets the cursor (+0x1424 = 0, +0x141c = [+0x1418],
// 0x00490416 / 0x00490420). The run width is -2 * [+0x1fa0] - [+0x1458] - [+0x1f98] + [+0x1f7c] (0x00490444-
// 0x0049044e). The first row's kind is [[[+0x141c]+8]+8] when [+0x1418] is set, else 0 (0x00490450-0x00490462);
// +0x1f94 starts at 0 (0x00490467). Kind 3 takes Widget_value 0x00477580 (call 0x0049047b), kind 2 wrapText 0x00476e20
// with (text, width, 0, 0) on the OWNER's surface (call 0x004904b1); either result is added to +0x1f94 (0x004904bd) and
// TextView_beginMeasure 0x004785e0 gets (0, width) (0x004904bc / 0x004904bf). Any other kind calls it with the row's
// text (0x004904dc / 0x004904dd); the call is at 0x004904ea. The loop (0x00490502..0x0049064e) runs [+0x1420] - 1
// times: it advances the node ([+0x141c] = [[+0x141c]+0xc], +0x1424 wrapped at [+0x1420], 0x0049050c-0x00490534),
// reads the row's text and kind, and for a non-zero kind adds TextView_endSegment 0x00478700 (call 0x0049058b), then
// wrapText on THIS object's surface for kind 2 (`lea ecx, [esi + 0x274]` 0x004905a9, call 0x004905af) or Widget_value
// for kind 3 (call 0x004905c7), adds that to +0x1f94 and calls TextView_beginMeasure with (0, width) (call 0x004905e9);
// any other non-zero kind calls it with the row's text (0x004905f0-0x0049060e). A zero kind calls measureText4
// 0x00478610 with the row's text (call 0x00490637).
// After the loop TextView_endSegment is added to +0x1f94 once more (call 0x00490660). With +0x1f70 non-zero
// (0x0049067d) +0x1f90 = (7 * [[+0x202c]+0x10]) / 4 (0x00490685-0x004906a1) and +0x1f8c = (-2 * [+0x1fa0] -
// ([+0x1f70] - 1) * [+0x1fa4] + [+0x1f7c]) / [+0x1f70] (0x004906a7-0x004906be); else both are 0 (0x004906c6 /
// 0x004906cc).
// With [+0x590] non-zero AND bit 1 of the flags set (0x004906d2 / 0x004906da) TextView::setLineSpacing gets
// [+0x5a0] - 2 * [+0x1fa4] - +0x1f94 - +0x1f90 (call 0x00490700) and panelStop runs again (call 0x0049070b): a
// NON-ZERO result leaves at once and IS the function's result (`jne 0x490928` 0x00490712).
// The virtual slot +0x18c of this is called (0x0049071c); bit 0x100 of the flags SET zeroes +0x1f90 (`test ah, 1; je`
// 0x00490728 / 0x0049072b skips the store at 0x0049072d when the bit is clear). With [+0x590] non-zero the three setters run again with the second set of operands
// (0x0049073f-0x004907bd), bits 0 and 1 are set at this+vb+0x1488 again (0x004907dc), and with [+0x1654] == -1
// (0x004907fb / 0x0049080a) +0x1f84 and [this+vb+0x14c8] are cleared (0x00490816 / 0x0049081f). With [+0x590] zero
// (0x0049082b) +0x1f80 = max(+0x1f94, +0x1f9c) + 2 * [+0x1fa0] + +0x1f84 + +0x1f90 (0x0049084f-0x00490858), plus
// [+0x1fa4] once for a non-zero +0x1f84 (0x00490856-0x00490866) and once more for a non-zero +0x1f90 (0x0049086c-
// 0x0049087e), plus [+0x1fac] + [+0x1fa8] (0x00490884-0x004908ae); +0x1f7c gains [+0x1fb4] + [+0x1fb0] (0x004908ac-
// 0x004908b6).
// The two globals: 0x0083b8dc = ([+0x5bc] != this ? [+0x1f74] : 0) + [+0x1fa0] + [+0x1fb0] + [+0x1f98] (0x004908bc-
// 0x004908e6) and 0x0083b8d8 = ([+0x5bc] != this ? [+0x1f78] : 0) + [+0x1f94] + [+0x1fa8] + [+0x1fa0] (0x004908eb-
// 0x00490919). The virtual slot +0x180 is called last (0x00490920) and the function returns 0 (`xor eax, eax`
// 0x00490926).
typedef int(__fastcall* TvRender_t)(void* ecx, void* edx);
TvRender_t TextViewRender_orig;
int __fastcall TextViewRender_re(void* self, void*) {
    char* S = static_cast<char*>(self);
    char* own = At<char*>(self, 0x5bc);
    void* osurf = own + 0x274;
    kWidgetSetQuad(osurf, 0, At<unsigned>(S, 0x1fe0), At<unsigned>(S, 0x1ff0), At<unsigned>(S, 0x2000),
                   At<unsigned>(S, 0x2010));                                   // 0x0048fe91
    kWidgetSetQuad70(osurf, 0, At<unsigned>(S, 0x1fe4), At<unsigned>(S, 0x1ff4), At<unsigned>(S, 0x2004),
                     At<unsigned>(S, 0x2014));                                 // 0x0048febe
    kWidgetSetQuad74(osurf, 0, At<unsigned>(S, 0x1fe8), At<unsigned>(S, 0x1ff8), At<unsigned>(S, 0x2008),
                     At<unsigned>(S, 0x2018));                                 // 0x0048feeb
    kWidgetSetQuad78(osurf, 0, At<unsigned>(S, 0x1fec), At<unsigned>(S, 0x1ffc), At<unsigned>(S, 0x200c),
                     At<unsigned>(S, 0x201c));                                 // 0x0048ff18
    kSetDrawTarget(osurf, 0, At<unsigned>(S, 0x1fd0), At<unsigned>(S, 0x1fd4), At<unsigned>(S, 0x1fd8),
                   At<unsigned>(S, 0x1fdc));                                   // 0x0048ff45

    At<int>(S, 0x1f70) = At<int>(S, 0x137c);                                   // 0x0048ff50
    if ((At<unsigned char>(S, 0x1f68) & 0x40) != 0) At<int>(S, 0x1f70) += 1;   // 0x0048ff5c / 0x0048ff61
    if ((At<unsigned>(S, 0x1f68) & 0x10100) == 0) At<int>(S, 0x1f70) += 1;     // 0x0048ff68 / 0x0048ff74

    if (At<int>(S, 0x590) != 0) {                                              // 0x0048ff82 / 0x0048ff84
        At<int>(S, 0x1fa0) = 0;                                                // 0x0048ff86
        At<int>(S, 0x1fa4) = At<int>(S, 0x1454);                               // 0x0048ffe8
    } else {
        void* disp = G<void*>(0x0083ad50);                                     // 0x0048ff99
        bool big = false;
        if ((At<unsigned>(S, 0x1f68) & 0x400) == 0 && disp != 0) {             // 0x0048ff94 / 0x0048ffa1
            int dw = reinterpret_cast<R0_t>(At<void**>(At<void*>(disp, 0), 0xa8))(disp, 0);   // 0x0048ffa5
            big = dw >= 0x400;                                                 // 0x0048ffab / 0x0048ffb0
        }
        if (big) {
            At<int>(S, 0x1fa0) = (At<int>(S, 0x1450) * 3) / 2;                 // 0x0048ffb8-0x0048ffc0
            At<int>(S, 0x1fa4) = (At<int>(S, 0x1454) * 3) / 2;                 // 0x0048ffc6-0x0048ffd4
        } else {
            At<int>(S, 0x1fa0) = At<int>(S, 0x1450);                           // 0x0048ffdc
            At<int>(S, 0x1fa4) = At<int>(S, 0x1454);                           // 0x0048ffe8
        }
    }
    At<int>(S, 0x1fa8) = At<int>(S, 0x1448);                                   // 0x0048fffc / 0x00490016
    At<int>(S, 0x1fac) = At<int>(S, 0x144c);                                   // 0x00490008 / 0x00490022

    void* rec = At<void*>(S, 0x13d0);                                          // 0x0049002e
    const int f1440 = At<int>(S, 0x1440);                                      // 0x00490034
    At<int>(S, 0x1fb0) = f1440;                                                // 0x00490046
    At<int>(S, 0x1fb4) = At<int>(S, 0x1444);                                   // 0x0049004c
    if (rec == 0) {                                                            // 0x00490040 / 0x00490052
        At<int>(S, 0x1f98) = 0;                                                // 0x004900a0
        At<int>(S, 0x1f9c) = 0;                                                // 0x004900a6
    } else {
        int extra = At<int>(S, 0x1420) != 0 ? At<int>(S, 0x1fa4) : 0;          // 0x00490054-0x00490064
        int num = At<int>(S, 0x1fc4), den = At<int>(S, 0x1fc8);                // 0x00490066 / 0x00490072
        At<int>(S, 0x1f98) = (At<int>(rec, 0x10) * num) / den + At<int>(S, 0x1fa0) + extra;   // 0x0049006f-0x00490083
        At<int>(S, 0x1f9c) = (At<int>(rec, 0x14) * num) / den;                 // 0x00490089-0x00490094
    }
    const int f202c10 = At<int>(At<void*>(S, 0x202c), 0x10);                   // 0x004900b2

    if (At<int>(S, 0x590) != 0) {                                              // 0x004900bb / 0x004900c1
        kSetFontSize(self, 0, static_cast<unsigned>(At<int>(S, 0x59c) - 2 * At<int>(S, 0x1fa0)
                                                    - At<int>(S, 0x1444) - f1440));   // 0x004900e4
        int ls = At<int>(S, 0x1f70) != 0 ? At<int>(S, 0x1f90) : 0;             // 0x004900e9-0x004900f9
        kSetLineSpacing(self, 0, static_cast<unsigned>(At<int>(S, 0x5a0) - At<int>(S, 0x1fac)
                                                       - At<int>(S, 0x1fa8) - ls));   // 0x00490116
        kSetSize(self, 0, At<unsigned>(S, 0x594), At<unsigned>(S, 0x598));     // 0x0049012b
        int vb0 = At<int>(At<void*>(S, 0x1464), 8);                            // 0x00490136
        At<unsigned>(S, vb0 + 0x1488) |= 3;                                    // 0x00490140 / 0x0049014a
    }
    kPanelStop(S + 0x1460, 0);                                                 // 0x00490155

    int vb = At<int>(At<void*>(S, 0x1464), 8);                                 // 0x00490168
    if (At<int>(S, 0x590) != 0) {                                              // 0x00490160 / 0x0049016b
        int a = At<int>(S, vb + 0x14c4);                                       // 0x0049016d
        At<int>(S, 0x1f7c) = a != 0 ? a : At<int>(S, vb + 0x1494);             // 0x0049017b-0x00490186
        int b = At<int>(S, vb + 0x14c8);                                       // 0x0049018f
        At<int>(S, 0x1f80) = b != 0 ? b : At<int>(S, vb + 0x1498);             // 0x0049019d-0x004901b1
    } else {
        int w = At<int>(S, vb + 0x14c4);                                       // 0x004901c3
        if (w == 0) w = At<int>(S, vb + 0x1494);                               // 0x004901cc / 0x004901ce
        int base = At<int>(S, 0x1f98);                                         // 0x004901d1
        if (w > base) {                                                        // 0x004901d7 / 0x004901d9
            int c = At<int>(S, vb + 0x14c4);                                   // 0x004901db (`[eax + 0x60]`, eax = this + vb + 0x1464)
            w = c != 0 ? c : At<int>(S, vb + 0x1494);                          // 0x004901de-0x004901e6
        } else {
            w = base;                                                          // 0x004901eb
        }
        At<int>(S, 0x1f7c) = w;                                                // 0x004901ed
        if (At<int>(S, 0x164) != 0) {                                          // 0x004901f9 / 0x004901fb
            void* msurf = S + 0x274;                                           // 0x004901ff
            kSetDrawTarget(msurf, 0, At<unsigned>(S, 0x164), 0, 0, 0);         // 0x0049020c
            int keep = At<int>(S, 0x1f7c);                                     // 0x00490217
            int t = kTextPut(msurf, 0, At<unsigned>(S, 0x5c8));                // 0x00490220
            int r = keep > t ? keep : kTextPut(msurf, 0, At<unsigned>(S, 0x5c8));   // 0x00490227 / 0x00490236
            At<int>(S, 0x1f7c) = r;                                            // 0x00490247
            kSetDrawTarget(osurf, 0, At<unsigned>(S, 0x1fd0), At<unsigned>(S, 0x1fd4), At<unsigned>(S, 0x1fd8),
                           At<unsigned>(S, 0x1fdc));                           // 0x00490269
        }
        int combo = 0;                                                         // 0x00490274
        void* cmb = G<void*>(0x0083fe08);                                      // 0x0049026e
        if (cmb != 0 && kListCurrentIdx(cmb, 0) != 0) {                        // 0x00490276-0x00490281
            void* msurf = S + 0x274;                                           // 0x00490289
            int cw = kTextPut(msurf, 0,
                              static_cast<unsigned>(kListCurrentIdx(G<void*>(0x0083fe08), 0)));   // 0x0049028f / 0x00490297
            if (cw < 0xc8) cw = 0xc8;                                          // 0x0049029c / 0x004902a3
            else cw = kTextPut(msurf, 0,
                               static_cast<unsigned>(kListCurrentIdx(G<void*>(0x0083fe08), 0)));  // 0x004902b0 / 0x004902b8
            combo = cw + At<int>(S, 0x1f98) + 0x32;                            // 0x004902c3
        }
        int wmax = At<int>(S, 0x1f7c);                                         // 0x004902c7
        if (wmax <= combo) wmax = combo;                                       // 0x004902cf / 0x004902d1
        At<int>(S, 0x1f7c) = wmax;                                             // 0x004902d9
        int pad = At<int>(S, 0x145c);                                          // 0x004902d3
        At<int>(S, 0x1f7c) = pad == -1 ? wmax + 2 * At<int>(S, 0x1fa0)
                                       : At<int>(S, 0x1fa0) + pad + wmax;      // 0x004902df-0x004902f9
    }

    vb = At<int>(At<void*>(S, 0x1464), 8);                                     // 0x00490305
    At<int>(S, 0x1f84) = At<int>(S, vb + 0x14c8);                              // 0x00490316
    unsigned sw = static_cast<unsigned>(At<int>(S, 0x1654) + 1);               // 0x0049030f / 0x00490315
    switch (sw) {                                                              // 0x0049031c / 0x00490325
        case 0:                                                                // 0x00490407
            break;
        case 2:
            At<unsigned>(S, 0x14e4) = 0x00490b30;                              // 0x0049032c
            break;
        case 3: {
            int id = At<int>(S, 0x5c4);                                        // 0x0049033b
            if (id != -1) kSelectById(S + 0x1460, 0, static_cast<unsigned>(id));   // 0x00490349
            else kSetSelection(S + 0x1460, 0, 0, 1);                           // 0x00490363
            At<unsigned>(S, 0x14b4) = 0x00490b30;                              // 0x0049034e / 0x00490368
            break;
        }
        case 5: {
            int id = At<int>(S, 0x5c4);                                        // 0x00490377
            if (id != -1) kFieldSelect(S + 0x15cc, 0, static_cast<unsigned>(id));   // 0x0049038d
            break;
        }
        case 9:
            At<int>(S, 0x15ac) = At<int>(S, 0x2020);                           // 0x0049039a
            At<int>(S, 0x15c4) = At<int>(S, 0x2024);                           // 0x004903a6
            At<int>(S, 0x15a8) = At<int>(S, 0x2028);                           // 0x004903b2
            At<int>(S, 0x1564) = At<int>(S, 0x1fc8);                           // 0x004903c4
            At<int>(S, 0x1560) = At<int>(S, 0x1fc4);                           // 0x004903ca
            break;
        default: {
            int id = At<int>(S, 0x5c4);                                        // 0x004903d2
            if (id != -1) kIndexOfId(S + vb + 0x1464, 0, static_cast<unsigned>(id));   // 0x004903e8
            else At<int>(S, vb + 0x1554) = 0;                                  // 0x004903f2
            At<unsigned>(S, 0x14cc) = 0x00490b30;                              // 0x004903fd
            break;
        }
    }

    if (At<int>(S, 0x1420) - 1 >= 0) {                                         // 0x0049040d / 0x0049040e
        At<int>(S, 0x1424) = 0;                                                // 0x00490416
        At<void*>(S, 0x141c) = At<void*>(S, 0x1418);                           // 0x00490420
    }
    const int width = -At<int>(S, 0x1fa0) * 2 - At<int>(S, 0x1458) - At<int>(S, 0x1f98)
                      + At<int>(S, 0x1f7c);                                    // 0x00490444-0x0049044e
    int kind0 = At<void*>(S, 0x1418) != 0
                    ? At<int>(At<void*>(At<void*>(S, 0x141c), 8), 8) : 0;      // 0x00490450-0x00490462
    At<int>(S, 0x1f94) = 0;                                                    // 0x00490467
    unsigned first = 0;
    if (kind0 == 3) {                                                          // 0x00490464 / 0x0049046d
        At<int>(S, 0x1f94) += kWidgetValue(osurf, 0);                          // 0x0049047b / 0x004904bd
    } else if (kind0 == 2) {                                                   // 0x00490482 / 0x0049048b
        unsigned txt = At<void*>(S, 0x1418) != 0
                           ? At<unsigned>(At<void*>(At<void*>(S, 0x141c), 8), 4) : 0;   // 0x0049048d-0x0049049f
        At<int>(S, 0x1f94) += kWrapText(osurf, 0, txt, static_cast<unsigned>(width), 0, 0);   // 0x004904b1 / 0x004904bd
    } else {
        first = At<void*>(S, 0x1418) != 0
                    ? At<unsigned>(At<void*>(At<void*>(S, 0x141c), 8), 4) : 0;  // 0x004904c8-0x004904da
    }
    kBeginMeasure(osurf, 0, first, static_cast<unsigned>(width));              // 0x004904ea

    for (int i = 0; i < At<int>(S, 0x1420) - 1; i++) {                         // 0x004904f9-0x004904fc / 0x0049064e
        void* head = At<void*>(S, 0x1418);                                     // 0x00490502
        if (head != 0) {                                                       // 0x00490508 / 0x0049050a
            int next = At<int>(S, 0x1424) + 1;                                 // 0x00490512 / 0x00490518
            At<void*>(S, 0x141c) = At<void*>(At<void*>(S, 0x141c), 0xc);       // 0x00490519 / 0x00490522
            At<int>(S, 0x1424) = next;                                         // 0x0049052c
            if (next == At<int>(S, 0x1420)) At<int>(S, 0x1424) = 0;            // 0x0049052a / 0x00490534
        }
        unsigned txt = 0;                                                      // 0x0049055b
        if (At<void*>(S, 0x141c) != 0 && head != 0)                            // 0x00490544 / 0x0049054a
            txt = At<unsigned>(At<void*>(At<void*>(S, 0x141c), 8), 4);         // 0x0049054c / 0x0049054f
        if (At<void*>(S, 0x1418) == 0) {                                       // 0x00490563 / 0x00490565
            kMeasureText4(osurf, 0, 0);                                        // 0x00490628 / 0x00490637
            continue;
        }
        int kind = At<int>(At<void*>(At<void*>(S, 0x141c), 8), 8);             // 0x00490571 / 0x00490574
        if (kind == 0) {                                                       // 0x00490577 / 0x00490579
            unsigned t = At<void*>(S, 0x1418) != 0
                             ? At<unsigned>(At<void*>(At<void*>(S, 0x141c), 8), 4) : 0;   // 0x00490610-0x00490626
            kMeasureText4(osurf, 0, t);                                        // 0x00490637
            continue;
        }
        At<int>(S, 0x1f94) += kEndSegment(osurf, 0);                           // 0x0049058b / 0x00490596
        if (kind == 2) {                                                       // 0x00490598 / 0x004905a1
            At<int>(S, 0x1f94) += kWrapText(S + 0x274, 0, txt, static_cast<unsigned>(width), 0, 0);   // 0x004905af
            kBeginMeasure(osurf, 0, 0, static_cast<unsigned>(width));          // 0x004905e9
        } else if (kind == 3) {                                                // 0x004905b6 / 0x004905b9
            At<int>(S, 0x1f94) += kWidgetValue(osurf, 0);                      // 0x004905c7 / 0x004905d3
            kBeginMeasure(osurf, 0, 0, static_cast<unsigned>(width));          // 0x004905e9
        } else {
            unsigned t = At<void*>(S, 0x1418) != 0
                             ? At<unsigned>(At<void*>(At<void*>(S, 0x141c), 8), 4) : 0;   // 0x004905f0-0x0049060d
            kBeginMeasure(osurf, 0, t, static_cast<unsigned>(width));          // 0x004905e9
        }
    }

    At<int>(S, 0x1f94) += kEndSegment(osurf, 0);                               // 0x00490660 / 0x0049066d
    const int measured = At<int>(S, 0x1f94);                                   // 0x00490675
    int rows = At<int>(S, 0x1f70);                                             // 0x00490677
    if (rows == 0) {                                                           // 0x0049067d / 0x0049067f
        At<int>(S, 0x1f90) = 0;                                                // 0x004906c6
        At<int>(S, 0x1f8c) = 0;                                                // 0x004906cc
    } else {
        At<int>(S, 0x1f90) = (f202c10 * 7) / 4;                                // 0x00490685-0x004906a1
        At<int>(S, 0x1f8c) = (-At<int>(S, 0x1fa0) * 2 - (rows - 1) * At<int>(S, 0x1fa4)
                              + At<int>(S, 0x1f7c)) / rows;                    // 0x004906a7-0x004906be
    }
    if (At<int>(S, 0x590) != 0 && (At<unsigned char>(S, 0x1f68) & 2) != 0) {   // 0x004906d2 / 0x004906da
        kSetLineSpacing(self, 0, static_cast<unsigned>(At<int>(S, 0x5a0) - 2 * At<int>(S, 0x1fa4)
                                                       - measured - At<int>(S, 0x1f90)));   // 0x00490700
        int r = kPanelStop(S + 0x1460, 0);                                     // 0x0049070b
        if (r != 0) return r;                                                  // 0x00490710 / 0x00490712
    }
    reinterpret_cast<R0_t>(At<void**>(At<void*>(S, 0), 0x18c))(self, 0);       // 0x0049071c
    if ((At<unsigned>(S, 0x1f68) & 0x100) != 0) At<int>(S, 0x1f90) = 0;        // 0x00490728 / 0x0049072b / 0x0049072d
    bool tail = At<int>(S, 0x590) == 0;                                        // 0x00490733 / 0x00490739
    if (!tail) {
        kSetFontSize(self, 0, static_cast<unsigned>(At<int>(S, 0x59c) - 2 * At<int>(S, 0x1fa0)
                                                    - At<int>(S, 0x1fb4) - At<int>(S, 0x1fb0)));   // 0x00490762
        int h = At<int>(S, 0x1f9c);                                            // 0x0049076d
        if (At<int>(S, 0x1f94) > h) h = At<int>(S, 0x1f94);                    // 0x00490773 / 0x00490777
        int ls = At<int>(S, 0x1f70) != 0 ? At<int>(S, 0x1f90) : 0;             // 0x00490779-0x00490789
        kSetLineSpacing(self, 0, static_cast<unsigned>(At<int>(S, 0x5a0) - At<int>(S, 0x1fac) - ls - h
                                                       - At<int>(S, 0x1fa8)));   // 0x004907a8
        kSetSize(self, 0, At<unsigned>(S, 0x594), At<unsigned>(S, 0x598));     // 0x004907bd
        int vb2 = At<int>(At<void*>(S, 0x1464), 8);                            // 0x004907c8
        At<unsigned>(S, vb2 + 0x1488) |= 3;                                    // 0x004907d9 / 0x004907dc
        // 0x004907df / 0x004907e5 / 0x004907e7 re-test +0x590 and jump to the block at 0x0049082b when it is 0: the
        // three setters above do not write +0x590, so this side is not reached from here.
        if (At<int>(S, 0x590) == 0) {
            tail = true;
        } else {
            At<int>(S, 0x1f7c) = At<int>(S, 0x59c);                            // 0x004907fe
            At<int>(S, 0x1f80) = At<int>(S, 0x5a0);                            // 0x00490804
            if (At<int>(S, 0x1654) == -1) {                                    // 0x004907fb / 0x0049080a
                At<int>(S, 0x1f84) = 0;                                        // 0x00490816
                At<int>(S, At<int>(At<void*>(S, 0x1464), 8) + 0x14c8) = 0;     // 0x0049081c / 0x0049081f
            }
        }
    }
    if (tail) {
        int h = At<int>(S, 0x1f9c);                                            // 0x00490831
        if (At<int>(S, 0x1f94) > h) h = At<int>(S, 0x1f94);                    // 0x00490837 / 0x0049083b
        int f84 = At<int>(S, 0x1f84), f90 = At<int>(S, 0x1f90);                // 0x00490843 / 0x00490849
        int v = h + 2 * At<int>(S, 0x1fa0) + f84 + f90;                        // 0x0049084f-0x00490854
        At<int>(S, 0x1f80) = v;                                                // 0x00490858
        if (f84 != 0) { v += At<int>(S, 0x1fa4); At<int>(S, 0x1f80) = v; }     // 0x00490856-0x00490866
        if (f90 != 0) At<int>(S, 0x1f80) += At<int>(S, 0x1fa4);                // 0x0049086c-0x0049087e
        At<int>(S, 0x1f80) += At<int>(S, 0x1fac) + At<int>(S, 0x1fa8);         // 0x00490884-0x004908ae
        At<int>(S, 0x1f7c) += At<int>(S, 0x1fb4) + At<int>(S, 0x1fb0);         // 0x004908ac-0x004908b6
    }
    int o1 = At<char*>(S, 0x5bc) != S ? At<int>(S, 0x1f74) : 0;                // 0x004908bc-0x004908cc
    G<int>(0x0083b8dc) = o1 + At<int>(S, 0x1fa0) + At<int>(S, 0x1fb0) + At<int>(S, 0x1f98);   // 0x004908e6
    int o2 = At<char*>(S, 0x5bc) != S ? At<int>(S, 0x1f78) : 0;                // 0x004908eb-0x004908fd
    G<int>(0x0083b8d8) = o2 + At<int>(S, 0x1f94) + At<int>(S, 0x1fa8) + At<int>(S, 0x1fa0);   // 0x00490919
    reinterpret_cast<R0_t>(At<void**>(At<void*>(S, 0), 0x180))(self, 0);       // 0x00490920
    return 0;                                                                  // 0x00490926
}

}  // namespace

// 0x00487090  BinkPlayer::play
SG_HOOK("golf_clean.exe", 0x00487090, BinkPlayer_play, BinkPlayerPlay_re, BinkPlayerPlay_orig);
// 0x0043cce0  playAudioFile
SG_HOOK("golf_clean.exe", 0x0043cce0, playAudioFile, PlayAudioFile_re, PlayAudioFile_orig);
// 0x0048fe60  TextView::render
SG_HOOK("golf_clean.exe", 0x0048fe60, TextView_render, TextViewRender_re, TextViewRender_orig);
