// FLAGS golf_clean.exe: /O2 /GX
// probe
#include <windows.h>
#define VP4(n) virtual void n##0(); virtual void n##1(); virtual void n##2(); virtual void n##3();
#define VP16(n) VP4(n##a) VP4(n##b) VP4(n##c) VP4(n##d)
struct Frame47 { VP16(a) VP16(b) VP16(c) VP16(d) VP16(e) VP4(f) VP4(g) VP4(h) virtual int height(); };
extern int g_83ff10;
struct Win47b {
    char pad[0x9c];
    unsigned m_9c;
    char pada0[0x15c - 0xa0];
    Frame47* m_15c;
    char pad160[0x180 - 0x160];
    int m_180, m_184, m_188;
    void grow(RECT* r);
};
// MATCH: golf_clean.exe 0x0047cc10 ?grow@Win47b@@QAEXPAUtagRECT@@@Z
void Win47b::grow(RECT* r)
{
    if (r) {
        if (m_9c & 4)
            r->bottom += g_83ff10;
        if (m_9c & 8)
            r->right += g_83ff10;
        if ((m_9c & 0x400) || (m_9c & 0x11)) {
            int d = m_184;
            r->left -= d;
            r->right += d;
            r->top -= d;
            r->bottom += d;
            if (m_188 != -1)
                r->bottom += m_188 - m_184;
        }
        if (m_9c & 0x10)
            r->top += m_184 - m_180;
        if (m_15c && !(m_9c & 0x20000000))
            r->top -= m_15c->height();
    }
}
struct Draw475 { void blit(void* src, int, int, int x, int y, int w, int h); };   // 0x475c60
extern Draw475 g_83a500;
extern char g_83a7b8[];
extern RECT g_83a4d8;
extern int g_83aaa0, g_83aaa4, g_83aaa8, g_83aab4, g_83aab8;
void flushDirty(RECT*);
inline void setRect1(RECT* r, int l, int t, int rr, int b) { r->left = l; r->top = t; r->right = rr; r->bottom = b; }
inline void setRectWH(RECT* r, int x, int y, int w, int h) { r->left = x; r->top = y; r->right = x + w; r->bottom = y + h; }
inline void setRectWH2(RECT* r, int x, int y, int w, int h) { r->right = w + x; r->left = x; r->bottom = h + y; r->top = y; }
struct XRect : RECT { XRect(int l, int t, int r, int b) { left = l; top = t; right = r; bottom = b; } };
     // 0x47cdb0
void addDirty1(int flush)
{
    RECT rc;
    if (g_83aaa0) {
        g_83a500.blit(g_83a7b8, 0, 0, g_83aaa4, g_83aaa8, g_83aab4, g_83aab8);
        setRect1(&rc, g_83aaa4, g_83aaa8, g_83aaa4 + g_83aab4, g_83aaa8 + g_83aab8);
        UnionRect(&g_83a4d8, &g_83a4d8, &rc);
        if (flush) {
            int save = g_83aaa0;
            g_83aaa0 = 0;
            flushDirty(&g_83a4d8);
            g_83aaa0 = save;
            g_83a4d8.left = 0;
            g_83a4d8.right = 0;
        }
    }
}
void addDirty2(int flush)
{
    RECT rc;
    if (g_83aaa0) {
        g_83a500.blit(g_83a7b8, 0, 0, g_83aaa4, g_83aaa8, g_83aab4, g_83aab8);
        setRect1(&rc, g_83aaa4, g_83aaa8, g_83aab4 + g_83aaa4, g_83aab8 + g_83aaa8);
        UnionRect(&g_83a4d8, &g_83a4d8, &rc);
        if (flush) {
            int save = g_83aaa0;
            g_83aaa0 = 0;
            flushDirty(&g_83a4d8);
            g_83aaa0 = save;
            g_83a4d8.left = 0;
            g_83a4d8.right = 0;
        }
    }
}
// MATCH: golf_clean.exe 0x0047d060 ?addDirty3@@YAXH@Z
void addDirty3(int flush)
{
    RECT rc;
    if (g_83aaa0) {
        g_83a500.blit(g_83a7b8, 0, 0, g_83aaa4, g_83aaa8, g_83aab4, g_83aab8);
        setRectWH(&rc, g_83aaa4, g_83aaa8, g_83aab4, g_83aab8);
        UnionRect(&g_83a4d8, &g_83a4d8, &rc);
        if (flush) {
            int save = g_83aaa0;
            g_83aaa0 = 0;
            flushDirty(&g_83a4d8);
            g_83aaa0 = save;
            g_83a4d8.left = 0;
            g_83a4d8.right = 0;
        }
    }
}
void addDirty4(int flush)
{
    RECT rc;
    if (g_83aaa0) {
        g_83a500.blit(g_83a7b8, 0, 0, g_83aaa4, g_83aaa8, g_83aab4, g_83aab8);
        setRectWH2(&rc, g_83aaa4, g_83aaa8, g_83aab4, g_83aab8);
        UnionRect(&g_83a4d8, &g_83a4d8, &rc);
        if (flush) {
            int save = g_83aaa0;
            g_83aaa0 = 0;
            flushDirty(&g_83a4d8);
            g_83aaa0 = save;
            g_83a4d8.left = 0;
            g_83a4d8.right = 0;
        }
    }
}
void addDirty5(int flush)
{
    if (g_83aaa0) {
        g_83a500.blit(g_83a7b8, 0, 0, g_83aaa4, g_83aaa8, g_83aab4, g_83aab8);
        XRect rc(g_83aaa4, g_83aaa8, g_83aaa4 + g_83aab4, g_83aaa8 + g_83aab8);
        UnionRect(&g_83a4d8, &g_83a4d8, &rc);
        if (flush) {
            int save = g_83aaa0;
            g_83aaa0 = 0;
            flushDirty(&g_83a4d8);
            g_83aaa0 = save;
            g_83a4d8.left = 0;
            g_83a4d8.right = 0;
        }
    }
}
void addDirty6(int flush)
{
    if (g_83aaa0) {
        g_83a500.blit(g_83a7b8, 0, 0, g_83aaa4, g_83aaa8, g_83aab4, g_83aab8);
        XRect rc(g_83aaa4, g_83aaa8, g_83aab4 + g_83aaa4, g_83aab8 + g_83aaa8);
        UnionRect(&g_83a4d8, &g_83a4d8, &rc);
        if (flush) {
            int save = g_83aaa0;
            g_83aaa0 = 0;
            flushDirty(&g_83a4d8);
            g_83aaa0 = save;
            g_83a4d8.left = 0;
            g_83a4d8.right = 0;
        }
    }
}
