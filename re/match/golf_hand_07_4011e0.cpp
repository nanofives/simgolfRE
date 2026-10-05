// FLAGS golf_clean.exe: /O2 /GX
#include <string.h>
#include "golf_hand_07_hdr.h"
struct Obj4011e0 { int m_0; char pad[0x1c]; short type; short x; short m_24; short m_26; short m_28; short m_2a; short m_2c; short m_2e; int m_30; int m_34; int m_38; };
extern Obj4011e0 g_56d1b8[100];
struct Kind4011e0 { char c; char pad[0x24]; };
extern Kind4011e0 g_55d758[];
struct Grid4011e0 { char cell[19][6]; char pad[2]; };
extern Grid4011e0 g_4e6d70[];
struct Rng4011e0 { unsigned short roll(int); };   // 0x45c1e0
extern Rng4011e0 g_822d9c;
// MATCH: golf_clean.exe 0x004011e0 ?spawn4011e0@@YAXHHFF@Z
void spawn4011e0(int x, int kind, short a, short b)
{
    int r, c;
    for (int i = 0; i < 100; i++) {
        if (g_56d1b8[i].type == -1) {
            memset(&g_56d1b8[i].m_0, 0, sizeof(int));
            g_56d1b8[i].x = x;
            g_56d1b8[i].type = g_55d758[kind].c;
            g_56d1b8[i].m_26 = a;
            g_56d1b8[i].m_28 = b;
            g_56d1b8[i].m_2c = 0x140;
            do {
                r = g_822d9c.roll(4);
                c = g_822d9c.roll(4);
            } while (g_4e6d70[x].cell[r][c] != -1);
            g_56d1b8[i].m_30 = (r * 0x400 + 0x200) / 2;
            g_56d1b8[i].m_34 = (c * 0x400 + 0x200) / 2;
            return;
        }
    }
}
