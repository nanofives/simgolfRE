// C3 batch c3z (2026-10-07) of golf_clean.exe: ui functions that have a static caller but are not reached by the
// test scenarios. Three panel hit-tests that map a screen point to an index (hitEmployeeSlot435570, pickAmenity434980,
// hitEmployee436b00), the course-upgrade message builder appendUpgradeText, the hotspot-list writer HotList::add, and
// the greedy word-wrap helper wrapTextToWidth. Hand-written from the 100%-matched sources (re/match/golf_hand_08_c.cpp,
// golf_hand_09_g.cpp, golf_raw_13.cpp, golf_hand_r3.cpp, golf_hand_r0.cpp) and, for wrapTextToWidth (no matched source),
// from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x00483980 --list) and the C2 transcription.
// Each body cites the address of every global, offset and callee it uses. __thiscall is emulated with __fastcall
// (ecx = this, edx unused). Callees are reached through their original addresses.
#include "hooks.h"
#include <string.h>

namespace {

template <typename T> T at(unsigned a) { return *reinterpret_cast<const T*>(a); }

// clamp(v, lo, hi) 0x00467130 and approxDistance(dx, dy) 0x00467170 (both __cdecl int leaves, already at C3).
typedef int(__cdecl* Clamp_t)(int, int, int);
typedef int(__cdecl* Dist_t)(int, int);
const Clamp_t kClamp = reinterpret_cast<Clamp_t>(0x00467130);
const Dist_t kDist = reinterpret_cast<Dist_t>(0x00467170);

typedef int(__cdecl* Hit2_t)(int, int);

// 0x00435570  hitEmployeeSlot435570(x, y): index of the employee-view slot under the point (match golf_hand_08_c.cpp).
// When x >= 0x136 and y >= 0x1f2 the point is in the 4x4 slot grid: column clamp((x-0x136)/121,0,3), row
// clamp((y-0x1f2)/21,0,3), returning row + column*4 (jl at 0x00435584/0x0043558c gate the grid). Five round buttons
// override with negative ids when approxDistance to their centre is < 20 (0x14): -2 at (0x11d,0x1ec), -3 at
// (0x100,0x1fe), -4 at (0xe8,0x21a), -5 at (0x14c, y scaled *3 about 0x250), -6 at (0x304, same). Otherwise -1.
Hit2_t HitSlot_orig;
int __cdecl HitSlot_re(int x, int y) {
    int r = -1;
    if (x >= 0x136 && y >= 0x1f2) {                 // 0x00435584, 0x0043558c
        r = kClamp((y - 0x1f2) / 21, 0, 3);         // 21 = 0x15
        r += kClamp((x - 0x136) / 121, 0, 3) * 4;   // 121 = 0x79
    }
    if (kDist(x - 0x11d, y - 0x1ec) < 20) r = -2;        // 0x004355f0
    if (kDist(x - 0x100, y - 0x1fe) < 20) r = -3;        // 0x00435610
    if (kDist(x - 0xe8, y - 0x21a) < 20) r = -4;         // 0x00435630
    if (kDist(x - 0x14c, (y - 0x250) * 3) < 20) r = -5;  // 0x00435651
    if (kDist(x - 0x304, (y - 0x250) * 3) < 20) r = -6;  // 0x00435670
    return r;
}

// 0x00434980  pickAmenity434980(x, y): index of the amenities/landmarks entry under the point (match
// golf_hand_09_g.cpp). Ten round hot-spots: approxDistance to each centre is tested against a radius and, when inside,
// sets the result; later tests override earlier ones. -1 when none match.
Hit2_t PickAmenity_orig;
int __cdecl PickAmenity_re(int x, int y) {
    int r = -1;
    if (kDist(x - 0x10d, y - 500) < 16) r = -2;    // 500 = 0x1f4, radius 16 = 0x10
    if (kDist(x - 0x12e, y - 0x233) < 20) r = 0;
    if (kDist(x - 0x14d, y - 0x20e) < 20) r = 1;
    if (kDist(x - 0x16b, y - 0x233) < 20) r = 2;
    if (kDist(x - 0x1d6, y - 0x23f) < 18) r = 3;   // radius 18 = 0x12
    if (kDist(x - 0x24f, y - 0x23f) < 18) r = 4;
    if (kDist(x - 0x28c, y - 0x23f) < 18) r = 5;
    if (kDist(x - 0x2ca, y - 0x23f) < 18) r = 6;
    if (kDist(x - 0xf8, y - 0x237) < 12) r = 7;    // radius 12 = 0xc
    if (kDist(x - 0x213, y - 0x244) < 12) r = 9;
    return r;
}

// 0x00436b00  hitEmployee436b00(x, y): control under the point in the employee detail panel (match golf_raw_13.cpp).
// Two fixed buttons first: local id -2 at (0x11e,0x1ec) and -3 at (0x100,0x1fe) when approxDistance < 0x10. Then it
// walks the 16-entry (short x, short y) table at 0x004c7af0 (4 bytes/entry, terminated by x == -1): for each entry it
// returns the entry index when the point is inside. The two special entries whose address is 0x004c7af8 / 0x004c7afc
// (indices 2, 3) use a wider test (dx scaled *4, offset 0x20/0x1e, radius 0x1e); every other entry uses offset
// 0x10/0x20 by whether the index is < 8, radius 0x10. If no entry matches, the fixed-button result is returned (with a
// 1 folded to 0, a case the fixed buttons can never produce since they only set -2/-3).
Hit2_t HitEmp_orig;
int __cdecl HitEmp_re(int x, int y) {
    int local = -1;
    if (kDist(x - 0x11e, y - 0x1ec) < 0x10) local = -2;  // 0x00436b23
    if (kDist(x - 0x100, y - 0x1fe) < 0x10) local = -3;  // 0x00436b46
    int idx = 0;
    const short* p = reinterpret_cast<const short*>(0x004c7af0);
    do {
        if (p == reinterpret_cast<const short*>(0x004c7af8) ||
            p == reinterpret_cast<const short*>(0x004c7afc)) {
            if (kDist((x - p[0]) * 4 - 0x20, (y - p[1]) - 0x1e) < 0x1e) return idx;  // 0x00436b96
        } else {
            if (kDist((x - (idx < 8 ? 0x10 : 0x20)) - p[0], (y - p[1]) - 0x10) < 0x10) return idx;  // 0x00436bc0
        }
        p += 2;
        idx++;
    } while (p[0] != -1);                             // terminator x == -1, 0x00436bd1
    if (local == 1) local = 0;                        // 0x00436be6 (unreachable: local is only -1/-2/-3)
    return local;
}

// 0x0040e5f0  appendUpgradeText(i): appends the course-upgrade message to the shared text buffer at 0x0051a068
// (match golf_hand_r3.cpp). strcat of the literals at 0x004c54cc / 0x004c54c4 / 0x004c5450 around the two class names
// read from the string-pointer table at 0x004c2a18 (entries i and i+1), plus a trailing clause: the string at
// 0x004c53ec when i == 0, at 0x004c53a4 when i == 1 (jne at 0x0040e6d6 / 0x0040e6e2). Leaf (the strcat / strlen are
// inlined as string ops in the original).
typedef void(__cdecl* Append_t)(int);
Append_t Append_orig;
void __cdecl Append_re(int i) {
    char* buf = reinterpret_cast<char*>(0x0051a068);
    const char* const* tbl = reinterpret_cast<const char* const*>(0x004c2a18);
    strcat(buf, reinterpret_cast<const char*>(0x004c54cc));
    strcat(buf, tbl[i]);
    strcat(buf, reinterpret_cast<const char*>(0x004c54c4));
    strcat(buf, tbl[i + 1]);
    strcat(buf, reinterpret_cast<const char*>(0x004c5450));
    if (i == 0) strcat(buf, reinterpret_cast<const char*>(0x004c53ec));
    else if (i == 1) strcat(buf, reinterpret_cast<const char*>(0x004c53a4));
}

// 0x004929b0  HotList::add(a, b, x, y, w, h, s): appends a hotspot to the list (match golf_hand_r0.cpp). Layout:
// array base m_50 at this+0x50, capacity m_54 at this+0x54, count m_58 at this+0x58; each entry is 0x20 bytes
// {m_0; rect l,t,r,b at +4/+8/+0xc/+0x10; m_14 at +0x14; m_18 at +0x18; char* text at +0x1c}. Grows (grow 0x00492690)
// when count == capacity, takes slot n = count++, clears the slot (init/freeEntryTip 0x00492660 frees any old text),
// stores the rect (l=x, t=y, r=x+w, b=y+h), m_18 = a, m_14 = b, and when s != 0 duplicates it into a malloc'd buffer
// (returns 4 on allocation failure). Returns the slot index n. Callees run through their original addresses.
struct HotEntry { int m_0; int l, t, r, b; int m_14; int m_18; char* text; };  // 0x20 bytes
typedef int(__fastcall* HotAdd_t)(void*, void*, int, int, int, int, int, int, const char*);
typedef void(__fastcall* FreeTip_t)(void*, void*);
typedef void(__fastcall* Grow_t)(void*, void*);
const FreeTip_t kFreeTip = reinterpret_cast<FreeTip_t>(0x00492660);
const Grow_t kGrow = reinterpret_cast<Grow_t>(0x00492690);
HotAdd_t HotAdd_orig;
int __fastcall HotAdd_re(void* self, void*, int a, int b, int x, int y, int w, int h, const char* s) {
    int* cap = reinterpret_cast<int*>(reinterpret_cast<char*>(self) + 0x54);
    int* cnt = reinterpret_cast<int*>(reinterpret_cast<char*>(self) + 0x58);
    if (*cnt >= *cap) kGrow(self, 0);                                    // 0x004929be/0x004929c2
    int n = (*cnt)++;                                                    // 0x004929cf
    HotEntry* e = *reinterpret_cast<HotEntry**>(reinterpret_cast<char*>(self) + 0x50) + n;  // re-read m_50 after grow
    kFreeTip(e, 0);                                                      // 0x004929da init/freeEntryTip
    e->l = x; e->t = y; e->r = x + w; e->b = y + h;
    e->m_18 = a;
    e->m_14 = b;
    if (s) {                                                            // 0x00492a23
        e->text = reinterpret_cast<char*>(malloc(strlen(s) + 1));        // 0x00492a2f
        if (!e->text) return 4;
        e->text[0] = 0;
        strcat(e->text, s);
    }
    return n;
}

// 0x00483980  wrapTextToWidth(this, s, budget, remain): greedy word wrap. `this` is a text layout whose font sits at
// this+4 (read by measureTextWidth 0x00483930). *budget is the remaining pixel width; remain is the remaining byte
// count of the string. It scans words delimited by space 0x20 (memchr), measuring each with measureTextWidth and
// subtracting from the budget, and accumulates consumed width into the global at 0x00839aa8. It returns 0 when the run
// fits (writing the leftover budget back and advancing the global) or the break pointer (start of the word that did
// not fit) otherwise; a first word wider than the whole budget sets *budget = 0 and returns the next char when the
// global was 0 at that point. Callees measureTextWidth and memchr run through their original addresses.
typedef char*(__fastcall* Wrap_t)(void*, void*, char*, int*, char*);
typedef int(__fastcall* Measure_t)(void*, void*, char*, int);  // measureTextWidth thiscall(this, str, maxlen)
const Measure_t kMeasure = reinterpret_cast<Measure_t>(0x00483930);
Wrap_t Wrap_orig;
char* __fastcall Wrap_re(void* self, void*, char* s, int* budget, char* remain) {
    int* acc = reinterpret_cast<int*>(0x00839aa8);
    int left = *budget;
    char* word = s;
    while (true) {
        if (remain == 0) {                                   // 0x004839b3
            *acc += *budget - left;
            *budget = left;
            return 0;
        }
        char* sp = reinterpret_cast<char*>(memchr(word + 1, 0x20, reinterpret_cast<size_t>(remain)));  // 0x004839a7
        if (sp == 0) break;                                  // 0x004839cc
        int wd = kMeasure(self, 0, word, static_cast<int>(sp - word));  // 0x004839bf
        int prevAcc = *acc;
        left -= wd;
        if (word == s) {                                     // 0x004839ee
            if (left < 0) {                                  // 0x004839f?
                *budget = 0;
                if (prevAcc == 0) return sp + 1;
                return word;
            }
        } else if (left < 0) {                               // 0x00483a27 -> LAB_00483a69
            *budget = 0;
            return word + 1;
        }
        remain = remain + (word - sp);
        word = sp;
    }
    int wd = kMeasure(self, 0, word, static_cast<int>(reinterpret_cast<size_t>(remain)));  // 0x00483a1a
    int prevAcc = *acc;
    left -= wd;
    if (word == s) {                                         // 0x00483a3b
        if (left >= 0) {
            *acc += *budget - left;
            *budget = left;
            return 0;
        }
        *budget = 0;
        if (prevAcc == 0) return 0;
        return word;
    }
    if (left >= 0) {                                         // 0x00483a67
        *acc += *budget - left;
        *budget = left;
        return 0;
    }
    *budget = 0;                                             // LAB_00483a69
    return word + 1;
}

}  // namespace

// 0x00435570  hitEmployeeSlot435570
SG_HOOK("golf_clean.exe", 0x00435570, hitEmployeeSlot435570, HitSlot_re, HitSlot_orig);
// 0x00434980  pickAmenity434980
SG_HOOK("golf_clean.exe", 0x00434980, pickAmenity434980, PickAmenity_re, PickAmenity_orig);
// 0x00436b00  hitEmployee436b00
SG_HOOK("golf_clean.exe", 0x00436b00, hitEmployee436b00, HitEmp_re, HitEmp_orig);
// 0x0040e5f0  appendUpgradeText
SG_HOOK("golf_clean.exe", 0x0040e5f0, appendUpgradeText, Append_re, Append_orig);
// 0x004929b0  HotList::add
SG_HOOK("golf_clean.exe", 0x004929b0, HotList__add, HotAdd_re, HotAdd_orig);
// 0x00483980  wrapTextToWidth
SG_HOOK("golf_clean.exe", 0x00483980, wrapTextToWidth, Wrap_re, Wrap_orig);
