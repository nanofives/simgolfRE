// Small golf_clean.exe functions, batch 12 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <windows.h>
#include <mmsystem.h>
#include <ctype.h>
#include <string.h>

void drain483c30();                      // 0x483c30
void pumpMessages();                     // 0x45c030
int  f_45af00();                         // 0x45af00
void f_45aed0();                         // 0x45aed0
extern int g_822d68, g_822d74;

// Waits ms * 1000 / 100 timeGetTime units, draining input; with `pump`, also pumps window messages and
// returns 1 early (after 0x45aed0) when 0x45af00 says stop or g_822d68 is set.
// MATCH: golf_clean.exe 0x0045bf80 ?waitTicks@@YAHHH@Z
int waitTicks(int ms, int pump)
{
    if (!ms)
        return 0;
    unsigned int limit = ms * 1000 / 100;
    DWORD t0 = timeGetTime();
    while (timeGetTime() - t0 < limit) {
        drain483c30();
        if (pump) {
            pumpMessages();
            if (!f_45af00() || g_822d68) {
                f_45aed0();
                return 1;
            }
        }
    }
    return 0;
}

// Pumps WM_MOUSEMOVE (0x200) messages only; clears 0x822d68 unless 0x822d74 was set, then calls 0x483cd0.
void f_483cd0();                         // 0x483cd0

// MATCH: golf_clean.exe 0x0045c030 ?pumpMessages@@YAXXZ
void pumpMessages()
{
    MSG msg;
    while (PeekMessageA(&msg, 0, WM_MOUSEMOVE, WM_MOUSEMOVE, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    if (!g_822d74)
        g_822d68 = 0;
    f_483cd0();
    g_822d74 = 0;
}

int f_40c170(int x, int y, int z);       // 0x40c170 (corner height)

// Max and min of the heights at (x, y), (x+1, y), (x+1, y-1), (x, y-1).
// MATCH: golf_clean.exe 0x0042f4b0 ?cornerRange@@YAXHHPAH0@Z
void cornerRange(int x, int y, int* mx, int* mn)
{
    int v = f_40c170(x, y, 0);
    *mn = v;
    *mx = v;
    v = f_40c170(x + 1, y, 0);
    if (v > *mx)
        *mx = v;
    if (v < *mn)
        *mn = v;
    y--;
    v = f_40c170(x + 1, y, 0);
    if (v > *mx)
        *mx = v;
    if (v < *mn)
        *mn = v;
    v = f_40c170(x, y, 0);
    if (v > *mx)
        *mx = v;
    if (v < *mn)
        *mn = v;
}

struct TypeDef30 { char pad[8]; signed char wall; char pad9[0x30 - 9]; };   // 0x578370, stride 0x30
extern TypeDef30 g_typeDefs[];
extern signed char g_tileType[50][50];   // 0x5722e8
extern signed char g_wallMask[50][50];   // 0x5619a0
extern int g_dirDx[], g_dirDy[];         // 0x4c2878, 0x4c2898

// Wall height of tile (x, y) toward `dir` (0 when bit dir of the mask is clear); a tile type without its own
// wall uses the neighbour's in that direction.
// MATCH: golf_clean.exe 0x00449330 ?wallHeight@@YAHHHH@Z
int wallHeight(int x, int y, int dir)
{
    if (!(g_wallMask[x][y] & (1 << dir)))
        return 0;
    signed char h = g_typeDefs[g_tileType[x][y]].wall;
    if (h)
        return h;
    return g_typeDefs[g_tileType[x + g_dirDx[dir]][y + g_dirDy[dir]]].wall;
}

// Removes leading white space in place (through a 0x200-byte copy).
// MATCH: golf_clean.exe 0x004924e0 ?trimLeft@@YAXPAD@Z
void trimLeft(char* s)
{
    char buf[0x200];
    char* p = s;
    if (isspace((unsigned char)*p)) {
        do
            p++;
        while (isspace((unsigned char)*p));
    }
    strcpy(buf, p);
    strcpy(s, buf);
}
