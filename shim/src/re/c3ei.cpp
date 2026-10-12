// C3 batch c3ei (round 30, 2026-10-12) of golf_clean.exe: image consumers (the asset loaders loadPcxSurface / loadFonts /
// loadCursors, the sprite queue addSprite / drawQueuedSprite, the golfer sprite compositor 0x00462020 and the toolbar /
// panel / window drawing bodies built on Image_draw and Surface_drawScaled). Fixtures: re/frida/js/fixtures.d/c3ei.js (built
// from log/c3/c3ei_parts). Registry: re/frida/registry.d/c3ei.py. Purposes and per-jcc tables: log/c3/c3ei_purpose.md.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list; copies in
// log/c3/c3ei_asm) and cites the instructions it reproduces. __thiscall is emulated with __fastcall (ecx = this, edx unused).
// Callees run through their original addresses; the display and the sprite objects are called through their own vtables.
#include "hooks.h"
#include <string.h>

namespace {

template <typename T> static inline T& G(unsigned addr) { return *reinterpret_cast<T*>(addr); }
static inline char* B(void* p) { return reinterpret_cast<char*>(p); }
template <typename T> static inline T& At(void* p, unsigned off) { return *reinterpret_cast<T*>(B(p) + off); }
static inline int& I(void* base, int off) { return *reinterpret_cast<int*>(B(base) + off); }
static inline int add(int a, int b) { return static_cast<int>(static_cast<unsigned>(a) + static_cast<unsigned>(b)); }
static inline int sub(int a, int b) { return static_cast<int>(static_cast<unsigned>(a) - static_cast<unsigned>(b)); }
static inline int mul(int a, int b) { return static_cast<int>(static_cast<unsigned>(a) * static_cast<unsigned>(b)); }

typedef int(__cdecl* C0_t)();
typedef int(__cdecl* C1_t)(int);
typedef int(__cdecl* C2_t)(int, int);
typedef int(__cdecl* C3_t)(int, int, int);
typedef int(__cdecl* C4_t)(int, int, int, int);
typedef int(__cdecl* C5_t)(int, int, int, int, int);
typedef int(__cdecl* C6_t)(int, int, int, int, int, int);
typedef int(__cdecl* C8_t)(int, int, int, int, int, int, int, int);
typedef int(__fastcall* T0_t)(void* ecx, void* edx);
typedef int(__fastcall* T1_t)(void* ecx, void* edx, int a);
typedef int(__fastcall* T2_t)(void* ecx, void* edx, int a, int b);
typedef int(__fastcall* T3_t)(void* ecx, void* edx, int a, int b, int c);
typedef int(__fastcall* T4_t)(void* ecx, void* edx, int a, int b, int c, int d);
typedef int(__fastcall* T5_t)(void* ecx, void* edx, int a, int b, int c, int d, int e);
typedef int(__fastcall* T6_t)(void* ecx, void* edx, int a, int b, int c, int d, int e, int f);
typedef int(__fastcall* T7_t)(void* ecx, void* edx, int a, int b, int c, int d, int e, int f, int g);

// Image 0x00474ae0 (constructor) / 0x00474c40 (destructor): `this` in ecx; the local Image object of the loaders is 0x2b8 bytes
// (frame `sub esp, 0x2b8`, object at [esp + 4] after the push of esi, 0x0049d1cc).
const T0_t kImageCtor = reinterpret_cast<T0_t>(0x00474ae0);
const T0_t kImageDtor = reinterpret_cast<T0_t>(0x00474c40);
const T5_t kLoadImage = reinterpret_cast<T5_t>(0x00475840);           // loadImage(this, name, a2, a3, a4, a5), ret 0x14
const T7_t kSurfaceBlit = reinterpret_cast<T7_t>(0x00473bf0);         // Surface_blit(this, img, a2..a7), ret 0x1c
const T2_t kSurfaceInit2 = reinterpret_cast<T2_t>(0x004745c0);        // Surface_init2(this, w, h), ret 8
const C1_t kDrawQueuedSprite = reinterpret_cast<C1_t>(0x00462be0);    // drawQueuedSprite(index), cdecl

}  // namespace

// ====================================================================================================================
// Asset loaders (loadPcxSurface, loadFonts, loadCursors)
// ====================================================================================================================

// 0x0049d1b0  loadPcxSurface(): a local Image (ctor 0x0049d1d0), loadImage(name 0x004e49fc, 0, 0xa, 0xec, 0) (pushes 0x0049d1d5..
// 0x0049d1e0, call 0x0049d1f4). eax != 0 (test 0x0049d1f9): Surface_init2 0x004745c0 on the Surface record 0x00840710 with
// (0x10, 0x10) (0x0049d1fd..0x0049d206); eax == 0: Surface_blit 0x00473bf0 on 0x00840710 with (img, 1, 0x12, 0x10, 0x10, 1, 1)
// (0x0049d20d..0x0049d223). esi = that result (0x0049d228); the destructor 0x00474c40 runs on the local either way (0x0049d23d /
// 0x0049d25a); esi != 0 returns esi (0x0049d242), esi == 0 returns 0 (xor 0x0049d266).
int __cdecl LoadPcxSurface_re() {
    alignas(16) unsigned char img[0x2c0];
    kImageCtor(img, 0);                                                      // 0x0049d1d0
    int r = kLoadImage(img, 0, 0x004e49fc, 0, 0xa, 0xec, 0);                 // 0x0049d1f4
    int s;
    if (r != 0) s = kSurfaceInit2(reinterpret_cast<void*>(0x00840710), 0, 0x10, 0x10);          // 0x0049d206
    else s = kSurfaceBlit(reinterpret_cast<void*>(0x00840710), 0, reinterpret_cast<int>(img), 1, 0x12, 0x10, 0x10, 1, 1);  // 0x0049d223
    kImageDtor(img, 0);                                                      // 0x0049d23d / 0x0049d25a
    return s;                                                                // 0x0049d242 (s != 0) / xor eax, eax 0x0049d266 (s == 0)
}
C0_t LoadPcxSurface_orig;
// 0x0049d1b0
SG_HOOK("golf_clean.exe", 0x0049d1b0, loadPcxSurface, LoadPcxSurface_re, LoadPcxSurface_orig);

// 0x0049fe50  loadFonts(): the same local Image; loadImage(name 0x004e49fc, 0, 0, 0x100, 2) (pushes 0x0049fe75..0x0049fe80,
// call 0x0049fe94). A non-zero result (test 0x0049fe9b) is destroyed-and-returned (0x0049feae, 0x0049feb3). Zero: Surface_blit on
// the record 0x008408c8 with (img, 1, 0x23, 0x20, 0x20, 1, 1) (0x0049fecb..0x0049fee1) and on 0x008408f8 with (img, 0x22, 0x23,
// 0x20, 0x20, 1, 1) (0x0049fee6..0x0049fefc), their results ignored; the destructor, then xor eax, eax (0x0049ff1c) returns 0.
int __cdecl LoadFonts_re() {
    alignas(16) unsigned char img[0x2c0];
    kImageCtor(img, 0);                                                      // 0x0049fe70
    int r = kLoadImage(img, 0, 0x004e49fc, 0, 0, 0x100, 2);                  // 0x0049fe94
    if (r != 0) {                                                            // 0x0049fe9b
        kImageDtor(img, 0);                                                  // 0x0049feae
        return r;                                                            // 0x0049feb3
    }
    kSurfaceBlit(reinterpret_cast<void*>(0x008408c8), 0, reinterpret_cast<int>(img), 1, 0x23, 0x20, 0x20, 1, 1);      // 0x0049fee1
    kSurfaceBlit(reinterpret_cast<void*>(0x008408f8), 0, reinterpret_cast<int>(img), 0x22, 0x23, 0x20, 0x20, 1, 1);   // 0x0049fefc
    kImageDtor(img, 0);                                                      // 0x0049ff10
    return 0;                                                                // 0x0049ff1c
}
C0_t LoadFonts_orig;
// 0x0049fe50
SG_HOOK("golf_clean.exe", 0x0049fe50, loadFonts, LoadFonts_re, LoadFonts_orig);

// 0x0049e9d0  loadCursors(): loadImage(name 0x004e49fc, 0, 0, 0x100, 2) (0x0049e9f5..0x0049ea14); non-zero: destroy and return it
// (0x0049ea1f..0x0049ea35). Zero: three Surface_blit calls with the image: record 0x00840830 (img, 1, 0x23, 0x20, 0x20, 1, 1)
// (0x0049ea4b..0x0049ea61), 0x00840890 (img, 1, 0x23, ...) (0x0049ea66..0x0049ea7c) and 0x00840860 (img, 0x22, 0x23, ...)
// (0x0049ea81..0x0049ea97); the destructor, then xor eax, eax returns 0 (0x0049eab7).
int __cdecl LoadCursors_re() {
    alignas(16) unsigned char img[0x2c0];
    kImageCtor(img, 0);                                                      // 0x0049e9f0
    int r = kLoadImage(img, 0, 0x004e49fc, 0, 0, 0x100, 2);                  // 0x0049ea14
    if (r != 0) {                                                            // 0x0049ea1d
        kImageDtor(img, 0);                                                  // 0x0049ea2e
        return r;                                                            // 0x0049ea33
    }
    kSurfaceBlit(reinterpret_cast<void*>(0x00840830), 0, reinterpret_cast<int>(img), 1, 0x23, 0x20, 0x20, 1, 1);      // 0x0049ea61
    kSurfaceBlit(reinterpret_cast<void*>(0x00840890), 0, reinterpret_cast<int>(img), 1, 0x23, 0x20, 0x20, 1, 1);      // 0x0049ea7c
    kSurfaceBlit(reinterpret_cast<void*>(0x00840860), 0, reinterpret_cast<int>(img), 0x22, 0x23, 0x20, 0x20, 1, 1);   // 0x0049ea97
    kImageDtor(img, 0);                                                      // 0x0049eaab
    return 0;                                                                // 0x0049eab7
}
C0_t LoadCursors_orig;
// 0x0049e9d0
SG_HOOK("golf_clean.exe", 0x0049e9d0, loadCursors, LoadCursors_re, LoadCursors_orig);

// ====================================================================================================================
// Sprite queue
// ====================================================================================================================

// 0x004628d0  addSprite(a1 x, a2 y, a3, a4, a5, a6, a7, a8): cdecl, eight stack arguments ([esp+0x14]..[esp+0x30] after four pushes).
// ebx = 0x10; [0x822c8c] > 1000 (jle 0x004628e3) -> 0xd; >= 0x4b0 (jl 0x004628ef) -> 0xa; a8 & 0x100 (test ah, 1, 0x004628fa)
// -> 0x10. i = [0x838200]; s = [0x4c2844]. a7 == 0 (0x0046290a): x[i] = a1 - ((s * a4 + ((s * a4 >> 31) & 3)) >> 2) and y[i] =
// a2 - ((s * a5 + ...) >> 2) (0x00462916..0x0046294f); a7 > 0 (0x0046295b): x[i] = a1 - s * a4 * a7 / d, y[i] = a2 - s * a5 * a7 / d
// (idiv 0x00462978 / 0x00462990); a7 < 0: d = 2 * d, x[i] = s * a4 * a7 / d + a1, y[i] = s * a5 * a7 / d + a2 (idiv 0x004629ba /
// 0x004629d6). Stores 0x0082c164[i] = a2, 0x0082915c[i] = a7, 0x0082b160[i] = a1, 0x0082415c[i] = a3, 0x0082a15c[i] = a6,
// 0x0082815c[i] = a8 (0x004629e9..0x00462a15), then drawQueuedSprite 0x00462be0 (i) (push esi 0x004629ff, call 0x00462a1c); its eax
// is the result (ret 0x00462a28).
int __cdecl AddSprite_re(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8) {
    int d = 0x10;
    int w = G<int>(0x00822c8c);
    if (w > 1000) d = 0xd;                                                   // 0x004628d8 / 0x004628e3
    if (w >= 0x4b0) d = 0xa;                                                 // 0x004628ea / 0x004628f1
    if (a8 & 0x100) d = 0x10;                                                // 0x004628fa / 0x004628ff
    int i = G<int>(0x00838200);
    int s = G<int>(0x004c2844);
    int* X = reinterpret_cast<int*>(0x00830164);
    int* Y = reinterpret_cast<int*>(0x00831164);
    if (a7 == 0) {                                                           // 0x0046290a
        int px = mul(s, a4), py = mul(s, a5);
        X[i] = sub(a1, (add(px, (px >> 31) & 3)) >> 2);                      // 0x00462927..0x00462937
        Y[i] = sub(a2, (add(py, (py >> 31) & 3)) >> 2);                      // 0x00462942..0x0046294f
    } else if (a7 > 0) {                                                     // 0x0046295b
        X[i] = sub(a1, mul(mul(s, a4), a7) / d);                             // 0x00462978
        Y[i] = sub(a2, mul(mul(s, a5), a7) / d);                             // 0x00462990
    } else {
        int d2 = d + d;                                                      // 0x004629a9
        X[i] = add(mul(mul(s, a4), a7) / d2, a1);                            // 0x004629ba
        Y[i] = add(mul(mul(s, a5), a7) / d2, a2);                            // 0x004629d6
    }
    reinterpret_cast<int*>(0x0082c164)[i] = a2;                              // 0x004629e9
    reinterpret_cast<int*>(0x0082915c)[i] = a7;                              // 0x004629f4
    reinterpret_cast<int*>(0x0082b160)[i] = a1;                              // 0x00462a00
    reinterpret_cast<int*>(0x0082415c)[i] = a3;                              // 0x00462a07
    reinterpret_cast<int*>(0x0082a15c)[i] = a6;                              // 0x00462a0e
    reinterpret_cast<int*>(0x0082815c)[i] = a8;                              // 0x00462a15
    return kDrawQueuedSprite(i);                                             // 0x00462a1c
}
C8_t AddSprite_orig;
// 0x004628d0
SG_HOOK("golf_clean.exe", 0x004628d0, addSprite, AddSprite_re, AddSprite_orig);

// ====================================================================================================================
// drawQueuedSprite
// ====================================================================================================================
namespace {
typedef int(__fastcall* T9_t)(void* ecx, void* edx, int a, int b, int c, int d, int e, int f, int g, int h, int i);
typedef int(__fastcall* Slot3_t)(void* ecx, void* edx, int a, int b, int c);
const T0_t kCacheCtor = reinterpret_cast<T0_t>(0x00482fd0);             // Cache482fd0::ctor(this), ret 0
const T0_t kCacheDtor = reinterpret_cast<T0_t>(0x00483010);             // Cache483010::dtor(this), ret 0
const C1_t kFormatGolferTitle = reinterpret_cast<C1_t>(0x00462020);     // cdecl, one argument
const T5_t kSpriteDraw = reinterpret_cast<T5_t>(0x004741b0);            // Sprite_draw(this, dst, x, y, a, b), ret 0x14
const T9_t kSpriteDrawScaled = reinterpret_cast<T9_t>(0x00474260);      // Sprite_drawScaled(this, 9 args), ret 0x24
const T6_t kSurfaceDrawSprite = reinterpret_cast<T6_t>(0x00474440);     // Surface_drawSprite(this, 6 args), ret 0x18
const T5_t kSurfaceBlit2 = reinterpret_cast<T5_t>(0x004740f0);          // Surface_blit2(this, dst, x, y, a, b), ret 0x14
const T4_t kSetDrawTarget = reinterpret_cast<T4_t>(0x004762d0);         // setDrawTarget(this, a, b, c, d), ret 0x10
const T5_t kWidgetFillRect = reinterpret_cast<T5_t>(0x00478b80);        // Widget_fillRect(this, x0, y0, x1, y1, colour), ret 0x14
const C4_t kDrawCenteredText = reinterpret_cast<C4_t>(0x00404b70);      // drawCenteredText(text, x, y, colour), cdecl
const C2_t kBuildGolferName = reinterpret_cast<C2_t>(0x004676e0);       // buildGolferName(golfer, flag), cdecl
const C3_t kItoa = reinterpret_cast<C3_t>(0x004ad425);                  // _itoa(value, buf, radix), cdecl

static inline int& Qa(unsigned base, int i) { return reinterpret_cast<int*>(base)[i]; }
// strcat as the original compiles it (repne scasb to find the end of both strings, rep movsd / movsb of strlen(src) + 1 bytes)
static void appendText(char* dst, const char* src) { strcpy(dst + strlen(dst), src); }
// the display's slot 0x9c (Display::setScale): [[disp]] + 0x9c, thiscall, three stack arguments
static void displaySetScale(void* disp, int nx, int ny, int den) {
    void* fn = *reinterpret_cast<void**>(*reinterpret_cast<char**>(disp) + 0x9c);
    reinterpret_cast<Slot3_t>(fn)(disp, 0, nx, ny, den);
}
}  // namespace

// 0x00462be0  drawQueuedSprite(i): cdecl, one argument (ebp = [esp + 0x7c] after the frame, 0x00462c05). A local Cache482fd0 (0x5c
// bytes, ctor 0x00482fd0 at 0x00462c00, destructor 0x00483010 at 0x004630e2 whose eax is the function's result). Queue entry i of
// the arrays addSprite fills: x 0x00830164, y 0x00831164, a2 0x0082c164, p7 0x0082915c, a1 0x0082b160, a3 0x0082415c, obj 0x0082a15c,
// a8 0x0082815c. d = 0x10; [0x822c8c] > 1000 -> 0xd; >= 0x4b0 -> 0xa; a8 & 0x100 -> 0x10 (0x00462c0e..0x00462c3f); obj + 0x1c is read
// (0x00462c4b) before any branch. p7 == 0: Surface_blit2 on obj (0x0046306b..0x0046308a). Otherwise esi = p7 * [0x4c2844] (p7 < 0:
// -(p7 * scale) and d doubled, 0x00462c89..0x00462c96) and the display's slot 0x9c gets (esi, esi, d) when a display exists
// (0x00462c7a / 0x00462c9a). a8 == -1: Surface_blit2 (0x00463042). a8 & 0x100: formatGolferTitle(a8 & 0xff), Sprite_draw with the
// object table 0x81ca10, then (golfer < 0x98) the name text in 0x51a068 (buildGolferName, the string at 0x4c4e60 appended for
// record flag 0x4000, the golfer number appended for [0x59e7b8] & 0x20, cleared in mode 3 without record flag 0x200), setDrawTarget,
// and when the label conditions hold drawCenteredText and the two Widget_fillRect bars. a8 & 0x800 / 0x400: Sprite_drawScaled
// (colours 0x80007c00 / 0x80007fff), a8 & 0x200: Surface_drawSprite with 0x3f19999a, else Sprite_draw with the table entry
// 0x81ca10 + 0x58 * a8. Finally p7 != 0 (re-read, 0x0046308f..0x00463099) restores the display scale: (scale, scale, 4) when
// [0x8371f8] != 0, else ([0x58b384], [0x58b384], 4) (0x004630b7 / 0x004630c1).
int __cdecl DrawQueuedSprite_re(int i) {
    alignas(16) unsigned char cache[0x60];
    kCacheCtor(cache, 0);                                                    // 0x00462c00
    int d = 0x10;
    int w = G<int>(0x00822c8c);
    if (w > 1000) d = 0xd;                                                   // 0x00462c0e / 0x00462c29
    if (w >= 0x4b0) d = 0xa;                                                 // 0x00462c2e / 0x00462c35
    if (Qa(0x0082815c, i) & 0x100) d = 0x10;                                 // 0x00462c3a / 0x00462c3f
    void* obj = reinterpret_cast<void*>(Qa(0x0082a15c, i));                  // 0x00462c44
    volatile int objWord = At<int>(obj, 0x1c);                               // 0x00462c4b
    (void)objWord;
    int* p7p = &Qa(0x0082915c, i);                                           // 0x00462c52
    int p7 = *p7p;                                                           // 0x00462c5d
    int esi = 0;
    if (p7 == 0) {                                                           // 0x00462c5f
        kSurfaceBlit2(obj, 0, G<int>(0x004c1570), Qa(0x00830164, i), Qa(0x00831164, i), G<int>(0x00822c74), 0);   // 0x0046308a
    } else {
        void* disp = G<void*>(0x0083ad50);
        if (p7 > 0) {                                                        // 0x00462c67
            esi = mul(p7, G<int>(0x004c2844));                               // 0x00462c69
            if (disp) displaySetScale(disp, esi, esi, d);                    // 0x00462c81
        } else {
            esi = -mul(p7, G<int>(0x004c2844));                              // 0x00462c89..0x00462c98
            d = d + d;                                                       // 0x00462c96
            if (disp) displaySetScale(disp, esi, esi, d);                    // 0x00462ca5
        }
        int a8 = Qa(0x0082815c, i);                                          // 0x00462cab
        if (a8 == -1) {                                                      // 0x00462cb2
            kSurfaceBlit2(obj, 0, G<int>(0x004c1570), Qa(0x00830164, i), Qa(0x00831164, i), G<int>(0x00822c74), 0);   // 0x0046306a
        } else if (a8 & 0x100) {                                             // 0x00462cbd
            int g = a8 & 0xff;                                               // 0x00462cc6
            kFormatGolferTitle(g);                                           // 0x00462ccd
            kSpriteDraw(obj, 0, G<int>(0x004c1570), Qa(0x00830164, i), Qa(0x00831164, i), G<int>(0x00824148), 0x0081ca10);   // 0x00462cff
            if (g != 0xff && g < 0x98) {                                     // 0x00462d04 / 0x00462d10
                char* text = reinterpret_cast<char*>(0x0051a068);
                text[0] = 0;                                                 // 0x00462d1f
                kBuildGolferName(g, 0);                                      // 0x00462d26
                unsigned rec = static_cast<unsigned>(g) << 8;                // 0x00462d30
                if (G<unsigned>(0x005794c8 + rec) & 0x4000) appendText(text, reinterpret_cast<const char*>(0x004c4e60));   // 0x00462d3d..0x00462d6d
                if (G<unsigned char>(0x0059e7b8) & 0x20) {                   // 0x00462d73
                    char* num = reinterpret_cast<char*>(kItoa(g, 0x00834164, 10));   // 0x00462d84
                    appendText(text, num);                                   // 0x00462d93..0x00462db4
                }
                if (G<int>(0x00567afc) == 3 && !(G<unsigned>(0x005794c8 + rec) & 0x200)) text[0] = 0;   // 0x00462dba..0x00462dce
                int target = G<int>(0x004c2844) > 4 ? 0x0051b360 : 0x00519fd8;   // 0x00462ddf..0x00462de8
                kSetDrawTarget(reinterpret_cast<void*>(G<int>(0x004c1570)), 0, target, 0, 0, 0);          // 0x00462dfa
                bool label = false;
                if ((G<unsigned char>(0x005a5a00) & 1) && G<int>(0x004c2844) >= 4) {      // 0x00462dff / 0x00462e08
                    bool idOk = G<unsigned char>(0x0057953c + rec) == 0;     // 0x00462e10
                    int me = G<int>(0x004c2e0c);
                    if (!idOk) idOk = (g == me) || (g == (me ^ 1));          // 0x00462e1a..0x00462e28
                    if (idOk) {
                        signed char c = G<signed char>(0x005794d5 + rec);    // 0x00462e2a
                        if (c >= 6 && c != 0x10) {                           // 0x00462e30 / 0x00462e34
                            unsigned fl = G<unsigned>(0x005794c8 + rec);     // 0x00462e38
                            if (!(fl & 0x20000000) || (G<unsigned char>(0x00834170) & 6)) {   // 0x00462e3e / 0x00462e46
                                if (!(fl & 0x8000)) label = true;            // 0x00462e4f
                            }
                        }
                    }
                }
                if (!label) label = (G<unsigned char>(0x0059e7b8) & 0x20) != 0;               // 0x00462e57
                if (label) {
                    unsigned fl = G<unsigned>(0x005794c8 + rec);             // 0x00462e7d
                    int col = (fl & 0x20000000) ? 0x80007d08 : 0x80007fff;   // 0x00462e8a..0x00462e98
                    kDrawCenteredText(0x0051a068, Qa(0x0082b160, i), Qa(0x0082c164, i), col);   // 0x00462ea5
                    short s = G<short>(0x0057955c + rec);                    // 0x00462eaa
                    int c2 = 0x80007d08;
                    if (s > 2) c2 = 0x80006300;                              // 0x00462eb4
                    if (s > 4) c2 = 0x80001304;                              // 0x00462ec4
                    if (s > 6) c2 = 0x800023e8;                              // 0x00462ecf
                    if (s < 0) c2 = 0x80000000;                              // 0x00462eda
                    if (G<int>(0x00567afc) == 2 && !(G<int>(0x0059e7b8) & 0x200000)) {   // 0x00462ee4 / 0x00462ef1
                        int y9 = add(Qa(0x0082c164, i), 9);                  // 0x00462f01..0x00462f05
                        int x1 = Qa(0x0082b160, i);
                        kWidgetFillRect(reinterpret_cast<void*>(0x00519cd4), 0, add(x1, -12), y9, add(x1, 12), y9, static_cast<int>(0x80000000));   // 0x00462f1c
                        int s4 = static_cast<int>(G<short>(0x0057955c + rec)) - 4;         // 0x00462f21..0x00462f2c
                        int y9b = add(Qa(0x0082c164, i), 9);                 // 0x00462f28..0x00462f30
                        int x1b = Qa(0x0082b160, i);                         // 0x00462f2a
                        int xe = add(s4, add(x1b, mul(s4, 2)));              // 0x00462f33..0x00462f37
                        kWidgetFillRect(reinterpret_cast<void*>(0x00519cd4), 0, add(x1b, -12), y9b, xe, y9b, c2);   // 0x00462f44
                    }
                }
            }
        } else if (a8 & 0x800) {                                             // 0x00462f4e
            int obj2 = 0x0081ca10 + 0x58 * (a8 & 0xff);                      // 0x00462f53..0x00462f63
            kSpriteDrawScaled(obj, 0, G<int>(0x004c1570), Qa(0x00830164, i), Qa(0x00831164, i), static_cast<int>(0x80007c00), esi, esi, d, obj2, 0xf8);   // 0x00462f90
        } else if (a8 & 0x400) {                                             // 0x00462f9a
            int obj2 = 0x0081ca10 + 0x58 * (a8 & 0xff);                      // 0x00462f9f..0x00462faf
            kSpriteDrawScaled(obj, 0, G<int>(0x004c1570), Qa(0x00830164, i), Qa(0x00831164, i), static_cast<int>(0x80007fff), esi, esi, d, obj2, 0xf8);   // 0x00462f90
        } else if (a8 & 0x200) {                                             // 0x00462fc1
            int obj2 = 0x0081ca10 + 0x58 * (a8 & 0x1ff);                     // 0x00462fc6..0x00462fdb
            kSurfaceDrawSprite(obj, 0, G<int>(0x004c1570), Qa(0x00830164, i), Qa(0x00831164, i), 0x3f19999a, obj2, 1);   // 0x00462ffe
        } else {
            int obj2 = static_cast<int>(0x0081ca10u + 0x58u * static_cast<unsigned>(a8));   // 0x00463008..0x00463014
            kSpriteDraw(obj, 0, G<int>(0x004c1570), Qa(0x00830164, i), Qa(0x00831164, i), G<int>(0x00824148), obj2);   // 0x0046303b
        }
    }
    if (*p7p != 0) {                                                         // 0x00463096 / 0x00463099
        void* disp = G<void*>(0x0083ad50);
        if (G<int>(0x008371f8) != 0) {                                       // 0x0046309d
            if (disp) { int sc = G<int>(0x004c2844); displaySetScale(disp, sc, sc, 4); }   // 0x004630a6..0x004630d0
        } else {
            int sc = G<int>(0x0058b384);                                     // 0x004630c1
            if (disp) displaySetScale(disp, sc, sc, 4);                      // 0x004630d0
        }
    }
    return kCacheDtor(cache, 0);                                             // 0x004630e2
}
C1_t DrawQueuedSprite_orig;
// 0x00462be0
SG_HOOK("golf_clean.exe", 0x00462be0, drawQueuedSprite, DrawQueuedSprite_re, DrawQueuedSprite_orig);

// ====================================================================================================================
// drawThoughtBubble
// ====================================================================================================================
namespace {
const T7_t kSurfaceDrawScaled = reinterpret_cast<T7_t>(0x00473cb0);     // Surface_drawScaled(this, dst, x, y, 1, 1, 1, 0), ret 0x1c
const T4_t kImageDraw = reinterpret_cast<T4_t>(0x00473e60);             // Image_draw(this, dst, x, y, z), ret 0x10
const T5_t kWinFillRect = reinterpret_cast<T5_t>(0x00478af0);           // Win_fillRect(this, x, y, w, h, colour), ret 0x14
const C1_t kTypeBit7Clear = reinterpret_cast<C1_t>(0x0046c940);         // typeBit7Clear(golfer), cdecl
const C1_t kThoughtFlag = reinterpret_cast<C1_t>(0x004675d0);           // thoughtFlag(golfer), cdecl
}  // namespace

// 0x0045c200  drawThoughtBubble(g, x, y, mode): cdecl, four arguments ([esp + 0x14..0x20] after the four pushes). t = the word at
// golfer record + 0x57956e (movsx, 0x0045c211), r = t * 0x230 (0x0045c218..0x0045c228); app = signed byte [r + 0x4d60aa]
// (0x0045c231) and col = ([r + 0x4d60ab] >> 4) & 3 (0x0045c22b..0x0045c23b). mode == 0 (test 0x0045c23e): Win_fillRect 0x00478af0 on
// [0x4c1570] with (x + 4, y, 0x50, 0x60, 0x80007fff) (0x0045c246..0x0045c25e). Then formatGolferTitle(g) (0x0045c264). mode != 1
// (cmp 0x0045c277): Surface_drawScaled 0x00473cb0 on the record 0x5628e8 + 0x2c * (3 * (app + 72 * typeBit7Clear(g)) +
// thoughtFlag(g)) with (tgt, x, y, 1, 1, 1, 0) (both arms of `cmp edi, 0x14` 0x0045c286 compute the same table address, copies of one
// expression at 0x0045c2c1 / 0x0045c300). mode == 1 (0x0045c30a): Image_draw on 0x5787a0 with (tgt, x + 0x10, y + 0x4c, 0) when
// [r + 0x4d60b4] & 8 (0x0045c30a..0x0045c326); formatGolferTitle(g) again; Image_draw on 0x5439e0 + 0x2c * (col [+ 4 with the flag] +
// 8 * typeBit7Clear(g)) with (tgt, x + 0x13, y + 0x4c, 0x81ca10) (0x0045c34f..0x0045c37c); then Surface_drawScaled with (tgt, x, y - 0x14,
// 1, 1, 1, 0) on 0x59e7c0 + 0x2c * (3 * (app + 20 * typeBit7Clear(g)) + thoughtFlag(g)) when app < 0x14 (0x0045c3c5), or with (tgt,
// x - 0x14, y - 0x14, ...) on 0x5628e8 + 0x2c * (3 * (app + 72 * typeBit7Clear(g)) + thoughtFlag(g)) otherwise (0x0045c40a). The
// result is the last callee's eax.
int __cdecl DrawThoughtBubble_re(int g, int x, int y, int mode) {
    unsigned rec = static_cast<unsigned>(g) << 8;                            // 0x0045c20d
    int t = static_cast<int>(G<short>(0x0057956e + rec));                    // 0x0045c211
    unsigned r = static_cast<unsigned>(t) * 0x230u;                          // 0x0045c218..0x0045c228
    int app = G<signed char>(0x004d60aa + r);                                // 0x0045c231
    int col = (G<unsigned char>(0x004d60ab + r) >> 4) & 3;                   // 0x0045c22b..0x0045c23b
    if (mode == 0)                                                           // 0x0045c23e
        kWinFillRect(reinterpret_cast<void*>(G<int>(0x004c1570)), 0, x + 4, y, 0x50, 0x60, static_cast<int>(0x80007fff));   // 0x0045c25e
    kFormatGolferTitle(g);                                                   // 0x0045c264
    if (mode != 1) {                                                         // 0x0045c277
        int tgt = G<int>(0x004c1570);                                        // 0x0045c280
        int s = kTypeBit7Clear(g);                                           // 0x0045c28c / 0x0045c2cb
        int idx = mul(3, add(app, mul(s, 72)));                              // 0x0045c291..0x0045c298
        idx = add(idx, kThoughtFlag(g));                                     // 0x0045c29b
        return kSurfaceDrawScaled(reinterpret_cast<void*>(0x005628e8 + 0x2c * static_cast<unsigned>(idx)), 0, tgt, x, y, 1, 1, 1, 0);   // 0x0045c2c1 / 0x0045c300
    }
    if (G<unsigned char>(0x004d60b4 + r) & 8)                                // 0x0045c30a
        kImageDraw(reinterpret_cast<void*>(0x005787a0), 0, G<int>(0x004c1570), x + 0x10, y + 0x4c, 0);   // 0x0045c326
    kFormatGolferTitle(g);                                                   // 0x0045c32c
    int c4 = col + ((G<unsigned char>(0x004d60b4 + r) & 8) ? 4 : 0);         // 0x0045c331..0x0045c347
    int s2 = kTypeBit7Clear(g);                                              // 0x0045c363
    int tgt = G<int>(0x004c1570);                                            // 0x0045c35b
    int e = add(c4, mul(s2, 8));                                             // 0x0045c368
    kImageDraw(reinterpret_cast<void*>(0x005439e0 + 0x2c * static_cast<unsigned>(e)), 0, tgt, x + 0x13, y + 0x4c, 0x0081ca10);   // 0x0045c37c
    int tgt2 = G<int>(0x004c1570);                                           // 0x0045c381
    int s3 = kTypeBit7Clear(g);                                              // 0x0045c38d / 0x0045c3cf
    if (app < 0x14) {                                                        // 0x0045c387
        int idx = mul(3, add(app, mul(s3, 20)));                             // 0x0045c392..0x0045c399
        idx = add(idx, kThoughtFlag(g));                                     // 0x0045c39c
        return kSurfaceDrawScaled(reinterpret_cast<void*>(0x0059e7c0 + 0x2c * static_cast<unsigned>(idx)), 0, tgt2, x, y - 0x14, 1, 1, 1, 0);   // 0x0045c3c5
    }
    int idx = mul(3, add(app, mul(s3, 72)));                                 // 0x0045c3d4..0x0045c3db
    idx = add(idx, kThoughtFlag(g));                                         // 0x0045c3de
    return kSurfaceDrawScaled(reinterpret_cast<void*>(0x005628e8 + 0x2c * static_cast<unsigned>(idx)), 0, tgt2, x - 0x14, y - 0x14, 1, 1, 1, 0);   // 0x0045c40a
}
C4_t DrawThoughtBubble_orig;
// 0x0045c200
SG_HOOK("golf_clean.exe", 0x0045c200, drawThoughtBubble, DrawThoughtBubble_re, DrawThoughtBubble_orig);

// ====================================================================================================================
// drawTerrainToolbar
// ====================================================================================================================
namespace {
const C2_t kHitBuildTool = reinterpret_cast<C2_t>(0x00433c60);          // hitBuildTool433c60(a, b), cdecl
const T5_t kStretchBlit = reinterpret_cast<T5_t>(0x00473f60);           // Surface_stretchBlit(this, mask, dst, x, y, extra), ret 0x14
const C2_t kDrawPanelBackdrop = reinterpret_cast<C2_t>(0x00432620);     // drawPanelBackdrop(a, b), cdecl
}  // namespace

// 0x00433e50  drawTerrainToolbar(a, b): cdecl, two arguments. sel = hitBuildTool433c60(a, b) (push ebp / push eax, call 0x00433e5c).
// Surface_stretchBlit on 0x541b78 with (mask 0x58b4f0, dst [0x4c1570], 0xd7, 0x1e2, 0) (0x00433e6c..0x00433e83); setDrawTarget on
// [0x4c1570] with (0x519fd8, 0, 0, 0) (0x00433e99). sel == -2: Image_draw 0x58b2a8 at (0xfe, 0x1e3); [0x59e7b8] & 0x8000000: Image_draw
// 0x571f7c at (0xfd, 0x222); sel == -3: Image_draw 0x571f50 at (0xfd, 0x222); sel >= 0: Image_draw 0x571d40 + 0x84 * sel at the shorts
// (x, y) = ([4 * sel + 0x4c7a88], [4 * sel + 0x4c7a8a]) (0x00433f0b..0x00433f2d); then always Image_draw 0x571d6c + 0x84 * [0x542f20]
// at the shorts of that index (0x00433f39..0x00433f67). sel != [0x5a9f54] (cmp 0x00433f6c): [0x5aa554] = 0 and [0x5a9f54] = sel
// (0x00434062), return. Otherwise [0x5aa554] += 1 (0x00433f7e); sel == -1 returns; eax = the new count (0x00433f8e) and a count <= 10 (signed) returns it; else the text
// buffer 0x51a068 is emptied (0x00433fa0) and, by the jump table 0x434078 on sel + 3 (unsigned > 6 -> default): -3 -> the string at
// 0x4c7f2c, -2 -> 0x4c8030, 0 -> 0x4c801c, 1 -> 0x4c8008, 2 -> 0x4c7ff4, 3 -> 0x4c7fe0, others none, is copied there (strlen + 1 bytes);
// finally drawPanelBackdrop(a, b - 5) (0x00434056). 
int __cdecl DrawTerrainToolbar_re(int a, int b) {
    int sel = kHitBuildTool(a, b);                                           // 0x00433e5c
    int tgt = G<int>(0x004c1570);
    int r = kStretchBlit(reinterpret_cast<void*>(0x00541b78), 0, 0x0058b4f0, tgt, 0xd7, 0x1e2, 0);   // 0x00433e83
    kSetDrawTarget(reinterpret_cast<void*>(G<int>(0x004c1570)), 0, 0x00519fd8, 0, 0, 0);            // 0x00433e99
    if (sel == -2) r = kImageDraw(reinterpret_cast<void*>(0x0058b2a8), 0, G<int>(0x004c1570), 0xfe, 0x1e3, 0);   // 0x00433ebb
    if (G<unsigned>(0x0059e7b8) & 0x8000000) r = kImageDraw(reinterpret_cast<void*>(0x00571f7c), 0, G<int>(0x004c1570), 0xfd, 0x222, 0);   // 0x00433ee3
    if (sel == -3) {                                                         // 0x00433ee8
        r = kImageDraw(reinterpret_cast<void*>(0x00571f50), 0, G<int>(0x004c1570), 0xfd, 0x222, 0);  // 0x00433f34
    } else if (sel >= 0) {                                                   // 0x00433f07
        int y = G<short>(0x004c7a8a + 4 * static_cast<unsigned>(sel));       // 0x00433f0b
        int x = G<short>(0x004c7a88 + 4 * static_cast<unsigned>(sel));       // 0x00433f13
        r = kImageDraw(reinterpret_cast<void*>(0x00571d40 + 0x84u * static_cast<unsigned>(sel)), 0, G<int>(0x004c1570), x, y, 0);   // 0x00433f34
    }
    int cur = G<int>(0x00542f20);                                            // 0x00433f39
    {
        int y = G<short>(0x004c7a8a + 4 * static_cast<unsigned>(cur));       // 0x00433f40
        int x = G<short>(0x004c7a88 + 4 * static_cast<unsigned>(cur));       // 0x00433f48
        r = kImageDraw(reinterpret_cast<void*>(0x00571d6c + 0x84u * static_cast<unsigned>(cur)), 0, G<int>(0x004c1570), x, y, 0);   // 0x00433f67
    }
    if (sel != G<int>(0x005a9f54)) {                                         // 0x00433f6c
        G<int>(0x005aa554) = 0;                                              // 0x00434062
        G<int>(0x005a9f54) = sel;                                            // 0x0043406c
        return r;
    }
    int n = G<int>(0x005aa554) + 1;                                          // 0x00433f78..0x00433f82
    G<int>(0x005aa554) = n;
    if (sel == -1) return r;                                                 // 0x00433f88
    r = n;                                                                   // 0x00433f8e (mov eax, ecx)
    if (n <= 10) return r;                                                   // 0x00433f93
    char* text = reinterpret_cast<char*>(0x0051a068);
    text[0] = 0;                                                             // 0x00433fa0
    switch (static_cast<unsigned>(sel + 3)) {                                // 0x00433f99 / 0x00433fa7 / 0x00433fad
        case 0: strcpy(text, reinterpret_cast<const char*>(0x004c7f2c)); break;   // 0x00433fb4
        case 1: strcpy(text, reinterpret_cast<const char*>(0x004c8030)); break;   // 0x00433fd6
        case 3: strcpy(text, reinterpret_cast<const char*>(0x004c801c)); break;   // 0x00433fdd
        case 4: strcpy(text, reinterpret_cast<const char*>(0x004c8008)); break;   // 0x00433fff
        case 5: strcpy(text, reinterpret_cast<const char*>(0x004c7ff4)); break;   // 0x00434006
        case 6: strcpy(text, reinterpret_cast<const char*>(0x004c7fe0)); break;   // 0x00434028
        default: break;                                                      // case 2 and the unsigned > 6 range: 0x0043404d
    }
    return kDrawPanelBackdrop(a, b - 5);                                     // 0x00434056
}
C2_t DrawTerrainToolbar_orig;
// 0x00433e50
SG_HOOK("golf_clean.exe", 0x00433e50, drawTerrainToolbar, DrawTerrainToolbar_re, DrawTerrainToolbar_orig);

// ====================================================================================================================
// drawAmenityRow
// ====================================================================================================================
namespace {
const C2_t kApproxDistance = reinterpret_cast<C2_t>(0x00467170);        // approxDistance(dx, dy), cdecl
const C3_t kCellAt = reinterpret_cast<C3_t>(0x0043d6f0);                // cellAt(a, b, c), cdecl
const C8_t kAddSprite = reinterpret_cast<C8_t>(0x004628d0);             // addSprite(a1..a8), cdecl
static inline int mulhi(int a, int b) { return static_cast<int>((static_cast<long long>(a) * static_cast<long long>(b)) >> 32); }
}  // namespace

// 0x00432200  drawAmenityRow(p1..p11): cdecl, eleven stack arguments; the frame is `sub esp, 0x10` plus four saved registers, the
// argument slot of p1 is reused for the loop's id (0x00432329). local_10 = -1 (0x00432207, the result, [esp] read at 0x00432558).
// p2 == 0x1a3: p3 = 3 * p3 (0x00432214..0x0043221d). n = p3. x0 = p1 - (47 * n + ((47 * n >> 31) & 3)) / 4 + 0x220 (0x0043222d..
// 0x00432248, sar after the bias), half = n / 2 truncating (0x00432243..0x00432254). Strip: for i < half (0x00432256, jl 0x0043229c)
// Surface_stretchBlit on 0x55e930 + 0x2c * (i != 0) with mask 0x542e50 + 0x2c * (i != 0), dst [0x4c1570], (x, 0x1dd, 0), x += 0x44 for
// i == 0 and 0x2f later (0x0043228b..0x00432297); then the end piece index (n & 1) | 2 (0x004322a6..0x004322ca). p9 == -1: p9 = 0xf0 when
// p6 >= -2 else 0x8c (0x004322cf..0x004322ec). Rows (n > 0, 0x004322f6): A = 0, B = 0; for each i: p10 != 0 and its bit (1 << (i & 31))
// clear skips the body (0x00432308..0x00432317); id = B + p2 (0x00432323); px = (A + (A < 0)) / 2 + x0 + 0x2c, py = 0x1f4 + (i odd ? 0x1b :
// 0) (0x00432333..0x0043234e); approxDistance(p7 - px, (p8 - py) * 2) < 0x14 (signed, 0x00432364) sets local_10 = i; k = 3, 0 when
// local_10 == i, 1 when [0x5a9f60] == i (0x00432371..0x00432384); Surface_drawScaled on (i odd ? 0x55ea90 : 0x55e9e0) + 0x2c * k with
// (tgt, px - 0x16, py - 0x14, 1, 1, 1, 0) (0x004323b8). p3 == 1 and id != 0x168 (0x004323bd / 0x004323c8): k2 = id + 3, id + 1 for id in
// {0x1ec, 0x1ca}; cell = cellAt(k2, [4 * k2 + 0x53f3e8] - 1, 0) stored at 0x5aa6c4, addSprite(px, py, 0, p9, p9, cell, p6, i + p5)
// (0x004323f2..0x00432414). Always: cell = cellAt(id, [4 * id + 0x53f3e8] - 1, 0) at 0x5aa6c4 and addSprite(px, py, 0, p9, p9, cell, p6,
// (i + p5) | (p11 bit i ? 0x400 : 0)) (0x0043243c..0x00432480). Then p2 == 0x1a9: when i + 1 == 2n / 3, p2 = 0x1af and p5 += the rounded
// (mulhi(0x55555555, n) - n) / 2 (0x00432491..0x004324d7); p2 == 0x1a3: when i + 1 == n / 3, p2 = 0x1a9 and p5 -= n / 3 (0x004324d9..
// 0x00432509); p2 == 0x226: p5 -= 1 (0x0043250b..0x00432512). Tail: B += p4, A += 0x2f, i < p3 (0x00432516..0x00432536). The display's
// slot 0x9c gets (1, 1, 1) when a display exists (0x0043253c..0x00432552); the result is local_10.
int __cdecl DrawAmenityRow_re(int p1, int p2, int p3, int p4, int p5, int p6, int p7, int p8, int p9, unsigned p10, unsigned p11) {
    int hover = -1;                                                          // 0x00432207
    if (p2 == 0x1a3) p3 = mul(p3, 3);                                        // 0x0043220f..0x0043221d
    int n = p3;                                                              // 0x00432223
    int n47 = sub(mul(n, 48), n);                                            // 0x0043222d..0x00432236
    int x0 = add(sub(p1, add(n47, (n47 >> 31) & 3) >> 2), 0x220);            // 0x00432238..0x00432248
    int half = add(n, (n >> 31) & 1) >> 1;                                   // 0x00432243..0x00432254
    int x = x0;
    for (int i = 0; i < half; i++) {                                         // 0x00432256 / 0x0043229c
        unsigned b = (i != 0) ? 1u : 0u;                                     // 0x00432264
        kStretchBlit(reinterpret_cast<void*>(0x0055e930 + 0x2c * b), 0, 0x00542e50 + 0x2c * b, G<int>(0x004c1570), x, 0x1dd, 0);   // 0x0043227f..0x00432286
        int step = 0x44;                                                     // 0x0043228b..0x00432294
        if (i != 0) step = 0x2f;
        x = add(x, step);                                                    // 0x00432297
    }
    unsigned e = static_cast<unsigned>(n & 1) | 2u;                          // 0x004322a6 / 0x004322ab
    kStretchBlit(reinterpret_cast<void*>(0x0055e930 + 0x2c * e), 0, 0x00542e50 + 0x2c * e, G<int>(0x004c1570), x, 0x1dd, 0);   // 0x004322ca
    if (p9 == -1) p9 = ((p6 >= -2) ? 0 : -100) + 0xf0;                       // 0x004322cf..0x004322ec
    if (n > 0) {                                                             // 0x004322f4 / 0x004322f6
        int A = 0, B = 0;                                                    // 0x004322fc / 0x00432300
        for (int i = 0; i < p3; i++) {                                       // 0x00432526..0x00432536
            if (p10 != 0 && !(p10 & (1u << (i & 31)))) { /* skipped body */ }   // 0x00432308..0x00432317
            else {
                int id = add(B, p2);                                         // 0x00432323
                int odd = i & 1;                                             // 0x0043232d
                int px = add(add(add(A, (A >> 31) & 1) >> 1, x0), 0x2c);     // 0x00432331..0x00432343
                int py = add(odd ? 0x1b : 0, 0x1f4);                         // 0x0043233f..0x0043234e
                int dist = kApproxDistance(sub(p7, px), mul(sub(p8, py), 2));   // 0x0043235c
                if (dist < 0x14) hover = i;                                  // 0x00432364 / 0x00432369
                int k = 3;                                                   // 0x00432371
                if (hover == i) k = 0;                                       // 0x00432376
                if (G<int>(0x005a9f60) == i) k = 1;                          // 0x0043237c
                kSurfaceDrawScaled(reinterpret_cast<void*>((odd ? 0x0055ea90u : 0x0055e9e0u) + 0x2cu * static_cast<unsigned>(k)), 0,
                                   G<int>(0x004c1570), sub(px, 0x16), sub(py, 0x14), 1, 1, 1, 0);   // 0x004323b8
                if (p3 == 1) {                                               // 0x004323bd
                    if (id != 0x168) {                                       // 0x004323c8
                        int k2 = add(id, 3);                                 // 0x004323d6
                        if (id == 0x1ec || id == 0x1ca) k2 = add(id, 1);     // 0x004323d0 / 0x004323db
                        int c = kCellAt(k2, G<int>(0x0053f3e8 + 4 * static_cast<unsigned>(k2)) - 1, 0);   // 0x004323f2
                        G<int>(0x005aa6c4) = c;                              // 0x004323ff
                        kAddSprite(px, py, 0, p9, p9, c, p6, add(i, p5));    // 0x00432414
                    }
                }
                int c2 = kCellAt(id, G<int>(0x0053f3e8 + 4 * static_cast<unsigned>(id)) - 1, 0);   // 0x0043243c
                G<int>(0x005aa6c4) = c2;                                     // 0x00432451
                int a8 = add(i, p5);                                         // 0x0043245e / 0x00432470
                if (p11 & (1u << (i & 31))) a8 |= 0x400;                     // 0x00432456 / 0x00432465
                kAddSprite(px, py, 0, p9, p9, c2, p6, a8);                   // 0x00432480
                if (p2 == 0x1a9) {                                           // 0x0043248c
                    int hi = mulhi(add(n, n), 0x55555556);                   // 0x0043249c..0x0043249f
                    int q = hi + static_cast<int>(static_cast<unsigned>(hi) >> 31);   // 0x004324a6..0x004324a9
                    if (i + 1 == q) {                                        // 0x004324ab
                        int h2 = mulhi(n, 0x55555555);                       // 0x004324b8
                        int xx = sub(h2, n);                                 // 0x004324ba
                        int s = xx >> 1;                                     // 0x004324c4
                        int t = s + static_cast<int>(static_cast<unsigned>(s) >> 31);   // 0x004324c6..0x004324cb
                        p2 = 0x1af;                                          // 0x004324bc
                        p5 = add(p5, t);                                     // 0x004324d1..0x004324d3
                    }
                } else if (p2 == 0x1a3) {                                    // 0x004324d9
                    int hi = mulhi(n, 0x55555556);                           // 0x004324e5
                    int q = hi + static_cast<int>(static_cast<unsigned>(hi) >> 31);   // 0x004324ee..0x004324f1
                    if (i + 1 == q) {                                        // 0x004324f5
                        p2 = 0x1a9;                                          // 0x004324fb
                        p5 = sub(p5, q);                                     // 0x00432503
                    }
                } else if (p2 == 0x226) {                                    // 0x00432510
                    p5 = sub(p5, 1);                                         // 0x00432512
                }
            }
            B = add(B, p4);                                                  // 0x00432527
            A = add(A, 0x2f);                                                // 0x00432529
        }
    }
    void* disp = G<void*>(0x0083ad50);                                       // 0x0043253c
    if (disp) displaySetScale(disp, 1, 1, 1);                                // 0x00432552
    return hover;                                                            // 0x00432558
}
typedef int(__cdecl* C11_t)(int, int, int, int, int, int, int, int, int, unsigned, unsigned);
C11_t DrawAmenityRow_orig;
// 0x00432200
SG_HOOK("golf_clean.exe", 0x00432200, drawAmenityRow, DrawAmenityRow_re, DrawAmenityRow_orig);

// ====================================================================================================================
// drawWindow
// ====================================================================================================================
namespace {
const C1_t kWrapMessageBuffer = reinterpret_cast<C1_t>(0x0045af30);     // wrapMessageBuffer(n), cdecl
const C5_t kDrawWindowFrame = reinterpret_cast<C5_t>(0x0040cdd0);       // drawWindowFrame(x, y, w, h, f), cdecl
const C5_t kDrawFrameR3 = reinterpret_cast<C5_t>(0x0040cef0);           // drawFrameR3(x, y, w, h, f), cdecl
const C5_t kDrawFrameS2 = reinterpret_cast<C5_t>(0x0040d0b0);           // drawFrameS2(x, y, w, h, style), cdecl
const C5_t kPlaySound = reinterpret_cast<C5_t>(0x004481b0);             // playSound(a, b, c, d, e), cdecl
const C4_t kDrawMessageBuffer = reinterpret_cast<C4_t>(0x0045b0d0);     // drawMessageBuffer(n, x, y, colour), cdecl
const C4_t kDrawThoughtBubbleCall = reinterpret_cast<C4_t>(0x0045c200); // drawThoughtBubble(g, x, y, mode), cdecl
}  // namespace

// 0x0040d320  drawWindow(x, y, p3, mode): cdecl, four arguments ([esp + 0x1c..0x28] after `sub esp, 8` and four pushes); p3 is not read.
// setDrawTarget on [0x4c1570] with (0x51b360, 0, 0, 0) (0x0040d338). sel = [0x4c2e08]. r = wrapMessageBuffer(sel != -1 ? 0x25 : 0x2c)
// (0x0040d33d..0x0040d34d); h = 15 * r + 0x20 (0x0040d358..0x0040d361). sel == -1: mode = p4 (0x0040d397). Otherwise H = max(h, 0x60)
// (signed, 0x0040d371..0x0040d37e), y += 0x2c, x += 0x1c, mode = -2 (0x0040d382..0x0040d392). [0x587ea0] = H (0x0040d3a5). mode >= 0
// (0x0040d3ab): drawWindowFrame(x - 8, y - 8, 0x1a0, (H * mode + ((H * mode >> 31) & 3)) >> 2, 1) (0x0040d3cb); mode == 0 then playSound(0x10b,
// 0x64, 0, 0, 0) (0x0040d3e5); the result is that call's eax. mode < 0: mode == -3 drawFrameS2(x - 8, y - 8, 0x1a0, H + 0x10, -1)
// (0x0040d433), mode == -2 drawFrameR3 (0x0040d419), else drawWindowFrame (0x0040d412), each with (.., H + 0x10, 1). sel == -1 (reloaded,
// 0x0040d438): setDrawTarget again, drawMessageBuffer(0x63, x + 0xc, y + 0x10, (mode != -3 ? 0x7fe7 : 0) + 0x80000018) (0x0040d479), return.
// sel >= 0: Surface_stretchBlit on 0x5a58c4 with (0x58bb7c, tgt, x - 0x32, y - 0x28, 0) and drawThoughtBubble(sel, x - 0x32, y - 0x28, -1)
// (0x0040d4a8 / 0x0040d4bb). -0xe <= sel <= -6: Surface_drawScaled on 0x53df84 - 16 * (11 * sel + 0x42) with (tgt, x + 8, y - 0x14, 1, 1, 1, 0)
// (0x0040d4ff). sel == -5: stretchBlit 0x5a58c4 (0x58bb7c, tgt, x - 0x28, y - 0x28, 0). sel == -4: stretchBlit on 0x541710 + 0x2c * k, mask
// 0x5791f8 + 0x2c * k, k = the signed byte [0x4c1ea8 + 0x82 * v] with v = the signed byte [0x571ff4 + 0x2e * [0x59bf90]] (0x0040d537..0x0040d57f),
// at (x + 8, y). Then y += 0x10 and one stretchBlit by sel: -3 (0x567a4c, 0x53dea4), -2 (0x567a78, 0x53ded0), -0x14 (0x567a20, 0x53de78), -0x15
// (0x567ad0, 0x53df28), -0x16 (0x567aa4, 0x53defc), each (this, mask, tgt, x, y, 0) (0x0040d58c..0x0040d634). Finally h < 0x60: y += (H - h - 0x10)
// / 2 truncating (0x0040d639..0x0040d64e); setDrawTarget; drawMessageBuffer(0x63, x + 0x64, y, colour) (0x0040d681). The result is the last
// callee's eax.
int __cdecl DrawWindow_re(int x, int y, int p3, int mode) {
    (void)p3;
    kSetDrawTarget(reinterpret_cast<void*>(G<int>(0x004c1570)), 0, 0x0051b360, 0, 0, 0);   // 0x0040d338
    int sel = G<int>(0x004c2e08);                                            // 0x0040d33d
    int arg = (sel != -1) ? 0x25 : 0x2c;                                     // 0x0040d343..0x0040d349
    int r = kWrapMessageBuffer(arg);                                         // 0x0040d34d
    int h = add(mul(r, 15), 0x20);                                           // 0x0040d358..0x0040d361
    int H = h;                                                               // 0x0040d365
    int ex, ey;
    if (sel == -1) {                                                         // 0x0040d36f
        ey = y; ex = x;                                                      // 0x0040d397..0x0040d3a3
    } else {
        if (h < 0x60) H = 0x60;                                              // 0x0040d371 / 0x0040d374 / 0x0040d376
        ey = add(y, 0x2c); ex = add(x, 0x1c); mode = -2;                     // 0x0040d38a..0x0040d392
    }
    G<int>(0x00587ea0) = H;                                                  // 0x0040d3a5
    int res;
    if (mode >= 0) {                                                         // 0x0040d3ab
        int hm = mul(H, mode);                                               // 0x0040d3b1
        int fh = add(hm, (hm >> 31) & 3) >> 2;                               // 0x0040d3b4..0x0040d3c0
        res = kDrawWindowFrame(add(ex, -8), add(ey, -8), 0x1a0, fh, 1);      // 0x0040d3cb
        if (mode != 0) return res;                                           // 0x0040d3d5
        return kPlaySound(0x10b, 0x64, 0, 0, 0);                             // 0x0040d3e5
    }
    if (mode == -3) res = kDrawFrameS2(add(ex, -8), add(ey, -8), 0x1a0, add(H, 0x10), -1);   // 0x0040d433
    else if (mode == -2) res = kDrawFrameR3(add(ex, -8), add(ey, -8), 0x1a0, add(H, 0x10), 1);   // 0x0040d419
    else res = kDrawWindowFrame(add(ex, -8), add(ey, -8), 0x1a0, add(H, 0x10), 1);   // 0x0040d412
    sel = G<int>(0x004c2e08);                                                // 0x0040d438
    if (sel == -1) {                                                         // 0x0040d440
        kSetDrawTarget(reinterpret_cast<void*>(G<int>(0x004c1570)), 0, 0x0051b360, 0, 0, 0);   // 0x0040d459
        int col = ((mode + 3 != 0) ? 0x7fe7 : 0) + static_cast<int>(0x80000018);   // 0x0040d45e..0x0040d46e
        return kDrawMessageBuffer(0x63, add(ex, 0xc), add(ey, 0x10), col);   // 0x0040d479
    }
    if (sel >= 0) {                                                          // 0x0040d489
        res = kStretchBlit(reinterpret_cast<void*>(0x005a58c4), 0, 0x0058bb7c, G<int>(0x004c1570), sub(ex, 0x32), sub(ey, 0x28), 0);   // 0x0040d4a8
        res = kDrawThoughtBubbleCall(G<int>(0x004c2e08), sub(ex, 0x32), sub(ey, 0x28), -1);   // 0x0040d4bb
        sel = G<int>(0x004c2e08);                                            // 0x0040d4c0
    }
    if (sel <= -6 && sel > -0xf) {                                           // 0x0040d4cc / 0x0040d4d1
        int k = add(mul(sel, 11), 0x42);                                     // 0x0040d4ec..0x0040d4f6
        res = kSurfaceDrawScaled(reinterpret_cast<void*>(0x0053df84 - 16u * static_cast<unsigned>(k)), 0, G<int>(0x004c1570), add(ex, 8), sub(ey, 0x14), 1, 1, 1, 0);   // 0x0040d4ff
        sel = G<int>(0x004c2e08);                                            // 0x0040d504
    }
    if (sel == -5) {                                                         // 0x0040d509
        res = kStretchBlit(reinterpret_cast<void*>(0x005a58c4), 0, 0x0058bb7c, G<int>(0x004c1570), sub(ex, 0x28), sub(ey, 0x28), 0);   // 0x0040d528
        sel = G<int>(0x004c2e08);                                            // 0x0040d52d
    }
    if (sel == -4) {                                                         // 0x0040d532
        int c = G<int>(0x0059bf90);                                          // 0x0040d537
        int v = G<signed char>(0x00571ff4 + 46u * static_cast<unsigned>(c)); // 0x0040d547
        int k = G<signed char>(0x004c1ea8 + 130u * static_cast<unsigned>(v));// 0x0040d556
        unsigned o = 44u * static_cast<unsigned>(k);                         // 0x0040d561..0x0040d56e
        res = kStretchBlit(reinterpret_cast<void*>(0x00541710 + o), 0, 0x005791f8 + o, G<int>(0x004c1570), add(ex, 8), ey, 0);   // 0x0040d57f
        sel = G<int>(0x004c2e08);                                            // 0x0040d584
    }
    ey = add(ey, 0x10);                                                      // 0x0040d589
    if (sel == -3) { res = kStretchBlit(reinterpret_cast<void*>(0x00567a4c), 0, 0x0053dea4, G<int>(0x004c1570), ex, ey, 0); sel = G<int>(0x004c2e08); }   // 0x0040d5a5
    if (sel == -2) { res = kStretchBlit(reinterpret_cast<void*>(0x00567a78), 0, 0x0053ded0, G<int>(0x004c1570), ex, ey, 0); sel = G<int>(0x004c2e08); }   // 0x0040d5c4
    if (sel == -0x14) { res = kStretchBlit(reinterpret_cast<void*>(0x00567a20), 0, 0x0053de78, G<int>(0x004c1570), ex, ey, 0); sel = G<int>(0x004c2e08); }   // 0x0040d5e8
    if (sel == -0x15) { res = kStretchBlit(reinterpret_cast<void*>(0x00567ad0), 0, 0x0053df28, G<int>(0x004c1570), ex, ey, 0); sel = G<int>(0x004c2e08); }   // 0x0040d60b
    if (sel == -0x16) { res = kStretchBlit(reinterpret_cast<void*>(0x00567aa4), 0, 0x0053defc, G<int>(0x004c1570), ex, ey, 0); }   // 0x0040d62f
    (void)res;
    if (h < 0x60) {                                                          // 0x0040d639 / 0x0040d63d
        int d = sub(sub(H, h), 0x10);                                        // 0x0040d642..0x0040d646
        ey = add(ey, (d - (d >> 31)) >> 1);                                  // 0x0040d649..0x0040d64e
    }
    kSetDrawTarget(reinterpret_cast<void*>(G<int>(0x004c1570)), 0, 0x0051b360, 0, 0, 0);   // 0x0040d661
    int col = ((mode + 3 != 0) ? 0x7fe7 : 0) + static_cast<int>(0x80000018); // 0x0040d666..0x0040d676
    return kDrawMessageBuffer(0x63, add(ex, 0x64), ey, col);                 // 0x0040d681
}
C4_t DrawWindow_orig;
// 0x0040d320
SG_HOOK("golf_clean.exe", 0x0040d320, drawWindow, DrawWindow_re, DrawWindow_orig);

// ====================================================================================================================
// scrollbarDraw
// ====================================================================================================================
namespace {
const T1_t kWindowPaint = reinterpret_cast<T1_t>(0x004808c0);           // Window::paint(this, a), ret 4
const T3_t kBevelBoxR = reinterpret_cast<T3_t>(0x004795d0);             // Widget_drawBevelBoxR(this, rect, a, b), ret 0xc
const T1_t kThumbRect = reinterpret_cast<T1_t>(0x004974d0);             // scrollbarThumbRect(this, &rect), ret 4
const T2_t kPaintBackground = reinterpret_cast<T2_t>(0x00480a10);       // Window::paintBackground(this, &rect, a), ret 8
const T7_t kBlitRight = reinterpret_cast<T7_t>(0x00476140);             // blitRight(this, 7 arguments), ret 0x1c
// a virtual call [[obj] + off](obj) with no stack argument (thiscall)
static inline int vcall0(void* obj, unsigned off) {
    void* fn = *reinterpret_cast<void**>(*reinterpret_cast<char**>(obj) + off);
    return reinterpret_cast<T0_t>(fn)(obj, 0);
}
}  // namespace

// 0x00496fc0  scrollbarDraw(this): thiscall, no stack argument. The display's slot 0x9c gets (1, 1, 1) when a display exists
// (0x00496fc8..0x00496fda). The sub-objects at this + 0x60c and this + 0xcdc each get their vtable slot 0x120 (ecx = the sub-object,
// 0x00496fec / 0x00496ffe). Window::paint(this, [this + 0x5c4]) (0x0049700d). [this + 0x57c] != -1 (0x00497012): the object [this +
// 0x278] answers its slot 0xd4 (0 when NULL, 0x0049702d / 0x00497035) and Widget_drawBevelBoxR on this + 0x274 gets (that, b, b) with b =
// the byte [this + 0x57c] (0x00497039..0x00497044). scrollbarThumbRect(this, &rect) (0x00497050), Window::paintBackground(this, &rect,
// [this + 0x5c4]) (0x00497063). [this + 0x5d8] != 0: ebx = slot 0xdc of [this + 0x278], eax = slot 0xd8 of [this + 0x278] (re-read through
// edi + 4, 0x00497090) and blitRight(this + 0x274, [this + 0x5d8], 0, 0, 0, 0, d8, dc) (0x004970b6). [this + 0x5d4] = img != 0: Image_draw
// (ecx = img) on this + 0x274 with x = (rect.right - [img + 0x18] - rect.left) / 2 + rect.left and y = (rect.bottom - [img + 0x1c] -
// rect.top) / 2 + rect.top, each truncating toward zero (0x004970d3..0x00497104), argument 0. Then [this + 0x5c8] times (re-read each
// round, 0x00497150 / 0x00497158): Widget_drawBevelBoxR(this + 0x274, &rect, [this + 0x5cc], [this + 0x5d0]) and the rectangle shrinks by
// one on every side (0x00497141..0x0049715e). The result is the last [this + 0x5c8] read (eax at 0x0049716a).
int __fastcall ScrollbarDraw_re(void* self, void*) {
    char* s = B(self);
    void* disp = G<void*>(0x0083ad50);
    if (disp) displaySetScale(disp, 1, 1, 1);                                // 0x00496fda
    vcall0(s + 0x60c, 0x120);                                                // 0x00496fec
    vcall0(s + 0xcdc, 0x120);                                                // 0x00496ffe
    kWindowPaint(self, 0, I(self, 0x5c4));                                   // 0x0049700d
    if (I(self, 0x57c) != -1) {                                              // 0x00497012
        void* h = At<void*>(self, 0x278);                                    // 0x0049701b
        int d4 = h ? vcall0(h, 0xd4) : 0;                                    // 0x0049702d / 0x00497035
        int b = At<unsigned char>(self, 0x57c);                              // 0x00497039
        kBevelBoxR(s + 0x274, 0, d4, b, b);                                  // 0x00497044
    }
    int rc[4];
    kThumbRect(self, 0, reinterpret_cast<int>(rc));                          // 0x00497050
    kPaintBackground(self, 0, reinterpret_cast<int>(rc), I(self, 0x5c4));    // 0x00497063
    if (I(self, 0x5d8) != 0) {                                               // 0x0049706e
        void* h = At<void*>(self, 0x278);                                    // 0x00497072
        int dc = h ? vcall0(h, 0xdc) : 0;                                    // 0x00497084 / 0x0049708e
        void* h2 = At<void*>(self, 0x278);                                   // 0x00497090
        int d8 = h2 ? vcall0(h2, 0xd8) : 0;                                  // 0x00497099 / 0x004970a1
        kBlitRight(s + 0x274, 0, I(self, 0x5d8), 0, 0, 0, 0, d8, dc);        // 0x004970b6
    }
    void* img = At<void*>(self, 0x5d4);                                      // 0x004970bb
    if (img) {                                                               // 0x004970c3
        int dx = sub(sub(rc[2], I(img, 0x18)), rc[0]);                       // 0x004970ef..0x004970fb
        int dy = sub(sub(rc[3], I(img, 0x1c)), rc[1]);                       // 0x004970d7..0x004970e0
        int y = add((dy - (dy >> 31)) >> 1, rc[1]);                          // 0x004970e4..0x004970e9
        int x = add((dx - (dx >> 31)) >> 1, rc[0]);                          // 0x004970fb..0x00497100
        kImageDraw(img, 0, reinterpret_cast<int>(s + 0x274), x, y, 0);       // 0x00497104
    }
    int n = I(self, 0x5c8);                                                  // 0x00497109
    if (n > 0) {                                                             // 0x00497113
        int i = 0;
        do {
            kBevelBoxR(s + 0x274, 0, reinterpret_cast<int>(rc), I(self, 0x5cc), I(self, 0x5d0));   // 0x00497130
            rc[0] = add(rc[0], 1); rc[2] = sub(rc[2], 1);                    // 0x00497141 / 0x0049714a
            rc[1] = add(rc[1], 1); rc[3] = sub(rc[3], 1);                    // 0x0049714b / 0x00497156
            n = I(self, 0x5c8);                                              // 0x00497150
            i++;                                                             // 0x00497157
        } while (i < n);                                                     // 0x00497158 / 0x00497162
    }
    return n;                                                                // 0x0049716a
}
typedef int(__fastcall* ScrollbarDraw_t)(void*, void*);
ScrollbarDraw_t ScrollbarDraw_orig;
// 0x00496fc0
SG_HOOK("golf_clean.exe", 0x00496fc0, scrollbarDraw, ScrollbarDraw_re, ScrollbarDraw_orig);

// ====================================================================================================================
// drawRecords
// ====================================================================================================================
namespace {
const C5_t kWorldToScreen = reinterpret_cast<C5_t>(0x0042fb90);         // worldToScreen(x, y, &sx, &sy, 0), cdecl
const C8_t kEnqueueSprite = reinterpret_cast<C8_t>(0x00462a30);         // enqueueSprite(a1..a8), cdecl
}  // namespace

// 0x004012d0  drawRecords(p1, p2, p3): cdecl, three stack arguments (p1 = [esp + 0x34], p2 = [esp + 0x38], p3 = [esp + 0x3c] after
// `sub esp, 0x20` and four pushes). The slots of p2 and p3 are reused as the two outputs of worldToScreen 0x0042fb90 (sx at
// [esp + 0x38], sy at [esp + 0x3c], 0x00401330 / 0x0040133d). X0 = p2 << 10, Y0 = p3 << 10 (0x004012e3 / 0x004012e6). Grid: for the
// outer round (a = 0.., c = 6 * a, d = 5 - a, e = 0x400 + 0x800 * a while e < 0x2400, 0x00401519) and the inner round (j = 0.., ebp = 5 - j,
// edi = 0x400 + 0x800 * j while edi < 0x2400, 0x004014f3): worldToScreen(X0 + e / 4, Y0 + edi / 4, &sx, &sy, 0) (arguments built at
// 0x00401315..0x00401353, division by 4 with the sign bias); a zero result skips the cell (0x0040135d). Otherwise Image_draw on 0x53ba48
// (p1 odd: + 0x2c * (signed byte [0x4e6d70 + c + 116 * p1 + j] >= 1), 0x004013b0..0x004013c1) with (tgt [0x4c1570], sx - 4 * scale, sy -
// 5 * scale / 2, 0) (0x004013ef); the rotation [0x5685f4] picks (ebx, m) = (a, j) for 0, (j, d) for 2, (d, ebp) for 4, (ebp, a) for 6
// (jump table 0x401728; rotations 1, 3, 5 and above 6 leave both stale, so the fixtures keep to 0, 2, 4, 6). Four enqueueSprite
// 0x00462a30 calls: ebx == 0 and !(m & 1): (sx, sy, sy + 0xb, 0x10, 8, 0x53baf8, 4, -1); m == 0 and !(ebx & 1): (sx, sy, sy - 0x15, 0x10,
// 0x1d, 0x53bb24, 4, -1); ebx == 5 and (m & 1): (sx, sy, sy - 0x15, 0x10, 0x1d, 0x53baf8, 4, -1); m == 5 and (ebx & 1): (sx, sy, sy + 0xb,
// 0x10, 8, 0x53bb24, 4, -1) (0x0040142f..0x004014e8). Second pass over the 16 words at 0x4e6d20 + 116 * p1 + 2 * j (0x0040154a..
// 0x00401718): 0xffff skips; t = the signed word, k = the signed byte [0x4c11e6 + 39 * t], A = [0x4e6d40 + 116 * p1 + j], B = [.. + 0x10]:
// worldToScreen(X0 + (((k + 2 * A) << 10) + bias) / 4, Y0 + (((k + 2 * B) << 10) + bias) / 4, &sx, &sy, 0); zero skips; t < 10: t < 3 first
// cell = cellAt(t + 0x13c, 0, ((rot - (rot >> 31)) >> 1) & 3) stored at 0x4e9a80 and addSprite(sx, sy, sy, 0x8c, 0x8c, cell, 4, t + 0x41)
// (0x00401605..0x0040161c), then (all t < 10) cellAt(t + 0x136, 0, same) at 0x4e9a80 and enqueueSprite(sx, sy, sy, 0x8c, 0x8c, cell, 4,
// t + 0x41) (0x0040164a..0x00401661); t >= 10: C = the byte [0x4e6d60 + 116 * p1 + j]; C == 0xff: cellAt(t + 0x136, 0, ((rot - sign) / 2 +
// j) & 3), otherwise cellAt(t + 0x13c, signed C, same) and C + 1 stored, reset to 0xff when its signed value is not below [0x53f8d8 + 4 * t]
// - 1 (0x004016a2..0x004016e3); then enqueueSprite(sx, sy, sy, 0x1e, 0x28, cell, 4, t + 0x41) (0x00401701). The eax left at ret is an
// incidental leftover of the last pass (void for the callers).
void __cdecl DrawRecords_re(int p1, int p2, int p3) {
    int sx = 0, sy = 0;
    int X0 = static_cast<int>(static_cast<unsigned>(p2) << 10);              // 0x004012e6
    int Y0 = static_cast<int>(static_cast<unsigned>(p3) << 10);              // 0x004012e3
    int a = 0, c = 0, d = 5, e = 0x400;                                      // 0x004012eb..0x00401303
    int ebx = p1, m = 0;
    do {
        int esi = 0, ebp = 5, edi = 0x400;                                   // 0x00401315..0x00401322
        int X1 = add(X0, add(e, (e >> 31) & 3) >> 2);                        // 0x00401317..0x0040132c
        do {
            int sum2 = add(Y0, add(edi, (edi >> 31) & 3) >> 2);              // 0x00401339..0x0040134a
            int r = kWorldToScreen(X1, sum2, reinterpret_cast<int>(&sx), reinterpret_cast<int>(&sy), 0);   // 0x00401353
            if (r != 0) {                                                    // 0x0040135d
                int s = G<int>(0x004c2844);                                  // 0x00401367
                int s5 = mul(s, 5);                                          // 0x0040136f
                int half = (s5 - (s5 >> 31)) >> 1;                           // 0x00401374..0x0040137d
                int y2 = sub(sy, half);                                      // 0x0040137f / 0x004013d2
                int x2 = sub(sx, mul(s, 4));                                 // 0x00401381..0x00401393
                void* self = reinterpret_cast<void*>(0x0053ba48);
                if (p1 & 1) {                                                // 0x0040136d
                    int k = (G<signed char>(0x004e6d70 + static_cast<unsigned>(add(add(c, mul(p1, 116)), esi))) >= 1) ? 1 : 0;   // 0x004013b0 / 0x004013b8
                    self = reinterpret_cast<void*>(0x0053ba48 + 0x2c * static_cast<unsigned>(k));   // 0x004013c1
                }
                kImageDraw(self, 0, G<int>(0x004c1570), x2, y2, 0);          // 0x004013ef
                int rot = G<int>(0x005685f4);                                // 0x004013f4
                if (rot == 0) { ebx = a; m = esi; }                          // 0x00401405
                else if (rot == 2) { ebx = esi; m = d; }                     // 0x0040140f
                else if (rot == 4) { ebx = d; m = ebp; }                     // 0x0040141b
                else if (rot == 6) { ebx = ebp; m = a; }                     // 0x00401425
                if (ebx == 0 && !(m & 1)) kEnqueueSprite(sx, sy, add(sy, 0xb), 0x10, 8, 0x0053baf8, 4, -1);        // 0x00401455
                if (m == 0 && !(ebx & 1)) kEnqueueSprite(sx, sy, sub(sy, 0x15), 0x10, 0x1d, 0x0053bb24, 4, -1);    // 0x00401485
                if (ebx == 5 && (m & 1)) kEnqueueSprite(sx, sy, sub(sy, 0x15), 0x10, 0x1d, 0x0053baf8, 4, -1);     // 0x004014b4
                if (m == 5 && (ebx & 1)) kEnqueueSprite(sx, sy, add(sy, 0xb), 0x10, 8, 0x0053bb24, 4, -1);         // 0x004014e3
            }
            esi++; edi = add(edi, 0x800); ebp--;                             // 0x004014eb..0x004014f2
        } while (edi < 0x2400);                                              // 0x004014f3
        a++; e = add(e, 0x800); c = add(c, 6); d--;                          // 0x0040150f..0x0040152a
    } while (e < 0x2400);                                                    // 0x00401519 / 0x0040152e
    int base = mul(p1, 116);                                                 // 0x0040153a..0x00401547
    for (int j = 0; j < 16; j++) {                                           // 0x00401709..0x00401718
        unsigned short w = G<unsigned short>(0x004e6d20 + static_cast<unsigned>(base) + 2u * static_cast<unsigned>(j));   // 0x00401558
        if (w == 0xffff) continue;                                           // 0x0040155b
        int t = static_cast<short>(w);                                       // 0x00401565
        int A = G<signed char>(0x004e6d40 + static_cast<unsigned>(base + j));// 0x00401568
        int k = G<signed char>(0x004c11e6 + 39u * static_cast<unsigned>(t));// 0x0040157a
        int v1 = static_cast<int>(static_cast<unsigned>(add(k, mul(A, 2))) << 10);   // 0x00401581..0x00401584
        int arg1 = add(add(v1, (v1 >> 31) & 3) >> 2, X0);                    // 0x00401588..0x0040159e
        int B = G<signed char>(0x004e6d50 + static_cast<unsigned>(base + j));// 0x0040158d
        int v2 = static_cast<int>(static_cast<unsigned>(add(k, mul(B, 2))) << 10);   // 0x004015a0..0x004015a3
        int arg2 = add(add(v2, (v2 >> 31) & 3) >> 2, Y0);                    // 0x004015a6..0x004015bc
        int r = kWorldToScreen(arg1, arg2, reinterpret_cast<int>(&sx), reinterpret_cast<int>(&sy), 0);   // 0x004015c1
        if (r == 0) continue;                                                // 0x004015cb
        int rot = G<int>(0x005685f4);
        if (t < 10) {                                                        // 0x004015d1
            if (t < 3) {                                                     // 0x004015da
                int r2 = ((rot - (rot >> 31)) >> 1) & 3;                     // 0x004015df..0x004015e9
                int cell = kCellAt(add(t, 0x13c), 0, r2);                    // 0x004015f6
                G<int>(0x004e9a80) = cell;                                   // 0x00401605
                kAddSprite(sx, sy, sy, 0x8c, 0x8c, cell, 4, add(t, 0x41));   // 0x0040161c
            }
            int r2 = ((G<int>(0x005685f4) - (G<int>(0x005685f4) >> 31)) >> 1) & 3;   // 0x00401624..0x0040162e
            int cell = kCellAt(add(t, 0x136), 0, r2);                        // 0x0040163b
            G<int>(0x004e9a80) = cell;                                       // 0x0040164a
            kEnqueueSprite(sx, sy, sy, 0x8c, 0x8c, cell, 4, add(t, 0x41));   // 0x00401661
        } else {
            unsigned char C = G<unsigned char>(0x004e6d60 + static_cast<unsigned>(base + j));   // 0x0040166e
            int cell;
            int r3 = add((rot - (rot >> 31)) >> 1, j) & 3;                   // 0x00401680..0x0040168c / 0x004016a2..0x004016a8
            if (C == 0xff) {                                                 // 0x0040167a
                cell = kCellAt(add(t, 0x136), 0, r3);                        // 0x00401693
                G<int>(0x004e9a80) = cell;                                   // 0x0040169b
            } else {
                cell = kCellAt(add(t, 0x13c), static_cast<signed char>(C), r3);   // 0x004016b7
                unsigned char C2 = static_cast<unsigned char>(G<unsigned char>(0x004e6d60 + static_cast<unsigned>(base + j)) + 1);   // 0x004016bc / 0x004016cd
                G<unsigned char>(0x004e6d60 + static_cast<unsigned>(base + j)) = C2;   // 0x004016cf
                int lim = G<int>(0x0053f8d8 + 4u * static_cast<unsigned>(t)) - 1;      // 0x004016c3 / 0x004016d6
                G<int>(0x004e9a80) = cell;                                   // 0x004016dc
                if (!(static_cast<signed char>(C2) < lim)) G<unsigned char>(0x004e6d60 + static_cast<unsigned>(base + j)) = 0xff;   // 0x004016da / 0x004016e3
            }
            kEnqueueSprite(sx, sy, sy, 0x1e, 0x28, cell, 4, add(t, 0x41));   // 0x00401701
        }
    }
}
typedef void(__cdecl* C3v_t)(int, int, int);
C3v_t DrawRecords_orig;
// 0x004012d0
SG_HOOK("golf_clean.exe", 0x004012d0, drawRecords, DrawRecords_re, DrawRecords_orig);

// ====================================================================================================================
// drawPanel4810d0
// ====================================================================================================================
namespace {
const T1_t kApplyPalette = reinterpret_cast<T1_t>(0x00478b30);          // Widget_applyPalette(this, a), ret 4
const T1_t kDrawPanelContents = reinterpret_cast<T1_t>(0x0047d850);     // drawPanelContents(this, a), ret 4 (one stack argument)
// the handle of a piece (its word +4) answers a virtual call with no stack argument; a missing handle gives dflt
static inline int hv(int piece, unsigned off, int dflt) {
    int h = *reinterpret_cast<int*>(piece + 4);
    return h ? vcall0(reinterpret_cast<void*>(h), off) : dflt;
}
static inline int Wd(int obj) { return *reinterpret_cast<int*>(obj + 0x18); }     // an image's width
static inline int Ht(int obj) { return *reinterpret_cast<int*>(obj + 0x1c); }     // an image's height
// this->vtable slot 0x118 (thiscall, two stack arguments): the pieces' redraw hook
static inline void slot118(void* self, int piece, int k) {
    void* fn = *reinterpret_cast<void**>(*reinterpret_cast<char**>(self) + 0x118);
    reinterpret_cast<T2_t>(fn)(self, 0, piece, k);
}
}  // namespace

// 0x004810d0  drawPanel4810d0(this, a): thiscall, one stack argument (ret 4); a = [esp + 0x24] after `sub esp, 0x10` and four pushes. The
// result is not defined (eax at ret is a leftover of the last call). [this + 0xa0] & 2 clear returns (0x004810d9); the display's slot 0x9c
// gets (1, 1, 1) (0x004810f8). f = [this + 0x9c]: f & 0x10 and not f & 0x400000 (0x00481104 / 0x00481109): L2 = [+0x180], L1 = [+0x52c],
// ebp = [+0x548], L0 = [+0x534]; else L2 = [+0x184], L1 = [+0x54c], ebp = [+0x554], L0 = [+0x550] (0x00481111..0x0048114f). L3 = [+0x188], or
// [+0x184] when it is -1 (0x00481157..0x0048116e). f & 0x10000000 clear: drawPanelContents(this, a) and return (0x00481172 / 0x00481745).
// Piece 0x11c (a == -1 or 0): applyPalette(piece, handle slot 0xc0 or 0xff); Image_draw(L1, piece, 0, 0, 0); Image_draw(L0, piece, width(handle
// slot 0xd8) - L0.w, 0, 0); blitRight(piece, ebp, 0, 0, L1.w, [+0x1c4] - [+0x1bc] - L0.w - L1.w, 0, height(slot 0xdc)) when ebp, L0 and L1 are
// non-zero; [+0x14c] != 0 and f & 0x10: the function pointer [+0x56c] (if set) is called cdecl with (this, [+0x14c], [+0x11c]) and slot 0x164 of
// this gets ([+0x14c], [+0x11c]); then slot 0x118 gets ([+0x11c], 0) (0x00481184..0x004812c6). Piece 0x128 (a == -1 or 2): applyPalette,
// Image_draw([+0x530], piece, 0, height - [+0x530].h, 0), Image_draw([+0x538], piece, width - [+0x538].w, height - [+0x538].h, 0), blitRight(piece,
// [+0x544], 0, 0, [+0x530].w, height - [+0x544].h, width - [+0x538].w - [+0x530].w, [+0x544].h) when [+0x544], [+0x530], [+0x538] are all set, slot 0x118
// ([+0x128], 2) (0x004812cc..0x0048145f). Piece 0x124 (a == -1 or 3): applyPalette, Image_draw(L1, piece, 0, -L2, 0), Image_draw([+0x530], piece, 0,
// height - [+0x530].h + L3, 0), blitRight(piece, [+0x53c], 0, 0, 0, L1.h - L2, [+0x184], height - [+0x530].h - L1.h + L3 + L2) when [+0x53c], L1 and
// [+0x530] are set, slot 0x118 ([+0x124], 3) (0x00481465..0x0048158f). Piece 0x120 (a == -1 or 1): applyPalette, Image_draw(L0, piece, width - L0.w, -L2,
// 0), Image_draw([+0x538], piece, width - [+0x538].w, height - [+0x538].h + L3, 0), blitRight(piece, [+0x540], 0, 0, width - [+0x540].w, L0.h - L2,
// [+0x540].w, height - [+0x538].h - L0.h + L3 + L2) when [+0x540], L0 and [+0x538] are set, slot 0x118 ([+0x120], 1) (0x00481595..0x0048172e).
void __fastcall DrawPanel4810d0_re(void* self, void*, int a) {
    if (!(At<unsigned char>(self, 0xa0) & 2)) return;                        // 0x004810d9
    void* disp = G<void*>(0x0083ad50);
    if (disp) displaySetScale(disp, 1, 1, 1);                                // 0x004810f8
    unsigned f = At<unsigned>(self, 0x9c);                                   // 0x004810fe
    int L2, L1, L0, ebp;
    if ((f & 0x10) && !(f & 0x400000)) {                                     // 0x00481104 / 0x00481109
        L2 = I(self, 0x180); L1 = I(self, 0x52c); ebp = I(self, 0x548); L0 = I(self, 0x534);   // 0x00481111..0x00481129
    } else {
        L2 = I(self, 0x184); L1 = I(self, 0x54c); ebp = I(self, 0x554); L0 = I(self, 0x550);   // 0x00481133..0x00481145
    }
    int L3 = (I(self, 0x188) == -1) ? I(self, 0x184) : I(self, 0x188);       // 0x0048115d..0x0048116e
    if (!(f & 0x10000000)) { kDrawPanelContents(self, 0, a); return; }       // 0x00481172 / 0x00481745
    int piece = I(self, 0x11c);                                              // 0x0048117e
    if (piece != 0 && (a == -1 || a == 0)) {                                 // 0x00481188..0x00481197
        kApplyPalette(reinterpret_cast<void*>(piece), 0, hv(piece, 0xc0, 0xff));   // 0x004811bc
        if (L1 != 0) kImageDraw(reinterpret_cast<void*>(L1), 0, I(self, 0x11c), 0, 0, 0);          // 0x004811d4
        if (L0 != 0) {                                                       // 0x004811df
            int w = hv(I(self, 0x11c), 0xd8, 0);                             // 0x004811f0
            kImageDraw(reinterpret_cast<void*>(L0), 0, I(self, 0x11c), sub(w, Wd(L0)), 0, 0);       // 0x0048120d
        }
        if (ebp != 0 && L0 != 0 && L1 != 0) {                                // 0x00481214 / 0x0048121a / 0x0048121e
            int w1 = Wd(L1);                                                 // 0x0048122e
            int w0 = Wd(L0);                                                 // 0x00481237
            int span = sub(sub(sub(I(self, 0x1c4), I(self, 0x1bc)), w0), w1);   // 0x0048123a..0x00481244
            int hh = hv(I(self, 0x11c), 0xdc, 0);                            // 0x0048124f
            kBlitRight(reinterpret_cast<void*>(I(self, 0x11c)), 0, ebp, 0, 0, w1, 0, span, hh);   // 0x00481269
        }
        if (I(self, 0x14c) != 0 && (At<unsigned char>(self, 0x9c) & 0x10)) {  // 0x00481276 / 0x00481280
            int fn = I(self, 0x56c);                                         // 0x00481289
            if (fn) reinterpret_cast<C3_t>(fn)(reinterpret_cast<int>(self), I(self, 0x14c), I(self, 0x11c));   // 0x0048129c
            void* f164 = *reinterpret_cast<void**>(*reinterpret_cast<char**>(self) + 0x164);   // 0x004812ad
            reinterpret_cast<T2_t>(f164)(self, 0, I(self, 0x14c), I(self, 0x11c));             // 0x004812b3
        }
        slot118(self, I(self, 0x11c), 0);                                    // 0x004812c6
    }
    piece = I(self, 0x128);                                                  // 0x004812cc
    if (piece != 0 && (a == -1 || a == 2)) {                                 // 0x004812da / 0x004812df
        kApplyPalette(reinterpret_cast<void*>(piece), 0, hv(piece, 0xc0, 0xff));   // 0x00481305
        if (I(self, 0x530) != 0) {                                           // 0x00481310
            int hh = hv(I(self, 0x128), 0xdc, 0);                            // 0x00481323
            kImageDraw(reinterpret_cast<void*>(I(self, 0x530)), 0, I(self, 0x128), 0, sub(hh, Ht(I(self, 0x530))), 0);   // 0x00481344
        }
        if (I(self, 0x538) != 0) {                                           // 0x00481349
            int hh = hv(I(self, 0x128), 0xdc, 0);                            // 0x00481362
            int h538 = Ht(I(self, 0x538));                                   // 0x0048137a
            int ww = hv(I(self, 0x128), 0xd8, 0);                            // 0x00481386
            kImageDraw(reinterpret_cast<void*>(I(self, 0x538)), 0, I(self, 0x128), sub(ww, Wd(I(self, 0x538))), sub(hh, h538), 0);   // 0x004813a6
        }
        if (I(self, 0x544) != 0 && I(self, 0x530) != 0 && I(self, 0x538) != 0) {   // 0x004813b3 / 0x004813bb / 0x004813c1 / 0x004813cf
            int w530 = Wd(I(self, 0x530));                                   // 0x004813d9
            int hh = sub(hv(I(self, 0x128), 0xdc, 0), Ht(I(self, 0x544)));   // 0x004813eb / 0x004813fb
            int ww = hv(I(self, 0x128), 0xd8, 0);                            // 0x0048140f
            int h544 = Ht(I(self, 0x544));                                   // 0x0048141f
            int w538 = Wd(I(self, 0x538));                                   // 0x00481429
            int span = sub(sub(ww, w538), Wd(I(self, 0x530)));               // 0x00481432 / 0x00481434
            kBlitRight(reinterpret_cast<void*>(I(self, 0x128)), 0, I(self, 0x544), 0, 0, w530, hh, span, h544);   // 0x00481445
        }
        slot118(self, I(self, 0x128), 2);                                    // 0x0048145f
    }
    piece = I(self, 0x124);                                                  // 0x00481465
    if (piece != 0 && (a == -1 || a == 3)) {                                 // 0x0048146d / 0x00481473 / 0x00481478
        kApplyPalette(reinterpret_cast<void*>(piece), 0, hv(piece, 0xc0, 0xff));   // 0x0048149e
        if (L1 != 0) kImageDraw(reinterpret_cast<void*>(L1), 0, I(self, 0x124), 0, -L2, 0);       // 0x004814bb
        if (I(self, 0x530) != 0) {                                           // 0x004814c0
            int hh = hv(I(self, 0x124), 0xdc, 0);                            // 0x004814d9
            kImageDraw(reinterpret_cast<void*>(I(self, 0x530)), 0, I(self, 0x124), 0, add(sub(hh, Ht(I(self, 0x530))), L3), 0);   // 0x004814fe
        }
        if (I(self, 0x53c) != 0 && L1 != 0 && I(self, 0x530) != 0) {         // 0x00481503 / 0x0048150b / 0x0048150f / 0x00481519
            int bx = I(self, 0x184);                                         // 0x00481529
            int edi = sub(Ht(L1), L2);                                       // 0x0048152f / 0x00481535
            int hh = hv(I(self, 0x124), 0xdc, 0);                            // 0x0048153d
            int e = sub(hh, Ht(I(self, 0x530)));                             // 0x00481547..0x00481550
            e = sub(e, Ht(L1));                                              // 0x00481552..0x00481559
            e = add(add(e, L3), L2);                                         // 0x0048155b..0x00481567
            kBlitRight(reinterpret_cast<void*>(I(self, 0x124)), 0, I(self, 0x53c), 0, 0, 0, edi, bx, e);   // 0x00481579
        }
        slot118(self, I(self, 0x124), 3);                                    // 0x0048158f
    }
    piece = I(self, 0x120);                                                  // 0x00481595
    if (piece != 0 && (a == -1 || a == 1)) {                                 // 0x0048159d / 0x004815a3 / 0x004815a8
        kApplyPalette(reinterpret_cast<void*>(piece), 0, hv(piece, 0xc0, 0xff));   // 0x004815ce
        if (L0 != 0) {                                                       // 0x004815d7
            int ww = hv(I(self, 0x120), 0xd8, 0);                            // 0x004815ea
            kImageDraw(reinterpret_cast<void*>(L0), 0, I(self, 0x120), sub(ww, Wd(L0)), -L2, 0);   // 0x0048160c
        }
        if (I(self, 0x538) != 0) {                                           // 0x00481611
            int ww = sub(hv(I(self, 0x120), 0xd8, 0), Wd(I(self, 0x538)));   // 0x0048162a / 0x0048163a
            int hh = hv(I(self, 0x120), 0xdc, 0);                            // 0x0048164e
            kImageDraw(reinterpret_cast<void*>(I(self, 0x538)), 0, I(self, 0x120), ww, add(sub(hh, Ht(I(self, 0x538))), L3), 0);   // 0x00481672
        }
        if (I(self, 0x540) != 0 && L0 != 0 && I(self, 0x538) != 0) {         // 0x00481677 / 0x0048167f / 0x00481685 / 0x0048168d
            int ww = hv(I(self, 0x120), 0xd8, 0);                            // 0x004816aa
            int w540 = Wd(I(self, 0x540));                                   // 0x004816be
            int h0 = Ht(L0);                                                 // 0x004816c1
            int ebpv = sub(ww, w540);                                        // 0x004816c8
            int edi = sub(h0, L2);                                           // 0x004816d2
            int hh = hv(I(self, 0x120), 0xdc, 0);                            // 0x004816dd
            int e = sub(hh, Ht(I(self, 0x538)));                             // 0x004816e7..0x004816f0
            e = sub(e, Ht(L0));                                              // 0x004816f6..0x004816fd
            e = add(add(e, L3), L2);                                         // 0x004816ff..0x00481705
            kBlitRight(reinterpret_cast<void*>(I(self, 0x120)), 0, I(self, 0x540), 0, 0, ebpv, edi, w540, e);   // 0x0048171c
        }
        slot118(self, I(self, 0x120), 1);                                    // 0x0048172e
    }
}
typedef void(__fastcall* DrawPanel4810d0_t)(void*, void*, int);
DrawPanel4810d0_t DrawPanel4810d0_orig;
// 0x004810d0
SG_HOOK("golf_clean.exe", 0x004810d0, drawPanel4810d0, DrawPanel4810d0_re, DrawPanel4810d0_orig);

// ====================================================================================================================
// drawBuildingPanel
// ====================================================================================================================
namespace {
const C2_t kBuildingHit = reinterpret_cast<C2_t>(0x004340a0);           // the hit test of the building panel, cdecl (a, b)
const C3_t kClamp = reinterpret_cast<C3_t>(0x00467130);                 // clamp(v, lo, hi), cdecl
const C1_t kAppendNumber = reinterpret_cast<C1_t>(0x0042dc00);          // appendNumber(n), cdecl
const C2_t kDrawTerrainToolbarCall = reinterpret_cast<C2_t>(0x00433e50);
// jump table 0x434954 (index sel + 1 = 0..9): the description string of that building (the string addresses read from the
// case bodies 0x43459a, 0x43453f, 0x43456e, 0x434593, 0x4345e5, 0x434564, 0x4345bc, 0x4345c3, 0x4345ec, 0x43460e)
const unsigned kBuildingDesc[10] = {0x004c8180, 0x004c81dc, 0x004c81b0, 0x004c8194, 0x004c8138, 0x004c81c8, 0x004c816c, 0x004c8150, 0x004c8120, 0x004c810c};
}  // namespace

// 0x00434350  drawBuildingPanel(a, b): cdecl, two arguments ([esp + 0x14] / [esp + 0x18] after `sub esp, 8` and two pushes). [0x55e928] != 0:
// drawTerrainToolbar(a, b) and its eax (0x00434368). Otherwise sel = the hit test 0x004340a0(a, b) (0x00434382), L = sel. sel == [0x5a9f54],
// sel >= 0 and [0x5aa554] > 10 (0x00434391..0x004343ac): x = clamp(rem * 80 + (sel > 4 ? 0x1af : 0x187), 0x50, 0x2d0) with rem = sel % 5
// (0x004343b2..0x004343db); drawFrameS2(x - 0x50, 0x1cc, 0xa0, 0x64, 1) (0x004343f6); setDrawTarget(0x519fd8, 0, 0, 0) (0x0043440f); the text
// buffer 0x51a068 is emptied (0x00434417); ebp = sel + 6: ebp != 7 and [0x5a8c38 + 4 * ebp] != 0: the string at 0x4c69a8 is copied there
// (0x0043441e..0x00434451); the 20-byte record string 0x4c2728 + 20 * sel is appended (0x00434467..0x00434498) and drawCenteredText(text, x,
// 0x1ce, 0x80000000) draws it (0x0043449f); the string at 0x4c81f8 replaces the text (0x004344a4..0x004344cd); the amount is the signed word
// [0x4c273a + 20 * sel] times 150 (ebp != 7 and the table entry set: 3 * 5 * 5 * 4 / 2, 0x004344dc..0x004344f4) or times 100 (0x004344fd);
// appendNumber(amount) (0x00434501) and drawCenteredText(text, x, 0x1d8, 0x80000000) (0x0043451d); the text is emptied and, for sel + 1 <= 9
// (unsigned, 0x00434528..0x00434532), replaced by the description table entry (jump table 0x434954); drawCenteredText(text, x, 0x1e2, 0x80000000)
// (0x00434643). Always: Surface_stretchBlit on 0x541a70 with (0x58b3e8, tgt, 0xd8, 0x1e2, 0) (0x0043466c), setDrawTarget, sel == -2: Image_draw
// 0x58b250 at (0xdc, 0x1fe), sel == -3: Image_draw 0x5a7d68 at (0x105, 0x229) (0x004346a6), [0x59e7b8] & 0x8000000: Image_draw 0x5a7d94 at (0x105,
// 0x229) (0x004346eb). Nine slots (i = 0.., x from 0x157 + 0x50 per slot, y from 0x1f5, at i == 4 y += 0x29 and x -= 0x117 afterwards, 0x0043479f..
// 0x004347ac): with t = [0x5a8c50 + 4 * i], i + 6 < [0x5a6364] - t and t < 2: Surface_drawScaled on 0x5a7c8c + 0xb0 * (i > 4), and again on 0x5a7c08 +
// 0xb0 * (i > 4) when i == L (0x0043473f / 0x00434760); i + 6 == [0x4c2854]: 0x5a7c34 + 0xb0 * (i > 4) (0x0043479a). setDrawTarget. Nine more icons (j,
// 0x004347d9..0x00434891) on 0x53df58 + 0x2c * (b2 + 2 * ((t != 0) + 2 * j)) at x = 0x158 + 40 * (j / 5 + 2 * (j % 5)), y = 0x1c5 + 40 * (j / 5) - flag, where
// flag = (j == L and j + 6 < [0x5a6364] - t and t < 2) and b2 = (j + 6 < [0x5a6364] and t < 2 and (t < 1 or [0x5685f0] > 10)). Finally sel == [0x5a9f54]:
// [0x5aa554] += 1 and for sel == -2 / -3 the text becomes the string 0x4c8100 / 0x4c7f2c and drawPanelBackdrop(a, b) draws it (0x004348ec / 0x0043492e);
// otherwise [0x5aa554] = 0 and [0x5a9f54] = sel (0x0043493c). The result is sel, or drawPanelBackdrop's eax.
int __cdecl DrawBuildingPanel_re(int a, int b) {
    if (G<int>(0x0055e928) != 0) return kDrawTerrainToolbarCall(a, b);       // 0x0043435c / 0x00434368
    int sel = kBuildingHit(a, b);                                            // 0x00434382
    int L = sel;
    char* text = reinterpret_cast<char*>(0x0051a068);
    if (sel == G<int>(0x005a9f54) && sel >= 0 && G<int>(0x005aa554) > 10) {  // 0x00434391 / 0x00434397 / 0x0043439d / 0x004343a5
        int base = (sel > 4) ? 0x1af : 0x187;                                // 0x004343b2..0x004343bc
        int rem = sel % 5;                                                   // 0x004343c3..0x004343c9
        int x = kClamp(add(base, mul(rem, 80)), 0x50, 0x2d0);                // 0x004343db
        kDrawFrameS2(add(x, -0x50), 0x1cc, 0xa0, 0x64, 1);                   // 0x004343f6
        kSetDrawTarget(reinterpret_cast<void*>(G<int>(0x004c1570)), 0, 0x00519fd8, 0, 0, 0);   // 0x0043440f
        int ebp = add(sel, 6);                                               // 0x00434414
        text[0] = 0;                                                         // 0x00434417
        bool up = (ebp != 7) && G<int>(0x005a8c38 + 4u * static_cast<unsigned>(ebp)) != 0;   // 0x0043441e / 0x00434423 / 0x0043442c
        if (up) strcpy(text, reinterpret_cast<const char*>(0x004c69a8));     // 0x0043442e..0x00434451
        unsigned ebx = 20u * static_cast<unsigned>(sel);                     // 0x00434457 / 0x0043445d
        strcat(text, reinterpret_cast<const char*>(0x004c2728 + ebx));       // 0x00434467..0x00434498
        kDrawCenteredText(0x0051a068, x, 0x1ce, static_cast<int>(0x80000000));   // 0x0043449f
        strcpy(text, reinterpret_cast<const char*>(0x004c81f8));             // 0x004344a4..0x004344cd
        int v = G<short>(0x004c273a + ebx);                                  // 0x004344dc / 0x004344f6
        int amount;
        if (up) {                                                            // 0x004344cf / 0x004344d1 / 0x004344d8
            int t = mul(v, 300);                                             // 0x004344e3..0x004344ec
            amount = (t - (t >> 31)) >> 1;                                   // 0x004344ef..0x004344f2
        } else {
            amount = mul(v, 100);                                            // 0x004344fd
        }
        kAppendNumber(amount);                                               // 0x00434501
        kDrawCenteredText(0x0051a068, x, 0x1d8, static_cast<int>(0x80000000));   // 0x0043451d
        ebp = add(ebp, -5);                                                  // 0x00434522
        text[0] = 0;                                                         // 0x0043452b
        if (static_cast<unsigned>(ebp) <= 9) strcpy(text, reinterpret_cast<const char*>(kBuildingDesc[ebp]));   // 0x00434528 / 0x00434538
        kDrawCenteredText(0x0051a068, x, 0x1e2, static_cast<int>(0x80000000));   // 0x00434643
    }
    int esi = L;                                                             // 0x00434648
    kStretchBlit(reinterpret_cast<void*>(0x00541a70), 0, 0x0058b3e8, G<int>(0x004c1570), 0xd8, 0x1e2, 0);   // 0x0043466c
    kSetDrawTarget(reinterpret_cast<void*>(G<int>(0x004c1570)), 0, 0x00519fd8, 0, 0, 0);   // 0x00434682
    if (esi == -2) kImageDraw(reinterpret_cast<void*>(0x0058b250), 0, G<int>(0x004c1570), 0xdc, 0x1fe, 0);   // 0x004346c2
    else if (esi == -3) kImageDraw(reinterpret_cast<void*>(0x005a7d68), 0, G<int>(0x004c1570), 0x105, 0x229, 0);   // 0x004346c2
    if (G<unsigned>(0x0059e7b8) & 0x8000000) kImageDraw(reinterpret_cast<void*>(0x005a7d94), 0, G<int>(0x004c1570), 0x105, 0x229, 0);   // 0x004346eb
    int px = 0x157, py = 0x1f5;                                              // 0x004346f0 / 0x004346f5
    for (int i = 0; i < 9; i++) {                                            // 0x004347b6
        int t = G<int>(0x005a8c50 + 4u * static_cast<unsigned>(i));          // 0x004346fc
        int lim = sub(G<int>(0x005a6364), t);                                // 0x00434703 / 0x0043470c
        if (!(add(i, 6) >= lim) && !(t >= 2)) {                              // 0x0043470e / 0x00434715
            unsigned k = (i > 4) ? 1u : 0u;                                  // 0x0043471c
            unsigned o = 176u * k;                                           // 0x00434730 / 0x00434734
            kSurfaceDrawScaled(reinterpret_cast<void*>(0x005a7c8c + o), 0, G<int>(0x004c1570), px, py, 1, 1, 1, 0);   // 0x0043473f
            if (i == L) kSurfaceDrawScaled(reinterpret_cast<void*>(0x005a7c08 + o), 0, G<int>(0x004c1570), px, py, 1, 1, 1, 0);   // 0x00434760
        }
        if (add(i, 6) == G<int>(0x004c2854)) {                               // 0x0043476e
            unsigned k = (i > 4) ? 1u : 0u;                                  // 0x0043477a / 0x00434783
            kSurfaceDrawScaled(reinterpret_cast<void*>(0x005a7c34 + 176u * k), 0, G<int>(0x004c1570), px, py, 1, 1, 1, 0);   // 0x0043479a
        }
        if (i == 4) { py = add(py, 0x29); px = sub(px, 0x117); }             // 0x004347a9 / 0x004347ac
        else px = add(px, 0x50);                                             // 0x004347a4
    }
    kSetDrawTarget(reinterpret_cast<void*>(G<int>(0x004c1570)), 0, 0x00519fd8, 0, 0, 0);   // 0x004347cd
    for (int j = 0; j < 9; j++) {                                            // 0x0043488b
        int ebpf = 0;                                                        // 0x004347e3
        int lim0 = G<int>(0x005a6364);                                       // 0x004347dd
        int tv = G<int>(0x005a8c50 + 4u * static_cast<unsigned>(j));         // 0x004347e9
        if (j == L) {                                                        // 0x004347e5
            if (add(j, 6) < sub(lim0, tv) && tv < 2) ebpf = -1;              // 0x004347f2 / 0x004347f6 / 0x004347fb
        }
        int b2;
        if (add(j, 6) >= lim0) b2 = 0;                                       // 0x004347fe / 0x00434803
        else if (tv >= 2) b2 = 0;                                            // 0x00434807
        else if (tv < 1) b2 = 1;                                             // 0x0043480c / 0x0043480f
        else b2 = (G<int>(0x005685f0) > 10) ? 1 : 0;                         // 0x00434811 / 0x00434818
        int q = j / 5, rm = j % 5;                                           // 0x00434823..0x00434852
        int y2 = add(add(ebpf, mul(q, 40)), 0x1c5);                          // 0x00434840
        int x2 = add(mul(add(q, mul(rm, 2)), 40), 0x158);                    // 0x00434854..0x0043485f
        int vb = (tv != 0) ? 1 : 0;                                          // 0x00434866..0x0043486a
        int idx = add(b2, mul(add(vb, mul(j, 2)), 2));                       // 0x0043486f / 0x00434872
        kSurfaceDrawScaled(reinterpret_cast<void*>(0x0053df58 + 44u * static_cast<unsigned>(idx)), 0, G<int>(0x004c1570), x2, y2, 1, 1, 1, 0);   // 0x00434882
    }
    if (L != G<int>(0x005a9f54)) {                                           // 0x004348a2 / 0x004348a5
        G<int>(0x005aa554) = 0;                                              // 0x0043493c
        G<int>(0x005a9f54) = L;                                              // 0x00434946
        return L;
    }
    G<int>(0x005aa554) = add(G<int>(0x005aa554), 1);                         // 0x004348ab..0x004348b5
    if (L == -2) {                                                           // 0x004348b2 / 0x004348bb
        strcpy(text, reinterpret_cast<const char*>(0x004c8100));             // 0x004348bd..0x004348e4
        return kDrawPanelBackdrop(a, b);                                 // 0x004348ec
    }
    if (L == -3) {                                                           // 0x004348fa / 0x004348fd
        strcpy(text, reinterpret_cast<const char*>(0x004c7f2c));             // 0x004348ff..0x00434926
        return kDrawPanelBackdrop(a, b);                                 // 0x0043492e
    }
    return L;                                                                // 0x0043494b
}
C2_t DrawBuildingPanel_orig;
// 0x00434350
SG_HOOK("golf_clean.exe", 0x00434350, drawBuildingPanel, DrawBuildingPanel_re, DrawBuildingPanel_orig);

// ====================================================================================================================
// drawAmenitiesPanel
// ====================================================================================================================
namespace {
const C2_t kAmenityHit = reinterpret_cast<C2_t>(0x00434980);            // the hit test of the amenities panel, cdecl (a, b)
const C2_t kBuildName4074a0 = reinterpret_cast<C2_t>(0x004074a0);       // writes a name into the text buffer 0x51a068, cdecl (n, 0)
typedef int(__cdecl* C11b_t)(int, int, int, int, int, int, int, int, int, int, int);
const C11b_t kDrawAmenityRowCall = reinterpret_cast<C11b_t>(0x00432200);
typedef int(__cdecl* C7_t)(int, int, int, int, int, int, int);
const C7_t kDrawMapLine7 = reinterpret_cast<C7_t>(0x004493d0);          // drawMapLine(x0, y0, x1, y1, colour, a, b), cdecl
// ebp & 3 (jump table 0x43552c): the four strings of the 0x4c82xx group
const unsigned kAmenityTitle[4] = {0x004c82b0, 0x004c82a0, 0x004c8290, 0x004c8280};
// L + 2 (jump table 0x43553c): string, and the source of ebp (kind: 0 none, 1 signed word at addr, 2 signed byte at addr)
struct AmenityCase { unsigned str; unsigned src; int kind; };
const AmenityCase kAmenityCase[12] = {
    {0x004c8270, 0, 0}, {0, 0, 0}, {0x004c8264, 0x004c26fe, 1}, {0x004c825c, 0x004c26c2, 1}, {0x004c824c, 0x004c2726, 1}, {0x004c8244, 0x004c26d6, 1},
    {0x004c8238, 0x004c2712, 1}, {0x004c822c, 0x004c26ea, 1}, {0x004c821c, 0x00578673, 2}, {0x004c7f2c, 0, 0}, {0x004c7f2c, 0, 0}, {0x004c820c, 0x004c283e, 1}};
}  // namespace

// 0x00434cf0  drawAmenitiesPanel(a, b): cdecl, two arguments ([esp + 0x1c] / [esp + 0x20] after `sub esp, 8` and four pushes). Surface_stretchBlit on
// 0x541a9c with (0x58b414, tgt, 0xd6, 0x1e2, 0) (0x00434d13); L = the hit test 0x00434980(a, b) (0x00434d22). L > -1 (0x00434d2c): Image_draw on
// 0x59ba68 + 0x84 * L at the shorts (x, y) = ([4 * L + 0x4c7a98], [4 * L + 0x4c7a9a]) (0x00434d5d); L == -2: Image_draw 0x58b27c at (0xfe, 0x1e3)
// (0x00434d7f). M = [0x4c2854]; ebx = 0 for 3, 1 for 0, 2 for 5, 3 for 1, 4 for 4, 5 for 2 else -1 (0x00434d84..0x00434dc6); [0x4c2848] == 0x10 or M ==
// 0x10: ebx = 6; M == 0x13: ebx = 9 (0x00434dd2..0x00434de3); ebx != -1 (or 9): Image_draw 0x59ba94 + 0x84 * ebx at its shorts (0x00434e18). [0x56d1b0] <= 0:
// Image_draw 0x59bbc8 at the shorts of index 2 (0x00434e49). Then by M: 1: drawAmenityRow(-0x54, 0x208, 5, M, 0xaa, 4, a, b, 0x28, 0, 0), a result
// != -1 goes to [0x5a9f50], Image_draw (0x00434e71..0x00434eab); 4: [0x543cfc] != 0: drawAmenityRow(0, 0x168, 0xe, 1, 0x64, -4, a, b, -1, [0x543cfc],
// 0) (0x00434ee1), a result r != -1: [0x5a9f50] = r, the text emptied, 0x004074a0(r, 0) (0x00434f04), an uppercase fix of its first char ('Z' + 1 and
// above lose 0x20, 0x00434f11..0x00434f17), the string at 0x4c52b8 appended, then FREE (0x4c82c0) when [0x822c70] & (1 << r) else the decimal of 1000 * r +
// 5000 (_itoa 0x004ad425 into 0x5a9f6c, 0x00434f61..0x00434f7b) appended, drawPanelBackdrop(a, b - 0xc) (0x00434fb5), the title of r & 3 (jump table
// 0x43552c) replaces the text and drawPanelBackdrop(a, b - 2) (0x00435049); then Image_draw as for 1 (0x0043505d..0x00435086); 0x13: drawAmenityRow(-0x10, 0x226,
// 8, 1, 0xa9, 4, a, b, 0x28, 0, 0) (0x004350af); 2: (0xd, 0x1a3, 5, 0, 0x2d, -6, a, b, 0x28, 0, 0) (0x0043510f); 0x10: (0x64, 0x12f, 7, 1, 0xb4, 2, a, b,
// 0x64, 0, 0) (0x00435173), each storing a result != -1 and drawing the ebx image. setDrawTarget(0x519fd8, 0, 0, 0) (0x004351c4). L != [0x5a9f54]:
// [0x5aa554] = 0, [0x5a9f54] = L (0x00435514). Otherwise [0x5aa554] += 1 (0x004351db); L == -1 or a count <= 10 returns; else the text is emptied and, by L + 2
// (jump table 0x43553c, above 0xb none), replaced by a string with a per-case value ebp (shorts at 0x4c26fe, 0x4c26c2, 0x4c2726, 0x4c26d6, 0x4c2712, 0x4c26ea, 0x4c283e,
// the byte 0x578673, 0 for L = 7, 8); L == -2 takes (a, b) as the point instead of the shorts (0x0043522a). drawMapLine(x - 3 * len + 0x14, y + 0xa, x + 3 * len + 0x14, y + 0xa,
// 0x80000000, 0xa, 5) (0x00435434), drawCenteredText(text, x + 0x14, y + 5, 0x80007fff) (0x00435453) which also overwrites the slot of `a` with x + 0x14
// (0x0043544f); ebp != 0: a second drawMapLine at y + 0x14 (0x00435473), the text emptied and the decimal of 100 * ebp appended (_itoa, 0x00435491), the string
// at 0x4c8200 replacing the text when L == 4 (0x004354cc..0x004354ef), drawCenteredText(text, the stored x + 0x14, y + 0xf, 0x80007fff) (0x00435504). The eax at
// ret is a leftover (void).
void __cdecl DrawAmenitiesPanel_re(int a, int b) {
    kStretchBlit(reinterpret_cast<void*>(0x00541a9c), 0, 0x0058b414, G<int>(0x004c1570), 0xd6, 0x1e2, 0);   // 0x00434d13
    int L = kAmenityHit(a, b);                                               // 0x00434d22
    char* text = reinterpret_cast<char*>(0x0051a068);
    if (L > -1) {                                                            // 0x00434d2c / 0x00434d33
        int y = G<short>(0x004c7a9a + 4u * static_cast<unsigned>(L));        // 0x00434d35
        int x = G<short>(0x004c7a98 + 4u * static_cast<unsigned>(L));        // 0x00434d3d
        kImageDraw(reinterpret_cast<void*>(0x0059ba68 + 0x84u * static_cast<unsigned>(L)), 0, G<int>(0x004c1570), x, y, 0);   // 0x00434d5d
    }
    if (L == -2) kImageDraw(reinterpret_cast<void*>(0x0058b27c), 0, G<int>(0x004c1570), 0xfe, 0x1e3, 0);   // 0x00434d7f
    int M = G<int>(0x004c2854);                                              // 0x00434d84
    int ebx = -1;                                                            // 0x00434d89
    if (M == 3) ebx = 0;                                                     // 0x00434d8c
    else if (M == 0) ebx = 1;                                                // 0x00434d95
    else if (M == 5) ebx = 2;                                                // 0x00434da0
    else if (M == 1) ebx = 3;                                                // 0x00434dac
    else if (M == 4) ebx = 4;                                                // 0x00434db8
    else if (M == 2) ebx = 5;                                                // 0x00434dc1
    if (G<int>(0x004c2848) == 0x10 || M == 0x10) ebx = 6;                    // 0x00434dcb / 0x00434dd4
    bool draw;
    if (M == 0x13) { ebx = 9; draw = true; }                                 // 0x00434dde
    else draw = (ebx != -1);                                                 // 0x00434dea
    auto ebxImage = [&]() {
        int y = G<short>(0x004c7a9a + 4u * static_cast<unsigned>(ebx));
        int x = G<short>(0x004c7a98 + 4u * static_cast<unsigned>(ebx));
        kImageDraw(reinterpret_cast<void*>(0x0059ba94 + 0x84u * static_cast<unsigned>(ebx)), 0, G<int>(0x004c1570), x, y, 0);
    };
    if (draw) { ebxImage(); M = G<int>(0x004c2854); }                        // 0x00434def..0x00434e1d
    if (G<int>(0x0056d1b0) <= 0) {                                           // 0x00434e22 / 0x00434e2a
        int y = G<short>(0x004c7aa2);                                        // 0x00434e2c
        int x = G<short>(0x004c7aa0);                                        // 0x00434e33
        kImageDraw(reinterpret_cast<void*>(0x0059bbc8), 0, G<int>(0x004c1570), x, y, 0);   // 0x00434e49
        M = G<int>(0x004c2854);                                              // 0x00434e4e
    }
    if (M == 1) {                                                            // 0x00434e53
        int r = kDrawAmenityRowCall(-0x54, 0x208, 5, M, 0xaa, 4, a, b, 0x28, 0, 0);   // 0x00434e71
        if (r != -1) G<int>(0x005a9f50) = r;                                 // 0x00434e79 / 0x00434e7e
        ebxImage(); M = G<int>(0x004c2854);                                  // 0x00434eab / 0x00434eb0
    }
    if (M == 4) {                                                            // 0x00434eb5
        int v543 = G<int>(0x00543cfc);                                       // 0x00434ebe
        if (v543 != 0) {                                                     // 0x00434ec3
            int r = kDrawAmenityRowCall(0, 0x168, 0xe, 1, 0x64, -4, a, b, -1, v543, 0);   // 0x00434ee1
            if (r != -1) {                                                   // 0x00434eeb
                G<int>(0x005a9f50) = r;                                      // 0x00434ef7
                text[0] = 0;                                                 // 0x00434efd
                kBuildName4074a0(r, 0);                                      // 0x00434f04
                unsigned char c0 = static_cast<unsigned char>(text[0]);      // 0x00434f09
                if (static_cast<signed char>(c0) > 0x5a) text[0] = static_cast<char>(static_cast<unsigned char>(c0 + 0xe0));   // 0x00434f11 / 0x00434f15 / 0x00434f17
                strcat(text, reinterpret_cast<const char*>(0x004c52b8));     // 0x00434f1c..0x00434f4c
                const char* src;
                if (G<unsigned>(0x00822c70) & (1u << (r & 31))) src = reinterpret_cast<const char*>(0x004c82c0);   // 0x00434f4e..0x00434f5a
                else src = reinterpret_cast<const char*>(kItoa(add(mul(add(mul(r, 5), 0x19), 200), 0), 0x005a9f6c, 10));   // 0x00434f61..0x00434f7b
                strcat(text, src);                                           // 0x00434f80..0x00434fad
                kDrawPanelBackdrop(a, sub(b, 0xc));                          // 0x00434fb5
                int k = r & 3;                                               // 0x00434fba
                strcpy(text, reinterpret_cast<const char*>(kAmenityTitle[k]));   // 0x00434fc5 (cases 0x434fcc / 0x434fee / 0x434ff5 / 0x435017)
                kDrawPanelBackdrop(a, sub(b, 2));                            // 0x00435049
            }
        }
        ebxImage();                                                          // 0x00435086
        M = G<int>(0x004c2854);                                              // 0x0043508b
    }
    if (M == 0x13) {                                                         // 0x00435090
        int r = kDrawAmenityRowCall(-0x10, 0x226, 8, 1, 0xa9, 4, a, b, 0x28, 0, 0);   // 0x004350af
        if (r != -1) G<int>(0x005a9f50) = r;                                 // 0x004350b7 / 0x004350bc
        ebxImage(); M = G<int>(0x004c2854);                                  // 0x004350e9 / 0x004350ee
    }
    if (M == 2) {                                                            // 0x004350f3
        int r = kDrawAmenityRowCall(0xd, 0x1a3, 5, 0, 0x2d, -6, a, b, 0x28, 0, 0);   // 0x0043510f
        if (r != -1) G<int>(0x005a9f50) = r;                                 // 0x00435117 / 0x0043511c
        ebxImage(); M = G<int>(0x004c2854);                                  // 0x0043514a / 0x0043514f
    }
    if (M == 0x10) {                                                         // 0x00435154
        int r = kDrawAmenityRowCall(0x64, 0x12f, 7, 1, 0xb4, 2, a, b, 0x64, 0, 0);   // 0x00435173
        if (r != -1) G<int>(0x005a9f50) = r;                                 // 0x0043517b / 0x00435180
        ebxImage();                                                          // 0x004351ae
    }
    kSetDrawTarget(reinterpret_cast<void*>(G<int>(0x004c1570)), 0, 0x00519fd8, 0, 0, 0);   // 0x004351c4
    if (L != G<int>(0x005a9f54)) {                                           // 0x004351c9 / 0x004351cf
        G<int>(0x005aa554) = 0;                                              // 0x00435514
        G<int>(0x005a9f54) = L;                                              // 0x0043551e
        return;
    }
    int cnt = add(G<int>(0x005aa554), 1);                                    // 0x004351d5 / 0x004351db
    G<int>(0x005aa554) = cnt;                                                // 0x004351df
    if (L == -1) return;                                                     // 0x004351dc / 0x004351e5
    if (cnt <= 10) return;                                                   // 0x004351ed / 0x004351f0
    text[0] = 0;                                                             // 0x004351fa
    int y = G<short>(0x004c7a9a + 4u * static_cast<unsigned>(L));            // 0x00435209 (ebx)
    int x = G<short>(0x004c7a98 + 4u * static_cast<unsigned>(L));            // 0x00435201 (edx)
    int ebp = 0;                                                             // 0x00435214
    unsigned ci = static_cast<unsigned>(add(L, 2));                          // 0x00435211
    if (ci <= 0xb) {                                                         // 0x00435216 / 0x0043521d
        const AmenityCase& cs = kAmenityCase[ci];
        if (ci == 0) { x = a; y = b; }                                       // 0x0043522a..0x0043524b
        if (cs.str) strcpy(text, reinterpret_cast<const char*>(cs.str));     // 0x0043522a..0x00435259 and the other cases
        if (cs.kind == 1) ebp = G<short>(cs.src);                            // 0x0043527e / 0x004352af / ...
        else if (cs.kind == 2) ebp = G<signed char>(cs.src);                 // 0x004353a4
    }
    int len3 = mul(static_cast<int>(strlen(text)), 3);                       // 0x00435409..0x00435424
    int x1 = add(add(len3, x), 0x14);                                        // 0x00435428
    int x0 = add(sub(x, len3), 0x14);                                        // 0x0043542c / 0x00435430
    int y0 = add(y, 0xa);                                                    // 0x0043541a
    kDrawMapLine7(x0, y0, x1, y0, static_cast<int>(0x80000000), 0xa, 5);    // 0x00435434
    int tx = add(x, 0x14);                                                   // 0x00435446
    kDrawCenteredText(0x0051a068, tx, add(y, 5), 0x80007fff);                // 0x00435453
    if (ebp == 0) return;                                                    // 0x0043545b / 0x0043545d
    int y1 = add(y, 0x14);                                                   // 0x00435465
    kDrawMapLine7(x0, y1, x1, y1, static_cast<int>(0x80000000), 0xa, 5);    // 0x00435473
    text[0] = 0;                                                             // 0x00435483
    const char* s = reinterpret_cast<const char*>(kItoa(mul(ebp, 100), 0x005a9f6c, 10));   // 0x00435491
    strcat(text, s);                                                         // 0x004354ba
    if (L == 4) strcpy(text, reinterpret_cast<const char*>(0x004c8200));     // 0x004354c5 / 0x004354cc
    kDrawCenteredText(0x0051a068, tx, add(y, 0xf), 0x80007fff);              // 0x00435504
}
typedef void(__cdecl* C2v_t)(int, int);
C2v_t DrawAmenitiesPanel_orig;
// 0x00434cf0
SG_HOOK("golf_clean.exe", 0x00434cf0, drawAmenitiesPanel, DrawAmenitiesPanel_re, DrawAmenitiesPanel_orig);

// ====================================================================================================================
// formatGolferTitle (the golfer sprite compositor)
// ====================================================================================================================
namespace {
// vtable slot of an object with three stack arguments (thiscall): [[obj] + off](obj, a, b, c)
static inline int vslot3(int obj, unsigned off, int a, int b, int c) {
    void* fn = *reinterpret_cast<void**>(*reinterpret_cast<char**>(obj) + off);
    return reinterpret_cast<T3_t>(fn)(reinterpret_cast<void*>(obj), 0, a, b, c);
}
// the compositor object [0x81ca14]; when it is NULL the object at 0x81ca10 creates it through its vtable slot 0 (ecx = 0x81ca10)
static inline int compositor() {
    int ecx = G<int>(0x0081ca14);
    if (ecx == 0) {
        void* fn = *reinterpret_cast<void**>(*reinterpret_cast<char**>(0x0081ca10));    // mov eax, [0x81ca10]; call [eax]
        reinterpret_cast<T0_t>(fn)(reinterpret_cast<void*>(0x0081ca10), 0);
        ecx = G<int>(0x0081ca14);
    }
    return ecx;
}
// one layer: the part object (NULL skips its slot 0x10 call) renders into the work buffer, then the compositor places a buffer
static inline int layer(int obj, int n10, unsigned char* work, unsigned char* buf, int c1, int c2) {
    if (obj) vslot3(obj, 0x10, reinterpret_cast<int>(work), 0, n10);
    int comp = compositor();
    return vslot3(comp, 0x14, reinterpret_cast<int>(buf), c1, c2);
}
static inline int partAt(int idx) { return G<int>(0x0081ca6c + 88u * static_cast<unsigned>(idx)); }
}  // namespace

// 0x00462020  formatGolferTitle(g): cdecl, one argument. The frame is `sub esp, 0x31c` plus four pushes; the work buffer is at [esp + 0x2c], the other
// layer buffers at [esp + 0x68], 0xa4, 0xe0, 0x16d, 0x19a, 0x1b8, 0x11c, 0x13a, 0x1dc (the lea operands of the pushes, offsets from the frame base used below).
// F1 = 0, F2 = 1 (0x0046203c / 0x00462044). g < 0x9c (signed, 0x00462035): F2 = typeBit7Clear(g) (0x0046204b); g == 0xff becomes 0 (0x00462055); t = the word
// [(g << 8) + 0x57956e]; D = t % 10, A = 3 * t % 10 (idiv 0x00462077 / 0x00462081), r = 560 * t; app = the signed byte [r + 0x4d60aa]; app < 0x13 (0x0046209e):
// B = the signed byte [0x4d55e8 + 68 * app] when F2 else [0x4d5b38 + 68 * app] (0x004620ae / 0x004620c7), else B = t % 4 (0x004620d1..0x004620de); flags =
// [(g << 8) + 0x5794d1]: ebx = A when flags & 4 else (F2 ? 0 : 9) (0x004620e3..0x004620f8); C = B, or 4 when flags & 2 (0x004620fb..0x00462107); F2 == 0:
// A = 2 * t % 3 + 1 (0x00462113..0x0046211f). Second pass (0x00462459): Ev = the signed byte [0x4d55eb + 68 * (app + (typeBit7Clear(g) ? 0 : 0x14))]
// (0x0046248d..0x0046249e); the dword [r + 0x4d60b4] != 0: A, D, ebx, B, C, Ev come from the bytes [r + 0x4d60ac / ad / ab (& 0xf) / af / ae / b0]
// (0x004624ac..0x004624e8); [(g << 8) + 0x5794d0] == 0x20: C = 4, F2 = 1 and A, B, D, ebx from the 56-byte records at 0x58dd71 + 56 * [(g << 8) + 0x579573]
// (0x004624f5..0x0046253d). F1 == 0: the type record bytes [r + 0x4d60ab] (low nibble), ac, ad, af, ae, b0 are written back from ebx, A, D, B, C, Ev (0x00462549..
// 0x00462586). g >= 0x9c (0x004620b8 / 0x00462134): F1 = 1, C = 4, F2 = 1 and B, ebx (+ 5), A, D come from the bytes 0x55c0cd + 37 * g (+0, +1, +2, +3) (0x0046214b..
// 0x00462179). F2 == 0: A, B, C, D, Ev and ebx grow by 0x8b and layer 1 uses the table 0x81fa34 (0x00462598..0x00462611). Layers: object slot 0x10 from the part tables
// 0x81ca6c + 88 * index and the compositor [0x81ca14] slot 0x14 (the compositor is created through [0x81ca10] when NULL): (obj 0, 0x78 | bufA 0, 0x78), (A, 0x14
// | bufA 0, 0x14), (D, 0x28 | 0x68: 0x14, 0x14), (B, 0x3c | 0xa4: 0x28, 0x14), (ebx, 0x50 | 0xe0: 0x3c, 0x14); F2: (A, 0x79 | 0x16d: 0x6b, 0xf), (D, 0x83 | 0x19a: 0x7a, 0xa),
// (B, 0x88 | 0x1b8: 0x84, 0x32), F1 == 0: (C, 0x5a | 0x11c: 0x50, 7); not F2: (Ev, 0x5a | 0x11c: 0x50, 0xa), (C, 0x64 | 0x13a: 0x5a, 7) and the object 0x81fa34 + 88 * (g & 1)
// slot 0x10 with 0xac. Last layer (0x004626f5): 0 <= g < 0x98 and bits 0..2 of the byte [0x5849e2 + 44 * t] >= 4: object [0x81fa8c], else [0x81fa34], slot 0x10
// with 0xac, then the compositor slot 0x14 with (0x1dc: 0x90, 0x1c). The result is the last compositor call's eax.
int __cdecl FormatGolferTitle_re(int g0) {
    alignas(16) unsigned char frame[0x330];
    unsigned char* bufA = frame + 0x2c;
    int g = g0;
    int F1 = 0, F2 = 1;                                                      // 0x0046203c / 0x00462044
    int A = 0, B = 0, C = 0, D = 0, Ev = 0, ebx = 0;
    if (g < 0x9c) {                                                          // 0x00462035 / 0x00462048
        F2 = kTypeBit7Clear(g);                                              // 0x0046204b
        if (g == 0xff) g = 0;                                                // 0x00462055
        unsigned rec = static_cast<unsigned>(g) << 8;                        // 0x0046206a
        int t = G<short>(0x0057956e + rec);                                  // 0x0046206d
        D = t % 10;                                                          // 0x00462077
        A = mul(t, 3) % 10;                                                  // 0x00462081
        unsigned r = 560u * static_cast<unsigned>(t);                        // 0x00462083..0x0046208f
        int app = G<signed char>(0x004d60aa + r);                            // 0x00462092
        if (app < 0x13) {                                                    // 0x00462098 / 0x0046209e
            unsigned o = 68u * static_cast<unsigned>(app);                   // 0x004620a9 / 0x004620ac
            B = G<signed char>((F2 != 0 ? 0x004d55e8u : 0x004d5b38u) + o);   // 0x004620ae / 0x004620c7
        } else {
            int m = t & static_cast<int>(0x80000003);                        // 0x004620d1..0x004620d3
            if (m < 0) m = ((m - 1) | static_cast<int>(0xfffffffc)) + 1;     // 0x004620da..0x004620de
            B = m;
        }
        unsigned fl = G<unsigned char>(0x005794d1 + rec);                    // 0x004620e3
        ebx = A;                                                             // 0x004620eb
        if (!(fl & 4)) ebx = (F2 != 0) ? 0 : 9;                              // 0x004620ed..0x004620f8
        C = B;                                                               // 0x004620fb
        if (fl & 2) C = 4;                                                   // 0x004620ff / 0x00462107
        if (F2 == 0) A = (2 * t) % 3 + 1;                                    // 0x0046210f..0x0046211f
        // second pass 0x00462459
        int res2 = kTypeBit7Clear(g);                                        // 0x00462475
        int off2 = 0x14;                                                     // 0x00462484..0x0046248a
        if (res2 != 0) off2 = 0;
        int idx2 = add(G<signed char>(0x004d60aa + r), off2);                // 0x0046248d
        Ev = G<signed char>(0x004d55eb + 68u * static_cast<unsigned>(idx2)); // 0x00462496
        if (G<unsigned>(0x004d60b4 + r) != 0) {                              // 0x004624a2
            A = G<signed char>(0x004d60ac + r);                              // 0x004624ac
            D = G<signed char>(0x004d60ad + r);                              // 0x004624b3
            B = G<signed char>(0x004d60af + r);                              // 0x004624ba
            ebx = G<unsigned char>(0x004d60ab + r) & 0xf;                    // 0x004624c1 / 0x004624d6
            C = G<signed char>(0x004d60ae + r);                              // 0x004624cb
            Ev = G<signed char>(0x004d60b0 + r);                             // 0x004624d9
        }
        if (G<unsigned char>(0x005794d0 + rec) == 0x20) {                    // 0x004624ec
            C = 4; F2 = 1;                                                   // 0x004624f7 / 0x00462505
            unsigned o = 56u * G<unsigned char>(0x00579573 + rec);           // 0x004624ff..0x00462516
            ebx = G<signed char>(0x0058dd72 + o);                            // 0x00462519
            B = G<signed char>(0x0058dd71 + o);                              // 0x00462520
            A = G<signed char>(0x0058dd73 + o);                              // 0x00462527
            D = G<signed char>(0x0058dd74 + o);                              // 0x0046252e
        }
        if (F1 == 0) {                                                       // 0x00462541
            unsigned char old = G<unsigned char>(0x004d60ab + r);            // 0x00462549
            G<unsigned char>(0x004d60ab + r) = static_cast<unsigned char>((old & 0xf0) | (ebx & 0xff));   // 0x0046255c
            G<unsigned char>(0x004d60ac + r) = static_cast<unsigned char>(A);   // 0x00462566
            G<unsigned char>(0x004d60ad + r) = static_cast<unsigned char>(D);   // 0x00462570
            G<unsigned char>(0x004d60af + r) = static_cast<unsigned char>(B);   // 0x0046257a
            G<unsigned char>(0x004d60ae + r) = static_cast<unsigned char>(C);   // 0x00462580
            G<unsigned char>(0x004d60b0 + r) = static_cast<unsigned char>(Ev);  // 0x00462586
        }
    } else {
        F1 = 1; C = 4; F2 = 1;                                               // 0x004620b8 / 0x0046213f / 0x00462147
        unsigned o = 37u * static_cast<unsigned>(g);                         // 0x00462134..0x0046214b
        B = G<signed char>(0x0055c0cd + o);                                  // 0x00462152
        ebx = add(G<signed char>(0x0055c0ce + o), 5);                        // 0x00462159..0x00462164
        A = G<signed char>(0x0055c0cf + o);                                  // 0x00462167 (byte +2)
        D = G<signed char>(0x0055c0d0 + o);                                  // 0x0046216e (byte +3)
    }
    int last = 0;
    if (F2 == 0) {                                                           // 0x0046258c / 0x00462590
        B = add(B, 0x8b); D = add(D, 0x8b); Ev = add(Ev, 0x8b); ebx = add(ebx, 0x8b); C = add(C, 0x8b); A = add(A, 0x8b);   // 0x00462598..0x004625d5
        last = layer(G<int>(0x0081fa34), 0x78, bufA, bufA, 0, 0x78);         // 0x004625e6 / 0x00462611
    } else {
        last = layer(G<int>(0x0081ca6c), 0x78, bufA, bufA, 0, 0x78);         // 0x00462192 / 0x004621bc
    }
    int Aidx = A;                                                            // 0x004621bf
    last = layer(partAt(Aidx), 0x14, bufA, bufA, 0, 0x14);                   // 0x004621e3 / 0x0046220e
    last = layer(partAt(D), 0x28, bufA, frame + 0x68, 0x14, 0x14);           // 0x00462231 / 0x0046225c
    last = layer(partAt(B), 0x3c, bufA, frame + 0xa4, 0x28, 0x14);           // 0x0046227f / 0x004622ad
    last = layer(partAt(ebx), 0x50, bufA, frame + 0xe0, 0x3c, 0x14);         // 0x004622d3 / 0x00462301
    if (F2 != 0) {                                                           // 0x00462304 / 0x00462308
        last = layer(partAt(Aidx), 0x79, bufA, frame + 0x16d, 0x6b, 0xf);    // 0x0046232a / 0x00462358
        last = layer(partAt(D), 0x83, bufA, frame + 0x19a, 0x7a, 0xa);       // 0x00462374 / 0x004623a1
        last = layer(partAt(B), 0x88, bufA, frame + 0x1b8, 0x84, 0x32);      // 0x004623bd / 0x004623ee
        if (F1 == 0) last = layer(partAt(C), 0x5a, bufA, frame + 0x11c, 0x50, 7);   // 0x004623f7 / 0x00462424 / 0x00462451
    } else {
        last = layer(partAt(Ev), 0x5a, bufA, frame + 0x11c, 0x50, 0xa);      // 0x00462640 / 0x0046266d
        last = layer(partAt(C), 0x64, bufA, frame + 0x13a, 0x5a, 7);         // 0x00462697 / 0x004626c4
        int o2 = G<int>(0x0081fa34 + 88u * static_cast<unsigned>(g & 1));    // 0x004626c7..0x004626d2
        if (o2) vslot3(o2, 0x10, reinterpret_cast<int>(bufA), 0, 0xac);      // 0x004626f2
    }
    if (g >= 0 && g < 0x98) {                                                // 0x004626f5 / 0x004626f9
        int t2 = G<short>(0x0057956e + (static_cast<unsigned>(g) << 8));     // 0x00462704
        unsigned char b = G<unsigned char>(0x005849e2 + 44u * static_cast<unsigned>(t2)) & 7;   // 0x00462711 / 0x00462718
        if (static_cast<signed char>(b) >= 4) {                              // 0x0046271a
            last = layer(G<int>(0x0081fa8c), 0xac, bufA, frame + 0x1dc, 0x90, 0x1c);   // 0x00462736 / 0x00462767
            return last;
        }
    }
    last = layer(G<int>(0x0081fa34), 0xac, bufA, frame + 0x1dc, 0x90, 0x1c); // 0x0046278d / 0x004627bd
    return last;
}
C1_t FormatGolferTitle_orig;
// 0x00462020
SG_HOOK("golf_clean.exe", 0x00462020, formatGolferTitle, FormatGolferTitle_re, FormatGolferTitle_orig);
