// FLAGS golf_clean.exe: /O2 /GX
#include <windows.h>
#include "golf_hand_07_hdr.h"
struct Rect47c { int a, b, c, d; };
struct Hot47c { int hitRect(int x, int y, int* a, int* b, Rect47c* out); };
struct I47c1b0 { V4(a) virtual void a4(); virtual void a5(); virtual void a6(); virtual void v7(); };
struct C47c1b0;
extern C47c1b0* g_83ab2c;
extern void (*g_83ab04)(int, int);
struct C47c1b0 {
    V16(p) virtual int hit(int, int, Rect47c*); V8(q) V4(r) virtual void r4(); virtual void miss(int, int);
    V8(s) virtual void s8(); virtual void s9(); virtual void s10(); virtual void alt(int, int);
    char pad0[0x24 - 4];
    I47c1b0* m_24;
    char pad1[0x58 - 0x28];
    I47c1b0* m_58;
    char pad2[0x9c - 0x5c];
    unsigned int m_9c;
    unsigned int m_a0;
    char pad3[0xbc - 0xa4];
    Hot47c m_bc;
    char pad4[0x248 - 0xc0];
    void (*m_248)(int, int);
    void click(int x, int y, int flag);
};
// MATCH: golf_clean.exe 0x0047c1b0 ?click@C47c1b0@@QAEXHHH@Z
void C47c1b0::click(int x, int y, int flag)
{
    int a, b;
    Rect47c r;
    if ((m_9c & 0x200000) || (m_a0 & 8))
        return;
    if (flag == 0) {
        g_83ab2c = this;
        if (m_248)
            m_248(x, y);
    }
    if (g_83ab04)
        g_83ab04(x, y);
    if (flag == 0) {
        if (m_bc.hitRect(x, y, &a, &b, &r) < 0 || !hit(a, b, &r))
            miss(x, y);
        if (m_24)
            m_24->v7();
    } else {
        alt(x, y);
        if (m_58)
            m_58->v7();
    }
}
