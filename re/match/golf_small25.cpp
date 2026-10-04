// golf_clean.exe functions, batch 25 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <string.h>

extern char g_text[];                    // 0x51a068
// Replaces the first occurrence of `key` in the text buffer with `repl` (tail kept in a 512-byte copy).
// MATCH: golf_clean.exe 0x0045b7c0 ?replaceInText@@YAXPBD0@Z
void replaceInText(const char* key, const char* repl)
{
    char tail[512];
    char* p = strstr(g_text, key);
    if (p) {
        strcpy(tail, p);
        *p = 0;
        strcat(g_text, repl);
        strcat(g_text, tail + strlen(key));
    }
}

int distance(int dx, int dy);            // 0x40acd0
struct Placed { short type, tx, ty; char b6; unsigned char flags; char pad[8]; };
extern Placed g_placed[256];             // 0x58bcb8, stride 0x10
struct ObjDef { signed char size; char pad[0x13]; };
extern ObjDef g_objDefs[];               // 0x4c26c0, stride 0x14
extern int g_nearestDist;                // 0x568d0c
// Index of the placed object of `type` nearest to world point (x, y) (distance to its centre, size/2 tiles in);
// for types 6 and up the scan stops at the first one without flag 0x40. Leaves the distance in g_nearestDist.
// MATCH: golf_clean.exe 0x0040ddb0 ?nearestPlaced@@YAHHHH@Z
int nearestPlaced(int type, int x, int y)
{
    int best = -1;
    g_nearestDist = 0xffff;
    for (int i = 0; i < 256; i++) {
        if (g_placed[i].type == type) {
            if (type >= 6 && !(g_placed[i].flags & 0x40))
                return best;
            int d = distance((g_placed[i].tx + g_objDefs[type].size / 2) * 0x400 - x + 0x200,
                             (g_placed[i].ty + g_objDefs[type].size / 2) * 0x400 - y + 0x200);
            if (d < g_nearestDist) {
                best = i;
                g_nearestDist = d;
            }
        }
    }
    return best;
}

class Random {
public:
    unsigned short range(unsigned short n);   // 0x45c1e0
};
extern Random g_rng;                     // 0x822d9c
struct Walker { int x, y; char pad8[8]; unsigned char b10; char pad11; char active; char kind; char pad14[2];
                char dir; char pad17[3]; short m_1a; char pad1c[2]; short m_1e; char pad20[0x4c - 0x20]; };
extern Walker g_walkers[];               // 0x585850, stride 0x4c (active byte +0x12 == 0: free)
extern char g_dir575cb9;                 // 0x575cb9
// Spawns a walker of `kind` in the first free slot at the centre of placed object 0's tile, facing
// (g_dir575cb9 + rand(5) - 2) & 7; returns the slot.
// MATCH: golf_clean.exe 0x00402970 ?spawnWalker@@YAHD@Z
int spawnWalker(char kind)
{
    int i = 0;
    while (g_walkers[i].active)
        i++;
    memset(&g_walkers[i], 0, sizeof(Walker));
    int r = g_rng.range(5);
    g_walkers[i].x = g_placed[0].tx * 0x400 + 0x600;
    g_walkers[i].y = g_placed[0].ty * 0x400 + 0x600;
    g_walkers[i].active = 1;
    g_walkers[i].dir = (g_dir575cb9 + r - 2) & 7;
    g_walkers[i].kind = kind;
    g_walkers[i].m_1e = 0xb;
    g_walkers[i].m_1a = g_rng.range(0x20);
    g_walkers[i].b10 = 0xff;
    return i;
}
