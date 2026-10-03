// Small golf_clean.exe functions reached by the scenarios (release build). Names describe behaviour only.
// FLAGS golf_clean.exe: /O2
#include <windows.h>
#include <mmsystem.h>

extern int g_pairA, g_pairB;             // 0x5a412c, 0x5a4130
extern DWORD g_timeSeed;                 // 0x51b380
extern DWORD g_timeSeed37;               // 0x822d9c
extern signed char g_tileType[50][50];   // 0x5722e8 (the exe's own 50x50 terrain type grid, [x][y])
extern unsigned char g_tileByte[50][50]; // 0x56988c
int f_467270(int a, int b);              // 0x467270
int f_40bfe0(int a, int b, int c, int d);// 0x40bfe0

// MATCH: golf_clean.exe 0x0040bf00 ?setPair@@YAXHH@Z
void setPair(int a, int b)
{
    g_pairA = a;
    g_pairB = b;
}

// MATCH: golf_clean.exe 0x00461810 ?brighten@@YAHH@Z
int brighten(int v)
{
    if (v < 0xdf)
        return v + 0x20;
    return 0xff;
}

// MATCH: golf_clean.exe 0x00467110 ?seedFromTime@@YAXXZ
void seedFromTime()
{
    g_timeSeed = timeGetTime();
    g_timeSeed37 = g_timeSeed * 37;
}

// MATCH: golf_clean.exe 0x004672b0 ?f_4672b0@@YAHHH@Z
int f_4672b0(int a, int b)
{
    return f_467270(a + 0x40000000, b);
}

// MATCH: golf_clean.exe 0x004492d0 ?tileType@@YAHHH@Z
int tileType(int x, int y)
{
    return g_tileType[x][y];
}

// MATCH: golf_clean.exe 0x004492f0 ?tileByte@@YAHHH@Z
int tileByte(int x, int y)
{
    return g_tileByte[x][y];
}

// MATCH: golf_clean.exe 0x00449310 ?f_449310@@YAHHHH@Z
int f_449310(int a, int b, int c)
{
    return f_40bfe0(a, b, c, 1);
}
