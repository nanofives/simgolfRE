// Small golf_clean.exe functions, batch 8 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <windows.h>
#include <stdlib.h>

class Sound6c {                          // 0x6c bytes; array at 0x80d840
public:
    void f_484f40(int v);                // 0x484f40
    void f_485140(int v);                // 0x485140
    void f_4847f0(int v);                // 0x4847f0
    void f_4846b0(int v);                // 0x4846b0
    void f_484940();                     // 0x484940
    char pad[0x6c];
};
extern Sound6c g_sounds[];               // 0x80d840

// Plays sound `id` (-1 = none): four setters then start. Called e.g. as (id, 100, 0, 0, 0) by FUN_00466370.
// MATCH: golf_clean.exe 0x004481b0 ?playSound@@YAXHHHHH@Z
void playSound(int id, int a, int b, int c, int d)
{
    if (id == -1)
        return;
    g_sounds[id].f_484f40(c);
    g_sounds[id].f_485140(a);
    g_sounds[id].f_4847f0(b);
    g_sounds[id].f_4846b0(d);
    g_sounds[id].f_484940();
}

class Res487b {
public:
    int  alloc2(int n);                  // 0x487ee0
    void free487f30();                   // 0x487f30
    char pad[0x15c];
    void* m_15c; void* m_160;
};

// MATCH: golf_clean.exe 0x00487ee0 ?alloc2@Res487b@@QAEHH@Z
int Res487b::alloc2(int n)
{
    free487f30();
    m_15c = malloc(n);
    if (!m_15c)
        return 4;
    m_160 = malloc(n);
    return m_160 ? 0 : 4;
}

class Node47b {
public:
    int contains(Node47b* n);            // 0x47b080
    char pad[0x224];
    Node47b** m_children;                // +0x224
    int pad228;
    int m_count;                         // +0x22c
};

// Whether n is a child or a descendant.
// MATCH: golf_clean.exe 0x0047b080 ?contains@Node47b@@QAEHPAV1@@Z
int Node47b::contains(Node47b* n)
{
    if (!n)
        return 0;
    for (int i = 0; i < m_count; i++) {
        if (m_children[i] == n)
            return 1;
        if (m_children[i]->contains(n))
            return 1;
    }
    return 0;
}

class View47c {
public:
    void offsetRect(RECT* r);            // 0x47b0d0
    void offsetRectLocal(RECT* r);       // 0x47b120
    void f_47b170(int* x, int* y);       // 0x47b170
    void toLocal(int* x, int* y);        // 0x47b290
};

// MATCH: golf_clean.exe 0x0047b0d0 ?offsetRect@View47c@@QAEXPAUtagRECT@@@Z
void View47c::offsetRect(RECT* r)
{
    if (!r)
        return;
    int x = 0, y = 0;
    f_47b170(&x, &y);
    r->left += x;
    r->right += x;
    r->top += y;
    r->bottom += y;
}

// MATCH: golf_clean.exe 0x0047b120 ?offsetRectLocal@View47c@@QAEXPAUtagRECT@@@Z
void View47c::offsetRectLocal(RECT* r)
{
    if (!r)
        return;
    int x = 0, y = 0;
    toLocal(&x, &y);
    r->left += x;
    r->right += x;
    r->top += y;
    r->bottom += y;
}

class Random {
public:
    unsigned int seed;
    unsigned short range(unsigned short n);   // 0x45c1e0: the call site keeps only ax (mov si, ax)
};
extern Random g_rng;                     // 0x822d9c
int sign(int v);                         // 0x467150 (golf_util.cpp)

// Random offset in -50..50 (101 values) reshaped: |v| < 20 halved, < 40 minus 10, else doubled minus 50.
// MATCH: golf_clean.exe 0x00405920 ?wobble@@YAHXZ
int wobble()
{
    int v = g_rng.range(101) - 50;
    int s = sign(v);
    int a = abs(v);
    if (a < 20)
        a /= 2;
    else if (a < 40)
        a -= 10;
    else
        a = a * 2 - 50;
    if (s != 1)
        a = -a;
    return a;
}

struct CourseRec { char pad[3]; char kind; char pad4[0x2e - 4]; };   // 0x2e bytes at 0x571ff4
extern CourseRec g_courses[];            // 0x571ff4
extern int g_curCourse;                  // 0x59bf90
extern signed char g_tileType[50][50];   // 0x5722e8
extern unsigned short g_tileFlags[50][50];   // 0x53caf0

// Sets a tile to type 0x11 (course kind 2, also clearing flag bits 0x0320) or 0x14 (hidden).
// MATCH: golf_clean.exe 0x00470a10 ?clearTile470a10@@YAXHH@Z
void clearTile470a10(int x, int y)
{
    char k = g_courses[g_curCourse].kind;
    g_tileType[x][y] = k == 2 ? 0x11 : 0x14;
    if (k == 2)
        g_tileFlags[x][y] &= 0xfcdf;
}
