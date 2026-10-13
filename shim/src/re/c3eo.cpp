// C3 batch c3eo (round 31, 2026-10-13) of golf_clean.exe: the map, roster and statistics screens, plus the golfer-state
// reset one of them calls. Fixtures: re/frida/js/fixtures.d/c3eo.js. Registry: re/frida/registry.d/c3eo.py. Purposes and
// per-jcc branch tables: log/c3/c3eo_purpose.md. Notes and drops: log/c3/c3eo_notes.txt.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list,
// listings in log/c3/alt/c3eo_decomp/) and cites the instructions it reproduces. __thiscall is emulated with __fastcall
// (ecx = this, edx unused). Callees run through their original addresses.
#include "hooks.h"

namespace {

template <typename T> static inline T& G(unsigned addr) { return *reinterpret_cast<T*>(addr); }

// virtual slot `off` of obj (thiscall, emulated with __fastcall)
template <typename F> static inline F vslot(void* obj, unsigned off) {
    return *reinterpret_cast<F*>(*reinterpret_cast<char**>(obj) + off);
}

typedef void(__cdecl* V0_t)();
typedef void(__cdecl* V1_t)(int);
typedef int(__cdecl* C0_t)();
typedef int(__cdecl* C1_t)(int);
typedef int(__cdecl* C2_t)(int, int);
typedef int(__cdecl* C3_t)(int, int, int);
typedef char*(__cdecl* Itoa_t)(int, char*, int);
// thiscall with N stack arguments
typedef int(__fastcall* Tn1_t)(void* ecx, void* edx, int);
typedef int(__fastcall* Tn3_t)(void* ecx, void* edx, int, int, int);
typedef int(__fastcall* Tn4_t)(void* ecx, void* edx, int, int, int, int);
typedef int(__fastcall* Tn5_t)(void* ecx, void* edx, int, int, int, int, int);
typedef int(__fastcall* Tn6_t)(void* ecx, void* edx, int, int, int, int, int, int);
typedef int(__fastcall* ViewToLocal_t)(void* ecx, void* edx, int* px, int* py);

// ---- callees reached by the bodies below (original addresses; a C3 hook at one of them runs through its detour) ----
const V1_t kClearObjectFootprint = reinterpret_cast<V1_t>(0x0040E400);
const C0_t kInitGolferState = reinterpret_cast<C0_t>(0x0046D040);

// ---- VC6's inline string primitives, as the `repne scasb` / `rep movsd` sequences of the screens below do it ----
static int SLen(const char* s) { int n = 0; while (s[n]) n++; return n; }
static void SCpy(char* d, const char* s) { int n = SLen(s); for (int i = 0; i <= n; i++) d[i] = s[i]; }
static void SCat(char* d, const char* s) { SCpy(d + SLen(d), s); }
static int SCmp(const char* a, const char* b) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(a);
    const unsigned char* q = reinterpret_cast<const unsigned char*>(b);
    while (*p != 0 && *p == *q) { p++; q++; }
    return *p < *q ? -1 : (*p > *q ? 1 : 0);
}

const Itoa_t kItoa = reinterpret_cast<Itoa_t>(0x004AD425);          // CRT _itoa(value, buffer, radix)
const C3_t kClamp = reinterpret_cast<C3_t>(0x00467130);             // clamp(value, lo, hi)
const Tn4_t kImageDraw = reinterpret_cast<Tn4_t>(0x00473E60);
const Tn5_t kStretchBlit = reinterpret_cast<Tn5_t>(0x00473F60);
const Tn6_t kDrawSprite = reinterpret_cast<Tn6_t>(0x00474440);
const Tn4_t kWidgetSetQuad = reinterpret_cast<Tn4_t>(0x00476310);
const Tn5_t kWidgetDraw = reinterpret_cast<Tn5_t>(0x00476650);
const Tn6_t kWidgetDraw2 = reinterpret_cast<Tn6_t>(0x004766A0);
const Tn5_t kDrawStringAt = reinterpret_cast<Tn5_t>(0x00476700);
const Tn5_t kFillWindowRect = reinterpret_cast<Tn5_t>(0x00480B00);
const Tn1_t kRefreshFull = reinterpret_cast<Tn1_t>(0x00480C80);
const ViewToLocal_t kViewToLocal = reinterpret_cast<ViewToLocal_t>(0x0047AB50);
const C2_t kWaitTicks = reinterpret_cast<C2_t>(0x0045BF80);
const C0_t kPumpMessages = reinterpret_cast<C0_t>(0x0045C030);
const C0_t kFlushRedraw = reinterpret_cast<C0_t>(0x0045AE70);
const C0_t kPumpLight = reinterpret_cast<C0_t>(0x0045C150);
const C0_t kEnterModal483d30 = reinterpret_cast<C0_t>(0x00483D30);

// The screens below compute their target surface as `0x00519a60 ? 0x00519cd4 : 0` (mov eax, 0x519a60; neg eax;
// sbb eax, eax; and eax, 0x519cd4): 0x00519a60 is a fixed data address, so the surface is always 0x00519cd4.
void* const kScreen = reinterpret_cast<void*>(0x00519CD4);
void* const kRootWindow = reinterpret_cast<void*>(0x00519A60);
char* const kText = reinterpret_cast<char*>(0x0051A068);            // the shared text buffer
char* const kItoaBuf = reinterpret_cast<char*>(0x00821C50);         // the shared itoa scratch

// the golfer-type table: 76 records of 0x2c bytes from 0x005849e0 (the count loop walks the word at +0x2a from
// 0x00584a0a to 0x0058571a, 0x00454c55..0x00454c77); the names are 0x230 bytes each from 0x004d6098
const unsigned kTypes = 0x005849E0, kTypeStride = 0x2C, kTypeCount = 76;
const unsigned kTypeNames = 0x004D6098, kTypeNameStride = 0x230;

// ---- showHoleStatsScreen's callees ----
typedef void(__cdecl* V4_t)(int, int, int, int);
const V1_t kAppendHoleName = reinterpret_cast<V1_t>(0x00407280);
const V1_t kAppendCents = reinterpret_cast<V1_t>(0x0042DD50);
const V1_t kAppendRatingLabel = reinterpret_cast<V1_t>(0x004532A0);
const V4_t kBuildGolferComment = reinterpret_cast<V4_t>(0x00469B00);
// WINMM.dll!timeGetTime through the IAT slot the original reads (`call dword ptr [0x4ba21c]`, 0x00453362)
typedef unsigned(__stdcall* TimeGetTime_t)();
static inline unsigned TimeGetTime() { return (*reinterpret_cast<TimeGetTime_t*>(0x004BA21C))(); }

// ---- showTournamentResults' callees ----
typedef void(__cdecl* V2_t)(int, int);
typedef void(__cdecl* V3_t)(int, int, int);
typedef void(__cdecl* V5_t)(int, int, int, int, int);
const Tn4_t kSetDrawTarget = reinterpret_cast<Tn4_t>(0x004762D0);
const V5_t kDrawFrameR3 = reinterpret_cast<V5_t>(0x0040CEF0);
const V4_t kDrawCentered = reinterpret_cast<V4_t>(0x00404B70);
const V4_t kDrawText4049d0 = reinterpret_cast<V4_t>(0x004049D0);
const V1_t kAppendCourseTitle = reinterpret_cast<V1_t>(0x0040DAA0);
const V2_t kBuildGolferName = reinterpret_cast<V2_t>(0x004676E0);
const V1_t kAppendNumber = reinterpret_cast<V1_t>(0x0042DC00);
const Tn4_t kDrawStringRightAt = reinterpret_cast<Tn4_t>(0x00478140);
const V2_t kLogTick = reinterpret_cast<V2_t>(0x0040C6F0);
const V3_t kPostEvent = reinterpret_cast<V3_t>(0x0046E7B0);
const V5_t kPlaySound = reinterpret_cast<V5_t>(0x004481B0);
const C0_t kResetGolferState = reinterpret_cast<C0_t>(0x0046D0C0);

}  // namespace

// ====================================================================================================================
// resetGolferState 0x0046d0c0 (__cdecl, no arguments)
// ====================================================================================================================
// Placed-object records: 256 of 0x10 bytes from 0x0058bcb8 (esi walks to 0x0058ccb8, 0x0046d0c5..0x0046d0ec); the word at
// +0 is the type. Course records: 0x208 bytes each from 0x00575ab0, the record index is the dword [0x005685f0] read once
// at 0x0046d0ee and multiplied by 0x208 at 0x0046d11d..0x0046d129 (<<6, +, <<3), so all 18 iterations of the loop at
// 0x0046d12c..0x0046d1dd work on the SAME record: the counter edi (0x12 at 0x0046d124, dec at 0x0046d1dc) is the only
// thing the loop changes. Inside the record: byte +0 (0x00575ab0), word +4 (0x00575ab4, sign-extended at 0x0046d147),
// byte +0x200 (0x00575cb0).
// 0x0046d0c0
int __cdecl ResetGolferState_re() {
    // 0x0046d0c3..0x0046d0ec: every placed object of type 0x11 or 0x12 is removed by index
    for (int i = 0; i < 256; i++) {
        unsigned short type = G<unsigned short>(0x0058BCB8 + 0x10 * i);          // 0x0046d0ca
        if (type == 0x11 || type == 0x12)                                        // 0x0046d0cd / 0x0046d0d3
            kClearObjectFootprint(i);                                            // 0x0046d0da
    }
    int rec = G<int>(0x005685F0);                                                // 0x0046d0ee
    // 0x0046d0f3..0x0046d102: 0x2df0 dwords from 0x0058f338 over 0x004d6088
    for (unsigned k = 0; k < 0x2DF0u; k++) G<unsigned>(0x004D6088 + 4 * k) = G<unsigned>(0x0058F338 + 4 * k);
    // 0x0046d104..0x0046d115: 0x668a dwords from 0x00520a28 over 0x00543d10
    for (unsigned k = 0; k < 0x668Au; k++) G<unsigned>(0x00543D10 + 4 * k) = G<unsigned>(0x00520A28 + 4 * k);
    int mult = G<int>(0x005A8C60);                                               // 0x0046d117 (esi)
    unsigned base = static_cast<unsigned>(rec) * 0x208u;                         // 0x0046d11d..0x0046d129
    for (int n = 0x12; n != 0; n--) {                                            // 0x0046d124, 0x0046d1dc
        if (G<unsigned char>(0x00575AB0 + base) == 0) continue;                  // 0x0046d12c / 0x0046d134
        G<unsigned char>(0x00575AB0 + base) = 3;                                 // 0x0046d13a
        unsigned char f = G<unsigned char>(0x00575CB0 + base);                   // 0x0046d141
        int v = G<short>(0x00575AB4 + base);                                     // 0x0046d147 (movsx)
        if ((f & 0x60) != 0 && v > 0xFA) v += 0x19;                              // 0x0046d14e / 0x0046d153 / 0x0046d15a
        if (v > 0x12C) v -= 25 * mult;                                           // 0x0046d15d / 0x0046d164..0x0046d16f
        // the same four thresholds are applied twice (0x0046d172..0x0046d1a1 and 0x0046d1a7..0x0046d1d6)
        for (int pass = 0; pass < 2; pass++) {
            if (v <= 0x32) G<unsigned char>(0x00575AB0 + base) = 2;              // 0x0046d175 / 0x0046d1aa
            if (v >= 0xFA) G<unsigned char>(0x00575AB0 + base) = 4;              // 0x0046d183 / 0x0046d1b8
            if (v >= 0x1DB) G<unsigned char>(0x00575AB0 + base) = 5;             // 0x0046d191 / 0x0046d1c6
            if (v > 0x271) G<unsigned char>(0x00575AB0 + base) = 6;              // 0x0046d19f / 0x0046d1d4 (bl = 6, 0x0046d113)
        }
    }
    G<int>(0x005A47E0) = 0;                                                      // 0x0046d1e3
    return kInitGolferState();                                                   // 0x0046d1ed: its eax is the function's
}
C0_t ResetGolferState_orig;
// 0x0046d0c0
SG_HOOK("golf_clean.exe", 0x0046d0c0, resetGolferState, ResetGolferState_re, ResetGolferState_orig);

// ====================================================================================================================
// showMembershipRoster 0x00454c50 (__cdecl, no arguments)
// ====================================================================================================================
// Lists the golfer types that have members, sorted by name, 22 rows at a time, and loops on the pumps until the player
// clicks. A type record is 0x2c bytes from 0x005849e0 (76 of them): byte +2 (0x005849e2) carries the exclude bit 0x80
// and a status in its low three bits, word +0x2a (0x00584a0a) the member count, byte +0x29 (0x00584a09) the 0xff that
// forces status -1, bytes +4..+0x15 (0x005849e3 + hole) the per-hole flags. Names are 0x230 bytes each from 0x004d6098.
// 0x00454c50
void __cdecl ShowMembershipRoster_re() {
    // 0x00454c53..0x00454c77: count the records that are not excluded and have a non-zero member count
    int count = 0;
    for (unsigned p = 0x00584A0A; p < 0x0058571A; p += kTypeStride)
        if ((G<unsigned char>(p - 0x28) & 0x80) == 0 && G<unsigned short>(p) != 0) count++;   // 0x00454c62 / 0x00454c68
    if (count == 0) count = 1;                                                   // 0x00454c7f / 0x00454c81
    int offset = 0;                                                              // 0x00454c5e

    void* disp = G<void*>(0x0083AD50);                                           // 0x00454c89
    if (disp != 0)                                                               // 0x00454c91
        vslot<Tn3_t>(disp, 0x9C)(disp, 0, 1, 1, 1);                              // 0x00454c9b
    // 0x00454caa..0x00454cc6
    kDrawSprite(reinterpret_cast<void*>(0x00821040), 0, reinterpret_cast<int>(kScreen), 0, 0, 0x3E800000, 0, 1);

    int hover = -1;          // esi: -1 none, 0 the close button, 1 the up arrow, 2 the down arrow
    int left = 0;            // bl: set when the pointer moved out of a zone, so the screen is redrawn
    int x = 0, y = 0;
    for (;;) {
        // ---------------------------------------------------------------- redraw (0x00454ccb)
        kStretchBlit(reinterpret_cast<void*>(0x00821F48), 0, 0x00821F74, reinterpret_cast<int>(kScreen), 0, 0, 0);
        int rowCounter = 0;                                                      // 0x00454cea
        kWidgetSetQuad(kScreen, 0, static_cast<int>(0x80000000), -1, 2, 2);      // 0x00454cf7
        kWidgetDraw(kScreen, 0, 0x00821020, 0x004D32F0, 0xD4, 0x13,
                    SLen(reinterpret_cast<char*>(0x004D32F0)));                  // 0x00454d0c
        kWidgetDraw(kScreen, 0, 0x00821EE8, 0x004C8674, 0x21, 0x3C,
                    SLen(reinterpret_cast<char*>(0x004C8674)));                  // 0x00454d3d
        static const unsigned kHead[4] = { 0x004D32EC, 0x004D32E8, 0x004D32E0, 0x004D32D8 };
        static const int kHeadX[4] = { 0xAA, 0xD1, 0xF9, 0x140 };
        for (int h = 0; h < 4; h++)                                              // 0x00454d67..0x00454e22
            kDrawStringAt(kScreen, 0, 0x00821EE8, static_cast<int>(kHead[h]), kHeadX[h], 0x3C,
                          SLen(reinterpret_cast<char*>(kHead[h])));
        // 0x00454e27..0x00454eab: the 18 hole numbers across the top
        for (int hole = 1, hx = 0x17D; hx < 0x2F7; hx += 0x15, hole++) {
            kText[0] = 0;                                                        // 0x00454e39
            SCat(kText, kItoa(hole, kItoaBuf, 10));                              // 0x00454e40
            kWidgetDraw2(kScreen, 0, 0x00821EE8, reinterpret_cast<int>(kText), hx, 0x3C, 0x13, SLen(kText));
        }
        // 0x00454ead..0x00454f1a: the scrollbar, only when more rows exist than fit
        if (count > 0x16) {
            int thumb = (364 * offset) / count;                                  // 0x00454ec0..0x00454ed7
            kImageDraw(reinterpret_cast<void*>(0x0082228C), 0, reinterpret_cast<int>(kScreen), 0x2FF, 0x50, 0);
            int h = 0x1F48 / count;                                              // 0x00454eef..0x00454efa
            if (h >= 0x16C - thumb) h = 0x16C - thumb;                           // 0x00454efe / 0x00454f02
            kFillWindowRect(kRootWindow, 0, 0x305, thumb + 0x7A, 6, h, static_cast<int>(0x80007FFF));
        }
        // ---------------------------------------------------------------- the rows (0x00454f24..0x0045541a)
        for (int rowY = 0x59; rowY < 0x211; ) {
            // 0x00454f24..0x00454f70: the seed name 0x004d32c4 (19 bytes) and the scan state
            char best[19];
            for (int b = 0; b < 19; b++) best[b] = G<char>(0x004D32C4 + b);
            int bestIdx = -1;                                                    // 0x00454f65
            // 0x00454f79..0x00454fff: the smallest name among the records that are not excluded and have members;
            // a name equal to the current best replaces it (the test is `jl`, 0x00454fb4)
            for (int k = 0; k < static_cast<int>(kTypeCount); k++) {
                unsigned rec = kTypes + kTypeStride * k;
                const char* nm = reinterpret_cast<const char*>(kTypeNames + kTypeNameStride * k);
                if ((G<unsigned char>(rec + 2) & 0x80) != 0) continue;           // 0x00454f79
                if (G<unsigned short>(rec + 0x2A) == 0) continue;                // 0x00454f7f
                if (SCmp(best, nm) < 0) continue;                                // 0x00454f89..0x00454fb4
                bestIdx = k;                                                     // 0x00454fdb
                SCpy(best, nm);                                                  // 0x00454fb6..0x00454fd9
            }
            if (bestIdx < 0) break;                                              // 0x00455005 / 0x00455008
            unsigned rec = kTypes + kTypeStride * bestIdx;                       // 0x0045500e..0x00455018
            unsigned char flags = static_cast<unsigned char>(G<unsigned char>(rec + 2) | 0x80);
            G<unsigned char>(rec + 2) = flags;                                   // 0x00455024: marked for this pass
            if (rowCounter < offset) { rowCounter++; continue; }                 // 0x0045502c..0x0045503b
            int status = flags & 7;                                              // 0x00455040
            if (G<unsigned char>(rec + 0x29) == 0xFF) status = -1;               // 0x00455045..0x00455056
            SCpy(kText, reinterpret_cast<const char*>(kTypeNames + kTypeNameStride * bestIdx));   // 0x0045505a
            kWidgetDraw(kScreen, 0, 0x00821EE8, reinterpret_cast<int>(kText), 0x1C, rowY, SLen(kText));  // 0x004550b7
            if (status != 1) {                                                   // 0x004550c0
                int img = status >= 3 ? (status > 3 ? 2 : 1) : (status == -1 ? 3 : 0);   // 0x004550d4..0x004550ed
                kImageDraw(reinterpret_cast<void*>(0x00821FCC + 88 * img), 0, reinterpret_cast<int>(kScreen),
                           0x7A, rowY - 4, 0);                                   // 0x004550f0..0x00455106
            }
            // 0x0045510b..0x0045518d: byte +0 as an unsigned number, the empty string 0x004c52bc when it is 0
            unsigned char v0 = G<unsigned char>(rec + 0);
            kText[0] = 0;
            SCat(kText, v0 != 0 ? kItoa(v0, kItoaBuf, 10) : reinterpret_cast<char*>(0x004C52BC));
            kDrawStringAt(kScreen, 0, 0x00821EE8, reinterpret_cast<int>(kText), 0xAA, rowY, SLen(kText));
            // 0x00455192..0x00455212: byte +1 as a signed number, drawn only when it is positive
            signed char v1 = static_cast<signed char>(G<unsigned char>(rec + 1));
            kText[0] = 0;
            SCat(kText, v1 > 0 ? kItoa(v1, kItoaBuf, 10) : reinterpret_cast<char*>(0x004C52BC));
            kWidgetDraw(kScreen, 0, 0x00821EE8, reinterpret_cast<int>(kText), 0xD1, rowY, SLen(kText));
            // 0x00455217..0x00455299: word +0x2a as a signed number
            short v2 = static_cast<short>(G<unsigned short>(rec + 0x2A));
            kText[0] = 0;
            SCat(kText, v2 != 0 ? kItoa(v2, kItoaBuf, 10) : reinterpret_cast<char*>(0x004C52BC));
            kWidgetDraw(kScreen, 0, 0x00821EE8, reinterpret_cast<int>(kText), 0xF9, rowY, SLen(kText));
            // 0x0045529e..0x00455335: one or two markers per hole from the flag byte at +3 + hole
            for (int hole = 1, hx = 0x17D; hx < 0x2F7; hx += 0x15, hole++) {
                unsigned char f = G<unsigned char>(rec + 3 + hole);              // 0x004552a8
                if ((f & 3) != 0) {                                              // 0x004552af
                    int e = (f & 1) != 0 ? ((~hole) & 1) | 8 : ((~hole) & 1) | 0xA;   // 0x004552c2..0x004552d4
                    kImageDraw(reinterpret_cast<void*>(0x00821FA0 + 44 * e), 0, reinterpret_cast<int>(kScreen),
                               hx, rowY - 4, 0);                                 // 0x004552d6..0x004552eb
                }
                if ((G<unsigned char>(rec + 3 + hole) & 4) != 0) {               // 0x004552f0
                    int e = ((~hole) & 1) | 6;                                   // 0x00455310..0x00455317
                    kImageDraw(reinterpret_cast<void*>(0x00821FA0 + 44 * e), 0, reinterpret_cast<int>(kScreen),
                               hx, rowY - 4, 0);                                 // 0x00455319..0x00455326
                }
            }
            // 0x0045533b..0x0045540c: the status label (jump table 0x00455a10, index status + 1 unsigned, 1 and
            // anything above 5 draw nothing)
            static const unsigned kLabel[6] = { 0x004C867C, 0, 0x004C3AC0, 0x004C8674, 0x004C8664, 0x004C8658 };
            unsigned sel = static_cast<unsigned>(status + 1);
            if (sel <= 5 && kLabel[sel] != 0)
                kDrawStringAt(kScreen, 0, 0x00821EE8, static_cast<int>(kLabel[sel]), 0x140, rowY,
                              SLen(reinterpret_cast<char*>(kLabel[sel])));
            rowY += 0x14;                                                        // 0x00455411
        }
        // ---------------------------------------------------------------- the bottom legend (0x00455420..0x00455548)
        static const unsigned kFoot[6] = { 0x004C8674, 0x004C8664, 0x004C8658, 0x004C867C, 0x004D32B8, 0x004D32A8 };
        static const int kFootX[6] = { 0x52, 0xD0, 0x146, 0x1C2, 0x23D, 0x2B5 };
        for (int f = 0; f < 6; f++)
            kDrawStringAt(kScreen, 0, 0x00821EE8, static_cast<int>(kFoot[f]), kFootX[f], 0x218,
                          SLen(reinterpret_cast<char*>(kFoot[f])));
        // 0x0045554d..0x00455711: a per-hole tally of the records whose byte +0x29 is 0xff, and a name list built in
        // the shared text buffer 0x0051a068. Nothing reads either: the list is appended to at 0x00455690 and each
        // non-zero tally clears the buffer again at 0x004556d4, and the buffer is next written at 0x00455111.
        {
            int cnt[0x13];
            for (int i = 0; i < 0x13; i++) cnt[i] = 0;                           // 0x0045554f..0x0045555a
            int listed = 0;                                                      // ebx
            for (int k = 0; k < static_cast<int>(kTypeCount); k++) {             // 0x00455564..0x00455659
                unsigned rec = kTypes + kTypeStride * k;
                if (G<unsigned char>(rec + 0x29) != 0xFF) continue;              // 0x00455569
                for (int i = 1; i < 0x13; i++)                                   // 0x00455573..0x0045558b
                    if ((G<unsigned char>(rec + 3 + i) & 4) != 0) cnt[i]++;
                if (listed != 0) SCat(kText, reinterpret_cast<const char*>(0x004C52B8));   // 0x0045558d..0x004555bf
                listed++;
                SCat(kText, reinterpret_cast<const char*>(kTypeNames + kTypeNameStride * k));   // 0x004555c1
                if (listed >= 0xA) {                                             // 0x004555eb
                    SCat(kText, reinterpret_cast<const char*>(0x004C76A8));      // 0x004555f2
                    listed = 0;                                                  // 0x00455618
                    SCpy(kText, reinterpret_cast<const char*>(0x004C3F70));      // 0x0045561f (a copy, not an append)
                }
            }
            if (listed == 0) SCat(kText, reinterpret_cast<const char*>(0x004D32A0));   // 0x0045565f..0x0045568e
            SCat(kText, reinterpret_cast<const char*>(0x004C4944));              // 0x00455690
            for (int i = 1; i < 0x13; i++) {                                     // 0x004556c6..0x00455711
                if (cnt[i] == 0) continue;
                kText[0] = 0;                                                    // 0x004556d4
                SCat(kText, kItoa(cnt[i], kItoaBuf, 10));
            }
        }
        // 0x00455713..0x00455728: the pass marker 0x80 is cleared from every record
        for (int k = 0; k < static_cast<int>(kTypeCount); k++)
            G<unsigned char>(kTypes + kTypeStride * k + 2) =
                static_cast<unsigned char>(G<unsigned char>(kTypes + kTypeStride * k + 2) & 0x7F);
        kViewToLocal(kRootWindow, 0, &x, &y);                                    // 0x0045572a
        hover = -1;                                                              // 0x00455743
        kImageDraw(reinterpret_cast<void*>(0x00820F40), 0, reinterpret_cast<int>(kScreen), 0x2DC, 0x224, 0);
        kRefreshFull(kRootWindow, 0, 0);                                         // 0x00455766
        kWaitTicks(0xA, 0);                                                      // 0x00455772
        kEnterModal483d30();                                                     // 0x0045577e
        left = 0;                                                                // 0x00455783
        // ---------------------------------------------------------------- the input loop (0x00455785)
        for (;;) {
            kWaitTicks(1, 0);                                                    // 0x00455785
            kViewToLocal(kRootWindow, 0, &x, &y);                                // 0x004557a0
            bool refresh = false, toButton = false;
            if (x >= 0x2DC && x < 0x308 && y >= 0x224 && y < 0x250) {            // 0x004557ad..0x004557c9
                if (hover != 0) {                                                // 0x004557cb
                    if (hover > 0) { left = 1; hover = 0; toButton = true; }     // 0x004557d3 / 0x00455961
                    else {
                        hover = 0;                                               // 0x004557de
                        kImageDraw(reinterpret_cast<void*>(0x00820F6C), 0, reinterpret_cast<int>(kScreen),
                                   0x2DC, 0x224, 0);                             // 0x004557e4..0x004557fa
                        refresh = true;                                          // 0x004557ff
                    }
                }
            } else if (x >= 0x2FF && x < 0x311 && y >= 0x50 && y < 0x75) {       // 0x00455804..0x00455822
                if (hover != 1) {                                                // 0x00455824
                    if (hover > 0) { left = 1; hover = 1; toButton = true; }     // 0x00455857 / 0x00455967
                    else {
                        if (hover == 0)                                          // 0x0045582d
                            kImageDraw(reinterpret_cast<void*>(0x00820F40), 0, reinterpret_cast<int>(kScreen),
                                       0x2DC, 0x224, 0);                         // 0x00455831..0x00455850
                        kImageDraw(reinterpret_cast<void*>(0x008221B0), 0, reinterpret_cast<int>(kScreen),
                                   0x2FF, 0x50, 0);                              // 0x0045585d..0x0045587f
                        hover = 1;
                        refresh = true;
                    }
                }
            } else if (x >= 0x2FF && x < 0x311 && y >= 0x1EA && y < 0x20F) {     // 0x00455889..0x0045589e
                if (hover != 2) {                                                // 0x004558a0
                    if (hover > 0) { left = 1; hover = 2; toButton = true; }     // 0x004558d3 / 0x00455970
                    else {
                        if (hover == 0)                                          // 0x004558a9
                            kImageDraw(reinterpret_cast<void*>(0x00820F40), 0, reinterpret_cast<int>(kScreen),
                                       0x2DC, 0x224, 0);                         // 0x004558ad..0x004558cc
                        kImageDraw(reinterpret_cast<void*>(0x008221DC), 0, reinterpret_cast<int>(kScreen),
                                   0x2FF, 0x1EA, 0);                             // 0x004558d9..0x004558fe
                        hover = 2;
                        refresh = true;
                    }
                }
            } else {                                                             // 0x00455905
                if (hover != -1) {                                               // 0x00455905
                    if (hover > 0) { left = 1; hover = -1; toButton = true; }    // 0x00455934 / 0x00455979
                    else {
                        kImageDraw(reinterpret_cast<void*>(0x00820F40), 0, reinterpret_cast<int>(kScreen),
                                   0x2DC, 0x224, 0);                             // 0x0045590e..0x0045592d
                        hover = -1;                                              // 0x00455936
                        refresh = true;
                    }
                }
            }
            if (toButton) break;                                                 // the zone changed: read the button now
            if (refresh) kRefreshFull(kRootWindow, 0, 0);                        // 0x00455939
            kPumpMessages();                                                     // 0x00455945
            if (G<int>(0x00822D68) != 0) break;                                  // 0x0045594a
            if (kFlushRedraw() != 0) break;                                      // 0x00455953
        }
        // ---------------------------------------------------------------- the button (0x0045597e)
        if (G<int>(0x00822D68) == 1) {                                           // 0x0045597e
            if (hover == 0) break;                                               // 0x00455989 -> 0x004559fc
            if (hover == 1) {                                                    // 0x0045598b
                if (offset > 0) offset -= kClamp(offset, 1, 3);                  // 0x00455990..0x004559ac
            } else if (hover == 2) {                                             // 0x004559b5
                if (count - 0x16 > offset)                                       // 0x004559be..0x004559cb
                    offset += kClamp(count - offset - 0x16, 1, 3);               // 0x004559d1..0x004559eb
            }
        } else {                                                                 // 0x004559f4
            if (left == 0) break;                                                // -> 0x004559fc
        }
    }
    G<unsigned char>(0x00569498) = 0;                                            // 0x004559fc
    kPumpLight();                                                                // 0x00455a03
}
V0_t ShowMembershipRoster_orig;
// 0x00454c50
SG_HOOK("golf_clean.exe", 0x00454c50, showMembershipRoster, ShowMembershipRoster_re, ShowMembershipRoster_orig);

// ====================================================================================================================
// showHoleStatsScreen 0x00453330 (__cdecl, one stack argument: the hole index)
// ====================================================================================================================
// A course record is 0x208 bytes from 0x00575ab0 and the index is multiplied by it at 0x00453337..0x00453347. Fields
// the screen reads: byte +0 (0x00575ab0, the par), word +4 (0x00575ab4), dword +0x20 (0x00575ad0, the rounds played),
// dword +0x24 (0x00575ad4), byte +0x200 (0x00575cb0), the words +0x58 / +0x5e / +0x60 / +0x62 / +0x66
// (0x00575c08..0x00575c16), the per-score histogram of 8 columns of 11 words at +0x28 (0x00575ad8) and the 0x40
// comment counters at +0xd8 (0x00575b88). The page it shows is (timeGetTime() >> 10) % ((rounds != 0) ? 5 : 3)
// (0x00453362..0x0045337b), and the input loop re-draws the whole screen whenever that value changes
// (0x00454588, 0x00454655, 0x0045465f).
// 0x00453330
void __cdecl ShowHoleStatsScreen_re(int hole) {
    unsigned rec = static_cast<unsigned>(hole) * 0x208u;                         // 0x00453337..0x00453347
    bool firstPass = true;                                                       // [esp+0x13] (0x00453355)
    int hover = 0;                                                               // [esp+0x38] (0x0045335a)
    int divisor = (G<int>(0x00575AD0 + rec) != 0) ? 5 : 3;                       // 0x0045336b..0x00453376
    unsigned page = TimeGetTime() >> 10;                                      // 0x00453362 / 0x00453368
    page %= static_cast<unsigned>(divisor);                                      // 0x0045337b (unsigned div)
    // 0x00453374..0x0045339e
    kDrawSprite(reinterpret_cast<void*>(0x00821040), 0, reinterpret_cast<int>(kScreen), 0, 0, 0x3E800000, 0, 1);

    int y = 0;                       // [esp+0x14]: the y the comment panels advance
    int commentBest = 0;             // [esp+0x48]: the original loads it uninitialised at 0x004542a9 and only writes
                                     // it back at 0x004544db; the scan below overwrites it before any use
    for (;;) {
        // ---------------------------------------------------------------- redraw (0x004533a3)
        int rounds = G<int>(0x00575AD0 + rec);                                   // 0x004533a3 -> [esp+0x1c]
        kStretchBlit(reinterpret_cast<void*>(0x00821200), 0, 0x0082122C, reinterpret_cast<int>(kScreen), 0, 0, 0);
        y = G<int>(0x0082121C);                                                  // 0x004533d1 / 0x004533db
        kWidgetSetQuad(kScreen, 0, static_cast<int>(0x80000000), -1, 2, 2);      // 0x004533eb
        SCpy(kText, reinterpret_cast<const char*>(0x004D326C));                  // 0x004533f0..0x00453414
        kAppendHoleName(hole);                                                   // 0x00453416
        if ((G<unsigned char>(0x00575CB0 + rec) & 0x81) != 0) {                  // 0x0045341b / 0x00453424
            SCat(kText, reinterpret_cast<const char*>(0x004C4974));              // 0x0045342c..0x0045345f
            SCat(kText, kItoa(hole, kItoaBuf, 10));                              // 0x00453461..0x00453491
            SCat(kText, reinterpret_cast<const char*>(0x004C59E0));              // 0x00453493..0x004534bc
        }
        kDrawStringAt(kScreen, 0, 0x00821020, reinterpret_cast<int>(kText), 0x181, 0x30, SLen(kText));   // 0x004534ea
        SCpy(kText, reinterpret_cast<const char*>(0x004C88F0));                  // 0x004534ef..0x00453519
        kWidgetDraw(kScreen, 0, 0x00821EE8, reinterpret_cast<int>(kText), 0x1B9, 0x53, SLen(kText));     // 0x00453540
        kText[0] = 0;                                                            // 0x00453554
        SCat(kText, kItoa(G<short>(0x00575AB4 + rec), kItoaBuf, 10));            // 0x00453545..0x00453592
        kDrawStringAt(kScreen, 0, 0x00821EE8, reinterpret_cast<int>(kText), 0x218, 0x53, SLen(kText));   // 0x004535b9
        if (G<int>(0x00575AD0 + rec) != 0) {                                     // 0x004535be / 0x004535c6
            // 0x004535d0..0x004535ed: one round is taken off for every golfer record whose byte 0x005794d9 + 0x100 * k
            // names this hole and whose next byte is above 0
            for (unsigned p = 0x005794DA; p < 0x00582CDA; p += 0x100) {
                if (G<signed char>(p - 1) != hole) continue;                     // 0x004535d5 / 0x004535db
                if (G<signed char>(p) <= 0) continue;                            // 0x004535dd / 0x004535e0
                rounds--;                                                        // 0x004535e2
            }
            if (rounds == 0) rounds = 1;                                         // 0x004535ef..0x004535f7
            // 0x004535ff..0x0045360c: one of five pages (jump table 0x00454694); a page above 4 draws nothing
            if (page <= 4) {
            int value = 0;
            const char* label = 0;
            const char* suffix = 0;
            bool cents = false, hasValue = false;
            switch (page) {
            case 0:                                                              // 0x00453613
                label = reinterpret_cast<const char*>(0x004D325C);
                value = G<short>(0x00575C0C + rec) / G<int>(0x00575AD0 + rec);   // 0x00453669..0x00453671 (raw rounds)
                suffix = reinterpret_cast<const char*>(0x004C60CC);              // 0x00453726 (shared with page 1)
                hasValue = true;
                break;
            case 1:                                                              // 0x00453688
                label = reinterpret_cast<const char*>(0x004D324C);
                value = G<short>(0x00575C16 + rec);                              // 0x004536de
                suffix = reinterpret_cast<const char*>(0x004C60CC);              // 0x00453726
                hasValue = true;
                break;
            case 2:                                                              // 0x00453763
                label = reinterpret_cast<const char*>(0x004D323C);
                value = G<short>(0x00575C0E + rec) * 100 / G<int>(0x00575AD0 + rec);   // 0x004537b9..0x004537d8
                suffix = reinterpret_cast<const char*>(0x004C60C4);              // 0x00453811
                hasValue = true;
                break;
            case 3:                                                              // 0x0045384e
                label = reinterpret_cast<const char*>(0x004D322C);
                value = G<short>(0x00575C10 + rec) * 100 / rounds;                // 0x004538a4..0x004538c3
                suffix = reinterpret_cast<const char*>(0x004C60C4);              // 0x004538fa
                hasValue = true;
                break;
            default:                                                             // case 4, 0x00453937
                label = reinterpret_cast<const char*>(0x004D321C);
                value = G<short>(0x00575C12 + rec) * 100 / rounds;               // 0x0045398d..0x004539a5
                cents = true;
                hasValue = true;
                break;
            }
            SCpy(kText, label);                                                  // each page's first copy
            kWidgetDraw(kScreen, 0, 0x00821EE8, reinterpret_cast<int>(kText), 0x1B9, 0x68, SLen(kText));
            kText[0] = 0;
            if (cents) kAppendCents(value);                                      // 0x004539aa
            else SCat(kText, kItoa(value, kItoaBuf, 10));                        // 0x004536f4
            if (suffix != 0) SCat(kText, suffix);
            if (hasValue)                                                        // 0x004539bb
                kDrawStringAt(kScreen, 0, 0x00821EE8, reinterpret_cast<int>(kText), 0x24F, 0x68, SLen(kText));
            }
        }
        // 0x004539e6..0x00453ab0
        SCpy(kText, reinterpret_cast<const char*>(0x004C64D0));
        kWidgetDraw(kScreen, 0, 0x00821EE8, reinterpret_cast<int>(kText), 0x1B9, 0x7D, SLen(kText));
        kText[0] = 0;                                                            // 0x00453a4b
        SCat(kText, kItoa(G<signed char>(0x00575AB0 + rec), kItoaBuf, 10));      // 0x00453a3c..0x00453a52
        kDrawStringAt(kScreen, 0, 0x00821EE8, reinterpret_cast<int>(kText), 0x218, 0x7D, SLen(kText));   // 0x00453ab0
        kWidgetDraw(kScreen, 0, 0x00821EE8, 0x004D3200, 0xBE, 0xB0,
                    SLen(reinterpret_cast<char*>(0x004D3200)));                  // 0x00453ab5..0x00453ae4
        // ---------------------------------------------------------------- the score histogram (0x00453ae9..0x00453ddd)
        int par = G<signed char>(0x00575AB0 + rec);                              // 0x00453ae9
        int total = 0, weighted = 0;                                             // 0x00453af7 / 0x00453afb
        int colWeighted[8], colCount[8];
        for (int c = 0; c < 8; c++) { colWeighted[c] = par * 8; colCount[c] = 8; }   // 0x00453aff..0x00453b17
        int lo = par - 2;                                                        // 0x00453b19
        if (lo < 1) lo = 1;                                                      // 0x00453b1c / 0x00453b21
        int hi = lo + 5;                                                         // 0x00453b26
        int baseY = lo * 0x22;                                                   // 0x00453b39..0x00453b40
        int rowY = baseY;                                                        // 0x00453b46
        for (int score = lo; score <= hi; score++, rowY += 0x22) {               // 0x00453b2d / 0x00453d04..0x00453d1e
            kText[0] = 0;                                                        // 0x00453b56
            SCat(kText, kItoa(score, kItoaBuf, 10));                             // 0x00453b5d
            if (score == hi) SCat(kText, reinterpret_cast<const char*>(0x004C5340));   // 0x00453b91 / 0x00453b97
            kDrawStringAt(kScreen, 0, 0x00821EE8, reinterpret_cast<int>(kText), rowY - baseY + 0x1BD, 0xA8,
                          SLen(kText));                                          // 0x00453bcd..0x00453bff
            int rowSum = 0;                                                      // ebp (0x00453c08)
            int col = 0;                                                         // esi / 4
            for (int off = 0; off < 0x58; off += 0xB, col++) {                   // 0x00453c74..0x00453c7d
                // the last row collects every score from `hi` up to 9 (0x00453c16..0x00453c1f)
                int last = (score == hi) ? 9 : score;
                for (int s = score; s <= last; s++) {                            // 0x00453c1f / 0x00453c71
                    int v = G<short>(0x00575AD8 + 0x208u * static_cast<unsigned>(hole) + 2 * (off + s));   // 0x00453c37
                    total += v;                                                  // 0x00453c3f
                    rowSum += v;                                                 // 0x00453c41
                    weighted += v * s;                                           // 0x00453c49..0x00453c4e
                    colCount[col] += v;                                          // 0x00453c52..0x00453c58
                    colWeighted[col] += v * s;                                   // 0x00453c5c..0x00453c66
                }
            }
            kText[0] = 0;                                                        // 0x00453c87
            SCat(kText, kItoa(rowSum, kItoaBuf, 10));                            // 0x00453c8e
            if (rowSum != 0)                                                     // 0x00453cbe / 0x00453cc2
                kDrawStringAt(kScreen, 0, 0x00821EE8, reinterpret_cast<int>(kText), rowY - baseY + 0x1BD, 0xBC,
                              SLen(kText));                                      // 0x00453ccd..0x00453cff
        }
        if (total != 0) {                                                        // 0x00453d24 / 0x00453d2a
            SCpy(kText, reinterpret_cast<const char*>(0x004D31F0));              // 0x00453d30..0x00453d5a
            kWidgetDraw(kScreen, 0, 0x00821EE8, reinterpret_cast<int>(kText), 0x1B9, 0x92, SLen(kText));
            kText[0] = 0;                                                        // 0x00453d8d
            kAppendCents(weighted * 100 / total);                                // 0x00453d89..0x00453da1
            kDrawStringAt(kScreen, 0, 0x00821EE8, reinterpret_cast<int>(kText), 0x24F, 0x92, SLen(kText));
        }
        kWidgetDraw(kScreen, 0, 0x00821EE8, 0x004D3064, 0xBE, 0x53,
                    SLen(reinterpret_cast<char*>(0x004D3064)));                  // 0x00453ddd..0x00453e05
        // 0x00453e0a..0x00453f43: the rating line, only when the record's dword +0x24 is non-zero
        if (G<int>(0x00575AD4 + rec) != 0) {
            int w = G<int>(0x00575AD4 + rec);
            int half = (w + (w < 0 ? 1 : 0)) >> 1;                              // 0x00453e23 / 0x00453e27 / 0x00453e39
            int denom = half + G<int>(0x00575AD0 + rec) + 4;                     // 0x00453e3e
            int v = G<short>(0x00575C08 + rec) * 100 / denom;                    // 0x00453e1c..0x00453e45
            kText[0] = 0;                                                        // 0x00453e47
            SCat(kText, kItoa(v, kItoaBuf, 10));                                 // 0x00453e51
            SCat(kText, reinterpret_cast<const char*>(0x004D31EC));              // 0x00453e83
            const char* word;
            if (v < 0) word = reinterpret_cast<const char*>(0x004D31E4);         // 0x00453eac / 0x00453eb2
            else {
                int tier = v / 10;                                               // 0x00453eb9..0x00453ec8
                word = tier == 0 ? reinterpret_cast<const char*>(0x004D31B8)     // 0x00453eca
                     : tier == 1 ? reinterpret_cast<const char*>(0x004D31C0)     // 0x00453ecf
                     : tier == 2 ? reinterpret_cast<const char*>(0x004D31C8)     // 0x00453ed2
                                 : reinterpret_cast<const char*>(0x004D31D4);    // 0x00453ed5
            }
            SCat(kText, word);                                                   // 0x00453eef
            kDrawStringAt(kScreen, 0, 0x00821EE8, reinterpret_cast<int>(kText), 0x164, 0x53, SLen(kText));
        }
        kWidgetDraw(kScreen, 0, 0x00821EE8, 0x004D31B0, 0xBE, 0x68,
                    SLen(reinterpret_cast<char*>(0x004D31B0)));                  // 0x00453f48..0x00453f74
        kWidgetDraw(kScreen, 0, 0x00821EE8, 0x004D31A4, 0xBE, 0x7D,
                    SLen(reinterpret_cast<char*>(0x004D31A4)));                  // 0x00453f79..0x00453fa1
        kWidgetDraw(kScreen, 0, 0x00821EE8, 0x004C88B0, 0xBE, 0x92,
                    SLen(reinterpret_cast<char*>(0x004C88B0)));                  // 0x00453fa6..0x00453fd5
        // 0x00453fda..0x004541c5: three comparisons of a column's mean against column 7's, each drawn only when both
        // counts are non-zero
        if (colCount[7] != 0) {                                                  // 0x00453fda / 0x00453fe0
            static const int kCmp[3][2] = { { 6, 0x68 }, { 5, 0x7D }, { 3, 0x92 } };
            for (int k = 0; k < 3; k++) {
                int c = kCmp[k][0];
                if (colCount[c] == 0) continue;                                  // 0x00453ff1 / 0x00454090 / 0x0045412f
                SCpy(kText, reinterpret_cast<const char*>(0x004C5340));          // 0x00453ff9..0x00454031
                kAppendCents(colWeighted[c] * 100 / colCount[c]
                             - colWeighted[7] * 100 / colCount[7]);              // 0x00454010..0x00454047
                kAppendRatingLabel(G<int>(0x00569628));                          // 0x0045404c / 0x00454053
                kDrawStringAt(kScreen, 0, 0x00821EE8, reinterpret_cast<int>(kText), 0x164, kCmp[k][1], SLen(kText));
            }
        }
        kWidgetDraw(kScreen, 0, 0x00821EE8, 0x004D3198, 0xBE, 0xCA,
                    SLen(reinterpret_cast<char*>(0x004D3198)));                  // 0x004541ca..0x004541f9
        // 0x004541fe..0x00454285: a banner, only when the par byte is zero; its text is drawn on the frames whose
        // clock has bit 0x200 set (`test ah, 2` at 0x00454239)
        if (G<unsigned char>(0x00575AB0 + rec) == 0) {                           // 0x00454202 / 0x0045420a
            kStretchBlit(reinterpret_cast<void*>(0x00821258), 0, 0x00821284, reinterpret_cast<int>(kScreen), 0, y, 0);
            if ((TimeGetTime() & 0x200) != 0) {                               // 0x00454233 / 0x00454239
                kWidgetSetQuad(kScreen, 0, static_cast<int>(0x80007D08), -1, 2, 2);   // 0x0045424e
                kDrawStringAt(kScreen, 0, 0x00821EE8, 0x004D3184, 0x190, y,
                              SLen(reinterpret_cast<char*>(0x004D3184)));        // 0x0045427a
            }
            y += G<int>(0x00821274);                                             // 0x0045427f
        }
        // 0x00454289..0x004544b3: the five loudest comments. The 0x40 counters at +0xd8 are sign-extended into a
        // local array, and each round takes the largest of the first 0x32 (0x004542c0 skips 0x32..0x3f) and clears it.
        int comment[0x40];
        for (int k = 0; k < 0x40; k++) comment[k] = G<short>(0x00575B88 + rec + 2 * k);   // 0x00454289..0x004542a7
        for (int round = 5; round != 0; round--) {                               // 0x004542ad / 0x004544aa
            int best = 0;                                                        // edx
            for (int k = 0; k < 0x40; k++) {                                     // 0x004542b9..0x004542d6
                if (k >= 0x32) continue;                                         // 0x004542c0
                if (comment[k] <= best) continue;                                // 0x004542c7
                best = comment[k];
                commentBest = k;                                                 // 0x004542cd
            }
            if (best == 0) continue;                                             // 0x004542d8
            if (G<int>(0x00575AD0 + rec) == 0) continue;                         // 0x004542e4 / 0x004542ec
            kStretchBlit(reinterpret_cast<void*>(0x00821258), 0, 0x00821284, reinterpret_cast<int>(kScreen), 0, y, 0);
            kText[0] = 0;                                                        // 0x0045432e
            SCat(kText, kItoa(comment[commentBest] * 100 / G<int>(0x00575AD0 + rec), kItoaBuf, 10));   // 0x00454319..0x00454343
            SCat(kText, reinterpret_cast<const char*>(0x004D317C));              // 0x00454377
            SCat(kText, reinterpret_cast<const char*>(0x004C4E54));              // 0x004543a2
            kBuildGolferComment(commentBest,
                                G<unsigned short>(0x00575C1C + rec + 2 * commentBest) & 0x3FFF,
                                hole * 11, 0x98);                                // 0x004543c8..0x004543ef
            SCat(kText, reinterpret_cast<const char*>(0x004C4E54));              // 0x004543f4
            int quad = G<int>(0x0058B198) == static_cast<int>(0x800023E8)        // 0x0045441d / 0x00454427
                     ? static_cast<int>(0x80001284)                              // 0x00454430
                     : (G<int>(0x0058B198) == static_cast<int>(0x80007D08)
                        ? static_cast<int>(0x80007D08)                           // 0x00454437..0x00454445
                        : static_cast<int>(0x80000000));
            kWidgetSetQuad(kScreen, 0, quad, -1, 2, 2);                          // 0x00454456
            kDrawStringAt(kScreen, 0, 0x00821EE8, reinterpret_cast<int>(kText), 0x190, y, SLen(kText));   // 0x0045448a
            y += G<int>(0x00821274);                                             // 0x0045448f..0x004544a6
            comment[commentBest] = 0;                                            // 0x0045449b
        }
        // 0x004544b9..0x00454522: the footer and the close button
        kStretchBlit(reinterpret_cast<void*>(0x008212B0), 0, 0x008212DC, reinterpret_cast<int>(kScreen), 0, y, 0);
        int buttonY = y + 8;                                                     // 0x004544ed
        kImageDraw(reinterpret_cast<void*>(0x00820F40 + 44 * (hover != 0 ? 1 : 0)), 0,
                   reinterpret_cast<int>(kScreen), 0x23E, buttonY, 0);           // 0x00454502..0x00454516
        kRefreshFull(kRootWindow, 0, 0);                                         // 0x00454522
        if (firstPass) {                                                         // 0x00454527 / 0x0045452d
            kWaitTicks(0xA, 0);                                                  // 0x00454533
            kEnterModal483d30();                                                 // 0x0045453b
            firstPass = false;                                                   // 0x00454540
        }
        // ---------------------------------------------------------------- the input loop (0x00454545)
        int px = 0, py = 0;
        kWaitTicks(1, 0);                                                        // 0x00454549
        kViewToLocal(kRootWindow, 0, &px, &py);                                  // 0x00454560
        int div2 = (rounds != 0) ? 5 : 3;                                        // 0x00454565..0x00454576
        bool again = false;
        if (page != (TimeGetTime() >> 10) % static_cast<unsigned>(div2)) {    // 0x0045457d..0x0045458c
            page = (TimeGetTime() >> 10) % static_cast<unsigned>(div2);       // 0x0045465f..0x0045466e
            continue;                                                           // 0x00454672 -> the redraw
        }
        for (;;) {
            // 0x00454592..0x00454610: the close button's box is (0x23e..0x269, buttonY..buttonY+0x2b)
            bool inBox = px >= 0x23E && px < 0x26A && py >= buttonY && py < buttonY + 0x2C;
            if (inBox && hover == 0) {                                           // 0x004545b3
                hover = 1;                                                       // 0x004545c2 / 0x004545d2
                kImageDraw(reinterpret_cast<void*>(0x00820F6C), 0, reinterpret_cast<int>(kScreen), 0x23E, buttonY, 0);
                kRefreshFull(kRootWindow, 0, 0);                                 // 0x0045460f
            } else if (!inBox && hover != 0) {                                   // 0x004545de / 0x004545e0
                hover = 0;                                                       // 0x004545e7 / 0x004545f9
                kImageDraw(reinterpret_cast<void*>(0x00820F40), 0, reinterpret_cast<int>(kScreen), 0x23E, buttonY, 0);
                kRefreshFull(kRootWindow, 0, 0);                                 // 0x0045460f
            }
            kPumpMessages();                                                     // 0x00454614
            if (G<int>(0x00822D68) != 0) { again = false; break; }               // 0x00454619 / 0x00454620
            if (kFlushRedraw() != 0) { again = false; break; }                   // 0x00454622 / 0x00454629
            kWaitTicks(1, 0);                                                    // 0x0045462e
            kViewToLocal(kRootWindow, 0, &px, &py);                              // 0x00454645
            if (page != (TimeGetTime() >> 10) % static_cast<unsigned>(div2)) {   // 0x0045464a..0x00454659
                page = (TimeGetTime() >> 10) % static_cast<unsigned>(div2);   // 0x0045465f..0x0045466e
                again = true;
                break;
            }
        }
        if (again) continue;                                                     // 0x00454672
        break;                                                                   // 0x00454677
    }
    G<int>(0x0053DF54) = 0;                                                      // 0x00454677
    kPumpLight();                                                                // 0x00454681
}
V1_t ShowHoleStatsScreen_orig;
// 0x00453330
SG_HOOK("golf_clean.exe", 0x00453330, showHoleStatsScreen, ShowHoleStatsScreen_re, ShowHoleStatsScreen_orig);

// ====================================================================================================================
// showTournamentResults 0x0045a090 (__cdecl, no arguments)
// ====================================================================================================================
// Scores every golfer against the course records, draws the leader board in rank order and, for the winner, pays the
// prize. Golfer records are 0x100 bytes (the hole the golfer is on at 0x005794d9 + 0x100 * k, its per-hole strokes at
// 0x005794db + 0x100 * k + hole); course records are 0x208 bytes from 0x00575ab0, with the hole's own stroke count at
// +8 (0x00575cb8 is record 1's). [0x005685f0] is the hole count, [0x00567b04] the prize purse.
// 0x0045a090
void __cdecl ShowTournamentResults_re() {
    int scores[152];
    for (int k = 0; k < 152; k++) scores[k] = -1;                                // 0x0045a09a..0x0045a0a8
    kSetDrawTarget(G<void*>(0x004C1570), 0, 0x00519FD8, 0, 0, 0);                // 0x0045a0c1
    int altLayout = 0;                                                           // 0x0045a0b9
    // 0x0045a0c6..0x0045a165: every golfer whose hole is a live course record is scored against every OTHER hole
    for (int k = 0; k < 152; k++) {
        unsigned g = 0x005794D9u + 0x100u * static_cast<unsigned>(k);
        int hole = k / 2 + 1;                                                    // 0x0045a0d9..0x0045a0de
        if (hole >= 0x13) continue;                                              // 0x0045a0df / 0x0045a0e2
        if (G<unsigned char>(0x00575AB0 + 0x208u * static_cast<unsigned>(hole)) == 0) continue;   // 0x0045a0eb
        if (G<unsigned char>(g) != 0) altLayout = 1;                             // 0x0045a0f6 / 0x0045a0fd
        int sum = 0;                                                             // 0x0045a105
        for (int h = 1; h <= 18; h++) {                                          // 0x0045a111..0x0045a13d
            signed char par = G<signed char>(0x00575CB8 + 0x208u * static_cast<unsigned>(h - 1));   // record h +0
            if (par == 0) continue;                                              // 0x0045a113
            signed char shot = G<signed char>(g + 2 + h);                        // 0x0045a117 (0x005794db + h)
            if (shot == 0) continue;                                             // 0x0045a11a
            if (G<signed char>(g) == h) continue;                                // 0x0045a11e / 0x0045a122
            sum += shot - par;                                                   // 0x0045a126..0x0045a12e
        }
        scores[k] = sum;                                                         // 0x0045a143
    }
    int rank = 0;                                                                // 0x0045a16f
    int xbase = 6;                                                               // 0x0045a179
    int y;
    if (altLayout != 0) {
        // 0x0045a187: the leader-board header
        kDrawFrameR3(0, 8, 0x90, G<int>(0x005685F0) * 0x16 + 0x10, 1);           // 0x0045a18c..0x0045a1a2
        if ((G<int>(0x0059E7B8) & 0x4000000) != 0) {                             // 0x0045a1a7 / 0x0045a1af
            kDrawCentered(0x004D3820, 0x48, 9, static_cast<int>(0x80007FF0));    // 0x0045a1c8
            switch (G<int>(0x00822C88)) {                                        // 0x0045a1cd (jump table 0x0045a1e4)
            case 0: SCpy(kText, reinterpret_cast<const char*>(0x004D2E88)); break;
            case 1: SCpy(kText, reinterpret_cast<const char*>(0x004D3808)); break;
            case 2: SCpy(kText, reinterpret_cast<const char*>(0x004D37F0)); break;
            case 3: SCpy(kText, reinterpret_cast<const char*>(0x004D37D8)); break;
            default: break;                                                      // the buffer keeps its old text
            }
            kDrawCentered(reinterpret_cast<int>(kText), 0x48, 0x15, static_cast<int>(0x80007FF0));   // 0x0045a25f
            SCpy(kText, reinterpret_cast<const char*>(0x004D37D4));              // 0x0045a264..0x0045a287
            kAppendCourseTitle(-1);                                              // 0x0045a28b
            kDrawCentered(reinterpret_cast<int>(kText), 0x48, 0x21, static_cast<int>(0x80007FF0));   // 0x0045a29e
        } else {
            kDrawCentered(0x004D37C4, 0x48, 9, static_cast<int>(0x80007FF0));    // 0x0045a2b8
            SCpy(kText, reinterpret_cast<const char*>(0x004D37BC));              // 0x0045a2bd..0x0045a2ec
            SCat(kText, kItoa(G<int>(0x00567B04), kItoaBuf, 10));                // 0x0045a2f0
            SCat(kText, reinterpret_cast<const char*>(0x004D37B4));              // 0x0045a32d
            kDrawCentered(reinterpret_cast<int>(kText), 0x48, 0x15, static_cast<int>(0x80007FF0));   // 0x0045a358
            kText[0] = 0;                                                        // 0x0045a35a
            SCat(kText, kItoa(G<short>(0x005A6D3C) + 0x7D1, kItoaBuf, 10));      // 0x0045a35d..0x0045a378
            SCat(kText, reinterpret_cast<const char*>(0x004C3F70));              // 0x0045a3b1
            SCat(kText, reinterpret_cast<const char*>(0x00571FD8 + 0x2E * G<int>(0x0059BF90)));   // 0x0045a3d4..0x0045a3ef
            SCat(kText, reinterpret_cast<const char*>(0x004D37AC));              // 0x0045a41c
            kDrawCentered(reinterpret_cast<int>(kText), 0x48, 0x21, static_cast<int>(0x80007FF0));   // 0x0045a447
        }
        y = 0x2D;                                                                // 0x0045a455
    } else {
        // 0x0045a45c: the scorecard header
        kDrawSprite(reinterpret_cast<void*>(0x00821040), 0, reinterpret_cast<int>(kScreen), 0, 0, 0x3E800000, 0, 1);
        kStretchBlit(reinterpret_cast<void*>(0x00821CB8), 0, 0x00821CE4, reinterpret_cast<int>(kScreen), 0, 0, 0);
        kSetDrawTarget(G<void*>(0x004C1570), 0, 0x00821020, 0, 0, 0);            // 0x0045a4b7
        kDrawCentered(G<unsigned char>(0x005795D9) == 0 ? 0x004D3784 : 0x004D3798,
                      0x140, 0x10, static_cast<int>(0x80000000));                // 0x0045a4bc..0x0045a4dc
        kSetDrawTarget(G<void*>(0x004C1570), 0, 0x00821EE8, 0, 0, 0);            // 0x0045a4f5
        kDrawText4049d0(0x004D377C, 0x19, 0x32, static_cast<int>(0x80000000));   // 0x0045a508
        xbase = 0x94;                                                            // 0x0045a50d
        int col = 0xAF;                                                          // 0x0045a515
        for (int h = 1; h <= 18; h++, col += 0x1B) {                             // 0x0045a51a..0x0045a595
            if (G<unsigned char>(0x00575CB8 + 0x208u * static_cast<unsigned>(h - 1)) == 0) continue;
            kText[0] = 0;                                                        // 0x0045a526
            SCat(kText, kItoa(h, kItoaBuf, 10));                                 // 0x0045a53f
            kDrawCentered(reinterpret_cast<int>(kText), col, 0x32, static_cast<int>(0x80000000));   // 0x0045a57b
        }
        kDrawCentered(0x004D3778, 0x29B, 0x32, static_cast<int>(0x80000000));    // 0x0045a5ac
        kDrawText4049d0(0x004D3770, 0x2BC, 0x32, static_cast<int>(0x80000000));  // 0x0045a5c2
        y = 0x4D;                                                                // 0x0045a5ca
    }
    // 0x0045a5d2: the purse defaults to 20 per hole less 20
    if (G<int>(0x00567B04) == 0)                                                 // 0x0045a5d2 / 0x0045a5df
        G<int>(0x00567B04) = G<int>(0x005685F0) * 0x14 - 0x14;                   // 0x0045a5e1..0x0045a5eb
    // 0x0045a5f1..0x0045ac3f: the rank order, one pass per score from -100 to 100
    for (int score = -100; score <= 100; score++) {                              // 0x0045a5f1 / 0x0045ac33
        for (int k = 0; k < 152; k++) {                                          // 0x0045a5f9 / 0x0045ac17
            int hole = k / 2 + 1;                                                // 0x0045a605..0x0045a60c
            if (hole >= 0x13) continue;                                          // 0x0045a610 -> the k tail
            if (G<unsigned char>(0x00575AB0 + 0x208u * static_cast<unsigned>(hole)) == 0) continue;   // 0x0045a626
            if (scores[k] == score) {                                            // 0x0045a630 / 0x0045a636
                rank++;                                                          // 0x0045a644
                if (altLayout == 0) {                                            // 0x0045a645
                    // 0x0045a64d..0x0045a6c3: the row's frame, by where the rank sits against the hole count
                    int rowY, img1 = 0, img2 = 0;
                    bool draw = true;
                    if (rank < G<int>(0x005685F0)) {                             // 0x0045a64d
                        if (rank == 0) draw = false;                             // 0x0045a651
                        else { rowY = y - 7; img2 = 0x00821D3C; img1 = 0x00821D10; }
                    } else if (rank == G<int>(0x005685F0)) {                     // 0x0045a67a
                        img1 = 0x00821D68; rowY = y - 8; img2 = 0x00821D94;
                    } else {
                        rowY = y - 4; img2 = 0x00821DEC; img1 = 0x00821DC0;
                    }
                    if (draw)
                        kStretchBlit(reinterpret_cast<void*>(img1), 0, img2, reinterpret_cast<int>(kScreen),
                                     0, rowY, 0);                                // 0x0045a6c3
                }
                // 0x0045a6c8..0x0045a7fd
                kText[0] = 0;
                SCat(kText, kItoa(rank, kItoaBuf, 10));                          // 0x0045a6d7
                SCat(kText, reinterpret_cast<const char*>(0x004D3414));          // 0x0045a6fd
                kBuildGolferName(k, 0);                                          // 0x0045a737
                if (altLayout != 0) {                                            // 0x0045a73c
                    SCat(kText, reinterpret_cast<const char*>(0x004C4974));      // 0x0045a73e..0x0045a76d
                    const char* s;
                    if (score == 0) s = reinterpret_cast<const char*>(0x004D376C);   // 0x0045a771 / 0x0045a777
                    else {
                        if (score > 0) SCat(kText, reinterpret_cast<const char*>(0x004C5340));   // 0x0045a77c..0x0045a7ab
                        s = kItoa(score, kItoaBuf, 10);                          // 0x0045a7be
                    }
                    SCat(kText, s);                                              // 0x0045a7c3
                    SCat(kText, reinterpret_cast<const char*>(0x004C59E0));      // 0x0045a7f2..0x0045a819
                }
                // 0x0045a81b..0x0045a87a: the colour and the x
                int colour, x;
                if (k == 1) {                                                    // 0x0045a81b
                    colour = (altLayout != 0 ? 0x1164 : 0) + static_cast<int>(0x80001284);   // 0x0045a822..0x0045a82d
                } else if (altLayout != 0) {                                     // 0x0045a846
                    colour = static_cast<int>(0x80007FFF);                       // 0x0045a84a
                } else {
                    colour = ((rank >= G<int>(0x005685F0) ? 1 : 0) - 1) & static_cast<int>(0xFFFFBDF0);
                    colour += static_cast<int>(0x80004210);                      // 0x0045a851..0x0045a864
                }
                if (altLayout != 0) x = (rank < 10) ? xbase + 1 : xbase - 5;     // 0x0045a832..0x0045a843 / 0x0045ab21
                else x = 0x19;                                                   // 0x0045a869
                kDrawText4049d0(reinterpret_cast<int>(kText), x, y, colour);     // 0x0045a87a
                if (altLayout != 0) { y += 0xB; }                                // 0x0045a882 -> 0x0045abec
                else {
                    // 0x0045a88a..0x0045a9bf: the per-hole strokes and the total
                    int total = 0;                                               // 0x0045a88e
                    unsigned gp = 0x005794DCu + 0x100u * static_cast<unsigned>(k);   // 0x0045a89b
                    int cx = xbase + 0x1B;                                       // 0x0045a8a4
                    for (unsigned rp = 0x00575CB8; rp < 0x00578148; rp += 0x208, gp++, cx += 0x1B) {
                        signed char v = G<signed char>(gp);                      // 0x0045a8ab
                        if (v == 0) continue;                                    // 0x0045a8ae
                        total += v;                                              // 0x0045a8c0
                        kText[0] = 0;                                            // 0x0045a8cb
                        SCat(kText, kItoa(v, kItoaBuf, 10));                     // 0x0045a8d2
                        int c = static_cast<int>(0x80000000);                    // 0x0045a8fd
                        if (v < G<signed char>(rp)) c = static_cast<int>(0x80006000);   // 0x0045a903 / 0x0045a908
                        if (G<signed char>(rp) < v) c = static_cast<int>(0x80000018);   // 0x0045a90e / 0x0045a913
                        kDrawCentered(reinterpret_cast<int>(kText), cx, y, c);   // 0x0045a92e
                    }
                    int c = static_cast<int>(0x80000000);                        // 0x0045a93b
                    if (score > 0) c = static_cast<int>(0x80006000);             // 0x0045a942
                    if (score < 0) c = static_cast<int>(0x80000018);             // 0x0045a94a
                    kText[0] = 0;                                                // 0x0045a978 region
                    SCat(kText, kItoa(total, kItoaBuf, 10));                     // 0x0045a980
                    kDrawCentered(reinterpret_cast<int>(kText), 0x29D, y, c);    // 0x0045a9bf
                    // 0x0045a9c4..0x0045abde: the prize line
                    if (rank < G<int>(0x005685F0) && G<unsigned char>(0x005795D9) == 0) {   // 0x0045a9c4 / 0x0045a9d8
                        kSetDrawTarget(G<void*>(0x004C1570), 0, 0x00821EE8, 0, 0, 0);       // 0x0045a9f6
                        SCpy(kText, reinterpret_cast<const char*>(0x004C61E0));  // 0x0045a9fb..0x0045aa15
                        kAppendNumber(G<int>(0x00567B04));                       // 0x0045aa27
                        SCat(kText, reinterpret_cast<const char*>(0x004D37B4));  // 0x0045aa2c..0x0045aa5b
                        kWidgetSetQuad(kScreen, 0, static_cast<int>(0x80001284), -1, 2, 2);  // 0x0045aa6c
                        kDrawStringRightAt(kScreen, 0, reinterpret_cast<int>(kText), 0x30C, y, SLen(kText));  // 0x0045aa97
                        if (k == 1) {                                            // 0x0045aa9c
                            kLogTick(0xE0, rank);                            // 0x0045aaad
                            int purse = G<int>(0x00567B04);                      // 0x0045aab2
                            G<int>(0x00571FD4) = G<int>(0x00571FD4) + (1000 * purse) / 100;   // 0x0045aac1..0x0045aaef
                            G<short>(0x0058421E + 0x14 * (G<short>(0x005A6D3C) % 100))
                                = static_cast<short>(G<short>(0x0058421E + 0x14 * (G<short>(0x005A6D3C) % 100))
                                                     + 10 * purse);              // 0x0045aad4..0x0045aafd
                            if (rank < 4) G<int>(0x005787CC) += 4 - rank;        // 0x0045ab05..0x0045ab19
                            else G<int>(0x005787CC)++;                           // 0x0045ab2d
                            if (rank == 1) {                                     // 0x0045ab33
                                if (G<int>(0x005685F0) > 9)                      // 0x0045ab38
                                    kPostEvent(0xB, G<int>(0x005795B8), G<int>(0x005795BC));   // 0x0045ab51
                                if (G<int>(0x005685F0) > 0x12) {                 // 0x0045ab59..0x0045ab65
                                    kPostEvent(0xF, G<int>(0x005795B8), G<int>(0x005795BC));   // 0x0045ab76
                                    if (G<int>(0x005685F0) > 0x12 && G<int>(0x00567B04) > 100000)   // 0x0045ab7b..0x0045ab91
                                        kPostEvent(G<signed char>(0x005A34E0) + 0x11, G<int>(0x005795B8),
                                                   G<unsigned char>(0x005795D6));   // 0x0045ab93..0x0045abad
                                }
                            }
                        }
                        y += G<int>(0x00821D2C);                                 // 0x0045abc0 / 0x0045abc9
                        G<int>(0x00567B04) = (2 * G<int>(0x00567B04)) / 3;       // 0x0045abc6..0x0045abd8
                    } else {
                        y += G<int>(0x00821DDC);                                 // 0x0045abe0
                    }
                }
            }
            // 0x0045abf1: the row limits
            if (y >= 0x225) break;                                               // 0x0045abf1 / 0x0045abf9
            if (rank >= (altLayout != 0 ? 0x24 : 0x12)) break;                   // 0x0045abfb..0x0045ac0f
        }
    }
    // 0x0045ac45..0x0045ae0d: the close button and the modal loop, only for the scorecard layout
    if (altLayout == 0 && G<unsigned char>(0x005795D9) == 0) {                    // 0x0045ac45 / 0x0045ac51
        int buttonY;
        if (rank == 0x12 && G<int>(0x005685F0) == 0x13) {                        // 0x0045ac5d / 0x0045ac66
            kStretchBlit(reinterpret_cast<void*>(0x00821E18), 0, 0x00821E44, reinterpret_cast<int>(kScreen),
                         0, y - 7, 0);                                           // 0x0045ac8f
            buttonY = y - 0x36 + G<int>(0x00821E34);                             // 0x0045ac94..0x0045aca2
        } else {
            kStretchBlit(reinterpret_cast<void*>(0x00821E70), 0, 0x00821E9C, reinterpret_cast<int>(kScreen),
                         0, y - 4, 0);                                           // 0x0045acc4
            buttonY = y - 0x33 + G<int>(0x00821E8C);                             // 0x0045acc9..0x0045acd1
        }
        kPlaySound(0x38, 100, 0, 0, 0);                                          // 0x0045acdd
        kImageDraw(reinterpret_cast<void*>(0x00820F40), 0, reinterpret_cast<int>(kScreen), 0x2DC, buttonY, 0);
        kRefreshFull(kRootWindow, 0, 0);                                         // 0x0045ad0d
        kWaitTicks(0x78, 0);                                                     // 0x0045ad16
        kEnterModal483d30();                                                     // 0x0045ad1e
        bool hover = false;                                                      // 0x0045ad23
        int px = 0, py = 0;
        for (;;) {
            kWaitTicks(1, 0);                                                    // 0x0045ad29
            kViewToLocal(kRootWindow, 0, &px, &py);                              // 0x0045ad40
            bool inBox = px >= 0x2DC && px < 0x2DC + G<int>(0x00820F58)
                      && py >= buttonY && py < buttonY + G<int>(0x00820F5C);     // 0x0045ad45..0x0045ad66
            if (!inBox) {
                if (hover) {                                                     // 0x0045ad6e
                    kImageDraw(reinterpret_cast<void*>(0x00820F40), 0, reinterpret_cast<int>(kScreen),
                               0x2DC, buttonY, 0);                               // 0x0045adbd
                    hover = false;
                    kRefreshFull(kRootWindow, 0, 0);                             // 0x0045adcb
                }
            } else if (!hover) {                                                 // 0x0045ad72
                hover = true;                                                    // 0x0045ad74
                kImageDraw(reinterpret_cast<void*>(0x00820F6C), 0, reinterpret_cast<int>(kScreen),
                           0x2DC, buttonY, 0);                                   // 0x0045ad96
                kRefreshFull(kRootWindow, 0, 0);                                 // 0x0045adcb
            }
            kPumpMessages();                                                     // 0x0045add0
            if (G<int>(0x00822D68) != 0) break;                                  // 0x0045add5
            if (kFlushRedraw() != 0) break;                                      // 0x0045adde
        }
        G<unsigned char>(0x00569498) = 0;                                        // 0x0045ade8
        kPumpLight();                                                            // 0x0045adf2
        G<int>(0x0059E7B8) &= static_cast<int>(0xFFDFFFFF);                      // 0x0045adf7
        kResetGolferState();                                                     // 0x0045ae01
        G<int>(0x00567B04) = 0;                                                  // 0x0045ae06
    }
}
V0_t ShowTournamentResults_orig;
// 0x0045a090
SG_HOOK("golf_clean.exe", 0x0045a090, showTournamentResults, ShowTournamentResults_re, ShowTournamentResults_orig);
