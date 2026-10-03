// golf_clean.exe 0x0047eee0 (release). WIP 55%: same logic, different loop shape (the original keeps p in eax,
// q in edx, n in esi, tests n before the loop and wraps p with `cmp eax, ring; jae`). Tried while, do-while,
// for, index form. Not in the 100% suite.
// FLAGS golf_clean.exe: /O2

extern char  g_ring[10];                 // 0x83aba4 .. 0x83abad
extern char* g_ringPos;                  // 0x4e42ec (last written byte)

// 0 when the n chars of s equal the last n chars written to the 10-byte ring (read backwards), else 1.
// MATCH: golf_clean.exe 0x0047eee0 ?ringMismatch@@YAHPBDH@Z
int ringMismatch(const char* s, int n)
{
    const char* q = s + n - 1;
    char* p = g_ringPos;
    while (n > 0) {
        if (*p != *q)
            return 1;
        p--;
        q--;
        if (p < g_ring)
            p = g_ring + 9;
        n--;
    }
    return 0;
}

