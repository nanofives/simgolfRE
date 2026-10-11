// C3 batch c3ed (simulation plan phase 5, 2026-10-11) of golf_clean.exe: terrain editing. editTerrainHeight 0x00409cf0 (one step of
// the terrain-edit queue) and applyTerrainEdit 0x00470a60 (the random course generator). Fixtures: re/frida/js/fixtures.d/c3ed.js.
// Registry: re/frida/registry.d/c3ed.py. Purposes and per-jcc tables: log/c3/c3ed_purpose.md.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and cites
// the instructions it reproduces. __thiscall is emulated with __fastcall (ecx = this, edx unused). Callees run through their
// original addresses.
#include "hooks.h"

namespace {

template <typename T> static inline T& G(unsigned addr) { return *reinterpret_cast<T*>(addr); }
static inline unsigned char& G8(unsigned addr) { return *reinterpret_cast<unsigned char*>(addr); }
static inline int& G32(unsigned addr) { return *reinterpret_cast<int*>(addr); }

typedef int(__cdecl* C0_t)();
typedef int(__cdecl* C1_t)(int a);
typedef int(__cdecl* C2_t)(int a, int b);
typedef int(__cdecl* C3_t)(int a, int b, int c);
typedef int(__cdecl* C4_t)(int a, int b, int c, int d);
typedef int(__cdecl* C5_t)(int a, int b, int c, int d, int e);
typedef int(__fastcall* T1_t)(void* ecx, void* edx, int a);

const C1_t kSign = reinterpret_cast<C1_t>(0x00467150);               // sign(v)
const C3_t kClamp = reinterpret_cast<C3_t>(0x00467130);              // clamp(v, lo, hi)
const C4_t kStampCourseTiles = reinterpret_cast<C4_t>(0x0040e000);   // stampCourseTiles(row, col, type, flag) -> placed-object index
const C1_t kUpdateMembership = reinterpret_cast<C1_t>(0x00421bc0);   // updateMembership(0) -> golfer index
const C0_t kMarkTileKind11 = reinterpret_cast<C0_t>(0x0042f2c0);     // markTileKind11()
const C0_t kRebuildTileCoverage = reinterpret_cast<C0_t>(0x0042f340);  // rebuildTileCoverage()
const C0_t kRebuildHeightfield = reinterpret_cast<C0_t>(0x0042f7a0); // rebuildHeightfield()
const C5_t kPlaySound = reinterpret_cast<C5_t>(0x004481b0);          // playSound(a, b, c, d, e)
const C3_t kHeightBlend = reinterpret_cast<C3_t>(0x0040c170);        // heightBlend(row, col, 0)
const C2_t kSampleHeight = reinterpret_cast<C2_t>(0x0042dba0);       // sampleHeight(x << 7, y << 7)
const C2_t kTileBlocked = reinterpret_cast<C2_t>(0x0040bf60);        // tileBlocked(row, col)
const C4_t kEvalPlacementArea = reinterpret_cast<C4_t>(0x0040db90);  // evalPlacementArea(row, col, w, h)
const C1_t kSpawnWalker = reinterpret_cast<C1_t>(0x00402970);        // spawnWalker(kind) -> walker index
const C2_t kClearTile = reinterpret_cast<C2_t>(0x00470a10);          // clearTile(x, y)
const T1_t kRandomRange = reinterpret_cast<T1_t>(0x0045c1e0);        // Random::range(this = 0x00822d9c, n)
static inline int Rnd(int n) { return kRandomRange(reinterpret_cast<void*>(0x00822d9c), 0, n); }

// the tile tables (50 x 50, cell = col + 50 * row)
static inline unsigned char& TT(int idx) { return G8(0x005722e8 + idx); }                 // tile type
static inline unsigned short& TW(int idx) { return G<unsigned short>(0x0053caf0 + 2 * idx); }   // tile flag word
static inline unsigned char& TWH(int idx) { return G8(0x0053caf1 + 2 * idx); }            // its high byte
static inline unsigned char& TWL(int idx) { return G8(0x0053caf0 + 2 * idx); }            // its low byte
static inline unsigned char& TB(int idx) { return G8(0x0053bbac + idx); }                 // byte copy of the type attribute
static inline unsigned char& LV(int off) { return G8(0x005a4998 + off); }                 // level grid (51 x 51, cell = col + 51 * row)
static inline int Attr0(int t) { return static_cast<signed char>(G8(0x00578375 + 0x30 * t)); }   // type attribute byte 0
static inline int Attr1(int t) { return static_cast<signed char>(G8(0x00578376 + 0x30 * t)); }   // type attribute byte 1
static inline int Dx(int k) { return G32(0x004c2878 + 4 * (k & 7)); }                     // 8-neighbour offsets (first coordinate)
static inline int Dy(int k) { return G32(0x004c2898 + 4 * (k & 7)); }                     // (second coordinate)

}  // namespace

// 0x00409cf0  editTerrainHeight(): cdecl, plain `ret`. One step of the terrain-edit queue. Queue cursor [0x0055e920], queue length
// [0x005a9cd4], entries (col = [0x00586fa8 + 4 i], row = [0x00586b50 + 4 i], mode = [0x005a8834 + 4 i]); tile index col + 50 * row.
// 1. While the camera target row [0x004c2f9c] is not -1 (cmp 0x00409cf8, je 0x00409d5b) and the camera ([0x004c2ba0],
//    [0x004c2ba4]) is not at (target, [0x005685fc]) (0x00409d06..0x00409d18) the camera takes one sign() step in each coordinate
//    (0x00409d1d, 0x00409d3e) and the call returns the second sign (eax at 0x00409d5a).
// 2. Otherwise the cursor is read and incremented (0x00409d5b..0x00409d6f); past the length (jle 0x00409d74 not taken) bit 0x02000000
//    of [0x0059e7b8] is cleared, the target row set to -1 and that flags word returned (0x00409d76..0x00409d95).
// 3. Else the entry is applied: mode & 0x80 xors the flag word with mode & 0xff7f (0x00409dc3..0x00409dd2); mode & 0x100 stamps
//    mode & ~0x100 through stampCourseTiles and, for the types 8 7 0xc 9 0xb 0xd 0xe 4 (0x00409e02..0x00409e2d), flags the tile
//    (0x40 in the high byte, byte 0x00578804 + idx = 0) and plays sound 0x90 (0x00409e6f); 0x7f / 0x7e raise / lower the level byte
//    0x005a4998 + 51 row + col (0x00409e9c / 0x00409eb4); modes below 0x7d (signed) are hole-marker edits (0x00409ebf..0x0040a0f5).
// 4. Every path but 1 and 2 ends markTileKind11, rebuildTileCoverage, [0x004c2e04] = row, [0x005a47e4] = col, rebuildHeightfield
//    (0x0040a100..0x0040a122) and returns the latter's eax.
int __cdecl EditTerrainHeight_re() {
    const int target = G32(0x004c2f9c);                                               // 0x00409cf0
    if (target != -1) {                                                               // 0x00409cf8 / 0x00409cfe
        const int cam0 = G32(0x004c2ba0);                                             // 0x00409d00
        bool moving = cam0 != target;                                                 // 0x00409d06 / 0x00409d08
        if (!moving) moving = G32(0x004c2ba4) != G32(0x005685fc);                     // 0x00409d0a..0x00409d18
        if (moving) {
            const int s0 = kSign(target - cam0);                                      // 0x00409d1a..0x00409d1d
            const int nx = G32(0x004c2ba0) + s0;                                      // 0x00409d22 / 0x00409d2e
            const int cy = G32(0x004c2ba4);                                           // 0x00409d28
            const int dy = G32(0x005685fc) - cy;                                      // 0x00409d30 / 0x00409d35
            G32(0x004c2ba0) = nx;                                                     // 0x00409d37
            const int s1 = kSign(dy);                                                 // 0x00409d3d / 0x00409d3e
            G32(0x004c2ba4) = G32(0x004c2ba4) + s1;                                   // 0x00409d43 / 0x00409d4c / 0x00409d50
            return s1;                                                                // eax at 0x00409d5a
        }
    }
    const int i = G32(0x0055e920);                                                    // 0x00409d5b
    const int len = G32(0x005a9cd4);                                                  // 0x00409d60
    G32(0x0055e920) = i + 1;                                                          // 0x00409d68 / 0x00409d6f
    if (i + 1 > len) {                                                                // 0x00409d69 / jle 0x00409d74
        const int f = G32(0x0059e7b8) & static_cast<int>(0xfdffffff);                 // 0x00409d76..0x00409d7c
        G32(0x0059e7b8) = f;                                                          // 0x00409d82
        G32(0x004c2f9c) = -1;                                                         // 0x00409d87
        return f;                                                                     // eax at 0x00409d95
    }
    const int col = G32(0x00586fa8 + 4 * i);                                          // 0x00409d96
    const int row = G32(0x00586b50 + 4 * i);                                          // 0x00409d9d
    const int mode = G32(0x005a8834 + 4 * i);                                         // 0x00409db1
    const int idx = col + 50 * row;                                                   // lea chains 0x00409dc3..0x00409dcf, 0x00409fe1..0x00409fe7
    if (mode & 0x80) {                                                                // test al, 0x80 / je 0x00409ddf
        TW(idx) = static_cast<unsigned short>(TW(idx) ^ static_cast<unsigned short>(mode & 0xff7f));   // 0x00409dc6..0x00409dd2
    } else if (mode & 0x100) {                                                        // test ah, 1 / je 0x00409e86
        kStampCourseTiles(row, col, mode & ~0x100, 0);                                // 0x00409de8..0x00409df0
        const int m2 = G32(0x005a8834 + 4 * i) & ~0x100;                              // 0x00409df5..0x00409dff
        G32(0x004c2854) = m2;                                                         // 0x00409e05
        if (m2 == 8 || m2 == 7 || m2 == 0xc || m2 == 9 || m2 == 0xb || m2 == 0xd || m2 == 0xe || m2 == 4) {   // 0x00409e0a..0x00409e2d
            TWH(idx) |= 0x40;                                                         // 0x00409e4a
            G8(0x00578804 + idx) = 0;                                                 // 0x00409e52
            const int r = Rnd(0x12c) & 0xffff;                                        // 0x00409e39..0x00409e5e
            kPlaySound(0x90, 0x64, 0, -r, 0);                                         // 0x00409e63..0x00409e6f
        }
        G32(0x004c2854) = -1;                                                         // 0x00409e77
    } else {
        if (mode == 0x7f) LV(51 * row + col) = static_cast<unsigned char>(LV(51 * row + col) + 1);   // 0x00409e86 / 0x00409e9c
        if (mode == 0x7e) LV(51 * row + col) = static_cast<unsigned char>(LV(51 * row + col) - 1);   // 0x00409e9e / 0x00409eb4
        if (mode < 0x7d) {                                                            // cmp 0x7d / jge 0x0040a100 (signed)
            int H = G32(0x005685f0);                                                  // 0x00409ebf
            const int holeNo = (mode == 0) ? H : H - 1;                               // 0x00409ec5..0x00409ed2
            if (mode == 0 || (mode == 2 && G32(0x00834170) % 6 == 0)) {               // 0x00409ed6..0x00409ef6
                G8(0x00575ab0 + 520 * H) = 4;                                         // 0x00409efc..0x00409f05
                const int g1 = kUpdateMembership(0);                                  // 0x00409f0d
                const int a = g1 << 8;
                G8(0x005794d9 + a) = static_cast<unsigned char>(holeNo);              // 0x00409f20
                const int rr = (G32(0x00586b50 + 4 * i) << 10) + 0x200;               // 0x00409f26..0x00409f30
                G32(0x0057958c + a) = rr;                                             // 0x00409f36
                G32(0x005794b8 + a) = rr;                                             // 0x00409f3c
                const int cc = (G32(0x00586fa8 + 4 * i) << 10) + 0x200;               // 0x00409f42..0x00409f4b
                G32(0x00579590 + a) = cc;                                             // 0x00409f51
                G32(0x005794bc + a) = cc;                                             // 0x00409f57
                G8(0x005794da + a) = 1;                                               // 0x00409f5d
                G<unsigned short>(0x00579556 + a) = 2;                                // 0x00409f64
                G<unsigned short>(0x0057955a + a) = static_cast<unsigned short>(g1 ^ 1);   // 0x00409f1b / 0x00409f6d
                const int g2 = kUpdateMembership(0);                                  // 0x00409f74
                const int b = g2 << 8;
                G8(0x005794d9 + b) = static_cast<unsigned char>(holeNo);              // 0x00409f8c
                const int rr2 = G32(0x00586b50 + 4 * i) << 10;                        // 0x00409f7d / 0x00409f94
                G32(0x0057958c + b) = rr2;                                            // 0x00409f97
                G32(0x005794b8 + b) = rr2;                                            // 0x00409f9d
                const int cc2 = G32(0x00586fa8 + 4 * i) << 10;                        // 0x00409fa7 / 0x00409fab
                G32(0x00579590 + b) = cc2;                                            // 0x00409fae
                G32(0x005794bc + b) = cc2;                                            // 0x00409fb4
                G8(0x005794da + b) = 1;                                               // 0x00409fba
                G<unsigned short>(0x00579556 + b) = 8;                                // 0x00409fc4
                G<unsigned short>(0x0057955a + b) = static_cast<unsigned short>(g2 ^ 1);   // 0x00409fc1 / 0x00409fcd
                H = G32(0x005685f0);                                                  // 0x00409fd4
            }
            TT(idx) = static_cast<unsigned char>(G32(0x005a8834 + 4 * i));            // 0x00409fda / 0x00409ff1
            const int m3 = G32(0x005a8834 + 4 * i);                                   // 0x00409fea
            if (Attr1(m3) == 0xd) {                                                   // 0x00409ffa..0x0040a004
                TWH(idx) |= 0x40;                                                     // 0x0040a006
                G8(0x00578804 + idx) = 0;                                             // 0x0040a00e
            }
            if (m3 == 1) {                                                            // 0x0040a015 / 0x0040a018
                const int rec = 520 * H;                                              // 0x0040a01a..0x0040a021
                if (G32(0x00575ac8 + rec) == 0) {                                     // 0x0040a024..0x0040a02c
                    G32(0x00575ac8 + rec) = row;                                      // 0x0040a032
                    G32(0x00575acc + rec) = col;                                      // 0x0040a041
                    const int w0 = G32(0x00575ab8 + rec);                             // 0x0040a04d
                    TW(idx) = static_cast<unsigned short>(H | 0x80);                  // 0x0040a047 / 0x0040a055
                    if (w0 != 0) {                                                    // 0x0040a053 / 0x0040a05d
                        G32(0x004c2f9c) = (w0 + row) / 2;                             // 0x0040a05f..0x0040a06a
                        const int e = G32(0x00575abc + rec) + col;                    // 0x0040a06f / 0x0040a075
                        G8(0x00575ab0 + rec) = 4;                                     // 0x0040a077
                        G32(0x005685fc) = e / 2;                                      // 0x0040a07e..0x0040a084
                        G32(0x005685f0) = H + 1;                                      // 0x0040a083 / 0x0040a089
                    }
                }
            }
            if (G32(0x005a8834 + 4 * i) == 0) {                                       // 0x0040a08f..0x0040a098
                const int rec = 520 * H;                                              // 0x0040a09e..0x0040a0ac
                G32(0x00575ab8 + rec) = row;                                          // 0x0040a0b2
                G32(0x00575abc + rec) = col;                                          // 0x0040a0b8
                TW(idx) = static_cast<unsigned short>(TW(idx) | static_cast<unsigned short>(H));   // 0x0040a0be
                const int w1 = G32(0x00575ac8 + rec);                                 // 0x0040a0c6
                if (w1 != 0) {                                                        // 0x0040a0cc / 0x0040a0ce
                    G32(0x004c2f9c) = (w1 + row) / 2;                                 // 0x0040a0d0..0x0040a0db
                    const int e = G32(0x00575acc + rec) + col;                        // 0x0040a0e0 / 0x0040a0e6
                    G8(0x00575ab0 + rec) = 4;                                         // 0x0040a0e8
                    G32(0x005685fc) = e / 2;                                          // 0x0040a0ef..0x0040a0f5
                    G32(0x005685f0) = H + 1;                                          // 0x0040a0f4 / 0x0040a0fa
                }
            }
        }
    }
    kMarkTileKind11();                                                                // 0x0040a100
    kRebuildTileCoverage();                                                           // 0x0040a105
    G32(0x004c2e04) = G32(0x00586b50 + 4 * i);                                        // 0x0040a10e / 0x0040a115
    G32(0x005a47e4) = G32(0x00586fa8 + 4 * i);                                        // 0x0040a11b / 0x0040a11d
    return kRebuildHeightfield();                                                     // 0x0040a122, eax at 0x0040a12e
}
C0_t EditTerrainHeight_orig;
// 0x00409cf0
SG_HOOK("golf_clean.exe", 0x00409cf0, editTerrainHeight, EditTerrainHeight_re, EditTerrainHeight_orig);

// ====================================================================================================================
// applyTerrainEdit
// ====================================================================================================================

namespace {
// the 4-neighbour smoothing tables the original builds on its stack (0x00471498..0x004714c0): bytes at [esp+0x38..0x3c] and
// [esp+0x2c..0x30]; the code reads them at +0, +1 (row offset), +0 / +1 (column offset)
const signed char kSmA[5] = {0, 1, 1, 0, 0};          // [esp+0x38+k] and, shifted by one, [esp+0x39+k]
const signed char kSmB[5] = {-1, -1, 0, 0, -1};       // [esp+0x2c+k] and, shifted by one, [esp+0x2d+k]

// the walker position source: bytes 0x00578150 / 0x00578154 are the low bytes of the stamped row / column dwords
static inline int Abs(int v) { return v < 0 ? -v : v; }
}  // namespace

// 0x00470a60  applyTerrainEdit(n): cdecl, plain `ret`. Generates course n at random. n is stored in [0x0059bf90] (0x00470a7b) and picks
// the 0x2e-byte record at 0x00571ff4 + 0x2e * n (0x00470a72..0x00470a83); its bytes are read as c0 (+0, 0x00470a87), the object-count
// byte (+1, 0x004720e8), c2 (+2, 0x00470a9a), kind (+3, 0x00470dce / 0x00470ff1 / 0x00472106) and the course type (+4, 0x00470a8e,
// also stored in [0x005a34e0] at 0x00470aa7); [0x005a6d3c] is copied to +6 (0x00470a94). Tile index = col + 50 * row; tile types
// 0x005722e8, attribute copy 0x0053bbac, flag words 0x0053caf0, level grid 0x005a4998 (51 x 51).
//  A. 0x00470a66..0x00470b3b  [0x004c2fa0] = 0x30 / 0x20 / 0x10 for c2 = 0 / 1 / 2 (else unchanged), then + half of it for n / 4 == 0,
//     + a quarter for 1, - a quarter for 3, + 0x10 when [0x0059e7b8] & 0x01000000.
//  B. 0x00470b3b..0x00470c1d  the type table is filled with the base type s10 (4 for course types 0..3; for n >= 8 and n >= 12 with the
//     flag clear 5 / 0xb, or 0xc for course type 1); s10 is NOT initialised for course type 4 (n < 8 or flag set) and for types > 4
//     (stack slot [esp+0x10]); the attribute copy gets the attribute byte of s10, four 2500-byte tables are zeroed.
//  C. 0x00470c1f..0x00470dc1  blobs of one type each (type by course type and pass number): a random walk from a random cell stamps
//     up to limit / 128 cells (limit 5000, +0x9c4 per pass, while the running total is <= 0x4e2 and limit < 0xafc8).
//  D. 0x00470dc1..0x00470fe2  kind != 2: one river from column 49, row 16 .. 31 (+ sampleHeight steps among three neighbours; tile
//     0x11, 0xb for course type 1; flag bit 0x1000; heightBlend == 3 on the neighbour row marks it 8 / 0xc / 0xa).
//  E. 0x00470fe2..0x004710c3  kind == 1: a ragged shore of tile 0x11 (0x0c for type 4) along the west edge, width 2 .. 12.
//  F. 0x004710c3..0x00471108  flag bit 0x1000 cleared everywhere; the level grid filled with heightBlend(row, col, 0), 51 x 51.
//  G. 0x00471108..0x00471363  24 patch walks (type from the level grid and course type; 0x12 on pass 0 and, with [0x00822c88] >= 2, whenever Random::range(8 / (g - 1)) is 0).
//  H. 0x00471363..0x004713d4  32 Random::range(0x2a) calls whose bit-0x100 stores are cleared at once, then type 0x10 becomes 0x0d.
//  I. 0x004713d4..0x00471473  random unblocked cells get bit 0x100 until 9 * (4 - [0x00822c88]) were flagged.
//  J. 0x00471473..0x004715b9  level-grid smoothing next to water (types 0x11 / 0x12).
//  K. 0x004715b9..0x00471f89  the clubhouse: a random cell of type s10 in rows / cols 15 .. 31 accepted by evalPlacementArea; stamped; the
//     features of c0 through the jump table 0x00472244 (c0 > 0xf: none).
//  L. 0x00471f89..0x004720b5  [0x00822c88] more objects on cells of type s10, each with a course-type value in its record.
//  M. 0x004720b5..0x00472207  spawnWalker(-6) placed at the clubhouse; the tile table is copied to 0x00542414; clearTile sweeps of the border
//     rows / columns (kind 2: widths from the count byte); markTileKind11; rebuildHeightfield (its eax is returned).
int __cdecl ApplyTerrainEdit_re(int n) {
    const unsigned rec = 46u * static_cast<unsigned>(n);                              // 0x00470a72..0x00470a83 (S20)
    G32(0x0059bf90) = n;                                                              // 0x00470a7b
    const int c0 = static_cast<signed char>(G8(0x00571ff4 + rec));                    // 0x00470a87 (S14)
    const unsigned char ctypeByte = G8(0x00571ff8 + rec);                             // 0x00470a8e
    G8(0x00571ffa + rec) = G8(0x005a6d3c);                                            // 0x00470a6a / 0x00470a94
    const int c2 = static_cast<signed char>(G8(0x00571ff6 + rec));                    // 0x00470a9a
    G8(0x005a34e0) = ctypeByte;                                                       // 0x00470aa7

    // ---- A: [0x004c2fa0]
    int e;                                                                            // ecx
    if (c2 == 0) { e = 0x30; G32(0x004c2fa0) = e; }                                   // 0x00470aad / 0x00470acf
    else if (c2 == 1) { e = 0x20; G32(0x004c2fa0) = e; }                              // 0x00470aaf..0x00470ac7
    else if (c2 == 2) { e = 0x10; G32(0x004c2fa0) = e; }                              // 0x00470ab5..0x00470aba
    else e = G32(0x004c2fa0);                                                         // 0x00470adc
    const int q = (n + ((n >> 31) & 3)) >> 2;                                         // 0x00470ae2..0x00470aea
    if (q == 0) { e += (e - (e >> 31)) >> 1; G32(0x004c2fa0) = e; }                   // 0x00470b15..0x00470b1e
    else if (q == 1) { e += (e + ((e >> 31) & 3)) >> 2; G32(0x004c2fa0) = e; }        // 0x00470b08..0x00470b13
    else if (q == 3) { e += -((e + ((e >> 31) & 3)) >> 2); G32(0x004c2fa0) = e; }     // 0x00470af9..0x00470b06
    const int flag = G32(0x0059e7b8) & 0x1000000;                                     // 0x00470b24 / 0x00470b2a
    if (flag != 0) { e += 0x10; G32(0x004c2fa0) = e; }                                // 0x00470b32 / 0x00470b35

    // ---- B
    const int ct = static_cast<signed char>(ctypeByte);                               // 0x00470b3b
    int s10 = 0;                                                                      // [esp+0x10]; uninitialised in the original for course type 4 and > 4 unless set below
    if (static_cast<unsigned>(ct) <= 4) {                                             // 0x00470b43 / ja 0x00470b65
        if (ct <= 3) s10 = 4;                                                         // 0x00470b4e
        else for (int k = 0; k < 2500; ++k) TT(k) = 0x0e;                             // 0x00470b54..0x00470b63 (0x271 dwords of 0x0e0e0e0e)
    }
    if (flag == 0) {                                                                  // 0x00470b65 / 0x00470b67
        if (n >= 8) s10 = (ctypeByte != 1) ? 5 : 0xc;                                 // 0x00470b69..0x00470b7b
        if (n >= 0xc) s10 = (ctypeByte != 1) ? 0xb : 0xc;                             // 0x00470b7f..0x00470b8d
    }
    for (int k = 0; k < 2500; ++k) TT(k) = static_cast<unsigned char>(s10);           // 0x00470b91..0x00470bb7
    {
        const unsigned char ab = static_cast<unsigned char>(Attr0(static_cast<signed char>(TT(0))));   // 0x00470bb9..0x00470bdf
        for (int k = 0; k < 2500; ++k) TB(k) = ab;                                    // 0x00470bec
    }
    for (int k = 0; k < 2500; ++k) { G8(0x0053ea24 + k) = 0; G8(0x005a6378 + k) = 0; G8(0x0056c7e4 + k) = 0; G8(0x0056988c + k) = 0; }   // 0x00470bed..0x00470c1d

    // ---- C: blobs
    int p = 0;                                                                        // [esp+0x44]
    int total = 0;                                                                    // [esp+0x24]
    int limit = 5000;                                                                 // [esp+0x1c] = 0x1388 (0x00470bcd)
    int t = 0;                                                                        // ebx (0x00470ba9)
    for (;;) {                                                                        // 0x00470c1f
        int cnt = 0;                                                                  // [esp+0x18] (0x00470c26)
        int r = Rnd(0x32) & 0xffff;                                                   // 0x00470c2e / 0x00470c3d
        int c = Rnd(0x32) & 0xffff;                                                   // 0x00470c43 / 0x00470c52
        const int ct2 = static_cast<signed char>(G8(0x005a34e0));                     // 0x00470c4b
        if (static_cast<unsigned>(ct2) <= 4) {                                        // 0x00470c58 / ja 0x00470cc2
            switch (ct2) {
            case 0: t = (c >= 0x19 ? 1 : 0) + 0xd; if (c0 == 9) t = 0xe; break;       // 0x00470c64..0x00470c78
            case 1: t = ((~p & 1) | 6) << 1; break;                                   // 0x00470c92..0x00470ca0
            case 2: t = (p & 1) ? (((p & 2) | 0x18) >> 1) : 0xe; break;               // 0x00470ca2..0x00470cb1
            case 3: t = ((p & 1) ? 6 : 0) + 5; break;                                 // 0x00470c7f..0x00470c8d
            case 4: t = ((p & 1) << 3) | 4; break;                                    // 0x00470cb5..0x00470cbf
            }
        }
        for (;;) {                                                                    // 0x00470cc2
            if (r < 0 || c < 0 || r >= 0x32 || c >= 0x32) break;                      // 0x00470cc2..0x00470cde
            if (cnt >= (limit + ((limit >> 31) & 0x7f)) >> 7) break;                  // 0x00470ce4..0x00470cf7
            const int idx = c + 50 * r;                                               // 0x00470cfd..0x00470d03
            ++cnt;                                                                    // 0x00470d09 / 0x00470d13
            TT(idx) = static_cast<unsigned char>(t);                                  // 0x00470d0d
            if (cnt == 0x30) TWH(idx) |= 0x20;                                        // 0x00470d0a / 0x00470d19
            TB(idx) = static_cast<unsigned char>(Attr0(static_cast<signed char>(t)));   // 0x00470d21..0x00470d37
            if ((Rnd(0x40) & 0xffff) == 0) TWH(idx) |= 1; else TWH(idx) &= 0xfe;      // 0x00470d3d..0x00470d51
            r += (Rnd(3) & 0xffff) - 1;                                               // 0x00470d60..0x00470d71
            c += (Rnd(3) & 0xffff) - 1;                                               // 0x00470d75..0x00470d81
        }
        total += cnt;                                                                 // 0x00470d8b..0x00470d9a
        if (total > 0x4e2) break;                                                     // 0x00470d95 / jg 0x00470dc1
        ++p; limit += 0x9c4;                                                          // 0x00470da0..0x00470dbb
        if (!(limit < 0xafc8)) break;                                                 // 0x00470dae / jl 0x00470c1f
    }

    // ---- D: river
    int cnt34 = 1;                                                                    // [esp+0x34] (0x00470dca)
    if (G8(0x00571ff7 + rec) == 2) cnt34 = 0;                                         // 0x00470dce..0x00470dd9
    p = 0;                                                                            // 0x00470ddd..0x00470de1
    int bestk = 0;                                                                    // [esp+0x38] (stale across loop passes)
    int prevk = 0;                                                                    // [esp+0x2c]
    if (cnt34 > 0) {                                                                  // 0x00470ddf / jle 0x00470fe2
        do {                                                                          // 0x00470ded
            const int rnd = Rnd(0x10) & 0xffff;                                       // 0x00470df1..0x00470dfd
            int col = 0x31;                                                           // [esp+0x18] (0x00470e0a)
            const int ecxv = (5 * p + 5) * 10;                                        // 0x00470e00..0x00470e1a
            const int hi = static_cast<int>((static_cast<long long>(ecxv) * 0x55555556LL) >> 32);   // 0x00470e1c
            int rowc = hi + rnd + static_cast<int>(static_cast<unsigned>(hi) >> 31);  // 0x00470e1e..0x00470e25
            prevk = 0;                                                                // 0x00470e27
            for (;;) {                                                                // 0x00470e2f
                if (rowc < 0 || rowc > 0x31) break;                                   // 0x00470e2f..0x00470e3a (-> 0x00470fcd)
                const unsigned char tile = static_cast<unsigned char>((static_cast<unsigned char>(G8(0x005a34e0) - 1) != 0 ? 6 : 0) + 0xb);   // 0x00470e40..0x00470e5c
                const int idx = col + 50 * rowc;                                      // 0x00470e52..0x00470e59
                TT(idx) = tile;                                                       // 0x00470e5e
                TW(idx) = static_cast<unsigned short>((TW(idx) & ~0x100) | 0x1000);   // 0x00470e64..0x00470e7b
                TB(idx) = static_cast<unsigned char>(Attr0(static_cast<signed char>(tile)));   // 0x00470e6c..0x00470e89
                if ((Rnd(0x40) & 0xffff) == 0) TWH(idx) |= 1;                         // 0x00470e8f..0x00470e9e
                const int c1 = col + 1;                                               // 0x00470ea6
                int best = 999;                                                       // [esp+0x24] (0x00470eae)
                int bestc = c1;                                                       // [esp+0x28] (0x00470eb6)
                const int sg = (Rnd(2) & 0xffff) != 0 ? 1 : -1;                       // 0x00470eba..0x00470eca
                int nrow = sg + rowc;                                                 // esi (0x00470ecb)
                const int idx2 = c1 + 50 * nrow;                                      // 0x00470ecd..0x00470ed3
                if (TT(idx2) != 0x11) {                                               // 0x00470eda..0x00470ee3
                    if (kHeightBlend(nrow, c1, 0) == 3) {                             // 0x00470ee5..0x00470ef4
                        unsigned char v;
                        if (p == 0) v = static_cast<unsigned char>((~((nrow + c1) * 2) & 4) | 8);   // 0x00470f05..0x00470f0f
                        else v = 0xc;                                                 // 0x00470efe
                        TT(idx2) = v;                                                 // 0x00470f11
                        if (G8(0x005a34e0) == 1) TT(idx2) = 0xa;                      // 0x00470f14..0x00470f1d
                    }
                }
                for (int k = -2; k <= 2; k += 2) {                                    // 0x00470f21..0x00470f90
                    const int er = Dx(k) + rowc;                                      // edi (0x00470f37 / 0x00470f47)
                    const int ec = Dy(k) + col;                                       // ebp (0x00470f3d / 0x00470f49)
                    if (p != 0 && er >= 0x30) continue;                               // 0x00470f4b..0x00470f52
                    const int h = kSampleHeight(er << 7, ec << 7);                    // 0x00470f54..0x00470f60
                    if (h >= best) continue;                                          // 0x00470f6c / jge 0x00470f82
                    best = h; nrow = er; bestc = ec; bestk = k;                       // 0x00470f70..0x00470f7e
                }
                if (bestk == -prevk) { col = col - 1; }                               // 0x00470f92..0x00470fa5 (ecx = [esp+0x18] - 1, ebx unchanged)
                else { col = bestc; rowc = nrow; }                                    // 0x00470fa7..0x00470fab
                prevk = bestk;                                                        // 0x00470fb0
                if (TT(col + 50 * rowc) == 0x11) break;                               // 0x00470fbb..0x00470fc3
                if (col < 0) break;                                                   // 0x00470fc5 / jge 0x00470e2f
            }
            ++p;                                                                      // 0x00470fcd..0x00470fd8
        } while (p < cnt34);                                                          // jl 0x00470ded
    }

    // ---- E: shore
    cnt34 = 0;                                                                        // 0x00470fe6..0x00470fe8
    int ebp5 = 5;                                                                     // 0x00470fec
    if (G8(0x00571ff7 + rec) == 1) {                                                  // 0x00470ff1 / 0x00470ff8
        p = 0;                                                                        // 0x00470ffe
        do {                                                                          // 0x00471002
            if (ebp5 > 0) {                                                           // 0x00471004 / jle 0x00471086
                int es = 0;
                do {                                                                  // 0x00471012
                    const int ix = p + es;
                    if (TT(ix) != 0x11) {                                             // 0x00471014 / je 0x00471071
                        const unsigned char tile = (G8(0x005a34e0) != 4) ? 0x11 : 0x0c;   // 0x0047101a..0x0047102e
                        TT(ix) = tile;                                                // 0x00471031
                        TB(ix) = static_cast<unsigned char>(Attr0(static_cast<signed char>(tile)));   // 0x00471034..0x00471043
                        TWH(ix) &= 0xfe;                                              // 0x0047104c
                        if (es == ebp5 - 1) {                                         // 0x00471054 / jne 0x00471071
                            if ((Rnd(0x10) & 0xffff) == 0) TWH(ix) |= 1;              // 0x00471058..0x00471069
                        }
                    }
                    if (es == 0) TT(p) = 0x14;                                        // 0x00471071..0x00471075
                    ++cnt34; ++es;                                                    // 0x00471078..0x00471080
                } while (es < ebp5);                                                  // jl 0x00471012
            }
            ebp5 = ebp5 + (Rnd(5) & 0xffff) - 2;                                      // 0x00471086..0x00471097
            if (ebp5 > 1) { if (ebp5 > 0xc) --ebp5; }                                 // 0x0047109b..0x004710ac
            else ebp5 = 2;                                                            // 0x004710a0
            p += 0x32;                                                                // 0x004710ad..0x004710bd
        } while (p < 0x9c4);
    }

    // ---- F: clear bit 0x1000, level grid
    for (int k = 0; k < 2500; ++k) TWH(k) &= 0xef;                                    // 0x004710c3..0x004710dc
    for (int row = 0; row <= 0x32; ++row)                                             // 0x004710de..0x00471106
        for (int col = 0; col <= 0x32; ++col)                                         // 0x004710e5..0x004710fa
            LV(51 * row + col) = static_cast<unsigned char>(kHeightBlend(row, col, 0));   // 0x004710e7..0x004710f3

    // ---- G: 24 patch walks
    t = 0;                                                                            // 0x00471108 / 0x00471110 (ebx = [esp+0x44])
    for (p = 0; ; ) {                                                                 // 0x00471114
        const int r4a = Rnd(4) & 0xffff;                                              // 0x00471114..0x00471129
        const int rowp = ((p & 3) * 50) >> 2;                                         // 0x0047112f..0x0047114b
        int es = r4a + rowp + 4;                                                      // 0x0047114e
        const int r4b = Rnd(4) & 0xffff;                                              // 0x00471157..0x00471165
        const int colp = (((p >> 2) * 50)) >> 2;                                      // 0x0047116b..0x0047117e
        int eb = r4b + colp + 4;                                                      // 0x00471181
        const int ct3 = static_cast<signed char>(G8(0x005a34e0));                     // 0x00471185
        if (static_cast<unsigned>(ct3) <= 4) {                                        // 0x0047118c / ja 0x00471221
            const unsigned lv = LV(51 * es + eb);
            switch (ct3) {
            case 0: t = (3u < lv) ? 0xc : 0x11; break;                                // 0x0047119c / 0x004711d6..0x004711e5
            case 1: t = (3u < lv) ? 0xc : 0xf; break;                                 // 0x004711ab..0x004711c7
            case 4: t = (4u < lv) ? 0xc : 0x11; break;                                // 0x004711c9..0x004711e5
            case 2: t = (3u < lv) ? 0xf : 0x11; break;                                // 0x004711e7..0x00471203
            case 3: t = (3u < lv) ? 0xf : 0xc; break;                                 // 0x00471205..0x0047121e
            }
        }
        bool water = false;
        const int g = G32(0x00822c88);                                                // 0x00471221
        if (g >= 2) {                                                                 // 0x00471226 / jl 0x00471246
            const int dv = 8 / (g - 1);                                               // 0x0047122b..0x00471234
            if ((Rnd(dv) & 0xffff) == 0) water = true;                                // 0x00471236..0x00471244
        }
        if (water || p == 0) t = 0x12;                                                // 0x00471246..0x0047124a
        if (c0 == 2 && (t == 8 || t == 0x12)) t = 0xc;                                // 0x0047124f..0x00471260
        int steps = 0;                                                                // [esp+0x34] (0x00471265)
        for (;;) {                                                                    // 0x0047126d
            const int idx = eb + 50 * es;                                             // 0x0047126d..0x00471273
            if ((TW(idx) & 0x1000) == 0) {                                            // 0x00471277..0x00471287
                TT(idx) = static_cast<unsigned char>(t);                              // 0x00471290
                if ((Rnd(8) & 0xffff) == 0 && t == 0x12) TWH(idx) |= 1;               // 0x00471289..0x004712a5
                TB(idx) = static_cast<unsigned char>(Attr0(static_cast<signed char>(TT(idx))));   // 0x004712ad..0x004712c3
                if (t == 0x11) LV(51 * es + eb) = 3;                                  // 0x004712ba / 0x004712c9..0x004712d6
            }
            if ((Rnd(2) & 0xffff) != 0) es += (Rnd(2) & 0xffff) != 0 ? 1 : -1;       // 0x004712de..0x00471306
            else eb += (Rnd(2) & 0xffff) != 0 ? 1 : -1;                               // 0x00471308..0x00471316
            if (es < 0 || es >= 0x32 || eb < 0 || eb >= 0x32) break;                  // 0x00471318..0x00471328
            ++steps;                                                                  // 0x0047132a..0x00471332
            if (steps <= 4) continue;                                                 // 0x00471336 / jle 0x0047126d
            if ((Rnd(0x10) & 0xffff) != 0) continue;                                  // 0x0047133c..0x0047134b
            break;
        }
        ++p;                                                                          // 0x00471351..0x00471359
        if (!(p < 0x18)) break;                                                       // jl 0x00471114
    }

    // ---- H: 16 pairs of RNG calls (their effect is cleared at once), type 0x10 -> 0x0d
    for (int k = 0; k < 16; ++k) {                                                    // 0x00471363..0x004713a5
        const int s = Rnd(0x2a) & 0xffff;                                             // 0x0047136d..0x0047137e
        const int r2 = Rnd(0x2a) & 0xffff;                                            // 0x00471389..0x0047138e
        TW(s + 50 * r2 + 204) = static_cast<unsigned short>(TW(s + 50 * r2 + 204) | 0x100);   // 0x00471393..0x0047139c
    }
    for (int k = 0; k < 2500; ++k) {                                                  // 0x004713a7..0x004713d2
        TWH(k) &= 0xfe;                                                               // 0x004713b3
        if (TT(k) == 0x10) TT(k) = 0xd;                                               // 0x004713b7..0x004713bd
    }

    // ---- I: random flagged tiles
    int m = 0;                                                                        // ebp (0x004713d4)
    do {                                                                              // 0x004713d6
        const int es = (Rnd(0x2e) & 0xffff) + 2;                                      // 0x004713d6..0x004713f2
        const int ed = (Rnd(0x2e) & 0xffff) + 2;                                      // 0x004713e5..0x00471403
        if (kTileBlocked(es, ed) == 0) {                                              // 0x00471406..0x00471412
            const int idx = ed + 50 * es;                                             // 0x00471414..0x0047141a
            const unsigned char al = TT(idx);                                         // 0x0047141d
            if (al != 0x15 && al != 0x16 && al != 4 && al != 0x12) {                  // 0x00471423..0x00471431
                const int lim = (G8(0x005a34e0) != 1) ? 0xa : 0xb;                    // 0x00471433..0x00471442
                if (static_cast<signed char>(al) != lim) {                            // 0x00471445 / je 0x0047145b
                    if (!(al == 0x11 && ed <= 6)) {                                   // 0x00471449..0x00471450
                        TW(idx) = static_cast<unsigned short>(TW(idx) | 0x100);      // 0x00471452
                        ++m;                                                          // 0x0047145a
                    }
                }
            }
        }
    } while (m < (4 - G32(0x00822c88)) * 9);                                          // 0x0047145b..0x0047146d

    // ---- J: level-grid smoothing around water
    for (int p2 = 0; p2 <= 50; ++p2) {                                                // 0x00471473..0x004715b3 (ecx <= 0x00572cac)
        for (int cx = 0; cx <= 0x32; ++cx) {                                          // 0x00471484..0x0047159b
            const unsigned char a0 = G8(0x005722e8 + 50 * p2 + cx);                   // 0x00471484
            if (a0 != 0x11 && a0 != 0x12) continue;                                   // 0x00471487..0x0047148d
            for (int k = 0; k < 4; ++k) {                                             // 0x004714c5..0x0047158d
                const int a = kSmA[k], b = kSmB[k];                                   // 0x004714c7 / 0x004714cc
                const int row1 = a + p2, col1 = b + cx;                               // 0x004714d1 / 0x004714d5
                if (row1 < 0 || row1 >= 0x32 || col1 < 0 || col1 >= 0x32) continue;  // 0x004714d3..0x004714f2
                const int nbr = Dx(2 * k) + p2;                                       // 0x004714f8 / 0x00471506 (table entry 2k)
                if (G8(0x005722e8 + Dy(2 * k) + 50 * nbr + cx) == 0x11) continue;     // 0x004714ff..0x00471511
                unsigned char* cell = &LV(51 * row1 + b + cx);                        // 0x0047151b..0x0047152e
                const unsigned char v = *cell;                                        // 0x00471527
                if (v <= 3) continue;                                                 // 0x00471535 / jbe 0x00471585
                const int row2 = kSmA[k + 1] + p2;                                    // 0x0047153a / 0x0047153f
                const int b2 = kSmB[k + 1];                                           // 0x00471543
                if (v != LV(51 * row2 + cx + b2)) continue;                           // 0x00471548..0x0047155a
                if (v == 4) *cell = 3;                                                // 0x0047155c..0x00471561
                else *cell = static_cast<unsigned char>(*cell + ((Rnd(2) & 0xffff) != 0 ? 1 : -1));   // 0x00471566..0x00471583
            }
        }
    }

    // ---- K: clubhouse spot, then the c0-specific features
    int es, ed;
    for (;;) {                                                                        // 0x004715b9
        es = (Rnd(0x11) & 0xffff) + 0xf;                                              // 0x004715b9..0x004715d5
        ed = (Rnd(0x11) & 0xffff) + 0xf;                                              // 0x004715c8..0x004715e9
        if (static_cast<signed char>(TT(ed + 50 * es)) != s10) continue;              // 0x004715f3..0x004715fd
        if (kEvalPlacementArea(es, ed, static_cast<signed char>(G8(0x004c27ec)), 0xf) == -1) continue;   // 0x004715ff..0x00471616
        break;
    }
    kStampCourseTiles(es, ed, 0xf, 0x60);                                             // 0x00471618..0x0047161e
    G8(0x0058bcbf) |= 0x40;                                                           // 0x00471623..0x00471636
    const int cell0 = ed + 50 * es;                                                   // ebx (0x00471642)
    G32(0x00578158) = es;                                                             // 0x00471648
    G32(0x00578150) = es;                                                             // 0x0047164e
    TWL(cell0) &= 0xdf;                                                               // 0x00471654
    G32(0x0057815c) = ed;                                                             // 0x0047165f
    G32(0x00578154) = ed;                                                             // 0x00471665
    G32(0x004c2ba0) = es + 6;                                                         // 0x0047166b
    G32(0x004c2ba4) = ed - 4;                                                         // 0x00471671

    if (static_cast<unsigned>(c0) <= 0xf) {                                           // 0x0047165c / ja 0x00471f89
        switch (c0) {
        case 2: case 4: case 9: case 11: case 14: {                                   // 0x00471691 / 0x0047169f / 0x00471683 / 0x00471698 / 0x0047168a
            int ty;                                                                   // the type each jump-table entry loads into ebp
            if (c0 == 2) ty = 0xd;                                                    // 0x00471691
            else if (c0 == 4) ty = 9;                                                 // 0x0047169f
            else if (c0 == 9) ty = 6;                                                 // 0x00471683
            else if (c0 == 11) ty = 0xe;                                              // 0x00471698
            else ty = 8;                                                              // 0x0047168a (c0 == 14)
            if (kTileBlocked(es, ed + 5) == 0) {                                      // 0x004716a4..0x004716b1
                kStampCourseTiles(es - 1, ed + 4, ty, 0);                             // 0x004716b8..0x004716be
                TW(cell0 + 2) = static_cast<unsigned short>(TW(cell0 + 2) | 0x20);    // 0x004716cb
                TW(cell0 + 3) = static_cast<unsigned short>(TW(cell0 + 3) | 0x20);    // 0x004716d3
                TW(cell0 + 4) = static_cast<unsigned short>(TW(cell0 + 4) | 0x20);    // 0x004716db
            } else {
                kStampCourseTiles(es - 1, ed - 5, ty, 0);                             // 0x004716ec..0x004716f2
                TW(cell0 - 1) = static_cast<unsigned short>(TW(cell0 - 1) | 0x20);    // 0x004716ff
                TW(cell0 - 2) = static_cast<unsigned short>(TW(cell0 - 2) | 0x20);    // 0x00471707
                TW(cell0 - 3) = static_cast<unsigned short>(TW(cell0 - 3) | 0x20);    // 0x0047170f
            }
            kRebuildTileCoverage();                                                   // 0x00471717
            if (c0 == 4 || c0 == 3) {                                                 // 0x0047171c..0x00471728
                int s2, d2;
                do {                                                                  // 0x0047172e
                    s2 = (Rnd(0x28) & 0xffff) + 5;                                    // 0x0047172e..0x0047174a
                    d2 = (Rnd(0x28) & 0xffff) + 5;                                    // 0x0047173d..0x0047175f
                } while (kEvalPlacementArea(s2, d2, 2, 4) == -1);                     // 0x00471762..0x0047176f
                const int oi = kStampCourseTiles(s2, d2, 4, 0);                       // 0x00471771..0x00471777
                G32(0x0058bcc0 + (oi << 4)) = 0xe;                                    // 0x0047177f / 0x00471782
            }
            break;
        }
        case 7: {                                                                     // 0x00471791
            for (int k = 0; k < 2; ++k) {                                             // 0x00471791 / 0x00471793
                const int rr = Rnd(0xa) & 0xffff;                                     // 0x00471795..0x004717a8
                const int edi = (k != 0 ? rr : -rr) + 0x19;                           // 0x004717aa..0x004717b6
                int ebx = 0;                                                          // 0x004717b9
                for (;;) {                                                            // 0x004717c1
                    const unsigned char al = TT(ebx + 50 * edi);
                    if ((al == 0x11 || al == 0x14) && ebx < 0x30) { ++ebx; continue; }   // 0x004717c8..0x004717d6
                    break;
                }
                const int oi = kStampCourseTiles(edi, ebx, 4, 0);                     // 0x004717d8..0x004717de
                const int rv = Rnd(2) & 0xffff;                                       // 0x004717ed..0x004717ef
                G32(0x0058bcc0 + (oi << 4)) = (rv != 0) ? 7 : 0xd;                    // 0x004717f4..0x00471801
                LV(51 * edi + ebx) = static_cast<unsigned char>(LV(51 * edi + ebx) + 2);   // 0x00471807..0x00471821
            }
            break;
        }
        case 1: {                                                                     // 0x0047182e
            const int rem = 8 - G32(0x00822c88);                                      // 0x0047182e..0x00471839
            if (rem > 0) {                                                            // 0x0047183d / jle 0x00471f89
                int done = 0;
                do {
                    int ix;
                    do {                                                              // 0x0047184a
                        const int s = (Rnd(0x1e) & 0xffff) + 0xa;                     // 0x0047184a..0x00471866
                        const int c = Rnd(0x1e) & 0xffff;                             // 0x00471869..0x00471871
                        ix = c + 50 * s;                                              // 0x00471876..0x00471879
                    } while (TT(ix) != 0x11);                                         // 0x0047187c..0x00471883
                    TW(ix) = static_cast<unsigned short>(TW(ix) | 0x100);            // 0x0047188b
                    ++done;                                                           // 0x00471898
                } while (done < 8 - G32(0x00822c88));                                 // 0x00471893..0x0047189d
            }
            break;
        }
        case 5: case 6: case 8: case 12: case 13: case 15: {                          // 0x004718a4
            int s3, d3;
            do {                                                                      // 0x004718a4
                s3 = (Rnd(0x28) & 0xffff) + 5;                                        // 0x004718a4..0x004718c0
                d3 = Rnd(0x28) & 0xffff;                                              // 0x004718b3..0x004718c8
                d3 += 5; d3 -= 2; s3 -= 2;                                            // 0x004718cf / 0x004718d4 / 0x004718d7
            } while (kEvalPlacementArea(s3, d3, 5, 0xf) == -1);                       // 0x004718dc..0x004718e7
            int p3 = 0;                                                               // 0x004718f6
            if (8 - G32(0x00822c88) > 0) {                                            // 0x004718e9..0x00471900
                do {                                                                  // 0x00471906
                    Rnd(7);                                                           // 0x0047190a..0x00471911
                    Rnd(7);                                                           // 0x00471916..0x0047191d
                    const int edi = (Rnd(0x28) & 0xffff) + 5;                         // 0x00471922..0x0047193e
                    const int ebp = (Rnd(0x28) & 0xffff) + 5;                         // 0x00471931..0x00471953
                    do {                                                              // single pass; `break` = jump to 0x00471eac
                        if (kEvalPlacementArea(edi - 1, ebp - 1, 3, 7) == -1) break;  // 0x00471956..0x00471969
                        const int ix = ebp + 50 * edi;                                // 0x0047196f..0x00471975
                        if (TT(ix) == 0x11) break;                                    // 0x00471983 / je 0x00471eac
                        if (c0 == 5) {                                                // 0x0047198c / jne 0x00471a92
                            TT(ix) = 0x11;                                            // 0x00471995
                            LV(51 * edi + ebp + 50) = 6;                              // 0x004719a5
                            Rnd(4);                                                   // 0x0047199f..0x004719b2
                            for (int k = 0; k < 8; ++k) {                             // 0x004719b7..0x00471a87
                                const int nr = Dx(k) + edi, nc = Dy(k) + ebp;         // 0x004719c8..0x004719d8
                                const int nx = nc + 50 * nr;
                                switch (k) {
                                case 0: TT(nx) = 0x11; LV(51 * nr + nc + 50) = 6; break;   // 0x004719e6..0x004719f4, 0x00471a14..0x00471a1e
                                case 1: TT(nx) = 5; LV(51 * nr + nc + 50) = 6; break;      // 0x004719f6..0x00471a04
                                case 6: TT(nx) = 0x11; LV(51 * nr + nc + 50) = 6; break;   // 0x00471a06..0x00471a0c
                                case 7: TT(nx) = 5; LV(51 * nr + nc + 50) = 6; break;      // table slot 7 -> 0x004719f6
                                case 2: TT(nx) = 0x11; LV(51 * nr + nc + 50) = 3; break;   // 0x00471a28..0x00471a40
                                default:                                                   // 3 4 5: 0x00471a4a
                                    if ((Rnd(2) & 0xffff) != 0) TT(nx) = 0x11;             // 0x00471a4a..0x00471a61
                                    LV(51 * nr + nc - 1) = 3;                              // 0x00471a69..0x00471a73
                                    break;
                                }
                            }
                            break;                                                    // 0x00471a8d
                        }
                        if (c0 == 0xf || c0 == 0xc || c0 == 8) {                      // 0x00471a92..0x00471a9f
                            const int oi = kStampCourseTiles(edi, ebp, 4, 0);         // 0x00471aa5..0x00471aab
                            const int ro = oi << 4;                                   // 0x00471ab0
                            G32(0x0058bcc0 + ro) = 0xb;                               // 0x00471abb
                            if (c0 == 8) G32(0x0058bcc0 + ro) = (Rnd(2) & 0xffff) != 0 ? 6 : 2;   // 0x00471ac5..0x00471ade
                            else if (c0 == 0xc) G32(0x0058bcc0 + ro) = 3;             // 0x00471ae6..0x00471aef
                            G8(0x0058bcbe + ro) = static_cast<unsigned char>(Rnd(4));  // 0x00471af5..0x00471b01
                            for (int k = 0; k < 8; ++k) {                             // 0x00471b07..0x00471bd2
                                const int ix2 = (Dx(k) + edi) * 50 + Dy(k) + ebp;     // 0x00471b09..0x00471b22
                                const unsigned char dl = TT(ix2);                     // 0x00471b25
                                if (dl == 0x16 || dl == 0x11) continue;               // 0x00471b2b..0x00471b37
                                if (p3 & 1) {                                         // 0x00471b3d / je 0x00471bb4
                                    TT(ix2) = (c0 == 0xc) ? 0xc : 4;                  // 0x00471b44..0x00471b54
                                    if (c0 == 0xf) { TT(ix2) = 0x11; continue; }      // 0x00471b51..0x00471b63
                                    if (k != 0) continue;                             // 0x00471b65 / jne 0x00471bce
                                    const int base = 51 * edi + ebp;                  // 0x00471b69..0x00471b74
                                    LV(base) = static_cast<unsigned char>(LV(base) + 2);             // 0x00471b76..0x00471b7f
                                    LV(base + 51) = static_cast<unsigned char>(LV(base + 51) + 2);   // 0x00471b85..0x00471b8e
                                    LV(base - 1) = static_cast<unsigned char>(LV(base - 1) + 2);     // 0x00471b94..0x00471b9d
                                    LV(base + 50) = static_cast<unsigned char>(LV(base + 50) + 2);   // 0x00471ba3..0x00471bac
                                } else {
                                    TT(ix2) = 5;                                      // 0x00471bb6
                                    TW(ix2) = static_cast<unsigned short>(TW(ix2) | (((k & 1) << 9) | 0x20));   // 0x00471bb4..0x00471bc6
                                }
                            }
                            break;                                                    // falls through the tests below to 0x00471eac
                        }
                        if (c0 == 0xa) {                                              // 0x00471bd8 / 0x00471bdd / 0x00471bdf (not reachable: table slot 10 -> 0x00471ecf)
                            const int oi = kStampCourseTiles(edi, ebp, 4, 0);         // 0x00471be5..0x00471beb
                            const int ro = oi << 4;                                   // 0x00471bf0
                            G32(0x0058bcc0 + ro) = 1;                                 // 0x00471bff
                            G8(0x0058bcbe + ro) = static_cast<unsigned char>(Rnd(4)); // 0x00471bfd..0x00471c0e
                            for (int k = 0; k < 8; ++k) {                             // 0x00471c14..0x00471c91
                                const int ix2 = (Dx(k) + edi) * 50 + Dy(k) + ebp;
                                const int tt = static_cast<signed char>(TT(ix2));
                                if (Attr1(tt) == 4) {                                 // 0x00471c44 / jne 0x00471c8b
                                    TT(ix2) = 5;                                      // 0x00471c4d
                                    if ((Rnd(6) & 0xffff) == 0) TWH(ix2) |= 1;        // 0x00471c50..0x00471c7c
                                }
                            }
                            break;
                        }
                        if (c0 == 0xd) {                                              // 0x00471c98 / jne 0x00471dfc
                            const int oi = kStampCourseTiles(edi, ebp, 4, 0);         // 0x00471ca1..0x00471ca7
                            const int ro = oi << 4;                                   // 0x00471cac
                            G32(0x0058bcc0 + ro) = 1;                                 // 0x00471cbb
                            G8(0x0058bcbe + ro) = static_cast<unsigned char>(Rnd(4)); // 0x00471cb9..0x00471cca
                            for (int k = 0; k < 8; ++k) {                             // 0x00471cd0..0x00471d13
                                const int ix2 = (Dx(k) + edi) * 50 + Dy(k) + ebp;
                                const unsigned char dl = TT(ix2);
                                if (Attr1(static_cast<signed char>(dl)) == 0xd) G32(0x0058bcc0 + ro) = 0xa;   // 0x00471cf9..0x00471d02
                                if (dl == 0x11) { G32(0x0058bcc0 + ro) = 9; break; }  // 0x00471d08 / 0x00471d17
                            }
                            for (int k = 0; k < 8; ++k) {                             // 0x00471d21..0x00471df1
                                const int rv = G32(0x0058bcc0 + ro);                  // 0x00471d23
                                const int ix2 = (Dx(k) + edi) * 50 + Dy(k) + ebp;
                                if (rv == 1) {                                        // 0x00471d29 / 0x00471d2c
                                    TWH(ix2) |= 1;                                    // 0x00471d48
                                    TT(ix2) = 0xb;                                    // 0x00471d4f
                                }
                                if (rv == 3) {                                        // 0x00471d56 / 0x00471d59
                                    if (TT(ix2) == 4) TT(ix2) = 0xa;                  // 0x00471d79..0x00471d85
                                }
                                if (rv == 9) {                                        // 0x00471d88 / 0x00471d8b
                                    if (TT(ix2) == 4 && (Rnd(3) & 0xffff) == 0) TWH(ix2) |= 1;   // 0x00471da6..0x00471ddc
                                }
                            }
                            break;
                        }
                        if (c0 == 6) {                                                // 0x00471dfc / 0x00471dff
                            const int oi = kStampCourseTiles(edi, ebp, 4, 0);         // 0x00471e05..0x00471e0b
                            const int bb = p3 & 1;                                    // 0x00471e10 / 0x00471e17
                            const int ro = oi << 4;                                   // 0x00471e1e
                            G32(0x0058bcc0 + ro) = 8 + 4 * bb;                        // 0x00471e21..0x00471e32
                            G8(0x0058bcbe + ro) = static_cast<unsigned char>(Rnd(4)); // 0x00471e2d..0x00471e3d
                            for (int k = 0; k < 8; ++k) {                             // 0x00471e43..0x00471eaa
                                const int ix2 = (Dx(k) + edi) * 50 + Dy(k) + ebp;
                                if (bb != 0) TWH(ix2) |= 0x10;                        // 0x00471e4c..0x00471e64
                                else if (TT(ix2) != 0x11)                             // 0x00471e75..0x00471e91
                                    TW(ix2) = static_cast<unsigned short>(TW(ix2) | (((k & 1) << 9) | 0x20));   // 0x00471e93..0x00471e9e
                            }
                            break;
                        }
                    } while (0);
                    ++p3;                                                             // 0x00471eac..0x00471ebe
                } while (p3 < 8 - G32(0x00822c88));                                   // 0x00471ec2 / jl 0x00471906
            }
            break;
        }
        case 0: case 10: {                                                            // 0x00471ecf
            int done = 0;                                                             // ebx
            do {                                                                      // 0x00471ed1
                const int s = (Rnd(0x2e) & 0xffff) + 2;                               // 0x00471ed1..0x00471eed
                const int d = (Rnd(0x2e) & 0xffff) + 2;                               // 0x00471ee0..0x00471efe
                if (kTileBlocked(s, d) != 0) continue;                                // 0x00471f01..0x00471f0d
                const int ix = d + 50 * s;                                            // 0x00471f0f..0x00471f15
                const unsigned char al = TT(ix);                                      // 0x00471f18
                if (al == 0x15 || al == 0x16 || al == 0x11) continue;                 // 0x00471f1e..0x00471f28
                if ((TW(ix) & 0x400) != 0) continue;                                  // 0x00471f2a..0x00471f3d
                TW(ix) = static_cast<unsigned short>(TW(ix) | 0x100);                 // 0x00471f3f / 0x00471f44
                if (c0 == 0) TT(ix) = 0xb;                                            // 0x00471f42..0x00471f4e
                else if (c0 == 0xd) TT(ix) = 0xb;                                     // 0x00471f57..0x00471f5c (not reachable here)
                else if (c0 == 0xf) TT(ix) = 0xf;                                     // 0x00471f65..0x00471f6a (not reachable here)
                else if (c0 == 0xa) TT(ix) = 4;                                       // 0x00471f73..0x00471f78
                ++done;                                                               // 0x00471f7f
            } while (done < 0x10);                                                    // 0x00471f80..0x00471f83
            break;
        }
        default:                                                                      // 3 (0x00471f89)
            break;
        }
    }

    // ---- L: one object per [0x00822c88]
    {
        int p4 = 0;                                                                   // 0x00471f8e
        if (G32(0x00822c88) > 0) {                                                    // 0x00471f89 / 0x00471f96 / jle 0x004720b5
            do {                                                                      // 0x00471f9e
                int s, d, ix;
                do {
                    s = (Rnd(0x27) & 0xffff) + 5;                                     // 0x00471f9e..0x00471fba
                    d = (Rnd(0x27) & 0xffff) + 5;                                     // 0x00471fad..0x00471fd2
                    ix = d + 50 * s;                                                  // 0x00471fc5..0x00471fd8
                } while (static_cast<signed char>(TT(ix)) != s10);                    // 0x00471fdb..0x00471fe4
                const int oi = kStampCourseTiles(s, d, 4, 0);                         // 0x00471fe6..0x00471fec
                const int ct4 = static_cast<signed char>(G8(0x005a34e0));             // 0x00471ff6
                if (static_cast<unsigned>(ct4) <= 3) {                                // 0x00471ffd / ja 0x0047204b
                    int v;
                    if (ct4 == 0) v = 0x10;                                           // 0x00472009
                    else if (ct4 == 1) v = 0x12;                                      // 0x0047201a
                    else v = 0x11;                                                    // 0x0047203c (course type 2), 0x0047202b (3)
                    G32(0x0058bcc0 + (oi << 4)) = v;
                }
                const int rv = Rnd(4);                                                // 0x0047204b..0x00472052
                TW(ix) = static_cast<unsigned short>(TW(ix) | 0x8000);                // 0x00472057
                G8(0x0058bcbe + (oi << 4)) = static_cast<unsigned char>(rv);          // 0x0047206c
                if (G8(0x005a34e0) == 1) {                                            // 0x00472072
                    for (int k = 0; k < 8; ++k)                                       // 0x00472077..0x0047209c
                        TT((Dx(k) + s) * 50 + Dy(k) + d) = 0xb;                       // 0x0047208e
                }
                ++p4;                                                                 // 0x0047209e..0x004720ab
            } while (p4 < G32(0x00822c88));                                           // jl 0x00471f9e
        }
    }

    // ---- M: walker, border sweeps
    {
        const int w = kSpawnWalker(-6);                                               // 0x004720b5 / 0x004720b7
        const int off = w * 19 * 4;                                                   // 0x004720c2..0x004720d3
        const unsigned char dl = G8(0x00578150);                                      // 0x004720bc
        const unsigned char cl = G8(0x00578154);                                      // 0x004720cd
        G8(0x00585860 + off) = dl;                                                    // 0x004720d6
        G8(0x00585861 + off) = cl;                                                    // 0x004720e0
        int count = 0;                                                                // ebp
        const int e5 = G8(0x00571ff5 + rec) * 10;                                     // 0x004720e6..0x004720f6
        int ecx = 0x19;
        int esiv;
        do {                                                                          // 0x004720f8
            ++count;
            --ecx;
            esiv = ecx * ecx * 4;                                                     // 0x004720fa..0x004720ff
        } while (esiv > e5);                                                          // 0x00472102 / jg 0x004720f8
        const unsigned char kind = G8(0x00571ff7 + rec);                              // 0x00472106
        for (int k = 0; k < 2500; ++k) G8(0x00542414 + k) = TT(k);                    // 0x0047210c..0x0047211d (rep movsd)
        if (kind == 2) {                                                              // 0x0047211b / jne 0x004721aa
            count -= 0xc;                                                             // 0x00472127
            for (int edi = 0; edi < 0x32; ++edi) {                                    // 0x00472125 / 0x0047212e..0x00472196
                const int bx = Rnd(3) & 0xffff;                                       // 0x0047212e..0x0047214a
                const int cl2 = kClamp(Abs(edi - 0x19), 8, 0x32);                     // 0x0047213d..0x00472150
                const int lim = cl2 + count + bx;                                     // 0x00472159..0x00472160
                int ebx = 0;
                if (lim > 0) {                                                        // 0x00472164 / jle 0x00472192
                    int ebp2 = 0x31;                                                  // 0x00472168
                    do {                                                              // 0x0047216d
                        kClearTile(ebx, edi);                                         // 0x0047216d..0x0047216f
                        kClearTile(edi, ebx);                                         // 0x00472174..0x00472176
                        kClearTile(ebp2, edi);                                        // 0x0047217b..0x0047217d
                        kClearTile(edi, ebp2);                                        // 0x00472182..0x00472184
                        ++ebx; --ebp2;                                                // 0x0047218c / 0x0047218d
                    } while (ebx < lim);                                              // 0x0047218e / jl 0x0047216d
                }
            }
            kMarkTileKind11();                                                        // 0x00472198
            return kRebuildHeightfield();                                             // 0x0047219d
        }
        if (count > 0) {                                                              // 0x004721ac / jle 0x004721f6
            int ebx = 0x31;                                                           // 0x004721b0
            for (int edi = 0; edi < count; ++edi, --ebx) {                            // 0x004721aa / 0x004721b5..0x004721f4
                for (int esi = 0; esi < 0x32; ++esi) {                                // 0x004721b5 / 0x004721b7..0x004721ee
                    kClearTile(edi, esi);                                             // 0x004721b7..0x004721b9
                    if (G8(0x00571ff7 + rec) == 0) kClearTile(esi, edi);              // 0x004721be..0x004721d1
                    kClearTile(esi, ebx);                                             // 0x004721d9..0x004721db
                    kClearTile(ebx, esi);                                             // 0x004721e0..0x004721e2
                }
            }
        }
        kMarkTileKind11();                                                            // 0x004721f6
        return kRebuildHeightfield();                                                 // 0x004721fb
    }
}
C1_t ApplyTerrainEdit_orig;
// 0x00470a60
SG_HOOK("golf_clean.exe", 0x00470a60, applyTerrainEdit, ApplyTerrainEdit_re, ApplyTerrainEdit_orig);
