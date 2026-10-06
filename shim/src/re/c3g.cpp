// UI leaves of golf_clean.exe: text builders (number/cents/date/hole/course title), a window-corner size
// computation, two widget-table copies, the message-ticker start, and the golfer-panel hit test. Hand-written
// from the C2 transcriptions re/analysis/ui/<addr>_<name>.md and the disassembly (re/tools/asm2inline.py); each
// body cites the address every field offset and constant comes from. __thiscall is emulated with __fastcall
// (ecx = this, edx unused). Callees (itoa, appendString, bucketValue, Random::range, approxDistance) and the
// self-recursion of appendNumber are reached through their original addresses, so a hooked callee runs its own
// reimplementation during the A/B (as shim/src/re/golf_course.cpp does).
#include <string.h>

#include "hooks.h"

namespace {

template <typename T> T& at(unsigned addr) { return *reinterpret_cast<T*>(addr); }
template <typename T> T& field(void* obj, unsigned off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(obj) + off);
}

char* const kText = reinterpret_cast<char*>(0x0051a068);    // shared text buffer (0x0051a068)
char* const kNumBuf = reinterpret_cast<char*>(0x0058a528);  // itoa scratch buffer (0x0058a528)

// __cdecl callees reached through their original addresses.
typedef char*(__cdecl* Itoa_t)(int value, char* buf, int radix);    // 0x004ad425 __itoa
typedef int(__cdecl* Int1_t)(int);                                  // appendString 0x0045b9f0 / bucketValue 0x0044faf0
typedef void(__cdecl* AppendNum_t)(int);                            // appendNumber 0x0042dc00 (self)
typedef unsigned short(__fastcall* Range_t)(void* self, void* edx, int n);  // Random::range 0x0045c1e0 (thiscall)
typedef int(__cdecl* Dist_t)(int dx, int dy);                       // approxDistance 0x00467170

const Itoa_t kItoa = reinterpret_cast<Itoa_t>(0x004ad425);
const Int1_t kAppendString = reinterpret_cast<Int1_t>(0x0045b9f0);
const Int1_t kBucketValue = reinterpret_cast<Int1_t>(0x0044faf0);
const AppendNum_t kAppendNumber = reinterpret_cast<AppendNum_t>(0x0042dc00);
const Range_t kRange = reinterpret_cast<Range_t>(0x0045c1e0);
const Dist_t kApproxDist = reinterpret_cast<Dist_t>(0x00467170);

// 0x0042dc00  appendNumber(n): appends n to the text buffer with thousands separators. A negative n writes '-'
// then negates (0x0042dc0e..0x0042dc24); n >= 1000 recurses on n/1000 through 0x0042dc00 (call at 0x0042dc54) and
// appends ',' (0x0042dc5c). r = n % 1000 (0x0042dc3a); when r < 100 and n != 0 it zero-pads: '0' when n >= 1000
// (0x0042dc80) and another '0' when r < 10 and n >= 100 (0x0042dc98). Finally appends itoa(r, kNumBuf, 10) via
// __itoa 0x004ad425 (call at 0x0042dd0e). __cdecl.
AppendNum_t AppendNumber_orig;
void __cdecl AppendNumber_re(int n) {
    if (n < 0) {
        strcat(kText, "-");
        n = -n;
    }
    if (n >= 1000) {
        kAppendNumber(n / 1000);
        strcat(kText, ",");
    }
    const int r = n % 1000;
    if (r < 100 && n != 0) {
        if (n >= 1000) strcat(kText, "0");
        if (r < 10 && n >= 100) strcat(kText, "0");
    }
    strcat(kText, kItoa(r, kNumBuf, 10));
}

// 0x0042dd50  appendCents(n): stores n in the dword at 0x00569628 (0x0042dd56), then appends n as "units.cc".
// A negative n (0x0042dd5f): if the text buffer's last char is '+' it is overwritten with NUL (0x0042dd6c..
// 0x0042dd79), then '-' is appended and n negated. Appends itoa(n/100, kNumBuf, 10), '.', a '0' when n%100 < 10
// (0x0042ddcc), then itoa(n%100, kNumBuf, 10) (itoa via 0x004ad425). __cdecl.
Int1_t AppendCents_orig;
void __cdecl AppendCents_re(int n) {
    at<int>(0x00569628) = n;
    if (n < 0) {
        const int len = static_cast<int>(strlen(kText));
        if (kText[len - 1] == '+') kText[len - 1] = 0;
        strcat(kText, "-");
        n = -n;
    }
    strcat(kText, kItoa(n / 100, kNumBuf, 10));
    strcat(kText, ".");
    if (n % 100 < 10) strcat(kText, "0");
    strcat(kText, kItoa(n % 100, kNumBuf, 10));
}

// 0x0040d7b0  appendDate(date): year = date / 8192, month = date / 1024 % 8 (0x0040d7b6..0x0040d7e0); stores
// (short)year in the word at 0x005a6d3c (0x0040d7ec); appends the month name from the pointer table at 0x004c2908
// (0x0040d7f5), a space, then itoa(year + 2001, kNumBuf, 10) (itoa via 0x004ad425). __cdecl.
Int1_t AppendDate_orig;
void __cdecl AppendDate_re(int date) {
    const int year = date / 8192;
    const int month = date / 1024 % 8;
    at<short>(0x005a6d3c) = static_cast<short>(year);
    strcat(kText, at<const char*>(0x004c2908 + month * 4));
    strcat(kText, " ");
    strcat(kText, kItoa(year + 2001, kNumBuf, 10));
}

// 0x00407280  appendHoleName(hole): rec = hole * 0x208. When the flags byte at 0x00575cb0 + rec has bit 0x81
// (test at 0x00407291) it calls appendString(hole) 0x0045b9f0 (call at 0x0040729b) and returns if that is
// non-zero (0x004072a5); else it appends a name by par (the byte at 0x00575ab0 + rec, 0x004072ab): par 3 ->
// 0x004c2e88[hole], par 4 -> 0x004c2e38[hole], else -> 0x004c2ed8[hole]. When bit 0x81 is clear it appends
// "Hole " (0x004c4978) then itoa(hole, kNumBuf, 10). __cdecl.
Int1_t AppendHoleName_orig;
void __cdecl AppendHoleName_re(int hole) {
    const unsigned rec = hole * 0x208;
    if (at<unsigned char>(0x00575cb0 + rec) & 0x81) {
        if (kAppendString(hole)) return;
        const signed char par = at<signed char>(0x00575ab0 + rec);
        unsigned table = par == 3 ? 0x004c2e88 : par == 4 ? 0x004c2e38 : 0x004c2ed8;
        strcat(kText, at<const char*>(table + hole * 4));
    } else {
        strcat(kText, reinterpret_cast<const char*>(0x004c4978));
        strcat(kText, kItoa(hole, kNumBuf, 10));
    }
}

// 0x0040daa0  appendCourseTitle(full): when appendString(0) 0x0045b9f0 returns 0 (call at 0x0040daa4) it appends
// the site name string at 0x004c1ea9 + site * 0x82, where site = the byte at 0x00571ff4 + course * 0x2e and
// course = the dword at 0x0059bf90 (0x0040dab0..0x0040dad8). When full != -1 (0x0040daff) it appends a suffix
// chosen by bucketValue(holes - 1) 0x0044faf0, holes = the dword at 0x005685f0 (0x0040db08..0x0040db17): case 1 ->
// 0x004c5364 (full == 1) / 0x004c5360; case 2 -> 0x004c5374 / 0x004c5370; case 3 -> 0x004c5384 / 0x004e9a84;
// default -> 0x004c5398 / 0x004c5394. __cdecl.
Int1_t AppendCourseTitle_orig;
void __cdecl AppendCourseTitle_re(int full) {
    if (!kAppendString(0)) {
        const int course = at<int>(0x0059bf90);
        const signed char site = at<signed char>(0x00571ff4 + course * 0x2e);
        strcat(kText, reinterpret_cast<const char*>(0x004c1ea9 + site * 0x82));
    }
    if (full != -1) {
        unsigned s;
        switch (kBucketValue(at<int>(0x005685f0) - 1)) {
            case 1: s = full == 1 ? 0x004c5364 : 0x004c5360; break;
            case 2: s = full == 1 ? 0x004c5374 : 0x004c5370; break;
            case 3: s = full == 1 ? 0x004c5384 : 0x004e9a84; break;
            default: s = full == 1 ? 0x004c5398 : 0x004c5394; break;
        }
        strcat(kText, reinterpret_cast<const char*>(s));
    }
}

// 0x00481760  Window::calcSizeFromCorners(): fills the two size words at this+0x1a4 / this+0x1a8 from four corner
// objects. When the flags byte at +0x9c has bit 0x10 (test at 0x00481769) it uses the corners at +0x52c, +0x534,
// +0x530, +0x538, else (0x004817f9) the corners at +0x54c, +0x550, +0x530, +0x538; nothing is written if any of
// the four is null (0x0048177a..0x004817a4). Each corner holds a width at +0x18 and a height at +0x1c. The width
// this+0x1a4 = max(c0->w + c1->w, c3->w + c2->w) and the height this+0x1a8 = max(c0->h + c2->h, c3->h + c1->h),
// with (c0, c1, c2, c3) = (+0x52c, +0x534, +0x530, +0x538) or (+0x54c, +0x550, +0x530, +0x538) (0x004817aa..
// 0x00481865). __thiscall (ret).
typedef void(__fastcall* Calc_t)(void* self, void* edx);
Calc_t CalcSize_orig;
void __fastcall CalcSize_re(void* self, void*) {
    void *c0, *c1, *c2, *c3;
    if (field<unsigned char>(self, 0x9c) & 0x10) {
        c0 = field<void*>(self, 0x52c); c1 = field<void*>(self, 0x534);
        c2 = field<void*>(self, 0x530); c3 = field<void*>(self, 0x538);
    } else {
        c0 = field<void*>(self, 0x54c); c1 = field<void*>(self, 0x550);
        c2 = field<void*>(self, 0x530); c3 = field<void*>(self, 0x538);
    }
    if (!c0 || !c1 || !c2 || !c3) return;
    int w = field<int>(c0, 0x18) + field<int>(c1, 0x18);
    const int w2 = field<int>(c3, 0x18) + field<int>(c2, 0x18);
    if (w < w2) w = w2;
    field<int>(self, 0x1a4) = w;
    int h = field<int>(c0, 0x1c) + field<int>(c2, 0x1c);
    const int h2 = field<int>(c3, 0x1c) + field<int>(c1, 0x1c);
    if (h < h2) h = h2;
    field<int>(self, 0x1a8) = h;
}

// 0x00495eb0  resetWidgetTable(dst): copies the 38-int template at 0x0083fe78 into dst, reordered: the source
// words 0x0083fe78, fe7c, fe80, fe84 go to dst+4, dst+0xc, dst+0x10, dst+8 (0x00495eb0..0x00495ec8), then the
// remaining source words 0x0083fe88..0x0083ff0c map to dst+0x14..dst+0x98 in order. __fastcall (ecx = dst).
typedef void(__fastcall* Reset_t)(void* dst, void* edx);
Reset_t ResetWidgetTable_orig;
void __fastcall ResetWidgetTable_re(void* dst, void*) {
    const int* src = reinterpret_cast<const int*>(0x0083fe78);
    field<int>(dst, 0x04) = src[0];
    field<int>(dst, 0x0c) = src[1];
    field<int>(dst, 0x10) = src[2];
    field<int>(dst, 0x08) = src[3];
    for (int i = 4; i < 38; i++) field<int>(dst, 0x14 + (i - 4) * 4) = src[i];
}

// 0x00495d30  initWidgetTable(dst): constructor. Writes the vtable pointer 0x004baa14 at dst+0 (0x00495d32), then
// copies the 38-int template at 0x0083fe78 into dst with the same reorder as resetWidgetTable: src[0..3] ->
// dst+4, dst+0xc, dst+0x10, dst+8, and src[4..37] -> dst+0x14..dst+0x98 (0x00495d38 onward). __thiscall; returns
// this (not compared).
typedef void*(__fastcall* Init_t)(void* self, void* edx);
Init_t InitWidgetTable_orig;
void* __fastcall InitWidgetTable_re(void* self, void*) {
    field<unsigned>(self, 0x00) = 0x004baa14;
    const int* src = reinterpret_cast<const int*>(0x0083fe78);
    field<int>(self, 0x04) = src[0];
    field<int>(self, 0x0c) = src[1];
    field<int>(self, 0x10) = src[2];
    field<int>(self, 0x08) = src[3];
    for (int i = 4; i < 38; i++) field<int>(self, 0x14 + (i - 4) * 4) = src[i];
    return self;
}

// 0x0040cb00  startMessage(arg, prio, sound): starts the message ticker with the text buffer. Refused (returns 0)
// when (the byte at 0x00569498 or the dword at 0x0053df54) is set and prio <= 0, or when the dword at 0x00567afc
// is 3 (0x0040cb09..0x0040cb34). Otherwise clears 0x0053df54, copies kText into the message buffer at 0x005a6d40
// (0x0040cb45), sets the direction byte 0x00569498 to 1, stores arg in 0x005a34ec, the display length
// strlen(kText) / ((0x00822c88 != 0) + 3) + 16 in 0x005a7144, sound in 0x004c2e08 (0x0040cb53..0x0040cb90); a
// negative prio sets the delay 0x005694a4 to -prio (0x0040cb95); then 0x0056d1a8 = Random::range(600) and
// 0x0056d1ac = Random::range(200) + 200 (0x0045c1e0 at 0x0040cbaa/0x0040cbc1) and returns 1. __cdecl.
typedef int(__cdecl* Start_t)(int, int, int);
Start_t StartMessage_orig;
int __cdecl StartMessage_re(int arg, int prio, int sound) {
    if (((at<unsigned char>(0x00569498) || at<int>(0x0053df54)) && prio <= 0) || at<int>(0x00567afc) == 3)
        return 0;
    at<int>(0x0053df54) = 0;
    strcpy(reinterpret_cast<char*>(0x005a6d40), kText);
    at<unsigned char>(0x00569498) = 1;
    at<int>(0x005a34ec) = arg;
    at<int>(0x005a7144) = static_cast<int>(strlen(kText)) / ((at<int>(0x00822c88) != 0) + 3) + 16;
    at<int>(0x004c2e08) = sound;
    if (prio < 0) at<int>(0x005694a4) = -prio;
    // Random::range is __thiscall with this = the RNG object g_rng at 0x00822d9c (ecx); it reads the seed at [this].
    void* const rng = reinterpret_cast<void*>(0x00822d9c);
    at<int>(0x0056d1a8) = kRange(rng, 0, 600);
    at<int>(0x0056d1ac) = kRange(rng, 0, 200) + 200;
    return 1;
}

// 0x00435f00  hitGolferPanel435f00(x, y): returns the index of the golfer-info-panel hotspot under (x, y), or -1.
// Each hotspot is approxDistance(x - hx, y - hy) 0x00467170 against a radius (0x00435f0c onward): hotspot 0 at
// (0x111, 0x22f) r 20, 1 at (0x139, 0x207) r 15, 2 at (0x139, 0x224) r 15, 3 at (0x139, 0x244) r 15; when the
// dword at 0x00567afc is 3 (0x00435fa2) also 4..8 at (0x19e/0x1ed/0x23c/0x28b/0x2da, 0x1f9) r 20; then 9 at
// (0x11e, 0x1ea) r 15 and 10 at (0xe7, 0x21c) r 15. Later matches win (the result is overwritten). __cdecl.
typedef int(__cdecl* Hit_t)(int, int);
Hit_t HitGolferPanel2_orig;
int __cdecl HitGolferPanel2_re(int x, int y) {
    int r = -1;
    if (kApproxDist(x - 0x111, y - 0x22f) < 20) r = 0;
    if (kApproxDist(x - 0x139, y - 0x207) < 15) r = 1;
    if (kApproxDist(x - 0x139, y - 0x224) < 15) r = 2;
    if (kApproxDist(x - 0x139, y - 0x244) < 15) r = 3;
    if (at<int>(0x00567afc) == 3) {
        if (kApproxDist(x - 0x19e, y - 0x1f9) < 20) r = 4;
        if (kApproxDist(x - 0x1ed, y - 0x1f9) < 20) r = 5;
        if (kApproxDist(x - 0x23c, y - 0x1f9) < 20) r = 6;
        if (kApproxDist(x - 0x28b, y - 0x1f9) < 20) r = 7;
        if (kApproxDist(x - 0x2da, y - 0x1f9) < 20) r = 8;
    }
    if (kApproxDist(x - 0x11e, y - 0x1ea) < 15) r = 9;
    if (kApproxDist(x - 0xe7, y - 0x21c) < 15) r = 10;
    return r;
}
}  // namespace

SG_HOOK("golf_clean.exe", 0x0042dc00, appendNumber, AppendNumber_re, AppendNumber_orig);
SG_HOOK("golf_clean.exe", 0x0042dd50, appendCents, AppendCents_re, AppendCents_orig);
SG_HOOK("golf_clean.exe", 0x0040d7b0, appendDate, AppendDate_re, AppendDate_orig);
SG_HOOK("golf_clean.exe", 0x00407280, appendHoleName, AppendHoleName_re, AppendHoleName_orig);
SG_HOOK("golf_clean.exe", 0x0040daa0, appendCourseTitle, AppendCourseTitle_re, AppendCourseTitle_orig);
SG_HOOK("golf_clean.exe", 0x00481760, Window_calcSizeFromCorners, CalcSize_re, CalcSize_orig);
SG_HOOK("golf_clean.exe", 0x00495eb0, resetWidgetTable, ResetWidgetTable_re, ResetWidgetTable_orig);
SG_HOOK("golf_clean.exe", 0x00495d30, initWidgetTable, InitWidgetTable_re, InitWidgetTable_orig);
SG_HOOK("golf_clean.exe", 0x0040cb00, startMessage, StartMessage_re, StartMessage_orig);
SG_HOOK("golf_clean.exe", 0x00435f00, hitGolferPanel435f00, HitGolferPanel2_re, HitGolferPanel2_orig);
