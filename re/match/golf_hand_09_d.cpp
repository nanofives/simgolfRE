// FLAGS golf_clean.exe: /O2 /GX
#include <string.h>
struct Placed46d0c0 { short m_type; char pad[0xe]; };
extern Placed46d0c0 g_placed58bcb8[256];
struct Hole46d0c0 { char m_0; char pad[3]; short m_4; char pad2[0x200 - 6]; unsigned char m_200; char pad3[7]; };
extern Hole46d0c0 g_holes575ab0[19];
extern int g_holesPlus15685f0;
extern int g_5a8c60;
extern int g_5a47e0;
struct Types46d0c0 { int d[0x2df0]; };
extern Types46d0c0 g_types4d6088, g_types58f338;
struct Tab46d0c0 { int d[0x668a]; };
extern Tab46d0c0 g_tab543d10, g_tab520a28;
void remove40e400(int);
void init46d040();
// MATCH: golf_clean.exe 0x0046d0c0 ?reset46d0c0@@YAXXZ
void reset46d0c0()
{
    int i, v;
    for (i = 0; i < 256; i++)
        if (g_placed58bcb8[i].m_type == 0x11 || g_placed58bcb8[i].m_type == 0x12)
            remove40e400(i);
    g_types4d6088 = g_types58f338;
    g_tab543d10 = g_tab520a28;
    for (i = 0; i < 18; i++) {
        if (g_holes575ab0[g_holesPlus15685f0].m_0) {
            g_holes575ab0[g_holesPlus15685f0].m_0 = 3;
            v = g_holes575ab0[g_holesPlus15685f0].m_4;
            if ((g_holes575ab0[g_holesPlus15685f0].m_200 & 0x60) && v > 250)
                v += 25;
            if (v > 300)
                v -= g_5a8c60 * 25;
            if (v <= 50)
                g_holes575ab0[g_holesPlus15685f0].m_0 = 2;
            if (v >= 250)
                g_holes575ab0[g_holesPlus15685f0].m_0 = 4;
            if (v >= 475)
                g_holes575ab0[g_holesPlus15685f0].m_0 = 5;
            if (v > 625)
                g_holes575ab0[g_holesPlus15685f0].m_0 = 6;
            if (v <= 50)
                g_holes575ab0[g_holesPlus15685f0].m_0 = 2;
            if (v >= 250)
                g_holes575ab0[g_holesPlus15685f0].m_0 = 4;
            if (v >= 475)
                g_holes575ab0[g_holesPlus15685f0].m_0 = 5;
            if (v > 625)
                g_holes575ab0[g_holesPlus15685f0].m_0 = 6;
        }
    }
    g_5a47e0 = 0;
    init46d040();
}
