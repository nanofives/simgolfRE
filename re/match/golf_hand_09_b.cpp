// FLAGS golf_clean.exe: /O2 /GX
#include <windows.h>
#pragma vtordisp(off)
extern int g_83ab2c;
struct V1_49fa90 {
    virtual int onKey(int key);
    void refresh();      // 0x480ce0
    char pad[0x130 - 4];
    int m_130;
};
struct V2_49fa90 { char pad[0x5c]; int m_5c; char pad2[0xd0 - 0x60]; int m_d0; char pad3[0xf0 - 0xd4]; unsigned m_f0; };
struct D49fa90 : virtual V1_49fa90, virtual V2_49fa90 {
    int m_4;
    void (*m_8)(unsigned);
    int m_c, m_10, m_14, m_18;
    virtual int onKey(int key);
    unsigned test(int bit);              // 0x49f030
    void show(int i, unsigned on, int);  // 0x49f370
    void toggle(int i);                  // 0x49eec0
    unsigned flags() { return m_f0; }
};
// MATCH: golf_clean.exe 0x0049fa90 ?onKey@D49fa90@@UAEHH@Z
int D49fa90::onKey(int key)
{
    g_83ab2c = m_130;
    show(m_4, test(m_4), 0);
    switch (key) {
    case 0x26:
    case 0x68:
        m_4--;
        m_4 = m_4 > 0 ? m_4 : 0;
        break;
    case 0x28:
    case 0x62:
        m_4++;
        m_4 = m_4 < m_d0 - 1 ? m_4 : m_d0 - 1;
        break;
    case 0x25:
    case 0x64:
        m_4 += m_5c;
        if (m_4 >= m_d0)
            m_4 -= m_5c;
        break;
    case 0x27:
    case 0x66:
        m_4 -= m_5c;
        if (m_4 < 0)
            m_4 += m_5c;
        break;
    case 0x20:
        toggle(m_4);
        break;
    default:
        return 0;
    }
    if (m_8)
        m_8(flags());
    show(m_4, test(m_4), 1);
    refresh();
    return 1;
}
