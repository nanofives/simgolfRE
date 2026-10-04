// golf_clean.exe functions, batch 30 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <string.h>

class Random {
public:
    unsigned short range(unsigned short n);   // 0x45c1e0
};
extern Random g_rng;                     // 0x822d9c
extern char g_text[];                    // 0x51a068
extern char g_msg5a6d40[];               // 0x5a6d40
extern char g_dir569498;                 // 0x569498
extern int g_busy53df54;                 // 0x53df54
extern int g_mode567afc;                 // 0x567afc
extern int g_arg5a34ec;                  // 0x5a34ec
extern int g_sound4c2e08;                // 0x4c2e08
extern int g_left5a7144;                 // 0x5a7144
extern int g_delay5694a4;                // 0x5694a4
extern int g_difficulty;                 // 0x822c88
extern int g_r56d1a8, g_r56d1ac;         // 0x56d1a8, 0x56d1ac
// Starts the message ticker (tickMessage, 0x40d6a0) with the text buffer: refused (0) while one runs or is pending
// unless prio >= 1, and always in mode 3. Display time = strlen / (3 or 4 at difficulty > 0) + 16 ticks;
// a negative prio delays the start by -prio ticks.
// MATCH: golf_clean.exe 0x0040cb00 ?startMessage@@YAHHHH@Z
int startMessage(int arg, int prio, int sound)
{
    if (((g_dir569498 || g_busy53df54) && prio <= 0) || g_mode567afc == 3)
        return 0;
    g_busy53df54 = 0;
    strcpy(g_msg5a6d40, g_text);
    g_dir569498 = 1;
    g_arg5a34ec = arg;
    g_left5a7144 = strlen(g_text) / ((g_difficulty != 0) + 3) + 16;
    g_sound4c2e08 = sound;
    if (prio < 0)
        g_delay5694a4 = -prio;
    g_r56d1a8 = g_rng.range(600);
    g_r56d1ac = g_rng.range(200) + 200;
    return 1;
}
