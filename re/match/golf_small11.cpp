// Small golf_clean.exe functions, batch 11 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <windows.h>
#include <string.h>

struct TypeDef30 { char pad[6]; char kind; char pad7[0x30 - 7]; };   // 0x578370, stride 0x30
extern TypeDef30 g_typeDefs[];
extern signed char g_tileType[50][50];   // 0x5722e8
extern unsigned char g_tileByte[50][50]; // 0x56988c
void f_42f1c0(int x, int y);             // 0x42f1c0

// Tiles whose type has kind 0x11: byte grid set to 2, then 0x42f1c0 on each.
// MATCH: golf_clean.exe 0x0042f2c0 ?markKind11@@YAXXZ
void markKind11()
{
    int x, y;
    for (x = 0; x < 50; x++)
        for (y = 0; y < 50; y++)
            if (g_typeDefs[g_tileType[x][y]].kind == 0x11)
                g_tileByte[x][y] = 2;
    for (x = 0; x < 50; x++)
        for (y = 0; y < 50; y++)
            if (g_typeDefs[g_tileType[x][y]].kind == 0x11)
                f_42f1c0(x, y);
}

class Cell2c { public: void f_473ae0(); char pad[0x2c]; };   // 0x473ae0
extern int    g_rowBase[];               // 0x5a9370
extern int    g_rowWidth[];              // 0x53f3e8
extern int    g_rowHeight[];             // 0x5a5a2c
extern Cell2c g_cells[];                 // 0x5aaa30
void f_43d520(int base, int n);          // 0x43d520

// MATCH: golf_clean.exe 0x0043d670 ?freeTable@@YAXH@Z
void freeTable(int t)
{
    if (g_rowBase[t] == -1)
        return;
    f_43d520(g_rowBase[t], g_rowWidth[t] * g_rowHeight[t]);
    for (int i = 0; i < g_rowWidth[t] * g_rowHeight[t]; i++)
        g_cells[g_rowBase[t] + i].f_473ae0();
    g_rowWidth[t] = 0;
    g_rowBase[t] = -1;
}

struct Rect4 { int x0, y0, x1, y1; };
int inRect(int x, int y, Rect4* r);      // 0x492610
struct Hot20 { int id; Rect4 r; int b; int a; int pad; };   // 0x20 bytes: rect +4, b +0x14, a +0x18

class HotList {
public:
    int hit(int x, int y, int* a, int* b);   // 0x492a90
    char pad[0x50];
    Hot20* m_50; int pad54; int m_58;
};

// Topmost entry (last first) whose rect contains (x, y); -1 if none.
// MATCH: golf_clean.exe 0x00492a90 ?hit@HotList@@QAEHHHPAH0@Z
int HotList::hit(int x, int y, int* a, int* b)
{
    for (int i = m_58 - 1; i >= 0; i--) {
        if (inRect(x, y, &m_50[i].r)) {
            if (a)
                *a = m_50[i].a;
            if (b)
                *b = m_50[i].b;
            return i;
        }
    }
    return -1;
}

extern int g_4e43f0, g_4e440c, g_4e4408, g_4e43f8, g_83afd8, g_83afdc, g_4e43f4;

class Cfg485 {
public:
    void reset485ff0();                  // 0x485ff0
    char pad[0x574];
    int m_574, m_578, m_57c, m_580, m_584, m_588, m_58c, m_590, m_594, m_598, m_59c, m_5a0, m_5a4, m_5a8;
};

// MATCH: golf_clean.exe 0x00485ff0 ?reset485ff0@Cfg485@@QAEXXZ
void Cfg485::reset485ff0()
{
    m_574 = 0;
    m_580 = 0;
    m_57c = 0;
    m_578 = 0;
    m_5a8 = 0;
    m_5a4 = 0;
    m_584 = g_4e43f0;
    m_588 = g_4e440c;
    m_58c = g_4e4408;
    m_59c = g_4e43f8;
    m_590 = g_83afd8;
    m_594 = g_83afdc;
    m_598 = g_4e43f4;
}

extern int  g_msgA[8], g_msgB[8], g_msgC[8], g_msgTtl[8];   // 0x56c770, 0x56c794, 0x56a794, 0x56a924
extern char g_msgText[8][0x40];          // 0x56c570
extern int  g_msgNext;                   // 0x53bba8
extern char g_text[];                    // 0x51a068

// Queues the shared text buffer as a message (8-entry ring, life 0x30); 0x40c860 clears entries by (a, b).
// MATCH: golf_clean.exe 0x0040c720 ?queueMessage@@YAXHHH@Z
void queueMessage(int a, int b, int c)
{
    g_msgA[g_msgNext] = a;
    g_msgB[g_msgNext] = b;
    g_msgC[g_msgNext] = c;
    g_msgTtl[g_msgNext] = 0x30;
    strcpy(g_msgText[g_msgNext], g_text);
    g_msgNext = (g_msgNext + 1) % 8;
}
