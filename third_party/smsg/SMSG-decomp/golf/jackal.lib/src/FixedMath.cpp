#include "jgldefs.h"
#include <math.h>

#include "FixedMath.h"

int sine_table[SIN_TABLE_STEPS + SIN_TABLE_GUARD] = {0};

void trig_init()
{
    for (int i = 0; i < SIN_TABLE_STEPS; i++)
        sine_table[i] = (int)(sin(i * 0.006159985596078431) * 65535.0);
    sin_table(0x40000000, 100);
    cos_table((int)0xa02d82d8, 100);
}

#define SIN_LERP(turn, idx) \
    (((sine_table[(idx) + 1] - sine_table[idx]) * ((turn) & 0x3fffff)) >> 22)

int sin_table(int angle, int amplitude)
{
    if (angle & 0x80000000)
        amplitude = -amplitude;
    int mirrored = angle & 0x40000000;
    int turn = angle & 0x3fffffff;
    int idx = turn >> 22;
    if (amplitude < 0xffff) {
        if (mirrored)
            return ((SIN_LERP(turn, idx) - sine_table[idx] + 0xffff) * amplitude) >> 16;
        return ((SIN_LERP(turn, idx) + sine_table[idx]) * amplitude) >> 16;
    }
    if (amplitude < 0xffffff) {
        amplitude >>= 8;
        if (mirrored)
            return ((SIN_LERP(turn, idx) - sine_table[idx] + 0xffff) * amplitude) >> 8;
        return ((SIN_LERP(turn, idx) + sine_table[idx]) * amplitude) >> 8;
    }
    amplitude >>= 16;
    if (mirrored)
        return (SIN_LERP(turn, idx) - sine_table[idx] + 0xffff) * amplitude;
    return (SIN_LERP(turn, idx) + sine_table[idx]) * amplitude;
}

int cos_table(int angle, int amplitude)
{
    return sin_table(angle + 0x3fffffff, amplitude);
}

int FixedFromDouble(double value)
{
    return (int)floor(value * 65536.0 + 0.5);
}
