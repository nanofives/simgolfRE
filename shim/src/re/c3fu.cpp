// C3 batch c3fu (round 35, 2026-10-14): rows round 33 classed NO_OBSERVABLE or NO_CALLER that re/tools/endstate.py shows
// write a global, return a field or have a reference (log/c3/c3fu_candidates.tsv). The caller or installer of every row is
// measured in log/c3/c3fu_notes.txt (step 1). Registry: re/frida/registry.d/c3fu.py. Fixtures: re/frida/js/fixtures.d/c3fu.js.
// Purposes and per-jcc tables: log/c3/c3fu_purpose.md.
//
// Each body is written from the disassembly (log/c3/alt/c3fu_dis.py <module> <lo> <hi>) and cites the instructions it
// reproduces. golf_clean.exe addresses are VAs (no relocations). sound.dll addresses in SG_HOOK and in the `// 0x...`
// lines are RVAs; instruction addresses quoted in the comments are VAs as the disassembly prints them (image base
// 0x10000000). sound.dll has base relocations, so every sound.dll global, thunk and immediate is computed from the loaded
// base (Snd()). __thiscall is emulated with __fastcall (ecx = this, edx unused). Callees run through the addresses the
// originals call (a callee another batch hooks runs its reimplementation; the verifier re-runs these keys with those hooks
// off). Imports are called through the module's own IAT slot, as the original does.
//
// Hooked callees of other batches (re-run with them off):
//   golf_clean.exe: StrList::dtor 0x00487260 (c3bx), Snd::stop484f00 0x00484f00 (c3ev), shutdownSound 0x00483f10 (c3eu),
//     shutdownBlitter 0x00492470 (c3dq), Timer::lockDec 0x00486ff0 (c3cz), freeFonts 0x0049ff30 (c3cz), freeCursors
//     0x0049ead0 (c3dl), clearPrimarySurface 0x00483b10 (c3dw), freeGfxGlobal_840710 0x0049d280 (c3dq), freeFileWinBuf
//     0x0049d460 (c3cz), MsgBox::freeLabels 0x00490c30 (c3dw), screenTick47d010 0x0047d010 (c3dq), clearScreenObjects
//     0x00483340 (c3dq), strFree 0x0049d0e0 (c3cn), freeGraphsyObject 0x004a0060 (c3dw), EditBox::lineIndexAt 0x00486330
//     (c3s), drawMarkupRun 0x00477280 (c3o), Widget_value 0x00477580 (c3c), Surface_pixelPtr 0x004796a0 (c3p)
//   sound.dll: SoundWindowObj_dtor 0x286a0 (c3dt)
// No function of this batch calls another function of this batch.
//
// Negative controls: shim/src/re/c3fu_negctl.cpp includes this file with C3FU_NEGCTL defined (built only through
// log/c3/c3fu_negctl_batches.txt, never listed in shim/re_batches.txt). NEG(k, right, wrong) picks `wrong` when the
// exported switch C3fuNegCtl[0] is armed (re/frida/js/freeze.js negOn, after the key froze the first thread) and
// SIMGOLF_NEGSEL equals k. In this file NEG is always `right`.
#include "hooks.h"

#ifndef NEG
#define NEG(k, right, wrong) (right)
#endif
#ifndef C3FU_NULL
#define C3FU_NULL() 0
#endif

namespace {

template <class T> T& At(void* p, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(p) + off); }
template <class T> T& G(unsigned va) { return *reinterpret_cast<T*>(va); }
template <class F> F VSlot(void* obj, unsigned off) { return reinterpret_cast<F>((*reinterpret_cast<void***>(obj))[off / 4]); }
static inline char* B(void* p) { return static_cast<char*>(p); }

// sound.dll is relocatable: RVA -> loaded address.
char* Snd(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}

typedef void(__fastcall* Void0_t)(void* ecx, void* edx);
typedef int(__fastcall* Int0_t)(void* ecx, void* edx);
typedef void(__fastcall* Void1_t)(void* ecx, void* edx, int a);
typedef int(__cdecl* CInt0_t)();
typedef void(__cdecl* CVoid0_t)();
typedef unsigned(__fastcall* U0_t)(void* ecx, void* edx);

// =====================================================================================================================
// golf_clean.exe
// =====================================================================================================================

// 0x004627d0  resetSpriteQueue: argument-free cdecl (`ret` 0x004627f8); it reads no register before writing it (eax at
// 0x004627d0, ecx at 0x004627d2). eax = 0 (0x004627d0), ecx = -1 (0x004627d2); [0x838200] = 0 (0x004627d5), [0x834174] = 0
// (0x004627da), [0x8371fc] = 0 (0x004627df), [0x82c160] = -1 (0x004627e4); eax = 0x400 (0x004627ea), then 0x400 times
// `dec eax; mov [0x8371f4], ecx; jne` (0x004627ef..0x004627f6): the SAME dword 0x8371f4 receives -1 on every pass, nothing
// after it is written. eax is 0 at the `ret`.
CInt0_t ResetSpriteQueue_orig;
int __cdecl ResetSpriteQueue_re() {
    if (C3FU_NULL()) return 0;
    G<int>(0x00838200) = NEG(1, 0, 1);                                        // 0x004627d5
    G<int>(0x00834174) = 0;                                                   // 0x004627da
    G<int>(0x008371fc) = 0;                                                   // 0x004627df
    G<int>(0x0082c160) = -1;                                                  // 0x004627e4
    for (int n = 0x400; n != 0; --n) G<int>(NEG(2, 0x008371f4u, 0x008371f8u)) = -1;   // 0x004627ea..0x004627f6
    return 0;                                                                 // eax 0 since 0x004627ef's last dec
}

// 0x00487a60  NodeListB::dtor: __thiscall, no stack argument. Stores the NodeListB vtable 0x004bafb0 at this+0
// (0x00487a60) and tail-jumps to StrList::dtor 0x00487260 (0x00487a66) with ecx = this. StrList::dtor's first instruction
// stores 0x004baeac over the same +0 (0x00487260) before anything reads it; then it loads [this+0x14] (0x00487266) and, when
// it is not NULL (0x00487269 / 0x0048726b), tail-jumps to slot +0x14 of that object's vtable with ecx = the object
// (0x0048726d / 0x0048726f). eax is never written by this body (void).
Void0_t NodeListBDtor_orig;
void __fastcall NodeListBDtor_re(void* self, void*) {
    if (C3FU_NULL()) return;
    At<unsigned>(self, 0) = 0x004bafb0;                                       // 0x00487a60
    if (NEG(3, 0, 1)) return;
    reinterpret_cast<Void0_t>(0x00487260)(self, 0);                           // 0x00487a66 jmp StrList::dtor
}

// 0x00487b80  NodeListC::dtor: the same shape with the NodeListC vtable 0x004bb014 (0x00487b80) and the tail jump to
// StrList::dtor 0x00487260 at 0x00487b86.
Void0_t NodeListCDtor_orig;
void __fastcall NodeListCDtor_re(void* self, void*) {
    if (C3FU_NULL()) return;
    At<unsigned>(self, 0) = 0x004bb014;                                       // 0x00487b80
    if (NEG(4, 0, 1)) { At<void*>(self, 0x14) = 0; }
    reinterpret_cast<Void0_t>(0x00487260)(self, 0);                           // 0x00487b86 jmp StrList::dtor
}

// 0x004490b0  shutdownSoundSystem: argument-free cdecl (`ret` 0x004490ce); esi = 0x80d840 (0x004490b1); for each record,
// Snd::stop484f00 0x00484f00 with ecx = esi (0x004490b6 / 0x004490b8), esi += 0x6c (0x004490bd), repeat while
// esi < 0x8156d0 (signed `jl`, 0x004490c0 / 0x004490c6): 300 records 0x80d840 .. 0x815664. Then shutdownSound 0x00483f10
// (0x004490c8); its eax is left in eax at the `ret`.
CInt0_t ShutdownSoundSystem_orig;
int __cdecl ShutdownSoundSystem_re() {
    if (C3FU_NULL()) return 0;
    char* r = reinterpret_cast<char*>(0x0080d840);                            // 0x004490b1
    do {
        reinterpret_cast<Int0_t>(0x00484f00)(r, 0);                           // 0x004490b8
        r += NEG(5, 0x6c, 0xd8);                                              // 0x004490bd
    } while (reinterpret_cast<int>(r) < 0x008156d0);                          // 0x004490c0 / 0x004490c6 jl
    return reinterpret_cast<CInt0_t>(0x00483f10)();                           // 0x004490c8
}

// 0x00485740  engineShutdown: argument-free cdecl; thirteen calls in this order, none with an argument and none of whose
// callees reads ecx before writing it (their entries: 0x00492470 call, 0x00486ff0 dec [mem], 0x004a0890 ret, 0x0049ff30
// mov ecx, imm, 0x0049ead0 mov ecx, imm, 0x00483b10 mov ecx, [mem], 0x0049d280 mov ecx, imm, 0x0049d460 mov eax, [mem],
// 0x00490c30 mov eax, [mem], 0x0047d010 mov ecx, [mem], 0x00483340 mov eax, [mem]): shutdownBlitter 0x00492470 (0x00485740),
// Timer::lockDec 0x00486ff0 (0x00485745), the empty 0x004a0890 twice (0x0048574a, 0x0048574f), freeFonts 0x0049ff30
// (0x00485754), freeCursors 0x0049ead0 (0x00485759), clearPrimarySurface 0x00483b10 (0x0048575e), freeGfxGlobal_840710
// 0x0049d280 (0x00485763), freeFileWinBuf 0x0049d460 (0x00485768), MsgBox::freeLabels 0x00490c30 (0x0048576d), 0x004a0890
// again (0x00485772), screenTick47d010 0x0047d010 (0x00485777), clearScreenObjects 0x00483340 (0x0048577c); then strFree
// 0x0049d0e0 with ecx = 0x8406e8 (0x00485781 / 0x00485786) and a tail jump to freeGraphsyObject 0x004a0060 (0x0048578b),
// whose eax is returned (it is 0 or FreeLibrary's result: `mov eax, [0x84092c]` 0x004a006f, `call [0x4ba0ac]` 0x004a0079).
CInt0_t EngineShutdown_orig;
int __cdecl EngineShutdown_re() {
    if (C3FU_NULL()) return 0;
    reinterpret_cast<CVoid0_t>(0x00492470)();                                 // 0x00485740 shutdownBlitter
    reinterpret_cast<CVoid0_t>(0x00486ff0)();                                 // 0x00485745 Timer::lockDec
    reinterpret_cast<CVoid0_t>(0x004a0890)();                                 // 0x0048574a
    reinterpret_cast<CVoid0_t>(0x004a0890)();                                 // 0x0048574f
    reinterpret_cast<CVoid0_t>(0x0049ff30)();                                 // 0x00485754 freeFonts
    reinterpret_cast<CVoid0_t>(NEG(6, 0x0049ead0u, 0x0049d280u))();           // 0x00485759 freeCursors
    reinterpret_cast<CVoid0_t>(0x00483b10)();                                 // 0x0048575e clearPrimarySurface
    reinterpret_cast<CVoid0_t>(0x0049d280)();                                 // 0x00485763 freeGfxGlobal_840710
    reinterpret_cast<CVoid0_t>(0x0049d460)();                                 // 0x00485768 freeFileWinBuf
    reinterpret_cast<CVoid0_t>(0x00490c30)();                                 // 0x0048576d MsgBox::freeLabels
    reinterpret_cast<CVoid0_t>(0x004a0890)();                                 // 0x00485772
    reinterpret_cast<CVoid0_t>(0x0047d010)();                                 // 0x00485777 screenTick47d010
    reinterpret_cast<CVoid0_t>(0x00483340)();                                 // 0x0048577c clearScreenObjects
    reinterpret_cast<Int0_t>(0x0049d0e0)(reinterpret_cast<void*>(0x008406e8), 0);   // 0x00485781 / 0x00485786 strFree
    if (NEG(7, 0, 1)) return 0;
    return reinterpret_cast<CInt0_t>(0x004a0060)();                           // 0x0048578b jmp freeGraphsyObject
}

// 0x00486360  EditBox::charPosToPixel(lines, n, pos, outX, outY): __thiscall, `ret 0x14` (0x004863d7). Stack arguments, in
// the original's frame: lines = [esp+4] (`mov ebx, [esp+8]` after one push, 0x00486365), n = [esp+8] (`mov eax, [esp+8]`
// 0x00486360), pos = [esp+0xc] (`mov ebp, [esp+0x14]` after two pushes, 0x0048636a), outX = [esp+0x10] (`mov ebp, [esp+0x20]`
// after four pushes, 0x00486398), outY = [esp+0x14] (`mov ecx, [esp+0x24]`, 0x004863ac).
//   idx = EditBox::lineIndexAt(this, pos, lines, n) (pushes n, lines, pos: 0x00486370..0x00486375);
//   start = lines[idx] (0x00486382); len = [this+0x574] - start + pos (0x0048637a, 0x0048638b, 0x0048638d);
//   *outX = drawMarkupRun(this + 0x274, start, len) (pushes len, start; ecx = this + 0x274: 0x00486385..0x0048639e);
//   *outY = Widget_value(this + 0x274) * (idx - [this+0x57c]) (0x004863a1..0x004863b5, imul);
//   m = [this+0x59c] + [this+0x598] (0x004863b7..0x004863c6); *outX += m (0x004863c3..0x004863cb), then *outY += m reading
//   *outY back (0x004863ce..0x004863d4); eax = m at the `ret`.
typedef int(__fastcall* LineIndexAt_t)(void* ecx, void* edx, int pos, char** lines, int n);
typedef int(__fastcall* DrawMarkupRun_t)(void* ecx, void* edx, const char* start, int len);
Int0_t CharPosToPixel_orig;
int __fastcall CharPosToPixel_re(void* self, void*, char** lines, int n, int pos, int* outX, int* outY) {
    if (C3FU_NULL()) return 0;
    int idx = reinterpret_cast<LineIndexAt_t>(0x00486330)(self, 0, pos, lines, n);     // 0x00486375
    char* start = lines[idx];                                                 // 0x00486382
    int len = static_cast<int>(At<char*>(self, 0x574) - start) + pos;         // 0x0048637a / 0x0048638b / 0x0048638d
    void* w = B(self) + 0x274;                                                // 0x00486385
    *outX = reinterpret_cast<DrawMarkupRun_t>(0x00477280)(w, 0, start, NEG(8, len, len + 1));   // 0x00486393 / 0x0048639e
    int h = reinterpret_cast<Int0_t>(0x00477580)(w, 0);                       // 0x004863a1 Widget_value
    *outY = h * (idx - At<int>(self, 0x57c));                                 // 0x004863a6..0x004863b5
    int m = At<int>(self, 0x59c) + At<int>(self, NEG(9, 0x598, 0x59c));       // 0x004863b7..0x004863c6
    *outX = *outX + m;                                                        // 0x004863c3..0x004863cb
    *outY = *outY + m;                                                        // 0x004863ce..0x004863d4
    return m;
}

// 0x00478ea0  Widget_writePixel(x, y, colour): __thiscall, `ret 0xc` on every path (0x00478eb4, 0x00478f24, 0x00478f4a);
// x = [esp+4], y = [esp+8], colour = [esp+0xc] (read after three pushes at 0x00478eb7 / 0x00478ec3 / 0x00478f27,
// 0x00478f31). The Surface is [this+4] (0x00478ea5):
//   NULL -> 7 (0x00478ea8 / 0x00478eae);
//   x < 0 or y < 0 -> 0 (signed `jl` 0x00478ebd / 0x00478ec9);
//   x >= its slot +0xd8 (0x00478ecd, signed `jge` 0x00478ed5) -> 0;
//   y >= its slot +0xdc (0x00478ee0, signed `jge` 0x00478eec) -> 0; the slot is re-read through [this+4] and a NULL there
//     would compare with 0 (0x00478ed7..0x00478ee8);
//   p = Surface_pixelPtr(this, x, y) (pushes y, x; ecx = this: 0x00478eee..0x00478ef2); when p is not NULL (0x00478ef9), the
//     format record is slot +0xe4's result (0x00478f06, NULL Surface -> 0, 0x00478f0e) and its first dword selects: 8 -> the
//     low byte of colour at p (0x00478f12 / 0x00478f15, 0x00478f31 / 0x00478f35), 0x10 -> the low word (0x00478f17 /
//     0x00478f1a, 0x00478f27 / 0x00478f2c), anything else -> 0x17 at once, without the call below (0x00478f1c..0x00478f24);
//   then, with p NULL or after the store, slot +0x24 of [this+4] (when not NULL) with the argument 1 (0x00478f37..0x00478f42)
//   and 0 (0x00478f47).
typedef unsigned char*(__fastcall* PixelPtr_t)(void* ecx, void* edx, int x, int y);
typedef int*(__fastcall* FmtSlot_t)(void* ecx, void* edx);
typedef int(__fastcall* Unlock_t)(void* ecx, void* edx, int a);
Int0_t WidgetWritePixel_orig;
int __fastcall WidgetWritePixel_re(void* self, void*, int x, int y, unsigned colour) {
    if (C3FU_NULL()) return 0;
    void* s = At<void*>(self, 4);                                             // 0x00478ea5
    if (s == 0) return 7;                                                     // 0x00478ea8 / 0x00478eae
    if (x < 0) return 0;                                                      // 0x00478ebd
    if (y < 0) return 0;                                                      // 0x00478ec9
    if (x >= VSlot<Int0_t>(s, 0xd8)(s, 0)) return 0;                          // 0x00478ecd / 0x00478ed5
    s = At<void*>(self, 4);                                                   // 0x00478ed7
    int h = s ? VSlot<Int0_t>(s, 0xdc)(s, 0) : 0;                             // 0x00478ee0 / 0x00478ee8
    if (y >= NEG(10, h, h + 1)) return 0;                                     // 0x00478eea / 0x00478eec
    unsigned char* p = reinterpret_cast<PixelPtr_t>(0x004796a0)(self, 0, x, y);   // 0x00478ef2
    if (p != 0) {                                                             // 0x00478ef9 / 0x00478efb
        s = At<void*>(self, 4);                                               // 0x00478efd
        int* fmt = s ? VSlot<FmtSlot_t>(s, 0xe4)(s, 0) : 0;                   // 0x00478f06 / 0x00478f0e
        int depth = *fmt;                                                     // 0x00478f10
        if (depth == 8) {                                                     // 0x00478f12 / 0x00478f15
            *p = static_cast<unsigned char>(colour);                          // 0x00478f31 / 0x00478f35
        } else if (depth == NEG(11, 0x10, 0x18)) {                            // 0x00478f17 / 0x00478f1a
            *reinterpret_cast<unsigned short*>(p) = static_cast<unsigned short>(colour);   // 0x00478f27 / 0x00478f2c
        } else {
            return 0x17;                                                      // 0x00478f1e
        }
    }
    s = At<void*>(self, 4);                                                   // 0x00478f37
    if (s) VSlot<Unlock_t>(s, 0x24)(s, 0, 1);                                 // 0x00478f3a..0x00478f42
    return 0;                                                                 // 0x00478f47
}

// =====================================================================================================================
// sound.dll
// =====================================================================================================================

// 0x0003a770  getDsCoopLevel (VA 0x1003a770): slot 15 (+0x3c) of the MultiSound vtable 0x1005c7e8. `mov eax, [0x100b4a3c]`
// (0x1003a770), `ret` (0x1003a775): ecx is never read.
Int0_t GetDsCoopLevel_orig;
int __fastcall GetDsCoopLevel_re(void*, void*) {
    if (C3FU_NULL()) return 0;
    return *reinterpret_cast<int*>(Snd(NEG(12, 0xb4a3c, 0xb4a38)));           // 0x1003a770
}

// 0x0000e2d0  getWaveDeviceInterface (VA 0x1000e2d0): slot 29 (+0x74) of the Wave_Device vtable 0x1005b304. eax = the
// global 0x100b49f0 (0x1000e2d0); NULL -> 0 (0x1000e2d5 / 0x1000e2d7, 0x1000e2dc); else the dword it points at (0x1000e2d9).
// ecx is never read.
U0_t GetWaveDeviceInterface_orig;
unsigned __fastcall GetWaveDeviceInterface_re(void*, void*) {
    if (C3FU_NULL()) return 0;
    unsigned* d = *reinterpret_cast<unsigned**>(Snd(0xb49f0));                // 0x1000e2d0
    if (d == 0) return NEG(13, 0u, 1u);                                       // 0x1000e2d5 / 0x1000e2dc
    return *d;                                                                // 0x1000e2d9
}

// 0x00042506  cfltcvtInit (VA 0x10042506): argument-free, `ret` (0x1004253d); eax = 0x1004477c (0x10042506); six stores in
// this order: [0x100651bc] = 0x10044426 (0x1004250b), [0x100651b8] = eax (0x10042515), [0x100651c0] = 0x1004448c
// (0x1004251a), [0x100651c4] = 0x100443cc (0x10042524), [0x100651c8] = 0x10044474 (0x1004252e), [0x100651cc] = eax
// (0x10042538). The immediates are relocated with the module (base + RVA); eax keeps base + 0x4477c at the `ret`.
U0_t CfltcvtInit_orig;
unsigned __cdecl CfltcvtInit_re() {
    if (C3FU_NULL()) return 0;
    unsigned a = reinterpret_cast<unsigned>(Snd(0x4477c));                   // 0x10042506
    *reinterpret_cast<unsigned*>(Snd(0x651bc)) = reinterpret_cast<unsigned>(Snd(0x44426));   // 0x1004250b
    *reinterpret_cast<unsigned*>(Snd(0x651b8)) = a;                           // 0x10042515
    *reinterpret_cast<unsigned*>(Snd(0x651c0)) = reinterpret_cast<unsigned>(Snd(0x4448c));   // 0x1004251a
    *reinterpret_cast<unsigned*>(Snd(0x651c4)) = reinterpret_cast<unsigned>(Snd(NEG(14, 0x443cc, 0x44474)));   // 0x10042524
    *reinterpret_cast<unsigned*>(Snd(0x651c8)) = reinterpret_cast<unsigned>(Snd(0x44474));   // 0x1004252e
    *reinterpret_cast<unsigned*>(Snd(0x651cc)) = a;                           // 0x10042538
    return a;
}

// 0x000091f0  atexitDtor_100b49b8 (VA 0x100091f0): `mov ecx, 0x100b49b8` (0x100091f0), `jmp 0x10002784` (0x100091f5), the
// incremental-link thunk of SoundWindowObj_dtor 0x100286a0; that body's eax is the result. Registered with _atexit by the
// dynamic initializer 0x100091c0 (`push 0x100091f0` 0x100091ca, `call 0x10042647` 0x100091cf).
U0_t AtexitDtor100b49b8_orig;
unsigned __cdecl AtexitDtor100b49b8_re() {
    if (C3FU_NULL()) return 0;
    void* obj = Snd(NEG(15, 0xb49b8, 0xb49bc));                               // 0x100091f0
    return reinterpret_cast<U0_t>(Snd(0x2784))(obj, 0);                       // 0x100091f5 jmp thunk -> 0x100286a0
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x004627d0, resetSpriteQueue, ResetSpriteQueue_re, ResetSpriteQueue_orig);
SG_HOOK("golf_clean.exe", 0x00487a60, NodeListB_dtor, NodeListBDtor_re, NodeListBDtor_orig);
SG_HOOK("golf_clean.exe", 0x00487b80, NodeListC_dtor, NodeListCDtor_re, NodeListCDtor_orig);
SG_HOOK("golf_clean.exe", 0x004490b0, shutdownSoundSystem, ShutdownSoundSystem_re, ShutdownSoundSystem_orig);
SG_HOOK("golf_clean.exe", 0x00485740, engineShutdown, EngineShutdown_re, EngineShutdown_orig);
SG_HOOK("golf_clean.exe", 0x00486360, EditBox_charPosToPixel, CharPosToPixel_re, CharPosToPixel_orig);
SG_HOOK("golf_clean.exe", 0x00478ea0, Widget_writePixel, WidgetWritePixel_re, WidgetWritePixel_orig);
SG_HOOK("sound.dll", 0x0003a770, getDsCoopLevel, GetDsCoopLevel_re, GetDsCoopLevel_orig);
SG_HOOK("sound.dll", 0x0000e2d0, getWaveDeviceInterface, GetWaveDeviceInterface_re, GetWaveDeviceInterface_orig);
SG_HOOK("sound.dll", 0x00042506, cfltcvtInit, CfltcvtInit_re, CfltcvtInit_orig);
SG_HOOK("sound.dll", 0x000091f0, atexitDtor_100b49b8, AtexitDtor100b49b8_re, AtexitDtor100b49b8_orig);
