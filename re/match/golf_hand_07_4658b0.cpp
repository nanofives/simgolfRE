// FLAGS golf_clean.exe: /O2 /GX
#include <string.h>
#include "golf_hand_07_hdr.h"
struct T4658b0 { char m_0, m_1; unsigned char m_2; char pad[0x2c - 3]; };
extern T4658b0 g_5849e0[];
extern char g_80b130[][100];
extern char g_51a068[];
extern char g_567328[];
extern const char s_004c84e8[];
extern const char s_004c856c[];
extern const char s_004e9a84[];
void clear4659a0();                                   // 0x4659a0
int list43d2a0(const char*, const char*, const char*); // 0x43d2a0
void load437fa0(char*, int, int);                     // 0x437fa0
// MATCH: golf_clean.exe 0x004658b0 ?loadThemes4658b0@@YAXH@Z
void loadThemes4658b0(int on)
{
    clear4659a0();
    if (on) {
        strcpy(g_51a068, s_004c84e8);
        strcat(g_51a068, g_567328);
        strcat(g_51a068, s_004c856c);
        int n = list43d2a0(g_51a068, s_004e9a84, s_004e9a84);
        for (int i = 0; i < n; i++) {
            g_5849e0[i + 1].m_2 |= 1;
            load437fa0(g_80b130[i], i + 1, -1);
        }
    }
}
