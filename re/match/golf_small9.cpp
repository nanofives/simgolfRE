// Small golf_clean.exe functions, batch 9 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <math.h>
#include <string.h>

extern short g_strOff[0x80];             // 0x59d81c (-1 = none; cleared to 0xff by clearBuffers)
extern char  g_strs[0x1002];             // 0x56fcb0
extern char  g_text[];                   // 0x51a068

// Appends string `id` of the table to the shared text buffer; 1 if it existed.
// MATCH: golf_clean.exe 0x0045b9f0 ?appendString@@YAHH@Z
int appendString(int id)
{
    if (id != -1) {
        short off = g_strOff[id];
        if (off != -1) {
            strcat(g_text, g_strs + off);
            return 1;
        }
    }
    return 0;
}

int f_4674c0(int x, int y);              // 0x4674c0
int clamp(int v, int lo, int hi);        // 0x467130

// Two samples at quarter and half resolution mixed 6:4, scaled by 7/128, clamped to 0..0x200.
// MATCH: golf_clean.exe 0x0042dba0 ?sample42dba0@@YAHHH@Z
int sample42dba0(int x, int y)
{
    x >>= 2;
    y >>= 2;
    int a = f_4674c0(x, y) * 6;
    int b = f_4674c0(x * 2, y * 2);
    return clamp((a + b * 4) * 7 / 64 / 2, 0, 0x200);
}

int f_40c2f0(int a, int b);              // 0x40c2f0
int f_40c3a0(int a, int b);              // 0x40c3a0
extern int g_4c2878[], g_4c2898[];

// MATCH: golf_clean.exe 0x0040c450 ?mix40c450@@YAHHHI@Z
int mix40c450(int a, int b, unsigned int k)
{
    int p = f_40c2f0(a, b);
    int q = f_40c3a0(a, b);
    if (k & 1) {
        p = clamp(p, -1, 1);
        q = clamp(q, -1, 1);
    }
    return g_4c2898[k] * q + g_4c2878[k] * p;
}

extern int g_blockStart[100];            // 0x820b70 (-1 = free slot)
extern int g_blockSize[100];             // 0x820d00

// First-fit allocation of n units from 100 (start, size) blocks; -1 when none is big enough.
// MATCH: golf_clean.exe 0x0043d5d0 ?allocBlock@@YAHH@Z
int allocBlock(int n)
{
    for (int i = 0; i < 100; i++) {
        if (g_blockStart[i] >= 0 && g_blockSize[i] >= n) {
            int r = g_blockStart[i];
            if (g_blockSize[i] > n) {
                g_blockStart[i] = r + n;
                g_blockSize[i] -= n;
            } else {
                g_blockSize[i] = 0;
                g_blockStart[i] = -1;
            }
            return r;
        }
    }
    return -1;
}

struct Win9c { char pad[0x9c]; unsigned int flags; };   // +0x9c bit 0x2000000 = stays at the back
extern Win9c* g_zlist[];                 // 0x83a2d8
extern int    g_zcount;                  // 0x83ab94
void f_47e450();                         // 0x47e450

// MATCH: golf_clean.exe 0x0047e4c0 ?zPush@@YAXPAUWin9c@@@Z
void zPush(Win9c* w)
{
    if (!w)
        return;
    int n = g_zcount;
    if (w->flags & 0x2000000) {
        g_zlist[n] = w;
        g_zcount = n + 1;
        return;
    }
    for (int i = n; i > 0; i--)
        g_zlist[i] = g_zlist[i - 1];
    g_zlist[0] = w;
    g_zcount = n + 1;
}

// MATCH: golf_clean.exe 0x0047e580 ?zRaise@@YAXPAUWin9c@@@Z
void zRaise(Win9c* w)
{
    if (!w || (w->flags & 0x2000000))
        return;
    int i;
    for (i = 0; i < g_zcount; i++)
        if (g_zlist[i] == w)
            break;
    if (i < g_zcount) {
        for (; i > 0; i--)
            g_zlist[i] = g_zlist[i - 1];
        g_zlist[0] = w;
    }
    f_47e450();
}

extern int g_sinTable[0x100];            // 0x83b9f4
int f_491c70(int a, int b);              // 0x491c70
int f_491d80(int a, int b);              // 0x491d80

// sin(i * 0.0061599856, 0x4bba50) * 65535.0 (0x4bba48) for i = 0..255.
// MATCH: golf_clean.exe 0x00491c10 ?initSinTable@@YAXXZ
void initSinTable()
{
    for (int i = 0; i < 0x100; i++)
        g_sinTable[i] = (int)(sin(i * 0.006159985596078431) * 65535.0);
    f_491c70(0x40000000, 0x64);
    f_491d80(0xa02d82d8, 0x64);
}
