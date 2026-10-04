// golf_clean.exe functions, batch 26 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <string.h>

extern char g_text[];                    // 0x51a068
int worldToScreen(int wx, int wy, int* sx, int* sy, int flags);   // 0x42fb90
void drawText404(const char* s, int x, int y, int color);        // 0x404bc0
struct Font476 { void select(void* style, int a, int b, int c); }; // 0x4762d0
extern Font476* g_font4c1570;            // 0x4c1570
extern char g_style51b360[];             // 0x51b360
extern int g_labelTime[8];               // 0x56a924
extern int g_labelColor[8];              // 0x56a794
extern int g_labelX[8];                  // 0x56c770
extern int g_labelY[8];                  // 0x56c794
extern char g_labelText[8][0x40];        // 0x56c570
// Draws the floating world labels that still have time left (4 px above their point), counting their time down.
// MATCH: golf_clean.exe 0x0040c7a0 ?drawLabels@@YAXXZ
void drawLabels()
{
    int sx, sy;
    for (int i = 0; i < 8; i++) {
        if (g_labelTime[i]) {
            g_labelTime[i]--;
            if (worldToScreen(g_labelX[i], g_labelY[i], &sx, &sy, 0)) {
                strcpy(g_text, g_labelText[i]);
                g_font4c1570->select(g_style51b360, 0, 0, 0);
                drawText404(g_text, sx, sy - 4, g_labelColor[i]);
            }
        }
    }
}
