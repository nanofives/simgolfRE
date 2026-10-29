#include "crt.h"
#include "GolferPairing.h"
#include "GolferDialogue.h"
#include "SkillPointsScreen.h"
#include "Top10Courses.h"
#include "WelcomeMessage.h"
#include "TutorialMessage.h"
#include "HoleTypeMessage.h"
#include "Scorecard.h"

void SwapGolferSlots(int first, int second)
{
    memcpy(&g_golferPool[SCRATCH_GOLFER], &g_golferPool[first], sizeof(Golfer));
    g_golferPool[first] = g_golferPool[second];
    memcpy(&g_golferPool[second], &g_golferPool[SCRATCH_GOLFER], sizeof(Golfer));
}

void PairGolfers(int golfer, int partner)
{
    int compat, i, tries, pick, ok, attempt;
    int traitMask, rating, n, k, mood;

    g_golferPool[golfer].holeIdx = 1;
    g_golferPool[golfer].partner = partner;
    g_golferPool[partner].holeIdx = 1;
    g_golferPool[partner].partner = golfer;
    g_golferPool[partner].arrivedAt = g_clock;
    g_golferPool[golfer].arrivedAt = g_clock;
    g_golferPool[golfer].seq = g_nextGolferOrder++;
    g_golferPool[partner].seq = g_nextGolferOrder++;
    g_golferPool[golfer].waitTimer = -6 - g_rng.NextInt(6);
    g_golferPool[partner].waitTimer = 0;
    g_golferPool[partner].storyId = -1;
    g_golferPool[golfer].storyId = -1;
    g_golferPool[partner].rapport = 0;
    g_golferPool[golfer].rapport = 0;
    g_golferPool[golfer].flags &= ~GOLFER_STORY_LEAD;
    g_golferPool[partner].flags &= ~GOLFER_STORY_LEAD;
    if (g_simOpenFacilityLvl[SCENERY_CART_GARAGE] != 0) {
        g_golferPool[golfer].flags |= 0x10000;
        g_golferPool[partner].flags |= 0x10000;
    }

    compat = 5;
    for (i = 0; i < 5; i++) {
        if ((g_golferTypeArray[g_golferPool[partner].typeIdx].caps
                ^ g_golferTypeArray[g_golferPool[golfer].typeIdx].caps)
            & (1 << i))
            compat--;
    }

    attempt = 0;
    while (1) {
        pick = g_rng.NextInt(100);
        tries = 0;
        do {
            pick = g_rng.NextInt(100);
            tries++;
        } while (tries < 100 && (g_gameStoryNameTable[pick][0] == 0 || pick == g_openingDayStoryId));
        ok = 1;
        if (tries >= 100)
            ok = 0;
        toupper(g_gameStoryNameTable[pick][0]);
        if (!ok)
            goto Lnext;
        switch (toupper(g_gameStoryNameTable[pick][1])) {
        case 'M':
            if (!IsGolferMale(golfer))
                goto Lnext;
            break;
        case 'F':
            if (IsGolferMale(golfer))
                goto Lnext;
            break;
        case 'S':
            if (IsGolferMale(golfer) != IsGolferMale(partner))
                goto Lnext;
            break;
        case 'O':
            if (IsGolferMale(golfer) == IsGolferMale(partner))
                goto Lnext;
            break;
        }
        switch (toupper(g_gameStoryNameTable[pick][2])) {
        case 'M':
            if (!(g_golferPool[golfer].ageFlags & 0x10))
                goto Lnext;
            break;
        case 'N':
            if (g_golferPool[golfer].ageFlags & 0x10)
                goto Lnext;
            break;
        case 'D':
            if (!(g_golferPool[golfer].ageFlags & 0x20))
                goto Lnext;
            break;
        case 'W':
            if (!(g_golferPool[golfer].ageFlags & 0x40))
                goto Lnext;
            break;
        case 'S':
            if (!(g_golferPool[golfer].ageFlags & 0x08))
                goto Lnext;
            break;
        }
        switch (toupper(g_gameStoryNameTable[pick][3])) {
        case 'T':
            if (!(g_golferTypeArray[g_golferPool[golfer].typeIdx].caps & 0x01))
                goto Lnext;
            break;
        case 'M':
            if (g_golferTypeArray[g_golferPool[golfer].typeIdx].caps & 0x01)
                goto Lnext;
            break;
        case 'O':
            if (!(g_golferTypeArray[g_golferPool[golfer].typeIdx].caps & 0x02))
                goto Lnext;
            break;
        case 'S':
            if (g_golferTypeArray[g_golferPool[golfer].typeIdx].caps & 0x02)
                goto Lnext;
            break;
        case 'A':
            if (!(g_golferTypeArray[g_golferPool[golfer].typeIdx].caps & 0x04))
                goto Lnext;
            break;
        case 'L':
            if (g_golferTypeArray[g_golferPool[golfer].typeIdx].caps & 0x04)
                goto Lnext;
            break;
        case 'P':
            if (!(g_golferTypeArray[g_golferPool[golfer].typeIdx].caps & TRAIT_PLAYFUL))
                goto Lnext;
            break;
        case 'B':
            if (g_golferTypeArray[g_golferPool[golfer].typeIdx].caps & TRAIT_PLAYFUL)
                goto Lnext;
            break;
        case 'N':
            if (!(g_golferTypeArray[g_golferPool[golfer].typeIdx].caps & 0x10))
                goto Lnext;
            break;
        case 'G':
            if (g_golferTypeArray[g_golferPool[golfer].typeIdx].caps & 0x10)
                goto Lnext;
            break;
        }
        switch (toupper(g_gameStoryNameTable[pick][4])) {
        case 'Y':
            if (GolferAge(golfer) > GolferAge(partner) - 10)
                goto Lnext;
            break;
        case 'S':
            if (abs(GolferAge(golfer) - GolferAge(partner)) > GolferAge(golfer) / 3)
                goto Lnext;
            break;
        case 'O':
            if (GolferAge(golfer) < GolferAge(partner) + 10)
                goto Lnext;
            break;
        }
        switch (toupper(g_gameStoryNameTable[pick][5])) {
        case 'M':
            if (!IsGolferMale(partner))
                goto Lnext;
            break;
        case 'F':
            if (IsGolferMale(partner))
                goto Lnext;
            break;
        case 'S':
            if (IsGolferMale(golfer) != IsGolferMale(partner))
                goto Lnext;
            break;
        case 'O':
            if (IsGolferMale(golfer) == IsGolferMale(partner))
                goto Lnext;
            break;
        }
        switch (toupper(g_gameStoryNameTable[pick][6])) {
        case 'M':
            if (!(g_golferPool[partner].ageFlags & 0x10))
                goto Lnext;
            break;
        case 'N':
            if (g_golferPool[partner].ageFlags & 0x10)
                goto Lnext;
            break;
        case 'D':
            if (!(g_golferPool[partner].ageFlags & 0x20))
                goto Lnext;
            break;
        case 'W':
            if (!(g_golferPool[partner].ageFlags & 0x40))
                goto Lnext;
            break;
        case 'S':
            if (!(g_golferPool[partner].ageFlags & 0x08))
                goto Lnext;
            break;
        }
        switch (toupper(g_gameStoryNameTable[pick][7])) {
        case 'T':
            if (!(g_golferTypeArray[g_golferPool[partner].typeIdx].caps & 0x01))
                ok = 0;
            break;
        case 'M':
            if (g_golferTypeArray[g_golferPool[partner].typeIdx].caps & 0x01)
                ok = 0;
            break;
        case 'O':
            if (!(g_golferTypeArray[g_golferPool[partner].typeIdx].caps & 0x02))
                ok = 0;
            break;
        case 'S':
            if (g_golferTypeArray[g_golferPool[partner].typeIdx].caps & 0x02)
                ok = 0;
            break;
        case 'A':
            if (!(g_golferTypeArray[g_golferPool[partner].typeIdx].caps & 0x04))
                ok = 0;
            break;
        case 'L':
            if (g_golferTypeArray[g_golferPool[partner].typeIdx].caps & 0x04)
                ok = 0;
            break;
        case 'P':
            if (!(g_golferTypeArray[g_golferPool[partner].typeIdx].caps & TRAIT_PLAYFUL))
                ok = 0;
            break;
        case 'B':
            if (g_golferTypeArray[g_golferPool[partner].typeIdx].caps & TRAIT_PLAYFUL)
                ok = 0;
            break;
        case 'N':
            if (!(g_golferTypeArray[g_golferPool[partner].typeIdx].caps & 0x10))
                ok = 0;
            break;
        case 'G':
            if (g_golferTypeArray[g_golferPool[partner].typeIdx].caps & 0x10)
                ok = 0;
            break;
        }
        for (k = 0; k < 152; k++) {
            if (g_golferPool[k].holeIdx > 0 && g_golferPool[k].storyId == pick)
                ok = 0;
        }
        if (ok) {
            g_golferPool[partner].storyId = pick;
            g_golferPool[golfer].storyId = pick;
            g_golferPool[golfer].flags |= GOLFER_STORY_LEAD;
            break;
        }
Lnext:
        attempt++;
        if (attempt >= 3)
            break;
    }
    if (golfer < 2 && g_clock < 0x800) {
        g_golferPool[partner].storyId = g_openingDayStoryId;
        g_golferPool[golfer].storyId = g_openingDayStoryId;
        g_golferPool[golfer].flags |= GOLFER_STORY_LEAD;
    }

    traitMask = 0;
    if (!(g_golferPool[partner].flags & 0x20000)) {
        for (i = 0; i < 152; i++) {
            if (g_golferPool[i].holeIdx > 0 && g_golferPool[i].typeIdx >= (short)'L')
                traitMask |= 1 << (g_golferPool[i].typeIdx - 'L');
        }
        if (g_runModeFlags & GAME_SANDBOX)
            traitMask = -1;
        rating = g_clubSkillsScore;
        if (g_level <= 1) {
            strcpy(g_printBuffer, "Your Fun Rating is up to ");
            strcat(g_printBuffer, itoa(g_courseTotalFunRate, g_numBuf, 10));
            strcat(g_printBuffer, " and your fame is spreading. ");
            rating = g_courseTotalFunRate;
        } else {
            strcpy(g_printBuffer, "Your Skill Rating is up to ");
            AppendMoney(g_clubSkillsScore);
            strcat(g_printBuffer, " and your fame is spreading. ");
        }
        if ((partner / 2) % 6 == 1 && !(traitMask & 4) && g_ceoLadderRung < 8
            && g_allHolesArray[(g_ceoLadderRung + 1) * 2].par != 0) {
            strcpy(g_printBuffer, "Corporate CEO ");
            g_golferPool[partner].persona = g_ceoLadderRung + PERSONA_CEO;
            AppendGolferName(partner, 0);
            g_golferPool[partner].typeIdx = 'O';
            strcat(g_printBuffer,
                " is playing your course today. If he likes your course he may invest in a seat on your board of directors!");
            if (ShowStoryMessage(COLOR_DARK_GREY, 1, partner)) {
                g_golferPool[partner].talents = SKILLS_ALL;
                g_golferPool[partner].persona = g_ceoLadderRung + PERSONA_CEO;
                g_golferPool[partner].skillMask = 1 << g_rng.NextInt(9);
                g_golferPool[partner].skillMask |= 1 << g_rng.NextInt(9);
                g_golferPool[partner].skillMask |= 1 << g_rng.NextInt(9);
                g_golferPool[partner].skillMask |= (1 << g_rng.NextInt(9)) | 0x0e00;
                g_ceoLadderRung = (g_ceoLadderRung + 1) | 0x80;
                ConfigureAndPlaySound(SOUND_TIME_TUNNEL, SOUND_FULL_VOLUME, 0, 0, 0);
                g_golferPool[partner].rapport = 0;
                g_golferPool[golfer].rapport = 0;
            } else {
                g_golferPool[partner].persona = 0;
            }
        }
        if ((partner / 2) % 6 == 3 && !(traitMask & 2) && g_mapCourseList[g_myWorldCourseIdx].coast != 2
            && rating > (g_landPurchaseNum + 2) * (g_level + 2) * 50) {
            strcat(g_printBuffer, "County commissioner ");
            g_golferPool[partner].persona = g_landPurchaseNum + PERSONA_COMMISSIONER;
            AppendGolferName(partner, 0);
            g_golferPool[partner].typeIdx = 'M';
            strcat(g_printBuffer,
                " is playing your course today. If he likes your course he may approve a request to purchase more land!");
            if (ShowStoryMessage(COLOR_DARK_GREY, 1, partner)) {
                g_golferPool[partner].talents = SKILLS_ALL;
                g_golferPool[partner].skillMask = 1 << g_rng.NextInt(12);
                g_golferPool[partner].skillMask |= 1 << g_rng.NextInt(12);
                g_golferPool[partner].skillMask |= 1 << g_rng.NextInt(12);
                g_golferPool[partner].skillMask |= (1 << g_rng.NextInt(12)) | 0x38;
                ConfigureAndPlaySound(SOUND_TIME_TUNNEL, SOUND_FULL_VOLUME, 0, 0, 0);
                g_golferPool[partner].rapport = 0;
                g_golferPool[golfer].rapport = 0;
                g_golferPool[partner].storyId = -1;
                g_golferPool[golfer].storyId = -1;
            } else {
                g_golferPool[partner].persona = 0;
            }
        }
        if ((partner / 2) % 6 == 5 && !(traitMask & 4)
            && rating > (g_curLandmarkTotal + 1) * (g_curLandmarkTotal + 1) * 25 && g_shownLandmarkBitmask < 0xffff) {
            g_golferPool[partner].persona = PERSONA_HEIRESS;
            strcat(g_printBuffer, "Wealthy Heiress ");
            AppendGolferName(partner, 0);
            g_golferPool[partner].typeIdx = 'N';
            strcat(g_printBuffer,
                " is playing your course today. If she enjoys her round she may donate a valuable scenic landmark to your course.");
            if (ShowStoryMessage(COLOR_DARK_GREY, 1, partner)) {
                g_golferPool[partner].talents = SKILLS_NO_LENGTH;
                g_golferPool[partner].skillMask = 1 << g_rng.NextInt(12);
                g_golferPool[partner].skillMask |= 1 << g_rng.NextInt(12);
                g_golferPool[partner].skillMask |= 1 << g_rng.NextInt(12);
                g_golferPool[partner].skillMask |= (1 << g_rng.NextInt(12)) | 0x0300;
                ConfigureAndPlaySound(SOUND_TIME_TUNNEL, SOUND_FULL_VOLUME, 0, 0, 0);
                g_golferPool[partner].rapport = 0;
                g_golferPool[golfer].rapport = 0;
                g_golferPool[partner].storyId = -1;
                g_golferPool[golfer].storyId = -1;
            } else {
                g_golferPool[partner].persona = 0;
            }
        }
    }
    if (g_golferPool[partner].persona != 0) {
        for (n = 0; n < 12; n++) {
            if (g_golferPool[partner].skillMask & (1 << n))
                g_golferPool[partner].ratings[n] = g_rng.NextInt(8) + 1;
        }
    }

    g_golferPool[partner].enjoyment = compat + 2;
    g_golferPool[golfer].enjoyment = compat + 2;
    if (g_mapCourseList[g_myWorldCourseIdx].site == 3)
        g_golferPool[i].enjoyment++;
    g_golferPool[partner].enjoymentFloor = compat * 2;
    g_golferPool[golfer].enjoymentFloor = compat * 2;
    if (g_golferPool[golfer].flags & GOLFER_STORY_LEAD)
        g_golferPool[golfer].holeResult[1] = compat * 2;
    else
        g_golferPool[partner].holeResult[1] = compat * 2;
    mood = g_storiesNum;
    if ((g_rng.NextInt(mood) > 6 && !(g_golferPool[golfer].flags & 0x20000)) || g_golferPool[partner].persona != 0) {
        g_golferPool[partner].storyId = -1;
        g_golferPool[golfer].storyId = -1;
        g_golferPool[partner].rapport = 0;
        g_golferPool[golfer].rapport = 0;
    }
    if (compat <= 1 && IsGolferMale(golfer) == IsGolferMale(partner)) {
        g_golferPool[partner].rapport = 2;
        g_golferPool[golfer].rapport = 2;
    }
    if (compat == 5) {
        g_golferPool[partner].rapport = 3;
        g_golferPool[golfer].rapport = 3;
    }
    if (compat >= 4) {
        if (IsGolferMale(golfer) != IsGolferMale(partner) && (g_golferPool[golfer].ageFlags & 0x10)
            && (g_golferPool[partner].ageFlags & 0x10)) {
            g_golferPool[partner].rapport = 1;
            g_golferPool[golfer].rapport = 1;
        }
    }
    g_golferPool[golfer].holeEnjoyment[0] = g_golferPool[golfer].enjoyment;
    g_golferPool[partner].holeEnjoyment[0] = g_golferPool[partner].enjoyment;
    if (g_golferPool[golfer].persona == 0)
        g_golferHistoryPerCharacter[g_golferPool[golfer].typeIdx].count++;
    if (g_golferPool[partner].persona == 0)
        g_golferHistoryPerCharacter[g_golferPool[partner].typeIdx].count++;
    g_golferPool[golfer].flags &= ~0x20000;
    g_golferPool[partner].flags &= ~0x20000;
    if (g_clock & 0x40) {
        SetGolferAction(golfer, 0x3d, g_golferPool[golfer].enjoyment);
        SetGolferAction(partner, 0x3d, g_golferPool[partner].enjoyment);
        g_golferPool[partner & 0xffe].talkTimer += 10;
        g_golferPool[(golfer & 0xffe) + 1].talkTimer += 8;
    }
}



int RenderSkillPointsScreen(char *name, int skillMask, int points, int golfer, int xOff)
{
    signed char saved[32];
    int nameX;
    int skillBits;
    int row;
    int cursorX;
    int hit;
    int boxX;
    int cursorY;
    int spent;

    memcpy(saved, g_golferSkillGrades, sizeof(saved));
    skillBits = skillMask;
    spent = 0;
    if (points > 0)
        flush_input();

    for (;;) {
        int col = xOff + 0x32;

        if (xOff >= 0)
            DrawPanelFrame(col - 4, 0x32, 0x140, 0x13c, 1);
        else
            DrawPanelFrame(col + 0x1c, 0x32, 0xd0, 0x13c, 1);

        g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
        nameX = col + 0xa0;
        DrawTextCenteredWithShadow(name, nameX, 0x3a, COLOR_WHITE);

        if (xOff >= 0) {
            g_headBodyBgPic.Draw(g_pMenu, col + 0xc5, 0x56, 1, 1, 1, 0);
            if (golfer != -1) {
                RenderGolferPortrait(golfer, col + 0xcd, 0x66, 1);
            } else {
                g_golferPool[PREVIEW_GOLFER].typeIdx = 0;
                g_golferPool[PREVIEW_GOLFER].ageFlags = g_golferTypeArray[0].ageBits;
                RenderGolferPortrait(PREVIEW_GOLFER, col + 0xcd, 0x66, 1);
            }
            g_transPopUpButtonBmps[5].draw_antialiased(&g_transPopUpMaskSprites[5], g_pMenu, col + 0x114, 0x13e, 0);
        }

        hit = -1;
        skillBits |= skillMask;
        if (points > 0)
            g_simScreen.get_mouse_pos(&cursorX, &cursorY);
        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);

        row = 0;
        boxX = col + 0x20;
        int labelX = col + 0x58;
        {
            int y = 0x61;
            for (; row < 10; y += 0x18, row++) {
                int level = (unsigned char)(golfer != -1 && g_golferPool[golfer].typeIdx != 0
                                                ? g_golferPool[golfer].ratings[row]
                                                : g_golferSkillGrades[row]);

                g_popupEdgeBmps[26].draw_antialiased(&g_transPopupCellsAlpha[26], g_pMenu, boxX, y - 7, 0);
                if (points > 0)
                    g_popupEdgeBmps[30].draw_antialiased(&g_transPopupCellsAlpha[30], g_pMenu, col, y - 7, 0);

                if (cursorX < boxX) {
                    if (cursorX >= col && cursorY > y - 7 && cursorY <= y + 5) {
                        g_popupEdgeBmps[31].draw_antialiased(&g_transPopupCellsAlpha[31], g_pMenu, col, y - 7, 0);
                        hit = row;
                    }
                    if (cursorX < boxX && cursorX >= col && cursorY > y + 5 && cursorY <= y + 0x11) {
                        g_popupEdgeBmps[32].draw_antialiased(&g_transPopupCellsAlpha[32], g_pMenu, col, y + 5, 0);
                        hit = row + 10;
                    }
                }

                if (level != 0) {
                    strcpy(g_printBuffer, level < 10 ? "+" : g_empty);
                    strcat(g_printBuffer, itoa(level * 10, g_numBuf, 10));
                    strcat(g_printBuffer, "%");
                    DrawTextWithColor(g_printBuffer, col + 0x25, y, 0x80000000);
                }
                DrawTextWithColor(g_skillNames[row], labelX, y, level ? 0x80000000 : 0x80004210);
            }
        }

        if (points > 0) {
            strcpy(g_printBuffer, "Add ");
            strcat(g_printBuffer, itoa(points - spent, g_numBuf, 10));
            strcat(g_printBuffer, " skill points.");
            DrawTextCenteredWithShadow(g_printBuffer, nameX, 0x50, spent <= points ? 0x80007fff : 0x80007d08);
        }

        if (points == -1)
            return -1;

        g_simScreen.RenderFullSurface(0);
        WaitWithTimeout(5, 0);
        ProcessMouseMessages();
        if (g_mouseButton == 0 && PollInputState() == 0)
            continue;

        WaitWithTimeout(5, 0);
        flush_input();

        if (g_mouseButton != 0 && abs(g_clickX - xOff - 0x15e) < 0x14 && abs(g_clickY - 0x14e) < 0x14) {
            if (points <= spent)
                break;
            AssetSurface save;
            save.Create(800, 600, 0, 1, 0, 0);
            g_pMenu->copy(&save, 0, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            strcpy(g_printBuffer, "You haven't used all your skill points!\nDo you really want to exit?\n");
            strcat(g_printBuffer, " Yea, I don't need no stinkin' skill points.\n Whoops, my bad.\n");
            if (ShowActionDialog(400, 200, 1, 1, 0) == 0)
                break;
            save.copy(g_pMenu, 0, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            continue;
        }

        if (points != 0 && g_mouseButton != 0 && hit != -1) {
            int i = hit % 10;
            if (hit < 10) {
                if (spent < points && g_golferSkillGrades[i] < 10) {
                    g_golferSkillGrades[i]++;
                    spent++;
                    g_skillMasks[0] |= 1 << i;
                    continue;
                }
            } else {
                if (g_golferSkillGrades[i] > 0 && g_golferSkillGrades[i] > saved[i]) {
                    g_golferSkillGrades[i]--;
                    spent--;
                    if (g_golferSkillGrades[i] == 0)
                        g_skillMasks[0] &= ~(1 << i);
                    continue;
                }
            }
        } else if (points <= 0 || spent <= points) {
            continue;
        }

        ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
    }
    return g_skillMasks[0];
}


void RenderTop10CoursesScreen(void)
{
    char rank;
    int bestIdx;
    int i;
    int y;

    int w = g_careerPlayYear * 12 + 0x113;
    int left = 0x177 - w / 2;

    g_simScreen.fill(left - 1, 0x0f, w, 0xe8, COLOR_WHITE);
    g_simScreen.fill(left + 1, 0x11, w, 0xe8, 0x80000000);
    g_simScreen.fill(left, 0x10, w, 0xe8, 0x80004090);

    g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
    strcpy(g_printBuffer, "Top 10 Golf Courses");
    DrawTextCenteredWithShadow(g_printBuffer, 0x177, 0x1a, COLOR_WHITE);
    y = 0x32;
    g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);

    char used[10];
    memset(used, 0, sizeof(used));

    g_top10CoursePoints[9][0] = g_holeAwardCount;
    g_top10CoursePoints[9][1] = g_homesSold;
    g_top10CoursePoints[9][2] = g_ratingWealth;
    g_top10CoursePoints[9][3] = g_ratingUnawarded;
    g_top10CoursePoints[9][4] = g_storiesNum;
    g_top10CoursePoints[9][TOP_COURSE_TOTAL] =
        g_holeAwardCount + g_homesSold + g_ratingWealth + g_ratingUnawarded + g_storiesNum;

    for (i = 0; i < 10; i++) {
        int best = 0, k;
        for (k = 0; k < 10; k++)
            if (!used[k] && g_top10CoursePoints[k][TOP_COURSE_TOTAL] >= best) {
                best = g_top10CoursePoints[k][TOP_COURSE_TOTAL];
                bestIdx = k;
            }
        used[bestIdx] = 1;
        if (bestIdx < 9) {
            strcpy(g_printBuffer, g_greatCourseNames[bestIdx]);
        } else {
            g_printBuffer[0] = 0;
            AppendCourseName(0);
            rank = i + 1;
        }
        DrawTextClampedToScreen(g_printBuffer, left + 0x10, y, bestIdx == 9 ? 0x80007c1f : 0x80007fff);

        int x = left + 0x88, j;
        for (j = 0; j < g_top10CoursePoints[bestIdx][0]; j++) {
            g_bubblePieces[GLYPH_HOLE_AWARD].BlitScaled(g_pMenu, x, y, 0);
            x += 0xc;
        }
        for (j = 0; j < g_top10CoursePoints[bestIdx][1]; j++) {
            g_bubblePieces[GLYPH_HOME_SOLD].BlitScaled(g_pMenu, x, y, 0);
            x += 0xc;
        }
        for (j = 0; j < g_top10CoursePoints[bestIdx][2]; j++) {
            g_bubblePieces[18].BlitScaled(g_pMenu, x, y, 0);
            x += 0xc;
        }
        for (j = 0; j < g_top10CoursePoints[bestIdx][3]; j++) {
            g_bubblePieces[19].BlitScaled(g_pMenu, x, y, 0);
            x += 0xc;
        }
        for (j = 0; j < g_top10CoursePoints[bestIdx][4]; j++) {
            g_bubblePieces[GLYPH_STORY].BlitScaled(g_pMenu, x, y, 0);
            x += 0xc;
        }
        y += 0xe;
    }

    y += 7;
    strcpy(g_printBuffer, "After ");
    strcat(g_printBuffer, itoa(g_clock / 8192, g_numBuf, 10));
    strcat(g_printBuffer, " years of operation your course");
    DrawTextCenteredWithShadow(g_printBuffer, 0x177, y, 0x80007c1f);

    y += 0xe;
    strcpy(g_printBuffer, "ranks ");
    switch (rank) {
    case 1: strcat(g_printBuffer, "1st"); break;
    case 2: strcat(g_printBuffer, "2nd"); break;
    case 3: strcat(g_printBuffer, "3rd"); break;
    case 4: strcat(g_printBuffer, "4th"); break;
    case 5: strcat(g_printBuffer, "5th"); break;
    case 6: strcat(g_printBuffer, "6th"); break;
    case 7: strcat(g_printBuffer, "7th"); break;
    case 8: strcat(g_printBuffer, "8th"); break;
    case 9: strcat(g_printBuffer, "9th"); break;
    case 10: strcat(g_printBuffer, "10th"); break;
    }
    strcat(g_printBuffer, " on the list of great courses");
    strcat(g_printBuffer, rank == 1 ? "!" : ".");
    DrawTextCenteredWithShadow(g_printBuffer, 0x177, y, 0x80007c1f);
}

void ShowWelcomeMessage(void)
{
    if (g_clock == 32) {
        if (g_runModeFlags & GAME_SANDBOX)
            return;
        switch (g_currentThemeId) {
        case THEME_PARK:
            strcpy(g_printBuffer, "Welcome to ");
            AppendCourseName(-1);
            strcat(g_printBuffer, " Golf Club.  The unexpected passing of your ");
            strcat(g_printBuffer, "great-uncle Harry has left you in possession of ");
            AppendCourseName(-1);
            strcat(g_printBuffer, ". The property includes rolling hills, crisp woodlands, and a ");
            strcat(g_printBuffer, "sparkling waters. However, uncle Harry was somewhat of a ");
            strcat(g_printBuffer, "lazy slacker and never built ");
            strcat(g_printBuffer, "any golf holes! ");
            strcat(g_printBuffer, "With the \xa7");
            strcat(g_printBuffer, itoa(g_gameCashHundreds * 100 / 1000, g_numBuf, 10));
            strcat(g_printBuffer, ",000 he left you can you turn ");
            AppendCourseName(-1);
            strcat(g_printBuffer, " into a world-class golf course?  Good Luck.");
            break;
        case THEME_DESERT:
            strcpy(g_printBuffer, "Welcome to the ");
            AppendCourseName(-1);
            strcat(g_printBuffer, " Country Club. What was once a vast expanse of ");
            strcat(g_printBuffer, "featureless desert is now a vast expanse of featureless desert interrupted ");
            strcat(g_printBuffer, "by a lonely golf clubhouse.");
            strcat(g_printBuffer, " This area is blessed with sunshine 365 days of ");
            strcat(g_printBuffer, "the year. A rapidly growing population of avid golfers is demanding a ");
            strcat(g_printBuffer, "larger improved course. As the new manager, can you use your \xa7");
            strcat(g_printBuffer, itoa(g_gameCashHundreds * 100 / 1000, g_numBuf, 10));
            strcat(g_printBuffer, ",000 budget to turn ");
            AppendCourseName(-1);
            strcat(g_printBuffer, " into a world-class golf resort?");
            break;
        case THEME_LINKS:
            strcpy(g_printBuffer, "Welcome to ");
            AppendCourseName(-1);
            strcat(g_printBuffer, " Golf Links.  This ancient and venerable institution ");
            strcat(g_printBuffer, "has fallen upon hard times. The sheep have returned to graze the pastoral fields ");
            strcat(g_printBuffer, "whereupon kings and princes did ply the game of Golfe in days of yore. ");
            strcat(g_printBuffer,
                "But all is not lost, with a tidy sum of money acquired by liquidating your late uncle's ");
            strcat(g_printBuffer,
                "butterfly collection and a royal deed of grant from the local magistrate, you are ready to return ");
            AppendCourseName(-1);
            strcat(g_printBuffer, " to it's former preeminence in the world of Golfe.");
            break;
        case THEME_TROPICAL:
            strcpy(g_printBuffer, "Welcome to ");
            AppendCourseName(-1);
            strcat(g_printBuffer, " Golf Course. This delightful slice of tropical paradise ");
            strcat(g_printBuffer, "needs only your help to become a world-class golfing destination. ");
            strcat(g_printBuffer,
                "After all, who doesn't enjoy a little humidity - and the alligators have been really well behaved lately. ");
            strcat(g_printBuffer, "With the successful conclusion of the recent mosquito eradication program and the ");
            strcat(g_printBuffer, "dedication of the new 'El Presidente' international airport, ");
            AppendCourseName(-1);
            strcat(g_printBuffer, " is bursting with potential. ");
            break;
        }
        ShowStoryMessage(0x8000211f, 1, -4);
        g_messageCountdown = REPEAT_MESSAGE_TICKS;
    }

    if (g_clock == 0x1400) {
        strcpy(g_printBuffer,
            "Building a great golf course requires imagination, intuition, persistence, and a bulldozer. ");
        strcat(g_printBuffer,
            "The basic idea is to present the player with a variety of shots which appear challenging ");
        strcat(g_printBuffer, "and require strategic thinking, but are within the player's ability. ");
        strcat(g_printBuffer, "This philosophy is known as 'look hard and play easy.' ");
        strcat(g_printBuffer,
            "Players who enjoy your course will tell their friends, return more often, and pay higher greens fees!");
        ShowStoryMessage(0x80002190, 0, -4);
    }
}

void SetupTutorialMessage(void)
{
    g_paintedCameraTileX = -1;
    g_camDirIdx = 0;

    switch (g_nextTutStepIdx) {
    case 1:
        g_cameraZoom = TUTORIAL_ZOOM_WIDE;
        strcpy(g_printBuffer,
            "Welcome to the SimGolf tutorial. We'll show you a couple of ways to make your course more fun for ");
        strcat(g_printBuffer, "your players. To proceed, press any key, to exit the tutorial press Escape.");
        break;

    case 2: {
        Hole *h = &g_allHolesArray[1];
        g_cameraTileX = h->teeX + 8;
        g_cameraTileY = h->teeY - 8;
    }
        g_cameraZoom = MAX_ZOOM;
        strcpy(g_printBuffer, "Players can get bored by easy shots and enjoy making shots that test their skills. ");
        strcat(g_printBuffer, "At the hole below players must hit their shots over the water. This adds to the ");
        strcat(g_printBuffer, "excitement and fun of the hole... ");
        strcat(g_printBuffer, "  (to proceed, press any key, to exit the tutorial press Escape).");
        break;

    case 3:
        g_cameraTileX = (g_allHolesArray[1].aimX + g_allHolesArray[1].teeX) / 2 + 5;
        g_cameraTileY = (g_allHolesArray[1].teeY + g_allHolesArray[1].aimY) / 2 - 5;
        strcpy(g_printBuffer, "Note that while the shot looks scary it ");
        strcat(g_printBuffer, "is really not that difficult. The green is also protected by bunkers and vegetation. ");
        strcat(g_printBuffer, "This is a secret to fun golf holes; shot that ");
        strcat(g_printBuffer, "look hard but are quite makeable.");
        break;

    case 4:
        g_cameraTileX = (g_allHolesArray[2].teeX + g_allHolesArray[2].aimX) / 2 + 8;
        g_cameraTileY = (g_allHolesArray[2].teeY + g_allHolesArray[2].aimY) / 2 - 4;
        strcpy(g_printBuffer,
            "Part of the fun of golfing is the pleasant environment. Holes which include scenic elements are ");
        strcat(g_printBuffer,
            "especially fun to play. Note how this hole includes a scenic ancient elm right in the middle of the fairway! ");
        break;

    case 5: {
        Hole *h = &g_allHolesArray[2];
        g_cameraTileX = (h->aimX + h->teeX) / 2 + 4;
        g_cameraTileY = (h->aimY + h->teeY) / 2 - 4;
    }
        strcpy(g_printBuffer,
            "Try to include scenic terrain in your holes where possible. Imagine where your players are looking as they ");
        strcat(g_printBuffer, "line up their shots and position your scenic elements where the players will see them.");
        break;

    case 6:
        g_cameraTileX = (g_allHolesArray[3].aimX + g_allHolesArray[3].teeX) / 2 + 4;
        g_cameraTileY = (g_allHolesArray[3].teeY + g_allHolesArray[3].aimY) / 2 - 4;
        strcpy(g_printBuffer, "Player enjoy interesting shots which cannot be aimed directly at the hole. ");
        strcat(g_printBuffer, "Note how this hole requires players to first aim their shots to the right to get ");
        strcat(g_printBuffer, "around the grove of trees.");
        break;

    case 7:
        strcpy(g_printBuffer, "This type of hole is called a 'dogleg' hole. ");
        strcat(g_printBuffer, "You can utilize a variety of terrain elements to create a dogleg hole.");
        break;

    case 8: {
        Building *v = &g_buildingTable[5];
        g_cameraTileX = v->row + 4;
        g_cameraTileY = v->col - 4;
    }
        strcpy(g_printBuffer,
            "Of course, golfers do not live by golf alone. After a while your players will become hungry and thirsty. ");
        strcat(g_printBuffer, "A convenient snack bar will cheer them up as they play.");
        break;

    case 9:
        strcpy(g_printBuffer, "Be sure to include plenty of benches, flower beds, shade trees, and other amenities ");
        strcat(g_printBuffer, "to insure the happiness of your golfers.");
        break;

    case 10: {
        Building *v = &g_buildingTable[0];
        g_cameraTileX = v->row + 4;
        g_cameraTileY = v->col - 4;
    }
        g_cameraZoom = TUTORIAL_ZOOM_WIDE;
        strcpy(g_printBuffer, "These are just a few of the ways to add fun and interest to your course. ");
        strcat(g_printBuffer,
            "Check the summaries at each hole to see what your players are saying and don't be afraid to try something creative and unusual. ");
        break;

    case 21: {
        Hole *h = &g_allHolesArray[2];
        g_cameraTileX = h->teeX + 8;
        g_cameraTileY = h->teeY - 8;
    }
        g_cameraZoom = TUTORIAL_ZOOM_WIDE;
        strcpy(g_printBuffer,
            "Welcome to the Sim Golf Course Tycoon tutorial. We'll show you a couple of ways to add skill requirements your course. ");
        strcat(g_printBuffer, "To proceed, press any key, to exit the tutorial press Escape.");
        break;

    case 22: {
        Hole *h = &g_allHolesArray[2];
        g_cameraTileX = h->teeX + 8;
        g_cameraTileY = h->teeY - 8;
    }
        g_cameraZoom = TUTORIAL_ZOOM_WIDE;
        strcpy(g_printBuffer,
            "Holes are rated for three types of skill: Length, the ability to hit the ball a long distance, ");
        strcat(g_printBuffer,
            "Accuracy, having the ball go where you want it to go, and Imagination, the ability to curve and shape ");
        strcat(g_printBuffer, "your shots to avoid hazards.");
        strcat(g_printBuffer, "  (to proceed, press any key, to exit the tutorial press Escape).");
        break;

    case 23: {
        Hole *h = &g_allHolesArray[1];
        g_cameraTileX = h->teeX + 5;
        g_cameraTileY = h->teeY - 10;
    }
        g_cameraZoom = MAX_ZOOM;
        strcpy(g_printBuffer, "This is a hole designed to require length. In addition to being long, note how ");
        strcat(g_printBuffer, "shorter hitters land on the upslope of the hill while longer hitters get an extra ");
        strcat(g_printBuffer, "kick by landing on the downslope.");
        break;

    case 24: {
        Hole *h = &g_allHolesArray[2];
        g_cameraTileX = h->teeX + 5;
        g_cameraTileY = h->teeY - 5;
    }
        strcpy(g_printBuffer,
            "This is a hole designed to require accuracy. Only the most accurate hitters can place their ball ");
        strcat(g_printBuffer, "on this postage stamp green in the middle of the lake!");
        break;

    case 25: {
        Hole *h = &g_allHolesArray[2];
        g_cameraTileX = h->teeX + 4;
        g_cameraTileY = h->teeY - 4;
    }
        strcpy(g_printBuffer, "Less accurate hitters may bail out to the small island fairway.");
        break;

    case 26: {
        Hole *h = &g_allHolesArray[3];
        g_cameraTileX = h->teeX + 6;
        g_cameraTileY = h->teeY - 6;
    }
        strcpy(g_printBuffer,
            "This is a hole designed to require imagination. To achieve maximum distance, the drive from the tee must be curved from right to left ");
        strcat(g_printBuffer, "to skirt the grove of trees and take advantage of the slope of the hill.");
        ShowStoryMessage(0x80002190, 1, -1);
        g_nextTutStepIdx++;
        return;

    case 27: {
        Hole *h = &g_allHolesArray[3];
        g_cameraTileX = h->aimX + 1;
        g_cameraTileY = h->aimY - 1;
    }
        strcpy(g_printBuffer, "The next shot, ");
        strcat(g_printBuffer,
            "across the water to the green, must be brought in high with backspin to stop on the green.");
        ShowStoryMessage(0x80002190, 1, -1);
        g_nextTutStepIdx++;
        return;

    case 28: {
        Hole *h = &g_allHolesArray[2];
        g_cameraTileX = h->teeX + 8;
        g_cameraTileY = h->teeY - 8;
    }
        g_cameraZoom = TUTORIAL_ZOOM_WIDE;
        strcpy(g_printBuffer, "Of course, the best golf holes require all three skills. ");
        strcat(g_printBuffer, "As you improve as a golf course designer, try to design classic holes which require ");
        strcat(g_printBuffer, "length, accuracy, and imagination. Good Luck.");
        break;

    case 11:
    case 29:
        SaveGame("One");
        {
            Building *v = &g_buildingTable[0];
            g_cameraTileX = v->row + 4;
            g_cameraTileY = v->col - 4;
        }
        g_cameraZoom = DEFAULT_ZOOM;
        strcpy(g_printBuffer, "Now back to your course.");
        ShowStoryMessage(0x80002190, 1, -1);
        g_runModeFlags &= ~GAME_TUTORIAL;
        g_nextTutStepIdx = 0;
        return;

    default: return;
    }

    ShowStoryMessage(0x80002190, 1, -1);
    g_nextTutStepIdx++;
}


void BuildHoleTypeMessage(int hole, int type)
{
    strcpy(g_printBuffer, "Golf holes fall into various categories depending on the skills they require. Hole ");
    strcat(g_printBuffer, itoa(hole, g_numBuf, 10));
    strcat(g_printBuffer, " has been recognized as your first ");

    switch (type) {
    case 1:
        strcat(g_printBuffer, "'FREEWAY' type hole. Freeway holes reward the player who hits the ball the farthest.");
        break;
    case 2:
        strcat(g_printBuffer, "'PRECISE' type hole. Precision holes reward accurate shots, length and imagination are "
                              "less important here.");
        break;
    case 3:
        strcat(g_printBuffer, "'CHALLENGE' type hole. Challenging holes require both length and accuracy, without "
                              "requiring the creative shotmaking of imaginative players.");
        if (g_level >= 2)
            TriggerAccomplishment(0, (g_allHolesArray[hole].aimX << 10) + 0x200,
                (g_allHolesArray[hole].aimY << 10) + 0x200);
        break;
    case 4:
        strcat(g_printBuffer, "'CREATIVE' type hole. Creative holes require the player to use imagination to create "
                              "unexpected shots, length and accuracy are less important here.");
        break;
    case 5:
        strcat(g_printBuffer, "'HEROIC' type hole. Heroic holes reward both length and imaginative shot selection, "
                              "accuracy is not so important.");
        if (g_level >= 2)
            TriggerAccomplishment(1, (g_allHolesArray[hole].aimX << 10) + 0x200,
                (g_allHolesArray[hole].aimY << 10) + 0x200);
        break;
    case 6:
        strcat(g_printBuffer,
            "'STRATEGIC' type hole. Strategic holes require a combination of accuracy and imaginative "
            "shot selection, length is not so important here.");
        if (g_level >= 2)
            TriggerAccomplishment(4, (g_allHolesArray[hole].aimX << 10) + 0x200,
                (g_allHolesArray[hole].aimY << 10) + 0x200);
        break;
    case 7:
        strcat(g_printBuffer, "'CLASSIC' type hole. A Classic hole demands maxim um skillfullness from the golfer: "
                              "length, accuracy, and imaginative shot selection.");
        TriggerAccomplishment(9, (g_allHolesArray[hole].aimX << 10) + 0x200,
            (g_allHolesArray[hole].aimY << 10) + 0x200);
        break;
    }
}

void RenderScorecard(int player)
{
    int golfer;
    Hole *pHole;
    int x;
    int matchTotal;
    int y;
    int hole;
    unsigned color;

    g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
    y = 0x228;
    golfer = player;
    matchTotal = 0;
    DrawTextWithColor("Hole", 0x19a, 0x207, COLOR_WHITE);
    DrawTextWithColor("Par", 0x19e, 0x216, COLOR_WHITE);
    g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
    if ((g_golferPool[player ^ 1].persona & PERSONA_KIND) == PERSONA_ROSTER) {
        if (g_runModeFlags & GAME_TOURNAMENT) {
            DrawTextWithColor("Professional", 0x145, 0x20a, 0x80000000);
            DrawTextWithColor("Tournament", 0x145, 0x215, 0x80000000);
        } else {
            DrawTextWithColor("Exhibition", 0x14e, 0x20a, 0x80000000);
            strcpy(g_printBuffer, "§");
            FormatNumberWithCommas(g_proMatchStakeLvl * EXHIBITION_STAKE);
            strcat(g_printBuffer, "/hole");
            DrawTextWithColor(g_printBuffer, 0x140, 0x215, 0x80000000);
        }
    } else {
        DrawTextWithColor(" Practice", 0x14e, 0x20a, 0x80000000);
        DrawTextWithColor("  Round", 0x14e, 0x215, 0x80000000);
    }
    do {
        g_printBuffer[0] = 0;
        AppendGolferName(golfer, 0);
        if (golfer == player)
            strcat(g_printBuffer, " vs.");
        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
        if (g_pMenu->text_width(g_printBuffer) > 0x73)
            g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
        DrawTextWithColor(g_printBuffer, 0x140, y, 0x80000000);
        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
        hole = 0;
        pHole = g_allHolesArray;
        x = 0x1aa;
        do {
            if (pHole->par != 0) {
                if (golfer == player) {
                    g_printBuffer[0] = 0;
                    strcat(g_printBuffer, itoa(hole, g_numBuf, 10));
                    DrawTextCenteredAtY(g_printBuffer, x, y - 0x22, COLOR_LIGHT_GREY);
                    g_printBuffer[0] = 0;
                    strcat(g_printBuffer, itoa(pHole->par, g_numBuf, 10));
                    DrawTextCenteredAtY(g_printBuffer, x, y - 0x13, COLOR_LIGHT_GREY);
                }
                if (g_golferPool[golfer].holeIdx >= hole) {
                    g_printBuffer[0] = 0;
                    if (g_golferPool[golfer].holeIdx == hole) {
                        strcat(g_printBuffer, itoa(g_golferPool[golfer].holeStrokes, g_numBuf, 10));
                    } else {
                        strcat(g_printBuffer, itoa(g_golferPool[golfer].holeScore[hole], g_numBuf, 10));
                        if (golfer == player)
                            matchTotal -= g_golferPool[golfer].holeScore[hole];
                        else
                            matchTotal += g_golferPool[golfer].holeScore[hole];
                    }
                    color = 0x80004210;
                    if (g_golferPool[golfer].holeIdx != hole) {
                        signed char score = g_golferPool[golfer].holeScore[hole];
                        char par = pHole->par;
                        color = 0x80002108;
                        if (score < par)
                            color = 0x80006000;
                        if (score < par - 1)
                            color = 0x80007d08;
                        if (score > par)
                            color = 0x80000018;
                        if (score > par + 1)
                            color = 0x80004010;
                    }
                    DrawTextCenteredAtY(g_printBuffer, x, y, color);
                }
            }
            hole++;
            pHole++;
            x += 0x12;
        } while (x < 0x300);
        golfer = g_golferPool[golfer].partner;
        y += 0x15;
    } while (y < 0x24e);
}
