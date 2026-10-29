#include "jgldefs.h"
#include "crt.h"
#include "GameFlags.h"
#include "Golfer.h"
#include "Geometry.h"
#include "Plot.h"
#include "AssetSurface.h"
#include "FloatingText.h"
#include "ShotTrail.h"
#include "MainLoop.h"
#include "PathFinding.h"
#include "BuildingSearch.h"
#include "CourseGrid.h"
#include "CourseScript.h"
#include "TerrainGrid.h"
#include "Employee.h"
#include "EmployeeRender.h"

char g_empty[4] = {0};

char *g_jobTitles[10] = {"Club Pro", "Celebrity", "Ranger", "Marshall", "Groundskeeper", "Technician", "Soda Vendor",
    "Refresher", "Joe Cool", "JC"};
char *g_staffFirstNames[10] = {"Chuck ", "ChiChi ", "Randy ", "Mike ", "Joe ", "Tommy ", "Sally ", "Rita", g_empty,
    "JC"};

void ClearDivotsAtPosition(int tileX, int tileY)
{
    for (int i = 0; i < 256; ++i) {
        if (g_tornDivots[i].x != -1 && (g_tornDivots[i].originX >> 10) == tileX
            && (g_tornDivots[i].originY >> 10) == tileY) {
            g_tornDivots[i].x = -1;
        }
    }
}

void ResetStaffSprite()
{
    g_lastStaffSprite = &AssetSurface::s_nullSprite;
}

int SpawnEmployee(int job)
{
    int i = 0;
    int r;

    while (g_staffSlots[i].flags != 0)
        i++;
    memset(&g_staffSlots[i], 0, sizeof(Employee));
    r = g_rng.NextInt(5);
    g_staffSlots[i].posX = g_buildingTable[0].row * 1024 + 0x600;
    g_staffSlots[i].posY = g_buildingTable[0].col * 1024 + 0x600;
    g_staffSlots[i].flags = 1;
    g_staffSlots[i].dir = (g_allHolesArray[1].teeDir + r - 2) & 7;
    g_staffSlots[i].job = job;
    g_staffSlots[i].anim = 0xb;
    g_staffSlots[i].waitTimer = g_rng.NextInt(0x20);
    g_staffSlots[i].postRow = -1;
    return i;
}

void UpdateAllEmployees(void)
{
    int i, destX, destY, target, kind, bestArg, bestTy, bestDist, j, k, sx, sy;
    int dx, dy;

    for (i = 0; i < 0x40; i++) {

        if (g_staffSlots[i].job >= 0 && g_watchedPlayerIdx == -1)
            g_staffSlots[i].flags = 0;
        if (g_staffSlots[i].flags == 0)
            continue;

        if (g_staffSlots[i].talkTimer != 0 && (g_clock & 3) == 0) {
            g_staffSlots[i].talkTimer = g_staffSlots[i].talkTimer - 1;
            if (g_staffSlots[i].talkTimer == 4 && g_staffSlots[i].talkKind == TALK_GREETING) {
                if (!(g_staffSlots[i].flags & STAFF_UPGRADED)
                    && (g_golferPool[g_staffSlots[i].talkGolfer].thirst > GOLFER_NEED_LEVEL
                        || g_golferPool[g_staffSlots[i].talkGolfer].hunger > GOLFER_NEED_LEVEL
                        || g_golferPool[g_staffSlots[i].talkGolfer].fatigue > GOLFER_TIRED_LEVEL))
                    SetGolferAction(g_staffSlots[i].talkGolfer, 0x3a, GetGolferMoodIndex(g_staffSlots[i].talkGolfer));
                else
                    SetGolferAction(g_staffSlots[i].talkGolfer, 0x22, GetGolferMoodIndex(g_staffSlots[i].talkGolfer));
            }
            if (g_staffSlots[i].talkTimer == 4 && g_staffSlots[i].talkKind == TALK_DRINK) {
                if (g_staffSlots[i].flags & STAFF_UPGRADED)
                    g_golferPool[g_staffSlots[i].talkGolfer].thirst = 0x63;
                SetGolferAction(g_staffSlots[i].talkGolfer, 0x19, 0x14);
                g_golferPool[g_staffSlots[i].talkGolfer].thirst = 0;
                g_gameCashHundreds += 2;
                AddMoneyPopup(2, g_staffSlots[i].posX, g_staffSlots[i].posY, -1);
                g_annualStatsRecords[g_careerPlayYear % 100].v[2] += 2;
            }
        }

        if (g_staffSlots[i].waitTimer != 0) {
            g_staffSlots[i].waitTimer = g_staffSlots[i].waitTimer - 1;
            continue;
        }

        if (g_staffSlots[i].job > 0) {
            destX = g_golferPool[g_watchedPlayerIdx].ballX;
            destY = g_golferPool[g_watchedPlayerIdx].ballY;
            if (destX == 0) {
                destX = g_golferPool[g_watchedPlayerIdx].x;
                destY = g_golferPool[g_watchedPlayerIdx].y;
            } else {
                destX += g_dirRowStep[i & 7] << 10;
                destY += g_dirColStep[i & 7] << 10;
            }
            dx = destX - g_staffSlots[i].posX;
            dy = destY - g_staffSlots[i].posY;
            if (ApproxDistance(dx, dy) < 0x800 || g_golferPool[g_watchedPlayerIdx].waitTimer > 0
                || (g_golferPool[g_watchedPlayerIdx].holeStrokes == 0
                    && g_golferPool[g_watchedPlayerIdx].holeIdx == 1)) {
                g_staffSlots[i].dir = ((((CalculateAngle(dx, dy) >> 0x1c) & 0xf) + 1) >> 1) & 7;
                g_staffSlots[i].anim = 0xb;
                g_staffSlots[i].waitTimer = g_rng.NextInt(0x10) + 1;
                g_staffSlots[i].stepsLeft = 0;
                continue;
            }
        } else {

            if (g_staffSlots[i].job == JOB_PLAYER_AVATAR) {
                if (g_watchedPlayerIdx != -1)
                    continue;
                if ((g_runModeFlags & GAME_TOURNAMENT) && g_golferPool[1].holeIdx != 0)
                    continue;
            }

            if (g_staffSlots[i].stepsLeft == 0 && !(g_staffSlots[i].flags & STAFF_QUITTING)) {

                bestDist = 0xfffff;
                target = -1;
                destX = g_staffSlots[i].posX;
                destY = g_staffSlots[i].posY;
                if (g_staffSlots[i].postRow != -1) {
                    destX = (g_staffSlots[i].postRow << 10) + 0x200;
                    destY = (g_staffSlots[i].postCol << 10) + 0x200;
                }
                kind = -1;
                bestArg = -1;

                if (g_staffSlots[i].job == -4 || g_staffSlots[i].job == JOB_PLAYER_AVATAR) {
                    int lim = 0x18;
                    if (g_staffSlots[i].flags & STAFF_UPGRADED)
                        lim = 0x20;
                    int ox, oy;
                    sx = g_staffSlots[i].posX >> 10;
                    sy = g_staffSlots[i].posY >> 10;
                    for (ox = -0x10; ox <= 0x10; ox++) {
                        for (oy = -0x10; oy <= 0x10; oy++) {
                            int tx = g_staffSlots[i].postRow + ox;
                            int ty = g_staffSlots[i].postCol + oy;
                            int d;
                            if (IsPositionInvalid(tx, ty))
                                continue;
                            if (!(g_tileExtra[tx * 50 + ty] & TILE_WEEDS))
                                continue;
                            d = ApproxDistance(tx * 2 - g_staffSlots[i].postRow - sx,
                                ty * 2 - g_staffSlots[i].postCol - sy);
                            if (d >= lim)
                                continue;
                            lim = d;
                            destX = (tx << 10) + 0x200;
                            bestArg = tx;
                            destY = (ty << 10) + 0x200;
                            bestTy = ty;
                            target = 1;
                            kind = -4;
                        }
                    }
                    if (target == -1 && (g_staffSlots[i].flags & STAFF_UPGRADED)) {
                        int lim2 = 0x1400;
                        int h;
                        for (h = 0; h < 0x100; h++) {
                            int d;
                            if (g_tornDivots[h].x == -1)
                                continue;
                            d = ApproxDistance(g_tornDivots[h].originX * 2 - 0x200 - (g_staffSlots[i].postRow << 10)
                                                   - g_staffSlots[i].posX,
                                g_tornDivots[h].originY * 2 - 0x200 - (g_staffSlots[i].postCol << 10)
                                    - g_staffSlots[i].posX);
                            if (d >= lim2)
                                continue;
                            lim2 = d;
                            destX = g_tornDivots[h].originX;
                            destY = g_tornDivots[h].originY;
                            bestArg = h;
                            target = 2;
                            kind = -4;
                        }
                    }
                }

                if ((g_staffSlots[i].job != -4 && g_staffSlots[i].job != JOB_PLAYER_AVATAR)
                    || (bestArg == -1 && g_staffSlots[i].job == JOB_PLAYER_AVATAR)) {
                    destX = (g_staffSlots[i].postRow << 10) + 0x200;
                    j = 0;
                    destY = (g_staffSlots[i].postCol << 10) + 0x200;
                    for (j = 0; j < 0x98; j++) {
                        Golfer *g = &g_golferPool[j];
                        int sc, lk;
                        lk = -1;
                        sc = CalculateDistanceInYards(g->x, g->y, g_staffSlots[i].postRow, g_staffSlots[i].postCol);
                        if (g->holeIdx <= 0)
                            continue;
                        if (g->activity < ACTIVITY_IDLE)
                            continue;
                        if (j == g_staffSlots[i].talkGolfer)
                            continue;
                        if (!GetTileAtPosition(g->x, g->y))
                            continue;
                        if (GetTileAtPosition(g->x, g->y) == TERRAIN_BUILDING)
                            continue;
                        if (g_staffSlots[i].job == -3 || g_staffSlots[i].job == JOB_PLAYER_AVATAR) {
                            if ((g->flags & GOLFER_WALKED_OFF) && (g_staffSlots[i].flags & STAFF_UPGRADED)) {
                                sc = sc / 8;
                                lk = -3;
                            } else if (!(g->flags & 0x4000) && g->holeStrokes <= 1) {
                                lk = -3;
                            }
                        }
                        if (g_staffSlots[i].job == -2 || g_staffSlots[i].job == JOB_PLAYER_AVATAR) {
                            if (g->slotKind[0] != 0x22 && g->slotKind[0] != 0x3a)
                                lk = -2;
                        }
                        if (g_staffSlots[i].job == -5 || g_staffSlots[i].job == JOB_PLAYER_AVATAR) {
                            if (g->thirst > ((g_staffSlots[i].flags & STAFF_UPGRADED) ? 0 : 4))
                                lk = -5;
                        }
                        if (lk == g_staffSlots[i].job || g_staffSlots[i].job == JOB_PLAYER_AVATAR) {
                            if (sc < bestDist) {
                                bestDist = sc;
                                target = j;
                                kind = lk;
                            }
                        }
                    }
                    if (bestDist < 0x64) {
                        destX = g_golferPool[target].x;
                        destY = g_golferPool[target].y;
                    } else {
                        if (target != -1)
                            g_staffSlots[i].dir = GetDirection8(g_golferPool[target].x - g_staffSlots[i].posX,
                                g_golferPool[target].y - g_staffSlots[i].posY);
                        target = -1;
                    }
                }

                if (g_staffSlots[i].job == JOB_PLAYER_AVATAR && g_allHolesArray[1].par == 0) {
                    if (!(g_clock & 0x200)
                        && g_gameTileTerrainKind[g_pointerPosX * 50 + g_pointerPosY] != TERRAIN_WATER) {
                        destX = (g_pointerPosX << 10) + 0x200;
                        destY = (g_pointerPosY << 10) + 0x200;
                    } else {
                        destX = g_staffSlots[i].posX;
                        destY = g_staffSlots[i].posY;
                    }
                }

                dx = destX - g_staffSlots[i].posX;
                dy = destY - g_staffSlots[i].posY;
                if (ApproxDistance(dx, dy) <= 0x400) {
                    int animBase;
                    g_staffSlots[i].anim = 0xb;
                    g_staffSlots[i].waitTimer = g_rng.NextInt(0x10) + 1;
                    g_staffSlots[i].stepsLeft = ApproxDistance(dx, dy) <= 0x800 ? 0 : 0x200;
                    animBase = 0;
                    if (g_staffSlots[i].flags & STAFF_UPGRADED)
                        animBase = 4;
                    if (g_staffSlots[i].job == JOB_PLAYER_AVATAR && kind != -2)
                        animBase = 8;
                    if (target != -1) {
                        g_staffSlots[i].dir = ((((CalculateAngle(dx, dy) >> 0x1c) & 0xf) + 1) >> 1) & 7;
                        g_staffSlots[i].waitTimer = g_staffSlots[i].waitTimer + 0x40;
                        g_staffSlots[i].talkTimer = TALK_SHOW_TICKS;
                        g_staffSlots[i].talkGolfer = target;
                        switch (kind) {
                        case -2:
                            g_staffSlots[i].talkKind = TALK_GREETING;
                            g_staffSlots[i].waitTimer = g_staffSlots[i].waitTimer + 0x40;
                            PlayPositionedSound(animBase + 0x5c, g_staffSlots[i].posX, g_staffSlots[i].posY, 0);
                            g_golferPool[target].waitTimer = g_golferPool[target].waitTimer - 0x10;
                            g_golferPool[target].dir = g_staffSlots[i].dir ^ 4;
                            g_staffSlots[i].anim = 0xc;
                            g_staffSlots[i].animTick = 0;
                            g_golferPool[target].activity = ACTIVITY_WALK;
                            g_staffSlots[i].tally = g_staffSlots[i].tally + 1;
                            break;
                        case -3:
                            if ((g_golferPool[target].flags & GOLFER_WALKED_OFF)
                                && (g_staffSlots[i].flags & STAFF_UPGRADED)) {
                                g_golferPool[target].enjoyment = -11;
                                g_staffSlots[i].talkKind = TALK_SEND_HOME;
                                g_golferPool[target].waitTimer = 0;
                            } else {
                                g_staffSlots[i].talkKind = TALK_HURRY_UP;
                                g_golferPool[target].flags |= 0x4000;
                            }
                            g_staffSlots[i].anim = 0xc;
                            g_staffSlots[i].animTick = 0;
                            PlayPositionedSound(animBase + 0x5d, g_staffSlots[i].posX, g_staffSlots[i].posY, 0x1f4);
                            g_staffSlots[i].tally = g_staffSlots[i].tally + 1;
                            break;
                        case -5:
                            g_staffSlots[i].talkKind = TALK_DRINK;
                            g_staffSlots[i].anim = 0xc;
                            g_staffSlots[i].animTick = 0;
                            PlayPositionedSound(animBase + 0x5f, g_staffSlots[i].posX, g_staffSlots[i].posY, 0);
                            g_staffSlots[i].waitTimer = g_staffSlots[i].waitTimer - 0x20;
                            g_golferPool[target].waitTimer = -0x20;
                            g_golferPool[target].activity = ACTIVITY_WALK;
                            g_golferPool[target].dir = g_staffSlots[i].dir ^ 4;
                            g_staffSlots[i].tally = g_staffSlots[i].tally + 1;
                            break;
                        case -4:
                            if (target == 2) {
                                g_staffSlots[i].talkKind = TALK_DIVOTS;
                                g_staffSlots[i].anim = 0xc;
                                g_staffSlots[i].waitTimer = 0xc;
                                g_tornDivots[bestArg].x = -1;
                                break;
                            } else {
                                int tx = bestArg;
                                int ty;
                                int idx;
                                short tf;
                                g_staffSlots[i].talkKind = TALK_WEEDS;
                                g_staffSlots[i].anim = 0xc;
                                ty = bestTy;
                                g_staffSlots[i].waitTimer =
                                    (unsigned short)(char)(~g_staffSlots[i].flags & STAFF_UPGRADED) >> 3;
                                idx = tx * 50 + ty;
                                tf = g_tileExtra[idx];
                                if (!(tf & TILE_ANIMATING)) {
                                    tf |= TILE_ANIMATING;
                                    g_tileExtra[idx] = tf;
                                    g_tileAnimFrame[idx] = g_flicBankFrameLens[BANK_FLOWERS] - 3;
                                    PlayPositionedSound(animBase + 0x5e, g_staffSlots[i].posX, g_staffSlots[i].posY, 0);
                                    ClearDivotsAtPosition(tx, ty);
                                    break;
                                } else {
                                    if (--g_tileAnimFrame[idx] > 1)
                                        break;
                                    tf &= 0xb7ff;
                                    g_staffSlots[i].waitTimer = 8;
                                    g_tileExtra[idx] = tf;
                                    g_staffSlots[i].tally = g_staffSlots[i].tally + 1;
                                }
                            }
                        }
                        if (g_staffSlots[i].waitTimer > 1 && (g_staffSlots[i].flags & STAFF_UPGRADED))
                            g_staffSlots[i].waitTimer = g_staffSlots[i].waitTimer / 2;
                        continue;
                    } else {
                        g_staffSlots[i].waitTimer = g_rng.NextInt(8);
                        continue;
                    }
                }
            }
        }

        if (g_staffSlots[i].flags & STAFF_QUITTING) {
            destX = (g_allHolesArray[CLUBHOUSE_HOLE].teeX << 10) + 0x200;
            destY = (g_allHolesArray[CLUBHOUSE_HOLE].teeY << 10) + 0x200;
            if ((g_clock + 35 * i) % 100 == 0) {
                g_staffSlots[i].talkTimer = TALK_SHOW_TICKS;
                g_staffSlots[i].talkKind = TALK_COMPLAINT;
            }
            if (ApproxDistance(destX - g_staffSlots[i].posX, destY - g_staffSlots[i].posY) < 0x600)
                g_staffSlots[i].flags = 0;
        }
        if (g_staffSlots[i].stepsLeft == 0) {
            int r;
            g_runModeFlags |= 0x100;
            r = FindPathDirection(destX, destY, g_staffSlots[i].posX, g_staffSlots[i].posY, i);
            g_staffSlots[i].dir = r;
            g_runModeFlags &= ~0x100u;
            if (g_staffSlots[i].dir != -1) {
                int cx = (g_staffSlots[i].posX >> 6) & 0xf;
                int cy = (g_staffSlots[i].posY >> 6) & 0xf;
                if (g_dirRowStep[g_staffSlots[i].dir] != 0)
                    g_staffSlots[i].stepsLeft = g_dirRowStep[g_staffSlots[i].dir] == 1 ? 0x18 - cx : cx + 8;
                else
                    g_staffSlots[i].stepsLeft = g_dirColStep[g_staffSlots[i].dir] == 1 ? 0x18 - cy : cy + 8;
                if ((g_staffSlots[i].dir & 1) && g_staffSlots[i].stepsLeft > 0x10)
                    g_staffSlots[i].stepsLeft = 0x10;
            } else {
                g_staffSlots[i].dir = ((((CalculateAngle(dx, dy) >> 0x1c) & 0xf) + 1) >> 1) & 7;
                g_staffSlots[i].anim = 0xb;
                g_staffSlots[i].waitTimer = g_rng.NextInt(0x10) + 1;
            }
        }
        if (g_staffSlots[i].stepsLeft == 0)
            continue;

        for (k = 0; k < i; k++) {
            int ddx, ddy;
            if (g_staffSlots[k].flags == 0)
                continue;
            if (k == i)
                continue;
            ddx = g_staffSlots[k].posX - g_staffSlots[i].posX;
            ddy = g_staffSlots[k].posY - g_staffSlots[i].posY;
            if (ApproxDistance(ddx, ddy) < 0x155) {
                int towardOther = GetDirection8(ddx, ddy);
                if (g_staffSlots[i].dir == towardOther || g_staffSlots[i].dir == ((towardOther + 1) & 7)
                    || g_staffSlots[i].dir == ((towardOther - 1) & 7)) {
                    g_staffSlots[i].waitTimer = g_rng.NextInt(4) + 4;
                    g_staffSlots[i].anim = 0xb;
                }
            }
            if (g_staffSlots[i].waitTimer != 0)
                g_staffSlots[i].dir = ((((CalculateAngle(ddx, ddy) >> 0x1c) & 0xf) + 1) >> 1) & 7;
        }

        {
            int speed;
            sx = g_staffSlots[i].posX >> 10;
            sy = g_staffSlots[i].posY >> 10;
            speed = Clamp(6 - g_squareTypeTable[g_gameTileTerrainKind[sx * 50 + sy]].walk, 2, 5);
            if (g_tileExtra[sx * 50 + sy] & TILE_PATH)
                speed = 6;
            if (g_staffSlots[i].flags & STAFF_UPGRADED)
                speed += 2;
            if (g_staffSlots[i].job == JOB_PLAYER_AVATAR)
                speed += 2;
            if (g_staffSlots[i].flags & STAFF_QUITTING)
                speed = 3;
            g_staffSlots[i].posX +=
                (g_dirRowStep[g_staffSlots[i].dir] * (speed << 6) / 2) / ((g_staffSlots[i].dir & 1) ? 4 : 3);
            g_staffSlots[i].posY +=
                (g_dirColStep[g_staffSlots[i].dir] * (speed << 6) / 2) / ((g_staffSlots[i].dir & 1) ? 4 : 3);
            g_staffSlots[i].stepsLeft = g_staffSlots[i].stepsLeft - 1;
            if (g_staffSlots[i].stepsLeft != 0 && g_rng.NextInt(4) == 0)
                g_staffSlots[i].stepsLeft = g_staffSlots[i].stepsLeft - 1;
            if (g_staffSlots[i].anim >= 7) {
                g_staffSlots[i].anim = g_staffSlots[i].anim + 1;
                if (g_staffSlots[i].anim <= 0xa)
                    continue;
            }
            g_staffSlots[i].anim = 7;
        }
    }
}
