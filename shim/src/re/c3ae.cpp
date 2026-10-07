// C3 batch c3ae (revisit, 2026-10-08) of golf_clean.exe: eight functions offered in earlier rounds and left at C2.
// Six are matched at 100% by VC6 (sources in re/match/); all are hand-written here from the disassembly
// (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and the C2 transcriptions, each body citing the
// address of every global, offset and callee it uses. __thiscall / single-argument __fastcall is emulated with
// __fastcall (ecx = this, edx unused). Callees are called through their original addresses; every one of them is in
// another, already-promoted C3 batch (clearCost c3ab, distance c3b, decorationName c3m, Widget_value c3c,
// Widget_setQuad c3c, Widget_setQuad74 c3x, Widget_applyPalette c3ab, trimSpaces c3ab) or is a CRT import
// (__strnicmp, strcat), so both A/B arms run the identical callee.
//   rateLot             0x0042ef40  (candidate row 1; callee clearCost is hooked)   match re/match/golf_hand_s1.cpp
//   matchDirective      0x0048cd80  (deferred by earlier util rounds)               match re/match/golf_hand_03.cpp
//   sceneryLabel        0x00407b60  (deferred by earlier course rounds)             match re/match/golf_hand_r2.cpp
//   TextView_endSegment 0x00478700  (exe_6 text family: not attempted)              match re/match/golf_raw_02.cpp
//   Button::setColorNormal  0x00488930  (deferred; sibling of c3u's setColorHover)  match re/match/golf_raw_04.cpp
//   Button::setColorPressed 0x004889b0  (deferred)                                  match re/match/golf_raw_04.cpp
//   Widget_fillRect     0x00478b80  (forwarder; c3t left forwarders for later)      match re/match/golf_raw_02.cpp
//   Widget_get          0x00477560  (getter: not attempted)                         match re/match/golf_hand_00.cpp
#include <string.h>

#include "hooks.h"

namespace {

template <typename T> T& gref(unsigned addr) { return *reinterpret_cast<T*>(addr); }
inline unsigned ua(const void* p) { return reinterpret_cast<unsigned>(p); }

// ---- callees reached through their original addresses (each already C3 in another batch, or a CRT import) ----
typedef int(__cdecl* Xy_t)(int, int);
const Xy_t kClearCost = reinterpret_cast<Xy_t>(0x0042ee80);        // clearCost      (C3, c3ab)
const Xy_t kDistance = reinterpret_cast<Xy_t>(0x0040acd0);         // distance       (C3, c3b)
typedef int(__cdecl* Strnicmp_t)(const char*, const char*, unsigned);
const Strnicmp_t kStrnicmp = reinterpret_cast<Strnicmp_t>(0x004ad580);  // CRT __strnicmp
typedef void(__cdecl* StrFn_t)(char*);
const StrFn_t kTrimSpaces = reinterpret_cast<StrFn_t>(0x004925b0);      // trimSpaces (C3, c3ab)
typedef int(__cdecl* Deco_t)(int, int, int);
const Deco_t kDecorationName = reinterpret_cast<Deco_t>(0x00407700);    // decorationName (C3, c3m)
typedef int(__fastcall* WidgetVal_t)(void*, void*);
const WidgetVal_t kWidgetValue = reinterpret_cast<WidgetVal_t>(0x00477580);  // Widget_value (C3, c3c)
typedef void(__fastcall* Quad_t)(void*, void*, int, int, int, int);
const Quad_t kSetQuad = reinterpret_cast<Quad_t>(0x00476310);     // Widget_setQuad   (C3, c3c)
const Quad_t kSetQuad74 = reinterpret_cast<Quad_t>(0x00476370);   // Widget_setQuad74 (C3, c3x)
typedef int(__fastcall* ApplyPal_t)(void*, void*, int);
const ApplyPal_t kApplyPalette = reinterpret_cast<ApplyPal_t>(0x004789f0);  // Widget_applyPalette (C3, c3ab)

// 0x0042ef40  rateLot(x, y): lot/home-site value. First sums clearCost 0x0042ee80 over the 12-cell ring of the 2x2
// footprint at (x, y) (0x0042ef66..0x0042efb3). Then over the 18-entry record table 0x00575cb8 (stride 0x208): a
// record is live when its byte +0 is set (0x0042efdd) and its dword +0x20 is nonzero (0x0042efe7). For a live record
// v = (short)+0x158 * 1000 / (+0x20 + +0x24/2 + 4) + (3 - dword 0x00822c88) * 100 (0x0042f00f); bit 0 and bit 1 of the
// dword +0x200 each add 100 (0x0042f02d, 0x0042f034). d is the smallest of distance 0x0040acd0 over (x-+8, y-+0xc)
// and (x-+0x18, y-+0x1c) (0x0042f04f, 0x0042f066), and, when 0x0059aea8[i].x is not -1 (0x0042f07d),
// (x - (.x >> 10), y - (.y >> 10)) (0x0042f092). v is divided by d + 8 (0x0042f097) and kept as the running max
// best (0x0042f0ac). Finally, when the dword 0x00543cd0 is nonzero (0x0042f0da), best += 0x00543cd0 * best / 3; the
// return is best * sum / 40 (0x0042f106). Every division is signed.
typedef int(__cdecl* RateLot_t)(int, int);
RateLot_t rateLot_orig;
int __cdecl rateLot_re(int x, int y) {
    int sum = 0;
    for (int i = -1; i < 3; i++) {
        sum += kClearCost(x + i, y - 1);
        sum += kClearCost(x + i, y + 2);
    }
    for (int i = 0; i < 2; i++) {
        sum += kClearCost(x - 1, y + i);
        sum += kClearCost(x + 2, y + i);
    }
    int best = 0;
    for (int i = 0; i < 18; i++) {
        const unsigned r = 0x00575cb8 + i * 0x208;
        if (gref<signed char>(r + 0) != 0 && gref<int>(r + 0x20) != 0) {
            const int n = gref<int>(r + 0x20);
            const int m = gref<int>(r + 0x24);
            const short s158 = gref<short>(r + 0x158);
            int v = s158 * 1000 / (n + m / 2 + 4) + (3 - gref<int>(0x00822c88)) * 100;
            const int flags = gref<int>(r + 0x200);
            if (flags & 1) v += 100;
            if (flags & 2) v += 100;
            int d = kDistance(x - gref<int>(r + 8), y - gref<int>(r + 0xc));
            int d2 = kDistance(x - gref<int>(r + 0x18), y - gref<int>(r + 0x1c));
            if (d2 < d) d = d2;
            const unsigned pos = 0x0059aea8 + i * 0x18;
            if (gref<int>(pos + 0) != -1) {
                d2 = kDistance(x - (gref<int>(pos + 0) >> 10), y - (gref<int>(pos + 4) >> 10));
                if (d2 < d) d = d2;
            }
            v = v / (d + 8);
            if (v > best) best = v;
        }
    }
    if (gref<int>(0x00543cd0) != 0) best += gref<int>(0x00543cd0) * best / 3;
    return best * sum / 40;
}

// 0x0048cd80  matchDirective(pp): matches the leading token of *pp against the 22-entry keyword pointer table
// 0x004e4584. For i in 0..21 while no match (0x0048cda2): __strnicmp 0x004ad580 of *pp against table[i] over
// strlen(table[i]) (0x0048cdb7); when it is 0 (0x0048cdc1) it advances *pp by that length (0x0048cdd7), records i and
// calls trimSpaces 0x004925b0 on the advanced *pp (0x0048cdda). Returns the matched index, or -1 when pp is null
// (0x0048cd8d) or nothing matched.
typedef int(__cdecl* MatchDir_t)(char**);
MatchDir_t matchDirective_orig;
int __cdecl matchDirective_re(char** pp) {
    const char* const* table = reinterpret_cast<const char* const*>(0x004e4584);
    int result = -1;
    if (pp) {
        for (int i = 0; result == -1 && i < 22; i++) {
            const unsigned len = static_cast<unsigned>(strlen(table[i]));
            if (kStrnicmp(*pp, table[i], len) == 0) {
                *pp += len;
                result = i;
                kTrimSpaces(*pp);
            }
        }
    }
    return result;
}

// 0x00407b60  sceneryLabel(a, b, text): scans the 100-entry table 0x005689e8 (8-byte records: short a, b, idx) for the
// record whose a (0x00407b78) and b (0x00407b92) match. On a match: when text is 0 it returns 1 (0x00407bcd); else it
// strcats the golfer name 0x004d6098 + idx*0x230 (0x00407bb8) and the separator string 0x004c4e34 (0x00407c19) onto
// the scratch text buffer 0x0051a068, calls decorationName(a, b, -1) 0x00407700 (0x00407c4d) and returns 1. With no
// match: when text is nonzero it strcats the "Scenic" string 0x004c4e38 (0x00407c44) and returns 0; otherwise returns 0.
typedef int(__cdecl* Scenery_t)(int, int, int);
Scenery_t sceneryLabel_orig;
int __cdecl sceneryLabel_re(int a, int b, int text) {
    const short* rec = reinterpret_cast<const short*>(0x005689e8);  // [a, b, idx, pad] x 100
    char* const buf = reinterpret_cast<char*>(0x0051a068);
    for (int i = 0; i < 100; i++) {
        if (rec[i * 4 + 0] == a && rec[i * 4 + 1] == b) {
            if (text == 0) return 1;
            const int idx = rec[i * 4 + 2];
            strcat(buf, reinterpret_cast<const char*>(0x004d6098 + idx * 0x230));
            strcat(buf, reinterpret_cast<const char*>(0x004c4e34));
            kDecorationName(a, b, -1);
            return 1;
        }
    }
    if (text != 0) strcat(buf, reinterpret_cast<const char*>(0x004c4e38));
    return 0;
}

// 0x00478700  TextView_endSegment(this): closes a measured text segment. When the pending-width flag 0x00839aa8 is
// nonzero (0x0047870d) it adds Widget_value(this) 0x00477580 to the pen x 0x00839aa0 (0x0047871c) and increments the
// segment count this+0x1c (0x00478726). It then clears 0x00839aa4 (0x0047871c... store at 0x00478729) and the
// per-segment fields this+0x30/0x38/0x34/0x3c (0x0047872f..0x00478738). Returns the pen x 0x00839aa0.
typedef int(__fastcall* EndSeg_t)(void*, void*);
EndSeg_t TextView_endSegment_orig;
int __fastcall TextView_endSegment_re(void* self, void*) {
    if (gref<int>(0x00839aa8) != 0) {
        gref<int>(0x00839aa0) += kWidgetValue(self, 0);
        gref<int>(ua(self) + 0x1c) += 1;
    }
    gref<int>(0x00839aa4) = 0;
    gref<int>(ua(self) + 0x30) = 0;
    gref<int>(ua(self) + 0x38) = 0;
    gref<int>(ua(self) + 0x34) = 0;
    gref<int>(ua(self) + 0x3c) = 0;
    return gref<int>(0x00839aa0);
}

// 0x00488930  Button::setColorNormal(this, a, b, c, d): when the active flag this+0x130 is set (0x00488938) it calls
// Widget_applyPalette 0x004789f0 on the font object this+0x274 with the global palette 0x0083ad10 (0x00488949), then
// Widget_setQuad 0x00476310 on this+0x274 with (a, b, c, d) (0x00488964). Nothing otherwise.
typedef void(__fastcall* ButtonColor_t)(void*, void*, int, int, int, int);
ButtonColor_t Button_setColorNormal_orig;
void __fastcall Button_setColorNormal_re(void* self, void*, int a, int b, int c, int d) {
    if (gref<int>(ua(self) + 0x130) != 0) {
        void* font = reinterpret_cast<char*>(self) + 0x274;
        kApplyPalette(font, 0, gref<int>(0x0083ad10));
        kSetQuad(font, 0, a, b, c, d);
    }
}

// 0x004889b0  Button::setColorPressed(this, a, b, c, d): when the active flag this+0x130 is set (0x004889b8) it calls
// Widget_applyPalette 0x004789f0 on the font object this+0x274 with the global palette 0x0083ad10 (0x004889c9), then
// Widget_setQuad74 0x00476370 on this+0x274 with (a, b, c, d) (0x004889e4). Nothing otherwise.
ButtonColor_t Button_setColorPressed_orig;
void __fastcall Button_setColorPressed_re(void* self, void*, int a, int b, int c, int d) {
    if (gref<int>(ua(self) + 0x130) != 0) {
        void* font = reinterpret_cast<char*>(self) + 0x274;
        kApplyPalette(font, 0, gref<int>(0x0083ad10));
        kSetQuad74(font, 0, a, b, c, d);
    }
}

// 0x00478b80  Widget_fillRect(this, p1, p2, p3, p4, p5): when the surface object this+4 is non-null (0x00478b87) it
// calls that surface's virtual method at vtable+0x64 (slot 25) thiscall with (p1, p2, p3, p4, p5, 1) (0x00478b9a).
// Nothing otherwise.
typedef void(__fastcall* FillRect_t)(void*, void*, int, int, int, int, int);
FillRect_t Widget_fillRect_orig;
void __fastcall Widget_fillRect_re(void* self, void*, int p1, int p2, int p3, int p4, int p5) {
    void* surf = *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 4);
    if (surf == 0) return;
    void* vtbl = *reinterpret_cast<void**>(surf);
    typedef void(__fastcall* V_t)(void*, void*, int, int, int, int, int, int);
    V_t v = reinterpret_cast<V_t>(*reinterpret_cast<void**>(reinterpret_cast<char*>(vtbl) + 0x64));
    v(surf, 0, p1, p2, p3, p4, p5, 1);
}

// 0x00477560  Widget_get(this): returns the dword at p + 0x10, where p is the object pointer this+0x5c; when that is
// null (0x00477563) it first defaults p to the global 0x0083ad44 and stores it back at this+0x5c (0x0047756c).
typedef int(__fastcall* WidgetGet_t)(void*, void*);
WidgetGet_t Widget_get_orig;
int __fastcall Widget_get_re(void* self, void*) {
    void** slot = reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 0x5c);
    void* p = *slot;
    if (p == 0) {
        p = gref<void*>(0x0083ad44);
        *slot = p;
    }
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(p) + 0x10);
}

}  // namespace

// 0x0042ef40  rateLot
SG_HOOK("golf_clean.exe", 0x0042ef40, rateLot, rateLot_re, rateLot_orig);
// 0x0048cd80  matchDirective
SG_HOOK("golf_clean.exe", 0x0048cd80, matchDirective, matchDirective_re, matchDirective_orig);
// 0x00407b60  sceneryLabel
SG_HOOK("golf_clean.exe", 0x00407b60, sceneryLabel, sceneryLabel_re, sceneryLabel_orig);
// 0x00478700  TextView_endSegment
SG_HOOK("golf_clean.exe", 0x00478700, TextView_endSegment, TextView_endSegment_re, TextView_endSegment_orig);
// 0x00488930  Button::setColorNormal (SG_HOOK name is identifier-safe; registry key keeps the hooks.csv "::" name)
SG_HOOK("golf_clean.exe", 0x00488930, Button_setColorNormal, Button_setColorNormal_re, Button_setColorNormal_orig);
// 0x004889b0  Button::setColorPressed
SG_HOOK("golf_clean.exe", 0x004889b0, Button_setColorPressed, Button_setColorPressed_re, Button_setColorPressed_orig);
// 0x00478b80  Widget_fillRect
SG_HOOK("golf_clean.exe", 0x00478b80, Widget_fillRect, Widget_fillRect_re, Widget_fillRect_orig);
// 0x00477560  Widget_get
SG_HOOK("golf_clean.exe", 0x00477560, Widget_get, Widget_get_re, Widget_get_orig);
