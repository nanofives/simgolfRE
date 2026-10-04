// golf_clean.exe functions, batch 21 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <stdlib.h>
#include <string.h>

void freeBuf840820();                        // 0x49d460
int setSection(const char* a, const char* b);   // 0x487e70
char* readValue487e90();                     // 0x487e90
extern char* g_buf840820;                    // 0x840820
// Reads the [jackal] FILEWIN value into a fresh heap copy at 0x840820 (6: section missing, 1: no value, 4: no memory).
// MATCH: golf_clean.exe 0x0049d3b0 ?loadFileWin@@YAHXZ
int loadFileWin()
{
    freeBuf840820();
    if (setSection("jackal", "FILEWIN"))
        return 6;
    char* v = readValue487e90();
    if (!v)
        return 1;
    if (g_buf840820) {
        free(g_buf840820);
        g_buf840820 = 0;
    }
    g_buf840820 = (char*)malloc(strlen(v) + 1);
    if (!g_buf840820)
        return 4;
    *g_buf840820 = 0;
    strcat(g_buf840820, v);
    return 0;
}
