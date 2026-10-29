#include "jgldefs.h"
#include <windows.h>
#include <mmsystem.h>

#include "Geometry.h"
#include "Golfer.h"
#include "Employee.h"
#include "RandomSource.h"
#include "crt.h"
#include "FixedMath.h"
#include "GolferPairing.h"

char g_noiseGrid[20][19] = {0};
signed char g_bilerpGrid[20][19] = {0};

void InitTimer()
{
    g_startTime = timeGetTime();
    g_rng.m_seed = g_startTime * 37;
}

int Clamp(int v, int lo, int hi)
{
    if (v < lo)
        v = lo;
    if (v > hi && hi >= lo)
        v = hi;
    return v;
}

int Sign(int v)
{
    if (v > 0)
        return 1;
    if (v < 0)
        return -1;
    return 0;
}

int ApproxDistance(int dx, int dy)
{
    int d;
    if (dx < 0)
        dx = -dx;
    if (dy < 0)
        dy = -dy;
    if (dx > dy)
        d = dx * 2 + dy;
    else
        d = dy * 2 + dx;
    return d / 2;
}

int GetDirection8(int dx, int dy)
{
    int dir;
    if (dx > 0) {
        if (dy > 0) {
            dir = 3;
            if (abs(dx) > 2 * abs(dy))
                dir = 2;
            if (abs(dy) > 2 * abs(dx))
                dir = 4;
        } else {
            dir = 1;
            if (abs(dx) > 2 * abs(dy))
                dir = 2;
            if (abs(dy) > 2 * abs(dx))
                dir = 0;
        }
    } else {
        if (dy > 0) {
            dir = 5;
            if (abs(dx) > 2 * abs(dy))
                dir = 6;
            if (abs(dy) > 2 * abs(dx))
                dir = 4;
        } else {
            dir = 7;
            if (abs(dx) > 2 * abs(dy))
                dir = 6;
            if (abs(dy) > 2 * abs(dx))
                dir = 0;
        }
    }
    return dir;
}

int FixedPointSin(int angle, int amp)
{
    if (angle & 0x80000000) {
        amp = -amp;
        angle &= 0x7fffffff;
    }
    if (angle & 0x40000000)
        angle = 0x7fffffff - angle;
    return sin_table(angle, amp);
}

int FixedPointCos(int angle, int amp)
{
    return FixedPointSin(angle + 0x40000000, amp);
}

int CalculateAngle(int dx, int dy)
{
    int num, den, big;
    dy = -dy;
    if (dx == 0)
        return dy > 0 ? 0 : 0x80000000;
    if (dy == 0)
        return dx > 0 ? 0x40000000 : 0xc0000000;
    if (abs(dx) > abs(dy)) {
        num = abs(dy) << 14;
        den = abs(dx);
        big = 1;
    } else {
        num = abs(dx) << 14;
        den = abs(dy);
        big = 0;
    }
    int r = num / den;
    int ang = ((0x2800 - ((abs(0x1333 - r) * 0xB00) >> 14)) * r) >> 14;
    int res;
    if (dx > 0) {
        if (dy > 0)
            res = big ? (0x4000 - ang) : ang;
        else
            res = big ? (ang + 0x4000) : (0x8000 - ang);
    } else {
        if (dy > 0)
            res = big ? (ang + 0xc000) : -ang;
        else
            res = big ? (0xc000 - ang) : (ang + 0x8000);
    }
    return res << 16;
}

void GenerateRandomTerrainNoise()
{
    int i, j, k;
    int sum;
    char v;

    for (i = 0; i < 18; i++) {
        for (j = 0; j < 18; j++)
            g_noiseGrid[i][j] = (char)g_rng.NextInt(16);
        g_noiseGrid[i][18] = g_noiseGrid[i][0];
    }
    for (j = 0; j < 18; j++)
        g_noiseGrid[18][j] = g_noiseGrid[0][j];

    for (i = 0; i <= 16; i++) {
        for (j = 0; j <= 16; j++) {
            v = g_noiseGrid[i][j];
            sum = v * 4;
            for (k = 0; k < 8; k++)
                sum += g_noiseGrid[i + g_dirRowStep[k] + 1][j + g_dirColStep[k] + 1];
            g_bilerpGrid[i][j] = v;
        }
    }
    for (i = 0; i <= 16; i++)
        g_bilerpGrid[i][16] = g_bilerpGrid[i][0];
    for (j = 0; j <= 16; j++)
        g_bilerpGrid[16][j] = g_bilerpGrid[0][j];
}

int BilinearSample(int x, int y)
{
    x -= 0x80;
    y -= 0x80;
    int ax = (x >> 8) & 0xf;
    int ay = (y >> 8) & 0xf;
    int fx = (x >> 3) & 0x1f;
    int fy = (y >> 3) & 0x1f;
    int s = g_bilerpGrid[ax][ay + 1] * (0x20 - fx) * fy + g_bilerpGrid[ax + 1][ay] * (0x20 - fy) * fx
            + g_bilerpGrid[ax][ay] * (0x20 - fx) * (0x20 - fy) + g_bilerpGrid[ax + 1][ay + 1] * fy * fx;
    return s / 32;
}

void GetPetNameSuffix(int golfer)
{
    switch (golfer & 3) {
    case 0: strcat(g_printBuffer, ", honey"); break;
    case 1: strcat(g_printBuffer, ", sugar"); break;
    case 2: strcat(g_printBuffer, ", dear"); break;
    case 3: strcat(g_printBuffer, ", sweetie"); break;
    }
}

int GetGolferMoodIndex(int golfer)
{
    unsigned short f = g_golferPool[golfer].slotParams[0];
    if (f & MOOD_ANNOYED)
        return 2;
    return !(f & MOOD_PLEASED);
}

void BuildEmployeeName(int employee)
{
    char *p = g_staffSlots[employee].customName;
    if (*p == 0) {
        signed char job = g_staffSlots[employee].job;
        unsigned bit = g_staffSlots[employee].flags & STAFF_UPGRADED;
        int i = (int)(bit >> 3) - (2 * job + 4);
        if (job != JOB_PLAYER_AVATAR) {
            strcat(g_printBuffer, g_staffFirstNames[i]);
            strcat(g_printBuffer, g_jobTitles[i]);
            return;
        }
        p = g_golferTypeArray[0].name;
    }
    strcat(g_printBuffer, p);
}

#pragma intrinsic(strcat)
void AppendGolferName(int golfer, int withComma)
{
    if (withComma)
        strcat(g_printBuffer, ", ");
    int t = g_golferPool[golfer].persona;
    int lo = t & ~PERSONA_KIND;
    switch (t & PERSONA_KIND) {
    case PERSONA_COMMISSIONER:
        strcat(g_printBuffer, g_commissionerName);
        switch (g_landPurchaseNum & 0x7f) {
        case 0: break;
        case 1: strcat(g_printBuffer, " II"); break;
        case 2: strcat(g_printBuffer, " III"); break;
        case 3: strcat(g_printBuffer, " IV"); break;
        case 4: strcat(g_printBuffer, " V"); break;
        case 5: strcat(g_printBuffer, " VI"); break;
        case 6: strcat(g_printBuffer, " VII"); break;
        case 7: strcat(g_printBuffer, " VIII"); break;
        case 8: strcat(g_printBuffer, " IX"); break;
        default: strcat(g_printBuffer, " X"); break;
        }
        break;
    case PERSONA_ROSTER:
        if (lo != PERSONA_OWN_PRO)
            strcat(g_printBuffer, g_golfProList[g_golferPool[golfer].rosterIndex].name);
        else
            strcat(g_printBuffer, g_golferTypeArray[0].name);
        break;
    case PERSONA_CEO:
        strcat(g_printBuffer, g_ceoName);
        switch (lo) {
        case 0: break;
        case 1: strcat(g_printBuffer, " II"); break;
        case 2: strcat(g_printBuffer, " III"); break;
        case 3: strcat(g_printBuffer, " IV"); break;
        case 4: strcat(g_printBuffer, " V"); break;
        case 5: strcat(g_printBuffer, " VI"); break;
        case 6: strcat(g_printBuffer, " VII"); break;
        case 7: strcat(g_printBuffer, " VIII"); break;
        case 8: strcat(g_printBuffer, " IX"); break;
        default: strcat(g_printBuffer, " X"); break;
        }
        break;
    case PERSONA_HEIRESS: strcat(g_printBuffer, (golfer & 1) ? "Ivana Richman" : "Agnes Heffledorp"); break;
    case 0x100: strcat(g_printBuffer, g_celebrityNames[lo]); break;
    default: strcat(g_printBuffer, g_golferTypeArray[g_golferPool[golfer].typeIdx].name); break;
    }
}

int g_openingDayStoryId;
int g_storySpare = 0;
