// golf_clean.exe 0x0045c0c0 (release). WIP 49%: logic as below, register roles differ (the original keeps
// timeGetTime in ebx and ms in edi). Confirms 0x45ae30/0x45ae50 return the flag value they clear.
// FLAGS golf_clean.exe: /O2
#include <windows.h>
#include <mmsystem.h>
extern int g_822d94, g_822d68, g_822d7c, g_83afcc, g_822b98, g_822b9c;
void pumpMessages();                     // 0x45c030
int  poll45ae70();                       // 0x45ae70
int  takeFlag822b9c();                   // 0x45ae50 (returns the value it clears)
int  takeFlag822b98();                   // 0x45ae30
void f_483d30();                         // 0x483d30

// Pumps messages until input arrives or `ms` timeGetTime units pass (0 = no limit); 1 when input arrived.
// Uses the return values of 0x45ae50/0x45ae30: those return the flag they clear (see wip/golf_flags.cpp).
// MATCH: golf_clean.exe 0x0045c0c0 ?waitInput@@YAHH@Z
int waitInput(int ms)
{
    DWORD t0 = timeGetTime();
    g_822d94 = 0;
    g_822d68 = 0;
    for (;;) {
        pumpMessages();
        DWORD t = timeGetTime();
        if (g_822d68 || poll45ae70() || g_83afcc)
            break;
        if (ms && (int)(t - t0) >= ms)
            break;
    }
    g_822d94 = g_822b9c;
    g_822d7c = g_822b98;
    if (!poll45ae70())
        return 0;
    g_822d94 = takeFlag822b9c();
    g_822d7c = takeFlag822b98();
    f_483d30();
    return 1;
}
