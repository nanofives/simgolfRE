// FLAGS golf_clean.exe: /O2 /GX
#include <string.h>
#include "golf_hand_07_hdr.h"
extern unsigned char g_53ea24[50][50];
extern unsigned char g_82376c[50][50];
extern unsigned char g_822da0[50][50];
extern unsigned char g_5a6378[50][50];
extern unsigned char g_56c7e4[50][50];
void add4615f0(int x, int y, int v);   // 0x4615f0
// MATCH: golf_clean.exe 0x004616f0 ?spread4616f0@@YAXXZ
void spread4616f0()
{
    int i, j;
    int total = 0;
    for (i = 0; i < 50; i++)
        for (j = 0; j < 50; j++)
            total += g_53ea24[i][j];
    memset(g_82376c, 0, sizeof(g_82376c));
    memset(g_822da0, 0, sizeof(g_822da0));
    if (total < 10)
        return;
    if (total > 0x6d6)
        total = 0x6d6;
    for (i = 0; i < 50; i++) {
        for (j = 0; j < 50; j++) {
            if (g_5a6378[i][j])
                add4615f0(i, j, g_5a6378[i][j] * 20000 / total);
            if (g_56c7e4[i][j])
                add4615f0(i, j, g_56c7e4[i][j] * -40000 / total);
        }
    }
}
