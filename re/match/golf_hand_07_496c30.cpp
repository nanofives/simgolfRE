// FLAGS golf_clean.exe: /O2 /GX
#include "golf_hand_07_hdr.h"
struct I496c30 { V32(a) V16(b) V4(c) virtual void c4(); virtual void c5(); virtual int width(); virtual int height(); };
struct C496c30;
extern C496c30* g_83ab30;
extern int g_83ab34;
struct C496c30 {
    V16(a) V8(b) virtual void b8(); virtual void b9(); virtual void b10(); virtual void press(int, int);
    char pad[0x278 - 4];
    I496c30* m_278;
    char pad2[0x598 - 0x27c];
    int m_598;
    char pad3[0x5a4 - 0x59c];
    int m_dir;
    char pad4[0x5ac - 0x5a8];
    int m_5ac, m_5b0, m_5b4, m_5b8;
    int width() { return m_278 ? m_278->width() : 0; }
    int height() { return m_278 ? m_278->height() : 0; }
    void down(int x, int y);
};
// MATCH: golf_clean.exe 0x00496c30 ?down@C496c30@@QAEXHH@Z
void C496c30::down(int x, int y)
{
    if (width() > height()) {
        if (x < m_5ac) {
            m_dir = -1;
            m_598 = 0;
            press(x, y);
        } else if (x > m_5b4) {
            m_dir = 1;
            m_598 = 0;
            press(x, y);
        } else
            m_dir = 0;
    } else {
        if (y < m_5b0) {
            m_dir = -1;
            m_598 = 0;
            press(x, y);
        } else if (y > m_5b8) {
            m_dir = 1;
            m_598 = 0;
            press(x, y);
        } else
            m_dir = 0;
    }
    if (!g_83ab30) {
        g_83ab30 = this;
        g_83ab34 = 0;
    }
}
