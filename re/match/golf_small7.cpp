// Small golf_clean.exe functions, batch 7 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <windows.h>
#include <stdlib.h>
#include <string.h>

struct Cell2c { char pad[0x2c]; };
extern int    g_rowBase[];               // 0x5a9370 (-1 = none)
extern int    g_rowWidth[];              // 0x53f3e8
extern Cell2c g_cells[];                 // 0x5aaa30
extern Cell2c g_noCell;                  // 0x53ba48

// MATCH: golf_clean.exe 0x0043d6f0 ?cellAt@@YAPAUCell2c@@HHH@Z
Cell2c* cellAt(int table, int col, int row)
{
    int base = g_rowBase[table];
    if (base == -1)
        return &g_noCell;
    int w = g_rowWidth[table];
    if (col >= w)
        return &g_noCell;
    return &g_cells[w * row + base + col];
}

class Member492920b { public: void f_492920(int n); };   // 0x492920

class List478 {
public:
    void reset478a20();                  // 0x478a20
    char pad0[0xb0];
    int  m_b0;                           // +0xb0
    Member492920b m_b4;                  // +0xb4
    char padB5[0x114 - 0xb5];
    int  m_114[0x28];                    // +0x114
    int  m_1b4[0x28];                    // +0x1b4
};

// MATCH: golf_clean.exe 0x00478a20 ?reset478a20@List478@@QAEXXZ
void List478::reset478a20()
{
    m_b4.f_492920(0x50);
    m_b0 = 0;
    for (int i = 0; i < 0x28; i++) {
        if (m_1b4[i])
            m_1b4[i] = 0;
        m_114[i] = -2;                   // every slot (the je at 0x478a2f only skips the store above)
    }
}

class Str49d {
public:
    int   assign(int n);                 // 0x49d090
    char* dup(const char* s);            // 0x49d100
    int   alloc(int n);                  // 0x474820
    char* take(int n);                   // 0x474860
    void  f_49d0e0();                    // 0x49d0e0
    char  pad[0x18];
    int   m_18;                          // +0x18
};
extern char g_4e49f4[];                  // 0x4e49f4

// MATCH: golf_clean.exe 0x0049d100 ?dup@Str49d@@QAEPADPBD@Z
char* Str49d::dup(const char* s)
{
    char* p = take(strlen(s) + 1);
    strcpy(p, s);
    return p;
}

// MATCH: golf_clean.exe 0x0049d090 ?assign@Str49d@@QAEHH@Z
int Str49d::assign(int n)
{
    if (m_18)
        f_49d0e0();
    if (alloc(n))
        return 1;
    dup(g_4e49f4);
    m_18 = 1;
    return 0;
}

class Widget47 {
public:
    void setQuad(int a, int b, int c, int d);              // 0x476310
    void f_477c30(const char* s, int x, int y, int len);   // 0x477c30
    void f_477da0(const char* s, int x, int y, int z, int len);  // 0x477da0
};
extern Widget47* g_widget;               // 0x4c1570

// MATCH: golf_clean.exe 0x004049d0 ?text4049d0@@YAXPBDHHH@Z
void text4049d0(const char* s, int x, int y, int q)
{
    g_widget->setQuad(q, -1, 2, 2);
    if (s)
        g_widget->f_477c30(s, x, y, strlen(s));
}

// MATCH: golf_clean.exe 0x00404b70 ?text404b70@@YAXPBDHHH@Z
void text404b70(const char* s, int x, int y, int q)
{
    g_widget->setQuad(q, -1, 2, 2);
    if (s)
        g_widget->f_477da0(s, x, y, 0, strlen(s));
}

// MATCH: golf_clean.exe 0x00404bc0 ?text404bc0@@YAXPBDHHH@Z
void text404bc0(const char* s, int x, int y, int q)
{
    g_widget->setQuad(q, 1, 0, 1);
    if (s)
        g_widget->f_477da0(s, x, y, 0, strlen(s));
}

class Res487 {
public:
    void free487f30();                   // 0x487f30
    void f_487f80();                     // 0x487f80
    char pad[0x15c];
    void* m_15c; void* m_160;
};

// MATCH: golf_clean.exe 0x00487f30 ?free487f30@Res487@@QAEXXZ
void Res487::free487f30()
{
    f_487f80();
    if (m_15c) {
        free(m_15c);
        m_15c = 0;
    }
    if (m_160) {
        free(m_160);
        m_160 = 0;
    }
}

class Mapping {
public:
    void close();                        // 0x492e80
    int    m_0;
    void*  m_view;                       // +0x04
    HANDLE m_file;                       // +0x08 (-1 = none)
    HANDLE m_map;                        // +0x0c
};

// MATCH: golf_clean.exe 0x00492e80 ?close@Mapping@@QAEXXZ
void Mapping::close()
{
    if (m_view) {
        UnmapViewOfFile(m_view);
        m_view = 0;
    }
    if (m_map) {
        CloseHandle(m_map);
        m_map = 0;
    }
    if (m_file != INVALID_HANDLE_VALUE) {
        CloseHandle(m_file);
        m_file = INVALID_HANDLE_VALUE;
    }
}
