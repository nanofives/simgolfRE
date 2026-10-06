// FLAGS golf_clean.exe: /O2 /GX
#include <windows.h>
#include <string.h>
#include <stdlib.h>
extern char g_83c340[];
extern const char s_004e41ec[];
extern const char s_004e41e0[];
extern const char s_004e41d4[];
extern const char s_004e41c0[];
extern const char s_004e41b0[];
void s0_log4925f0(char*);   // 0x4925f0
struct S0Heap474860 {
    int m_0;
    unsigned char m_4;
    char* m_8;
    char* m_c;
    int m_10;
    int m_14;
    char* alloc(int n);
};
// MATCH: golf_clean.exe 0x00474860 ?alloc@S0Heap474860@@QAEPADH@Z
char* S0Heap474860::alloc(int n)
{
    char buf[80];
    while (m_14 < n) {
        if (m_4 & 1)
            return 0;
        char* p = (char*)realloc(m_8, m_10 + 0x400);
        if (!p) {
            g_83c340[0] = 0;
            strcat(g_83c340, s_004e41ec);
            s0_log4925f0(g_83c340);
            strcat(g_83c340, s_004e41e0);
            itoa(m_10, buf, 10);
            strcat(g_83c340, buf);
            s0_log4925f0(g_83c340);
            strcat(g_83c340, s_004e41d4);
            itoa(m_14, buf, 10);
            strcat(g_83c340, buf);
            s0_log4925f0(g_83c340);
            strcat(g_83c340, s_004e41c0);
            itoa(n, buf, 10);
            strcat(g_83c340, buf);
            MessageBoxA(0, g_83c340, s_004e41b0, 0);
            exit(3);
        }
        m_8 = p;
        m_10 += 0x400;
        m_14 += 0x400;
    }
    char* r = m_c;
    m_c += n;
    m_14 -= n;
    return r;
}
