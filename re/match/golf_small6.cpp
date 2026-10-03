// Small golf_clean.exe functions, batch 6 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <windows.h>
#include <stdlib.h>
#include <string.h>

struct GolferB {                         // golfer record, 0x100 bytes at 0x5794b8
    char  pad0[0x1d];
    char  b1d;                           // +0x1d
    char  pad1e[0x21 - 0x1e];
    char  hole;                          // +0x21
    char  b22;                           // +0x22
    char  pad23[0xb6 - 0x23];
    short type;                          // +0xb6 (golfer type record index)
    short flagsB8;                       // +0xb8
    char  padBA[0x100 - 0xba];
};
extern GolferB g_gb[];                   // 0x5794b8

// type % 10 plus 20 / 30 / 45 for flag bits 1 / 2 / 4 at +0xb8.
// MATCH: golf_clean.exe 0x00453260 ?golferScore453260@@YAHH@Z
int golferScore453260(int g)
{
    int v = g_gb[g].type % 10;
    short f = g_gb[g].flagsB8;
    if (f & 1)
        v += 20;
    if (f & 2)
        v += 30;
    if (f & 4)
        v += 45;
    return v;
}

struct QEntry { short id; short pad; short type; short pad2; };   // 8 bytes
extern QEntry g_queue[100];              // 0x5689e8

// Clears three per-golfer bytes and frees every queued entry of the golfer's type.
// MATCH: golf_clean.exe 0x00426670 ?resetGolfer426670@@YAXH@Z
void resetGolfer426670(int g)
{
    g_gb[g].b22 = 0;
    g_gb[g].b1d = 0;
    g_gb[g].hole = 0;
    short t = g_gb[g].type;
    for (int i = 0; i < 100; i++)
        if (g_queue[i].type == t)
            g_queue[i].id = -1;
}

extern int g_83ad4c;                     // 0x83ad4c
int  f_483c10();                         // 0x483c10
void f_497b00();                         // 0x497b00
void f_497b20();                         // 0x497b20

// MATCH: golf_clean.exe 0x00483c30 ?drain483c30@@YAXXZ
void drain483c30()
{
    g_83ad4c = 0x20;
    if (f_483c10()) {
        do
            g_83ad4c = 0x20;
        while (f_483c10());
    }
    g_83ad4c = 0;
    f_497b00();
    f_497b20();
}

class Slot120 {
public:
    int f_488310(int a, int b);          // 0x488310
    char pad[0x104];
    int  m_104;                          // +0x104
    char pad2[0x120 - 0x108];
};
extern Slot120 g_slots[4];               // 0x83b170

// MATCH: golf_clean.exe 0x00488420 ?dispatch488420@@YAHHH@Z
int dispatch488420(int a, int b)
{
    for (int i = 0; i < 4; i++) {
        if (g_slots[i].m_104) {
            int r = g_slots[i].f_488310(a, b);
            if (r >= 0)
                return r;
        }
    }
    return -1;
}

// Removes trailing white space in place.
// MATCH: golf_clean.exe 0x00492570 ?trimRight@@YAXPAD@Z
void trimRight(char* s)
{
    int n = strlen(s);
    if (!n)
        return;
    for (char* p = s + n - 1; p >= s; p--) {
        if (!isspace((unsigned char)*p))
            return;
        *p = 0;
        if (p == s)
            return;
    }
}

extern int g_pairA, g_pairB;             // 0x5a412c, 0x5a4130
int f_4493d0(int a, int b, int c, int d, int e, int f, int g);   // 0x4493d0

// Calls 0x4493d0 from the previous point (g_pairA, g_pairB) to (x, y), then stores (x, y).
// MATCH: golf_clean.exe 0x0040bf20 ?lineTo40bf20@@YAXHHHHH@Z
void lineTo40bf20(int x, int y, int c, int d, int e)
{
    f_4493d0(x, y, g_pairA, g_pairB, c, d, e);
    g_pairA = x;
    g_pairB = y;
}

// MATCH: golf_clean.exe 0x004223f0 ?decaySum2@@YAHHH@Z
int decaySum2(int x, int y)
{
    int r = 0, s = 0;
    do {
        r += x / 8;
        s += y / 16;
        y -= 0x80;
        x -= x >> 4;
    } while (s > 0);
    return r;
}

class Buf474 {
public:
    int  alloc(int n);                   // 0x474820
    void release();                      // 0x4747e0
    char pad[8];
    void* m_8; void* m_c; int m_10, m_14;
};

// MATCH: golf_clean.exe 0x00474820 ?alloc@Buf474@@QAEHH@Z
int Buf474::alloc(int n)
{
    if (m_8)
        release();
    m_8 = malloc(n);
    if (!m_8)
        return 1;
    m_14 = n;
    m_10 = n;
    m_c = m_8;
    return 0;
}

class Win478 {
public:
    int fillRect(int x, int y, int w, int h, int c);   // 0x478af0
    int f_478b50(RECT* r, int c);                       // 0x478b50
};

// Source NOT determined here: field-by-field stores in left, top, right, bottom order compile identically.
// MATCH: golf_clean.exe 0x00478af0 ?fillRect@Win478@@QAEHHHHHH@Z
int Win478::fillRect(int x, int y, int w, int h, int c)
{
    RECT r = { x, y, x + w, y + h };
    return f_478b50(&r, c);
}
