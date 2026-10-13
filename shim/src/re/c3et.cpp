// C3 batch c3et (round 32, 2026-10-13) of golf_clean.exe: the large screens and round 31's drops.
// Fixtures: re/frida/js/fixtures.d/c3et.js. Registry: re/frida/registry.d/c3et.py.
// Purposes and per-jcc tables: log/c3/c3et_purpose.md. Notes and drops: log/c3/c3et_notes.txt.
//
// Every body is hand-written from the disassembly (log/c3/alt/c3et_decomp/<addr>_<name>.asm, from
// py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and cites the instruction each fact comes from.
// __thiscall is emulated with __fastcall (ecx = this, edx unused); callees run through their original addresses, so a
// hooked callee uses its own reimplementation during the A/B (the verifier re-runs these keys with those hooks off).
#include "hooks.h"

#include <string.h>
#include <intrin.h>

namespace {

template <class T>
static inline T& At(const void* p, unsigned off) {
    return *reinterpret_cast<T*>(const_cast<char*>(static_cast<const char*>(p)) + off);
}
template <class T>
static inline T& G(unsigned va) { return *reinterpret_cast<T*>(va); }
// slot `off` of the vtable of `obj`
static inline void* Slot(const void* obj, unsigned off) { return At<void*>(At<void*>(obj, 0), off); }

typedef void*(__cdecl* Malloc_t)(unsigned n);
typedef void(__cdecl* Free_t)(void* p);
typedef int(__fastcall* Measure_t)(void* ecx, void* edx, const char* s, int n);
typedef int(__cdecl* TextCb_t)(const char* text, int x, int y, int len);
typedef int(__fastcall* V0_t)(void* ecx, void* edx);
typedef int(__fastcall* V1_t)(void* ecx, void* edx, unsigned a);
typedef int(__fastcall* V3_t)(void* ecx, void* edx, unsigned a, unsigned b, unsigned c);
typedef int(__fastcall* V4_t)(void* ecx, void* edx, int a, int b, const char* c, int d);

const Malloc_t kMalloc = reinterpret_cast<Malloc_t>(0x004a504f);              // 0x00476816
const Free_t kFree = reinterpret_cast<Free_t>(0x004a5007);                    // 0x00476cf5
const Measure_t kMeasureTextWidth = reinterpret_cast<Measure_t>(0x00483930);  // 0x00476c3e, 0x00476cb8, 0x00476d12

static inline int V0(void* o, unsigned off) { return reinterpret_cast<V0_t>(Slot(o, off))(o, 0); }
static inline int V1(void* o, unsigned off, unsigned a) { return reinterpret_cast<V1_t>(Slot(o, off))(o, 0, a); }
static inline int V3(void* o, unsigned off, unsigned a, unsigned b, unsigned c) {
    return reinterpret_cast<V3_t>(Slot(o, off))(o, 0, a, b, c);
}
static inline int V4(void* o, unsigned off, int a, int b, const char* c, int d) {
    return reinterpret_cast<V4_t>(Slot(o, off))(o, 0, a, b, c, d);
}

// ====================================================================================================================
// 0x004767a0  allocTextBuffer (the text-run drawer of the text object at [0x004c1570]; name proposed in
// log/c3/c3et_names.tsv)
// ====================================================================================================================
// thiscall (this, text, x, y, len), `ret 0x10` (0x004767ec, 0x00476d29, 0x00476d3c). The prologue keeps `this` in ebp
// (0x004767a8) and has no frame pointer; stack offsets below are given relative to esp after the three pushes
// (0x004767a6..0x004767aa), so the arguments are [esp+0x128] text, [+0x12c] x, [+0x130] y, [+0x134] len. Between
// 0x00476810 (push edi) and 0x00476cef (pop edi) every [esp+N] reads the slot N - 4.
//
// Object fields read: +4 the surface it draws on, +0xc an override callback, +0x20 a flag byte (bit 2), +0x30 the
// state index, +0x40 the per-state switch, +0x5c fonts[4] (+0x10 of a font is its height), +0x6c colours[4],
// +0x7c shadow colours[4], +0x8c shadow x offsets[4], +0x9c shadow y offsets[4].
//
// Surface slots used: +0xa8 setFont(font + 4) (0x00476944), +0xac end (0x00476ce5), +0xb0 colour by index
// (0x00476aa1, 0x00476bc0), +0xb4 colour by components (0x004769f6, 0x00476a44, 0x00476b6f, 0x00476bac), +0xb8
// draw(x, y, text, count) (0x00476ade, 0x00476c26, 0x00476ca0), +0xe4 the pixel format (0x00476865), +0xe8 its palette
// (0x0047688f, 0x0047689e). Display [0x0083ad50] slot +0xb4 is the format test (== 1) of a direct colour.
typedef int(__fastcall* AllocTextBuffer_t)(void* ecx, void* edx, const char* text, int x, int y, int len);
AllocTextBuffer_t AllocTextBuffer_orig;

// One colour call, shared by the shadow (first) and the main (second) colour blocks: 0x0047696b..0x00476a9b and
// 0x00476ae8..0x00476bc0. The two blocks differ only in their last arm (see the comment there).
static void SetRunColour(void* surf, void* pal, int c, int flags, bool first) {
    if (c & 0x80000000) {                                   // 0x0047696b / 0x00476ae8 test ebx, 0x80000000
        // two display queries, the first for the green component, the second for the red one; the result == 1 picks
        // the 5-6-5 split, any other the 5-5-5 split (0x0047697f / 0x00476afc and 0x004769ac / 0x00476b28, cmp eax, 1)
        void* d1 = G<void*>(0x0083ad50);
        const unsigned g = (V0(d1, 0xb4) == 1) ? ((c >> 3) & 0xfc) : ((c >> 2) & 0xf8);   // sar 3 / and 0xfc, sar 2 / and 0xf8
        void* d2 = G<void*>(0x0083ad50);
        const unsigned r = (V0(d2, 0xb4) == 1) ? ((c >> 8) & 0xf8) : ((c >> 7) & 0xf8);   // sar 8 / sar 7, and 0xf8
        const unsigned b = static_cast<unsigned char>(static_cast<unsigned char>(c) << 3);  // shl bl, 3; and 0xff
        V3(surf, 0xb4, r, g, b);                            // pushes b, g, r (0x004769e9..0x004769f5)
        return;
    }
    // 0x00476a01 / 0x00476b77 test edi, edi. The first block also takes the palette arm when the palette is NULL and
    // bit 2 of +0x20 is set (0x00476a4c..0x00476a72): there it reads [edi] = [0] (0x00476a5a) and faults. The second
    // block skips the colour call in that case instead (0x00476bb4 / 0x00476bb8 jne 0x476bc6).
    if (pal != 0 || (first && (flags & 4) != 0)) {
        // three lookups of the same index, components 2, 1, 0 in that order (0x00476a13 [eax+2], 0x00476a23 [eax+1],
        // 0x00476a35 [eax]), passed as (c0, c1, c2) (0x00476a44)
        const unsigned c2 = reinterpret_cast<unsigned char*>(V1(pal, 0x20, c))[2];
        const unsigned c1 = reinterpret_cast<unsigned char*>(V1(pal, 0x20, c))[1];
        const unsigned c0 = reinterpret_cast<unsigned char*>(V1(pal, 0x20, c))[0];
        V3(surf, 0xb4, c0, c1, c2);
        return;
    }
    if (first || (flags & 4) == 0) V1(surf, 0xb0, c);       // 0x00476aa1 / 0x00476bc0
}

int __fastcall AllocTextBuffer_re(void* self, void*, const char* text, int x, int y, int len) {
    // 0x004767b3 / 0x004767b8: the override callback gets the four arguments unchanged (pushes 0x004767d6..0x004767d9)
    // and a non-zero result is returned as it is (0x004767df / 0x004767e1)
    TextCb_t cb = At<TextCb_t>(self, 0xc);
    if (cb != 0) {
        const int r = cb(text, x, y, len);
        if (r != 0) return r;
    }
    // 0x004767fd / 0x004767ff len == 0 and 0x00476805 / 0x0047680a no surface: return x (0x00476d2c)
    if (len == 0) return x;
    void* surf = At<void*>(self, 4);
    if (surf == 0) return x;

    const char* const original = text;                        // 0x00476812 [esp+0x1c]
    char* const buf = static_cast<char*>(kMalloc(len));       // 0x00476811 push ebx (len), 0x0047681e [esp+0x18]
    if (buf != 0) {                                           // 0x00476822 / 0x00476824
        memcpy(buf, text, static_cast<unsigned>(len));        // 0x00476826..0x00476836 rep movsd / rep movsb
        for (int i = 0; i < len; ++i)                         // 0x00476838..0x0047684b
            if (buf[i] == '_') buf[i] = ' ';                  // 0x0047683e cmp 0x5f / 0x00476844 mov 0x20
        text = buf;                                           // 0x0047684d [esp+0x128] = buf
    }

    // the pixel format: surface slot +0xe4 returns a pointer whose first dword is compared with 0x10 (0x0047686f)
    void* pal = 0;
    int palFromGlobal = 0;                                    // [esp+0x10] = 0 at 0x004767ab, 1 at 0x00476880
    if (*reinterpret_cast<int*>(V0(surf, 0xe4)) == 0x10) {
        void* g = G<void*>(0x0083ad18);                       // 0x00476874
        if (g != 0) {
            pal = At<void*>(g, 4);                            // 0x0047687d
            palFromGlobal = 1;
        } else if (V0(surf, 0xe8) != 0) {                     // 0x0047688f / 0x00476895
            pal = reinterpret_cast<void*>(V0(surf, 0xe8));    // the second call's result (0x0047689e / 0x004768a4)
        } else {
            pal = At<void*>(G<void*>(0x0083ad0c), 4);         // 0x004768a8 / 0x004768ae
        }
    }

    // font index fi ([esp+0x14]) and colour index ci (esi)
    int fi, ci;
    if (At<int>(self, 0x40) == 0) {                           // 0x004768b5 / 0x004768b8
        fi = 0;
        ci = 0;
        y += At<int>(At<void*>(self, 0x5c), 0x10);            // 0x004768ba..0x004768cd (font 0 is not tested)
    } else {
        const int s = At<int>(self, 0x30);                    // 0x004768d6
        fi = At<void*>(self, 0x5c + 4 * s) != 0 ? s : 0;      // 0x004768d9, neg / sbb / and 0x004768e1..0x004768e5
        ci = At<int>(self, 0x6c + 4 * s) != -1 ? s : 0;       // 0x004768dd, inc / neg / sbb / and 0x004768e7..0x004768f3
        // the tallest of fonts 0..2 (0x004768f5..0x00476922): font 0 counts only above 0, the others above the max so far
        int h = 0;
        void* f0 = At<void*>(self, 0x5c);
        if (f0 != 0 && At<int>(f0, 0x10) > 0) h = At<int>(f0, 0x10);
        void* f1 = At<void*>(self, 0x60);
        if (f1 != 0 && At<int>(f1, 0x10) > h) h = At<int>(f1, 0x10);
        void* f2 = At<void*>(self, 0x64);
        if (f2 != 0 && At<int>(f2, 0x10) > h) h = At<int>(f2, 0x10);
        y += h;                                               // 0x00476924
    }

    void* font = At<void*>(self, 0x5c + 4 * fi);              // 0x0047692f
    V1(surf, 0xa8, font != 0 ? reinterpret_cast<unsigned>(font) + 4 : 0);   // 0x00476933..0x00476944

    const int flags = At<unsigned char>(self, 0x20);
    // the shadow: skipped for -1 (0x0047694e) and, with the palette from [0x0083ad18], for 0xff (0x0047695f)
    const int shadow = At<int>(self, 0x7c + 4 * ci);          // 0x0047694a
    if (shadow != -1 && !(palFromGlobal != 0 && shadow == 0xff)) {
        SetRunColour(surf, pal, shadow, flags, true);
        // draw(x + dx[ci], y + dy[ci], text, len) (0x00476aa7..0x00476ade)
        V4(surf, 0xb8, x + At<int>(self, 0x8c + 4 * ci), y + At<int>(self, 0x9c + 4 * ci), text, len);
    }
    // the main colour (0x00476ae4), never tested for -1
    SetRunColour(surf, pal, At<int>(self, 0x6c + 4 * ci), flags, false);

    // the runs between tabs, each copied into the frame buffer [esp+0x24] (0x104 bytes, no bound: a run longer than
    // that overwrites the frame) and drawn at the pen; a tab moves the pen to the next multiple of 30
    char seg[0x104];
    int pen = x;                                              // 0x00476bcd esi = [esp+0x130]
    if (len >= 0) {                                           // 0x00476bd8 / 0x00476bda jl
        int start = 0;                                        // edi (0x00476bd4)
        int i = 0;                                            // eax (0x00476bd6)
        do {
            const bool end = (i == len);                      // 0x00476be0 / 0x00476be2
            if (end || text[i] == '\t') {                     // 0x00476c56 cmp byte [ecx + ebx - 1], 9
                const int n = i - start;                      // 0x00476be4..0x00476bec / 0x00476c5d..0x00476c65
                for (int k = start; k < i; ++k) seg[k - start] = text[k];   // 0x00476bf2..0x00476c07, 0x00476c6b..0x00476c80
                V4(surf, 0xb8, pen, y, seg, n);               // 0x00476c26 / 0x00476ca0
                pen += kMeasureTextWidth(At<void*>(self, 0x5c + 4 * fi), 0, seg, n);   // 0x00476c3e / 0x00476cb8, 0x00476ccd / 0x00476cbd
                if (!end) pen += 30 - pen % 30;               // 0x00476cbf..0x00476ccd (cdq / idiv 0x1e)
                start = i + 1;                                // 0x00476c22 / 0x00476c92 mov edi, ebx
            }
            ++i;                                              // ebx = eax + 1 (0x00476c15, 0x00476c4f)
        } while (i <= len);                                   // 0x00476cd8 / 0x00476cda jle
    }

    V0(surf, 0xac);                                           // 0x00476ce5
    if (buf != 0) kFree(buf);                                 // 0x00476cf0..0x00476cf5
    // x + the width of the ORIGINAL text (0x00476d04 [esp+0x1c]) in the chosen font (0x00476d12, 0x00476d20)
    return x + kMeasureTextWidth(At<void*>(self, 0x5c + 4 * fi), 0, original, len);
}

// ====================================================================================================================
// 0x00459850  showGolferPairSelect
// ====================================================================================================================
// __cdecl, no arguments, `ret` (0x00459feb, 0x0045a08f). Lists the golfers whose record byte +0x21 is 0xff
// (0x004599c8), lets the player toggle up to two of them with clicks and, when exactly two are selected at the exit,
// moves them into the adjacent record pair (lastGolfer, lastGolfer ^ 1) with swapTableEntry, flags both and pairs them.
// Golfer records: 0x100 bytes from 0x005794b8 (swapTableEntry 0x0045de30's base); the fields this function reads are
// +0x10 (dword, |= 0x20000 at 0x0045a05c / 0x0045a075), +0x21 (0x005794d9), +0xb6 the type word (0x0057956e) and +0xb8
// the flag word (0x00579570). Golfer-type records: 0x230 bytes from 0x004d6088 (name at +0, trait bits +0x20, image
// column +0x22).
typedef int(__cdecl* I0_t)();
typedef int(__cdecl* I1_t)(int a);
typedef int(__cdecl* I2_t)(int a, int b);
typedef int(__cdecl* I5_t)(int a, int b, int c, int d, int e);
typedef int(__cdecl* Text4_t)(const char* t, int x, int y, int colour);
typedef char*(__cdecl* Itoa_t)(int v, char* buf, int radix);
typedef int(__fastcall* T1_t)(void* ecx, void* edx, int a);
typedef int(__fastcall* T2p_t)(void* ecx, void* edx, int* a, int* b);
typedef int(__fastcall* T4_t)(void* ecx, void* edx, unsigned a, unsigned b, unsigned c, unsigned d);
typedef int(__fastcall* T6_t)(void* ecx, void* edx, unsigned a, unsigned b, unsigned c, unsigned d, unsigned e,
                              unsigned f);
typedef int(__fastcall* T7_t)(void* ecx, void* edx, unsigned a, unsigned b, unsigned c, unsigned d, unsigned e,
                              unsigned f, unsigned g);
typedef int(__fastcall* T9_t)(void* ecx, void* edx, unsigned a, unsigned b, unsigned c, unsigned d, unsigned e,
                              unsigned f, unsigned g, unsigned h, unsigned i);

const T6_t kSurfaceDrawSprite = reinterpret_cast<T6_t>(0x00474440);     // 0x00459882, ret 0x18
const T4_t kImageDraw = reinterpret_cast<T4_t>(0x00473e60);             // 0x004598ac, 0x00459a4b
const T4_t kSetDrawTarget = reinterpret_cast<T4_t>(0x004762d0);         // 0x004598bf, 0x00459b51, 0x00459b93, ...
const Text4_t kDrawCenteredText = reinterpret_cast<Text4_t>(0x00404b70);  // 0x004598d5, 0x00459b7a, ...
const Text4_t kDrawText4049d0 = reinterpret_cast<Text4_t>(0x004049d0);  // 0x00459e24
const I1_t kTypeBit7Clear = reinterpret_cast<I1_t>(0x0046c940);         // 0x00459a6c, 0x00459ad0
const T7_t kSurfaceDrawScaled = reinterpret_cast<T7_t>(0x00473cb0);     // 0x00459ac2
const T9_t kSurfaceDraw = reinterpret_cast<T9_t>(0x00474380);           // 0x00459b25, ret 0x24
const I2_t kBuildGolferName = reinterpret_cast<I2_t>(0x004676e0);       // 0x00459b60
const I1_t kGolferScore = reinterpret_cast<I1_t>(0x00453260);           // 0x00459cf2
const Itoa_t kItoa = reinterpret_cast<Itoa_t>(0x004ad425);              // 0x00459cfb
const T1_t kRefreshFull = reinterpret_cast<T1_t>(0x00480c80);           // 0x00459e67  Window::refreshFull
const I2_t kWaitTicks = reinterpret_cast<I2_t>(0x0045bf80);             // 0x00459e70, 0x00459e81
const I0_t kEnterModal = reinterpret_cast<I0_t>(0x00483d30);            // 0x00459e78  enterModal483d30
const T2p_t kViewToLocal = reinterpret_cast<T2p_t>(0x0047ab50);         // 0x00459e9c  View_toLocal
const I0_t kPumpMessages = reinterpret_cast<I0_t>(0x0045c030);          // 0x00459f0c
const I0_t kFlushRedraw = reinterpret_cast<I0_t>(0x0045ae70);           // 0x00459f1a
const I5_t kPlaySound = reinterpret_cast<I5_t>(0x004481b0);             // 0x00459f9b, 0x00459fd9
const I2_t kSwapTableEntry = reinterpret_cast<I2_t>(0x0045de30);        // 0x0045a022, 0x0045a02e, 0x0045a03f
const I2_t kPairGolfersWithStory = reinterpret_cast<I2_t>(0x0045de80);  // 0x0045a07d

char* const kText = reinterpret_cast<char*>(0x0051a068);
// `mov eax, 0x519a60; neg eax; sbb eax, eax; and eax, 0x519cd4` (0x00459858..0x0045986d, again at 0x0045988f..
// 0x0045989b and 0x004599fa..0x00459a1e): the immediate is not zero, so the target is always 0x00519cd4.
const unsigned kFill = 0x00519cd4;
const unsigned kBlack = 0x80000000;

static inline char* GolferRec(int g) { return reinterpret_cast<char*>(0x005794b8 + (g << 8)); }
static inline const char* TypeRec(int g) {
    return reinterpret_cast<const char*>(0x004d6088 + At<short>(GolferRec(g), 0xb6) * 0x230);
}

typedef int(__cdecl* ShowGolferPairSelect_t)(int unused);
ShowGolferPairSelect_t ShowGolferPairSelect_orig;

int __cdecl ShowGolferPairSelect_re(int) {
    unsigned mask = 0;          // [esp+0x18] (0x00459873), bit k = drawn slot k selected
    int count = 0;              // [esp+0x1c] (0x00459877)
    int listed[0x98];           // [esp+0x48]: the golfer drawn in slot k (0x00459b35)
    int lastGolfer = 0;         // [esp+0x30] (0x00459a10): read only once a slot was drawn
    // Surface_drawSprite(0x00821040; 0x00519cd4, 0, 0, 0.25f, 0, 1) (pushes 0x00459863..0x0045987c, call 0x00459882)
    kSurfaceDrawSprite(reinterpret_cast<void*>(0x00821040), 0, kFill, 0, 0, 0x3e800000, 0, 1);
    int hover = -1;             // [esp+0x14] (0x00459887)

    for (;;) {                  // 0x0045988f: redraw
        kImageDraw(reinterpret_cast<void*>(0x00821120), 0, kFill, 0, 0, 0);                   // 0x004598ac
        kSetDrawTarget(G<void*>(0x004c1570), 0, 0x00821020, 0, 0, 0);                        // 0x004598bf
        kDrawCenteredText(reinterpret_cast<const char*>(0x004d374c), 0x152, 0xe, kBlack);    // 0x004598d5

        // the pair's rating [esp+0x28]: 1 unless exactly two are selected; then the number of the five trait bits
        // (+0x20, movsx 0x00459950 / 0x00459966) the two types share: under 2 -> 2, over 3 -> 0 (0x00459981..0x00459993)
        int rating = 1;                                                                       // 0x004598e4
        if (count == 2) {                                                                     // 0x004598e1 / 0x004598ec
            int first = -1, second = 0;                                                       // edx, [esp+0x44]
            for (int c = 0; c < 16; ++c)                                                      // 0x004598ff..0x00459920
                if (mask & (1u << c)) {
                    if (first == -1) first = listed[c]; else second = listed[c];
                }
            const int diff = At<signed char>(TypeRec(second), 0x20) ^ At<signed char>(TypeRec(first), 0x20);
            int same = 5;
            for (int c = 0; c < 5; ++c) if (diff & (1 << c)) --same;                          // 0x0045996f..0x0045997f
            if (same < 2) rating = 2;
            if (same > 3) rating = 0;
        }

        // the list (0x0045999b..0x00459e5a): golfers from [0x005a5a24] downwards, wrapping at 0x98
        int k = 0;                                                                            // [esp+0x20] / ebp
        for (int i = 0; i < 0x98; ++i) {
            const int g = (G<int>(0x005a5a24) - i + 0x98) % 0x98;                             // 0x004599a9..0x004599bd
            char* const rec = GolferRec(g);
            if (At<unsigned char>(rec, 0x21) != 0xff) continue;                               // 0x004599c8 / 0x004599cf
            const int colX = (k & 1) ? 0x14f : 6;                                             // 0x004599d5..0x004599f7
            int rowY = (k / 2) * 0x88 + 0x32;                                                 // 0x004599e3..0x00459a05
            lastGolfer = g;                                                                   // 0x00459a10
            const unsigned sel = (1u << (k & 31)) & mask;                                     // 0x00459a1c / 0x00459a23
            const int state = sel != 0 ? 2 : (hover == k ? 1 : 0);                            // 0x00459a25..0x00459a37
            // Image_draw(0x0082114c + 0x2c * state; 0x00519cd4, colX, rowY, 0) (0x00459a3e..0x00459a4b)
            kImageDraw(reinterpret_cast<void*>(0x0082114c + 0x2c * state), 0, kFill, colX, rowY, 0);
            if (hover == k && sel == 0) --rowY;                                               // 0x00459a54..0x00459a60
            const char* const type = TypeRec(g);
            const int bit7Clear = kTypeBit7Clear(g);
            const int column = At<signed char>(type, 0x22) + 72 * bit7Clear;                  // 0x00459a71..0x00459aa4
            if (sel != 0) {
                // Surface_drawScaled(0x005628e8 + 0x2c * (3 * column + rating); [0x004c1570], colX - 6, rowY + 4,
                // 1, 1, 1, 0) (0x00459aa7..0x00459ac2)
                kSurfaceDrawScaled(reinterpret_cast<void*>(0x005628e8 + 0x2c * (3 * column + rating)), 0,
                                   G<unsigned>(0x004c1570), colX - 6, rowY + 4, 1, 1, 1, 0);
            } else {
                // Surface_draw(0x00562914 + 0x84 * column; [0x004c1570], colX - 6, rowY + 4, 0.7f, 1, 1, 1, 0, 1)
                // (0x00459ae6..0x00459b25)
                kSurfaceDraw(reinterpret_cast<void*>(0x00562914 + 0x84 * column), 0, G<unsigned>(0x004c1570),
                             colX - 6, rowY + 4, 0x3f333333, 1, 1, 1, 0, 1);
            }
            listed[k] = g;                                                                    // 0x00459b35
            ++k;                                                                              // 0x00459b32 / 0x00459b39

            kSetDrawTarget(G<void*>(0x004c1570), 0, 0x00821f08, 0, 0, 0);                    // 0x00459b51
            kText[0] = 0;                                                                     // 0x00459b59
            kBuildGolferName(g, 0);                                                           // 0x00459b60
            kDrawCenteredText(kText, colX + 0xd0, rowY + 9, kBlack);                          // 0x00459b7a
            kSetDrawTarget(G<void*>(0x004c1570), 0, 0x00821ee8, 0, 0, 0);                    // 0x00459b93
            strcpy(kText, type);                                                              // 0x00459bbb..0x00459bdf
            kDrawCenteredText(kText, colX + 0x100, rowY + 0x28, kBlack);                      // 0x00459bf1
            // the flag word: each set bit replaces the text (strcpy), so the highest of the four wins and with none
            // the type name is drawn a second time (0x00459bfd..0x00459caa)
            const unsigned short fl = At<unsigned short>(rec, 0xb8);
            if (fl & 0x08) strcpy(kText, reinterpret_cast<const char*>(0x004c86d4));
            if (fl & 0x10) strcpy(kText, reinterpret_cast<const char*>(0x004c86cc));
            if (fl & 0x20) strcpy(kText, reinterpret_cast<const char*>(0x004c86c0));
            if (fl & 0x40) strcpy(kText, reinterpret_cast<const char*>(0x004c86b8));
            kDrawCenteredText(kText, colX + 0x100, rowY + 0x68, kBlack);                      // 0x00459cc5
            kSetDrawTarget(G<void*>(0x004c1570), 0, 0x00821ee8, 0, 0, 0);                    // 0x00459cde
            kText[0] = 0;                                                                     // 0x00459ceb
            strcat(kText, kItoa(kGolferScore(g), reinterpret_cast<char*>(0x00821c50), 10));  // 0x00459cf2..0x00459d2d
            strcat(kText, reinterpret_cast<const char*>(0x004d3740));                         // 0x00459d2f..0x00459d5c
            kDrawCenteredText(kText, colX + 0x100, rowY + 0x48, kBlack);                      // 0x00459d6e
            kSetDrawTarget(G<void*>(0x004c1570), 0, 0x00821ee8, 0, 0, 0);                    // 0x00459d87
            // the trait lines: one per set bit of the five, from the string table 0x004c2864, centred on the space
            // left by the missing ones (0x00459da6..0x00459e45)
            int traits = At<signed char>(type, 0x20);
            int set = 0;
            for (int c = 0; c < 5; ++c) if (traits & (1 << c)) ++set;
            int y = rowY + 9 * (5 - set) + 0x24;                                              // 0x00459dc3..0x00459dd8
            for (int c = 0; c < 5; ++c) {
                if ((traits & (1 << c)) == 0) continue;
                traits -= 1 << c;                                                             // 0x00459de9
                strcpy(kText, reinterpret_cast<const char* const*>(0x004c2864)[c]);           // 0x00459de7..0x00459e1d
                kDrawText4049d0(kText, colX + 0x8c, y, kBlack);                               // 0x00459e24
                y += 0x12;                                                                    // 0x00459e30
            }
        }

        kRefreshFull(reinterpret_cast<void*>(0x00519a60), 0, 0);                              // 0x00459e67
        kWaitTicks(10, 0);                                                                    // 0x00459e70
        kEnterModal();                                                                        // 0x00459e78

        bool redraw = false;
        while (!redraw) {                                                                     // 0x00459e7d
            kWaitTicks(1, 0);                                                                 // 0x00459e81
            const int prev = hover;                                                           // edi (0x00459e86)
            int px, py;
            kViewToLocal(reinterpret_cast<void*>(0x00519a60), 0, &px, &py);                   // 0x00459e9c
            // the slot under the cursor: rows of 0x88 from y 0x32, columns of 0x14e from x 6, inside the box whose
            // right and bottom edges both come from [0x00821164] (0x00459ea5..0x00459efa)
            const int bound = G<int>(0x00821164);
            if (px >= 6 && py >= 0x32 && px < bound + 0x14f && py < bound + 0x1ca)
                hover = ((py - 0x32) / 0x88) * 2 + (px - 6) / 0x14e;
            else
                hover = -1;                                                                   // 0x00459f00
            kPumpMessages();                                                                  // 0x00459f0c
            if (G<int>(0x00822d68) == 0 && kFlushRedraw() == 0) {                             // 0x00459f11..0x00459f21
                if (hover != prev) redraw = true;                                             // 0x00459f23 / 0x00459f2b
                continue;
            }
            if (hover != prev) { redraw = true; continue; }                                   // 0x00459f30 / 0x00459f32
            if (G<int>(0x00822d68) != 0 && hover != -1) {                                     // 0x00459f38..0x00459f48
                const unsigned bit = 1u << (hover & 31);
                if (hover < k && (count < 2 || (mask & bit) != 0)) {                          // 0x00459f4a..0x00459f62
                    mask ^= bit;                                                              // 0x00459f6f
                    if (mask & bit) ++count; else --count;                                    // 0x00459f71..0x00459f88
                } else {
                    kPlaySound(0x18, 0x64, 0, 0, 0);                                          // 0x00459f9b
                }
                redraw = true;
                continue;
            }
            // the exit (0x00459fa8)
            if (mask == 0) return G<int>(0x00822d68);                                         // 0x00459fac / 0x00459fae
            int n = 0;
            for (int c = 0; c < 16; ++c) if (mask & (1u << c)) ++n;                           // 0x00459fb4..0x00459fc8
            if (n != 2) return kPlaySound(0x18, 0x64, 0, 0, 0);                               // 0x00459fca..0x00459fd9
            int first = -1, second = lastGolfer;                                              // edx, ebx (0x00459fec)
            for (int c = 0; c < 16; ++c)                                                      // 0x00459ff9..0x0045a016
                if (mask & (1u << c)) {
                    if (first == -1) first = listed[c]; else second = listed[c];
                }
            int a, b;
            if (second == lastGolfer) {                                                       // 0x0045a01c / 0x0045a01e
                kSwapTableEntry(first, lastGolfer ^ 1);                                       // 0x0045a03f
                a = lastGolfer ^ 1;
                b = second;
            } else {
                kSwapTableEntry(first, lastGolfer);                                           // 0x0045a022
                kSwapTableEntry(second, lastGolfer ^ 1);                                      // 0x0045a02e
                a = lastGolfer;
                b = lastGolfer ^ 1;
            }
            At<unsigned>(GolferRec(a), 0x10) |= 0x20000;                                      // 0x0045a050..0x0045a062
            At<unsigned>(GolferRec(b), 0x10) |= 0x20000;                                      // 0x0045a064..0x0045a07b
            return kPairGolfersWithStory(a, b);                                               // 0x0045a07d
        }
    }
}

// ====================================================================================================================
// 0x004a1580  LabelButton::drawContents
// ====================================================================================================================
// thiscall (this), no stack arguments, `ret` (0x004a2002). Slot 22 (+0x58) of the table 0x004bb5b4 (the vtable slot
// 0x004bb60c; installers per pointer_callers.py: LabelButton::dtor 0x00491410, ctorObj4a13f0 0x004a13f0 and three more).
// The SEH frame (handler 0x004b932b, 0x004a1580..0x004a158e) only covers the scratch Image of the masked arm (state
// [esp+0x2f0] = 0 at 0x004a1680, -1 at 0x004a17d3) and is not reproduced.
//
// The object: +0x130 owner window, +0x1ac / +0x1b0 the position the masked arm blits from, +0x274 the Surface part
// (ebx), whose +4 is the jgld surface S (slots +0xc0 background value, +0xc4 end, +0xd0 rect, +0xd8 width, +0xdc
// height), +0x290, +0x578, +0x5e4, +0x5e8, +0x5ec / +0x5f0 pressed offset, +0x5f4 caption, +0x600 / +0x604 bevel colours,
// +0x608 bevel count, +0x60c focus colour, +0x614 pressed, +0x634 state, and per state s (dword tables): +0x638 caption
// colour, +0x644 composite, +0x650 single image, +0x664 / +0x670 / +0x67c / +0x688 left / right / top / bottom edges,
// +0x694 / +0x6a0 / +0x6ac / +0x6b8 top-left / top-right / bottom-left / bottom-right corners, +0x6c4 centre; +0x65c and
// +0x660 are the mask and the source of the masked arm. Images: +0x18 width, +0x1c height.
typedef int(__fastcall* T3_t)(void* ecx, void* edx, unsigned a, unsigned b, unsigned c);
typedef int(__fastcall* T7b_t)(void* ecx, void* edx, unsigned a, unsigned b, unsigned c, unsigned d, unsigned e,
                               unsigned f, unsigned g);
typedef int(__fastcall* T6b_t)(void* ecx, void* edx, unsigned a, unsigned b, unsigned c, unsigned d, unsigned e,
                               unsigned f);
typedef int(__fastcall* T2_t)(void* ecx, void* edx, unsigned a, unsigned b);
typedef int(__fastcall* T0_t)(void* ecx, void* edx);
typedef int(__fastcall* T1u_t)(void* ecx, void* edx, unsigned a);

const T1u_t kApplyPalette = reinterpret_cast<T1u_t>(0x004789f0);       // 0x004a15e3  Widget_applyPalette
const T1u_t kWindowPaint = reinterpret_cast<T1u_t>(0x004808c0);        // 0x004a1615  Window::paint
const T0_t kImageCtor = reinterpret_cast<T0_t>(0x00474ae0);            // 0x004a1678
const T6b_t kSurfaceCreateFull = reinterpret_cast<T6b_t>(0x00474dd0);  // 0x004a16b6, ret 0x18
const T9_t kSurfaceBlit3 = reinterpret_cast<T9_t>(0x00475d00);         // 0x004a1740, ret 0x24
const T6b_t kSurfaceBlitMasked = reinterpret_cast<T6b_t>(0x00473fc0);  // 0x004a17ca, ret 0x18
const T0_t kImageDtor = reinterpret_cast<T0_t>(0x00474c40);            // 0x004a17de
const T2_t kDrawComposite = reinterpret_cast<T2_t>(0x004791b0);        // 0x004a187e, ret 8
const T7b_t kBlitRight = reinterpret_cast<T7b_t>(0x00476140);          // 0x004a1a16 .. 0x004a1c92, ret 0x1c
const T3_t kDrawBevelBoxR = reinterpret_cast<T3_t>(0x004795d0);        // 0x004a1cb6 .. 0x004a1db6, ret 0xc
const T0_t kIsFocusChild = reinterpret_cast<T0_t>(0x0047d7d0);         // 0x004a1cf4  Window::isFocusChild
const T4_t kSetColorNormal = reinterpret_cast<T4_t>(0x00488930);       // 0x004a1e3a  Button::setColorNormal
const T4_t kWrapText = reinterpret_cast<T4_t>(0x00476e20);             // 0x004a1ebf
const T0_t kWidgetValue = reinterpret_cast<T0_t>(0x00477580);          // 0x004a1ec8
const T0_t kWidgetGet = reinterpret_cast<T0_t>(0x00477560);            // 0x004a1ed1
const T1u_t kTextPut = reinterpret_cast<T1u_t>(0x00477250);            // 0x004a1f0b, 0x004a1f5d
const T4_t kMeasureText3 = reinterpret_cast<T4_t>(0x00478530);         // 0x004a1f33
const T1u_t kFillRectR = reinterpret_cast<T1u_t>(0x00475b00);          // 0x004a1f73  Widget_fillRectR
const T3_t kDrawCentered = reinterpret_cast<T3_t>(0x00477cd0);         // 0x004a1f97, 0x004a1fe5

static inline unsigned U(const void* p) { return reinterpret_cast<unsigned>(p); }
static inline int Wd(const char* img) { return At<int>(img, 0x18); }
static inline int Ht(const char* img) { return At<int>(img, 0x1c); }

typedef int(__fastcall* LabelButtonDraw_t)(void* ecx, void* edx);
LabelButtonDraw_t LabelButtonDraw_orig;

int __fastcall LabelButtonDraw_re(void* self, void*) {
    char* const o = static_cast<char*>(self);
    G<void*>(0x0083ab2c) = At<void*>(o, 0x130);                                   // 0x004a15a3 / 0x004a15a9
    if (At<void*>(o, 0x130) == 0) return 0;                                       // 0x004a15b4 / 0x004a15b6 (eax = 0)
    void* d = G<void*>(0x0083ad50);
    if (d != 0) V3(d, 0x9c, 1, 1, 1);                                             // 0x004a15bc..0x004a15ce
    char* const part = o + 0x274;                                                 // ebx (0x004a15da)
    kApplyPalette(part, 0, G<unsigned>(0x0083ad10));                              // 0x004a15d4..0x004a15e3
    int rect[4] = {0, 0, 0, 0};                                                   // [esp+0x14] x1, y1, x2, y2
    if (At<void*>(part, 4) != 0) V1(At<void*>(part, 4), 0xd0, U(rect));           // 0x004a15e8..0x004a15f6
    {
        void* s = At<void*>(part, 4);
        kWindowPaint(o, 0, s != 0 ? static_cast<unsigned>(V0(s, 0xc0)) : 0xff);  // 0x004a15fc..0x004a1615
    }
    // the state index (0x004a161a..0x004a164a): +0x634 when in 0..2, else 2 when pressed (+0x614) or +0x578 is set,
    // else +0x5e4 != 0
    int st = At<int>(o, 0x634);
    if (st < 0 || st >= 3) {
        if (At<int>(o, 0x614) != 0 || At<int>(o, 0x578) != 0) st = 2;
        else st = At<int>(o, 0x5e4) != 0 ? 1 : 0;
    }
    char* const T = o + 4 * st;                                                   // the per-state tables
    void* const S = At<void*>(part, 4);
    // width (+0xd8) and height (+0xdc) of S, 0 without S; every use re-reads them, as the original does
#define SW() (At<void*>(part, 4) != 0 ? V0(At<void*>(part, 4), 0xd8) : 0)
#define SH() (At<void*>(part, 4) != 0 ? V0(At<void*>(part, 4), 0xdc) : 0)
    (void)S;

    if (At<char*>(T, 0x650) != 0) {                                               // 0x004a164f / 0x004a1656
        char* const img = At<char*>(T, 0x650);
        if (At<void*>(o, 0x65c) != 0 && At<void*>(o, 0x660) != 0) {               // 0x004a165c..0x004a166e
            alignas(16) unsigned char scratch[0x2c0];                            // [esp+0x30], the masked arm's Image
            kImageCtor(scratch, 0);                                               // 0x004a1678
            const int h0 = SH();                                                  // 0x004a168d (ebp)
            const int w0 = SW();                                                  // 0x004a169e
            kSurfaceCreateFull(scratch, 0, w0, h0, 0x10, 1, 0, 0);                // 0x004a16a8..0x004a16b6
            const int h1 = SH();                                                  // 0x004a16c6 [esp+0x10]
            const int w1 = SW();                                                  // 0x004a16df [esp+0x24]
            const int h2 = SH();                                                  // 0x004a16f8 (ebp)
            const int w2 = SW();                                                  // 0x004a170d
            // Surface_blit3(+0x660; scratch, [+0x1ac], [+0x1b0], w2, h2, 0, 0, w1, h1) (0x004a1717..0x004a1740)
            kSurfaceBlit3(At<void*>(o, 0x660), 0, U(scratch), At<unsigned>(o, 0x1ac), At<unsigned>(o, 0x1b0), w2, h2,
                          0, 0, w1, h1);
            const int h3 = SH();                                                  // 0x004a1756 [esp+0x24]
            const int ih = Ht(img);                                               // 0x004a1771 [esp+0x28]
            const int w3 = SW();                                                  // 0x004a1781 [esp+0x10]
            // Surface_blitMasked(image; [+0x65c], scratch, part, (w3 - iw) / 2, (h3 - ih) / 2, 0) (0x004a1795..0x004a17ca)
            kSurfaceBlitMasked(img, 0, At<unsigned>(o, 0x65c), U(scratch), U(part), (w3 - Wd(img)) / 2,
                               (h3 - ih) / 2, 0);
            kImageDtor(scratch, 0);                                               // 0x004a17de
        } else {
            const int h3 = SH();                                                  // 0x004a17f9 [esp+0x24]
            const int ih = Ht(img);                                               // 0x004a1814 [esp+0x28]
            const int w3 = SW();                                                  // 0x004a1824 [esp+0x10]
            // Image_draw(image; part, (w3 - iw) / 2, (h3 - ih) / 2, 0) (0x004a1838..0x004a1861)
            kImageDraw(img, 0, U(part), (w3 - Wd(img)) / 2, (h3 - ih) / 2, 0);
        }
    } else if (At<void*>(T, 0x644) != 0) {                                        // 0x004a186b..0x004a1874
        kDrawComposite(part, 0, U(rect), At<unsigned>(T, 0x644));                 // 0x004a1876..0x004a187e
    } else if (At<char*>(T, 0x694) != 0) {                                        // 0x004a1888..0x004a1891
        // the nine pieces; `pieces` counts the drawn ones ([esp+0x10]) and the centre needs all eight others
        int pieces = 1;                                                           // 0x004a18a3
        kImageDraw(At<char*>(T, 0x694), 0, U(part), 0, 0, 0);                     // 0x004a18ab (top-left)
        if (char* p = At<char*>(T, 0x6a0)) {                                      // 0x004a18b0 (top-right)
            pieces = 2;                                                           // 0x004a18c2
            kImageDraw(p, 0, U(part), SW() - Wd(p), 0, 0);                        // 0x004a18cc..0x004a18ec
        }
        if (char* p = At<char*>(T, 0x6ac)) {                                      // 0x004a18f1 (bottom-left)
            ++pieces;
            kImageDraw(p, 0, U(part), 0, SH() - Ht(p), 0);                        // 0x004a1910..0x004a1930
        }
        if (char* p = At<char*>(T, 0x6b8)) {                                      // 0x004a1935 (bottom-right)
            ++pieces;
            const int h = SH();                                                   // 0x004a195a
            const int ph = Ht(p);
            kImageDraw(p, 0, U(part), SW() - Wd(p), h - ph, 0);                   // 0x004a1985..0x004a19a8
        }
        char* const tl = At<char*>(T, 0x694);
        char* const tr = At<char*>(T, 0x6a0);
        char* const bl = At<char*>(T, 0x6ac);
        char* const br = At<char*>(T, 0x6b8);
        if (char* e = At<char*>(T, 0x664)) {                                      // 0x004a19ad (left edge)
            if (tl != 0 && bl != 0) {                                             // 0x004a19b8 / 0x004a19c3
                ++pieces;
                // blitRight(part; edge, 0, 0, 0, tl.h, edge.w, height - bl.h - tl.h) (0x004a19d7..0x004a1a16)
                kBlitRight(part, 0, U(e), 0, 0, 0, Ht(tl), Wd(e), SH() - Ht(bl) - Ht(tl));
            }
        }
        if (char* e = At<char*>(T, 0x670)) {                                      // 0x004a1a1b (right edge)
            if (tr != 0 && br != 0) {                                             // 0x004a1a2a / 0x004a1a39
                ++pieces;
                const int h = SH();                                               // 0x004a1a5a (ebp)
                const int trh = Ht(tr), brh = Ht(br), ew = Wd(e);                 // 0x004a1a74..0x004a1a89
                kBlitRight(part, 0, U(e), 0, 0, SW() - ew, trh, ew, h - brh - trh);   // 0x004a1a99..0x004a1aca
            }
        }
        if (char* e = At<char*>(T, 0x67c)) {                                      // 0x004a1acf (top edge)
            if (tl != 0 && tr != 0) {                                             // 0x004a1ada / 0x004a1ae5
                ++pieces;
                const int eh = Ht(e);                                             // 0x004a1af4
                kBlitRight(part, 0, U(e), 0, 0, Wd(tl), 0, SW() - Wd(tr) - Wd(tl), eh);   // 0x004a1b05..0x004a1b3a
            }
        }
        if (char* e = At<char*>(T, 0x688)) {                                      // 0x004a1b3f (bottom edge)
            if (bl != 0 && br != 0) {                                             // 0x004a1b4e / 0x004a1b5d
                ++pieces;
                const int eh = Ht(e);                                             // 0x004a1b75 [esp+0x2c]
                const int w = SW();                                               // 0x004a1b85 (ebp)
                const int blw = Wd(bl), brw = Wd(br);                             // 0x004a1b9f / 0x004a1ba2
                kBlitRight(part, 0, U(e), 0, 0, blw, SH() - eh, w - blw - brw, eh);   // 0x004a1bb6..0x004a1bf1
            }
        }
        if (char* c = At<char*>(T, 0x6c4)) {                                      // 0x004a1bf6 (centre)
            if (pieces == 8) {                                                    // 0x004a1c05 / 0x004a1c0a
                const int h = SH();                                               // 0x004a1c19 (ebp)
                const int teh = Ht(At<char*>(T, 0x67c)), beh = Ht(At<char*>(T, 0x688));   // 0x004a1c33 / 0x004a1c36
                const int w = SW();                                               // 0x004a1c4a
                const int rew = Wd(At<char*>(T, 0x670));                          // 0x004a1c65
                const int lew = Wd(At<char*>(T, 0x664));                          // 0x004a1c72
                // x is the RIGHT edge's width (0x004a1c8a push ecx = [+0x670]+0x18), y the top edge's height
                kBlitRight(part, 0, U(c), 0, 0, rew, teh, w - lew - rew, h - beh - teh);   // 0x004a1c8f / 0x004a1c92
            }
        }
    } else {
        // nested bevels (0x004a1c9c..0x004a1e14)
        int n = 0;                                                                // ebp
        if (At<int>(o, 0x60c) != -1) {                                            // 0x004a1ca4 / 0x004a1ca7
            kDrawBevelBoxR(part, 0, U(rect), At<unsigned>(o, 0x60c), At<unsigned>(o, 0x60c));   // 0x004a1cb6
            ++rect[0]; ++rect[1]; --rect[2]; --rect[3];                           // 0x004a1cbb..0x004a1ce1
            // a second ring when bit 12 of the owner's +0x9c is clear and this window is the focus child
            if (((~At<unsigned>(At<char*>(o, 0x130), 0x9c)) & 0x1000) != 0 && kIsFocusChild(o, 0) != 0) {
                kDrawBevelBoxR(part, 0, U(rect), At<unsigned>(o, 0x60c), At<unsigned>(o, 0x60c));   // 0x004a1d0c
                ++rect[0]; ++rect[1]; --rect[2]; --rect[3];                       // 0x004a1d11..0x004a1d31
                n = 1;                                                            // 0x004a1d35
            }
        }
        const int count = At<int>(o, 0x608);
        if (At<int>(o, 0x614) == 0) {                                             // 0x004a1d3a..0x004a1d48
            for (; n < count; ++n) {                                              // 0x004a1d4a / 0x004a1d99
                kDrawBevelBoxR(part, 0, U(rect), At<unsigned>(o, 0x600), At<unsigned>(o, 0x604));   // 0x004a1d67
                ++rect[0]; ++rect[1]; --rect[2]; --rect[3];
            }
        } else {
            for (; n < count; ++n) {                                              // 0x004a1d9d / 0x004a1de8
                kDrawBevelBoxR(part, 0, U(rect), At<unsigned>(o, 0x604), At<unsigned>(o, 0x600));   // 0x004a1db6
                ++rect[0]; ++rect[1]; --rect[2]; --rect[3];
            }
            for (int k = 0; k < 4; ++k) rect[k] += count;                         // 0x004a1dea..0x004a1e14
        }
    }
#undef SW
#undef SH

    // the caption (0x004a1e18)
    const char* const cap = At<const char*>(o, 0x5f4);
    if (cap == 0) return 0;                                                       // 0x004a1e1e / 0x004a1e20 (eax = 0)
    int r = At<int>(T, 0x638);                                                    // 0x004a1e26
    if (r >= 0) r = kSetColorNormal(o, 0, r, 0xffffffff, 1, 1);                   // 0x004a1e2f..0x004a1e3a
    char* const owner = At<char*>(o, 0x130);
    if (owner == 0) return r;                                                     // 0x004a1e45 / 0x004a1e47
    // the owner's slot +0x14 (this, caption, +0x5e8) may draw the caption itself (0x004a1e4d..0x004a1e63)
    r = V3(owner, 0x14, U(o), U(cap), At<unsigned>(o, 0x5e8));
    if (r != 0) return r;
    if (st == 2) {                                                                // 0x004a1e69 / 0x004a1e6c: pressed offset
        rect[0] += At<int>(o, 0x5ec); rect[2] += At<int>(o, 0x5ec);
        rect[1] += At<int>(o, 0x5f0); rect[3] += At<int>(o, 0x5f0);
    }
    const int avail = rect[2] - rect[0] - 6;                                      // 0x004a1ea8..0x004a1eb6
    int lines = kWrapText(part, 0, U(At<const char*>(o, 0x5f4)), avail, 0, 0);    // 0x004a1ebf
    lines -= kWidgetValue(part, 0);                                               // 0x004a1ec8 / 0x004a1ecf
    lines += kWidgetGet(part, 0);                                                 // 0x004a1ed1 / 0x004a1eda
    const int top = (rect[3] - rect[1] - lines) / 2 + rect[1];                    // 0x004a1ed6..0x004a1eeb
    bool fits = top >= 0;                                                         // 0x004a1eed js
    if (fits && At<int>(o, 0x290) < 2)                                            // 0x004a1eef / 0x004a1ef6
        fits = kTextPut(part, 0, U(At<const char*>(o, 0x5f4))) <= rect[2] - rect[0] - 6;   // 0x004a1f0b..0x004a1f15
    if (fits)
        return kMeasureText3(part, 0, U(At<const char*>(o, 0x5f4)), rect[0] + 3, top, rect[2] - rect[0] - 6);   // 0x004a1f33
    // the fallback (0x004a1f3d): the caption left in a rect shrunk by 3 and by the width of the string 0x004c5db4,
    // then that string to the right of it
    const char* const tail = reinterpret_cast<const char*>(0x004c5db4);
    rect[0] += 3;                                                                 // 0x004a1f53
    rect[2] -= 3;                                                                 // 0x004a1f59
    rect[2] -= kTextPut(part, 0, U(tail));                                        // 0x004a1f5d..0x004a1f6f
    kFillRectR(part, 0, U(rect));                                                 // 0x004a1f73
    const char* const cap2 = At<const char*>(o, 0x5f4);                           // 0x004a1f78
    if (cap2 != 0) kDrawCentered(part, 0, U(cap2), U(rect), static_cast<unsigned>(strlen(cap2)));   // 0x004a1f97
    if (At<void*>(part, 4) != 0) V0(At<void*>(part, 4), 0xc4);                    // 0x004a1f9c..0x004a1fa5
    const int wdt = rect[2] - rect[0];                                            // 0x004a1fab..0x004a1fb5
    rect[0] += wdt;                                                               // 0x004a1fb7
    rect[2] += wdt;                                                               // 0x004a1fb9
    // `mov eax, 0x4c5db4; test eax, eax` (0x004a1fbb..0x004a1fca): never zero
    return kDrawCentered(part, 0, U(tail), U(rect), static_cast<unsigned>(strlen(tail)));   // 0x004a1fe5
}

// ====================================================================================================================
// 0x0048e900  TextView::build
// ====================================================================================================================
// thiscall (this, quiet, modalArg), `ret 8` (0x0048fe5d). [esp+0x40] is quiet (0x0048ea06) and [esp+0x44] the
// argument passed on to the modal run (0x0048fd42 / 0x0048fd60). The SEH frame (handler 0x004b806b) only guards the
// operator new of the list buttons ([esp+0x38] = 0 at 0x0048f580, -1 at 0x0048f59b) and is not reproduced.
// Virtual base: [this+0x1464] is the vbtable; its +8 dword is the offset of the window part from this+0x1464
// (0x0048e93f, 0x0048e9c0, 0x0048ef76) and its +4 dword the offset of the panel part (0x0048efcd).
typedef int(__fastcall* T8_t)(void* ecx, void* edx, unsigned a, unsigned b, unsigned c, unsigned d, unsigned e,
                              unsigned f, unsigned g, unsigned h);
typedef int(__fastcall* T5_t)(void* ecx, void* edx, unsigned a, unsigned b, unsigned c, unsigned d, unsigned e);
typedef void*(__cdecl* New_t)(unsigned n);
typedef void(__cdecl* Cb2_t)(void* p, int flag);
typedef int(__cdecl* C4_t)(int a, int b, int c, int d);

const T0_t kWindowVisible = reinterpret_cast<T0_t>(0x004801f0);       // 0x0048e97e
const T1u_t kSetFontSize = reinterpret_cast<T1u_t>(0x0048e120);       // 0x0048e990  TextView::setFontSize
const T1u_t kSetLineSpacing = reinterpret_cast<T1u_t>(0x0048e0b0);    // 0x0048e99e  TextView::setLineSpacing
const T2_t kTvSetSize = reinterpret_cast<T2_t>(0x0048e190);           // 0x0048e9b3  TextView::setSize
const T1u_t kSetFlagBit0 = reinterpret_cast<T1u_t>(0x00491490);       // 0x0048e9cc  Widget::setFlagBit0
const T1u_t kSetFlagBit1 = reinterpret_cast<T1u_t>(0x004914b0);       // 0x0048e9d5  Widget::setFlagBit1
const T9_t kWindowCreate = reinterpret_cast<T9_t>(0x004806c0);        // 0x0048eb1a, ret 0x24
const T4_t kSetQuad = reinterpret_cast<T4_t>(0x00476310);             // 0x0048eb62  Widget_setQuad
const T4_t kSetQuad70 = reinterpret_cast<T4_t>(0x00476340);           // 0x0048eb9c
const T4_t kSetQuad74 = reinterpret_cast<T4_t>(0x00476370);           // 0x0048ebd6
const T4_t kSetQuad78 = reinterpret_cast<T4_t>(0x004763a0);           // 0x0048ec10
const T1u_t kLabelSetText = reinterpret_cast<T1u_t>(0x0047b8f0);      // 0x0048ec45  Label::setText
const T5_t kPanelDispatch = reinterpret_cast<T5_t>(0x004a08a0);       // 0x0048efc0, ret 0x14
const T8_t kButtonCreate = reinterpret_cast<T8_t>(0x004887c0);        // 0x0048f230, 0x0048f281, 0x0048f615, 0x0048fa1e
const T4_t kSetColorHover = reinterpret_cast<T4_t>(0x00488970);       // 0x0048f45f ...  Button::setColorHover
const T4_t kSetColorPressed = reinterpret_cast<T4_t>(0x004889b0);     // 0x0048f484 ...  Button::setColorPressed
const T1u_t kSetTooltip = reinterpret_cast<T1u_t>(0x00488a20);        // 0x0048f4df, 0x0048fcc9  Button::setTooltip
const New_t kOperatorNew = reinterpret_cast<New_t>(0x004a57f1);       // 0x0048f572
const T0_t kCtorObj = reinterpret_cast<T0_t>(0x004a13f0);             // 0x0048f588  ctorObj4a13f0
const T1u_t kFn4a2980 = reinterpret_cast<T1u_t>(0x004a2980);          // 0x0048fe13, ret 4
const C4_t kSetHudTextSlot = reinterpret_cast<C4_t>(0x00494cb0);      // 0x0048fe1a

static inline int Vb(char* o, unsigned k) { return At<int>(At<char*>(o, 0x1464), k); }   // vbtable dword k

// The skin copied into a button that has no image set of its own: ten triplets from this+0x2090, 12 bytes apart,
// into the button's +0x644 and +0x664..+0x6c4 (0x0048f286..0x0048f3e8, 0x0048f61a..0x0048f792, 0x0048fa58..0x0048fbba)
static void CopySkin(char* btn, const char* tv) {
    static const unsigned kDst[10] = {0x644, 0x664, 0x670, 0x67c, 0x688, 0x694, 0x6a0, 0x6ac, 0x6b8, 0x6c4};
    for (int k = 0; k < 10; ++k)
        for (int j = 0; j < 3; ++j) At<unsigned>(btn, kDst[k] + 4 * j) = At<unsigned>(tv, 0x2090 + 12 * k + 4 * j);
}
// The colours and the style words every built button gets (0x0048f3ee..0x0048f4c7 and the two copies)
static void StyleButton(char* btn, const char* tv, unsigned* styleDst) {
    kSetDrawTarget(btn + 0x274, 0, At<unsigned>(tv, 0x202c), At<unsigned>(tv, 0x2030), At<unsigned>(tv, 0x2034), 0);
    V0(btn, 0x120);
    kSetColorNormal(btn, 0, At<unsigned char>(tv, 0x2038), At<unsigned>(tv, 0x203c), At<unsigned>(tv, 0x2048),
                    At<unsigned>(tv, 0x2054));
    kSetColorHover(btn, 0, At<unsigned char>(tv, 0x2039), At<unsigned>(tv, 0x2040), At<unsigned>(tv, 0x204c),
                   At<unsigned>(tv, 0x2058));
    kSetColorPressed(btn, 0, At<unsigned char>(tv, 0x203a), At<unsigned>(tv, 0x2044), At<unsigned>(tv, 0x2050),
                     At<unsigned>(tv, 0x205c));
    styleDst[0] = At<unsigned char>(tv, 0x2060);
    for (int k = 1; k < 6; ++k) styleDst[k] = At<unsigned>(tv, 0x2060 + 4 * k);
}
// strlen + 1 bytes from malloc, emptied, then the source appended (0x0048f10b..0x0048f15f and its copies)
static char* DupString(const char* sizeFrom, const char* src) {
    char* p = static_cast<char*>(kMalloc(static_cast<unsigned>(strlen(sizeFrom) + 1)));
    p[0] = 0;
    strcat(p, src);
    return p;
}

typedef int(__fastcall* TextViewBuild_t)(void* ecx, void* edx, int quiet, int modalArg);
TextViewBuild_t TextViewBuild_orig;

int __fastcall TextViewBuild_re(void* self, void*, int quiet, int modalArg) {
    char* const tv = static_cast<char*>(self);
    if (V0(tv, 0x194) == 0) return -1;                                            // 0x0048e921..0x0048e92e
    int modalResult = 0;                                                          // [esp+0x20] (0x0048e93b)
    if (At<int>(tv, Vb(tv, 8) + 0x1534) == 0) At<int>(tv, 0x1654) = -1;           // 0x0048e93f..0x0048e94d
    if (At<int>(tv, 0x1578) > 1 && At<int>(tv, 0x590) == 0) At<int>(tv, 0x1654) = 8;   // 0x0048e957..0x0048e96a
    void* p590 = At<void*>(tv, 0x590);
    if (p590 != 0 && kWindowVisible(p590, 0) != 0) {                              // 0x0048e974..0x0048e985
        kSetFontSize(tv, 0, At<unsigned>(tv, 0x59c));                             // 0x0048e990
        kSetLineSpacing(tv, 0, At<unsigned>(tv, 0x5a0));                          // 0x0048e99e
        kTvSetSize(tv, 0, At<unsigned>(tv, 0x594), At<unsigned>(tv, 0x598));      // 0x0048e9b3
        char* const win = tv + Vb(tv, 8) + 0x1464;                                // 0x0048e9c3
        kSetFlagBit0(win, 0, 1);                                                  // 0x0048e9cc
        kSetFlagBit1(win, 0, 1);                                                  // 0x0048e9d5
    } else {
        At<void*>(tv, 0x590) = 0;                                                 // 0x0048e9dc
        At<unsigned>(tv, 0x1f68) &= 0xffefffff;                                   // 0x0048e9e2..0x0048e9ed
    }
    {
        const int r = V0(tv, 0x17c);                                              // 0x0048e9f8
        if (r != 0) return r;                                                     // 0x0048ea00
    }
    if (quiet != 0) return 0;                                                     // 0x0048ea06 / 0x0048ea0a
    unsigned f = At<unsigned>(tv, 0x1f68);
    if (f & 0x8000) return 0;                                                     // 0x0048ea16 test ah, 0x80

    // the window style (0x0048ea1f..0x0048ea7f)
    p590 = At<void*>(tv, 0x590);
    const unsigned parent = p590 != 0 ? reinterpret_cast<unsigned>(p590) : At<unsigned>(tv, 0x2110);
    unsigned style = At<unsigned>(tv, 0x1f64);
    if (f & 0x20) style |= 0x20;
    style |= (p590 == 0) ? 1 : 0x400000;
    if (G<int>(0x0083abe4) != 0) style |= 0x10000000;
    if (At<int>(tv, 0x13cc) != 0) style |= 0x10000;
    if (f & 0x4100) style |= 0x1000;                                              // test ah, 0x41
    if ((f & 0x20000) != 0 || p590 != 0) style |= 0x80000;

    if (At<char*>(tv, 0x5bc) == tv) {                                             // 0x0048ea85
        V0(tv, 0x184);                                                            // 0x0048ea96
        int r3[3];                                                                // [esp+0x24]
        if (char* img = At<char*>(tv, 0x13cc)) {                                  // 0x0048ea9c..0x0048eac5
            const int a = At<int>(img, 0x1c) + 2 * At<int>(tv, 0x194);
            r3[0] = a > At<int>(tv, 0x180) ? a : At<int>(tv, 0x180);
        } else {
            r3[0] = At<int>(tv, 0x180);                                           // 0x0048eacb
        }
        r3[1] = At<int>(tv, 0x184);                                               // 0x0048ead5 / 0x0048eae3
        r3[2] = At<int>(tv, 0x188);
        if (r3[1] == 0) style &= ~1u;                                             // 0x0048eaeb / 0x0048eaed
        // Window::create(this; 0, 0, +0x1f7c, +0x1f80, +0x5c8, style | 0x800, parent, 0, r3) (0x0048eaf0..0x0048eb1a)
        const int r = kWindowCreate(tv, 0, 0, 0, At<unsigned>(tv, 0x1f7c), At<unsigned>(tv, 0x1f80),
                                    At<unsigned>(tv, 0x5c8), style | 0x800, parent, 0, reinterpret_cast<unsigned>(r3));
        if (r != 0) return r;                                                     // 0x0048eb1f / 0x0048eb21
        // the four frame quads, each in 0x8000ffff when the display's slot +0xb4 answers 1, else 0x80007fff
        // (dec / neg / sbb / and 0xffff8000 / add 0x8000ffff, 0x0048eb2f..0x0048eb5a and three copies)
        char* const part = tv + 0x274;
        const T4_t quads[4] = {kSetQuad, kSetQuad70, kSetQuad74, kSetQuad78};
        for (int k = 0; k < 4; ++k) {
            const unsigned colour = V0(G<void*>(0x0083ad50), 0xb4) == 1 ? 0x8000ffffu : 0x80007fffu;
            quads[k](part, 0, colour, At<unsigned>(tv, 0x1ff0 + 4 * k), At<unsigned>(tv, 0x2000 + 4 * k),
                     At<unsigned>(tv, 0x2010 + 4 * k));
        }
        kSetDrawTarget(part, 0, At<unsigned>(tv, 0x1fd0), At<unsigned>(tv, 0x1fd4), At<unsigned>(tv, 0x1fd8),
                       At<unsigned>(tv, 0x1fdc));                                 // 0x0048ec33
        if (At<unsigned>(tv, 0x5c8) != 0) kLabelSetText(tv, 0, At<unsigned>(tv, 0x5c8));   // 0x0048ec38..0x0048ec45
        V0(tv, 0x188);                                                            // 0x0048ec4f
    }

    // the position (0x0048ec55..0x0048ef1a): +0x1fb8 / +0x1fbc are 0x1000 for centred, negative for an offset from
    // the right / bottom edge, else the coordinate itself
    {
        char* const own = At<char*>(tv, 0x5bc);
        const int fx = At<int>(tv, 0x1fb8), fy = At<int>(tv, 0x1fbc);
        void* const d = G<void*>(0x0083ad50);
        if (own == tv) {
            if (At<unsigned char>(own, 0xa0) & 2) {                               // 0x0048ec63
                if (fx == 0x1000) {
                    const int dw = d != 0 ? V0(d, 0xa8) : 0;                      // 0x0048ec7d..0x0048ec8d
                    At<int>(tv, 0x1f74) = (At<int>(own, 0x1bc) - At<int>(own, 0x1c4) + dw) >> 1;   // sar 0x0048eca9
                } else if (fx < 0) {
                    const int dw = d != 0 ? V0(d, 0xa8) : 0;                      // 0x0048ecb7..0x0048ecc7
                    At<int>(tv, 0x1f74) = At<int>(own, 0x1bc) - At<int>(own, 0x1c4) + dw + At<int>(tv, 0x1fb8);
                } else {
                    At<int>(tv, 0x1f74) = fx;                                     // 0x0048ecf3
                }
                if (fy == 0x1000) {
                    At<int>(tv, 0x1f78) = 0x64;                                   // 0x0048ed06
                } else if (fy < 0) {
                    const int dh = d != 0 ? V0(d, 0xac) : 0;                      // 0x0048ed1d..0x0048ed2d
                    At<int>(tv, 0x1f78) = At<int>(own, 0x1c0) - At<int>(own, 0x1c8) + dh + At<int>(tv, 0x1fbc);
                } else {
                    At<int>(tv, 0x1f78) = fy;                                     // 0x0048ef1a
                }
            } else {
                if (fx == 0x1000) {
                    const int dw = d != 0 ? V0(d, 0xa8) : 0;                      // 0x0048ed63..0x0048ed79
                    void* s = At<void*>(At<char*>(tv, 0x5bc), 0x278);
                    const int w = s != 0 ? V0(s, 0xd8) : 0;                       // 0x0048ed81..0x0048ed9f
                    At<int>(tv, 0x1f74) = (dw - w) >> 1;
                } else if (fx < 0) {
                    const int dw = d != 0 ? V0(d, 0xa8) : 0;                      // 0x0048edb1..0x0048edc7
                    void* s = At<void*>(At<char*>(tv, 0x5bc), 0x278);
                    const int w = s != 0 ? V0(s, 0xd8) : 0;
                    At<int>(tv, 0x1f74) = dw - w + At<int>(tv, 0x1fb8);           // 0x0048ede5..0x0048edef
                } else {
                    At<int>(tv, 0x1f74) = fx;                                     // 0x0048edf7
                }
                if (fy == 0x1000) {
                    At<int>(tv, 0x1f78) = 0x64;                                   // 0x0048ee0a
                } else if (fy < 0) {
                    const int dh = d != 0 ? V0(d, 0xac) : 0;                      // 0x0048ee21..0x0048ee37
                    void* s = At<void*>(At<char*>(tv, 0x5bc), 0x278);
                    const int h = s != 0 ? V0(s, 0xdc) : 0;
                    At<int>(tv, 0x1f78) = dh - h + At<int>(tv, 0x1fbc);           // 0x0048ee55..0x0048ee5f
                } else {
                    At<int>(tv, 0x1f78) = fy;
                }
            }
        } else {
            void* s = At<void*>(own, 0x278);
            int x;
            if (fx == 0x1000) {                                                   // 0x0048ee70 / 0x0048ee75
                x = ((s != 0 ? V0(s, 0xd8) : 0) - At<int>(tv, 0x1f7c)) >> 1;      // 0x0048ee77..0x0048ee9d
            } else if (fx < 0) {
                x = (s != 0 ? V0(s, 0xd8) : 0) - At<int>(tv, 0x1f7c) + At<int>(tv, 0x1fb8);   // 0x0048eea5..0x0048eec9
            } else {
                x = fx;
            }
            At<int>(tv, 0x1f74) = x;                                              // 0x0048eecb
            if (fy == 0x1000) {
                At<int>(tv, 0x1f78) = 0x64;                                       // 0x0048eede
            } else if (fy < 0) {
                void* s2 = At<void*>(At<char*>(tv, 0x5bc), 0x278);
                const int h = s2 != 0 ? V0(s2, 0xdc) : 0;                         // 0x0048eeee..0x0048ef08
                At<int>(tv, 0x1f78) = h - At<int>(tv, 0x1f80) + At<int>(tv, 0x1fbc);   // 0x0048ef0a..0x0048ef18
            } else {
                At<int>(tv, 0x1f78) = fy;
            }
        }
    }

    V0(tv, 0x58);                                                                 // 0x0048ef25
    int L10, L14, L18;                                                            // [esp+0x10], [esp+0x14], [esp+0x18]
    L14 = G<int>(0x0083b8d8);                                                     // 0x0048ef2e
    if (At<int>(tv, 0x1f94) != 0) L14 += At<int>(tv, 0x1fa4);                     // 0x0048ef3a / 0x0048ef3c
    L10 = At<int>(tv, 0x145c);                                                    // 0x0048ef46
    if (L10 < 0) L10 = G<int>(0x0083b8dc);                                        // 0x0048ef4e / 0x0048ef50
    if (At<int>(tv, 0x1654) != -1) {                                              // 0x0048ef59..0x0048ef62
        if (At<unsigned>(tv, 0x1f68) & 0x800000)                                  // 0x0048ef64
            At<unsigned>(tv, Vb(tv, 8) + 0x1488) |= 0x1000;                       // 0x0048ef70..0x0048ef8a (or ch, 0x10)
        At<unsigned>(tv, 0x14f4) = At<unsigned>(tv, 0x1f68) & 0x800;              // 0x0048ef90..0x0048efa1
        // panelDispatch(this+0x1460; +0x1654, +0x5bc, L10, L14, 0x80000) (0x0048ef96..0x0048efc0)
        kPanelDispatch(tv + 0x1460, 0, At<unsigned>(tv, 0x1654), At<unsigned>(tv, 0x5bc), L10, L14, 0x80000);
        V1(tv + Vb(tv, 4) + 0x1464, 4, 0);                                        // 0x0048efc5..0x0048efde
    }

    // the button area (0x0048efe1..0x0048f077)
    L10 = At<int>(tv, 0x1fa0);
    int ebx = At<int>(tv, 0x1fa0);                                                // 0x0048f001
    L14 = At<int>(tv, 0x1f80) - At<int>(tv, 0x1fac) - At<int>(tv, 0x1fa0) - At<int>(tv, 0x1f90);   // 0x0048efe1..0x0048f009
    L18 = 0;                                                                      // 0x0048f011
    if (At<void*>(tv, 0x2078) != 0 && At<void*>(tv, 0x207c) != 0 && At<void*>(tv, 0x2080) != 0 &&
        (At<unsigned>(tv, 0x1f68) & 0x10000) == 0) {                              // 0x0048f003..0x0048f039
        void* s = At<void*>(tv, 0x278);
        const int w = s != 0 ? V0(s, 0xd8) : 0;                                   // 0x0048f03b..0x0048f04f
        L10 = w - 3 * At<int>(At<char*>(tv, 0x2078), 0x18);                       // 0x0048f051..0x0048f05f
    }
    // slot +0x190 may move the three (0x0048f063..0x0048f077)
    reinterpret_cast<int(__fastcall*)(void*, void*, int*, int*, int*)>(Slot(tv, 0x190))(tv, 0, &L10, &L14, &L18);
    f = At<unsigned>(tv, 0x1f68);
    const int L1c = L10;                                                          // 0x0048f08c

    if ((f & 0x80100) == 0 && At<char*>(tv, 0x5bc) == tv) {                       // 0x0048f087..0x0048f09c
        if ((f & 0x10000) == 0) {                                                 // 0x0048f0a2 / 0x0048f0a7
            // the default button +0x5dc
            if (L10 < 0)                                                          // 0x0048f0ad..0x0048f0c1
                L10 -= At<int>(tv, 0x1fa4) + At<int>(tv, 0x1f8c);
            else if (At<void*>(tv, 0x2078) == 0)                                  // 0x0048f0c3..0x0048f0e2
                L10 += (At<int>(tv, 0x1fa4) + At<int>(tv, 0x1f8c)) * At<int>(tv, 0x137c);
            char* const cur = At<char*>(tv, 0x2108);
            if (cur == 0) {                                                       // 0x0048f0e8..0x0048f0f0
                if ((f & 0x1000000) == 0) {                                       // 0x0048f0f6
                    const char* src = G<const char*>(0x0083b994);
                    if (src == 0) src = reinterpret_cast<const char*>(0x004e3db0);    // 0x0048f101..0x0048f163
                    At<char*>(tv, 0x2108) = DupString(src, src);                  // 0x0048f10b..0x0048f15f / 0x0048f163..0x0048f1b5
                }
            } else if (f & 0x1000000) {                                           // 0x0048f1bd
                kFree(cur);                                                       // 0x0048f1c9
                At<char*>(tv, 0x2108) = 0;                                        // 0x0048f1d1
            }
            char* const btn = tv + 0x5dc;                                         // esi (0x0048f211 / 0x0048f262)
            char* const img = At<char*>(tv, 0x2078);
            // the size: the +0x2078 image's when it is set, loaded BEFORE +0x207c / +0x2080 are tested
            // (0x0048f1db..0x0048f1f4), so the skin arm also uses it; else +0x1f8c / +0x1f90
            const unsigned w = img != 0 ? At<unsigned>(img, 0x18) : At<unsigned>(tv, 0x1f8c);
            const unsigned h = img != 0 ? At<unsigned>(img, 0x1c) : At<unsigned>(tv, 0x1f90);
            kButtonCreate(btn, 0, At<unsigned>(tv, 0x2108), 0xffffffff, L10, L14, w, h,
                          reinterpret_cast<unsigned>(tv), L18);                   // 0x0048f230 / 0x0048f281
            if (img != 0 && At<void*>(tv, 0x207c) != 0 && At<void*>(tv, 0x2080) != 0) {   // 0x0048f1ed..0x0048f20b
                At<unsigned>(btn, 0x654) = At<unsigned>(tv, 0x207c);              // 0x0048f247
                At<unsigned>(btn, 0x650) = At<unsigned>(tv, 0x2078);              // 0x0048f24d
                At<unsigned>(btn, 0x658) = At<unsigned>(tv, 0x2080);              // 0x0048f253
            } else {
                CopySkin(btn, tv);                                                // 0x0048f286..0x0048f3e8
            }
            StyleButton(btn, tv, &At<unsigned>(btn, 0x5fc));
            if (G<unsigned>(0x0083b99c) != 0) kSetTooltip(btn, 0, G<unsigned>(0x0083b99c));   // 0x0048f4d3..0x0048f4df
            V1(btn, 4, 0);                                                        // 0x0048f4e4..0x0048f4ea
        }

        // the list buttons (0x0048f4ed..0x0048f8b7)
        L10 = ebx;
        if (ebx < 0)                                                              // 0x0048f4ed..0x0048f50f
            L10 = ebx - (At<int>(tv, 0x1fa4) + At<int>(tv, 0x1f8c)) * (At<int>(tv, 0x1f70) - 1);
        {
            // the list cursor is put on the last entry: index n - 1, node = head's +0x10 (0x0048f513..0x0048f54f);
            // the cursor is written even when the list is empty
            const int n = At<int>(tv, 0x13f0);
            if (n - 1 >= -1) {
                At<char*>(tv, 0x13ec) = At<char*>(tv, 0x13e8);                    // 0x0048f52a
                if (n >= 1) {                                                     // 0x0048f535 / 0x0048f537
                    At<char*>(tv, 0x13ec) = At<char*>(At<char*>(tv, 0x13ec), 0x10);   // 0x0048f53d..0x0048f547
                    At<int>(tv, 0x13f4) = n - 1;                                  // 0x0048f54f
                }
            }
        }
        for (int i = 0; i < At<int>(tv, 0x13f0); ++i) {                           // 0x0048f555..0x0048f55f, 0x0048f8b5
            void* const mem = kOperatorNew(0x6d0);                                // 0x0048f56b / 0x0048f572
            char* const btn = mem != 0 ? static_cast<char*>(reinterpret_cast<void*>(kCtorObj(mem, 0))) : 0;   // 0x0048f584..0x0048f58f
            At<char*>(tv, 0x1380 + 4 * i) = btn;                                  // 0x0048f591
            char* const head = At<char*>(tv, 0x13e8);
            if (head != 0) {                                                      // 0x0048f599 / 0x0048f5a3: advance
                At<char*>(tv, 0x13ec) = At<char*>(At<char*>(tv, 0x13ec), 0xc);    // 0x0048f5a5..0x0048f5b4
                const int idx = At<int>(tv, 0x13f4) + 1;                          // 0x0048f5c0
                At<int>(tv, 0x13f4) = idx;
                if (idx == At<int>(tv, 0x13f0)) At<int>(tv, 0x13f4) = 0;          // 0x0048f5c1..0x0048f5cb
            }
            unsigned text = 0;
            if (char* node = At<char*>(tv, 0x13ec)) {                             // 0x0048f5d1..0x0048f5d9
                // with a node but no head the original reads [0 + 4] (0x0048f5e7 / 0x0048f5e9)
                text = head != 0 ? At<unsigned>(At<char*>(node, 8), 4) : *reinterpret_cast<volatile unsigned*>(4);
            }
            kButtonCreate(btn, 0, text, i + 1, L10, L14, At<unsigned>(tv, 0x1f8c), At<unsigned>(tv, 0x1f90),
                          reinterpret_cast<unsigned>(tv), L18);                   // 0x0048f5f0..0x0048f615
            CopySkin(btn, tv);                                                    // 0x0048f61a..0x0048f792
            StyleButton(btn, tv, &At<unsigned>(btn, 0x5fc));                      // 0x0048f798..0x0048f885
            V1(btn, 4, 0);                                                        // 0x0048f88b..0x0048f88f
            L10 += At<int>(tv, 0x1f8c) + At<int>(tv, 0x1fa4);                     // 0x0048f892..0x0048f8a8
        }

        // the second button +0xcac (0x0048f8bd..0x0048fcd4)
        const unsigned f2 = At<unsigned>(tv, 0x1f68);
        if (f2 & 0x40) {                                                          // 0x0048f8c3 / 0x0048f8c6
            L10 = L1c;                                                            // 0x0048f8cc..0x0048f8d4
            if (L1c >= 0)
                L10 = (At<int>(tv, 0x1fa4) + At<int>(tv, 0x1f8c)) * (At<int>(tv, 0x1f70) - 1) + L1c;   // 0x0048f8da..0x0048f8f4
            char* const cur = At<char*>(tv, 0x210c);
            if (cur == 0) {                                                       // 0x0048f8f8..0x0048f900
                if ((f2 & 0x1000000) == 0) {                                      // 0x0048f906
                    const char* src = G<const char*>(0x0083b998);
                    if (src != 0) At<char*>(tv, 0x210c) = DupString(src, src);    // 0x0048f912..0x0048f964
                    // no string: the size comes from 0x004c8590 but 0x004e3db0 is copied (0x0048f96c / 0x0048f98c)
                    else At<char*>(tv, 0x210c) = DupString(reinterpret_cast<const char*>(0x004c8590),
                                                           reinterpret_cast<const char*>(0x004e3db0));
                }
            } else if (f2 & 0x1000000) {                                          // 0x0048f997
                kFree(cur);                                                       // 0x0048f9a4
                At<char*>(tv, 0x210c) = 0;                                        // 0x0048f9b0
            }
            unsigned w, h;
            int x = L10;
            if (char* img = At<char*>(tv, 0x2084)) {                              // 0x0048f9ba..0x0048f9c2
                void* s = At<void*>(tv, 0x278);
                const int sw = s != 0 ? V0(s, 0xd8) : 0;                          // 0x0048f9c4..0x0048f9d8
                x = sw - 2 * At<int>(img, 0x18);                                  // 0x0048f9da..0x0048f9e5
                L10 = x;                                                          // 0x0048f9e7
                w = At<unsigned>(img, 0x18);
                h = At<unsigned>(img, 0x1c);
            } else {
                w = At<unsigned>(tv, 0x1f8c);                                     // 0x0048f9f3
                h = At<unsigned>(tv, 0x1f90);
            }
            char* const btn = tv + 0xcac;                                         // esi (0x0048fa03)
            kButtonCreate(btn, 0, At<unsigned>(tv, 0x210c), 0xfffffffe, x, L14, w, h,
                          reinterpret_cast<unsigned>(tv), L18);                   // 0x0048fa1e
            if (At<void*>(tv, 0x2084) != 0 && At<void*>(tv, 0x2088) != 0 && At<void*>(tv, 0x208c) != 0) {   // 0x0048fa23..0x0048fa3f
                At<unsigned>(btn, 0x650) = At<unsigned>(tv, 0x2084);              // 0x0048fa41
                At<unsigned>(btn, 0x654) = At<unsigned>(tv, 0x2088);
                At<unsigned>(btn, 0x658) = At<unsigned>(tv, 0x208c);
            } else {
                CopySkin(btn, tv);                                                // 0x0048fa58..0x0048fbba
            }
            L10 -= At<int>(tv, 0x1fa4) + At<int>(tv, 0x1f8c);                     // 0x0048fbc0..0x0048fbe0
            StyleButton(btn, tv, &At<unsigned>(tv, 0x12a8));                      // 0x0048fbe4..0x0048fcb7
            if (G<unsigned>(0x0083b9a0) != 0) kSetTooltip(btn, 0, G<unsigned>(0x0083b9a0));   // 0x0048fcbd..0x0048fcc9
            V1(btn, 4, 0);                                                        // 0x0048fcce..0x0048fcd4
        }
    }

    // run (0x0048fcd7..0x0048fe47)
    if ((At<unsigned>(tv, 0x1f68) & 0x400000) == 0 && At<Cb2_t>(tv, 0x1f00) != 0)    // 0x0048fcd7..0x0048fcee
        At<Cb2_t>(tv, 0x1f00)(tv + 0x1cd0, 1);                                    // 0x0048fcf9
    V1(tv, 4, 3);                                                                 // 0x0048fd05
    f = At<unsigned>(tv, 0x1f68);
    if ((f & 0x4100) == 0 && At<char*>(tv, 0x5bc) == tv) {                        // 0x0048fd0e..0x0048fd1d
        const unsigned saved = G<unsigned>(0x0083b990);                           // 0x0048fd25
        G<void*>(0x0083b990) = tv;                                                // 0x0048fd2c
        G<int>(0x0083b8f0) += 1;                                                  // 0x0048fd1f..0x0048fd32
        void* const runner = At<void*>(tv, 0x590) != 0 ? At<void*>(tv, 0x590) : tv;   // 0x0048fd38..0x0048fd40
        // slot +0x10c (Window::runModal) with ((+0x1f68 >> 21) & 1, modalArg, 0) (0x0048fd42..0x0048fd73)
        modalResult = V3(runner, 0x10c, (At<unsigned>(tv, 0x1f68) >> 21) & 1, modalArg, 0);
        G<unsigned>(0x0083b990) = saved;                                          // 0x0048fd7b
        G<int>(0x0083b9e4) = At<int>(tv, 0x1bc);                                  // 0x0048fd81..0x0048fd87
        G<int>(0x0083b9e8) = At<int>(tv, 0x1c0);                                  // 0x0048fd8c..0x0048fd92
    }
    f = At<unsigned>(tv, 0x1f68);
    if ((f & 0x4300) == 0 && At<char*>(tv, 0x5bc) == tv) {                        // 0x0048fd9e..0x0048fdaf
        if (--G<int>(0x0083b8f0) == 0) G<int>(0x0083b8ec) = 0;                    // 0x0048fdb1..0x0048fdbe
        V0(tv, 0x16c);                                                            // 0x0048fdcd
        V0(tv, 8);                                                                // 0x0048fdd8
        if ((At<unsigned>(tv, 0x1f68) & 0x400000) == 0 && At<Cb2_t>(tv, 0x1f00) != 0)   // 0x0048fddb..0x0048fdeb
            At<Cb2_t>(tv, 0x1f00)(tv + 0x1de8, 0);                                // 0x0048fdf6
    }
    if (At<int>(tv, 0x1654) == 4) {                                               // 0x0048fdfb / 0x0048fe02
        for (int i = 0; i < 8; ++i)                                               // 0x0048fe04..0x0048fe26
            kSetHudTextSlot(i, kFn4a2980(tv + 0x15cc, 0, i), -1, -1);             // 0x0048fe13 / 0x0048fe1a
    }
    if (modalResult != 0)                                                         // 0x0048fe28 / 0x0048fe2a
        return (At<unsigned>(tv, 0x1f68) & 0x40) ? -1 : G<int>(0x004e44e0);       // 0x0048fe2c..0x0048fe3f
    return At<int>(tv, 0x5c0);                                                    // 0x0048fe41
}

// ====================================================================================================================
// 0x00456be0  showRoutingMapScreen
// ====================================================================================================================
// __cdecl, no arguments, `ret` (0x00458729, 0x00458736). Frame: `sub esp, 0x120` then four pushes; offsets below are
// relative to esp after the pushes (L<off>), the pre-push slots +0x80 / +0xd0 being L90 / Le0. Every `mov eax, 0x519a60;
// neg eax; sbb eax, eax; and eax, 0x519cd4` (0x00456c63.., 0x00456c95.., ...) is the constant 0x00519cd4 (kFill).
//
// Tables it reads: the tile grid 0x005722e8 (50 x 50 signed bytes, x * 50 + y; 0x14 = no tile, the pass skips it), the
// byte per tile type at 0x0058dd34, the type records' byte at 0x00578376 + 0x30 * type, the cell-table widths 0x0053f3e8,
// the two shade grids 0x00822da0 / 0x0082376c (mode 1), the 18 hole records 0x00575ab0 + 0x208 * h (+0 par byte, +4 word,
// +8 / +0xc tee tile, +0x18 / +0x1c green tile, +0x20 count, +0x1ec total; record 0 is scratch), the waypoint records
// 0x0059ae80 + 0x18 * h (+8 / +0xc and +0x10 / +0x14, tile << 10, -1 = none), the 64 walker records 0x00585850 + 0x4c * i
// (pickGolferName 0x00467600's base: +0x10 / +0x11 tile, +0x12 flags, +0x13 signed state, +0x16 byte, +0x1c / +0x1e /
// +0x22 / +0x24 / +0x26 words), the 152 golfer records' hole byte 0x005794d9 + 0x100 * g, and the mode [0x004d20dc].
// BR(jcc, cond) is `cond`. In the coverage build (shim/src/re/c3et_cov.cpp, generated by log/c3/alt/c3et_cov_gen.py
// with C3ET_COV defined, never listed in shim/re_batches.txt) it also counts cond's two outcomes per jcc address in
// the exported C3etCovTable (entries {addr, true count, false count}); log/c3/alt/c3et_cov.py reads it.
#ifdef C3ET_COV
}  // namespace
extern "C" __declspec(dllexport) volatile unsigned C3etCovTable[3 * 512] = {0};
namespace {
static bool Br(unsigned addr, bool c) {
    for (int i = 0; i < 512; ++i) {
        volatile unsigned* e = &C3etCovTable[3 * i];
        if (e[0] == 0) e[0] = addr;
        if (e[0] == addr) { ++e[c ? 1 : 2]; break; }
    }
    return c;
}
#define BR(a, c) Br((a), (c) ? true : false)
#else
#define BR(a, c) (c)
#endif

typedef int(__cdecl* I3_t)(int a, int b, int c);
typedef int(__cdecl* I4_t)(int a, int b, int c, int d);
typedef int(__cdecl* I8_t)(int a, int b, int c, int d, int e, int f, int g, int h);
typedef void(__cdecl* Map_t)(int x, int y, int* sx, int* sy);
typedef void*(__cdecl* Cell_t)(int table, int col, int row);

const Map_t kMapToScreen = reinterpret_cast<Map_t>(0x00456b70);           // 0x00456d13, 0x00456daf, ...
const T3_t kDrawRoutingPoly = reinterpret_cast<T3_t>(0x00475df0);         // 0x00456d73, 0x00457030, ret 0xc
const I0_t kSpreadAllShade = reinterpret_cast<I0_t>(0x004616f0);          // 0x00456ce1
const I2_t kSampleHeight = reinterpret_cast<I2_t>(0x0042dba0);            // 0x00456df9, 0x00456e1a
const Cell_t kCellAt = reinterpret_cast<Cell_t>(0x0043d6f0);              // 0x00456e54, 0x0045775f, 0x00457906, ...
const I2_t kRateLot = reinterpret_cast<I2_t>(0x0042ef40);                 // 0x00456e6c
const I4_t kEvalPlacementArea = reinterpret_cast<I4_t>(0x0040db90);       // 0x00456e79
const I3_t kClamp = reinterpret_cast<I3_t>(0x00467130);                   // 0x00456e9d, 0x00457749, 0x00458398, ...
const I2_t kTileBlocked = reinterpret_cast<I2_t>(0x0040bf60);             // 0x00456f2b, 0x00456fd2
const I2_t kOverlayCharAt = reinterpret_cast<I2_t>(0x00456bb0);           // 0x00456f41 .. 0x00456f59
const I1_t kAppendCourseTitle = reinterpret_cast<I1_t>(0x0040daa0);       // 0x0045707c
const Text4_t kDrawCenteredText2 = reinterpret_cast<Text4_t>(0x00404bc0);  // 0x004578a8, 0x00457f09, 0x00457f4f
const T5_t kFillWindowRect = reinterpret_cast<T5_t>(0x00480b00);          // 0x004574ff, 0x0045751d, 0x004579df
const I2_t kSetLinePoint = reinterpret_cast<I2_t>(0x0040bf00);            // 0x00457738, 0x00457e55
const I5_t kLineTo = reinterpret_cast<I5_t>(0x0040bf20);                  // 0x00457833, 0x004578f0, 0x00457e66
const I8_t kAddSprite = reinterpret_cast<I8_t>(0x004628d0);               // 0x0045777d, 0x0045792d, 0x00457b1c
const I1_t kAppendDate = reinterpret_cast<I1_t>(0x0040d7b0);              // 0x00457bf9
const I1_t kAppendNumber = reinterpret_cast<I1_t>(0x0042dc00);            // 0x00457c3d
const I1_t kPickGolferName = reinterpret_cast<I1_t>(0x00467600);          // 0x00457bac, 0x00457f21
const I2_t kFoldRange = reinterpret_cast<I2_t>(0x00467270);               // 0x00457e2e
const I2_t kAbsDiff = reinterpret_cast<I2_t>(0x004672b0);                 // 0x00457e41

static inline const char* Cs(unsigned va) { return reinterpret_cast<const char*>(va); }
static inline char* HoleRec(int h) { return reinterpret_cast<char*>(0x00575ab0 + 0x208 * h); }
static inline char* WayRec(int h) { return reinterpret_cast<char*>(0x0059ae80 + 0x18 * h); }
static inline void StrcatItoa(int v) { strcat(kText, kItoa(v, reinterpret_cast<char*>(0x00821c50), 10)); }
static inline void DispSlot9c(unsigned c) {                                       // 0x00456c3e.., 0x00456ce6.., ...
    void* const d = G<void*>(0x0083ad50);
    if (d != 0) V3(d, 0x9c, 1, 1, c);
}
// the diamond of four points around (sx, sy): (sx-6, sy), (sx, sy-3), (sx+6, sy), (sx, sy+3) (0x00456d2e..0x00456d6f,
// 0x00456fe3..0x00457029), drawn by drawRoutingPoly(0x00519cd4; pts, 4, colour)
static void Diamond(int sx, int sy, unsigned colour) {
    int pts[8] = {sx - 6, sy, sx, sy - 3, sx + 6, sy, sx, sy + 3};
    kDrawRoutingPoly(reinterpret_cast<void*>(kFill), 0, reinterpret_cast<unsigned>(pts), 4, colour);
}
// word map 0x0053caf0 (x * 50 + y): the low five bits are the hole number (0x004584c9..0x004584e3 and its three twins)
static inline void SetTileHole(int x, int y, int h) {
    unsigned short& w = G<unsigned short>(0x0053caf0 + 2 * (x * 50 + y));
    w = static_cast<unsigned short>((w & 0xffe0) | h);
}
// the button under (x, y) (0x004581c2..0x004582dc)
// (each test is one jcc; BR records the outcome "inside")
static int RouteHit(int x, int y, int count) {
    if (BR(0x004581cf, x >= 0x3e) && BR(0x004581d6, x < 0x8e) && BR(0x004581de, y >= 0x13b) &&
        BR(0x004581e6, y < 0x18b)) return 1;
    if (BR(0x004581f7, x >= 0xb7) && BR(0x004581fe, x < 0x107) && BR(0x00458206, y >= 0xfe) &&
        BR(0x0045820e, y < 0x14e)) return 0;
    if (BR(0x0045821c, x >= 0x218) && BR(0x00458223, x < 0x268) && BR(0x0045822b, y >= 0xfe) &&
        BR(0x00458233, y < 0x14e)) return 2;
    if (BR(0x00458244, x >= 0x293) && BR(0x0045824b, x < 0x2e3) && BR(0x00458253, y >= 0x13b) &&
        BR(0x0045825b, y < 0x18b)) return 3;
    if (BR(0x00458269, x >= 0x296) && BR(0x00458270, x < 0x2e6) && BR(0x00458278, y >= 0x214) &&
        BR(0x00458280, y < 0x255)) return -2;
    if (BR(0x00458291, G<int>(0x004d20dc) == 0) && BR(0x00458296, count > 8) && BR(0x0045829d, x >= 0x307)) {
        // 0x004582a4 / 0x004582c3: x < 0x319 is tested before each y pair
        if (BR(0x004582a4, x < 0x319) && BR(0x004582a9, y >= 0x40) && BR(0x004582ae, y < 0x65)) return 9;
        if (BR(0x004582c3, x < 0x319) && BR(0x004582cb, y >= 0xcd) && BR(0x004582d3, y < 0xf2)) return 10;
    }
    return -1;
}

typedef int(__cdecl* ShowRoutingMap_t)();
ShowRoutingMap_t ShowRoutingMap_orig;

int __cdecl ShowRoutingMap_re() {
    int scroll = 0;                       // L34 (0x00456be6)
    int selected = 1;                     // L38 (0x00456bee)
    int perm[20];                         // L90 (0x00456bf6..0x00456c08): perm[i] = i
    for (int i = 0; i < 20; ++i) perm[i] = i;
    int state = -1;                       // L24 (0x00456c10)
    // L3c: the listed walkers, state byte != 0xfa, flags != 0, state < 0 (0x00456c1c..0x00456c3c)
    int count = 0;
    for (int i = 0; i < 64; ++i) {
        const char* w = reinterpret_cast<const char*>(0x00585850 + 0x4c * i);
        if (BR(0x00456c26, At<unsigned char>(w, 0x13) != 0xfa) && BR(0x00456c2d, w[0x12] != 0) &&
            BR(0x00456c31, At<signed char>(w, 0x13) < 0)) ++count;
    }
    DispSlot9c(1);                                                                    // 0x00456c54
    unsigned colour = 0;   // edi / L48: assigned on every drawn tile for modes -2..2 (others read L48 uninitialised)
    bool more = false;     // L23

    for (;;) {                                                                        // 0x00456c5a: redraw
        if (BR(0x00456c61, G<int>(0x00822b80) == 0)) {                                // 0x00456c61
            kImageDraw(reinterpret_cast<void*>(0x008224c8), 0, kFill, 0, 0xfd, 0);    // 0x00456c80
            const int m = G<int>(0x004d20dc);                                         // 0x00456c85
            // modes -1..2 index 0x00822520 + 0x2c * m (m = -1 lands on 0x008224f4, the else image)
            const unsigned img = (BR(0x00456c8e, m >= -1) && BR(0x00456c93, m < 3))  // 0x00456c8e..0x00456cce
                                     ? 0x00822520 + 0x2c * m : 0x008224f4;
            kImageDraw(reinterpret_cast<void*>(img), 0, kFill, 0, 0, 0);              // 0x00456cd3
        }
        if (BR(0x00456cdf, G<int>(0x004d20dc) == 1)) kSpreadAllShade();               // 0x00456cd8..0x00456ce1
        DispSlot9c(5);                                                                // 0x00456cf8

        const signed char* const grid = reinterpret_cast<const signed char*>(0x005722e8);
        // pass 1 (0x00456cfe..0x00456d88): a diamond in 0x80001284 for every tile whose type's 0x0058dd34 byte is 0xff
        for (int x = 0; x < 50; ++x)
            for (int y = 0; y < 50; ++y) {
                int sx, sy;
                kMapToScreen(x, y, &sx, &sy);                                         // 0x00456d13
                const signed char t = grid[x * 50 + y];
                if (BR(0x00456d20, t == 0x14)) continue;                              // 0x00456d1e
                if (BR(0x00456d2c, G<unsigned char>(0x0058dd34 + t) != 0xff)) continue;   // 0x00456d25
                Diamond(sx, sy, 0x80001284);                                          // 0x00456d73
            }
        // pass 2 (0x00456d8e..0x00457047): one diamond per tile in the mode's colour
        for (int x = 0; x < 50; ++x)
            for (int y = 0; y < 50; ++y) {
                int sx, sy;
                kMapToScreen(x, y, &sx, &sy);                                         // 0x00456daf
                const signed char t = grid[x * 50 + y];
                if (BR(0x00456dc0, t == 0x14)) continue;                              // 0x00456dbe
                if (BR(0x00456dd0, G<unsigned char>(0x0058dd34 + t) != 0xff) &&
                    BR(0x00456de3, G<unsigned char>(0x00578376 + 0x30 * t) == 0xd)) {   // 0x00456dc9 / 0x00456ddc
                    // the results of these sampleHeight calls are not read (0x00456dfe, 0x00456e1f)
                    int table = 0x19d;                                                // 0x00456df1
                    kSampleHeight(x << 8, y << 8);                                    // 0x00456df9
                    if (BR(0x00456e04, t == 0xf)) {
                        table = 0x197;                                                // 0x00456e12
                        kSampleHeight(x << 7, y << 7);                                // 0x00456e1a
                    } else if (BR(0x00456e27, t == 0xe)) {
                        table = 0x194;                                                // 0x00456e29
                    } else if (BR(0x00456e36, t == 0x10)) {
                        table = 0xf9;                                                 // 0x00456e38
                    }
                    G<void*>(0x00822b84) =                                            // 0x00456e54 / 0x00456e5c
                        kCellAt(table, G<int>(0x0053f3e8 + 4 * table) - 1, x & 3);
                }
                if (BR(0x00456e68, G<int>(0x004d20dc) == 2)) {                        // 0x00456e61
                    const int lot = kRateLot(x, y);                                   // 0x00456e6c
                    const int area = kEvalPlacementArea(x, y, 2, 5);                  // 0x00456e79
                    const int v = kClamp((lot / 4 - area) * 3 / 2, 0, 0xff);          // 0x00456e80..0x00456e9d
                    colour = 0x80000000 | ((static_cast<unsigned>(v) & 0xff) >> 3) << 5;   // 0x00456ea7..0x00456eb6
                    if (BR(0x00456ebc, area == -1)) colour = 0x80002000;              // 0x00456eb9..0x00456ebe
                }
                const int m = G<int>(0x004d20dc);                                     // 0x00456ec3
                bool sw = false;
                if (BR(0x00456ecb, m == 1)) {
                    // red = shade A >> 3, green = shade B >> 3, blue = (A + B) & 0xff >> 4 (0x00456ed1..0x00456f0d)
                    const unsigned a = G<unsigned char>(0x00822da0 + x * 50 + y);
                    const unsigned b = G<unsigned char>(0x0082376c + x * 50 + y);
                    colour = 0x80000000 | (a >> 3) << 10 | (b >> 3) << 5 | ((a + b) & 0xff) >> 4;
                    if (BR(0x00456f18, t == 0x11)) colour = 0x80000218;               // 0x00456f16 / 0x00456f1a
                }
                if (m == 1 || BR(0x00456f27, m != 0)) {                               // 0x00456f25 (mode 1 jumps here)
                    if (BR(0x00456f35, kTileBlocked(x, y) != 0)) colour = 0x80002108;   // 0x00456f2b..0x00456f37
                    kOverlayCharAt(x + 1, y);                                         // 0x00456f41 (results not read)
                    kOverlayCharAt(x, y);                                             // 0x00456f48
                    kOverlayCharAt(x, y + 1);                                         // 0x00456f52
                    kOverlayCharAt(x, y);                                             // 0x00456f59
                }
                if (BR(0x00456f68, G<int>(0x004d20dc) <= 0)) sw = true;               // 0x00456f61..0x00456f68
                if (sw) {
                    // the type record's byte through the byte table 0x0045875c and the jump table 0x00458738
                    const int k = G<signed char>(0x00578376 + 0x30 * t);              // 0x00456f6a..0x00456f78
                    BR(0x00456f82, static_cast<unsigned>(k) > 0x12);                  // ja 0x456fcb
                    switch (k) {                                                      // 0x00456f8c
                        case 0: case 1: BR(0x00456f93, true); colour = 0x80003394; break;
                        case 2: BR(0x00456f9a, true); colour = 0x80001310; break;
                        case 4: BR(0x00456fa1, true); colour = 0x80000204; break;
                        case 7: BR(0x00456fa8, true); colour = 0x80006310; break;
                        case 10: BR(0x00456faf, true); colour = 0x80005304; break;
                        case 13: BR(0x00456fbd, true); colour = 0x80001184; break;
                        case 17: BR(0x00456fb6, true); colour = 0x80000218; break;
                        case 18: BR(0x00456fc4, true); colour = 0x800010c8; break;
                        default: BR(0x00456fcb, true); colour = 0x8000318c; break;  // (and above 0x12)
                    }
                    if (BR(0x00456fdc, kTileBlocked(x, y) != 0)) colour = 0x80000000;   // 0x00456fd2..0x00456fde
                }
                Diamond(sx, sy, colour);                                              // 0x00457030
            }
        if (BR(0x00457058, G<int>(0x00822b80) != 0)) {                                // 0x0045704d / 0x00457058
            DispSlot9c(1);                                                            // 0x004586b5
            break;
        }

        // the title and the mode's legend (0x0045705e..0x00457489)
        kSetDrawTarget(reinterpret_cast<void*>(kFill), 0, 0x00821f28, 0, 0, 0);       // 0x0045706e
        kText[0] = 0;                                                                 // 0x00457075
        kAppendCourseTitle(1);                                                        // 0x0045707c
        const int m0 = G<int>(0x004d20dc);                                            // 0x00457081
        if (BR(0x0045708c, m0 == 0)) strcat(kText, Cs(0x004d3650));                   // 0x0045708e..0x004570b9
        kDrawCenteredText(kText, 0x190, m0 != 0 ? 0x3a : 0x30, kBlack);               // 0x004570bb..0x004570d5
        kSetDrawTarget(reinterpret_cast<void*>(kFill), 0, 0x00821020, 0, 0, 0);       // 0x004570ed
        kDrawCenteredText(Cs(0x004d3644), 0x190, 0xf, kBlack);                        // 0x00457103
        kSetDrawTarget(reinterpret_cast<void*>(kFill), 0, 0x00821ee8, 0, 0, 0);       // 0x0045711b
        const int m1 = G<int>(0x004d20dc);                                            // 0x00457120
        if (BR(0x00457128, m1 == -1)) {                                               // 0x004571f5
            kDrawCenteredText(Cs(0x004d3600), 0x190, 0x55, kBlack);
            kDrawCenteredText(Cs(0x004d35e4), 0x190, 0x6c, kBlack);
            kDrawCenteredText(Cs(0x004d35c8), 0x190, 0x7e, kBlack);
            kDrawCenteredText(Cs(0x004c591c), 0x47, 0x55, kBlack);
            kDrawCenteredText(Cs(0x004c591c), 0x234, 0x55, kBlack);
            kDrawCenteredText(Cs(0x004d35c4), 0x84, 0x55, kBlack);
            kDrawCenteredText(Cs(0x004d35c4), 0x276, 0x55, kBlack);
            kDrawCenteredText(Cs(0x004d35c0), 0xb6, 0x55, kBlack);
            kDrawCenteredText(Cs(0x004d35c0), 0x2a8, 0x55, kBlack);
            kDrawCenteredText(Cs(0x004d2124), 0xef, 0x55, kBlack);
            kDrawCenteredText(Cs(0x004d2124), 0x2e1, 0x55, kBlack);                   // 0x004572e5
        } else if (BR(0x00457131, m1 == 1)) {                                         // 0x0045718c
            kDrawCenteredText(Cs(0x004d3620), 0x190, 0xbc, kBlack);
            kDrawCenteredText(Cs(0x004d3618), 0x190, 0x55, kBlack);
            kDrawText4049d0(Cs(0x004c69d8), 0x270, 0xce, kBlack);
            kDrawText4049d0(Cs(0x004d3610), 0x88, 0xce, kBlack);                      // 0x004571e8
        } else if (BR(0x00457136, m1 == 2)) {                                         // 0x0045713c
            kDrawCenteredText(Cs(0x004d3634), 0x190, 0xbc, kBlack);
            kDrawText4049d0(Cs(0x004d362c), 0x277, 0xce, kBlack);
            kDrawText4049d0(Cs(0x004d32ec), 0x8e, 0xce, kBlack);                      // 0x00457182
        }
        if (BR(0x004572f4, G<int>(0x004d20dc) == 1)) {                                // 0x004572ed
            kSetQuad(reinterpret_cast<void*>(kFill), 0, 0x80000000, 0xffffffff, 2, 2);   // 0x00457306
            strcpy(kText, Cs(0x004d3540));                                            // 0x0045730b..0x0045733f
            kMeasureText3(reinterpret_cast<void*>(kFill), 0, reinterpret_cast<unsigned>(kText), 0xfa, 0x6e, 0x12c);
        }                                                                             // 0x00457346 drawCenteredTextLines
        if (BR(0x00457352, G<int>(0x004d20dc) == 2)) {                                // 0x0045734b
            kDrawText4049d0(Cs(0x004d351c), 0x33, 0x56, kBlack);
            kDrawText4049d0(Cs(0x004d3500), 0x33, 0x68, kBlack);
            kDrawText4049d0(Cs(0x004d34e4), 0x33, 0x79, kBlack);
            kDrawText4049d0(Cs(0x004d34c0), 0x33, 0x8a, kBlack);
            strcpy(kText, Cs(0x004d34b4));                                            // 0x004573a7..0x004573db
            strcat(kText, Cs(0x004c27a0));                                            // 0x004573dd..0x0045740b
            kDrawText4049d0(kText, 0x33, 0x9c, kBlack);                               // 0x0045740d
            kDrawText4049d0(Cs(0x004d3490), 0x1ee, 0x56, kBlack);
            kDrawText4049d0(Cs(0x004d3478), 0x1ee, 0x68, kBlack);
            kDrawText4049d0(Cs(0x004d345c), 0x1ee, 0x79, kBlack);
            kDrawText4049d0(Cs(0x004d3438), 0x1ee, 0x8a, kBlack);
            kDrawText4049d0(Cs(0x004d3418), 0x1ee, 0x9c, kBlack);                     // 0x00457484
        }

        // the 18 holes (0x0045748c..0x00457957): L14 = h, L1c = the waypoint pointer, ebx = record + 0x20
        for (int h = 1; h <= 18; ++h) {
            char* const rec = HoleRec(h);
            const int y = ((h - 1) % 9) * 17 + 0x68;                                  // 0x004574aa..0x004574b5
            const int colX = h <= 9 ? 0x47 : 0x239;                                   // 0x004574c0..0x004574cc
            if (BR(0x004574d6, G<int>(0x004d20dc) == -1)) {                           // 0x004574cf / 0x004574d6
                if (BR(0x004574e2, h == selected)) {                                  // 0x004574e2
                    kFillWindowRect(reinterpret_cast<void*>(0x00519a60), 0, colX - 0x27, y - 3, 0xee, 0x11,
                                    0x800023e8);                                      // 0x004574ff
                    kFillWindowRect(reinterpret_cast<void*>(0x00519a60), 0, colX - 0x26, y - 2, 0xec, 0xf,
                                    0x80007fff);                                      // 0x0045751d
                }
                kText[0] = 0;
                StrcatItoa(h);                                                        // 0x00457535..0x0045756e
                kDrawCenteredText(kText, colX, y, rec[0] != 0 ? 0x80000000 : 0x80006318);   // 0x00457565..0x00457581
                if (BR(0x0045758e, rec[0] != 0)) {                                    // 0x0045758e
                    kText[0] = 0;
                    StrcatItoa(At<signed char>(rec, 0));                              // 0x004575a6
                    kDrawCenteredText(kText, colX + 0x3d, y, kBlack);                 // 0x004575e8
                    kText[0] = 0;
                    StrcatItoa(At<short>(rec, 4));                                    // 0x00457600
                    kDrawCenteredText(kText, colX + 0x6f, y, kBlack);                 // 0x00457642
                    if (BR(0x0045764e, At<int>(rec, 0x20) != 0)) {                    // 0x0045764e
                        kText[0] = 0;
                        StrcatItoa(At<int>(rec, 0x1ec) / At<int>(rec, 0x20) / 40);    // 0x0045765d..0x0045767f
                        strcat(kText, Cs(0x004d3140));                                // 0x004576b4..0x004576ec
                        kDrawCenteredText(kText, colX + 0xa8, y, kBlack);             // 0x004576ee
                    }
                }
            }
            if (BR(0x004576fb, rec[0] == 0)) continue;                                // 0x004576f6..0x004576fb
            // the tee (0x00457701..0x00457782): the screen pair is seeded with tile * 8 + (0x184, 0x68) and then
            // overwritten by mapToScreen
            int sx = At<int>(rec, 8) * 8 + 0x184, sy = At<int>(rec, 0xc) * 8 + 0x68;
            kMapToScreen(At<int>(rec, 8), At<int>(rec, 0xc), &sx, &sy);              // 0x00457729
            kSetLinePoint(sx, sy);                                                    // 0x00457738
            const int pv = kClamp(At<signed char>(rec, 0) - 3, 0, 2);                 // 0x00457749
            void* cell = kCellAt(pv + 0x18d, G<int>(0x0053f3e8 + 4 * (pv + 0x18d)) - 1, 0);   // 0x0045775f
            G<void*>(0x00822b84) = cell;                                              // 0x0045776c
            kAddSprite(sx, sy, sy, 0x28, 0x28, reinterpret_cast<int>(cell), 4, 0x5e);   // 0x0045777d
            kText[0] = 0;
            StrcatItoa(h);                                                            // 0x00457798..0x004577c8
            // the waypoint: +0x10 / +0x14, else +8 / +0xc, else none (0x004577ca..0x004577ee)
            const int* const wp = reinterpret_cast<const int*>(WayRec(h) + 0x10);
            int ax = 0, ay = 0;
            bool have = true;
            if (BR(0x004577d3, wp[0] != -1)) { ax = wp[0] >> 10; ay = wp[1] >> 10; }
            else if (BR(0x004577ee, wp[-2] != -1)) { ax = wp[-2] >> 10; ay = wp[-1] >> 10; }
            else have = false;
            if (have) {
                sx = ax * 8 + 0x184;                                                  // 0x004577d8 / 0x004577f3
                sy = ay * 8 + 0x68;                                                   // 0x00457804
                kMapToScreen(ax, ay, &sx, &sy);                                       // 0x0045781b
                kLineTo(sx, sy, 0x80007fff, 2, 0xa);                                  // 0x00457833
                if (BR(0x00457842, G<int>(0x004d20dc) != 0))                          // 0x00457838 / 0x00457842
                    kDrawCenteredText2(kText, sx, sy - 4, 0x80007ff0);                // 0x004578a8
            } else {
                const int tx = sx, ty = sy;                                           // edi, ebp (0x0045785a / 0x0045785e)
                kMapToScreen(At<int>(rec, 0x18), At<int>(rec, 0x1c), &sx, &sy);       // 0x00457871
                if (BR(0x00457880, G<int>(0x004d20dc) != 0))                          // 0x00457876 / 0x00457880
                    kDrawCenteredText2(kText, (sx + tx) / 2, (sy + ty) / 2 - 4, 0x80007ff0);   // 0x00457882..0x004578a8
            }
            // the green (0x004578b0..0x00457932)
            sx = At<int>(rec, 0x18) * 8 + 0x184;
            sy = At<int>(rec, 0x1c) * 8 + 0x68;
            kMapToScreen(At<int>(rec, 0x18), At<int>(rec, 0x1c), &sx, &sy);           // 0x004578d8
            kLineTo(sx, sy, 0x80007fff, 2, 0xa);                                      // 0x004578f0
            const int g5 = G<signed char>(0x005a34e0);                                // 0x004578f5 / 0x0045790b
            cell = kCellAt(g5 + 0x189, 0, 3);                                         // 0x00457906
            G<void*>(0x00822b84) = cell;                                              // 0x00457915
            kAddSprite(sx, sy, sy, 0x28, 0x28, reinterpret_cast<int>(cell), 2, g5 + 0x60);   // 0x0045792d
        }

        // the walker list (0x0045795d..0x00457f97)
        int skipped = 0;                  // ebp / L30 (0x00457962)
        int n = 1;                        // L18 (0x00457966)
        more = false;                     // L23 (0x00457972)
        if (BR(0x00457977, G<int>(0x004d20dc) == 0) && BR(0x00457980, count > 8)) {   // 0x00457977..0x00457980
            const int off = scroll * 0x5f / count;                                    // 0x00457982..0x0045799e
            kImageDraw(reinterpret_cast<void*>(0x00822788), 0, kFill, 0x307, 0x40, 0);   // 0x004579af
            int hgt = 0x2f8 / count;                                                  // 0x004579b4..0x004579bf
            if (BR(0x004579c5, hgt >= 0x5f - off)) hgt = 0x5f - off;                  // 0x004579c1..0x004579c7
            kFillWindowRect(reinterpret_cast<void*>(0x00519a60), 0, 0x30d, off + 0x6a, 6, hgt, 0x80007fff);
        }                                                                             // 0x004579df
        for (int i = 0; i < 64; ++i) {                                                // L1c, L10 = record + 0x1e
            char* const w = reinterpret_cast<char*>(0x00585850 + 0x4c * i);
            if (BR(0x004579fc, G<int>(0x004d20dc) != 0)) continue;                    // 0x004579f5..0x004579fc
            const unsigned char fl = At<unsigned char>(w, 0x12);
            if (BR(0x00457a07, fl == 0)) continue;                                    // 0x00457a05
            const signed char st = At<signed char>(w, 0x13);
            if (BR(0x00457a12, st >= 0)) continue;                                    // 0x00457a10
            const int row = (G<int>(0x005685f4) - At<unsigned char>(w, 0x16) - 2) & 7;   // 0x00457a18..0x00457a30
            int base = -2 - st;                                                       // 0x00457a23..0x00457a2e
            if (BR(0x00457a36, (fl & 8) != 0)) base += 4;                             // 0x00457a33..0x00457a38
            const int last = base + 0x82;                                             // L40 (0x00457a3b / 0x00457a44)
            if (BR(0x00457a48, st != -6)) {                                           // 0x00457a41 / 0x00457a48
                if (BR(0x00457a52, skipped < scroll)) {                               // 0x00457a4e / 0x00457a52
                    ++skipped;
                } else if (BR(0x00457a67, n - skipped > 8)) {                         // 0x00457a5e..0x00457a67
                    more = true;                                                      // 0x00457a69
                } else {
                    const int line = n - skipped;                                     // esi
                    const short kind = At<short>(w, 0x1e);                            // 0x00457a73
                    int table = BR(0x00457a7a, kind < 0xb) ? 0x20e                   // 0x00457a76..0x00457a8e
                                : (BR(0x00457a8c, kind == 0xc) ? 0x21e : 0x216);
                    table += base;                                                    // 0x00457a93
                    const int col = At<short>(w, 0x1c) % G<int>(0x0053f3e8 + 4 * table);   // 0x00457a95..0x00457a9a
                    if (BR(0x00457aa3, col == 0) && BR(0x00457aa9, kind == 0xc))      // 0x00457aa1..0x00457aab
                        At<short>(w, 0x1e) = 0xb;
                    void* const cell = kCellAt(table, col, row);                      // 0x00457ab3
                    G<void*>(0x00822b84) = cell;                                      // 0x00457abe
                    const int pick = ((At<unsigned char>(w, 0x12) & 8) >> 3) - (2 * At<signed char>(w, 0x13) + 4);
                    int x = BR(0x00457ad9, line >= 5) ? 0x19f : 0x29;                 // setge 0x00457ad9, L14 = pick
                    int y = ((line - 1) % 4) * 45 + 0x47;                             // 0x00457add..0x00457b07
                    kAddSprite(x + 0x14b, y + 0x20, y, 0x28, 0x28, reinterpret_cast<int>(cell), 4, last);   // 0x00457b1c
                    kSetDrawTarget(G<void*>(0x004c1570), 0, 0x00821ee8, 0, 0, 0);    // 0x00457b35
                    kText[0] = 0;
                    StrcatItoa(n);                                                    // 0x00457b4d..0x00457b7a
                    strcat(kText, Cs(0x004d3414));                                    // 0x00457b7c..0x00457baa
                    kPickGolferName(i);                                               // 0x00457bac
                    kDrawText4049d0(kText, x, y, kBlack);                             // 0x00457bbd
                    y += 0xf;                                                         // 0x00457bcc
                    x += 0xf;                                                         // 0x00457bcf
                    strcpy(kText, Cs(0x004c849c));                                    // 0x00457bc2..0x00457bef
                    kAppendDate(At<short>(w, 0x22) << 10);                            // 0x00457bf1..0x00457bf9
                    strcat(kText, Cs(0x004d3408));                                    // 0x00457bfe..0x00457c34
                    kAppendNumber(At<short>(w, 0x24) * 100);                          // 0x00457c28..0x00457c3d
                    kDrawText4049d0(kText, x, y, kBlack);                             // 0x00457c4e
                    y += 0xf;                                                         // 0x00457c5a
                    // the jump table 0x00458770: each case replaces the text (strcpy); above 7 the text stays
                    static const unsigned kPick[8] = {0x004d33f4, 0x004d33e0, 0x004d33cc, 0x004d33b4,
                                                      0x004d33a0, 0x004d338c, 0x004d3378, 0x004d3360};
                    if (BR(0x00457c60, static_cast<unsigned>(pick) <= 7)) {           // 0x00457c5d..0x00457d32
                        BR(0x00457c68 + pick, true);   // one counter per case of the jump table 0x00458770
                        strcpy(kText, Cs(kPick[pick]));
                    }
                    StrcatItoa(At<short>(w, 0x26));                                   // 0x00457d34..0x00457d7d
                    kDrawText4049d0(kText, x, y, kBlack);                             // 0x00457d7f
                }
            }
            // the marker (0x00457d8b..0x00457f7d)
            int sx = At<signed char>(w, 0x10) * 8 + 0x184;                            // L28 (0x00457d8f)
            int sy = At<signed char>(w, 0x11) * 8 + 0x68;                             // L2c (0x00457d9e)
            kMapToScreen(At<signed char>(w, 0x10), At<signed char>(w, 0x11), &sx, &sy);   // 0x00457db5
            unsigned mc;
            int len = 0x18;
            switch (At<signed char>(w, 0x13) + 5) {                                   // the jump table 0x00458790
                case 0: BR(0x00457de3, true); mc = 0x800003ff; break;                 // 0x00457de3
                case 1: BR(0x00457dd0, true); mc = 0x80007ff0; len = 0x30; break;     // 0x00457dd0
                case 2: BR(0x00457ddc, true); mc = 0x80000018; break;                 // 0x00457ddc
                case 3: BR(0x00457dea, true); mc = 0x80006318; break;                 // 0x00457dea
                default: BR(0x00457df1, true); mc = 0x80007fff; break;                // 0x00457df1 (ja 0x00457dc7)
            }
            if (BR(0x00457e04, (At<unsigned char>(w, 0x12) & 8) != 0)) len += len / 2;   // 0x00457dfb..0x00457e0f
            const int half = len / 2;                                                 // L40 (0x00457e13..0x00457e1c)
            for (int k = 0; k < 25; ++k) {                                            // 0x00457e20..0x00457e72
                const int a = static_cast<int>(static_cast<unsigned>(k) * 0x0aaaaaaau);
                const int px = kFoldRange(a, len) + sx;                               // 0x00457e2e / 0x00457e3f
                const int py = kAbsDiff(a, half) + sy;                                // 0x00457e41 / 0x00457e4d
                if (BR(0x00457e51, k == 0)) kSetLinePoint(px, py);                    // 0x00457e55
                else kLineTo(px, py, mc, 2, 7);                                       // 0x00457e66
            }
            if (BR(0x00457e7c, At<signed char>(w, 0x13) != -6)) {                     // 0x00457e78
                kText[0] = 0;
                StrcatItoa(n);                                                        // 0x00457e82..0x00457eca
                ++n;                                                                  // 0x00457e8a / 0x00457e98
                strcat(kText, Cs(0x004d3414));                                        // 0x00457ecc..0x00457efc
                kDrawCenteredText2(kText, sx, sy - 10, mc);                           // 0x00457f09
            }
            kText[0] = 0;                                                             // 0x00457f19
            kPickGolferName(i);                                                       // 0x00457f21
            kSetDrawTarget(G<void*>(0x004c1570), 0, 0x00519fd8, 0, 0, 0);            // 0x00457f3a
            kDrawCenteredText2(kText, sx, sy, mc);                                    // 0x00457f4f
            // 0x00457f54..0x00457f7d recompute L28 / L2c from the dwords at +0 / +4; nothing reads them afterwards
        }

        // the buttons (0x00457f9d..0x004581a5)
        DispSlot9c(1);                                                                // 0x00457faf
        struct Img { unsigned img, x, y; };
        static const Img kState[5] = {{0x008225fc, 0xb6, 0xfe}, {0x008225a4, 0x3c, 0x139},
                                      {0x00822628, 0x1e3, 0xfe}, {0x008225d0, 0x264, 0x139},
                                      {0x00822704, 0x296, 0x214}};
        // the state's image: -2, 1, 0, 2, 3 tested in that order (0x00457fbc / 0x00457fe6 / 0x00458009 / 0x0045802f /
        // 0x00458056)
        const Img* si = 0;
        if (BR(0x00457fbc, state == -2)) si = &kState[4];
        else if (BR(0x00457fe6, state == 1)) si = &kState[1];
        else if (BR(0x00458009, state == 0)) si = &kState[0];
        else if (BR(0x0045802f, state == 2)) si = &kState[2];
        else if (BR(0x00458056, state == 3)) si = &kState[3];
        if (si) kImageDraw(reinterpret_cast<void*>(si->img), 0, kFill, si->x, si->y, 0);   // 0x00457fb9..0x00458078
        const int m2 = G<int>(0x004d20dc);                                            // 0x0045807d
        if (BR(0x00458084, m2 == 0)) {
            if (BR(0x00458089, state == 9))
                kImageDraw(reinterpret_cast<void*>(0x00822730), 0, kFill, 0x307, 0x40, 0);
            else if (BR(0x004580ad, state == 10))
                kImageDraw(reinterpret_cast<void*>(0x0082275c), 0, kFill, 0x307, 0xcd, 0);
            kImageDraw(reinterpret_cast<void*>(0x00822654), 0, kFill, 0x3c, 0x139, 0);   // 0x004580d4..0x00458166
        } else if (BR(0x004580f6, m2 == -1)) {
            kImageDraw(reinterpret_cast<void*>(0x008226ac), 0, kFill, 0xb6, 0xfe, 0);
        } else if (BR(0x0045811d, m2 == 1)) {
            kImageDraw(reinterpret_cast<void*>(0x008226d8), 0, kFill, 0x1e2, 0xfe, 0);
        } else if (BR(0x00458144, m2 == 2)) {
            kImageDraw(reinterpret_cast<void*>(0x00822680), 0, kFill, 0x264, 0x139, 0);
        }
        kImageDraw(reinterpret_cast<void*>(0x00822b50), 0, kFill, 0x47, 0x215, 0);   // 0x00458188
        kRefreshFull(reinterpret_cast<void*>(0x00519a60), 0, 0);                      // 0x00458194
        kWaitTicks(0x14, 0);                                                          // 0x0045819d
        kEnterModal();                                                                // 0x004581a5

        // the input loop (0x004581ae..0x004586a1)
        bool redraw = false, quit = false;
        while (!redraw && !quit) {
            int px, py;
            kViewToLocal(reinterpret_cast<void*>(0x00519a60), 0, &px, &py);           // 0x004581bd
            const int prev = state;                                                   // edx (0x004581cd)
            state = RouteHit(px, py, count);                                          // L24 (0x004582e1)
            if (BR(0x004582e5, prev != state)) { redraw = true; break; }              // 0x004582df / 0x004582e5
            kPumpMessages();                                                          // 0x004582eb
            kWaitTicks(1, 0);                                                         // 0x004582f4
            int click = G<int>(0x00822d68);                                           // 0x004582f9
            int cy;
            bool clicked = true;
            if (BR(0x00458304, click == 0)) {
                if (BR(0x0045830d, kFlushRedraw() == 0)) continue;                    // 0x00458306 / 0x0045830d
                click = G<int>(0x00822d68);                                           // 0x00458313
                clicked = BR(0x0045831b, click != 0);                                 // je 0x00458343
            }
            cy = G<int>(0x00822d64);                                                  // 0x0045831d / 0x00458343
            if (clicked && BR(0x00458327, cy > 0x118)) {                              // 0x00458322 / 0x00458327
                if (BR(0x0045832b, state >= 0)) G<int>(0x004d20dc) = state - 1;       // 0x00458329..0x00458330
                if (BR(0x00458338, state == -2)) quit = true; else redraw = true;     // 0x00458335 / 0x00458338
                break;
            }
            if (BR(0x0045834b, state == 9)) { if (BR(0x00458353, scroll > 0)) --scroll; }   // 0x00458348..0x00458356
            else if (BR(0x0045835b, state == 10) && BR(0x00458363, more)) ++scroll;   // 0x00458358..0x00458369
            if (BR(0x00458373, cy >= 0x101)) { quit = true; break; }                  // 0x0045836e / 0x00458373
            if (BR(0x0045837c, click == 1)) {                                         // 0x00458379
                int v = kClamp((cy - 0x68) / 17 + 1, click, 0x13);                    // 0x0045837e..0x00458398
                if (BR(0x004583ac, G<int>(0x00822d60) >= 0x190)) v += 9;              // 0x004583a6 / 0x004583ae
                selected = kClamp(v, 1, 0x12);                                        // 0x004583b6 / 0x004583c1
                click = G<int>(0x00822d68);                                           // 0x004583bb
                cy = G<int>(0x00822d64);                                              // 0x004583c5
            }
            if (BR(0x004583d4, click == 2)) {                                         // 0x004583d1
                int t = kClamp((cy - 0x68) / 17 + 1, 1, 0x13);                        // 0x004583da..0x004583f5
                if (BR(0x00458409, G<int>(0x00822d60) >= 0x190)) t += 9;              // 0x00458403 / 0x0045840b
                t = kClamp(t, 1, 0x12);                                               // 0x00458413
                const int s = selected;                                               // ebx (0x004583cd)
                // hole s into the scratch record 0, its waypoint record and its permutation entry likewise
                memcpy(HoleRec(0), HoleRec(s), 0x208);                                // 0x00458435..0x00458440
                const int saved = perm[s];                                            // 0x00458424
                memcpy(WayRec(0), WayRec(s), 0x18);                                   // 0x00458453..0x00458463
                perm[0] = saved;                                                      // 0x0045844c
                if (BR(0x00458470, t > s)) {                                          // 0x0045844a / 0x00458470
                    for (int k = s; k < t; ++k) {                                     // 0x0045848c..0x00458518
                        memcpy(HoleRec(k), HoleRec(k + 1), 0x208);
                        memcpy(WayRec(k), WayRec(k + 1), 0x18);
                        perm[k] = perm[k + 1];
                        SetTileHole(At<int>(HoleRec(k), 8), At<int>(HoleRec(k), 0xc), k);
                        SetTileHole(At<int>(HoleRec(k), 0x18), At<int>(HoleRec(k), 0x1c), k);
                    }
                } else if (BR(0x00458524, t < s)) {                                   // 0x00458524 / 0x0045852c
                    for (int k = s; k > t; --k) {                                     // 0x00458548..0x004585d4
                        memcpy(HoleRec(k), HoleRec(k - 1), 0x208);
                        memcpy(WayRec(k), WayRec(k - 1), 0x18);
                        perm[k] = perm[k - 1];
                        SetTileHole(At<int>(HoleRec(k), 8), At<int>(HoleRec(k), 0xc), k);
                        SetTileHole(At<int>(HoleRec(k), 0x18), At<int>(HoleRec(k), 0x1c), k);
                    }
                }
                memcpy(HoleRec(t), HoleRec(0), 0x208);                                // 0x004585da..0x004585f4
                memcpy(WayRec(t), WayRec(0), 0x18);                                   // 0x004585f6..0x0045860a
                perm[t] = perm[0];                                                    // 0x0045860c / 0x00458619
                perm[0] = 0;                                                          // 0x0045862b
                SetTileHole(At<int>(HoleRec(t), 8), At<int>(HoleRec(t), 0xc), t);    // 0x00458620..0x0045864c
                SetTileHole(At<int>(HoleRec(t), 0x18), At<int>(HoleRec(t), 0x1c), t);   // 0x00458654..0x00458678
                memset(HoleRec(0), 0, 0x208);                                         // 0x00458680 / 0x00458685
                memset(WayRec(0), 0, 0x18);                                           // 0x00458687..0x00458691
                click = G<int>(0x00822d68);                                           // 0x00458693
            }
            if (BR(0x0045869b, click != 0)) redraw = true; else quit = true;          // 0x00458699 / 0x0045869b
        }
        if (quit) break;
    }

    // the exit (0x004586bb..0x00458736): the inverse permutation (pre-push slot 0xd0, 20 dwords), applied to every
    // golfer's hole byte that is positive; a byte of 20 or more indexes past the table into the dwords from the return
    // address on (pre-push 0x120 = the entry esp), which _AddressOfReturnAddress() addresses identically here
    unsigned inv[20];
    for (int i = 0; i < 20; ++i) inv[perm[i]] = i;                                    // 0x004586c8..0x004586d8
    const unsigned char* const above = static_cast<const unsigned char*>(_AddressOfReturnAddress());
    for (int g = 0; g < 152; ++g) {                                                   // 0x004586da..0x004586fb
        signed char& b = G<signed char>(0x005794d9 + 0x100 * g);
        if (BR(0x004586e3, b <= 0)) continue;                                         // 0x004586e1 / 0x004586e3
        b = static_cast<signed char>(b < 20 ? inv[b] : above[4 * (b - 20)]);          // 0x004586e8 / 0x004586ef
    }
    G<int>(0x005685f0) = 0x13;                                                        // 0x004586fd
    for (int h = 1; h <= 18; ++h)                                                     // 0x00458707..0x00458721
        if (BR(0x00458714, HoleRec(h)[0] == 0)) {
            G<int>(0x005685f0) = h;                                                   // 0x0045872a
            return reinterpret_cast<int>(HoleRec(h));                                 // eax: the record's address
        }
    return 0x00578148;                                                                // eax after the scan (0x00458721)
}

}  // namespace

// 0x0048e900  TextView::build
SG_HOOK("golf_clean.exe", 0x0048e900, TextView_build, TextViewBuild_re, TextViewBuild_orig);
// 0x004a1580  LabelButton::drawContents
SG_HOOK("golf_clean.exe", 0x004a1580, LabelButton_drawContents, LabelButtonDraw_re, LabelButtonDraw_orig);
// 0x00459850  showGolferPairSelect
SG_HOOK("golf_clean.exe", 0x00459850, showGolferPairSelect, ShowGolferPairSelect_re, ShowGolferPairSelect_orig);
// 0x004767a0  allocTextBuffer
SG_HOOK("golf_clean.exe", 0x004767a0, allocTextBuffer, AllocTextBuffer_re, AllocTextBuffer_orig);
// 0x00456be0  showRoutingMapScreen
SG_HOOK("golf_clean.exe", 0x00456be0, showRoutingMapScreen, ShowRoutingMap_re, ShowRoutingMap_orig);
