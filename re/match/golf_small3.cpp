// Small golf_clean.exe functions, batch 3 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <windows.h>
#include <stdlib.h>

extern int g_4e4504, g_4e4514, g_4e4524, g_4e4534;   // 0x4e4504 + 0x10 * i
extern int g_4e4544, g_4e4550, g_4e455c, g_4e4568;   // 0x4e4544 + 0xc * i
extern char g_587da0[];                  // 0x587da0
void f_431fa0(char* s);                  // 0x431fa0
void f_431ee0(char* s);                  // 0x431ee0
extern int g_822d68;                     // 0x822d68
void f_483bd0();                         // 0x483bd0
void f_45c030();                         // 0x45c030
int  f_45ae70();                         // 0x45ae70
int  f_4493d0(int a, int b, int c, int d, int e, int f, int g);   // 0x4493d0

// MATCH: golf_clean.exe 0x00490cc0 ?set4e4504@@YAXHHHH@Z
void set4e4504(int a, int b, int c, int d)
{
    g_4e4504 = a;
    g_4e4514 = b;
    g_4e4524 = c;
    g_4e4534 = d;
}

// MATCH: golf_clean.exe 0x00490d20 ?set4e4544@@YAXHHHH@Z
void set4e4544(int a, int b, int c, int d)
{
    g_4e4544 = a;
    g_4e4550 = b;
    g_4e455c = c;
    g_4e4568 = d;
}

// Octagonal distance: max(|dx|,|dy|) + min(|dx|,|dy|) / 2, computed as (2 * max + min) / 2.
// MATCH: golf_clean.exe 0x00467170 ?approxDistance@@YAHHH@Z
int approxDistance(int dx, int dy)
{
    if (dx < 0)
        dx = -dx;
    if (dy < 0)
        dy = -dy;
    if (dx > dy)
        return (dy + dx * 2) / 2;
    return (dx + dy * 2) / 2;
}

// MATCH: golf_clean.exe 0x00432170 ?flush587da0@@YAXXZ
void flush587da0()
{
    if (!g_587da0[0])
        return;
    f_431fa0(g_587da0);
    f_431ee0(g_587da0);
    g_587da0[0] = 0;
}

// MATCH: golf_clean.exe 0x004378a0 ?fileExists@@YAHPBD@Z
int fileExists(const char* path)
{
    WIN32_FIND_DATAA fd;
    return FindFirstFileA(path, &fd) != INVALID_HANDLE_VALUE;
}

// MATCH: golf_clean.exe 0x0045c150 ?pump45c150@@YAXXZ
void pump45c150()
{
    g_822d68 = 0;
    do {
        f_483bd0();
        f_45c030();
    } while (g_822d68 || f_45ae70());
}

// MATCH: golf_clean.exe 0x00406220 ?f_406220@@YAHHHHHHHH@Z
int f_406220(int a, int b, int c, int d, int e, int f, int g)
{
    return f_4493d0(a, b, c, d, e, f, g);
}
