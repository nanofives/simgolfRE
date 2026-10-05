// FLAGS golf_clean.exe: /O2 /GX
// probe
#include <string.h>
#define VP4(n) virtual void n##0(); virtual void n##1(); virtual void n##2(); virtual void n##3();
#define VP16(n) VP4(n##a) VP4(n##b) VP4(n##c) VP4(n##d)
extern char g_51a068[];
extern const char s_004c7ec4[];
extern const char s_004c7e7c[];
extern int g_4c2e08, g_55e928, g_567afc, g_59e7b8, g_542f20, g_4c2848;
struct S433d30 { void f480c80(int); };
extern S433d30 g_519a60;
int f433c60(int, int);
void f40d320(int, int, int, int);
int f45c0c0(int);
#pragma warning(disable: 4715)
int click433d30(int x, int y)
{
    int r = f433c60(x, y);
    switch (r) {
    case -2:
        g_55e928 = 0;
        g_567afc = 1;
        g_59e7b8 &= ~2;
        return -1;
    case -3:
        strcpy(g_51a068, s_004c7ec4);
        g_4c2e08 = -22;
        strcat(g_51a068, s_004c7e7c);
        f40d320(200, 0x154, 0x80007fff, -2);
        g_59e7b8 &= ~2;
        g_55e928 = 0;
        g_567afc = 1;
        g_519a60.f480c80(0);
        return f45c0c0(0);
    }
    if (r >= 0) {
        g_542f20 = r;
        g_59e7b8 |= 2;
        g_4c2848 = -1;
    }
}

struct C47af60;
struct N47af60 { int m_0; C47af60* m_obj; int m_8; N47af60* m_next; };
struct L47af60 {
    int m_0;
    int m_4;
    N47af60* m_cur;
    int m_count;
    int m_idx;
    C47af60* cur() { if (m_4) return m_cur->m_obj; return 0; }
    void next()
    {
        if (m_4) {
            m_cur = m_cur->m_next;
            if (++m_idx == m_count)
                m_idx = 0;
        }
    }
};
struct G47af60 {
    char pad[0x134];
    L47af60 m_list;
    C47af60* first() { if (m_list.m_4) return m_list.m_cur->m_obj; return 0; }
    void advance()
    {
        if (m_list.m_4) {
            m_list.m_cur = m_list.m_cur->m_next;
            if (++m_list.m_idx == m_list.m_count)
                m_list.m_idx = 0;
        }
    }
};
extern C47af60* g_83ab98;
struct C47af60 {
    VP16(a) VP16(b) VP16(c) VP4(d) VP4(e) virtual void v56(); virtual void v57(); virtual void v58(); virtual void v59(); virtual void select(int);
    char pad[0x130 - 4];
    G47af60* m_130;
    int f4801f0();
    void cycle();
};
// MATCH: golf_clean.exe 0x0047af60 ?cycle@C47af60@@QAEXXZ
void C47af60::cycle()
{
    if (m_130 && m_130->m_list.m_count > 1) {
        g_83ab98 = m_130->first();
        for (int i = 0; i < m_130->m_list.m_count; i++) {
            m_130->advance();
            if (m_130->m_list.cur()->f4801f0())
                break;
        }
        if (m_130->m_list.cur() != g_83ab98) {
            select(0);
            C47af60* o = m_130->m_list.cur();
            if (o)
                o->select(1);
        }
    }
}
