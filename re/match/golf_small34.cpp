// golf_clean.exe functions, batch 34 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <string.h>

class Random {
public:
    unsigned short range(unsigned short n);   // 0x45c1e0
};
extern Random g_rng;                     // 0x822d9c
extern char g_text[];                    // 0x51a068
extern int g_altPanel55e924;             // 0x55e924
extern int g_selected4c2848;             // 0x4c2848
extern signed char g_slotIds[];          // 0x4c79a0
extern int g_sound4c2e08;                // 0x4c2e08
extern int g_mode567afc;                 // 0x567afc
int clickAlt434ac0(int x, int y);        // 0x434ac0
int hitSlot(int x, int y);               // 0x432f90
void showMessage40d320(int x, int y, int color, int mode);   // 0x40d320
struct Sound519 { void stop(int a); };   // 0x480c80
extern Sound519 g_sound519a60;           // 0x519a60
void pause45c0c0(int a);                 // 0x45c0c0
// Click on the slot panel: selects a slot (or none for 0x14), shows the undo help for 0x15 (game text at
// 0x4c7ec4 / 0x4c7e7c, placeholders here) or switches to the alternate panel for 0x16; returns rand(3).
// MATCH: golf_clean.exe 0x00433040 ?clickSlotPanel@@YAHHH@Z
int clickSlotPanel(int x, int y)
{
    if (g_altPanel55e924)
        return clickAlt434ac0(x, y);
    int hit = hitSlot(x, y);
    if (hit != -1) {
        if (hit < 0x14) {
            g_selected4c2848 = g_slotIds[hit];
        } else if (hit == 0x14) {
            g_selected4c2848 = -1;
        } else if (hit == 0x15) {
            strcpy(g_text, "<0x4c7ec4>");
            g_sound4c2e08 = -22;
            strcat(g_text, "<0x4c7e7c>");
            showMessage40d320(200, 0x154, 0x80007fff, -2);
            g_sound519a60.stop(0);
            pause45c0c0(0);
        } else if (hit == 0x16) {
            g_altPanel55e924 = 1;
            g_mode567afc = 0;
        }
    }
    return g_rng.range(3);
}
