// golf_clean.exe functions, batch 24 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <stdlib.h>
#include <string.h>

extern int g_delay5694a4;                // 0x5694a4
extern char g_text[];                    // 0x51a068
extern char g_msg5a6d40[];               // 0x5a6d40
extern char g_dir569498;                 // 0x569498 (1: count up)
extern int g_count5a9ccc;                // 0x5a9ccc
extern int g_arg5a34ec;                  // 0x5a34ec
extern unsigned int g_flags;             // 0x59e7b8
extern int g_date;                       // 0x834170
extern int g_left5a7144;                 // 0x5a7144
void show40d320(int x, int y, int a, int n);   // 0x40d320
// Message ticker: waits g_delay5694a4 calls, then copies the message into the text buffer and shows it with a
// counter that runs up to 3 (direction 1) or down to 0; outside flag 0x8000 it ends the up phase after
// g_left5a7144 calls on dates not divisible by 3.
// MATCH: golf_clean.exe 0x0040d6a0 ?tickMessage@@YAXXZ
void tickMessage()
{
    if (g_delay5694a4) {
        g_delay5694a4--;
        return;
    }
    strcpy(g_text, g_msg5a6d40);
    int n = -1;
    if (g_dir569498 != 1) {
        if (g_count5a9ccc) {
            n = g_count5a9ccc--;
            if (!n)
                return;
        }
    } else if (g_count5a9ccc < 3) {
        n = ++g_count5a9ccc;
    }
    show40d320(0xf0, -8, g_arg5a34ec, n);
    if (!(g_flags & 0x8000) && g_date % 3 != 0 && --g_left5a7144 <= 0)
        g_dir569498 = 0;
}

struct Hole208 { signed char par; char pad[0x1ff]; unsigned int flags; char pad2[4]; };
extern Hole208 g_holes[];                // 0x575ab0, stride 0x208
extern char g_numBuf[];                  // 0x58a528
extern char* g_par3Names[];              // 0x4c2e88
extern char* g_par4Names[];              // 0x4c2e38
extern char* g_par5Names[];              // 0x4c2ed8
int f_45b9f0(int hole);                  // 0x45b9f0
// Appends the hole's name: "Hole <n>" unless hole flags 0x81 are set, then a name by par (3, 4, else) from three
// per-hole tables; appends nothing when FUN_0045b9f0(hole) is non-zero.
// MATCH: golf_clean.exe 0x00407280 ?appendHoleName@@YAXH@Z
void appendHoleName(int hole)
{
    if (g_holes[hole].flags & 0x81) {
        if (f_45b9f0(hole))
            return;
        switch (g_holes[hole].par) {
        case 3: strcat(g_text, g_par3Names[hole]); break;
        case 4: strcat(g_text, g_par4Names[hole]); break;
        default: strcat(g_text, g_par5Names[hole]); break;
        }
    } else {
        strcat(g_text, "Hole ");
        strcat(g_text, itoa(hole, g_numBuf, 10));
    }
}
