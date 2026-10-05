// FLAGS golf_clean.exe: /O2 /GX
// probe
#include <string.h>
struct T44b1b0 { ~T44b1b0() { release(); } virtual void v0(); void release(); char pad[0x28]; };   // release 0x473ae0
struct C44b1b0 {
    T44b1b0 m_0, m_2c, m_58, m_84, m_b0, m_dc, m_108, m_134;
    T44b1b0 m_160[8];
    T44b1b0 m_2c0[0x24];
    ~C44b1b0();
};
// MATCH: golf_clean.exe 0x0044b1b0 ??1C44b1b0@@QAE@XZ
C44b1b0::~C44b1b0()
{
}

#define MIN477eb0(a, b) ((a) < (b) ? (a) : (b))
struct R477eb0 { int left, top, right, bottom; };
struct F477eb0 { int m_0; int m_4; };
struct C477eb0 {
    char pad[0x5c];
    F477eb0* m_5c;
    int width(const char*);                       // 0x477250
    int height();                                 // 0x477560
    int draw(const char*, int, int, int);         // 0x4775b0
    int drawCentered(const char* s, R477eb0* r, int len);
};
// MATCH: golf_clean.exe 0x00477eb0 ?drawCentered@C477eb0@@QAEHPBDPAUR477eb0@@H@Z
int C477eb0::drawCentered(const char* s, R477eb0* r, int len)
{
    if (s && r) {
        R477eb0 rc = *r;
        if (m_5c == 0 || m_5c->m_4 == 0)
            return 3;
        int n = MIN477eb0((int)strlen(s), len);
        if (n >= 0) {
            len = MIN477eb0((int)strlen(s), len);
            if (len == 0)
                return 0;
            int x = (rc.right - rc.left - width(s)) / 2 + rc.left;
            int y = (rc.bottom - rc.top - height()) / 2 + rc.top;
            return draw(s, x, y, len);
        }
    }
    return 0;
}
