#include "TileValue.h"
#include <math.h>
#include "crt.h"
#include "GolferShot.h"
#include "TerrainGrid.h"
#include "FloatingText.h"
#include "CourseGrid.h"
#include "Plot.h"
#include "ShotPower.h"
#include "Geometry.h"
#include "RandomSource.h"
#include "Screen.h"
#include "PathFinding.h"
#include "BuildingSearch.h"
#include "SaveGame.h"
#include "ShotTrail.h"
#include "GolferStats.h"
#include "SceneryNames.h"
#include "IsoView.h"
#include "ShotDifficulty.h"
#include "TileObstacle.h"
#include "ScreenText.h"
#include "MainLoop.h"
#include "VipEvents.h"
#include "HoleCompletion.h"
#include "GolferUpdate.h"
#include "NumberText.h"
#include "HoleStatistics.h"
#include "InputPoll.h"
#include "jgldefs.h"

int g_hazardCommentary;
int g_shotTargetExact;

void SimulateBallPhysics(int gi, int tx, int ty, int spin)
{
    int trail;
    int steps;
    int type;
    int subX, subY;
    int atEdge;
    int tileX, tileY;
    int dx, dy;
    int octant;
    int groundH, roll, bounce, nearCentre, newSpeed, hit, climb;
    int i;

    trail = -1;
    steps = 0;
    for (i = 0; i < 16; i++) {
        if (g_ballPathSlots[i].golferId == gi)
            trail = i;
    }

    g_golferPool[155] = g_golferPool[gi];
    PlanGolferShot(gi, 0, tx, ty, spin);

    g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve / 2;

    while (g_golferPool[gi].ballSpeed != 0) {
        tileY = g_golferPool[gi].ballY >> 10;
        tileX = g_golferPool[gi].ballX >> 10;

        if (IsPositionInvalid(tileX, tileY))
            type = TERRAIN_OUT_OF_BOUNDS;
        else
            type = g_gameTileTerrainKind[tileX * 50 + tileY];

        groundH = GetTerrainHeightAtWorld(g_golferPool[gi].ballX, g_golferPool[gi].ballY);
        dx = FixedPointSin(g_golferPool[gi].ballBearing, g_golferPool[gi].ballSpeed / 16);
        dy = FixedPointCos(g_golferPool[gi].ballBearing, g_golferPool[gi].ballSpeed / 16);
        g_golferPool[gi].ballX = g_golferPool[gi].ballX + dx;
        g_golferPool[gi].ballY = g_golferPool[gi].ballY - dy;

        g_golferPool[gi].ballZ = g_golferPool[gi].ballZ + g_golferPool[gi].ballClimb / 32;
        if (g_golferPool[gi].ballZ != 0 || g_golferPool[gi].ballClimb != 0) {
            g_golferPool[gi].ballClimb = g_golferPool[gi].ballClimb - 0x40;
            if (trail != -1 && g_golferPool[gi].ballClimb > 0 && g_golferPool[gi].ballClimb < 0x40)
                AddGolferTrailPoint(trail);
        }

        steps = steps + 1;
        if (trail != -1 && (steps & 7) == 0)
            AddGolferTrailPoint(trail);

        subX = (g_golferPool[gi].ballX >> 6) & 0xf;
        subY = (g_golferPool[gi].ballY >> 6) & 0xf;
        atEdge = 0;
        if (subX <= 1) {
            if (g_gameTileTerrainKind[(tileX - 1) * 50 + tileY] != type)
                atEdge = 1;
        }
        if (subY <= 1) {
            if (g_gameTileTerrainKind[tileX * 50 + tileY - 1] != type)
                atEdge = 1;
        }
        if (subX >= 0xe) {
            if (g_gameTileTerrainKind[(tileX + 1) * 50 + tileY] != type)
                atEdge = 1;
        }
        if (subY >= 0xe) {
            if (g_gameTileTerrainKind[tileX * 50 + tileY + 1] != type)
                atEdge = 1;
        }

        octant = (((g_golferPool[gi].ballBearing >> 0x1c) + 1) >> 1) & 7;

        if (g_golferPool[gi].ballZ > 1) {
            groundH = groundH - GetTerrainHeightAtWorld(g_golferPool[gi].ballX, g_golferPool[gi].ballY);
            g_golferPool[gi].ballZ = g_golferPool[gi].ballZ + groundH;
            g_golferPool[gi].ballSpeed = g_golferPool[gi].ballSpeed - (g_golferPool[gi].ballSpeed >> 5);
            g_golferPool[gi].ballBearing = g_golferPool[gi].ballBearing + g_golferPool[gi].ballCurve;

            hit = BallHitsObstacle(type, g_golferPool[gi].ballZ, tileX, tileY);
            if (g_shotPlanMode != SHOT_PLAN_ROUTE && hit != 0) {
                int dist = Math_Distance2D(g_golferPool[gi].ballX - (tileX << 10) - 0x200,
                    g_golferPool[gi].ballY - (tileY << 10) - 0x200);
                if (g_golferPool[gi].persona != 0 && (g_golferPool[gi].skillMask & 0x200) != 0)
                    dist = dist + g_golferPool[gi].ratings[9] * dist / 4;

                if (dist < g_rng.NextInt(0x300)) {
                    g_golferPool[gi].ballBearing = g_golferPool[gi].ballBearing + ((0x40 + g_rng.NextInt(0x80)) << 24);
                    g_golferPool[gi].ballSpeed = g_golferPool[gi].ballSpeed - g_rng.NextInt(g_golferPool[gi].ballSpeed);
                    g_golferPool[gi].flags = g_golferPool[gi].flags | 2;
                }
            }
        } else {
            if (g_simGolferFlags & 4)
                roll = Clamp(g_squareTypeTable[type].rollShift
                                 - CalculateDirectionalOffset(g_golferPool[gi].ballX, g_golferPool[gi].ballY, octant),
                    0, 0x63);
            else
                roll = g_squareTypeTable[type].rollShift;

            int side = CalculateDirectionalOffset(g_golferPool[gi].ballX, g_golferPool[gi].ballY, (octant + 2) & 7);

            if (roll < 2 && atEdge != 0)
                roll = 2;
            else if (roll > 4 && steps > 0x80)
                roll = 4;

            if (g_simGolferFlags & 4)
                g_golferPool[gi].ballBearing = g_golferPool[gi].ballBearing - (side << 25);

            if (roll < 5)
                g_golferPool[gi].ballSpeed = g_golferPool[gi].ballSpeed - (g_golferPool[gi].ballSpeed >> roll) / 2;
            else if (roll > 4)
                g_golferPool[gi].ballSpeed = g_golferPool[gi].ballSpeed + (0x20 - (g_golferPool[gi].ballSpeed >> 6));

            if (type == TERRAIN_WATER) {
                if (atEdge == 0)
                    g_golferPool[gi].ballSpeed = g_golferPool[gi].ballSpeed / 2;
            } else if (type == TERRAIN_RAVINE) {
                if (abs(8 - subX) <= 2 && abs(8 - subY) <= 2)
                    g_golferPool[gi].ballSpeed = g_golferPool[gi].ballSpeed / 2;
            }

            if (tileX != (g_golferPool[gi].ballX >> 10)) {
                if (g_tileUphillMap[tileX][tileY] & ((dx > 0) ? 4 : 0x40))
                    g_golferPool[gi].ballBearing = -g_golferPool[gi].ballBearing;
            }
            if (tileY != (g_golferPool[gi].ballY >> 10)) {
                if (g_tileUphillMap[tileX][tileY] & ((dy > 0) ? 1 : 0x10))
                    g_golferPool[gi].ballBearing = 0x80000000 - g_golferPool[gi].ballBearing;
            }
        }

        if (g_golferPool[gi].ballZ <= 0 && g_golferPool[gi].ballClimb < 0) {
            climb = g_golferPool[gi].ballClimb;
            nearCentre = 0;
            if (subX >= 5 && subX < 0xb && subY >= 5)
                nearCentre = 1;

            bounce = g_squareTypeTable[type].bounce;
            if (g_shotPlanMode == 0) {
                if (bounce < 2 && atEdge != 0)
                    bounce = 2;
                if (nearCentre != 0 && (g_tileExtra[tileX * 50 + tileY] & TILE_PATH) != 0)
                    bounce = 4;
            }

            g_golferPool[gi].ballClimb = Clamp(-0x40 - (bounce * climb) / 12, 0, 0x270f);
            if (g_golferPool[gi].ballClimb < 0x80)
                g_golferPool[gi].ballClimb = 0;
            g_golferPool[gi].ballZ = 0;
            if (trail != -1)
                AddGolferTrailPoint(trail);

            if (g_golferPool[gi].flags & 0x100) {
                g_golferPool[gi].ballBearing = g_golferPool[gi].ballBearing ^ 0x80000000;
                g_golferPool[gi].flags = g_golferPool[gi].flags & ~0x100;
                if (g_golferPool[gi].persona == 0) {
                    int range = g_golferPool[gi].ballSpeed * 2;
                    newSpeed = g_rng.NextInt(range);
                } else {
                    int range = g_golferPool[gi].ballSpeed * 2;
                    newSpeed =
                        g_golferPool[gi].ballSpeed
                        + (g_rng.NextInt(range) - g_golferPool[gi].ballSpeed) * 3 / (g_golferPool[gi].ratings[9] + 5);
                }
                g_golferPool[gi].ballSpeed = newSpeed;
            }

            if (g_golferPool[gi].flags & 0x80) {
                g_golferPool[gi].ballSpeed = g_golferPool[gi].ballSpeed / 2;
                g_golferPool[gi].flags = (g_golferPool[gi].flags & ~0x80) | 0x100;
            }

            if (g_simGolferFlags & 4) {
                int turn = g_golferPool[gi].ballClimb * 0xaec33;
                g_golferPool[gi].ballBearing =
                    g_golferPool[gi].ballBearing
                    - CalculateDirectionalOffset(g_golferPool[gi].ballX, g_golferPool[gi].ballY,
                          ((((g_golferPool[gi].ballBearing >> 0x1d) + 1) & ~1) + 2) & 7)
                          * turn;

                g_golferPool[gi].ballSpeed =
                    g_golferPool[gi].ballSpeed
                    + -(Clamp(CalculateDirectionalOffset(g_golferPool[gi].ballX, g_golferPool[gi].ballY, octant), -2, 2)
                          * g_golferPool[gi].ballClimb)
                          * 2;

                g_golferPool[gi].ballClimb =
                    g_golferPool[gi].ballClimb
                    + Clamp(CalculateDirectionalOffset(g_golferPool[gi].ballX, g_golferPool[gi].ballY, octant), -2, 2)
                          * g_golferPool[gi].ballClimb / 2;
            }

            if (type == TERRAIN_WATER) {
                if (atEdge == 0) {
                    if (g_shotPlanMode != 0 || nearCentre == 0 || (g_tileExtra[tileX * 50 + tileY] & TILE_PATH) == 0) {
                        g_golferPool[gi].ballClimb = 0;
                        g_golferPool[gi].ballSpeed = 0;
                    }
                }
            } else if (type == TERRAIN_ROCKS) {
                if (g_golferPool[gi].ballSpeed > 0x100 && atEdge == 0)
                    g_golferPool[gi].ballBearing = g_golferPool[gi].ballBearing + ((0x50 - g_rng.NextInt(0xa0)) << 24);
            }
        }

        if (g_golferPool[gi].ballSpeed < 0x40 && g_golferPool[gi].ballZ == 0 && g_golferPool[gi].ballClimb == 0) {
            g_simBallStopPosX = g_golferPool[gi].ballX;
            g_simBallStopPosY = g_golferPool[gi].ballY;
            g_golferPool[gi].ballSpeed = 0;
        }
    }

    g_golferPool[gi] = g_golferPool[155];
}

#define OPT_PLANNING_SHOT     0x800000
#define TILE_SHIFT            10
#define HALF_TILE             0x200
#define YARDS_PER_TILE        25
#define SEARCH_RADIUS         10
#define SEARCH_WIDTH          21
#define SEARCH_CELLS          441
#define SHOT_VARIANTS         6
#define COST_PRUNED           99999
#define COST_REJECTED         100000
#define LIE_ROUGH             1
#define SHOT_RANDOM_THIRD     1
#define SHAPE_MINUS_OK        1
#define SHAPE_PLUS_OK         2
#define MIN_SHAPED_YARDS      75
#define HEADING_RAN_ON        1
#define DOUBT_LENGTH          1
#define DOUBT_LINE            2
#define DOUBT_RAN_ON          4
#define TURN_30_DEGREES       0x15555555
#define SIMULATION_BUDGET     250
#define PIN_X                 g_allHolesArray[g_golferPool[gi].holeIdx].aimX
#define PIN_Y                 g_allHolesArray[g_golferPool[gi].holeIdx].aimY

#define BEST_SHOT_UNRECOVERED(c, a, b) ((c) ? (a) : (b))
int CalculateBestShot(int gi)
{
    int shotCost[SEARCH_CELLS][SHOT_VARIANTS];

    memset(shotCost, 0, sizeof(shotCost));
    g_runModeFlags |= OPT_PLANNING_SHOT;
    g_shotSearchLocked = 1;
    g_simGolferFlags = g_golferPool[gi].talents;
    int hole = g_golferPool[gi].holeIdx;
    g_hazardCommentary = 1;
    g_shotTargetExact = 0;
    int bestX = -1, bestY;
    int bestShape = 0;
    int bestRanOn;
    int maxDist = CalculateGolferMaxDistance(gi);

    int oldTargetX = g_golferPool[gi].targetX;
    int oldTargetY = g_golferPool[gi].targetY;
    int ballTileX = g_golferPool[gi].ballX >> TILE_SHIFT;
    int ballTileY = g_golferPool[gi].ballY >> TILE_SHIFT;
    int ballTerrain = g_gameTileTerrainKind[ballTileX * GRID_SIZE + ballTileY];
    if (g_shotPlanMode == 0 && !(g_golferPool[gi].flags & GOLFER_AFTER_PENALTY)) {
        if (g_squareTypeTable[ballTerrain].lieGrade <= 0 || g_golferPool[gi].persona == 0)
            g_shotPlanMode = SHOT_RANDOM_THIRD;
    }
    int allowedShapes =
        (g_squareTypeTable[ballTerrain].lieGrade <= 0 && (g_golferPool[gi].talents & TALENT_IMAGINATION))
            ? SHAPE_MINUS_OK | SHAPE_PLUS_OK
            : 0;
    if (g_golferPool[gi].persona != 0) {
        if (!(g_golferPool[gi].skillMask & 0x20))
            allowedShapes &= ~SHAPE_MINUS_OK;
        if (!(g_golferPool[gi].skillMask & 0x40))
            allowedShapes &= ~SHAPE_PLUS_OK;
    }
    g_golferPool[gi].targetX = g_allHolesArray[hole].aimX;
    g_golferPool[gi].targetY = g_allHolesArray[hole].aimY;

    int dx = (g_golferPool[gi].targetX << TILE_SHIFT) - g_golferPool[gi].ballX + HALF_TILE;
    int dy = (g_golferPool[gi].targetY << TILE_SHIFT) - g_golferPool[gi].ballY + HALF_TILE;
    int pinYardsX = (dx * YARDS_PER_TILE) >> TILE_SHIFT;
    int pinYardsY = (dy * YARDS_PER_TILE) >> TILE_SHIFT;
    int pinYards = (int)sqrt((double)(pinYardsY * pinYardsY + pinYardsX * pinYardsX));
    g_golferPool[gi].ballBearing = CalculateAngle(pinYardsX, pinYardsY);
    int pinBeyondTwoShots = 0;
    int bestShotUnrecovered = 0;
    g_golferPool[gi].holeStrokes++;
    if (pinYards > CalculateGolferMaxDistance(gi) + maxDist)
        pinBeyondTwoShots = 1;
    g_golferPool[gi].holeStrokes--;
    int layupYards = Clamp(maxDist - YARDS_PER_TILE, 0, pinYards);
    int centreX =
        (g_golferPool[gi].ballX
            + FixedPointSin(g_golferPool[gi].ballBearing, ((layupYards << TILE_SHIFT) + HALF_TILE) / YARDS_PER_TILE))
        >> TILE_SHIFT;
    int centreY =
        (g_golferPool[gi].ballY
            - FixedPointCos(g_golferPool[gi].ballBearing, ((layupYards << TILE_SHIFT) + HALF_TILE) / YARDS_PER_TILE))
        >> TILE_SHIFT;
    CalculateDistanceInYards(g_golferPool[gi].ballX, g_golferPool[gi].ballY, centreX, centreY);
    Math_Distance2D(centreX - PIN_X, centreY - PIN_Y);
    int pinDistance = CalculateDistanceInYards(ballTileX, ballTileY, PIN_X, PIN_Y);
    int distanceDivisor = 4;
    if (pinDistance < 100)
        distanceDivisor = 2;
    if (pinDistance > 200)
        distanceDivisor = 6;
    int simsFlown = 0;
    int simsPerShot = 2;
    int wholePath;
    int survivors;

    do {
        int shotReach[SEARCH_CELLS][SHOT_VARIANTS];
        int shotHeading[SEARCH_CELLS][SHOT_VARIANTS];
        memset(shotReach, 0, sizeof(shotReach));
        int bestCost = COST_PRUNED;

        for (int offsetX = -SEARCH_RADIUS; offsetX <= SEARCH_RADIUS; offsetX++) {
            for (int offsetY = -SEARCH_RADIUS; offsetY <= SEARCH_RADIUS; offsetY++) {
                int col = offsetY + SEARCH_RADIUS;
                int row = (offsetX + SEARCH_RADIUS) * SEARCH_WIDTH;
                int tileCost = shotCost[row + col][1];
                if (tileCost > COST_PRUNED)
                    continue;
                int targetX = centreX + offsetX;
                int targetY = centreY + offsetY + (bestShotUnrecovered & (centreY - SEARCH_RADIUS + offsetY));
                int shotDX = (targetX << TILE_SHIFT) - g_golferPool[gi].ballX + HALF_TILE;
                int shotDY = (targetY << TILE_SHIFT) - g_golferPool[gi].ballY + HALF_TILE;
                int shotYardsX = (shotDX * YARDS_PER_TILE) >> TILE_SHIFT;
                int shotYardsY = (shotDY * YARDS_PER_TILE) >> TILE_SHIFT;
                int shotYards = (int)sqrt((double)(shotYardsY * shotYardsY + shotYardsX * shotYardsX));
                int heading = CalculateAngle(shotYardsX, shotYardsY);
                int nearCleanLie = 1;
                if (tileCost == 0) {
                    shotCost[row + col][0] = shotCost[row + col][1] = shotCost[row + col][2] = COST_REJECTED;
                    shotCost[row + col][3] = shotCost[row + col][4] = shotCost[row + col][5] = COST_REJECTED;
                    if ((g_golferPool[gi].flags & GOLFER_AFTER_PENALTY) && targetX == oldTargetX
                        && targetY == oldTargetY)
                        continue;
                    if (IsPositionInvalid(targetX, targetY))
                        continue;
                    if (shotYards > maxDist + 16)
                        continue;
                    if (shotYards < maxDist / 3 && shotYards < pinYards / 3
                        && !(g_golferPool[gi].flags & GOLFER_AFTER_PENALTY))
                        continue;
                    if (targetX == ballTileX && targetY == ballTileY)
                        continue;
                    if (g_gameTileTerrainKind[targetX * GRID_SIZE + targetY] == 0)
                        continue;
                    if (g_squareTypeTable[g_gameTileTerrainKind[targetX * GRID_SIZE + targetY]].lieGrade > LIE_ROUGH)
                        continue;
                    if (g_squareTypeTable[g_gameTileTerrainKind[targetX * GRID_SIZE + targetY]].lieGrade == LIE_ROUGH) {
                        nearCleanLie = 0;
                        for (int dir = 0; dir < 8; dir += 2) {
                            if (g_squareTypeTable[g_gameTileTerrainKind[(g_dirRowStep[dir] + targetX) * GRID_SIZE
                                                                        + g_dirColStep[dir] + targetY]]
                                    .lieGrade
                                <= 0)
                                nearCleanLie = 1;
                        }
                    }
                    int targetToPin = Math_Distance2D(targetX - PIN_X, targetY - PIN_Y);
                    int ballToPin = Math_Distance2D(ballTileX - PIN_X, ballTileY - PIN_Y);
                    if (g_gameTileTerrainKind[targetX * GRID_SIZE + targetY] == TERRAIN_GREEN && targetToPin > 3)
                        continue;
                    int minGain = nearCleanLie ? 1 : ballToPin / 2;
                    if (minGain + targetToPin > ballToPin)
                        continue;
                    shotCost[row + col][0] = shotCost[row + col][1] = shotCost[row + col][2] = 0;
                    shotCost[row + col][3] = shotCost[row + col][4] = shotCost[row + col][5] = 0;
                }
                for (int aimSpot = 0; aimSpot < 2; aimSpot++) {
                    if (aimSpot != 0
                        && g_squareTypeTable[g_gameTileTerrainKind[ballTileX * GRID_SIZE + ballTileY]].lieGrade > 0)
                        continue;
                    for (int shape = -1; shape <= 1; shape++) {
                        int variant = aimSpot * 3 + 1 + shape;
                        if (shotCost[row + col][variant] >= COST_PRUNED)
                            continue;
                        if (shape == -1) {
                            if (!(allowedShapes & SHAPE_MINUS_OK)) {
                                shotCost[row + col][variant] = COST_REJECTED;
                                continue;
                            }
                        } else if (shape == 1) {
                            if (!(allowedShapes & SHAPE_PLUS_OK)) {
                                shotCost[row + col][variant] = COST_REJECTED;
                                continue;
                            }
                        }
                        if (shape != 0 && shotYards <= MIN_SHAPED_YARDS) {
                            shotCost[row + col][variant] = COST_REJECTED;
                            continue;
                        }

                        int score = 0;
                        int cleanLandings = 0;
                        for (int sim = 0; sim < simsPerShot; sim++) {
                            if (aimSpot == 0) {
                                SimulateBallPhysics(gi, (targetX << TILE_SHIFT) + HALF_TILE,
                                    (targetY << TILE_SHIFT) + HALF_TILE, shape);
                                simsFlown++;
                            } else
                                SimulateBallPhysics(gi, targetX << TILE_SHIFT, targetY << TILE_SHIFT, shape);
                            if (g_golferPool[gi].talents & TALENT_IMAGINATION) {
                                for (int dir = 0; dir < 8; dir++) {
                                    int fraction = (dir & 1) ? 4 : 3;
                                    int probeX = ((g_dirRowStep[dir] << TILE_SHIFT) / fraction + g_simBallStopPosX)
                                                 >> TILE_SHIFT;
                                    int probeY = ((g_dirColStep[dir] << TILE_SHIFT) / fraction + g_simBallStopPosY)
                                                 >> TILE_SHIFT;
                                    if (IsPositionInvalid(probeX, probeY)) {
                                        score += g_squareTypeTable[TERRAIN_OUT_OF_BOUNDS].lieGrade;
                                    } else {
                                        score += g_squareTypeTable[g_gameTileTerrainKind[probeX * GRID_SIZE + probeY]]
                                                     .lieGrade;
                                        if (g_tileExtra[probeX * GRID_SIZE + probeY] & TILE_PIN)
                                            score--;
                                    }
                                }
                            }
                            int restTileX = g_simBallStopPosX >> TILE_SHIFT;
                            int restTileY = g_simBallStopPosY >> TILE_SHIFT;
                            int restingLie =
                                g_squareTypeTable[g_gameTileTerrainKind[restTileX * GRID_SIZE + restTileY]].lieGrade;
                            if (IsPositionInvalid(restTileX, restTileY)
                                || (g_tileExtra[restTileX * GRID_SIZE + restTileY] & TILE_BUILDING)) {
                                restingLie = g_squareTypeTable[TERRAIN_OUT_OF_BOUNDS].lieGrade;
                            } else if (g_tileExtra[restTileX * GRID_SIZE + restTileY] & TILE_PIN) {
                                if ((g_tileExtra[restTileX * GRID_SIZE + restTileY] & TILE_HOLE_MASK)
                                    == g_golferPool[gi].holeIdx)
                                    restingLie--;
                                else
                                    restingLie += 2;
                            }
                            if (heading & HEADING_RAN_ON)
                                heading ^= HEADING_RAN_ON;
                            if (g_squareTypeTable[g_gameTileTerrainKind[restTileX * GRID_SIZE + restTileY]].lieGrade
                                <= 0)
                                cleanLandings++;
                            if (aimSpot == 0)
                                shotReach[row + col][variant] =
                                    CalculateDistanceInYards((targetX << TILE_SHIFT) + HALF_TILE,
                                        (targetY << TILE_SHIFT) + HALF_TILE, PIN_X, PIN_Y);
                            else
                                shotReach[row + col][variant] = CalculateDistanceInYards(targetX << TILE_SHIFT,
                                    targetY << TILE_SHIFT, PIN_X, PIN_Y);
                            int restToPin =
                                CalculateDistanceInYards(g_simBallStopPosX, g_simBallStopPosY, PIN_X, PIN_Y);
                            if (shotReach[row + col][variant] - restToPin > YARDS_PER_TILE
                                && shotReach[row + col][variant] > restToPin * 2)
                                heading |= HEADING_RAN_ON;
                            shotHeading[row + col][variant] = heading;
                            score = score
                                    + CalculateDistanceInYards(g_simBallStopPosX, g_simBallStopPosY,
                                          g_allHolesArray[hole].aimX, g_allHolesArray[hole].aimY)
                                          / distanceDivisor
                                    + restingLie * 8;
                            if (simsPerShot == 4 && g_shotPlanMode != SHOT_PLAN_ROUTE
                                && (g_golferPool[gi].talents & TALENT_IMAGINATION) && pinBeyondTwoShots == 0) {
                                if (restingLie > 0) {
                                    score = score
                                            + CalculateShotDifficulty(g_simBallStopPosX, g_simBallStopPosY, PIN_X,
                                                  PIN_Y, maxDist, 0, 0)
                                                  * 2;
                                } else {
                                    int nextYards =
                                        CalculateDistanceInYards(g_simBallStopPosX, g_simBallStopPosY, PIN_X, PIN_Y);
                                    int nextRange = maxDist;
                                    if (g_golferPool[gi].holeStrokes == 0)
                                        nextRange -= nextRange / 5;
                                    if ((g_golferPool[gi].talents & TALENT_IMAGINATION)
                                        && g_squareTypeTable[g_gameTileTerrainKind[restTileX * GRID_SIZE + restTileY]]
                                                   .lieGrade
                                               <= 0
                                        && nextYards >= 50) {
                                        if ((nextRange - nextYards) * 60 / (nextRange * 3) >= 4)
                                            wholePath = 1;
                                    }
                                    int nextDifficulty = CalculateShotDifficulty(g_simBallStopPosX, g_simBallStopPosY,
                                        PIN_X, PIN_Y, nextRange, 0, wholePath);
                                    if (allowedShapes & SHAPE_MINUS_OK) {
                                        int curved = CalculateShotDifficulty(g_simBallStopPosX, g_simBallStopPosY,
                                            PIN_X, PIN_Y, nextRange, -1, 0);
                                        if (curved < nextDifficulty)
                                            nextDifficulty = curved;
                                    }
                                    if (allowedShapes & SHAPE_PLUS_OK) {
                                        int curved = CalculateShotDifficulty(g_simBallStopPosX, g_simBallStopPosY,
                                            PIN_X, PIN_Y, nextRange, 1, 0);
                                        if (curved < nextDifficulty)
                                            nextDifficulty = curved;
                                    }
                                    score += nextDifficulty / 2;
                                }
                            }
                        }
                        shotCost[row + col][variant] += score;
                        if (cleanLandings >= (g_level + 4) * simsPerShot / 8)
                            g_hazardCommentary = 0;
                        if (shotCost[row + col][variant] < bestCost) {
                            bestX = targetX;
                            bestY = targetY;
                            g_plannedEndX = g_simBallStopPosX;
                            g_plannedEndY = g_simBallStopPosY;
                            bestShape = shape;
                            g_shotTargetExact = aimSpot;
                            bestCost = shotCost[row + col][variant];
                            bestRanOn = shotHeading[row + col][variant] & HEADING_RAN_ON;
                        }
                        if (g_debugBestShotGolfer == gi) {
                            int screenX, screenY;
                            g_printBuffer[0] = 0;
                            strcat(g_printBuffer, itoa(score, g_numberBuf, 10));
                            TileToScreenCached(targetX, targetY, &screenX, &screenY);
                            g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
                            DrawTextCenteredWithShadow(g_printBuffer, screenX, screenY - shape * 8 - 4,
                                (offsetX == 0 && offsetY == 0) ? COLOR_PALE_YELLOW : COLOR_WHITE);
                        }
                    }
                }
            }
        }
        if (simsPerShot >= 8 || g_shotPlanMode == SHOT_PLAN_ROUTE)
            break;

        int pruneMargin = 128;
        for (;;) {
            int minReach = 0xffff;
            survivors = 0;
            int maxReach = -1000;
            int minTurn = 0x0fffffff;
            int maxTurn = 0xe0000000;
            int pinBearing = CalculateAngle(
                BEST_SHOT_UNRECOVERED(PIN_X << TILE_SHIFT, PIN_X << TILE_SHIFT,
                    BEST_SHOT_UNRECOVERED(g_golferPool[gi].ballY, PIN_X << TILE_SHIFT, PIN_X << TILE_SHIFT))
                    - g_golferPool[gi].ballX + HALF_TILE,
                BEST_SHOT_UNRECOVERED((PIN_Y << TILE_SHIFT) - g_golferPool[gi].ballY + HALF_TILE,
                    (PIN_Y << TILE_SHIFT) - g_golferPool[gi].ballY + HALF_TILE,
                    (PIN_Y << TILE_SHIFT) - g_golferPool[gi].ballY + HALF_TILE));
            for (int offsetX = -SEARCH_RADIUS; offsetX <= SEARCH_RADIUS; offsetX++) {
                for (int offsetY = -SEARCH_RADIUS; offsetY <= SEARCH_RADIUS; offsetY++) {
                    int deadVariants = 0;
                    int dxr = offsetX + SEARCH_RADIUS;
                    int row = dxr * SEARCH_WIDTH;
                    if (shotCost[row + offsetY + SEARCH_RADIUS][1] > COST_PRUNED)
                        continue;
                    int col = offsetY + SEARCH_RADIUS;
                    for (int variant = 0; variant < SHOT_VARIANTS; variant++) {
                        int cost = shotCost[row + col][variant];
                        if (cost >= COST_PRUNED) {
                            deadVariants++;
                            continue;
                        }
                        if (cost > pruneMargin + bestCost) {
                            shotCost[row + col][variant] = COST_PRUNED;
                            deadVariants++;
                            continue;
                        }
                        survivors++;
                        if (simsPerShot == 4 && shotReach[row + col][variant] != 0) {
                            int reach = shotReach[row + col][variant];
                            if (reach < minReach)
                                minReach = reach;
                            if (reach > maxReach)
                                maxReach = reach;
                            int targetX = centreX + dxr - SEARCH_RADIUS;
                            int targetY = centreY + offsetY;
                            if (CalculateDistanceInYards(g_golferPool[gi].ballX, g_golferPool[gi].ballY, targetX,
                                    targetY)
                                > 100) {
                                int turn = shotHeading[row + col][variant] - pinBearing;
                                if (turn < minTurn)
                                    minTurn = turn;
                                if (turn > maxTurn)
                                    maxTurn = turn;
                            }
                        }
                    }
                    if (deadVariants == SHOT_VARIANTS)
                        shotCost[row + col][1] = COST_REJECTED;
                }
            }
            do_all_tasks();
            if (survivors > 4 && pruneMargin > 16) {
                if (simsFlown + survivors * simsPerShot * 3 > SIMULATION_BUDGET) {
                    pruneMargin = BEST_SHOT_UNRECOVERED(pruneMargin / 2, pruneMargin / 2, pruneMargin / 2);
                    continue;
                }
            }
            if (simsPerShot == 4) {
                g_shotDoubts = 0;
                if (maxReach - minReach > 75) {
                    g_shotDoubts = DOUBT_LENGTH;
                } else if (maxTurn - minTurn > TURN_30_DEGREES) {
                    g_shotDoubts = DOUBT_LINE;
                }
            }
            break;
        }
        simsPerShot = simsPerShot * 2;
    } while (survivors > 1);

    if (g_debugBestShotGolfer == gi) {
        g_simScreen.RenderFullSurface(0);
        WaitForInputWithTimeout(0);
    }
    if (bestX != -1) {
        g_golferPool[gi].targetX = bestX;
        g_golferPool[gi].targetY = bestY;
        if (bestRanOn != 0)
            g_shotDoubts |= DOUBT_RAN_ON;
    } else {
        g_shotTargetExact = 0;
        g_golferPool[gi].targetX = g_allHolesArray[hole].aimX;
        g_golferPool[gi].targetY = g_allHolesArray[hole].aimY;
        bestShape = 0;
    }
    g_runModeFlags &= ~OPT_PLANNING_SHOT;
    if (g_shotPlanMode != SHOT_PLAN_ROUTE)
        g_shotPlanMode = 0;
    g_simGolferFlags = 7;
    return bestShape;
}
#undef PIN_X
#undef PIN_Y

void PlanGolferShot(int gi, int mode, int px, int py, int shape)
{
    int weight[32];
    int heading[32];
    char grid[50][50];
    int hole = g_golferPool[gi].holeIdx;
    int maxDist = CalculateGolferMaxDistance(gi);

    if (g_golferPool[gi].flags & GOLFER_AFTER_PENALTY) {
        g_squareTypeTable[TERRAIN_WATER].lieGrade = 0x20;
        g_squareTypeTable[TERRAIN_OUT_OF_BOUNDS].lieGrade = 0x20;
    }
    g_debugShotGolfer = -1;
    if (g_runModeFlags & GAME_DEBUG_OVERLAY) {
        if (g_displayFocus > 0)
            g_debugShotGolfer = g_displayFocus - 1;
    }

    int tx = g_golferPool[gi].ballX >> 10;
    int ty = g_golferPool[gi].ballY >> 10;
    int terrain = g_gameTileTerrainKind[tx * 50 + ty];

    if (mode == 0) {
        g_golferPool[gi].targetX = g_allHolesArray[hole].aimX;
        g_golferPool[gi].targetY = g_allHolesArray[hole].aimY;
    } else {
        shape = 0;
        if (abs(g_shotSelectedKind <= 1))
            shape = g_shotSelectedKind;
    }

    int dx = (g_golferPool[gi].targetX << 10) - g_golferPool[gi].ballX + 0x200;
    int dy = (g_golferPool[gi].targetY << 10) - g_golferPool[gi].ballY + 0x200;
    if (px != -1) {
        dx = px - g_golferPool[gi].ballX;
        dy = py - g_golferPool[gi].ballY;
        g_golferPool[gi].targetX = px >> 10;
        g_golferPool[gi].targetY = py >> 10;
    }

    int u = (dx * 25) >> 10;
    int v = (dy * 25) >> 10;
    int dist = (int)sqrt((double)(v * v + u * u));
    g_golferPool[gi].ballBearing = CalculateAngle(dx, dy);
    int oct = ((g_golferPool[gi].ballBearing >> 28) + 1) >> 1 & 7;
    int s;
    g_golferPool[gi].flags &= ~GOLFER_EXACT_AIM;

    if (mode == 0 && px == -1) {
        int limit;
        if ((g_golferPool[gi].flags & GOLFER_AFTER_PENALTY) || (g_golferPool[gi].talents & TALENT_IMAGINATION))
            limit = 25;
        else
            limit = 75;
        if (dist > limit && terrain != TERRAIN_GREEN) {
            int carry = Clamp(maxDist - 25, 0, dist);
            int nx =
                (g_golferPool[gi].ballX + FixedPointSin(g_golferPool[gi].ballBearing, ((carry << 10) + 0x200) / 25))
                >> 10;
            int ny =
                (g_golferPool[gi].ballY - FixedPointCos(g_golferPool[gi].ballBearing, ((carry << 10) + 0x200) / 25))
                >> 10;
            CalculateDistanceInYards(g_golferPool[gi].ballX, g_golferPool[gi].ballY, nx, ny);
            int curHole = g_golferPool[gi].holeIdx;
            Math_Distance2D(nx - g_allHolesArray[curHole].aimX, ny - g_allHolesArray[curHole].aimY);
            shape = CalculateBestShot(gi);

            int dx2, dy2;
            if (g_shotTargetExact == 0) {
                dx2 = (g_golferPool[gi].targetX << 10) - g_golferPool[gi].ballX + 0x200;
                dy2 = (g_golferPool[gi].targetY << 10) - g_golferPool[gi].ballY + 0x200;
                g_golferPool[gi].flags = g_golferPool[gi].flags & ~GOLFER_EXACT_AIM;
            } else {
                dx2 = (g_golferPool[gi].targetX << 10) - g_golferPool[gi].ballX;
                dy2 = (g_golferPool[gi].targetY << 10) - g_golferPool[gi].ballY;
                g_golferPool[gi].flags = g_golferPool[gi].flags | GOLFER_EXACT_AIM;
            }
            if (g_golferPool[gi].talents & TALENT_IMAGINATION) {
                int gx = g_golferPool[gi].targetX;
                int best = grid[gx][g_golferPool[gi].targetY];
                int c = best;
                int d;
                for (d = 0; d < 8; d += 2) {
                    dx = g_dirRowStep[d];
                    int dc = g_dirColStep[d];
                    int q = (grid[gx - dx][g_golferPool[gi].targetY - dc] - c) / 4;
                    int w = grid[gx + dx][g_golferPool[gi].targetY + dc] - q;
                    if (w < best)
                        best = w;
                }
            }
            int aimU = (dx2 * 25) >> 10;
            int aimV = (dy2 * 25) >> 10;
            g_golferPool[gi].ballBearing = CalculateAngle(aimU, aimV);
            dist = (int)sqrt((double)(aimU * aimU + aimV * aimV));
            if (g_debugShotGolfer == gi) {
                g_simScreen.RenderFullSurface(0);
                WaitForInputWithTimeout(0);
            }
            goto walk;
        }
        {
            int ax = g_golferPool[gi].targetX;
            int ay = g_golferPool[gi].targetY;
            g_shotDoubts = 0;
            g_plannedEndX = (ax << 10) + 0x200;
            g_plannedEndY = (ay << 10) + 0x200;
            if (terrain != TERRAIN_GREEN && dist > 25) {
                s = g_squareTypeTable[g_gameTileTerrainKind[(ax - g_dirRowStep[oct & 6]) * 50 + ay
                                                            - g_dirColStep[oct & 6]]]
                        .lieGrade;
                s += g_squareTypeTable[g_gameTileTerrainKind[(ax - g_dirRowStep[(oct + 1) & 6]) * 50 + ay
                                                             - g_dirColStep[(oct + 1) & 6]]]
                         .lieGrade;
                s -= g_squareTypeTable[g_gameTileTerrainKind[(ax + g_dirRowStep[oct & 6]) * 50 + ay
                                                             + g_dirColStep[oct & 6]]]
                         .lieGrade;
                s -= g_squareTypeTable[g_gameTileTerrainKind[(ax + g_dirRowStep[(oct + 1) & 6]) * 50 + ay
                                                             + g_dirColStep[(oct + 1) & 6]]]
                         .lieGrade;
                if (s >= 4)
                    dist += Clamp(dist / 4 - (g_squareTypeTable[terrain].lieGrade > 0 ? 0 : 6), 0, 12);
                if (s <= -4)
                    dist -= 6;
            }
        }
    }

walk:
    int oct2 = ((g_golferPool[gi].ballBearing >> 28) + 1) >> 1 & 7;
    int rough = 0, lastFlagged = 0, landmarkSite = 0, firstWaterStep = 0, penTarget = 0;
    int acc = 4 - (gi & 3);
    int k;
    for (k = 0; k < 32; k++)
        weight[k] = 0;
    int i;
    int steps = dist / 25;
    for (i = 0; i < steps; i++) {
        int radius = i * 0x400 + 0x200;
        int nx = (g_golferPool[gi].ballX + FixedPointSin(g_golferPool[gi].ballBearing, radius)) >> 10;
        int ny = (g_golferPool[gi].ballY - FixedPointCos(g_golferPool[gi].ballBearing, radius)) >> 10;
        if (IsPositionInvalid(nx, ny))
            break;
        int idx = nx * 50 + ny;
        int tt = g_gameTileTerrainKind[nx * 50 + ny];
        s = g_squareTypeTable[tt].lieGrade;
        acc = acc + (i * s) * 2;
        weight[tt] = weight[tt] + ((i + 1) * s) * 2;
        heading[tt] = oct2 ^ 4;
        if (g_gameTileTerrainKind[nx * 50 + ny] == TERRAIN_WATER && firstWaterStep == 0)
            firstWaterStep = i;
        if (radius != 0x200 && g_squareTypeTable[tt].category == CATEGORY_TREES)
            rough++;
        if (g_tileExtra[idx] & TILE_SCENIC)
            lastFlagged = tt;
        if (i <= steps / 2) {
            int base = GetTileElevation(tx, ty, 0) + 1;
            if (GetTileElevation(nx, ny, 0) > base)
                rough += 2;
        } else {
            int base = GetTileElevation(g_golferPool[gi].targetX, g_golferPool[gi].targetY, 0) + 1;
            if (GetTileElevation(nx, ny, 0) > base)
                rough += 2;
        }
        int wob = g_rng.NextInt(2) ? 0xaaaaaaa : -0xaaaaaaa;
        int mx = (g_golferPool[gi].ballX + FixedPointSin(g_golferPool[gi].ballBearing + wob, radius)) >> 10;
        int my = (g_golferPool[gi].ballY - FixedPointCos(g_golferPool[gi].ballBearing + wob, radius)) >> 10;
        acc += g_squareTypeTable[g_gameTileTerrainKind[mx * 50 + my]].lieGrade * (i + 1);
    }

    int d;
    for (d = 0; d < 8; d++) {
        int ex = g_dirRowStep[d] + g_golferPool[gi].targetX;
        int ey = g_dirColStep[d] + g_golferPool[gi].targetY;
        if (IsPositionInvalid(ex, ey)) {
            acc += steps * 4 + 4;
        } else {
            int tt = g_gameTileTerrainKind[ex * 50 + ey];
            int w = g_squareTypeTable[tt].lieGrade * (steps + 1) / 2;
            acc += w;
            weight[tt] = weight[tt] + w;
            heading[tt] = d;
        }
    }

    int bestWeight = -1, worstType, worstHeading;
    for (k = 0; k < 32; k++) {
        if (weight[k] > bestWeight) {
            bestWeight = weight[k];
            worstType = k;
            worstHeading = heading[k];
        }
    }

    int score = acc / (steps + 3);
    if (rough >= 2)
        score += 4;
    if (g_gameTileTerrainKind[(g_golferPool[gi].ballX >> 10) * 50 + (g_golferPool[gi].ballY >> 10)] == 1)
        score = 0;
    if (dist <= 40 && score > 10)
        score = 10;
    g_golferPool[gi].shotDifficulty = (signed char)score;

    if (g_golferPool[gi].talents & TALENT_IMAGINATION) {
        int e = GetTileElevation(g_golferPool[gi].targetX, g_golferPool[gi].targetY, 0) - GetTileElevation(tx, ty, 0);
        if (e > 0)
            dist += e * 25 / 8;
        else
            dist += e * 25 / 10;
    }

    dist = Clamp(dist, 0, maxDist);
    g_golferPool[gi].ballSpeed = dist;
    g_golferPool[gi].club = (signed char)((maxDist - dist) * 60 / (maxDist * 3));
    g_golferPool[gi].club = (signed char)Clamp(g_golferPool[gi].club, terrain != 0, 11);
    if (mode != 0 && g_shotSelectedKind == SHOT_HIGH_BACKSPIN && g_golferPool[gi].club < 5) {
        dist = maxDist * 13 / 18;
        g_golferPool[gi].ballSpeed = dist;
        g_golferPool[gi].club = 5;
    }
    if (terrain == TERRAIN_GREEN && dist < 50 && !(g_golferPool[gi].flags & GOLFER_AFTER_PENALTY))
        g_golferPool[gi].club = CLUB_PUTTER;

    int reach = g_golferPool[gi].ballSpeed * 20 / 25;
    g_golferPool[gi].ballSpeed = reach * 33 - reach * reach / 48 + 0x40;
    g_golferPool[gi].ballClimb = g_golferPool[gi].ballSpeed / 8 + 0x200;
    g_golferPool[gi].ballSpeed = CalculateShotPower(dist * 4 / 5, g_golferPool[gi].ballClimb, 0);

    int w = (g_curTournPrepBits & 2) ? 12 : 20;
    if (g_golferPool[gi].talents & TALENT_IMAGINATION) {
        if (g_simOpenFacilityLvl[SCENERY_PUTTING_GREEN] != 0) {
            if ((g_golferPool[gi].persona & PERSONA_KIND) != PERSONA_ROSTER)
                w += w / (4 - g_simOpenFacilityLvl[SCENERY_PUTTING_GREEN]);
        }
    }
    if (g_golferPool[gi].skillMask & 0x10)
        w += g_golferPool[gi].ratings[4] * w / 8;
    int curHole = g_golferPool[gi].holeIdx;
    if (g_tileVariationIdx[g_allHolesArray[curHole].aimX * 50 + g_allHolesArray[curHole].aimY] & 0x80)
        w -= 10;
    if (g_golferPool[gi].attitude < 2)
        w = w / 2;
    int half = w / 2;
    int puttReach = (half < 0 ? g_rng.NextInt(half) : g_rng.NextInt(half)) + half + 4;

    steps = CalculateShotPower(maxDist * 4 / 5, g_golferPool[gi].ballClimb, 0);
    g_golferPool[gi].ballCurve = 0;
    if (g_shotPlanMode < SHOT_PLAN_ROUTE)
        g_golferPool[gi].ballCurve = CalculateRandomShotVariance() * 5 << 16;
    if (g_shotPlanMode == SHOT_RANDOM_THIRD)
        g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve / 3;
    if (g_golferPool[gi].attitude < 0) {
        int base = g_golferPool[gi].ballCurve;
        g_golferPool[gi].ballCurve =
            Clamp(-g_golferPool[gi].attitude, 0, 3) * base / (terrain != TERRAIN_GREEN ? 3 : 8) + base;
    }

    int level = g_level;
    if (terrain != TERRAIN_GREEN && (g_golferPool[gi].persona & PERSONA_KIND) != PERSONA_ROSTER
        && ((g_golferPool[gi].talents & TALENT_ACCURACY) || (g_golferPool[gi].flags & GOLFER_AFTER_PENALTY))) {
        int curve = g_golferPool[gi].ballCurve / (g_simOpenFacilityLvl[SCENERY_PRO_SHOP] + 2);
        g_golferPool[gi].ballCurve = curve;
        if (level < 2)
            g_golferPool[gi].ballCurve = curve - curve / (level + 2);
    }
    if ((g_golferPool[gi].flags & 0x4000000) && (g_golferPool[gi].persona & PERSONA_KIND) != PERSONA_ROSTER)
        g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve - g_golferPool[gi].ballCurve / 3;
    g_golferPool[gi].flags &= ~GOLFER_BALL_TRAIL;
    d = -3;
    if (g_golferPool[gi].club == CLUB_PUTTER) {
        g_golferPool[gi].ballCurve = 0;
        if (((g_runModeFlags & GAME_TOURNAMENT) ? 2 : 1) * dist > puttReach) {
            switch (g_rng.NextInt(2)) {
            case 0: g_golferPool[gi].ballCurve = (g_rng.NextInt(150) + 150) << 18; break;
            case 1: g_golferPool[gi].ballCurve = (-150 - g_rng.NextInt(150)) << 18; break;
            }
            if (dist <= 5)
                g_golferPool[gi].ballCurve = 0;
            if (dist > 15)
                g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve / 2;
            if (dist > 25)
                g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve / 2;
            if (dist > 35)
                g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve / 2;
        }
    } else {
        if (g_golferPool[gi].persona != 0) {
            if (gi != g_watchedPlayerIdx && (g_golferPool[gi].persona & PERSONA_KIND) == PERSONA_ROSTER)
                g_golferPool[gi].ballCurve = (3 - level) * g_golferPool[gi].ballCurve / 3 + g_golferPool[gi].ballCurve;
            else if (gi == g_watchedPlayerIdx)
                g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve - (3 - level) * g_golferPool[gi].ballCurve / 6;
            else
                d = g_golferPool[gi].attitude - 3;
            if (g_golferPool[gi].club > LAST_LONG_CLUB) {
                g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve * 6 / (g_golferPool[gi].ratings[3] + 4);
                d += g_golferPool[gi].ratings[3];
            } else if (g_golferPool[gi].holeStrokes == 0) {
                g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve * 6 / (g_golferPool[gi].ratings[2] + 4);
                d += g_golferPool[gi].ratings[2];
            }
            if (shape == 1) {
                g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve * 6 / (g_golferPool[gi].ratings[5] + 3);
                d += g_golferPool[gi].ratings[5];
            } else if (shape == -1) {
                g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve * 6 / (g_golferPool[gi].ratings[6] + 3);
                d += g_golferPool[gi].ratings[6];
            } else if (shape == 0 && g_shotSelectedKind != SHOT_HIGH_BACKSPIN) {
                g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve * 6 / 7;
                d += 3;
            }
        }
        if (!(g_runModeFlags & OPT_PLANNING_SHOT) && dist > 75) {
            int speed = g_golferPool[gi].ballSpeed;
            if (g_rng.NextInt(speed) > (abs(g_golferPool[gi].ballCurve) >> 9) + 0x200) {
                g_golferPool[gi].flags |= GOLFER_BALL_TRAIL;
                d += 4;
                if (gi == g_watchedPlayerIdx)
                    g_golferPool[gi].ballCurve = 0;
                else
                    g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve / 2;
            }
        }
    }

    int aim = g_golferPool[gi].ballBearing;
    if (shape == 1)
        aim += 0x15555554;
    else if (shape == -1)
        aim -= 0x15555554;
    if (g_golferPool[gi].club != CLUB_PUTTER) {
        int cl = Clamp(g_golferPool[gi].ballCurve, -0x38e38e3, 0x38e38e3);
        g_golferPool[gi].ballBearing = g_golferPool[gi].ballBearing + cl * 3;
        g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve + -(cl / 2);
        if (g_shotPlanMode == 0 && (g_golferPool[gi].persona & PERSONA_KIND) == PERSONA_ROSTER) {
            g_golferPool[gi].ballCurve = (dist - 100) * g_golferPool[gi].ballCurve / 256 + g_golferPool[gi].ballCurve;
            g_golferPool[gi].ballCurve += (g_rng.NextInt(0x71c6) - 0x38e38e3) / 50;
        }
    }
    g_golferPool[gi].flags &= ~0x180;
    g_golferPool[gi].shotType = 0;
    int waterStep;
    if ((g_golferPool[gi].flags & GOLFER_AFTER_PENALTY) && mode == 0)
        waterStep = 1;
    else
        waterStep = firstWaterStep;

    if (((g_golferPool[gi].talents & TALENT_IMAGINATION) || (g_golferPool[gi].flags & 2)) && terrain != TERRAIN_GREEN
        && waterStep != 1 && (mode == 0 || g_shotSelectedKind == SHOT_LOW_PUNCH)
        && (mode != 0
            || g_squareTypeTable[g_gameTileTerrainKind[((g_golferPool[gi].ballX + FixedPointSin(aim, 0x80)) >> 10) * 50
                                                       + ((g_golferPool[gi].ballY - FixedPointCos(aim, 0x80)) >> 10)]]
                       .category
                   == CATEGORY_TREES
            || g_squareTypeTable[g_gameTileTerrainKind[((g_golferPool[gi].ballX + FixedPointSin(aim, 0x400)) >> 10) * 50
                                                       + ((g_golferPool[gi].ballY - FixedPointCos(aim, 0x400)) >> 10)]]
                       .category
                   == CATEGORY_TREES)) {
        g_golferPool[gi].ballClimb = Clamp(g_golferPool[gi].ballSpeed / 12, 0, 0x100);
        g_golferPool[gi].ballSpeed = (maxDist * 33 - maxDist * maxDist / 48 + g_golferPool[gi].ballSpeed + 0x40) / 2;
        g_golferPool[gi].ballSpeed = CalculateShotPower(dist * 3 / 4, g_golferPool[gi].ballClimb, 0);
        if (waterStep != 0)
            g_golferPool[gi].ballSpeed = CalculateShotPower(waterStep * 50 / 3, g_golferPool[gi].ballClimb, 0);
        shape = 0;
        g_golferPool[gi].shotType = SHOT_LOW_PUNCH;
    } else if (g_golferPool[gi].talents & TALENT_IMAGINATION) {
        if (shape == 0 && terrain != TERRAIN_GREEN && g_squareTypeTable[terrain].lieGrade == 0
            && g_golferPool[gi].club >= 4 && dist > 25
            && (mode != 0
                || g_gameTileTerrainKind[g_golferPool[gi].targetX * 50 + g_golferPool[gi].targetY] == TERRAIN_GREEN)
            && (mode == 0 || g_shotSelectedKind == SHOT_HIGH_BACKSPIN)) {
            g_golferPool[gi].flags |= 0x80;
            g_golferPool[gi].ballClimb = (dist + 50) * g_golferPool[gi].ballClimb / 400 + g_golferPool[gi].ballClimb;
            g_golferPool[gi].ballSpeed = CalculateShotPower(dist, g_golferPool[gi].ballClimb, 0);
            g_golferPool[gi].shotType = SHOT_HIGH_BACKSPIN;
            g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve * 6 / (g_golferPool[gi].ratings[7] + 3);
            d += g_golferPool[gi].ratings[7];
        }
    }

    maxDist = g_golferPool[gi].slotStamp[0];
    if (px == -1) {
        if (g_golferPool[gi].club != CLUB_PUTTER) {
            g_allHolesArray[g_golferPool[gi].holeIdx].shots++;
            int sample;
            for (sample = 0; sample < GetTileElevation(tx, ty, 0) * 16 / (g_level + 2); sample++) {
                int aim2 = g_golferPool[gi].ballBearing;
                aim2 += (12 - g_rng.NextInt(25)) << 24;
                int dd = dist - g_rng.NextInt(200) + 100;
                if (dd < 0)
                    continue;
                int rr = (dd / 25) << 10;
                int nx = (g_golferPool[gi].x + FixedPointSin(aim2, rr)) >> 10;
                int ny = (g_golferPool[gi].y - FixedPointCos(aim2, rr)) >> 10;
                int oct3 = ((aim2 >> 28) + 1) >> 1 & 7;
                if (IsPositionInvalid(nx, ny))
                    continue;
                int idx2 = nx * 50 + ny;
                if ((g_tileExtra[idx2] & TILE_SCENIC) || g_gameTileTerrainKind[nx * 50 + ny] == TERRAIN_MARSH) {
                    if (GetTileElevation(nx, ny, 0) <= GetTileElevation(tx, ty, 0) + 1) {
                        int behindCover =
                            g_gameTileTerrainKind[(nx - g_dirRowStep[oct3]) * 50 + ny - g_dirColStep[oct3]];
                        if (g_squareTypeTable[behindCover].category != CATEGORY_TREES)
                            lastFlagged = nx + ny * 50;
                    }
                }
                if (g_squareTypeTable[g_gameTileTerrainKind[nx * 50 + ny]].kind == 0x10) {
                    int behindCover = g_gameTileTerrainKind[(nx - g_dirRowStep[oct3]) * 50 + ny - g_dirColStep[oct3]];
                    if (g_squareTypeTable[behindCover].category != CATEGORY_TREES) {
                        int r = FindBuildingAtTile(nx, ny);
                        if (g_gameTileTerrainKind[nx * 50 + ny] == TERRAIN_HOME_SITE) {
                            if (r != -1 && g_buildingTable[r].kind == SCENERY_HOME_SITE
                                && g_buildingTable[r].count != 0)
                                penTarget = g_buildingTable[r].count;
                            else
                                landmarkSite = nx + ny * 50;
                        } else if (g_buildingTable[r].kind == SCENERY_LANDMARK) {
                            if (g_buildingTable[r].count < LAST_FIXED_LANDMARK)
                                lastFlagged = nx + ny * 50;
                            else
                                landmarkSite = nx + ny * 50;
                        }
                    }
                }
            }
        } else if (g_gameTileTerrainKind[g_golferPool[gi].targetX * 50 + g_golferPool[gi].targetY] == TERRAIN_GREEN) {
            g_golferPool[gi].ballSpeed = CalculateShotPower(dist + 2, 0, 1);
        }

        g_golferPool[gi].shotDifficulty += (signed char)abs(
            GetTileElevation(tx, ty, 0) - GetTileElevation(g_golferPool[gi].targetX, g_golferPool[gi].targetY, 0));
        if (g_hazardCommentary != 0
            && g_squareTypeTable[g_gameTileTerrainKind[g_golferPool[gi].targetX * 50 + g_golferPool[gi].targetY]]
                       .lieGrade
                   > 0
            && g_squareTypeTable[terrain].lieGrade <= 0 && mode == 0
            && !(g_golferPool[gi].flags & GOLFER_AFTER_PENALTY)) {
            SetGolferAction(gi, ACTION_UPSET, 20);
        } else if (landmarkSite != 0 && penTarget == 0) {
            SetGolferAction(gi, 0x14, landmarkSite);
        } else if (penTarget != 0) {
            SetGolferAction(gi, 0x16, penTarget - 1);
        } else if ((g_tileExtra[tx * 50 + ty] & TILE_WEEDS) && GetGolferMoodIndex(gi)
                   && !(g_tileExtra[tx * 50 + ty] & TILE_ANIMATING)) {
            SetGolferAction(gi, 0x18, 20);
        } else {
            if (dist < 100 || mode != 0)
                g_shotDoubts &= ~(DOUBT_LENGTH | DOUBT_LINE);
            int m = g_shotDoubts;
            int roll;
            if (m != 0 && g_golferPool[gi].shotType != SHOT_LOW_PUNCH) {
                if (m & (DOUBT_LENGTH | DOUBT_LINE))
                    SetGolferAction(gi, (~m & 1) | 0x20, 20);
                else if (g_squareTypeTable[terrain].lieGrade <= 0)
                    SetGolferAction(gi, 6, 20);
            } else if (g_golferPool[gi].holeIdx > 1 && g_golferPool[gi].holeStrokes == (gi & 1) + 1
                       && g_allHolesArray[g_golferPool[gi].holeIdx].similar <= (roll = g_rng.NextInt(3))) {
                SetGolferAction(gi, ACTION_LIKES_DIFFERENT_HOLE, 20);
            } else if (g_level != 0 && g_golferPool[gi].holeIdx > 1 && g_golferPool[gi].holeStrokes == 1
                       && g_allHolesArray[g_golferPool[gi].holeIdx].similar > (roll = g_rng.NextInt(3)) + 3) {
                SetGolferAction(gi, 0x1e, 20);
            } else if (g_level != 0 && g_golferPool[gi].shotDifficulty <= g_rng.NextInt(4) + g_level * 2 && dist > 100
                       && g_squareTypeTable[terrain].lieGrade <= 0 && lastFlagged == 0) {
                SetGolferAction(gi, 4, 20);
                g_golferPool[gi].shotDifficulty = 0;
            } else {
                roll = g_rng.NextInt(5);
                if (g_golferPool[gi].shotDifficulty > roll + g_level + 6 && dist > 40 && lastFlagged == 0) {
                    if (g_golferPool[gi].shotDifficulty >= 24) {
                        SetGolferAction(gi, 0x1f, worstType);
                    } else {
                        int e = (worstHeading - oct2) & 7;
                        if (e >= 2 && e <= 6) {
                            if (e == 2 || e == 6)
                                SetGolferAction(gi, 5, worstType);
                            else
                                SetGolferAction(gi, 0x25, worstType);
                        } else {
                            SetGolferAction(gi, 0x26, worstType);
                        }
                    }
                    g_golferPool[gi].shotDifficulty = (g_golferPool[gi].shotDifficulty >= 24) ? 20 : 10;
                } else {
                    g_golferPool[gi].shotDifficulty = 6;
                }
            }
        }
        g_golferPool[gi].flags &= ~(GOLFER_LEANS_LEFT | GOLFER_LEANS_RIGHT);

        if (g_golferPool[gi].slotStamp[0] == maxDist && g_golferPool[gi].talkTimer == 0) {
            if (lastFlagged != 0 && g_squareTypeTable[terrain].lieGrade < 1) {
                SetGolferAction(gi, 0x1c, lastFlagged);
            } else {
                if (shape == 1)
                    SetGolferAction(gi, 0x37, 20);
                else if (shape == -1)
                    SetGolferAction(gi, 0x38, 20);
                if ((g_golferPool[gi].flags & 0x80) && dist > 100)
                    SetGolferAction(gi, 0x39, 20);
                if (g_golferPool[gi].shotType == SHOT_LOW_PUNCH)
                    SetGolferAction(gi, g_golferPool[gi].holeStrokes ? 0x3c : 9, terrain);
                if (dist > 75) {
                    if (g_golferPool[gi].ballCurve > (30 << ((g_golferPool[gi].talents & TALENT_ACCURACY) ? 15 : 16)))
                        g_golferPool[gi].flags |= GOLFER_LEANS_LEFT;
                    if (g_golferPool[gi].ballCurve < (-30 << ((g_golferPool[gi].talents & TALENT_ACCURACY) ? 15 : 16)))
                        g_golferPool[gi].flags |= GOLFER_LEANS_RIGHT;
                }
                if (g_golferPool[gi].slotStamp[0] == maxDist && g_golferPool[gi].talkTimer == 0
                    && g_golferPool[gi].holeStrokes == 0
                    && g_golferHistoryPerCharacter[g_golferPool[gi].typeIdx].perHoleStrokes[g_golferPool[gi].holeIdx]
                           != 0
                    && g_golferPool[gi].persona == 0)
                    SetGolferAction(gi, 0x3b, 20);
                if (g_golferPool[gi].slotStamp[0] == maxDist && g_golferPool[gi].holeStrokes == 3
                    && !((g_golferPool[gi].holeIdx + gi) & 1) && g_golferPool[gi].persona == 0
                    && g_golferPool[g_golferPool[gi].partner].persona == 0) {
                    int other = gi ^ 1;
                    if (IsGolferMale(gi) == IsGolferMale(other)) {
                        if (g_golferPool[g_golferPool[gi].partner].talkTimer == 0 && g_golferPool[gi].talkTimer == 0) {
                            SetGolferAction(gi, 0x30, 20);
                            SetGolferAction(other, 0x31, 20);
                            g_golferPool[g_golferPool[gi].partner].talkTimer++;
                        }
                    }
                }
            }
        }
        if (g_golferPool[gi].talkTimer != 0)
            maxDist = -1;
        if (g_golferPool[gi].slotStamp[0] == maxDist && g_golferPool[gi].holeIdx > 1
            && g_golferPool[gi].club < CLUB_PUTTER && !((1 << g_golferPool[gi].club) & g_golferPool[gi].clubsUsed))
            SetGolferAction(gi, 0x36, g_golferPool[gi].club);
        g_golferPool[gi].clubsUsed |= (short)(1 << g_golferPool[gi].club);
        if (g_golferPool[gi].slotStamp[0] == maxDist) {
            if (GetTileElevation(tx, ty, 0) > GetTileElevation(g_golferPool[gi].targetX, g_golferPool[gi].targetY, 0))
                SetGolferAction(gi, 0x2e, 20);
            if (GetTileElevation(tx, ty, 0) < GetTileElevation(g_golferPool[gi].targetX, g_golferPool[gi].targetY, 0))
                SetGolferAction(gi, 0x2d, 20);
        }
        if (g_golferPool[gi].slotStamp[0] == maxDist && g_golferPool[gi].persona == 0 && g_golferPool[gi].talkTimer == 0
            && terrain == TERRAIN_GREEN) {
            if (!((g_golferPool[gi].holeStrokes + (gi + g_golferPool[gi].holeIdx)) & 3))
                SetGolferAction(gi, 0x3e, 20);
        }
    }

    g_golferPool[gi].flags &= ~2;
    int drift = 0;
    int hi;
    if (shape == 1) {
        g_golferPool[gi].ballBearing += 0x15555554;
        if (g_golferPool[gi].flags & 0x80)
            g_golferPool[gi].ballBearing = g_golferPool[gi].ballBearing - 0x5555555;
        if (g_golferPool[gi].ballCurve > 0 && gi == g_watchedPlayerIdx)
            g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve / 2;
        drift = -0x239a955;
        int den = 400 - dist;
        steps += steps * 16 / den;
        if (dist < 300) {
            if (dist < 250)
                g_golferPool[gi].ballSpeed += g_golferPool[gi].ballSpeed * 16 / (330 - dist);
            else
                g_golferPool[gi].ballSpeed += g_golferPool[gi].ballSpeed * 16 / den;
        } else {
            g_golferPool[gi].ballSpeed = steps;
        }
        g_golferPool[gi].shotType = 1;
        hi = 999999;
    } else if (shape == -1) {
        g_golferPool[gi].ballBearing -= 0x15555554;
        if (g_golferPool[gi].flags & 0x80)
            g_golferPool[gi].ballBearing = g_golferPool[gi].ballBearing + 0x5555555;
        if (g_golferPool[gi].ballCurve < 0 && gi == g_watchedPlayerIdx)
            g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve / 2;
        drift = 0x239a955;
        int den2 = 400 - dist;
        steps += steps * 16 / den2;
        if (dist < 300) {
            if (dist < 250)
                g_golferPool[gi].ballSpeed += g_golferPool[gi].ballSpeed * 16 / (330 - dist);
            else
                g_golferPool[gi].ballSpeed += g_golferPool[gi].ballSpeed * 16 / den2;
        } else {
            g_golferPool[gi].ballSpeed = steps;
        }
        g_golferPool[gi].shotType = -1;
        hi = 999999;
    } else {
        hi = steps;
    }
    g_golferPool[gi].ballSpeed = Clamp(g_golferPool[gi].ballSpeed, 0, hi);
    int planned = g_golferPool[gi].ballSpeed;
    if (dist < 75) {
        int curl = 1;
        if (g_level == 0) {
            if (gi == mode) {
                g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve / 3;
                curl = 0;
            } else if (gi == (mode | 1)) {
                g_golferPool[gi].ballCurve = g_golferPool[gi].ballCurve * 2;
            }
        }
        if (curl && terrain != TERRAIN_GREEN)
            g_golferPool[gi].ballBearing +=
                (((g_golferPool[gi].talents & SKILLS_ALL) == SKILLS_ALL) ? g_level + 4 : 9 - g_level)
                * g_golferPool[gi].ballCurve * 2;
    }

    if (g_golferPool[gi].persona != 0 && g_squareTypeTable[terrain].lieGrade > 0) {
        int odds = g_squareTypeTable[terrain].lieGrade * 10;
        int roll = g_rng.NextInt(odds);
        if (g_golferPool[gi].ratings[8] >= roll) {
            g_golferPool[gi].flags |= GOLFER_BALL_TRAIL;
            terrain = TERRAIN_FAIRWAY;
        }
    }
    if (g_shotPlanMode == 0 && terrain != TERRAIN_GREEN) {
        int rj = g_rng.NextInt(0x5555);
        int wc2 = g_squareTypeTable[terrain].lieGrade;
        g_golferPool[gi].ballBearing += Sign(g_golferPool[gi].ballCurve) * (rj + 0x1555555) * wc2;
        g_golferPool[gi].ballCurve = (wc2 + 2) * g_golferPool[gi].ballCurve / 2;
    }
    if ((g_golferPool[gi].flags & GOLFER_AFTER_PENALTY) && g_golferPool[gi].holeStrokes > 6) {
        g_golferPool[gi].ballSpeed = planned;
        terrain = -1;
    }
    g_golferPool[gi].ballCurve += drift;
    int luck;
    switch (terrain) {
    case TERRAIN_GREEN:
        luck = g_rng.NextInt(g_golferPool[gi].ballSpeed / 8);
        g_golferPool[gi].ballSpeed = g_golferPool[gi].ballSpeed / 12 - luck + g_golferPool[gi].ballSpeed;
        if (g_golferPool[gi].club == CLUB_PUTTER)
            g_golferPool[gi].ballClimb = 0;
        break;
    case 0:
        luck = g_rng.NextInt(g_golferPool[gi].ballSpeed / 4);
        g_golferPool[gi].ballSpeed = g_golferPool[gi].ballSpeed / 8 - luck + g_golferPool[gi].ballSpeed;
        break;
    case TERRAIN_FAIRWAY:
        if (g_tileExtra[tx * 50 + ty] & TILE_WEEDS) {
            luck = g_golferPool[gi].ballSpeed;
            luck = g_rng.NextInt(luck);
            g_golferPool[gi].ballSpeed = g_golferPool[gi].ballSpeed / 2 - luck + g_golferPool[gi].ballSpeed;
        } else {
            luck = g_rng.NextInt(g_golferPool[gi].ballSpeed / 3);
            g_golferPool[gi].ballSpeed = g_golferPool[gi].ballSpeed / 6 - luck + g_golferPool[gi].ballSpeed;
        }
        break;
    case TERRAIN_ROUGH:
    case TERRAIN_DEEP_ROUGH:
    case 8:
    case TERRAIN_HOME_SITE:
    case TERRAIN_BUILDING:
        luck = g_rng.NextInt(g_golferPool[gi].ballSpeed / 2);
        g_golferPool[gi].ballSpeed = g_golferPool[gi].ballSpeed / 4 - luck + g_golferPool[gi].ballSpeed;
        g_golferPool[gi].ballCurve += g_rng.NextInt(g_golferPool[gi].ballCurve / 2);
        if (terrain == (g_currentThemeId != THEME_DESERT) + 4)
            g_golferPool[gi].ballBearing += g_golferPool[gi].ballCurve * 4;
        if (!(g_curTournPrepBits & 4))
            break;
    case TERRAIN_RAVINE:
    case TERRAIN_TREE:
    case TERRAIN_PINE_TREE:
    case TERRAIN_PALM_TREE:
    case TERRAIN_ELM_TREE:
        luck = g_rng.NextInt(g_golferPool[gi].ballSpeed / 2);
        g_golferPool[gi].ballSpeed = g_golferPool[gi].ballSpeed / 4 - luck + g_golferPool[gi].ballSpeed;
        break;
    case 9:
        luck = g_golferPool[gi].ballSpeed * 2;
        g_golferPool[gi].ballSpeed = g_rng.NextInt(luck);
        g_golferPool[gi].ballBearing += g_golferPool[gi].ballCurve * 16;
        break;
    case TERRAIN_BRUSH:
    case TERRAIN_ROCKS:
        luck = g_golferPool[gi].ballSpeed;
        luck = g_rng.NextInt(luck);
        g_golferPool[gi].ballSpeed = g_golferPool[gi].ballSpeed / 2 + luck;
        break;
    case -1: break;
    default:
        luck = g_golferPool[gi].ballSpeed;
        luck = g_rng.NextInt(luck);
        g_golferPool[gi].ballSpeed = g_golferPool[gi].ballSpeed / 2 - luck + g_golferPool[gi].ballSpeed;
        break;
    }
    if (((g_golferPool[gi].talents & TALENT_ACCURACY) && g_squareTypeTable[terrain].lieGrade <= 0)
        || (g_golferPool[gi].flags & GOLFER_AFTER_PENALTY))
        g_golferPool[gi].ballSpeed = (planned + g_golferPool[gi].ballSpeed) / 2;
    if (g_shotPlanMode == 0)
        g_golferPool[gi].ballSpeed = g_squareTypeTable[terrain].lieGrade * (g_golferPool[gi].ballSpeed - planned) / 8
                                     + g_golferPool[gi].ballSpeed;
    if (g_golferPool[gi].ballSpeed < planned / 3)
        g_golferPool[gi].ballSpeed = planned / 3;
    if (g_shotPlanMode >= SHOT_PLAN_ROUTE)
        g_golferPool[gi].ballSpeed = planned;
    if (g_shotPlanMode == SHOT_RANDOM_THIRD)
        g_golferPool[gi].ballSpeed = (g_golferPool[gi].ballSpeed + planned * 2) / 3;
    g_golferPool[gi].ballSpeed =
        Clamp(d, -10, 16) * (planned - g_golferPool[gi].ballSpeed) / 16 + g_golferPool[gi].ballSpeed;
    g_golferPool[gi].ballCurve = Clamp(g_golferPool[gi].ballCurve, -0x15555555, 0x15555555);
    g_golferPool[gi].flags &= ~GOLFER_AFTER_PENALTY;
    g_squareTypeTable[TERRAIN_WATER].lieGrade = 8;
    g_squareTypeTable[TERRAIN_OUT_OF_BOUNDS].lieGrade = 8;
}

void ClearGolferSlotAndNamedScenery(int gi)
{
    g_golferPool[gi].holeStrokes = 0;
    g_golferPool[gi].activity = ACTIVITY_SWING;
    g_golferPool[gi].holeIdx = 0;

    short type = g_golferPool[gi].typeIdx;
    for (int i = 0; i < 100; i++)
        if (g_namedSceneryArray[i].owner == type)
            g_namedSceneryArray[i].x = -1;
}

void ProcessVIPGolferEvent(int gi)
{
    int amount, rating, landmark, i;
    const char *pronoun;
    char pronounText[4];

    if ((g_golferPool[gi].persona & PERSONA_KIND) == PERSONA_CEO) {
        strcpy(g_printBuffer, "Corporate CEO ");
        AppendGolferName(gi, 0);
        g_ceoLadderRung &= ~0x80;
        if (g_allHolesArray[g_golferPool[gi].holeIdx].par == 0 && g_golferPool[gi].enjoyment > 2) {
            if (g_golferPool[gi].holeIdx <= g_ceoLadderRung * 2) {
                strcat(g_printBuffer,
                    " has decided not to invest in your course. 'Good course, needs more holes' he says.");
                ShowStoryMessage(COLOR_DARK_RED, 1, gi);
                g_ceoLadderRung--;
            } else {
                NotifyEvent(0x100, g_golferPool[gi].persona & ~PERSONA_KIND);
                amount = ((g_golferPool[gi].enjoyment > 4) + 1) * 5000 / 100;
                strcat(g_printBuffer, " has decided to invest in your course. 'I'll give you \xa7");
                FormatNumberWithCommas(amount * 100);
                strcat(g_printBuffer, " for a seat on the board' he says.");
                g_reportScreenId = 0;
                ShowStoryMessage(COLOR_DARK_GREEN, 1, gi);
                g_gameCashHundreds += amount;
                g_annualStatsRecords[g_careerPlayYear % 100].v[LEDGER_OTHER] += (short)amount;
                ConfigureAndPlaySound(0x19, SOUND_FULL_VOLUME, 0, 0, 0);
            }
        } else {
            strcat(g_printBuffer, " has decided not to invest in your course. '");
            if (g_golferPool[gi].enjoyment > 2) {
                strcat(g_printBuffer, "My partner ");
                AppendGolferName(g_golferPool[gi].partner, 0);
                strcat(g_printBuffer, " has quit, even though ");
            }
            AppendPartnerNote(g_golferPool[gi].enjoyment, 0);
            strcat(g_printBuffer, "' he fumes.");
            ShowStoryMessage(COLOR_DARK_RED, 1, gi);
        }
    }

    if ((g_golferPool[gi].persona & PERSONA_KIND) == PERSONA_COMMISSIONER) {
        strcpy(g_printBuffer, "County commissioner ");
        AppendGolferName(gi, 0);
        g_landPurchaseNum &= ~0x80;
        if (g_allHolesArray[g_golferPool[gi].holeIdx].par == 0 && g_golferPool[gi].enjoyment > 2) {
            strcat(g_printBuffer, " has decided to approve an expansion request!");
            strcat(g_printBuffer, " 'I'll be back again if your ");
            if (g_level <= 1) {
                strcat(g_printBuffer, "fun rating is over ");
                strcat(g_printBuffer, itoa((g_level + 2) * (g_landPurchaseNum + 3) * 50, g_numberBuf, 10));
            } else {
                strcat(g_printBuffer, "skill rating is over ");
                rating = (g_level + 2) * (g_landPurchaseNum + 3) * 50;
                AppendMoney(rating);
            }
            strcat(g_printBuffer, "', he says.");
            g_reportScreenId = 0;
            g_bubbleStep = BUBBLE_OPEN;
            ShowStoryMessage(COLOR_DARK_GREEN, 1, gi);
            ConfigureAndPlaySound(SOUND_APPLAUSE_GOOD, SOUND_FULL_VOLUME, 0, 0, 0);
            g_allowedExpansionRequestFlag |= 1;
        } else {
            strcat(g_printBuffer, " has decided not to approve your expansion request. '");
            if (g_golferPool[gi].enjoyment > 2) {
                strcat(g_printBuffer, "My partner ");
                AppendGolferName(g_golferPool[gi].partner, 0);
                strcat(g_printBuffer, " has quit, even though ");
            }
            AppendPartnerNote(g_golferPool[gi].enjoyment, 0);
            strcat(g_printBuffer, "' he comments.");
            ShowStoryMessage(COLOR_DARK_RED, 1, gi);
        }
    }

    if ((g_golferPool[gi].persona & PERSONA_KIND) == PERSONA_HEIRESS) {
        strcpy(g_printBuffer, "Wealthy Heiress ");
        AppendGolferName(gi, 0);
        if (g_allHolesArray[g_golferPool[gi].holeIdx].par == 0 && g_golferPool[gi].enjoyment > 2) {
            i = g_golferPool[gi].enjoyment;
            while (1) {
                landmark = Clamp(g_rng.NextInt(i) & 0xffff, 0, 15);
                if (!(g_shownLandmarkBitmask & (1 << landmark)))
                    break;
                i++;
                if (i > 24)
                    break;
            }
            strcat(g_printBuffer, " has decided to donate a");
            AppendLandmarkName(landmark, 1);
            strcat(g_printBuffer, " to your course! In the area near this landmark ");
            switch (landmark & 3) {
            case 0: strcat(g_printBuffer, "golfers will have happy thoughts."); break;
            case 1: strcat(g_printBuffer, "no dandelions will appear."); break;
            case 2:
                strcat(g_printBuffer, g_golferTypeArray[0].name);
                strcat(g_printBuffer, "'s skills will improve rapidly.");
                break;
            case 3: strcat(g_printBuffer, "golfer stories will proceed happily."); break;
            }
            strcat(g_printBuffer, " Go to 'Landmarks' under the Improvements menu to place your landmark. Additional ");
            AppendLandmarkName(landmark, 0);
            strcat(g_printBuffer, "s can be added to your course for ");
            FormatNumberWithCommas((landmark + 1) * 1000);
            strcat(g_printBuffer, ". 'I'll be back if your ");
            g_curLandmarkTotal++;
            if (g_level <= 1) {
                strcat(g_printBuffer, "fun rating is up to ");
                strcat(g_printBuffer, itoa((g_curLandmarkTotal + 1) * (g_curLandmarkTotal + 1) * 25, g_numberBuf, 10));
            } else {
                strcat(g_printBuffer, "skill rating is up to ");
                rating = (g_curLandmarkTotal + 1) * (g_curLandmarkTotal + 1) * 25;
                AppendMoney(rating);
            }
            strcat(g_printBuffer, "', she says.");
            g_reportScreenId = 0;
            ShowStoryMessage(COLOR_DARK_GREEN, 1, gi);
            ConfigureAndPlaySound(SOUND_APPLAUSE_GOOD, SOUND_FULL_VOLUME, 0, 0, 0);
            NotifyEvent(0x160, landmark);
            g_shownLandmarkBitmask |= 1 << landmark;
            g_freeLandmarks |= 1 << landmark;
        } else {
            strcat(g_printBuffer, " has decided not to donate a landmark to your course. '");
            if (g_golferPool[gi].enjoyment > 2) {
                strcat(g_printBuffer, "My partner ");
                AppendGolferName(g_golferPool[gi].partner, 0);
                strcat(g_printBuffer, " has quit, even though ");
            }
            AppendPartnerNote(g_golferPool[gi].enjoyment, 0);
            strcat(g_printBuffer, "' she comments. 'I'll be back in a few months.'");
            ShowStoryMessage(COLOR_DARK_RED, 1, gi);
        }
    }

    if ((g_golferPool[gi].persona & PERSONA_KIND) == 0x100) {
        strcpy(g_printBuffer, "International celebrity ");
        AppendGolferName(gi, 0);
        if (g_golferPool[gi].persona & 1)
            pronoun = "he";
        else
            pronoun = "she";
        strcpy(pronounText, pronoun);
        if (g_allHolesArray[g_golferPool[gi].holeIdx].par == 0 && g_golferPool[gi].enjoyment >= 2) {
            strcat(g_printBuffer, " has decided to buy a vacation home at your course. 'Groovy course!', ");
            g_reportScreenId = 0;
            strcat(g_printBuffer, pronounText);
            strcat(g_printBuffer, " says.");
            ShowStoryMessage(COLOR_PURPLE, 1, gi);
            NotifyEvent(EVENT_CELEBRITY, g_golferPool[gi].persona & ~PERSONA_KIND);
            ConfigureAndPlaySound(0x2c, SOUND_FULL_VOLUME, 0, 0, 0);
            for (i = 0; i < 256; i++) {
                if (g_buildingTable[i].kind == SCENERY_HOME_SITE && g_buildingTable[i].resident == -1) {
                    g_buildingTable[i].resident = g_golferPool[gi].persona & ~PERSONA_KIND;
                    break;
                }
            }
        } else {
            strcat(g_printBuffer, " has decided not to buy a vacation home at your course. 'More fun in Malibu' ");
            g_reportScreenId = 0;
            strcat(g_printBuffer, pronounText);
            strcat(g_printBuffer, " says.");
            ShowStoryMessage(COLOR_DARK_RED, 1, gi);
        }
    }

    g_golferPool[gi].ballX = 0;
    g_golferPool[gi].holeIdx = CLUBHOUSE_HOLE;
    if (g_golferPool[gi].flags & GOLFER_PRO_MATCH)
        g_watchedPlayerIdx = -1;
}

#define LAST_HOLE             18
#define HOLE_SLOTS            19
#define PAIR_EVEN_MASK        0xfffe
#define TALENT_MATCH_PLAY     0xf0
#define MONEY_UNIT            100
#define LEDGER_YEARS          100
#define LEDGER_FEES           0
#define SLOT_BEST_NAMES       0x14
#define BEST_SCORES           10
#define UNPLAYED_STROKES      5
#define LOW_FUNDS             200
#define UPGRADE_PAUSE         8
#define ENJOYMENT_FOR_UPGRADE(level) (2 * (level) + 6)
#define ACCOMP_FIRST_MATCH_WIN 5
#define EVENT_MATCH_WON       0x60

void ProcessHoleCompletion(int gi)
{
    if (g_golferPool[gi].persona == 0) {
        int missingTalents = (unsigned char)~g_golferPool[gi].talents & SKILLS_ALL;
        CalculateHoleTalentStrokeGap(g_golferPool[gi].holeIdx, missingTalents);
        g_allHolesArray[g_golferPool[gi].holeIdx]
            .dist[g_golferPool[gi].talents & 0xf][Clamp(g_golferPool[gi].holeStrokes, 0, 9)]++;
        CalculateHoleTalentStrokeGap(g_golferPool[gi].holeIdx, missingTalents);

        int row = g_golferPool[gi].talentLevel + (g_golferPool[gi].talents & SKILLS_ALL) * 4;
        g_skillClassStatRows[row].holeTotal[g_golferPool[gi].holeIdx] += g_golferPool[gi].holeStrokes;
        g_skillClassStatRows[row].holeCount[g_golferPool[gi].holeIdx]++;
    }

    g_golferPool[gi].holeScore[g_golferPool[gi].holeIdx] = g_golferPool[gi].holeStrokes;

    g_calculatedGreenFee = g_golferPool[gi].enjoyment;
    if (g_currentFeesOption == 2)
        g_calculatedGreenFee += g_calculatedGreenFee;
    if (g_allHolesArray[g_golferPool[gi].holeIdx].flags & HOLE_TOP_100)
        g_calculatedGreenFee += 2;
    if (g_allHolesArray[g_golferPool[gi].holeIdx].flags & HOLE_TOP_18)
        g_calculatedGreenFee += 2;
    if (g_simOpenFacilityLvl[SCENERY_AIRSTRIP])
        g_calculatedGreenFee += g_simOpenFacilityLvl[SCENERY_AIRSTRIP];

    char tier = g_golferHistoryPerCharacter[g_golferPool[gi].typeIdx].flags & MEMBERSHIP_MASK;
    if (tier > 3) {
        g_calculatedGreenFee += (tier == 4) ? 2 : 5;
        PlayPositionedSound(0x19, g_golferPool[gi].x, g_golferPool[gi].y, 0);
    }

    if (!(g_runModeFlags & GAME_TOURNAMENT)) {
        if (g_careerPlayYear == 0 && g_annualStatsRecords[0].v[LEDGER_FEES] == 0) {
            g_selectedGolferIndex = gi | SIMFOTO_NOT_SAVED;
            g_printBuffer[0] = 0;
            AppendGolferName(gi, 0);
            strcat(g_printBuffer, " has just paid you your first greens fee of ");
            strcat(g_printBuffer, itoa(g_calculatedGreenFee * MONEY_UNIT, g_numberBuf, 10));
            strcat(g_printBuffer, " simoleans!  Golfers pay a fee at the end of each hole - ");
            strcat(g_printBuffer, "happy golfers pay higher fees, unhappy golfers pay less.");
            strcat(g_printBuffer,
                " Greens fees are your main source of revenue, so it pays to keep your golfers happy.");
            g_bubbleStep = BUBBLE_OPEN;
            strcat(g_printBuffer, " Refer to your financial report for more detailed information.");
            ShowStoryMessage(1, MESSAGE_URGENT, gi);
        }

        g_gameCashHundreds += g_calculatedGreenFee;
        g_allHolesArray[g_golferPool[gi].holeIdx].revenue += g_calculatedGreenFee;
        g_annualStatsRecords[g_careerPlayYear % LEDGER_YEARS].v[LEDGER_FEES] += g_calculatedGreenFee;
        AddMoneyPopup(g_calculatedGreenFee, g_golferPool[gi].x, g_golferPool[gi].y, -1);
    }

    if (g_golferPool[gi].talkTimer == 0)
        SetGolferAction(gi, 0x13, g_golferPool[gi].enjoyment);
    g_golferHistoryPerCharacter[g_golferPool[gi].typeIdx].perHoleStrokes[g_golferPool[gi].holeIdx] =
        (unsigned char)g_golferPool[gi].holeStrokes;

    for (int e = 0; e < 64; e++) {
        if (g_staffSlots[e].flags && g_staffSlots[e].job == gi)
            g_staffSlots[e].flags |= 2;
    }

    int now = g_clock;
    g_golferPool[gi].ballX = 0;
    g_golferPool[gi].holeStrokes = 0;
    if (now > g_golferPool[gi].arrivedAt)
        g_allHolesArray[g_golferPool[gi].holeIdx].playTime += (now - g_golferPool[gi].arrivedAt) / 2;
    g_golferPool[gi].arrivedAt = now;
    g_golferPool[gi].flags &= 0xfbdfbbff;
    g_golferPool[gi].club = 0;

    int hole;

    if (!(g_runModeFlags & GAME_TOURNAMENT) && (g_golferPool[gi].talents >> 4) != 0
        && (g_golferPool[g_golferPool[gi].partner].talents >> 4) != 0
        && (g_golferPool[g_golferPool[gi].partner].holeIdx > g_golferPool[gi].holeIdx
            || g_golferPool[g_golferPool[gi].partner].holeIdx == 0)) {
        int partner = g_golferPool[gi].partner;
        int myStrokes = 0;
        int partnerStrokes = 0;
        int parStrokes = 0;
        hole = g_golferPool[gi].holeIdx;
        for (int i = 1; i <= hole; i++) {
            myStrokes += g_golferPool[gi].holeScore[i];
            partnerStrokes += g_golferPool[partner].holeScore[i];
            parStrokes += g_allHolesArray[i].par;
        }

        int odd = gi | 1;
        int even = gi & PAIR_EVEN_MASK;
        signed char *evenScore = &g_golferPool[even].holeScore[hole];
        g_printBuffer[0] = 0;

        if (g_golferPool[odd].holeScore[hole] < *evenScore) {
            int amount = (g_proMatchStakeLvl * EXHIBITION_STAKE) / MONEY_UNIT;
            AppendGolferName(odd, 0);
            strcat(g_printBuffer, " wins hole #");
            strcat(g_printBuffer, itoa(hole, g_numberBuf, 10));
            strcat(g_printBuffer, " by a score of ");
            strcat(g_printBuffer, itoa(g_golferPool[odd].holeScore[hole], g_numberBuf, 10));
            strcat(g_printBuffer, " to ");
            strcat(g_printBuffer, itoa(*evenScore, g_numberBuf, 10));
            strcat(g_printBuffer, "! Collect \xa7");
            FormatNumberWithCommas(amount * MONEY_UNIT);
            strcat(g_printBuffer, ".");
            g_annualStatsRecords[g_careerPlayYear % LEDGER_YEARS].v[LEDGER_OTHER] += amount;
            AddMoneyPopup(amount, g_golferPool[odd].x, g_golferPool[odd].y, -1);
            g_gameCashHundreds += amount;
            ShowStoryMessage(COLOR_DARK_GREY, MESSAGE_NORMAL, g_watchedPlayerIdx);
            ConfigureAndPlaySound(SOUND_WIN, SOUND_FULL_VOLUME, 0, 0, 0);
        }
        if (g_golferPool[odd].holeScore[hole] > *evenScore) {
            int amount = (g_proMatchStakeLvl * -EXHIBITION_STAKE) / MONEY_UNIT;
            AppendGolferName(even, 0);
            strcat(g_printBuffer, " wins hole #");
            strcat(g_printBuffer, itoa(hole, g_numberBuf, 10));
            strcat(g_printBuffer, " by a score of ");
            strcat(g_printBuffer, itoa(*evenScore, g_numberBuf, 10));
            strcat(g_printBuffer, " to ");
            strcat(g_printBuffer, itoa(g_golferPool[odd].holeScore[hole], g_numberBuf, 10));
            strcat(g_printBuffer, ". Pay \xa7");
            FormatNumberWithCommas(abs(amount) * MONEY_UNIT);
            strcat(g_printBuffer, ".");
            g_annualStatsRecords[g_careerPlayYear % LEDGER_YEARS].v[LEDGER_OTHER] += amount;
            AddMoneyPopup(amount, g_golferPool[odd].x, g_golferPool[odd].y, -1);
            g_gameCashHundreds += amount;
            ShowStoryMessage(COLOR_DARK_GREY, MESSAGE_NORMAL, g_watchedPlayerIdx);
            ConfigureAndPlaySound(SOUND_LOSE, SOUND_FULL_VOLUME, 0, 0, 0);
        }

        if (g_allHolesArray[g_golferPool[gi].holeIdx + 1].par == 0) {
            strcat(g_printBuffer, " At the end of the match, ");
            int stake = (g_proMatchStakeLvl * EXHIBITION_STAKE) / MONEY_UNIT;
            if ((g_golferPool[odd].talents & TALENT_MATCH_PLAY) != 0) {
                if ((gi == even && myStrokes < partnerStrokes) || (gi == odd && partnerStrokes < myStrokes)) {
                    g_gameCashHundreds -= stake;
                    AddMoneyPopup(-stake, g_golferPool[gi].x, g_golferPool[gi].y, -1);
                    strcat(g_printBuffer, "you lose \xa7");
                    FormatNumberWithCommas(stake * MONEY_UNIT);
                    strcat(g_printBuffer, " as ");
                    g_annualStatsRecords[g_careerPlayYear % LEDGER_YEARS].v[LEDGER_OTHER] -= stake;
                    ConfigureAndPlaySound(SOUND_LOSE, SOUND_FULL_VOLUME, 0, 0, 0);
                    g_proMatchStakeLvl--;
                }
                if (myStrokes == partnerStrokes) {
                    g_proMatchStakeLvl--;
                    strcat(g_printBuffer, "the score is tied.");
                }
                if ((gi == even && myStrokes > partnerStrokes) || (gi == odd && partnerStrokes > myStrokes)) {
                    g_gameCashHundreds += stake;
                    AddMoneyPopup(stake, g_golferPool[gi].x, g_golferPool[gi].y, -1);
                    g_annualStatsRecords[g_careerPlayYear % LEDGER_YEARS].v[LEDGER_OTHER] += stake;
                    strcat(g_printBuffer, "you win \xa7");
                    FormatNumberWithCommas(stake * MONEY_UNIT);
                    strcat(g_printBuffer, " as ");
                    ConfigureAndPlaySound(SOUND_WIN, SOUND_FULL_VOLUME, 0, 0, 0);
                    g_winningPtsTotal++;
                    TriggerAccomplishment(ACCOMP_FIRST_MATCH_WIN, g_golferPool[gi].x, g_golferPool[gi].y);
                    NotifyEvent(EVENT_MATCH_WON,
                        gi == even ? g_golferPool[gi].rosterIndex : g_golferPool[g_golferPool[gi].partner].rosterIndex);
                }
            }

            g_watchedPlayerIdx = -1;
            AppendGolferName(g_golferPool[gi].partner, 0);
            const char *tail;
            if (myStrokes == partnerStrokes) {
                strcat(g_printBuffer, " and ");
                AppendGolferName(gi, 0);
                strcat(g_printBuffer, " are tied at ");
                if (myStrokes == parStrokes) {
                    tail = "even par.";
                } else {
                    strcat(g_printBuffer, itoa(abs(myStrokes - parStrokes), g_numberBuf, 10));
                    tail = myStrokes < parStrokes ? " under par." : " over par.";
                }
            } else {
                const char *lead;
                if (myStrokes > partnerStrokes) {
                    if ((g_golferPool[odd].talents & TALENT_MATCH_PLAY)
                        && (g_golferPool[odd ^ 1].talents & TALENT_MATCH_PLAY))
                        ConfigureAndPlaySound(SOUND_APPLAUSE_GOOD + (gi == odd), SOUND_FULL_VOLUME, 0, 0, 0);
                    lead = " leads ";
                } else {
                    if ((g_golferPool[odd].talents & TALENT_MATCH_PLAY)
                        && (g_golferPool[odd ^ 1].talents & TALENT_MATCH_PLAY))
                        ConfigureAndPlaySound(SOUND_APPLAUSE_GOOD + (gi != odd), SOUND_FULL_VOLUME, 0, 0, 0);
                    lead = " trails ";
                }
                strcat(g_printBuffer, lead);
                AppendGolferName(gi, 0);
                strcat(g_printBuffer, " by ");
                int diff = abs(myStrokes - partnerStrokes);
                strcat(g_printBuffer, itoa(diff, g_numberBuf, 10));
                tail = diff > 1 ? " shots." : " shot.";
            }
            strcat(g_printBuffer, tail);
            ShowStoryMessage(COLOR_DARK_GREY, MESSAGE_URGENT, g_watchedPlayerIdx);
        }
    }

    if (!(g_runModeFlags & GAME_TOURNAMENT)
        && (g_golferPool[gi].holeIdx == LAST_HOLE || g_allHolesArray[g_golferPool[gi].holeIdx + 1].par == 0)) {
        int cardScore = 0;
        int overPar = 0;
        int upgradePoints = 0;
        int parTotal = 0;
        int strokes = 0;
        int complete = 1;
        hole = g_golferPool[gi].holeIdx;
        for (int i = 1; i < HOLE_SLOTS; i++) {
            if (i <= hole) {
                cardScore += g_golferPool[gi].holeScore[i];
                strokes += g_golferPool[gi].holeScore[i];
                parTotal += g_allHolesArray[i].par;
                overPar += g_golferPool[gi].holeScore[i] - g_allHolesArray[i].par;
                upgradePoints += g_golferHistoryPerCharacter[g_golferPool[gi].typeIdx].perHoleFlags[i] & HOLE_MARKS;
            } else {
                cardScore += UNPLAYED_STROKES;
                overPar += 1;
            }
            if (i < g_nextOpenHoleIndex && g_golferPool[gi].holeScore[i] == 0)
                complete = 0;
        }

        g_printBuffer[0] = 0;
        if (complete) {
            for (int k = 0; k < BEST_SCORES; k++) {
                if (g_bestTenScoresTable[k] == 0) {
                    g_bestTenScoresTable[k] = strokes;
                    g_printBuffer[0] = 0;
                    AppendGolferName(gi, 0);
                    StoreStringInPool(k + SLOT_BEST_NAMES);
                    break;
                }
                if (strokes < g_bestTenScoresTable[k]) {
                    for (int j = BEST_SCORES - 2; j >= k; j--) {
                        if (g_bestTenScoresTable[j] != 0) {
                            g_printBuffer[0] = 0;
                            g_bestTenScoresTable[j + 1] = g_bestTenScoresTable[j];
                            AppendPooledString(j + SLOT_BEST_NAMES);
                            StoreStringInPool(j + SLOT_BEST_NAMES + 1);
                        }
                    }
                    g_bestTenScoresTable[k] = strokes;
                    g_printBuffer[0] = 0;
                    AppendGolferName(gi, 0);
                    StoreStringInPool(k + SLOT_BEST_NAMES);
                    break;
                }
            }
        }

        if (g_nextOpenHoleIndex > 2 && g_golferPool[gi].holeScore[1] != 0 && g_selectedGolferIndex == -1 && complete
            && (g_bestTenScoresTable[0] == 0 || strokes < g_bestTenScoresTable[0]) && strokes <= parTotal) {
            g_selectedGolferIndex = gi | SIMFOTO_NOT_SAVED;
            AppendGolferName(gi, 0);
            StoreStringInPool(SLOT_BEST_NAMES);
            strcat(g_printBuffer, " has just set a new course record of ");
            strcat(g_printBuffer, itoa(strokes, g_numberBuf, 10));
            strcat(g_printBuffer, " strokes for ");
            strcat(g_printBuffer, itoa(g_nextOpenHoleIndex - 1, g_numberBuf, 10));
            strcat(g_printBuffer, " holes! ");
            upgradePoints++;
        }

        if (g_golferPool[gi].enjoyment >= ENJOYMENT_FOR_UPGRADE(g_level))
            upgradePoints++;
        if (cardScore < g_golferHistoryPerCharacter[g_golferPool[gi].typeIdx].bestScore
            || g_golferHistoryPerCharacter[g_golferPool[gi].typeIdx].bestScore == 0)
            g_golferHistoryPerCharacter[g_golferPool[gi].typeIdx].bestScore = cardScore;
        if (g_golferHistoryPerCharacter[g_golferPool[gi].typeIdx].avgScore == 0)
            g_golferHistoryPerCharacter[g_golferPool[gi].typeIdx].avgScore = overPar;
        else
            g_golferHistoryPerCharacter[g_golferPool[gi].typeIdx].avgScore =
                (g_golferHistoryPerCharacter[g_golferPool[gi].typeIdx].avgScore + overPar) / 2;
        if ((g_golferHistoryPerCharacter[g_golferPool[gi].typeIdx].perHoleFlags[g_nextOpenHoleIndex - 1] & HOLE_MARKS)
            || g_level == 0 || g_careerPlayYear == 0 || (g_gameCashHundreds < LOW_FUNDS && g_waitListLength <= 0))
            upgradePoints++;
        if (g_simOpenFacilityLvl[SCENERY_SWIM_CLUB] != 0)
            upgradePoints += g_simOpenFacilityLvl[SCENERY_SWIM_CLUB];
        if (g_golferPool[gi].persona == 0
            && upgradePoints >= (1 << (g_golferHistoryPerCharacter[g_golferPool[gi].typeIdx].flags & MEMBERSHIP_MASK))
            && (g_golferHistoryPerCharacter[g_golferPool[gi].typeIdx].flags & MEMBERSHIP_MASK) < TOP_TIER) {
            g_golferPool[gi].flags |= GOLFER_UPGRADE_DUE;
            g_golferPool[gi].waitTimer = -UPGRADE_PAUSE;
        }
    }

    g_golferPool[gi].holeIdx++;
    if (g_runModeFlags & (GAME_TOURNAMENT | GAME_CHAMPIONSHIP)) {
        if (g_allHolesArray[g_golferPool[gi].holeIdx].par == 0)
            g_golferPool[gi].holeIdx = 1;
        g_golferPool[gi].seq += 2 * g_nextOpenHoleIndex;
        if (g_golferPool[gi].holeScore[g_golferPool[gi].holeIdx] != 0)
            ProcessVIPGolferEvent(gi);
    } else {
        if (g_allHolesArray[g_golferPool[gi].holeIdx].par == 0 || g_golferPool[g_golferPool[gi].partner].holeIdx == 0
            || g_golferPool[g_golferPool[gi].partner].holeIdx == CLUBHOUSE_HOLE) {
            ProcessVIPGolferEvent(gi);
            return;
        }
    }

    if (g_allHolesArray[g_golferPool[gi].holeIdx + 1].par == 0) {
        int vip = g_golferPool[gi].persona & PERSONA_KIND;
        if (vip == PERSONA_COMMISSIONER || vip == PERSONA_CEO) {
            if (vip == PERSONA_CEO) {
                strcpy(g_printBuffer, "Corporate CEO ");
                AppendGolferName(gi, 0);
            } else {
                strcpy(g_printBuffer, "County commissioner ");
                AppendGolferName(gi, 0);
            }
            strcat(g_printBuffer, " is playing the last hole on your course. He was last heard to mutter, '");
            AppendPartnerNote(g_golferPool[gi].enjoyment, 0);
            strcat(g_printBuffer, "'");
            ShowStoryMessage(COLOR_TEAL, MESSAGE_URGENT, gi);
        }
    }

    g_golferPool[gi].enjoymentFloor = g_golferPool[gi].enjoymentFloor * 7 / 8 + g_golferPool[gi].enjoyment;
    if (g_golferPool[gi].flags & GOLFER_STORY_LEAD)
        g_golferPool[gi].holeResult[g_golferPool[gi].holeIdx] = g_golferPool[gi].enjoymentFloor;
    else
        g_golferPool[gi].enjoymentFloor = g_golferPool[gi].enjoyment;
    g_golferPool[gi].holeEnjoyment[g_golferPool[gi].holeIdx - 1] = g_golferPool[gi].enjoyment;
    g_golferPool[gi].attitude -= Sign(g_golferPool[gi].attitude);
    if ((g_golferPool[gi].persona & PERSONA_KIND) != PERSONA_COMMISSIONER) {
        g_golferPool[gi].enjoyment -=
            (short)((g_golferHistoryPerCharacter[g_golferPool[gi].typeIdx].count + g_golferPool[gi].holeIdx + 6)
                    * (g_golferPool[gi].enjoyment + g_level - 1) * (g_level + 1)
                    / ((5 * g_simOpenFacilityLvl[SCENERY_RESORT_HOTEL] + 15) * 8));
    } else {
        g_golferPool[gi].enjoyment -=
            (short)((g_golferPool[gi].holeIdx + 6) * g_golferPool[gi].enjoyment * g_level / 160);
    }
}

void UpdateAllGolfers(void)
{
    int i;
    int slotIdx, wd, bidx;
    unsigned short cs;
    int ballTileX, ballTileY, terrainType, posTileX, posTileY, period;
    int nearSpecial, k, terrType, pairBehind, pairCloser;
    int entryFlags, screenAX, screenAY, screenBX, screenBY;
    int bldg;
    int angleSelf, queuedAhead, dir2, distToTarget, flg, hn, mult, restX, restY, thr2, tileAtPartnerTarget, wantSwap,
        wx, wy;
    int blockedFlag, partnerBlocked, tileIdx, prevDir, aheadCount;
    int colOffset;
    int speedIdx;
    int s, p;
    int aim;
    int amount;
    int amt2;
    int amt;
    int ang2;
    int ang3;
    int ang;
    int anim;
    int band;
    int bestScore;
    int bit;
    int bounceEdge;
    int bound;
    int d;
    int dir;
    int dirOctant;
    int dist;
    int speed;
    int jitter;
    int dx;
    int dxs2;
    int dy, ody;
    int dys2;
    int edgeFlags;
    int groundH;
    int inc;
    int j;
    int other;
    int kind;
    int lieResist = 0;
    int mask;
    int ok;
    int onScreen;
    int partner;
    int pinWX;
    int pinWY;
    int prevTerrain;
    int price2;
    int price;
    int px;
    int py;
    int rough;
    int prevKind;
    int score;
    int slope;
    int newGround;

    int spinScale;
    int startYards;
    int statIdx;
    int step;
    int probeDist;
    int stepX;
    int stepY;
    int subX;
    int subY;
    int tx;
    int ty;
    int walkAngle;
    int walkLen;
    int yards;
    short tf;
    int bounceSnd;

    for (i = 0; i < 0x98; i++) {

        if (g_golferPool[i].holeIdx == 0)
            continue;
        g_golferPassScratchWord = 0;
        if (g_golferPool[i].holeIdx == -1) {
            if (g_golferPool[i].talkTimer == 0 && g_rng.NextInt(100) == 0) {
                g_golferPool[i].x = (g_buildingTable[0].row << 10) + 0x600;
                g_golferPool[i].y = (g_buildingTable[0].col << 10) + 0x600;
            }
            if (g_golferPool[i].talkTimer != 0 && g_rng.NextInt(2) != 0)
                g_golferPool[i].talkTimer--;
            continue;
        }

        bounceEdge = 0;
        slotIdx = -1;
        for (s = 0; s < 16; s++) {

            if (g_ballPathSlots[s].golferId == i)
                slotIdx = s;
        }
        if (slotIdx != -1 && (g_golferPool[i].flags & GOLFER_BALL_MOVING) == 0) {
            g_ballPathSlots[slotIdx].golferId = -1;
            slotIdx = -1;
        }

        if (g_golferPool[i].talkTimer != 0 && (g_clock & 7) == 0) {
            if (--g_golferPool[i].talkTimer == 1 && g_rng.NextInt(6) >= g_level + 1)
                GetGolferMoodIndex(i);
            if (g_golferPool[i].talkTimer == 6 && g_golferPool[i].msgLine == LINE_STORY && g_golferPool[i].screenX != -1
                && !(g_golferPool[i].flags & GOLFER_STORY_LEAD) && g_golferPool[i].storyChapter == STORY_LAST_CHAPTER)
                g_selectedGolferIndex = i;
        }

        if ((g_golferPool[i].flags & GOLFER_STORY_LEAD)
            && g_golferPool[i].holeIdx == g_golferPool[g_golferPool[i].partner].holeIdx
            && !(g_golferPool[i].holeIdx == CLUBHOUSE_HOLE && g_golferPool[i].storyChapter == 0)
            && !(g_golferPool[i].flags & GOLFER_STORY_ADVANCED) && ((g_clock - i * 31) & 0x3f) == 0
            && g_golferPool[i].activity >= ACTIVITY_IDLE && g_golferPool[i].activity != ACTIVITY_STEP_UP
            && g_golferPool[g_golferPool[i].partner].activity >= ACTIVITY_IDLE
            && g_golferPool[g_golferPool[i].partner].activity != ACTIVITY_STEP_UP
            && g_clock - g_curPreviousConvoClock > 0x96 && g_reportScreenId == 0
            && Math_Distance2D(g_golferPool[i].x - g_golferPool[g_golferPool[i].partner].x,
                   g_golferPool[i].y - g_golferPool[g_golferPool[i].partner].y)
                   < 0x3000 / (g_golferPool[i].storyChapter + 6)
            && g_cameraZoom >= DEFAULT_ZOOM && g_golferPool[i].screenX > 0x32 && g_golferPool[i].screenX < 0x2ee
            && g_golferPool[i].screenY > 0x32 && g_golferPool[i].screenY < 0x1c2) {
            if (InLandmarkInfluence(g_golferPool[i].x, g_golferPool[i].y, 8))
                g_golferPool[g_golferPool[i].partner].holeResult[g_golferPool[g_golferPool[i].partner].holeIdx] =
                    STORY_RESULT_SPECIAL;
            AdvanceStoryChapter(i, 0);
            g_golferPool[i].flags |= GOLFER_STORY_ADVANCED;
            g_golferPool[g_golferPool[i].partner].flags |= GOLFER_STORY_ADVANCED;
        }

        cs = g_golferPool[i].slotParams[0];
        mask = 0x1ff;
        if ((cs & MOOD_MASK) == 0)
            mask = 0xff;
        if (cs & MOOD_PLEASED)
            mask >>= 2;
        if (g_golferPool[i].walkLeft > 0)
            mask >>= 1;
        if ((mask & (g_clock + i * 33)) == 0 && (g_golferPool[i].slotKind[0] & MSG_LINE_MASK) != 0x0b
            && (g_golferPool[i].slotKind[1] & MSG_LINE_MASK) != 0x0b) {
            int nx, ny;
            wd = g_rng.NextInt(5);
            wd = (g_golferPool[i].dir + wd - 2) & 7;
            nx = (g_golferPool[i].x >> 10) + g_dirRowStep[wd];
            ny = (g_golferPool[i].y >> 10) + g_dirColStep[wd];
            tileIdx = nx * 50 + ny;
            if (g_gameTileTerrainKind[tileIdx] == TERRAIN_BUILDING) {
                bidx = FindBuildingAtTile(nx, ny);
                if (g_buildingTable[bidx].kind == SCENERY_FLOWER_BED
                    || g_buildingTable[bidx].kind == SCENERY_LANDMARK) {
                    if (g_buildingTable[bidx].kind != SCENERY_FLOWER_BED
                        && g_buildingTable[bidx].count >= LAST_FIXED_LANDMARK) {
                        SetGolferAction(i, 0x14, ny * 50 + nx);
                    } else {
                        SetGolferAction(i, 0x0b, ny * 50 + nx);
                    }
                }
            }

            if ((g_tileExtra[tileIdx] & TILE_FLOWERS)
                && (GetGolferMoodIndex(i) == 2 || (g_tileExtra[tileIdx] & TILE_WEEDS)))
                SetGolferAction(i, (g_tileExtra[tileIdx] & TILE_WEEDS) ? 0x14 : 0x0b, ny * 50 + nx);
        }

        if ((g_golferPool[i].flags & GOLFER_PRO_MATCH) != 0) {
            g_staffSlots[0].posX = g_golferPool[i].x;
            g_staffSlots[0].posY = g_golferPool[i].y;
            g_staffSlots[0].dir = g_golferPool[i].dir;
        }

        ballTileX = g_golferPool[i].ballX >> 10;
        ballTileY = g_golferPool[i].ballY >> 10;
        if (IsPositionInvalid(ballTileX, ballTileY) != 0) {
            terrainType = TERRAIN_OUT_OF_BOUNDS;
        } else {
            terrainType = g_gameTileTerrainKind[ballTileX * 50 + ballTileY];
        }

        posTileX = g_golferPool[i].x >> 10;
        posTileY = g_golferPool[i].y >> 10;
        period = g_golferPool[i].walkLeft ? 0x78 : 0xa0;
        if (((g_clock + i * 37) % period) == 0 && g_golferPool[i].holeIdx != CLUBHOUSE_HOLE) {
            if (g_golferPool[i].holeIdx > 1 && g_golferPool[i].holeScore[0] == 0 && g_golferPool[i].clubsUsed == 0
                && g_rng.NextInt(g_nextOpenHoleIndex / 2) == 0) {
                SetGolferAction(i, 0x3f, 0x14);
            }

            nearSpecial = 0;
            if (g_squareTypeTable[g_gameTileTerrainKind[posTileX * 50 + posTileY]].kind == 7
                || g_squareTypeTable[g_gameTileTerrainKind[posTileX * 50 + posTileY]].kind == 8
                || g_squareTypeTable[g_gameTileTerrainKind[posTileX * 50 + posTileY]].kind == 0x0e)
                nearSpecial = 1;
            for (k = 0; k < 8; k++) {
                int nx = posTileX + g_dirRowStep[k];
                int ny = posTileY + g_dirColStep[k];
                if (g_squareTypeTable[g_gameTileTerrainKind[nx * 50 + ny]].kind == 7
                    || g_squareTypeTable[g_gameTileTerrainKind[nx * 50 + ny]].kind == 8
                    || g_squareTypeTable[g_gameTileTerrainKind[nx * 50 + ny]].kind == 0x0e)
                    nearSpecial = 1;
            }

            if ((g_currentThemeId == THEME_LINKS && g_rng.NextInt(2) != 0) || nearSpecial != 0) {
                if (g_golferPool[i].holeIdx > 2 && g_rng.NextInt(2) == 0) {
                    g_golferPool[i].hunger++;
                    if (g_golferPool[i].hunger >= GOLFER_NEED_LEVEL) {
                        if ((g_golferPool[i].hunger % 4) == 0) {
                            if ((g_golferPool[i].persona & PERSONA_KIND) != PERSONA_ROSTER) {
                                SetGolferAction(i, 0x0f, 0x14);
                            } else {
                                g_golferPool[i].hunger = GOLFER_NEED_LEVEL;
                            }
                        }
                    }
                }
            } else {
                g_golferPool[i].thirst++;
                if (g_golferPool[i].thirst >= GOLFER_NEED_LEVEL) {
                    if ((g_golferPool[i].thirst % 4) == 0) {
                        if ((g_golferPool[i].persona & PERSONA_KIND) != PERSONA_ROSTER) {
                            SetGolferAction(i, 0x0e, 0x14);
                        } else {
                            g_golferPool[i].thirst = GOLFER_NEED_LEVEL;
                        }
                    }
                }
            }
        }

        nearSpecial = 0;
        tileIdx = posTileX * 50 + posTileY;
        terrType = g_gameTileTerrainKind[posTileX * 50 + posTileY];
        if (g_golferPool[i].ballSpeed == 0)
            g_golferPool[i].flags &= ~GOLFER_BALL_MOVING;

        if (g_golferPool[i].waitTimer < 0) {
            if ((g_clock & 1) != 0)
                g_golferPool[i].waitTimer++;
            if ((g_golferPool[i].flags & GOLFER_BALL_MOVING) != 0)
                goto moveBall;
            continue;
        }

        if ((g_golferPool[i].flags & GOLFER_UPGRADE_DUE) != 0)
            ProcessMembershipUpgrade(i);

        entryFlags = g_golferPool[i].flags;
        g_golferPool[i].flags = entryFlags & ~GOLFER_IN_CART;
        if (g_golferPool[i].holeStrokes >= 0x0a) {
            ProcessHoleCompletion(i);
            continue;
        }

        if ((g_runModeFlags & GAME_DEBUG_OVERLAY) != 0
            && !(g_golferPool[i].ballSpeed == 0 && terrainType == TERRAIN_GREEN)) {
            if (WorldToScreen(g_golferPool[i].shotFromX, g_golferPool[i].shotFromY, &screenAX, &screenAY, 0) != 0) {
                if ((g_golferPool[i].flags & GOLFER_EXACT_AIM) == 0) {
                    onScreen =
                        TileToScreenCached(g_golferPool[i].targetX, g_golferPool[i].targetY, &screenBX, &screenBY);
                } else {
                    onScreen = WorldToScreen(g_golferPool[i].targetX << 10, g_golferPool[i].targetY << 10, &screenBX,
                        &screenBY, 0);
                }
                if (onScreen != 0) {
                    g_simScreen.line(screenAX, screenAY, screenBX, screenBY, COLOR_LIGHT_GREY);
                    TileToScreenCached(g_allHolesArray[g_golferPool[i].holeIdx].aimX,
                        g_allHolesArray[g_golferPool[i].holeIdx].aimY, &screenAX, &screenAY);
                }
            }
        }

        pairBehind = 0;
        blockedFlag = 1;
        if (g_golferPool[g_golferPool[i].partner].holeIdx == 0 && g_golferPool[i].holeStrokes == 0
            && g_golferPool[i].holeIdx != CLUBHOUSE_HOLE) {
            ProcessVIPGolferEvent(i);
        }

        if (g_golferPool[g_golferPool[i].partner].holeIdx != 0
            && g_golferPool[g_golferPool[i].partner].holeIdx == g_golferPool[i].holeIdx
            && (g_golferPool[i].flags & 0x400) != 0 && (g_golferPool[g_golferPool[i].partner].flags & 0x400) == 0) {
            pairBehind = 1;
        }

        pairCloser = 0;
        if ((g_golferPool[i].flags & 0x400) != 0 && g_golferPool[i].ballX != 0
            && g_golferPool[g_golferPool[i].partner].ballX != 0
            && g_golferPool[i].holeIdx == g_golferPool[g_golferPool[i].partner].holeIdx) {
            pairCloser =
                (CalculateDistanceInYards(g_golferPool[i].ballX, g_golferPool[i].ballY,
                     g_allHolesArray[g_golferPool[i].holeIdx].aimX, g_allHolesArray[g_golferPool[i].holeIdx].aimY)
                    < CalculateDistanceInYards(g_golferPool[g_golferPool[i].partner].ballX,
                        g_golferPool[g_golferPool[i].partner].ballY,
                        g_allHolesArray[g_golferPool[g_golferPool[i].partner].holeIdx].aimX,
                        g_allHolesArray[g_golferPool[g_golferPool[i].partner].holeIdx].aimY));
        }

        if (g_golferPool[i].ballX == 0 || g_golferPool[i].shotStep == 0 || pairBehind != 0
            || (g_golferPool[i].flags & GOLFER_WALKED_OFF) != 0) {

            queuedAhead = 0;
            bldg = -1;
            wantSwap = 0;
            if ((g_runModeFlags & GAME_TOURNAMENT) != 0 || (g_golferPool[i].persona & PERSONA_KIND) == PERSONA_ROSTER) {
                if (g_golferPool[i].enjoyment < 0)
                    g_golferPool[i].enjoyment = 0;
            }

            if (g_golferPool[i].talkTimer == 0 && (g_golferPool[i].flags & GOLFER_WALKED_OFF) == 0
                && g_golferPool[i].enjoyment < 0) {
                g_printBuffer[0] = '\0';
                AppendGolferName(i, 0);

                {
                    switch (g_golferPool[i].msgLine & MSG_LINE_MASK) {
                    case 4: strcat(g_printBuffer, " will be looking for a tougher course after "); break;
                    case 9: strcat(g_printBuffer, " has punched out another golfer at "); break;
                    case 21: strcat(g_printBuffer, " has insulted another golfer at "); break;
                    case 12:
                        strcat(g_printBuffer, " has wrapped ");
                        strcat(g_printBuffer, IsGolferMale(i) ? "his" : "her");
                        strcat(g_printBuffer, " club around a tree on ");
                        break;
                    case 13:
                        strcat(g_printBuffer, " has thrown ");
                        strcat(g_printBuffer, IsGolferMale(i) ? "his" : "her");
                        strcat(g_printBuffer, " clubs into the lake on ");
                        break;
                    case 15: strcat(g_printBuffer, " is too hungry to keep playing after "); break;
                    case 14: strcat(g_printBuffer, " is too thirsty to play any more after "); break;
                    case 26: strcat(g_printBuffer, " is too tired to continue after "); break;
                    case 8:
                    case 23:
                    case 30: strcat(g_printBuffer, " thinks your course needs improvement after "); break;
                    default: strcat(g_printBuffer, " is leaving the course in disgust after ");
                    }

                    if (g_golferPool[i].holeIdx < CLUBHOUSE_HOLE) {
                        strcat(g_printBuffer, "hole ");
                        strcat(g_printBuffer, itoa(g_golferPool[i].holeIdx, g_numberBuf, 10));
                    } else {
                        strcat(g_printBuffer, "today's round");
                    }

                    strcat(g_printBuffer, "!");

                    if ((g_golferHistoryPerCharacter[g_golferPool[i].typeIdx].flags & MEMBERSHIP_MASK) > 1) {
                        strcat(g_printBuffer, " ");
                        AppendGolferName(i, 0);
                        strcat(g_printBuffer, " resigns ");
                        strcat(g_printBuffer, IsGolferMale(i) ? "his" : "her");
                        strcat(g_printBuffer, " membership.");
                        g_golferHistoryPerCharacter[g_golferPool[i].typeIdx].flags = 0;
                    }

                    g_reportScreenId = 0;
                    g_golferHistoryPerCharacter[g_golferPool[i].typeIdx].perHoleFlags[g_golferPool[i].holeIdx] |=
                        HOLE_QUIT;
                    g_golferHistoryPerCharacter[g_golferPool[i].typeIdx].finishedStoryId = STORY_NONE;

                    ShowStoryMessage(COLOR_BLACK, 0, i);
                    ConfigureAndPlaySound(0x29, SOUND_FULL_VOLUME, 0, 0, 0);

                    hn = g_golferPool[i].holeIdx;
                    g_golferPool[i].waitTimer = -99;
                    g_golferPool[i].activity = ACTIVITY_ANNOYED;
                    g_allHolesArray[hn].quitCount++;
                    g_golferPool[i].holeIdx = CLUBHOUSE_HOLE;
                    g_golferPool[i].ballX = 0;
                    g_golferPool[i].flags = (g_golferPool[i].flags & ~0x10000) | GOLFER_WALKED_OFF;
                    g_golferPool[i].fatigue = 0;
                    g_golferPool[i].attitude = -3;

                    if (g_golferPool[i].flags & GOLFER_PRO_MATCH)
                        g_watchedPlayerIdx = -1;
                }
            }

            if (g_golferPool[i].flags & GOLFER_WALKED_OFF) {
                if (((g_clock + i * 35) % 100) == 0) {
                    SetGolferAction(i, 0x23, 0x14);
                    g_golferPool[i].activity = ACTIVITY_ANNOYED;
                    if (g_rng.NextInt(2))
                        g_golferPool[i].flags |= GOLFER_ALT_REACTION;
                    else
                        g_golferPool[i].flags &= ~GOLFER_ALT_REACTION;
                    g_golferPool[i].waitTimer = -99;
                    if (--g_golferPool[i].enjoyment < -10)
                        g_golferPool[i].holeIdx = 0;
                    continue;
                } else {
                    wx = (g_allHolesArray[CLUBHOUSE_HOLE].teeX << 10) + 0x200;
                    wy = (g_allHolesArray[CLUBHOUSE_HOLE].teeY << 10) + 0x200;
                }
            } else {

                if (g_golferPool[i].holeIdx == CLUBHOUSE_HOLE
                    && ((posTileX == g_allHolesArray[CLUBHOUSE_HOLE].teeX
                            && posTileY == g_allHolesArray[CLUBHOUSE_HOLE].teeY)
                        || (g_golferPool[i].flags & GOLFER_PRO_MATCH) || g_golferPool[i].screenX == -1)) {
                    g_golferPool[i].talkTimer = 0;
                    g_golferPool[i].holeIdx = 0;
                    if ((g_golferPool[i].flags & GOLFER_PRO_MATCH)
                        || (g_golferPool[g_golferPool[i].partner].flags & GOLFER_PRO_MATCH)) {
                        if (g_golferPool[g_golferPool[i].partner].holeIdx != 0
                            && g_golferPool[g_golferPool[i].partner].holeIdx != CLUBHOUSE_HOLE)
                            continue;
                        if (i == g_watchedPlayerIdx)
                            g_watchedPlayerIdx = -1;
                    }
                    ClearGolferSlotAndNamedScenery(i);
                    continue;
                }

                if (!g_allHolesArray[g_golferPool[i].holeIdx].par)
                    g_golferPool[i].holeIdx = CLUBHOUSE_HOLE;
                if (!g_golferPool[i].holeStrokes) {
                    for (j = 0; j < 152; j++) {
                        if (g_golferPool[j].holeIdx <= 0)
                            continue;
                        if (j == i)
                            continue;
                        if (g_golferPool[j].holeIdx != g_golferPool[i].holeIdx)
                            continue;
                        if (g_golferPool[j].holeStrokes)
                            continue;

                        if (g_nextGolferOrder - g_golferPool[j].seq < g_nextGolferOrder - g_golferPool[i].seq)
                            continue;
                        if (j == g_golferPool[i].partner)
                            continue;
                        if (i == g_watchedPlayerIdx || g_golferPool[i].partner == g_watchedPlayerIdx) {
                            if (!(g_runModeFlags & GAME_ANY_TOURNAMENT)) {
                                ProcessVIPGolferEvent(j);
                                ProcessVIPGolferEvent(j ^ 1);
                                continue;
                            }
                        }
                        queuedAhead++;
                    }
                }

                if (g_golferPool[i].ballX != 0)
                    angleSelf = CalculateAngle((g_allHolesArray[g_golferPool[i].holeIdx].aimX << 10)
                                                   - g_golferPool[i].ballX + 0x200,
                        (g_allHolesArray[g_golferPool[i].holeIdx].aimY << 10) - g_golferPool[i].ballY + 0x200);

                if (g_golferPool[g_golferPool[i].partner].ballX != 0)

                    CalculateAngle((g_allHolesArray[g_golferPool[i].holeIdx].aimX << 10)
                                       - g_golferPool[g_golferPool[i].partner].ballX + 0x200,
                        (g_allHolesArray[g_golferPool[i].holeIdx].aimY << 10)
                            - g_golferPool[g_golferPool[i].partner].ballY + 0x200);

                if (g_golferPool[i].ballX != 0 && g_golferPool[g_golferPool[i].partner].ballX != 0
                    && g_golferPool[i].holeIdx == g_golferPool[g_golferPool[i].partner].holeIdx) {
                    if (CalculateDistanceInYards(g_golferPool[i].ballX, g_golferPool[i].ballY,
                            g_allHolesArray[g_golferPool[i].holeIdx].aimX,
                            g_allHolesArray[g_golferPool[i].holeIdx].aimY)
                        < CalculateDistanceInYards(g_golferPool[g_golferPool[i].partner].ballX,
                            g_golferPool[g_golferPool[i].partner].ballY, g_allHolesArray[g_golferPool[i].holeIdx].aimX,
                            g_allHolesArray[g_golferPool[i].holeIdx].aimY)) {
                        tileAtPartnerTarget = GetTileAtPosition(g_golferPool[g_golferPool[i].partner].ballX,
                            g_golferPool[g_golferPool[i].partner].ballY);

                        if (Math_Distance2D(g_golferPool[i].x - g_golferPool[g_golferPool[i].partner].ballX,
                                g_golferPool[i].y - g_golferPool[g_golferPool[i].partner].ballY)
                                < Math_Distance2D(g_golferPool[i].x - g_golferPool[i].ballX,
                                    g_golferPool[i].y - g_golferPool[i].ballY)
                            || tileAtPartnerTarget == 1) {
                            if (CalculateDistanceInYards(g_golferPool[i].x, g_golferPool[i].y,
                                    g_allHolesArray[g_golferPool[i].holeIdx].aimX,
                                    g_allHolesArray[g_golferPool[i].holeIdx].aimY)
                                > CalculateDistanceInYards(g_golferPool[g_golferPool[i].partner].ballX,
                                    g_golferPool[g_golferPool[i].partner].ballY,
                                    g_allHolesArray[g_golferPool[i].holeIdx].aimX,
                                    g_allHolesArray[g_golferPool[i].holeIdx].aimY))
                                wantSwap = 1;
                        }

                        if (g_squareTypeTable[terrainType].lieGrade > 0
                            || g_squareTypeTable[tileAtPartnerTarget].lieGrade <= 0) {
                            if (g_golferPool[g_golferPool[i].partner].flags & GOLFER_BALL_MOVING)
                                wantSwap = 0;
                        } else
                            wantSwap = 0;

                        if (terrainType != TERRAIN_GREEN) {
                            if (Math_Distance2D(g_golferPool[i].ballX - g_golferPool[g_golferPool[i].partner].ballX,
                                    g_golferPool[i].ballY - g_golferPool[g_golferPool[i].partner].ballY)
                                < 0x600)
                                wantSwap = 1;
                            if (Math_Distance2D(g_golferPool[i].ballX - g_golferPool[g_golferPool[i].partner].x,
                                    g_golferPool[i].ballY - g_golferPool[g_golferPool[i].partner].y)
                                < 0x200)
                                wantSwap = 1;
                        }
                    }
                }

                if (g_golferPool[i].flags & 0x4000)
                    wantSwap = 0;
                if (g_golferPool[i].ballX == 0)
                    wantSwap = i & 1;

                if (terrainType != TERRAIN_GREEN && g_golferPool[g_golferPool[i].partner].holeIdx != 0
                    && ((Math_Distance2D(g_golferPool[i].x - g_golferPool[g_golferPool[i].partner].x,
                             g_golferPool[i].y - g_golferPool[g_golferPool[i].partner].y)
                            * 25)
                           & ~0x3ff)
                           < 0xa000
                    && g_golferPool[g_golferPool[i].partner].ballZ != 0
                    && ((g_golferPool[g_golferPool[i].partner].flags & GOLFER_BALL_MOVING)
                        || g_golferPool[g_golferPool[i].partner].activity < ACTIVITY_WALK
                        || g_golferPool[g_golferPool[i].partner].activity == ACTIVITY_STEP_UP
                        || g_golferPool[g_golferPool[i].partner].activity == ACTIVITY_PLEASED
                        || g_golferPool[g_golferPool[i].partner].activity == ACTIVITY_ANNOYED)) {
                    ang = CalculateAngle(g_golferPool[g_golferPool[i].partner].ballX - g_golferPool[i].x,
                        g_golferPool[g_golferPool[i].partner].ballY - g_golferPool[i].y);
                    if (abs(ang - g_golferPool[g_golferPool[i].partner].ballBearing) < 0x4aaaaaaa) {
                        g_golferPool[i].activity = ACTIVITY_WALK;
                        g_golferPool[i].dir = ((((ang >> 28) & 0xf) + 1) >> 1) & 7;
                        g_golferPool[i].waitTimer = -g_rng.NextInt(4);
                        g_golferPool[i].walkLeft = 0;
                        continue;
                    }
                }

                if (g_golferPool[i].ballX != 0 && pairBehind == 0) {
                    wx = g_golferPool[i].ballX;
                    dir = ((angleSelf >> 0x1c) + 1) >> 1 & 7;
                    wy = g_golferPool[i].ballY;
                    restX = wx;
                    restY = wy;

                    if (wantSwap != 0) {

                        if (terrainType == TERRAIN_GREEN
                            && GetTileAtPosition(g_golferPool[g_golferPool[i].partner].ballX,
                                   g_golferPool[g_golferPool[i].partner].ballY)
                                   == 1) {

                            wx = g_golferPool[i].ballX - (((g_dirRowStep[dir & 7] << 10) + 0x200) / 2);
                            wy = g_golferPool[i].ballY - ((g_dirColStep[dir & 7] << 10) + 0x200) / 2;
                        } else {

                            ang = CalculateAngle((g_allHolesArray[g_golferPool[i].holeIdx].aimX << 10)
                                                     - g_golferPool[g_golferPool[i].partner].ballX + 0x200,
                                (g_allHolesArray[g_golferPool[i].holeIdx].aimY << 10)
                                    - g_golferPool[g_golferPool[i].partner].ballY + 0x200);
                            d = ((ang >> 0x1c) + 1) >> 1 & 7;

                            wx = g_golferPool[g_golferPool[i].partner].ballX
                                 - (g_dirRowStep[(d - ((i & 1) ? 2 : 1)) & 7] << 10);
                            wy = g_golferPool[g_golferPool[i].partner].ballY
                                 - (g_dirColStep[(d - ((i & 1) ? 2 : 1)) & 7] << 10);
                        }

                        if (GetTileAtPosition(wx, wy) == TERRAIN_WATER) {
                            wx = g_golferPool[i].ballX;
                            wy = g_golferPool[i].ballY;
                        }
                    } else {

                        wantSwap = 0;
                        if (terrainType != TERRAIN_GREEN) {
                            wx -= g_dirRowStep[dir] * (((dir & 1) ? 3 : 5) << 6);
                            wy -= g_dirColStep[dir] * (((dir & 1) ? 3 : 5) << 6);
                        } else {
                            wx -= g_dirRowStep[dir] * (((~dir & 1) | 2) << 6);
                            wy -= g_dirColStep[dir] * (((~dir & 1) | 2) << 6);
                        }
                    }
                } else {

                    if (g_golferPool[i].talents <= 4 && g_golferPool[i].talents != 3 && g_golferPool[i].persona == 0) {
                        hn = g_golferPool[i].holeIdx;
                        wx = (g_allHolesArray[hn].secondTeeX << 10) + 0x200;
                        wy = (g_allHolesArray[hn].secondTeeY << 10) + 0x200;
                    } else {
                        hn = g_golferPool[i].holeIdx;
                        wx = (g_allHolesArray[hn].teeX << 10) + 0x200;
                        wy = (g_allHolesArray[hn].teeY << 10) + 0x200;
                    }

                    restX = wx;
                    restY = wy;
                    ang = CalculateAngle((g_allHolesArray[hn].aimX << 10) - wx + 0x200,
                        (g_allHolesArray[hn].aimY << 10) - wy + 0x200);
                    d = ((ang >> 0x1c) + 1) >> 1 & 7;
                    mult = (d & 1) ? 3 : 5;
                    wx -= g_dirRowStep[d] * (mult << 6);
                    wy -= g_dirColStep[d] * (mult << 6);

                    if (queuedAhead != 0) {
                        wx += (-g_dirRowStep[(g_allHolesArray[g_golferPool[i].holeIdx].teeDir + (i & 1)) & 7]) << 10;
                        wy += (-g_dirColStep[(g_allHolesArray[g_golferPool[i].holeIdx].teeDir + (i & 1)) & 7]) << 10;
                    }

                    flg = g_golferPool[i].flags;
                    if ((g_golferPool[i].hunger >= ((flg & 0x2000) ? 4 : 8) - queuedAhead * 2
                            && g_golferPool[i].hunger > 1)
                        || (g_golferPool[i].thirst >= ((flg & 0x1000000) ? 8 : 12) - queuedAhead * 2
                            && g_golferPool[i].thirst > 2)) {
                        bldg = FindNearestBuildingOfType(7, g_golferPool[i].x, g_golferPool[i].y);
                        flg = g_golferPool[i].flags;

                        if (g_closestDistanceFound < ((flg & 0x2000) ? 10 : 8) * 0x400) {
                            flg |= 0x2000;
                            wx = (g_buildingTable[bldg].row << 10) + 0x200;
                            wy = (g_buildingTable[bldg].col << 10) + 0x200;
                            g_golferPool[i].flags = flg;
                        } else {
                            flg &= ~0x2000;
                            g_golferPool[i].flags = flg;
                            bldg = -1;
                        }
                    } else {
                        flg &= ~0x2000;
                        g_golferPool[i].flags = flg;
                    }

                    if (bldg == -1) {
                        short fatigue = g_golferPool[i].fatigue;

                        if ((fatigue / 20 >= 6 - queuedAhead || (g_golferPool[i].flags & 0x1000) != 0) && fatigue != 0
                            && g_golferPool[i].holeStrokes == 0) {
                            FindNearestBench(g_golferPool[i].x, g_golferPool[i].y,
                                (g_golferPool[i].flags & 0x2000000) ? 4 : 2);
                            if (g_destRestX != -1) {
                                bldg = -2;
                                wx = (g_destRestX << 10) + 0x200;
                                wy = (g_destRestY << 10) + 0x200;
                                g_golferPool[i].flags = g_golferPool[i].flags | 0x2000000;
                            } else {
                                flg = g_golferPool[i].flags & 0xfdffffff;
                                bldg = -1;
                                g_golferPool[i].flags = flg;
                                goto snackBar;
                            }
                        }
                    } else {
                        g_golferPool[i].flags = g_golferPool[i].flags & ~0x2000000;
                    }

                    if (bldg == -1) {
snackBar:
                        if ((g_nextGolferOrder - g_golferPool[g_golferPool[i].partner].seq
                                    > g_nextGolferOrder - g_golferPool[i].seq
                                || g_golferPool[g_golferPool[i].partner].holeStrokes != 0)
                            && (g_golferPool[i].flags & 0x4000000) == 0) {
                            bldg = FindNearestBuildingOfType(3, g_golferPool[i].x, g_golferPool[i].y);

                            if (g_closestDistanceFound < 0xc00) {
                                wx = (g_buildingTable[bldg].row << 10) + 0x200;
                                wy = (g_buildingTable[bldg].col << 10) + 0x200;
                            } else {
                                bldg = -1;
                            }
                        }
                    }

                    if (bldg == -1 && g_golferPool[i].holeStrokes == 0) {
                        g_golferPool[i].club = 0;
                        if ((g_golferPool[i].flags & GOLFER_PRACTISED_GREEN) == 0
                            && (g_golferPool[i].talents & TALENT_IMAGINATION) != 0 && g_golferPool[i].persona == 0
                            && g_simOpenFacilityLvl[SCENERY_PUTTING_GREEN] != 0) {
                            bldg = FindNearestBuildingOfType(6, g_golferPool[i].x, g_golferPool[i].y);
                            flg = g_golferPool[i].flags;
                            if (g_closestDistanceFound < ((flg & 0x40) ? 7 : 5) * 0x400) {
                                wx = (g_buildingTable[bldg].row + (i & 1) + 1) << 10;
                                wy = (g_buildingTable[bldg].col + (i & 1) + 1) << 10;
                                flg |= 0x40;
                                g_golferPool[i].flags = flg;
                            } else {
                                bldg = -1;
                                flg &= ~0x40;
                                g_golferPool[i].flags = flg;
                            }
                        }
                    } else {
                        g_golferPool[i].flags = g_golferPool[i].flags & ~0x40;
                    }

                    if (bldg == -1 && g_golferPool[i].holeStrokes == 0) {
                        if ((g_golferPool[i].flags & GOLFER_VISITED_PRO_SHOP) == 0
                            && (g_golferPool[i].talents & TALENT_ACCURACY) != 0 && g_golferPool[i].persona == 0
                            && g_simOpenFacilityLvl[SCENERY_PRO_SHOP] != 0) {
                            bldg = FindNearestBuildingOfType(8, g_golferPool[i].x, g_golferPool[i].y);
                            flg = g_golferPool[i].flags;
                            if (g_closestDistanceFound < ((flg & 0x100) ? 7 : 5) * 0x400) {
                                wx = ((g_buildingTable[bldg].row + (i & 1)) << 10) + 0x200;
                                wy = ((g_buildingTable[bldg].col + (i & 1)) << 10) + 0x200;
                                flg |= 0x100;
                                g_golferPool[i].flags = flg;
                            } else {
                                bldg = -1;
                                flg &= ~0x100;
                                g_golferPool[i].flags = flg;
                            }
                        }
                    } else
                        g_golferPool[i].flags = g_golferPool[i].flags & ~0x100;

                    if (bldg == -1 && g_golferPool[i].holeStrokes == 0) {
                        if ((g_golferPool[i].flags & GOLFER_PRACTISED_RANGE) == 0
                            && (g_golferPool[i].talents & TALENT_LENGTH) != 0 && g_golferPool[i].persona == 0
                            && g_simOpenFacilityLvl[SCENERY_DRIVING_RANGE] != 0) {
                            bldg = FindNearestBuildingOfType(0xa, g_golferPool[i].x, g_golferPool[i].y);
                            flg = g_golferPool[i].flags;
                            if (g_closestDistanceFound < ((((flg & 0x20) != 0) ? 9 : 6) << 10)) {
                                wx = ((g_buildingTable[bldg].row + (i & 1)) << 10) + 0xa00;
                                wy = ((g_buildingTable[bldg].col + (i & 1)) << 10) + 0xa00;
                                flg |= 0x20;
                                g_golferPool[i].flags = flg;
                            } else {
                                flg &= ~0x20;
                                bldg = -1;
                                g_golferPool[i].flags = flg;
                            }
                        }
                    } else
                        g_golferPool[i].flags = g_golferPool[i].flags & ~0x20;
                    if (bldg == -1) {

                        partner = g_golferPool[i].partner;
                        if (g_golferPool[partner].holeIdx != 0
                            && g_golferPool[partner].holeIdx <= g_golferPool[i].holeIdx && i < partner
                            && (!(g_golferPool[partner].flags & 0x400)
                                || g_golferPool[partner].holeIdx < g_golferPool[i].holeIdx
                                || g_golferPool[partner].activity == ACTIVITY_PLEASED)) {
                            blockedFlag = 0;
                            wx += -g_dirRowStep[(g_allHolesArray[g_golferPool[i].holeIdx].teeDir - 1) & 7] * 0x400 / 2;
                            wy += -g_dirColStep[(g_allHolesArray[g_golferPool[i].holeIdx].teeDir - 1) & 7] * 0x400 / 2;
                            g_golferPool[i].flags |= 0x800;
                        }

                        partner = g_golferPool[i].partner;
                        if (g_golferPool[partner].holeIdx != 0 && pairBehind != 0) {
                            blockedFlag = 0;
                            wx += -g_dirRowStep[g_allHolesArray[g_golferPool[i].holeIdx].teeDir & 7] * 0x400 / 2;
                            wy += -g_dirColStep[g_allHolesArray[g_golferPool[i].holeIdx].teeDir & 7] * 0x400 / 2;
                            g_golferPool[i].flags |= 0x800;
                        }
                    }
                }
            }
            if (g_golferPool[i].ballX != 0) {
                if (GetTileAtPosition(wx, wy) == TERRAIN_WATER) {
                    wx = g_golferPool[i].ballX;
                    wy = g_golferPool[i].ballY;
                }
            }

            dx = wx - g_golferPool[i].x;
            dy = wy - g_golferPool[i].y;
            distToTarget = Math_Distance2D(dx, dy);
            if (distToTarget < (((bldg != -1 && g_buildingTable[bldg].kind != 1) ? 8 : 2) << 6)) {

                g_golferPool[i].activity = ACTIVITY_WALK;
                if (g_golferPool[i].ballX != 0) {
                    partner = g_golferPool[i].partner;
                    ang = CalculateAngle(g_golferPool[partner].ballX - g_golferPool[i].x,
                        g_golferPool[partner].ballY - g_golferPool[i].y);
                    g_golferPool[i].dir = ((((ang >> 0x1c) & 0xf) + 1) >> 1) & 7;
                }

                if (g_golferPool[i].flags & GOLFER_BALL_MOVING)
                    goto moveBall;
                if (g_golferPool[i].holeIdx == CLUBHOUSE_HOLE) {
                    g_golferPool[i].holeIdx = 0;
                    continue;
                }

                if (bldg == -2) {
                    g_golferPool[i].waitTimer = -g_golferPool[i].fatigue * 8 / 20;
                    g_golferPool[i].activity = ACTIVITY_SIT_DOWN;
                    g_golferPool[i].flags = g_golferPool[i].flags & ~0x1000;
                    SetGolferAction(i, 0x1b, 0x14);
                    do {
                        dir2 = g_rng.NextInt(4) << 1;
                        if (IsNeighborWalkable(posTileX, posTileY, dir2)) {
                            g_golferPool[i].x = (g_dirRowStep[dir2] * 511) + (posTileX << 10) + 0x200;
                            g_golferPool[i].y = (g_dirColStep[dir2] * 511) + (posTileY << 10) + 0x200;
                            g_golferPool[i].fatigue = 0;
                            g_golferPool[i].dir = dir2;
                        } else {
                            g_golferPool[i].fatigue--;
                        }
                    } while (g_golferPool[i].fatigue != 0);
                    g_golferPool[i].shotStep = 0;
                    continue;
                }

                if (bldg != -1) {
                    if (g_buildingTable[bldg].kind == SCENERY_SNACK_BAR) {
                        SetGolferAction(i, 0x12, 0x14);
                        g_golferPool[i].thirst = 0;
                        g_golferPool[i].hunger = 0;
                        g_golferPool[i].waitTimer = -0x30;
                        g_golferPool[i].activity = ACTIVITY_WALK;
                        PlayPositionedSound(0x29, g_golferPool[i].x, g_golferPool[i].y, 0);
                        g_gameCashHundreds += 5;
                        AddMoneyPopup(5, g_golferPool[i].x, g_golferPool[i].y, -1);
                        g_annualStatsRecords[g_careerPlayYear % 100].v[2] += 5;
                    }
                    if (g_buildingTable[bldg].kind == 3) {
                        g_golferPool[i].waitTimer = -12 - g_rng.NextInt(8);
                        g_golferPool[i].activity = ACTIVITY_WASH_BALL;
                        g_golferPool[i].flags |= 0x4000000;
                        SetGolferAction(i, 0x29, 0x14);
                        g_golferPool[i].talkTimer = g_golferPool[i].talkTimer + 3;
                        tileIdx = g_buildingTable[bldg].col + g_buildingTable[bldg].row * 50;
                        g_tileExtra[tileIdx] |= TILE_ANIMATING;
                        g_tileAnimFrame[tileIdx] = 0;
                    }

                    if (g_buildingTable[bldg].kind == SCENERY_PUTTING_GREEN) {
                        g_golferPool[i].waitTimer = -16 - g_rng.NextInt(9);
                        g_golferPool[i].club = CLUB_PUTTER;
                        g_golferPool[i].activity = ACTIVITY_SWING;
                        g_golferPool[i].flags |= GOLFER_PRACTISED_GREEN;
                        amt = (((g_buildingLevelPerKind[SCENERY_PUTTING_GREEN] > 1) ? 8 : 4) * 100) / 100;
                        g_gameCashHundreds += amt;
                        AddMoneyPopup(amt, g_golferPool[i].x, g_golferPool[i].y, -1);
                        g_annualStatsRecords[g_careerPlayYear % 100].v[2] += amt;
                        SetGolferAction(i, 0x35, 0x14);
                    }

                    if (g_buildingTable[bldg].kind == SCENERY_PRO_SHOP) {

                        g_golferPool[i].waitTimer = -0x10 - g_rng.NextInt(9);
                        g_golferPool[i].flags |= GOLFER_VISITED_PRO_SHOP;
                        SetGolferAction(i, 0x34, 0x14);
                        price = ((g_buildingLevelPerKind[SCENERY_PRO_SHOP] > 1) ? 10 : 6) * 100;
                        amt = price / 100;
                        g_gameCashHundreds += amt;
                        AddMoneyPopup(amt, g_golferPool[i].x, g_golferPool[i].y, -1);
                        g_annualStatsRecords[g_careerPlayYear % 100].v[2] += amt;
                    }

                    if (g_buildingTable[bldg].kind != SCENERY_DRIVING_RANGE)
                        continue;
                    g_golferPool[i].waitTimer = -0x10 - g_rng.NextInt(9);
                    g_golferPool[i].flags |= GOLFER_PRACTISED_RANGE;
                    SetGolferAction(i, 0x33, 0x14);
                    price2 = ((g_buildingLevelPerKind[SCENERY_DRIVING_RANGE] > 1) ? 12 : 8) * 100;
                    amt2 = price2 / 100;
                    g_gameCashHundreds += amt2;
                    AddMoneyPopup(amt2, g_golferPool[i].x, g_golferPool[i].y, -1);
                    g_annualStatsRecords[g_careerPlayYear % 100].v[2] += amt2;
                    continue;
                }

                if (g_golferPool[g_golferPool[i].partner].holeIdx != 0 && g_golferPool[i].ballX == 0
                    && g_golferPool[g_golferPool[i].partner].ballX != 0
                    && g_golferPool[i].holeIdx > g_golferPool[g_golferPool[i].partner].holeIdx) {
                    g_golferPool[i].activity = ACTIVITY_WALK;
                    ang = CalculateAngle(g_golferPool[g_golferPool[i].partner].x - g_golferPool[i].x,
                        g_golferPool[g_golferPool[i].partner].y - g_golferPool[i].y);
                    g_golferPool[i].dir = ((((ang >> 0x1c) & 0xf) + 1) >> 1) & 7;
                    g_golferPool[i].walkLeft = 0;
                    continue;
                }

                if (g_golferPool[g_golferPool[i].partner].holeIdx != 0
                    && g_golferPool[i].holeIdx == g_golferPool[g_golferPool[i].partner].holeIdx
                    && g_golferPool[i].ballSpeed == 0) {

                    if ((i < g_golferPool[i].partner && g_golferPool[g_golferPool[i].partner].holeStrokes == 0
                            && !(g_golferPool[i].flags & 0x4000))
                        || (i > g_golferPool[i].partner && (g_golferPool[i].flags & 0x400)
                            && g_golferPool[g_golferPool[i].partner].holeStrokes == 0)) {
                        g_golferPool[i].activity = ACTIVITY_WALK;
                        ang = CalculateAngle(g_golferPool[g_golferPool[i].partner].x - g_golferPool[i].x,
                            g_golferPool[g_golferPool[i].partner].y - g_golferPool[i].y);
                        g_golferPool[i].dir = ((((ang >> 0x1c) & 0xf) + 1) >> 1) & 7;
                        g_golferPool[i].walkLeft = 0;
                        g_golferPool[i].waitTimer = -4 - g_rng.NextInt(4);
                        continue;
                    }
                }

                pinWX = (g_allHolesArray[g_golferPool[i].holeIdx].aimX << 10) + 0x200;
                pinWY = (g_allHolesArray[g_golferPool[i].holeIdx].aimY << 10) + 0x200;
                if (g_golferPool[g_golferPool[i].partner].holeIdx == g_golferPool[i].holeIdx
                    && g_golferPool[g_golferPool[i].partner].holeStrokes != 0 && g_golferPool[i].ballX != 0) {
                    if (Math_Distance2D(g_golferPool[i].ballX - pinWX, g_golferPool[i].ballY - pinWY)
                        < g_golferPool[g_golferPool[i].partner].ballY - pinWY) {
                        g_golferPool[i].activity = ACTIVITY_WALK;
                        ang = CalculateAngle(g_golferPool[g_golferPool[i].partner].x - g_golferPool[i].x,
                            g_golferPool[g_golferPool[i].partner].y - g_golferPool[i].y);
                        g_golferPool[i].dir = ((((ang >> 0x1c) & 0xf) + 1) >> 1) & 7;
                        g_golferPool[i].walkLeft = 0;
                        g_golferPool[i].waitTimer = -g_rng.NextInt(4);
                        continue;
                    }
                }

                g_golferPool[i].targetX = 0;
                if (pairBehind == 0 && blockedFlag != 0 && queuedAhead == 0) {
                    if (g_golferPool[i].ballX == 0) {
                        g_golferPool[i].ballX = restX;
                        g_golferPool[i].ballY = restY;
                    }
                    g_golferPool[i].shotStep = 1;
                }
                g_golferPool[i].walkLeft = 0;
                g_golferPool[i].ballClimb = 0;
                g_golferPool[i].ballSpeed = 0;
                g_golferPool[i].ballZ = 0;
            } else {

                prevDir = g_golferPool[i].dir;
                if (distToTarget < 0x400) {
                    g_golferPool[i].dir = GetDirection8(dx + distToTarget / 2, dy + distToTarget / 2);
                    g_golferPool[i].walkLeft = 0;
                } else if (g_golferPool[i].walkLeft <= 0) {
                    if (g_gameTileTerrainKind[posTileX * 50 + posTileY] == TERRAIN_WATER) {
                        if ((g_tileExtra[tileIdx] & TILE_PATH) && (g_tileExtra[tileIdx] & TILE_SCENIC)
                            && (g_golferPool[i].msgLine & MSG_LINE_MASK) != 7) {
                            bit = g_tileExtra[tileIdx] & 7;
                            SetGolferAction(i, 7, 0);
                            g_golferPool[i].bridgesSeen =
                                (unsigned char)((~(1 << g_rng.NextInt(5)) & g_golferPool[i].bridgesSeen) | (1 << bit));
                        }
                    }

                    g_runModeFlags &= ~0x80000;
                    g_golferPool[i].dir = FindPathDirection(wx, wy, g_golferPool[i].x, g_golferPool[i].y, i);
                    g_runModeFlags &= ~0x1000;
                    if (terrType != 0 && g_golferPool[i].dir == (prevDir ^ 4)) {
                        g_golferPool[i].dir = (unsigned char)((g_golferPool[i].dir + ((g_clock & 0x10) ? 1 : -1)) & 7);
                    }

                    tx = (g_golferPool[i].x >> 6) & 0xf;
                    ty = (g_golferPool[i].y >> 6) & 0xf;
                    if (g_dirRowStep[g_golferPool[i].dir] != 0) {
                        g_golferPool[i].walkLeft =
                            (unsigned short)((g_dirRowStep[g_golferPool[i].dir] == 1) ? 0x18 - tx : tx + 8);
                    } else {
                        g_golferPool[i].walkLeft =
                            (unsigned short)((g_dirColStep[g_golferPool[i].dir] == 1) ? 0x18 - ty : ty + 8);
                    }

                    if (g_golferPool[i].dir & 1) {
                        if (g_golferPool[i].walkLeft > 0x10)
                            g_golferPool[i].walkLeft = 0x10;
                    }

                    g_golferPool[i].walkLeft--;
                    if (g_runModeFlags & 0x80000)
                        g_golferPool[i].walkLeft = 3;

                    g_golferPool[i].walkLeft = g_golferPool[i].walkLeft * 0x40;
                    if (g_level != 0 && g_golferPool[i].holeStrokes <= 1 && g_golferPool[i].holeIdx != CLUBHOUSE_HOLE
                        && g_squareTypeTable[terrainType].lieGrade <= 0
                        && (g_golferPool[i].holeStrokes == 0 || (g_golferPool[i].flags & 0x800) == 0)
                        && (g_tileExtra[tileIdx] & TILE_PATH) == 0 && terrType != TERRAIN_BUILDING
                        && g_squareTypeTable[terrType].lieGrade > 1) {
                        if ((g_golferPool[i].dir & 1) == 0 && terrType != TERRAIN_SAND_TRAP) {
                            if ((g_golferPool[i].flags & 0x40000000) && g_golferPool[i].msgLine != 0xa) {
                                SetGolferAction(i, 0xa, terrType);
                                g_golferPool[i].flags &= ~0x40000000;
                            } else {
                                g_golferPool[i].flags |= 0x40000000;
                            }
                        }
                    } else {
                        g_golferPool[i].flags &= ~0x40000000;
                    }

                    if (g_golferPool[i].holeIdx != CLUBHOUSE_HOLE && !(g_golferPool[i].flags & 0x10000)
                        && g_golferPool[i].msgLine != 0x2b && !(g_golferPool[i].dir & 1)
                        && CalculateDirectionalOffset(g_golferPool[i].x, g_golferPool[i].y, g_golferPool[i].dir) > 2)
                        SetGolferAction(i, 0x2b,
                            CalculateDirectionalOffset(g_golferPool[i].x, g_golferPool[i].y, g_golferPool[i].dir));
                }

                if (terrType != 0 && g_golferPool[i].dir == (prevDir ^ 4)) {
                    g_golferPool[i].waitTimer = -8 - g_rng.NextInt(4);
                    g_golferPool[i].activity = ACTIVITY_WALK;
                }

                other = g_simLatestGolferNum;
                aheadCount = 0;
                partnerBlocked = 0;
                if (!(g_golferPool[i].flags & 0x21002000)) {
                    do {
                        other = (other + 1) % 0x98;

                        if (!(g_golferPool[other].flags & GOLFER_WALKED_OFF) || g_golferPool[other].holeIdx == 0
                            || i == other) {
                            if (g_golferPool[other].holeIdx <= 0)
                                continue;
                            if (other == i)
                                break;
                            if (g_golferPool[other].holeIdx > g_golferPool[i].holeIdx)
                                continue;
                            if (g_nextGolferOrder - g_golferPool[other].seq < g_nextGolferOrder - g_golferPool[i].seq)
                                continue;

                            if (other == g_golferPool[i].partner) {
                                if (g_golferPool[i].dir != g_golferPool[g_golferPool[i].partner].dir)
                                    continue;
                                if (g_golferPool[g_golferPool[i].partner].activity == ACTIVITY_WALK)
                                    continue;
                            }
                            if (other == g_golferPool[i].partner && wantSwap == 0)
                                continue;
                            if (g_golferPool[other].holeIdx == g_golferPool[i].holeIdx
                                && g_golferPool[other].holeStrokes == 0) {
                                aheadCount++;
                            }
                        }

                        dx = g_golferPool[other].x - g_golferPool[i].x;
                        ody = g_golferPool[other].y - g_golferPool[i].y;
                        if (Math_Distance2D(dx, ody)
                            >= (((g_golferPool[other].flags & GOLFER_WALKED_OFF) ? 4 : 1) << 10) / 2)
                            continue;

                        dir2 = GetDirection8(dx, ody);
                        if (g_golferPool[i].dir == dir2 || other == g_golferPool[i].partner
                            || g_golferPool[i].dir == ((dir2 + 1) & 7) || g_golferPool[i].dir == ((dir2 - 1) & 7)) {
                            g_golferPool[i].waitTimer = -4 - g_rng.NextInt(4);
                            partnerBlocked = 1;
                            g_golferPool[i].activity = ACTIVITY_WALK;
                        }

                        if (g_golferPool[i].slotKind[0] == 0x24)
                            continue;
                        if (!(g_golferPool[other].flags & GOLFER_WALKED_OFF))
                            continue;
                        SetGolferAction(i, 0x24, IsGolferMale(other));
                        g_golferPool[i].waitTimer = -4 - g_rng.NextInt(4);
                        g_golferPool[i].activity = ACTIVITY_WALK;
                        g_golferPool[i].dir = GetDirection8(dx, ody);
                        partnerBlocked = 1;
                    } while (other != i);

                    if (partnerBlocked != 0 && g_golferPool[i].holeStrokes == 0 && aheadCount >= 4
                        && !(g_golferPool[i].flags & 0x6000)) {
                        if (g_golferPool[i].flags & 0x1000) {
                            SetGolferAction(i, 0x15, 0x14);
                            g_golferPool[i].fatigue++;
                        } else
                            g_golferPool[i].flags |= 0x1000;

                        g_golferPool[i].waitTimer = -0x20 - g_rng.NextInt(0x40);
                        if (g_level == 0) {
                            g_golferPool[i].waitTimer = -0x64 - g_rng.NextInt(0x1c);
                            g_golferPool[i].flags |= 0x4000;
                        }

                        g_golferPool[i].activity = ACTIVITY_WALK;
                        if (g_golferPool[i].enjoyment < 0
                            && (g_golferPool[i].persona & PERSONA_KIND) != PERSONA_ROSTER) {

                            g_printBuffer[0] = '\0';
                            AppendGolferName(i, 0);
                            strcat(g_printBuffer, " and ");
                            AppendGolferName(g_golferPool[i].partner, 0);
                            strcat(g_printBuffer, " are leaving the course at hole ");
                            strcat(g_printBuffer, itoa(g_golferPool[i].holeIdx, g_numberBuf, 10));
                            strcat(g_printBuffer, ", 'Arghh, I am so tired of waiting for these idiots!'");
                            g_reportScreenId = 0;
                            ShowStoryMessage(COLOR_BLACK, 0, i);
                            ConfigureAndPlaySound(0x29, SOUND_FULL_VOLUME, 0, 0, 0);

                            {
                                short *pCount = &g_allHolesArray[g_golferPool[i].holeIdx].quitCount;
                                g_golferPool[i].waitTimer = 0;
                                (*pCount)++;
                            }
                            g_golferHistoryPerCharacter[g_golferPool[i].typeIdx]
                                .perHoleFlags[g_golferPool[i].holeIdx] |= HOLE_QUIT;
                            g_golferHistoryPerCharacter[g_golferPool[i].typeIdx].finishedStoryId = STORY_NONE;

                            g_golferPool[g_golferPool[i].partner].ballX = 0;
                            g_golferPool[g_golferPool[i].partner].x = 0;
                            ProcessVIPGolferEvent(g_golferPool[i].partner);

                            g_golferPool[i].holeIdx = CLUBHOUSE_HOLE;
                            g_golferPool[i].flags = (g_golferPool[i].flags & ~0x10000) | GOLFER_WALKED_OFF;
                            g_golferPool[i].fatigue = 0;
                            g_golferPool[i].attitude = -3;
                            continue;
                        }
                    }
                }

                if (g_golferPool[i].waitTimer < 0 || g_golferPool[i].dir == -1) {
                    g_golferPool[i].walkLeft = 0;
                    continue;
                }

                speedIdx = Clamp(6 - g_squareTypeTable[terrType].walk, 3,
                    (g_golferPool[i].fatigue >= GOLFER_TIRED_LEVEL) ? 3 : 5);

                if (i == g_watchedPlayerIdx || g_golferPool[i].partner == g_watchedPlayerIdx
                    || (g_golferPool[i].flags & 0x4000) || (g_runModeFlags & GAME_TOURNAMENT))
                    speedIdx++;

                if ((g_tileExtra[tileIdx] & TILE_PATH) || (entryFlags & GOLFER_IN_CART)) {
                    dxs2 = g_dirRowStep[g_golferPool[i].dir];
                    dys2 = g_dirColStep[g_golferPool[i].dir];
                    colOffset = dys2;
                    if ((g_tileExtra[dys2 + 50 * (posTileX + dxs2) + posTileY] & TILE_PATH)
                        || (g_tileExtra[50 * (posTileX - dxs2) - colOffset + posTileY] & TILE_PATH)) {
                        speedIdx = 6;
                        if (g_golferPool[i].flags & 0x10000) {
                            if (g_golferPool[i].holeStrokes == 0 && (entryFlags & GOLFER_IN_CART)) {
                                lieResist = -1;
                            } else {
                                lieResist = 0;
                            }
                            if (g_squareTypeTable[terrType].lieGrade > lieResist)
                                g_golferPool[i].flags |= GOLFER_IN_CART;
                        }
                    }
                }

                if ((((g_clock >> 4) & 3) ^ (i & 3)) == 0)
                    speedIdx++;

                mask = GOLFER_IN_CART;
                if ((g_golferPool[i].flags & 0x10000) && g_golferPool[i].holeStrokes <= 1
                    && g_golferPool[g_golferPool[i].partner].holeStrokes <= 1) {
                    g_golferPool[i].flags |= mask;
                }
                if (terrType == 1 || terrType == 0 || distToTarget < 0x400) {
                    g_golferPool[i].flags &= ~GOLFER_IN_CART;
                }

                if (g_golferPool[i].flags & mask) {
                    speedIdx = g_simOpenFacilityLvl[SCENERY_CART_GARAGE] * 4 + 8;
                    if (g_golferPool[i].fatigue > 0x28)
                        g_golferPool[i].fatigue = 0x28;
                    if ((g_golferPool[i].flags & mask) && !(entryFlags & mask))
                        PlayPositionedSound(0x31, g_golferPool[i].x, g_golferPool[i].y, 0);
                }

                if (g_runModeFlags & GAME_SLOW_PACE)
                    speedIdx = speedIdx + speedIdx / -3;

                amount = Clamp(speedIdx, 0, distToTarget * 7 / 128);
                if (g_level == 0)
                    step = (((g_fastRun ? 2 : 1) << 6) * amount) / ((g_golferPool[i].dir & 1) ? 10 : 7);
                else
                    step = (((g_fastRun ? 2 : 1) << 6) * amount) / ((g_golferPool[i].dir & 1) ? 8 : 5);

                if (step < 0x1e) {
                    g_golferPool[i].flags |= GOLFER_SLOW_WALK;
                } else {
                    g_golferPool[i].flags &= ~GOLFER_SLOW_WALK;
                }
                g_golferPool[i].x += g_dirRowStep[g_golferPool[i].dir] * step;
                g_golferPool[i].y += g_dirColStep[g_golferPool[i].dir] * step;
                g_golferPool[i].walkLeft -= step;

                if (g_gameTileTerrainKind[posTileX * 50 + posTileY] != TERRAIN_WATER) {
                    if (GetTileAtPosition(g_golferPool[i].x, g_golferPool[i].y) == TERRAIN_WATER)
                        g_golferPool[i].walkLeft = 0;
                }

                if ((g_clock + 11 * i) % 24 == 0 && g_golferPool[i].holeIdx != CLUBHOUSE_HOLE) {
                    band = g_golferPool[i].fatigue / 40;
                    if (g_tileExtra[tileIdx] & TILE_PATH)
                        inc = 1;
                    else
                        inc = g_squareTypeTable[terrType].walk;
                    g_golferPool[i].fatigue += inc;
                    g_golferPool[i].fatigue += g_golferPool[i].holeIdx / 6;
                    if (g_golferPool[i].fatigue >= GOLFER_TIRED_LEVEL && g_golferPool[i].fatigue / 40 != band) {
                        if ((g_golferPool[i].persona & PERSONA_KIND) != PERSONA_ROSTER)
                            SetGolferAction(i, 0x1a, g_golferPool[i].rapport == 3);
                        else
                            g_golferPool[i].fatigue = GOLFER_TIRED_LEVEL;
                    }
                }

                if (g_runModeFlags & 0x10) {
                    g_golferPool[i].x = wx;
                    g_golferPool[i].y = wy;
                    g_golferPool[i].walkLeft = 0;
                }

                if (g_golferPool[i].activity < ACTIVITY_IDLE || ++g_golferPool[i].activity > ACTIVITY_IDLE_LAST)
                    g_golferPool[i].activity = ACTIVITY_IDLE;
            }
            if (g_golferPool[i].ballSpeed == 0)
                continue;
        }
        if (pairBehind == 0 && (g_golferPool[i].ballSpeed != 0 || pairCloser == 0)) {

            if (g_golferPool[i].ballSpeed == 0) {

                g_golferPool[i].flags &= 0xffffe7ff;

                partner = g_golferPool[i].partner;

                if (g_golferPool[partner].holeIdx != 0
                    && g_golferPool[partner].activity
                           < (g_golferPool[i].holeStrokes ? ACTIVITY_FOLLOW_THROUGH : ACTIVITY_IDLE)) {
                    ang = CalculateAngle(g_golferPool[partner].x - g_golferPool[i].x,
                        g_golferPool[partner].y - g_golferPool[i].y);
                    g_golferPool[i].dir = ((((ang >> 28) & 0xf) + 1) >> 1) & 7;
                    g_golferPool[i].activity = ACTIVITY_WALK;
                    g_golferPool[i].walkLeft = 0;
                    continue;
                }

                if ((g_golferPool[i].persona & PERSONA_KIND) != PERSONA_ROSTER) {
                    if (Math_Distance2D(g_golferPool[i].ballX - (g_allHolesArray[g_golferPool[i].holeIdx].aimX << 10)
                                            - 0x200,
                            g_golferPool[i].ballY - (g_allHolesArray[g_golferPool[i].holeIdx].aimY << 10) - 0x200)
                        < 0x100) {
                        g_golferPool[i].holeStrokes++;

                        statIdx = g_golferPool[i].talentLevel + (g_golferPool[i].talents & SKILLS_ALL) * 4;
                        g_skillClassStatRows[statIdx].head[3]++;
                        g_allHolesArray[g_golferPool[i].holeIdx].putts++;
                        ProcessHoleCompletion(i);
                        continue;
                    }
                }

                if ((g_golferPool[i].flags & GOLFER_PRO_MATCH) != 0 && terrainType != TERRAIN_GREEN
                    && CalculateDistanceInYards(g_golferPool[i].ballX, g_golferPool[i].ballY,
                           g_allHolesArray[g_golferPool[i].holeIdx].aimX, g_allHolesArray[g_golferPool[i].holeIdx].aimY)
                           >= 0x19) {

                    if (g_golferPool[i].targetX == 0) {
                        if (g_golferPool[i].screenX == -1) {
                            g_cameraTileX = g_golferPool[i].x >> 10;
                            g_cameraTileY = g_golferPool[i].y >> 10;
                        }
                        g_golferPool[i].targetX = -1;
                        g_shotSelectedKind = 0;
                        g_mouseClickDelay = 0x0a;
                        if (g_golferPool[i].holeIdx == 1) {
                            partner = g_golferPool[i].partner;
                            if ((g_golferPool[partner].persona & PERSONA_KIND) == PERSONA_ROSTER
                                && g_golferPool[i].holeStrokes == 0 && (g_runModeFlags & GAME_TOURNAMENT) == 0) {
                                ConfigureAndPlaySound(0x2a, SOUND_FULL_VOLUME, 0, 0, 0);
                                g_printBuffer[0] = 0;
                                AppendGolferName(i ^ 1, 0);
                                strcat(g_printBuffer, " vs...");

                                RenderSkillPointsScreen(g_printBuffer, g_golferPool[i ^ 1].skillMask, -1, i ^ 1, 0);
                                g_printBuffer[0] = 0;
                                AppendGolferName(i, 0);
                                RenderSkillPointsScreen(g_printBuffer, g_golferPool[i].skillMask, -1, i, 0x15e);
                                g_simScreen.RenderFullSurface(0);
                                WaitForInputWithTimeout(0);
                            }
                        }
                    }

                    if (g_golferPool[i].targetX == -1) {
                        g_toolbarView = TOOLBAR_SHOT;
                        g_watchedPlayerIdx = i;
                        g_employeeListActive = 0;
                        g_golferPool[i].activity = ACTIVITY_WALK;
                        continue;
                    }

                    PlanGolferShot(i, 1, -1, 0, 0);
                } else {
                    if (g_shotSearchLocked != 0)
                        continue;
                    PlanGolferShot(i, 0, -1, 0, 0);
                }

                g_golferPool[i].dir = (unsigned char)((((((g_golferPool[i].ballBearing) >> 28) & 0xf) + 1) >> 1) & 7);

                if (terrainType == TERRAIN_GREEN) {
                    statIdx = g_golferPool[i].talentLevel + (g_golferPool[i].talents & SKILLS_ALL) * 4;
                    g_skillClassStatRows[statIdx].head[3]++;
                    g_allHolesArray[g_golferPool[i].holeIdx].putts++;

                    if ((g_tileExtra[ballTileX * 50 + ballTileY] & TILE_PIN) == 0) {
                        g_golferPool[i].waitTimer = 0xffe7;
                        g_golferPool[i].animTick = 0;
                    }
                }

                g_golferPool[i].shotFromX = g_golferPool[i].ballX;
                g_golferPool[i].shotFromY = g_golferPool[i].ballY;
                if (g_golferPool[i].activity != ACTIVITY_TALK)
                    g_golferPool[i].activity = ACTIVITY_WALK;
                g_golferPool[i].shotStep = 1;
                continue;
            }

            if (g_golferPool[i].shotStep != 0) {
                if (g_golferPool[i].shotStep == 1) {

                    if (g_golferPool[i].activity == ACTIVITY_STEP_UP)
                        continue;
                    g_golferPool[i].activity = ACTIVITY_WALK;

                    if (g_golferPool[i].holeStrokes != 0 || (i & 1) == 0
                        || g_golferPool[g_golferPool[i].partner].waitTimer < 0) {
                        if (g_fastRun == 0) {

                            bound = (terrainType == TERRAIN_GREEN) ? 0 : (g_golferPool[i].holeIdx + 4);
                            if (g_rng.NextInt(bound) != 0 && (g_golferPool[i].flags & 0x4200) == 0)
                                continue;
                        }
                    }

                    g_golferPool[i].dir =
                        (unsigned char)((((((g_golferPool[i].ballBearing) >> 28) & 0xf) + 1) >> 1) & 7);

                    ok = 1;
                    j = 0;
                    do {
                        if (g_golferPool[j].holeIdx == g_golferPool[i].holeIdx
                            && (g_nextGolferOrder - g_golferPool[j].seq) >= (g_nextGolferOrder - g_golferPool[i].seq)
                            && j != g_golferPool[i].partner && j != i && (g_golferPool[j].flags & 0x400) != 0
                            && g_golferPool[j].holeStrokes > g_golferPool[i].holeStrokes) {
                            if (CalculateDistanceInYards(g_golferPool[j].x, g_golferPool[j].y,
                                    g_allHolesArray[g_golferPool[i].holeIdx].teeX,
                                    g_allHolesArray[g_golferPool[i].holeIdx].teeY)
                                < 0x64) {
                                ok = 0;
                            } else {
                                if (CalculateDistanceInYards(g_golferPool[j].ballX, g_golferPool[j].ballY,
                                        g_golferPool[i].targetX, g_golferPool[i].targetY)
                                    < 0x25)
                                    ok = 0;
                                if (CalculateDistanceInYards(g_golferPool[j].x, g_golferPool[j].y,
                                        g_golferPool[i].targetX, g_golferPool[i].targetY)
                                    < 0x25) {
                                    ok = 0;
                                } else if (ok != 0
                                           && CalculateDistanceInYards(g_golferPool[j].x, g_golferPool[j].y,
                                                  g_allHolesArray[g_golferPool[i].holeIdx].teeX,
                                                  g_allHolesArray[g_golferPool[i].holeIdx].teeY)
                                                  < CalculateDistanceInYards(g_golferPool[i].x, g_golferPool[i].y,
                                                      g_golferPool[i].targetX, g_golferPool[i].targetY)) {
                                    ang2 = CalculateAngle(g_golferPool[j].x - g_golferPool[i].x,
                                        g_golferPool[j].y - g_golferPool[i].y);
                                    ang3 = CalculateAngle((g_golferPool[i].targetX << 10) - g_golferPool[i].x + 0x200,
                                        (g_golferPool[i].targetY << 10) - g_golferPool[i].y + 0x200);
                                    if (abs(ang2 - ang3) < 0x0aaaaaaa)
                                        ok = 0;
                                }
                            }
                        }
                    } while (++j < 0x98);

                    if ((g_golferPool[i].flags & GOLFER_PRO_MATCH) == 0 && ok == 0) {
                        g_golferPool[i].activity = ACTIVITY_WALK;
                        g_golferPool[i].walkLeft = 0;
                        g_golferPool[i].waitTimer = -g_rng.NextInt(4);
                        continue;
                    }

                    g_golferPool[i].activity = ACTIVITY_STEP_UP;
                    g_golferPool[i].animTick = 0;
                    g_golferPool[i].x = g_golferPool[i].ballX;
                    g_golferPool[i].y = g_golferPool[i].ballY;
                    g_golferPool[i].flags &= ~GOLFER_ALT_REACTION;
                    continue;
                }

                if (g_golferPool[i].shotStep == SHOT_STEP_ADDRESSED) {
                    if (g_golferPool[i].club != CLUB_PUTTER)
                        g_golferPool[i].dir = (g_golferPool[i].dir + 2) & 7;
                    g_golferPool[i].flags |= 0x800;
                    g_golferPool[i].animTick = 0;
                }
                g_golferPool[i].shotStep++;
                g_golferPool[i].activity = Clamp(g_golferPool[i].shotStep - 3, ACTIVITY_SWING, ACTIVITY_FOLLOW_THROUGH);
                if (g_golferPool[i].shotStep > 0x78)
                    g_golferPool[i].shotStep = 0x78;

                kind = GOLFER_BALL_MOVING;
                if (g_golferPool[i].activity == ACTIVITY_STRIKE) {

                    anim = 0xbe;
                    if ((g_golferPool[i].talents & TALENT_LENGTH) == 0)
                        anim = 0xc1;
                    if ((g_golferPool[i].talents & TALENT_ACCURACY) == 0)
                        anim = 0xbf;
                    if ((g_golferPool[i].talents & TALENT_IMAGINATION) == 0)
                        anim = 0xc0;
                    if (g_golferPool[i].flags & GOLFER_BALL_TRAIL) {
                        anim = 0xbd;
                        SetGolferAction(i, 0x28, 0);
                        g_golferPool[i].talkTimer = 3;
                    }
                    if (g_golferPool[i].club >= 0x0a)
                        anim = 2;
                    if (terrainType == TERRAIN_GREEN)
                        anim = 3;
                    if ((g_golferPool[i].flags & kind) == 0) {
                        PlayPositionedSound(anim, g_golferPool[i].x, g_golferPool[i].y, 0);
                        if (terrainType == TERRAIN_FAIRWAY)
                            SpawnGolferDivot(i);
                    }
                    g_golferPool[i].flags |= kind;
                    if (g_golferPool[i].screenX != -1)
                        StartShotTrail(i);
                    if ((g_golferPool[i].talents & TALENT_MATCH_PLAY) != 0 && terrainType != TERRAIN_GREEN
                        && i == g_watchedPlayerIdx) {
                        PlayPositionedSound(SOUND_APPLAUSE, g_golferPool[i].x, g_golferPool[i].y, 0x3e8);
                    }
                    g_gameStruckClock[i] = g_clock;
                }
                if (g_golferPool[i].activity <= ACTIVITY_STRIKE - 1 && (g_golferPool[i].flags & kind) == 0)
                    continue;

                if (g_golferPool[i].shotStep == 0x20 && (g_golferPool[i].flags & 0x4000)) {
                    g_golferPool[i].flags |= 0x400;
                    g_golferPool[i].shotStep = 0;
                }
                if (g_golferPool[i].shotStep == 0) {

                    if (terrainType != TERRAIN_GREEN) {
                        g_golferPool[i].x =
                            g_golferPool[i].shotFromX
                            - g_dirRowStep[(g_golferPool[i].dir + 2) & 7] * (((g_golferPool[i].dir & 1) ? 2 : 3) << 7);
                        g_golferPool[i].y =
                            g_golferPool[i].shotFromY
                            - g_dirColStep[(g_golferPool[i].dir + 2) & 7] * (((g_golferPool[i].dir & 1) ? 2 : 3) << 7);
                    } else {
                        g_golferPool[i].x =
                            g_golferPool[i].shotFromX
                            - g_dirRowStep[(g_golferPool[i].dir + 2) & 7] * (((g_golferPool[i].dir & 1) ? 2 : 3) << 6);
                        g_golferPool[i].y =
                            g_golferPool[i].shotFromY
                            - g_dirColStep[(g_golferPool[i].dir + 2) & 7] * (((g_golferPool[i].dir & 1) ? 2 : 3) << 6);
                    }
                }
            }
moveBall:
            if (g_golferPool[i].ballSpeed == 0)
                continue;

            if (g_gameStruckClock[i] == 0)
                g_gameStruckClock[i] = g_clock;

            groundH = GetTerrainHeightAtWorld(g_golferPool[i].ballX, g_golferPool[i].ballY);

            stepX = FixedPointSin(g_golferPool[i].ballBearing, g_golferPool[i].ballSpeed / 16);
            stepY = FixedPointCos(g_golferPool[i].ballBearing, g_golferPool[i].ballSpeed / 16);
            g_golferPool[i].ballX += stepX;
            g_golferPool[i].ballY -= stepY;

            g_golferPool[i].ballZ = g_golferPool[i].ballZ + g_golferPool[i].ballClimb / 32;
            if (g_golferPool[i].ballZ != 0 || g_golferPool[i].ballClimb != 0) {

                if (slotIdx != -1 && g_golferPool[i].ballClimb > 0 && g_golferPool[i].ballClimb < 0x40)
                    AddGolferTrailPoint(slotIdx);
                g_golferPool[i].ballClimb -= 0x40;
            }
            if (slotIdx != -1 && (g_clock & 7) == 0)
                AddGolferTrailPoint(slotIdx);

            subX = g_golferPool[i].ballX >> 6 & 0xf;
            subY = g_golferPool[i].ballY >> 6 & 0xf;
            edgeFlags = 0;
            if (subX <= 1) {
                if (g_gameTileTerrainKind[(ballTileX - 1) * 50 + ballTileY] != terrainType)
                    edgeFlags = 8;
            }
            if (subY <= 1) {
                if (g_gameTileTerrainKind[ballTileX * 50 + ballTileY - 1] != terrainType)
                    edgeFlags |= 1;
            }
            if (subX >= 0xe) {
                if (g_gameTileTerrainKind[(ballTileX + 1) * 50 + ballTileY] != terrainType)
                    edgeFlags |= 2;
            }
            if (subY >= 0xe) {
                if (g_gameTileTerrainKind[ballTileX * 50 + ballTileY + 1] != terrainType)
                    edgeFlags |= 4;
            }

            dirOctant = (((g_golferPool[i].ballBearing >> 0x1c) + 1) >> 1) & 7;

            if (g_golferPool[i].ballZ > 1) {
                newGround = GetTerrainHeightAtWorld(g_golferPool[i].ballX, g_golferPool[i].ballY);
                g_golferPool[i].ballZ += groundH - newGround;
                g_golferPool[i].ballSpeed -= (g_golferPool[i].ballSpeed >> 4) / 2;
                g_golferPool[i].ballBearing += g_golferPool[i].ballCurve / 2;

                if (BallHitsObstacle(terrainType, g_golferPool[i].ballZ, ballTileX, ballTileY)
                    && !(g_golferPool[i].flags & 2)) {
                    dist = Math_Distance2D(g_golferPool[i].ballX - (ballTileX << 10) - 0x200,
                        g_golferPool[i].ballY - (ballTileY << 10) - 0x200);
                    if (g_golferPool[i].persona != 0 && (g_golferPool[i].skillMask & 0x200) != 0)
                        dist += g_golferPool[i].ratings[9] * dist / 4;

                    if (dist < g_rng.NextInt(0x180)) {
                        g_golferPool[i].ballBearing += (g_rng.NextInt(0x80) + 0x40) << 24;
                        speed = g_golferPool[i].ballSpeed;
                        g_golferPool[i].ballSpeed -= g_rng.NextInt(speed);
                        bounceSnd = g_rng.NextInt(3);
                        PlayPositionedSound(bounceSnd + 6, g_golferPool[i].ballX, g_golferPool[i].ballY, 0);
                        if ((g_golferPool[i].flags & 2) == 0)
                            SetGolferAction(i, 0xc, terrainType);
                        g_golferPool[i].flags |= 2;
                    }
                }
            } else {

                lieResist =
                    Clamp(g_squareTypeTable[terrainType].rollShift
                              - CalculateDirectionalOffset(g_golferPool[i].ballX, g_golferPool[i].ballY, dirOctant),
                        0, 0x63);
                slope = CalculateDirectionalOffset(g_golferPool[i].ballX, g_golferPool[i].ballY, (dirOctant + 2) & 7);
                if (lieResist < 2 && edgeFlags != 0)
                    lieResist = 2;

                if (g_gameTileTerrainKind[(g_golferPool[i].shotFromX >> 10) * 50 + (g_golferPool[i].shotFromY >> 10)]
                    == 1) {
                    lieResist = g_squareTypeTable[terrainType].rollShift;
                    slope = 0;
                }
                if (lieResist > 4 && g_clock - g_gameStruckClock[i] > 0x80)
                    lieResist = 4;

                g_golferPool[i].ballBearing -= (slope << 26) / 2;
                if (lieResist < 5) {
                    g_golferPool[i].ballSpeed -= (g_golferPool[i].ballSpeed >> lieResist) / 2;
                } else if (lieResist > 4)
                    g_golferPool[i].ballSpeed += (0x40 - (g_golferPool[i].ballSpeed >> 5)) / 2;

                if (g_gameTileTerrainKind[ballTileX * 50 + ballTileY] == TERRAIN_GREEN) {
                    g_golferPool[i].ballBearing += g_golferPool[i].ballCurve / 2;
                    if ((g_clock & 7) == 0 && g_rng.NextInt(8) == 0)
                        g_golferPool[i].ballCurve = -g_golferPool[i].ballCurve;
                }

                if (terrainType == TERRAIN_WATER) {
                    if (edgeFlags == 0)
                        g_golferPool[i].ballSpeed = g_golferPool[i].ballSpeed / 2;
                } else if (terrainType == TERRAIN_RAVINE) {
                    if (abs(8 - subX) <= 2) {
                        if (abs(8 - subY) <= 2)
                            bounceEdge = 1;
                        if (subY < 8) {
                            if (g_gameTileTerrainKind[ballTileX * 50 + ballTileY - 1] == TERRAIN_RAVINE)
                                bounceEdge = 1;
                        }
                        if (subY > 8) {
                            if (g_gameTileTerrainKind[ballTileX * 50 + ballTileY + 1] == TERRAIN_RAVINE)
                                bounceEdge = 1;
                        }
                    }
                    if (abs(8 - subY) <= 2) {
                        if (subX < 8) {
                            if (g_gameTileTerrainKind[(ballTileX - 1) * 50 + ballTileY] == TERRAIN_RAVINE)
                                bounceEdge = 1;
                        }
                        if (subX > 8) {
                            if (g_gameTileTerrainKind[(ballTileX + 1) * 50 + ballTileY] == TERRAIN_RAVINE) {
                                bounceEdge = 1;
                            }
                        }
                    }
                    if (bounceEdge != 0)
                        g_golferPool[i].ballSpeed = g_golferPool[i].ballSpeed / 2;
                }

                if (!IsPositionInvalid(ballTileX, ballTileY) && (g_tileExtra[ballTileX * 50 + ballTileY] & TILE_PIN)
                    && g_golferPool[i].ballSpeed < 0x140
                    && Math_Distance2D(g_golferPool[i].ballX - (ballTileX << 10) - 0x200,
                           g_golferPool[i].ballY - (ballTileY << 10) - 0x200)
                           < 0x400 / ((g_golferPool[i].club != CLUB_PUTTER) ? 3 : 1)
                                 / (((g_runModeFlags & GAME_TOURNAMENT) != 0) ? 0x1e : 0x14)) {

                    PlayPositionedSound(4, g_golferPool[i].ballX, g_golferPool[i].ballY, 0);
                    g_golferPool[i].ballX = (ballTileX << 10) + 0x200;
                    g_golferPool[i].ballY = (ballTileY << 10) + 0x200;
                    if (slotIdx != -1)
                        CheckGolferShotDeviation(slotIdx);
                    g_golferPool[i].holeStrokes = g_golferPool[i].holeStrokes + 1;
                    ProcessHoleCompletion(i);
                    g_golferPool[i].ballSpeed = 0;
                    g_golferPool[i].flags = g_golferPool[i].flags & ~GOLFER_BALL_MOVING;
                    continue;
                }

                if (ballTileX != (g_golferPool[i].ballX >> 10)) {
                    if (g_tileUphillMap[ballTileX][ballTileY] & ((stepX > 0) ? 4 : 0x40)) {
                        PlayPositionedSound(6, g_golferPool[i].ballX, g_golferPool[i].ballY, 0);
                        g_golferPool[i].ballBearing = -g_golferPool[i].ballBearing;
                    }
                }
                if (ballTileY != (g_golferPool[i].ballY >> 10)) {
                    if (g_tileUphillMap[ballTileX][ballTileY] & ((stepY > 0) ? 1 : 0x10)) {
                        PlayPositionedSound(6, g_golferPool[i].ballX, g_golferPool[i].ballY, 0);
                        g_golferPool[i].ballBearing = 0x80000000 - g_golferPool[i].ballBearing;
                    }
                }
            }

            if (g_golferPool[i].ballZ <= 0 && g_golferPool[i].ballClimb < 0) {

                lieResist = g_squareTypeTable[terrainType].bounce;
                if (lieResist < 2 && edgeFlags != 0)
                    lieResist = 2;
                if (g_golferPool[i].ballClimb < -0x100)
                    PlayPositionedSound(0x36 + ((terrainType == TERRAIN_SAND_TRAP) ? 1 : 0), g_golferPool[i].ballX,
                        g_golferPool[i].ballY, 0);

                g_golferPool[i].ballClimb = Clamp(-0x40 - ((g_golferPool[i].ballClimb * lieResist) / 12), 0, 0x270f);
                if (g_golferPool[i].ballClimb < 0x80)
                    g_golferPool[i].ballClimb = 0;

                g_golferPool[i].ballZ = 0;
                if (terrainType == TERRAIN_GREEN && g_golferPool[i].ballClimb < -0xc0
                    && (g_tileVariationIdx[ballTileX * 50 + ballTileY] & 0x80) != 0) {
                    g_golferPool[i].ballBearing += (g_rng.NextInt(2) != 0) ? 0x0aaaaaaa : -0x0aaaaaaa;
                }

                if (slotIdx != -1)
                    AddGolferTrailPoint(slotIdx);
                if (g_golferPool[i].flags & 0x100) {
                    g_golferPool[i].ballBearing = g_golferPool[i].ballBearing ^ 0x80000000;
                    g_golferPool[i].flags = g_golferPool[i].flags & ~0x100;
                }
                if (g_golferPool[i].flags & 0x80) {
                    g_golferPool[i].ballSpeed = g_golferPool[i].ballSpeed / 2;
                    g_golferPool[i].flags = (g_golferPool[i].flags & ~0x80) | 0x100;
                }

                spinScale = g_golferPool[i].ballClimb * 0xaec33;
                g_golferPool[i].ballBearing -= CalculateDirectionalOffset(g_golferPool[i].ballX, g_golferPool[i].ballY,
                                                   ((((g_golferPool[i].ballBearing >> 0x1d) + 1) & ~1) + 2) & 7)
                                               * spinScale;

                g_golferPool[i].ballSpeed +=
                    -(Clamp(CalculateDirectionalOffset(g_golferPool[i].ballX, g_golferPool[i].ballY, dirOctant), -2, 2)
                        * g_golferPool[i].ballClimb)
                    * 2;

                g_golferPool[i].ballClimb +=
                    (Clamp(CalculateDirectionalOffset(g_golferPool[i].ballX, g_golferPool[i].ballY, dirOctant), -2, 2)
                        * g_golferPool[i].ballClimb)
                    / 2;

                if (g_squareTypeTable[terrainType].lieGrade > 0) {
                    rough = Clamp(g_squareTypeTable[terrainType].lieGrade, 1, 3);
                    g_golferPool[i].ballBearing += g_rng.NextInt(rough * 0xaaaa) - ((rough * 0xaaaaaaa) / 2);
                }

                if (GetTileAtPosition(g_golferPool[i].ballX, g_golferPool[i].ballY) == TERRAIN_WATER) {
                    if (edgeFlags == 0) {
                        g_golferPool[i].ballClimb = 0;
                        g_golferPool[i].ballSpeed = 0;
                        g_waterSprayFxPositionX = g_golferPool[i].ballX;
                        g_waterSprayFxPositionY = g_golferPool[i].ballY;
                        g_waterSprayFxFrame = 0;
                    }
                }
                if (terrainType == TERRAIN_ROCKS && g_golferPool[i].ballSpeed > 0x100 && edgeFlags == 0) {
                    if (g_golferPool[i].persona != 0 && (g_golferPool[i].skillMask & 0x200) != 0) {
                        jitter = g_rng.NextInt(0x50);
                        g_golferPool[i].ballBearing += (0x28 - jitter) << 0x18;
                    } else {
                        jitter = g_rng.NextInt(0xa0);
                        g_golferPool[i].ballBearing += (0x50 - jitter) << 0x18;
                    }
                    bounceSnd = g_rng.NextInt(3);
                    PlayPositionedSound(bounceSnd + 6, g_golferPool[i].ballX, g_golferPool[i].ballY, 0);
                }

                if (g_golferPool[i].ballSpeed > 0x100 && g_golferPool[i].persona != 0
                    && g_rng.NextInt(0x64) < g_golferPool[i].ratings[9]) {

                    if (CalculateDistanceInYards(g_golferPool[i].ballX, g_golferPool[i].ballY,
                            g_allHolesArray[g_golferPool[i].holeIdx].aimX,
                            g_allHolesArray[g_golferPool[i].holeIdx].aimY)
                            < 0x4b
                        && g_squareTypeTable[terrainType].lieGrade <= 0) {

                        PlayPositionedSound(0x18, g_golferPool[i].ballX, g_golferPool[i].ballY, 0);
                        aim = CalculateAngle((g_allHolesArray[g_golferPool[i].holeIdx].aimX << 10)
                                                 - g_golferPool[i].ballX + 0x200,
                            (g_allHolesArray[g_golferPool[i].holeIdx].aimY << 10) - g_golferPool[i].ballY + 0x200);
                        g_golferPool[i].ballBearing += ((aim - g_golferPool[i].ballBearing) * 4) / 5;
                        SetGolferAction(i, 0x2a, 0x14);
                    }
                }

                if (g_golferPool[i].ballClimb > 0xc8) {
                    j = 0;
                    do {
                        if (g_golferPool[j].holeIdx != 0 && g_golferPool[j].ballX != 0
                            && g_golferPool[j].holeIdx != g_golferPool[i].holeIdx
                            && g_golferPool[j].holeIdx != CLUBHOUSE_HOLE) {

                            if (Math_Distance2D(g_golferPool[i].ballX - g_golferPool[j].x,
                                    g_golferPool[i].ballY - g_golferPool[j].y)
                                < (((g_level + 2) << 10) / 2))
                                SetGolferAction(j, 9, 0x14);
                        }
                    } while (++j < 0x98);
                }
            }

            if (g_golferPool[i].ballSpeed >= 0x40)
                continue;
            if (g_golferPool[i].ballZ != 0)
                continue;
            if (g_golferPool[i].ballClimb != 0)
                continue;

            g_golferPool[i].ballSpeed = 0;
            g_golferPool[i].waitTimer = 0;
            g_golferPool[i].shotStep = 0;
            g_golferPool[i].flags &= ~GOLFER_BALL_MOVING;

            ballTileX = g_golferPool[i].ballX >> 10;
            ballTileY = g_golferPool[i].ballY >> 10;
            terrainType = g_gameTileTerrainKind[ballTileX * 50 + ballTileY];
            if (terrainType != TERRAIN_FAIRWAY) {
                g_golferPool[i].flags &= 0xfbffffff;
            }
            if (g_golferPool[i].holeStrokes == 0) {
                g_allHolesArray[g_golferPool[i].holeIdx].rounds++;
                statIdx = g_golferPool[i].talentLevel + (g_golferPool[i].talents & SKILLS_ALL) * 4;
                g_skillClassStatRows[statIdx].head[4]++;

                yards = (Math_Distance2D(g_golferPool[i].ballX - g_golferPool[i].shotFromX,
                             g_golferPool[i].ballY - g_golferPool[i].shotFromY)
                            * 25)
                        / 1024;
                statIdx = g_golferPool[i].talentLevel + (g_golferPool[i].talents & SKILLS_ALL) * 4;
                g_skillClassStatRows[statIdx].head[0] += yards;
                if (g_skillClassStatRows[statIdx].head[5] < yards)
                    g_skillClassStatRows[statIdx].head[5] = yards;

                g_allHolesArray[g_golferPool[i].holeIdx].driveTot += yards;
                if (yards > g_allHolesArray[g_golferPool[i].holeIdx].longDrive)
                    g_allHolesArray[g_golferPool[i].holeIdx].longDrive = yards;

                if (g_squareTypeTable[terrainType].lieGrade <= 0) {
                    g_skillClassStatRows[statIdx].head[1]++;
                    g_allHolesArray[g_golferPool[i].holeIdx].fairways++;
                }
            }

            g_golferPool[i].holeStrokes++;
            if (g_golferPool[i].holeStrokes == g_allHolesArray[g_golferPool[i].holeIdx].par - 2
                && terrainType == TERRAIN_GREEN) {
                statIdx = g_golferPool[i].talentLevel + (g_golferPool[i].talents & SKILLS_ALL) * 4;
                g_skillClassStatRows[statIdx].head[2]++;
                g_allHolesArray[g_golferPool[i].holeIdx].greens++;
            }

            if (!IsPositionInvalid(ballTileX, ballTileY)) {
                if (g_ballStopCountMap[ballTileX * 50 + ballTileY] < 0xff)
                    g_ballStopCountMap[ballTileX * 50 + ballTileY]++;
            }

            prevKind = g_golferPool[i].slotKind[0];

            if (g_squareTypeTable[terrainType].lieGrade <= 0 && !IsPositionInvalid(ballTileX, ballTileY)) {

                if (g_golferPool[i].shotDifficulty > 8) {
                    if (Math_Distance2D(ballTileX - g_allHolesArray[g_golferPool[i].holeIdx].aimX,
                            ballTileY - g_allHolesArray[g_golferPool[i].holeIdx].aimY)
                        < Math_Distance2D((g_golferPool[i].shotFromX >> 10)
                                              - g_allHolesArray[g_golferPool[i].holeIdx].aimX,
                            (g_golferPool[i].shotFromY >> 10) - g_allHolesArray[g_golferPool[i].holeIdx].aimY)) {
                        g_golferPool[i].activity = ACTIVITY_PLEASED;
                        g_golferPool[i].waitTimer = 0xff9d;
                        g_golferPool[i].dir = (g_golferPool[i].dir - 2) & 7;
                        SetGolferAction(i, 1, terrainType);
                    }
                }

            } else {

                if (g_golferPool[i].shotDifficulty < g_level + 3 && (g_golferPool[i].flags & 2) == 0
                    && (g_tileExtra[ballTileX * 50 + ballTileY] & TILE_SCENIC) == 0) {
                    g_golferPool[i].activity = ACTIVITY_ANNOYED;
                    g_golferPool[i].waitTimer = 0xff9d;
                    g_golferPool[i].dir = (g_golferPool[i].dir - 2) & 7;
                    SetGolferAction(i, 2, terrainType);
                }

                prevTerrain =
                    g_gameTileTerrainKind[(g_golferPool[i].shotFromX >> 10) * 50 + (g_golferPool[i].shotFromY >> 10)];
                if (g_squareTypeTable[prevTerrain].lieGrade > 0
                    && g_squareTypeTable[terrainType].lieGrade >= g_squareTypeTable[prevTerrain].lieGrade
                    && (g_tileExtra[ballTileX * 50 + ballTileY] & TILE_SCENIC) == 0) {
                    if (g_golferPool[i].activity != ACTIVITY_ANNOYED) {
                        g_golferPool[i].activity = ACTIVITY_ANNOYED;
                        g_golferPool[i].dir = (g_golferPool[i].dir - 2) & 7;
                    }
                    g_golferPool[i].waitTimer = 0xff9d;
                    SetGolferAction(i, 2 + ((terrainType != prevTerrain) ? 1 : 0), terrainType);
                    g_golferPool[i].flags |= GOLFER_ALT_REACTION;
                }

                if (terrainType == TERRAIN_WATER || IsPositionInvalid(ballTileX, ballTileY) || bounceEdge != 0) {

                    if (terrainType != TERRAIN_WATER && bounceEdge == 0) {
                        SetGolferAction(i, 2, terrainType);
                        g_golferPool[i].ballX = g_golferPool[i].shotFromX;
                        g_golferPool[i].ballY = g_golferPool[i].shotFromY;
                    } else {
                        PlayPositionedSound(5, g_golferPool[i].ballX, g_golferPool[i].y, 0);
                        SetGolferAction(i, 0xd, terrainType);

                        walkAngle = CalculateAngle(g_golferPool[i].ballX - g_golferPool[i].shotFromX,
                            g_golferPool[i].ballY - g_golferPool[i].shotFromY);
                        walkLen = Math_Distance2D(g_golferPool[i].ballX - g_golferPool[i].shotFromX,
                            g_golferPool[i].ballY - g_golferPool[i].shotFromY);
                        startYards = CalculateDistanceInYards(g_golferPool[i].ballX, g_golferPool[i].ballY,
                            g_allHolesArray[g_golferPool[i].holeIdx].aimX,
                            g_allHolesArray[g_golferPool[i].holeIdx].aimY);
                        g_golferPool[i].ballX = g_golferPool[i].shotFromX;
                        g_golferPool[i].ballY = g_golferPool[i].shotFromY;
                        probeDist = 0;
                        bestScore = -0x63;

                        if (walkLen > 0) {
                            do {
                                px = g_golferPool[i].shotFromX + FixedPointSin(walkAngle, probeDist);
                                py = g_golferPool[i].shotFromY - FixedPointCos(walkAngle, probeDist);

                                if (probeDist != 0) {
                                    if (CalculateDistanceInYards(px, py, g_allHolesArray[g_golferPool[i].holeIdx].aimX,
                                            g_allHolesArray[g_golferPool[i].holeIdx].aimY)
                                        < startYards)
                                        continue;
                                }
                                if (GetTileAtPosition(px, py) == TERRAIN_WATER)
                                    continue;
                                if (GetTileAtPosition(px, py) == TERRAIN_RAVINE)
                                    continue;
                                score = probeDist - (g_squareTypeTable[GetTileAtPosition(px, py)].lieGrade << 2);
                                if (score <= bestScore)
                                    continue;
                                g_golferPool[i].ballX = px;
                                g_golferPool[i].ballY = py;
                                bestScore = score;
                                if (probeDist != 0)
                                    g_golferPool[i].flags |= 0x400;
                            } while ((probeDist += 0x200) < walkLen);
                        }
                    }

                    if (g_golferPool[i].activity != ACTIVITY_ANNOYED) {
                        g_golferPool[i].activity = ACTIVITY_ANNOYED;
                        g_golferPool[i].dir = (g_golferPool[i].dir - 2) & 7;
                    }
                    g_golferPool[i].waitTimer = 0xff9d;
                    g_golferPool[i].flags |= GOLFER_AFTER_PENALTY;
                    if (g_golferPool[i].holeStrokes < 8)
                        g_golferPool[i].holeStrokes++;
                }
            }

            if (terrainType != TERRAIN_WATER) {
                if (!IsPositionInvalid(ballTileX, ballTileY)) {
                    g_golferPool[i].flags = (g_golferPool[i].flags & ~GOLFER_AFTER_PENALTY) | 0x400;
                    if (slotIdx != -1)
                        CheckGolferShotDeviation(slotIdx);
                }
            }

            if (g_squareTypeTable[terrainType].lieGrade <= 0 && prevKind == g_golferPool[i].slotKind[0]
                && g_golferPool[i].holeIdx <= 3) {
                yards = (Math_Distance2D(g_golferPool[i].shotFromX - g_golferPool[i].ballX,
                             g_golferPool[i].shotFromY - g_golferPool[i].ballY)
                            * 25)
                        / 1024;
                if (g_golferPool[i].holeStrokes == 1 && (g_golferPool[i].flags & GOLFER_PRACTISED_RANGE) != 0
                    && yards > CalculateGolferMaxDistance(i) && g_golferPool[i].holeIdx <= 5)
                    SetGolferAction(i, 0x33, 0x14);
            }

            if (g_golferPool[i].talkTimer != 0)
                continue;
            if (g_squareTypeTable[terrainType].lieGrade <= 0)
                continue;
            if (g_golferPool[i].flags & GOLFER_LEANS_LEFT)
                SetGolferAction(i, 0x11, 0x14);
            if (g_golferPool[i].flags & GOLFER_LEANS_RIGHT)
                SetGolferAction(i, 0x10, 0x14);
        } else {
            g_golferPool[i].waitTimer = -g_rng.NextInt(8);
            g_golferPool[i].shotStep = 0;
            g_golferPool[i].activity = ACTIVITY_WALK;
        }
    }
}

int CalculateTerrainHeight(int x, int y)
{
    x >>= 2;
    y >>= 2;
    int v = BilinearSample(x, y) * 6;
    v += BilinearSample(x * 2, y * 2) * 4;
    return Clamp(v * 7 / 64 / 2, 0, 0x200);
}

void FormatNumberWithCommas(int v)
{
    int rem;

    if (v < 0) {
        strcat(g_printBuffer, "-");
        v = -v;
    }
    if (v >= 1000) {
        FormatNumberWithCommas(v / 1000);
        strcat(g_printBuffer, ",");
    }

    rem = v % 1000;
    if (rem < 100 && v != 0) {
        if (v >= 1000)
            strcat(g_printBuffer, "0");
        if (rem < 10 && v >= 100)
            strcat(g_printBuffer, "0");
    }
    strcat(g_printBuffer, itoa(rem, g_numberBuf, 10));
}

void AppendMoney(int hundredths)
{
    int n;
    int cents;

    g_lastHundredthsShown = hundredths;
    if (hundredths < 0) {
        n = strlen(g_printBuffer);
        if (g_printBuffer[n - 1] == '+')
            g_printBuffer[n - 1] = '\0';
        strcat(g_printBuffer, "-");
        hundredths = -hundredths;
    }
    strcat(g_printBuffer, itoa(hundredths / 100, g_numberBuf, 10));
    strcat(g_printBuffer, ".");
    cents = hundredths % 100;
    if (cents < 10)
        strcat(g_printBuffer, "0");
    strcat(g_printBuffer, itoa(cents, g_numberBuf, 10));
}

void CalculateAllHoleStatistics(void)
{
    int colTot[8], colWtd[8];
    int i, k, r, c, d;
    int rt, w, fl, total, wtotal, dp, mask, worstBit, best, sum, snapSum, snapFun;
    int prevMask, moodTotal, shotAcc, ratio, holeDelta, t, bit, time;
    int prevAngle;

    total = 0;
    sum = 0;
    g_waitListLength = 0;
    for (k = 0; k < 76; k++) {
        if (g_golferHistoryPerCharacter[k].finishedStoryId != -1
            && (g_golferHistoryPerCharacter[k].flags & MEMBERSHIP_MASK) > 2)
            g_waitListLength++;
    }
    for (k = 0; k < 256; k++) {
        if (g_buildingTable[k].kind == SCENERY_HOME_SITE)
            g_waitListLength--;
    }
    g_courseTotalFunRate = 0;
    prevMask = -1;
    moodTotal = 0;
    shotAcc = 0x48;
    g_gameCourseTotalPar = 0;
    g_simTotalLengthYards = 0;
    g_totalImagination = 0;
    g_gameCourseAccurPointsTotal = 0;
    g_lengthAbilitySum = 0;
    for (i = 1; i < 19; i++) {
        if (g_allHolesArray[i].par == 0)
            continue;
        g_gameCourseTotalPar = g_gameCourseTotalPar + g_allHolesArray[i].par;
        g_simTotalLengthYards = g_simTotalLengthYards + g_allHolesArray[i].yards;
        for (c = 0; c < 8; c++)
            colWtd[c] = g_allHolesArray[i].par * 8;
        for (c = 0; c < 8; c++)
            colTot[c] = 8;
        dp = g_allHolesArray[i].moodSum * 100;
        moodTotal += dp;
        shotAcc += g_allHolesArray[i].rounds + g_allHolesArray[i].shots;
        time = g_allHolesArray[i].rounds ? (g_allHolesArray[i].playTime / g_allHolesArray[i].rounds) / 40 : 0;
        shotAcc -= 4;
        snapFun = g_courseTotalFunRate;
        if (g_allHolesArray[i].rounds != 0)
            g_courseTotalFunRate += dp / (g_allHolesArray[i].shots / 2 + g_allHolesArray[i].rounds + 4);
        total = 0;
        wtotal = 0;
        for (r = 1; r <= 9; r++)
            for (c = 0; c < 8; c++) {
                d = g_allHolesArray[i].dist[c][r];
                colTot[c] += d;
                w = d * r;
                colWtd[c] += w;
                total += d;
                wtotal += w;
            }
        g_allHolesArray[i].flags = g_allHolesArray[i].flags & ~0xc;
        fl = g_allHolesArray[i].flags;
        if (total >= 10) {
            if (wtotal > (6 - g_level) * total / 3 + g_allHolesArray[i].par * total)
                g_allHolesArray[i].flags = fl | 4;
            if (wtotal < g_allHolesArray[i].par * total - (3 - g_level) * total / 6)
                g_allHolesArray[i].flags |= 8;
        }
        mask = 0;
        snapSum = sum;
        best = 0x64;
        g_allHolesArray[i].flags = g_allHolesArray[i].flags & ~0x700;
        if (colTot[7] != 0) {
            if (colTot[6] != 0) {
                ratio = colWtd[6] * 100 / colTot[6] - colWtd[7] * 100 / colTot[7];
                sum += ratio;
                g_lengthAbilitySum += ratio;
                if (ratio >= ((g_level != 0) ? 0x32 : 0x19))
                    mask |= 1;
                if (ratio >= 0x32)
                    g_allHolesArray[i].flags |= 0x100;
                if (ratio < best) {
                    best = ratio;
                    worstBit = 1;
                }
            }
            if (colTot[5] != 0) {
                ratio = colWtd[5] * 100 / colTot[5] - colWtd[7] * 100 / colTot[7];
                sum += ratio;
                g_gameCourseAccurPointsTotal += ratio;
                if (ratio >= ((g_level != 0) ? 0x32 : 0x19))
                    mask |= 2;
                if (ratio >= 0x32)
                    g_allHolesArray[i].flags |= 0x200;
                if (ratio < best) {
                    best = ratio;
                    worstBit = 2;
                }
            }
            if (colTot[3] != 0) {
                ratio = colWtd[3] * 100 / colTot[3] - colWtd[7] * 100 / colTot[7];
                sum += ratio;
                g_totalImagination += ratio;
                if (ratio >= ((g_level != 0) ? 0x32 : 0x19))
                    mask |= 4;
                if (ratio >= 0x32)
                    g_allHolesArray[i].flags |= 0x400;
                if (ratio < best) {
                    best = ratio;
                    worstBit = 4;
                }
            }
        }
        if (best < ((g_level != 0) ? 0x64 : 0x32))
            mask &= ~worstBit;
        if (g_forceBits & 2) {
            i = 1;
            mask = 4;
            g_forceBits = g_forceBits & ~2;
        }
        if (mask != 0) {
            bit = 1 << mask;
            if (!(bit & g_simPostedHoleCategories)) {
                BuildHoleTypeMessage(i, mask);
                if (ShowStoryMessage(0x80001284, 0, -4)) {
                    g_simPostedHoleCategories |= bit;
                    ConfigureAndPlaySound(0x2a, SOUND_FULL_VOLUME, 0, 0, 0);
                }
            }
        }
        g_allHolesArray[i].flags = g_allHolesArray[i].flags & ~0x10;
        g_allHolesArray[i].similar = 0;
        if (i > 1) {
            if (mask == prevMask && g_allHolesArray[i].rounds >= 8 && mask != 7)
                g_allHolesArray[i].similar = 1;
            if (!((g_allHolesArray[i - 1].flags ^ g_allHolesArray[i].flags) & 0x60))
                g_allHolesArray[i].similar++;
            if (g_allHolesArray[i].cmt[45] == 0 && g_allHolesArray[i].cmt[46] == 0)
                g_allHolesArray[i].similar++;
            if (g_allHolesArray[i].par == g_allHolesArray[i - 1].par)
                g_allHolesArray[i].similar++;
            prevAngle = CalculateAngle(g_allHolesArray[i - 1].aimX - g_allHolesArray[i - 1].teeX,
                g_allHolesArray[i - 1].aimY - g_allHolesArray[i - 1].teeY);
            if (abs((CalculateAngle(g_allHolesArray[i].aimX - g_allHolesArray[i].teeX,
                         g_allHolesArray[i].aimY - g_allHolesArray[i].teeY)
                        - prevAngle)
                    >> 24)
                < 0x28)
                g_allHolesArray[i].similar++;
            rt = g_allHolesArray[i].similar;
            if (rt != 0 && g_level < 2)
                g_allHolesArray[i].similar = rt - 1;
        }
        prevMask = mask;
        holeDelta = sum - snapSum;
        t = (g_courseTotalFunRate - snapFun) * 6 / ((g_level != 0) + 2);
        if (g_level <= 1 && t > holeDelta)
            holeDelta = t;
        if (g_allHolesArray[i].revenue > 0xc8 && holeDelta > 0xc8 && !(g_allHolesArray[i].flags & 0xd)) {
            strcpy(g_printBuffer, "Hole #");
            strcat(g_printBuffer, itoa(i, g_numberBuf, 10));
            strcat(g_printBuffer, " (henceforth known as '");
            g_allHolesArray[i].flags |= 0x80;
            AppendHoleName(i);
            strcat(g_printBuffer, "'), has been rated as one of the best 100 holes ");
            strcat(g_printBuffer, "in the country by Golf Enquirer magazine!");
            strcat(g_printBuffer, " Increase greens fees by \xa7"
                                  "100.");
            if (ShowStoryMessage(COLOR_PURPLE, 0, -0x15)) {
                g_allHolesArray[i].flags |= HOLE_TOP_100;
                g_calculatedGreenFee++;
                NotifyEvent(0x80, i);
                ConfigureAndPlaySound(SOUND_APPLAUSE, SOUND_FULL_VOLUME, 0, 0, 0);
                TriggerAccomplishment(7, (g_allHolesArray[i].aimX << 10) + 0x200,
                    (g_allHolesArray[i].aimY << 10) + 0x200);
            } else
                g_allHolesArray[i].flags = g_allHolesArray[i].flags & ~0x80;
        }
        if (g_allHolesArray[i].revenue > 0x190 && holeDelta > 0x12c && !(g_allHolesArray[i].flags & 0xe)) {
            g_printBuffer[0] = '\0';
            AppendHoleName(i);
            strcat(g_printBuffer, " has been rated as one of the Top 18 holes ");
            strcat(g_printBuffer, "in the country by Great Golf Holes magazine!");
            strcat(g_printBuffer, " Increase greens fees by \xa7"
                                  "100.");
            if (ShowStoryMessage(0x80005288, 0, -0x15)) {
                g_allHolesArray[i].flags |= HOLE_TOP_18;
                g_calculatedGreenFee++;
                NotifyEvent(0xa0, i);
                ConfigureAndPlaySound(SOUND_APPLAUSE_GOOD, SOUND_FULL_VOLUME, 0, 0, 0);
                TriggerAccomplishment(0xa, (g_allHolesArray[i].aimX << 10) + 0x200,
                    (g_allHolesArray[i].aimY << 10) + 0x200);
            }
        }
    }
    g_clubSkillsScore = sum;
}

int GetCardinalDirection(int dx, int dy)
{
    if (abs(dx) > abs(dy))
        return dx > 0 ? 2 : 6;
    return dy > 0 ? 4 : 0;
}

int FindPathDirection(int x1, int y1, int x2, int y2, int golfer)
{
    int dx, dy, dist, mask, d;
    int ox, oy;
    int direct;
    int head, tail, limit;
    int best;

    memset(g_pathFindDist, 0, sizeof(g_pathFindDist));
    direct = GetDirection8(x1 - x2, y1 - y2);
    x1 = Clamp(x1 >> 10, 0, 49);
    y1 = Clamp(y1 >> 10, 0, 49);
    x2 = Clamp(x2 >> 10, 0, 49);
    y2 = Clamp(y2 >> 10, 0, 49);
    ox = Clamp(g_golferPool[golfer ^ 1].x >> 10, 0, 49);
    oy = Clamp(g_golferPool[golfer ^ 1].y >> 10, 0, 49);
    if (ox == x1 && oy == y1)
        ox = -1;
    if (x1 == x2 && y1 == y2)
        return direct;

    dy = y1 - y2;
    dx = x1 - x2;
    dist = Math_Distance2D(dx, dy);
    d = ((((CalculateAngle(dx, dy) >> 28) + 1) ^ -7) >> 1) & 7;
    g_scanOpenListX[0] = x1;
    g_scanOpenListY[0] = y1;
    mask = (1 << ((d + 1) & 6)) | (1 << (d & 6));
    head = 0;
    tail = 1;
    g_pathFindDist[x1 * 50 + y1] = 1;
    limit = (g_runModeFlags & 0x100) ? 250 : 160;
    do {
        int cx = g_scanOpenListX[head];
        int cy = g_scanOpenListY[head];
        int idx, cost;
        head = (head + 1) & 0x3ff;
        idx = cx * 50 + cy;
        cost = g_pathFindDist[idx];
        if (cost <= limit) {
            int k;
            for (k = 0; k < 8; k++) {
                int nx = cx + g_dirRowStep[k];
                int ny = cy + g_dirColStep[k];
                int nidx, add, existing, total;
                if (nx < 0 || ny < 0 || nx >= 50 || ny >= 50)
                    continue;
                nidx = nx * 50 + ny;
                existing = g_pathFindDist[nidx];
                add = g_simGolferWalkCostMap[nx][ny] + (k & 1);
                if (nx == ox && ny == oy)
                    add += 2;
                if (!(k & 1) && dist > (int)(((unsigned)((~g_clock) & 0x40) | 0x80) >> 6)
                    && (g_tileExtra[nidx] & TILE_WALKABLE) == TILE_PATH && (g_tileExtra[idx] & TILE_PATH))
                    add = (k != GetCardinalDirection(nx - x1, ny - y1));
                if ((g_runModeFlags & 0x100) || g_golferPool[golfer].holeIdx == CLUBHOUSE_HOLE) {
                    int t = g_gameTileTerrainKind[nx * 50 + ny];
                    if (t == TERRAIN_GREEN || t == TERRAIN_FAIRWAY || t == 0
                        || g_squareTypeTable[t].category == CATEGORY_SAND)
                        add++;
                    else
                        add = (add + 1) >> 1;
                }
                if (g_gameTileTerrainKind[nx * 50 + ny] == TERRAIN_WATER
                    || g_gameTileTerrainKind[nx * 50 + ny] == TERRAIN_OUT_OF_BOUNDS) {
                    if (!(g_tileExtra[nidx] & TILE_PATH) && cost < 0x40)
                        add += 0x10;
                }
                total = add + cost;
                if ((existing == 0 || existing > total) && total <= 255) {
                    g_pathFindDist[nidx] = total;
                    g_scanOpenListX[tail] = nx;
                    g_scanOpenListY[tail] = ny;
                    tail = (tail + 1) & 0x3ff;
                    if (nx == x2 && ny == y2)
                        limit = total;
                }
            }
        }
    } while (head != tail && !quit);

    if (g_runModeFlags & 0x1000) {
        int i, j, sy, sx;
        for (i = 0; i < 50; i++) {
            for (j = 0; j < 50; j++) {
                if (g_pathFindDist[i * 50 + j]) {
                    g_printBuffer[0] = 0;
                    strcat(g_printBuffer, itoa(g_pathFindDist[i * 50 + j], g_numberBuf, 10));
                    TileToScreenCached(i, j, &sx, &sy);
                    DrawTextCenteredAtY(g_printBuffer, sx, sy - 3, COLOR_WHITE);
                }
            }
        }
        g_simScreen.RenderFullSurface(0);
        WaitForInputWithTimeout(0);
    }

    {
        int destIdx, bestCost, rnd, j;
        best = -1;
        destIdx = x2 * 50 + y2;
        bestCost = 2 * g_pathFindDist[destIdx] + 1;
        rnd = g_rng.NextInt(8);
        for (j = 0; j < 8; j++) {
            int dirn = (j + rnd) & 7;
            int nx = x2 + g_dirRowStep[dirn];
            int ny = y2 + g_dirColStep[dirn];
            int cost, diag, v;
            if (nx < 0 || ny < 0 || nx >= 50 || ny >= 50)
                continue;
            if (IsPositionInvalid(nx, ny))
                continue;
            cost = g_pathFindDist[nx * 50 + ny];
            if (cost == 0)
                continue;
            diag = dirn & 1;
            v = 2 * cost + diag;
            if (g_gameTileTerrainKind[x2 * 50 + y2] == TERRAIN_WATER && (g_tileExtra[destIdx] & TILE_PATH) && diag)
                continue;
            if (mask & (1 << (dirn ^ 4)))
                v--;
            if (v < bestCost) {
                best = dirn;
                bestCost = v;
            }
        }

        if (!(g_runModeFlags & 0x100) && g_gameTileTerrainKind[x2 * 50 + y2] == TERRAIN_WATER
            && (g_tileExtra[destIdx] & TILE_PATH)) {
            int gx = g_golferPool[golfer].x - (x2 << 10) - 512;
            int gy = g_golferPool[golfer].y - (y2 << 10) - 512;
            int orig = best;
            int turned = 0;
            switch (best) {
            case 0:
                if (abs(gx) <= 256)
                    break;
                best = (gx > 0) ? -1 : 1;
                turned = 1;
                break;
            case 2:
                if (abs(gy) <= 256)
                    break;
                best = 2 + ((gy > 0) ? -1 : 1);
                turned = 1;
                break;
            case 4:
                if (abs(gx) <= 256)
                    break;
                best = -4 + ((gx > 0) ? 1 : -1);
                turned = 1;
                break;
            case 6:
                if (abs(gy) <= 256)
                    break;
                best = -2 + ((gy > 0) ? 1 : -1);
                turned = 1;
                break;
            }
            if (turned) {
                best &= 7;
                if (best != orig)
                    g_runModeFlags |= 0x80000;
            }
        }
    }
    if (best == -1) {
        if (x1 != x2 || y1 != y2)
            best = GetDirection8(dx, dy);
    }
    return best;
}

int GetTileValue(int row, int col)
{
    if (IsPositionInvalid(row, col))
        return 0;
    int v = g_gameTileTerrainKind[row * 50 + col];
    if (v == TERRAIN_ROUGH)
        return 12;
    if (v == TERRAIN_OUT_OF_BOUNDS)
        return 0;
    if (v == TERRAIN_HOME_SITE)
        return -16;
    if (g_squareTypeTable[v].lieGrade <= 0)
        return -8;
    if (v == TERRAIN_WATER)
        return 32;
    if (g_currentThemeId == THEME_DESERT && v == TERRAIN_WETLANDS)
        return 16;
    if (g_squareTypeTable[v].category == CATEGORY_TREES)
        return g_squareTypeTable[v].price * 5 / 2;
    return g_squareTypeTable[v].price * g_squareTypeTable[v].lieGrade / 2;
}

int CalculateLotScore(int row, int col)
{
    int total = 0;
    int i;
    int colLo = col - 1;
    int rowLo = row - 1;
    int t = rowLo;
    int best;

    for (i = 0; i < 4; i++) {
        total += GetTileValue(t, colLo);
        total += GetTileValue(t, col + 2);
        t++;
    }
    t = col;
    for (i = 0; i < 2; i++) {
        total += GetTileValue(rowLo, t);
        total += GetTileValue(row + 2, t);
        t++;
    }

    best = 0;
    for (i = 1; i < 19; i++) {
        Hole *h = &g_allHolesArray[i];
        if (h->par != 0 && h->rounds != 0) {
            int v = h->moodSum * 1000 / (h->shots / 2 + h->rounds + 4) + (3 - g_level) * 100;
            int nearest;
            int dist;
            if (h->flags & HOLE_TOP_100)
                v += 100;
            if (h->flags & HOLE_TOP_18)
                v += 100;
            nearest = Math_Distance2D(row - h->teeX, col - h->teeY);
            dist = Math_Distance2D(row - h->aimX, col - h->aimY);
            if (dist < nearest)
                nearest = dist;
            if (g_gameFairwayTargetPoints[i][2].x != -1) {
                dist = Math_Distance2D(row - (g_gameFairwayTargetPoints[i][2].x >> 10),
                    col - (g_gameFairwayTargetPoints[i][2].y >> 10));
                if (dist < nearest)
                    nearest = dist;
            }
            v /= nearest + 8;
            if (v > best)
                best = v;
        }
    }
    if (g_simOpenFacilityLvl[SCENERY_MARINA] != 0)
        best += g_simOpenFacilityLvl[SCENERY_MARINA] * best / 3;
    return best * total / 40;
}

void FloodFillWalkableArea(int row, int col)
{
    short f = g_tileExtra[row * GRID_SIZE + col];
    if (f & TILE_REACHED)
        return;
    signed char t = g_gameTileTerrainKind[row * GRID_SIZE + col];
    g_tileExtra[row * GRID_SIZE + col] = f | TILE_REACHED;
    if (g_squareTypeTable[t].lieGrade <= 0)
        return;
    for (int k = 0; k < 8; k += 2) {
        int nrow = g_dirRowStep[k] + row;
        int ncol = g_dirColStep[k] + col;
        short nf = g_tileExtra[nrow * GRID_SIZE + ncol];
        if ((nf & TILE_WALKABLE) && !(nf & TILE_REACHED))
            FloodFillWalkableArea(nrow, ncol);
    }
}

void UpdateWaterShoreDepth(int row, int col)
{
    unsigned char *cell = &g_tileVariationIdx[row * GRID_SIZE + col];
    int k, state, nrow, ncol, t;
    state = *cell;
    for (k = 0; k < 8; k++) {
        nrow = row + g_dirRowStep[k];
        ncol = col + g_dirColStep[k];
        if (IsPositionInvalid(nrow, ncol) != 0)
            continue;
        t = g_gameTileTerrainKind[nrow * GRID_SIZE + ncol];
        if (g_squareTypeTable[t].category != CATEGORY_WATER)
            state = 0;
        if (g_tileVariationIdx[nrow * GRID_SIZE + ncol] == 0 && state != 0)
            state = 1;
    }
    if (state != *cell) {
        *cell = (unsigned char)state;
        for (k = 0; k < 8; k++) {
            nrow = row + g_dirRowStep[k];
            ncol = col + g_dirColStep[k];
            if (IsPositionInvalid(nrow, ncol) != 0)
                continue;
            t = g_gameTileTerrainKind[nrow * GRID_SIZE + ncol];
            if (g_squareTypeTable[t].category == CATEGORY_WATER)
                UpdateWaterShoreDepth(nrow, ncol);
        }
    }
}

void RecalculateAllWaterShoreDepths()
{
    int r, c;
    for (r = 0; r < GRID_SIZE; r++)
        for (c = 0; c < GRID_SIZE; c++)
            if (g_squareTypeTable[g_gameTileTerrainKind[r * GRID_SIZE + c]].category == CATEGORY_WATER)
                g_tileVariationIdx[r * GRID_SIZE + c] = 2;
    for (r = 0; r < GRID_SIZE; r++)
        for (c = 0; c < GRID_SIZE; c++)
            if (g_squareTypeTable[g_gameTileTerrainKind[r * GRID_SIZE + c]].category == CATEGORY_WATER)
                UpdateWaterShoreDepth(r, c);
}

void UpdateBuildingAccessibility()
{
    int col0 = g_buildingTable[0].col;
    int row0 = g_buildingTable[0].row;
    for (int i = 0; i < GRID_SIZE; i++)
        for (int j = 0; j < GRID_SIZE; j++)
            g_tileExtra[i * GRID_SIZE + j] &= ~TILE_REACHED;
    for (int r = row0; r < row0 + g_facilityTypes[SCENERY_CLUBHOUSE].side; r++)
        for (int c = col0; c < col0 + g_facilityTypes[SCENERY_CLUBHOUSE].side; c++)
            if (g_tileExtra[r * GRID_SIZE + c] & TILE_WALKABLE)
                FloodFillWalkableArea(r, c);
    for (int k = 0; k < BUILDING_SLOT_COUNT; k++) {
        if (g_buildingTable[k].kind < FIRST_BUILDING_TYPE)
            continue;
        if (g_buildingTable[k].kind < FACILITIES_ALL) {
            g_buildingTable[k].flags &= ~SLOT_REACHABLE;
            int t = g_buildingTable[k].kind;
            int span = g_facilityTypes[t].side;
            if (t > 5 && t != 7) {
                g_buildingLevelPerKind[t] = Clamp(g_buildingLevelPerKind[t], g_buildingTable[k].count + 1, 0x63);
                span += g_buildingLevelPerKind[t] - 1;
            }
            for (int dr = 0; dr < span; dr++)
                for (int dc = 0; dc < span; dc++)
                    if (g_tileExtra[(g_buildingTable[k].row + dr) * GRID_SIZE + g_buildingTable[k].col + dc]
                        & TILE_REACHED)
                        g_buildingTable[k].flags |= SLOT_REACHABLE;
            if (g_level && (g_runModeFlags & GAME_SANDBOX) == 0)
                continue;
        }
        g_buildingTable[k].flags |= SLOT_REACHABLE;
    }
}

void GetTileElevationMinMax(int row, int col, int *pMax, int *pMin)
{
    int h;
    h = GetTileElevation(row, col, 0);
    *pMax = *pMin = h;
    h = GetTileElevation(row + 1, col, 0);
    if (h > *pMax)
        *pMax = h;
    if (h < *pMin)
        *pMin = h;
    h = GetTileElevation(row + 1, col - 1, 0);
    if (h > *pMax)
        *pMax = h;
    if (h < *pMin)
        *pMin = h;
    h = GetTileElevation(row, col - 1, 0);
    if (h > *pMax)
        *pMax = h;
    if (h < *pMin)
        *pMin = h;
}

void CalculateTileSlopeFlags(int row, int col)
{
    int hi, lo;
    int k;
    signed char *p;
    GetTileElevationMinMax(row, col, &hi, &lo);
    p = &g_tileUphillMap[row][col];
    *p = 0;
    for (k = 0; k < 8; k += 2) {
        int nrow = g_dirRowStep[k] + row;
        int ncol = g_dirColStep[k] + col;
        if (IsPositionInvalid(nrow, ncol))
            continue;
        if (GetCornerElevation(row, col, k - 1, 0) < GetCornerElevation(nrow, ncol, k + 5, 0))
            *p |= 1 << k;
        if (GetCornerElevation(row, col, k + 1, 0) < GetCornerElevation(nrow, ncol, k + 3, 0))
            *p |= 1 << k;
    }
}

int PropagateMinElevation(int row, int col)
{
    int updated = 0;
    int lowest = g_flatSquareAltMap[row * GRID_SIZE + col];
    for (int k = 0; k < 8; k += 2) {
        int nrow = row + g_dirRowStep[k];
        int ncol = col + g_dirColStep[k];
        if (IsPositionInvalid(nrow, ncol))
            continue;
        if (g_gameTileTerrainKind[nrow * GRID_SIZE + ncol] != g_gameTileTerrainKind[row * GRID_SIZE + col])
            continue;
        if (g_flatSquareAltMap[nrow * GRID_SIZE + ncol] >= lowest)
            continue;
        lowest = g_flatSquareAltMap[nrow * GRID_SIZE + ncol];
        g_flatSquareAltMap[row * GRID_SIZE + col] = lowest;
        updated = 1;
    }
    return updated;
}

int PropagateMaxElevation(int row, int col, int sameVariation)
{
    int changed = 0;
    int highest = g_flatSquareAltMap[row * GRID_SIZE + col];
    int k;
    for (k = 0; k < 8; k += 2) {
        int nrow = g_dirRowStep[k] + row;
        int ncol = g_dirColStep[k] + col;
        if (IsPositionInvalid(nrow, ncol))
            continue;
        if (g_gameTileTerrainKind[nrow * GRID_SIZE + ncol] != g_gameTileTerrainKind[row * GRID_SIZE + col])
            continue;
        if (sameVariation && g_tileVariationIdx[nrow * GRID_SIZE + ncol] != g_tileVariationIdx[row * GRID_SIZE + col])
            continue;
        if (g_flatSquareAltMap[nrow * GRID_SIZE + ncol] > highest) {
            highest = g_flatSquareAltMap[nrow * GRID_SIZE + ncol];
            g_flatSquareAltMap[row * GRID_SIZE + col] = highest;
            changed = 1;
        }
    }
    return changed;
}

void RebuildTerrainElevation()
{
    int mx, mn;
    int y, x, total;
    g_editedTileRowIdx = -1;
    memset(g_flatSquareAltMap, 0, sizeof(g_flatSquareAltMap));
    memset(g_cornerElevation, 0, sizeof(g_cornerElevation));
    for (y = 0; y < GRID_SIZE; y++) {
        for (x = 0; x < GRID_SIZE; x++) {
            g_cornerElevation[y][x][1] = GetCornerElevation(y, x, 1, 0);
            g_cornerElevation[y][x][3] = GetCornerElevation(y, x, CORNER_SE, 0);
            g_cornerElevation[y][x][5] = GetCornerElevation(y, x, 5, 0);
            g_cornerElevation[y][x][7] = GetCornerElevation(y, x, CORNER_NW, 0);
            if (g_squareTypeTable[g_gameTileTerrainKind[y * GRID_SIZE + x]].flags & (ELEV_FROM_MIN | ELEV_FROM_MAX)) {
                GetTileElevationMinMax(y, x, &mx, &mn);
                if (g_squareTypeTable[g_gameTileTerrainKind[y * GRID_SIZE + x]].flags & ELEV_FROM_MIN)
                    g_flatSquareAltMap[y * GRID_SIZE + x] = mn;
                if (g_squareTypeTable[g_gameTileTerrainKind[y * GRID_SIZE + x]].flags & ELEV_FROM_MAX)
                    g_flatSquareAltMap[y * GRID_SIZE + x] = mx;
            }
        }
    }
    do {
        total = 0;
        for (y = 0; y < GRID_SIZE; y++) {
            for (x = 0; x < GRID_SIZE; x++) {
                if (g_squareTypeTable[g_gameTileTerrainKind[y * GRID_SIZE + x]].flags & ELEV_PROPAGATE) {
                    if (g_squareTypeTable[g_gameTileTerrainKind[y * GRID_SIZE + x]].flags & ELEV_FROM_MIN)
                        total += PropagateMinElevation(y, x);
                    if (g_squareTypeTable[g_gameTileTerrainKind[y * GRID_SIZE + x]].flags & ELEV_FROM_MAX)
                        total += PropagateMaxElevation(y, x,
                            g_squareTypeTable[g_gameTileTerrainKind[y * GRID_SIZE + x]].kind == 0x10);
                }
            }
        }
    } while (total);
    for (y = 0; y < GRID_SIZE; y++)
        for (x = 0; x < GRID_SIZE; x++)
            CalculateTileSlopeFlags(y, x);
    g_tileUphillMap[0][0] = (g_tileUphillMap[0][0] & ~VIEW_TILES_SYNCED) | VIEW_ELEVATION_REBUILT;
    g_runModeFlags &= ~GAME_ROUTE_RECORDED;
}

int TileToScreenCached(int row, int col, int *px, int *py)
{
    if (row >= 0 && row < GRID_SIZE && col >= 0 && col < GRID_SIZE) {
        short v = g_cellScreenPosX[row][col];
        if (v != 0) {
            if (v != TILE_OFFSCREEN) {
                *px = v;
                *py = g_cellScreenPosY[row][col];
                return 1;
            }
        } else {
            WorldToScreen(row * TILE_WORLD_SIZE + 512, col * TILE_WORLD_SIZE + 512, px, py, 0);
            if (*px >= -64 && *px < g_screenWidth + 64 && *py >= -42 && *py < g_screenHeight + 42) {
                g_cellScreenPosX[row][col] = (short)*px;
                g_cellScreenPosY[row][col] = (short)*py;
                return 1;
            }
            g_cellScreenPosX[row][col] = TILE_OFFSCREEN;
        }
    }
    return 0;
}

int GetTerrainHeightAtWorld(int wx, int wy)
{
    int row = wx >> 10;
    int col = wy >> 10;
    int squareFlags = g_squareTypeTable[g_gameTileTerrainKind[row * 50 + col]].flags;
    int hi, lo;
    int corner5, corner7, corner1, corner3;
    int fx, fy;
    if (squareFlags & ELEV_FLAT_ZERO)
        return 0;
    if (squareFlags & ELEV_FROM_MIN) {
        GetTileElevationMinMax(row, col, &hi, &lo);
        return (lo - 3) * 16;
    }
    if (squareFlags & ELEV_FROM_MAX) {
        GetTileElevationMinMax(row, col, &hi, &lo);
        return (hi - 3) * 16;
    }
    corner5 = GetCornerElevation(row, col, 5, 1) - 3;
    corner7 = GetCornerElevation(row, col, CORNER_NW, 1) - 3;
    corner1 = GetCornerElevation(row, col, 1, 1) - 3;
    corner3 = GetCornerElevation(row, col, CORNER_SE, 1) - 3;
    if (corner5 == corner7 && corner5 == corner1 && corner5 == corner3)
        return corner5 * 16;
    fx = wx - (row << 10);
    fy = wy - (col << 10);
    return (((1024 - fx) * corner7 + fx * corner1) * (1024 - fy) + ((1024 - fx) * corner5 + fx * corner3) * fy) * 16
           / 1024 / 1024;
}

int WorldToScreen(int wx, int wy, int *px, int *py, int margin)
{
    int cx = ((wx - (g_cameraTileX << 10) - 0x200) * g_cameraZoom) >> 7;
    int cy = ((wy - (g_cameraTileY << 10) - 0x200) * g_cameraZoom) >> 7;
    switch (g_screenWidth) {
    case 0x400:
        cx = (cx * 80) / 64;
        cy = (cy * 52) / 40;
        break;
    case 0x500:
        cx = (cx * 104) / 64;
        cy = (cy * 68) / 40;
        break;
    }
    if (g_camDirIdx == 0) {
        *px = g_screenWidth / 2 + cy + g_cameraZoom * 8 + cx;
        *py = (cy * 20) / 32 - (cx * 20) / 32 + g_screenHeight / 2;
    }
    if (g_camDirIdx == QUARTER_TURN) {
        *px = g_screenWidth / 2 - cx + cy;
        *py = (-cx * 20) / 32 - (cy * 20) / 32 + g_screenHeight / 2 - g_cameraZoom * 5;
    }
    if (g_camDirIdx == HALF_TURN) {
        *px = g_screenWidth / 2 - g_cameraZoom * 8 - cy - cx;
        *py = (cx * 20) / 32 - (cy * 20) / 32 + g_screenHeight / 2;
    }
    if (g_camDirIdx == THREE_QUARTER_TURN) {
        *px = g_screenWidth / 2 - cy + cx;
        *py = (cy * 20) / 32 + (cx * 20) / 32 + g_screenHeight / 2 + g_cameraZoom * 5;
    }
    if (*px >= -margin && *px < g_screenWidth + margin && *py >= -(margin / 2) && *py < margin / 2 + g_screenHeight) {
        {
            int row = wx >> 10;
            int col = wy >> 10;
            int squareFlags = g_squareTypeTable[g_gameTileTerrainKind[row * 50 + col]].flags;
            if ((squareFlags & ELEV_FLAT_ZERO) == 0) {
                int hi, lo;
                if (squareFlags & ELEV_FROM_MIN) {
                    GetTileElevationMinMax(row, col, &hi, &lo);
                    *py += -(((lo - 3) * g_elevStep * g_cameraZoom) / 4);
                } else if (squareFlags & ELEV_FROM_MAX) {
                    hi = g_flatSquareAltMap[row * 50 + col];
                    *py -= ((hi - 3) * g_elevStep * g_cameraZoom) / 4;
                } else {
                    int corner5 = GetCornerElevation(row, col, 5, 1) - 3;
                    int corner7 = GetCornerElevation(row, col, CORNER_NW, 1) - 3;
                    int corner1 = GetCornerElevation(row, col, 1, 1) - 3;
                    int corner3 = GetCornerElevation(row, col, CORNER_SE, 1) - 3;
                    if (corner5 == corner7 && corner5 == corner1 && corner5 == corner3) {
                        *py += -((g_elevStep * g_cameraZoom * corner5) / 4);
                    } else {
                        int fx = wx - (row << 10);
                        int fy = wy - (col << 10);
                        *py += ((-((((0x400 - fx) * corner7 + fx * corner1) * (0x400 - fy)
                                       + ((0x400 - fx) * corner5 + fx * corner3) * fy)
                                     * g_elevStep * g_cameraZoom)
                                   / 1024))
                               / 1024 / 4;
                    }
                }
            }
        }
        if (g_zoomDouble) {
            *px = *px * 2 - 400;
            *py = *py * 2 - 300;
        }
        if (*px >= -margin && *px < g_screenWidth + margin && *py >= -(margin / 2) && *py < margin / 2 + g_screenHeight)
            return 1;
    }
    return 0;
}

void ScreenToTileCoords(int screenX, int screenY, int *pRow, int *pCol, int useWorld)
{
    int oX, oY;
    int sx, sy;
    int tx, ty;
    int best, d;
    int i;
    if (g_zoomDouble != 0) {
        screenX = (screenX - 400) / 2 + 400;
        screenY = (screenY - 300) / 2 + 300;
    }
    sx = screenX - g_screenWidth / 2 - 0x40;
    sy = screenY - g_screenHeight / 2;
    switch (g_camDirIdx) {
    case QUARTER_TURN:
        sx += 0x20;
        sy += 0x14;
        break;
    case HALF_TURN: sx += 0x40; break;
    case THREE_QUARTER_TURN:
        sx += 0x20;
        sy -= 0x14;
        break;
    }
    sy = (sy << 5) / 40;
    if (g_camDirIdx == 0) {
        *pRow = (((g_cameraTileX * 2 + 1) * g_tileSpan - sy * 2 + sx) / g_tileSpan) / 2;
        *pCol = ((g_tileSpan + (g_cameraTileY * g_tileSpan + sy) * 2 + sx) / g_tileSpan) / 2;
    }
    if (g_camDirIdx == QUARTER_TURN) {
        *pRow = (((g_cameraTileX * 2 + 1) * g_tileSpan - sy * 2 - sx) / g_tileSpan) / 2;
        *pCol = (((g_cameraTileY * 2 + 1) * g_tileSpan - sy * 2 + sx) / g_tileSpan) / 2;
    }
    if (g_camDirIdx == HALF_TURN) {
        *pRow = (((g_tileSpan * g_cameraTileX + sy) * 2 - sx + g_tileSpan) / g_tileSpan) / 2;
        *pCol = (((g_cameraTileY * 2 + 1) * g_tileSpan - sy * 2 - sx) / g_tileSpan) / 2;
    }
    if (g_camDirIdx == THREE_QUARTER_TURN) {
        *pRow = ((g_tileSpan + (g_tileSpan * g_cameraTileX + sy) * 2 + sx) / g_tileSpan) / 2;
        *pCol = (((g_cameraTileY * g_tileSpan + sy) * 2 - sx + g_tileSpan) / g_tileSpan) / 2;
    }
    do {
        tx = *pRow;
        ty = *pCol;
        if (useWorld == 0)
            TileToScreenCached(tx, ty, &oX, &oY);
        else
            WorldToScreen(tx << 10, (ty + 1) << 10, &oX, &oY, 0);
        best = Math_Distance2D(screenX - oX, (screenY - oY) * 2);
        for (i = 0; i < 8; i++) {
            if (useWorld == 0)
                TileToScreenCached(g_dirRowStep[i] + tx, g_dirColStep[i] + ty, &oX, &oY);
            else
                WorldToScreen((g_dirRowStep[i] + tx) << 10, (g_dirColStep[i] + ty + 1) << 10, &oX, &oY, 0);
            d = Math_Distance2D(screenX - oX, (screenY - oY) * 2);
            if (d < best) {
                best = d;
                *pRow = g_dirRowStep[i] + tx;
                *pCol = g_dirColStep[i] + ty;
            }
        }
    } while (*pRow != tx || *pCol != ty);
}
