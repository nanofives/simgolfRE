// Read-only lookups of golf_clean.exe into the course, golfer and cell tables (all __cdecl leaves).
// Reimplemented from the C2 transcriptions re/analysis/<subsystem>/<addr>_<name>.md; addresses cited inline.
#include "hooks.h"

namespace {

typedef int(__cdecl* Int1_t)(int);
typedef int(__cdecl* Int2_t)(int, int);
typedef int(__cdecl* Int3_t)(int, int, int);
typedef void*(__cdecl* Ptr3_t)(int, int, int);
typedef int(__cdecl* InRect_t)(int, int, const int*);

const signed char* const kTileType = reinterpret_cast<const signed char*>(0x005722e8);  // [x*50 + y]
const signed char* const kWallMask = reinterpret_cast<const signed char*>(0x005619a0);  // [x*50 + y]
const signed char* const kTypeWall = reinterpret_cast<const signed char*>(0x00578378);  // [type*0x30]
const int* const kDirDx = reinterpret_cast<const int*>(0x004c2878);                      // 8 directions
const int* const kDirDy = reinterpret_cast<const int*>(0x004c2898);
const char* const kGolfer = reinterpret_cast<const char*>(0x0057956e);                   // 0x100 per golfer

// 0x004492d0  tileType: the signed tile type byte at 0x005722e8 + x*50 + y (no bounds check).
Int2_t TileType_orig;
int __cdecl TileType_re(int x, int y) { return kTileType[x * 50 + y]; }

// 0x00449330  wallHeight(x, y, dir): 0 when bit `dir` of the wall mask byte at 0x005619a0 + x*50 + y is clear
// (test at 0x00449356); else the signed wall height of the cell's type (0x00578378 + type*0x30) when non-zero,
// else the wall height of the type of the neighbour (x + dx[dir], y + dy[dir]) from the tables at 0x004c2878 and
// 0x004c2898.
Int3_t WallHeight_orig;
int __cdecl WallHeight_re(int x, int y, int dir) {
    const int cell = x * 50 + y;
    if (!(kWallMask[cell] & (1 << (dir & 31)))) return 0;
    const signed char h = kTypeWall[kTileType[cell] * 0x30];
    if (h) return h;
    const int n = (x + kDirDx[dir]) * 50 + kDirDy[dir] + y;
    return kTypeWall[kTileType[n] * 0x30];
}

// 0x0043d6f0  cellAt(table, col, row): the empty cell 0x0053ba48 when the table's base index at 0x005a9370 is -1
// or col >= its width at 0x0053f3e8 (signed); else &cells[(width*row + base + col)], 0x2c-byte cells at
// 0x005aaa30.
Ptr3_t CellAt_orig;
void* __cdecl CellAt_re(int table, int col, int row) {
    const int base = reinterpret_cast<const int*>(0x005a9370)[table];
    if (base == -1) return reinterpret_cast<void*>(0x0053ba48);
    const int w = reinterpret_cast<const int*>(0x0053f3e8)[table];
    if (col >= w) return reinterpret_cast<void*>(0x0053ba48);
    return reinterpret_cast<char*>(0x005aaa30) + (w * row + base + col) * 0x2c;
}

// 0x00453260  golferScore(g): (type word at 0x0057956e + g*0x100) % 10 (signed, idiv), plus 20 / 30 / 45 for bits
// 0 / 1 / 2 of the flags word at 0x00579570 + g*0x100.
Int1_t GolferScore_orig;
int __cdecl GolferScore_re(int g) {
    const char* rec = kGolfer + g * 0x100;
    int v = *reinterpret_cast<const short*>(rec) % 10;
    const short f = *reinterpret_cast<const short*>(rec + 2);
    if (f & 1) v += 20;
    if (f & 2) v += 30;
    if (f & 4) v += 45;
    return v;
}

// 0x0046c940  typeBit7Clear(g): 1 when bit 7 of the byte at 0x004d60a9 + type*0x230 is clear, type being the
// signed word at 0x0057956e + g*0x100.
Int1_t TypeBit7Clear_orig;
int __cdecl TypeBit7Clear_re(int g) {
    const short type = *reinterpret_cast<const short*>(kGolfer + g * 0x100);
    const unsigned char b = *reinterpret_cast<const unsigned char*>(0x004d60a9 + type * 0x230);
    return (b & 0x80) ? 0 : 1;
}

// 0x00492610  pointInRect(x, y, r): 1 when r[0] <= x < r[2] and r[1] <= y < r[3] (signed), else 0.
InRect_t InRect_orig;
int __cdecl InRect_re(int x, int y, const int* r) {
    if (x < r[0] || x >= r[2] || y < r[1]) return 0;
    return y < r[3] ? 1 : 0;
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x004492d0, tileType, TileType_re, TileType_orig);
SG_HOOK("golf_clean.exe", 0x00449330, wallHeight, WallHeight_re, WallHeight_orig);
SG_HOOK("golf_clean.exe", 0x0043d6f0, cellAt, CellAt_re, CellAt_orig);
SG_HOOK("golf_clean.exe", 0x00453260, golferScore, GolferScore_re, GolferScore_orig);
SG_HOOK("golf_clean.exe", 0x0046c940, typeBit7Clear, TypeBit7Clear_re, TypeBit7Clear_orig);
SG_HOOK("golf_clean.exe", 0x00492610, pointInRect, InRect_re, InRect_orig);
