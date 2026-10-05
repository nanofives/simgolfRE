// FLAGS golf_clean.exe: /O2 /GX
#include <string.h>
#include "golf_hand_07_hdr.h"
extern char g_80b130[][100];
extern char g_56e928[][50];
extern char g_567328[];
extern const char s_004e40a0[];
extern const char s_004e9a84[];
extern const char s_004c4944[];
extern const char s_004c3c5c[];
extern const char s_004c6c10[];
int list43d2a0(const char*, const char*, const char*); // 0x43d2a0
// MATCH: golf_clean.exe 0x004724c0 ?listThemes4724c0@@YAHXZ
int listThemes4724c0()
{
    int n = 0;
    list43d2a0(s_004e40a0, s_004e9a84, s_004e9a84);
    for (int i = 1; i < 100; i++) {
        if (!strstr(g_80b130[i], s_004c4944) && strlen(g_80b130[i]) && strcmp(g_80b130[i], s_004c3c5c)) {
            strcpy(g_56e928[n++], g_80b130[i]);
        }
    }
    strcpy(g_567328, s_004c6c10);
    return n;
}
