// Batch c3dk (round 26, jgld.dll): the leads c3dg left (log/c3/c3dg_notes.txt "Doable with the stand-ins") and the
// two rows that were blocked only by unnamed callees (names proposed in log/c3/c3dk_names.tsv).
//
//   group D - Display construction       Display::Display 0x000654b0, DisplayBase::ctor 0x00065630,
//                                        DisplayMember::ctor 0x00069270
//   group S - Sprite dispatchers         Sprite::drawC 0x000157e0, Sprite::drawD 0x00015bb0,
//                                        Sprite::drawAlpha 0x00018920
//   group W - Display::createWindow 0x000657e0      group A - allocDrawBuffer 0x00007e40
//   group F - Font::loadFile 0x00068ae0
//   group P - libpng: png_set_pCAL 0x0007de10, png_read_destroy 0x0006d250, png_read_start_row 0x0007d6b0,
//                                        png_build_gamma_table 0x00077d30
//
// SG_HOOK addresses are RVAs; every instruction cited in a comment is a VA (0x10000000 + RVA), the form the debug
// build's disassembly prints. jgld.dll is relocated at run time, so every module address (vtables, globals,
// callees, import slots) is resolved against the running base. __thiscall is emulated with __fastcall (ecx = this,
// edx unused). Callees run through the incremental-link thunks the originals call, so a callee hooked by another
// batch (or by this one) runs that batch's body.
#include "hooks.h"

namespace {

char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}
void* JgldVA(unsigned va) { return JgldBase() + (va - 0x10000000); }
template <class T> T& G(unsigned va) { return *static_cast<T*>(JgldVA(va)); }
template <class T> T& F(void* obj, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(obj) + off); }
// Slot `off` of the vtable of `obj` (`mov edx, [obj]; call [edx + off]`).
inline void* VSlot(void* obj, unsigned off) { return F<void*>(F<void*>(obj, 0), off); }

typedef void*(__fastcall* Ctor_t)(void*, void*);

// =====================================================================================================================
// group D - Display construction
// =====================================================================================================================

// 0x00065630  DisplayBase::ctor (thiscall, no stack argument, plain `ret` at 0x1006565f): stores the DisplayBase
// vtable 0x1011d7dc into this[0] (0x10065650) and returns this (`mov eax, [ebp-4]` 0x10065656).
Ctor_t DisplayBaseCtor_orig;
void* __fastcall DisplayBaseCtor_re(void* self, void*) {
    F<void*>(self, 0) = JgldVA(0x1011d7dc);
    return self;
}

// 0x00069270  DisplayMember::ctor (thiscall, no stack argument, plain `ret` at 0x100692a0): stores 0 into the
// module's command-handler pointer 0x10128728 (0x1006928d) and returns this (0x10069297). Nothing inside the
// object is written.
Ctor_t DisplayMemberCtor_orig;
void* __fastcall DisplayMemberCtor_re(void* self, void*) {
    G<void*>(0x10128728) = 0;
    return self;
}

// 0x000654b0  Display::Display (thiscall, no stack argument, plain `ret` at 0x10065584; the SEH frame
// 0x100654b3..0x100654c1 only brackets the member construction). In order: DisplayBase::ctor on this through the
// thunk 0x100019dd (0x100654e5), DisplayMember::ctor on this + 4 through the thunk 0x1000122b (0x100654f4 /
// 0x100654f7), the Display vtable 0x1011d640 into this[0] (0x100654ff), then 0 into +0x12c, +0x130, +0x134, +0x138,
// +0x140, +0x144 and +0x148 (0x10065508..0x10065549; +0x13c is not written). Returns this (0x10065567).
Ctor_t DisplayCtor_orig;
void* __fastcall DisplayCtor_re(void* self, void*) {
    reinterpret_cast<Ctor_t>(JgldVA(0x100019dd))(self, 0);
    reinterpret_cast<Ctor_t>(JgldVA(0x1000122b))(static_cast<char*>(self) + 4, 0);
    F<void*>(self, 0) = JgldVA(0x1011d640);
    F<int>(self, 0x12c) = 0;
    F<int>(self, 0x130) = 0;
    F<int>(self, 0x134) = 0;
    F<int>(self, 0x138) = 0;
    F<int>(self, 0x140) = 0;
    F<int>(self, 0x144) = 0;
    F<int>(self, 0x148) = 0;
    return self;
}

// =====================================================================================================================
// group S - Sprite dispatchers (the frame of c3dc's drawB..drawP, shim/src/re/c3dc.cpp)
// The Sprite's Palette word is +0x10, its bits per pixel +0x20; slot +0x20 of the Sprite vtable is the validity /
// lock test, slot +0x24 the release (called with 1); the destination's slot +0xe4 returns a pointer to its format
// pair {bits per pixel, 0 = 565 / 1 = 555}. "busy" is Sprite::getFlag18 0x100180e0 (through the thunk 0x10001118).
// Every report is VC6's _RPTF0(_CRT_WARN, msg): _CrtDbgReport 0x10081f90 (0, file 0x1011d508, line, NULL,
// "%s" 0x1011d528, msg) with the line = a /ZI line-base word + a constant, followed by `cmp eax, 1 / int3`.
// =====================================================================================================================
typedef int(__fastcall* IntSlot_t)(void*, void*);
typedef void(__fastcall* UnlockSlot_t)(void*, void*, int);
typedef int*(__fastcall* FormatSlot_t)(void*, void*);
inline int SpriteValid(void* self) { return reinterpret_cast<IntSlot_t>(VSlot(self, 0x20))(self, 0); }
inline void SpriteUnlock(void* self) { reinterpret_cast<UnlockSlot_t>(VSlot(self, 0x24))(self, 0, 1); }
inline int SpriteBusy(void* self) { return reinterpret_cast<IntSlot_t>(JgldVA(0x10001118))(self, 0); }
inline int* SurfaceFormat(void* s) { return reinterpret_cast<FormatSlot_t>(VSlot(s, 0xe4))(s, 0); }

typedef int(__cdecl* Report_t)(int, const char*, int, const char*, const char*, const char*);
void JReport(unsigned lineWord, int delta, unsigned msg) {
    const int line = G<short>(lineWord) + delta;
    const int r = reinterpret_cast<Report_t>(JgldVA(0x10081f90))(
        0, static_cast<const char*>(JgldVA(0x1011d508)), line, 0, static_cast<const char*>(JgldVA(0x1011d528)),
        static_cast<const char*>(JgldVA(msg)));
    if (r == 1)
        __debugbreak();
}
const unsigned kMsgBusy = 0x1011d548;    // "NOT WRITTEN FOR COMPRESSION"
const unsigned kMsgDepth = 0x1011d52c;   // "LIBERR_NOT_THIS_DEPTH"
const unsigned kMsgNone = 0x1011d56c;    // "NOT AVAILABLE TO THIS SURFACE"
const int kFill = static_cast<int>(0xcccccccc);   // the /GZ fill of a result local no arm stores

// draw16m_1001df80(dst, x, y, mask, level, colour) / drawRle16mk 0x1005a000 (same six arguments), and
// draw16m_1001e970(dst, x, y, mask, level) / drawRle16m 0x1005a790. `level` is pushed from a byte load
// (0x1001591a, 0x10015ce6); both callees read it masked to a byte (0x1001e0f7, 0x1005ac22).
typedef int(__fastcall* M6_t)(void*, void*, void*, int, int, void*, int, int);
typedef int(__fastcall* M5_t)(void*, void*, void*, int, int, void*, int);

// 0x000157e0  Sprite::drawC(dst, x, y, mask, level, colour, pal), Sprite vtable 0x1011d380 slot 39 (+0x9c).
// `ret 0x1c` at 0x10015ae9. A NULL destination (je 0x10015801) or an invalid sprite (slot +0x20 at 0x1001580d,
// jne 0x10015819) returns 7 (0x1001581b) before the Palette word is touched; then it is saved (0x1001582b) and
// replaced when pal != 0 (0x1001582e / 0x1001583a). The result local is never initialised.
//   [this+0x20] 8 (0x10015846): depth 8 (0x10015877): busy (0x10015896) -> report base 0x10122ddc + 0x10 BUSY,
//                               result untouched; not busy -> report + 0x12 DEPTH, 0x17 (0x100158fe);
//                               depth 0x10 (0x1001587d): busy (0x10015914) -> drawRle16mk 0x1005a000 through
//                               0x1000182f (0x10015931), else draw16m_1001df80 through 0x1000140b (0x10015956),
//                               both with (dst, x, y, mask, level, colour); other -> report + 0x20, 0x17
//   [this+0x20] 0x10 (0x1001584c): depth 8 or 0x10 (0x100159ba / 0x100159c0): busy (0x100159d2) -> report + 0x2b
//                               BUSY, result untouched; else report + 0x2d, 0x17; other -> report + 0x34, 0x17
//   other: report + 0x3c, 0x17 (0x10015ab0)
// Join (0x10015ab7): Palette word restored, slot +0x24 with 1 (0x10015acc), the result local returned.
typedef int(__fastcall* DrawC_t)(void*, void*, void*, int, int, void*, int, int, void*);
DrawC_t DrawC_orig;
int __fastcall DrawC_re(void* self, void*, void* dst, int x, int y, void* mask, int level, int colour, void* pal) {
    if (dst == 0 || !SpriteValid(self))
        return 7;
    void* const saved = F<void*>(self, 0x10);
    if (pal)
        F<void*>(self, 0x10) = pal;
    int r = kFill;
    const int lv = static_cast<unsigned char>(level);
    switch (F<int>(self, 0x20)) {
    case 8:
        switch (*SurfaceFormat(dst)) {
        case 8:
            if (SpriteBusy(self)) {
                JReport(0x10122ddc, 0x10, kMsgBusy);
            } else {
                JReport(0x10122ddc, 0x12, kMsgDepth);
                r = 0x17;
            }
            break;
        case 0x10:
            if (SpriteBusy(self))
                r = reinterpret_cast<M6_t>(JgldVA(0x1000182f))(self, 0, dst, x, y, mask, lv, colour);
            else
                r = reinterpret_cast<M6_t>(JgldVA(0x1000140b))(self, 0, dst, x, y, mask, lv, colour);
            break;
        default:
            JReport(0x10122ddc, 0x20, kMsgDepth);
            r = 0x17;
        }
        break;
    case 0x10:
        switch (*SurfaceFormat(dst)) {
        case 8:
        case 0x10:
            if (SpriteBusy(self)) {
                JReport(0x10122ddc, 0x2b, kMsgBusy);
            } else {
                JReport(0x10122ddc, 0x2d, kMsgDepth);
                r = 0x17;
            }
            break;
        default:
            JReport(0x10122ddc, 0x34, kMsgDepth);
            r = 0x17;
        }
        break;
    default:
        JReport(0x10122ddc, 0x3c, kMsgDepth);
        r = 0x17;
    }
    F<void*>(self, 0x10) = saved;
    SpriteUnlock(self);
    return r;
}

// 0x00015bb0  Sprite::drawD(dst, x, y, mask, level, pal), Sprite vtable 0x1011d380 slot 40 (+0xa0). `ret 0x18` at
// 0x10015eb1. drawC's frame instruction for instruction with the line-base word 0x10122de0, the Palette override
// in the sixth argument ([ebp+0x1c], 0x10015bfe), and the 8 bpp -> 16 arm calling drawRle16m 0x1005a790 through
// 0x100019b5 (busy, 0x10015cfd) or draw16m_1001e970 through 0x10001a78 (0x10015d1e) with (dst, x, y, mask, level).
typedef int(__fastcall* DrawD_t)(void*, void*, void*, int, int, void*, int, void*);
DrawD_t DrawD_orig;
int __fastcall DrawD_re(void* self, void*, void* dst, int x, int y, void* mask, int level, void* pal) {
    if (dst == 0 || !SpriteValid(self))
        return 7;
    void* const saved = F<void*>(self, 0x10);
    if (pal)
        F<void*>(self, 0x10) = pal;
    int r = kFill;
    const int lv = static_cast<unsigned char>(level);
    switch (F<int>(self, 0x20)) {
    case 8:
        switch (*SurfaceFormat(dst)) {
        case 8:
            if (SpriteBusy(self)) {
                JReport(0x10122de0, 0x10, kMsgBusy);
            } else {
                JReport(0x10122de0, 0x12, kMsgDepth);
                r = 0x17;
            }
            break;
        case 0x10:
            if (SpriteBusy(self))
                r = reinterpret_cast<M5_t>(JgldVA(0x100019b5))(self, 0, dst, x, y, mask, lv);
            else
                r = reinterpret_cast<M5_t>(JgldVA(0x10001a78))(self, 0, dst, x, y, mask, lv);
            break;
        default:
            JReport(0x10122de0, 0x20, kMsgDepth);
            r = 0x17;
        }
        break;
    case 0x10:
        switch (*SurfaceFormat(dst)) {
        case 8:
        case 0x10:
            if (SpriteBusy(self)) {
                JReport(0x10122de0, 0x2b, kMsgBusy);
            } else {
                JReport(0x10122de0, 0x2d, kMsgDepth);
                r = 0x17;
            }
            break;
        default:
            JReport(0x10122de0, 0x34, kMsgDepth);
            r = 0x17;
        }
        break;
    default:
        JReport(0x10122de0, 0x3c, kMsgDepth);
        r = 0x17;
    }
    F<void*>(self, 0x10) = saved;
    SpriteUnlock(self);
    return r;
}

// 0x00018920  Sprite::drawAlpha(dst, x, y, float alpha, pal, char flag), Sprite vtable 0x1011d380 slot 37 (+0x94).
// `ret 0x18` at 0x100191a3. The result local starts at 0 (0x10018947) and a 565 flag at 1 (0x10018943). The
// Palette word is saved (0x10018954) and replaced when pal != 0 (0x10018957 / 0x10018963) FIRST; a NULL
// destination then returns 0x10 (0x1001896c) and an invalid sprite (slot +0x20 at 0x10018980) 7 (0x1001898e), both
// with the word left replaced and no release. The flag is cleared when the destination's format word [1] is 1
// (0x100189af). [this+0x20] other than 8 (0x100189c2) -> report base 0x10122e14 + 0x82 DEPTH, 0x17 (0x10019167).
// 8 bpp: depth 8 (0x100189e9) -> report + 0x1e NOT AVAILABLE, result 0; depth 0x10 (0x100189ef): busy (0x10018a3b)
// -> report + 0x23 BUSY, result 0, else the alpha tree; other depth -> report + 0x78 DEPTH, 0x17 (0x10019129).
//
// The alpha tree compares the float argument (x87, `fld dword [ebp+0x14]`) against the module's tolerance g, the
// float at 0x1011d378 (0.03125 in the file), combined with the doubles 0.5 0x1011d5f0, 0.75 0x1011d5e0, 0.875
// 0x1011d5d0, 0.625 0x1011d5c0, 0.25 0x1011d5b0, 0.375 0x1011d5a0, 0.125 0x1011d590. Every sum and difference
// with these is exact in double, and the A/B thread runs x87 at 53 bits, so a double computation is the same
// comparison. `fcompp` sets C0 when st0 < st1 (and C0/C2/C3 when unordered); `test ah, 1` reads C0, `test ah,
// 0x41` C0|C3.
//   alpha < 0.0 0x1011d040 or unordered (0x10018a82) -> 3; alpha > 1.0 0x1011d030 (0x10018a92) -> 3
//   alpha > g+0.5 (0x10018ab6):
//     alpha > g+0.75 (0x10018ad2):
//       alpha > g+0.875 (0x10018aee): 100.0 0x1011d34c - g < alpha (0x10018b08) -> slot +0x44 (dst, x, y, pal);
//                                     it cannot hold for alpha <= 1.0, so this arm never runs
//                                     else 0x100358b0 / 0x10049ea0
//       0.875-g <= alpha (0x10018b98) -> 0x10032d60 / 0x100473b0, else 0x1003dca0 / 0x100521d0
//     0.75-g <= alpha (0x10018c4d) -> 0x10030290 / 0x10044940
//     alpha > g+0.625 (0x10018c69) -> 0x10040930 / 0x10054e20
//     0.625-g <= alpha (0x10018cd0) -> 0x10038460 / 0x1004ca10, else 0x1003b060 / 0x1004f5d0
//   0.5-g <= alpha (0x10018dd1) -> 0x1002d890 / 0x10041f80
//   alpha > g+0.25 (0x10018ded):
//     alpha > g+0.375 (0x10018e09) -> 0x10039a60 / 0x1004dff0
//     0.375-g <= alpha (0x10018e71) -> 0x10036e60 / 0x1004b430, else 0x1003f2e0 / 0x100537f0
//   0.25-g <= alpha (0x10018f25) -> 0x1002ed60 / 0x10043430
//   alpha > g+0.125 (0x10018f41) -> 0x1003c660 / 0x10050bb0
//   0.125-g <= alpha (0x10018fa9) -> 0x100317c0 / 0x10045e50
//   alpha <= g (0x10018fb9) -> result 0, else 0x10034300 / 0x10048910
// (first member when the 565 flag is set, the second otherwise), each called through its thunk with (dst, x, y,
// flag). Join (0x1001916e): Palette word restored, slot +0x24 with 1, the result returned.
typedef int(__fastcall* Blend_t)(void*, void*, void*, int, int, int);
typedef int(__fastcall* Opaque_t)(void*, void*, void*, int, int, void*);
// thunk pairs (565, 555) as the original calls them
struct BlendPair { unsigned t565, t555; };
const BlendPair kB_358b0 = {0x100010e6, 0x10001154};   // 0x100358b0 / 0x10049ea0
const BlendPair kB_3dca0 = {0x10001046, 0x10001799};   // 0x1003dca0 / 0x100521d0
const BlendPair kB_32d60 = {0x10001505, 0x100014d3};   // 0x10032d60 / 0x100473b0
const BlendPair kB_40930 = {0x100010fa, 0x10001136};   // 0x10040930 / 0x10054e20
const BlendPair kB_3b060 = {0x100011a9, 0x10001389};   // 0x1003b060 / 0x1004f5d0
const BlendPair kB_38460 = {0x10001627, 0x1000106e};   // 0x10038460 / 0x1004ca10
const BlendPair kB_30290 = {0x100016e5, 0x10001802};   // 0x10030290 / 0x10044940
const BlendPair kB_39a60 = {0x100013d9, 0x10001262};   // 0x10039a60 / 0x1004dff0
const BlendPair kB_3f2e0 = {0x1000172b, 0x100016f9};   // 0x1003f2e0 / 0x100537f0
const BlendPair kB_36e60 = {0x10001276, 0x100012f3};   // 0x10036e60 / 0x1004b430
const BlendPair kB_3c660 = {0x1000150a, 0x10001460};   // 0x1003c660 / 0x10050bb0
const BlendPair kB_34300 = {0x100017cb, 0x10001681};   // 0x10034300 / 0x10048910
const BlendPair kB_317c0 = {0x10001668, 0x10001384};   // 0x100317c0 / 0x10045e50
const BlendPair kB_2ed60 = {0x100011ea, 0x1000126c};   // 0x1002ed60 / 0x10043430
const BlendPair kB_2d890 = {0x1000168b, 0x10001abe};   // 0x1002d890 / 0x10041f80

int Blend(const BlendPair& b, bool is565, void* self, void* dst, int x, int y, int flag) {
    return reinterpret_cast<Blend_t>(JgldVA(is565 ? b.t565 : b.t555))(self, 0, dst, x, y,
                                                                      static_cast<unsigned char>(flag));
}

int AlphaTree(void* self, void* dst, int x, int y, float alphaF, void* pal, int flag, bool is565) {
    const double a = static_cast<double>(alphaF);
    const double g = static_cast<double>(G<float>(0x1011d378));
    if (a < 0.0 || a != a)
        return 3;
    if (!(a <= 1.0))
        return 3;
    if (g + 0.5 < a) {
        if (g + 0.75 < a) {
            if (g + 0.875 < a) {
                if (static_cast<double>(G<float>(0x1011d34c)) - g < a)
                    return reinterpret_cast<Opaque_t>(VSlot(self, 0x44))(self, 0, dst, x, y, pal);
                return Blend(kB_358b0, is565, self, dst, x, y, flag);
            }
            if (0.875 - g <= a)
                return Blend(kB_32d60, is565, self, dst, x, y, flag);
            return Blend(kB_3dca0, is565, self, dst, x, y, flag);
        }
        if (0.75 - g <= a)
            return Blend(kB_30290, is565, self, dst, x, y, flag);
        if (g + 0.625 < a)
            return Blend(kB_40930, is565, self, dst, x, y, flag);
        if (0.625 - g <= a)
            return Blend(kB_38460, is565, self, dst, x, y, flag);
        return Blend(kB_3b060, is565, self, dst, x, y, flag);
    }
    if (0.5 - g <= a)
        return Blend(kB_2d890, is565, self, dst, x, y, flag);
    if (g + 0.25 < a) {
        if (g + 0.375 < a)
            return Blend(kB_39a60, is565, self, dst, x, y, flag);
        if (0.375 - g <= a)
            return Blend(kB_36e60, is565, self, dst, x, y, flag);
        return Blend(kB_3f2e0, is565, self, dst, x, y, flag);
    }
    if (0.25 - g <= a)
        return Blend(kB_2ed60, is565, self, dst, x, y, flag);
    if (g + 0.125 < a)
        return Blend(kB_3c660, is565, self, dst, x, y, flag);
    if (0.125 - g <= a)
        return Blend(kB_317c0, is565, self, dst, x, y, flag);
    if (a <= g)
        return 0;
    return Blend(kB_34300, is565, self, dst, x, y, flag);
}

typedef int(__fastcall* DrawAlpha_t)(void*, void*, void*, int, int, float, void*, int);
DrawAlpha_t DrawAlpha_orig;
int __fastcall DrawAlpha_re(void* self, void*, void* dst, int x, int y, float alpha, void* pal, int flag) {
    int r = 0;
    bool is565 = true;
    void* const saved = F<void*>(self, 0x10);
    if (pal)
        F<void*>(self, 0x10) = pal;
    if (dst == 0)
        return 0x10;
    if (!SpriteValid(self))
        return 7;
    if (SurfaceFormat(dst)[1] == 1)
        is565 = false;
    if (F<int>(self, 0x20) == 8) {
        switch (*SurfaceFormat(dst)) {
        case 8:
            JReport(0x10122e14, 0x1e, kMsgNone);
            break;
        case 0x10:
            if (SpriteBusy(self))
                JReport(0x10122e14, 0x23, kMsgBusy);
            else
                r = AlphaTree(self, dst, x, y, alpha, pal, flag, is565);
            break;
        default:
            JReport(0x10122e14, 0x78, kMsgDepth);
            r = 0x17;
        }
    } else {
        JReport(0x10122e14, 0x82, kMsgDepth);
        r = 0x17;
    }
    F<void*>(self, 0x10) = saved;
    SpriteUnlock(self);
    return r;
}


// =====================================================================================================================
// group W/A/F - Win32 users under the import stand-in (re/frida/js/importstandin.js). RULE (c3cy): every import is
// called through jgld's OWN import slot (base + slot RVA, log/c3/c3dg_jgld_iat.txt), never through the shim's.
// =====================================================================================================================
template <class T> T Imp(unsigned slotVa) { return *static_cast<T*>(JgldVA(slotVa)); }
const unsigned kIatGetModuleHandleA = 0x1012c410, kIatGetCurrentDirectoryA = 0x1012c418;
const unsigned kIatFormatMessageA = 0x1012c420, kIatGetLastError = 0x1012c444, kIatMessageBoxA = 0x1012c5dc;
const unsigned kIatGetDC = 0x1012c5ec, kIatRegisterClassA = 0x1012c5f8, kIatCreateWindowExA = 0x1012c5fc;
const unsigned kIatGetSystemMetrics = 0x1012c600, kIatLoadIconA = 0x1012c608, kIatPostMessageA = 0x1012c61c;
const unsigned kIatSelectObject = 0x1012c378, kIatSetTextAlign = 0x1012c37c, kIatCreateDIBSection = 0x1012c384;
const unsigned kIatCreateCompatibleDC = 0x1012c388, kIatSetBkMode = 0x1012c38c, kIatGetStockObject = 0x1012c3a4;
const unsigned kIatCreateScalableFontResourceA = 0x1012c3c4, kIatAddFontResourceA = 0x1012c3c8;

typedef void*(__stdcall* PtrOfPtr_t)(const void*);                    // GetModuleHandleA, GetDC, CreateCompatibleDC
typedef void*(__stdcall* LoadIconA_t)(void*, const char*);
typedef void*(__stdcall* GetStockObject_t)(int);
typedef unsigned(__stdcall* RegisterClassA_t)(const void*);
typedef int(__stdcall* GetSystemMetrics_t)(int);
typedef void*(__stdcall* CreateWindowExA_t)(unsigned, const char*, const char*, unsigned, int, int, int, int, void*,
                                            void*, void*, void*);
typedef int(__stdcall* SetBkMode_t)(void*, int);
typedef unsigned(__stdcall* SetTextAlign_t)(void*, unsigned);
typedef void*(__stdcall* CreateDIBSection_t)(void*, const void*, unsigned, void**, void*, unsigned);
typedef void*(__stdcall* SelectObject_t)(void*, void*);
typedef unsigned(__stdcall* NoArgs_t)();
typedef unsigned(__stdcall* FormatMessageA_t)(unsigned, const void*, unsigned, unsigned, char*, unsigned, void*);
typedef int(__stdcall* MessageBoxA_t)(void*, const char*, const char*, unsigned);
typedef unsigned(__stdcall* GetCurrentDirectoryA_t)(unsigned, char*);
typedef int(__stdcall* CreateScalableFontResourceA_t)(unsigned, const char*, const char*, const char*);
typedef int(__stdcall* AddFontResourceA_t)(const char*);
typedef int(__stdcall* PostMessageA_t)(void*, unsigned, unsigned, long);

// 0x000657e0  Display::createWindow(listener, title, mode), Display vtable 0x1011d640 slot 1 (+0x4). `ret 0xc` at
// 0x100659fe. GetModuleHandleA(NULL) 0x10065801 -> +0x140; the listener into the module's event sink 0x1012872c
// (0x1006581a); mode -> +0x148 (0x10065826). A WNDCLASSA on the stack: style 0x2b (0x1006582c), the window
// procedure thunk 0x10001302 (0x10065833), no class or window extra bytes, hInstance +0x140 (0x10065851),
// LoadIconA(NULL, IDI_APPLICATION 0x7f00) 0x1006585d, no cursor, GetStockObject(BLACK_BRUSH 4) 0x10065878, no
// menu, class name 0x1011d978 (0x1006588f). RegisterClassA 0x1006589c: an atom whose low word is 0 (`and eax,
// 0xffff` 0x100658a9) returns 1 (0x100658b2). CreateWindowExA(WS_EX_APPWINDOW 0x40000, 0x1011d978, title,
// 0x86000000 (0x100658bc), 0, 0, GetSystemMetrics(0), GetSystemMetrics(1), NULL, NULL, +0x140, NULL) 0x1006590f,
// the height's GetSystemMetrics(1) called first (0x100658d9, then 0x100658eb) -> +0x144; NULL returns 1
// (0x10065931). Then GetSystemMetrics(0) -> +0x12c (0x1006593f), GetSystemMetrics(1) -> +0x130 (0x10065959), 8 ->
// +0x134 (0x10065972), GetDC(+0x144) -> +0x138 (0x10065988), SetBkMode(+0x138, TRANSPARENT 1) 0x100659ac, and the
// mode's slot 0 (`call [edx]` 0x100659e3) with (+0x12c, +0x130, +0x134, 1), its result discarded; returns 0.
struct WndClassA { unsigned style; void* proc; int clsExtra, wndExtra; void* inst; void* icon; void* cursor;
                   void* brush; const char* menu; const char* cls; };
typedef int(__fastcall* ModeSet_t)(void*, void*, int, int, int, int);
typedef int(__fastcall* CreateWindow_t)(void*, void*, void*, const char*, void*);
CreateWindow_t CreateWindow_orig;
int __fastcall CreateWindow_re(void* self, void*, void* listener, const char* title, void* mode) {
    F<void*>(self, 0x140) = Imp<PtrOfPtr_t>(kIatGetModuleHandleA)(0);
    G<void*>(0x1012872c) = listener;
    F<void*>(self, 0x148) = mode;
    WndClassA wc;
    wc.style = 0x2b;
    wc.proc = JgldVA(0x10001302);
    wc.clsExtra = 0;
    wc.wndExtra = 0;
    wc.inst = F<void*>(self, 0x140);
    wc.icon = Imp<LoadIconA_t>(kIatLoadIconA)(0, reinterpret_cast<const char*>(0x7f00));
    wc.cursor = 0;
    wc.brush = Imp<GetStockObject_t>(kIatGetStockObject)(4);
    wc.menu = 0;
    wc.cls = static_cast<const char*>(JgldVA(0x1011d978));
    if ((Imp<RegisterClassA_t>(kIatRegisterClassA)(&wc) & 0xffff) == 0)
        return 1;
    const int h = Imp<GetSystemMetrics_t>(kIatGetSystemMetrics)(1);
    const int w = Imp<GetSystemMetrics_t>(kIatGetSystemMetrics)(0);
    F<void*>(self, 0x144) = Imp<CreateWindowExA_t>(kIatCreateWindowExA)(
        0x40000, static_cast<const char*>(JgldVA(0x1011d978)), title, 0x86000000, 0, 0, w, h, 0, 0,
        F<void*>(self, 0x140), 0);
    if (F<void*>(self, 0x144) == 0)
        return 1;
    F<int>(self, 0x12c) = Imp<GetSystemMetrics_t>(kIatGetSystemMetrics)(0);
    F<int>(self, 0x130) = Imp<GetSystemMetrics_t>(kIatGetSystemMetrics)(1);
    F<int>(self, 0x134) = 8;
    F<void*>(self, 0x138) = Imp<PtrOfPtr_t>(kIatGetDC)(F<void*>(self, 0x144));
    Imp<SetBkMode_t>(kIatSetBkMode)(F<void*>(self, 0x138), 1);
    reinterpret_cast<ModeSet_t>(VSlot(mode, 0))(mode, 0, F<int>(self, 0x12c), F<int>(self, 0x130),
                                                 F<int>(self, 0x134), 1);
    return 0;
}

// 0x00007e40  allocDrawBuffer(w, h, bpp, unused), Surface vtable 0x1011d0b0 slot 1 (+0x4). `ret 0x10` at
// 0x1000821e; the fourth argument is never read. Surface fields: +0x24 bits per pixel, +0x28 format, +0x2c..+0x36
// the channel words, +0x38 / +0x3c width / height, +0x40 pitch in pixels, +0x54 the bounds rect, +0x84 a
// BITMAPINFOHEADER, +0x4b0 the DC's previous bitmap, +0x4b4 the DIB section, +0x4bc the memory DC, +0x4c0 the bits.
//   w < 0 or h < 0 (signed, 0x10007e67 / 0x10007e6d) -> 3. Same w (+0x38, 0x10007e82) and h (+0x3c, 0x10007e8d)
//   and either the same depth (+0x24, 0x10007e98) or bpp 0 (0x10007e9e) -> 0.
//   Otherwise the surface's slot +0x8 (Surface::release on jgld's vtable, 0x10007eb1). bpp 0 (0x10007ebf): the
//   depth is slot +0xb0 of the module Display 0x10128420 (0x10007ed0), 8 when that is 0 (0x10007eea); else bpp.
//   CreateCompatibleDC(NULL) 0x10007f05 -> +0x4bc; NULL -> 1 (0x10007f27). biWidth +0x88 = w (0x10007f37),
//   biHeight +0x8c = -h (0x10007f40), biBitCount word +0x92 = the depth's low word (0x10007f55), biClrUsed +0xa4 = 0
//   (0x10007f5f); CreateDIBSection(+0x4bc, +0x84, DIB_RGB_COLORS 0, &+0x4c0, NULL, 0) 0x10007f8e -> +0x4b4.
//   NULL (0x10007fae): FormatMessageA(0x1200, NULL, GetLastError(), 0x400, stack buffer, 0x400, NULL) 0x10007fdc,
//   MessageBoxA(NULL, 0x1011d1d0, 0x1011d210, MB_ICONEXCLAMATION 0x30) 0x10007ff9 and the CRT's _exit(4) 0x1007f1a0
//   (0x10008008), which does not return in the game; the code after it is the success path.
//   SelectObject(+0x4bc, +0x4b4) 0x10008023 -> +0x4b0. Pitch +0x40 by depth through the tables 0x10008235 /
//   0x10008221 (0x1000806b): 8 -> (w + 3) >> 2 << 2, 16 -> (w + 1) >> 1 << 1 (arithmetic shifts), 24 -> w + (w & 1),
//   32 -> w, any other depth -> 0x17 (0x100080b6) with the DC and DIB kept. +0x38 = w, +0x3c = h, +0x28 = 0
//   (0x100080d5), and since +0x28 was just cleared the test 0x100080e3 always takes the same words: +0x2c 0xa,
//   +0x2e 5, +0x30 0, +0x36 0x1f, +0x34 0x1f ^ 0x3ff, +0x32 (+0x36 | +0x34) ^ 0x7fff (0x100080e5..0x1000813c).
//   SetBkMode(+0x4bc, 1) 0x100081ab, SetTextAlign(+0x4bc, 0x18) 0x100081c6, rectFromXYWH(+0x54, 0, 0, w, h)
//   through 0x10001019 (0x100081e6), slot +0x34 with +0x54 (Surface::setClip, 0x100081ff), returns 0.
typedef void(__fastcall* Release_t)(void*, void*);
typedef int(__fastcall* DispDepth_t)(void*, void*);
typedef void(__cdecl* RectFromXYWH_t)(void*, int, int, int, int);
typedef int(__fastcall* SetClip_t)(void*, void*, void*);
typedef void(__cdecl* Exit_t)(int);
typedef int(__fastcall* AllocDraw_t)(void*, void*, int, int, int, int);
AllocDraw_t AllocDraw_orig;
int __fastcall AllocDraw_re(void* self, void*, int w, int h, int bpp, int) {
    if (w < 0 || h < 0)
        return 3;
    if (w == F<int>(self, 0x38) && h == F<int>(self, 0x3c) && (F<int>(self, 0x24) == bpp || bpp == 0))
        return 0;
    reinterpret_cast<Release_t>(VSlot(self, 0x8))(self, 0);
    if (bpp == 0) {
        void* disp = G<void*>(0x10128420);
        F<int>(self, 0x24) = reinterpret_cast<DispDepth_t>(VSlot(disp, 0xb0))(disp, 0);
        if (F<int>(self, 0x24) == 0)
            F<int>(self, 0x24) = 8;
    } else {
        F<int>(self, 0x24) = bpp;
    }
    F<void*>(self, 0x4bc) = Imp<PtrOfPtr_t>(kIatCreateCompatibleDC)(0);
    if (F<void*>(self, 0x4bc) == 0)
        return 1;
    F<int>(self, 0x88) = w;
    F<int>(self, 0x8c) = -h;
    F<unsigned short>(self, 0x92) = static_cast<unsigned short>(F<int>(self, 0x24));
    F<int>(self, 0xa4) = 0;
    F<void*>(self, 0x4b4) = Imp<CreateDIBSection_t>(kIatCreateDIBSection)(
        F<void*>(self, 0x4bc), static_cast<char*>(self) + 0x84, 0, &F<void*>(self, 0x4c0), 0, 0);
    if (F<void*>(self, 0x4b4) == 0) {
        char buf[0x400];
        const unsigned err = Imp<NoArgs_t>(kIatGetLastError)();
        Imp<FormatMessageA_t>(kIatFormatMessageA)(0x1200, 0, err, 0x400, buf, 0x400, 0);
        Imp<MessageBoxA_t>(kIatMessageBoxA)(0, static_cast<const char*>(JgldVA(0x1011d1d0)),
                                            static_cast<const char*>(JgldVA(0x1011d210)), 0x30);
        reinterpret_cast<Exit_t>(JgldVA(0x1007f1a0))(4);
    }
    F<void*>(self, 0x4b0) = Imp<SelectObject_t>(kIatSelectObject)(F<void*>(self, 0x4bc), F<void*>(self, 0x4b4));
    // the adds wrap in 32 bits and the right shifts are arithmetic (`sar` 0x10008078 / 0x1000808c)
    const unsigned uw = static_cast<unsigned>(w);
    switch (F<int>(self, 0x24)) {
    case 8: F<unsigned>(self, 0x40) = static_cast<unsigned>(static_cast<int>(uw + 3) >> 2) << 2; break;
    case 16: F<unsigned>(self, 0x40) = static_cast<unsigned>(static_cast<int>(uw + 1) >> 1) << 1; break;
    case 24: F<unsigned>(self, 0x40) = uw + (uw & 1); break;
    case 32: F<int>(self, 0x40) = w; break;
    default: return 0x17;
    }
    F<int>(self, 0x38) = w;
    F<int>(self, 0x3c) = h;
    F<int>(self, 0x28) = 0;
    F<unsigned short>(self, 0x2c) = 0xa;
    F<unsigned short>(self, 0x2e) = 5;
    F<unsigned short>(self, 0x30) = 0;
    F<unsigned short>(self, 0x36) = 0x1f;
    F<unsigned short>(self, 0x34) = static_cast<unsigned short>(F<unsigned short>(self, 0x36) ^ 0x3ff);
    F<unsigned short>(self, 0x32) =
        static_cast<unsigned short>((F<unsigned short>(self, 0x36) | F<unsigned short>(self, 0x34)) ^ 0x7fff);
    Imp<SetBkMode_t>(kIatSetBkMode)(F<void*>(self, 0x4bc), 1);
    Imp<SetTextAlign_t>(kIatSetTextAlign)(F<void*>(self, 0x4bc), 0x18);
    reinterpret_cast<RectFromXYWH_t>(JgldVA(0x10001019))(static_cast<char*>(self) + 0x54, 0, 0, w, h);
    reinterpret_cast<SetClip_t>(VSlot(self, 0x34))(self, 0, static_cast<char*>(self) + 0x54);
    return 0;
}

// 0x00068ae0  Font::loadFile(file, name, a, b), Font vtable 0x1011da6c slot 1 (+0x4). `ret 0x10` at 0x10068d10.
// First the font's slot +0xc (0x10068b0d). file or name NULL (0x10068b1b / 0x10068b21) -> 0x10. +0x20 = jalloc
// (strlen(file) + 1) through 0x100017c6 (strlen 0x1007f6e0 at 0x10068b31, jalloc at 0x10068b3d); NULL -> 4
// (0x10068b54). strcpy(+0x20, file) 0x1007f760 (0x10068b69); the byte 4 before the copy's end is cleared
// (0x10068b94..0x10068ba3: a name shorter than 4 characters clears a byte before the block) and the string
// 0x1011da88 is appended (strcat 0x1007f770, 0x10068bb2). GetCurrentDirectoryA(0x104, stack buffer) 0x10068bc8,
// then strcat of 0x1011da84 (0x10068be1) and of file (0x10068bf4) onto it. CreateScalableFontResourceA(0, +0x20,
// buffer, NULL) 0x10068c10: on 0 (0x10068c1f), GetLastError 0x10068c23; a code other than 0x50 (0x10068c36) and 0
// (0x10068c3f) frees +0x20 (jfree through 0x100017b7, 0x10068c4f), clears it and returns 1. AddFontResourceA(+0x20)
// 0x10068c74: on 0 (0x10068c83), GetLastError 0x10068c87 (result unused), the same free, clear and 1.
// PostMessageA(HWND_BROADCAST 0xffff, WM_FONTCHANGE 0x1d, 0, 0) 0x10068cc1, +0x14 |= 1 (0x10068cd4), and the
// result of the font's slot +0x8 with (name, a, b) (0x10068cf3) is returned.
typedef void(__fastcall* FontSlot0_t)(void*, void*);
typedef int(__fastcall* FontCreate_t)(void*, void*, void*, int, int);
typedef unsigned(__cdecl* Strlen_t)(const char*);
typedef char*(__cdecl* Strcpy_t)(char*, const char*);
typedef void*(__cdecl* Jalloc_t)(unsigned);
typedef void(__cdecl* Jfree_t)(void*);
typedef int(__fastcall* LoadFile_t)(void*, void*, const char*, void*, int, int);
LoadFile_t LoadFile_orig;
int __fastcall LoadFile_re(void* self, void*, const char* file, void* name, int a, int b) {
    const Strlen_t strlen_ = reinterpret_cast<Strlen_t>(JgldVA(0x1007f6e0));
    const Strcpy_t strcpy_ = reinterpret_cast<Strcpy_t>(JgldVA(0x1007f760));
    const Strcpy_t strcat_ = reinterpret_cast<Strcpy_t>(JgldVA(0x1007f770));
    const Jfree_t jfree = reinterpret_cast<Jfree_t>(JgldVA(0x100017b7));
    reinterpret_cast<FontSlot0_t>(VSlot(self, 0xc))(self, 0);
    if (file == 0 || name == 0)
        return 0x10;
    F<char*>(self, 0x20) = static_cast<char*>(reinterpret_cast<Jalloc_t>(JgldVA(0x100017c6))(strlen_(file) + 1));
    if (F<char*>(self, 0x20) == 0)
        return 4;
    strcpy_(F<char*>(self, 0x20), file);
    char* end = F<char*>(self, 0x20) + strlen_(F<char*>(self, 0x20)) - 4;
    *end = 0;
    strcat_(F<char*>(self, 0x20), static_cast<const char*>(JgldVA(0x1011da88)));
    char path[0x104];
    Imp<GetCurrentDirectoryA_t>(kIatGetCurrentDirectoryA)(0x104, path);
    strcat_(path, static_cast<const char*>(JgldVA(0x1011da84)));
    strcat_(path, file);
    if (!Imp<CreateScalableFontResourceA_t>(kIatCreateScalableFontResourceA)(0, F<char*>(self, 0x20), path, 0)) {
        const unsigned err = Imp<NoArgs_t>(kIatGetLastError)();
        if (err != 0x50 && err != 0) {
            jfree(F<char*>(self, 0x20));
            F<char*>(self, 0x20) = 0;
            return 1;
        }
    }
    if (!Imp<AddFontResourceA_t>(kIatAddFontResourceA)(F<char*>(self, 0x20))) {
        Imp<NoArgs_t>(kIatGetLastError)();
        jfree(F<char*>(self, 0x20));
        F<char*>(self, 0x20) = 0;
        return 1;
    }
    Imp<PostMessageA_t>(kIatPostMessageA)(reinterpret_cast<void*>(0xffff), 0x1d, 0, 0);
    F<unsigned>(self, 0x14) |= 1;
    return reinterpret_cast<FontCreate_t>(VSlot(self, 0x8))(self, 0, name, a, b);
}


// =====================================================================================================================
// group P - libpng 1.0.5 (statically linked; re/match/jgld_png_*.cpp). Callees at their module addresses, cdecl:
// png_malloc 0x10078e60, png_free 0x10078ed0, png_zfree 0x10078750, png_info_destroy 0x10078980, inflateEnd
// 0x1009be70, memcpy 0x1007f3a0, memset 0x1007e7c0, strlen 0x1007f6e0.
// =====================================================================================================================
typedef void*(__cdecl* PngMalloc_t)(void*, unsigned);
typedef void(__cdecl* PngFree_t)(void*, void*);
typedef void*(__cdecl* Memcpy_t)(void*, const void*, unsigned);
typedef void*(__cdecl* Memset_t)(void*, int, unsigned);
typedef int(__cdecl* Cdecl1_t)(void*);
inline void* PngMalloc(void* png, unsigned n) { return reinterpret_cast<PngMalloc_t>(JgldVA(0x10078e60))(png, n); }
inline void PngFree(void* png, void* p) { reinterpret_cast<PngFree_t>(JgldVA(0x10078ed0))(png, p); }
inline void* JMemcpy(void* d, const void* s, unsigned n) { return reinterpret_cast<Memcpy_t>(JgldVA(0x1007f3a0))(d, s, n); }
inline unsigned JStrlen(const char* s) { return reinterpret_cast<Strlen_t>(JgldVA(0x1007f6e0))(s); }

// 0x0007de10  png_set_pCAL(png_ptr, info_ptr, purpose, X0, X1, type, nparams, units, params), cdecl, plain `ret`
// at 0x1007dfbe. png_ptr or info_ptr NULL (0x1007de2c / 0x1007de32) returns at once. Otherwise: info+0xa0 =
// png_malloc(strlen(purpose) + 1) (0x1007de53) and purpose copied there with that length (memcpy 0x1007de76);
// info+0xa4 = X0, +0xa8 = X1 (0x1007de84 / 0x1007de90); the byte +0xb4 = type, the byte +0xb5 = nparams (0x1007de9c /
// 0x1007dea8); info+0xac = png_malloc(strlen(units) + 1) (0x1007dec8) and units copied (0x1007deeb); info+0xb0 =
// png_malloc(nparams * 4 + 4) (the whole int nparams, `lea edx, [ecx*4+4]` 0x1007def6), its entry [nparams] = NULL
// (0x1007df1f), and for each i < nparams (signed `jge` 0x1007df3e) entry i = png_malloc(strlen(params[i]) + 1)
// (0x1007df60) with params[i] copied (0x1007df95). Last, info+0x8 |= 0x400 (`or dh, 4` 0x1007dfa5).
typedef void(__cdecl* SetPcal_t)(void*, void*, const char*, int, int, int, int, const char*, const char**);
SetPcal_t SetPcal_orig;
void __cdecl SetPcal_re(void* png, void* info, const char* purpose, int x0, int x1, int type, int nparams,
                        const char* units, const char** params) {
    if (png == 0 || info == 0)
        return;
    unsigned len = JStrlen(purpose) + 1;
    F<void*>(info, 0xa0) = PngMalloc(png, len);
    JMemcpy(F<void*>(info, 0xa0), purpose, len);
    F<int>(info, 0xa4) = x0;
    F<int>(info, 0xa8) = x1;
    F<unsigned char>(info, 0xb4) = static_cast<unsigned char>(type);
    F<unsigned char>(info, 0xb5) = static_cast<unsigned char>(nparams);
    len = JStrlen(units) + 1;
    F<void*>(info, 0xac) = PngMalloc(png, len);
    JMemcpy(F<void*>(info, 0xac), units, len);
    F<void**>(info, 0xb0) = static_cast<void**>(PngMalloc(png, static_cast<unsigned>(nparams) * 4 + 4));
    F<void**>(info, 0xb0)[nparams] = 0;
    for (int i = 0; i < nparams; i++) {
        len = JStrlen(params[i]) + 1;
        F<void**>(info, 0xb0)[i] = PngMalloc(png, len);
        JMemcpy(F<void**>(info, 0xb0)[i], params[i], len);
    }
    F<unsigned>(info, 0x8) |= 0x400;
}

// 0x0006d250  png_read_destroy(png_ptr, info_ptr, end_info_ptr), cdecl, plain `ret` at 0x1006d5cc.
// png_info_destroy (0x10078980) on info_ptr and on end_info_ptr when not NULL (0x1006d272 / 0x1006d288); png_free of
// +0xac, +0xec, +0xe8, +0x1ec, +0x1f0, +0x164, +0x168, +0x16c in that order (0x1006d2a8..0x1006d342); with the flags
// word +0x6c: 0x1000 -> png_zfree(+0x114) (0x1006d368), 0x2000 -> png_free(+0x188) (0x1006d38e), 0x4000 ->
// png_free(+0x1f4) (0x1006d3b3). Each of the three table arrays +0x170, +0x174, +0x178 when not NULL (0x1006d3c5,
// 0x1006d437, 0x1006d4a9): its first (1 << (8 - +0x158)) entries freed (`shl eax, cl`, so the count is taken mod 32;
// the loop compares signed, 0x1006d3f7) and then the array. png_free(+0x210) (0x1006d51f), inflateEnd(+0x74)
// (0x1006d52e), png_free(+0x1b0) (0x1006d544). The 0x40-byte jmp_buf at +0 is copied to the stack (0x1006d556), the
// three words +0x40 / +0x44 / +0x48 kept, the 0x21c-byte struct cleared (memset 0x1006d584), the three words put
// back (0x1006d592..0x1006d5a4) and the jmp_buf copied back (0x1006d5b1).
typedef void(__cdecl* ReadDestroy_t)(void*, void*, void*);
ReadDestroy_t ReadDestroy_orig;
void __cdecl ReadDestroy_re(void* png, void* info, void* endInfo) {
    const PngFree_t infoDestroy = reinterpret_cast<PngFree_t>(JgldVA(0x10078980));
    if (info)
        infoDestroy(png, info);
    if (endInfo)
        infoDestroy(png, endInfo);
    static const unsigned kFreed[] = {0xac, 0xec, 0xe8, 0x1ec, 0x1f0, 0x164, 0x168, 0x16c};
    for (unsigned k = 0; k < sizeof(kFreed) / sizeof(kFreed[0]); k++)
        PngFree(png, F<void*>(png, kFreed[k]));
    if (F<unsigned>(png, 0x6c) & 0x1000)
        reinterpret_cast<PngFree_t>(JgldVA(0x10078750))(png, F<void*>(png, 0x114));
    if (F<unsigned>(png, 0x6c) & 0x2000)
        PngFree(png, F<void*>(png, 0x188));
    if (F<unsigned>(png, 0x6c) & 0x4000)
        PngFree(png, F<void*>(png, 0x1f4));
    static const unsigned kTables[] = {0x170, 0x174, 0x178};
    for (unsigned t = 0; t < 3; t++) {
        void** table = F<void**>(png, kTables[t]);
        if (table == 0)
            continue;
        const int istop = static_cast<int>(1u << ((8 - F<int>(png, 0x158)) & 31));
        for (int i = 0; i < istop; i++)
            PngFree(png, F<void**>(png, kTables[t])[i]);
        PngFree(png, F<void*>(png, kTables[t]));
    }
    PngFree(png, F<void*>(png, 0x210));
    reinterpret_cast<Cdecl1_t>(JgldVA(0x1009be70))(static_cast<char*>(png) + 0x74);
    PngFree(png, F<void*>(png, 0x1b0));
    unsigned char jmp[0x40];
    JMemcpy(jmp, png, 0x40);
    void* const errorFn = F<void*>(png, 0x40);
    void* const warningFn = F<void*>(png, 0x44);
    void* const errorPtr = F<void*>(png, 0x48);
    reinterpret_cast<Memset_t>(JgldVA(0x1007e7c0))(png, 0, 0x21c);
    F<void*>(png, 0x40) = errorFn;
    F<void*>(png, 0x44) = warningFn;
    F<void*>(png, 0x48) = errorPtr;
    JMemcpy(png, jmp, 0x40);
}


// 0x0007d6b0  png_read_start_row(png_ptr), cdecl, plain `ret` at 0x1007dacd. Fields: +0x6c flags, +0x70
// transformations, +0x78 zstream.avail_in, +0xc8 width, +0xcc height, +0xd0 num_rows, +0xd8 rowbytes, +0xdc
// irowbytes, +0xe0 iwidth, +0xe8 prev_row, +0xec row_buf, the word +0x11a num_trans, the bytes +0x123 interlaced,
// +0x124 pass, +0x126 color_type, +0x127 bit_depth, +0x129 pixel_depth, +0x64 / +0x65 the user transform's depth and
// channels. The Adam7 tables are module data: png_pass_start 0x10123228, png_pass_inc 0x10123244, png_pass_ystart
// 0x10123260, png_pass_yinc 0x1012327c.
//   avail_in = 0 (0x1007d6cb); png_init_read_transformations 0x100702a0 (0x1007d6d6).
//   interlaced (0x1007d6eb): num_rows = height when transformations & 2 (0x1007d6fc), else (height + yinc[0] - 1 -
//     ystart[0]) / yinc[0] (unsigned `div` 0x1007d719); iwidth = (width + inc[pass] - 1 - start[pass]) / inc[pass]
//     (0x1007d77c); irowbytes = ((iwidth * pixel_depth + 7) >> 3) + 1 (0x1007d7a0..0x1007d7b5). The png_error at
//     0x1007d7d2 needs the 32-bit value read back to differ from itself (0x1007d7c4) and cannot run.
//   not interlaced: num_rows = height, iwidth = width, irowbytes = rowbytes + 1 (0x1007d7dc..0x1007d80f).
//   max = pixel_depth (0x1007d820); PACK 4 with bit_depth < 8 -> 8 (0x1007d82e / 0x1007d83e). EXPAND 0x1000
//   (0x1007d855): palette 3 -> 32 with num_trans, else 24 (0x1007d879); gray 0 -> at least 8 (0x1007d8a0), doubled
//   with num_trans (0x1007d8b7); RGB 2 with num_trans -> max * 4 / 3 (signed `idiv` 0x1007d8f5). FILLER 0x8000
//   (0x1007d907): palette -> 32; gray -> 16 if max <= 8 else 32 (0x1007d935); RGB -> 32 if max <= 32 else 64
//   (0x1007d95d). GRAY_TO_RGB 0x4000 (0x1007d97d): when (num_trans and EXPAND) or FILLER or color_type 4
//   (0x1007d98d / 0x1007d99c / 0x1007d9ac / 0x1007d9bc) -> 32 if max <= 16 else 64 (0x1007d9c2); otherwise 24 if
//   max <= 8 else 48 (0x1007d9da). USER_TRANSFORM 0x100000 (0x1007d9f9): depth * channels when larger (signed `jle`
//   0x1007da17). row_buf = png_malloc(((((width + 7) & ~7) * max + 7) >> 3) + ((max + 7) >> 3) + 1) (`sar` for the
//   second term, 0x1007da1f..0x1007da56); prev_row = png_malloc(rowbytes + 1) (0x1007da78), cleared with
//   png_memset_check 0x10078f80 (0x1007daa6); flags |= 0x40 (0x1007dab4).
typedef void(__cdecl* MemsetCheck_t)(void*, void*, int, unsigned);
typedef void(__cdecl* StartRow_t)(void*);
StartRow_t StartRow_orig;
void __cdecl StartRow_re(void* png) {
    const unsigned* passStart = static_cast<const unsigned*>(JgldVA(0x10123228));
    const unsigned* passInc = static_cast<const unsigned*>(JgldVA(0x10123244));
    const unsigned* passYStart = static_cast<const unsigned*>(JgldVA(0x10123260));
    const unsigned* passYInc = static_cast<const unsigned*>(JgldVA(0x1012327c));
    F<unsigned>(png, 0x78) = 0;
    reinterpret_cast<StartRow_t>(JgldVA(0x100702a0))(png);
    const unsigned trans = F<unsigned>(png, 0x70);
    if (F<unsigned char>(png, 0x123)) {
        if (!(trans & 2))
            F<unsigned>(png, 0xd0) = (F<unsigned>(png, 0xcc) + passYInc[0] - 1 - passYStart[0]) / passYInc[0];
        else
            F<unsigned>(png, 0xd0) = F<unsigned>(png, 0xcc);
        const unsigned pass = F<unsigned char>(png, 0x124);
        F<unsigned>(png, 0xe0) = (F<unsigned>(png, 0xc8) + passInc[pass] - 1 - passStart[pass]) / passInc[pass];
        F<unsigned>(png, 0xdc) = ((F<unsigned>(png, 0xe0) * F<unsigned char>(png, 0x129) + 7) >> 3) + 1;
    } else {
        F<unsigned>(png, 0xd0) = F<unsigned>(png, 0xcc);
        F<unsigned>(png, 0xe0) = F<unsigned>(png, 0xc8);
        F<unsigned>(png, 0xdc) = F<unsigned>(png, 0xd8) + 1;
    }
    int maxDepth = F<unsigned char>(png, 0x129);
    const int colorType = F<unsigned char>(png, 0x126);
    const int numTrans = F<unsigned short>(png, 0x11a);
    if ((trans & 4) && F<unsigned char>(png, 0x127) < 8)
        maxDepth = 8;
    if (trans & 0x1000) {
        if (colorType == 3) {
            maxDepth = numTrans ? 0x20 : 0x18;
        } else if (colorType == 0) {
            if (maxDepth < 8)
                maxDepth = 8;
            if (numTrans)
                maxDepth <<= 1;
        } else if (colorType == 2 && numTrans) {
            maxDepth = (maxDepth << 2) / 3;
        }
    }
    if (trans & 0x8000) {
        if (colorType == 3)
            maxDepth = 0x20;
        else if (colorType == 0)
            maxDepth = maxDepth <= 8 ? 0x10 : 0x20;
        else if (colorType == 2)
            maxDepth = maxDepth <= 0x20 ? 0x20 : 0x40;
    }
    if (trans & 0x4000) {
        if ((numTrans && (trans & 0x1000)) || (trans & 0x8000) || colorType == 4)
            maxDepth = maxDepth <= 0x10 ? 0x20 : 0x40;
        else
            maxDepth = maxDepth <= 8 ? 0x18 : 0x30;
    }
    if (trans & 0x100000) {
        const int user = F<unsigned char>(png, 0x64) * F<unsigned char>(png, 0x65);
        if (user > maxDepth)
            maxDepth = user;
    }
    unsigned rowBytes = (F<unsigned>(png, 0xc8) + 7) & 0xfffffff8u;
    rowBytes = ((rowBytes * static_cast<unsigned>(maxDepth) + 7) >> 3) + static_cast<unsigned>((maxDepth + 7) >> 3) + 1;
    F<void*>(png, 0xec) = PngMalloc(png, rowBytes);
    F<void*>(png, 0xe8) = PngMalloc(png, F<unsigned>(png, 0xd8) + 1);
    reinterpret_cast<MemsetCheck_t>(JgldVA(0x10078f80))(png, F<void*>(png, 0xe8), 0, F<unsigned>(png, 0xd8) + 1);
    F<unsigned>(png, 0x6c) |= 0x40;
}


// ---- x87 helpers for png_build_gamma_table. The original leaves the result of FUN_10082a79 (called with two
// doubles on the stack, result in st0; it is the call pngrtran.c writes as pow, re/match/jgld_png_rtran.cpp 100%)
// in st0 and multiplies it there, so the product is rounded once from whatever precision that routine returned.
// A C++ expression would first store the result to a double. PowScaled replays the original's instruction
// sequence instead: pow(x, g), `fmul qword` scale, `fadd qword` 0.5 (0x1011d5f0), then jgld's __ftol 0x1007f12c
// (fistp qword with the rounding mode set to chop, low dword returned), under the caller's control word.
unsigned PowScaled(double x, double g, unsigned scaleVa) {
    void* const powFn = JgldVA(0x10082a79);
    void* const ftolFn = JgldVA(0x1007f12c);
    const double* const scale = static_cast<const double*>(JgldVA(scaleVa));
    const double* const half = static_cast<const double*>(JgldVA(0x1011d5f0));
    unsigned r;
    __asm {
        sub esp, 16
        fld qword ptr [x]
        fstp qword ptr [esp]
        fld qword ptr [g]
        fstp qword ptr [esp + 8]
        call dword ptr [powFn]
        add esp, 16
        mov eax, scale
        fmul qword ptr [eax]
        mov eax, half
        fadd qword ptr [eax]
        call dword ptr [ftolFn]
        mov r, eax
    }
    return r;
}
// FUN_10082a79 with its result stored to a double, as the 16_TO_8 arm does (`fstp qword [ebp-0x34]` 0x1007816a)
double PowStored(double x, double g) {
    typedef double(__cdecl* Pow_t)(double, double);
    return reinterpret_cast<Pow_t>(JgldVA(0x10082a79))(x, g);
}
// jgld's __ftol 0x1007f12c on a double (low dword)
unsigned Ftol(double v) {
    void* const ftolFn = JgldVA(0x1007f12c);
    unsigned r;
    __asm {
        fld qword ptr [v]
        call dword ptr [ftolFn]
        mov r, eax
    }
    return r;
}

// 0x00077d30  png_build_gamma_table(png_ptr), cdecl, plain `ret` at 0x10078541. Fields: +0x15c gamma and +0x160
// screen_gamma (floats), +0x70 transformations, the bytes +0x126 color_type, +0x127 bit_depth, +0x17c..+0x17f
// sig_bit red / green / blue / gray, the dword +0x158 gamma_shift, the tables +0x164 gamma_table, +0x168
// gamma_from_1, +0x16c gamma_to_1, +0x170 gamma_16_table, +0x174 gamma_16_from_1, +0x178 gamma_16_to_1.
// Constants: 0.0 0x1011d040, 1.0 0x1011d030, 0.5 0x1011d5f0, 255.0 0x1011ddc0, 65535.0 0x1011de40, 256.0
// 0x1011de48, 1e-6 0x1011de50; png_gamma_shift 0x1012316c.
//   gamma == 0.0 or unordered (`test ah, 0x40` 0x10077d5f) -> nothing.
//   bit_depth <= 8 (0x10077d76): g = 1 / (gamma * screen_gamma) when screen_gamma > 1e-6 (`test ah, 0x41`
//     0x10077d8d), else 1.0. gamma_table = png_malloc(256) (0x10077dc6); entry i = __ftol(pow(i / 255.0, g) * 255.0
//     + 0.5) (0x10077dfa..0x10077e2e). When transformations & 0x600080 (0x10077e41): g = 1 / gamma, gamma_to_1 (+0x16c,
//     0x10077e62) the same way; gamma_from_1 (+0x168, 0x10077ed8) with g = 1 / screen_gamma when screen_gamma > 1e-6
//     (0x10077efd), else gamma.
//   bit_depth > 8: sig_bit = max(red, green, blue) when color_type & 2 (0x10077f90), else gray; shift = 16 - sig_bit
//     when sig_bit > 0 (0x10077ff0), else 0; with 16_TO_8 0x400 (0x10078014) at least 5 (0x1007801a); at most 8
//     (0x10078027), at least 0 (0x10078034); gamma_shift = shift & 0xff (dword store 0x10078049); num = 1 << (8 -
//     shift); g as in the 8-bit case (0x10078075); gamma_16_table = png_malloc(num * 4) (0x100780ad).
//     transformations & 0x480 (0x100780cc): num tables of png_malloc(0x200) (0x100780f5), g = 1 / g (0x1007810e),
//       last = 0; for i < 256: fout = (i + 0.5) / 256.0, fin = pow(fout, g) stored to a double, max = __ftol(fin *
//       (num << 8)) (0x1007817d..0x10078183); while last <= max (unsigned `ja` 0x10078191) the word
//       table[last & (0xff >> shift)][last >> (8 - shift)] = i | i << 8; then the rest up to num << 8 (`jae`
//       0x100781ee) gets 0xffff.
//     otherwise: for i < num, table i = png_malloc(0x200) (0x10078255), ig = (i * png_gamma_shift[shift]) >> 4,
//       entry j = __ftol(pow((ig + (j << 8)) / 65535.0, g) * 65535.0 + 0.5) (0x100782b8..0x100782db).
//     then transformations & 0x600080 (0x1007830a): g = 1 / gamma, gamma_16_to_1 (+0x178, 0x1007832d) the same way;
//     g = 1 / screen_gamma when screen_gamma > 1e-6 (0x10078421) else gamma, gamma_16_from_1 (+0x174, 0x1007844e).
typedef void(__cdecl* BuildGamma_t)(void*);
BuildGamma_t BuildGamma_orig;

void Fill8(void* png, unsigned off, double g) {
    F<unsigned char*>(png, off) = static_cast<unsigned char*>(PngMalloc(png, 0x100));
    for (int i = 0; i < 0x100; i++)
        F<unsigned char*>(png, off)[i] = static_cast<unsigned char>(PowScaled(static_cast<double>(i) / 255.0, g, 0x1011ddc0));
}
void Fill16(void* png, unsigned off, int num, int shift, double g) {
    const unsigned* gammaShift = static_cast<const unsigned*>(JgldVA(0x1012316c));
    F<unsigned short**>(png, off) = static_cast<unsigned short**>(PngMalloc(png, static_cast<unsigned>(num) << 2));
    for (int i = 0; i < num; i++) {
        F<unsigned short**>(png, off)[i] = static_cast<unsigned short*>(PngMalloc(png, 0x200));
        const unsigned ig = (static_cast<unsigned>(i) * gammaShift[shift]) >> 4;
        for (int j = 0; j < 0x100; j++) {
            const double x = static_cast<double>(static_cast<unsigned long long>(ig + (static_cast<unsigned>(j) << 8)));
            F<unsigned short**>(png, off)[i][j] = static_cast<unsigned short>(PowScaled(x / 65535.0, g, 0x1011de40));
        }
    }
}

void __cdecl BuildGamma_re(void* png) {
    const double gamma = static_cast<double>(F<float>(png, 0x15c));
    const double screen = static_cast<double>(F<float>(png, 0x160));
    if (gamma == 0.0 || gamma != gamma)
        return;
    const unsigned trans = F<unsigned>(png, 0x70);
    if (F<unsigned char>(png, 0x127) <= 8) {
        double g = screen > 1e-6 ? 1.0 / (static_cast<double>(F<float>(png, 0x15c)) * screen) : 1.0;
        Fill8(png, 0x164, g);
        if (trans & 0x600080) {
            g = 1.0 / gamma;
            Fill8(png, 0x16c, g);
            g = screen > 1e-6 ? 1.0 / screen : gamma;
            Fill8(png, 0x168, g);
        }
        return;
    }
    int sigBit;
    if (F<unsigned char>(png, 0x126) & 2) {
        sigBit = F<unsigned char>(png, 0x17c);
        if (F<unsigned char>(png, 0x17d) > sigBit)
            sigBit = F<unsigned char>(png, 0x17d);
        if (F<unsigned char>(png, 0x17e) > sigBit)
            sigBit = F<unsigned char>(png, 0x17e);
    } else {
        sigBit = F<unsigned char>(png, 0x17f);
    }
    int shift = sigBit > 0 ? 0x10 - sigBit : 0;
    if ((trans & 0x400) && shift < 5)
        shift = 5;
    if (shift > 8)
        shift = 8;
    if (shift < 0)
        shift = 0;
    F<unsigned>(png, 0x158) = static_cast<unsigned>(shift) & 0xff;
    const int num = 1 << (8 - shift);
    double g = screen > 1e-6 ? 1.0 / (gamma * screen) : 1.0;
    if (trans & 0x480) {
        F<unsigned short**>(png, 0x170) = static_cast<unsigned short**>(PngMalloc(png, static_cast<unsigned>(num) << 2));
        for (int i = 0; i < num; i++)
            F<unsigned short**>(png, 0x170)[i] = static_cast<unsigned short*>(PngMalloc(png, 0x200));
        g = 1.0 / g;
        unsigned last = 0;
        const unsigned total = static_cast<unsigned>(num) << 8;
        for (int i = 0; i < 0x100; i++) {
            const double fout = (static_cast<double>(i) + 0.5) / 256.0;
            const double fin = PowStored(fout, g);
            const unsigned max = Ftol(static_cast<double>(static_cast<unsigned long long>(total)) * fin);
            while (last <= max) {
                F<unsigned short**>(png, 0x170)[last & (0xffu >> shift)][last >> (8 - shift)] =
                    static_cast<unsigned short>((i & 0xffff) | ((i & 0xffff) << 8));
                last++;
            }
        }
        while (last < total) {
            F<unsigned short**>(png, 0x170)[last & (0xffu >> shift)][last >> (8 - shift)] = 0xffff;
            last++;
        }
    } else {
        // the original allocates the array here (0x100780ad) before testing 0x480; Fill16 does the same
        Fill16(png, 0x170, num, shift, g);
    }
    if (trans & 0x600080) {
        g = 1.0 / gamma;
        Fill16(png, 0x178, num, shift, g);
        g = screen > 1e-6 ? 1.0 / screen : gamma;
        Fill16(png, 0x174, num, shift, g);
    }
}

}  // namespace

// 0x000654b0  Display::Display
SG_HOOK("jgld.dll", 0x000654B0, Display_Display, DisplayCtor_re, DisplayCtor_orig);
// 0x00065630  DisplayBase::ctor
SG_HOOK("jgld.dll", 0x00065630, DisplayBase_ctor, DisplayBaseCtor_re, DisplayBaseCtor_orig);
// 0x00069270  DisplayMember::ctor
SG_HOOK("jgld.dll", 0x00069270, DisplayMember_ctor, DisplayMemberCtor_re, DisplayMemberCtor_orig);
// 0x000157e0  Sprite::drawC
SG_HOOK("jgld.dll", 0x000157E0, Sprite_drawC, DrawC_re, DrawC_orig);
// 0x00015bb0  Sprite::drawD
SG_HOOK("jgld.dll", 0x00015BB0, Sprite_drawD, DrawD_re, DrawD_orig);
// 0x00018920  Sprite::drawAlpha
SG_HOOK("jgld.dll", 0x00018920, Sprite_drawAlpha, DrawAlpha_re, DrawAlpha_orig);
// 0x000657e0  Display::createWindow
SG_HOOK("jgld.dll", 0x000657E0, Display_createWindow, CreateWindow_re, CreateWindow_orig);
// 0x00007e40  allocDrawBuffer
SG_HOOK("jgld.dll", 0x00007E40, allocDrawBuffer, AllocDraw_re, AllocDraw_orig);
// 0x00068ae0  Font::loadFile
SG_HOOK("jgld.dll", 0x00068AE0, Font_loadFile, LoadFile_re, LoadFile_orig);
// 0x0007de10  png_set_pCAL
SG_HOOK("jgld.dll", 0x0007DE10, png_set_pCAL, SetPcal_re, SetPcal_orig);
// 0x0006d250  png_read_destroy
SG_HOOK("jgld.dll", 0x0006D250, png_read_destroy, ReadDestroy_re, ReadDestroy_orig);
// 0x0007d6b0  png_read_start_row
SG_HOOK("jgld.dll", 0x0007D6B0, png_read_start_row, StartRow_re, StartRow_orig);
// 0x00077d30  png_build_gamma_table
SG_HOOK("jgld.dll", 0x00077D30, png_build_gamma_table, BuildGamma_re, BuildGamma_orig);
