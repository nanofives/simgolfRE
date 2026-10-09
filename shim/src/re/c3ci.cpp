// Batch c3ci: the jgld.dll Sprite methods that are reached only through a slot of the Sprite vtable at
// RVA 0x11d380 (VA 0x1011d380), which Sprite::ctor 0x10014af0, Sprite::ctorN 0x10014cd0 and Sprite::dtor
// 0x10014de0 store at +0 of every Sprite. SG_HOOK addresses are RVAs; every instruction cited in a comment is
// a VA (0x10000000 + RVA), the way the debug build's disassembly prints it. jgld.dll is LoadLibrary'd after
// the shim starts, so these hooks install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp)
// and diff_hook finds them by (module, rva) through SimGolfShim_FindHookM.
//
// __thiscall is emulated with __fastcall: ecx = this, edx unused.
//
// Sprite fields, each with the instruction that proves the offset:
//   +0x10  Palette pointer 0x10017eb0 writes it (VA 0x10017ed3), 0x10017ef0 reads it (VA 0x10017f10); every
//                          dispatcher and every forwarder of this batch saves it, overwrites it with its last
//                          argument when that is not zero, and (the dispatchers only) restores it afterwards.
//                          That the word is a Palette and not a colour was measured: with 0x33 stored there,
//                          the blitter Sprite::draw16_100298b0 that blt16a forwards to faults reading 0x33
//                          (log/c3/alt/c3ci_bisect.py, vectors 1 and 16 of the first Sprite::blt16a run),
//                          and the blitters take their colour table from [sprite + 0x10] + 0x40c / + 0x60c.
//                          Neither slot 4 nor any of the bodies here tests the value before storing it.
//   +0x14  payload base    0x10017f70 returns it (VA 0x10017f90); slot 8 (+0x20 of the vtable) is this
//                          function, and every dispatcher uses it as the sprite's validity test
//   +0x18  flag word       0x100180e0 returns bit 0 of it (VA 0x10018100 / 0x10018103); slot 6 (+0x18 of the
//                          vtable) is this function, and the dispatchers call it as isBusy
//   +0x20  bits per pixel  0x10017fe0 returns it (VA 0x10018000); the outer switch of every dispatcher
//   +0x24  span byte count remap8 0x1001bae0 uses it as the payload length when bit 0 of +0x18 is set
//   +0x28  a field slot 7 returns (0x10017f30, VA 0x10017f50)
//   +0x2c  payload pitch   0x10018020 returns it (VA 0x10018040)
//   +0x30  payload width   0x10018060 returns it (VA 0x10018080)
//   +0x34  payload height  0x100180a0 returns it (VA 0x100180c3)
//
// THE DEBUG REPORTS. Every arm of a dispatcher that does not reach a blitter is a VC6 `_RPTF0(_CRT_WARN, ...)`,
// which expands to six pushes and a call to _CrtDbgReport 0x10081f90 followed by `cmp eax, 1 / int3`
// (0x1001523d.. and its forty twins). The arguments are (_CRT_WARN = 0, the file-name string 0x1011d508, the
// line, NULL, the format string 0x1011d528, the message string); the line is `movsx <reg>, word ptr [<base>]`
// plus a per-report constant, because /ZI stores each compilation chunk's line base in a module word. With
// _CrtDbgMode[_CRT_WARN] = 2 (_CRTDBG_MODE_DEBUG, the value in jgld.dll's .data at RVA 0x123378) the report
// writes the text with OutputDebugStringA (VA 0x10082283) and returns 0 through `xor eax, eax` (VA 0x10082319),
// so the `int3` is never reached; the reimplementation calls the same function with the same arguments and
// makes the same test.
//
// THE UNINITIALISED RESULT. Sprite::draw 0x15180, drawF 0x16270 and drawH 0x169a0 declare their result local
// at [ebp - 0xc] and never store to it on a report-only arm, so what they return there is the /GZ frame fill
// 0xcccccccc the prologue wrote (`mov eax, 0xcccccccc; rep stosd`, VA 0x10015192 / 0x10016282 / 0x100169b2).
// drawJ 0x16fc0, drawL 0x178a0 and drawQ 0x175d0 clear it first (VA 0x10016fdd / 0x100178bd / 0x100175ed).
#include "hooks.h"

namespace {

// jgld.dll is relocatable, so every module address used below is resolved against the running base.
char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}

inline char* S(void* self) { return reinterpret_cast<char*>(self); }
inline int& I(void* self, unsigned off) { return *reinterpret_cast<int*>(S(self) + off); }
inline void*& P(void* self, unsigned off) { return *reinterpret_cast<void**>(S(self) + off); }

// A slot of the object's own vtable: [[this] + off].
inline void* slot(void* self, unsigned off) {
    return *reinterpret_cast<void**>(*reinterpret_cast<char**>(self) + off);
}

typedef int(__fastcall* IntSlot_t)(void*, void*);              // Sprite +0x20 (valid), +0x28 (bits)
typedef void(__fastcall* UnlockSlot_t)(void*, void*, int);     // Sprite +0x24
typedef int*(__fastcall* FormatSlot_t)(void*, void*);          // Surface +0xe4

inline int SpriteValid(void* self) { return reinterpret_cast<IntSlot_t>(slot(self, 0x20))(self, 0); }
inline int SpriteBits(void* self) { return reinterpret_cast<IntSlot_t>(slot(self, 0x28))(self, 0); }
inline void SpriteUnlock(void* self, int n) {
    reinterpret_cast<UnlockSlot_t>(slot(self, 0x24))(self, 0, n);
}
inline int SurfaceFormat(void* dst) { return *reinterpret_cast<FormatSlot_t>(slot(dst, 0xe4))(dst, 0); }

// Sprite::getFlag18 at RVA 0x180e0, which every dispatcher of this file calls through the thunk at
// VA 0x10001118. It is batch c3ak's hook, not one of this file's.
inline int SpriteBusy(void* self) {
    return reinterpret_cast<IntSlot_t>(JgldBase() + 0x180e0)(self, 0);
}

// _CrtDbgReport 0x00081f90 with the six arguments of VC6's `_RPTF0(rptno, msg)`, which is
// `_RPT_BASE((rptno, __FILE__, __LINE__, NULL, "%s", msg))`.  `lineBase` is the RVA of the module word the
// /ZI line number is relative to, `msg` the RVA of the message string.
typedef int(__cdecl* Report_t)(int, const char*, int, const char*, const char*, const char*);
int JReport(unsigned lineBase, int delta, unsigned msg) {
    char* b = JgldBase();
    const int line = *reinterpret_cast<short*>(b + lineBase) + delta;
    const int r = reinterpret_cast<Report_t>(b + 0x81f90)(0, b + 0x11d508, line, 0, b + 0x11d528, b + msg);
    if (r == 1)
        __debugbreak();
    return r;
}

// The blitters the dispatchers and the blt forwarders reach, called at their module addresses so that the
// A/B compares this code and not a copy of the blitter. Each is __thiscall on the Sprite.
typedef int(__fastcall* Blit3_t)(void*, void*, void*, int, int);                  // dst, x, y
typedef int(__fastcall* Blit4P_t)(void*, void*, void*, int, int, void*);          // dst, x, y, table
typedef int(__fastcall* Blit5P_t)(void*, void*, void*, int, int, void*, void*);   // dst, x, y, table, table
typedef int(__fastcall* Blit5I_t)(void*, void*, void*, int, int, int, int);       // dst, x, y, word, byte
typedef int(__fastcall* Pair1_t)(void*, void*, void*, void*, int, int);           // other, dst, x, y
typedef int(__fastcall* Pair_t)(void*, void*, void*, void*, void*, int, int);     // other, dst, dst2, x, y
typedef int(__fastcall* Remap8_t)(void*, void*, int, int);

// ===================================================================== the accessor slots

// 0x00017eb0  Sprite::setField10 (VA 0x10017eb0), slot 4 (+0x10) of the table at 0x1011d380: stores its only
// argument into [this + 0x10] (VA 0x10017ed0 / 0x10017ed3) and returns nothing. No branch, no call.
typedef void(__fastcall* SetF10_t)(void*, void*, int);
SetF10_t SetF10_orig;
void __fastcall SetF10_re(void* self, void*, int v) {
    I(self, 0x10) = v;
}

// 0x00017ef0  Sprite::getField10 (VA 0x10017ef0), slot 5 (+0x14): returns [this + 0x10] (VA 0x10017f10).
typedef int(__fastcall* GetF_t)(void*, void*);
GetF_t GetF10_orig;
int __fastcall GetF10_re(void* self, void*) { return I(self, 0x10); }

// 0x00017f30  getField28 (VA 0x10017f30), slot 7 (+0x1c): returns [this + 0x28] (VA 0x10017f50).
GetF_t GetF28_orig;
int __fastcall GetF28_re(void* self, void*) { return I(self, 0x28); }

// 0x00017f70  getField14 (VA 0x10017f70), slot 8 (+0x20): returns [this + 0x14] (VA 0x10017f90), the payload
// base. Every dispatcher of this batch calls this slot and treats a zero result as "not drawable".
GetF_t GetF14_orig;
int __fastcall GetF14_re(void* self, void*) { return I(self, 0x14); }

// 0x00017fe0  getField20 (VA 0x10017fe0), slot 10 (+0x28): returns [this + 0x20] (VA 0x10018000), the bits
// per pixel the outer switch of every dispatcher reads.
GetF_t GetF20_orig;
int __fastcall GetF20_re(void* self, void*) { return I(self, 0x20); }

// 0x00018020  getField2c (VA 0x10018020), slot 11 (+0x2c): returns [this + 0x2c] (VA 0x10018040).
GetF_t GetF2c_orig;
int __fastcall GetF2c_re(void* self, void*) { return I(self, 0x2c); }

// 0x00018060  Sprite::getField30 (VA 0x10018060), slot 12 (+0x30): returns [this + 0x30] (VA 0x10018080).
GetF_t GetF30_orig;
int __fastcall GetF30_re(void* self, void*) { return I(self, 0x30); }

// 0x000180a0  Sprite::getField34 (VA 0x100180a0), slot 13 (+0x34): returns [this + 0x34] (VA 0x100180c0).
GetF_t GetF34_orig;
int __fastcall GetF34_re(void* self, void*) { return I(self, 0x34); }

// ===================================================================== remap, slot 14 (+0x38)

// 0x000150d0  Sprite::remap(a, b) (VA 0x100150d0): switches on the bits per pixel at [this + 0x20]
// (VA 0x100150f0, compared with 8 at VA 0x100150f6). Eight forwards both arguments to remap8 0x1001bae0
// through the thunk at VA 0x1000127b (VA 0x10015109) and returns what it returned; anything else reports at
// line base 0x10122dd0 + 9 (VA 0x1001511c) and returns 0x17 (VA 0x10015142).
typedef int(__fastcall* Remap_t)(void*, void*, int, int);
Remap_t Remap_orig;
int __fastcall Remap_re(void* self, void*, int a, int b) {
    if (I(self, 0x20) == 8)
        return reinterpret_cast<Remap8_t>(JgldBase() + 0x1bae0)(self, 0, a, b);
    JReport(0x122dd0, 9, 0x11d52c);
    return 0x17;
}

// ===================================================================== the five blt forwarders

// All five test the DESTINATION's depth and the sprite's bits per pixel before they forward, and all five
// read [this + 0x10] into a local that nothing ever reads again (VA 0x1001988c / 0x1001988f and its four
// twins): the saved Palette is never restored, so a non-zero `pal` argument leaves the sprite carrying it.
// That is a defect of the original; both arms do it and the $spr state region records it.

// 0x00019830  Sprite::blt16a(dst, x, y, table, pal) (VA 0x10019830), slot 30 (+0x78). Returns 0x17 unless the
// destination's depth is 16 (slot +0xe4 of the Surface, VA 0x10019857; `cmp dword ptr [eax], 0x10`
// VA 0x10019864) and the sprite's bits per pixel is 8 (slot +0x28, VA 0x10019873; `cmp eax, 8` VA 0x1001987d).
// Then it overwrites [this + 0x10] with a non-zero Palette argument (VA 0x10019892 / 0x1001989e) and
// tail-forwards (dst, x, y, table) to 0x100298b0 through the thunk at VA 0x1000114a (VA 0x100198b4).
typedef int(__fastcall* Blt16a_t)(void*, void*, void*, int, int, void*, void*);
Blt16a_t Blt16a_orig;
int __fastcall Blt16a_re(void* self, void*, void* dst, int x, int y, void* table, void* pal) {
    if (SurfaceFormat(dst) != 0x10 || SpriteBits(self) != 8)
        return 0x17;
    if (pal)
        P(self, 0x10) = pal;
    return reinterpret_cast<Blit4P_t>(JgldBase() + 0x298b0)(self, 0, dst, x, y, table);
}

// 0x00019900  Sprite::blt16b(dst, x, y, colour, pal, flag) (VA 0x10019900), slot 18 (+0x48). The same two
// guards (VA 0x10019934, VA 0x1001994d), the same Palette store (VA 0x10019962 / 0x1001996e), then a forward
// to 0x10022a10 through the thunk at VA 0x10019989. The two small arguments are passed inside dwords whose
// upper bits are leftovers: `mov cl, byte ptr [ebp + 0x1c]; push ecx` (VA 0x10019971 / 0x10019974) pushes the
// flag byte over the low byte of the Palette pointer this function loaded at VA 0x1001995c, and `mov dx, word
// ptr [ebp + 0x14]; push edx` (VA 0x10019975 / 0x10019979) pushes the colour word over the high half of edx,
// which holds `this` when the Palette store ran (VA 0x10019968) and the sprite's vtable pointer when it did
// not (VA 0x1001993c, preserved across the two calls and the /GZ frame check). 0x10022a10 reads only the low
// word of the first (`mov bp, word ptr [ebp + 0x14]`, VA 0x10022d26) and nothing of the second.
typedef int(__fastcall* Blt16b_t)(void*, void*, void*, int, int, int, void*, int);
Blt16b_t Blt16b_orig;
int __fastcall Blt16b_re(void* self, void*, void* dst, int x, int y, int colour, void* pal, int flag) {
    if (SurfaceFormat(dst) != 0x10 || SpriteBits(self) != 8)
        return 0x17;
    const int saved = I(self, 0x10);
    const int high = pal ? reinterpret_cast<int>(self) : *reinterpret_cast<int*>(self);
    if (pal)
        P(self, 0x10) = pal;
    const int colourArg = (high & ~0xffff) | (colour & 0xffff);
    const int flagArg = (saved & ~0xff) | (flag & 0xff);
    return reinterpret_cast<Blit5I_t>(JgldBase() + 0x22a10)(self, 0, dst, x, y, colourArg, flagArg);
}

// 0x000199d0  Sprite::blt16c (VA 0x100199d0), slot 19 (+0x4c): blt16b's body with the same guards
// (VA 0x10019a04, VA 0x10019a1d), the same two dirty dwords (VA 0x10019a41 / 0x10019a45) and a forward to
// 0x10024140 through the thunk at VA 0x10019a59.
Blt16b_t Blt16c_orig;
int __fastcall Blt16c_re(void* self, void*, void* dst, int x, int y, int colour, void* pal, int flag) {
    if (SurfaceFormat(dst) != 0x10 || SpriteBits(self) != 8)
        return 0x17;
    const int saved = I(self, 0x10);
    const int high = pal ? reinterpret_cast<int>(self) : *reinterpret_cast<int*>(self);
    if (pal)
        P(self, 0x10) = pal;
    const int colourArg = (high & ~0xffff) | (colour & 0xffff);
    const int flagArg = (saved & ~0xff) | (flag & 0xff);
    return reinterpret_cast<Blit5I_t>(JgldBase() + 0x24140)(self, 0, dst, x, y, colourArg, flagArg);
}

// 0x00019aa0  Sprite::bltS(other, dst, x, y, pal) (VA 0x10019aa0), slot 21 (+0x54). Three guards: the
// destination's depth is 16 (VA 0x10019ad4), the sprite's bits per pixel is 8 (VA 0x10019aed) and the OTHER
// sprite's bits per pixel is 8 (slot +0x28 on [ebp + 8], VA 0x10019afc / 0x10019b06); any of them failing
// returns 0x17 (VA 0x10019b0b). Then the Palette store (VA 0x10019b1b / 0x10019b27) and a forward of
// (other, dst, x, y) to blitPair1 0x10025dd0 through the thunk at VA 0x10019b3d.
typedef int(__fastcall* BltS_t)(void*, void*, void*, void*, int, int, void*);
BltS_t BltS_orig;
int __fastcall BltS_re(void* self, void*, void* other, void* dst, int x, int y, void* pal) {
    if (SurfaceFormat(dst) != 0x10 || SpriteBits(self) != 8 || SpriteBits(other) != 8)
        return 0x17;
    if (pal)
        P(self, 0x10) = pal;
    return reinterpret_cast<Pair1_t>(JgldBase() + 0x25dd0)(self, 0, other, dst, x, y);
}

// 0x00019b90  Sprite::bltS2(other, dst, dst2, x, y, pal) (VA 0x10019b90), slot 20 (+0x50): bltS's three
// guards (VA 0x10019bc4, VA 0x10019bdd, VA 0x10019bf6), the Palette store (VA 0x10019c0b / 0x10019c17) and a
// forward of (other, dst, dst2, x, y) to blitPair 0x10025900 through the thunk at VA 0x10019c31. The guarded
// Surface is the FIRST one, which blitPair only reads (VA 0x10025bb4); the one it writes is the third
// argument (VA 0x10025c4b / 0x10025c5d), whose depth nothing here tests.
typedef int(__fastcall* BltS2_t)(void*, void*, void*, void*, void*, int, int, void*);
BltS2_t BltS2_orig;
int __fastcall BltS2_re(void* self, void*, void* other, void* dst, void* dst2, int x, int y, void* pal) {
    if (SurfaceFormat(dst) != 0x10 || SpriteBits(self) != 8 || SpriteBits(other) != 8)
        return 0x17;
    if (pal)
        P(self, 0x10) = pal;
    return reinterpret_cast<Pair_t>(JgldBase() + 0x25900)(self, 0, other, dst, dst2, x, y);
}

// Sprite::lineTo 0x18490 (slot 35) and Sprite::dashTo 0x18510 (slot 34) were prepared and dropped: their
// callees blitMask 0x1002cec0 and blitMaskScaled 0x1002d390 hang the game. See log/c3/c3ci_notes.txt.

// ===================================================================== the draw dispatchers

// 0x00015180  Sprite::draw(dst, x, y, pal) (VA 0x10015180), slot 23 (+0x5c).
// Guards first: a NULL destination (VA 0x1001519d) or a sprite whose slot +0x20 returns 0 (VA 0x100151ad /
// 0x100151b7) return 7 (VA 0x100151bb) with nothing saved and nothing restored. Then the Palette is saved
// (VA 0x100151c8) and replaced when the argument is not zero (VA 0x100151ce / 0x100151da).
// The outer switch is on the bits per pixel at [this + 0x20] (VA 0x100151e0, compared with 8 at
// VA 0x100151e6 and with 0x10 at VA 0x100151ec):
//   8     the destination's depth decides (VA 0x10015205 / 0x10015217 / 0x1001521d):
//           8     busy (VA 0x1001522b / 0x10015230) reports at line base 0x10122dd4 + 0x10, otherwise
//                 0x1005dde0 is called through the thunk at VA 0x10015277 and its result stored
//           0x10  busy reports at + 0x17 (VA 0x10015287), otherwise 0x1001cde0 through VA 0x100152d3
//           other reports at + 0x20 (VA 0x100152dd) and the result becomes 0x17 (VA 0x1001530f)
//   0x10  the destination's depth is fetched (VA 0x10015325) and stored but never tested; the arm reports at
//         + 0x2d (VA 0x10015337) and the result becomes 0x17 (VA 0x10015369)
//   other reports at + 0x35 (VA 0x10015372) and the result becomes 0x17 (VA 0x100153a4)
// The join restores the Palette (VA 0x100153b1), calls slot +0x24 with 1 (VA 0x100153c0) and returns the
// result local (VA 0x100153ca), which on the two busy arms is still the frame fill.
typedef int(__fastcall* Draw_t)(void*, void*, void*, int, int, void*);
Draw_t Draw_orig;
int __fastcall Draw_re(void* self, void*, void* dst, int x, int y, void* pal) {
    if (dst == 0 || !SpriteValid(self))
        return 7;
    void* const saved = P(self, 0x10);
    if (pal)
        P(self, 0x10) = pal;
    int r = static_cast<int>(0xcccccccc);
    char* b = JgldBase();
    switch (I(self, 0x20)) {
    case 8:
        switch (SurfaceFormat(dst)) {
        case 8:
            if (SpriteBusy(self))
                JReport(0x122dd4, 0x10, 0x11d548);
            else
                r = reinterpret_cast<Blit3_t>(b + 0x5dde0)(self, 0, dst, x, y);
            break;
        case 0x10:
            if (SpriteBusy(self))
                JReport(0x122dd4, 0x17, 0x11d548);
            else
                r = reinterpret_cast<Blit3_t>(b + 0x1cde0)(self, 0, dst, x, y);
            break;
        default:
            JReport(0x122dd4, 0x20, 0x11d52c);
            r = 0x17;
        }
        break;
    case 0x10:
        SurfaceFormat(dst);
        JReport(0x122dd4, 0x2d, 0x11d52c);
        r = 0x17;
        break;
    default:
        JReport(0x122dd4, 0x35, 0x11d52c);
        r = 0x17;
    }
    P(self, 0x10) = saved;
    SpriteUnlock(self, 1);
    return r;
}

// 0x00016270  Sprite::drawF(dst, x, y, table, pal) (VA 0x10016270), slot 28 (+0x70).
// The Palette is saved and replaced BEFORE the guards (VA 0x10016290 / 0x100162a2), so the two early
// returns leave the sprite carrying the argument: a NULL destination gives 0x10 (VA 0x100162a5 /
// 0x100162ab) and an invalid sprite gives 7 (VA 0x100162bf / 0x100162cd), neither restoring the field.
// Outer switch on [this + 0x20] (VA 0x100162da / 0x100162e0 / 0x100162e6):
//   8     destination depth (VA 0x100162ff / 0x10016311 / 0x10016317):
//           8     busy (VA 0x10016325) reports at line base 0x10122de8 + 0x11, otherwise 0x10060cf0 is
//                 called with (dst, x, y, table) through the thunk at VA 0x10016375
//           0x10  reports at + 0x17 (VA 0x1001637f) with no test and no result store
//           other reports at + 0x22 (VA 0x100163b3) and the result becomes 0x17 (VA 0x100163e5)
//   0x10  depth 8 or 0x10 (VA 0x1001640d / 0x10016413) report at + 0x2d (VA 0x1001641b) with no result
//         store; anything else reports at + 0x38 (VA 0x1001644f) and gives 0x17 (VA 0x10016481)
//   other reports at + 0x40 (VA 0x1001648a) and gives 0x17 (VA 0x100164bc)
// The join restores the Palette (VA 0x100164c9), calls slot +0x24 with 1 (VA 0x100164d8) and returns the
// result local (VA 0x100164e2), which on four of the six arms is still the frame fill.
typedef int(__fastcall* Draw4_t)(void*, void*, void*, int, int, void*, void*);
Draw4_t DrawF_orig;
int __fastcall DrawF_re(void* self, void*, void* dst, int x, int y, void* table, void* pal) {
    void* const saved = P(self, 0x10);
    if (pal)
        P(self, 0x10) = pal;
    if (dst == 0)
        return 0x10;
    if (!SpriteValid(self))
        return 7;
    int r = static_cast<int>(0xcccccccc);
    switch (I(self, 0x20)) {
    case 8:
        switch (SurfaceFormat(dst)) {
        case 8:
            if (SpriteBusy(self))
                JReport(0x122de8, 0x11, 0x11d548);
            else
                r = reinterpret_cast<Blit4P_t>(JgldBase() + 0x60cf0)(self, 0, dst, x, y, table);
            break;
        case 0x10:
            JReport(0x122de8, 0x17, 0x11d56c);
            break;
        default:
            JReport(0x122de8, 0x22, 0x11d52c);
            r = 0x17;
        }
        break;
    case 0x10:
        switch (SurfaceFormat(dst)) {
        case 8:
        case 0x10:
            JReport(0x122de8, 0x2d, 0x11d56c);
            break;
        default:
            JReport(0x122de8, 0x38, 0x11d52c);
            r = 0x17;
        }
        break;
    default:
        JReport(0x122de8, 0x40, 0x11d52c);
        r = 0x17;
    }
    P(self, 0x10) = saved;
    SpriteUnlock(self, 1);
    return r;
}

// 0x000169a0  Sprite::drawH(dst, x, y, table, pal) (VA 0x100169a0), slot 16 (+0x40): drawF's frame with two
// blitters. Palette saved and replaced first (VA 0x100169c0 / 0x100169d2), NULL destination gives 0x10
// (VA 0x100169d5 / 0x100169db), an invalid sprite 7 (VA 0x100169ef / 0x100169fd).
// Outer switch (VA 0x10016a0a / 0x10016a10 / 0x10016a16):
//   8     destination depth (VA 0x10016a2f / 0x10016a41 / 0x10016a47):
//           8     busy (VA 0x10016a55) reports at line base 0x10122df0 + 0x11, otherwise 0x10061db0 with
//                 (dst, x, y, table) through the thunk at VA 0x10016aa5
//           0x10  busy (VA 0x10016ab5) reports at + 0x18, otherwise 0x10026240 through VA 0x10016b05
//           other reports at + 0x20 (VA 0x10016b0f) and gives 0x17 (VA 0x10016b41)
//   0x10  the depth is fetched (VA 0x10016b57) but never tested; reports at + 0x2e (VA 0x10016b69) and gives
//         0x17 (VA 0x10016b9b)
//   other reports at + 0x36 (VA 0x10016ba4) and gives 0x17
// The join restores the Palette, calls slot +0x24 with 1 and returns the result local, which on the two
// busy arms is still the frame fill.
Draw4_t DrawH_orig;
int __fastcall DrawH_re(void* self, void*, void* dst, int x, int y, void* table, void* pal) {
    void* const saved = P(self, 0x10);
    if (pal)
        P(self, 0x10) = pal;
    if (dst == 0)
        return 0x10;
    if (!SpriteValid(self))
        return 7;
    int r = static_cast<int>(0xcccccccc);
    char* b = JgldBase();
    switch (I(self, 0x20)) {
    case 8:
        switch (SurfaceFormat(dst)) {
        case 8:
            if (SpriteBusy(self))
                JReport(0x122df0, 0x11, 0x11d548);
            else
                r = reinterpret_cast<Blit4P_t>(b + 0x61db0)(self, 0, dst, x, y, table);
            break;
        case 0x10:
            if (SpriteBusy(self))
                JReport(0x122df0, 0x18, 0x11d548);
            else
                r = reinterpret_cast<Blit4P_t>(b + 0x26240)(self, 0, dst, x, y, table);
            break;
        default:
            JReport(0x122df0, 0x20, 0x11d52c);
            r = 0x17;
        }
        break;
    case 0x10:
        SurfaceFormat(dst);
        JReport(0x122df0, 0x2e, 0x11d52c);
        r = 0x17;
        break;
    default:
        JReport(0x122df0, 0x36, 0x11d52c);
        r = 0x17;
    }
    P(self, 0x10) = saved;
    SpriteUnlock(self, 1);
    return r;
}

// 0x00016fc0  Sprite::drawJ(dst, x, y, table, pixels, pal) (VA 0x10016fc0), slot 26 (+0x68): drawF's frame
// with a five-argument blitter and a result local cleared to 0 first (VA 0x10016fdd), so the report-only arms
// return 0 instead of the frame fill. Palette saved and replaced (VA 0x10016fe7 / 0x10016ff9), NULL
// destination 0x10 (VA 0x10016ffc / 0x10017002), invalid sprite 7 (VA 0x10017016 / 0x10017024).
// Outer switch (VA 0x10017031 / 0x10017037 / 0x1001703d):
//   8     destination depth (VA 0x10017056 / 0x10017068 / 0x1001706e):
//           8     busy (VA 0x1001707c) reports at line base 0x10122df8 + 0x13, otherwise 0x10062dd0 with
//                 (dst, x, y, table, pixels) through the thunk at VA 0x100170d0
//           0x10  reports at + 0x19 (VA 0x100170da) with no test and no result store
//           other reports at + 0x24 (VA 0x1001710e) and gives 0x17 (VA 0x10017140)
//   0x10  the depth is fetched (VA 0x10017156) but never tested; reports at + 0x31 (VA 0x10017168) and gives
//         0x17 (VA 0x1001719a)
//   other reports at + 0x39 (VA 0x100171a3) and gives 0x17 (VA 0x100171d5)
typedef int(__fastcall* Draw5_t)(void*, void*, void*, int, int, void*, void*, void*);
Draw5_t DrawJ_orig;
int __fastcall DrawJ_re(void* self, void*, void* dst, int x, int y, void* table, void* pixels, void* pal) {
    void* const saved = P(self, 0x10);
    if (pal)
        P(self, 0x10) = pal;
    if (dst == 0)
        return 0x10;
    if (!SpriteValid(self))
        return 7;
    int r = 0;
    switch (I(self, 0x20)) {
    case 8:
        switch (SurfaceFormat(dst)) {
        case 8:
            if (SpriteBusy(self))
                JReport(0x122df8, 0x13, 0x11d548);
            else
                r = reinterpret_cast<Blit5P_t>(JgldBase() + 0x62dd0)(self, 0, dst, x, y, table, pixels);
            break;
        case 0x10:
            JReport(0x122df8, 0x19, 0x11d56c);
            break;
        default:
            JReport(0x122df8, 0x24, 0x11d52c);
            r = 0x17;
        }
        break;
    case 0x10:
        SurfaceFormat(dst);
        JReport(0x122df8, 0x31, 0x11d52c);
        r = 0x17;
        break;
    default:
        JReport(0x122df8, 0x39, 0x11d52c);
        r = 0x17;
    }
    P(self, 0x10) = saved;
    SpriteUnlock(self, 1);
    return r;
}

// 0x000178a0  Sprite::drawL(dst, x, y, table, pal) (VA 0x100178a0), slot 32 (+0x80): the result local is
// cleared to 0 (VA 0x100178bd), the Palette saved and replaced (VA 0x100178ca / 0x100178d9), a NULL
// destination gives 0x10 (VA 0x100178dc / 0x100178e2) and an invalid sprite 7 (VA 0x100178f6 / 0x10017904).
// Outer switch (VA 0x10017917 / 0x1001791d):
//   8     the destination's depth is tested against 8 ONLY (VA 0x10017948): 8 means busy (VA 0x10017953)
//         reports at line base 0x10122e04 + 0x18, otherwise 0x10063f60 with (dst, x, y, table) through the
//         thunk at VA 0x100179a3; any other depth reports at + 0x21 (VA 0x100179ad) and gives 0x17
//         (VA 0x100179df)
//   0x10  the depth is fetched (VA 0x100179f5) but never tested; reports at + 0x2f (VA 0x10017a07) and gives
//         0x17 (VA 0x10017a39)
//   other reports at + 0x37 (VA 0x10017a42) and gives 0x17 (VA 0x10017a74)
Draw4_t DrawL_orig;
int __fastcall DrawL_re(void* self, void*, void* dst, int x, int y, void* table, void* pal) {
    void* const saved = P(self, 0x10);
    if (pal)
        P(self, 0x10) = pal;
    if (dst == 0)
        return 0x10;
    if (!SpriteValid(self))
        return 7;
    int r = 0;
    switch (I(self, 0x20)) {
    case 8:
        if (SurfaceFormat(dst) == 8) {
            if (SpriteBusy(self))
                JReport(0x122e04, 0x18, 0x11d548);
            else
                r = reinterpret_cast<Blit4P_t>(JgldBase() + 0x63f60)(self, 0, dst, x, y, table);
        } else {
            JReport(0x122e04, 0x21, 0x11d52c);
            r = 0x17;
        }
        break;
    case 0x10:
        SurfaceFormat(dst);
        JReport(0x122e04, 0x2f, 0x11d52c);
        r = 0x17;
        break;
    default:
        JReport(0x122e04, 0x37, 0x11d52c);
        r = 0x17;
    }
    P(self, 0x10) = saved;
    SpriteUnlock(self, 1);
    return r;
}

// 0x000175d0  Sprite::drawQ(dst, p1, p2, p3, p4, pal) (VA 0x100175d0), slot 24 (+0x60): a dispatcher that
// reaches no blitter at all. The four middle arguments are read nowhere in the body. The result local is
// cleared to 0 (VA 0x100175ed), the Palette saved (VA 0x100175f7) and replaced when the argument is not
// zero (VA 0x100175fd / 0x10017609); then a NULL destination returns 0x10 (VA 0x1001760c / 0x10017612) and a
// sprite whose slot +0x20 returns 0 gives 7 (VA 0x10017626 / 0x10017634), neither restoring the field.
// Outer switch on [this + 0x20] (VA 0x10017641, compared with 8 at VA 0x10017647 and 0x10 at VA 0x1001764d):
//   8     reports at line base 0x10122e00 + 0xf (VA 0x1001765c) and leaves the result 0
//   0x10  the destination's depth is fetched (VA 0x1001770f) and compared with 8 (VA 0x10017721): equal
//         reports at + 0x29 (VA 0x10017729) and leaves the result 0, anything else reports at + 0x2f
//         (VA 0x1001775d) and gives 0x17 (VA 0x1001778f)
//   other reports at + 0x37 (VA 0x10017798) and gives 0x17 (VA 0x100177ca)
// The join restores the Palette (VA 0x100177d7), calls slot +0x24 with 1 (VA 0x100177e6) and returns the
// result local (VA 0x100177f0).
// Two complete report sequences sit inside the body and are the target of no jump: 0x10017693 (line + 0x14)
// and 0x100176c7 (line + 0x20, which would set the result to 0x17). The only branches to either are the
// `jne` each macro makes back to itself (VA 0x100176c3, VA 0x100176f7), and a scan of jgld.dll's mapped image
// finds no other reference to either address. They are not reimplemented, and their eight conditional-jump
// sides are the unreachable ones in this function's branch table.
typedef int(__fastcall* DrawQ_t)(void*, void*, void*, int, int, int, int, void*);
DrawQ_t DrawQ_orig;
int __fastcall DrawQ_re(void* self, void*, void* dst, int, int, int, int, void* pal) {
    int r = 0;
    void* const saved = P(self, 0x10);
    if (pal)
        P(self, 0x10) = pal;
    if (dst == 0)
        return 0x10;
    if (!SpriteValid(self))
        return 7;
    switch (I(self, 0x20)) {
    case 8:
        JReport(0x122e00, 0xf, 0x11d56c);
        break;
    case 0x10:
        if (SurfaceFormat(dst) == 8) {
            JReport(0x122e00, 0x29, 0x11d56c);
        } else {
            JReport(0x122e00, 0x2f, 0x11d52c);
            r = 0x17;
        }
        break;
    default:
        JReport(0x122e00, 0x37, 0x11d52c);
        r = 0x17;
    }
    P(self, 0x10) = saved;
    SpriteUnlock(self, 1);
    return r;
}

}  // namespace

SG_HOOK("jgld.dll", 0x00017eb0, Sprite_setField10, SetF10_re, SetF10_orig);
SG_HOOK("jgld.dll", 0x00017ef0, Sprite_getField10, GetF10_re, GetF10_orig);
SG_HOOK("jgld.dll", 0x00017f30, Sprite_getField28, GetF28_re, GetF28_orig);
SG_HOOK("jgld.dll", 0x00017f70, Sprite_getField14, GetF14_re, GetF14_orig);
SG_HOOK("jgld.dll", 0x00017fe0, Sprite_getField20, GetF20_re, GetF20_orig);
SG_HOOK("jgld.dll", 0x00018020, Sprite_getField2c, GetF2c_re, GetF2c_orig);
SG_HOOK("jgld.dll", 0x00018060, Sprite_getField30, GetF30_re, GetF30_orig);
SG_HOOK("jgld.dll", 0x000180a0, Sprite_getField34, GetF34_re, GetF34_orig);
SG_HOOK("jgld.dll", 0x000150d0, Sprite_remap, Remap_re, Remap_orig);
SG_HOOK("jgld.dll", 0x00019830, Sprite_blt16a, Blt16a_re, Blt16a_orig);
SG_HOOK("jgld.dll", 0x00019900, Sprite_blt16b, Blt16b_re, Blt16b_orig);
SG_HOOK("jgld.dll", 0x000199d0, Sprite_blt16c, Blt16c_re, Blt16c_orig);
SG_HOOK("jgld.dll", 0x00019aa0, Sprite_bltS, BltS_re, BltS_orig);
SG_HOOK("jgld.dll", 0x00019b90, Sprite_bltS2, BltS2_re, BltS2_orig);
SG_HOOK("jgld.dll", 0x00015180, Sprite_draw, Draw_re, Draw_orig);
SG_HOOK("jgld.dll", 0x00016270, Sprite_drawF, DrawF_re, DrawF_orig);
SG_HOOK("jgld.dll", 0x000169a0, Sprite_drawH, DrawH_re, DrawH_orig);
SG_HOOK("jgld.dll", 0x00016fc0, Sprite_drawJ, DrawJ_re, DrawJ_orig);
SG_HOOK("jgld.dll", 0x000178a0, Sprite_drawL, DrawL_re, DrawL_orig);
SG_HOOK("jgld.dll", 0x000175d0, Sprite_drawQ, DrawQ_re, DrawQ_orig);
