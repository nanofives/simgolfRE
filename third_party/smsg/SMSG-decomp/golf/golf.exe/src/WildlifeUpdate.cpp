#include "jgldefs.h"
#include "WildlifeUpdate.h"
#include "NewGame.h"
#include "Screenshot.h"

void UpdateAndRenderWildlife()
{
    int sy;
    int sx;
    int frame;
    int i;
    int facing;
    int bank;

    for (i = 0; i < 128; i++) {
        if (g_gameAnimals[i].kind != -1 && (g_configBitMask & 0x20)) {
            if (g_gameAnimals[i].kind >= 9)
                g_gameAnimals[i].kind = 1;
            if (g_gameAnimals[i].state < 0) {
                g_gameAnimals[i].sprite = 0x103;
                frame = 0;
                facing = g_gameAnimals[i].dir;
            } else {
                bank = g_gameAnimals[i].kind + g_gameAnimals[i].sprite;
                frame = g_gameAnimals[i].animFrame;
                if (!(g_runModeFlags & GAME_PAUSED)) {
                    if (g_flicBankFrameLens[bank] == 0)
                        frame = 0;
                    else
                        frame = ++g_gameAnimals[i].animFrame % g_flicBankFrameLens[bank];

                    if (g_gameAnimals[i].sprite == 0x103) {
                        int row = (g_gameAnimals[i].x + g_dirRowStep[g_gameAnimals[i].dir] * 1024 / 3) >> 10;
                        int col = (g_gameAnimals[i].y + g_dirColStep[g_gameAnimals[i].dir] * 1024 / 3) >> 10;
                        if (IsPositionInvalid(row, col) || g_gameTileTerrainKind[row * 50 + col] == TERRAIN_WATER
                            || g_gameTileTerrainKind[row * 50 + col] == TERRAIN_BUILDING) {
                            g_gameAnimals[i].dir = (g_gameAnimals[i].dir + 1) & 7;
                            if (g_rng.NextInt(100) == 0)
                                g_gameAnimals[i].kind = -1;
                        }
                        if (!(g_gameAnimals[i].state & 2)
                            && (g_gameAnimals[i].leader == -1 || !(g_gameAnimals[g_gameAnimals[i].leader].state & 2))) {
                            g_gameAnimals[i].x +=
                                (g_dirRowStep[g_gameAnimals[i].dir] << 6) / (g_gameAnimals[i].kind ? 4 : 2);
                            g_gameAnimals[i].y +=
                                (g_dirColStep[g_gameAnimals[i].dir] << 6) / (g_gameAnimals[i].kind ? 4 : 2);
                        } else {
                            g_gameAnimals[i].x += g_dirRowStep[g_gameAnimals[i].dir] << 6;
                            g_gameAnimals[i].y += g_dirColStep[g_gameAnimals[i].dir] << 6;
                        }
                    }

                    if (frame == 0) {
                        int d;
                        if (g_gameAnimals[i].leader == -1 && g_gameAnimals[g_gameAnimals[i].leader].kind != -1) {
                            int gi;
                            if (g_rng.NextInt(3) == 0)
                                g_gameAnimals[i].state &= ~2;
                            for (gi = 0; gi < 152; gi++) {
                                if (g_golferPool[gi].holeIdx != -1
                                    && (Math_Distance2D(g_golferPool[gi].x - g_gameAnimals[i].x,
                                            g_golferPool[gi].y - g_gameAnimals[i].y)
                                            < 0x800
                                        || (g_golferPool[gi].ballSpeed > 0 && g_golferPool[gi].ballZ < 0x96
                                            && Math_Distance2D(g_golferPool[gi].ballX - g_gameAnimals[i].x,
                                                   g_golferPool[gi].ballY - g_gameAnimals[i].y)
                                                   < 0x800))) {
                                    if (!(g_gameAnimals[i].state & 2) && g_golferPool[gi].talkTimer == 0
                                        && g_golferPool[gi].msgLine != 0x27)
                                        SetGolferAction(gi, 0x27, g_gameAnimals[i].kind);
                                    g_gameAnimals[i].state |= 2;
                                    g_gameAnimals[i].dir = GetDirection8(g_gameAnimals[i].x - g_golferPool[gi].x,
                                        g_gameAnimals[i].y - g_golferPool[gi].y);
                                    if (g_rng.NextInt(3) == 0)
                                        PlayCritterCall(g_gameAnimals[i].kind, g_gameAnimals[i].x, g_gameAnimals[i].y);
                                }
                            }
                            d = g_rng.NextInt(8);
                            if (g_gameAnimals[i].sprite == 0x103 && g_rng.NextInt(3))
                                d = g_gameAnimals[i].dir;
                            if (GetTileAtPosition(g_gameAnimals[i].x + g_dirRowStep[d] * 1024,
                                    g_gameAnimals[i].y + g_dirColStep[d] * 1024)
                                    != g_animalKinds[g_gameAnimals[i].kind].habitat
                                && GetTileAtPosition(g_gameAnimals[i].x + g_dirRowStep[d] * 1024,
                                       g_gameAnimals[i].y + g_dirColStep[d] * 1024)
                                       != TERRAIN_ROUGH
                                && GetTileAtPosition(g_gameAnimals[i].x, g_gameAnimals[i].y)
                                       == g_animalKinds[g_gameAnimals[i].kind].habitat)
                                d = -1;
                            if (g_gameAnimals[i].state & 2) {
                                d = g_rng.NextInt(3);
                                d = (g_gameAnimals[i].dir + d - 1) & 7;
                                g_gameAnimals[i].dir = d;
                                g_gameAnimals[i].sprite = 0x103;
                            }
                        } else {
                            int dx = g_gameAnimals[g_gameAnimals[i].leader].x - g_gameAnimals[i].x;
                            int dy = g_gameAnimals[g_gameAnimals[i].leader].y - g_gameAnimals[i].y;
                            int dist = Math_Distance2D(dx, dy);
                            if (g_gameAnimals[g_gameAnimals[i].leader].state & 2) {
                                g_gameAnimals[i].sprite = 0x103;
                                dist = 0x2800;
                            }
                            if (dist > ((g_gameAnimals[i].sprite != 0x103) + 2) * 1024
                                           / ((g_gameAnimals[i].kind != 0) + 2)) {
                                d = GetDirection8(dx, dy);
                                if (dist < 0x800)
                                    d = (d + i % 3 - 1) & 7;
                                g_gameAnimals[i].dir = d;
                            } else
                                d = -1;
                        }

                        int reverseTransition = g_gameAnimals[i].sprite == 0x11e && g_rng.NextInt(2);
                        if (!reverseTransition && g_gameAnimals[i].sprite != 0x115) {
                            if (g_gameAnimals[i].sprite == 0x10c && g_rng.NextInt(10) == 0)
                                g_gameAnimals[i].sprite = 0x115;
                            if (g_gameAnimals[i].sprite == 0x10c && g_rng.NextInt(2) == 0 && d != -1) {
                                g_gameAnimals[i].sprite = 0x103;
                                g_gameAnimals[i].dir = d;
                            }
                            if (g_gameAnimals[i].sprite == 0x103
                                && (g_rng.NextInt(5) == 0 || g_gameAnimals[i].dir != d))
                                g_gameAnimals[i].sprite = 0x10c;
                        } else if (reverseTransition) {
                            g_gameAnimals[i].sprite = 0x115;
                            g_gameAnimals[i].state |= 1;
                        } else {
                            g_gameAnimals[i].sprite = (g_gameAnimals[i].state & 1) ? 0x10c : 0x11e;
                            g_gameAnimals[i].state &= ~1;
                        }
                    }
                }
                g_gameAnimals[i].animFrame = frame;
                if (g_gameAnimals[i].sprite == 0x115 && (g_gameAnimals[i].state & 1))
                    frame = g_flicBankFrameLens[g_gameAnimals[i].kind + g_gameAnimals[i].sprite] - frame - 1;
                facing = (g_camDirIdx - g_gameAnimals[i].dir - 2) & 7;
                if (g_gameAnimals[i].sprite != 0x103 && g_gameAnimals[i].kind != 4)
                    facing /= 2;
            }

            bank = g_gameAnimals[i].kind + g_gameAnimals[i].sprite;
            if (g_gameAnimals[i].kind != -1 && WorldToScreen(g_gameAnimals[i].x, g_gameAnimals[i].y, &sx, &sy, 0)) {
                int size = 4;
                if (g_gameAnimals[i].leader != -1) {
                    size = 3;
                    if (g_gameAnimals[i].kind == 4)
                        size = 2;
                }
                g_curSprite = GetFlicBankSprite(bank, frame, facing);
                signed char hop[23] = {-55, -54, -52, -49, -45, -40, -34, -27, -19, -10, 0, -4, -7, -8, -7, -4, 0, -2,
                    0, 0, 0, 0, 0};
                if (g_gameAnimals[i].state < 0) {
                    int t = g_gameAnimals[i].state + 20;
                    g_curSprite->draw_translucent(g_pMenu, sx - 30, sy + hop[t] * 4 - 40, t * 0.04 + 0.2,
                        &g_palettes[g_gameAnimals[i].kind + 120], 1);
                    if (++g_gameAnimals[i].state == 0)
                        PlayCritterCall(g_gameAnimals[i].kind, g_gameAnimals[i].x, g_gameAnimals[i].y);
                } else
                    AddSpriteToRenderQueue(sx, sy, sy, 0x1e, 0x28, g_curSprite, size, g_gameAnimals[i].kind + 0x78);
                if (g_runModeFlags & GAME_DEBUG_OVERLAY) {
                    g_printBuffer[0] = 0;
                    strcat(g_printBuffer, itoa(i, g_numberBuf, 10));
                    DrawTextCenteredWithShadow(g_printBuffer, sx, sy + 4, COLOR_WHITE);
                }
            }
            if (GetTileAtPosition(g_gameAnimals[i].x, g_gameAnimals[i].y) == TERRAIN_OUT_OF_BOUNDS)
                g_gameAnimals[i].kind = -1;
        }
    }

    g_runModeFlags |= 1;
    for (i = 0; i < 4; i++) {
        switch (g_backgroundActors[i].type) {
        case 0:
            if (g_backgroundActors[i].x != -1) {
                if (WorldToScreen(g_backgroundActors[i].x, g_backgroundActors[i].y, &sx, &sy, 0)
                    && GetTileAtPosition(g_backgroundActors[i].x, g_backgroundActors[i].y) != TERRAIN_OUT_OF_BOUNDS) {
                    g_curSprite = GetFlicBankSprite(0xf1, 0, (g_clock / 1000) & 3);
                    AddSpriteToRenderQueue(g_backgroundActors[i].height / 2 + sx - 0x16,
                        sy - g_backgroundActors[i].height / 2, sy - 1, 0x8c, 0x8c, g_curSprite, SPRITE_FULL_SCALE,
                        0x54);
                    g_curSprite = GetFlicBankSprite(0xf0, 0, (g_backgroundActors[i].y >> 10) & 3);
                    AddSpriteToRenderQueue(sx, sy - g_backgroundActors[i].height + 0x16, sy, 0x8c, 0x8c, g_curSprite,
                        SPRITE_FULL_SCALE, 0x54);
                }
                if (!(g_runModeFlags & GAME_PAUSED)) {
                    g_backgroundActors[i].x += (Clamp(g_backgroundActors[i].height, 0, 0x28) << 6) / 200;
                    if (g_backgroundActors[i].x > 0xcc00)
                        g_backgroundActors[i].x = -1;
                    int row = g_backgroundActors[i].x >> 10;
                    int col = g_backgroundActors[i].y >> 10;
                    if ((g_clock & 3) == 0) {
                        switch (g_clock / (col * 4 + 0x100) % 5) {
                        case 0:
                            if (g_backgroundActors[i].height == 0)
                                PlayPositionedSound(0x39, g_backgroundActors[i].x, g_backgroundActors[i].y, 0);
                            if (g_backgroundActors[i].height < GetTileElevation(row, col, 0) * g_elevStep + 0x3c)
                                g_backgroundActors[i].height++;
                            break;
                        case 1:
                        case 4:
                            if (g_backgroundActors[i].height == 0)
                                PlayPositionedSound(0x39, g_backgroundActors[i].x, g_backgroundActors[i].y, 0);
                            if (g_backgroundActors[i].height < GetTileElevation(row, col, 0) * g_elevStep + 0x28)
                                g_backgroundActors[i].height++;
                            break;
                        case 2:
                        case 3:
                            if (g_squareTypeTable[g_gameTileTerrainKind[row * 50 + col]].category != CATEGORY_TREES
                                && g_squareTypeTable[g_gameTileTerrainKind[(row + 1) * 50 + col]].category
                                       != CATEGORY_TREES
                                && g_gameTileTerrainKind[row * 50 + col] != TERRAIN_BUILDING
                                && g_gameTileTerrainKind[(row + 1) * 50 + col] != TERRAIN_BUILDING
                                && g_gameTileTerrainKind[row * 50 + col] != TERRAIN_HOME_SITE
                                && g_gameTileTerrainKind[(row + 1) * 50 + col] != TERRAIN_HOME_SITE
                                && g_squareTypeTable[g_gameTileTerrainKind[row * 50 + col]].category != CATEGORY_WATER
                                && g_squareTypeTable[g_gameTileTerrainKind[row * 50 + col]].lieGrade > 0) {
                                if (g_backgroundActors[i].height > 0)
                                    g_backgroundActors[i].height--;
                            } else {
                                if (g_backgroundActors[i].height < 0x28)
                                    g_backgroundActors[i].height++;
                            }
                            break;
                        }
                    }
                    if (g_backgroundActors[i].y > 0xcc00)
                        g_backgroundActors[i].x = -1;
                }
            } else if (g_rng.NextInt(100) == 0 && g_clock > TICKS_PER_YEAR) {
                g_backgroundActors[i].x = 0;
                g_backgroundActors[i].y = g_rng.NextInt(50) << 10;
                g_backgroundActors[i].height = 0x28;
            }
            break;

        case 2:
            if (g_backgroundActors[i].x != -1) {
                if (WorldToScreen(g_backgroundActors[i].x, g_backgroundActors[i].y, &sx, &sy, 0)
                    && GetTileAtPosition(g_backgroundActors[i].x, g_backgroundActors[i].y) != TERRAIN_OUT_OF_BOUNDS) {
                    g_curSprite = GetFlicBankSprite(0xf3, 0, (g_camDirIdx / 2 - 2) & 3);
                    AddSpriteToRenderQueue(sx - 0x2c, sy, sy - 1, 0xf0, 0xf0, g_curSprite, SPRITE_FULL_SCALE, 0x3a);
                    g_curSprite = GetFlicBankSprite(0xf2, 0, (g_camDirIdx / 2 - 2) & 3);
                    AddSpriteToRenderQueue(sx, sy - (g_backgroundActors[i].height - 0x20) * g_cameraZoom / 4, sy, 0xf0,
                        0xf0, g_curSprite, SPRITE_FULL_SCALE, 0x3a);
                }
                if (!(g_runModeFlags & GAME_PAUSED)) {
                    g_backgroundActors[i].x += (Clamp(g_backgroundActors[i].height, 0, 0x28) << 6) / 128;
                    if (g_backgroundActors[i].x > 0xcc00)
                        g_backgroundActors[i].x = -1;
                    if (g_backgroundActors[i].y > 0xcc00)
                        g_backgroundActors[i].x = -1;
                }
            } else if (g_rng.NextInt(100) == 0 && g_clock > TICKS_PER_YEAR) {
                g_backgroundActors[i].x = 0;
                g_backgroundActors[i].y = g_rng.NextInt(50) << 10;
                g_backgroundActors[i].height = 0x50;
            }
            break;

        case 3:
            if (g_backgroundActors[i].x != -1) {
                if (WorldToScreen(g_backgroundActors[i].x, g_backgroundActors[i].y, &sx, &sy, 0)
                    && GetTileAtPosition(g_backgroundActors[i].x, g_backgroundActors[i].y) != TERRAIN_OUT_OF_BOUNDS) {
                    int f = g_clock % 0x30;
                    bank = 0x206;
                    if (f >= g_flicBankFrameLens[0x207] * 2)
                        f = g_clock % g_flicBankFrameLens[0x206];
                    else {
                        bank = 0x207;
                        f %= g_flicBankFrameLens[0x207];
                    }
                    g_curSprite = GetFlicBankSprite(bank, f, (g_camDirIdx / 2 - g_backgroundActors[i].dir / 2 - 1) & 3);
                    AddSpriteToRenderQueue(sx, sy, sy + g_tileSpan, 0x8c, 0x8c, g_curSprite, SPRITE_FULL_SCALE, 0x3c);
                }
                if (!(g_runModeFlags & GAME_PAUSED)) {
                    g_backgroundActors[i].x += g_dirRowStep[g_backgroundActors[i].dir] << 7;
                    g_backgroundActors[i].y += g_dirColStep[g_backgroundActors[i].dir] << 7;
                    if (g_backgroundActors[i].x > 0xcc00 || g_backgroundActors[i].x < 0)
                        g_backgroundActors[i].x = -1;
                    if (g_backgroundActors[i].y > 0xcc00 || g_backgroundActors[i].y < 0)
                        g_backgroundActors[i].x = -1;
                }
            } else if (g_rng.NextInt(100) == 0) {
                switch (g_rng.NextInt(4)) {
                case 0:
                    g_backgroundActors[i].x = 0;
                    g_backgroundActors[i].y = g_rng.NextInt(50) << 10;
                    g_backgroundActors[i].dir = 2;
                    break;
                case 1:
                    g_backgroundActors[i].y = 0;
                    g_backgroundActors[i].x = g_rng.NextInt(50) << 10;
                    g_backgroundActors[i].dir = 4;
                    break;
                case 2:
                    g_backgroundActors[i].x = 0xc800;
                    g_backgroundActors[i].y = g_rng.NextInt(50) << 10;
                    g_backgroundActors[i].dir = 6;
                    break;
                case 3:
                    g_backgroundActors[i].y = 0xc800;
                    g_backgroundActors[i].x = g_rng.NextInt(50) << 10;
                    g_backgroundActors[i].dir = 0;
                    break;
                }
            }
            break;

        case 1:
            if (g_backgroundActors[i].x != -1) {
                if (!(g_runModeFlags & GAME_PAUSED)) {
                    int step = 0x80 / (((g_clock >> 8) & 3) + 3);
                    g_backgroundActors[i].x += g_dirRowStep[g_backgroundActors[i].dir] * step;
                    g_backgroundActors[i].y += g_dirColStep[g_backgroundActors[i].dir] * step;
                    if ((g_clock & 7) == 0 && (g_backgroundActors[i].dir != 0 || g_backgroundActors[i].y > 0x800)
                        && GetTileAtPosition(g_backgroundActors[i].x + g_dirRowStep[g_backgroundActors[i].dir] * 0x600,
                               g_backgroundActors[i].y + g_dirColStep[g_backgroundActors[i].dir] * 0x600)
                               != TERRAIN_WATER) {
                        int bestScore = 99;
                        int best = g_backgroundActors[i].dir;
                        int k;
                        for (k = 0; k < 8; k += 2) {
                            if (k != g_backgroundActors[i].dir
                                && GetTileAtPosition(g_backgroundActors[i].x + g_dirRowStep[k] * 1024,
                                       g_backgroundActors[i].y + g_dirColStep[k] * 1024)
                                       == TERRAIN_WATER
                                && GetTileAtPosition(g_backgroundActors[i].x + g_dirRowStep[k] * 2048,
                                       g_backgroundActors[i].y + g_dirColStep[k] * 2048)
                                       == TERRAIN_WATER) {
                                int turn = (g_backgroundActors[i].dir - k) & 7;
                                if (turn > 4)
                                    turn = 8 - turn;
                                int score = g_rng.NextInt(15) + turn * 16;
                                if (score < bestScore) {
                                    bestScore = score;
                                    best = k;
                                }
                            }
                        }
                        g_backgroundActors[i].dir = best;
                    }
                    if (g_backgroundActors[i].y < 0x200 && g_dirColStep[g_backgroundActors[i].dir] == -1)
                        g_backgroundActors[i].x = -1;
                }
                if (WorldToScreen(g_backgroundActors[i].x, g_backgroundActors[i].y, &sx, &sy, 0)) {
                    bank = 0x1a1;
                    if (g_gameAnimNumAngles[0x1a1] == 8)
                        g_curSprite = GetFlicBankSprite(bank, g_clock / 2 % g_flicBankFrameLens[0x1a1],
                            (g_camDirIdx - g_backgroundActors[i].dir - 2) & 7);
                    else
                        g_curSprite = GetFlicBankSprite(bank, g_clock / 2 % g_flicBankFrameLens[0x1a1],
                            ((g_camDirIdx - g_backgroundActors[i].dir - 2) >> 1) & 3);
                    AddSpriteToRenderQueue(sx, sy, sy, 0x50, 0x50, g_curSprite, SPRITE_FULL_SCALE, 0x53);
                    if ((g_clock & 0x7f) == 0)
                        PlayPositionedSound(0xd1, g_backgroundActors[i].x, g_backgroundActors[i].y, 0);
                } else if (g_rng.NextInt(200) == 0)
                    g_backgroundActors[i].x = -1;
            } else if (g_rng.NextInt(100) == 0) {
                int r = g_rng.NextInt(50);
                if (g_squareTypeTable[g_gameTileTerrainKind[r * 50 + 0]].category == CATEGORY_WATER) {
                    g_backgroundActors[i].x = (r << 10) + 0x200;
                    g_backgroundActors[i].y = 0x200;
                    g_backgroundActors[i].dir = 4;
                }
            }
            break;
        }
    }
    g_runModeFlags &= ~1;
}


void InitNewGameData()
{
    int i;

    memset(g_buildingLevelPerKind, 0, 0x50);
    memset(g_simOpenFacilityLvl, 0, 0x50);
    memset(g_tileAnimFrame, 0, 0x9c4);
    memset(g_golferPool, 0, 0x9c00);
    memset(g_allHolesArray, 0, 0x28a0);
    memset(g_staffSlots, 0, 0x1300);
    memset(g_buildingTable, 0, 0x1000);
    memset(g_accomp, 0, 0x420);
    for (i = 0; i < 256; i++)
        g_buildingTable[i].kind = -1;
    memset(g_namedSceneryArray, -1, 0x320);
    memset(g_backgroundActors, -1, 0x40);
    memset(g_tileExtra, 0, 0x1388);
    memset(g_tileVariationIdx, 0, 0x9c4);
    memset(g_golferHistoryPerCharacter, 0, 0xe70);
    memset(g_trailSavedPlotPosX, -1, 0x260);
    memset(g_demolishPreviousRecs, -1, 0x9c4);
    memset(g_bestTenScoresTable, 0, 0x28);
    memset(g_gameFairwayTargetPoints, -1, 0x1c8);
    memset(g_ballStopCountMap, 0, 0x9c4);
    g_skillMasks[0] = 0;
    ClearStringPool();
    memset(g_monthEventKindHistory, 0, 0x3e8);
    for (i = 0; i < 152; i++)
        g_golferPool[i].partner = -1;

    i = 0;
    for (;;) {
        int ok = 1;
        int kind = 3;
        int t = g_rng.NextInt(0x4c);
        if (t % 3 != i % 3)
            ok = 0;
        if (g_golferHistoryPerCharacter[t].flags)
            ok = 0;
        if (g_golferTypeArray[t].ageBits & GOLFER_TYPE_FEMALE) {
            kind = 6;
        } else {
            unsigned char a = g_golferTypeArray[t].caps;
            kind = 1;
            if (a & 0x11)
                kind = 3;
            if (a & 0xc)
                kind |= 4;
            if (kind == 1)
                kind |= (t & 1) ? 2 : 4;
        }
        switch (i / 3) {
        case 0:
            if (kind != 5)
                continue;
            break;
        case 1:
            if (kind != 3)
                continue;
            break;
        case 2:
            if (kind != 6)
                continue;
            break;
        case 3:
            if (kind != 7)
                continue;
            break;
        }
        if (!ok)
            continue;
        i++;
        g_golferHistoryPerCharacter[t].flags = 1;
        if (i >= 12)
            break;
    }

    for (i = 0; i < 76; i++) {
        int a = g_rng.NextInt(0x28);
        int b = g_rng.NextInt(0x27);
        b++;
        strcpy(g_printBuffer, g_golferTypeArray[i].name);
        strcat(g_printBuffer, " likes ");
        strcat(g_printBuffer, g_golferLikes[a]);
        strcat(g_printBuffer, ", ");
        strcat(g_printBuffer, g_golferLikes[(b + a) % 0x28]);
        strcat(g_printBuffer, ", and ");
        strcat(g_printBuffer, g_golferLikes[(a + b * 2) % 0x28]);
        strcat(g_printBuffer, ".");
        if (g_rng.NextInt(2)) {
            strcat(g_printBuffer, i < 57 ? " His" : " Her");
            strcat(g_printBuffer, " favorite color is ");
            switch (g_rng.NextInt(10)) {
            case 0: strcat(g_printBuffer, "red."); break;
            case 1: strcat(g_printBuffer, "blue."); break;
            case 2: strcat(g_printBuffer, "purple."); break;
            case 3: strcat(g_printBuffer, "pink."); break;
            case 4: strcat(g_printBuffer, "yellow."); break;
            case 5: strcat(g_printBuffer, "lavender."); break;
            case 6: strcat(g_printBuffer, "plaid."); break;
            case 7: strcat(g_printBuffer, "black."); break;
            case 8: strcat(g_printBuffer, "orange."); break;
            case 9: strcat(g_printBuffer, "grey."); break;
            }
        }
        strcpy(g_golferTypeArray[i].bio, g_printBuffer);
    }

    g_backgroundActors[0].type = 0;
    g_backgroundActors[1].type = 1;
    g_backgroundActors[2].type = 3;
    g_backgroundActors[3].type = 2;
    for (i = 0; i < 256; i++)
        g_tornDivots[i].x = -1;
    for (i = 0; i < 16; i++)
        g_ballPathSlots[i].golferId = -1;
    for (i = 0; i < 128; i++)
        g_gameAnimals[i].kind = -1;
    g_nextOpenHoleIndex = 1;
    g_winnersPurseK = 0;
    g_ceoLadderRung = 0;
    g_proMatchStakeLvl = 0;
    g_celebHomesBought = 0;
    g_curLandmarkTotal = 0;
    g_landPurchaseNum = 0;
    g_winningPtsTotal = 0;
    g_storiesNum = 0;
    g_nextGolferOrder = 0;
    g_curNextSpawnPos = 0;
    g_waterSprayFxFrame = -1;
    g_watchedPlayerIdx = -1;
    InitPlots();
}


unsigned char *DownsampleRGB555ToRGB888(int count)
{
    unsigned short *src = g_sampleSrcPtr;
    unsigned char *dst = g_jpegOutRowPtr;
    while (count--) {
        unsigned r = 0, g = 0, b = 0;
        for (int y = 0; y < g_jpegBlockSize; y++) {
            for (int x = 0; x < g_jpegBlockSize; x++) {
                unsigned px = src[x * g_jpegSurfacePitch + y];
                r += (px >> 7) & 0xf8;
                g += (px >> 2) & 0xf8;
                b += (px & 0x1f) << 3;
            }
        }
        src += g_jpegBlockSize;
        *dst++ = (unsigned char)(r / (g_jpegBlockSize * g_jpegBlockSize));
        *dst++ = (unsigned char)(g / (g_jpegBlockSize * g_jpegBlockSize));
        *dst++ = (unsigned char)(b / (g_jpegBlockSize * g_jpegBlockSize));
    }
    g_sampleSrcPtr += g_jpegSurfacePitch * g_jpegBlockSize;
    return g_jpegOutRowPtr;
}

void SaveSurfaceToJPEG(const char *filename, AssetSurface *surface, int x, int y, int width, int height, int blockSize)
{
    int surfW, surfH, stride;
    PixelFormat *fmt;
    FILE *f;

    if (filename == 0)
        return;
    if (surface == 0)
        return;
    if (width == 0)
        return;
    if (height == 0)
        return;
    if (blockSize == 0)
        return;
    if (x < 0)
        return;
    if (y < 0)
        return;
    if (width < 0)
        return;
    if (height < 0)
        return;
    if (blockSize < 0)
        return;
    if (blockSize > 20)
        return;

    surfW = surface->m_surface ? surface->m_surface->get_width() : 0;
    surfH = surface->m_surface ? surface->m_surface->get_height() : 0;
    stride = surface->m_surface ? surface->m_surface->get_pitch() : 0;
    g_jpegSurfacePitch = stride;
    fmt = surface->m_surface ? surface->m_surface->get_pixformat() : 0;

    if (fmt->bpp != 16)
        return;
    if (fmt->code != 0)
        return;
    if (x >= surfW)
        return;
    if (y >= surfH)
        return;

    if (x + width > surfW)
        width = surfW - x;
    if (y + height > surfH)
        height = surfH - y;

    f = fopen(filename, "wb");
    if (f == 0)
        return;

    g_jpegBlockSize = blockSize;
    g_sampleSrcPtr =
        (unsigned short *)(surface->m_surface == 0 ? 0 : surface->m_surface->get_data()) + (g_jpegSurfacePitch * y + x);
    g_jpegOutRowPtr = (unsigned char *)malloc((width * 3) / blockSize);
    CreateJPG(f, width / blockSize, height / blockSize, DownsampleRGB555ToRGB888);
    if (g_jpegOutRowPtr != 0)
        free(g_jpegOutRowPtr);
    fclose(f);
}

void SaveScreenshotToJPEG(char *path)
{
    char *dot = strchr(path, '.');
    if (dot)
        *dot = 0;
    strcat(path, "Full.jpg");
    SaveSurfaceToJPEG(path, g_pMenu, 0, 0, 800, 600, 2);
    if (dot)
        *dot = 0;
    strcat(path, "Thumb.jpg");
    SaveSurfaceToJPEG(path, g_pMenu, 0, 0, 800, 600, 10);
}

int SaveCourseInfoToText(char *path)
{
    char *dot;
    FILE *f;

    dot = strchr(path, '.');
    if (dot)
        *dot = 0;
    strcat(path, ".srf");
    f = fopen(path, "wt");
    fprintf(f, "[Course]\n");
    g_printBuffer[0] = 0;
    AppendCourseName(1);
    fprintf(f, "Name=%s\n", g_printBuffer);
    fprintf(f, "Holes=%d\n", g_nextOpenHoleIndex - 1);
    fprintf(f, "Par=%d\n", g_gameCourseTotalPar);
    fprintf(f, "Yards=%d\n", g_simTotalLengthYards);
    fprintf(f, "Type=%s\n", g_courseTypeNames[g_currentThemeId & 3]);
    fprintf(f, "Theme=%s\n", g_activeThemeDir);
    fprintf(f, "Designer=%s\n", g_golferTypeArray[0].name);
    g_printBuffer[0] = 0;
    AppendPooledString(0x14);
    fprintf(f, "Record=%d by %s\n", g_bestTenScoresTable[0], g_printBuffer);
    fprintf(f, "Cash=%d\n", g_gameCashHundreds * 100);
    fprintf(f, "FunRating=%d\n", g_courseTotalFunRate);
    fprintf(f, "SkillRating=%.2f\n", g_clubSkillsScore * 0.01);
    fprintf(f, "LengthSkill=%.2f\n", g_lengthAbilitySum * 0.01);
    fprintf(f, "AccuracySkill=%.2f\n", g_gameCourseAccurPointsTotal * 0.01);
    fprintf(f, "Imagination=%.2f\n", g_totalImagination * 0.01);
    return fclose(f);
}

void ProcessPendingScreenshot()
{
    if (g_gameCoursePath[0]) {
        SaveCourseInfoToText(g_gameCoursePath);
        SaveScreenshotToJPEG(g_gameCoursePath);
        g_gameCoursePath[0] = 0;
    }
}

void SetCourseFileName(const char *name)
{
    strcpy(g_gameCoursePath, name);
}

int g_wildlifeSpare;
