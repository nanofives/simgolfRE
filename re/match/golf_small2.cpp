// More small golf_clean.exe functions reached by the scenarios (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <string.h>

extern short g_tileFlags[50][50];        // 0x53caf0
extern unsigned char g_gridA[50][50];    // 0x5830b8
extern unsigned char g_gridB[50][50];    // 0x59c090
extern int g_screenW;                    // 0x822c8c (800 / 1024 / 1280)
extern unsigned char g_buf56fcb0[0x1002];// 0x56fcb0
extern unsigned char g_buf59d81c[0x100]; // 0x59d81c
struct Rec3c { short id; char pad[0x3a]; };
extern Rec3c g_rec3c[100];               // 0x56d1d8, stride 0x3c, id -1 = empty
void f_4017d0(int i);                    // 0x4017d0

struct GolferFlags { char pad[0x88]; unsigned short tflags; char pad2[0x100 - 0x8a]; };
extern GolferFlags g_gflags[];           // 0x5794b8 (golfer records)

// MATCH: golf_clean.exe 0x004493b0 ?tileFlag20@@YAHHH@Z
int tileFlag20(int x, int y)
{
    return g_tileFlags[x][y] & 0x20;
}

// MATCH: golf_clean.exe 0x0040a130 ?setGrids@@YAXHHDD@Z
void setGrids(int x, int y, char a, char b)
{
    g_gridA[x][y] = a;
    g_gridB[x][y] = b;
}

// Scales a 320-wide coordinate to the window width, rounded.
// MATCH: golf_clean.exe 0x00404970 ?scaleX@@YAHH@Z
int scaleX(int v)
{
    return (g_screenW * v + 160) / 320;
}

// MATCH: golf_clean.exe 0x00401c00 ?forEachRec3c@@YAXXZ
void forEachRec3c()
{
    for (int i = 0; i < 100; i++)
        if (g_rec3c[i].id != -1)
            f_4017d0(i);
}

// MATCH: golf_clean.exe 0x0045b880 ?clearBuffers@@YAXXZ
void clearBuffers()
{
    memset(g_buf56fcb0, 0, sizeof(g_buf56fcb0));
    memset(g_buf59d81c, 0xff, sizeof(g_buf59d81c));
}

// First recent-thought flag of a golfer (+0x88): 2 if bit 15, else 1 if bit 14 is clear, else 0.
// MATCH: golf_clean.exe 0x004675d0 ?thoughtFlag@@YAHH@Z
int thoughtFlag(int golfer)
{
    unsigned short f = g_gflags[golfer].tflags;
    if (f & 0x8000)
        return 2;
    return ~(unsigned int)f >> 14 & 1;
}

class Member492920 { public: void f_492920(int n); };   // 0x492920
struct Obj83ad44 { int a, b, i, base0, base; };         // fields at +8, +0xc, +0x10
extern Obj83ad44* g_obj83ad44;                         // 0x83ad44

class Widget {
public:
    void setQuad(int a, int b, int c, int d);   // 0x476310
    void reset478a70();                          // 0x478a70
    int  value477580();                          // 0x477580
    char pad0[0x5c];
    Obj83ad44* m_5c;                             // +0x5c
    char pad60[0x6c - 0x60];
    int  m_6c; char pad70[0xc];
    int  m_7c; char pad80[0xc];
    int  m_8c; char pad90[0xc];
    int  m_9c; char padA0[0x254 - 0xa0];
    Member492920 m_254; char pad255[0x2b4 - 0x255];
    int  m_2b4;
};

// MATCH: golf_clean.exe 0x00476310 ?setQuad@Widget@@QAEXHHHH@Z
void Widget::setQuad(int a, int b, int c, int d)
{
    m_6c = a;
    m_7c = b;
    m_8c = c;
    m_9c = d;
}

// MATCH: golf_clean.exe 0x00478a70 ?reset478a70@Widget@@QAEXXZ
void Widget::reset478a70()
{
    m_254.f_492920(10);
    m_2b4 = 0;
}

// MATCH: golf_clean.exe 0x00477580 ?value477580@Widget@@QAEHXZ
int Widget::value477580()
{
    if (!m_5c)
        m_5c = g_obj83ad44;
    if (m_5c->i >= 0)
        return m_5c->base + m_5c->i;
    return m_5c->base0;
}
