// golf_clean.exe 0x004672d0 / 0x0040bfe0 (release). WIP, not in the 100% suite.
// angleOf 71%: the original pushes edi only after the two early returns; tried a nested block for the tail.
// cornerHeight 31%: the original saves only esi/edi up front (x in esi, y in edi, corner in ecx), ours saves four.
// FLAGS golf_clean.exe: /O2
#include <stdlib.h>

// Angle of (dx, -dy) as a 32-bit turn fraction (0x40000000 = quarter turn), from a polynomial arctangent of the
// smaller/larger component ratio (0x4000 = 1).
// MATCH: golf_clean.exe 0x004672d0 ?angleOf@@YAHHH@Z
int angleOf(int dx, int dy)
{
    int y = -dy;
    if (dx == 0)
        return y > 0 ? 0 : 0x80000000;
    if (y == 0)
        return dx > 0 ? 0x40000000 : 0xc0000000;
    int ax = abs(dx);
    int ay = abs(y);
    int n, d, swapped;
    if (ax > ay) {
        n = ay << 14;
        d = ax;
        swapped = 1;
    } else {
        n = ax << 14;
        d = ay;
        swapped = 0;
    }
    int t = n / d;
    int a = (0x2800 - (abs(0x1333 - t) * 0xb00 >> 14)) * t >> 14;
    if (dx <= 0) {
        if (y <= 0) {
            if (!swapped)
                return (a + 0x8000) << 16;
            return (0xc000 - a) << 16;
        }
        if (swapped)
            return (a + 0xc000) << 16;
        a = -a;
    } else {
        if (y < 1) {
            if (!swapped)
                return (0x8000 - a) << 16;
            return (a + 0x4000) << 16;
        }
        if (swapped)
            return (0x4000 - a) << 16;
    }
    return a << 16;
}

extern signed char g_tileType[50][50];   // 0x5722e8
struct TypeDef30 { char pad[0xc]; unsigned int flags; char pad10[0x30 - 0x10]; };   // 0x578370, stride 0x30
extern TypeDef30 g_typeDefs[];
extern signed char g_level543018[50][50];// 0x543018
extern signed char g_cornerCache[50][50][8];   // 0x51b770
void slopeEnds42f4b0(int x, int y, int* lo, int* hi);   // 0x42f4b0
int tileHeight40c170(int x, int y, int flags);          // 0x40c170
// Height of odd corner 1/3/5/7 of tile (x, y): the cache when asked and non-zero; flat level or one end of a
// slope for types with +0xc flags 2/4; else the height of the neighbouring tile at that corner. 3 off the map.
// MATCH: golf_clean.exe 0x0040bfe0 ?cornerHeight@@YAHHHHH@Z
int cornerHeight(int x, int y, int corner, int useCache)
{
    if (x >= 0 && x < 50 && y >= 0 && y < 50 && (corner & 1)) {
        if (useCache) {
            signed char c = g_cornerCache[x][y][corner];
            if (c)
                return c;
        }
        unsigned int f = g_typeDefs[g_tileType[x][y]].flags;
        if (f & 2) {
            if (f & 1)
                return g_level543018[x][y];
            slopeEnds42f4b0(x, y, &y, &x);
            return x;
        }
        if (f & 4) {
            if (f & 1)
                return g_level543018[x][y];
            slopeEnds42f4b0(x, y, &y, &x);
            return y;
        }
        if (!(f & 8)) {
            int a = tileHeight40c170(x + 1, y - 1, 0);
            int b = tileHeight40c170(x + 1, y, 0);
            int c = tileHeight40c170(x, y, 0);
            int d = tileHeight40c170(x, y - 1, 0);
            switch (corner & 7) {
            case 1: return a;
            case 3: return b;
            case 5: return c;
            case 7: return d;
            default: return corner;
            }
        }
    }
    return 3;
}
