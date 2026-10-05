// FLAGS golf_clean.exe: /O2 /GX
#include "golf_hand_07_hdr.h"
struct C47c5d0;
extern C47c5d0* g_83ab2c;
struct I47c5d0 { V4(a) virtual void a4(); virtual void a5(); virtual void a6(); virtual void v7(); };
struct L47c5d0 { int m_0; C47c5d0* m_4; };
struct C47c5d0 {
    V32(p) virtual void p32(); virtual void p33(); virtual void p34(); virtual int onKey(int, int);
    V16(q) virtual void q16(); virtual void close(int); V16(r) V4(s) virtual void tab();
    char pad0[0x38 - 4];
    I47c5d0* m_38;
    char pad1[0x9c - 0x3c];
    unsigned int m_9c;
    unsigned int m_a0;
    char pad2[0x138 - 0xa4];
    int m_138;
    L47c5d0* m_13c;
    int m_140;
    char pad3[0x25c - 0x144];
    int (*m_25c)(int, int);
    C47c5d0* first() { return m_138 ? m_13c->m_4 : 0; }
    int key(int a, int k);
};
// MATCH: golf_clean.exe 0x0047c5d0 ?key@C47c5d0@@QAEHHH@Z
int C47c5d0::key(int a, int k)
{
    int r = 0;
    if ((m_9c & 0x200000) || (m_a0 & 8))
        return 0;
    switch (k) {
    case 0xd:
    case 0x1000d:
        close(-1);
        break;
    case 0x1b:
        close(-2);
        break;
    }
    if (m_140) {
        C47c5d0* c = first();
        if (k == 9)
            c->tab();
        else {
            r = c->key(a, k);
            if (r)
                return 1;
        }
    }
    g_83ab2c = this;
    if (m_25c)
        r = m_25c(a, k);
    r += onKey(a, k);
    if (m_38)
        m_38->v7();
    return r;
}
