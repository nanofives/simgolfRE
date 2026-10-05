// FLAGS golf_clean.exe: /O2 /GX
#include <string.h>
#include "golf_hand_07_hdr.h"
struct Site40daa0 { char m_0; char name[0x81]; };
extern Site40daa0 g_4c1ea8[];
struct Course40daa0 { char site; char pad[0x2d]; };
extern Course40daa0 g_571ff4[];
extern int g_59bf90;
extern int g_5685f0;
extern char g_51a068[];
extern const char s_004c5364[], s_004c5360[], s_004c5374[], s_004c5370[], s_004c5384[], s_004e9a84[], s_004c5398[], s_004c5394[];
int f45b9f0(int);   // 0x45b9f0
int f44faf0(int);   // 0x44faf0
// MATCH: golf_clean.exe 0x0040daa0 ?courseTitle40daa0@@YAXH@Z
void courseTitle40daa0(int full)
{
    const char* s;
    if (!f45b9f0(0))
        strcat(g_51a068, g_4c1ea8[g_571ff4[g_59bf90].site].name);
    if (full != -1) {
        switch (f44faf0(g_5685f0 - 1)) {
        case 1:
            s = s_004c5364;
            if (full != 1)
                s = s_004c5360;
            break;
        case 2:
            s = s_004c5374;
            if (full != 1)
                s = s_004c5370;
            break;
        case 3:
            s = s_004c5384;
            if (full != 1)
                s = s_004e9a84;
            break;
        default:
            s = s_004c5398;
            if (full != 1)
                s = s_004c5394;
            break;
        }
        strcat(g_51a068, s);
    }
}
