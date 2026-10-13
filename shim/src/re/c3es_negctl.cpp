// NEGATIVE CONTROL for batch c3es (the frame-mask pilot, re/frida/js/framemask.js). NEVER list this file in
// shim/re_batches.txt. It is a byte-faithful copy of c3es.cpp's ShowShortcutsScreen_re with exactly ONE wrong detail:
// a single callee is handed the WRONG frame object (a Surface swapped for the other), chosen by SG_NEG. Everything else
// -- the 63 text draws onto the fixed screen, the modal loop, the return value -- is identical, so a diff_hook run proves
// the frame mask (framemask.js) is what makes the wrong-target body read RED on $fmlog while the correct body is GREEN:
// the recorded token for that call's `this` carries the other Surface's object ordinal.
//
//   SG_NEG=1  Surface_stretchBlit's `this` and its source argument are swapped (surfB.stretch(surfA) for
//             surfA.stretch(surfB))                                   -> RED: two $fmlog tokens carry the wrong ordinal
//   SG_NEG=2  the second Surface_blitTakeDirty writes surfA again instead of surfB (both takes land on surfA)
//                                                                     -> RED: that call's `this` token is ordinal(surfA)
//   SG_NEG=3  the two Surfaces are freed in the wrong order (surfB's records appear where surfA's should)
//                                                                     -> RED: the free records' ordinals are swapped
//
// The wrong detail stays DORMANT until the exported switch C3esNegCtl[0] is set to 1 by __freeze.negOn() (freeze.js),
// which the fixture calls after it has frozen the first thread -- so the real game, which runs this hook from DllMain
// through startup, never sees the wrong body. Built INSTEAD of c3es.cpp through log/c3/c3es_negctl_batches.txt (driven by
// log/c3/alt/c3es_ctl.py --mode wrong, CSVs in log/c3/alt/c3es_ctl/wrong, never log/diff). No fixed-address page.
#include "hooks.h"
#include <stdlib.h>

// The exported switch (freeze.js arms word 0 after the freeze; word 1 is the magic the scanner matches).
extern "C" __declspec(dllexport) volatile int C3esNegCtl[4] = { 0, 0x5147454e, 0, 0 };

namespace {

template <typename T> static inline T& G(unsigned addr) { return *reinterpret_cast<T*>(addr); }
template <typename T> static inline T& At(void* p, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(p) + off); }
template <typename F> static inline F vslot(void* obj, unsigned off) {
    return *reinterpret_cast<F*>(*reinterpret_cast<char**>(obj) + off);
}

typedef int(__cdecl* C0_t)();
typedef int(__cdecl* C2_t)(int, int);
typedef int(__fastcall* T0_t)(void* ecx, void* edx);
typedef int(__fastcall* Tn1_t)(void* ecx, void* edx, int);
typedef int(__fastcall* Tn2_t)(void* ecx, void* edx, int, int);
typedef int(__fastcall* Tn4_t)(void* ecx, void* edx, int, int, int, int);
typedef int(__fastcall* Tn5_t)(void* ecx, void* edx, int, int, int, int, int);
typedef int(__fastcall* Tn6_t)(void* ecx, void* edx, int, int, int, int, int, int);
typedef int(__fastcall* Tn7_t)(void* ecx, void* edx, int, int, int, int, int, int, int);

static int SLen(const char* s) { int n = 0; while (s[n]) n++; return n; }

const T0_t kImageCtor = reinterpret_cast<T0_t>(0x00474AE0);
const T0_t kSurfaceCtor = reinterpret_cast<T0_t>(0x00473AB0);
const Tn6_t kSurfaceCreateFull = reinterpret_cast<Tn6_t>(0x00474DD0);
const Tn5_t kLoadImage = reinterpret_cast<Tn5_t>(0x00475840);
const Tn7_t kBlitTakeDirty = reinterpret_cast<Tn7_t>(0x00473C60);
const T0_t kImageReleaseAll = reinterpret_cast<T0_t>(0x00474CB0);
const Tn6_t kDrawSprite = reinterpret_cast<Tn6_t>(0x00474440);
const Tn5_t kStretchBlit = reinterpret_cast<Tn5_t>(0x00473F60);
const Tn4_t kWidgetSetQuad = reinterpret_cast<Tn4_t>(0x00476310);
const Tn5_t kDrawStringAt = reinterpret_cast<Tn5_t>(0x00476700);
const Tn5_t kWidgetDraw = reinterpret_cast<Tn5_t>(0x00476650);
const Tn4_t kImageDraw = reinterpret_cast<Tn4_t>(0x00473E60);
const Tn1_t kRefreshFull = reinterpret_cast<Tn1_t>(0x00480C80);
const Tn2_t kViewToLocal = reinterpret_cast<Tn2_t>(0x0047AB50);
const C2_t kWaitTicks = reinterpret_cast<C2_t>(0x0045BF80);
const C0_t kPumpMessages = reinterpret_cast<C0_t>(0x0045C030);
const C0_t kFlushRedraw = reinterpret_cast<C0_t>(0x0045AE70);
const C0_t kPumpLight = reinterpret_cast<C0_t>(0x0045C150);
const C0_t kEnterModal483d30 = reinterpret_cast<C0_t>(0x00483D30);
const T0_t kSurfaceFree = reinterpret_cast<T0_t>(0x00473AE0);
const T0_t kImageDtor = reinterpret_cast<T0_t>(0x00474C40);

void* const kScreen = reinterpret_cast<void*>(0x00519CD4);
void* const kRootWindow = reinterpret_cast<void*>(0x00519A60);
const int kScreenI = 0x00519CD4;
const unsigned kSurfaceVtbl = 0x004BA2D8;

// armed only after the freeze; 0 during real startup
static int NegVariant() {
    if (C3esNegCtl[0] == 0) return 0;
    static int v = -1;
    if (v < 0) { const char* e = getenv("SG_NEG"); v = e ? atoi(e) : 1; }
    return v;
}

struct ShortcutText { unsigned char widget; unsigned font; unsigned text; short x, y; };

const ShortcutText kShortcutTop[] = {
    {0, 0x00821020, 0x004D2DAC, 0x11F, 0x1C}, {0, 0x00821EE8, 0x004D2DA8, 0x44, 0x51},
    {1, 0x00821EE8, 0x004D2D90, 0x6C, 0x51}, {0, 0x00821EE8, 0x004D2D8C, 0x44, 0x62},
    {1, 0x00821EE8, 0x004D2D74, 0x6C, 0x62}, {0, 0x00821EE8, 0x004D2D70, 0x44, 0x73},
    {1, 0x00821EE8, 0x004D2D64, 0x6C, 0x73}, {0, 0x00821EE8, 0x004D2D60, 0x44, 0x84},
    {1, 0x00821EE8, 0x004D2D4C, 0x6C, 0x84}, {0, 0x00821EE8, 0x004D2D48, 0x44, 0x95},
    {1, 0x00821EE8, 0x004D2D30, 0x6C, 0x95}, {0, 0x00821EE8, 0x004D2D2C, 0x44, 0xA6},
    {1, 0x00821EE8, 0x004D2D20, 0x6C, 0xA6}, {0, 0x00821EE8, 0x004D2D1C, 0x44, 0xB7},
    {1, 0x00821EE8, 0x004D2D0C, 0x6C, 0xB7}, {0, 0x00821EE8, 0x004D2D08, 0x44, 0xC8},
};
const ShortcutText kShortcutMid = {1, 0x00821EE8, 0x004D2CE8, 0x6C, 0xC8};
const ShortcutText kShortcutRest[] = {
    {0, 0x00821EE8, 0x004D2CE4, 0x44, 0xD9}, {1, 0x00821EE8, 0x004D2CD0, 0x6C, 0xD9},
    {0, 0x00821EE8, 0x004D2CCC, 0x44, 0xEA}, {1, 0x00821EE8, 0x004D2CAC, 0x6C, 0xEA},
    {0, 0x00821EE8, 0x004C5C0C, 0x44, 0xFB}, {1, 0x00821EE8, 0x004D2C94, 0x6C, 0xFB},
    {0, 0x00821EE8, 0x004D2C8C, 0x44, 0x10C}, {1, 0x00821EE8, 0x004D2C74, 0x6C, 0x10C},
    {0, 0x00821EE8, 0x004D2C6C, 0x44, 0x11D}, {1, 0x00821EE8, 0x004D2C4C, 0x6C, 0x11D},
    {0, 0x00821EE8, 0x004D2C44, 0x44, 0x12E}, {1, 0x00821EE8, 0x004D2C28, 0x6C, 0x12E},
    {0, 0x00821EE8, 0x004D2C24, 0x44, 0x13F}, {1, 0x00821EE8, 0x004D2BFC, 0x6C, 0x13F},
    {0, 0x00821EE8, 0x004D2BF8, 0x44, 0x157}, {1, 0x00821EE8, 0x004D2BE8, 0x6C, 0x157},
    {0, 0x00821EE8, 0x004D2BE4, 0x1BC, 0x51}, {1, 0x00821EE8, 0x004D2BCC, 0x1E3, 0x51},
    {0, 0x00821EE8, 0x004D2BC8, 0x1BC, 0x62}, {1, 0x00821EE8, 0x004D2BB0, 0x1E3, 0x62},
    {0, 0x00821EE8, 0x004D2BAC, 0x1BC, 0x73}, {1, 0x00821EE8, 0x004D2B98, 0x1E3, 0x73},
    {0, 0x00821EE8, 0x004D2B94, 0x1BC, 0x84}, {1, 0x00821EE8, 0x004D2B7C, 0x1E3, 0x84},
    {0, 0x00821EE8, 0x004D2B78, 0x1BC, 0x95}, {1, 0x00821EE8, 0x004D2B64, 0x1E3, 0x95},
    {0, 0x00821EE8, 0x004D2B60, 0x1BC, 0xA6}, {1, 0x00821EE8, 0x004D2B4C, 0x1E3, 0xA6},
    {0, 0x00821EE8, 0x004D2B48, 0x1BC, 0xB7}, {1, 0x00821EE8, 0x004D2B34, 0x1E3, 0xB7},
    {0, 0x00821EE8, 0x004D2B30, 0x1BC, 0xC8}, {1, 0x00821EE8, 0x004D2B18, 0x1E3, 0xC8},
    {0, 0x00821EE8, 0x004D2B14, 0x1BC, 0xD9}, {1, 0x00821EE8, 0x004D2B04, 0x1E3, 0xD9},
    {0, 0x00821EE8, 0x004C61E4, 0x1BC, 0xEA}, {1, 0x00821EE8, 0x004D2AF0, 0x1E3, 0xEA},
    {0, 0x00821EE8, 0x004D2AE8, 0x1BC, 0xFB}, {1, 0x00821EE8, 0x004D2AD0, 0x1E3, 0xFB},
    {0, 0x00821EE8, 0x004D2AC8, 0x1BC, 0x10C}, {1, 0x00821EE8, 0x004D2AB4, 0x1E3, 0x10C},
    {0, 0x00821EE8, 0x004D2AAC, 0x1BC, 0x11D}, {1, 0x00821EE8, 0x004D2A94, 0x1E3, 0x11D},
    {0, 0x00821EE8, 0x004D2A90, 0x1BC, 0x12E}, {1, 0x00821EE8, 0x004D2A78, 0x1E3, 0x12E},
    {0, 0x00821EE8, 0x004C1430, 0x1BC, 0x13F}, {1, 0x00821EE8, 0x004D2A60, 0x1E3, 0x13F},
};

void DrawShortcut(const ShortcutText& t) {
    const char* s = reinterpret_cast<const char*>(t.text);
    if (s == 0) return;
    Tn5_t f = t.widget ? kWidgetDraw : kDrawStringAt;
    f(kScreen, 0, static_cast<int>(t.font), static_cast<int>(t.text), t.x, t.y, SLen(s));
}

const unsigned kSsImage = 0x000, kSsSurfA = 0x2C0, kSsSurfB = 0x2F0, kSsFrame = 0x320;

}  // namespace

// 0x0044e770  (NEGATIVE CONTROL)
int __cdecl NegShowShortcutsScreen_re() {
    const int neg = NegVariant();
    alignas(8) char frame[kSsFrame];
    void* image = frame + kSsImage;
    void* surfA = frame + kSsSurfA;
    void* surfB = frame + kSsSurfB;
    kImageCtor(image, 0);
    kSurfaceCtor(surfA, 0);
    kSurfaceCtor(surfB, 0);
    kSurfaceCreateFull(image, 0, 0x320, 0x258, 0, 1, 0, 0);
    kLoadImage(image, 0, 0x004D2DEC, 0, 0, 0x100, 1);
    kBlitTakeDirty(surfA, 0, reinterpret_cast<int>(image), 0, 0, 0x320, 0x199, 1, 0);
    kLoadImage(image, 0, 0x004D2DC0, 0, 0, 0x100, 1);
    // SG_NEG=2: the second take targets surfA again instead of surfB (its `this` token becomes ordinal(surfA))
    kBlitTakeDirty(neg == 2 ? surfA : surfB, 0, reinterpret_cast<int>(image), 0, 0, 0x320, 0x199, 1, 0);
    kImageReleaseAll(image, 0);
    kDrawSprite(reinterpret_cast<void*>(0x00821040), 0, kScreenI, 0, 0, 0x3E800000, 0, 1);
    // SG_NEG=1: `this` and the source argument are swapped (surfB.stretch(surfA))
    if (neg == 1)
        kStretchBlit(surfB, 0, reinterpret_cast<int>(surfA), kScreenI, 0, 0, 0);
    else
        kStretchBlit(surfA, 0, reinterpret_cast<int>(surfB), kScreenI, 0, 0, 0);
    kWidgetSetQuad(kScreen, 0, static_cast<int>(0x80000000), -1, 2, 2);
    for (unsigned i = 0; i < sizeof(kShortcutTop) / sizeof(kShortcutTop[0]); i++)
        DrawShortcut(kShortcutTop[i]);
    void* disp = G<void*>(0x0083AD50);
    int mode = vslot<T0_t>(disp, 0xB4)(disp, 0);
    int colour = mode == 1 ? static_cast<int>(0x80000400) : static_cast<int>(0x80000200);
    kWidgetSetQuad(kScreen, 0, colour, -1, 2, 2);
    DrawShortcut(kShortcutMid);
    kWidgetSetQuad(kScreen, 0, static_cast<int>(0x80000000), -1, 2, 2);
    for (unsigned i = 0; i < sizeof(kShortcutRest) / sizeof(kShortcutRest[0]); i++)
        DrawShortcut(kShortcutRest[i]);

    bool hot = false;
    bool again = true;
    kImageDraw(reinterpret_cast<void*>(0x00820F40), 0, kScreenI, 0x2C6, 0x16A, 0);
    kRefreshFull(kRootWindow, 0, 0);
    kWaitTicks(0x3C, 0);
    kEnterModal483d30();
    for (;;) {
        kWaitTicks(1, 0);
        kPumpMessages();
        int x, y;
        kViewToLocal(kRootWindow, 0, reinterpret_cast<int>(&x), reinterpret_cast<int>(&y));
        kRefreshFull(kRootWindow, 0, 0);
        if (x >= 0x2C6 && x < 0x2F2 && y >= 0x16A && y < 0x196) {
            if (!hot) {
                kImageDraw(reinterpret_cast<void*>(0x00820F6C), 0, kScreenI, 0x2C6, 0x16A, 0);
                hot = true;
                kRefreshFull(kRootWindow, 0, 0);
            }
            if (G<int>(0x00822D68) == 1)
                again = false;
        } else if (hot) {
            hot = false;
            kImageDraw(reinterpret_cast<void*>(0x00820F40), 0, kScreenI, 0x2C6, 0x16A, 0);
            kRefreshFull(kRootWindow, 0, 0);
        }
        if (kFlushRedraw() != 0) break;
        if (!again) break;
    }
    G<unsigned char>(0x00569498) = 0;
    kPumpLight();
    // SG_NEG=3: free the two Surfaces in the wrong order (surfB first, then surfA): the free records' `this`
    // tokens appear swapped relative to the original (which frees surfA, then surfB).
    void* f0 = neg == 3 ? surfB : surfA;
    void* f1 = neg == 3 ? surfA : surfB;
    kSurfaceFree(f0, 0);
    kSurfaceFree(f1, 0);
    At<unsigned>(f1, 0) = kSurfaceVtbl;
    kSurfaceFree(f1, 0);
    At<unsigned>(f0, 0) = kSurfaceVtbl;
    kSurfaceFree(f0, 0);
    return kImageDtor(image, 0);
}
C0_t NegShowShortcutsScreen_orig;
// 0x0044e770
SG_HOOK("golf_clean.exe", 0x0044e770, showShortcutsScreen, NegShowShortcutsScreen_re, NegShowShortcutsScreen_orig);
