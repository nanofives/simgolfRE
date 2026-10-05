// FLAGS golf_clean.exe: /O2 /GX
// probe
#define VP4(n) virtual void n##0(); virtual void n##1(); virtual void n##2(); virtual void n##3();
#define VP16(n) VP4(n##a) VP4(n##b) VP4(n##c) VP4(n##d)
struct Mgr474 {
    VP16(a) VP16(b) VP4(c) virtual void d0(); virtual void d1(); virtual void d2();
    virtual void setScale(int mul, int mul2, int div);      // 0x9c
    virtual void getScale(int* mul, int* mul2, int* div);   // 0xa0
};
extern Mgr474* g_83ad50s;
struct Dev474 { VP16(a) VP4(b) VP4(c) virtual void d0(); virtual void d1(); virtual void d2(); virtual int blit(Dev474* src, int x, int y, int flags, Dev474* mask); };   // 0x6c
struct Surf474 {
    int m_0;
    Dev474* m_dev;
    int m_8;
    char pad[0x20 - 0xc];
    int m_20, m_24;
    int blit(Surf474* src, int x, int y, int flags, Surf474* mask);
    int draw(int a1, int a2, int a3, int a4, int mul, int mul2, int div, int a8, int a9);
    int draw474440(int a1, int a2, int a3, int a4, int a8, int a9);
};
// MATCH: golf_clean.exe 0x004740f0 ?blit@Surf474@@QAEHPAU1@HHH0@Z
int Surf474::blit(Surf474* src, int x, int y, int flags, Surf474* mask)
{
    if (m_8)
        return 0x18;
    if (!src)
        return 0x10;
    if (m_dev && src->m_dev) {
        int d, sy, sx;
        g_83ad50s->getScale(&sx, &sy, &d);
        int dx = m_20 * sx / d;
        int dy = m_24 * sy / d;
        return m_dev->blit(src->m_dev, x + dx, y + dy, flags, mask ? mask->m_dev : 0);
    }
    return 7;
}
// MATCH: golf_clean.exe 0x00474380 ?draw@Surf474@@QAEHHHHHHHHHH@Z
int Surf474::draw(int a1, int a2, int a3, int a4, int mul, int mul2, int div, int a8, int a9)
{
    int d, m2, m1;
    int x = m_20;
    int y = m_24;
    g_83ad50s->getScale(&m1, &m2, &d);
    g_83ad50s->setScale(mul, mul2, div);
    m_20 = mul * m_20 / div;
    m_24 = m_24 * mul2 / div;
    int r = draw474440(a1, a2, a3, a4, a8, a9);
    m_20 = x;
    m_24 = y;
    g_83ad50s->setScale(m1, m2, d);
    return r;
}
