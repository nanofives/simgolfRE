// FLAGS golf_clean.exe: /O2 /GX
#include <windows.h>
struct C47ed30 {
    char pad[0x9c];
    unsigned int m_9c;
    unsigned int m_a0;
    char pad1[0x180 - 0xa4];
    int m_180;
    char pad2[0x1bc - 0x184];
    RECT m_rect;
    char pad3[0x1ec - 0x1cc];
    int m_1ec;
    int m_1f0;
    int width() { return m_rect.right - m_rect.left; }
    int height() { return m_rect.bottom - m_rect.top; }
    int hitTest(int x, int y);
};
// MATCH: golf_clean.exe 0x0047ed30 ?hitTest@C47ed30@@QAEHHH@Z
int C47ed30::hitTest(int x, int y)
{
    if (m_a0 & 2) {
        if (x < m_1f0 && y < m_1f0)
            return 0xd;
        if (x < m_1f0 && y > height() - m_1f0)
            return 0x10;
        if (x > width() - m_1f0 && y < m_1f0)
            return 0xe;
        if (x > width() - m_1f0 && y > height() - m_1f0)
            return 0x11;
        if (x < m_1f0)
            return 10;
        if (y < m_1f0)
            return 0xc;
        if (x > width() - m_1f0)
            return 0xb;
        if (y > height() - m_1f0)
            return 0xf;
    }
    unsigned int f = m_9c;
    if (f & 1)
        return 2;
    if (f & 0x10) {
        if (y < m_180)
            return 2;
    } else if ((f & 0x4000000) && y < m_1ec)
        return 2;
    return 0;
}
