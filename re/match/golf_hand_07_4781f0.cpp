// FLAGS golf_clean.exe: /O2 /GX
#include <string.h>
#include "golf_hand_07_hdr.h"
#define MIN4781(a, b) ((a) < (b) ? (a) : (b))
struct R4781 { int left, top, right, bottom; };
struct F4781 { int m_0; int m_4; };
struct C4781f0 {
    char pad[0x5c];
    F4781* m_5c;
    int width(const char* s, int n);              // 0x477280
    int height();                                 // 0x477560
    int draw(const char* s, int x, int y, int n); // 0x4775b0
    int drawRight(const char* s, R4781* r, int n);
};
// MATCH: golf_clean.exe 0x004781f0 ?drawRight@C4781f0@@QAEHPBDPAUR4781@@H@Z
int C4781f0::drawRight(const char* s, R4781* r, int n)
{
    if (s && r) {
        if (!m_5c || !m_5c->m_4)
            return 3;
        if (MIN4781((int)strlen(s), n) >= 0) {
            n = MIN4781((int)strlen(s), n);
            if (n) {
                int x = r->right - width(s, n);
                int top = r->top;
                int y = (r->bottom - top - height()) / 2 + top;
                return draw(s, x, y, n);
            }
        }
    }
    return 0;
}
