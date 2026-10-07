// C3 batch c3ab (revisit, 2026-10-07) of golf_clean.exe: eight functions offered in earlier rounds and left at C2
// for time/scope (not for a hard reason). Hand-written from the disassembly
// (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and the C2 transcriptions; each body cites the
// address of every global, offset and callee it uses. __thiscall is emulated with __fastcall (ecx = this, edx
// unused), and so are the virtual methods reached through an object's vtable. Callees are called through their
// original addresses.
//   clearCost     0x0042ee80  (c3q deferred: "next plausible table-driven one")
//   landmarkName  0x004074a0  (not attempted in earlier course rounds)
//   stripNewline  0x004925d0  (c3w/c3r string helpers: not attempted)
//   trimSpaces    0x004925b0  (c3w: left for a later batch)
//   measureTextWidth 0x00483930 (ui text helper: not attempted)
//   Widget_applyPalette 0x004789f0 (c3t: "not examined in detail")
//   Widget_fillRectR 0x00475b00   (c3t: "not examined in detail")
//   Widget_fillArea  0x00478b50   (c3t: forwarders left for a later round)
#include <string.h>

#include "hooks.h"

namespace {

// ---- callees reached through their original addresses (none of them are hooked by any batch) ----
typedef int(__cdecl* Blocked_t)(int, int);
const Blocked_t kTileBlocked = reinterpret_cast<Blocked_t>(0x0040bf60);   // tileBlocked (C3, golf_math.cpp)
typedef char*(__cdecl* Strchr_t)(const char*, int);
const Strchr_t kStrchr = reinterpret_cast<Strchr_t>(0x004a6c50);          // CRT strchr
typedef void(__cdecl* StrFn_t)(char*);
const StrFn_t kTrimTrailing = reinterpret_cast<StrFn_t>(0x00492570);      // trailing-space trim (0x004a6598 class)
const StrFn_t kTrimLeading = reinterpret_cast<StrFn_t>(0x004924e0);       // leading-space trim

char* const kTextBuf = reinterpret_cast<char*>(0x0051a068);               // shared scratch text buffer

// 0x0042ee80  clearCost(x, y): the cost to clear tile (x, y) from its type. 0 when tileBlocked 0x0040bf60 returns
// nonzero (0x0042ee88). Type byte at 0x005722e8 + x*50 + y (0x0042ee93): type 4 -> 12 (0x0042ee9f); type 0x14 -> 0
// (0x0042eea6, dead because tileBlocked already returns for 0x14); type 0x15 -> -16 (0x0042eeb0). Otherwise the
// type record is 0x00578370 + type*0x30: when byte +2 <= 0 -> -8 (0x0042eec1); type 0x11 -> 32 (0x0042eed0); when the
// course-type byte 0x005a34e0 == 1 and type 0x12 -> 16 (0x0042eedf); when byte +6 == 13 -> (+3 * 5) / 2 (0x0042eef9);
// else (+3 * +2) / 2 (0x0042ef10). All divisions are signed (truncating toward zero).
typedef int(__cdecl* ClearCost_t)(int, int);
ClearCost_t ClearCost_orig;
int __cdecl ClearCost_re(int x, int y) {
    if (kTileBlocked(x, y)) return 0;
    const signed char* types = reinterpret_cast<const signed char*>(0x005722e8);
    const int t = types[x * 50 + y];
    if (t == 4) return 0xc;
    if (t == 0x14) return 0;
    if (t == 0x15) return -0x10;
    const signed char* rec = reinterpret_cast<const signed char*>(0x00578370 + t * 0x30);
    if (rec[2] <= 0) return -8;
    if (t == 0x11) return 0x20;
    if (*reinterpret_cast<const signed char*>(0x005a34e0) == 1 && t == 0x12) return 0x10;
    if (rec[6] == 13) return rec[3] * 5 / 2;
    return rec[3] * rec[2] / 2;
}

// 0x004074a0  landmarkName(type, full): appends to the scratch buffer 0x0051a068 the landmark name for `type`
// (switch 0..0x12 through the jump table at 0x004076ac, default at 0x00407679); `full` nonzero picks the long name,
// zero the short name (case 10 at 0x004074ba has no short variant). The names are the binary strings at
// 0x004c4980..0x004c4c44; the append is the inlined strcat into 0x0051a068 at 0x0040769d.
typedef void(__cdecl* Landmark_t)(int, int);
Landmark_t LandmarkName_orig;
inline const char* pick(int full, unsigned longAddr, unsigned shortAddr) {
    return reinterpret_cast<const char*>(full ? longAddr : shortAddr);
}
void __cdecl LandmarkName_re(int type, int full) {
    const char* s;
    switch (type) {
    case 0:  s = pick(full, 0x004c4c44, 0x004c4c3c); break;
    case 1:  s = pick(full, 0x004c4c28, 0x004c4c20); break;
    case 2:  s = pick(full, 0x004c4c00, 0x004c4bec); break;
    case 3:  s = pick(full, 0x004c4bd0, 0x004c4bc4); break;
    case 4:  s = pick(full, 0x004c4bac, 0x004c4ba0); break;
    case 5:  s = pick(full, 0x004c4b8c, 0x004c4b80); break;
    case 6:  s = pick(full, 0x004c4b60, 0x004c4b4c); break;
    case 7:  s = pick(full, 0x004c4b2c, 0x004c4b20); break;
    case 8:  s = pick(full, 0x004c4b0c, 0x004c4b04); break;
    case 9:  s = pick(full, 0x004c4af4, 0x004c4ae8); break;
    case 10: s = reinterpret_cast<const char*>(0x004c4ad4); break;
    case 11: s = pick(full, 0x004c4ab4, 0x004c4aa0); break;
    case 12: s = pick(full, 0x004c4a8c, 0x004c4a84); break;
    case 13: s = pick(full, 0x004c4a64, 0x004c4a50); break;
    case 14: s = pick(full, 0x004c4a38, 0x004c4a28); break;
    case 15: s = pick(full, 0x004c4a10, 0x004c4a00); break;
    case 16: s = pick(full, 0x004c49e8, 0x004c49dc); break;
    case 17: s = pick(full, 0x004c49c0, 0x004c49b0); break;
    case 18: s = pick(full, 0x004c4998, 0x004c498c); break;
    default: s = reinterpret_cast<const char*>(0x004c4980); break;
    }
    strcat(kTextBuf, s);
}

// 0x004925d0  stripNewline(s): truncates the string at its first '\n' by writing a NUL there. strchr(s, '\n') at
// 0x004925d7 (the import at 0x004a6c50); when it returns non-null, store 0 at that byte (0x004925e3).
typedef void(__cdecl* StripNewline_t)(char*);
StripNewline_t StripNewline_orig;
void __cdecl StripNewline_re(char* s) {
    char* p = kStrchr(s, '\n');
    if (p) *p = 0;
}

// 0x004925b0  trimSpaces(s): trims the string in place by calling the trailing-space trimmer 0x00492570
// (0x004925b6) and then the leading-space trimmer 0x004924e0 (0x004925bc), both on the same pointer.
typedef void(__cdecl* TrimSpaces_t)(char*);
TrimSpaces_t TrimSpaces_orig;
void __cdecl TrimSpaces_re(char* s) {
    kTrimTrailing(s);
    kTrimLeading(s);
}

// ---- helper for the vtable slot at a byte offset ----
inline void* vslot(void* obj, unsigned off) {
    void* vtbl = *reinterpret_cast<void**>(obj);
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(vtbl) + off);
}

// 0x00483930  measureTextWidth(this, str, maxlen): 0 when the font object this+4 is null (0x00483936) or `str` is
// null (0x0048393e). Else length = min(maxlen, strlen(str)) with a signed compare (0x00483951: maxlen < strlen keeps
// maxlen, else recompute strlen); returns the font's virtual method at vtable+0x10 (slot 4) called thiscall with
// (str, length) at 0x00483969.
typedef int(__fastcall* MeasureText_t)(void*, void*, const char*, int);
MeasureText_t MeasureTextWidth_orig;
int __fastcall MeasureTextWidth_re(void* self, void*, const char* str, int maxlen) {
    void* font = *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 4);
    if (!font) return 0;
    if (!str) return 0;
    int len = static_cast<int>(strlen(str));
    if (maxlen < len) len = maxlen;
    typedef int(__fastcall* Measure_t)(void*, void*, const char*, int);
    Measure_t m = reinterpret_cast<Measure_t>(vslot(font, 0x10));
    return m(font, 0, str, len);
}

// 0x004789f0  Widget_applyPalette(this, param_1): 7 when the object this+4 is null (0x004789f4); 3 when param_1 is
// null (0x004789fe); else calls the object's virtual method at vtable+0xec (slot 59) thiscall with *(param_1+4)
// (0x00478a0c) and returns 0.
typedef int(__fastcall* ApplyPalette_t)(void*, void*, void*);
ApplyPalette_t Widget_applyPalette_orig;
int __fastcall Widget_applyPalette_re(void* self, void*, void* param_1) {
    void* obj = *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 4);
    if (!obj) return 7;
    if (!param_1) return 3;
    typedef void(__fastcall* V_t)(void*, void*, int);
    V_t v = reinterpret_cast<V_t>(vslot(obj, 0xec));
    v(obj, 0, *reinterpret_cast<int*>(reinterpret_cast<char*>(param_1) + 4));
    return 0;
}

// 0x00475b00  Widget_fillRectR(this, param_1): 0x18 when the surface object this+4 is null (0x00475b05); else returns
// the surface's virtual method at vtable+0x34 (slot 13) called thiscall with param_1 (0x00475b0d).
typedef int(__fastcall* FillRectR_t)(void*, void*, int);
FillRectR_t Widget_fillRectR_orig;
int __fastcall Widget_fillRectR_re(void* self, void*, int param_1) {
    void* obj = *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 4);
    if (!obj) return 0x18;
    typedef int(__fastcall* V_t)(void*, void*, int);
    V_t v = reinterpret_cast<V_t>(vslot(obj, 0x34));
    return v(obj, 0, param_1);
}

// 0x00478b50  Widget_fillArea(this, param_1, param_2): 7 when the surface object this+4 is null (0x00478b55); else
// returns the surface's virtual method at vtable+0x44 (slot 17) called thiscall with (param_1, param_2) at
// 0x00478b6b.
typedef int(__fastcall* FillArea_t)(void*, void*, int, int);
FillArea_t Widget_fillArea_orig;
int __fastcall Widget_fillArea_re(void* self, void*, int param_1, int param_2) {
    void* obj = *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 4);
    if (!obj) return 7;
    typedef int(__fastcall* V_t)(void*, void*, int, int);
    V_t v = reinterpret_cast<V_t>(vslot(obj, 0x44));
    return v(obj, 0, param_1, param_2);
}

}  // namespace

// 0x0042ee80  clearCost
SG_HOOK("golf_clean.exe", 0x0042ee80, clearCost, ClearCost_re, ClearCost_orig);
// 0x004074a0  landmarkName
SG_HOOK("golf_clean.exe", 0x004074a0, landmarkName, LandmarkName_re, LandmarkName_orig);
// 0x004925d0  stripNewline
SG_HOOK("golf_clean.exe", 0x004925d0, stripNewline, StripNewline_re, StripNewline_orig);
// 0x004925b0  trimSpaces
SG_HOOK("golf_clean.exe", 0x004925b0, trimSpaces, TrimSpaces_re, TrimSpaces_orig);
// 0x00483930  measureTextWidth
SG_HOOK("golf_clean.exe", 0x00483930, measureTextWidth, MeasureTextWidth_re, MeasureTextWidth_orig);
// 0x004789f0  Widget_applyPalette
SG_HOOK("golf_clean.exe", 0x004789f0, Widget_applyPalette, Widget_applyPalette_re, Widget_applyPalette_orig);
// 0x00475b00  Widget_fillRectR
SG_HOOK("golf_clean.exe", 0x00475b00, Widget_fillRectR, Widget_fillRectR_re, Widget_fillRectR_orig);
// 0x00478b50  Widget_fillArea
SG_HOOK("golf_clean.exe", 0x00478b50, Widget_fillArea, Widget_fillArea_re, Widget_fillArea_orig);
