// FLAGS golf_clean.exe: /O2 /GX
// golf_clean.exe batch t2 (release /O2; /GX only for the EH frames where a function has one). Names chosen here, not recovered.
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <mmsystem.h>

extern "C" char* __cdecl _itoa(int, char*, int);

extern char g_51a068[];
extern char g_824134[];
extern int g_571fd4;        // cash
extern int g_5a7144;
extern int g_834170;        // date
extern unsigned g_59e7b8;   // flags
extern char g_5a34e0;       // course type
void __cdecl FUN_0040daa0(int);
void __cdecl FUN_0040cb00(unsigned, int, int);
extern const char s_004d44d4[];
extern const char s_004d44a4[];
extern const char s_004d4470[];
extern const char s_004d4430[];
extern const char s_004d43f4[];
extern const char s_004d43d4[];
extern const char s_004d43c0[];
extern const char s_004d43b4[];
extern const char s_004d4394[];
extern const char s_004d4364[];
extern const char s_004d4354[];
extern const char s_004d4324[];
extern const char s_004d42d8[];
extern const char s_004d42bc[];
extern const char s_004d4288[];
extern const char s_004d4240[];
extern const char s_004d4200[];
extern const char s_004d41e8[];
extern const char s_004d41c4[];
extern const char s_004d418c[];
extern const char s_004d4138[];
extern const char s_004d40f0[];
extern const char s_004d4094[];
extern const char s_004d4030[];
extern const char s_004d3ffc[];
extern const char s_004d3fc0[];
extern const char s_004d3f7c[];
extern const char s_004d3f10[];
extern const char s_004d3ebc[];
extern const char s_004d3e7c[];
extern const char s_004d3e5c[];
extern const char s_004d3dfc[];
extern const char s_004d3da0[];
extern const char s_004d3d58[];
extern const char s_004d3d20[];
extern const char s_004d3cb8[];
// MATCH: golf_clean.exe 0x0045fd80 FUN_0045fd80
void __cdecl FUN_0045fd80()
{
    const char* tail;
    if (g_834170 != 0x20)
        goto date2;
    if (g_59e7b8 & 0x1000000)
        return;
    switch (g_5a34e0) {
    case 0:
        strcpy(g_51a068, s_004d44d4);
        FUN_0040daa0(-1);
        strcat(g_51a068, s_004d44a4);
        strcat(g_51a068, s_004d4470);
        FUN_0040daa0(-1);
        strcat(g_51a068, s_004d4430);
        strcat(g_51a068, s_004d43f4);
        strcat(g_51a068, s_004d43d4);
        strcat(g_51a068, s_004d43c0);
        strcat(g_51a068, s_004d43b4);
        strcat(g_51a068, _itoa((g_571fd4 * 100) / 1000, g_824134, 10));
        strcat(g_51a068, s_004d4394);
        FUN_0040daa0(-1);
        tail = s_004d4364;
        break;
    case 1:
        strcpy(g_51a068, s_004d4354);
        FUN_0040daa0(-1);
        strcat(g_51a068, s_004d4324);
        strcat(g_51a068, s_004d42d8);
        strcat(g_51a068, s_004d42bc);
        strcat(g_51a068, s_004d4288);
        strcat(g_51a068, s_004d4240);
        strcat(g_51a068, s_004d4200);
        strcat(g_51a068, _itoa((g_571fd4 * 100) / 1000, g_824134, 10));
        strcat(g_51a068, s_004d41e8);
        FUN_0040daa0(-1);
        tail = s_004d41c4;
        break;
    case 3:
        strcpy(g_51a068, s_004d44d4);
        FUN_0040daa0(-1);
        strcat(g_51a068, s_004d418c);
        strcat(g_51a068, s_004d4138);
        strcat(g_51a068, s_004d40f0);
        strcat(g_51a068, s_004d4094);
        strcat(g_51a068, s_004d4030);
        FUN_0040daa0(-1);
        tail = s_004d3ffc;
        break;
    case 2:
        strcpy(g_51a068, s_004d44d4);
        FUN_0040daa0(-1);
        strcat(g_51a068, s_004d3fc0);
        strcat(g_51a068, s_004d3f7c);
        strcat(g_51a068, s_004d3f10);
        strcat(g_51a068, s_004d3ebc);
        strcat(g_51a068, s_004d3e7c);
        FUN_0040daa0(-1);
        tail = s_004d3e5c;
        break;
    default:
        goto def;
    }
    strcat(g_51a068, tail);
def:
    FUN_0040cb00(0x8000211f, 1, -4);
    g_5a7144 = 0x80;
date2:
    if (g_834170 == 0x1400) {
        strcpy(g_51a068, s_004d3dfc);
        strcat(g_51a068, s_004d3da0);
        strcat(g_51a068, s_004d3d58);
        strcat(g_51a068, s_004d3d20);
        strcat(g_51a068, s_004d3cb8);
        FUN_0040cb00(0x80002190, 0, -4);
    }
}
