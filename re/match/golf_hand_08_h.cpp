// FLAGS golf_clean.exe: /O2 /GX
// probe
#include <string.h>
#define VP4(n) virtual void n##0(); virtual void n##1(); virtual void n##2(); virtual void n##3();
#define VP16(n) VP4(n##a) VP4(n##b) VP4(n##c) VP4(n##d)
struct Scr474260 { VP16(a) VP16(b) VP4(c) virtual void v36(); virtual void v37(); virtual void v38(); virtual void setScale(int, int, int); virtual void scale(int*, int*, int*); };
extern Scr474260* g_83ad50;
struct D474260 { VP16(a) virtual void v16(); virtual void v17(); virtual void v18(); virtual int blit(void*, int, int, int, void*, int); };
struct S474260 { int m_0; void* m_4; };
struct C474260 {
    int m_0;
    D474260* m_4;
    char pad[0x20 - 8];
    int m_20;
    int m_24;
    int drawScaled(S474260* s, int x, int y, int d, int sx, int sy, int div, S474260* mask, int e);
};
// MATCH: golf_clean.exe 0x00474260 ?drawScaled@C474260@@QAEHPAUS474260@@HHHHHH0H@Z
int C474260::drawScaled(S474260* s, int x, int y, int d, int sx, int sy, int div, S474260* mask, int e)
{
    if (!s)
        return 0x10;
    if (!m_4 || !s->m_4)
        return 7;
    int nx, ny, nd;
    g_83ad50->scale(&nx, &ny, &nd);
    g_83ad50->setScale(sx, sy, div);
    int o20 = m_20;
    int o24 = m_24;
    m_20 = o20 * sx / div;
    m_24 = o24 * sy / div;
    int r = m_4->blit(s->m_4, x + m_20, y + m_24, d, mask ? mask->m_4 : 0, e);
    m_20 = o20;
    m_24 = o24;
    g_83ad50->setScale(nx, ny, nd);
    return r;
}

struct O49b690 { VP16(a) VP16(b) virtual void v32(); virtual void post(int, int, int); };
struct N49b690 { int m_0, m_4; O49b690* m_obj; };
struct L49b690 {
    char pad[8];
    int m_8;
    N49b690* m_c;
    void f49b970(int);
    O49b690* first() { if (m_8) return m_c->m_obj; return 0; }
};
struct E49b690 { int m_id; char pad[0x10]; unsigned char m_flags; char m_name[0x58 - 0x15]; };
struct C49b690 {
    virtual void v0(); virtual void v1(); virtual void added(const char*, int);
    char pad[0xc0 - 4];
    L49b690 m_c0;
    char pad1[0x170 - 0xd0];
    E49b690 m_e[16];
    char pad2[0x6f8 - 0x6f0];
    int m_6f8;
    char pad3[0x77c - 0x6fc];
    int m_77c;
    int m_780;
    void f497b40();
    void add(const char* name, int id, unsigned char flags);
};
// MATCH: golf_clean.exe 0x0049b690 ?add@C49b690@@QAEXPBDHE@Z
void C49b690::add(const char* name, int id, unsigned char flags)
{
    int i;
    for (i = 0; i < 16; i++) {
        if (!m_e[i].m_id)
            break;
        if (m_e[i].m_id == id)
            return;
    }
    if (i < 16) {
        m_e[i].m_id = id;
        m_e[i].m_flags = flags;
        m_e[i].m_name[0] = 0;
        strcat(m_e[i].m_name, name);
        if (flags & 1)
            m_780 = id;
    }
    m_6f8++;
    if (m_77c == m_780)
        f497b40();
    added(m_e[i].m_name, m_e[i].m_id);
    m_c0.f49b970(m_e[i].m_id);
    m_c0.first()->post(0x1000, 0, 0);
}
