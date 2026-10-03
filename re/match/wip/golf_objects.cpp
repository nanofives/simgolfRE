// golf_clean.exe 0x00407000 (release). Separate file so tileDistance (golf_util.cpp) stays a call.
// WIP 77.6%: identical except the loop pointer anchor (original esi = record + 4 = &ty, VC6 here picks
// record + 8 = &level). Tried: unsized extern, pointer loop, do/while. Not in the 100% suite (re/match/wip/).
// FLAGS golf_clean.exe: /O2
int tileDistance(int x, int y, int tx, int ty);  // 0x0040c4b0

// 256 placed objects at 0x58bcb8 (stride 0x10). Type-4 objects with level < 16 whose tile is within
// (level * 5 + 40) * 5 / 3 of (x, y) set bit (level & 3); the mask is stored at 0x541318 and the function
// returns whether every bit of `want` is present. (What type 4 is: not established here.)
struct PlacedObject {
    short type;                          // +0x00
    short tx, ty;                        // +0x02, +0x04
    short pad;
    int   level;                         // +0x08
    int   pad2;
};
extern PlacedObject g_objects[256];      // 0x58bcb8
extern unsigned int g_nearMask;          // 0x541318

// MATCH: golf_clean.exe 0x00407000 ?nearObjects@@YAHHHI@Z
int nearObjects(int x, int y, unsigned int want)
{
    unsigned int mask = 0;
    for (int i = 0; i < 256; i++) {
        if (g_objects[i].type == 4 && g_objects[i].level < 16) {
            int d = tileDistance(x, y, g_objects[i].tx, g_objects[i].ty);
            if (d < (g_objects[i].level * 5 + 40) * 5 / 3) {
                switch (g_objects[i].level & 3) {
                case 0: mask |= 1; break;
                case 1: mask |= 2; break;
                case 2: mask |= 4; break;
                case 3: mask |= 8; break;
                }
            }
        }
    }
    g_nearMask = mask;
    return (mask & want) == want;
}
