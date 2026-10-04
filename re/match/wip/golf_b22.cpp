// golf_clean.exe 0x004671a0 / 0x00409bf0 (release). WIP, not in the 100% suite.
// octant 84%: identical code except esi/edi swapped between dx and the result (tried: copy of dx, long result,
//   initialised result; the condition order below is the one that reaches 84%).
// moveFlyers 84.5%: same code, the original stores x/y before loading the next field (load/store scheduling).
//   Tried: pointer loop (72%), index loop, z <= 0 and date-left comparison (the two that raised it to 84.5%).
// FLAGS golf_clean.exe: /O2
#include <stdlib.h>

// Octant 0..7 of the vector (dx, dy) (8-way facing): an axis wins when the other component is under half of it.
// MATCH: golf_clean.exe 0x004671a0 ?octant@@YAHHH@Z
int octant(int dx, int dy)
{
    int r;
    if (dx > 0) {
        if (dy > 0) {
            r = 3;
            if (abs(dx) > abs(dy) * 2)
                r = 2;
            if (abs(dy) > abs(dx) * 2)
                return 4;
        } else {
            r = 1;
            if (abs(dx) > abs(dy) * 2)
                r = 2;
            if (abs(dy) > abs(dx) * 2)
                return 0;
        }
    } else if (dy > 0) {
        r = 5;
        if (abs(dx) > abs(dy) * 2)
            r = 6;
        if (abs(dy) > abs(dx) * 2)
            return 4;
    } else {
        r = 7;
        if (abs(dx) > abs(dy) * 2)
            r = 6;
        if (abs(dy) > abs(dx) * 2)
            return 0;
    }
    return r;
}

int f_467270(int a, int b);
int f_4672b0(int a, int b);
extern int g_date;
// 256 records at 0x5736b0 (same table as Rec24 in golf_small5.cpp); +8 == -1: free slot.
struct Flyer { int ox, oy, x, y, z, dir, speed, vz, born; };
extern Flyer g_flyers[256];
// MATCH: golf_clean.exe 0x00409bf0 ?moveFlyers@@YAXXZ
void moveFlyers()
{
    for (int i = 0; i < 256; i++) {
        Flyer* f = &g_flyers[i];
        if (f->x != -1) {
            f->x += f_467270(f->dir, f->speed / 16);
            f->y -= f_4672b0(f->dir, f->speed / 16);
            f->z += f->vz / 32;
            if (f->z <= 0) {
                f->vz = 0;
                f->speed = 0;
                f->z = 0;
                if (g_date > f->born + 0x400)
                    f->x = -1;
            }
            f->speed -= f->speed >> 4;
            if (f->z || f->vz)
                f->vz -= 0x40;
        }
    }
}
