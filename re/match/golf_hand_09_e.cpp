// FLAGS golf_clean.exe: /O2 /GX
#include <windows.h>
#define VP4(n) virtual void n##0(); virtual void n##1(); virtual void n##2(); virtual void n##3();
#define VP16(n) VP4(n##a) VP4(n##b) VP4(n##c) VP4(n##d)
struct H473cb0 { int m_0; int m_4; };
struct Img473cb0 {
    int m_0;
    int m_4;
    int load(const char*, int, int, int, int);   // 0x475840
    int blit(H473cb0*, int, int);                // 0x475b60
    void release();                              // 0x474cb0
};
extern Img473cb0 g_img839348;
struct Scr473cb0 { VP16(a) VP16(b) VP4(c) virtual void e0(); virtual void e1(); virtual void e2(); virtual void setScale(int, int, int); virtual void getScale(int*, int*, int*); };
extern Scr473cb0* g_83ad50;
struct Dev473cb0 { VP16(a) virtual void b0(); virtual int draw(int, int, int, int); };
struct C473cb0 {
    int m_0;
    Dev473cb0* m_4;
    const char* m_8;
    char pad[0x20 - 0xc];
    int m_20;
    int m_24;
    int drawScaled(H473cb0* src, int x, int y, int sx, int sy, int div, H473cb0* mask);
};
// MATCH: golf_clean.exe 0x00473cb0 ?drawScaled@C473cb0@@QAEHPAUH473cb0@@HHHHH0@Z
int C473cb0::drawScaled(H473cb0* src, int x, int y, int sx, int sy, int div, H473cb0* mask)
{
    int a, b, c, r, dx, dy;
    if (!src)
        return 0x10;
    if (!m_4 || !src->m_4)
        return 7;
    if (m_8) {
        r = g_img839348.load(m_8, 0, 10, 0xec, 0);
        if (r)
            return r;
        r = g_img839348.blit(src, x, y);
        g_img839348.release();
        return r;
    }
    g_83ad50->getScale(&a, &b, &c);
    g_83ad50->setScale(sx, sy, div);
    dx = m_20 * sx / div;
    dy = m_24 * sy / div;
    m_4->draw(src->m_4, dx + x, dy + y, mask ? mask->m_4 : 0);
    g_83ad50->setScale(a, b, c);
    return 0;
}
