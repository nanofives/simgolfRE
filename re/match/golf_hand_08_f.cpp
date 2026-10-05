// FLAGS golf_clean.exe: /O2 /GX
// probe
#define VP4(n) virtual void n##0(); virtual void n##1(); virtual void n##2(); virtual void n##3();
#define VP16(n) VP4(n##a) VP4(n##b) VP4(n##c) VP4(n##d)
struct I4973c0 { VP4(a) virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); };
struct L4973c0 {
    VP16(a) VP16(b) VP16(c) VP4(d) virtual void v52(); virtual void v53(); virtual void v54(); virtual void v55(); virtual void v56(); virtual void setPos(int, int);
    char pad[0x84 - 4];
    I4973c0* m_84;
};
extern L4973c0* g_83ab2c;
extern int g_83ff18;
struct C4973c0 {
    VP16(a) VP16(b) VP16(c) VP16(d) VP4(e) VP4(f) virtual void v72();
    char pad[0x130 - 4];
    L4973c0* m_130;
    char pad1[0x578 - 0x134];
    int m_578;
    int m_57c;
    int m_580;
    int m_584;
    int m_588;
    int m_58c;
    char pad2[0x598 - 0x590];
    int m_598;
    int m_59c;
    char pad3[0x13ac - 0x5a0];
    void (*m_13ac)(int, int);
    void scroll(int dir);
};
// MATCH: golf_clean.exe 0x004973c0 ?scroll@C4973c0@@QAEXH@Z
void C4973c0::scroll(int dir)
{
    m_598 = 0;
    g_83ab2c = m_130;
    g_83ff18 = 0;
    switch (dir) {
    case 1:
        if (m_58c > m_580)
            m_58c--;
        break;
    case 2:
        if (m_58c < m_584)
            m_58c++;
        break;
    }
    if (!m_588) {
        m_130->setPos(m_578, m_58c);
        if (m_13ac)
            m_13ac(m_578, m_58c);
    } else {
        m_130->setPos(m_578, m_584 - m_58c + m_580);
        if (m_13ac)
            m_13ac(m_578, m_584 - m_58c + m_580);
    }
    if (m_130->m_84)
        m_130->m_84->v7();
    m_59c = -1;
    v72();
}

struct P481760 { char pad[0x18]; int m_w; int m_h; };
struct C481760 {
    char pad[0x9c];
    unsigned char m_9c;
    char pad1[0x1a4 - 0x9d];
    int m_1a4;
    int m_1a8;
    char pad2[0x52c - 0x1ac];
    P481760* m_52c;
    P481760* m_530;
    P481760* m_534;
    P481760* m_538;
    char pad3[0x54c - 0x53c];
    P481760* m_54c;
    P481760* m_550;
    void measure();
};
void C481760::measure()
{
    P481760 *b, *c, *d;
    if (m_9c & 0x10) {
        if (!m_52c || !(b = m_534) || !(c = m_530) || !(d = m_538))
            return;
        m_1a4 = m_52c->m_w + b->m_w;
        m_1a4 = m_1a4 > d->m_w + c->m_w ? m_1a4 : d->m_w + c->m_w;
        m_1a8 = m_52c->m_h + c->m_h;
        m_1a8 = m_1a8 > d->m_h + b->m_h ? m_1a8 : d->m_h + b->m_h;
    } else {
        if (!m_54c || !(b = m_550) || !(c = m_530) || !(d = m_538))
            return;
        m_1a4 = m_54c->m_w + b->m_w;
        m_1a4 = m_1a4 > d->m_w + c->m_w ? m_1a4 : d->m_w + c->m_w;
        m_1a8 = m_54c->m_h + c->m_h;
        m_1a8 = m_1a8 > d->m_h + b->m_h ? m_1a8 : d->m_h + b->m_h;
    }
}
