// golf_clean.exe functions, batch 22 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <string.h>

typedef int Page[0x400];
extern int g_82b15c, g_838200, g_8371f4, g_82c160;
extern Page g_830164, g_832164, g_831164, g_833164, g_82b160, g_82615c, g_82c164, g_82715c, g_82415c, g_82d164,
    g_82915c, g_82f164, g_8361f4, g_82515c, g_82a15c, g_8351f4, g_834174, g_8371fc, g_82815c, g_82e164;
// Snapshot: copies ten 4 KB tables to their backups (and two scalars).
// MATCH: golf_clean.exe 0x00462800 ?snapshotTables@@YAXXZ
void snapshotTables()
{
    g_82b15c = g_838200;
    memcpy(g_832164, g_830164, sizeof(Page));
    memcpy(g_833164, g_831164, sizeof(Page));
    memcpy(g_82615c, g_82b160, sizeof(Page));
    memcpy(g_82715c, g_82c164, sizeof(Page));
    memcpy(g_82d164, g_82415c, sizeof(Page));
    memcpy(g_82f164, g_82915c, sizeof(Page));
    memcpy(g_82515c, g_8361f4, sizeof(Page));
    memcpy(g_8351f4, g_82a15c, sizeof(Page));
    memcpy(g_8371fc, g_834174, sizeof(Page));
    memcpy(g_82e164, g_82815c, sizeof(Page));
    g_8371f4 = g_82c160;
}
