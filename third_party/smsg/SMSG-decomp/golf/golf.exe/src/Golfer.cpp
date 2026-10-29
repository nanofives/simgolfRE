#include "jgldefs.h"
#include "Golfer.h"
#include "crt.h"
#include "Geometry.h"
#include "SoundEffects.h"
#include "FloatingText.h"
#include "WorldMap.h"
int g_lastStoryResponse = 0;
short g_nextThemeSlot;
ThemeSlot g_themeSlots[8];

char *g_creditLines[] = {"Sid Meier's GETTYSBURG!", "(C) 1997 Firaxis Games Inc.", "Design and Programming: Sid Meier",
    "Additional Design: Jeff Briggs, Brian Reynolds", "Additional Programming: Jason Coleman",
    "Sound Design and Programming: David Evans", "Development Support and Multimedia: Tim Train, Michael Ely",
    "Art and Graphics: Michael Haire, Jerome Atherholt, Dave Inscore",
    "Greg Foertsh, Nicholas Rusko-Berger, Michael Bazzell", "Testing and Quality Assurance: Absolute Quality Inc.",
    "Civil War Historical Paintings: Don Troiani", g_empty, g_empty, g_empty};
int g_creditLine = -1;


int AdvanceStoryChapter(int golfer, int force)
{
    int twin;
    int advanced;
    signed char dir;
    int ang;

    if (g_golferPool[golfer].storyId != -1
        && (force != 0 || g_golferPool[golfer].holeIdx > 1 || g_golferPool[golfer].holeStrokes != 0)) {
        g_printBuffer[0] = 0;
        if ((g_golferPool[golfer].storyChapter <= 3 || g_golferPool[g_golferPool[golfer].partner].storyChapter <= 3)
            && (g_golferPool[golfer].enjoymentFloor > g_golferPool[golfer].storyChapter * 4 + 4 || force != 0)) {
            g_golferPool[golfer].storyChapter++;
            g_golferPool[golfer].chapterAtHole[g_golferPool[golfer].holeIdx] =
                (signed char)g_golferPool[golfer].storyChapter;
            g_golferPool[golfer].flags |= 0x800000;
            g_golferPool[g_golferPool[golfer].partner].flags |= 0x800000;

            g_curPreviousConvoClock = g_clock;
            if (GetStoryChapterLine(g_golferPool[golfer].storyId, g_golferPool[golfer].holeIdx, golfer)) {
                SetGolferAction(golfer, 0x32, (g_lastStoryResponse << 4) + g_golferPool[golfer].storyChapter);
                PlayPositionedSound(IsGolferMale(golfer) + 0x58, g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
            }

            twin = golfer ^ 1;
            if (GetStoryChapterLine(g_golferPool[golfer].storyId, g_golferPool[golfer].holeIdx, twin)) {
                SetGolferAction(twin, 0x32, (g_lastStoryResponse << 4) + g_golferPool[golfer].storyChapter);
                g_golferPool[g_golferPool[golfer].partner].talkTimer += 3;

                if (g_golferPool[g_golferPool[golfer].partner].storyChapter >= g_golferPool[golfer].storyChapter) {
                    PlayPositionedSound(IsGolferMale(g_golferPool[golfer].partner) + 0xa6, g_golferPool[golfer].x,
                        g_golferPool[golfer].y, 0x5dc);
                    if (g_golferPool[golfer].storyChapter == 1) {
                        switch (g_currentThemeId) {
                        case THEME_LINKS:
                            ConfigureAndPlaySound((g_golferPool[golfer].storyId & 1) ? 0x75 : 0x73, SOUND_FULL_VOLUME,
                                0, 0, 0);
                            break;
                        case THEME_TROPICAL:
                            ConfigureAndPlaySound((g_golferPool[golfer].storyId & 1) ? 0x70 : 0x6e, SOUND_FULL_VOLUME,
                                0, 0, 0);
                            break;
                        case THEME_DESERT:
                            ConfigureAndPlaySound((g_golferPool[golfer].storyId & 1) ? 0x7a : 0x78, SOUND_FULL_VOLUME,
                                0, 0, 0);
                            break;
                        case THEME_PARK:
                            ConfigureAndPlaySound((g_golferPool[golfer].storyId & 1) ? 0x34 : 0x32, SOUND_FULL_VOLUME,
                                0, 0, 0);
                            break;
                        }
                        g_golferPool[golfer].talkTimer++;
                        g_golferPool[g_golferPool[golfer].partner].talkTimer++;
                    }
                    g_golferPool[golfer].enjoyment++;
                    if (g_golferTypeArray[g_golferPool[golfer].typeIdx].caps & 2)
                        OnTraitMatched(golfer);
                    if (g_golferTypeArray[g_golferPool[g_golferPool[golfer].partner].typeIdx].caps & 2)
                        OnTraitMatched(twin);
                } else if (g_golferPool[g_golferPool[golfer].partner].storyChapter
                           == g_golferPool[golfer].storyChapter - 1) {
                    PlayPositionedSound(IsGolferMale(g_golferPool[golfer].partner) + 0xa4,
                        g_golferPool[g_golferPool[golfer].partner].x, g_golferPool[g_golferPool[golfer].partner].y,
                        0x5dc);
                } else {
                    PlayPositionedSound(IsGolferMale(g_golferPool[golfer].partner) + 0x5a,
                        g_golferPool[g_golferPool[golfer].partner].x, g_golferPool[g_golferPool[golfer].partner].y,
                        0x5dc);
                    g_golferPool[golfer].enjoyment--;
                }
            }

            advanced = 0;
            if (g_golferPool[g_golferPool[golfer].partner].storyChapter < g_golferPool[golfer].storyChapter) {
                g_golferPool[golfer].storyChapter--;
                g_golferPool[golfer].enjoymentFloor -= g_golferPool[golfer].enjoymentFloor / 3;
            } else {
                advanced = 1;
                if (g_golferPool[golfer].storyChapter == STORY_LAST_CHAPTER
                    && g_golferPool[g_golferPool[golfer].partner].storyChapter == g_golferPool[golfer].storyChapter) {
                    switch (g_currentThemeId) {
                    case THEME_LINKS: ConfigureAndPlaySound(0x73, SOUND_FULL_VOLUME, 0, 0, 0); break;
                    case THEME_TROPICAL: ConfigureAndPlaySound(0x6e, SOUND_FULL_VOLUME, 0, 0, 0); break;
                    case THEME_DESERT: ConfigureAndPlaySound(0x78, SOUND_FULL_VOLUME, 0, 0, 0); break;
                    case THEME_PARK: ConfigureAndPlaySound(SOUND_PURCHASE, SOUND_FULL_VOLUME, 0, 0, 0); break;
                    }
                    g_storiesNum++;
                    NotifyEvent(0x120, g_golferPool[golfer].storyId);
                    g_golferHistoryPerCharacter[g_golferPool[golfer].typeIdx].finishedStoryId =
                        (unsigned char)g_golferPool[golfer].storyId;
                    g_golferHistoryPerCharacter[g_golferPool[golfer].typeIdx]
                        .perHoleFlags[g_golferPool[golfer].holeIdx] |= 2;
                    g_golferHistoryPerCharacter[g_golferPool[g_golferPool[golfer].partner].typeIdx]
                        .perHoleFlags[g_golferPool[golfer].holeIdx] |= 2;
                    UnlockLandmarkByGolfer(golfer, g_golferPool[golfer].storyId);
                }
            }

            g_golferPool[g_golferPool[golfer].partner].chapterAtHole[g_golferPool[golfer].holeIdx] =
                (signed char)g_golferPool[g_golferPool[golfer].partner].storyChapter;
            g_golferPool[g_golferPool[golfer].partner].waitTimer = (-4 - g_golferPool[golfer].storyChapter) * 4;
            g_golferPool[golfer].waitTimer = g_golferPool[g_golferPool[golfer].partner].waitTimer;

            ang = CalculateAngle(g_golferPool[g_golferPool[golfer].partner].x - g_golferPool[golfer].x,
                g_golferPool[g_golferPool[golfer].partner].y - g_golferPool[golfer].y);
            dir = ((((ang >> 28) & 0xf) + 1) >> 1) & 7;
            g_golferPool[golfer].dir = dir;
            g_golferPool[g_golferPool[golfer].partner].dir = dir ^ 4;
            g_golferPool[g_golferPool[golfer].partner].activity = ACTIVITY_WALK;
            g_golferPool[golfer].activity = ACTIVITY_WALK;
            g_golferPool[g_golferPool[golfer].partner].walkLeft = 0;
            g_golferPool[golfer].walkLeft = 0;
            return advanced;
        }
    }
    return 0;
}

int GetStoryChapterLine(int story, int hole, int gi)
{
    int f, i, len, id;

    f = g_golferPool[gi].flags & GOLFER_STORY_LEAD;
    if (f)
        id = g_golferPool[gi].chapterAtHole[hole];
    else
        id = g_golferPool[g_golferPool[gi].partner].chapterAtHole[hole];

    if (id == 0)
        return 0;

    ResolveStoryResponse(story, id, hole, f ? gi : gi ^ 1);

    len = strlen(g_printBuffer);

    if (g_golferPool[gi].flags & GOLFER_STORY_LEAD) {
        for (i = 0; i < len; i++) {
            if (g_printBuffer[i] == '\n')
                g_printBuffer[i] = 0;
        }
    } else {
        for (i = 0; i < len; i++) {
            if (g_printBuffer[i] == '\n') {
                strcpy(g_printBuffer, &g_printBuffer[i + 1]);
                return g_printBuffer[0];
            }
        }
        g_printBuffer[0] = 0;
    }

    return g_printBuffer[0];
}

void ResolveStoryResponse(int file, int range, int hole, int gi)
{
    int r, v, n;

    r = Clamp(g_golferPool[gi ^ 1].holeResult[hole] >> 3, 0, range);
    if (g_golferPool[g_golferPool[gi].partner].holeIdx == hole
        && g_golferPool[g_golferPool[gi].partner].holeResult[hole] == 0) {
        n = 0;
        while (g_golferPool[g_golferPool[gi].partner].slotKind[n] == 0x13 && n < 4)
            n++;
        v = (Clamp(g_golferPool[g_golferPool[gi].partner].enjoyment,
                 g_golferPool[g_golferPool[gi].partner].enjoymentFloor, 99)
                - 1)
            / 2;
        if (range >= 2 && v > range - 1)
            v = range - 1;
        if ((g_golferPool[g_golferPool[gi].partner].slotParams[n] & MOOD_MASK) == MOOD_PLEASED) {
            v++;
        } else {
            if (v != 0 && (g_golferPool[g_golferPool[gi].partner].slotParams[n] & MOOD_MASK) != 0)
                v--;
            if (v != 0 && (g_golferPool[g_golferPool[gi].partner].slotParams[n] & MOOD_MASK) == MOOD_MASK)
                v--;
        }
        if (InLandmarkInfluence(g_golferPool[g_golferPool[gi].partner].x, g_golferPool[g_golferPool[gi].partner].y, 8))
            v = range;
        r = Clamp(v, 0, range);
        g_golferPool[g_golferPool[gi].partner].storyChapter = r;
        g_golferPool[g_golferPool[gi].partner].holeResult[hole] = (r << 3) + 1;
        if (r != 0)
            goto resultSetUnrecovered;
        else
            goto resultZeroUnrecovered;
    } else {
        g_golferPool[g_golferPool[gi].partner].storyChapter = r;
        g_golferPool[g_golferPool[gi].partner].holeResult[hole] = (r << 3) + 1;
    }
resultSetUnrecovered:;
resultZeroUnrecovered:
    g_lastStoryResponse = r;
    AppendThemeText(file, range, r, -1);
}

void AppendThemeText(int file, int para, int sub, int mode)
{
    char prev[256];
    char line[256];
    char path[60];
    int para0, cnum, lnum, count;
    int i, j, len0, len1;
    FILE *fp;

    para0 = para;
    for (i = 0; i < 8; i++)
        if (g_themeSlots[i].id == file && g_themeSlots[i].para == para && g_themeSlots[i].sub == sub) {
            if (mode == -1) {
                strcat(g_printBuffer, g_themeSlots[i].text);
                return;
            }
            len0 = strlen(g_printBuffer);
            strcat(g_printBuffer, g_themeSlots[i].text);
            len1 = strlen(g_printBuffer);
            if (mode == 1) {
                for (j = len0; j < len1; j++)
                    if (g_printBuffer[j] == '\n')
                        g_printBuffer[j] = 0;
            } else {
                for (j = len0; j < len1; j++)
                    if (g_printBuffer[j] == '\n')
                        strcpy(&g_printBuffer[len0], &g_printBuffer[j + 1]);
            }
            return;
        }

    g_themeSlots[g_nextThemeSlot].id = file;
    g_themeSlots[g_nextThemeSlot].para = -2;
    g_themeSlots[g_nextThemeSlot].sub = sub;
    strcpy(path, "Themes\\");
    if ((g_runModeFlags & 0x10000000) == 0)
        strcat(path, g_activeThemeDir);
    else
        strcat(path, "Standard");
    strcat(path, "\\");
    strcat(path, g_gameStoryNameTable[file]);
    fp = fopen(path, "rt");
    if (fp == 0)
        return;
    lnum = 1;
    count = 0;
    cnum = para;
    while (fgets(line, 250, fp)) {
        if (line[1] != 0) {
            line[strlen(line) - 1] = 0;
            if (para0 == -1) {
                strcat(g_printBuffer, line);
                fclose(fp);
                return;
            }
            if (line[0] != ' ') {
                if (para == lnum) {
                    strcat(g_printBuffer, line);
                    strcat(g_printBuffer, "\n");
                    para = -1;
                } else if (para == -1 && sub != -1) {
                    if (mode != 0)
                        strcat(g_printBuffer, prev);
                    sub = -1;
                }
                lnum++;
            } else {
                if (para == -1) {
                    if (cnum == sub) {
                        if (mode != 1)
                            strcat(g_printBuffer, line);
                        sub = -1;
                    } else {
                        cnum--;
                    }
                }
                strcpy(prev, line);
            }
            if (++count > 200) {
                fclose(fp);
                return;
            }
        }
        if (para == -1 && sub == -1)
            break;
    }
    if (para == -1 && sub != -1 && mode != 1)
        strcat(g_printBuffer, prev);
    fclose(fp);
    if (mode == -1) {
        strcpy(g_themeSlots[g_nextThemeSlot].text, g_printBuffer);
        g_themeSlots[g_nextThemeSlot].para = para0;
        g_nextThemeSlot = (short)(g_nextThemeSlot + 1) % 8;
    }
}

void OnTraitMatched(int)
{
}

int g_themeSpareSlots[3];
int g_storyResponseSpare = 0;
