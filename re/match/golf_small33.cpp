// golf_clean.exe functions, batch 33 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <stdlib.h>
#include <string.h>

extern char g_text[];                    // 0x51a068
extern char g_numBuf[];                  // 0x58a528
extern int g_lastCents;                  // 0x569628
// Appends an amount in cents (and keeps it in g_lastCents) as "units.cc"; a negative amount replaces a trailing '+' and gets a '-'.
// MATCH: golf_clean.exe 0x0042dd50 ?appendCents@@YAXH@Z
void appendCents(int n)
{
    g_lastCents = n;
    if (n < 0) {
        int len = strlen(g_text);
        if (g_text[len - 1] == '+')
            g_text[len - 1] = 0;
        strcat(g_text, "-");
        n = -n;
    }
    strcat(g_text, itoa(n / 100, g_numBuf, 10));
    strcat(g_text, ".");
    if (n % 100 < 10)
        strcat(g_text, "0");
    strcat(g_text, itoa(n % 100, g_numBuf, 10));
}

void appendNumber(int n);                // 0x42dc00
void showMessage40d320(int x, int y, int color, int mode);   // 0x40d320
struct Sound519 { void stop(int a); };   // 0x480c80
extern Sound519 g_sound519a60;           // 0x519a60
void pause45c0c0(int a);                 // 0x45c0c0
extern int g_cashDiv100;                 // 0x571fd4 (cash / 100)
extern unsigned int g_flags;             // 0x59e7b8
extern int g_holesPlus1;                 // 0x5685f0
// Affordability check for `cost` (in hundreds): 1 when it is free, covered by the cash, under flag 0x1000000 or
// with fewer than 3 holes open; otherwise shows "<cost> ... <cash>." and returns 0. The sentence parts
// (0x4c495c, 0x4c4948) are game text, replaced by placeholders (masked by the matcher).
// MATCH: golf_clean.exe 0x00406c30 ?canAfford@@YAHH@Z
int canAfford(int cost)
{
    if (cost <= 0 || cost <= g_cashDiv100 || (g_flags & 0x1000000) || g_holesPlus1 <= 3)
        return 1;
    strcpy(g_text, "<0x4c495c>");
    appendNumber(cost * 100);
    int cash = g_cashDiv100;
    strcat(g_text, "<0x4c4948>");
    appendNumber(cash * 100);
    strcat(g_text, ".");
    showMessage40d320(200, 0x96, 0x80007fff, -2);
    g_sound519a60.stop(0);
    pause45c0c0(0);
    return 0;
}
