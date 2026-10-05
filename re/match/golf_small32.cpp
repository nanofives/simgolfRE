// golf_clean.exe functions, batch 32 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <stdlib.h>
#include <string.h>

extern char g_text[];                    // 0x51a068
extern char g_numBuf[];                  // 0x58a528
// Appends n with thousands separators ("-" first when negative; zero-pads the groups after the first).
// MATCH: golf_clean.exe 0x0042dc00 ?appendNumber@@YAXH@Z
void appendNumber(int n)
{
    if (n < 0) {
        strcat(g_text, "-");
        n = -n;
    }
    if (n >= 1000) {
        appendNumber(n / 1000);
        strcat(g_text, ",");
    }
    int r = n % 1000;
    if (r < 100 && n != 0) {
        if (n >= 1000)
            strcat(g_text, "0");
        if (r < 10 && n >= 100)
            strcat(g_text, "0");
    }
    strcat(g_text, itoa(r, g_numBuf, 10));
}
