// C3 batch c3es (round 32, 2026-10-13) of golf_clean.exe: the frame-mask pilot (re/frida/js/framemask.js) and the
// screens that keep their working objects in their own frame. Fixtures: re/frida/js/fixtures.d/c3es.js. Registry:
// re/frida/registry.d/c3es.py. Purposes and per-jcc branch tables: log/c3/c3es_purpose.md. Notes: log/c3/c3es_notes.txt.
//
// Every body is hand-written from the disassembly (listings in log/c3/alt/c3eo_decomp/ and log/c3/alt/c3ep_decomp/)
// and cites the instructions it reproduces. __thiscall is emulated with __fastcall (ecx = this, edx unused). Callees
// run through their original addresses.
//
// FRAME OBJECTS. The originals construct objects in their own frame and pass pointers to them to their callees. The
// reimplementations keep the same objects in a local buffer whose layout is chosen here (kOff* below) and differs from
// the original's on purpose: framemask.js logs a frame pointer by (object ordinal, offset), so the A/B compares which
// object each call targets without depending on where either frame puts it.
#include "hooks.h"

namespace {

template <typename T> static inline T& G(unsigned addr) { return *reinterpret_cast<T*>(addr); }
template <typename T> static inline T& At(void* p, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(p) + off); }

// virtual slot `off` of obj (thiscall, emulated with __fastcall)
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

// ---- callees (original addresses; a C3 hook at one of them runs through its detour) ----
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

// The screens compute their target surface as `0x00519a60 ? 0x00519cd4 : 0` (mov eax, 0x519a60; neg eax; sbb eax, eax;
// and eax, 0x519cd4): 0x00519a60 is a fixed data address, so the value is always 0x00519cd4.
void* const kScreen = reinterpret_cast<void*>(0x00519CD4);
void* const kRootWindow = reinterpret_cast<void*>(0x00519A60);
const int kScreenI = 0x00519CD4;
const unsigned kSurfaceVtbl = 0x004BA2D8;          // the vtable the bodies store before each second Surface_free

}  // namespace

// ====================================================================================================================
// showShortcutsScreen 0x0044e770 (__cdecl, no arguments; `ret` at 0x0044f6a9)
// ====================================================================================================================
// The keyboard-shortcuts screen. Frame objects of the original (esp after the prologue's two pushes, 0x0044e78d): an
// Image at [esp+0x6c] (Image_ctor 0x0044e791), Surfaces at [esp+0x40] (Surface_ctor 0x0044e7a5) and [esp+0x14]
// (0x0044e7b6); the cursor point at [esp+0x10] (x) / [esp+0xc] (y); the byte [esp+0xb] that keeps the loop going. The
// text table below is the 63 drawStringAt / Widget_draw calls 0x0044e8f2..0x0044f4f9, in order.
namespace {

struct ShortcutText {
    unsigned char widget;     // 0: drawStringAt 0x00476700, 1: Widget_draw 0x00476650
    unsigned font;            // the first stack argument (0x00821020 for the title, 0x00821ee8 for the rest)
    unsigned text;            // a fixed string address
    short x, y;
};

const ShortcutText kShortcutTop[] = {
    {0, 0x00821020, 0x004D2DAC, 0x11F, 0x1C},   // 0x0044e8f2
    {0, 0x00821EE8, 0x004D2DA8, 0x44, 0x51},    // 0x0044e920
    {1, 0x00821EE8, 0x004D2D90, 0x6C, 0x51},    // 0x0044e94a
    {0, 0x00821EE8, 0x004D2D8C, 0x44, 0x62},    // 0x0044e978
    {1, 0x00821EE8, 0x004D2D74, 0x6C, 0x62},    // 0x0044e9a6
    {0, 0x00821EE8, 0x004D2D70, 0x44, 0x73},    // 0x0044e9d0
    {1, 0x00821EE8, 0x004D2D64, 0x6C, 0x73},    // 0x0044e9fe
    {0, 0x00821EE8, 0x004D2D60, 0x44, 0x84},    // 0x0044ea2f
    {1, 0x00821EE8, 0x004D2D4C, 0x6C, 0x84},    // 0x0044ea5c
    {0, 0x00821EE8, 0x004D2D48, 0x44, 0x95},    // 0x0044ea8d
    {1, 0x00821EE8, 0x004D2D30, 0x6C, 0x95},    // 0x0044eabe
    {0, 0x00821EE8, 0x004D2D2C, 0x44, 0xA6},    // 0x0044eaeb
    {1, 0x00821EE8, 0x004D2D20, 0x6C, 0xA6},    // 0x0044eb1c
    {0, 0x00821EE8, 0x004D2D1C, 0x44, 0xB7},    // 0x0044eb4d
    {1, 0x00821EE8, 0x004D2D0C, 0x6C, 0xB7},    // 0x0044eb7a
    {0, 0x00821EE8, 0x004D2D08, 0x44, 0xC8},    // 0x0044ebab
};
// drawn between the two Widget_setQuad calls 0x0044ebd9 / 0x0044ec1f, in the colour the display's slot +0xb4 picks
const ShortcutText kShortcutMid = {1, 0x00821EE8, 0x004D2CE8, 0x6C, 0xC8};   // 0x0044ec0a
const ShortcutText kShortcutRest[] = {
    {0, 0x00821EE8, 0x004D2CE4, 0x44, 0xD9},    // 0x0044ec50
    {1, 0x00821EE8, 0x004D2CD0, 0x6C, 0xD9},    // 0x0044ec7d
    {0, 0x00821EE8, 0x004D2CCC, 0x44, 0xEA},    // 0x0044ecae
    {1, 0x00821EE8, 0x004D2CAC, 0x6C, 0xEA},    // 0x0044ecdf
    {0, 0x00821EE8, 0x004C5C0C, 0x44, 0xFB},    // 0x0044ed0c
    {1, 0x00821EE8, 0x004D2C94, 0x6C, 0xFB},    // 0x0044ed3d
    {0, 0x00821EE8, 0x004D2C8C, 0x44, 0x10C},   // 0x0044ed6e
    {1, 0x00821EE8, 0x004D2C74, 0x6C, 0x10C},   // 0x0044ed9b
    {0, 0x00821EE8, 0x004D2C6C, 0x44, 0x11D},   // 0x0044edcc
    {1, 0x00821EE8, 0x004D2C4C, 0x6C, 0x11D},   // 0x0044edfd
    {0, 0x00821EE8, 0x004D2C44, 0x44, 0x12E},   // 0x0044ee2a
    {1, 0x00821EE8, 0x004D2C28, 0x6C, 0x12E},   // 0x0044ee5b
    {0, 0x00821EE8, 0x004D2C24, 0x44, 0x13F},   // 0x0044ee8c
    {1, 0x00821EE8, 0x004D2BFC, 0x6C, 0x13F},   // 0x0044eeb9
    {0, 0x00821EE8, 0x004D2BF8, 0x44, 0x157},   // 0x0044eeea
    {1, 0x00821EE8, 0x004D2BE8, 0x6C, 0x157},   // 0x0044ef1b
    {0, 0x00821EE8, 0x004D2BE4, 0x1BC, 0x51},   // 0x0044ef48
    {1, 0x00821EE8, 0x004D2BCC, 0x1E3, 0x51},   // 0x0044ef79
    {0, 0x00821EE8, 0x004D2BC8, 0x1BC, 0x62},   // 0x0044efaa
    {1, 0x00821EE8, 0x004D2BB0, 0x1E3, 0x62},   // 0x0044efd7
    {0, 0x00821EE8, 0x004D2BAC, 0x1BC, 0x73},   // 0x0044f008
    {1, 0x00821EE8, 0x004D2B98, 0x1E3, 0x73},   // 0x0044f039
    {0, 0x00821EE8, 0x004D2B94, 0x1BC, 0x84},   // 0x0044f069
    {1, 0x00821EE8, 0x004D2B7C, 0x1E3, 0x84},   // 0x0044f09d
    {0, 0x00821EE8, 0x004D2B78, 0x1BC, 0x95},   // 0x0044f0d1
    {1, 0x00821EE8, 0x004D2B64, 0x1E3, 0x95},   // 0x0044f101
    {0, 0x00821EE8, 0x004D2B60, 0x1BC, 0xA6},   // 0x0044f135
    {1, 0x00821EE8, 0x004D2B4C, 0x1E3, 0xA6},   // 0x0044f169
    {0, 0x00821EE8, 0x004D2B48, 0x1BC, 0xB7},   // 0x0044f199
    {1, 0x00821EE8, 0x004D2B34, 0x1E3, 0xB7},   // 0x0044f1cd
    {0, 0x00821EE8, 0x004D2B30, 0x1BC, 0xC8},   // 0x0044f201
    {1, 0x00821EE8, 0x004D2B18, 0x1E3, 0xC8},   // 0x0044f231
    {0, 0x00821EE8, 0x004D2B14, 0x1BC, 0xD9},   // 0x0044f265
    {1, 0x00821EE8, 0x004D2B04, 0x1E3, 0xD9},   // 0x0044f299
    {0, 0x00821EE8, 0x004C61E4, 0x1BC, 0xEA},   // 0x0044f2c9
    {1, 0x00821EE8, 0x004D2AF0, 0x1E3, 0xEA},   // 0x0044f2fd
    {0, 0x00821EE8, 0x004D2AE8, 0x1BC, 0xFB},   // 0x0044f331
    {1, 0x00821EE8, 0x004D2AD0, 0x1E3, 0xFB},   // 0x0044f361
    {0, 0x00821EE8, 0x004D2AC8, 0x1BC, 0x10C},  // 0x0044f395
    {1, 0x00821EE8, 0x004D2AB4, 0x1E3, 0x10C},  // 0x0044f3c9
    {0, 0x00821EE8, 0x004D2AAC, 0x1BC, 0x11D},  // 0x0044f3f9
    {1, 0x00821EE8, 0x004D2A94, 0x1E3, 0x11D},  // 0x0044f42d
    {0, 0x00821EE8, 0x004D2A90, 0x1BC, 0x12E},  // 0x0044f461
    {1, 0x00821EE8, 0x004D2A78, 0x1E3, 0x12E},  // 0x0044f491
    {0, 0x00821EE8, 0x004C1430, 0x1BC, 0x13F},  // 0x0044f4c5
    {1, 0x00821EE8, 0x004D2A60, 0x1E3, 0x13F},  // 0x0044f4f9
};

// each entry is guarded by `test reg, reg; je` on its (constant, non-NULL) string address and passes strlen as the
// last argument (`repne scasb; not ecx; dec ecx`, e.g. 0x0044e8cb..0x0044e8db)
void DrawShortcut(const ShortcutText& t) {
    const char* s = reinterpret_cast<const char*>(t.text);
    if (s == 0) return;
    Tn5_t f = t.widget ? kWidgetDraw : kDrawStringAt;
    f(kScreen, 0, static_cast<int>(t.font), static_cast<int>(t.text), t.x, t.y, SLen(s));
}

// The local objects: offsets inside the reimplementation's frame buffer (an Image of 0x2b8 bytes, the class size
// uitree.js uses; two Surfaces of 0x28 bytes, Surface_ctor's last store being +0x24, 0x00473ad2).
const unsigned kSsImage = 0x000, kSsSurfA = 0x2C0, kSsSurfB = 0x2F0, kSsFrame = 0x320;

}  // namespace

// 0x0044e770
int __cdecl ShowShortcutsScreen_re() {
    alignas(8) char frame[kSsFrame];
    void* image = frame + kSsImage;     // the original's [esp+0x6c]
    void* surfA = frame + kSsSurfA;     // [esp+0x40]
    void* surfB = frame + kSsSurfB;     // [esp+0x14]
    kImageCtor(image, 0);                                                            // 0x0044e791
    kSurfaceCtor(surfA, 0);                                                          // 0x0044e7a5
    kSurfaceCtor(surfB, 0);                                                          // 0x0044e7b6
    kSurfaceCreateFull(image, 0, 0x320, 0x258, 0, 1, 0, 0);                          // 0x0044e7bb..0x0044e7dc
    kLoadImage(image, 0, 0x004D2DEC, 0, 0, 0x100, 1);                                // 0x0044e7e1..0x0044e7f8
    kBlitTakeDirty(surfA, 0, reinterpret_cast<int>(image), 0, 0, 0x320, 0x199, 1, 0);   // 0x0044e7fd..0x0044e81b
    kLoadImage(image, 0, 0x004D2DC0, 0, 0, 0x100, 1);                                // 0x0044e820..0x0044e837
    kBlitTakeDirty(surfB, 0, reinterpret_cast<int>(image), 0, 0, 0x320, 0x199, 1, 0);   // 0x0044e83c..0x0044e85a
    kImageReleaseAll(image, 0);                                                      // 0x0044e863
    kDrawSprite(reinterpret_cast<void*>(0x00821040), 0, kScreenI, 0, 0, 0x3E800000, 0, 1);   // 0x0044e868..0x0044e889
    kStretchBlit(surfA, 0, reinterpret_cast<int>(surfB), kScreenI, 0, 0, 0);         // 0x0044e88e..0x0044e8ac
    kWidgetSetQuad(kScreen, 0, static_cast<int>(0x80000000), -1, 2, 2);              // 0x0044e8b1..0x0044e8c1
    for (unsigned i = 0; i < sizeof(kShortcutTop) / sizeof(kShortcutTop[0]); i++)
        DrawShortcut(kShortcutTop[i]);
    // 0x0044ebb0..0x0044ebd9: the colour of the next line from the display's slot +0xb4: 1 -> 0x80000400, any other
    // value -> 0x80000200 (dec eax; neg eax; sbb eax, eax; and eax, 0xfffffe00; add eax, 0x80000400)
    void* disp = G<void*>(0x0083AD50);
    int mode = vslot<T0_t>(disp, 0xB4)(disp, 0);
    int colour = mode == 1 ? static_cast<int>(0x80000400) : static_cast<int>(0x80000200);
    kWidgetSetQuad(kScreen, 0, colour, -1, 2, 2);
    DrawShortcut(kShortcutMid);                                                      // 0x0044ec0a
    kWidgetSetQuad(kScreen, 0, static_cast<int>(0x80000000), -1, 2, 2);              // 0x0044ec0f..0x0044ec1f
    for (unsigned i = 0; i < sizeof(kShortcutRest) / sizeof(kShortcutRest[0]); i++)
        DrawShortcut(kShortcutRest[i]);

    // 0x0044f4fe..0x0044f53f: the close button in its normal state, a refresh, a 0x3c-tick wait
    bool hot = false;                                                                // bl (0x0044f503)
    bool again = true;                                                               // [esp+0xb] = 1 (0x0044f520)
    kImageDraw(reinterpret_cast<void*>(0x00820F40), 0, kScreenI, 0x2C6, 0x16A, 0);   // 0x0044f525
    kRefreshFull(kRootWindow, 0, 0);                                                 // 0x0044f531
    kWaitTicks(0x3C, 0);                                                             // 0x0044f53a
    kEnterModal483d30();                                                             // 0x0044f542 (once, before the loop:
                                                                                     // the back-edge at 0x0044f629 jumps
                                                                                     // to 0x0044f547, past this call)
    for (;;) {
        kWaitTicks(1, 0);                                                            // 0x0044f547..0x0044f54b
        kPumpMessages();                                                             // 0x0044f553
        int x, y;
        kViewToLocal(kRootWindow, 0, reinterpret_cast<int>(&x), reinterpret_cast<int>(&y));   // 0x0044f558..0x0044f567
        kRefreshFull(kRootWindow, 0, 0);                                             // 0x0044f573
        // 0x0044f578..0x0044f59a: the button's box, x in [0x2c6, 0x2f2), y in [0x16a, 0x196), signed
        if (x >= 0x2C6 && x < 0x2F2 && y >= 0x16A && y < 0x196) {
            if (!hot) {                                                              // 0x0044f59c
                kImageDraw(reinterpret_cast<void*>(0x00820F6C), 0, kScreenI, 0x2C6, 0x16A, 0);   // 0x0044f5c2
                hot = true;                                                          // 0x0044f5c0
                kRefreshFull(kRootWindow, 0, 0);                                     // 0x0044f5ce
            }
            if (G<int>(0x00822D68) == 1)                                             // 0x0044f5d3
                again = false;                                                       // 0x0044f5dc
        } else if (hot) {                                                            // 0x0044f5e3
            hot = false;                                                             // 0x0044f5ec
            kImageDraw(reinterpret_cast<void*>(0x00820F40), 0, kScreenI, 0x2C6, 0x16A, 0);   // 0x0044f609
            kRefreshFull(kRootWindow, 0, 0);                                         // 0x0044f615
        }
        if (kFlushRedraw() != 0) break;                                              // 0x0044f61a / 0x0044f621
        if (!again) break;                                                           // 0x0044f623..0x0044f629
    }
    G<unsigned char>(0x00569498) = 0;                                                // 0x0044f62f
    kPumpLight();                                                                    // 0x0044f636
    kSurfaceFree(surfA, 0);                                                          // 0x0044f63f
    kSurfaceFree(surfB, 0);                                                          // 0x0044f648
    At<unsigned>(surfB, 0) = kSurfaceVtbl;                                           // 0x0044f659
    kSurfaceFree(surfB, 0);                                                          // 0x0044f661
    At<unsigned>(surfA, 0) = kSurfaceVtbl;                                           // 0x0044f672
    kSurfaceFree(surfA, 0);                                                          // 0x0044f67a
    return kImageDtor(image, 0);                                                     // 0x0044f68e: its eax is returned
}
C0_t ShowShortcutsScreen_orig;
// 0x0044e770
SG_HOOK("golf_clean.exe", 0x0044e770, showShortcutsScreen, ShowShortcutsScreen_re, ShowShortcutsScreen_orig);

// ====================================================================================================================
// showThemePackSelect 0x004725b0 (__cdecl, no arguments; `ret` at 0x004732ca)  --  DROPPED (round 32), NOT HOOKED.
// ====================================================================================================================
// Lists the theme packs, probes six companion files per theme (FindFirstFileA), builds two Surface arrays (the vector
// constructor iterator ??_L, ordinals 0 and 1 to the frame mask) and an Image (ordinal 2), draws a grid of thumbnails,
// then runs a hover/click modal loop. The frame mask (framemask.js) carries which array element each stood-in draw takes.
//
// This reimplementation is a complete, verified-by-eye transcription kept for a future round, but it is compiled out
// (`#if 0`) and NOT registered, because its A/B cannot be run: showThemePackSelect's ORIGINAL arm cannot be executed
// under the frame-mask fixture. With the vector constructor iterator ??_L 0x004a55d2 stubbed (so the two Surface arrays
// are never really constructed), the function runs to completion but its SEH registration record's saved-fs:[0] field
// (pushed 0xffffffff at entry, 0x004725be) is overwritten with a stack pointer by the time of the unlink (0x004732b2);
// on return it restores fs:[0] to a bogus in-stack record whose handler field is garbage (0x10005), so the next
// exception during teardown dispatches there and the process dies (confirmed 2026-10-13, log/c3/c3es_notes.txt section
// 5). Running the original safely needs ??_L to really construct the Surfaces, which needs the live display slot +0x7c
// and the heap and reintroduces exactly the frame-layout dependence the mask exists to remove. The four
// structurally-identical SEH+array browser screens (pickAProScreen, courseActionMenu, loadGameBrowserScreen,
// showCharacterEditor) share this blocker.
#if 0
namespace {
typedef int(__cdecl* VecCtor_t)(void* base, int size, int count, void* ctor, void* dtor);
typedef int(__cdecl* VecDtor_t)(void* base, int size, int count, void* dtor);
const C0_t kListThemes = reinterpret_cast<C0_t>(0x004724C0);
const VecCtor_t kVecCtor = reinterpret_cast<VecCtor_t>(0x004A55D2);   // ??_L(base, size, count, ctor, dtor)
const VecDtor_t kVecDtor = reinterpret_cast<VecDtor_t>(0x004A5713);   // ??_M(base, size, count, dtor)
const T0_t kImageCtorT = reinterpret_cast<T0_t>(0x00474AE0);
const Tn7_t kSurfaceBlit = reinterpret_cast<Tn7_t>(0x00473BF0);
const Tn7_t kBlitThunk = reinterpret_cast<Tn7_t>(0x00475C60);
typedef void(__cdecl* V4c_t)(int, int, int, int);
typedef void(__cdecl* V5c_t)(int, int, int, int, int);
const V4c_t kDrawText4049 = reinterpret_cast<V4c_t>(0x004049D0);
const V4c_t kDrawCentered = reinterpret_cast<V4c_t>(0x00404B70);
const V4c_t kDrawCentered2 = reinterpret_cast<V4c_t>(0x00404BC0);
const V5c_t kBlitPanel = reinterpret_cast<V5c_t>(0x0040CA10);
const Tn4_t kImageDrawT = reinterpret_cast<Tn4_t>(0x00473E60);
const Tn7_t kDrawScaled = reinterpret_cast<Tn7_t>(0x00473CB0);
const T0_t kImageDtorT = reinterpret_cast<T0_t>(0x00474C40);
const Tn1_t kRefreshFullT = reinterpret_cast<Tn1_t>(0x00480C80);
const Tn2_t kViewToLocalT = reinterpret_cast<Tn2_t>(0x0047AB50);
const C2_t kApproxDistance = reinterpret_cast<C2_t>(0x00467170);
const C2_t kWaitTicks2 = reinterpret_cast<C2_t>(0x0045BF80);
const C0_t kPumpMessagesT = reinterpret_cast<C0_t>(0x0045C030);
const C0_t kFlushRedrawT = reinterpret_cast<C0_t>(0x0045AE70);
const C0_t kEnterModalT = reinterpret_cast<C0_t>(0x00483D30);
const Tn4_t kSetDrawTargetT = reinterpret_cast<Tn4_t>(0x004762D0);

// FindFirstFileA through the exe's IAT slot 0x004ba090 (`mov ebx,[0x4ba090]; call ebx`, 0x0047260b)
typedef unsigned(__stdcall* FindFirst_t)(const char*, void*);
static inline unsigned FindFirstFileSlot(const char* path, void* data) {
    return (*reinterpret_cast<FindFirst_t*>(0x004BA090))(path, data);
}

char* const kThemeNames = reinterpret_cast<char*>(0x0056E928);     // theme names, stride 0x32
const unsigned kThemeStride = 0x32;
char* const kCurTheme = reinterpret_cast<char*>(0x00567328);       // the selected theme name
const int kScreenPtr = 0x004C1570;                                 // [0x4c1570]: the surface the draws pass as `this`

const char* const kPrefix = reinterpret_cast<const char*>(0x004C84E8);
const char* const kSuffix[6] = {
    reinterpret_cast<const char*>(0x004E18E4), reinterpret_cast<const char*>(0x004C856C),
    reinterpret_cast<const char*>(0x004E18AC), reinterpret_cast<const char*>(0x004E1868),
    reinterpret_cast<const char*>(0x004E4140), reinterpret_cast<const char*>(0x004E4130) };
const unsigned char kSuffixBit[6] = { 1, 2, 4, 8, 0x20, 0x40 };    // 0x4726b7, 0x472754, 0x4727f1, 0x47288e, 0x47292b, 0x4729c8

static void SConcat3(char* d, const char* a, const char* b, const char* c) {
    int n = 0; while (a[n]) { d[n] = a[n]; n++; } int m = 0; while (b[m]) { d[n + m] = b[m]; m++; } d += n + m;
    int k = 0; while (c[k]) { d[k] = c[k]; k++; } d[k] = 0;
}
static int SCmpExe(const char* a, const char* b) { int i = 0; while (a[i] == b[i]) { if (!a[i]) return 0; i++; } return 1; }
static void SCopy(char* d, const char* s) { int i = 0; while (s[i]) { d[i] = s[i]; i++; } d[i] = 0; }
}  // namespace

// 0x004725b0
int __cdecl ShowThemePackSelect_re() {
    char findData[0x140];
    char path[0x200];
    unsigned char flags[64];
    // 0x004725cf..0x004725eb: five column words; the sixth (ebx==5 at 0x472eb2) reads the next slot, 0 in the original.
    short colX[6] = { 0x15a, 0x1b4, 0x20e, 0x268, 0x2c2, 0 };
    // the hover tables set before ??_L (0x00472a17 / 0x004729ff): X = [esp+0x34], Y = [esp+0x48].
    const int tableX[4] = { 0x208, 0x215, 0x51, 0x1d7 };
    const int tableY[4] = { 0x245, 0x2d6, 0x301, 0x301 };
    int count = kListThemes();                                     // 0x004725f2
    if (count > 64) count = 64;
    // 0x00472616..0x004729de: six FindFirstFileA probes per theme, one flag bit each (no FindClose)
    for (int i = 0; i < count; i++) {                              // 0x00472605 jle skips when count <= 0
        flags[i] = 0;                                              // 0x00472626
        const char* nm = kThemeNames + kThemeStride * i;
        for (int s = 0; s < 6; s++) {
            SConcat3(path, kPrefix, nm, kSuffix[s]);
            if (FindFirstFileSlot(path, findData) != 0xFFFFFFFFu)
                flags[i] |= kSuffixBit[s];
        }
    }

    char arrayA[8 * 0x2c];     // ordinal 0 (??_L #1)
    char arrayB[4 * 0x2c];     // ordinal 1 (??_L #2)
    char image[0x2b8];         // ordinal 2 (Image_ctor)
    kVecCtor(arrayA, 0x2c, 8, reinterpret_cast<void*>(0x00473AB0), reinterpret_cast<void*>(0x004041F0));   // 0x004729e4
    kVecCtor(arrayB, 0x2c, 4, reinterpret_cast<void*>(0x00473AB0), reinterpret_cast<void*>(0x004041F0));   // 0x00472a3c
    kImageCtorT(image, 0);                                                                                 // 0x00472a6f
    kLoadImage(image, 0, 0x004E4110, 0, 0, 0x100, 2);                                                      // 0x00472a91
    static const struct { int x, y, w, h; } kBlitA[8] = {
        {1, 1, 0x57, 0x1f}, {0x59, 1, 0x57, 0x1f}, {0xb1, 1, 0x57, 0x1f}, {1, 0x21, 0xdd, 0x1f},
        {0x109, 1, 0x47, 0x50}, {0x151, 1, 0x30, 0x31}, {0x182, 1, 0x13, 0x23}, {0x196, 1, 0x13, 0x23} };
    for (int k = 0; k < 8; k++)
        kSurfaceBlit(arrayA + 0x2c * k, 0, reinterpret_cast<int>(image), kBlitA[k].x, kBlitA[k].y, kBlitA[k].w,
                     kBlitA[k].h, 1, 1);                                                                   // 0x00472a96..0x00472ba3
    kLoadImage(image, 0, 0x004E40F4, 0, 0, 0x100, 2);                                                      // 0x00472ba8
    static const struct { int x, y, w, h; } kBlitB[4] = {
        {0x245, 1, 0x47, 0x50}, {0x2d6, 0x215, 0x30, 0x31}, {0x301, 0x51, 0x13, 0x23}, {0x301, 0x1d7, 0x13, 0x23} };
    for (int k = 0; k < 4; k++)
        kSurfaceBlit(arrayB + 0x2c * k, 0, reinterpret_cast<int>(image), kBlitB[k].x, kBlitB[k].y, kBlitB[k].w,
                     kBlitB[k].h, 1, 1);                                                                   // 0x00472bc2..0x00472c4a
    kEnterModalT();                                                                                        // 0x00472c4f
    G<int>(0x00822D68) = 0;                                                                                // 0x00472c54

    int selected = -1;         // [esp+0x24]
    int prevHover = -1;        // ebp (0x00472ee6)
    int frames = 0;            // ebx (0x00472ee9)

    for (;;) {                  // the redraw target (0x00472c5a)
        kSetDrawTargetT(reinterpret_cast<void*>(G<int>(kScreenPtr)), 0, 0x0051A028, 0, 0, 0);                 // 0x00472c6b
        kBlitThunk(image, 0, reinterpret_cast<int>(kScreen), 0, 0, 0, 0, 0x320, 0x258);                    // 0x00472c98
        kDrawText4049(0x004E40E0, 0x4e, 0x22, static_cast<int>(0x80000000));                               // 0x00472cab
        kSetDrawTargetT(reinterpret_cast<void*>(G<int>(kScreenPtr)), 0, 0x0051B360, 0, 0, 0);                 // 0x00472cc4
        kDrawCentered(0x004E40D8, 0x15a, 0x3e, static_cast<int>(0x80000000));                              // 0x00472cda
        kDrawCentered(0x004E40CC, 0x1b4, 0x25, static_cast<int>(0x80000000));                              // 0x00472cf0
        kDrawCentered(0x004E40C0, 0x20e, 0x3e, static_cast<int>(0x80000000));                              // 0x00472d06
        kDrawCentered(0x004E40B4, 0x268, 0x25, static_cast<int>(0x80000000));                              // 0x00472d1c
        kDrawCentered(0x004E40AC, 0x2c2, 0x3e, static_cast<int>(0x80000000));                              // 0x00472d35
        kSetDrawTargetT(reinterpret_cast<void*>(G<int>(kScreenPtr)), 0, 0x0051A028, 0, 0, 0);                 // 0x00472d4e
        selected = -1;                                                                                     // 0x00472d57
        int rowY = 0x5c;                                                                                   // 0x00472d6f
        for (int i = 0; i < count; i++) {                                                                  // 0x00472d69 / 0x00472ed4
            const char* nm = kThemeNames + kThemeStride * i;
            SCopy(reinterpret_cast<char*>(0x0051A068), nm);                                                // 0x00472d7c
            int sel = (SCmpExe(reinterpret_cast<char*>(0x0051A068), kCurTheme) == 0);                      // 0x00472daa
            if (sel) {                                                                                     // 0x00472dd9
                kImageDrawT(arrayB, 0, G<int>(kScreenPtr), 0x48, rowY, 0);                                 // 0x00472dec ([esp+0x33c])
                kDrawText4049(0x0051A068, 0x49, rowY + 0xb, static_cast<int>(0x80000000));                 // 0x00472e01
                selected = reinterpret_cast<int>(nm);                                                      // 0x00472e12
            } else {
                kDrawText4049(0x0051A068, 0x49, rowY + 0xb, static_cast<int>(0x80004210));                 // 0x00472e28
            }
            for (int b = 0; b < 6; b++) {                                                                  // 0x00472eb2
                int x = static_cast<int>(colX[b]) - 0x2b;                                                  // 0x00472e59 / 0x00472e62
                if (flags[i] & (1u << b))                                                                  // 0x00472e55
                    kDrawScaled(arrayA + 0x2c * (sel ? 5 : 6), 0, G<int>(kScreenPtr), x, rowY, 1, 1, 1, 0);// 0x00472e7f
                else if (sel)                                                                              // 0x00472e88
                    kDrawScaled(arrayA + 0x2c * 4, 0, G<int>(kScreenPtr), x, rowY, 1, 1, 1, 0);            // 0x00472ea2
            }
            rowY += 0x20;                                                                                  // 0x00472ec3
        }
        kRefreshFullT(kRootWindow, 0, 0);                                                                  // 0x00472edc

        bool backToRedraw = false, done = false;
        for (;;) {                                                                                         // 0x00472eeb
            kWaitTicks2(2, 0);                                                                             // 0x00472eef
            kPumpMessagesT();                                                                              // 0x00472ef7
            int lx, ly;
            kViewToLocalT(kRootWindow, 0, reinterpret_cast<int>(&lx), reinterpret_cast<int>(&ly));         // 0x00472f06
            int prev = prevHover;                                                                          // 0x00472f24
            int hover = -1;                                                                                // 0x00472f27
            if (kApproxDistance(ly - 0x267, lx - 0x22b) < 0x1a) hover = 0;                                 // 0x00472f2a
            if (kApproxDistance(ly - 0x2ed, lx - 0x22c) < 0x18) hover = 1;                                 // 0x00472f4e
            if (kApproxDistance(ly - 0x30a, lx - 0x63) < 0x14) hover = 2;                                  // 0x00472f73
            if (kApproxDistance(ly - 0x30a, lx - 0x1e7) < 0x14) hover = 3;                                 // 0x00472f9a
            prevHover = hover;
            if (hover == prev) {                                                                           // 0x00472fac
                frames++;                                                                                  // 0x00473018
                if (frames > 0x1e && hover != -1) {                                                        // 0x00473019 / 0x00473022
                    kSetDrawTargetT(reinterpret_cast<void*>(G<int>(kScreenPtr)), 0, 0x0051B360, 0, 0, 0);     // 0x0047302b
                    char* text = reinterpret_cast<char*>(0x0051A068);
                    text[0] = 0;                                                                           // 0x00473043
                    if (hover == 0) SCopy(text, reinterpret_cast<const char*>(selected == -1 ? 0x004E9A84 : 0x004C87E4));   // 0x00473074
                    else if (hover == 1) SCopy(text, reinterpret_cast<const char*>(0x004C8590));           // 0x00473052
                    if (frames == 0x1f)                                                                    // 0x004730a7
                        kBlitPanel(tableY[hover], tableX[hover] + 0xe, 0x30, 0x10, 0);                     // 0x004730bf
                    kDrawCentered2(tableX[hover] + 0x10, tableY[hover] + 0x18, 0x0051A068, static_cast<int>(0x80007FFF));   // 0x004730db
                    kRefreshFullT(kRootWindow, 0, 0);                                                      // 0x004730ee
                }
            } else {                                                                                       // 0x00472fb0
                if (prev != -1)
                    kImageDrawT(arrayB + 0x2c * prev, 0, G<int>(kScreenPtr), tableY[prev], tableX[prev], 0);  // 0x00472fd5
                if (hover != -1)
                    kImageDrawT(arrayA + 0x2c * hover, 0, G<int>(kScreenPtr), tableY[hover], tableX[hover], 0);// 0x00473000
                kRefreshFullT(kRootWindow, 0, 0);                                                          // 0x0047300c
                frames = 0;                                                                                // 0x00473011
            }
            // exit test (0x004730f5)
            int btn = G<int>(0x00822D68);
            bool dispatch;                            // reached 0x473120 (the ebp dispatch) directly
            if (btn != 0) {
                dispatch = false;                     // 0x4730fa jne 0x47311b
            } else {
                if (kFlushRedrawT() == 0) continue;   // 0x4730fe je 0x472eeb
                btn = G<int>(0x00822D68);             // 0x0047310b
                if (btn != 0) dispatch = false;       // 0x473112 jne 0x47311b
                else { prevHover = 1; dispatch = true; }   // 0x473114 ebp=1; jmp 0x473120
            }
            if (!dispatch) {                          // 0x0047311b
                if (prevHover == -1) {                // je 0x47313b
                    if (btn == 0) { done = true; break; }       // 0x47325f (default exit, no copy)
                    // 0x00473143: select by click y. idx = (y - 0x5b) / 32 signed (cdq; and edx,0x1f; add; sar 5)
                    int d = G<int>(0x00822D64) - 0x5b;          // 0x00473149
                    int idx = (d + ((d >> 31) & 0x1f)) >> 5;    // 0x0047314c..0x00473152
                    if (idx < 0 || idx >= count) { done = true; break; }   // 0x00473155 js / 0x0047315b jge -> 0x47325f
                    SCopy(kCurTheme, kThemeNames + kThemeStride * idx);     // 0x00473165
                    backToRedraw = true; break;                 // 0x00473192 (jmp 0x472c5a)
                }
                dispatch = true;                      // fall to 0x473120
            }
            // 0x00473120 dispatch on prevHover
            G<int>(0x00822D68) = 0;                   // 0x00473123
            if (prevHover == 0) { done = true; break; }         // 0x0047320f
            if (prevHover == 1) { SCopy(kCurTheme, reinterpret_cast<const char*>(0x004C6C10)); done = true; break; }  // 0x00473197
            backToRedraw = true; break;               // ebp 2/3 -> 0x472c5a
        }
        if (backToRedraw) continue;
        if (done) break;
    }
    kImageDtorT(image, 0);                                                                                 // 0x004731cb / 0x0047321e / 0x0047326e
    kVecDtor(arrayB, 0x2c, 4, reinterpret_cast<void*>(0x004041F0));                                        // 0x004731e9
    return kVecDtor(arrayA, 0x2c, 8, reinterpret_cast<void*>(0x004041F0));                                 // 0x004732ad: its eax is returned
}
C0_t ShowThemePackSelect_orig;
// 0x004725b0 (NOT hooked: dropped, see the #if 0 banner above)
// SG_HOOK("golf_clean.exe", 0x004725b0, showThemePackSelect, ShowThemePackSelect_re, ShowThemePackSelect_orig);
#endif  // showThemePackSelect dropped
