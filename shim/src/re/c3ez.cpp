// Batch c3ez (round 33): golf_clean.exe rows whose blocker a later tool removed (log/c3/c3ez_candidates.tsv): the
// Terrain grid (on c3ey's fixture Terrain object), the net family, unlinkSound, Window::setSize. Purposes and per-jcc
// tables: log/c3/c3ez_purpose.md; registry: re/frida/registry.d/c3ez.py; fixtures: re/frida/js/fixtures.d/c3ez.js;
// what was not taken and why: log/c3/c3ez_notes.txt.
//
// golf_clean.exe addresses are VAs (image base 0x00400000, no relocations). __thiscall is emulated with __fastcall
// (ecx = this, edx unused). Callees are called at the addresses the originals call, so a hook on them stays in the
// chain, and Terrain.dll methods through the exe's `jmp [slot]` import thunks, as the originals do.
#include "hooks.h"

// CZ(cond, jcc): a condition tagged with the address of the original's jcc (or switch table + case) it stands for. It
// is the bare condition here; the branch-coverage build (log/c3/alt/c3ez_gen.py, never linked into the main build)
// counts each side, so the purpose file's per-jcc tables are measured.
#ifndef CZ
#define CZ(c, jcc) (c)
#endif

namespace {

template <class T> T& G(unsigned va) { return *reinterpret_cast<T*>(va); }
template <class T> T& F(void* obj, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(obj) + off); }

}  // namespace

// =====================================================================================================================
// The Terrain grid. The Terrain object is [0x00820ed0] (width +0x14, height +0x18, tiles of 0x248 bytes from +0x3a4,
// tile (x, y) = obj + 0x3a4 + (x + y * width) * 0x248, NULL outside 0 <= x < width, 0 <= y < height with signed
// compares: the inline Terrain::tileAt each function repeats, e.g. 0x0044a615..0x0044a643).
// =====================================================================================================================
namespace {

typedef int(__cdecl* XY_t)(int, int);
typedef int(__cdecl* XYC_t)(int, int, int);
typedef int(__cdecl* C0_t)();
typedef int(__cdecl* TXY_t)(char*, int, int);
typedef int(__fastcall* T0_t)(void* self, void* edx);
typedef int(__fastcall* T1_t)(void* self, void* edx, unsigned a);
typedef int(__fastcall* T2_t)(void* self, void* edx, unsigned a, unsigned b);
typedef int(__fastcall* T3_t)(void* self, void* edx, unsigned a, unsigned b, unsigned c);
typedef int(__fastcall* T4_t)(void* self, void* edx, unsigned a, unsigned b, unsigned c, unsigned d);
typedef int(__fastcall* T7_t)(void* self, void* edx, unsigned a, unsigned b, unsigned c, unsigned d, unsigned e,
                              unsigned f, unsigned g);

const XY_t kTileBlocked = reinterpret_cast<XY_t>(0x0040bf60);
const XY_t kTileType = reinterpret_cast<XY_t>(0x004492d0);
const XY_t kTileByte = reinterpret_cast<XY_t>(0x004492f0);
const XYC_t kTileQuery = reinterpret_cast<XYC_t>(0x00449310);
const XY_t kTileFlag20 = reinterpret_cast<XY_t>(0x004493b0);
const TXY_t kApplyTileWalls = reinterpret_cast<TXY_t>(0x0044a380);
const C0_t kRebuildTerrainGrid = reinterpret_cast<C0_t>(0x00449540);
// Terrain.dll import thunks (`jmp [slot]`, log/c3/alt/c3ez_thunks.py)
const T2_t kTerPassCollarInfo = reinterpret_cast<T2_t>(0x004a4f22);   // passCollarInfo(int*, int)
const T3_t kTerSetType = reinterpret_cast<T3_t>(0x004a4f28);          // setType(Tile*, int, int)
const T1_t kTerGetVariation = reinterpret_cast<T1_t>(0x004a4f2e);     // getVariation(Tile*)
const T3_t kTerLayPath = reinterpret_cast<T3_t>(0x004a4f3a);          // layPath(Tile*, int, int)
const T2_t kTerElevateCorner = reinterpret_cast<T2_t>(0x004a4f4c);    // elevateCorner(Tile*, int)
const T0_t kTerInitTerrain = reinterpret_cast<T0_t>(0x004a4f52);      // initTerrain()
const T4_t kTerInitSystem = reinterpret_cast<T4_t>(0x004a4f58);       // initSystem(int, int, HDC, bool)
const T2_t kTerPathUpdateRender = reinterpret_cast<T2_t>(0x004a4f6a); // pathUpdateRender(Tile*, float)
const T3_t kTerLocalRender = reinterpret_cast<T3_t>(0x004a4f70);      // localRender(Tile*, Tile*, float)
const T1_t kTerCalcAllNormals = reinterpret_cast<T1_t>(0x004a4f76);   // calcAllNormals(Tile*)
const T1_t kTerSetZoomLevel = reinterpret_cast<T1_t>(0x004a4f7c);     // setZoomLevel(int)
const T2_t kTerRender = reinterpret_cast<T2_t>(0x004a4f82);           // render(Tile*, float)
const T4_t kTerLowerEdgeCorner = reinterpret_cast<T4_t>(0x004a4f88);  // lowerEdgeCorner(Tile*, int, Tile*, float)
const T2_t kTerLowerCorner = reinterpret_cast<T2_t>(0x004a4f8e);      // lowerCorner(Tile*, int)
const T2_t kTerHasConnectedPath = reinterpret_cast<T2_t>(0x004a4f94); // hasConnectedPath(int, int)
const T3_t kTerUpdatePath = reinterpret_cast<T3_t>(0x004a4f9a);       // updatePath(int, int, int)
const T1_t kTerHasPath = reinterpret_cast<T1_t>(0x004a4fa0);          // hasPath(Tile*)
const T3_t kTerStripRender = reinterpret_cast<T3_t>(0x004a4fa6);      // stripRender(Tile*, int, float)

void* Terrain() { return G<void*>(0x00820ed0); }
unsigned P(const void* p) { return static_cast<unsigned>(reinterpret_cast<unsigned long>(p)); }

// site: the address of the inline copy's first bound test (`jge` x >= width); its next three tests (x < 0, y >= height,
// y < 0) follow it in that order in every copy and are tagged site + 1 .. site + 3.
char* TileAt(int x, int y, unsigned site) {
    char* t = static_cast<char*>(Terrain());
    const int w = F<int>(t, 0x14);
    if (CZ(x >= w, site) || CZ(x < 0, site + 1) || CZ(y >= F<int>(t, 0x18), site + 2) || CZ(y < 0, site + 3))
        return 0;
    (void)site;
    return t + 0x3a4 + (x + y * w) * 0x248;
}

// The camera-rotation angle every caller of render / stripRender / lowerEdgeCorner builds with the same switch
// (`cmp eax, 6; ja default; jmp [eax*4 + table]`, e.g. 0x0044a12f / table 0x0044a348): rotation 2 -> 90.0f
// (0x42b40000), 4 -> 180.0f (0x43340000), 6 -> -90.0f (0xc2b40000), anything else (0, 1, 3, 5, unsigned > 6) -> 0.
// table: the copy's jump table (tagged table + 2 / + 4 / + 6 for the three cases, table + 7 for the default).
unsigned RotAngle(int r, unsigned table) {
    (void)table;
    switch (static_cast<unsigned>(r)) {
        case 2: CZ(true, table + 2); return 0x42b40000u;
        case 4: CZ(true, table + 4); return 0x43340000u;
        case 6: CZ(true, table + 6); return 0xc2b40000u;
        default: CZ(true, table + 7); return 0;
    }
}

// The byte flags the Terrain rows set for updateTerrainRender (0x00820f2a..0x00820f2c)
unsigned char& NeedNormals() { return G<unsigned char>(0x00820f2c); }
unsigned char& NeedPathRender() { return G<unsigned char>(0x00820f2b); }

}  // namespace

// 0x00449f00  applyTileVariation(tile, x, y) (cdecl, returns a byte in al). The tile's current type is its +0x24
// (0x00449f0b, read with no NULL test). When tileType(x, y) (0x00449f15) equals it (`jne` 0x00449f1f) and
// Terrain::getVariation(tile) (0x00449f28) equals tileByte(x, y) (0x00449f31, `jne` 0x00449f3b), nothing changes: al = 0
// (0x00449f40; the upper bytes are tileByte's). Otherwise, when tileType(x, y) is 7 (0x00449f4e) or the tile's +0x24 is
// 7 (0x00449f53), or else tileType(x, y) is 6 (0x00449f63, a third call) or +0x24 is 6 (0x00449f68), the byte
// [0x00820f2c] is set to 1 (0x00449f6e); then Terrain::setType(tile, tileType(x, y), tileByte(x, y)) (tileByte
// 0x00449f77 first, tileType 0x00449f82, setType 0x00449f92) and al = 1 (0x00449f9a; the upper bytes are setType's eax).
int __cdecl ApplyTileVariation_re(char* tile, int x, int y) {
    const int cur = F<int>(tile, 0x24);                                         // 0x00449f0b
    int r;
    if (CZ(kTileType(x, y) == cur, 0x00449f1f)) {                               // 0x00449f15 / 0x00449f1f
        const int v = kTerGetVariation(Terrain(), 0, P(tile));                  // 0x00449f28
        r = kTileByte(x, y);                                                    // 0x00449f31
        if (CZ(r == v, 0x00449f3b))                                             // 0x00449f3b
            return r & ~0xff;                                                   // 0x00449f40
    }
    if (CZ(kTileType(x, y) == 7, 0x00449f51) || CZ(F<int>(tile, 0x24) == 7, 0x00449f57) ||  // 0x00449f46..0x00449f57
        CZ(kTileType(x, y) == 6, 0x00449f66) || CZ(F<int>(tile, 0x24) == 6, 0x00449f6c))    // 0x00449f5b..0x00449f6c
        NeedNormals() = 1;                                                      // 0x00449f6e
    const int b = kTileByte(x, y);                                              // 0x00449f77
    const int t = kTileType(x, y);                                              // 0x00449f82
    r = kTerSetType(Terrain(), 0, P(tile), t, b);                               // 0x00449f92
    return (r & ~0xff) | 1;                                                     // 0x00449f9a
}
TXY_t ApplyTileVariation_orig;
// 0x00449f00
SG_HOOK("golf_clean.exe", 0x00449f00, applyTileVariation, ApplyTileVariation_re, ApplyTileVariation_orig);

namespace {

// Is any of the eight neighbours of (x, y) blocked? The order of the tileBlocked calls in both copies of the test
// (0x0044a06c..0x0044a0fe and 0x0044a1de..0x0044a26e): (x-1, y), (x, y-1), (x+1, y), (x, y+1), (x-1, y-1), (x+1, y-1),
// (x+1, y+1), (x-1, y+1); the first nonzero answer stops the test.
// copy: 0 for the first test (jne 0x0044a07b .. 0x0044a0fe), 1 for the second (jne 0x0044a1ed .. 0x0044a26e)
bool NeighbourBlocked(int x, int y, int copy) {
    static const unsigned kJne[2][8] = {
        {0x0044a07b, 0x0044a090, 0x0044a0a3, 0x0044a0b8, 0x0044a0cd, 0x0044a0db, 0x0044a0e9, 0x0044a0fe},
        {0x0044a1ed, 0x0044a202, 0x0044a215, 0x0044a22a, 0x0044a23d, 0x0044a24b, 0x0044a259, 0x0044a26e}};
    const unsigned* j = kJne[copy];
    (void)j;
    return CZ(kTileBlocked(x - 1, y) != 0, j[0]) || CZ(kTileBlocked(x, y - 1) != 0, j[1]) ||
           CZ(kTileBlocked(x + 1, y) != 0, j[2]) || CZ(kTileBlocked(x, y + 1) != 0, j[3]) ||
           CZ(kTileBlocked(x - 1, y - 1) != 0, j[4]) || CZ(kTileBlocked(x + 1, y - 1) != 0, j[5]) ||
           CZ(kTileBlocked(x + 1, y + 1) != 0, j[6]) || CZ(kTileBlocked(x - 1, y + 1) != 0, j[7]);
}

// One lowering step at corner c: lowerCorner(tile, c) when no neighbour is blocked (0x0044a110 / 0x0044a280), else
// lowerEdgeCorner(tile, c, tileAt(x2, y2), angle of g_cameraRot 0x005685f4) (0x0044a1a5 / 0x0044a315).
void LowerStep(char* tile, int x, int y, int c, int x2, int y2, int copy) {
    if (!NeighbourBlocked(x, y, copy)) {
        kTerLowerCorner(Terrain(), 0, P(tile), c);
        return;
    }
    const unsigned angle = RotAngle(G<int>(0x005685f4), copy ? 0x0044a364 : 0x0044a348);   // 0x0044a12a / 0x0044a29a
    kTerLowerEdgeCorner(Terrain(), 0, P(tile), c, P(TileAt(x2, y2, copy ? 0x0044a2e0 : 0x0044a170)), angle);
}

}  // namespace

// 0x00449fa0  applyTileElevation(tile, x, y, x2, y2) (cdecl; y is kept in ebx, 0x00449fa8, and its argument slot
// [esp+0x2c] receives the angle the switch builds, 0x0044a13b..0x0044a159; x2 / y2 are [esp+0x30] / [esp+0x34],
// 0x0044a167 / 0x0044a176). For corners c = 1, 3, 5, 7 (ebp from 1, `add ebp, 2; cmp ebp, 8; jl`
// 0x0044a323..0x0044a334) with the tile's height h = [tile + 4 + 8 * (c - 1) / 2] (the pointer at [esp+0x18] starts at
// tile + 4, 0x00449fb2, and steps by 8, 0x0044a326; h is 0 for a NULL tile, 0x00449fca) and the target q =
// tileQuery449310(x, y, c) - 3 (0x00449fdd / 0x00449fe2):
//   q > h (`jle` 0x00449fea): Terrain::elevateCorner(tile, c) (0x00449ff8), [0x00820f2c] = 1 and the result byte = 1
//     (0x00449ffd / 0x0044a004), then again while q > h, both re-read (0x0044a009..0x0044a042);
//   else q is re-read and, when q < h (`jge` 0x0044a066 skips), one lowering step (0x0044a06c..0x0044a1a5), [0x00820f2c]
//     = 1 and the result = 1 (0x0044a1aa / 0x0044a1b1), then more steps while q < h (0x0044a1b6..0x0044a31a).
// al at `ret` (0x0044a33a) is the result byte (0 when nothing moved); the upper bytes are the last q's (every exit to
// 0x0044a31f follows `sub eax, 3; cmp eax, esi`).
int __cdecl ApplyTileElevation_re(char* tile, int x, int y, int x2, int y2) {
    unsigned char res = 0;                                                     // 0x00449fb7
    int q = 0;
    for (int c = 1, k = 0; c < 8; c += 2, ++k) {
        const unsigned off = 4 + 8 * k;
        int h = CZ(tile != 0, 0x00449fca) ? F<int>(tile, off) : 0;              // 0x00449fc4..0x00449fd4
        q = kTileQuery(x, y, c) - 3;                                            // 0x00449fdd / 0x00449fe2
        if (CZ(q > h, 0x00449fea)) {                                            // 0x00449fea
            kTerElevateCorner(Terrain(), 0, P(tile), c);                        // 0x00449ff8
            NeedNormals() = 1;                                                  // 0x00449ffd
            res = 1;                                                            // 0x0044a004
            for (;;) {
                h = CZ(tile != 0, 0x0044a00f) ? F<int>(tile, off) : 0;          // 0x0044a009..0x0044a019
                q = kTileQuery(x, y, c) - 3;                                    // 0x0044a01e
                if (CZ(q <= h, 0x0044a02b))                                     // 0x0044a02b
                    break;
                kTerElevateCorner(Terrain(), 0, P(tile), c);                    // 0x0044a03d
            }
            CZ(c < 7, 0x0044a334);
            continue;
        }
        h = CZ(tile != 0, 0x0044a04a) ? F<int>(tile, off) : 0;                  // 0x0044a044..0x0044a054
        q = kTileQuery(x, y, c) - 3;                                            // 0x0044a059
        if (CZ(q >= h, 0x0044a066)) {                                           // 0x0044a066
            CZ(c < 7, 0x0044a334);
            continue;
        }
        LowerStep(tile, x, y, c, x2, y2, 0);                                    // 0x0044a06c..0x0044a1a5
        NeedNormals() = 1;                                                      // 0x0044a1aa
        res = 1;                                                                // 0x0044a1b1
        for (;;) {
            h = CZ(tile != 0, 0x0044a1bc) ? F<int>(tile, off) : 0;              // 0x0044a1b6..0x0044a1c6
            q = kTileQuery(x, y, c) - 3;                                        // 0x0044a1cb
            if (CZ(q >= h, 0x0044a1d8))                                         // 0x0044a1d8
                break;
            LowerStep(tile, x, y, c, x2, y2, 1);                                // 0x0044a1de..0x0044a315
        }
        CZ(c < 7, 0x0044a334);
    }
    return (q & ~0xff) | res;                                                   // 0x0044a33a
}
typedef int(__cdecl* ATE_t)(char*, int, int, int, int);
ATE_t ApplyTileElevation_orig;
// 0x00449fa0
SG_HOOK("golf_clean.exe", 0x00449fa0, applyTileElevation, ApplyTileElevation_re, ApplyTileElevation_orig);

namespace {

// Every tile of the 50 x 50 grid with tileFlag20 set gets Terrain::updatePath(x', y', its flag word's 0x40 bit)
// (x' outer, edi 0..49 with ebx = 50 * x', y' inner, esi 0..49: 0x0044a460..0x0044a49f and 0x0044a518..0x0044a557),
// then [0x00820f2b] = 1 (0x0044a4a9 / 0x0044a55d).
// je: the copy's `je` on tileFlag20 (0x0044a472 or 0x0044a52a)
void UpdateAllPaths(unsigned je) {
    (void)je;
    for (int xx = 0; xx < 50; ++xx)
        for (int yy = 0; yy < 50; ++yy)
            if (CZ(kTileFlag20(xx, yy) != 0, je))
                kTerUpdatePath(Terrain(), 0, xx, yy, G<unsigned char>(0x0053caf0 + (xx * 50 + yy) * 2) & 0x40);
    NeedPathRender() = 1;
}

}  // namespace

// 0x0044a410  togglePath(tile, x, y) (cdecl, returns a byte in al). With f = tileFlag20(x, y) (0x0044a420) and the
// flag word's byte b = [0x0053caf0 + (x * 50 + y) * 2]:
//   f set and Terrain::hasPath(tile) false (0x0044a43b, `test al, al` 0x0044a440): a path is laid. When b & 0x40
//     (0x0044a456) every flagged tile is re-pathed (UpdateAllPaths); then Terrain::layPath(tile, tileFlag20(x, y), b &
//     0x40) (0x0044a4d1; b is read again after the sweep, 0x0044a4b0) and al = 1 (0x0044a4d9).
//   otherwise tileFlag20 is called again (0x0044a4df): set -> al = 0 (0x0044a59d, eax = 0 from tileFlag20's 0x20 with al
//     cleared); clear and hasPath(tile) false (0x0044a4fa / 0x0044a501) -> al = 0 with hasPath's upper bytes; clear and
//     hasPath true: the path is removed. When Terrain::hasConnectedPath(x, y) (0x0044a50f, `test al, al` 0x0044a514)
//     every flagged tile is re-pathed; then layPath(tile, tileFlag20(x, y) (0, 0x0044a57b), b & 0x40) (0x0044a58f) and
//     al = 1 (0x0044a597).
// The upper bytes of eax at the al = 1 exits are layPath's.
int __cdecl TogglePath_re(char* tile, int x, int y) {
    const int idx = x * 50 + y;                                                 // 0x0044a448..0x0044a44f
    if (CZ(kTileFlag20(x, y) != 0, 0x0044a42a)) {                               // 0x0044a420 / 0x0044a42a
        if (CZ(!(kTerHasPath(Terrain(), 0, P(tile)) & 0xff), 0x0044a442)) {     // 0x0044a43b / 0x0044a442
            if (CZ((G<unsigned char>(0x0053caf0 + idx * 2) & 0x40) != 0, 0x0044a45e))   // 0x0044a456
                UpdateAllPaths(0x0044a472);                                     // 0x0044a460..0x0044a4a9
            const int b40 = G<unsigned char>(0x0053caf0 + idx * 2) & 0x40;      // 0x0044a4b0
            const int f = kTileFlag20(x, y);                                    // 0x0044a4bd
            const int r = kTerLayPath(Terrain(), 0, P(tile), f, b40);           // 0x0044a4d1
            return (r & ~0xff) | 1;                                             // 0x0044a4d9
        }
    }
    const int f = kTileFlag20(x, y);                                            // 0x0044a4df
    if (CZ(f != 0, 0x0044a4e9))                                                 // 0x0044a4e9
        return f & ~0xff;                                                       // 0x0044a59d
    const int hp = kTerHasPath(Terrain(), 0, P(tile));                          // 0x0044a4fa
    if (CZ(!(hp & 0xff), 0x0044a501))                                           // 0x0044a501
        return hp & ~0xff;                                                      // 0x0044a59d
    if (CZ((kTerHasConnectedPath(Terrain(), 0, x, y) & 0xff) != 0, 0x0044a516)) // 0x0044a50f / 0x0044a516
        UpdateAllPaths(0x0044a52a);                                             // 0x0044a518..0x0044a55d
    const int b40 = G<unsigned char>(0x0053caf0 + idx * 2) & 0x40;              // 0x0044a56e
    const int f2 = kTileFlag20(x, y);                                           // 0x0044a57b
    const int r = kTerLayPath(Terrain(), 0, P(tile), f2, b40);                  // 0x0044a58f
    return (r & ~0xff) | 1;                                                     // 0x0044a597
}
TXY_t TogglePath_orig;
// 0x0044a410
SG_HOOK("golf_clean.exe", 0x0044a410, togglePath, TogglePath_re, TogglePath_orig);

namespace {

typedef void*(__cdecl* New_t)(unsigned);
const New_t kOperatorNew = reinterpret_cast<New_t>(0x004a57f1);
const TXY_t kApplyTileVariation = reinterpret_cast<TXY_t>(0x00449f00);
const ATE_t kApplyTileElevation = reinterpret_cast<ATE_t>(0x00449fa0);
const TXY_t kTogglePath = reinterpret_cast<TXY_t>(0x0044a410);

// std::list<Tile*> at 0x00820f18: _Head node pointer [0x00820f1c], _Size [0x00820f20]; nodes {next, prev, value}.
// The insert at 0x0044a67c..0x0044a6bc puts a new node before the head's NEXT node (insert(begin(), tile)).
void PushTile(char* tile) {
    char* head = G<char*>(0x00820f1c);
    char* at = F<char*>(head, 0);                                               // 0x0044a683
    char* prev = F<char*>(at, 4);                                               // 0x0044a685
    char* node = static_cast<char*>(kOperatorNew(0xc));                         // 0x0044a68c
    F<char*>(node, 0) = CZ(at != 0, 0x0044a698) ? at : node;                    // 0x0044a694..0x0044a69c
    F<char*>(node, 4) = CZ(prev != 0, 0x0044a6a4) ? prev : node;                // 0x0044a69e..0x0044a6a8
    F<char*>(at, 4) = node;                                                     // 0x0044a6ab
    F<char*>(F<char*>(node, 4), 0) = node;                                      // 0x0044a6ae / 0x0044a6b1
    if (CZ(node + 8 != 0, 0x0044a6b8))                                          // 0x0044a6b6
        F<char*>(node, 8) = tile;                                               // 0x0044a6ba
    ++G<int>(0x00820f20);                                                       // 0x0044a6bc
}

}  // namespace

// 0x0044a5b0  rebuildTileData(x0, y0) (cdecl). With k = 4 / g_zoomScale [0x004c2844] (signed idiv 0x0044a5b8) and
// d = 13 * k (0x0044a5c2 / 0x0044a5c5): x runs from max(x0 - d, 0) (0x0044a5d6 / 0x0044a5da) to 49 and, for each x, y from
// max(y0 - d, 0) to 49 (0x0044a5fd / 0x0044a601); x0 - d >= 50 returns at once (0x0044a5cd) and y0 - d >= 50 skips the x
// (0x0044a5f7, `jge` to the x increment). For every (x, y) that tileBlocked 0x0040bf60 leaves free (0x0044a605 /
// 0x0044a60f): tile = tileAt(x, y); applyTileVariation(tile, x, y), applyTileElevation(tile, x, y, x0, y0)
// (0x0044a645..0x0044a65c), applyTileWalls 0x0044a380 and togglePath (`add esp, 0x38` 0x0044a675 pops all four); when
// any of the four bytes is nonzero (`or` 0x0044a664..0x0044a678) the tile is pushed onto the list 0x00820f18 (PushTile).
// The only caller, updateTerrainRender, overwrites eax right after (0x00449ad4..0x00449ae3), so nothing is returned.
void __cdecl RebuildTileData_re(int x0, int y0) {
    const int d = 4 / G<int>(0x004c2844) * 13;                                  // 0x0044a5b8..0x0044a5c5
    int x = x0 - d;
    if (CZ(x >= 50, 0x0044a5cd))                                                // 0x0044a5cd
        return;
    for (;;) {
        if (CZ(x < 0, 0x0044a5d8)) x = 0;                                       // 0x0044a5d6 / 0x0044a5da
        int y = y0 - 4 / G<int>(0x004c2844) * 13;                               // 0x0044a5e1..0x0044a5f2
        if (!CZ(y >= 50, 0x0044a5f7)) {                                         // 0x0044a5f7
            for (;;) {
                if (CZ(y < 0, 0x0044a5ff)) y = 0;                               // 0x0044a5fd / 0x0044a601
                if (!CZ(kTileBlocked(x, y) != 0, 0x0044a60f)) {                 // 0x0044a605 / 0x0044a60f
                    char* tile = TileAt(x, y, 0x0044a620);                      // 0x0044a615..0x0044a643
                    unsigned char any = static_cast<unsigned char>(kApplyTileVariation(tile, x, y));  // 0x0044a648
                    any |= static_cast<unsigned char>(kApplyTileElevation(tile, x, y, x0, y0));       // 0x0044a65c
                    any |= static_cast<unsigned char>(kApplyTileWalls(tile, x, y));                    // 0x0044a666
                    any |= static_cast<unsigned char>(kTogglePath(tile, x, y));                        // 0x0044a670
                    if (CZ(any != 0, 0x0044a67a))                               // 0x0044a678 / 0x0044a67a
                        PushTile(tile);                                         // 0x0044a67c..0x0044a6bc
                }
                if (!CZ(++y < 50, 0x0044a6c6))                                  // 0x0044a6c2..0x0044a6c6
                    break;
            }
        }
        if (!CZ(++x < 50, 0x0044a6d0))                                          // 0x0044a6cc..0x0044a6d0
            break;
    }
}
typedef void(__cdecl* RTD_t)(int, int);
RTD_t RebuildTileData_orig;
// 0x0044a5b0
SG_HOOK("golf_clean.exe", 0x0044a5b0, rebuildTileData, RebuildTileData_re, RebuildTileData_orig);

// 0x00449790  initTerrain(w, h) (cdecl). GetVersionExA (IAT 0x004ba09c, 0x004497a3) on an OSVERSIONINFOA of 0x94 bytes
// (0x0044979a); dwMajorVersion (+4) >= 5 (unsigned `jb` 0x004497ae) sets the byte [0x00820f28] = 1 (0x004497b0). w and h
// are stored at 0x00820f30 / 0x00820f34 (0x004497ce / 0x004497d4). The 0x17 type records from 0x00578372 (stride 0x30,
// `cmp eax, 0x5787c2; jl` 0x0044980d) give an int array: each record's signed byte +4 (0x004497e3), replaced by 4 when the
// byte [0x005a34e0] is 1 (0x004497e7), the record's byte +0 is > 0 (signed `jle` 0x004497f1) and the record is neither
// 0x00578462 (type 5, 0x004497f3) nor 0x005784c2 (type 7, 0x004497fa). Then Terrain::passCollarInfo(array, 0x17)
// (0x00449821); Terrain::initSystem(w, h, hdc, the byte [0x00820f28]) where hdc is the result of slot +0x28 of the object
// [0x00519cd8] (0x00449833 / 0x00449835; Surface::acquireDC takes no argument, so the byte pushed at 0x00449832 is
// initSystem's fourth); Terrain::initTerrain (0x0044984c); rebuildTerrainGrid 0x00449540 (0x00449851), whose eax is
// returned.
int __cdecl InitTerrain_re(int w, int h) {
    unsigned char info[0x94] = {0};
    F<unsigned>(info, 0) = 0x94;                                                // 0x0044979a
    (*reinterpret_cast<int(__stdcall**)(void*)>(0x004ba09c))(info);             // 0x004497a3
    if (CZ(F<unsigned>(info, 4) >= 5, 0x004497ae))                              // 0x004497a9 / 0x004497ae
        G<unsigned char>(0x00820f28) = 1;                                       // 0x004497b0
    const unsigned char mode = G<unsigned char>(0x005a34e0);                    // 0x004497b7
    G<int>(0x00820f30) = w;                                                     // 0x004497ce
    G<int>(0x00820f34) = h;                                                     // 0x004497d4
    int collar[0x17];
    for (int t = 0; t < 0x17; ++t) {                                            // 0x004497e3..0x00449812
        const unsigned rec = 0x00578372 + 0x30 * t;
        collar[t] = G<signed char>(rec + 4);                                    // 0x004497e3 / 0x004497ea
        if (CZ(mode == 1, 0x004497ec) && CZ(G<signed char>(rec) > 0, 0x004497f1) &&
            CZ(rec != 0x00578462, 0x004497f8) && CZ(rec != 0x005784c2, 0x004497ff))
            collar[t] = 4;                                                      // 0x00449801
        CZ(t < 0x16, 0x00449812);
    }
    kTerPassCollarInfo(Terrain(), 0, P(collar), 0x17);                          // 0x00449821
    void* surf = G<void*>(0x00519cd8);                                          // 0x00449826
    const unsigned flag = G<unsigned char>(0x00820f28);                         // 0x0044982c
    const int hdc = (*reinterpret_cast<T0_t*>(F<char*>(surf, 0) + 0x28))(surf, 0);  // 0x00449835
    kTerInitSystem(Terrain(), 0, w, h, hdc, flag);                              // 0x00449841
    kTerInitTerrain(Terrain(), 0);                                              // 0x0044984c
    return kRebuildTerrainGrid();                                               // 0x00449851
}
typedef int(__cdecl* IT_t)(int, int);
IT_t InitTerrain_orig;
// 0x00449790
SG_HOOK("golf_clean.exe", 0x00449790, initTerrain, InitTerrain_re, InitTerrain_orig);

namespace {

const T7_t kSurfaceBlit = reinterpret_cast<T7_t>(0x00475c60);                  // Surface_blitThunk (thiscall, 7)
void* Screen() { return G<void*>(0x004c1574); }

}  // namespace

// 0x0044a6e0  renderTerrainStrip(surf, x, y, dx, dy, rot, zoom) (cdecl; surf is never read, the screen [0x004c1574] is
// re-read at 0x0044ab7a). The screen size [0x00820f30] x [0x00820f34] picks four widths (0x0044a6e8..0x0044a785):
// 800 x 600 -> A 0x20, L10 0x14, L14 0x28, L18 0x40; 1024 x 768 -> 0x28, 0x1a, 0x34, 0x50; 1280 x 1024 -> 0x34, 0x22,
// 0x44, 0x68 (any other size leaves A = rot and the three locals uninitialised, 0x0044a787). The shift s is 2 / 1 / 0 for
// zoom 1 / 2 / 4, else rot (0x0044a78f..0x0044a7b0). r2 = 6 for rot 2, 2 for rot 6, else rot (0x0044a7b4..0x0044a7ce).
// The step (dx, dy) gives a direction D (the zoom slot, 0x0044a7d8..0x0044a864): (-2,0) 7; (-1,-1) 8, (-1,0) 7, (-1,1) 6;
// (0,-2)/(0,-1) 1, (0,1)/(0,2) 5; (1,-1) 2, (1,0) 3, (1,1) 4; (2,0) 3; any other step leaves D = zoom. e = D + r2,
// folded to e % 9 + 1 when > 8 (0x0044a872..0x0044a883), selects the strip (switch 0x0044a894, table 0x0044abc4); each case
// fills the blit's source x, y, width, height and destination x, y (registers ebp, edi, esi, ebx and the two argument
// slots [esp+0x30] / [esp+0x2c]), with m = |dx| or |dy| depending on rot in {0, 4} (Strip below). Then
// Terrain::stripRender(tileAt(x, y), D, angle(rot)) (0x0044ab67) and Surface_blitThunk on the screen with source
// 0x00519cd4 (the Surface sub-object of the window 0x00519a60, `neg; sbb; and` 0x0044ab81..0x0044ab87); its eax is
// returned.
int __cdecl RenderTerrainStrip_re(int surf, int x, int y, int dx, int dy, int rot, int zoom) {
    (void)surf;
    const int w = G<int>(0x00820f30), h = G<int>(0x00820f34);
    int A = rot, L10 = 0, L14 = 0, L18 = 0;
    if (CZ(w == 800, 0x0044a6f1)) {                                                       // 0x0044a6e8 / 0x0044a6f1
        if (CZ(h == 600, 0x0044a6fd)) { A = 0x20; L10 = 0x14; L14 = 0x28; L18 = 0x40; }   // 0x0044a6f3..0x0044a721
    } else if (CZ(w == 0x400, 0x0044a728)) {                                              // 0x0044a723 / 0x0044a728
        if (CZ(h == 0x300, 0x0044a734)) { A = 0x28; L10 = 0x1a; L14 = 0x34; L18 = 0x50; } // 0x0044a72a..0x0044a753
    } else if (CZ(w == 0x500, 0x0044a75a) && CZ(h == 0x400, 0x0044a766)) {                // 0x0044a755..0x0044a766
        A = 0x34; L10 = 0x22; L14 = 0x44; L18 = 0x68;                                     // 0x0044a768..0x0044a785
    }
    // `sar r, cl` uses cl & 31
    const int s = (CZ(zoom == 1, 0x0044a794) ? 2 : CZ(zoom == 2, 0x0044a797) ? 1 :       // 0x0044a78f..0x0044a7b0
                   CZ(zoom == 4, 0x0044a79c) ? 0 : rot) & 31;
    const int r2 = CZ(rot == 2, 0x0044a7bb) ? 6 : CZ(rot == 6, 0x0044a7cc) ? 2 : rot;     // 0x0044a7b4..0x0044a7ce
    int D = zoom;                                                                         // the zoom slot
    switch (dx) {                                                                         // 0x0044a7d8, table 0x0044ab9c
        case -2: CZ(true, 0x0044ab9c); if (CZ(dy == 0, 0x0044a862)) D = 7; break;        // 0x0044a860
        case -1:                                                                          // 0x0044a830
            CZ(true, 0x0044ab9d);
            if (CZ(dy == -1, 0x0044a833)) D = 8;
            else if (CZ(dy == 0, 0x0044a837)) D = 7;
            else if (CZ(dy == 1, 0x0044a83c)) D = 6;
            break;
        case 0:                                                                           // 0x0044a7eb, table 0x0044abb0
            CZ(true, 0x0044ab9e);
            if (CZ(static_cast<unsigned>(dy + 2) > 4u, 0x0044a7f1)) break;              // `ja` 0x0044a7f1
            if (dy == -2 || dy == -1) D = 1;                                              // 0x0044a804
            else if (dy == 1 || dy == 2) D = 5;                                           // 0x0044a7fa
            CZ(true, 0x0044abb0 + static_cast<unsigned>(dy + 2));
            break;
        case 1:                                                                           // 0x0044a80e
            CZ(true, 0x0044ab9f);
            if (CZ(dy == -1, 0x0044a811)) D = 2;
            else if (CZ(dy == 0, 0x0044a815)) D = 3;
            else if (CZ(dy == 1, 0x0044a81a)) D = 4;
            break;
        case 2: CZ(true, 0x0044aba0); if (CZ(dy == 0, 0x0044a854)) D = 3; break;         // 0x0044a852
        default: CZ(true, 0x0044a7de); break;                                             // `ja` 0x0044a7de
    }
    int e = D + r2;                                                                       // 0x0044a870
    if (CZ(e > 8, 0x0044a875)) e = e % 9 + 1;                                             // 0x0044a875..0x0044a883
    const bool side = rot != 0 && rot != 4;                                               // e.g. 0x0044a8a3 / 0x0044a8a7
    const int ad = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;                             // cdq; xor; sub
    const int As = A >> s, L10s = L10 >> s;                                               // sar edx, cl / sar ebx, cl
    int bp = 0, di = 0, si = 0, bx = 0, X = dy, Y = dx;
    // the rot-in-{0, 4} tests of cases 1, 3, 5, 7 (`je` at +0 / +1 of each pair)
    static const unsigned kSide[9] = {0, 0x0044a8a5, 0, 0x0044a937, 0, 0x0044a9c7, 0, 0x0044aa5a, 0};
    if (e >= 1 && e <= 8) {                                                               // table 0x0044abc4
        CZ(true, 0x0044abc4 + static_cast<unsigned>(e - 1));
        if (kSide[e]) {
            CZ(rot == 0, kSide[e]);
            if (rot != 0) CZ(rot == 4, kSide[e] + 5);
        }
    } else {
        CZ(true, 0x0044a88e);                                                             // `ja` 0x0044a88e
    }
    switch (e) {
        case 1: {                                                                         // 0x0044a89b
            const int m = side ? ad : ay;
            si = As * m; bx = L10s * m; X = w - As; Y = h - L10s;
            break;
        }
        case 2: bx = L14 >> s; X = w; Y = h - bx; break;                                  // 0x0044a90b
        case 3: {                                                                         // 0x0044a931
            const int m = side ? ay : ad;
            bp = As * m; bx = L10s * m; X = w - As; Y = h - L10s;
            break;
        }
        case 4: bp = L18 >> s; X = w - bp; Y = h; break;                                  // 0x0044a99b
        case 5: {                                                                         // 0x0044a9c1
            const int m = side ? ad : ay;
            bp = As * m; di = L10s * m; X = w - As; Y = h - L10s;
            break;
        }
        case 6: di = L14 >> s; X = w; Y = h - di; break;                                  // 0x0044aa2d
        case 7: {                                                                         // 0x0044aa52
            const int m = side ? ay : ad;
            di = L10s * m; si = As * m; X = w - As; Y = h - L10s;
            break;
        }
        case 8: si = L18 >> s; X = w - si; Y = h; break;                                  // 0x0044aab9
        default: bp = di = si = bx = rot; break;                                          // 0x0044aadc
    }
    kTerStripRender(Terrain(), 0, P(TileAt(x, y, 0x0044ab31)), D, RotAngle(rot, 0x0044abe4));  // 0x0044aaec..0x0044ab67
    return kSurfaceBlit(Screen(), 0, 0x00519cd4, bp, di, si, bx, X, Y);                    // 0x0044ab8f
}
typedef int(__cdecl* RTS_t)(int, int, int, int, int, int, int);
RTS_t RenderTerrainStrip_orig;
// 0x0044a6e0
SG_HOOK("golf_clean.exe", 0x0044a6e0, renderTerrainStrip, RenderTerrainStrip_re, RenderTerrainStrip_orig);

namespace {

typedef int(__fastcall* Paint_t)(void* self, void* edx, unsigned a);
typedef int(__fastcall* Erase_t)(void* self, void* edx, void* out, void* first, void* last);
const Paint_t kWindowPaint = reinterpret_cast<Paint_t>(0x004808c0);
const Erase_t kListEraseRange = reinterpret_cast<Erase_t>(0x0044ac00);

// The whole-screen blit from the window surface 0x00519cd4 to the screen (0x0044997d..0x0044998d, 0x00449e35..0x00449e45)
int BlitFull() {
    return kSurfaceBlit(reinterpret_cast<void*>(0x00519cd4), 0, P(Screen()), 0, 0, 0, 0, G<int>(0x00820f30),
                        G<int>(0x00820f34));
}
void StoreCamera(int x, int y) {
    G<int>(0x004d204c) = y;
    G<int>(0x004d2048) = x;
}

}  // namespace

// 0x004498a0  updateTerrainRender(x, y, zoom, rot) (cdecl). Globals: the byte [0x004d2044] (first run), [0x00820f2a]
// (grid synced by syncTiles), the last camera [0x004d2048] / [0x004d204c], rotation [0x00820f38] and zoom [0x004d2050].
//   [0x004d2044] set (0x004498ab): initTerrain([0x00822c8c], [0x00822c90]) (0x00449d7a), then as below;
//   either byte set (0x004498b8): Window::paint(0x00519a60, 0x80000000) (0x00449d8c), Terrain::render(tileAt(x, y),
//     angle(rot)) (0x00449e05), both bytes cleared and the camera stored (0x00449e0a..0x00449e1e), the full blit;
//   rot != [0x00820f38] (0x004498e6): paint, render, full blit, camera stored, [0x00820f38] = rot (0x004499a0);
//   zoom != [0x004d2050] (0x004499b5): Terrain::setZoomLevel(zoom) (0x004499c4), paint, render, full blit, camera,
//     [0x004d2050] = zoom (0x00449a7d);
//   no move (dx = x - [0x004d2048], dy = y - [0x004d204c] both 0, 0x00449a88 / 0x00449a90): the screen-to-window blit
//     (Surface_blitThunk on the screen, source 0x00519cd4, 0x00449abd); when bit 2 of [0x005619a0] is clear
//     (0x00449acb) rebuildTileData(x, y) and the bit set (0x00449acf..0x00449ade). With the list 0x00820f18 empty
//     ([0x00820f20] = 0, 0x00449aea) it returns 0. Else Terrain::calcAllNormals(tileAt(x, y)) when [0x00820f2c]
//     (0x00449af7 / 0x00449b2a, then cleared), Terrain::localRender(node value, tileAt(x, y), angle(rot)) for every node
//     from the head's next (iterator [0x00820f14], 0x00449b36..0x00449bd4), listEraseRange(0x00820f18, &zoom slot,
//     first, head) (0x00449be8), Terrain::pathUpdateRender(tileAt(x, y), angle(rot)) when [0x00820f2b] (0x00449bf4 /
//     0x00449c66, then cleared), and the full blit (0x00449e45);
//   |dx| + |dy| > 2 (0x00449c96): paint, render, full blit, camera; else paint and renderTerrainStrip(screen, x, y, dx,
//     dy, rot, zoom) (0x00449d63), full blit, camera.
// eax is the last call's (the full blit's, or 0 on the empty-list exit).
int __cdecl UpdateTerrainRender_re(int x, int y, int zoom, int rot) {
    void* const win = reinterpret_cast<void*>(0x00519a60);
    const bool first = CZ(G<unsigned char>(0x004d2044) != 0, 0x004498ab);       // 0x004498ab
    if (first || CZ(G<unsigned char>(0x00820f2a) != 0, 0x004498b8)) {           // 0x004498b8
        if (first)
            reinterpret_cast<IT_t>(0x00449790)(G<int>(0x00822c8c), G<int>(0x00822c90));  // 0x00449d7a
        kWindowPaint(win, 0, 0x80000000u);                                      // 0x00449d8c
        kTerRender(Terrain(), 0, P(TileAt(x, y, 0x00449dda)), RotAngle(rot, 0x00449edc));  // 0x00449e05
        G<unsigned char>(0x00820f2a) = 0;                                       // 0x00449e0a
        G<unsigned char>(0x004d2044) = 0;                                       // 0x00449e11
        G<int>(0x004d2048) = x;                                                 // 0x00449e18
        G<int>(0x004d204c) = y;                                                 // 0x00449e1e
        return BlitFull();                                                      // 0x00449e45
    }
    const int dx = x - G<int>(0x004d2048);                                      // 0x004498da
    const int dy = y - G<int>(0x004d204c);                                      // 0x004498e2
    if (!CZ(rot == G<int>(0x00820f38), 0x004498e6)) {                           // 0x004498e4 / 0x004498e6
        kWindowPaint(win, 0, 0x80000000u);                                      // 0x004498f6
        kTerRender(Terrain(), 0, P(TileAt(x, y, 0x0044993c)), RotAngle(rot, 0x00449e50));  // 0x00449967
        const int r = BlitFull();                                               // 0x0044998d
        StoreCamera(x, y);                                                      // 0x00449992 / 0x00449998
        G<int>(0x00820f38) = rot;                                               // 0x004499a0
        return r;
    }
    if (!CZ(zoom == G<int>(0x004d2050), 0x004499b5)) {                          // 0x004499b3 / 0x004499b5
        kTerSetZoomLevel(Terrain(), 0, zoom);                                   // 0x004499c4
        kWindowPaint(win, 0, 0x80000000u);                                      // 0x004499d3
        kTerRender(Terrain(), 0, P(TileAt(x, y, 0x00449a19)), RotAngle(rot, 0x00449e6c));  // 0x00449a44
        const int r = BlitFull();                                               // 0x00449a6a
        StoreCamera(x, y);
        G<int>(0x004d2050) = zoom;                                              // 0x00449a7d
        return r;
    }
    if (CZ(dy == 0, 0x00449a88) && CZ(dx == 0, 0x00449a90)) {                   // 0x00449a88 / 0x00449a90
        kSurfaceBlit(Screen(), 0, 0x00519cd4, 0, 0, 0, 0, G<int>(0x00820f30), G<int>(0x00820f34));  // 0x00449abd
        if (!CZ((G<unsigned char>(0x005619a0) & 2) != 0, 0x00449acb)) {         // 0x00449ac9 / 0x00449acb
            reinterpret_cast<RTD_t>(0x0044a5b0)(x, y);                          // 0x00449acf
            G<unsigned char>(0x005619a0) |= 2;                                  // 0x00449adc / 0x00449ade
        }
        if (CZ(G<int>(0x00820f20) == 0, 0x00449aea))                            // 0x00449ae3 / 0x00449aea
            return 0;
        if (CZ(NeedNormals() != 0, 0x00449af7)) {                               // 0x00449af0 / 0x00449af7
            kTerCalcAllNormals(Terrain(), 0, P(TileAt(x, y, 0x00449b04)));      // 0x00449b2a
            NeedNormals() = 0;                                                  // 0x00449b2f
        }
        char* head = G<char*>(0x00820f1c);
        char*& it = G<char*>(0x00820f14);
        it = F<char*>(head, 0);                                                 // 0x00449b3b / 0x00449b3f
        if (!CZ(it == head, 0x00449b45)) {
            do {
                kTerLocalRender(Terrain(), 0, F<unsigned>(it, 8), P(TileAt(x, y, 0x00449b8c)),
                                RotAngle(rot, 0x00449e88));                     // 0x00449bbb
                it = F<char*>(it, 0);                                           // 0x00449bc0..0x00449bce
            } while (CZ(it != G<char*>(0x00820f1c), 0x00449bd4));               // 0x00449bcc / 0x00449bd4
        }
        head = G<char*>(0x00820f1c);                                            // 0x00449bc7
        kListEraseRange(reinterpret_cast<void*>(0x00820f18), 0, &zoom, F<char*>(head, 0), head);  // 0x00449be8
        if (CZ(NeedPathRender() != 0, 0x00449bf4)) {                            // 0x00449bed / 0x00449bf4
            kTerPathUpdateRender(Terrain(), 0, P(TileAt(x, y, 0x00449c3b)), RotAngle(rot, 0x00449ea4));  // 0x00449c66
            NeedPathRender() = 0;                                               // 0x00449c6b
        }
        return BlitFull();                                                      // 0x00449e45
    }
    const int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    kWindowPaint(win, 0, 0x80000000u);                                          // 0x00449c9c / 0x00449d49
    if (CZ(ady + adx > 2, 0x00449c96)) {                                        // 0x00449c8e / 0x00449c96
        kTerRender(Terrain(), 0, P(TileAt(x, y, 0x00449ce2)), RotAngle(rot, 0x00449ec0));  // 0x00449d0d
    } else {
        reinterpret_cast<RTS_t>(0x0044a6e0)(static_cast<int>(P(Screen())), x, y, dx, dy, rot, zoom);  // 0x00449d63
    }
    const int r = BlitFull();                                                   // 0x00449d33
    StoreCamera(x, y);                                                          // 0x00449d38 / 0x00449d3e
    return r;
}
typedef int(__cdecl* UTR_t)(int, int, int, int);
UTR_t UpdateTerrainRender_orig;
// 0x004498a0
SG_HOOK("golf_clean.exe", 0x004498a0, updateTerrainRender, UpdateTerrainRender_re, UpdateTerrainRender_orig);

// =====================================================================================================================
// The net family: thiscall methods of the network session object (the pointer [0x008400b8] that gfxFlushA / gfxFlushB
// 0x00497b00 / 0x00497b20 pass when it and [0x008400b0] are set). [0x008400b0] is a COM-style interface: `push iface;
// mov edx, [iface]; call [edx + slot]` (stdcall, the interface first). Fields used here: +4 a receive buffer, +0x14 ..
// +0x24 handles, +0x20 an event, +0x34 / +0x38 a critical section and its flag, +0x60 a mode, +0x64 a sub-object with a
// vtable, +0xc0 a list (S3List), +0xe4 / +0xe8 / +0xec / +0xf0 a value, the flag word, a time and a timeout, +0x148 a
// locked queue (queuePop 0x004a4c70), +0x170 16 peer slots of 0x58 bytes (+0 id, +0x10 a time, +0x14 a flag byte, +0x15
// a name), +0x6f8 a count, +0x77c / +0x780 two peer ids (equal when this side is the host, as the code reads them).
// =====================================================================================================================
namespace {

typedef unsigned(__fastcall* M0_t)(void* self, void* edx);
typedef unsigned(__fastcall* M1_t)(void* self, void* edx, unsigned a);
typedef unsigned(__fastcall* M2_t)(void* self, void* edx, unsigned a, unsigned b);
typedef unsigned(__fastcall* M3_t)(void* self, void* edx, unsigned a, unsigned b, unsigned c);
typedef unsigned(__fastcall* M5_t)(void* self, void* edx, unsigned a, unsigned b, unsigned c, unsigned d, unsigned e);
typedef unsigned(__fastcall* Pop_t)(void* self, void* edx, void* dst, unsigned* outA, unsigned* outB, unsigned* outSize);
typedef unsigned(__stdcall* S0_t)();
typedef unsigned(__stdcall* S1_t)(unsigned a);
typedef unsigned(__stdcall* S4_t)(unsigned a, unsigned b, unsigned c, unsigned d);
typedef void*(__cdecl* Malloc_t)(unsigned);
typedef void(__cdecl* Free_t)(void*);

const M5_t kNetService = reinterpret_cast<M5_t>(0x00497fc0);
const M5_t kNetSendMessages = reinterpret_cast<M5_t>(0x00499140);
const M2_t kNetHandleControl = reinterpret_cast<M2_t>(0x0049acf0);
const M0_t kNetSendList = reinterpret_cast<M0_t>(0x00497b40);
const M0_t kNetSendShutdown = reinterpret_cast<M0_t>(0x00497d10);
const M3_t kNetQueueMessage = reinterpret_cast<M3_t>(0x0049b690);
const M1_t kS3ListAddVoiceRx = reinterpret_cast<M1_t>(0x0049b970);
const M0_t kListClear = reinterpret_cast<M0_t>(0x004a4b00);
const Pop_t kQueuePop = reinterpret_cast<Pop_t>(0x004a4c70);
const Malloc_t kMalloc = reinterpret_cast<Malloc_t>(0x004a504f);
const Free_t kFree = reinterpret_cast<Free_t>(0x004a5007);

// an import, called through the exe's IAT slot as the originals do (importstandin.js patches that slot)
template <class Fn> Fn Iat(unsigned slot) { return reinterpret_cast<Fn>(G<void*>(slot)); }
// slot `off` of the vtable at [obj]
template <class Fn> Fn V(void* obj, unsigned off) { return *reinterpret_cast<Fn*>(F<char*>(obj, 0) + off); }
void* Iface() { return G<void*>(0x008400b0); }
char* Peer(void* self, int k) { return static_cast<char*>(self) + 0x170 + 0x58 * k; }
char* At(void* self, unsigned off) { return static_cast<char*>(self) + off; }
// strcat into an empty buffer: the `repne scasb` length (terminator included) and `rep movsd / movsb` copy
void CopyString(char* dst, const char* src) {
    unsigned n = 0;
    while (src[n]) ++n;
    for (unsigned i = 0; i <= n; ++i) dst[i] = src[i];
}

}  // namespace

// 0x0049aa70  netPump (thiscall, no argument). The sub-object this+0x64 is asked for its pending count (slot +0x94).
// With bit 0x40000000 of the flag word +0xe8 set (0x0049aa7a): a count >= 0x32 (unsigned `jae` 0x0049aa91) returns it,
// else the bit is cleared and the new flag word returned (0x0049aa97..0x0049aaa3). With the bit clear: a count > 0x64
// (`jbe` 0x0049aabd) sets the bit and returns slot +0x20's result (0x0049aad1); otherwise slot +0x88 gives the next
// message (0x0049aadc), NULL returns 0 (0x0049aae6), and the message's bytes [+0x18, +0x20) go to netService 0x00497fc0
// when the mode +0x60 is nonzero (0x0049ab09), else to netSendMessages 0x00499140 (0x0049ab23), each with (+0xe4, 0x100, 0);
// then slot +0x8c releases the message (0x0049ab13 / 0x0049ab2d) and its eax is returned.
unsigned __fastcall NetPump_re(void* self, void*) {
    char* sub = At(self, 0x64);
    unsigned& flags = F<unsigned>(self, 0xe8);
    if (CZ((flags & 0x40000000) != 0, 0x0049aa80)) {                            // 0x0049aa7a / 0x0049aa80
        const unsigned n = V<M0_t>(sub, 0x94)(sub, 0);                          // 0x0049aa88
        if (CZ(n >= 0x32, 0x0049aa91))                                          // 0x0049aa91
            return n;
        flags &= 0xbfffffff;                                                    // 0x0049aa9e / 0x0049aaa3
        return flags;
    }
    const unsigned n = V<M0_t>(sub, 0x94)(sub, 0);                              // 0x0049aab4
    if (CZ(n > 0x64, 0x0049aabd)) {                                             // 0x0049aabd
        flags |= 0x40000000;                                                    // 0x0049aac7 / 0x0049aac9
        return V<M0_t>(sub, 0x20)(sub, 0);                                      // 0x0049aad1
    }
    char* msg = reinterpret_cast<char*>(V<M0_t>(sub, 0x88)(sub, 0));            // 0x0049aadc
    if (CZ(msg == 0, 0x0049aae6))                                               // 0x0049aae6
        return 0;
    const unsigned b = F<unsigned>(msg, 0x18), len = F<unsigned>(msg, 0x20) - b;  // 0x0049aaf1 / 0x0049aaf8 / sub
    if (CZ(F<unsigned>(self, 0x60) != 0, 0x0049ab01))                           // 0x0049aae8 / 0x0049ab01
        kNetService(self, 0, b, len, F<unsigned>(self, 0xe4), 0x100, 0);        // 0x0049ab09
    else
        kNetSendMessages(self, 0, b, len, F<unsigned>(self, 0xe4), 0x100, 0);   // 0x0049ab23
    return V<M1_t>(sub, 0x8c)(sub, 0, P(msg));                                  // 0x0049ab13 / 0x0049ab2d
}
M0_t NetPump_orig;
// 0x0049aa70
SG_HOOK("golf_clean.exe", 0x0049aa70, netPump, NetPump_re, NetPump_orig);

// 0x00497b40  netSendList (thiscall, no argument; nothing is returned, its one caller netQueueMessage overwrites eax at
// 0x0049b74d). Only when +0x77c equals +0x780 (`jne` 0x00497b5a) and the count n = +0x6f8 is nonzero (0x00497b68): a
// block of n * 0x2c + 4 bytes from malloc 0x004a504f (0x00497b6e..0x00497b81) gets n in its first byte (0x00497b91) and,
// for each of the first n peer slots (0x00497ba2..0x00497bee), a record of 0x28 bytes from +4: the slot's id, its name
// (the slot's byte +0x34, i.e. name byte 0x1f, is cleared first, 0x00497bb3, so at most 31 characters are copied, `repne
// scasb` / `rep movs` 0x00497bba..0x00497bdf) and its flag byte +0x14 as a dword at +0x24 (0x00497be4 / 0x00497beb).
// netSendMessages(block, size, 0, 0x10, 1) (0x00497c06), then free 0x004a5007 (0x00497c11).
void __fastcall NetSendList_re(void* self, void*) {
    if (!CZ(F<unsigned>(self, 0x77c) == F<unsigned>(self, 0x780), 0x00497b5a))  // 0x00497b58 / 0x00497b5a
        return;
    const int n = F<int>(self, 0x6f8);                                          // 0x00497b60
    if (CZ(n == 0, 0x00497b68))                                                 // 0x00497b68
        return;
    const unsigned size = static_cast<unsigned>(n) * 0x2c + 4;                  // 0x00497b6e..0x00497b75
    char* buf = static_cast<char*>(kMalloc(size));                              // 0x00497b81
    buf[0] = static_cast<char>(n);                                              // 0x00497b91
    if (!CZ(n <= 0, 0x00497b93)) {                                              // 0x00497b93
        int k = 0;
        do {
            char* s = Peer(self, k);
            char* rec = buf + 4 + 0x28 * k;
            F<unsigned>(rec, 0) = F<unsigned>(s, 0);                            // 0x00497ba2 / 0x00497ba8
            rec[4] = 0;                                                         // 0x00497bab
            s[0x34] = 0;                                                        // 0x00497bb3
            CopyString(rec + 4, s + 0x15);                                      // 0x00497bba..0x00497bdf
            F<unsigned>(rec, 0x24) = static_cast<unsigned char>(s[0x14]);       // 0x00497be4 / 0x00497beb
        } while (CZ(++k != n, 0x00497bee));                                     // 0x00497be3 / 0x00497bee
    }
    kNetSendMessages(self, 0, P(buf), size, 0, 0x10, 1);                        // 0x00497c06
    if (CZ(buf != 0, 0x00497c0e))                                               // 0x00497c0b / 0x00497c0e
        kFree(buf);                                                             // 0x00497c11
}
M0_t NetSendList_orig;
// 0x00497b40
SG_HOOK("golf_clean.exe", 0x00497b40, netSendList, NetSendList_re, NetSendList_orig);

// 0x0049b690  netQueueMessage(name, id, flags) (thiscall, `ret 0xc`). The 16 peer slots are searched (0x0049b6a1..
// 0x0049b6b6): a slot with this id returns the id at once (0x0049b6a9); the first empty slot (id 0, 0x0049b6a5) takes
// the id, the flag byte at +0x14 and the name at +0x15 (0x0049b6d1..0x0049b718), and bit 0 of flags (0x0049b71a) copies
// the id to +0x780 (0x0049b723). With no empty slot index 16 is used from here on (the record at this+0x6f0). Then the
// count +0x6f8 grows (0x0049b73b / 0x0049b73e), netSendList runs when +0x77c equals +0x780 (0x0049b744 / 0x0049b748), the
// object's own slot +8 gets (name, id) of that slot (0x0049b76b), S3List_addVoiceRx 0x0049b970 on +0xc0 gets the id
// (0x0049b77d), and slot +0x84 of the list's current payload ([[+0xc0 + 0xc] + 8], or NULL when +0xc8 is 0,
// 0x0049b782..0x0049b791) is called with (0x1000, 0, 0) (0x0049b79e); its eax is returned.
unsigned __fastcall NetQueueMessage_re(void* self, void*, const char* name, unsigned id, unsigned flags) {
    int k = 0;
    for (;;) {
        const unsigned v = F<unsigned>(Peer(self, k), 0);                       // 0x0049b6a1
        if (CZ(v == 0, 0x0049b6a5))                                             // 0x0049b6a3 / 0x0049b6a5
            break;
        if (CZ(v == id, 0x0049b6a9))                                            // 0x0049b6a7 / 0x0049b6a9
            return v;                                                           // 0x0049b7a4
        if (!CZ(++k < 16, 0x0049b6b6))                                          // 0x0049b6af..0x0049b6b6
            break;
    }
    if (!CZ(k >= 16, 0x0049b6bd)) {                                             // 0x0049b6ba / 0x0049b6bd
        char* s = Peer(self, k);
        F<unsigned>(s, 0) = id;                                                 // 0x0049b6d1
        s[0x14] = static_cast<char>(flags);                                     // 0x0049b6db
        s[0x15] = 0;                                                            // 0x0049b6e9
        CopyString(s + 0x15, name);                                             // 0x0049b6ec..0x0049b718
        if (CZ((flags & 1) != 0, 0x0049b71d))                                   // 0x0049b71a / 0x0049b71d
            F<unsigned>(self, 0x780) = id;                                      // 0x0049b723
    }
    ++F<int>(self, 0x6f8);                                                      // 0x0049b73b / 0x0049b73e
    if (CZ(F<unsigned>(self, 0x77c) == F<unsigned>(self, 0x780), 0x0049b744))  // 0x0049b73c / 0x0049b744
        kNetSendList(self, 0);                                                  // 0x0049b748
    char* s = Peer(self, k);
    V<M2_t>(self, 8)(self, 0, P(s + 0x15), F<unsigned>(s, 0));                  // 0x0049b76b
    char* list = At(self, 0xc0);
    kS3ListAddVoiceRx(list, 0, F<unsigned>(s, 0));                              // 0x0049b77d
    void* obj = CZ(F<unsigned>(list, 8) != 0, 0x0049b787) ? F<void*>(F<char*>(list, 0xc), 8) : 0;  // 0x0049b782..0x0049b791
    return V<M3_t>(obj, 0x84)(obj, 0, 0x1000, 0, 0);                            // 0x0049b79e
}
typedef unsigned(__fastcall* NQM_t)(void*, void*, const char*, unsigned, unsigned);
NQM_t NetQueueMessage_orig;
// 0x0049b690
SG_HOOK("golf_clean.exe", 0x0049b690, netQueueMessage, NetQueueMessage_re, NetQueueMessage_orig);

#if 0  // netPoll: reimplementation drafted, not A/B'd this round (round-34 lead, see log/c3/c3ez_notes.txt).
// 0x0049b7b0  netPoll (thiscall, no argument). Returns 0 when the interface [0x008400b0] is NULL (0x0049b7be) and that
// pointer when the receive buffer +4 is NULL (0x0049b7c9). Otherwise every message queuePop 0x004a4c70 takes from the
// queue +0x148 into the buffer (0x0049b7df / 0x0049b95b, loop `jne` 0x0049b962) is dispatched on queuePop's outA word:
//   nonzero (0x0049b7f4): bit 0x80 of the buffer's first byte (0x0049b7f9) -> netHandleControl 0x0049acf0(+8, 1)
//     (0x0049b805); else bit 0x10 (0x0049b80f) -> netQueueMessage(entry + 4, entry +0, entry +0x24) for the byte +8 entries
//     of 0x28 bytes from +0xc (the count re-read after each, 0x0049b83c..0x0049b844);
//   zero: the dword +0 is a message type: 0x31 -> the object's slot +0x14, then netSendShutdown 0x00497d10 (0x0049b8f0 /
//     0x0049b8f5); 3 with +0x77c == +0x780 and +4 == 1 -> slot +0x54 of the interface (iface, +8, 0x0083ff9c, &size 0x100)
//     (0x0049b8c2), then netQueueMessage([0x0083ffa8], +8, the byte at [+0x10] or 0) (0x0049b8e4); 5 with +4 == 1 ->
//     netHandleControl(+8, 1) (0x0049b87c); 0x102 -> the peer slot whose id is +8 (searched, 0x0049b91d..0x0049b933)
//     takes the byte at [+0xc] as its flag byte when [+0xc] is set (0x0049b938..0x0049b944); 0x103 -> the object's slot
//     +0x10 with (+0x18, +8) (0x0049b913); any other type is ignored.
// queuePop's 0 at the end is returned.
unsigned __fastcall NetPoll_re(void* self, void*) {
    if (CZ(Iface() == 0, 0x0049b7be))                                           // 0x0049b7b8 / 0x0049b7be
        return 0;
    char* msg = F<char*>(self, 4);                                              // 0x0049b7c4
    if (CZ(msg == 0, 0x0049b7c9))                                               // 0x0049b7c9
        return G<unsigned>(0x008400b0);
    void* q = At(self, 0x148);
    unsigned user = 0;                                                          // [esp+0x10]
    unsigned r = kQueuePop(q, 0, msg, &user, 0, 0);                             // 0x0049b7df
    if (CZ(r == 0, 0x0049b7e6))                                                 // 0x0049b7e6
        return 0;
    do {
        if (CZ(user != 0, 0x0049b7f4)) {                                        // 0x0049b7f2 / 0x0049b7f4
            const unsigned char b = msg[0];                                     // 0x0049b7f6
            if (CZ((b & 0x80) != 0, 0x0049b7fb)) {                              // 0x0049b7f9 / 0x0049b7fb
                kNetHandleControl(self, 0, F<unsigned>(msg, 8), 1);             // 0x0049b805
            } else if (CZ((b & 0x10) != 0, 0x0049b811)) {                       // 0x0049b80f / 0x0049b811
                if (!CZ(static_cast<unsigned char>(msg[8]) == 0, 0x0049b81e)) {  // 0x0049b81c / 0x0049b81e
                    unsigned k = 0;
                    do {
                        char* e = msg + 0xc + 0x28 * k;
                        kNetQueueMessage(self, 0, P(e + 4), F<unsigned>(e, 0), F<unsigned>(e, 0x24));  // 0x0049b834
                    } while (CZ(++k < static_cast<unsigned char>(msg[8]), 0x0049b844));  // 0x0049b83b..0x0049b844
                }
            }
        } else {
            const unsigned t = F<unsigned>(msg, 0);                             // 0x0049b84b
            if (CZ(t > 0x31, 0x0049b850)) {                                     // 0x0049b84d / 0x0049b850
                if (CZ(t == 0x102, 0x0049b901)) {                               // 0x0049b8fc / 0x0049b901
                    const unsigned id = F<unsigned>(msg, 8);
                    int k = 0;
                    while (!CZ(F<unsigned>(Peer(self, k), 0) == id, 0x0049b925) && CZ(++k < 16, 0x0049b92e)) {
                    }
                    if (!CZ(k == 16, 0x0049b933) && !CZ(F<void*>(msg, 0xc) == 0, 0x0049b93a))  // 0x0049b930..0x0049b93a
                        Peer(self, k)[0x14] = *F<char*>(msg, 0xc);              // 0x0049b93c / 0x0049b944
                } else if (CZ(t == 0x103, 0x0049b904)) {                        // 0x0049b903 / 0x0049b904
                    V<M2_t>(self, 0x10)(self, 0, F<unsigned>(msg, 0x18), F<unsigned>(msg, 8));  // 0x0049b913
                }
            } else if (CZ(t == 0x31, 0x0049b856)) {                             // 0x0049b856
                V<M0_t>(self, 0x14)(self, 0);                                   // 0x0049b8f0
                kNetSendShutdown(self, 0);                                      // 0x0049b8f5
            } else if (CZ(t == 3, 0x0049b85f)) {                                // 0x0049b85c / 0x0049b85f
                if (CZ(F<unsigned>(self, 0x77c) == F<unsigned>(self, 0x780), 0x0049b894) &&
                    CZ(F<unsigned>(msg, 4) == 1, 0x0049b89e)) {                 // 0x0049b892 / 0x0049b89a / 0x0049b89e
                    unsigned size = 0x100;                                      // 0x0049b8ad
                    void* dp = Iface();                                         // 0x0049b8a4
                    V<S4_t>(dp, 0x54)(P(dp), F<unsigned>(msg, 8), 0x0083ff9c, P(&size));  // 0x0049b8c2
                    const unsigned char* fp = F<unsigned char*>(msg, 0x10);     // 0x0049b8c5
                    const unsigned fl = CZ(fp != 0, 0x0049b8ca) ? *fp : 0;      // 0x0049b8c8..0x0049b8d4
                    kNetQueueMessage(self, 0, G<unsigned>(0x0083ffa8), F<unsigned>(msg, 8), fl);  // 0x0049b8e4
                }
            } else if (CZ(t == 5, 0x0049b864)) {                                // 0x0049b861 / 0x0049b864
                if (CZ(F<unsigned>(msg, 4) == 1, 0x0049b86e))                   // 0x0049b86a / 0x0049b86e
                    kNetHandleControl(self, 0, F<unsigned>(msg, 8), 1);         // 0x0049b87c
            }
        }
        r = kQueuePop(q, 0, msg, &user, 0, 0);                                  // 0x0049b95b
    } while (CZ(r != 0, 0x0049b962));                                           // 0x0049b960 / 0x0049b962
    return r;
}
M0_t NetPoll_orig;
// 0x0049b7b0
SG_HOOK("golf_clean.exe", 0x0049b7b0, netPoll, NetPoll_re, NetPoll_orig);
#endif

// 0x0049ab40  netServiceControl (thiscall, no argument). With the interface [0x008400b0] set (0x0049ab4e) and
// timeGetTime (IAT 0x004ba21c, the slot read once into ebp at 0x0049ab54) - 2000 above the time +0xec (unsigned `jbe`
// 0x0049ab69) and the guard [0x008400c0] clear (0x0049ab76): the guard is set around netSendMessages(0, 0, 0, 0x20, 0)
// (0x0049ab8d..0x0049aba7) and +0xec takes the time again (0x0049ab9a). Then, unless the guard [0x008400bc] is set
// (0x0049abb1), it is set (0x0049abb7) and, with bit 0x40000 of +0xe8 (0x0049abc8), drops are checked:
//   host (+0x77c == +0x780, 0x0049abdc): every peer slot whose id is neither 0 nor +0x77c and whose time +0x10 is set
//     and older than the timeout +0xf0 (0x0049abf0..0x0049ac0d) goes to netHandleControl(id, 0); a 0 result stamps its time
//     (0x0049ac1d / 0x0049ac21);
//   +0x77c equal to slot 1's id (+0x1c8, 0x0049ac48): slot 0's time +0x180, when set and older than the timeout
//     (0x0049ac52 / 0x0049ac64), gets netHandleControl(slot 0's id, 0) and a 0 result stamps it (0x0049ac7c);
//   otherwise: when slots 0 and 1 both have a time at least the timeout old (0x0049aca2 / 0x0049acb4; an unset or
//     younger time returns at once, 0x0049acea, LEAVING [0x008400bc] set), netHandleControl(-1, 0) and a 0 result stamps
//     +0x1d8 and +0x180 (0x0049acd8 / 0x0049acde).
// [0x008400bc] is cleared on every other exit after it was set (0x0049ac35 / 0x0049ac85 / 0x0049ace0). eax at each exit
// is the last value the code left there (a time difference, a guard, the flag word, a handler result, a time or 0).
unsigned __fastcall NetServiceControl_re(void* self, void*) {
    if (CZ(Iface() == 0, 0x0049ab4e))                                           // 0x0049ab49 / 0x0049ab4e
        return 0;
    const S0_t now = Iat<S0_t>(0x004ba21c);                                     // 0x0049ab54
    const unsigned t = now() - 0x7d0;                                           // 0x0049ab5a / 0x0049ab62
    if (CZ(t <= F<unsigned>(self, 0xec), 0x0049ab69))                           // 0x0049ab67 / 0x0049ab69
        return t;
    if (CZ(G<unsigned>(0x008400c0) != 0, 0x0049ab76))                           // 0x0049ab74 / 0x0049ab76
        return G<unsigned>(0x008400c0);
    G<unsigned>(0x008400c0) = 1;                                                // 0x0049ab8d
    kNetSendMessages(self, 0, 0, 0, 0, 0x20, 0);                                // 0x0049ab93
    F<unsigned>(self, 0xec) = now();                                            // 0x0049ab98 / 0x0049ab9a
    const unsigned bc = G<unsigned>(0x008400bc);                                // 0x0049aba0
    G<unsigned>(0x008400c0) = 0;                                                // 0x0049aba7
    if (CZ(bc != 0, 0x0049abb1))                                                // 0x0049aba5 / 0x0049abb1
        return bc;
    G<unsigned>(0x008400bc) = 1;                                                // 0x0049abb7
    const unsigned fl = F<unsigned>(self, 0xe8);                                // 0x0049abbd
    if (CZ(!(fl & 0x40000), 0x0049abc8)) {                                      // 0x0049abc3 / 0x0049abc8
        G<unsigned>(0x008400bc) = 0;                                            // 0x0049ace0
        return fl;
    }
    const unsigned host = F<unsigned>(self, 0x77c);                             // 0x0049abce
    if (CZ(host == F<unsigned>(self, 0x780), 0x0049abdc)) {                     // 0x0049abda / 0x0049abdc
        for (int k = 0; k < 16; ++k) {                                          // 0x0049abe4 / 0x0049ac30
            char* s = Peer(self, k);
            const unsigned id = F<unsigned>(s, 0);                              // 0x0049abec
            if (CZ(id == 0, 0x0049abf0) || CZ(id == F<unsigned>(self, 0x77c), 0x0049abf8))
                continue;
            const unsigned t0 = F<unsigned>(s, 0x10);                           // 0x0049abfa
            if (CZ(t0 == 0, 0x0049abff))
                continue;
            if (CZ(now() - t0 <= F<unsigned>(self, 0xf0), 0x0049ac0d))          // 0x0049ac01..0x0049ac0d
                continue;
            if (CZ(kNetHandleControl(self, 0, F<unsigned>(s, 0), 0) != 0, 0x0049ac1d))  // 0x0049ac16 / 0x0049ac1d
                continue;
            F<unsigned>(s, 0x10) = now();                                       // 0x0049ac1f / 0x0049ac21
        }
        G<unsigned>(0x008400bc) = 0;                                            // 0x0049ac35
        return 0;
    }
    if (CZ(host == F<unsigned>(self, 0x1c8), 0x0049ac48)) {                     // 0x0049ac42 / 0x0049ac48
        const unsigned t0 = F<unsigned>(self, 0x180);                           // 0x0049ac4a
        unsigned r = host;
        if (!CZ(t0 == 0, 0x0049ac52)) {                                         // 0x0049ac50 / 0x0049ac52
            r = now() - t0;                                                     // 0x0049ac58..0x0049ac60
            if (!CZ(r <= F<unsigned>(self, 0xf0), 0x0049ac64)) {                // 0x0049ac62 / 0x0049ac64
                r = kNetHandleControl(self, 0, F<unsigned>(self, 0x170), 0);    // 0x0049ac71
                if (!CZ(r != 0, 0x0049ac78)) {                                  // 0x0049ac76 / 0x0049ac78
                    r = now();                                                  // 0x0049ac7a
                    F<unsigned>(self, 0x180) = r;                               // 0x0049ac7c
                }
            }
        }
        G<unsigned>(0x008400bc) = 0;                                            // 0x0049ac85 / 0x0049ace0
        return r;
    }
    unsigned r = host;
    for (int k = 0; k < 2; ++k) {                                               // 0x0049acb6..0x0049acbd
        const unsigned t0 = F<unsigned>(Peer(self, k), 0x10);                   // 0x0049ac9e
        if (CZ(t0 == 0, 0x0049aca2))                                            // 0x0049aca0 / 0x0049aca2
            return r;                                                           // 0x0049acea
        r = Iat<S0_t>(0x004ba21c)() - t0;                                       // 0x0049aca4..0x0049acb0
        if (CZ(r < F<unsigned>(self, 0xf0), 0x0049acb4))                        // 0x0049acb2 / 0x0049acb4
            return r;                                                           // 0x0049acea
    }
    r = kNetHandleControl(self, 0, 0xffffffffu, 0);                             // 0x0049acc5
    if (!CZ(r != 0, 0x0049accc)) {                                              // 0x0049acca / 0x0049accc
        r = Iat<S0_t>(0x004ba21c)();                                            // 0x0049acce
        F<unsigned>(self, 0x1d8) = r;                                           // 0x0049acd8
        F<unsigned>(self, 0x180) = r;                                           // 0x0049acde
    }
    G<unsigned>(0x008400bc) = 0;                                                // 0x0049ace0
    return r;
}
M0_t NetServiceControl_orig;
// 0x0049ab40
SG_HOOK("golf_clean.exe", 0x0049ab40, netServiceControl, NetServiceControl_re, NetServiceControl_orig);

#if 0  // netSendShutdown: reimplementation drafted, not A/B'd this round (round-34 lead, see log/c3/c3ez_notes.txt).
// 0x00497d10  netSendShutdown (thiscall, no argument). Slot +0x14 of the sub-object +0x64 (0x00497d1d); +0xf0 = 20000
// (0x00497d22), +0x77c, +0x780, +0xec, +0x6f8 = 0. With the interface [0x008400b0] set (0x00497d4a): inside the critical
// section +0x38 (EnterCriticalSection, the IAT slot 0x004ba0e8 read once into ebp, 0x00497d4c): SetEvent(+0x20)
// (0x00497d5c), the interface's slots +0x10 and +8 (0x00497d6a / 0x00497d75), [0x008400b0] = 0 and +0x28 = 0, then
// LeaveCriticalSection (0x00497d82), Sleep(0) (0x00497d89), an empty Enter / Leave pair (0x00497d90 / 0x00497d93) and
// +0x2c, +0x30, +0xc = 0. With +0x34 set (0x00497da5) it is cleared and DeleteCriticalSection(+0x38) runs (0x00497dae).
// CloseHandle (slot 0x004ba0cc read once, 0x00497db7) on each nonzero handle +0x14, +0x18, +0x1c, +0x20, +0x24, which is
// cleared (0x00497dbd..0x00497deb); free 0x004a5007 on each nonzero block +0x720, +0x724, +0x6fc, +4, cleared
// (0x00497dfb..0x00497e56); +8 = 0, +0x718 = 0xc8, +0xe8 = 0; the 16 peer slots' +0, +4, +8 and flag byte cleared
// (0x00497e77..0x00497e86). The list +0xc0, when its current node +0xc8 is set (0x00497e96), is walked for its count +0xd0
// (re-read each turn, 0x00497eed): each node's next +0xc is kept at +0xcc, the list's slot +4 gets the node's payload
// +8 (0x00497eb4), a nonzero payload its slot 0 with 1 (0x00497ec2), the node's payload is cleared, and the node itself
// its virtual base's slot 0 with 1 (the base offset at [[node] + 4], 0x00497ed1..0x00497edd); then +0xc8, +0xd4, +0xd0 = 0.
// +0xe4 = +0xe8 = 0, listClear4a4b 0x004a4b00 on +0xf8, +0x120 and +0x148 (0x00497f10..0x00497f26), +0x708 .. +0x714 = 0,
// +0xf4 = 1, +0x54 .. +0x60 = 0. The last listClear's eax is left in eax.
unsigned __fastcall NetSendShutdown_re(void* self, void*) {
    char* sub = At(self, 0x64);
    V<M0_t>(sub, 0x14)(sub, 0);                                                 // 0x00497d1d
    F<unsigned>(self, 0xf0) = 0x4e20;                                           // 0x00497d22
    F<unsigned>(self, 0x77c) = 0;                                               // 0x00497d2c
    F<unsigned>(self, 0x780) = 0;                                               // 0x00497d32
    F<unsigned>(self, 0xec) = 0;                                                // 0x00497d38
    F<unsigned>(self, 0x6f8) = 0;                                               // 0x00497d3e
    if (CZ(Iface() != 0, 0x00497d4a)) {                                         // 0x00497d44 / 0x00497d4a
        const S1_t enter = Iat<S1_t>(0x004ba0e8);                               // 0x00497d4c
        const unsigned cs = P(At(self, 0x38));                                  // 0x00497d52
        enter(cs);                                                              // 0x00497d56
        Iat<S1_t>(0x004ba0e4)(F<unsigned>(self, 0x20));                         // 0x00497d5c
        void* dp = Iface();
        V<S1_t>(dp, 0x10)(P(dp));                                               // 0x00497d6a
        dp = Iface();
        V<S1_t>(dp, 8)(P(dp));                                                  // 0x00497d75
        G<unsigned>(0x008400b0) = 0;                                            // 0x00497d78
        F<unsigned>(self, 0x28) = 0;                                            // 0x00497d7f
        Iat<S1_t>(0x004ba0e0)(cs);                                              // 0x00497d82
        Iat<S1_t>(0x004ba0b0)(0);                                               // 0x00497d89
        enter(cs);                                                              // 0x00497d90
        Iat<S1_t>(0x004ba0e0)(cs);                                              // 0x00497d93
        F<unsigned>(self, 0x2c) = 0;                                            // 0x00497d99
        F<unsigned>(self, 0x30) = 0;                                            // 0x00497d9c
        F<unsigned>(self, 0xc) = 0;                                             // 0x00497d9f
    }
    if (CZ(F<unsigned>(self, 0x34) != 0, 0x00497da5)) {                         // 0x00497da2 / 0x00497da5
        F<unsigned>(self, 0x34) = 0;                                            // 0x00497daa
        Iat<S1_t>(0x004ba0dc)(P(At(self, 0x38)));                               // 0x00497dae
    }
    const S1_t closeHandle = Iat<S1_t>(0x004ba0cc);                             // 0x00497db7
    static const unsigned kHandles[5] = {0x14, 0x18, 0x1c, 0x20, 0x24};
    static const unsigned kHandleJe[5] = {0x00497dbf, 0x00497dcc, 0x00497dd9, 0x00497de6, 0x00497df3};
    for (int i = 0; i < 5; ++i) {
        (void)kHandleJe;
        if (CZ(F<unsigned>(self, kHandles[i]) != 0, kHandleJe[i])) {
            closeHandle(F<unsigned>(self, kHandles[i]));
            F<unsigned>(self, kHandles[i]) = 0;
        }
    }
    static const unsigned kBlocks[4] = {0x720, 0x724, 0x6fc, 4};
    static const unsigned kBlockJe[4] = {0x00497e03, 0x00497e1c, 0x00497e35, 0x00497e4b};
    for (int i = 0; i < 4; ++i) {
        (void)kBlockJe;
        if (CZ(F<unsigned>(self, kBlocks[i]) != 0, kBlockJe[i])) {
            kFree(F<void*>(self, kBlocks[i]));
            F<unsigned>(self, kBlocks[i]) = 0;
        }
    }
    F<unsigned>(self, 8) = 0;                                                   // 0x00497e59
    F<unsigned>(self, 0x718) = 0xc8;                                            // 0x00497e5c
    F<unsigned>(self, 0xe8) = 0;                                                // 0x00497e66
    for (int k = 0; k < 16; ++k) {                                              // 0x00497e77..0x00497e86
        char* s = Peer(self, k);
        F<unsigned>(s, 0) = 0;
        F<unsigned>(s, 4) = 0;
        F<unsigned>(s, 8) = 0;
        s[0x14] = 0;
    }
    char* L = At(self, 0xc0);
    if (CZ(F<unsigned>(L, 8) != 0, 0x00497e96)) {                               // 0x00497e94 / 0x00497e96
        if (!CZ(F<int>(L, 0x10) <= 0, 0x00497ea1)) {                            // 0x00497e9f / 0x00497ea1
            int k = 0;
            do {
                char* node = F<char*>(L, 8);                                    // 0x00497ea3
                F<char*>(L, 0xc) = F<char*>(node, 0xc);                         // 0x00497ea8 / 0x00497eab
                void* payload = F<void*>(node, 8);                              // 0x00497eae
                V<M1_t>(L, 4)(L, 0, P(payload));                                // 0x00497eb4
                if (CZ(payload != 0, 0x00497eb9))                               // 0x00497eb7 / 0x00497eb9
                    V<M1_t>(payload, 0)(payload, 0, 1);                         // 0x00497ec2
                F<unsigned>(F<char*>(L, 8), 8) = 0;                             // 0x00497ec4 / 0x00497ec7
                char* n2 = F<char*>(L, 8);                                      // 0x00497eca
                if (CZ(n2 != 0, 0x00497ecf)) {                                  // 0x00497ecd / 0x00497ecf
                    char* vb = n2 + F<int>(F<char*>(n2, 0), 4);                 // 0x00497ed1..0x00497edb
                    V<M1_t>(vb, 0)(vb, 0, 1);                                   // 0x00497edd
                }
                F<char*>(L, 8) = F<char*>(L, 0xc);                              // 0x00497edf / 0x00497ee5
            } while (CZ(++k < F<int>(L, 0x10), 0x00497ef3));                    // 0x00497eec..0x00497ef3
        }
        F<unsigned>(L, 8) = 0;                                                  // 0x00497ef5
        F<unsigned>(L, 0x14) = 0;                                               // 0x00497ef8
        F<unsigned>(L, 0x10) = 0;                                               // 0x00497efb
    }
    F<unsigned>(self, 0xe4) = 0;                                                // 0x00497f04
    F<unsigned>(self, 0xe8) = 0;                                                // 0x00497f0a
    kListClear(At(self, 0xf8), 0);                                              // 0x00497f10
    kListClear(At(self, 0x120), 0);                                             // 0x00497f1b
    const unsigned r = kListClear(At(self, 0x148), 0);                          // 0x00497f26
    F<unsigned>(self, 0x708) = 0;                                               // 0x00497f34
    F<unsigned>(self, 0x70c) = 0;                                               // 0x00497f36
    F<unsigned>(self, 0x710) = 0;                                               // 0x00497f39
    F<unsigned>(self, 0x714) = 0;                                               // 0x00497f3c
    F<unsigned>(self, 0xf4) = 1;                                                // 0x00497f3f
    F<unsigned>(self, 0x54) = 0;                                                // 0x00497f49
    F<unsigned>(self, 0x58) = 0;                                                // 0x00497f4c
    F<unsigned>(self, 0x5c) = 0;                                                // 0x00497f4f
    F<unsigned>(self, 0x60) = 0;                                                // 0x00497f52
    return r;
}
M0_t NetSendShutdown_orig;
// 0x00497d10
SG_HOOK("golf_clean.exe", 0x00497d10, netSendShutdown, NetSendShutdown_re, NetSendShutdown_orig);
#endif
