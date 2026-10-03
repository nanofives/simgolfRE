// Small golf_clean.exe helpers (release build).
// FLAGS golf_clean.exe: /O2
#include <math.h>
#include <stdlib.h>

// Linear congruential generator (seed * 0x41c64e6d + 0x3039, 15 bits from bit 16, * 1/32768 at 0x4ba800).
class Random {
public:
    unsigned int seed;
    double next();
    int range(unsigned short n);
};

// MATCH: golf_clean.exe 0x0045c1a0 ?next@Random@@QAENXZ
double Random::next()
{
    seed = seed * 0x41c64e6d + 0x3039;
    return ((seed >> 16) & 0x7fff) * (1.0 / 32768.0);
}

// MATCH: golf_clean.exe 0x0045c1e0 ?range@Random@@QAEHG@Z
int Random::range(unsigned short n)
{
    return (int)(next() * n);
}

// MATCH: golf_clean.exe 0x00467130 ?clamp@@YAHHHH@Z
int clamp(int v, int lo, int hi)
{
    if (v < lo)
        v = lo;
    if (v > hi && hi >= lo)
        v = hi;
    return v;
}

// MATCH: golf_clean.exe 0x00467150 ?sign@@YAHH@Z
int sign(int v)
{
    if (v > 0)
        return 1;
    return (v >= 0) - 1;
}

// Distance of (dx, dy) in the game's fixed-point units; components beyond 0x4000 are divided by 8 first
// and the result scaled back (avoids int overflow in dx*dx + dy*dy).
// MATCH: golf_clean.exe 0x0040acd0 ?distance@@YAHHH@Z
int distance(int dx, int dy)
{
    int scale = 1;
    if (abs(dx) > 0x4000) {
        scale = 8;
        dx /= 8;
    }
    if (abs(dy) > 0x4000) {
        dy /= 8;
        scale *= 8;
    }
    return (int)(sqrt((double)(dx * dx + dy * dy)) * scale);
}

// Bit 7 of byte +1 of the golfer type record (stride 0x230 at 0x4d60a8), golfer type = short at
// golfer record +0xb6 (0x57956e, stride 0x100): returns 1 when the bit is clear.
extern unsigned char g_golferTypes[][0x230];   // 0x4d60a8
extern short g_golferType[][0x80];             // 0x57956e (stride 0x100 bytes)

// MATCH: golf_clean.exe 0x0046c940 ?typeBit7Clear@@YAIH@Z
unsigned int typeBit7Clear(int golfer)
{
    return ~(unsigned int)g_golferTypes[g_golferType[golfer][0]][1] >> 7 & 1;
}

// Distance from a point (fixed point, 0x400 per tile) to the centre of tile (tx, ty), * 25 / 1024.
// MATCH: golf_clean.exe 0x0040c4b0 ?tileDistance@@YAHHHHH@Z
int tileDistance(int x, int y, int tx, int ty)
{
    return distance(x - tx * 0x400 - 0x200, y - ty * 0x400 - 0x200) * 25 / 1024;
}
