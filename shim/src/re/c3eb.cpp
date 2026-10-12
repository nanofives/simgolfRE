// C3 batch c3eb (simulation plan phase 3, 2026-10-11) of golf_clean.exe: the movement functions pathStep 0x0042e7e0,
// updateWalkers 0x00402a40 and placeAnimal 0x004017d0. Fixtures: re/frida/js/fixtures.d/c3eb.js. Registry:
// re/frida/registry.d/c3eb.py. Purposes and per-jcc tables: log/c3/c3eb_purpose.md.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and cites
// the instructions it reproduces. __thiscall is emulated with __fastcall (ecx = this, edx unused). Callees run through their
// original addresses.
#include "hooks.h"

namespace {

template <typename T> static inline T& G(unsigned addr) { return *reinterpret_cast<T*>(addr); }
static inline char* B(void* p) { return reinterpret_cast<char*>(p); }
template <typename T> static inline T& At(void* p, unsigned off) { return *reinterpret_cast<T*>(B(p) + off); }

typedef int(__cdecl* C0_t)();
typedef int(__cdecl* C1_t)(int a);
typedef int(__cdecl* C2_t)(int a, int b);
typedef int(__cdecl* C3_t)(int a, int b, int c);
typedef int(__cdecl* C4_t)(int a, int b, int c, int d);
typedef int(__cdecl* C5_t)(int a, int b, int c, int d, int e);
typedef int(__cdecl* C8_t)(int a, int b, int c, int d, int e, int f, int g, int h);
typedef int(__fastcall* T1_t)(void* ecx, void* edx, int a);

const C2_t kDirection8 = reinterpret_cast<C2_t>(0x004671a0);       // direction8(dx, dy)
const C3_t kClamp = reinterpret_cast<C3_t>(0x00467130);            // clamp(v, lo, hi)
const C2_t kDistance = reinterpret_cast<C2_t>(0x0040acd0);         // distance(dx, dy)
const C2_t kAngleFixed = reinterpret_cast<C2_t>(0x004672d0);       // angleFixed(dx, dy)
const C2_t kApproxDistance = reinterpret_cast<C2_t>(0x00467170);   // approxDistance(dx, dy)
const C2_t kDirectionOf = reinterpret_cast<C2_t>(0x0042e7a0);      // directionOf(dx, dy)
const C2_t kTileBlocked = reinterpret_cast<C2_t>(0x0040bf60);      // tileBlocked(x, y)
const C2_t kTypeAtPos = reinterpret_cast<C2_t>(0x0040bfa0);        // typeAtPos(x, y)
const C4_t kTileDistance = reinterpret_cast<C4_t>(0x0040c4b0);     // tileDistance(x1, y1, x2, y2)
const C4_t kTileToScreen = reinterpret_cast<C4_t>(0x0042f940);     // tileToScreen(x, y, &sx, &sy)
const C4_t kDrawCenteredText = reinterpret_cast<C4_t>(0x00404b70); // drawCenteredText(text, x, y, color)
const C3_t kItoa = reinterpret_cast<C3_t>(0x004ad425);             // __itoa(value, buffer, radix)
const C1_t kPumpAndRedraw = reinterpret_cast<C1_t>(0x0045c0c0);    // pumpAndRedraw(limit)
const T1_t kRefreshFull = reinterpret_cast<T1_t>(0x00480c80);      // Window::refreshFull(this = 0x00519a60, 0)
const T1_t kRandomRange = reinterpret_cast<T1_t>(0x0045c1e0);      // Random::range(this = 0x00822d9c, n)
const C1_t kThoughtFlag = reinterpret_cast<C1_t>(0x004675d0);      // thoughtFlag(who)
const C3_t kEmitGolferThought = reinterpret_cast<C3_t>(0x00467a00); // emitGolferThought(who, code, flag)
const C4_t kPointsPopup = reinterpret_cast<C4_t>(0x0040c890);      // pointsPopup(n, x, y, z)
const C4_t kPlaySoundAt = reinterpret_cast<C4_t>(0x0040c500);      // playSoundAt(snd, x, y, vol)
const C2_t kFreeAtTile = reinterpret_cast<C2_t>(0x00402930);       // freeAtTile(x, y)
const C5_t kPathStep = reinterpret_cast<C5_t>(0x0042e7e0);         // pathStep(tx, ty, x, y, i)
const C3_t kCellAt = reinterpret_cast<C3_t>(0x0043d6f0);           // cellAt(sprite, frame, dir)
const C8_t kEnqueueSprite = reinterpret_cast<C8_t>(0x00462a30);    // enqueueSprite(sx, sy, sy, w, h, cell, 4, tag)
typedef int(__cdecl* WorldToScreen_t)(int wx, int wy, int* sx, int* sy, int margin);
const WorldToScreen_t kWorldToScreen = reinterpret_cast<WorldToScreen_t>(0x0042fb90);

static inline int RandRange(int n) { return kRandomRange(reinterpret_cast<void*>(0x00822d9c), 0, n); }
static inline const int* Dx8() { return reinterpret_cast<const int*>(0x004c2878); }
static inline const int* Dy8() { return reinterpret_cast<const int*>(0x004c2898); }

const unsigned kGrid = 0x0058d370;       // 50 x 50 byte distance grid, cleared by pathStep (0x271 dwords)
const unsigned kQueueX = 0x005a7404;     // 0x400 shorts
const unsigned kQueueY = 0x005a8028;     // 0x400 shorts
const unsigned kFlagsWord = 0x0059e7b8;  // g_flags
const unsigned kTileFlags = 0x0053caf0;  // word per tile (50 x 50)
const unsigned kTileType = 0x005722e8;   // byte per tile (g_tile_type)
const unsigned kTileCost = 0x0053bbac;   // signed byte per tile
const unsigned kTypeAttr = 0x00578376;   // 0x30-byte records, byte +0x378376
const unsigned kGolfer = 0x005794b8;     // golfer records, 0x100 stride: +0 x, +4 y

// copies a NUL-terminated string (including the NUL) to dst, as the strlen / rep movs sequences do after an emptied buffer
static inline void CopyStr(char* dst, const char* src) {
    while ((*dst++ = *src++) != 0) {}
}
static inline void AppendStr(char* dst, const char* src) {
    while (*dst) dst++;
    CopyStr(dst, src);
}

}  // namespace

// ====================================================================================================================
// pathStep
// ====================================================================================================================

// 0x0042e7e0  pathStep(p1, p2, p3, p4, p5): cdecl, plain `ret`, 5 stack arguments, returns the direction in eax. p1/p2 are the
// target world position and p3/p4 the walker's, as updateWalkers calls it (0x004035c8: pushes edi = target x last, ebp = walker
// index first); p5 selects the record in the golfer table whose partner (p5 ^ 1) is avoided. The 50 x 50 byte grid at 0x0058d370
// is cleared (rep stosd of 0x271 dwords, 0x0042e7f3..0x0042e7ff), the tile coordinates are clamped to 0..49 (clamp calls
// 0x0042e820..0x0042e89d; they overwrite the argument slots), and a breadth-first wave spreads from the start tile through the
// queue 0x005a7404 / 0x005a8028 (0x400 entries each) with the cost rules at 0x0042ea08..0x0042eb3f; a second pass at 0x0042ec37
// picks, from a random start direction, the neighbour of the target with the lowest wave cost.
int __cdecl PathStep_re(int p1, int p2, int p3, int p4, int p5) {
    unsigned char* grid = reinterpret_cast<unsigned char*>(kGrid);
    for (unsigned i = 0; i < 0x271; i++) G<int>(kGrid + 4 * i) = 0;              // 0x0042e7f3..0x0042e7ff
    int dir0 = kDirection8(p1 - p3, p2 - p4);                                    // 0x0042e80f (result kept at [esp+0x20])
    int sx = kClamp(p1 >> 10, 0, 0x31);                                          // 0x0042e81b..0x0042e820
    int sy = kClamp(p2 >> 10, 0, 0x31);                                          // 0x0042e829..0x0042e833
    int tx = kClamp(p3 >> 10, 0, 0x31);                                          // 0x0042e83c..0x0042e844
    int ty = kClamp(p4 >> 10, 0, 0x31);                                          // 0x0042e84d..0x0042e858
    unsigned rec = static_cast<unsigned>(p5 ^ 1) << 8;                           // 0x0042e866 / 0x0042e86b
    int ox = kClamp(G<int>(kGolfer + rec) >> 10, 0, 0x31);                       // 0x0042e877..0x0042e881
    int oy = kClamp(G<int>(kGolfer + 4 + rec) >> 10, 0, 0x31);                   // 0x0042e88b..0x0042e89d
    if (ox == sx && oy == sy) ox = -1;                                           // 0x0042e8a9..0x0042e8b5
    if (sx == tx && sy == ty) return dir0;                                       // 0x0042e8bd..0x0042e8c7
    int ex = sx - tx;                                                            // 0x0042e8db (ebx)
    int ey = sy - ty;                                                            // 0x0042e8d9 (edi)
    int dist = kDistance(ex, ey);                                                // 0x0042e8e9 (kept at [esp+0x30])
    int ang = kAngleFixed(ex, ey);                                               // 0x0042e8f4
    int a = ((((ang >> 28) + 1) ^ static_cast<int>(0xfffffff9)) >> 1) & 7;       // 0x0042e8f9..0x0042e90a
    G<short>(kQueueX) = static_cast<short>(sx);                                  // 0x0042e90d
    G<short>(kQueueY) = static_cast<short>(sy);                                  // 0x0042e918
    int tail = 1;                                                                // 0x0042e922
    grid[sx * 50 + sy] = 1;                                                      // 0x0042e93f
    unsigned mask = (1u << ((a + 1) & 6)) | (1u << (a & 6));                     // 0x0042e91f..0x0042e950
    int limit = ((G<unsigned>(kFlagsWord) & 0x100) ? 0x5a : 0) + 0xa0;           // 0x0042e947..0x0042e968
    int head = 0;                                                                // 0x0042e955
    do {
        int x = G<short>(kQueueX + 2 * head);                                    // 0x0042e972
        int y = G<short>(kQueueY + 2 * head);                                    // 0x0042e97a
        head = (head + 1) & 0x3ff;                                               // 0x0042e982 / 0x0042e987
        int flat = y + x * 50;                                                   // 0x0042e994..0x0042e99a
        int cur = grid[flat];                                                    // 0x0042e9a4
        if (!(cur > limit)) {                                                    // 0x0042e9ac / 0x0042e9b2
            for (int k = 0; k < 8; k++) {                                        // 0x0042eb47..0x0042eb4b
                int nx = Dx8()[k] + x;                                           // 0x0042e9c4..0x0042e9cb
                int ny = Dy8()[k] + y;                                           // 0x0042e9cd..0x0042e9d4
                if (nx < 0 || ny < 0 || nx >= 0x32 || ny >= 0x32) continue;      // 0x0042e9d6..0x0042e9f2
                int flat2 = ny + nx * 50;                                        // 0x0042e9f8..0x0042ea05
                int par = k & 1;                                                 // 0x0042e9ff
                int cost = static_cast<signed char>(G<char>(kTileCost + flat2)) + par;   // 0x0042ea08 / 0x0042ea15
                int ndist = grid[flat2];                                         // 0x0042ea0f
                if (nx == ox && ny == oy) cost += 2;                             // 0x0042ea17..0x0042ea27
                if (par == 0) {                                                  // 0x0042ea2a / 0x0042ea2c
                    unsigned date = G<unsigned>(0x00834170);                     // 0x0042ea2e
                    int v = static_cast<int>(((~date & 0x40u) | 0x80u) >> 6);    // 0x0042ea34..0x0042ea3c
                    if (dist > v) {                                              // 0x0042ea3f / 0x0042ea43
                        if ((G<unsigned short>(kTileFlags + 2 * flat2) & 0x420) == 0x20 &&   // 0x0042ea45..0x0042ea57
                            (G<unsigned char>(kTileFlags + 2 * flat) & 0x20) != 0) {         // 0x0042ea5d / 0x0042ea65
                            int r = kDirectionOf(nx - sx, ny - sy);              // 0x0042ea79
                            cost = (k != r) ? 1 : 0;                             // 0x0042ea87..0x0042ea8c
                        }
                    }
                }
                if ((G<unsigned>(kFlagsWord) & 0x100) != 0 ||                    // 0x0042ea8e..0x0042ea97
                    G<unsigned char>(kGolfer + 0x21 + (static_cast<unsigned>(p5) << 8)) == 0x13) {   // 0x0042ea99..0x0042eaa7
                    int t = static_cast<signed char>(G<char>(kTileType + flat2));    // 0x0042eaa9
                    if (t == 1 || t == 2 || t == 0 ||                            // 0x0042eab0..0x0042eabc
                        G<unsigned char>(kTypeAttr + t * 0x30) == 7) {           // 0x0042eabe..0x0042eacb
                        cost += 1;                                               // 0x0042ead2
                    } else {
                        cost = (cost + 1) >> 1;                                  // 0x0042eacd..0x0042ead0
                    }
                }
                unsigned char tt = G<unsigned char>(kTileType + flat2);          // 0x0042ead3
                if ((tt == 0x11 || tt == 0x14) &&                                // 0x0042ead9..0x0042eae1
                    (G<unsigned char>(kTileFlags + 2 * flat2) & 0x20) == 0 &&    // 0x0042eae3..0x0042eaeb
                    cur < 0x40) {                                                // 0x0042eaed / 0x0042eaf2
                    cost += 0x10;                                                // 0x0042eaf4
                }
                int total = cost + cur;                                          // 0x0042eaf7 / 0x0042eafb
                if (ndist != 0 && ndist <= total) continue;                      // 0x0042eafd..0x0042eb03
                if (total > 0xff) continue;                                      // 0x0042eb05 / 0x0042eb0a
                grid[flat2] = static_cast<unsigned char>(total);                 // 0x0042eb10
                G<short>(kQueueX + 2 * tail) = static_cast<short>(nx);           // 0x0042eb16
                G<short>(kQueueY + 2 * tail) = static_cast<short>(ny);           // 0x0042eb1e
                tail = (tail + 1) & 0x3ff;                                       // 0x0042eb26 / 0x0042eb27
                if (nx == tx && ny == ty) limit = total;                         // 0x0042eb31..0x0042eb3f
            }
        }
    } while (head != tail && G<int>(0x0083afcc) == 0);                           // 0x0042eb59..0x0042eb64
    if ((G<unsigned>(kFlagsWord) & 0x1000) != 0) {                               // 0x0042eb6a / 0x0042eb6f
        for (int gx = 0; gx < 0x32; gx++) {                                      // 0x0042ec10..0x0042ec1b
            for (int gy = 0; gy < 0x32; gy++) {                                  // 0x0042ec03..0x0042ec07
                unsigned char v = grid[gx * 50 + gy];                            // 0x0042eb85
                if (v == 0) continue;                                            // 0x0042eb88 / 0x0042eb8a
                G<char>(0x0051a068) = 0;                                         // 0x0042eb99
                const char* s = reinterpret_cast<const char*>(kItoa(v, 0x0058a528, 10));   // 0x0042eba0
                AppendStr(reinterpret_cast<char*>(0x0051a068), s);               // 0x0042eba5..0x0042ebd2 (strcat)
                int outA = 0, outB = 0;
                kTileToScreen(gx, gy, reinterpret_cast<int>(&outA), reinterpret_cast<int>(&outB));   // 0x0042ebdb
                kDrawCenteredText(0x0051a068, outA, outB - 3, static_cast<int>(0x80007fff));        // 0x0042ebf7
            }
        }
        kRefreshFull(reinterpret_cast<void*>(0x00519a60), 0, 0);                 // 0x0042ec28
        kPumpAndRedraw(0);                                                       // 0x0042ec2f
    }
    int targetFlat = ty + tx * 50;                                               // 0x0042ec44..0x0042ec4b
    int best = -1;                                                               // 0x0042ec41
    int bestCost = grid[targetFlat] * 2 + 1;                                     // 0x0042ec54 / 0x0042ec5a
    int rnd = RandRange(8) & 0xffff;                                             // 0x0042ec67 / 0x0042ec6c
    for (int n = 0; n < 8; n++) {                                                // 0x0042ed17..0x0042ed23
        int k = (n + rnd) & 7;                                                   // 0x0042ec85 / 0x0042ec8b
        int nx = Dx8()[k] + tx;                                                  // 0x0042ec8e / 0x0042ec95
        int ny = Dy8()[k] + ty;                                                  // 0x0042ec97 / 0x0042ec9e
        if (nx < 0 || ny < 0 || nx >= 0x32 || ny >= 0x32) continue;              // 0x0042eca0..0x0042ecb0
        if (kTileBlocked(nx, ny) != 0) continue;                                 // 0x0042ecb4..0x0042ecbe
        int d = grid[ny + nx * 50];                                              // 0x0042ecc8
        if (d == 0) continue;                                                    // 0x0042eccf / 0x0042ecd1
        int par = k & 1;                                                         // 0x0042ecd5
        int cand = par + d * 2;                                                  // 0x0042ecd8
        if (G<unsigned char>(kTileType + targetFlat) == 0x11 &&                  // 0x0042ecdf / 0x0042ece6
            (G<unsigned char>(kTileFlags + 2 * targetFlat) & 0x20) != 0 &&       // 0x0042ece8 / 0x0042ecf0
            par != 0) continue;                                                  // 0x0042ecf2 / 0x0042ecf4
        if ((mask & (1u << (k ^ 4))) != 0) cand -= 1;                            // 0x0042ecf6..0x0042ed0a
        if (cand < bestCost) {                                                   // 0x0042ed0b / 0x0042ed0f
            best = k;                                                            // 0x0042ed11
            bestCost = cand;                                                     // 0x0042ed13
        }
    }
    if ((G<unsigned>(kFlagsWord) & 0x100) == 0 &&                                // 0x0042ed29..0x0042ed31
        G<unsigned char>(kTileType + targetFlat) == 0x11 &&                      // 0x0042ed3b / 0x0042ed42
        (G<unsigned char>(kTileFlags + 2 * targetFlat) & 0x20) != 0) {           // 0x0042ed48 / 0x0042ed50
        unsigned prec = static_cast<unsigned>(p5) << 8;                          // 0x0042ed5e
        int gdx = G<int>(kGolfer + prec) - (tx << 10) - 0x200;                   // 0x0042ed64..0x0042ed7b
        int gdy = G<int>(kGolfer + 4 + prec) - (ty << 10) - 0x200;               // 0x0042ed6a..0x0042ed81
        if (static_cast<unsigned>(best) <= 6) {                                  // 0x0042ed87..0x0042ed8c
            int orig = best;                                                     // 0x0042ed8a
            bool apply = true;
            switch (best) {                                                      // 0x0042ed92: table 0x0042ee64
            case 0: {                                                            // 0x0042ed99
                int s = gdx >> 31;
                if (((gdx ^ s) - s) <= 0x100) return best;                       // 0x0042ed9b..0x0042eda5
                best = ((gdx <= 0 ? 0 : -1) & -2) + 1;                           // 0x0042edab..0x0042edb6
                break;
            }
            case 2: {                                                            // 0x0042edb9
                int s = gdy >> 31;
                if (((gdy ^ s) - s) <= 0x100) return best;                       // 0x0042edb9..0x0042edc5
                best = ((gdy <= 0 ? 0 : -1) & -2) + 3;                           // 0x0042edcb..0x0042edd6
                break;
            }
            case 4: {                                                            // 0x0042eddb
                int s = gdx >> 31;
                if (((gdx ^ s) - s) <= 0x100) return best;                       // 0x0042eddb..0x0042ede7
                best = ((gdx <= 0 ? 0 : -1) & 2) - 5;                            // 0x0042ede9..0x0042edf4
                break;
            }
            case 6: {                                                            // 0x0042edf9
                int s = gdy >> 31;
                if (((gdy ^ s) - s) <= 0x100) return best;                       // 0x0042edf9..0x0042ee05
                best = ((gdy <= 0 ? 0 : -1) & 2) - 3;                            // 0x0042ee07..0x0042ee12
                break;
            }
            default:
                apply = false;                                                   // 0x0042ee26 (cases 1, 3, 5)
                break;
            }
            if (apply) {
                best &= 7;                                                       // 0x0042ee15
                if (best != orig) G<unsigned>(kFlagsWord) |= 0x80000;            // 0x0042ee18..0x0042ee1c
            }
        }
    }
    if (best == -1 && (sx != tx || sy != ty)) best = kDirection8(sx - tx, sy - ty);   // 0x0042ee26..0x0042ee4d
    return best;                                                                 // 0x0042ee55 / 0x0042ee59
}
C5_t PathStep_orig;
// 0x0042e7e0
SG_HOOK("golf_clean.exe", 0x0042e7e0, pathStep, PathStep_re, PathStep_orig);

// ====================================================================================================================
// updateWalkers
// ====================================================================================================================

namespace {
const unsigned kWalkers = 0x00585850;  // 64 records of 0x4c bytes
// golfer record fields (stride 0x100 from 0x005794b8): +0 x, +4 y, +0x10 (0x5794c8) flags, +0x21 (0x5794d9), +0x22 (0x5794da)
const unsigned kSel = 0x005a59f8;      // selected golfer index (-1 = none)
}  // namespace

// 0x00402a40  updateWalkers(): cdecl, `ret`, no arguments; leaves eax = 0x40 (the loop counter, 0x004038bb..0x004038bf).
// Loops over the 64 walker records at 0x00585850 (stride 0x4c). Two stack slots keep their value from one record to the next in
// the original ([esp+0x30] and [esp+0x28]: the distance pair of the follow arm 0x00402c4b / 0x00402c51, the loop counters and
// the follow arm of 0x0040317d); the flagged-walker arm at 0x00403661 reads them without writing them, so s30 / s28 below are
// function-level variables written at the same places.
int __cdecl UpdateWalkers_re() {
    int s30 = 0, s28 = 0;
    for (int i = 0; i < 0x40;) {                                                 // 0x00402a49 / 0x004038bb..0x004038c3
        char* w = reinterpret_cast<char*>(kWalkers + i * 0x4c);                  // 0x00402a53..0x00402a59
        int curIdx = i;
        i++;
        signed char mode = At<signed char>(w, 0x13);                             // 0x00402a5c (al)
        int tx = 0, ty = 0;                                                      // [esp+0x10] / [esp+0x14] (and edi / ebx)
        if (mode >= 0 && G<int>(kSel) == -1) At<unsigned char>(w, 0x12) = 0;     // 0x00402a62..0x00402a6f
        if (At<unsigned char>(w, 0x12) == 0) continue;                           // 0x00402a76..0x00402a7e
        // ---- the timer block
        if (At<unsigned char>(w, 0x15) != 0 && (G<unsigned>(0x00834170) & 3) == 0) {   // 0x00402a84..0x00402a99
            unsigned char t = static_cast<unsigned char>(At<unsigned char>(w, 0x15) - 1);   // 0x00402a9f
            At<unsigned char>(w, 0x15) = t;                                      // 0x00402aa3
            if (t == 4 && At<unsigned char>(w, 0x14) == 0xfe) {                  // 0x00402aa1 / 0x00402aa9..0x00402ab2
                int who = At<short>(w, 0x20);                                    // 0x00402abd / 0x00402afe
                bool closeBy = false;
                if ((At<unsigned char>(w, 0x12) & 8) == 0) {                     // 0x00402ab4 / 0x00402abb
                    unsigned rec = static_cast<unsigned>(who) << 8;              // 0x00402acb
                    if (!(G<short>(0x00579560 + rec) > 0x10) &&                  // 0x00402ace / 0x00402ad5
                        !(G<short>(0x0057955e + rec) > 0x10) &&                  // 0x00402ad7 / 0x00402ade
                        !(G<short>(0x00579562 + rec) > 0xa0)) closeBy = true;       // 0x00402ae0 / 0x00402ae9
                } else {
                    closeBy = true;                                                 // 0x00402abb -> 0x00402afe
                }
                if (closeBy) {
                    int f = kThoughtFlag(At<short>(w, 0x20));                    // 0x00402afe..0x00402b06
                    kEmitGolferThought(At<short>(w, 0x20), 0x22, f);             // 0x00402b0c..0x00402b16
                } else {
                    int f = kThoughtFlag(who);                                   // 0x00402aeb / 0x00402aec
                    kEmitGolferThought(At<short>(w, 0x20), 0x3a, f);             // 0x00402af1..0x00402b16
                }
            }
            if (At<unsigned char>(w, 0x15) == 4 && At<unsigned char>(w, 0x14) == 0xfb) {   // 0x00402b1e..0x00402b32
                if ((At<unsigned char>(w, 0x12) & 8) != 0)                       // 0x00402b38 / 0x00402b3f
                    G<short>(0x00579560 + (static_cast<unsigned>(At<short>(w, 0x20)) << 8)) = 0x63;   // 0x00402b41..0x00402b4b
                kEmitGolferThought(At<short>(w, 0x20), 0x19, 0x14);              // 0x00402b54..0x00402b60
                int y = At<int>(w, 4);                                           // 0x00402b6b
                G<int>(0x00571fd4) = G<int>(0x00571fd4) + 2;                     // 0x00402b65..0x00402b82
                int x = At<int>(w, 0);                                           // 0x00402b88
                G<short>(0x00579560 + (static_cast<unsigned>(At<short>(w, 0x20)) << 8)) = 0;   // 0x00402b71 / 0x00402b93
                kPointsPopup(2, x, y, -1);                                       // 0x00402b9c
                int rem = static_cast<int>(G<short>(0x005a6d3c)) % 100;          // 0x00402ba1..0x00402bb1
                G<short>(0x00584214 + rem * 20) = static_cast<short>(G<short>(0x00584214 + rem * 20) + 2);   // 0x00402bb3..0x00402bb6
            }
        }
        // ---- wait
        if (At<short>(w, 0x1a) != 0) {                                           // 0x00402bbe..0x00402bc8
            At<short>(w, 0x1a) = static_cast<short>(At<short>(w, 0x1a) - 1);     // 0x00402bca / 0x00402bcb
            continue;                                                            // 0x00402bd2
        }
        mode = At<signed char>(w, 0x13);                                         // 0x00402bd7
        bool toMove = false;                                                     // reaches 0x00403503
        if (mode > 0) {                                                          // 0x00402bdd / 0x00402bdf
            unsigned srec = static_cast<unsigned>(G<int>(kSel)) << 8;            // 0x00402be5 / 0x00402bea
            int gx = G<int>(0x0057958c + srec);                                  // 0x00402bed
            int gy = G<int>(0x00579590 + srec);                                  // 0x00402bf3
            if (gx == 0) {                                                       // 0x00402bf9 / 0x00402bfb
                gx = G<int>(kGolfer + srec);                                     // 0x00402bfd
                gy = G<int>(kGolfer + 4 + srec);                                 // 0x00402c03
            } else {
                gx += Dx8()[curIdx & 7] << 10;                                   // 0x00402c0f..0x00402c28
                gy += Dy8()[curIdx & 7] << 10;                                   // 0x00402c1f / 0x00402c2a..0x00402c31
            }
            tx = gx;                                                             // 0x00402c2d / 0x00402c09
            ty = gy;                                                             // 0x00402c41
            int ex = gx - At<int>(w, 0);                                         // 0x00402c33 / 0x00402c45
            int ey = gy - At<int>(w, 4);                                         // 0x00402c39 / 0x00402c49
            s30 = ex;                                                            // 0x00402c4b
            s28 = ey;                                                            // 0x00402c51
            int d = kApproxDistance(ex, ey);                                     // 0x00402c55
            if (d >= 0x800 && !(G<short>(0x00579556 + srec) > 0) &&              // 0x00402c5d..0x00402c74
                (G<unsigned char>(0x005794da + srec) != 0 || G<unsigned char>(0x005794d9 + srec) != 1)) {   // 0x00402c76..0x00402c8b
                toMove = true;                                                   // 0x00403503
            } else {
                int ang = kAngleFixed(ex, s28);                                  // 0x00402c91..0x00402c97
                At<unsigned char>(w, 0x16) = static_cast<unsigned char>((((ang >> 28) & 0xf) + 1) >> 1 & 7);   // 0x00402c9c..0x00402cb1
                At<short>(w, 0x1e) = 0xb;                                        // 0x00402cb7
                At<short>(w, 0x1a) = static_cast<short>(RandRange(0x10) + 1);    // 0x00402cc0..0x00402cc6
                At<short>(w, 0x18) = 0;                                          // 0x00402ccd
                continue;                                                        // 0x00402cd6
            }
        } else {
            // ---- mode <= 0
            if (mode == static_cast<signed char>(0xfa)) {                        // 0x00402cdb / 0x00402cde
                if (G<int>(kSel) != -1) continue;                                // 0x00402ce0 / 0x00402ce7
                if ((G<unsigned>(kFlagsWord) & 0x200000) != 0 && G<unsigned char>(0x005795d9) != 0) continue;   // 0x00402ced..0x00402d00
            }
            unsigned char flags = At<unsigned char>(w, 0x12);                    // 0x00402d14
            if (At<short>(w, 0x18) != 0 || (flags & 0x10) != 0) {                // 0x00402d06..0x00402d1d
                toMove = true;                                                   // 0x004034fb
            } else {
                int x = At<int>(w, 0), y = At<int>(w, 4);                        // 0x00402d29 / 0x00402d2f
                int best = 0xfffff;                                              // 0x00402d3b ([esp+0x30])
                s30 = best;
                int bestIdx = -1;                                                // 0x00402d43 ([esp+0x18])
                tx = x;                                                          // 0x00402d47
                ty = y;                                                          // 0x00402d4b
                if (At<unsigned char>(w, 0x10) != 0xff) {                        // 0x00402d38 / 0x00402d4f
                    tx = (static_cast<int>(At<signed char>(w, 0x10)) << 10) + 0x200;   // 0x00402d51..0x00402d5d
                    ty = (static_cast<int>(At<signed char>(w, 0x11)) << 10) + 0x200;   // 0x00402d61..0x00402d71
                }
                int kind = -1;                                                   // 0x00402d78 ([esp+0x20])
                int selIdx = -1;                                                 // 0x00402d7c ([esp+0x24])
                int selGy = 0;                                                   // [esp+0x3c]
                if (mode == static_cast<signed char>(0xfc) || mode == static_cast<signed char>(0xfa)) {   // 0x00402d75..0x00402d85
                    int lim = (flags & 8) ? 0x20 : 0x18;                         // 0x00402d8b..0x00402d95
                    int px = x >> 10, py = y >> 10;                              // 0x00402d9a / 0x00402d9d
                    for (int a = -16; a <= 16; a++) {                            // 0x00402da8..0x00402e88
                        for (int b = -16; b <= 16; b++) {                        // 0x00402db0..0x00402e76
                            int gx = At<signed char>(w, 0x10) + a;               // 0x00402db8..0x00402dc7
                            int gy = At<signed char>(w, 0x11) + b;               // 0x00402dc9..0x00402dd0
                            if (kTileBlocked(gx, gy) != 0) continue;             // 0x00402dd4..0x00402dde
                            if ((G<unsigned short>(kTileFlags + 2 * (gy + gx * 50)) & 0x800) == 0) continue;   // 0x00402dee..0x00402dff
                            int dd = kApproxDistance(gx * 2 - At<signed char>(w, 0x10) - px,
                                                     gy * 2 - At<signed char>(w, 0x11) - py);   // 0x00402e01..0x00402e28
                            if (dd >= lim) continue;                             // 0x00402e30 / 0x00402e32
                            lim = dd;                                            // 0x00402e34
                            selIdx = gx;                                         // 0x00402e40
                            tx = (gx << 10) + 0x200;                             // 0x00402e36..0x00402e44
                            selGy = gy;                                          // 0x00402e52
                            ty = (gy << 10) + 0x200;                             // 0x00402e48..0x00402e56
                            bestIdx = 1;                                         // 0x00402e5a
                            kind = -4;                                           // 0x00402e62
                        }
                    }
                    s28 = 17;                                                    // the counter after the last pass of 0x00402e72
                    if (bestIdx == -1 && (flags & 8) != 0) {                     // 0x00402e8e..0x00402ea0
                        int lim2 = 0x1400;                                       // 0x00402ea6
                        int e = 0;                                               // 0x00402eab
                        for (unsigned p = 0x005736b0; p < 0x00575ab0; p += 0x24, e++) {   // 0x00402ead / 0x00402f1e..0x00402f28
                            if (G<int>(p + 8) == -1) continue;                   // 0x00402eb2 / 0x00402eb6
                            int qy = G<int>(p + 4);                              // 0x00402eb8
                            int wx = At<int>(w, 0);                              // 0x00402ebb
                            int a2 = qy + qy - 0x200 - (At<signed char>(w, 0x11) << 10) - wx;   // 0x00402ec1..0x00402ed4
                            int qx = G<int>(p);                                  // 0x00402ed7
                            int a1 = qx + qx - 0x200 - (At<signed char>(w, 0x10) << 10) - wx;   // 0x00402ed9..0x00402eec
                            int dd = kApproxDistance(a1, a2);                    // 0x00402eef
                            if (dd >= lim2) continue;                            // 0x00402ef7 / 0x00402ef9
                            lim2 = dd;                                           // 0x00402efe
                            ty = G<int>(p + 4);                                  // 0x00402efb / 0x00402f02
                            tx = G<int>(p);                                      // 0x00402f00 / 0x00402f06
                            selIdx = e;                                          // 0x00402f0a
                            bestIdx = 2;                                         // 0x00402f0e
                            kind = -4;                                           // 0x00402f16
                        }
                    }
                }
                signed char m2 = At<signed char>(w, 0x13);                       // 0x00402f2a
                bool search = true;
                if (m2 == static_cast<signed char>(0xfc) || m2 == static_cast<signed char>(0xfa)) {   // 0x00402f30..0x00402f36
                    if (selIdx != -1) search = false;                            // 0x00402f38 / 0x00402f3d
                    else if (m2 != static_cast<signed char>(0xfa)) search = false;   // 0x00402f43 / 0x00402f45
                }
                if (search) {
                    tx = (At<signed char>(w, 0x10) << 10) + 0x200;               // 0x00402f4b..0x00402f62
                    ty = (At<signed char>(w, 0x11) << 10) + 0x200;               // 0x00402f6b..0x00402f7a
                    int gi = 0;                                                  // 0x00402f5a
                    s28 = 0;
                    s30 = best;                                                  // [esp+0x30] still 0xfffff here
                    best = 0xfffff;
                    for (unsigned p = 0x005794bc; p < 0x00582cbc; p += 0x100, gi++) {   // 0x00402f66 / 0x004030a4..0x004030af
                        s28 = gi;
                        int gx = G<int>(p - 4), gy = G<int>(p);                  // 0x00402f8c / 0x00402f8f
                        int dd = kTileDistance(gx, gy, At<signed char>(w, 0x10), At<signed char>(w, 0x11));   // 0x00402f98
                        int kd = -1;                                             // 0x00402f95 (ebx)
                        if (!(G<signed char>(p + 0x1d) <= 0) &&                  // 0x00402f9f..0x00402fa7
                            !(G<signed char>(p + 0x19) < 7) &&                   // 0x00402fad / 0x00402fb1
                            gi != At<short>(w, 0x20) &&                          // 0x00402fb7..0x00402fc2
                            kTypeAtPos(gx, gy) != 0 &&                           // 0x00402fcf..0x00402fd9
                            kTypeAtPos(gx, gy) != 0x16) {                        // 0x00402fe6..0x00402ff1
                            signed char cl = At<signed char>(w, 0x13);           // 0x00402ff7
                            if (cl == static_cast<signed char>(0xfd) || cl == static_cast<signed char>(0xfa)) {   // 0x00403000 / 0x00403005
                                unsigned gflags = G<unsigned>(p + 0xc);          // 0x00403007
                                if ((gflags & 0x20000000) != 0 && (At<unsigned char>(w, 0x12) & 8) != 0) {   // 0x0040300a..0x00403018
                                    dd = (dd + ((dd >> 31) & 7)) >> 3;           // 0x0040301a..0x00403025
                                    kd = -3;                                     // 0x00403034
                                } else if ((gflags & 0x4000) == 0 && !(G<signed char>(p + 0x1e) > 1)) {   // 0x00403029..0x00403032
                                    kd = -3;                                     // 0x00403034
                                }
                            }
                            if ((cl == static_cast<signed char>(0xfe) || cl == static_cast<signed char>(0xfa)) &&   // 0x00403039..0x00403041
                                G<unsigned char>(p + 0x6c) != 0x22 && G<unsigned char>(p + 0x6c) != 0x3a)           // 0x00403043..0x0040304c
                                kd = -2;                                         // 0x0040304e
                            if (cl == static_cast<signed char>(0xfb) || cl == static_cast<signed char>(0xfa)) {   // 0x00403053..0x0040305b
                                int lowv = ((~At<unsigned char>(w, 0x12)) & 8) >> 1;   // 0x0040305d..0x00403068
                                if (G<short>(p + 0xa4) > lowv) kd = -5;          // 0x0040306a / 0x00403071
                            }
                            if ((kd == static_cast<int>(cl) || cl == static_cast<signed char>(0xfa)) && dd < best) {   // 0x00403078..0x00403088
                                best = dd;                                       // 0x0040308e
                                bestIdx = gi;                                    // 0x00403092
                                kind = kd;                                       // 0x00403096
                                s30 = dd;
                            }
                        }
                    }
                    s28 = gi;                                                    // the idx stored at 0x004030af after the last pass
                    if (best < 0x64) {                                           // 0x004030b9..0x004030c0
                        unsigned brec = static_cast<unsigned>(bestIdx) << 8;     // 0x004030c2
                        tx = G<int>(kGolfer + brec);                             // 0x004030c5
                        ty = G<int>(kGolfer + 4 + brec);                         // 0x004030cb
                    } else {
                        if (bestIdx != -1) {                                     // 0x004030db / 0x004030de
                            unsigned brec = static_cast<unsigned>(bestIdx) << 8;   // 0x004030e4..0x004030f0
                            int dyv = G<int>(kGolfer + 4 + brec) - At<int>(w, 4);   // 0x004030f3 / 0x004030f9
                            int dxv = G<int>(kGolfer + brec) - At<int>(w, 0);    // 0x004030fb..0x00403101
                            At<unsigned char>(w, 0x16) = static_cast<unsigned char>(kDirection8(dxv, dyv));   // 0x00403105..0x0040310d
                        }
                        bestIdx = -1;                                            // 0x00403113
                    }
                }
                if (At<signed char>(w, 0x13) == static_cast<signed char>(0xfa) && G<unsigned char>(0x00575cb8) == 0) {   // 0x0040311b..0x0040312b
                    if ((G<unsigned>(0x00834170) & 0x200) == 0 &&                // 0x0040312d..0x00403135
                        G<unsigned char>(kTileType + G<int>(0x00543d0c) + G<int>(0x00543d08) * 50) != 0x11) {   // 0x00403137..0x00403150
                        tx = (G<int>(0x00543d08) << 10) + 0x200;                 // 0x00403152..0x0040315d
                        ty = (G<int>(0x00543d0c) << 10) + 0x200;                 // 0x0040315a..0x00403167
                    } else {
                        tx = At<int>(w, 0);                                      // 0x00403169..0x00403175
                        ty = At<int>(w, 4);                                      // 0x0040316f..0x00403179
                    }
                }
                int ey = ty - At<int>(w, 4);                                     // 0x0040317d..0x00403191
                int ex = tx - At<int>(w, 0);                                     // 0x00403187..0x00403193
                s30 = ex;                                                        // 0x00403197
                s28 = ey;                                                        // 0x0040319b
                if (kApproxDistance(ex, ey) > 0x400) {                           // 0x0040319f..0x004031ac
                    toMove = true;                                               // 0x004034fb
                } else {
                    At<short>(w, 0x1e) = 0xb;                                    // 0x004031b9
                    At<short>(w, 0x1a) = static_cast<short>(RandRange(0x10) + 1);   // 0x004031c2..0x004031ca
                    int d2 = kApproxDistance(ex, ey);                            // 0x004031d1
                    At<short>(w, 0x18) = static_cast<short>(d2 > 0x800 ? 0x200 : 0);   // 0x004031d9..0x004031f2
                    int snd = 0;                                                 // 0x004031e4
                    if ((At<unsigned char>(w, 0x12) & 8) != 0) snd = 4;          // 0x004031f9..0x00403203
                    if (At<signed char>(w, 0x13) == static_cast<signed char>(0xfa) && kind != -2) snd = 8;   // 0x00403208..0x00403218
                    if (bestIdx == -1) {                                         // 0x0040321d..0x00403224
                        At<short>(w, 0x1a) = static_cast<short>(RandRange(8));   // 0x004034e3..0x004034ef
                        continue;                                                // 0x004034f6
                    }
                    int ang = kAngleFixed(ex, ey);                               // 0x00403230
                    At<unsigned char>(w, 0x16) = static_cast<unsigned char>((((ang >> 28) & 0xf) + 1) >> 1 & 7);   // 0x00403235..0x0040324c
                    At<short>(w, 0x1a) = static_cast<short>(At<short>(w, 0x1a) + 0x40);   // 0x00403252
                    At<unsigned char>(w, 0x15) = 7;                              // 0x0040325c
                    At<short>(w, 0x20) = static_cast<short>(bestIdx);            // 0x00403266
                    unsigned sel = static_cast<unsigned>(kind + 5);              // 0x00403259 / 0x00403263 / 0x0040326d
                    bool cnt = false;                                            // reaches 0x004034aa
                    if (sel <= 3) {
                        unsigned brec = static_cast<unsigned>(bestIdx) << 8;
                        switch (sel) {                                           // 0x00403273: table 0x004038d4
                        case 3:                                                  // kind -2: 0x0040327a
                            At<unsigned char>(w, 0x14) = 0xfe;                   // 0x0040327a
                            At<short>(w, 0x1a) = static_cast<short>(At<short>(w, 0x1a) + 0x40);   // 0x00403281
                            kPlaySoundAt(snd + 0x5c, At<int>(w, 0), At<int>(w, 4), 0);   // 0x00403288..0x0040329c
                            At<short>(w, 0x1e) = 0xc;                            // 0x004032af
                            At<short>(w, 0x1c) = 0;                              // 0x004032b8
                            G<short>(0x00579556 + brec) = static_cast<short>(G<short>(0x00579556 + brec) - 0x10);   // 0x004032c1
                            G<unsigned char>(0x005794d2 + brec) = static_cast<unsigned char>(At<unsigned char>(w, 0x16) ^ 4);   // 0x004032a1 / 0x004032c9 / 0x004032cc
                            G<unsigned char>(0x005794d5 + brec) = 0xb;           // 0x004032d2
                            cnt = true;                                          // 0x004032d9
                            break;
                        case 2: {                                                // kind -3: 0x004032de
                            unsigned gflags = G<unsigned>(0x005794c8 + brec);    // 0x004032e1
                            if ((gflags & 0x20000000) != 0 && (At<unsigned char>(w, 0x12) & 8) != 0) {   // 0x004032e7..0x004032f5
                                G<short>(0x0057955c + brec) = static_cast<short>(0xfff5);   // 0x004032f7
                                At<unsigned char>(w, 0x14) = 0xf9;               // 0x00403300
                                G<short>(0x00579556 + brec) = 0;                 // 0x00403307
                            } else {
                                At<unsigned char>(w, 0x14) = 0xfd;               // 0x00403315
                                G<unsigned>(0x005794c8 + brec) = gflags | 0x4000;   // 0x00403312 / 0x0040331c
                            }
                            At<short>(w, 0x1e) = 0xc;                            // 0x00403322
                            At<short>(w, 0x1c) = 0;                              // 0x0040332b
                            kPlaySoundAt(snd + 0x5d, At<int>(w, 0), At<int>(w, 4), 500);   // 0x00403334..0x0040334b
                            cnt = true;                                          // 0x00403353
                            break;
                        }
                        case 0:                                                  // kind -5: 0x00403358
                            At<unsigned char>(w, 0x14) = 0xfb;                   // 0x00403358
                            At<short>(w, 0x1e) = 0xc;                            // 0x0040335f
                            At<short>(w, 0x1c) = 0;                              // 0x00403368
                            kPlaySoundAt(snd + 0x5f, At<int>(w, 0), At<int>(w, 4), 0);   // 0x00403371..0x00403385
                            At<short>(w, 0x1a) = static_cast<short>(At<short>(w, 0x1a) - 0x20);   // 0x0040338a / 0x00403392
                            G<short>(0x00579556 + brec) = static_cast<short>(0xffe0);   // 0x004033a5
                            G<unsigned char>(0x005794d5 + brec) = 0xb;           // 0x004033ac
                            G<unsigned char>(0x005794d2 + brec) = static_cast<unsigned char>(At<unsigned char>(w, 0x16) ^ 4);   // 0x00403399 / 0x004033a2 / 0x004033b3
                            cnt = true;                                          // 0x004033b9
                            break;
                        case 1:                                                  // kind -4: 0x004033be
                            if (bestIdx == 2) {                                  // 0x004033be / 0x004033c1
                                At<unsigned char>(w, 0x14) = 0xfa;               // 0x004033c8
                                At<short>(w, 0x1e) = 0xc;                        // 0x004033cf
                                At<short>(w, 0x1a) = 0xc;                        // 0x004033d6
                                G<int>(0x005736b8 + selIdx * 0x24) = -1;         // 0x004033dd..0x004033e4
                            } else {
                                At<unsigned char>(w, 0x14) = 0xfc;               // 0x004033f8
                                At<short>(w, 0x1e) = 0xc;                        // 0x004033ff
                                At<short>(w, 0x1a) = static_cast<short>(((~At<unsigned char>(w, 0x12)) & 8) >> 3);   // 0x00403408..0x00403424
                                int fl = selGy + selIdx * 50;                    // 0x0040341a..0x0040342b
                                unsigned short fw = G<unsigned short>(kTileFlags + 2 * fl);   // 0x0040342f
                                if ((fw & 0x4000) == 0) {                        // 0x00403435..0x00403442
                                    fw |= 0x4000;                                // 0x00403444
                                    G<unsigned short>(kTileFlags + 2 * fl) = fw; // 0x0040344d
                                    G<unsigned char>(0x00578804 + fl) = static_cast<unsigned char>(G<unsigned char>(0x0053fa28) - 3);   // 0x00403455..0x0040345e
                                    kPlaySoundAt(snd + 0x5e, At<int>(w, 0), At<int>(w, 4), 0);   // 0x00403464..0x00403470
                                    kFreeAtTile(selIdx, selGy);                  // 0x00403475..0x00403477
                                } else {
                                    unsigned char c = static_cast<unsigned char>(G<unsigned char>(0x00578804 + fl) - 1);   // 0x00403481 / 0x00403487
                                    G<unsigned char>(0x00578804 + fl) = c;       // 0x0040348b
                                    if (c <= 1) {                                // 0x00403491 / 0x00403494
                                        fw &= 0xb7ff;                            // 0x00403496
                                        At<short>(w, 0x1a) = 8;                  // 0x00403499
                                        G<unsigned short>(kTileFlags + 2 * fl) = fw;   // 0x004034a2
                                        cnt = true;                              // 0x004034aa
                                    }
                                }
                            }
                            break;
                        }
                    }
                    if (cnt) At<short>(w, 0x26) = static_cast<short>(At<short>(w, 0x26) + 1);   // 0x004034aa
                    if (At<short>(w, 0x1a) > 1 && (At<unsigned char>(w, 0x12) & 8) != 0) {   // 0x004034b1..0x004034c9
                        int wv = At<short>(w, 0x1a);                             // 0x004034cf
                        At<short>(w, 0x1a) = static_cast<short>((wv - (wv >> 31)) >> 1);   // 0x004034d2..0x004034d7
                    }
                    continue;                                                    // 0x004034de
                }
            }
        }
        // ---- 0x004034fb / 0x00403503: move toward (tx, ty) when the target is far or the walker is flagged
        (void)toMove;
        if ((At<unsigned char>(w, 0x12) & 0x10) != 0) {                          // 0x00403503 / 0x0040350a
            tx = (G<int>(0x00578150) << 10) + 0x200;                             // 0x0040351a / 0x0040352f / 0x0040353c
            ty = (G<int>(0x00578154) << 10) + 0x200;                             // 0x00403520 / 0x00403545 / 0x00403548
            int rem = (G<int>(0x00834170) + curIdx * 0x23) % 100;                // 0x00403514..0x00403543
            if (rem == 0) {                                                      // 0x00403556 / 0x00403558
                At<unsigned char>(w, 0x15) = 7;                                  // 0x0040355a
                At<unsigned char>(w, 0x14) = 0x23;                               // 0x00403561
            }
            int d = kApproxDistance(tx - At<int>(w, 0), ty - At<int>(w, 4));     // 0x00403568..0x0040357e
            if (d < 0x600) At<unsigned char>(w, 0x12) = 0;                       // 0x00403586 / 0x0040358b / 0x0040358d
        }
        if (At<short>(w, 0x18) == 0) {                                           // 0x0040359a / 0x004035a2
            G<unsigned>(kFlagsWord) = G<unsigned>(kFlagsWord) | 0x100;           // 0x004035a8..0x004035b8
            int r = kPathStep(tx, ty, At<int>(w, 0), At<int>(w, 4), curIdx);     // 0x004035b7..0x004035c8
            G<unsigned>(kFlagsWord) = G<unsigned>(kFlagsWord) & 0xfffffeff;      // 0x004035cd..0x004035e1
            unsigned char al = static_cast<unsigned char>(r);                    // 0x004035d9
            At<unsigned char>(w, 0x16) = al;                                     // 0x004035db
            if (al != 0xff) {                                                    // 0x004035d9 / 0x004035e7
                int x6 = (At<int>(w, 0) >> 6) & 0xf;                             // 0x004035e9 / 0x004035fb / 0x00403604
                int y6 = (At<int>(w, 4) >> 6) & 0xf;                             // 0x004035ef / 0x00403607 / 0x0040360a
                int di = static_cast<signed char>(al);                           // 0x004035f5
                int sv;
                if (Dx8()[di] == 0) {                                            // 0x0040360d / 0x0040360f
                    sv = (Dy8()[di] == 1) ? (0x18 - y6) : (y6 + 8);              // 0x00403624..0x00403636
                } else if (Dx8()[di] == 1) {                                     // 0x00403611 / 0x00403614
                    sv = 0x18 - x6;                                              // 0x00403616..0x0040361b
                } else {
                    sv = x6 + 8;                                                 // 0x0040361f
                }
                At<short>(w, 0x18) = static_cast<short>(sv);                     // 0x00403639
                if ((At<unsigned char>(w, 0x16) & 1) != 0 && At<short>(w, 0x18) > 0x10)   // 0x00403640..0x0040364f
                    At<short>(w, 0x18) = 0x10;                                   // 0x00403658
            } else {
                int ang = kAngleFixed(s30, s28);                                 // 0x00403661..0x0040366b
                At<unsigned char>(w, 0x16) = static_cast<unsigned char>((((ang >> 28) & 0xf) + 1) >> 1 & 7);   // 0x00403670..0x00403685
                At<short>(w, 0x1e) = 0xb;                                        // 0x0040368b
                At<short>(w, 0x1a) = static_cast<short>(RandRange(0x10) + 1);    // 0x00403694..0x0040369a
            }
            if (At<short>(w, 0x18) == 0) continue;                               // 0x004036a1 / 0x004036a9
        }
        // ---- 0x004036af: keep clear of the earlier walkers
        if (curIdx > 0) {                                                        // 0x004036b5 / 0x004036bb
            for (int j = 0; j < curIdx; j++) {                                   // 0x00403773..0x0040377d
                char* o = reinterpret_cast<char*>(kWalkers + j * 0x4c);          // 0x004036c1 / 0x00403774
                if (At<unsigned char>(o, 0x12) == 0 || j == curIdx) continue;    // 0x004036c6..0x004036d3
                int dxv = At<int>(o, 0) - At<int>(w, 0);                         // 0x004036d9..0x004036e8
                int dyv = At<int>(o, 4) - At<int>(w, 4);                         // 0x004036ea..0x004036ed
                if (kApproxDistance(dxv, dyv) < 0x155) {                         // 0x004036f1..0x004036fe
                    int d8 = kDirection8(dxv, dyv);                              // 0x00403702
                    int dv = At<signed char>(w, 0x16);                           // 0x00403707
                    if (dv == d8 || dv == ((d8 + 1) & 7) || dv == ((d8 - 1) & 7)) {   // 0x00403711..0x00403725
                        At<short>(w, 0x1a) = static_cast<short>(RandRange(4) + 4);   // 0x00403727..0x00403736
                        At<short>(w, 0x1e) = 0xb;                                // 0x0040373d
                    }
                }
                if (At<short>(w, 0x1a) != 0) {                                   // 0x00403746 / 0x0040374e
                    int ang = kAngleFixed(dxv, dyv);                             // 0x00403752
                    At<unsigned char>(w, 0x16) = static_cast<unsigned char>((((ang >> 28) & 0xf) + 1) >> 1 & 7);   // 0x00403757..0x00403765
                }
            }
        }
        // ---- 0x00403783: take the step
        int wxv = At<int>(w, 0) >> 10, wyv = At<int>(w, 4) >> 10;                // 0x00403783..0x00403792
        int tile = wyv + wxv * 50;                                               // 0x00403795..0x0040379f
        int ttype = static_cast<signed char>(G<char>(kTileType + tile));         // 0x004037a2
        int speed = kClamp(6 - G<signed char>(0x00578375 + ttype * 0x30), 2, 5); // 0x004037a9..0x004037be
        if ((G<unsigned char>(kTileFlags + 2 * tile) & 0x20) != 0) speed = 6;    // 0x004037c3..0x004037d2
        unsigned char wf = At<unsigned char>(w, 0x12);                           // 0x004037d7
        if ((wf & 8) != 0) speed += 2;                                           // 0x004037dd..0x004037e2
        if (At<signed char>(w, 0x13) == static_cast<signed char>(0xfa)) speed += 2;   // 0x004037e5..0x004037ee
        if ((wf & 0x10) != 0) speed = 3;                                         // 0x004037f1..0x004037f6
        int sp = speed << 6;                                                     // 0x00403801 / 0x00403806
        int dsel = At<signed char>(w, 0x16);                                     // 0x004037fb / 0x00403803
        int div = ((At<unsigned char>(w, 0x16) & 1) != 0 ? 1 : 0) + 3;           // 0x00403810 / 0x00403821..0x00403828
        int px = Dx8()[dsel] * sp;                                               // 0x00403809 / 0x00403813
        px = (px - (px >> 31)) >> 1;                                             // 0x00403816..0x0040381f
        At<int>(w, 0) = At<int>(w, 0) + px / div;                                // 0x00403827..0x0040382f
        int dsel2 = At<signed char>(w, 0x16);                                    // 0x00403835 / 0x0040383b
        int py = Dy8()[dsel2] * sp;                                              // 0x00403841 / 0x00403848
        py = (py - (py >> 31)) >> 1;                                             // 0x0040384b..0x0040384e
        At<int>(w, 4) = At<int>(w, 4) + py / (((At<unsigned char>(w, 0x16) & 1) != 0 ? 1 : 0) + 3);   // 0x00403850..0x00403864
        At<short>(w, 0x18) = static_cast<short>(At<short>(w, 0x18) - 1);         // 0x0040386a
        if (At<short>(w, 0x18) != 0) {                                           // 0x00403871 / 0x00403879
            if (static_cast<short>(RandRange(4)) == 0)                           // 0x0040387b..0x0040388a
                At<short>(w, 0x18) = static_cast<short>(At<short>(w, 0x18) - 1); // 0x0040388c
        }
        short an = At<short>(w, 0x1e);                                           // 0x00403893
        if (an < 7) {                                                            // 0x0040389a / 0x0040389e
            At<short>(w, 0x1e) = 7;                                              // 0x004038ae
        } else {
            short an1 = static_cast<short>(an + 1);                              // 0x004038a0
            At<short>(w, 0x1e) = an1;                                            // 0x004038a5
            if (an1 > 10) At<short>(w, 0x1e) = 7;                                // 0x004038a1 / 0x004038ac / 0x004038ae
        }
    }
    return 0x40;                                                                 // 0x004038bb..0x004038bf
}
C0_t UpdateWalkers_orig;
// 0x00402a40
SG_HOOK("golf_clean.exe", 0x00402a40, updateWalkers, UpdateWalkers_re, UpdateWalkers_orig);

// ====================================================================================================================
// placeAnimal
// ====================================================================================================================

// 0x004017d0  placeAnimal(idx): cdecl, `ret`, one stack argument; the result register is not read by its only caller (tickAnimals
// 0x00401c10 pops the argument and loops). Animal records are 0x3c bytes from 0x0056d1d8 (0x004017da..0x004017e8): +0 type
// (short), +2 map (short), +6 / +8 tile x / y (shorts), +0xa wait (short), +0xc sprite state (short), +0x10 / +0x14 sub-tile
// x / y (ints), +0x18 heading (byte), +0x19 step counter (byte), +0x1a frame (byte). It draws the animal when worldToScreen
// accepts its position and moves it across its 4 x 6 map (0x004e6d70 + 0x74 * map).
void __cdecl PlaceAnimal_re(int idx) {
    char* a = reinterpret_cast<char*>(0x0056d1d8 + idx * 0x3c);                  // 0x004017da..0x004017e8
    int wx = (static_cast<int>(At<short>(a, 6)) << 10) + At<int>(a, 0x10);       // 0x004017eb..0x00401801
    int wy = (static_cast<int>(At<short>(a, 8)) << 10) + At<int>(a, 0x14);       // 0x00401803..0x00401814
    int sprite = At<short>(a, 0) + At<short>(a, 0xc);                            // 0x00401816 / 0x0040180a / 0x0040181d
    int sx = 0, sy = 0;
    int onScreen = kWorldToScreen(wx, wy, &sx, &sy, 0);                          // 0x0040182a
    if (onScreen != 0) {                                                         // 0x00401832 / 0x00401834
        if (!(At<short>(a, 0xa) >= 0x80)) {                                      // 0x0040183a / 0x00401843
            int n = 0;                                                           // 0x00401849
            for (unsigned p = 0x0058bcb8; p < 0x0058ccb8; p += 0x10) {           // 0x0040184b / 0x00401882
                if (G<short>(p) == 5 && G<int>(p + 8) > 0 &&                     // 0x00401850..0x0040185c
                    G<short>(p + 2) == At<short>(a, 6) &&                        // 0x0040185e..0x00401868
                    G<short>(p + 4) == At<short>(a, 8))                          // 0x0040186a..0x00401875
                    n = G<int>(p + 8) - 1;                                       // 0x00401877
            }
            int heading = (G<int>(0x005685f4) - At<unsigned char>(a, 0x18) - 2) & 7;   // 0x00401884..0x004018a4
            int cells = G<int>(0x0053f3e8 + 4 * sprite);                         // 0x0040189a
            int fr = static_cast<signed char>(At<char>(a, 0x1a)) % cells;        // 0x00401892..0x0040189a
            At<char>(a, 0x1a) = static_cast<char>(fr);                           // 0x004018ad
            int cell = kCellAt(sprite, static_cast<signed char>(fr), heading);   // 0x004018b3
            G<int>(0x004e9a80) = cell;                                           // 0x004018c8
            kEnqueueSprite(sx, sy, sy, 0x1e, 0x28, cell, 4, (n + 0x9c) | 0x100); // 0x004018dc
            if (!(G<unsigned char>(kFlagsWord) & 4)) {                           // 0x004018e1 / 0x004018e9
                signed char f1 = static_cast<signed char>(At<char>(a, 0x1a) + 1);   // 0x004018ed / 0x004018fa
                At<char>(a, 0x1a) = f1;                                          // 0x00401901
                if (!(f1 < G<int>(0x0053f3e8 + 4 * sprite))) At<char>(a, 0x1a) = 0;   // 0x004018f3 / 0x004018ff / 0x00401907 / 0x00401909
            }
            G<char>(0x0051a068) = 0;                                             // 0x00401923
            const char* s1 = reinterpret_cast<const char*>(kItoa((At<int>(a, 0x10) << 1) >> 10, 0x004e9a70, 10));   // 0x0040192a
            AppendStr(reinterpret_cast<char*>(0x0051a068), s1);                  // 0x0040192f..0x0040195e
            AppendStr(reinterpret_cast<char*>(0x0051a068), reinterpret_cast<const char*>(0x004c1430));   // 0x00401960..0x00401994
            const char* s2 = reinterpret_cast<const char*>(kItoa((At<int>(a, 0x14) << 1) >> 10, 0x004e9a70, 10));   // 0x00401997
            AppendStr(reinterpret_cast<char*>(0x0051a068), s2);                  // 0x0040199c..0x004019c7
        }
    } else if (!(G<unsigned char>(kFlagsWord) & 4)) {                            // 0x004019cb / 0x004019d2
        if (static_cast<short>(RandRange(500)) == 0) At<short>(a, 0xa) = 500;    // 0x004019d4..0x004019e8
    }
    if (At<short>(a, 0xa) != 0) {                                                // 0x004019f1 / 0x004019f9
        At<short>(a, 0xc) = 0x140;                                               // 0x00401a00
        if (!(G<unsigned char>(kFlagsWord) & 4)) At<short>(a, 0xa) = static_cast<short>(At<short>(a, 0xa) - 1);   // 0x004019fb / 0x00401a09 / 0x00401a11
        return;                                                                  // 0x00401a18..0x00401a1f
    }
    if (At<unsigned char>(a, 0x1a) == 0 && At<short>(a, 0xc) != 0x15a && At<short>(a, 0xc) != 0x140) {   // 0x00401a20..0x00401a3b
        At<short>(a, 0xc) = 0x140;                                               // 0x00401a44
        At<short>(a, 0xa) = static_cast<short>(RandRange(0x40) + 0x40);          // 0x00401a3d / 0x00401a4d..0x00401a56
    }
    if (At<unsigned char>(a, 0x19) == 0) {                                       // 0x00401a5c..0x00401a64
        if (At<short>(a, 0xc) == 0x140 && static_cast<short>(RandRange(8)) == 0) {   // 0x00401a6a..0x00401a88
            int tx = (At<int>(a, 0x10) << 1) >> 10;                              // 0x00401a8e / 0x00401a9a / 0x00401aa5
            int ty = (At<int>(a, 0x14) << 1) >> 10;                              // 0x00401a94 / 0x00401a9c / 0x00401aa8
            int r = RandRange(3) & 0xffff;                                       // 0x00401aab / 0x00401ab6
            unsigned char hd = static_cast<unsigned char>((At<unsigned char>(a, 0x18) + static_cast<unsigned char>(r) - 1) & 7);   // 0x00401ab0..0x00401abf
            At<unsigned char>(a, 0x18) = hd;                                     // 0x00401ac8
            tx += Dx8()[static_cast<signed char>(hd)];                           // 0x00401ac2..0x00401ada
            ty += Dy8()[static_cast<signed char>(hd)];                           // 0x00401ad4 / 0x00401adc
            if (tx < 0 || ty < 0 || tx >= 4 || ty >= 4) {                        // 0x00401ade..0x00401aee
                int lim = (G<unsigned>(kFlagsWord) & 0x200000) ? 0x40 : 0x18;    // 0x00401b1c..0x00401b32
                if (static_cast<short>(RandRange(lim)) == 0) {                   // 0x00401b36..0x00401b3e
                    kPlaySoundAt(At<short>(a, 0) + 0x82, wx, wy, 0);             // 0x00401b40..0x00401b55
                    At<short>(a, 0xc) = 0x14d;                                   // 0x00401b5a
                    At<char>(a, 0x1a) = 0;                                       // 0x00401b66
                }
            } else if (G<unsigned char>(0x004e6d70 + At<short>(a, 2) * 0x74 + tx * 6 + ty) == 0xff) {   // 0x00401af0..0x00401b09
                At<unsigned char>(a, 0x19) = 0x10;                               // 0x00401b13
            }
        }
        if (At<unsigned char>(a, 0x19) == 0) return;                             // 0x00401b6d..0x00401b75
    }
    At<short>(a, 0xc) = 0x15a;                                                   // 0x00401b77
    At<int>(a, 0x10) = At<int>(a, 0x10) + ((Dx8()[At<signed char>(a, 0x18)] << 10) / 32);   // 0x00401b80..0x00401ba2
    At<int>(a, 0x14) = At<int>(a, 0x14) + ((Dy8()[At<signed char>(a, 0x18)] << 10) / 32);   // 0x00401ba8..0x00401bca
    signed char st = static_cast<signed char>(At<char>(a, 0x19) - 1);            // 0x00401bd0 / 0x00401bd6
    At<char>(a, 0x19) = st;                                                      // 0x00401bda
    if (!(st > 0)) At<short>(a, 0xc) = 0x140;                                    // 0x00401be0..0x00401be4
}
C1_t PlaceAnimal_orig;
// 0x004017d0
SG_HOOK("golf_clean.exe", 0x004017d0, placeAnimal, PlaceAnimal_re, PlaceAnimal_orig);
