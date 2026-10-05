// FLAGS golf_clean.exe: /O2 /GX
#include <stdlib.h>
#include <string.h>
#include "golf_hand_07_hdr.h"
struct O4a28a0 {
    char pad[0x10];
    int m_10;
    char pad2[0x1c - 0x14];
    int m_1c;
    int m_20;
    int m_24;
    int run(int);   // 0x401d10
    int go(int s, int p, int k) { m_1c = s; m_20 = 0; m_24 = k; return run(p); }
};
struct V1_4a28a0 { virtual void a0(); };
struct V2_4a28a0 { virtual void b0(); char pad[0xc0 - 4]; O4a28a0 m_c0; };
struct D4a28a0 : virtual V1_4a28a0, virtual V2_4a28a0 {
    char pad[0x2c - 4];
    char* m_2c[10];
    int m_54[10];
    int add(int a, const char* s, int c);
};
// MATCH: golf_clean.exe 0x004a28a0 ?add@D4a28a0@@QAEHHPBDH@Z
int D4a28a0::add(int a, const char* s, int c)
{
    int i = m_c0.m_10;
    if (i == 10)
        return 1;
    if (s) {
        if (m_2c[i])
            free(m_2c[i]);
        m_2c[i] = (char*)malloc(strlen(s) + 1);
        if (!m_2c[i])
            return 4;
        *m_2c[i] = 0;
        strcat(m_2c[i], s);
    }
    if (m_c0.go(a, i, 0))
        return 4;
    m_54[i] = c;
    return 0;
}
