// golf_clean.exe 0x0045ae30 / 0x0045ae50 (release). WIP 66.7%: the original is
//   mov eax,[g]; test eax,eax; jne L; ret; L: mov [g],0; ret
// and VC6 /O2 here emits a single exit for every spelling tried (void/int, early return, == 0, != 0 + else,
// returning the old value). Not in the 100% suite.
// FLAGS golf_clean.exe: /O2
extern int g_flag822b98, g_flag822b9c;   // 0x822b98, 0x822b9c

// MATCH: golf_clean.exe 0x0045ae30 ?clearFlag822b98@@YAXXZ
void clearFlag822b98()
{
    if (!g_flag822b98)
        return;
    g_flag822b98 = 0;
}

// MATCH: golf_clean.exe 0x0045ae50 ?clearFlag822b9c@@YAXXZ
void clearFlag822b9c()
{
    if (!g_flag822b9c)
        return;
    g_flag822b9c = 0;
}

