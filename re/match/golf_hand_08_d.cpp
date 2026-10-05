// FLAGS golf_clean.exe: /O2 /GX
// probe
#include <windows.h>
#define VP4(n) virtual void n##0(); virtual void n##1(); virtual void n##2(); virtual void n##3();
#define VP16(n) VP4(n##a) VP4(n##b) VP4(n##c) VP4(n##d)
struct M488fd0a { void f486ec0(); };
struct M488fd0b { void start(void (*)(), int, void*, int, int); };   // 0x486d60
void cb488fb0();
struct C488fd0 {
    VP16(a) VP4(b) VP4(c) virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(int, int);
    char pad[0x1ac - 4];
    RECT m_1ac;
    char pad1[0x57c - 0x1bc];
    M488fd0a m_57c;
    char pad2[0x5ac - 0x57d];
    int m_5ac;
    int m_5b0;
    M488fd0b m_5b4;
    char pad3[0x5f8 - 0x5b5];
    char* m_5f8;
    int f4801f0();
    void toScreen(int*, int*);        // 0x47b170
    void show(char*, RECT*);          // 0x480220
    void onEvent(int e);
};
// MATCH: golf_clean.exe 0x00488fd0 ?onEvent@C488fd0@@QAEXH@Z
void C488fd0::onEvent(int e)
{
    if (e == 0) {
        m_57c.f486ec0();
        if (f4801f0() && m_5f8) {
            int x = 0;
            e = 0;
            RECT r = m_1ac;
            toScreen(&x, &e);
            int dy = e - r.top;
            int dx = x - r.left;
            r.left += dx;
            r.right += dx;
            r.top += dy;
            r.bottom += dy;
            show(m_5f8, &r);
        }
    } else if (e == 1) {
        int t = m_5b0;
        if (t <= 0)
            t = m_5ac;
        m_5b4.start(cb488fb0, 2, this, t, 5);
    } else
        v30(0, 0);
}

struct R492690 { int a, b, c, d; };
struct E492690 {
    E492690();            // 0x492650
    virtual ~E492690();   // 0x492d70
    R492690 m_4;
    int m_14, m_18, m_1c;
};
extern int g_83d348;
struct C492690 {
    char pad[0x50];
    E492690* m_50;
    int m_54;
    int m_58;
    void grow();
};
// MATCH: golf_clean.exe 0x00492690 ?grow@C492690@@QAEXXZ
void C492690::grow()
{
    m_54 += 10;
    E492690* n = new E492690[m_54];
    for (int i = 0; i < m_58; i++)
        n[i] = m_50[i];
    g_83d348 = 1;
    delete[] m_50;
    g_83d348 = 0;
    m_50 = n;
}
