#include "GolferSpawn.h"
#include "ShotDifficulty.h"
#include "ShotPower.h"

int SpawnGolfer(int requireEligible)
{
    int slot, i, j, best, count, r, tries, ok, kind, hi, fl, fl2;

    slot = g_curNextSpawnPos % 152;
    g_curNextSpawnPos++;
    memset(&g_golferPool[slot], 0, sizeof(Golfer));
    g_golferPool[slot].x = (g_allHolesArray[CLUBHOUSE_HOLE].teeX << 10) + 0x600;
    g_golferPool[slot].y = (g_allHolesArray[CLUBHOUSE_HOLE].teeY << 10) + 0x600;
    g_golferPool[slot].dir = g_allHolesArray[1].teeDir;
    g_golferPool[slot].ageFlags = g_rng.NextInt(0x7fff);

    i = 0;
    best = 9999;
    fl = g_runModeFlags & GAME_SECOND_TEES;
    count = requireEligible;
    for (; i < 8; i++) {
        if (fl || (i >= 3 && i != 4)) {
            int v = g_talentKindCount[i + (slot & 3) * 8];
            if (v < best) {
                best = v;
                count = 1;
            } else if (v == best) {
                count++;
            }
        }
    }

    r = g_rng.NextInt(count);
    i = 0;
    fl2 = g_runModeFlags & GAME_SECOND_TEES;
    for (; i < 8; i++) {
        if (fl2 || (i >= 3 && i != 4)) {
            if (g_talentKindCount[i + (slot & 3) * 8] == best) {
                if (--r < 0) {
                    g_golferPool[slot].talents = (unsigned char)i;
                    r = 99;
                }
            }
        }
    }

    tries = 0;
    g_talentKindCount[g_golferPool[slot].talents + g_golferPool[slot].talentLevel * 8]++;

    do {
        g_golferPool[slot].typeIdx = g_rng.NextInt(0x4b) + 1;
        if (IsGolferMale(slot) == 0) {
            kind = 6;
        } else {
            kind = 1;
            if (g_golferTypeArray[g_golferPool[slot].typeIdx].caps & 0x11)
                kind = 3;
            if (g_golferTypeArray[g_golferPool[slot].typeIdx].caps & 0x0c)
                kind |= 4;
            if (kind == 1)
                kind = ((slot & 1) ? 2 : 4) | 1;
        }
        g_golferPool[slot].talents = (unsigned char)kind;
        if (g_golferTypeArray[g_golferPool[slot].typeIdx].fixedKind != 0)
            g_golferPool[slot].talents =
                (unsigned char)(g_golferTypeArray[g_golferPool[slot].typeIdx].fixedKind & SKILLS_ALL);

        ok = 1;
        for (j = 0; j < 152; j++) {
            if (j != slot) {
                if (g_golferPool[j].holeIdx == -1) {
                    int m = g_golferPool[slot].typeIdx % 19;
                    if (g_golferPool[j].typeIdx % 19 == m)
                        ok = 0;
                }
                if (g_golferPool[j].holeIdx == -1) {
                    if (g_golferPool[j].typeIdx == g_golferPool[slot].typeIdx)
                        ok = 0;
                }
                if (g_golferPool[j].holeIdx > 0) {
                    if (g_golferPool[j].typeIdx == g_golferPool[slot].typeIdx)
                        ok = 0;
                }
            }
        }

        if (g_golferHistoryPerCharacter[g_golferPool[slot].typeIdx].finishedStoryId == -1)
            ok = 0;

        if ((requireEligible == 0 || g_golferHistoryPerCharacter[g_golferPool[slot].typeIdx].flags != 0) && ok)
            break;
        tries++;
    } while (tries < 999);

    if (tries >= 999) {
        strcpy(g_printBuffer, "Your membership is declining.");
        g_curNextSpawnPos = (g_curNextSpawnPos - 1) % 152;
        return -1;
    }

    hi = g_rng.NextInt(0x80);
    g_golferPool[slot].ageFlags = (hi << 8) + g_golferTypeArray[g_golferPool[slot].typeIdx].ageBits;
    g_golferPool[slot].talentLevel = (signed char)(g_golferPool[slot].typeIdx % 3);
    g_golferPool[slot].holeIdx = -1;
    g_golferPool[slot].activity = ACTIVITY_WALK;
    g_golferPool[slot].arrivedAt = g_clock;
    g_golferPool[slot].enjoyment = g_rng.NextInt(3) + 3;
    if (g_level == 0)
        g_golferPool[slot].enjoyment = 4;
    if (g_simOpenFacilityLvl[SCENERY_SWIM_CLUB] != 0)
        g_golferPool[slot].enjoyment = (short)(g_simOpenFacilityLvl[SCENERY_SWIM_CLUB] + 4);
    if (g_simOpenFacilityLvl[SCENERY_CART_GARAGE] != 0)
        g_golferPool[slot].flags |= 0x10000;
    return slot;
}


int CalculateShotDifficulty(int sx, int sy, int tx, int ty, int maxPower, int curve, int force)
{
    int wx, wy, ang, oct, dist, cnt, i, dx, dy;
    int off, x, y, subX, subY, t, k;
    int difficulty, tile;

    wx = tx << 10;
    wy = ty << 10;
    dx = wx - sx + 0x200;
    dy = wy - sy + 0x200;
    g_squareTypeTable[1].lieGrade = 0;
    ang = CalculateAngle(dx, dy);
    oct = (((ang >> 28) + 1) >> 1) & 7;
    dist = (Math_Distance2D(dx, dy) + 0x200) >> 10;
    int power = Clamp(dist, 0, maxPower / 25 + 1);
    difficulty = 0;
    cnt = 0;
    for (i = 0; i <= dist; i++) {
        off = 0;
        if (curve != 0) {
            if (i <= power / 2)
                off = curve * 0xffffffc;
            else if (i < power)
                off = ((power - i) * curve * 0x15555554) / ((power + 1) / 2);
        }
        x = sx + FixedPointSin(ang + off, i << 10);
        y = sy - FixedPointCos(ang + off, i << 10);
        if (curve != 0 && i > power / 2 && i < power) {
            off = curve * 0x15555554;
            x = wx - FixedPointSin(ang - off, (power - i) << 10) + 0x200;
            y = FixedPointCos(ang - off, (power - i) << 10) + wy + 0x200;
        }
        subX = (x % 1024) >> 4;
        subY = (y % 1024) >> 4;
        x >>= 10;
        y >>= 10;
        tile = x * 50 + y;
        t = g_gameTileTerrainKind[tile];
        k = (power > 5) ? ((power >= 9) + 2) : 1;
        if ((i >= power - k && force == 0) || g_squareTypeTable[t].category == CATEGORY_TREES) {
            if (g_squareTypeTable[t].lieGrade > 0)
                difficulty += g_squareTypeTable[t].lieGrade;
            if (g_squareTypeTable[t].category == CATEGORY_TREES)
                difficulty += 32 / (i + 1);
            if (t == TERRAIN_WATER || IsPositionInvalid(x, y))
                difficulty += 0x10;
            if (subX < 15 && g_dirColStep[oct] != 0)
                difficulty += g_squareTypeTable[g_gameTileTerrainKind[tile - 50]].lieGrade / 2;
            if (subX > 48 && g_dirColStep[oct] != 0)
                difficulty += g_squareTypeTable[g_gameTileTerrainKind[tile + 50]].lieGrade / 2;
            if (subY < 15 && g_dirRowStep[oct] != 0)
                difficulty += g_squareTypeTable[g_gameTileTerrainKind[tile - 1]].lieGrade / 2;
            if (subY > 48 && g_dirRowStep[oct] != 0)
                difficulty += g_squareTypeTable[g_gameTileTerrainKind[tile + 1]].lieGrade / 2;
        }
        x = sx + FixedPointSin(ang, (i << 10) + 0x200);
        y = sy - FixedPointCos(ang, (i << 10) + 0x200);
        x >>= 10;
        y >>= 10;
        if (g_squareTypeTable[g_gameTileTerrainKind[x * 50 + y]].category == CATEGORY_TREES)
            cnt++;
    }
    if (cnt >= 2)
        difficulty++;
    if (difficulty < 0)
        difficulty = 0;
    g_squareTypeTable[1].lieGrade = -1;
    return difficulty;
}

int g_shotCacheNext = 0;

int CalculateBallRollDistance(int speed)
{
    int total = 0;
    do {
        total += speed / 8;
        speed -= speed >> g_squareTypeTable[1].rollShift;
    } while (speed >= 64);
    return total;
}

int CalculateBallCarryDistance(int speed, int height)
{
    int total = 0;
    int heightSum = 0;
    do {
        total += speed / 8;
        heightSum += height / 16;
        height -= 128;
        speed -= speed >> 4;
    } while (heightSum > 0);
    return total;
}

int CalculateShotPower(int distance, int height, int mode)
{
    int i, speed, step, target, reach;
    for (i = 0; i < 10; i++)
        if (distance == g_gameMemoDists[i] && height == g_shotMemoHgt[i])
            return g_shotPowerMemoSpeed[i];
    g_gameMemoDists[g_shotCacheNext] = distance;
    g_shotMemoHgt[g_shotCacheNext] = height;
    speed = (distance * 20) / 25;
    speed = speed * 33 - (speed * speed) / 48 + 64;
    step = speed / 2;
    target = (distance * 1024) / 25;
    do {
        if (mode == 0)
            reach = CalculateBallCarryDistance(speed, height);
        else
            reach = CalculateBallRollDistance(speed);
        if (reach > target)
            speed -= step;
        if (reach < target)
            speed += step;
        step /= 2;
    } while (step > 2);
    g_shotPowerMemoSpeed[g_shotCacheNext] = speed;
    g_shotCacheNext = (g_shotCacheNext + 1) % 10;
    return speed;
}

int CalculateGolferMaxDistance(int golfer)
{
    int d = (g_golferPool[golfer].talents & TALENT_LENGTH) ? 200 : 150;
    if (g_level >= 1)
        d += g_golferPool[golfer].talentLevel * 50 / 3;
    else
        d += (g_golferPool[golfer].talents & TALENT_LENGTH) ? 40 : 25;
    int t = GetTileAtPosition(g_golferPool[golfer].ballX, g_golferPool[golfer].ballY);
    if (golfer >= 152)
        t = g_golferPool[golfer].holeStrokes ? 2 : 0;
    if (g_golferPool[golfer].persona) {
        if (g_golferPool[golfer].skillMask & 1)
            d += g_golferPool[golfer].ratings[0] * 4 - 20;
        if ((g_golferPool[golfer].skillMask & 2) && t == 0)
            d += (g_golferPool[golfer].ratings[1] - 5) * 6;
    }
    if (g_golferPool[golfer].attitude > 0)
        d += Clamp(g_golferPool[golfer].attitude, 0, 3) * d / 24;
    if (g_golferPool[golfer].talents & TALENT_LENGTH)
        d += g_simOpenFacilityLvl[SCENERY_DRIVING_RANGE] * 15;
    if (g_squareTypeTable[t].lieGrade > 0)
        d -= Clamp(g_squareTypeTable[t].lieGrade, 0, 3) * d / 8;
    if (t)
        d += d / -5;
    return d > 330 ? 330 : d;
}
