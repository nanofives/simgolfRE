// C3 batch c3fb (round 33, 2026-10-13) of golf_clean.exe: size-only rows of 1-2 KB (log/c3/c3fb_candidates.tsv).
// Fixtures: re/frida/js/fixtures.d/c3fb.js. Registry: re/frida/registry.d/c3fb.py.
// Purposes and per-jcc tables: log/c3/c3fb_purpose.md. Notes and drops: log/c3/c3fb_notes.txt.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list, kept
// in log/c3/c3fb_asm/) and cites the instruction each fact comes from. __thiscall is emulated with __fastcall (ecx =
// this, edx unused); callees run through their original addresses, so a hooked callee uses its own reimplementation.
#include "hooks.h"

#include <string.h>

namespace {

template <typename T> static inline T& G(unsigned addr) { return *reinterpret_cast<T*>(addr); }
static inline void* P(unsigned addr) { return reinterpret_cast<void*>(addr); }
static inline char* S(unsigned addr) { return reinterpret_cast<char*>(addr); }

typedef int(__cdecl* SaveFieldIo_t)(void* buf, int n);
typedef int(__cdecl* C2pi_t)(const char*, int);
typedef void*(__fastcall* T0p_t)(void* ecx, void* edx);
typedef int(__fastcall* T6_t)(void* ecx, void* edx, int a, int b, int c, int d, int e, int f);
typedef int(__fastcall* T5s_t)(void* ecx, void* edx, const char* a, int b, int c, int d, int e);
typedef int(__fastcall* T7p_t)(void* ecx, void* edx, void* a, int b, int c, int d, int e, int f, int g);

// saveFieldIo(buf, n) 0x0040af70 (C3, shim/src/re/c3dw.cpp): __read or __write of the save descriptor [0x00568d08],
// chosen by the mode word [0x0053e638].
const SaveFieldIo_t kSaveFieldIo = reinterpret_cast<SaveFieldIo_t>(0x0040af70);

}  // namespace

// ====================================================================================================================
// 0x0040afa0  serializeCourse
// ====================================================================================================================
// __cdecl serializeCourse(mode), `ret` at 0x0040b492. The argument is stored in the mode word 0x0053e638 (0x0040afa0 /
// 0x0040afab) that saveFieldIo tests (non-zero reads, zero writes). Then one saveFieldIo(buf, n) per row below, in
// this order (each `push n; push buf; call 0x40af70`; the callers' pops are grouped every eight calls, `add esp,
// 0x40`). The flag word 0x0059e7b8 is itself one of the fields (call 0x0040b36d), so in read mode the two tests at
// the end see the value just read.
namespace {
struct Field { unsigned buf; int n; };
const Field kFields[] = {
    {0x00822c78, 0x4},     // call 0x0040afb0
    {0x005685f0, 0x4},     // call 0x0040afbc
    {0x0058d36c, 0x4},     // call 0x0040afc8
    {0x0059aafc, 0x4},     // call 0x0040afd4
    {0x005a34e0, 0x1},     // call 0x0040afe0
    {0x0059ae78, 0x4},     // call 0x0040afec
    {0x00541cd8, 0x4},     // call 0x0040aff8
    {0x005a882c, 0x4},     // call 0x0040b004
    {0x0056949c, 0x4},     // call 0x0040b013
    {0x005a636c, 0x4},     // call 0x0040b01f
    {0x005a9cac, 0x4},     // call 0x0040b02b
    {0x005a9cb0, 0x4},     // call 0x0040b037
    {0x005787cc, 0x4},     // call 0x0040b043
    {0x00561258, 0x4},     // call 0x0040b04f
    {0x00571fd4, 0x4},     // call 0x0040b05b
    {0x0059bf90, 0x4},     // call 0x0040b067
    {0x00575ab0, 0x28a0},  // call 0x0040b079
    {0x005722e8, 0x9c4},   // call 0x0040b088
    {0x0056988c, 0x9c4},   // call 0x0040b097
    {0x0053caf0, 0x1388},  // call 0x0040b0a6
    {0x0053bbac, 0x9c4},   // call 0x0040b0b5
    {0x005a4998, 0xa29},   // call 0x0040b0c4
    {0x00838c1c, 0x169},   // call 0x0040b0d3
    {0x0058bcb8, 0x1000},  // call 0x0040b0e2
    {0x004c284c, 0x4},     // call 0x0040b0f1
    {0x00561250, 0x4},     // call 0x0040b0fd
    {0x005794b8, 0x9c00},  // call 0x0040b10c
    {0x00585850, 0x1300},  // call 0x0040b11b
    {0x00834170, 0x4},     // call 0x0040b127
    {0x005a6364, 0x4},     // call 0x0040b133
    {0x0053a450, 0x4},     // call 0x0040b13f
    {0x00822c88, 0x4},     // call 0x0040b14b
    {0x005409ac, 0x3e8},   // call 0x0040b15d
    {0x0051b388, 0x3e8},   // call 0x0040b16c
    {0x0053e63c, 0x3e8},   // call 0x0040b17b
    {0x004c2850, 0x4},     // call 0x0040b187
    {0x0059b730, 0x4},     // call 0x0040b193
    {0x0059ae7c, 0x4},     // call 0x0040b19f
    {0x0059dea0, 0x80},    // call 0x0040b1ae
    {0x0059c08c, 0x4},     // call 0x0040b1ba
    {0x00568600, 0x3e8},   // call 0x0040b1cc
    {0x00822c70, 0x4},     // call 0x0040b1d8
    {0x005685f8, 0x4},     // call 0x0040b1e4
    {0x005a5a24, 0x4},     // call 0x0040b1f0
    {0x00584210, 0x7d0},   // call 0x0040b1ff
    {0x00543cfc, 0x4},     // call 0x0040b20b
    {0x004c2e14, 0x4},     // call 0x0040b217
    {0x00572cac, 0x4},     // call 0x0040b223
    {0x0059aaf8, 0x4},     // call 0x0040b232
    {0x0056ae70, 0x1700},  // call 0x0040b241
    {0x005422f8, 0xf0},    // call 0x0040b250
    {0x0059ae80, 0x1c8},   // call 0x0040b25f
    {0x0059b048, 0x4},     // call 0x0040b26b
    {0x005736b0, 0x2400},  // call 0x0040b27a
    {0x0059fc60, 0x3880},  // call 0x0040b289
    {0x005849e0, 0xe70},   // call 0x0040b298
    {0x005a59f8, 0x4},     // call 0x0040b2a7
    {0x005a5a04, 0x20},    // call 0x0040b2b3
    {0x0056d1b8, 0x1770},  // call 0x0040b2c2
    {0x004c2c94, 0x12},    // call 0x0040b2ce
    {0x005689e8, 0x320},   // call 0x0040b2dd
    {0x005419d0, 0x40},    // call 0x0040b2e9
    {0x00572cb0, 0xa00},   // call 0x0040b2f8
    {0x004d6088, 0xa640},  // call 0x0040b307
    {0x00543d10, 0x19a28}, // call 0x0040b319
    {0x00567b04, 0x4},     // call 0x0040b325
    {0x005a6378, 0x9c4},   // call 0x0040b334
    {0x0056c7e4, 0x9c4},   // call 0x0040b343
    {0x0053ea24, 0x9c4},   // call 0x0040b352
    {0x00542414, 0x9c4},   // call 0x0040b361
    {0x0059e7b8, 0x4},     // call 0x0040b36d  (the flag word the tail tests)
    {0x004c1578, 0x420},   // call 0x0040b37c
    {0x00571fd8, 0x30e},   // call 0x0040b38e
    {0x00520640, 0x3e8},   // call 0x0040b39d
    {0x00567328, 0x64},    // call 0x0040b3a9
    {0x00571d38, 0x4},     // call 0x0040b3b5
    {0x0056fcb0, 0x1002},  // call 0x0040b3c4
    {0x0059d81c, 0x100},   // call 0x0040b3d3
    {0x005a46b8, 0x100},   // call 0x0040b3e2
    {0x0056a51c, 0x4},     // call 0x0040b3ee
    {0x005a6374, 0x4},     // call 0x0040b3fd
    {0x0056a524, 0x28},    // call 0x0040b409
    {0x005a47e0, 0x4},     // call 0x0040b415
    {0x005830b8, 0x9c4},   // call 0x0040b424
    {0x0059c090, 0x9c4},   // call 0x0040b433
    {0x005a5a00, 0x4},     // call 0x0040b43f
};
}  // namespace

typedef int(__cdecl* SerializeCourse_t)(int mode);
SerializeCourse_t SerializeCourse_orig;

// The result: nothing in the body sets eax after the last call it makes, so the caller gets the last saveFieldIo
// result, or, when neither flag bit is set, the flag word loaded into eax at 0x0040b444 (the second test at 0x0040b465
// reads memory and leaves eax alone). saveGame 0x0040b663 / loadGame 0x0040bad6 drop it; it is reproduced anyway.
int __cdecl SerializeCourse_re(int mode) {
    G<int>(0x0053e638) = mode;                                   // 0x0040afab
    for (unsigned i = 0; i < sizeof(kFields) / sizeof(kFields[0]); ++i)
        kSaveFieldIo(P(kFields[i].buf), kFields[i].n);
    int r = G<int>(0x0059e7b8);                                  // 0x0040b444 mov eax, [0x59e7b8]
    if (r & 0x40000000)                                          // 0x0040b44c test / 0x0040b451 je
        r = kSaveFieldIo(P(0x004e06c8), 0x8c0);                  // 0x0040b45d
    if (G<int>(0x0059e7b8) & 0x200000) {                         // 0x0040b465 test [0x59e7b8] / 0x0040b46f je
        kSaveFieldIo(P(0x0058f338), 0xb7c0);                     // 0x0040b47b
        r = kSaveFieldIo(P(0x00520a28), 0x19a28);                // 0x0040b48a
    }
    return r;
}
// 0x0040afa0
SG_HOOK("golf_clean.exe", 0x0040afa0, serializeCourse, SerializeCourse_re, SerializeCourse_orig);

// ====================================================================================================================
// 0x0043cd70  showTitleScreen
// ====================================================================================================================
// __cdecl, no arguments, `ret` at 0x0043d27a. It builds the title screen's button surfaces: plays the intro movie, makes
// one 0x400 x 0x320 Image in its frame, and cuts four nine-piece frame sets from two pop-up images and seven button
// pieces from each of two title images into global Surfaces with Surface_blit 0x00473bf0. The SEH frame
// (0x0043cd70..0x0043cd7e, state slot [esp+0x2ec] = 0 at 0x0043cdbb and -1 at 0x0043d256) only matters if a callee
// throws; it is not reproduced.
namespace {
const C2pi_t kPlayAudioFile = reinterpret_cast<C2pi_t>(0x0043cce0);   // 0x0043cd97
const T0p_t kImageCtorT = reinterpret_cast<T0p_t>(0x00474ae0);       // 0x0043cda3
const T6_t kSurfaceCreateFull = reinterpret_cast<T6_t>(0x00474dd0);   // 0x0043cdc2
const T5s_t kLoadImageT = reinterpret_cast<T5s_t>(0x00475840);       // 0x0043cdd9, 0x0043cf09, 0x0043d03b, 0x0043d147
const T7p_t kSurfaceBlit = reinterpret_cast<T7p_t>(0x00473bf0);      // 37 calls
const T0p_t kImageDtorT = reinterpret_cast<T0p_t>(0x00474c40);       // 0x0043d261

// The eight pieces of one frame set, in call order: destination Surface index k (Surfaces are 0x2c apart from the set's
// base, set i adding 0x160 * i: lea esi, [ebx + ebx*10]; shl esi, 5 at 0x0043ce05 / 0x0043ce0a), source x, source y
// offset from the set's row, width, height. From the first loop's pushes 0x0043cdff..0x0043cee6 (the second loop
// 0x0043cf31..0x0043d018 uses the same numbers with base 0x0059d920).
struct Piece { int k, sx, dy, w, h; };
const Piece kFrame[8] = {
    {1, 0, 0, 0x10, 0x10},          // 0x0043ce15 [esi + 0x562394]
    {2, 0, 0x11, 0x10, 0x10},       // 0x0043ce33 [esi + 0x5623c0]
    {3, 0, 0x22, 0x10, 0x1a},       // 0x0043ce55 [esi + 0x5623ec]
    {4, 0x11, 0, 0x10, 0x10},       // 0x0043ce70 [esi + 0x562418]
    {5, 0x22, 0, 0x1a, 0x10},       // 0x0043ce8b [esi + 0x562444]
    {6, 0x22, 0x11, 0x1a, 0x10},    // 0x0043ceaa [esi + 0x562470]
    {7, 0x22, 0x22, 0x1a, 0x1a},    // 0x0043cec5 [esi + 0x56249c]
    {0, 0x11, 0x22, 0x10, 0x1a},    // 0x0043cee0 [esi + 0x562368]
};
// The seven title buttons: source rect (x, y, w, h) and the destination for the unselected image (0x0043d054..
// 0x0043d129) and for the highlighted one (0x0043d160..0x0043d235).
struct Button { int x, y, w, h; unsigned unsel, sel; };
const Button kButtons[7] = {
    {0x2a, 0x15, 0x144, 0x79, 0x0053b7e0, 0x0053b80c},
    {0x1a0, 0x1f, 0x16b, 0x8d, 0x0053b838, 0x0053b864},
    {0x1fe, 0x167, 0x10f, 0x6f, 0x0053b890, 0x0053b8bc},
    {0x11f, 0x1da, 0x120, 0x73, 0x0053b8e8, 0x0053b914},
    {0xd, 0x17e, 0x11f, 0x73, 0x0053b940, 0x0053b96c},
    {0xa8, 0xc6, 0x1d8, 0xb0, 0x0053b9f0, 0x0053ba1c},
    {0x2d1, 0x210, 0x3a, 0x39, 0x0053b998, 0x0053b9c4},
};

// The row of frame set i: the jump tables 0x0043d27c / 0x0043d28c send i = 1 to `mov edi, 0xc8` (0x0043cdec), i = 2 to
// `mov edi, 0x64` (0x0043cdf3) and i = 0 / 3 to `xor edi, edi` (0x0043cdfa); i > 3 never occurs (loop bound 4).
static int frameRow(int i) {
    if (i == 1) return 200;
    if (i == 2) return 100;
    return 0;
}

static void cutFrames(void* image, unsigned base) {
    for (int i = 0; i < 4; ++i) {                                // 0x0043cdde .. 0x0043ceef (jl)
        const int row = frameRow(i);
        for (int p = 0; p < 8; ++p) {
            const Piece& f = kFrame[p];
            kSurfaceBlit(P(base + 0x160 * i + 0x2c * f.k), 0, image, f.sx, row + f.dy, f.w, f.h, 1, 0);
        }
    }
}
}  // namespace

typedef int(__cdecl* ShowTitleScreen_t)();
ShowTitleScreen_t ShowTitleScreen_orig;

int __cdecl ShowTitleScreen_re() {
    alignas(16) unsigned char img[0x2c0];   // the frame's Image at [esp+0x14] (0x0043cd9f); the frame is 0x2bc (0x0043cd85)
    void* const image = img;
    kPlayAudioFile(S(0x004c8a0c), 0);                            // 0x0043cd91 / 0x0043cd92 / 0x0043cd97
    kImageCtorT(image, 0);                                       // 0x0043cda3
    kSurfaceCreateFull(image, 0, 0x400, 0x320, 0, 1, 0, 0);      // pushes 0x0043cda8..0x0043cdb2, 0x0043cdc2
    kLoadImageT(image, 0, S(0x004c89f4), 0, 0, 0x100, 2);        // 0x0043cdd9
    cutFrames(image, 0x00562368);
    kLoadImageT(image, 0, S(0x004c89d8), 0, 0, 0x100, 2);        // 0x0043cf09
    cutFrames(image, 0x0059d920);
    kLoadImageT(image, 0, S(0x004c89c0), 0, 0, 0x100, 2);        // 0x0043d03b
    for (int b = 0; b < 7; ++b)
        kSurfaceBlit(P(kButtons[b].unsel), 0, image, kButtons[b].x, kButtons[b].y, kButtons[b].w, kButtons[b].h, 1, 1);
    kLoadImageT(image, 0, S(0x004c89ac), 0, 0, 0x100, 2);        // 0x0043d147
    for (int b = 0; b < 7; ++b)
        kSurfaceBlit(P(kButtons[b].sel), 0, image, kButtons[b].x, kButtons[b].y, kButtons[b].w, kButtons[b].h, 1, 1);
    // 0x0043d242..0x0043d250: a loop that only steps eax from 0x004c26c2 by 0x14 to 0x004c2852 and stores nothing.
    return reinterpret_cast<int>(kImageDtorT(image, 0));         // 0x0043d261; eax is Image_dtor's at the ret
}
// 0x0043cd70
SG_HOOK("golf_clean.exe", 0x0043cd70, showTitleScreen, ShowTitleScreen_re, ShowTitleScreen_orig);

// ====================================================================================================================
// 0x004315e0  buildGolferBioText
// ====================================================================================================================
// __cdecl, no arguments, `ret` at 0x00431ba7 (the `push ecx` / `pop ecx` pair only reserves the slot [esp+0x10] that
// holds the record pointer). A new-course reset: clears the course tables, picks the twelve golfer types of the four
// arrival groups, writes a one-line bio into each of the 76 golfer-type records and resets more tables.
namespace {
typedef int(__fastcall* Range_t)(void* ecx, void* edx, int n);
typedef int(__cdecl* C0_t)();
typedef void(__cdecl* V0_t)();
const Range_t kRange = reinterpret_cast<Range_t>(0x0045c1e0);   // Random::range(this = 0x00822d9c, n), C3 (c3f)
void* const kRng = reinterpret_cast<void*>(0x00822d9c);

// rep stosd fills before clearBuffers, in order: (destination, dword count, value)
struct Fill { unsigned dst; unsigned n; unsigned v; };
const Fill kFillsA[] = {
    {0x005a8c38, 0x14, 0},          // 0x004315f1
    {0x00543ca0, 0x14, 0},          // 0x004315fd
    {0x00578804, 0x271, 0},         // 0x00431609
    {0x005794b8, 0x2700, 0},        // 0x00431615
    {0x00575ab0, 0xa28, 0},         // 0x00431621
    {0x00585850, 0x4c0, 0},         // 0x0043162d
    {0x0058bcb8, 0x400, 0},         // 0x00431639
    {0x004c1578, 0x108, 0},         // 0x00431645
};
const Fill kFillsB[] = {
    {0x005689e8, 0xc8, 0xffffffff},   // 0x00431668
    {0x005419d0, 0x10, 0xffffffff},   // 0x00431677
    {0x0053caf0, 0x4e2, 0},           // 0x00431685
    {0x0056988c, 0x271, 0},           // 0x00431691
    {0x005849e0, 0x39c, 0},           // 0x0043169d
    {0x00567b34, 0x98, 0xffffffff},   // 0x004316ac
    {0x005830b8, 0x271, 0xffffffff},  // 0x004316bb
    {0x0056a524, 0xa, 0},             // 0x004316c9
    {0x0059ae80, 0x72, 0xffffffff},   // 0x004316d8
};
static void fill(const Fill& f) {
    unsigned* p = reinterpret_cast<unsigned*>(f.dst);
    for (unsigned i = 0; i < f.n; ++i) p[i] = f.v;
}

// The class of golfer type r from its two flag bytes at 0x004d60a8 + 0x230 * r (lea eax,[ecx*8]; sub eax,ecx;
// lea eax,[eax+eax*4]; shl eax,4 at 0x00431762..0x0043176e).
static int typeClass(unsigned r) {
    const unsigned char* t = reinterpret_cast<const unsigned char*>(0x004d60a8 + 0x230 * r);
    if (t[1] & 0x80) return 6;                     // 0x00431771 test [eax+0x4d60a9], 0x80 / 0x0043177a mov esi, 6
    int c = 1;                                     // 0x00431787
    if (t[0] & 0x11) c = 3;                        // 0x0043178c / 0x00431790
    if (t[0] & 0x0c) c |= 4;                       // 0x00431795 / 0x00431799
    if (c == 1) c = ((r & 1) ? 2 : 4) | 1;         // 0x0043179c..0x004317b0: odd r -> 3, even r -> 5
    return c;
}
// the class each arrival group needs: jump table 0x00431ba8 = 0x004317cc (5), 0x004317d7 (3), 0x004317e2 (6),
// 0x004317ed (7), indexed by count / 3 (0x004317b2..0x004317c5, magic 0x55555556)
const int kGroupClass[4] = {5, 3, 6, 7};

// the forty `likes` words (dword pointer table 0x004c2fd4, read at 0x00431897, 0x00431901, 0x00431967)
const char* like(int i) { return G<const char*>(0x004c2fd4 + 4 * i); }
// the ten colours: jump table 0x00431bb8 sends 0..9 to 0x004c77ec, 0x004c77e4, 0x004c77dc, 0x004c77d4, 0x004c77cc,
// 0x004c77c0, 0x004c77b8, 0x004c77b0, 0x004c77a8, 0x004c77a0 (0x00431a55..0x00431a94)
const unsigned kColour[10] = {0x004c77ec, 0x004c77e4, 0x004c77dc, 0x004c77d4, 0x004c77cc,
                              0x004c77c0, 0x004c77b8, 0x004c77b0, 0x004c77a8, 0x004c77a0};
}  // namespace

typedef int(__cdecl* BuildGolferBioText_t)();
BuildGolferBioText_t BuildGolferBioText_orig;

int __cdecl BuildGolferBioText_re() {
    for (unsigned i = 0; i < sizeof(kFillsA) / sizeof(kFillsA[0]); ++i) fill(kFillsA[i]);
    for (unsigned a = 0x0058bcb8; static_cast<int>(a) < 0x0058ccb8; a += 0x10)   // 0x0043164c..0x00431659 (jl)
        G<unsigned short>(a) = 0xffff;
    for (unsigned i = 0; i < sizeof(kFillsB) / sizeof(kFillsB[0]); ++i) fill(kFillsB[i]);
    G<unsigned short>(0x004c2c9c) = 0;                                           // 0x004316e6
    fill(Fill{0x0053ea24, 0x271, 0});                                            // 0x004316ec
    reinterpret_cast<V0_t>(0x0045b880)();                                        // 0x004316ee clearBuffers
    fill(Fill{0x00568600, 0xfa, 0});                                             // 0x004316ff
    for (unsigned a = 0x0057955a; static_cast<int>(a) < 0x00582d5a; a += 0x100)  // 0x00431706..0x00431715 (jl)
        G<unsigned short>(a) = 0xffff;

    // Twelve picks: a type r = range(0x4c) (0x00431725, low word kept: mov cx, ax / and ecx, 0xffff) is taken when its
    // residue mod 3 equals the pick count's (0x00431740 / 0x00431747, cmp 0x00431749), its taken byte
    // 0x005849e2 + 0x2c * r is clear (0x00431755..0x0043175e) and its class is the group's (0x004317c5 jump table,
    // jne back to 0x00431719 otherwise); then the byte is set (0x004317ff) and the count rises (0x004317fe).
    for (int count = 0; count < 12;) {                                           // 0x00431807 / 0x0043180a (jl)
        const unsigned r = static_cast<unsigned>(kRange(kRng, 0, 0x4c)) & 0xffff;
        bool ok = static_cast<int>(r) % 3 == count % 3;
        unsigned char* taken = reinterpret_cast<unsigned char*>(0x005849e2 + 0x2c * r);
        if (*taken != 0) ok = false;
        const int cls = typeClass(r);
        if (cls != kGroupClass[count / 3]) continue;                             // 0x004317cc..0x004317f0
        if (!ok) continue;                                                       // 0x004317f6 / 0x004317f8
        ++count;
        *taken = 1;
    }

    // One bio per golfer-type record: the text at +0x30 of each 0x230 record, 0x004d60b8 .. 0x004e06f8
    // ([esp+0x10] = 0x004d60b8 at 0x00431810; add ebx, 0x230 / cmp ebx, 0x4e06f8 / jl at 0x00431ad7..0x00431af3).
    char* const text = S(0x0051a068);
    for (unsigned rec = 0x004d60b8; static_cast<int>(rec) < 0x004e06f8; rec += 0x230) {
        const int a = kRange(kRng, 0, 0x28) & 0xffff;                            // 0x0043181f, bp
        const int b = (kRange(kRng, 0, 0x27) & 0xffff) + 1;                      // 0x00431834, bx, inc ebx 0x0043184e
        strcpy(text, S(rec - 0x20));                                             // the name at +0x10 (0x00431849..0x0043186a)
        strcat(text, S(0x004c7820));                                             // 0x0043186c..0x00431895
        strcat(text, like(a));                                                   // 0x00431897..0x004318c2
        strcat(text, S(0x004c52b8));                                             // 0x004318c4..0x004318f0
        strcat(text, like((a + b) % 0x28));                                      // 0x004318ea..0x00431929 (idiv 0x28)
        strcat(text, S(0x004c7818));                                             // 0x0043192b..0x00431958
        strcat(text, like((a + 2 * b) % 0x28));                                  // 0x00431951..0x0043198f
        strcat(text, S(0x004c4944));                                             // 0x00431991..0x004319ba
        if ((kRange(kRng, 0, 2) & 0xffff) != 0) {                                // push 2 at 0x004318ff, call 0x004319c1, je 0x004319cd
            // the first 57 records ((0x004ddd68 - 0x004d60b8) / 0x230) take 0x004c7810, the rest 0x004c7808
            // (cmp ebx, 0x4ddd68 / jl at 0x004319d3 / 0x004319de)
            strcat(text, S((static_cast<int>(rec) < 0x004ddd68) ? 0x004c7810 : 0x004c7808));
            strcat(text, S(0x004c77f4));                                         // 0x00431a0f..0x00431a38
            const unsigned c = static_cast<unsigned>(kRange(kRng, 0, 10)) & 0xffff;   // push 0xa 0x004319f0, call 0x00431a3f
            if (c <= 9) strcat(text, S(kColour[c]));                             // cmp eax, 9 / ja 0x00431ac1 at 0x00431a49
        }
        strcpy(S(rec), text);                                                    // 0x00431ac1..0x00431af1
    }

    G<unsigned char>(0x005419dd) = 0;                                            // 0x00431afc
    G<unsigned char>(0x005419ed) = 1;                                            // 0x00431b03
    G<unsigned char>(0x005419fd) = 3;                                            // 0x00431b0a
    G<unsigned char>(0x00541a0d) = 2;                                            // 0x00431b11
    for (unsigned a = 0x005736b8; static_cast<int>(a) < 0x00575ab8; a += 0x24)   // 0x00431b1e..0x00431b2c
        G<unsigned>(a) = 0xffffffff;
    for (unsigned a = 0x0059fc60; static_cast<int>(a) < 0x005a34e0; a += 0x388)  // 0x00431b33..0x00431b42
        G<unsigned short>(a) = 0xffff;
    for (unsigned a = 0x00572cc2; static_cast<int>(a) < 0x005736c2; a += 0x14)   // 0x00431b49..0x00431b54
        G<unsigned char>(a) = 0xff;
    G<int>(0x005685f0) = 1;                                                      // 0x00431b58
    G<int>(0x00567b04) = 0;                                                      // 0x00431b62
    G<int>(0x00572cac) = 0;                                                      // 0x00431b67
    G<int>(0x0059b730) = 0;                                                      // 0x00431b6c
    G<int>(0x004c284c) = 0;                                                      // 0x00431b71
    G<int>(0x0059aaf8) = 0;                                                      // 0x00431b76
    G<int>(0x0053a450) = 0;                                                      // 0x00431b7b
    G<int>(0x005787cc) = 0;                                                      // 0x00431b80
    G<int>(0x00561258) = 0;                                                      // 0x00431b85
    G<int>(0x0056a51c) = 0;                                                      // 0x00431b8a
    G<int>(0x0059ae7c) = 0;                                                      // 0x00431b8f
    G<int>(0x004c2e28) = -1;                                                     // 0x00431b97
    G<int>(0x005a59f8) = -1;                                                     // 0x00431b9c
    return reinterpret_cast<C0_t>(0x00401750)();                                 // 0x00431ba1 initRecords; eax is its result
}
// 0x004315e0
SG_HOOK("golf_clean.exe", 0x004315e0, buildGolferBioText, BuildGolferBioText_re, BuildGolferBioText_orig);

// ====================================================================================================================
// 0x0040f190  customizeCharacter
// ====================================================================================================================
// __cdecl customizeCharacter(withPro), `ret` at 0x0040f5bb. Sets up the player's pair of golfers at the start of a
// course: two golfers g1 / g2 come from updateMembership 0x00421bc0, become each other's partners, and g2 is the player
// character (flag 0x200, the course's start position). With a non-zero argument, g1 becomes the golf pro chosen in
// [0x004c2e14], whose `.pro` file loadThemeFile 0x00437fa0 reads; when that load succeeds the pro's record is written
// into the golfer-type record 76 at 0x004e06c8. Golfer records are 0x100 bytes from 0x005794b8 (the index is shifted
// left by 8: shl ebx, 8 / shl ebp, 8 at 0x0040f26f / 0x0040f272).
namespace {
typedef int(__cdecl* PlaySound5_t)(int, int, int, int, int);
typedef int(__cdecl* C1_t)(int);
typedef int(__cdecl* C3s_t)(const char*, int, int);
typedef int(__cdecl* Clamp_t)(int, int, int);
template <typename T> static inline T& Rec(int g, unsigned off) { return *reinterpret_cast<T*>(g * 0x100 + off); }
}  // namespace

typedef int(__cdecl* CustomizeCharacter_t)(int withPro);
CustomizeCharacter_t CustomizeCharacter_orig;

int __cdecl CustomizeCharacter_re(int withPro) {
    reinterpret_cast<PlaySound5_t>(0x004481b0)(0x2f, 100, 0, 0, 0);              // pushes 0x0040f199..0x0040f19e, call 0x0040f1a0

    unsigned short mask = G<unsigned short>(0x004c2c9c);                         // 0x0040f1a5
    for (int i = 0; i < 12; ++i)                                                 // 0x0040f1b0..0x0040f1c8
        if (G<unsigned char>(0x005a5a04 + i) != 0) mask |= static_cast<unsigned short>(1 << i);
    G<unsigned short>(0x004c2c9c) = mask;                                        // 0x0040f1cd
    if (mask == 0) reinterpret_cast<V0_t>(0x004065c0)();                         // 0x0040f1ca / 0x0040f1d3, call 0x0040f1d5 showCharacterIntro

    const int pro = G<int>(0x004c2e14);                                          // 0x0040f1da, kept at [esp+0x14]
    const int cycle = G<int>(0x0059ae7c);                                        // 0x0040f1e3
    if (cycle & 2) G<int>(0x0059ae7c) = (cycle + 2) % 0x98;                      // 0x0040f1e8..0x0040f1f7 (signed idiv)

    // every golfer whose byte +0x21 is 1 with +0x22 clear, or 0xff, is reset (0x0040f1fd..0x0040f22b)
    for (int g = 0; 0x005794d9 + 0x100 * g < 0x00582cd9; ++g) {
        const unsigned char st = Rec<unsigned char>(g, 0x005794d9);
        bool reset = false;
        if (st == 1 && Rec<unsigned char>(g, 0x005794da) == 0) reset = true;    // 0x0040f206 / 0x0040f208, 0x0040f20d / 0x0040f20f
        else if (st == 0xff) reset = true;                                       // 0x0040f211 / 0x0040f213
        if (reset) reinterpret_cast<C1_t>(0x00426670)(g);                        // 0x0040f216 resetGolfer
    }

    const C1_t updateMembership = reinterpret_cast<C1_t>(0x00421bc0);
    int g1 = updateMembership(0);                                                // 0x0040f22e
    if (g1 & 1) g1 = updateMembership(0);                                        // 0x0040f238 test bl, 1 / 0x0040f242
    if (g1 != -1) G<int>(0x005a5a24) = g1;                                       // 0x0040f250 / 0x0040f255
    const int g2 = updateMembership(0);                                          // 0x0040f25c
    G<int>(0x005a5a24) = g2;                                                     // 0x0040f263
    Rec<unsigned short>(g1, 0x0057955a) = static_cast<unsigned short>(g2);       // 0x0040f275 partner words
    Rec<unsigned short>(g2, 0x0057955a) = static_cast<unsigned short>(g1);       // 0x0040f27f
    Rec<unsigned short>(g1, 0x00579556) = 0;                                     // 0x0040f28a
    Rec<unsigned short>(g2, 0x00579556) = 6;                                     // 0x0040f297

    if (withPro != 0) {                                                          // 0x0040f291 / 0x0040f2a0
        Rec<unsigned char>(g1, 0x005794d0) = 0x20;                               // 0x0040f2aa
        Rec<unsigned char>(g1, 0x00579573) = static_cast<unsigned char>(pro);    // 0x0040f2b1
        const unsigned rec = static_cast<unsigned>(pro) * 0x38;                  // 0x0040f2b9..0x0040f2c7 (stored over the argument slot)
        char* const text = S(0x0051a068);
        strcpy(text, S(0x0058dd50 + rec));                                       // 0x0040f2d3..0x0040f2fd
        Rec<unsigned short>(g1, 0x0057956e) = 0x4c;                              // 0x0040f2e1
        strcat(text, S(0x004c5948));                                             // 0x0040f2ff..0x0040f328
        // loadThemeFile(text, 0x4c, -1) (pushes 0x0040f2b7 / 0x0040f2c0 / 0x0040f2ce, call 0x0040f32a)
        if (reinterpret_cast<C3s_t>(0x00437fa0)(text, 0x4c, -1) == 0) {           // 0x0040f332 / 0x0040f334
            G<char>(0x004e06e9) = 9;                                             // 0x0040f348
            memset(P(0x0055b028), 0, 0x138 * 4 + 2);                             // 0x0040f34f rep stosd, 0x0040f35d stosw
            G<unsigned char>(0x004e06ec) = G<unsigned char>(0x0058dd73 + rec);   // 0x0040f351 / 0x0040f365
            G<unsigned char>(0x004e06ed) = G<unsigned char>(0x0058dd74 + rec);   // 0x0040f35f / 0x0040f36b
            const unsigned char b0 = G<unsigned char>(0x0058dd70 + rec);         // 0x0040f370
            G<unsigned char>(0x004e06ef) = G<unsigned char>(0x0058dd71 + rec);   // 0x0040f357 / 0x0040f378
            G<unsigned char>(0x004e06eb) =                                       // 0x0040f384..0x0040f38e
                static_cast<unsigned char>(((b0 & 3) << 4) | G<unsigned char>(0x0058dd72 + rec));
            if (b0 & 4) G<char>(0x004e06e9) = static_cast<char>(0x89);           // 0x0040f38c / 0x0040f396
            const unsigned c = static_cast<unsigned>(kRange(kRng, 0, 5)) & 0xffff;   // 0x0040f3a4
            G<unsigned char>(0x004e06e8) = static_cast<unsigned char>(1u << (c & 0x1f));   // mov dl, 1 / shl dl, cl 0x0040f3ba
            strcpy(S(0x004e06d8), S(0x0058dd50 + rec));                          // 0x0040f3b4..0x0040f3e4
            strcpy(S(0x004e06c8), S(0x004c593c));                                // 0x0040f3e6..0x0040f407
        }
        Rec<short>(g1, 0x00579570) = static_cast<short>(G<signed char>(0x004e06e9));   // movsx 0x0040f409 / 0x0040f411
        Rec<unsigned>(g1, 0x005794c8) &= 0xffffffe3u;                            // 0x0040f418..0x0040f423
        Rec<unsigned char>(g1, 0x00579572) = static_cast<unsigned char>(         // clamp(level, 0, 2) 0x0040f432
            reinterpret_cast<Clamp_t>(0x00467130)(G<int>(0x00822c88), 0, 2));
        Rec<unsigned char>(g1, 0x005794d1) =                                     // 0x0040f437..0x0040f44c
            static_cast<unsigned char>(G<unsigned char>(0x0059b730) * 16 + 7);
        Rec<unsigned short>(g1, 0x005794ce) = 0xe0;                              // 0x0040f452
        for (int i = 0; i < 12; ++i) {                                           // 0x0040f45d..0x0040f485
            const unsigned char v = G<unsigned char>(0x0058dd75 + rec + i);
            Rec<unsigned char>(g1, 0x005795a8 + i) = v;
            if (v != 0) Rec<unsigned short>(g1, 0x005794ce) |= static_cast<unsigned short>(1 << i);
        }
        G<int>(0x004c2e14) = -1;                                                 // 0x0040f48b
    }

    const unsigned flags = G<unsigned>(0x0059e7b8);                              // 0x0040f49d
    Rec<unsigned char>(g2, 0x005794c8) &= 0xe3;                                  // 0x0040f497..0x0040f4a8 (and al, 0xe3)
    Rec<unsigned char>(g2, 0x005794d0) = 0x24;                                   // 0x0040f4ae
    Rec<unsigned short>(g2, 0x0057956e) = 0;                                     // 0x0040f4bd
    Rec<short>(g2, 0x00579570) = static_cast<short>(G<signed char>(0x004d60a9)); // 0x0040f4b5 / 0x0040f4c4
    const int date = G<int>(0x00834170);                                         // 0x0040f4cb
    Rec<unsigned char>(g2, 0x005794d9) = 1;                                      // 0x0040f4d0
    Rec<unsigned char>(g1, 0x005794d9) = 1;                                      // 0x0040f4d7
    Rec<int>(g2, 0x00579578) = date;                                             // 0x0040f4de
    Rec<int>(g1, 0x00579578) = date;                                             // 0x0040f4e4
    Rec<unsigned short>(g2, 0x00579568) = 0xffff;                                // 0x0040f4ed
    Rec<unsigned short>(g1, 0x00579568) = 0xffff;                                // 0x0040f4f4
    Rec<unsigned short>(g2, 0x00579566) = 0;                                     // 0x0040f4fb
    Rec<unsigned short>(g1, 0x00579566) = 0;                                     // 0x0040f502
    G<unsigned>(0x0059e7b8) = flags & 0xffff9fffu;                               // and ch, 0x9f 0x0040f4a5, store 0x0040f50f
    Rec<unsigned>(g1, 0x005794c8) &= 0xffefffffu;                                // 0x0040f509..0x0040f522
    Rec<unsigned>(g2, 0x005794c8) &= 0xffefffe3u;                                // 0x0040f528..0x0040f533
    Rec<unsigned char>(g2, 0x00579572) = 2;                                      // 0x0040f539
    Rec<unsigned char>(g2, 0x005794d1) = 0x47;                                   // 0x0040f540
    Rec<unsigned short>(g2, 0x005794ce) = G<unsigned short>(0x004c2c9c);         // 0x0040f51b / 0x0040f547
    for (int i = 0; i < 12; ++i)                                                 // 0x0040f550..0x0040f573 (movsx of the word)
        if (static_cast<int>(Rec<short>(g2, 0x005794ce)) & (1 << i))
            Rec<unsigned char>(g2, 0x005795a8 + i) = G<unsigned char>(0x005a5a04 + i);
    const unsigned f2 = Rec<unsigned>(g2, 0x005794c8) | 0x200;                   // 0x0040f575 / 0x0040f581 (or ah, 2)
    G<int>(0x005a59f8) = g2;                                                     // 0x0040f584
    Rec<unsigned>(g2, 0x005794c8) = f2;                                          // 0x0040f590
    const unsigned char start = G<unsigned char>(0x00585866);                    // 0x0040f596
    Rec<int>(g2, 0x005794b8) = G<int>(0x00585850);                               // 0x0040f57b / 0x0040f59b
    Rec<int>(g2, 0x005794bc) = G<int>(0x00585854);                               // 0x0040f58a / 0x0040f5a1
    Rec<unsigned short>(g2, 0x00579556) = 0;                                     // 0x0040f5a7
    Rec<unsigned char>(g2, 0x005794d2) = start;                                  // 0x0040f5af
    return static_cast<int>((f2 & 0xffffff00u) | start);                         // eax at the ret: the flags with al = start
}
// 0x0040f190
SG_HOOK("golf_clean.exe", 0x0040f190, customizeCharacter, CustomizeCharacter_re, CustomizeCharacter_orig);

// ====================================================================================================================
// 0x0046c970  loadGolfPro
// ====================================================================================================================
// __cdecl, no arguments, `ret` at 0x0046d030. Builds a tournament field: backs up the golfer-type records and their
// stat block (0x0046c985..0x0046c9ac), resets every golfer's status byte and partner word, then for each of the 18
// hole records (0x208 bytes from 0x00575cb8) whose first byte is set, fills two golfers g = 2 * hole + 0 / 1: golfer 1
// is the player; every other golfer gets a pro drawn from the 100 pro records (0x38 bytes from 0x0058dd50) by a
// rejection loop, whose `.pro` file loadThemeFile 0x00437fa0 loads into golfer type t (t = g, golfer 0 using type 1).
// Frame (after the four pushes): [esp+0x10] tries / type, [esp+0x14] g / pro offset, [esp+0x18] inner, [esp+0x1c]
// hole pointer, [esp+0x20] hole number, [esp+0x24] 2 * hole index, [esp+0x30] the 100 taken bytes.
typedef int(__cdecl* LoadGolfPro_t)();
LoadGolfPro_t LoadGolfPro_orig;

int __cdecl LoadGolfPro_re() {
    unsigned char taken[100];
    memset(taken, 0, sizeof(taken));                                             // 0x0046c981..0x0046c98a
    memcpy(P(0x0058f338), P(0x004d6088), 0x2df0 * 4);                            // 0x0046c996
    memcpy(P(0x00520a28), P(0x00543d10), 0x668a * 4);                            // 0x0046c9ac
    for (unsigned a = 0x0057955a; static_cast<int>(a) < 0x00582d5a; a += 0x100) {   // 0x0046c9ae..0x0046c9c4
        G<unsigned char>(a - 0x81) = 0;
        G<unsigned short>(a) = 0xffff;
    }
    G<unsigned char>(0x00572cac) &= 0x7f;                                        // 0x0046c9c6..0x0046c9dc (and al, 0x7f)
    G<int>(0x0056a51c) = 100;                                                    // 0x0046c9d2
    char* const text = S(0x0051a068);
    int hole = 1;                                                                // 0x0046c9cb / 0x0046c9e1
    unsigned hp = 0x00575cb9;                                                    // 0x0046c9e5
    for (int outer = 0; outer < 0x24; outer += 2, ++hole, hp += 0x208) {         // 0x0046cfdc..0x0046cffd
        if (G<unsigned char>(hp - 1) == 0) continue;                             // 0x0046c9f9..0x0046c9fe
        for (int inner = 0; inner < 2; ++inner) {                                // 0x0046cfc6..0x0046cfd6
            const int g = outer + inner;                                         // 0x0046ca1b
            memset(&Rec<char>(g, 0x005794b8), 0, 0x100);                         // 0x0046ca34
            Rec<int>(g, 0x005794b8) = (static_cast<int>(G<short>(0x0058bcba)) << 10) + 0x600;   // 0x0046ca36..0x0046ca4c
            Rec<int>(g, 0x005794bc) = (static_cast<int>(G<short>(0x0058bcbc)) << 10) + 0x600;   // 0x0046ca3d..0x0046ca5d
            Rec<unsigned char>(g, 0x005794d9) = static_cast<unsigned char>(hole);   // 0x0046ca63 (bl)
            Rec<unsigned char>(g, 0x005794d2) = G<unsigned char>(hp);             // 0x0046ca55 / 0x0046ca6e
            Rec<short>(g, 0x00579570) = static_cast<short>(kRange(kRng, 0, 0x7fff));   // 0x0046ca74 / 0x0046ca82
            const int base = hole - G<int>(0x005685f0);                          // 0x0046ca8d
            Rec<unsigned char>(g, 0x00579572) = 2;                               // 0x0046ca93
            Rec<unsigned char>(g, 0x005794d1) = 7;                               // 0x0046ca9d
            Rec<short>(g, 0x0057956e) = static_cast<short>((g != 1) ? 0x4c : 0);   // 0x0046ca7f..0x0046caa7
            Rec<short>(g, 0x00579556) = static_cast<short>((inner + 2 * base) << 2);   // 0x0046caa4..0x0046cab1
            Rec<unsigned short>(g, 0x00579568) = 0xffff;                         // 0x0046cac2
            {
                const int r = kRange(kRng, 0, 0x80) & 0xffff;                    // 0x0046cacb
                const int type = Rec<short>(g, 0x0057956e);                      // 0x0046cad0
                Rec<short>(g, 0x00579570) = static_cast<short>(                  // 0x0046cade..0x0046caff
                    static_cast<int>(G<signed char>(0x004d60a9 + 0x230 * type)) + (r << 8));
            }
            Rec<int>(g, 0x00579578) = G<int>(0x00834170);                        // 0x0046cafa / 0x0046cb12
            Rec<unsigned char>(g, 0x005794d5) = 0xb;                             // 0x0046cb06
            Rec<short>(g, 0x0057955c) = static_cast<short>(kRange(kRng, 0, 4) + 3);   // push 4 0x0046cadc, 0x0046cb18..0x0046cb20
            const int seq = G<int>(0x0056a51c);                                  // 0x0046cb27
            Rec<short>(g, 0x00579576) = static_cast<short>(seq);                 // 0x0046cb2c
            G<int>(0x0056a51c) = seq - 1;                                        // 0x0046cb33 / 0x0046cb34
            if (G<int>(0x00822c88) == 0) Rec<short>(g, 0x0057955c) = 5;          // 0x0046cb39..0x0046cb42
            if (G<int>(0x00543cc4) != 0)                                         // 0x0046cb4b..0x0046cb57
                Rec<short>(g, 0x0057955c) = static_cast<short>(G<int>(0x00543cc4) + 4);
            Rec<short>(g, 0x0057955a) = static_cast<short>(g ^ 1);               // 0x0046cb5e / 0x0046cb69

            // The pro draw (0x0046cb70 .. 0x0046cc90): r = range(100); its skill word +0x36 gives
            // v = (skill - 10)^2 / ((level * 5 + 20) * 2), raised by -(level * v) / 6 for golfer 0 (0x0046cbb7..
            // 0x0046cbca, magic 0xd5555555) and replaced by range(v) when flag 0x4000000 is set (0x0046cbcc..0x0046cbee).
            int tries = 0;                                                       // 0x0046cb61
            int r, v;
            for (;;) {
                r = kRange(kRng, 0, 0x64) & 0xffff;                              // 0x0046cb77
                int level = G<int>(0x00822c88);                                  // 0x0046cb7f
                const int skill = G<short>(0x0058dd86 + 0x38 * r);               // 0x0046cb9d
                v = ((skill - 10) * (skill - 10)) / ((level * 5 + 0x14) * 2);    // 0x0046cb8b..0x0046cbab (cdq; idiv)
                if (g == 0) {                                                    // 0x0046cbb3 / 0x0046cbb5
                    const int hi = static_cast<int>((static_cast<long long>(static_cast<int>(0xd5555555)) *
                                                     static_cast<long long>(level * v)) >> 32);
                    v += hi + static_cast<int>(static_cast<unsigned>(hi) >> 31);
                }
                if (G<unsigned>(0x0059e7b8) & 0x4000000) {                       // 0x0046cbcc / 0x0046cbd6
                    v = kRange(kRng, 0, v) & 0xffff;                             // 0x0046cbde / 0x0046cbe9
                    level = G<int>(0x00822c88);                                  // 0x0046cbe3
                }
                if (G<unsigned char>(0x0058dd70 + 0x38 * r) == 0xff) continue;   // 0x0046cbf0 / 0x0046cbf7
                if (r == 0) continue;                                            // 0x0046cbfd / 0x0046cbff
                ++tries;                                                         // 0x0046cc09
                if (tries < 0x320 && taken[r] != 0) continue;                    // 0x0046cc0a..0x0046cc1b
                const int cash = G<int>(0x00571fd4) / 200;                       // 0x0046cc21..0x0046cc36 (magic 0x51eb851f, sar 6)
                const int rep = G<int>(0x00567b04) / 10;                         // 0x0046cc41..0x0046cc51 (magic 0x66666667, sar 2)
                if (v > tries / (4 - level) + rep + cash) continue;              // 0x0046cc53..0x0046cc6f (jg)
                const int slack = (g != 0) ? tries : tries / 4;                    // 0x0046cc75..0x0046cc87 (signed / 4)
                if (v < rep - slack + cash) continue;                            // 0x0046cc8a..0x0046cc90 (jl)
                break;
            }
            if (taken[r] != 0) r = 0;                                            // 0x0046cc96..0x0046cc9e
            else taken[r] = 1;                                                   // 0x0046cca2

            Rec<unsigned char>(g, 0x005794d0) = (g != 1) ? 0x20 : 0x24;            // 0x0046cca7..0x0046ccbb
            if (g != 1) Rec<unsigned char>(g, 0x00579573) = static_cast<unsigned char>(r);   // 0x0046ccc1 / 0x0046ccc3
            Rec<unsigned>(g, 0x005794c8) &= 0xffffffe3u;                         // 0x0046ccc9..0x0046ccd5
            Rec<unsigned char>(g, 0x00579572) = static_cast<unsigned char>(      // 0x0046ccdb..0x0046ccfa
                (g != 1) ? reinterpret_cast<Clamp_t>(0x00467130)(G<int>(0x00822c88), 0, 2) : 2);
            if (g == 1) {                                                        // 0x0046cd00 je 0x0046cf3f: the player
                unsigned short mask = 0;                                         // 0x0046cf3f
                G<unsigned char>(0x005795d1) = 0x47;                             // 0x0046cf42
                G<unsigned short>(0x004c2c9c) = 0;                               // 0x0046cf49
                for (int i = 0; i < 12; ++i) {                                   // 0x0046cf52..0x0046cf70
                    const unsigned char b = G<unsigned char>(0x005a5a04 + i);
                    G<unsigned char>(0x005796a8 + i) = b;
                    if (b != 0) mask |= static_cast<unsigned short>(1 << i);
                }
                const unsigned f = G<unsigned>(0x0059e7b8);                      // 0x0046cf78
                G<unsigned short>(0x004c2c9c) = mask;                            // 0x0046cf80
                G<unsigned short>(0x005795ce) = mask;                            // 0x0046cf8c
                G<unsigned>(0x005795c8) |= 0x200;                                // 0x0046cf72 / 0x0046cf7d / 0x0046cf93
                if ((f & 0x4000000) == 0) {                                      // 0x0046cf87 / 0x0046cf99
                    G<int>(0x005795b8) = G<int>(0x00585850);                     // 0x0046cfac
                    G<int>(0x005795bc) = G<int>(0x00585854);                     // 0x0046cfb2
                    G<unsigned char>(0x005795d2) = G<unsigned char>(0x00585866); // 0x0046cfb8
                }
                G<unsigned short>(0x00579656) = 0;                               // 0x0046cfbd
                continue;
            }
            const int t = (g != 0) ? g : 1;                                        // 0x0046cd06..0x0046cd0e
            const unsigned rec = static_cast<unsigned>(r) * 0x38;                // 0x0046cd16..0x0046cd27
            strcpy(text, S(0x0058dd50 + rec));                                   // 0x0046cd2b..0x0046cd4e
            strcat(text, S(0x004c5948));                                         // 0x0046cd50..0x0046cd7e
            // loadThemeFile(text, t, -1) (pushes 0x0046cd22 / 0x0046cd7d / 0x0046cd80, call 0x0046cd85)
            if (reinterpret_cast<C3s_t>(0x00437fa0)(text, t, -1) == 0) {         // 0x0046cd8d / 0x0046cd8f
                memset(P(0x00543d10 + 0x4e2 * t), 0, 0x4e2);                     // 0x0046cd9e..0x0046cdd2 (rep stosd + stosw)
                const unsigned T = 0x230 * static_cast<unsigned>(t);             // 0x0046cda1..0x0046cdb7
                const unsigned char b0 = G<unsigned char>(0x0058dd70 + rec);     // 0x0046cde0
                G<unsigned char>(0x004d60ad + T) = G<unsigned char>(0x0058dd74 + rec);   // 0x0046cdd4 / 0x0046cdda
                G<unsigned char>(0x004d60ac + T) = G<unsigned char>(0x0058dd73 + rec);   // 0x0046cdcc / 0x0046cde6
                G<unsigned char>(0x004d60af + T) = G<unsigned char>(0x0058dd71 + rec);   // 0x0046cdc4 / 0x0046cdf1
                G<unsigned char>(0x004d60ab + T) =                               // 0x0046cdee..0x0046ce02
                    static_cast<unsigned char>(G<unsigned char>(0x0058dd72 + rec) | ((b0 & 3) << 4));
                G<char>(0x004d60a9 + T) = 9;                                     // 0x0046ce08
                if (b0 & 4) G<char>(0x004d60a9 + T) = static_cast<char>(0x89);   // 0x0046ce0f / 0x0046ce13
                const unsigned c = static_cast<unsigned>(kRange(kRng, 0, 5)) & 0xffff;   // 0x0046ce21
                G<unsigned char>(0x004d60a8 + T) = static_cast<unsigned char>(1u << (c & 0x1f));   // mov al, 1 / shl al, cl
                const int r4 = kRange(kRng, 0, 0x80) & 0xffff;                   // push 0x80 0x0046ce31, call 0x0046ce43
                Rec<short>(g, 0x00579570) = static_cast<short>(                  // 0x0046ce48..0x0046ce63
                    (r4 << 8) + static_cast<int>(G<signed char>(0x004d60a9 + T)));
                strcpy(S(0x004d6098 + T), S(0x0058dd50 + rec));                  // 0x0046ce55..0x0046ce90
                strcpy(S(0x004d6088 + T), S(0x004c593c));                        // 0x0046ce92..0x0046ceb0
                Rec<int>(g, 0x005795a8) = G<int>(0x0058dd75 + rec);              // 0x0046ceb6 / 0x0046cec2
                Rec<int>(g, 0x005795ac) = G<int>(0x0058dd79 + rec);              // 0x0046cec8 / 0x0046cece
                Rec<int>(g, 0x005795b0) = G<int>(0x0058dd7d + rec);              // 0x0046cecb / 0x0046ced8
            } else {
                Rec<short>(g, 0x00579570) =                                      // 0x0046cee0..0x0046cefb
                    static_cast<short>(G<signed char>(0x004d60a9 + 0x230 * static_cast<unsigned>(t)));
            }
            Rec<short>(g, 0x0057956e) = static_cast<short>(t);                   // 0x0046cf02
            Rec<unsigned char>(g, 0x005794d1) = 7;                               // 0x0046cf09
            Rec<unsigned short>(g, 0x005794ce) = 0xe0;                           // 0x0046cf10
            for (int i = 0; i < 12; ++i)                                         // 0x0046cf1b..0x0046cf38
                if (Rec<unsigned char>(g, 0x005795a8 + i) != 0)
                    Rec<unsigned short>(g, 0x005794ce) |= static_cast<unsigned short>(1 << i);
        }
    }
    const unsigned f = G<unsigned>(0x0059e7b8) | 0x200000;                       // 0x0046d003 / 0x0046d009
    G<int>(0x0056a51c) = 100;                                                    // 0x0046d010
    G<int>(0x005a47e0) = -1;                                                     // 0x0046d01a
    G<unsigned>(0x0059e7b8) = f;                                                 // 0x0046d024
    return static_cast<int>(f);                                                  // eax at the ret
}
// 0x0046c970
SG_HOOK("golf_clean.exe", 0x0046c970, loadGolfPro, LoadGolfPro_re, LoadGolfPro_orig);

// ====================================================================================================================
// 0x0046d200  buildTournamentPrep
// ====================================================================================================================
// __cdecl, no arguments, `ret` at 0x0046d6dd. Before a tournament: every open hole whose par byte disagrees with its
// length class (3 under 300, 4 under 500, else 5) gets a "change hole" line and the new par; the text goes to showPopup
// 0x0046d6e0, whose answer is a bit mask of accepted items (or 0xfffff, all, when flag 0x4000000 skips the popup); a
// declined par change (bit 3 + i clear) is raised by one again; with bit 0 set, every open hole gets up to two
// decorations placed by a random walk from the hole's marker (stampCourseTiles 0x0040e000).
// Frame: [esp+0x10] hole number, [esp+0x14] change count / walk direction, [esp+0x18] step, [esp+0x1c] pass,
// [esp+0x20] walk y, [esp+0x24] hole pointer, [esp+0x28] the changed hole numbers (words).
namespace {
typedef char*(__cdecl* Itoa_t)(int, char*, int);
typedef int(__cdecl* Popup5_t)(int, int, int, int, int);
typedef int(__cdecl* C2_t)(int, int);
typedef int(__cdecl* C4i_t)(int, int, int, int);
const Itoa_t kItoa = reinterpret_cast<Itoa_t>(0x004ad425);
char* const kNum = reinterpret_cast<char*>(0x00838eac);   // the itoa buffer (pushes 0x0046d30b, 0x0046d346, 0x0046d3ef)
}  // namespace

typedef int(__cdecl* BuildTournamentPrep_t)();
BuildTournamentPrep_t BuildTournamentPrep_orig;

int __cdecl BuildTournamentPrep_re() {
    char* const text = S(0x0051a068);
    strcpy(text, S(0x004e3d54));                                                 // 0x0046d207..0x0046d241
    strcat(text, S(0x004e3d20));                                                 // 0x0046d243..0x0046d26c
    strcat(text, S(0x004e3cf4));                                                 // 0x0046d26e..0x0046d297
    strcat(text, S(0x004e3cc0));                                                 // 0x0046d299..0x0046d2c2
    short changed[24];
    int count = 0;                                                               // [esp+0x14] = 0 at 0x0046d211
    int hole = 1;                                                                // [esp+0x10] = 1 at 0x0046d21f
    for (unsigned rec = 0x00575cb8; static_cast<int>(rec) < 0x00578148; rec += 0x208, ++hole) {   // 0x0046d470..0x0046d485
        const signed char par = G<signed char>(rec);                             // 0x0046d2c4
        if (par == 0) continue;                                                  // 0x0046d2c7 / 0x0046d2c9
        const short len = G<short>(rec + 4);                                     // 0x0046d2cf
        int want = 3;                                                            // 0x0046d2d3
        if (len >= 0x12c) want = 4;                                              // 0x0046d2d8 / 0x0046d2dc (signed)
        if (len >= 0x1f4) want = 5;                                              // 0x0046d2e3 / 0x0046d2e7
        if (want == par) continue;                                               // 0x0046d2ee..0x0046d2f3
        strcat(text, S(0x004e3cb0));                                             // 0x0046d2f9..0x0046d330
        strcat(text, kItoa(hole, kNum, 10));                                     // 0x0046d332 .. 0x0046d366
        strcat(text, S(0x004e3ca0));                                             // 0x0046d368..0x0046d396
        strcat(text, kItoa(par, kNum, 10));                                      // movsx 0x0046d38c, 0x0046d398 .. 0x0046d3c5
        strcat(text, S(0x004e3c94));                                             // 0x0046d3c7..0x0046d3f8
        strcat(text, kItoa(want, kNum, 10));                                     // 0x0046d3fa .. 0x0046d42a
        G<signed char>(rec) = static_cast<signed char>(want);                    // 0x0046d43a
        strcat(text, S(0x004c5fd8));                                             // 0x0046d42c..0x0046d461
        changed[static_cast<short>(count)] = static_cast<short>(hole);           // movsx ecx, ax / 0x0046d467
        ++count;                                                                 // 0x0046d466 / 0x0046d46c
    }

    unsigned mask;
    if (G<unsigned>(0x0059e7b8) & 0x4000000) mask = 0xfffff;                     // 0x0046d48b..0x0046d497
    else mask = static_cast<unsigned>(                                           // pushes 0x0046d49e..0x0046d4a9, call 0x0046d4ae
        reinterpret_cast<Popup5_t>(0x0046d6e0)(0x190, 0x64, 0xfffff, 0, 0));
    G<unsigned>(0x005a47e0) = mask;                                              // 0x0046d4bf
    const int n = static_cast<short>(count);                                     // movsx esi, word [esp+0x14] 0x0046d4b8
    int eax = 0;                                                                 // 0x0046d4bd
    if (n > 0) {                                                                 // 0x0046d4c5 / 0x0046d4c7 (jle)
        for (int i = 0; i < n; ++i) {                                            // 0x0046d4f3..0x0046d4f9
            if ((mask & (1u << ((i + 3) & 0x1f))) == 0)                          // 0x0046d4cd..0x0046d4d9
                ++G<unsigned char>(0x00575ab0 + 0x208 * changed[i]);             // 0x0046d4db..0x0046d4e5
        }
        eax = n;
    }

    hole = 1;                                                                    // 0x0046d500
    for (unsigned hp = 0x00575cd0; static_cast<int>(hp) < 0x00578160; hp += 0x208, ++hole) {   // 0x0046d6b7..0x0046d6d0
        eax = (eax & ~0xff) | G<unsigned char>(hp - 0x18);                        // mov al, [esi - 0x18] 0x0046d50c
        if (G<unsigned char>(hp - 0x18) == 0) continue;                          // 0x0046d50f / 0x0046d511
        if ((mask & 1) == 0) continue;                                           // 0x0046d517 / 0x0046d51a (bl: the mask)
        for (int pass = 0; pass < 2; ++pass) {                                   // 0x0046d520..0x0046d6ab
            if (pass != 0 && hole < G<int>(0x005685f0) - 2) continue;            // 0x0046d526..0x0046d539
            int dir = G<signed char>(hp - 0x17);                                 // 0x0046d53f
            int y = G<int>(hp + 4);                                              // 0x0046d543
            int x = G<int>(hp);                                                  // 0x0046d546
            if (pass != 0)                                                       // 0x0046d548 / 0x0046d552
                dir = ((((kRange(kRng, 0, 2) & 0xffff) != 0) ? 2 : -2) + dir) & 7;   // 0x0046d55b..0x0046d56d
            for (int step = 0; step < 7; ++step) {                               // 0x0046d68b..0x0046d699 (cmp old step, 6; jl)
                if ((kRange(kRng, 0, 2) & 0xffff) != 0) x += G<int>(0x004c2878 + 4 * dir);   // 0x0046d581..0x0046d58f
                if ((kRange(kRng, 0, 2) & 0xffff) != 0) y += G<int>(0x004c2898 + 4 * dir);   // 0x0046d59d..0x0046d5ab
                if (step <= 1) continue;                                         // 0x0046d5b6 / 0x0046d5b9 (jle)
                const int idx = x * 50 + y;                                      // 0x0046d5bf..0x0046d5c6
                const signed char type = G<signed char>(0x005722e8 + idx);       // 0x0046d5c9
                const unsigned char cls = G<unsigned char>(0x00578376 + 0x30 * type);   // 0x0046d5cf..0x0046d5d8 (movsx)
                if (cls != 4 && cls != 0xd) continue;                            // 0x0046d5de..0x0046d5e4
                if (type == 0x16) continue;                                      // 0x0046d5ea / 0x0046d5ed
                if (reinterpret_cast<C2_t>(0x0040bf60)(x, y) != 0) continue;     // 0x0046d5f5 tileBlocked / 0x0046d5ff
                if (G<short>(0x0053caf0 + 2 * idx) != 0) continue;               // 0x0046d605 / 0x0046d60d
                // stampCourseTiles(x, y, 0x11 or 0x12 on the second pass, 0) (pushes 0x0046d613..0x0046d61e)
                const unsigned obj = static_cast<unsigned>(
                    reinterpret_cast<C4i_t>(0x0040e000)(x, y, (pass != 0) ? 0x12 : 0x11, 0)) << 4;
                G<unsigned char>(0x0058bcbf + obj) |= 0x40;                      // 0x0046d62c..0x0046d635
                const int dx = G<int>(hp) - x;                                   // 0x0046d63b / 0x0046d640
                const int dy = G<int>(hp + 4) - y;                               // 0x0046d63d / 0x0046d642
                const int ady = (dy < 0) ? -dy : dy;                             // cdq / xor / sub 0x0046d644..0x0046d64d
                const int adx = (dx < 0) ? -dx : dx;                             // 0x0046d64f..0x0046d652
                if (adx > ady)                                                   // 0x0046d654 / 0x0046d656 (jle)
                    G<unsigned char>(0x0058bcbe + obj) = (dx <= 0) ? 3 : 1;      // 0x0046d658..0x0046d664
                else
                    G<unsigned char>(0x0058bcbe + obj) = (dy <= 0) ? 0 : 2;      // 0x0046d66c..0x0046d675
                break;                                                           // [esp+0x18] = 0x63 at 0x0046d683
            }
        }
        eax = 2;                                                                 // the pass counter leaves eax = 2 (0x0046d6a3..0x0046d6ab)
        mask = G<unsigned>(0x005a47e0);                                          // 0x0046d6b1: ebx reloaded after a walked hole
    }
    return eax;
}
// 0x0046d200
SG_HOOK("golf_clean.exe", 0x0046d200, buildTournamentPrep, BuildTournamentPrep_re, BuildTournamentPrep_orig);

// ====================================================================================================================
// 0x0040a4e0  demolishObject
// ====================================================================================================================
// __cdecl demolishObject(x, y), `ret` at 0x0040a5fd / 0x0040a6f0 / 0x0040a809 / 0x0040a8e4 / 0x0040a903 /
// 0x0040a920 / 0x0040a99c. The demolish tool on tile (x, y): grids are 50 x 50 with index x * 50 + y
// (lea / lea / shl at 0x0040a4fe..0x0040a507): the flag words 0x0053caf0, the structure bytes 0x005830b8 (0xff none,
// bit 0x80 a building, else a hole part), the tile types 0x005722e8 and the refund bytes 0x0059c090. The value left
// in eax differs by exit; it is reproduced (mainLoop, the recorded caller, is C2).
namespace {
typedef int(__cdecl* C1u_t)(unsigned);
typedef int(__cdecl* Pop4_t)(int, int, int, int);
typedef int(__cdecl* C3i_t)(int, int, int);

// "Do you really want to demolish your <name>?" (0x0040a546..0x0040a5de and 0x0040a82d..0x0040a8c5), the popup and,
// on a zero answer, clearObjectFootprint 0x0040e400(obj). Returns what the original leaves in eax.
static int confirmDemolish(int obj) {
    char* const text = S(0x0051a068);
    strcpy(text, S(0x004c5188));
    const int type = G<short>(0x0058bcb8 + (static_cast<unsigned>(obj) << 4));   // movsx 0x0040a570 / 0x0040a852
    strcat(text, S(0x004c26b0 + 20 * type));                                    // lea [edx*4 + 0x4c26b0], edx = 5 * type
    strcat(text, S(0x004c516c));
    // showPopup(0x190, 0x64, 1, 1, 0) (pushes 0x0040a57f..0x0040a5ab, call 0x0040a5e0)
    const int r = reinterpret_cast<Popup5_t>(0x0046d6e0)(0x190, 0x64, 1, 1, 0);
    if (r != 0) return r;                                                        // 0x0040a5e8 / 0x0040a5ea
    return reinterpret_cast<C1_t>(0x0040e400)(obj);                              // 0x0040a5f1
}
}  // namespace

typedef int(__cdecl* DemolishObject_t)(int x, int y);
DemolishObject_t DemolishObject_orig;

int __cdecl DemolishObject_re(int x, int y) {
    const int blocked = reinterpret_cast<C2_t>(0x0040bf60)(x, y);                // 0x0040a4ee tileBlocked
    if (blocked != 0) return blocked;                                            // 0x0040a4f6 / 0x0040a4f8
    const int idx = x * 50 + y;                                                  // 0x0040a4fe..0x0040a507 (ebx = x * 50)
    unsigned short& w = G<unsigned short>(0x0053caf0 + 2 * idx);
    G<unsigned char>(0x0053caf1 + 2 * idx) &= 0xfe;                              // 0x0040a50a
    unsigned eaxHi = 0;                                                          // the upper half of eax at the word reads below
    const C2_t objectAt = reinterpret_cast<C2_t>(0x0040df80);
    const C3i_t placeHabitat = reinterpret_cast<C3i_t>(0x00405e30);

    if (w & 0x8000) goto sound;                                                  // 0x0040a51a / 0x0040a51d
    if (w & 0x400) {                                                             // 0x0040a523..0x0040a52b
        const int obj = objectAt(x, y);                                          // 0x0040a533
        if (obj != -1) return confirmDemolish(obj);                              // 0x0040a53d / 0x0040a540
        eaxHi = 0xffff0000u;                                                     // eax = -1 when it reaches 0x0040a602
    }
    {
        const unsigned char s = G<unsigned char>(0x005830b8 + idx);              // 0x0040a602, kept in [esp+0x18]
        if (s == 0xff) goto none;                                                // 0x0040a609 / 0x0040a60f
        if (s & 0x80) {                                                          // 0x0040a615 / 0x0040a617: a building
            if ((s & 0x7f) == 0x10) placeHabitat(x, y, 4);                       // 0x0040a61d..0x0040a627
            const unsigned char b = G<unsigned char>(0x005830b8 + idx) & 0x7f;   // 0x0040a62f..0x0040a63b (re-read)
            if (b == 0x13) w &= 0xffdf;                                          // 0x0040a63d..0x0040a641
            if (b == 1) G<unsigned char>(0x0053caf1 + 2 * idx) &= 0xfd;          // 0x0040a649..0x0040a64d
            else if (b == 0) w &= 0xffdf;                                        // 0x0040a65a..0x0040a65e
            else if (w & 0x1000) w &= 0xefff;                                    // 0x0040a66b..0x0040a683
            else w &= 0xeddf;                                                    // 0x0040a690 / 0x0040a695
        } else if (static_cast<signed char>(s) < 0x7d) {                         // 0x0040a6a2 / 0x0040a6a4 (jge)
            unsigned short dx = w;                                               // 0x0040a6aa
            const int hole = dx & 0x1f;                                          // 0x0040a6b8 / 0x0040a6ba
            const signed char t = G<signed char>(0x005722e8 + idx);
            if (G<unsigned>(0x0059e7b8) & 0x200000) {                            // 0x0040a6bd / 0x0040a6c3
                if (t == 0) goto sound;                                          // 0x0040a6cc / 0x0040a6ce
                if (t == 1 && (dx & 0x80)) goto sound;                           // 0x0040a6d0..0x0040a6d8
            }
            if (t == 0) {                                                        // 0x0040a6f1..0x0040a6fc
                if (hole != 0) {                                                 // 0x0040a735 / 0x0040a737
                    w = static_cast<unsigned short>(dx & 0xffe0);                // 0x0040a73b / 0x0040a743
                    G<int>(0x00575ab8 + 0x208 * hole) = 0;                       // 0x0040a74e
                    G<unsigned char>(0x00575ab0 + 0x208 * hole) = 0;             // 0x0040a758
                    if (hole < G<int>(0x005685f0)) G<int>(0x005685f0) = hole;    // 0x0040a75f..0x0040a767
                }
            } else if (t == 1) {                                                 // 0x0040a6fe / 0x0040a6ff
                if (hole != 0) {                                                 // 0x0040a701 / 0x0040a703
                    w = static_cast<unsigned short>(dx & 0xffe0);                // 0x0040a707 / 0x0040a70f
                    G<unsigned char>(0x00575ab0 + 0x208 * hole) = 0;             // 0x0040a71a
                    G<int>(0x00575ac8 + 0x208 * hole) = 0;                       // 0x0040a721
                    if (hole < G<int>(0x005685f0)) G<int>(0x005685f0) = hole;    // 0x0040a72b..0x0040a767
                }
            }
            placeHabitat(x, y, static_cast<signed char>(s));                     // 0x0040a76c..0x0040a774
        }
        // the refund (0x0040a77c..0x0040a7e0)
        const int refund = G<signed char>(0x0059c090 + idx);                     // 0x0040a77c
        G<int>(0x00571fd4) += refund;                                            // 0x0040a784..0x0040a790
        reinterpret_cast<Pop4_t>(0x0040c890)(refund, (x << 10) + 0x200, (y << 10) + 0x200, -1);   // 0x0040a7ad pointsPopup
        const int yearSlot = static_cast<int>(G<short>(0x005a6d3c)) % 100;       // 0x0040a7b2..0x0040a7c2 (signed idiv)
        G<unsigned char>(0x005830b8 + idx) = 0xff;                               // 0x0040a7c4
        G<short>(0x00584216 + 20 * yearSlot) += static_cast<short>(G<signed char>(0x0059c090 + idx));   // 0x0040a7cc..0x0040a7d8
        goto rebuild;
    }

none:                                                                            // 0x0040a7e5: no structure on the tile
    if (w & 0x200) {                                                             // 0x0040a7ed..0x0040a7f8
        w &= 0xfdff;                                                             // 0x0040a7fa / 0x0040a7fd
        return static_cast<int>(eaxHi | w);
    }
    eaxHi = 0;                                                                   // and eax, 0x400 at 0x0040a80a
    if (w & 0x400) {                                                             // 0x0040a80a..0x0040a812
        const int obj = objectAt(x, y);                                          // 0x0040a81a
        if (obj != -1) return confirmDemolish(obj);                              // 0x0040a824 / 0x0040a827
        eaxHi = 0xffff0000u;
    }
    if (w & 0x20) {                                                              // 0x0040a8e9..0x0040a8f3
        w &= 0xffdf;                                                             // 0x0040a8f5 / 0x0040a8f7
        return static_cast<int>(eaxHi | w);
    }
    if (w & 0x1000) {                                                            // 0x0040a904..0x0040a90f
        w &= 0xefff;                                                             // 0x0040a911 / 0x0040a914
        return static_cast<int>(eaxHi | w);
    }
    {
        const signed char t = G<signed char>(0x005722e8 + idx);                  // 0x0040a921
        const int hole = w & 0x1f;                                               // 0x0040a92f / 0x0040a95b
        if (t == 0) {                                                            // 0x0040a92b / 0x0040a92d
            G<unsigned char>(0x00575ab0 + 0x208 * hole) = 0;                     // 0x0040a93c
            G<int>(0x00575ab8 + 0x208 * hole) = 0;                               // 0x0040a942
            if (hole < G<int>(0x005685f0)) G<int>(0x005685f0) = hole;            // 0x0040a948..0x0040a97d
        } else if (t == 1 && (w & 0x80)) {                                       // 0x0040a952..0x0040a959
            G<unsigned char>(0x00575ab0 + 0x208 * hole) = 0;                     // 0x0040a968
            G<int>(0x00575ac8 + 0x208 * hole) = 0;                               // 0x0040a96f
            if (hole < G<int>(0x005685f0)) G<int>(0x005685f0) = hole;            // 0x0040a975..0x0040a97d
        }
    }

rebuild:                                                                         // 0x0040a982
    G<int>(0x004c2e04) = x;
    G<int>(0x005a47e4) = y;                                                      // 0x0040a988
    reinterpret_cast<V0_t>(0x0042f7a0)();                                        // 0x0040a98e rebuildHeightfield
    return reinterpret_cast<C0_t>(0x0042f2c0)();                                 // 0x0040a993 markTileKind11; eax is its result

sound:                                                                           // 0x0040a6da
    return reinterpret_cast<PlaySound5_t>(0x004481b0)(0x18, 100, 0, 0, 0);       // pushes 0x0040a6da..0x0040a6e2
}
// 0x0040a4e0
SG_HOOK("golf_clean.exe", 0x0040a4e0, demolishObject, DemolishObject_re, DemolishObject_orig);

// ====================================================================================================================
// 0x00432720  OptionsMenuBuild
// ====================================================================================================================
// __cdecl OptionsMenuBuild(x, y), `ret` at every exit. The click handler of the menu bar: nearestMenuSpot 0x004326a0
// (x, y) picks a spot; spots 0..2 switch the speed word 0x00567afc between 5 and the spot's own speed, spot 3 opens the
// Information popup, spot 5 the System popup, the others return a command code. Jump tables: spots 0x00432b24 (0..10),
// the Information answers 0x00432b50 (0..11), the System answers 0x00432b80 (0..7); anything above a table's bound
// (unsigned `ja`) returns -1 through 0x00432774.
namespace {
typedef int(__cdecl* Prompt3_t)(int, int, int);
typedef int(__cdecl* Msg3_t)(int, int, int);
typedef int(__cdecl* Input4_t)(const char*, int, int, int);
typedef int(__cdecl* C1s_t)(const char*);

// the System popup text: 0x004c7cac, the course name 0x004d6098, 0x004c7c48 (0x00432899..0x00432923, rebuilt at
// 0x00432953..0x004329db), then showPopup(0xfa, 0x154, 1, 1, 0)
static int systemPopup() {
    char* const text = S(0x0051a068);
    strcpy(text, S(0x004c7cac));
    strcat(text, S(0x004d6098));
    strcat(text, S(0x004c7c48));
    return reinterpret_cast<Popup5_t>(0x0046d6e0)(0xfa, 0x154, 1, 1, 0);
}
}  // namespace

typedef int(__cdecl* OptionsMenuBuild_t)(int x, int y);
OptionsMenuBuild_t OptionsMenuBuild_orig;

int __cdecl OptionsMenuBuild_re(int x, int y) {
    const unsigned spot = static_cast<unsigned>(reinterpret_cast<C2_t>(0x004326a0)(x, y));   // 0x0043272c
    int& speed = G<int>(0x00567afc);
    switch (spot) {                                                              // 0x00432734 / 0x00432739
    case 0:                                                                      // 0x00432740: 5 when the speed is 0, else 0
        speed = (speed != 0) ? 0 : 5;
        break;
    case 1:                                                                      // 0x0043277a: 5 when the speed is 1, else 1
        speed = (speed == 1) ? 5 : 1;
        break;
    case 2:                                                                      // 0x00432790: 5 when the speed is 2, else 2
        speed = (speed == 2) ? 5 : 2;
        break;
    case 3: {                                                                    // 0x004327ab: the Information popup
        if (G<int>(0x00822d68) == 2) return -0x70;                               // 0x004327b2 / 0x004327b5
        char* const text = S(0x0051a068);
        strcpy(text, S(0x004c7d80));                                             // 0x004327bc..0x004327e9
        strcat(text, S(0x004c7d00));                                             // 0x004327eb..0x00432819
        const unsigned r = static_cast<unsigned>(                                // pushes 0x004327cc..0x004327f9, call 0x0043281b
            reinterpret_cast<Popup5_t>(0x0046d6e0)(0xc8, 0xfa, 1, 1, 0));
        switch (r) {                                                             // 0x00432823 / 0x0043282c
        case 0: return 0x3f;                                                     // 0x00432833
        case 1: return -0x70;                                                    // 0x004327b4
        case 2: return -0x71;                                                    // 0x0043283b
        case 3: return 0x52;                                                     // 0x00432843
        case 4: return -0x72;                                                    // 0x0043284b
        case 5: return -0x76;                                                    // 0x00432853
        case 6: return -0x73;                                                    // 0x0043285b
        case 7: return -0x78;                                                    // 0x00432863
        case 8: return -0x79;                                                    // 0x0043286b
        case 9: return 0x57;                                                     // 0x00432873
        case 10:                                                                 // 0x0043287b
            reinterpret_cast<V0_t>(0x00455a30)();                                // showBestScoresScreen
            return -1;
        case 11:                                                                 // 0x00432886
            reinterpret_cast<V0_t>(0x00473470)();                                // drawScoreSummary
            return -1;
        default: return -1;                                                      // ja 0x00432774
        }
    }
    case 4: return 0x50;                                                         // 0x00432891
    case 5: {                                                                    // 0x00432899: the System popup
        unsigned r = static_cast<unsigned>(systemPopup());                       // 0x00432925
        for (;;) {
            switch (r) {                                                         // 0x0043292d / 0x00432936
            case 0: return 0x53;                                                 // 0x004329f4
            case 1: return 0x4c;                                                 // 0x004329fc
            case 2: return 0x43;                                                 // 0x00432a04
            case 3:                                                              // 0x0043293d: save the course for championship play
                if (reinterpret_cast<Prompt3_t>(0x00437910)(0, 1, 1) != 0) {     // 0x00432943 promptAndSaveFile / 0x0043294d
                    char* const text = S(0x0051a068);
                    strcpy(text, S(0x004d6098));                                 // 0x00432a0c..0x00432a34
                    strcat(text, S(0x004c5f18));                                 // 0x00432a36..0x00432a64
                    reinterpret_cast<Msg3_t>(0x0040cb00)(0x80006000, 0, -1);     // pushes 0x00432a16 / 0x00432a33 / 0x00432a44, call 0x00432a66
                    return -1;
                }
                r = static_cast<unsigned>(systemPopup());                        // 0x004329dd; jbe 0x00432936 at 0x004329e8
                continue;
            case 4: {                                                            // 0x00432a74: rename the course
                G<char>(0x0051a068) = 0;                                         // 0x00432a76
                reinterpret_cast<C1_t>(0x0040daa0)(-1);                          // 0x00432a7d appendCourseTitle
                reinterpret_cast<Input4_t>(0x0045b2c0)(S(0x004c5bf8), 0xc8, 0x14c, 0x20);   // 0x00432a93 textInputBox
                // 0x00432a98..0x00432aa2 (text empty), 0x00432aa9 sanitizeFileName / 0x00432ab3
                if (G<char>(0x0051a068) != 0 && reinterpret_cast<C1s_t>(0x00405ac0)(S(0x0051a068)) != 0) {
                    reinterpret_cast<C1_t>(0x0045b8b0)(0);                       // 0x00432ab7 appendToBuffer45b8b0
                    return -1;
                }
                reinterpret_cast<PlaySound5_t>(0x004481b0)(0x18, 100, 0, 0, 0);  // 0x00432acf
                return -1;
            }
            case 5:                                                              // 0x00432add
                reinterpret_cast<V0_t>(0x00432560)();                            // showPreferencesMenu
                return -1;
            case 6: return 0x6e;                                                 // 0x00432ae8
            case 7:                                                              // 0x00432af0
                G<int>(0x0059b734) = 1;
                return 0x1b;
            default: return -1;                                                  // 0x00432930 ja / 0x004329ee
            }
        }
    }
    case 6: return 0x3f;                                                         // 0x00432833
    case 7: return 0x7a;                                                         // 0x00432b02
    case 8: return 0x78;                                                         // 0x00432b0a
    case 9: return -0x24;                                                        // 0x00432b12
    case 10: return -0x21;                                                       // 0x00432b1a
    default: return -1;                                                          // 0x00432737 ja 0x00432774
    }
    // spots 0..2 continue at 0x00432756
    G<int>(0x004c2848) = -1;
    G<int>(0x0053df54) = 0;                                                      // 0x00432760
    G<int>(0x004c2854) = -1;                                                     // 0x0043276a
    return -1;                                                                   // 0x00432775
}
// 0x00432720
SG_HOOK("golf_clean.exe", 0x00432720, OptionsMenuBuild, OptionsMenuBuild_re, OptionsMenuBuild_orig);

// ====================================================================================================================
// 0x0045f870  showCourseRanking
// ====================================================================================================================
// __cdecl, no arguments, `ret` at 0x0045fd4d. The Top 10 Golf Courses panel: a frame on the live window 0x00519a60
// (fillWindowRect 0x00480b00), the title, the ten course records (0x18 bytes from 0x005422f8: five icon counts +0x00..
// +0x10, score +0x14; record 9 is the player's, rebuilt from five globals) drawn in score order with their icons
// (Image_draw 0x00473e60), then the player's rank sentence. eax is the last drawCenteredText2's.
// Frame: [esp+0x13] rank byte, [esp+0x14] chosen record, [esp+0x18] row, [esp+0x1c] x0, [esp+0x20] ten taken bytes.
namespace {
typedef int(__fastcall* T5i_t)(void* ecx, void* edx, int, int, int, int, int);
typedef int(__fastcall* T4p_t)(void* ecx, void* edx, void*, int, int, int);
typedef int(__cdecl* Text4_t)(const char*, int, int, int);
const T5i_t kFillWindowRect = reinterpret_cast<T5i_t>(0x00480b00);
const T4p_t kSetDrawTarget = reinterpret_cast<T4p_t>(0x004762d0);
const Text4_t kDrawCenteredText2 = reinterpret_cast<Text4_t>(0x00404bc0);
const Text4_t kDrawTextSurface = reinterpret_cast<Text4_t>(0x00404ad0);
const T4p_t kImageDraw = reinterpret_cast<T4p_t>(0x00473e60);
// the five icon images, by count slot (0x0045fa88, 0x0045fab6, 0x0045fae5, 0x0045fb14, 0x0045fb42)
const unsigned kRankIcons[5] = {0x0059b28c, 0x0059b33c, 0x0059b368, 0x0059b394, 0x0059b578};
// the ordinal suffixes by rank - 1: jump table 0x0045fd50 -> 0x0045fc5e..0x0045fc9d
const unsigned kOrdinal[10] = {0x004c3e88, 0x004c3e84, 0x004c3e80, 0x004c3e7c, 0x004c3e78,
                               0x004c3e74, 0x004c3e70, 0x004c3e6c, 0x004c3e68, 0x004c3e60};
}  // namespace

typedef int(__cdecl* ShowCourseRanking_t)();
ShowCourseRanking_t ShowCourseRanking_orig;

int __cdecl ShowCourseRanking_re() {
    const int h = static_cast<int>(G<short>(0x005a6d3c)) * 12 + 0x113;          // 0x0045f873..0x0045f886
    const int x0 = 0x177 - h / 2;                                                // 0x0045f892..0x0045f89f (cdq; sub; sar 1)
    void* const window = P(0x00519a60);
    kFillWindowRect(window, 0, x0 - 1, 0xf, h, 0xe8, static_cast<int>(0x80007fff));    // 0x0045f8b0
    kFillWindowRect(window, 0, x0 + 1, 0x11, h, 0xe8, static_cast<int>(0x80000000));   // 0x0045f8cb
    kFillWindowRect(window, 0, x0, 0x10, h, 0xe8, static_cast<int>(0x80004090));       // 0x0045f8e3
    kSetDrawTarget(G<void*>(0x004c1570), 0, P(0x00519928), 0, 0, 0);             // 0x0045f8f9
    char* const text = S(0x0051a068);
    strcpy(text, S(0x004d3ca4));                                                 // 0x0045f8fe..0x0045f932
    kDrawCenteredText2(text, 0x177, 0x1a, static_cast<int>(0x80007fff));         // 0x0045f934
    int y = 0x32;                                                                // 0x0045f942
    kSetDrawTarget(G<void*>(0x004c1570), 0, P(0x0051b360), 0, 0, 0);             // 0x0045f952

    // the player's record 9 (0x005423d0) from the five live counts, score = their sum (0x0045f957..0x0045f9b0)
    const int a = G<int>(0x005a9cac), b = G<int>(0x005a9cb0), c = G<int>(0x005a9cb4), d = G<int>(0x005a9cb8);
    const int e = G<int>(0x00561258);
    G<int>(0x005423d0) = a;
    G<int>(0x005423d4) = b;
    G<int>(0x005423d8) = c;
    G<int>(0x005423dc) = d;
    G<int>(0x005423e0) = e;
    G<int>(0x005423e4) = e + d + c + b + a;

    unsigned char taken[10];
    memset(taken, 0, sizeof(taken));                                             // 0x0045f96a / 0x0045f973 / 0x0045f97d
    unsigned char rank = 0;   // [esp+0x13]: only written when record 9 is drawn (see the notes: never-drawn reads the frame)
    int pick = 0;             // [esp+0x14]: kept from the previous row when no record qualifies
    for (int row = 0; row < 10; ++row) {                                         // 0x0045fb5a..0x0045fb69
        int best = 0;                                                            // 0x0045f9bc
        for (int k = 0; k < 10; ++k) {                                           // 0x0045f9c5..0x0045f9e3
            if (taken[k] != 0) continue;                                         // 0x0045f9c9 / 0x0045f9cb
            const int score = G<int>(0x0054230c + 0x18 * k);
            if (score < best) continue;                                          // 0x0045f9cf / 0x0045f9d1 (jl): ties go to the later record
            best = score;
            pick = k;
        }
        taken[pick] = 1;                                                         // 0x0045f9ec
        if (pick < 9) {                                                          // 0x0045f9e9 / 0x0045f9f1 (jge)
            strcpy(text, G<const char*>(0x004c2a60 + 4 * pick));                 // 0x0045f9f3..0x0045fa18
        } else {
            text[0] = 0;                                                         // 0x0045fa1e
            reinterpret_cast<C1_t>(0x0040daa0)(0);                               // 0x0045fa25 appendCourseTitle
            rank = static_cast<unsigned char>(row + 1);                          // 0x0045fa2a..0x0045fa37
        }
        const int colour = static_cast<int>((pick != 9) ? 0x80007fff : 0x80007c1f);   // 0x0045fa3b..0x0045fa4b
        kDrawTextSurface(text, x0 + 0x10, y, colour);                            // 0x0045fa59
        int x = x0 + 0x88;                                                       // 0x0045fa62
        const unsigned rec = 0x005422f8 + 0x18 * pick;                           // 0x0045fa6d / 0x0045fa70
        for (int kind = 0; kind < 5; ++kind)                                     // the five loops 0x0045fa73..0x0045fb58
            for (int n = 0; n < G<int>(rec + 4 * kind); ++n) {                   // count re-read each step, jle / jl (signed)
                kImageDraw(P(kRankIcons[kind]), 0, G<void*>(0x004c1570), x, y, 0);
                x += 0xc;
            }
        y += 0xe;                                                                // 0x0045fb5e
    }
    y += 7;                                                                      // 0x0045fb79
    strcpy(text, S(0x004d3c9c));                                                 // 0x0045fb6f..0x0045fbad
    const int years = G<int>(0x00834170) / 0x2000;                               // 0x0045fb82..0x0045fbaa (cdq; and 0x1fff; add; sar 0xd)
    strcat(text, reinterpret_cast<Itoa_t>(0x004ad425)(years, S(0x00824134), 10));   // 0x0045fbb0..0x0045fbed
    strcat(text, S(0x004d3c7c));                                                 // 0x0045fbef..0x0045fc18
    kDrawCenteredText2(text, 0x177, y, static_cast<int>(0x80007c1f));            // 0x0045fc1a
    y += 0xe;                                                                    // 0x0045fc2c
    strcpy(text, S(0x004d3c74));                                                 // 0x0045fc1f..0x0045fc4c
    const unsigned r1 = static_cast<unsigned>(static_cast<int>(static_cast<signed char>(rank)) - 1);   // movsx / dec 0x0045fc4e
    if (r1 <= 9) strcat(text, S(kOrdinal[r1]));                                  // 0x0045fc52 / 0x0045fc55 (ja)
    strcat(text, S(0x004d3c54));                                                 // 0x0045fcca..0x0045fcf8
    strcat(text, S((rank == 1) ? 0x004c4e60 : 0x004c4944));                      // 0x0045fcf5 / 0x0045fcff
    return kDrawCenteredText2(text, 0x177, y, static_cast<int>(0x80007c1f));     // 0x0045fd3e
}
// 0x0045f870
SG_HOOK("golf_clean.exe", 0x0045f870, showCourseRanking, ShowCourseRanking_re, ShowCourseRanking_orig);

// ====================================================================================================================
// 0x00435760  drawEmployeeViewPanel
// ====================================================================================================================
// __cdecl drawEmployeeViewPanel(x, y), `ret` at 0x00435787 / 0x00435ee9 / 0x00435efe. The golfer list panel: while
// [0x00561254] is set it hands over to drawEmployeeDetailPanel 0x00436e50. Otherwise it draws the panel background
// and the hovered button (hitEmployeeSlot 0x00435570), lists every golfer that is on the course, or is the partner of
// one that is, newest first (g = ([0x005a5a24] - i + 0x98) % 0x98), past the scroll offset [0x005aa6c8], in two
// layouts by the text mode [0x004c2844]; records each listed golfer in 0x005a9cf0; draws the scroll bar; and shows a
// tooltip after the same button has been hovered more than ten calls in a row ([0x005a9f54] / [0x005aa554]).
// Frame (after push ebx / ebp): [esp+0x10] x1, [esp+0x14] row, [esp+0x18] c, [esp+0x1c] d, [esp+0x20] b, [esp+0x24] a,
// [esp+0x28] g, [esp+0x2c] list cursor, [esp+0x30] listed count, [esp+0x34] i.
namespace {
typedef int(__fastcall* T7s_t)(void* ecx, void* edx, void*, int, int, int, int, int, int);
typedef int(__fastcall* T5s2_t)(void* ecx, void* edx, void*, void*, int, int, int);
typedef int(__cdecl* MapLine7_t)(int, int, int, int, int, int, int);
const T7s_t kDrawScaled = reinterpret_cast<T7s_t>(0x00473cb0);      // Surface_drawScaled(this = image, surf, x, y, 1, 1, 1, 0)
const T5s2_t kStretchBlit = reinterpret_cast<T5s2_t>(0x00473f60);   // Surface_stretchBlit
const Text4_t kDrawText4049d0 = reinterpret_cast<Text4_t>(0x004049d0);
const Text4_t kDrawCenteredText = reinterpret_cast<Text4_t>(0x00404b70);

// the name colour of golfer g (0x00435974..0x004359b6 and 0x00435c62..0x00435ca4): signed word compares, last wins
static int golferColour(int g) {
    int c = static_cast<int>(0x80000000);
    if (Rec<short>(g, 0x00579562) > 0xa0) c = static_cast<int>(0x80004210);
    if (Rec<short>(g, 0x00579560) > 0x10) c = static_cast<int>(0x80000018);
    if (Rec<short>(g, 0x0057955e) > 0x10) c = static_cast<int>(0x80006000);
    if (Rec<unsigned>(g, 0x005794c8) & 0x20000000) c = static_cast<int>(0x80007d08);
    return c;
}
// the mood face of golfer g: clamp(word +0xa4 + 2, 1, 10) selects one of the 0x2c-byte images from 0x0059b778
static void* moodFace(int g) {
    const int k = reinterpret_cast<Clamp_t>(0x00467130)(static_cast<int>(Rec<short>(g, 0x0057955c)) + 2, 1, 10);
    return P(0x0059b778 + 0x2c * k);
}
}  // namespace

typedef int(__cdecl* DrawEmployeeViewPanel_t)(int x, int y);
DrawEmployeeViewPanel_t DrawEmployeeViewPanel_orig;

int __cdecl DrawEmployeeViewPanel_re(int x, int y) {
    if (G<int>(0x00561254) != 0)                                                 // 0x00435760..0x0043576e
        return reinterpret_cast<C2_t>(0x00436e50)(x, y);                         // 0x0043577a drawEmployeeDetailPanel
    void* const surf = G<void*>(0x004c1570);
    kStretchBlit(P(0x00541ac8), 0, P(0x0058b440), surf, 0xd7, 0x1da, 0);         // 0x004357a4
    const C2_t hitSlot = reinterpret_cast<C2_t>(0x00435570);
    const int hit = hitSlot(x, y);                                               // 0x004357b3
    if (hit == -3) kImageDraw(P(0x0058b1a0), 0, surf, 0xf0, 0x1ee, 0);           // 0x004357bb..0x004357d7
    else if (hit == -4) kImageDraw(P(0x0058b1cc), 0, surf, 0xd9, 0x20a, 0);      // 0x004357d9..0x004357f4
    else if (hit == -5) kImageDraw(P(0x0059b95c), 0, surf, 0x135, 0x24b, 0);     // 0x004357f6..0x00435812
    else if (hit == -6) kImageDraw(P(0x0059b988), 0, surf, 0x2f4, 0x24b, 0);     // 0x00435814..0x00435830
    memset(P(0x005a9cf0), 0, 0x98 * 4);                                          // 0x00435837..0x00435844
    kSetDrawTarget(surf, 0, P(0x00519fd8), 0, 0, 0);                             // 0x00435864
    int x1 = 0x136, row = 0, a = 0x19c, b = 0x146, c = 0x156, d = 0x145;         // 0x00435853..0x00435887
    int ly = 0x1f2;                                                              // 0x0043585b
    unsigned out = 0x005a9cf0;                                                   // 0x0043588f
    int count = 0;
    char* const text = S(0x0051a068);
    for (int i = 0; i < 0x98; ++i) {                                             // 0x00435d32..0x00435d41
        const int g = (G<int>(0x005a5a24) - i + 0x98) % 0x98;                    // 0x00435897..0x004358a9 (cdq; idiv)
        const bool active = Rec<signed char>(g, 0x005794d9) > 0;                 // 0x004358b6..0x004358c0
        bool show = false;
        const unsigned short p = Rec<unsigned short>(g, 0x0057955a);             // 0x004358c5
        if (p != 0xffff) {                                                       // 0x004358cc / 0x004358d0
            const int q = static_cast<short>(p);                                 // movsx 0x004358d2
            if (Rec<short>(q, 0x0057955a) == g && Rec<signed char>(q, 0x005794d9) > 0) show = true;   // 0x004358d8..0x004358eb
        }
        if (!show && !active) continue;                                          // 0x004358ed / 0x004358ef
        ++count;                                                                 // 0x004358fa
        if (count <= G<int>(0x005aa6c8)) continue;                               // 0x004358fb / 0x00435901 (jle)
        if (G<int>(0x004c2844) < 4) {                                            // 0x00435907..0x00435915: compact layout
            const int old = row++;
            if (old == 6) {                                                      // 0x0043591c / 0x00435923: next column
                row = 1;
                x1 += 0x3c; d += 0x3c; c += 0x3c; b += 0x3c; a += 0x3c;
                ly = 0x1f2;
                if (d > 0x2f3) continue;                                         // 0x00435958 / 0x0043596e (jg)
            }
            const int colour = golferColour(g);
            kDrawScaled(moodFace(g), 0, surf, x1, ly, 1, 1, 1, 0);               // 0x004359f4
            text[0] = 0;                                                         // 0x00435a00
            reinterpret_cast<C2_t>(0x004676e0)(g, 0);                            // 0x00435a07 buildGolferName
            kDrawText4049d0(text, d, ly + 1, colour);                            // 0x00435a1b
            G<int>(out) = g;                                                     // 0x00435a2f
            out += 4;
            ly += 0xe;                                                           // 0x00435a38
            continue;
        }
        const int old = row++;                                                   // 0x00435a40 / 0x00435a44: wide layout
        if (old == 4) {                                                          // 0x00435a41 / 0x00435a48
            row = 1;
            x1 += 0x79; d += 0x79; c += 0x79; b += 0x79; a += 0x79;
            ly = 0x1f2;
            if (d > 0x2b7) continue;                                             // 0x00435a7d / 0x00435a93
        }
        kSetDrawTarget(surf, 0, P(0x00519fd8), 0, 0, 0);                         // 0x00435aaa
        if (g & 1) {                                                             // 0x00435aaf / 0x00435ab4 (test byte [esp+0x28], 1)
            kDrawScaled(P(0x0059b778), 0, surf, x1, ly, 1, 1, 1, 0);             // 0x00435ad4
            text[0] = 0;                                                         // 0x00435ae8
            strcat(text, reinterpret_cast<Itoa_t>(0x004ad425)(Rec<signed char>(g, 0x005794d9), S(0x005a9f6c), 10));   // 0x00435aef..0x00435b28
            const signed char st = Rec<signed char>(g, 0x005794d9);              // 0x00435b1c
            if (st == 0x13) strcpy(text, S(0x004c61e4));                         // 0x00435b25 / 0x00435b2a
            if (st <= 0) {                                                       // 0x00435b4f / 0x00435b51 (jg)
                const int q = Rec<short>(g, 0x0057955a);                         // 0x00435b53
                text[0] = 0;                                                     // 0x00435b6c
                strcat(text, reinterpret_cast<Itoa_t>(0x004ad425)(Rec<signed char>(q, 0x005794d9), S(0x005a9f6c), 10));   // 0x00435b73..0x00435ba3
            }
            const int tx = x1 + ((Rec<signed char>(g, 0x005794d9) >= 10) ? 5 : 8);   // 0x00435bae..0x00435bc6 (setge)
            kDrawText4049d0(text, tx, ly + 0xf, static_cast<int>(0x80000000));   // 0x00435bce
            if (Rec<short>(g, 0x00579568) != -1) {                               // 0x00435bd6 / 0x00435bde
                // both rows start at column c ([esp+0x18], read at 0x00435be0 and 0x00435c18)
                for (int k = 0; k < 4; ++k)                                      // 0x00435be9..0x00435c0b (edi = 4, dec / jne)
                    kDrawScaled(P(0x0059ba38), 0, surf, c + 10 * k, ly + 0x11, 1, 1, 1, 0);
                for (int k = 0; k < Rec<short>(g, 0x0057956a); ++k)              // 0x00435c0d..0x00435c46 (signed, re-read)
                    kDrawScaled(P(0x0059b9b4), 0, surf, c + 10 * k, ly + 0x11, 1, 1, 1, 0);
            }
        }
        kSetDrawTarget(surf, 0, P(0x0051b360), 0, 0, 0);                         // 0x00435c5d
        const int colour = golferColour(g);
        text[0] = 0;                                                             // 0x00435cb0
        reinterpret_cast<C2_t>(0x004676e0)(g, 0);                                // 0x00435cb7 buildGolferName
        if (Rec<signed char>(g, 0x005794d9) > 0) {                               // 0x00435cbc..0x00435cc7 (jle)
            kDrawText4049d0(text, b, ly + 5, colour);                            // 0x00435cd8
            kDrawScaled(moodFace(g), 0, surf, a, ly + 2, 1, 1, 1, 0);            // 0x00435d19
        }
        G<int>(out) = g;                                                         // 0x00435d26
        out += 4;
        ly += 0x15;                                                              // 0x00435d2f
    }
    if (count != 0) {                                                            // 0x00435d47 / 0x00435d49: the scroll bar
        const Clamp_t clamp = reinterpret_cast<Clamp_t>(0x00467130);
        const int top = clamp(G<int>(0x005aa6c8) * 400 / count, 0, 0x18f);      // 0x00435d4f..0x00435d68
        const int page = (G<int>(0x004c2844) >= 4) ? 0x10 : 0x30;                // 0x00435d6d..0x00435d8e
        const int bottom = clamp((G<int>(0x005aa6c8) + page) * 400 / count, 1, 0x190);   // 0x00435d91..0x00435da0
        kFillWindowRect(P(0x00519a60), 0, top + 0x15f, 0x24e, bottom - top - 1, 6, static_cast<int>(0x8000739f));   // 0x00435dc4
        if (G<int>(0x005aa6c8) > count) G<int>(0x005aa6c8) -= 0x10;              // 0x00435dc9..0x00435dd5
    }
    kSetDrawTarget(surf, 0, P(0x00519fd8), 0, 0, 0);                             // 0x00435deb
    const int hover = hitSlot(x, y);                                             // 0x00435dfa
    if (hover != G<int>(0x005a9f54)) {                                           // 0x00435e08 / 0x00435e0c
        G<int>(0x005aa554) = 0;                                                  // 0x00435eea
        G<int>(0x005a9f54) = hover;                                              // 0x00435ef4
        return hover;
    }
    const int held = ++G<int>(0x005aa554);                                       // 0x00435e12..0x00435e1c
    if (hover == -1 || held <= 10) return hover;                                 // 0x00435e19..0x00435e2d
    text[0] = 0;                                                                 // 0x00435e36
    if (hover == -4) strcpy(text, S(0x004c82c8));                                // 0x00435e33 / 0x00435e3d / 0x00435e72
    else if (hover == -3) strcpy(text, S(0x004d6098));                           // 0x00435e3f / 0x00435e42 / 0x00435e50
    else if (hover == -2) strcpy(text, S(0x004c5dc8));                           // 0x00435e44 / 0x00435e47 / 0x00435e49
    const int half = 3 * static_cast<int>(strlen(text));                         // 0x00435e97..0x00435eba
    // drawMapLine(x - 3n, y - 5, x + 3n, y - 5, 0x80000000, 10, 5) (pushes 0x00435eab..0x00435ec7, call 0x00435ec8)
    reinterpret_cast<MapLine7_t>(0x004493d0)(x - half, y - 5, x + half, y - 5, static_cast<int>(0x80000000), 10, 5);
    return kDrawCenteredText(text, x, y - 10, static_cast<int>(0x80007fff));     // 0x00435edc
}
// 0x00435760
SG_HOOK("golf_clean.exe", 0x00435760, drawEmployeeViewPanel, DrawEmployeeViewPanel_re, DrawEmployeeViewPanel_orig);

// ====================================================================================================================
// 0x004362f0  drawGolferActionPanel
// ====================================================================================================================
// __cdecl drawGolferActionPanel(x, y), `ret` at 0x0043631e / 0x00436ab7 / 0x00436acd. The golfer action panel: while the
// detail panel is up ([0x00561254], unless the speed word 0x00567afc is 3) it hands over to drawEmployeeDetailPanel
// 0x00436e50. Otherwise it draws the background, the hovered button (hitGolferPanel 0x00435f00; button k's position is
// the word pair at 0x004c7ac0 + 4k, its image 0x00583a80 + 0xb0 * k), then either the player golfer's buttons and the
// five shot buttons (state by the selected shot [0x0058f330], the hover and the lie under the golfer), the match
// scoreboard, or, with no player golfer ([0x005a59f8] = -1), the three start buttons and the bag of clubs; last, the
// hover tooltip after more than ten calls on the same button ([0x005a9f54] / [0x005aa554]).
namespace {
static int btnX(int k) { return G<short>(0x004c7ac0 + 4 * k); }   // movsx word [k*4 + 0x4c7ac0]
static int btnY(int k) { return G<short>(0x004c7ac2 + 4 * k); }   // movsx word [k*4 + 0x4c7ac2]
// a shot button: frame image `image + 0x2c * a` over mask `0x00586f50 + 0x2c * c` (lea / lea / lea [x*4 + base])
static void shotButton(unsigned image, int a, int c, int k) {
    kStretchBlit(P(image + 0x2c * a), 0, P(0x00586f50 + 0x2c * c), G<void*>(0x004c1570), btnX(k), btnY(k), 0);
}
}  // namespace

typedef int(__cdecl* DrawGolferActionPanel_t)(int x, int y);
DrawGolferActionPanel_t DrawGolferActionPanel_orig;

int __cdecl DrawGolferActionPanel_re(int x, int y) {
    if (G<int>(0x00561254) != 0 && G<int>(0x00567afc) != 3)                      // 0x004362f0..0x00436305
        return reinterpret_cast<C2_t>(0x00436e50)(x, y);                         // 0x00436311 drawEmployeeDetailPanel
    void* const surf = G<void*>(0x004c1570);
    kStretchBlit(P(0x00541af4), 0, P(0x0058b46c), surf, 0xd6, 0x1da, 0);         // 0x0043633c
    const int hit = reinterpret_cast<C2_t>(0x00435f00)(x, y);                    // 0x0043634b hitGolferPanel, kept in esi / [esp+0x10]
    if (hit != -1) {                                                             // 0x00436355 / 0x0043635c
        const unsigned image = 0x00583a80 + 0xb0 * static_cast<unsigned>(hit);   // 0x00436381..0x00436391
        if (hit >= 4 && hit <= 8)                                                // 0x0043635e..0x00436366
            kStretchBlit(P(image), 0, P(0x00586f50), surf, btnX(hit), btnY(hit), 0);   // 0x00436397
        else
            kImageDraw(P(image), 0, surf, btnX(hit), btnY(hit), 0);              // 0x004363c8
    }
    if (G<int>(0x005a59f8) != -1) {                                              // 0x004363cd..0x004363d6: a player golfer
        kImageDraw(P(0x00583b88), 0, surf, btnX(1), btnY(1), 0);                 // 0x004363f9
        kImageDraw(P(0x00583c38), 0, surf, btnX(2), btnY(2), 0);                 // 0x0043641b
        kImageDraw(P(0x00583ce8), 0, surf, btnX(3), btnY(3), 0);                 // 0x0043643d
        const int g = G<int>(0x005a59f8);                                        // 0x00436442 / 0x00436447
        if (Rec<int>(g, 0x00579584) != -1) {                                     // 0x0043644a / 0x00436451: in a match
            kImageDraw(P(0x00541b20), 0, surf, 0x129, 0x1ed, 0);                 // 0x004366ec
            reinterpret_cast<C1_t>(0x00461110)(G<int>(0x005a59f8));              // 0x004366f8 drawMatchScoreboard
        } else {
            // the lie: typeAtPos(+0xd4, +0x04) 0x00436465, then the class byte 0x00578372 + 0x30 * type (0x0043646a..0x0043647a)
            const int type = reinterpret_cast<C2_t>(0x0040bfa0)(Rec<int>(g, 0x0057958c), Rec<int>(g, 0x005794bc));
            const bool rough = G<signed char>(0x00578372 + 0x30 * type) <= 0;    // 0x00436485 / 0x00436487 (setle)
            const int sel = G<int>(0x0058f330);                                  // 0x00436480
            int a, c;
            if (sel == 0) { a = 1; c = 1; }                                      // 0x0043648a..0x00436494
            else { c = (hit != 4) ? 1 : 0; a = (hit != 4) ? 3 : 0; }             // 0x00436496..0x004364a7
            shotButton(0x00583d40, a, c, 4);                                     // 0x004364de
            if (!rough) {                                                        // 0x004364e3 / 0x004364e5: the other three greyed
                kStretchBlit(P(0x00583e48), 0, P(0x00586f7c), surf, btnX(5), btnY(5), 0);   // 0x00436509
                kStretchBlit(P(0x00583ef8), 0, P(0x00586f7c), surf, btnX(6), btnY(6), 0);   // 0x00436530
                kStretchBlit(P(0x00583fa8), 0, P(0x00586f7c), surf, btnX(7), btnY(7), 0);   // 0x00436557 / 0x0043666a
            } else {
                // each button's mask uses the hover test of button 4 (`cmp esi, 4; setne cl` at 0x0043656d,
                // 0x004365c8, 0x00436624) and its frame its own button (sub eax, 5 / 6 / 7)
                if (G<int>(0x0058f330) == -1) { a = 1; c = 1; }                  // 0x0043655c..0x00436569
                else { c = (hit != 4) ? 1 : 0; a = (hit != 5) ? 3 : 0; }         // 0x0043656b..0x0043657c
                shotButton(0x00583df0, a, c, 5);                                 // 0x004365b3
                if (G<int>(0x0058f330) == 1) { a = 1; c = 1; }                   // 0x004365b8..0x004365c4
                else { c = (hit != 4) ? 1 : 0; a = (hit != 6) ? 3 : 0; }         // 0x004365c6..0x004365d7
                shotButton(0x00583ea0, a, c, 6);                                 // 0x0043660e
                if (G<int>(0x0058f330) == 3) { a = 1; c = 1; }                   // 0x00436613..0x00436620
                else { c = (hit != 4) ? 1 : 0; a = (hit != 7) ? 3 : 0; }         // 0x00436622..0x00436633
                shotButton(0x00583f50, a, c, 7);                                 // 0x0043666a
            }
            const int s2 = G<int>(0x0058f330);                                   // 0x0043666f
            if (s2 == 0) { c = 1; a = (hit != 8) ? 3 : 0; }                      // 0x00436674..0x00436683
            else {
                c = (hit != 4) ? 1 : 0;                                          // 0x004366c4..0x004366c9
                if (s2 == 4) a = 1;                                              // 0x004366cc / 0x004366d1
                else a = (hit != 8) ? 3 : 0;                                     // 0x004366cf jne 0x0043667a
            }
            shotButton(0x00584000, a, c, 8);                                     // 0x004366ba
        }
    } else {                                                                     // 0x00436705: no player golfer
        if (G<int>(0x005685f0) <= 1)                                             // 0x0043670f / 0x00436711 (jg)
            kImageDraw(P(0x00583b88), 0, surf, btnX(1), btnY(1), 0);             // 0x00436731
        const unsigned flags = G<unsigned>(0x0059e7b8);
        // 0x0043673b..0x0043674f: drawn unless flag 0x2000 is set with more than one hole and a pro chosen
        if (!(flags & 0x2000) || G<int>(0x005685f0) <= 1 || G<int>(0x004c2e14) == -1)
            kImageDraw(P(0x00583c38), 0, surf, btnX(2), btnY(2), 0);             // 0x0043676f
        if ((G<unsigned>(0x0059e7b8) & 0x400000) == 0)                           // 0x00436774 / 0x0043677e
            kImageDraw(P(0x00583ce8), 0, surf, btnX(3), btnY(3), 0);             // 0x0043679e
        kSetDrawTarget(surf, 0, P(0x00519fd8), 0, 0, 0);                         // 0x004367b4
        int col = 0x1ee, ly = 0x21c;                                             // 0x004367be / 0x004367c6
        char* const text = S(0x0051a068);
        for (int i = 0; i < 10; ++i) {                                           // 0x004368e0..0x004368f9 (0x004c2c3c .. 0x004c2c64)
            const signed char n = G<signed char>(0x005a5a04 + i);                // 0x004367db
            if (n == 0) continue;                                                // 0x004367e1 / 0x004367e3
            strcpy(text, G<const char*>(0x004c2c3c + 4 * i));                    // 0x004367e9..0x00436812
            strcat(text, S(0x004c60c8));                                         // 0x00436814..0x0043684a
            strcat(text, reinterpret_cast<Itoa_t>(0x004ad425)(n * 10, S(0x005a9f6c), 10));   // 0x00436844..0x00436885
            strcat(text, S(0x004c60c4));                                         // 0x00436887..0x004368b5
            kDrawText4049d0(text, col, ly, static_cast<int>(0x80007fff));        // 0x004368bc
            ly += 0xc;                                                           // 0x004368c1
            if (ly >= 0x258) { col = 0x281; ly = 0x21c; }                        // 0x004368c7..0x004368d7
        }
    }

    const int last = G<int>(0x005a9f54);                                         // 0x004368ff
    if (hit != last) {                                                           // 0x00436905 / 0x00436907
        G<int>(0x005aa554) = 0;                                                  // 0x00436ab8
        G<int>(0x005a9f54) = hit;                                                // 0x00436ac2
        return last;                                                             // eax still holds the old value
    }
    const int held = ++G<int>(0x005aa554);                                       // 0x0043690d..0x00436917
    if (hit == -1) return last;                                                  // 0x00436914 / 0x0043691d
    if (held <= 10) return held;                                                 // 0x00436923..0x00436928 (eax = the count)
    char* const text = S(0x0051a068);
    text[0] = 0;                                                                 // 0x00436931
    switch (hit - 1) {                                                           // 0x0043692e..0x00436941, table 0x00436ad0
    case 0: strcpy(text, S(0x004c836c)); break;                                  // 0x00436948
    case 1:                                                                      // 0x00436952
        strcpy(text, S(0x004c8364));
        if (G<int>(0x004c2e14) != -1) strcat(text, S(0x0058dd50 + 0x38 * G<int>(0x004c2e14)));   // 0x0043697a..0x004369ad
        else strcat(text, S(0x004c8358));                                        // 0x004369b2..0x004369d3
        break;
    case 2: strcpy(text, S(0x004c8344)); break;                                  // 0x004369d8
    case 3: strcpy(text, S(0x004c8334)); break;                                  // 0x004369fd
    case 4: strcpy(text, S(0x004c8320)); break;                                  // 0x00436a04
    case 5: strcpy(text, S(0x004c830c)); break;                                  // 0x00436a26
    case 6: strcpy(text, S(0x004c82f8)); break;                                  // 0x00436a2d
    case 7: strcpy(text, S(0x004c82e8)); break;                                  // 0x00436a4f
    case 8: strcpy(text, S(0x004c5dc8)); break;                                  // 0x00436a56
    case 9: strcpy(text, S(0x004c82d8)); break;                                  // 0x00436a78
    default: break;                                                              // ja 0x00436a9d
    }
    return reinterpret_cast<C2_t>(0x00432620)(x, y - 5);                         // 0x00436aaa drawPanelBackdrop
}
// 0x004362f0
SG_HOOK("golf_clean.exe", 0x004362f0, drawGolferActionPanel, DrawGolferActionPanel_re, DrawGolferActionPanel_orig);

// ====================================================================================================================
// 0x004a2400  panelCreate4a2400
// ====================================================================================================================
// __thiscall panelCreate4a2400(parent, a1, a2, flags), `ret 0x10` at 0x004a2898. Creates a panel part and its grid of
// edit boxes. `this` (ebx) sits 0x604 bytes into its object (ebp = this - 0x604, 0x004a2430); the object's vbtable
// pointer is at ebp+0 and every field access goes through it: F(off) = [this + vbtable[2] + off] (mov eax, [ebp];
// mov ecx, [eax + 8] at each access), the window base is this + vbtable[1] - 0x604 and the quad / draw-target
// sub-object this + vbtable[1] - 0x390. Three per-box arrays are addressed from `this` directly: the box pointers
// [this - 0x600 + 4k], their texts [this - 0x5d8 + 4k] and buffer sizes [this - 0x5b0 + 4k] (esi + 0x28 / esi + 0x50),
// and so is one geometry word, [this - 0x588] (0x004a2582 / 0x004a25c2). The SEH frame (0x004a2400..0x004a240e, state
// slot [esp+0x40]) only matters if a callee throws; it is not reproduced.
namespace {
typedef int(__fastcall* T0i_t)(void* ecx, void* edx);
typedef int(__fastcall* T1i_t)(void* ecx, void* edx, int);
typedef int(__fastcall* T4i_t)(void* ecx, void* edx, int, int, int, int);
typedef int(__fastcall* T6i_t)(void* ecx, void* edx, int, int, int, int, void*, int);
typedef int(__fastcall* T9i_t)(void* ecx, void* edx, int, int, int, int, int, unsigned, void*, int, int);
typedef void*(__cdecl* New_t)(unsigned);
}  // namespace

typedef int(__fastcall* PanelCreate_t)(char* self, void* edx, void* parent, int a1, int a2, unsigned flags);
PanelCreate_t PanelCreate_orig;

int __fastcall PanelCreate_re(char* self, void*, void* parent, int a1, int a2, unsigned flags) {
    if (parent == 0) return 3;                                                   // 0x004a241f..0x004a2426
    char* const base = self - 0x604;                                             // 0x004a2430
    reinterpret_cast<T0i_t>(0x004a2a60)(base, 0);                                // 0x004a2438 panelLayoutLabeledText
    // the vbtable is re-read at every access (0x004a243d, 0x004a244e, 0x004a2472, ...)
    const auto vb = [base](int i) { return (*reinterpret_cast<int**>(base))[i]; };
    const auto F = [self, vb](int off) -> int& { return *reinterpret_cast<int*>(self + vb(2) + off); };
    F(-0x5dc) = a1;                                                              // 0x004a2447
    F(-0x5d8) = a2;                                                              // 0x004a2458
    if ((*reinterpret_cast<unsigned*>(static_cast<char*>(parent) + 0x9c) & 0x1000) == 0)   // 0x004a245f..0x004a2468
        flags |= 0x100000;                                                       // 0x004a246a
    // Window::create(window, a1, a2, F(-0x5a4), F(-0x5a0), 0, flags, parent, 0, 0) (pushes 0x004a2475..0x004a24a4)
    const int created = reinterpret_cast<T9i_t>(0x004806c0)(self + vb(1) - 0x604, 0, a1, a2, F(-0x5a4), F(-0x5a0), 0,
                                                            flags, parent, 0, 0);
    if (created != 0) return created;                                            // 0x004a24ac / 0x004a24ae
    char* const quad = self + vb(1) - 0x390;                                     // 0x004a24c0
    reinterpret_cast<T4i_t>(0x00476310)(quad, 0, F(-0x584), F(-0x578), F(-0x56c), F(-0x560));   // 0x004a24e3 Widget_setQuad
    reinterpret_cast<T4i_t>(0x00476340)(self + vb(1) - 0x390, 0, F(-0x580), F(-0x574), F(-0x568), F(-0x55c));   // 0x004a2517
    reinterpret_cast<T4i_t>(0x00476370)(self + vb(1) - 0x390, 0, F(-0x57c), F(-0x570), F(-0x564), F(-0x558));   // 0x004a254b
    reinterpret_cast<T4i_t>(0x004762d0)(self + vb(1) - 0x390, 0, F(-0x590), F(-0x58c), F(-0x588), 0);   // 0x004a257a

    // the grid (0x004a257f..0x004a25e0), in EditBox::create's (x, y, w, h) terms (shim/src/re/c3ep.cpp): the outer
    // loop steps x by F(-0x59c), the inner loop steps y by F(-0x598); s = F(-0x5bc)
    const int direct = *reinterpret_cast<int*>(self - 0x588);                    // 0x004a2582 / 0x004a25c2
    const int s = F(-0x5bc);
    const int span = F(-0x598) - s;                                              // 0x004a25a2 ([esp+0x50])
    const int x0 = F(-0x5cc) + 2 * s + direct;                                   // 0x004a25bf..0x004a25c8 ([esp+0x10])
    const int xEnd0 = F(-0x59c) - 2 * s - direct + x0;                           // 0x004a25ad..0x004a25ca ([esp+0x30])
    const int half = s >> 1;                                                     // 0x004a25d8 (sar), [esp+0x4c]
    const int yEnd0 = span + half;                                               // 0x004a25de ([esp+0x34])
    int k = 0;                                                                   // [esp+0x54]
    for (int row = 0; row < F(-0x5ac); ++row) {                                  // 0x004a25ec..0x004a25f2, 0x004a285c..0x004a2878
        const int dx = F(-0x59c) * row;                                          // 0x004a260c / 0x004a2612
        const int x = x0 + dx, xEnd = xEnd0 + dx;                                // [esp+0x18], [esp+0x20]
        int y = half, yEnd = yEnd0;                                              // [esp+0x1c], [esp+0x24]
        for (int col = 0; col < F(-0x5a8); ++col) {                              // 0x004a262e..0x004a263a, 0x004a283d..0x004a284e
            void** const slot = reinterpret_cast<void**>(self - 0x600 + 4 * k);  // 0x004a2644 / 0x004a2836
            if (*slot != 0)                                                      // 0x004a2651 / 0x004a2653: delete the old box
                (*reinterpret_cast<T1i_t**>(*slot))[0](*slot, 0, 1);             // 0x004a2659
            void* box = reinterpret_cast<New_t>(0x004a57f1)(0x5dc);              // 0x004a2660 operator_new
            if (box != 0) box = reinterpret_cast<void*(__fastcall*)(void*, void*)>(0x00486070)(box, 0);   // 0x004a267a EditBox::ctor
            *slot = box;                                                         // 0x004a268d
            if (box == 0) return 4;                                              // 0x004a268f / 0x004a287d
            char* const b = static_cast<char*>(box);
            // EditBox::create(box, x, y, xEnd - x, yEnd - y, window, 1) (pushes 0x004a26bc..0x004a26ce)
            const int r = reinterpret_cast<T6i_t>(0x00486110)(box, 0, x, y, xEnd - x, yEnd - y,
                                                               self + vb(1) - 0x604, 1);
            if (r != 0) return r;                                                // 0x004a26d6 / 0x004a26d8
            if (F(-0x5e0) & 0x1000) *reinterpret_cast<unsigned*>(b + 0x580) |= 2;   // 0x004a26e4..0x004a26f2
            reinterpret_cast<T4i_t>(0x00476310)(b + 0x274, 0, F(-0x584), F(-0x578), F(-0x56c), F(-0x560));   // 0x004a2726
            reinterpret_cast<T4i_t>(0x004762d0)(b + 0x274, 0, F(-0x590), F(-0x58c), F(-0x588), 0);           // 0x004a2754
            reinterpret_cast<T1i_t>(0x00486250)(box, 0, *reinterpret_cast<int*>(self - 0x5b0 + 4 * k));      // 0x004a275f resizeBuffer
            reinterpret_cast<T1i_t>(0x00486200)(box, 0, *reinterpret_cast<int*>(self - 0x5d8 + 4 * k));      // 0x004a276a setText
            *reinterpret_cast<int*>(b + 0x584) = F(-0x594);                      // 0x004a277e
            *reinterpret_cast<int*>(b + 0x590) = F(-0x554);                      // 0x004a2793
            *reinterpret_cast<int*>(b + 0x594) = F(-0x550);                      // 0x004a27a8
            *reinterpret_cast<int*>(b + 0x598) = F(-0x54c);                      // 0x004a27bd
            *reinterpret_cast<int*>(b + 0x59c) = F(-0x5c0);                      // 0x004a27d5
            *reinterpret_cast<int*>(b + 0x5a0) = 0;                              // 0x004a27e5
            *reinterpret_cast<int*>(b + 0x5a4) =                                 // 0x004a27df..0x004a27f6 (repne scasb)
                static_cast<int>(strlen(*reinterpret_cast<const char**>(b + 0x574)));
            (*reinterpret_cast<T0i_t**>(box))[0x120 / 4](box, 0);                // 0x004a27fe
            (*reinterpret_cast<T1i_t**>(box))[1](box, 0, 0);                     // 0x004a280a
            const int pitch = F(-0x598);                                         // 0x004a281a
            y += pitch;                                                          // 0x004a2825
            yEnd += pitch;                                                       // 0x004a282f
            ++k;                                                                 // 0x004a2835
        }
    }
    return 0;                                                                    // 0x004a2884
}
// 0x004a2400
SG_HOOK("golf_clean.exe", 0x004a2400, panelCreate4a2400, PanelCreate_re, PanelCreate_orig);

// ====================================================================================================================
// 0x00481f40  Flic::open
// ====================================================================================================================
// __thiscall Flic::open(name, mode, source, flags), `ret 0x10` at 0x004823b2. With a name it resets the Flic
// (Flic::reset 0x00481ba0), replaces its MappedFile at +0x54 and opens the file (MappedFile::open 0x00492dd0, failure
// returns 6); flag bit 0 then uses (creating when absent) the Cache482fd0 at +0x78, bit 1 frees that cache and appends a
// node pair to the decoder pool at 0x0083ac88 (head +8 = 0x0083ac90, tail 0x0083ac94, count 0x0083ac98, previous count
// 0x0083ac9c, heap 0x0083aca0, current cache 0x0083aca4 = pool +0x1c, which the pool's slot-0 virtual createCache
// 0x0049c830 sets at 0x0049c894), and neither bit takes `source`; the chosen object becomes +0x74 and
// Flic::buildFrameTable 0x00481ca0 runs. Then (also without a name) +0x44 must be set (7 otherwise), Flic::indexFrames
// 0x00481e40 indexes the header's two words and Flic::createStream 0x00482b90 builds the stream. Every failure first
// resets the Flic. The SEH frame (0x00481f40..0x00481f4e, state [esp+0x1c]) only matters if a callee throws.
namespace {
typedef void*(__cdecl* NewHeap_t)(unsigned size, int heap);
typedef void*(__fastcall* T0r_t)(void* ecx, void* edx);
typedef void*(__fastcall* T1r_t)(void* ecx, void* edx, int);
typedef int(__fastcall* T2i_t)(void* ecx, void* edx, int, int);
typedef int(__fastcall* T2s_t)(void* ecx, void* edx, const char*, int);
typedef void(__cdecl* Free1_t)(void*);
static inline int& D(unsigned addr) { return *reinterpret_cast<int*>(addr); }
static inline int& At(int base, int off) { return *reinterpret_cast<int*>(base + off); }
// the pool's virtual base value: eax = [[0x0083ac8c] + 4]; eax = [eax + 0x0083ac90] (0x004820bb..0x004820c4 and the
// three copies at 0x0048213c, 0x004821f2, 0x004822a1)
static int poolBaseValue() { return D(static_cast<unsigned>(D(static_cast<unsigned>(D(0x0083ac8c)) + 4)) + 0x0083ac90); }
static void* poolAlloc(unsigned size) {
    const NewHeap_t setNewHeap = reinterpret_cast<NewHeap_t>(0x004823c0);
    const int heap = D(0x0083aca0);
    return (heap != 0) ? setNewHeap(size, heap) : setNewHeap(size, poolBaseValue());
}
// the pool's slot 0, this = 0x0083ac88, with the new decoder (0x00482194..0x004821a1, 0x004822f5..0x00482302)
static void poolAdd(int d) {
    (*reinterpret_cast<T1i_t**>(0x0083ac88))[0](reinterpret_cast<void*>(0x0083ac88), 0, d);
}
}  // namespace

typedef int(__fastcall* FlicOpen_t)(char* self, void* edx, const char* name, int mode, int source, unsigned flags);
FlicOpen_t FlicOpen_orig;

int __fastcall FlicOpen_re(char* self, void*, const char* name, int mode, int source, unsigned flags) {
    const T0i_t reset = reinterpret_cast<T0i_t>(0x00481ba0);
    if (name != 0) {                                                             // 0x00481f60 / 0x00481f68
        reset(self, 0);                                                          // 0x00481f6e
        void*& file = *reinterpret_cast<void**>(self + 0x54);
        if (file != 0) {                                                         // 0x00481f73..0x00481f78
            (*reinterpret_cast<T1i_t**>(file))[0](file, 0, 1);                   // 0x00481f7e deleting destructor
            file = 0;                                                            // 0x00481f80
        }
        void* m = reinterpret_cast<New_t>(0x004a57f1)(0x14);                     // 0x00481f85
        if (m != 0) m = reinterpret_cast<T0r_t>(0x00492d80)(m, 0);               // 0x00481f9b MappedFile::ctor
        file = m;                                                                // 0x00481fb0
        if (reinterpret_cast<T2s_t>(0x00492dd0)(m, 0, name, 1) == 0) {           // 0x00481fb3 / 0x00481fba
            reset(self, 0);
            return 6;                                                            // 0x00481fc3
        }
        void* src;                                                               // esi at 0x00482336
        if (flags & 1) {                                                         // 0x00481fcd / 0x00481fd3
            if (source != 0) {                                                   // 0x00481fd5 / 0x00481fd9
                reset(self, 0);
                return 3;                                                        // 0x00481fe2
            }
            void*& cache = *reinterpret_cast<void**>(self + 0x78);
            if (cache == 0) {                                                    // 0x00481fec / 0x00481fef
                void* c = reinterpret_cast<New_t>(0x004a57f1)(0x58);             // 0x00481ff3
                if (c != 0) c = reinterpret_cast<T0r_t>(0x00482fd0)(c, 0);       // 0x0048200d Cache482fd0::ctor
                cache = c;                                                       // 0x0048201c
                if (c == 0) {                                                    // 0x0048201f
                    reset(self, 0);
                    return 4;                                                    // 0x00482028
                }
            }
            src = cache;                                                         // 0x00482032
            (*reinterpret_cast<T0i_t**>(src))[0](src, 0);                        // 0x00482039
        } else if (flags & 2) {                                                  // 0x00482040 / 0x00482042
            void*& cache = *reinterpret_cast<void**>(self + 0x78);
            if (cache != 0) {                                                    // 0x0048204b / 0x0048204d
                void* c = cache;
                reinterpret_cast<T0i_t>(0x00483010)(c, 0);                       // 0x00482051 Cache483010::dtor
                reinterpret_cast<Free1_t>(0x004a4ffc)(c);                        // 0x00482057 operatorDelete_4a4ffc
                cache = 0;                                                       // 0x0048205f
            }
            const int count = D(0x0083ac98);                                     // 0x00482062
            D(0x0083aca4) = 0;                                                   // 0x00482067
            if (count == 0) {                                                    // 0x0048206d / 0x00482074: the first pair
                int n;
                if (D(0x0083aca0) != 0) {                                        // 0x0048207a / 0x0048207c
                    n = reinterpret_cast<int>(reinterpret_cast<NewHeap_t>(0x004823c0)(0x1c, D(0x0083aca0)));   // 0x00482081
                    if (n != 0) {                                                // 0x00482089 / 0x0048208b: the constructor, inline
                        At(n, 0) = 0x004ba28c;                                   // 0x0048208d
                        At(n, 0x14) = 0x004ba278;                                // 0x00482093
                        At(n, 0x18) = D(0x00839650);                             // 0x0048209a / 0x004820a0
                        D(0x00839650) = 0;                                       // 0x004820a3
                        At(n, At(At(n, 0), 4)) = 0x004ba288;                     // 0x004820a9..0x004820ae
                    }
                } else {
                    n = reinterpret_cast<int>(reinterpret_cast<NewHeap_t>(0x004823c0)(0x1c, poolBaseValue()));   // 0x004820d1
                    if (n != 0) n = reinterpret_cast<int>(reinterpret_cast<T1r_t>(0x00482ae0)(reinterpret_cast<void*>(n), 0, 1));   // 0x004820ed
                }
                D(0x0083ac90) = n;                                               // 0x004820fc
                if (n != 0) {                                                    // 0x004820fa / 0x00482101
                    At(n, 4) = 0;                                                // 0x00482107
                    At(D(0x0083ac90), 0xc) = D(0x0083ac90);                      // 0x0048210a / 0x0048210f
                    At(D(0x0083ac90), 0x10) = D(0x0083ac90);                     // 0x00482112 / 0x00482117
                    int d = reinterpret_cast<int>(poolAlloc(0x10));              // 0x0048211a..0x00482152
                    if (d != 0) d = reinterpret_cast<int>(reinterpret_cast<T1r_t>(0x00482b20)(reinterpret_cast<void*>(d), 0, 1));   // 0x0048216e
                    At(D(0x0083ac90), 8) = d;                                    // 0x00482177..0x00482181
                    if (At(D(0x0083ac90), 8) != 0) {                             // 0x00482184..0x0048218e
                        poolAdd(At(D(0x0083ac90), 8));                           // 0x004821a1
                        D(0x0083ac94) = D(0x0083ac90);                           // 0x004821a3 / 0x004821a9
                        D(0x0083ac9c) = D(0x0083ac98);                           // 0x00482304 / 0x00482309
                        ++D(0x0083ac98);                                         // 0x0048230e / 0x0048230f
                    }
                }
            } else {                                                             // 0x004821b4: append after the tail
                int n = reinterpret_cast<int>(poolAlloc(0x1c));                  // 0x004821b4..0x00482208
                if (n != 0) n = reinterpret_cast<int>(reinterpret_cast<T1r_t>(0x00482ae0)(reinterpret_cast<void*>(n), 0, 1));   // 0x004821d7 / 0x00482224
                At(At(D(0x0083ac90), 0x10), 0xc) = n;                            // 0x004821e0..0x004821ed / 0x0048222d..0x0048223a
                const int tail = At(D(0x0083ac90), 0x10);                        // 0x0048223d / 0x00482242
                const int nn = At(tail, 0xc);                                    // 0x00482245
                if (nn != 0) {                                                   // 0x00482248 / 0x0048224a
                    At(nn, 0x10) = tail;                                         // 0x00482250
                    At(At(At(D(0x0083ac90), 0x10), 0xc), 0xc) = D(0x0083ac90);   // 0x00482253..0x0048225e
                    At(D(0x0083ac90), 0x10) = At(At(D(0x0083ac90), 0x10), 0xc);  // 0x00482261..0x0048226c
                    const int cur = At(D(0x0083ac90), 0x10);                     // 0x0048226f / 0x00482274
                    D(0x0083ac94) = cur;                                         // 0x00482277
                    At(cur, 4) = 0;                                              // 0x0048227c
                    int d = reinterpret_cast<int>(poolAlloc(0x10));              // 0x0048227f..0x004822b7
                    if (d != 0) d = reinterpret_cast<int>(reinterpret_cast<T1r_t>(0x00482b20)(reinterpret_cast<void*>(d), 0, 1));   // 0x004822d3
                    At(D(0x0083ac94), 8) = d;                                    // 0x004822dc..0x004822e6
                    if (At(D(0x0083ac94), 8) != 0) {                             // 0x004822e9..0x004822f3
                        poolAdd(At(D(0x0083ac94), 8));                           // 0x00482302
                        D(0x0083ac9c) = D(0x0083ac98);                           // 0x00482304 / 0x00482309
                        ++D(0x0083ac98);                                         // 0x0048230e / 0x0048230f
                    }
                }
            }
            // 0x00482314: the current cache's slot 0 (createCache 0x0049c830 set 0x0083aca4; on the jumps here that
            // skip poolAdd it is still 0 and the original faults reading [0] at 0x0048231c)
            src = reinterpret_cast<void*>(D(0x0083aca4));
            (*reinterpret_cast<T0i_t**>(src))[0](src, 0);                        // 0x0048231e
            D(0x0083ac24) = reinterpret_cast<int>(src);                          // 0x00482320
        } else {
            src = reinterpret_cast<void*>(static_cast<intptr_t>(source));        // 0x00482328..0x00482332 ((flags & 3) == 0 here)
        }
        *reinterpret_cast<void**>(self + 0x74) = src;                            // 0x0048233d
        const int built = reinterpret_cast<T1i_t>(0x00481ca0)(self, 0, mode);    // 0x00482340 Flic::buildFrameTable
        if (built != 0) {                                                        // 0x00482347 / 0x00482349
            reset(self, 0);
            return built;
        }
    }
    if (*reinterpret_cast<int*>(self + 0x44) == 0) {                             // 0x0048235a / 0x0048235d
        reset(self, 0);
        return 7;                                                                // 0x00482366
    }
    const char* const hdr = *reinterpret_cast<const char* const*>(self + 0x58);  // 0x0048236d
    // Flic::indexFrames(this, word +0x60, word +0x62): the words are pushed in edx / ecx whose upper halves the
    // body never sets (0x00482370 / 0x00482374); indexFrames masks both to 16 bits (shim/src/re/c3dx.cpp)
    const int indexed = reinterpret_cast<T2i_t>(0x00481e40)(self, 0, *reinterpret_cast<const unsigned short*>(hdr + 0x60),
                                                             *reinterpret_cast<const unsigned short*>(hdr + 0x62));
    if (indexed != 0) {                                                          // 0x00482383 / 0x00482385
        reset(self, 0);
        return indexed;
    }
    reinterpret_cast<T2i_t>(0x00482b90)(self, 0, reinterpret_cast<int>(self), mode & 2);   // 0x00482392..0x00482399 createStream
    return 0;                                                                    // 0x0048239e
}
// 0x00481f40
SG_HOOK("golf_clean.exe", 0x00481f40, Flic_open, FlicOpen_re, FlicOpen_orig);
