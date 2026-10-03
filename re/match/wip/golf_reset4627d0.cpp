// golf_clean.exe 0x004627d0 (release). WIP: the original keeps a loop of 0x400 stores of -1 to the single
// address 0x8371f4 (dec eax; mov [0x8371f4], ecx; jne). A plain loop is removed by /O2; with `volatile` (a
// guess, not evidence) it reaches 90.9% with dec/store swapped (for-up, for-down, do/while tried).
// FLAGS golf_clean.exe: /O2
extern int g_838200, g_834174, g_8371fc, g_82c160;
extern volatile int g_8371f4;

// MATCH: golf_clean.exe 0x004627d0 ?reset4627d0@@YAXXZ
void reset4627d0()
{
    g_838200 = 0;
    g_834174 = 0;
    g_8371fc = 0;
    g_82c160 = -1;
    for (int i = 0; i < 0x400; i++)
        g_8371f4 = -1;
}

