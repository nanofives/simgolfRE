// golf_clean.exe functions, batch 20 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2

extern unsigned int g_flags;             // 0x59e7b8
extern signed char g_tileType[50][50];   // 0x5722e8
struct TypeDef30 { char pad[0xc]; unsigned char flags; char padd[0x30 - 0xd]; };   // 0x578370, stride 0x30
extern TypeDef30 g_typeDefs[];
int f_40bfe0(int a, int b, int c, int d);// 0x40bfe0 (corner height of tile a,b; corners 1, 3, 5, 7)

// Height difference across the tile under world point (x, y) (1024 units per tile), along y: 0 when flag 1
// of g_flags is set, on tile types 7 and 9, and on types whose +0xc flags have any of bits 1..3.
// MATCH: golf_clean.exe 0x0040c2f0 ?slopeY@@YAHHH@Z
int slopeY(int x, int y)
{
    if (g_flags & 1)
        return 0;
    int tx = x >> 10;
    int ty = y >> 10;
    signed char t = g_tileType[tx][ty];
    if (t == 7 || t == 9)
        return 0;
    if (g_typeDefs[t].flags & 0xe)
        return 0;
    int c5 = f_40bfe0(tx, ty, 5, 1);
    int c7 = f_40bfe0(tx, ty, 7, 1);
    int c1 = f_40bfe0(tx, ty, 1, 1);
    int c3 = f_40bfe0(tx, ty, 3, 1);
    if (y > (ty << 10) + 0x200)
        return c3 - c5;
    return c1 - c7;
}

// Same along x.
// MATCH: golf_clean.exe 0x0040c3a0 ?slopeX@@YAHHH@Z
int slopeX(int x, int y)
{
    if (g_flags & 1)
        return 0;
    int tx = x >> 10;
    int ty = y >> 10;
    signed char t = g_tileType[tx][ty];
    if (t == 7 || t == 9)
        return 0;
    if (g_typeDefs[t].flags & 0xe)
        return 0;
    int c5 = f_40bfe0(tx, ty, 5, 1);
    int c7 = f_40bfe0(tx, ty, 7, 1);
    int c1 = f_40bfe0(tx, ty, 1, 1);
    int c3 = f_40bfe0(tx, ty, 3, 1);
    if (x > (tx << 10) + 0x200)
        return c3 - c1;
    return c5 - c7;
}
