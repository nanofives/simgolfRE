#include "jgldefs.h"
#include "SetupScreens.h"
#include "FLCPalette.h"
#include "GameSetup.h"
#include "Geometry.h"
#include "InputPoll.h"
#include "Plot.h"
#include "Screen.h"
#include "Sprite.h"
#include "SpriteAssets.h"
#include "Golfer.h"
#include "SaveGame.h"
#include "Wildlife.h"
#include "ImageSet.h"
#include "ScreenText.h"
#include "DialogFrame.h"
#include "IntroVideo.h"
#include "GolferStats.h"
#include "FloatingText.h"
#include "TextWrap.h"
#include "MainLoop.h"
#include "TerrainGrid.h"
#include "BuildingSearch.h"
#include "ThemeLoader.h"
#include "StringPool.h"
#include "ToolbarSelect.h"
#include "EditBox.h"
#include "Toolbar.h"
#include "GameMain.h"
#include "MessagePump.h"
#include "GolferDialogue.h"
#include "crt.h"
#include <windows.h>
#include "Scorecard.h"

int FileExists(const char *path)
{
    WIN32_FIND_DATAA fd;
    return FindFirstFileA(path, &fd) != INVALID_HANDLE_VALUE;
}

FLCPalette g_themeWritePalette;

struct GolferFileBlock {
    int v[4];
};

#define HEAD_BANK(t) (((unsigned)~g_golferTypeArray[t].ageBits >> 7) & 1)

int SaveGolferToTheme(int type, int golfer, int champ)
{
    GolferFileBlock block;
    const char *ext;
    FILE *fp;

    if (g_themeWritePaletteFresh) {
        g_themeWritePalette.init();
        g_themeWritePaletteFresh = 0;
    }

    strcpy(g_printBuffer, "Themes\\");
    if (golfer != -2) {
        strcat(g_printBuffer, g_activeThemeDir);
        strcat(g_printBuffer, "\\");
        strcat(g_printBuffer, g_golferTypeArray[type].name);
        ext = ".chr";
    } else {
        strcat(g_printBuffer, "Default");
    }
    if (golfer != -1)
        ext = ".pro";
    strcat(g_printBuffer, ext);

    if (champ) {
        strcpy(g_printBuffer, "Themes\\Championship\\");
        strcat(g_printBuffer, g_golferTypeArray[type].name);
        strcat(g_printBuffer, ".pro");
    }

    if (FileExists(g_printBuffer) && golfer != -2) {
        int len = strlen(g_printBuffer);
        strcat(g_printBuffer, "\nalready exists!\n Overwrite the old version.\n Cancel\n");
        if (ShowActionDialog(0xc8, 0x1e, 1, 1, 0))
            return 0;
        g_printBuffer[len] = 0;
    }

    if (!ValidateAndTrimString(g_golferTypeArray[type].name)) {
        strcat(g_printBuffer, "\nInvalid file name.\n");
        ShowListDialogWithParams(g_printBuffer, 0x1e, 0x1e);
        return 0;
    }

    fp = fopen(g_printBuffer, "wb");
    if (!fp) {
        strcat(g_printBuffer, "\nInvalid file path.\n");
        ShowListDialogWithParams(g_printBuffer, 0x1e, 0x1e);
        return 0;
    }

    fwrite(&g_golferTypeArray[type], 0x230, 1, fp);
    fwrite(&g_phrasesPerGolferType[type * 0x4e2], 0x4e2, 1, fp);

    memset(&block, 0, sizeof block);
    if (golfer != -1) {
        const unsigned char *src = g_golferPool[golfer].ratings;
        memcpy(&block, src, sizeof block);
    }
    if (type == 0)
        memcpy(&block, g_golferSkillGrades, sizeof block);
    fwrite(&block, sizeof block, 1, fp);
    fwrite("*PCXFILE", 8, 1, fp);

    {
        AssetSurface surf;
        SpriteObject *face, *cell, *third;

        surf.Create(HEAD_PORTRAIT_SIZE, 0x1a4, 8, 1, 0, 0);
        surf.fill(0, 0, 0x8c, 0x1a4, 0xff);
        if (g_golferTypeArray[type].head < 20) {
            g_themeWritePalette.pal =
                g_playerHeadBmpArray[(HEAD_BANK(type) * 72 + g_golferTypeArray[type].head) * 3 + 1]
                    .m_surface->get_palette();
            surf.sync_to_palette(&g_themeWritePalette);
            face = &g_playerHeadBmpArray[(HEAD_BANK(type) * 72 + g_golferTypeArray[type].head) * 3];
            face->Draw(&surf, 0, 0, 1, 1, 1, 0);
            cell = &g_playerHeadBmpArray[(HEAD_BANK(type) * 72 + g_golferTypeArray[type].head) * 3 + 1];
            cell->Draw(&surf, 0, 0x8c, 1, 1, 1, 0);
            third = &g_playerHeadBmpArray[(HEAD_BANK(type) * 72 + g_golferTypeArray[type].head) * 3 + 2];
            third->Draw(&surf, 0, 0x118, 1, 1, 1, 0);
        } else {
            int i = g_golferTypeArray[type].head - 20;
            g_themeWritePalette.pal =
                g_playerHeadBmpArray[(HEAD_BANK(type) * 72 + i) * 3 + 61].m_surface->get_palette();
            surf.sync_to_palette(&g_themeWritePalette);
            face = &g_playerHeadBmpArray[(HEAD_BANK(type) * 72 + i) * 3 + 60];
            face->Draw(&surf, 0, 0, 1, 1, 1, 0);
            cell = &g_playerHeadBmpArray[(HEAD_BANK(type) * 72 + i) * 3 + 61];
            cell->Draw(&surf, 0, 0x8c, 1, 1, 1, 0);
            third = &g_playerHeadBmpArray[(HEAD_BANK(type) * 72 + i) * 3 + 62];
            third->Draw(&surf, 0, 0x118, 1, 1, 1, 0);
        }
        surf.SaveToPCX(fp);
        fclose(fp);
    }
    return 1;
}

#pragma optimize("a", on)
int LoadGolferFromTheme(const char *file, int type, int golfer)
{
    char name[64];
    int block[4];
    FILE *fp;
    int sex, n, slot;
    int *p, *q;

    strcpy(name, file);
    if (type <= 0x4c) {
        strcpy(g_printBuffer, "Themes\\");
        strcat(g_printBuffer, g_activeThemeDir);
        strcat(g_printBuffer, "\\");
    } else {
        strcpy(g_printBuffer, "Themes\\Standard\\");
    }
    strcat(g_printBuffer, name);
    fp = fopen(g_printBuffer, "rb");
    if (fp == 0)
        return 0;
    fread(&g_golferTypeArray[type], 0x230, 1, fp);
    fread(&g_phrasesPerGolferType[type * 0x4e2], 0x4e2, 1, fp);
    fread(block, 16, 1, fp);
    if (golfer != -1 && (type != 0 || (g_runModeFlags & GAME_CHAMPIONSHIP))) {
        p = g_golferPool[golfer].ratingWords;
        p[0] = block[0];
        p[1] = block[1];
        p[2] = block[2];
        p[3] = block[3];
        if (type == 0) {
            q = (int *)g_golferSkillGrades;
            q[0] = block[0];
            q[1] = block[1];
            q[2] = block[2];
            q[3] = block[3];
        }
    }
    fread(block, 8, 1, fp);
    g_golferHeadPage.LoadPCXFromFile(fp);
    sex = (g_golferTypeArray[type].ageBits & GOLFER_TYPE_FEMALE) == 0;
    if (g_golferTypeArray[type].head >= 20) {
        n = g_numHeads[sex];
        if (n < 72) {
            g_golferTypeArray[type].head = n;
            g_numHeads[sex] = n + 1;
            slot = n + 72 * sex;
            g_playerHeadBmpArray[slot * 3].extract(&g_golferHeadPage, 0, 0, 0x8c, 0x8c, 1, 1);
            g_playerHeadBmpArray[slot * 3 + 1].extract(&g_golferHeadPage, 0, 0x8c, 0x8c, 0x8c, 1, 1);
            g_playerHeadBmpArray[slot * 3 + 2].extract(&g_golferHeadPage, 0, 0x118, 0x8c, 0x8c, 1, 1);
        }
    }
    fclose(fp);
    return 1;
}
#pragma optimize("", on)

int g_headPageTopIndex;

int g_panelHoverTicks;
int g_employeeListScroll;
int g_employeeListIds[64] = {0};
int g_toolbarHoverButton = 0;
static int s_spareWord = 0;
int g_employeeListCount = 0;
SpriteObject *g_rowIconSprite = 0;
int g_peopleListScroll = 0;

int HitTestGolferCustomizationButton(int x, int y)
{
    int i = 0;
    int best = 0x28;
    int hit = -1;
    do {
        int d;
        if (i < 8)
            d = ApproxDistance((x - g_customizeSpots[i][0]) / 3, y - g_customizeSpots[i][1]);
        else
            d = ApproxDistance(x - g_customizeSpots[i][0], y - g_customizeSpots[i][1]);
        if (d < best) {
            best = d;
            hit = i;
        }
        i++;
    } while (g_customizeSpots[i][0] != 0);
    return hit;
}

int HitTestCircularButtonGrid(int x, int y)
{
    int i;

    for (i = 0; i < 10; i++) {
        if (ApproxDistance(x - g_headSlots[i][0] - 60, y - g_headSlots[i][1] - 60) < 60)
            return i;
    }
    if (ApproxDistance(x - 778, y - 363) < 60)
        return 10;
    return ApproxDistance(x - 18, y - 495) < 60 ? 11 : -1;
}

int ShowPaginatedCircularListDialog(int sex)
{
    int mx, my;

    for (;;) {
        g_cgScreenSprites[41].BlitScaled(g_pMenu, 0, 0x102, 0);
        g_simScreen.get_mouse_pos(&mx, &my);
        int hit = HitTestCircularButtonGrid(mx, my);

        for (int i = 0; i < 10; i++) {
            g_cgScreenSprites[46 + (hit == i)].Draw(g_pMenu, g_headSlots[i][0], g_headSlots[i][1], 1, 1, 1, 0);
            if (g_headPageTopIndex + i < g_numHeads[sex])
                g_playerHeadBmpArray[(g_headPageTopIndex + sex * 72 + i) * 3 + 1].Draw(g_pMenu, g_headSlots[i][0] - 8,
                    g_headSlots[i][1], 1, 1, 1, 0);
        }

        if (hit == 10)
            g_cgScreenSprites[44].BlitScaled(g_pMenu, 0x2c3, 0x11c, 0);
        else if (hit == 11)
            g_cgScreenSprites[42].BlitScaled(g_pMenu, 0xa, 0x1a9, 0);

        if (g_headPageTopIndex == 0)
            g_cgScreenSprites[43].BlitScaled(g_pMenu, 0xa, 0x1a9, 0);

        g_simScreen.RenderFullSurface(0);
        WaitWithTimeout(5, 0);
        ProcessMouseMessages();

        if (g_mouseButton != 0) {
            if (g_mouseButton == MOUSE_RIGHT || my < 0x102)
                return -1;
            if (g_mouseButton == 1) {
                if (hit != -1 && hit < 10)
                    return g_headPageTopIndex + hit;
                if (hit == 10) {
                    if (g_headPageTopIndex < 0x46)
                        g_headPageTopIndex += 10;
                } else if (hit == 11 && g_headPageTopIndex != 0) {
                    g_headPageTopIndex -= 10;
                }
                if (g_headPageTopIndex > g_numHeads[sex] - 10)
                    g_headPageTopIndex = g_numHeads[sex] - 10;
                if (g_headPageTopIndex < 0)
                    g_headPageTopIndex = 0;
            } else {
                return 0;
            }
        }
    }
}

EditBox g_golferEditBox;

void RenderGolferCustomizationScreen(int type, int golfer)
{
    int special, mx, my, hit, sel, variant, head, frame, pose, ypos, edited;
    int k, variantSel, t;
    char *bio;
    SpriteObject *face;
    AssetSurface *menu;
    unsigned nameColor;

    edited = 0;
    special = ((g_golferPool[golfer].persona & PERSONA_KIND) == PERSONA_ROSTER);
    g_golferTypeBackup = g_golferTypeArray[type];
    g_headPageTopIndex = 0;
    FadeOutSound(SOUND_COURSE_AMBIENCE);
    ConfigureAndPlaySound((g_golferTypeArray[type].ageBits & GOLFER_TYPE_FEMALE) ? 0x7d : 0x80, SOUND_FULL_VOLUME, 0, 0,
        0);

    g_golferPool[PREVIEW_GOLFER].typeIdx = type;
    g_golferPool[PREVIEW_GOLFER].talentLevel = g_rng.NextInt(3);
    g_golferPool[PREVIEW_GOLFER].ageFlags = g_golferTypeArray[type].ageBits;
    g_golferPool[PREVIEW_GOLFER].persona = 0;
    g_golferPool[PREVIEW_GOLFER].talents = g_golferPool[golfer].talents;

    AssetSurface surf;
    if (g_golferTypeArray[type].ageBits & GOLFER_TYPE_FEMALE)
        surf.load_pcx("interface\\CustGolfBckgrnd", 0, 0, PALETTE_COLORS, 2);
    else
        surf.load_pcx("interface\\CustGlfBckMale", 0, 0, PALETTE_COLORS, 2);

    g_mouseButton = 0;
    for (;;) {
        surf.copy(&g_simScreen, 0, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        g_simScreen.get_mouse_pos(&mx, &my);
        hit = HitTestGolferCustomizationButton(mx, my);
        if (hit == 0xf && !(g_golferTypeArray[type].ageBits & GOLFER_TYPE_FEMALE))
            hit = -1;

        sel = -1;
        if (hit != -1 && hit != 0x14) {
            g_cgScreenSprites[hit * 2].Draw(g_pMenu, g_customizeDrawSpots[hit][0], g_customizeDrawSpots[hit][1], 1, 1,
                0);
        } else if (abs(mx - 0x9a) < 0x50 && abs(my - 0x40) < 0x28) {
            switch ((my - 0x18) / 20) {
            case 0: sel = 0; break;
            case 1: sel = 1; break;
            case 2: sel = 2; break;
            case 3: sel = 3; break;
            case 4: sel = 4; break;
            }
        }

        t = g_golferPool[PREVIEW_GOLFER].typeIdx;
        variant = (g_golferTypeArray[t].appearance >> 4) & 3;
        head = g_golferTypeArray[t].head;
        if (g_golferTypeArray[t].fixedKind & TYPE_JUNIOR)
            g_juniorsBarrelArt.BlitScaled(g_pMenu, 0x160, 0x74, 0);
        SetupGolferAppearance(PREVIEW_GOLFER);
        frame = (g_golferTypeArray[t].fixedKind & TYPE_JUNIOR) ? variant + 4 : variant;
        g_gameGolferBodyGfx[frame + IsGolferMale(PREVIEW_GOLFER) * 8].BlitScaled(g_pMenu, 0x163, 0x74, g_palettes);
        menu = g_pMenu;
        if (head < 20) {
            face = &g_allExpressionHeads[(head + IsGolferMale(PREVIEW_GOLFER) * 20) * 3
                                         + GetGolferMoodIndex(PREVIEW_GOLFER)];
            face->Draw(menu, 0x150, 0x14, 1, 1, 1, 0);
        } else {
            face = &g_playerHeadBmpArray[(head + IsGolferMale(PREVIEW_GOLFER) * 72) * 3
                                         + GetGolferMoodIndex(PREVIEW_GOLFER)];
            face->Draw(menu, 0x13c, 0x14, 1, 1, 1, 0);
        }
        if (graphsys != 0)
            graphsys->set_sprite_scale(1, 1, 1);
        g_cgScreenSprites[40].BlitScaled(g_pMenu, 0x12b, 0xdc, 0);

        if (IsGolferMale(PREVIEW_GOLFER) == 0) {
            t = g_golferPool[PREVIEW_GOLFER].typeIdx;
            pose = 7;
            if (g_golferTypeArray[t].ageBits & AGE_PLUS_20)
                pose = 6;
            if (g_golferTypeArray[t].caps & TRAIT_PLAYFUL)
                pose = 8;
            if (g_golferTypeArray[t].ageBits & AGE_PLUS_45)
                pose = 5;
            if (g_golferTypeArray[t].fixedKind != 0)
                pose = ((g_golferTypeArray[t].appearance >> 4) & 3) + 5;
        } else {
            unsigned char talents = g_golferPool[PREVIEW_GOLFER].talents;
            if (talents & TALENT_IMAGINATION) {
                if (talents & TALENT_ACCURACY)
                    pose = 2;
                else
                    pose = 3;
            } else
                pose = 1;
            t = g_golferPool[PREVIEW_GOLFER].typeIdx;
            if (g_golferTypeArray[t].fixedKind != 0)
                pose = (g_golferTypeArray[t].appearance >> 4) & 3;
            if (g_golferPool[PREVIEW_GOLFER].persona == PERSONA_ROSTER)
                pose = g_golfProList[g_golferPool[PREVIEW_GOLFER].rosterIndex].figure;
        }
        g_rowIconSprite = GetFlicBankSprite(pose, 5, 0);
        DrawSpriteImmediate(0x13b, 0xef, 0xef, 0x1e, 0x28, g_rowIconSprite, SPRITE_FULL_SCALE, 0x199);
        g_rowIconSprite = GetFlicBankSprite(pose, 5, 4);
        DrawSpriteImmediate(0x14f, 0xef, 0xef, 0x1e, 0x28, g_rowIconSprite, SPRITE_FULL_SCALE, 0x199);

        g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
        g_printBuffer[0] = 0;
        AppendGolferName(PREVIEW_GOLFER, 0);
        if (g_golferTypeArray[g_golferPool[PREVIEW_GOLFER].typeIdx].fixedKind & TYPE_JUNIOR)
            strcat(g_printBuffer, " Jr.");
        nameColor = sel ? 0x80000848 : 0x80007b20;
        DrawTextCenteredWithShadow(g_printBuffer, 0x9a, 0x18, nameColor);
        strcpy(g_printBuffer, "(");
        strcat(g_printBuffer, g_golferTypeArray[g_golferPool[PREVIEW_GOLFER].typeIdx].title);
        strcat(g_printBuffer, ")");
        DrawTextCenteredWithShadow(g_printBuffer, 0x9a, 0x30, (sel == 1) ? 0x80007fff : 0x80000848);

        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
        g_printBuffer[0] = 0;
        if (g_golferPool[PREVIEW_GOLFER].ageFlags & 0x08)
            strcpy(g_printBuffer, "Single");
        if (g_golferPool[PREVIEW_GOLFER].ageFlags & 0x10)
            strcpy(g_printBuffer, "Married");
        if (g_golferPool[PREVIEW_GOLFER].ageFlags & 0x20)
            strcpy(g_printBuffer, "Divorced");
        if (g_golferPool[PREVIEW_GOLFER].ageFlags & 0x40)
            strcpy(g_printBuffer, "Widowed");
        if (sel == 2)
            DrawTextCenteredWithShadow(g_printBuffer, 0x9a, 0x45, COLOR_WHITE);
        else
            DrawTextCenteredAtY(g_printBuffer, 0x9a, 0x45, 0x80000848);
        g_printBuffer[0] = 0;
        if (g_golferPool[PREVIEW_GOLFER].ageFlags & 0x01)
            strcpy(g_printBuffer, "Young");
        if (g_golferPool[PREVIEW_GOLFER].ageFlags & 0x02)
            strcpy(g_printBuffer, "Middle Aged");
        if (g_golferPool[PREVIEW_GOLFER].ageFlags & 0x04)
            strcpy(g_printBuffer, "Mature");
        if (sel == 3)
            DrawTextCenteredWithShadow(g_printBuffer, 0x9a, 0x5a, COLOR_WHITE);
        else
            DrawTextCenteredAtY(g_printBuffer, 0x9a, 0x5a, 0x80000848);

        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
        {
            int caps = g_golferTypeArray[g_golferPool[PREVIEW_GOLFER].typeIdx].caps;
            int i, bit;
            for (i = 0; i < 5; i++) {
                bit = (1 << i) & caps;
                if (bit != 0)
                    g_cgScreenSprites[7 + i * 2].Draw(g_pMenu, g_customizeDrawSpots[3 + i][0],
                        g_customizeDrawSpots[3 + i][1], 1, 1, 0);
                strcpy(g_printBuffer, g_traitNames[i]);
                DrawTextCenteredAtY(g_printBuffer, 0x5d, 0x7a + i * 0x1c, bit ? 0x80000848 : 0x80004210);
            }
        }

        g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
        {
            int i, bit;

            for (i = 0; i < 3; i++) {
                bit = 1 << i;
                if ((char)(bit & g_golferPool[PREVIEW_GOLFER].talents) != 0)
                    g_cgScreenSprites[1 + i * 2].Draw(g_pMenu, g_customizeDrawSpots[i][0], g_customizeDrawSpots[i][1],
                        1, 1, 0);
                strcpy(g_printBuffer, g_talentNames[i]);
                g_printBuffer[0] += 0xe0;
                DrawTextCenteredAtY(g_printBuffer, 0xd4, 0x8e + i * 0x23,
                    (char)(bit & g_golferPool[PREVIEW_GOLFER].talents) ? 0x80000848 : 0x80004210);
            }
        }

        if (special) {
            DrawTextCenteredAtY("Golf Skill Levels", 0x249, 0x1e, 0x80000000);
            g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
            int k;
            int yy;
            for (k = 0, yy = 0x3d; k < 10; k++) {
                int sv = (type != 0) ? g_golferPool[golfer].ratings[k] : g_golferSkillGrades[k];
                g_simScreen.fill(0x1f2, yy - 3, 0x2a, 0xe, 0x80000000);
                g_simScreen.fill(0x1f3, yy - 2, 0x28, 0xc, sv ? 0x800023e8 : 0x80007d08);
                if (sv != 0) {
                    strcpy(g_printBuffer, "+");
                    strcat(g_printBuffer, itoa(sv * 10, g_numText, 10));
                    strcat(g_printBuffer, "%");
                    DrawTextWithColor(g_printBuffer, 0x1f5, yy, 0x80000000);
                }
                DrawTextWithColor(g_skillNames[k], 0x222, yy, sv ? 0x80000000 : 0x80004210);
                yy += 0x10;
            }
        } else {
            int n = g_golferHistoryPerCharacter[type].flags & MEMBERSHIP_MASK;
            if (g_golferHistoryPerCharacter[type].finishedStoryId == -1)
                n = -1;
            else if (n > 3)
                g_simScreen.fill(0x1f4, 0x1c, 0xa0, 0x11, COLOR_TEAL);
            switch (n) {
            case -1: DrawTextCenteredAtY("Resigned", 0x244, 0x1e, 0x80006000); break;
            case 1: DrawTextCenteredAtY("Visitor", 0x244, 0x1e, COLOR_LIGHT_GREY); break;
            case 2: DrawTextCenteredAtY("Member", 0x244, 0x1e, COLOR_GREY); break;
            case 3: DrawTextCenteredAtY("Silver Member", 0x244, 0x1e, COLOR_DARK_GREY); break;
            case 4: DrawTextCenteredAtY("Gold Member", 0x244, 0x1e, 0x80007ff0); break;
            case TOP_TIER: DrawTextCenteredWithShadow("Platinum Member", 0x244, 0x1e, COLOR_WHITE); break;
            }
            g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);

            strcpy(g_printBuffer, "Low round: ");
            if (g_golferHistoryPerCharacter[type].bestScore != 0)
                strcat(g_printBuffer, itoa(g_golferHistoryPerCharacter[type].bestScore, g_numText, 10));
            else
                strcat(g_printBuffer, "-");
            DrawTextCenteredAtY(g_printBuffer, 0x244, 0x32, COLOR_DARK_GREY);

            strcpy(g_printBuffer, "Handicap: ");
            if (g_golferHistoryPerCharacter[type].avgScore > 0)
                strcat(g_printBuffer, itoa(g_golferHistoryPerCharacter[type].avgScore, g_numText, 10));
            else
                strcat(g_printBuffer, "-");
            DrawTextCenteredAtY(g_printBuffer, 0x244, 0x40, COLOR_DARK_GREY);

            strcpy(g_printBuffer, "Rounds played: ");
            if (g_golferHistoryPerCharacter[type].count != 0)
                strcat(g_printBuffer, itoa(g_golferHistoryPerCharacter[type].count, g_numText, 10));
            else
                strcat(g_printBuffer, "none");
            DrawTextCenteredAtY(g_printBuffer, 0x244, 0x4e, COLOR_DARK_GREY);

            DrawTextCenteredAtY("Bio", 0x244, 0x62, (hit == 0x14) ? 0x80007b20 : 0x8000211f);
            strcpy(g_printBuffer, g_golferTypeArray[type].bio);
            DrawTextWordWrapped(0x1e, 0x1e4, 0x72, 0x80000018);
        }
        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);

        ypos = 0x10f;
        k = 0;
        do {
            unsigned color;
            g_golferPool[PREVIEW_GOLFER].rapport = 0;
            g_printBuffer[0] = 0;
            BuildGolferDialogue(g_typeLineIds[k], g_actionArgs[g_typeLineIds[k]] & 0x3fff, 0, PREVIEW_GOLFER);
            if (g_remarkColourValue == 0x80006318)
                g_remarkColourValue = 0x80006318;
            strcpy(g_printBuffer, g_quoteSituationNames[k]);
            strcat(g_printBuffer, "...");
            DrawTextWithColor(g_printBuffer, 0x34, ypos, g_remarkColourValue);
            strcpy(g_printBuffer, &g_phrasesPerGolferType[type * 1250 + k * 50]);
            color = 0x80000848;
            g_remarkColourValue = color;
            if (g_printBuffer[0] == 0) {
                BuildGolferDialogue(g_typeLineIds[k], g_actionArgs[g_typeLineIds[k]] & 0x3fff, 0, PREVIEW_GOLFER);
                color = 0x80004210;
                g_remarkColourValue = color;
            }
            DrawTextWithColor(g_printBuffer, 0x19c, ypos, color);
            ypos += 0x10;
            k++;
        } while (g_typeLineIds[k] != -1);
        g_golferPool[PREVIEW_GOLFER].typeIdx = type;

        if (hit == g_panelHoverButton && hit != -1) {
            if (++g_panelHoverTicks > 10) {
                g_printBuffer[0] = 0;
                switch (hit) {
                case 8: strcpy(g_printBuffer, "Load character"); break;
                case 9: strcpy(g_printBuffer, "Save character"); break;
                case 10: strcpy(g_printBuffer, "Body type"); break;
                case 11: strcpy(g_printBuffer, "Adult/child"); break;
                case 12: strcpy(g_printBuffer, "Select face"); break;
                case 13: strcpy(g_printBuffer, "Shirt Color"); break;
                case 14: strcpy(g_printBuffer, "Pants Color"); break;
                case 15: strcpy(g_printBuffer, "Hair Color"); break;
                case 16: strcpy(g_printBuffer, "Skin Tone"); break;
                case 17: strcpy(g_printBuffer, "Gender"); break;
                case 18: strcpy(g_printBuffer, "Cancel"); break;
                case 19: strcpy(g_printBuffer, "Exit"); break;
                case 20:
                    if (!special)
                        strcpy(g_printBuffer, "Update Bio");
                    break;
                }
                if (g_printBuffer[0] != 0)
                    RenderToolbarTooltip(mx, my);
            }
        } else {
            g_panelHoverTicks = 0;
            g_panelHoverButton = hit;
        }
        g_simScreen.RenderFullSurface(0);
        WaitWithTimeout(5, 0);
        ProcessMouseMessages();
        WaitWithTimeout(5, 0);

        if (g_mouseButton == 0)
            continue;

        switch (hit) {
        case 0: g_golferPool[PREVIEW_GOLFER].talents ^= TALENT_LENGTH; break;
        case 1: g_golferPool[PREVIEW_GOLFER].talents ^= TALENT_ACCURACY; break;
        case 2: g_golferPool[PREVIEW_GOLFER].talents ^= TALENT_IMAGINATION; break;
        case 3: g_golferTypeArray[type].caps ^= 1; break;
        case 4: g_golferTypeArray[type].caps ^= 2; break;
        case 5: g_golferTypeArray[type].caps ^= 4; break;
        case 6: g_golferTypeArray[type].caps ^= TRAIT_PLAYFUL; break;
        case 7: g_golferTypeArray[type].caps ^= 0x10; break;

        case 8: {
            strcpy(g_printBuffer, "Themes\\");
            strcat(g_printBuffer, g_activeThemeDir);
            strcat(g_printBuffer, special ? "\\*.pro" : "\\*.chr");
            int cnt = FindFilesWithFilter(g_printBuffer, g_empty, g_empty);
            if (cnt == 0)
                break;
            strcpy(g_printBuffer, "Pick one...\n ");
            for (k = 0; k < cnt; k++) {
                strcat(g_printBuffer, g_fileList[k]);
                strcat(g_printBuffer, "\n ");
            }
            k = ShowListDialogWithParams(g_printBuffer, 0x64, 0x14);
            if (k == -1)
                break;
            LoadGolferFromTheme(g_fileList[k], type, special ? golfer : -1);
            break;
        }

        case 9: {
            if (SaveGolferToTheme(type, special ? golfer : -1, 0) == 0)
                break;
            strcpy(g_printBuffer, "Character saved as\nThemes\\");
            strcat(g_printBuffer, g_activeThemeDir);
            strcat(g_printBuffer, "\\");
            strcat(g_printBuffer, g_golferTypeArray[type].name);
            strcat(g_printBuffer, !special ? ".chr\n" : ".pro\n");
            ShowActionDialog(0x12c, 0x64, 0, 1, 0);
            break;
        }

        case 10:
            if (g_mouseButton == 1)
                variantSel = (variantSel + 1) % 4;
            else
                variantSel = (variantSel + 3) % 4;
            g_golferTypeArray[type].appearance =
                (variantSel << 4) | (g_golferTypeArray[type].appearance & APPEARANCE_LOW_BITS);
            g_golferTypeArray[type].fixedKind |= 0x80;
            break;

        case 11: g_golferTypeArray[type].fixedKind = (g_golferTypeArray[type].fixedKind ^ 0x8) | 0x80; break;

        case 12: {
            int face = ShowPaginatedCircularListDialog(IsGolferMale(PREVIEW_GOLFER));
            if (face == -1)
                break;
            int preset = face;
            g_golferTypeArray[type].head = face;
            if (face >= 0x13)
                break;
            if (IsGolferMale(PREVIEW_GOLFER) == 0)
                preset = face + 0x14;
            g_golferTypeArray[type].skinTone = g_headPresets[preset].skinTone;
            g_golferTypeArray[type].hairColor = g_headPresets[preset].hairColor;
            break;
        }

        case 13:
            if (g_mouseButton == 1) {
                g_golferTypeArray[type].shirtColor++;
                g_golferTypeArray[type].shirtColor = g_golferTypeArray[type].shirtColor % 10;
            } else {
                g_golferTypeArray[type].shirtColor--;
                g_golferTypeArray[type].shirtColor = (g_golferTypeArray[type].shirtColor + 10) % 10;
            }
            g_golferTypeArray[type].fixedKind |= 0x80;
            break;

        case 14:
            if (g_mouseButton == 1) {
                g_golferTypeArray[type].pantsColor++;
                g_golferTypeArray[type].pantsColor = g_golferTypeArray[type].pantsColor % 10;
            } else {
                g_golferTypeArray[type].pantsColor--;
                g_golferTypeArray[type].pantsColor = (g_golferTypeArray[type].pantsColor + 10) % 10;
            }
            g_golferTypeArray[type].fixedKind |= 0x80;
            break;

        case 15:
            if (g_mouseButton == 1) {
                g_golferTypeArray[type].hairColor++;
                g_golferTypeArray[type].hairColor = g_golferTypeArray[type].hairColor % 5;
            } else {
                g_golferTypeArray[type].hairColor--;
                g_golferTypeArray[type].hairColor = (g_golferTypeArray[type].hairColor + 5) % 5;
            }
            g_golferTypeArray[type].fixedKind |= 0x80;
            break;

        case 16:
            if (g_mouseButton == 1) {
                g_golferTypeArray[type].skinTone++;
                g_golferTypeArray[type].skinTone = g_golferTypeArray[type].skinTone % 4;
            } else {
                g_golferTypeArray[type].skinTone--;
                g_golferTypeArray[type].skinTone = (g_golferTypeArray[type].skinTone + 4) % 4;
            }
            g_golferTypeArray[type].fixedKind |= 0x80;
            break;

        case 17:
            g_golferTypeArray[type].ageBits ^= GOLFER_TYPE_FEMALE;
            g_golferTypeArray[type].fixedKind |= 0x80;
            if ((g_golferTypeArray[type].ageBits & GOLFER_TYPE_FEMALE) != 0)
                surf.load_pcx("interface\\CustGolfBckgrnd", 0, 0, PALETTE_COLORS, 2);
            else
                surf.load_pcx("interface\\CustGlfBckMale", 0, 0, PALETTE_COLORS, 2);
            break;

        case 18: g_golferTypeArray[type] = g_golferTypeBackup; break;

        case 19:
            g_golferPool[golfer].talents = g_golferPool[PREVIEW_GOLFER].talents;
            edited = 1;
            g_golferTypeArray[type].fixedKind =
                (g_golferTypeArray[type].fixedKind & ~SKILLS_ALL) | (g_golferPool[PREVIEW_GOLFER].talents & SKILLS_ALL);
            break;

        case 20:
            if (special)
                break;
            DrawDialogFrameWithBorder(0x1d8, 0x68, 0x13c, 0xd8, 1);
            g_simScreen.GetPixelRed(0x258, 0x78, 0);
            g_simScreen.GetPixelGreen(0x258, 0x78, 0);
            g_simScreen.GetPixelBlue(0x258, 0x78, 0);
            {
                int white = g_activePalettePtr->get_nearest_palette_index(0xff, 0xff, 0xff, 0);
                g_golferEditBox.Show(0x1e0, 0x70, 0x12c, 0xc8, &g_simScreen, 0);
                g_golferEditBox.on_dialog_focus(1);
                g_golferEditBox.set_text_limit(0x1f4);
                bio = g_golferTypeArray[type].bio;
                g_golferEditBox.set_text(bio);
                g_golferEditBox.m_backColor = white;
                g_golferEditBox.m_bevelHiColor = 1;
                g_golferEditBox.m_bevelLoColor = 2;
                g_golferEditBox.m_border = 0;
                g_golferEditBox.m_inset = 4;
                g_golferEditBox.show(0);
            }
            g_mouseButton = 0;
            g_pendingChar = 0;
            do {
                do_task();
                do_key();
                PumpUserTick();
                ProcessMouseMessages();
                g_simScreen.RenderFullSurface(0);
            } while (g_mouseButton == 0 && g_pendingChar != 0xd);
            if (bio != 0) {
                bio[0] = '\0';
                strcat(bio, g_golferEditBox.m_text);
            }
            g_golferEditBox.close();
            break;

        default:
            if (hit != -1)
                break;
            if (abs(mx - 0x9a) < 0x50 && abs(my - 0x40) < 0x28) {
                switch ((my - 0x18) / 20) {
                case 0:
                    g_printBuffer[0] = 0;
                    ShowTextInputDialog("New name...", NAME_DIALOG_X, NAME_DIALOG_Y, 0x10);
                    if (g_printBuffer[0] != 0)
                        strcpy(g_golferTypeArray[type].name, g_printBuffer);
                    break;
                case 1:
                    g_printBuffer[0] = 0;
                    ShowTextInputDialog("Profession...", 0xc8, 0x20, 0x10);
                    if (g_printBuffer[0] != 0)
                        strcpy(g_golferTypeArray[type].title, g_printBuffer);
                    break;
                case 2: {
                    signed char a = g_golferTypeArray[type].ageBits;
                    int v = a & 0x78;
                    if (g_mouseButton == 1) {
                        v = (v & 0x3c) << 1;
                        if (v == 0)
                            v = 0x8;
                    } else {
                        v = (v >> 1) & 0x78;
                        if (v == 0)
                            v = 0x40;
                    }
                    v |= (a & 0x87);
                    g_golferTypeArray[type].ageBits = v;
                    Golfer *g = &g_golferPool[PREVIEW_GOLFER];
                    g->ageFlags &= 0xff87;
                    g->ageFlags |= v;
                    break;
                }
                case 3: {
                    signed char a = g_golferTypeArray[type].ageBits;
                    int v = a & 0x7;
                    if (g_mouseButton == 1) {
                        v = (v & 0x3) << 1;
                        if (v == 0)
                            v = 1;
                    } else {
                        v = (v >> 1) & 0x7;
                        if (v == 0)
                            v = 4;
                    }
                    v |= (a & 0xf8);
                    g_golferTypeArray[type].ageBits = v;
                    Golfer *g = &g_golferPool[PREVIEW_GOLFER];
                    g->ageFlags &= 0xfff8;
                    g->ageFlags |= v;
                    break;
                }
                }
            }
            if (abs(mx - 0x242) < 0xb2 && my >= 0x10e) {
                int line = (my - 0x10e) / 16;
                strcpy(g_printBuffer, &g_phrasesPerGolferType[(line + type * 25) * 50]);
                ShowTextInputDialog("New text...", 0x12c, (line + 0xf) * 16, 0x30);
                strcpy(&g_phrasesPerGolferType[(line + type * 25) * 50], g_printBuffer);
            }
            if (special != 0 && type != 0 && abs(mx - 0x208) < 0x19 && my >= 0x3e && my < 0xde) {
                Golfer *g = &g_golferPool[golfer];
                int skill = (my - 0x40) / 16;
                if (g_mouseButton == 1) {
                    if (g->ratings[skill] < 10)
                        g->ratings[skill]++;
                    else
                        ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                } else {
                    if (g->ratings[skill] > 0)
                        g->ratings[skill]--;
                    else
                        ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                }
            }
        }

        do {
            ProcessMouseMessages();
        } while (g_mouseButton != 0);
        WaitWithTimeout(5, 0);
        do {
            ProcessMouseMessages();
        } while (g_mouseButton != 0);
        if (edited != 0)
            break;
    }

    if ((g_golferPool[golfer].persona & PERSONA_KIND) == PERSONA_ROSTER && g_golferPool[golfer].typeIdx != 0
        && g_golferPool[golfer].rosterIndex != -1) {
        int ri = g_golferPool[golfer].rosterIndex;
        g_golfProList[ri].appearanceColor = g_golferTypeArray[type].appearance & APPEARANCE_LOW_BITS;
        g_golfProList[ri].skinTone = g_golferTypeArray[type].skinTone;
        g_golfProList[ri].shirtColor = g_golferTypeArray[type].shirtColor;
        g_golfProList[ri].pantsColor = g_golferTypeArray[type].pantsColor;
    }
    FadeOutSound((g_golferTypeArray[type].ageBits & GOLFER_TYPE_FEMALE) ? 0x7d : 0x80);
    ConfigureAndPlaySound(SOUND_COURSE_AMBIENCE, SOUND_FULL_VOLUME, 0, 0, 0);
    flush_input();
}

int ShowDifficultySelectScreen()
{
    int bx[5] = {0xc1, 0x96, 0xa1, 0xc8, 0x2dc};
    int by[5] = {0x20, 0xa1, 0x140, 0x1b4, 0x214};
    int hover, prev, frames;
    int mx, my;

    SpriteObject btn[5];
    AssetSurface sheet;

    sheet.load_pcx("interface\\TitleSelDiffMO", 0, 0, PALETTE_COLORS, 2);
    btn[0].extract(&sheet, 0, 0, 0x152, 0x92, 1, 1);
    btn[1].extract(&sheet, 0x190, 0, 0x152, 0x92, 1, 1);
    btn[2].extract(&sheet, 0, 0x12c, 0x14c, 0x72, 1, 1);
    btn[3].extract(&sheet, 0x190, 0x12c, 0x14c, 0x82, 1, 1);
    btn[4].extract(&sheet, 0x2dc, 0x214, 0x44, 0x44, 1, 1);
    sheet.load_pcx("interface\\TitleSelDiffUnSel", 0, 0, PALETTE_COLORS, 2);

    g_mouseButton = 0;
    hover = -1;
    frames = 0;

    for (;;) {
        g_pMenu->SetFontsForStyles(&g_kleptoBigFont, 0, 0, 0);
        sheet.copy(&g_simScreen, 0, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        DrawTextCenteredAtY("Select Difficulty", 0x25a, 0x2a, 0x80000000);
        if (hover != -1)
            btn[hover].BlitScaled(g_pMenu, bx[hover], by[hover], 0);
        DrawTextCenteredAtY("Easy", 0x184, hover == 0 ? 0x71 : 0x73, hover == 0 ? 0x80000000u : 0x80004210u);
        DrawTextCenteredAtY("Moderate", 0x168, hover == 1 ? 0xe3 : 0xe5, hover == 1 ? 0x80000000u : 0x80004210u);
        DrawTextCenteredAtY("Difficult", 0x16e, hover == 2 ? 0x15a : 0x15c, hover == 2 ? 0x80000000u : 0x80004210u);
        DrawTextCenteredAtY("Impossible", 0x192, hover == 3 ? 0x1d0 : 0x1d2, hover == 3 ? 0x80000000u : 0x80004210u);

        ProcessMouseMessages();
        g_simScreen.get_mouse_pos(&mx, &my);
        prev = hover;
        hover = -1;
        if (ApproxDistance((mx - 0x15c) / 2, my - 0x6b) < 0x50)
            hover = 0;
        if (ApproxDistance((mx - 0x13f) / 2, my - 0xda) < 0x50)
            hover = 1;
        if (ApproxDistance((mx - 0x140) / 2, my - 0x16f) < 0x50)
            hover = 2;
        if (ApproxDistance((mx - 0x16b) / 2, my - 0x1ef) < 0x50)
            hover = 3;
        if (ApproxDistance(mx - 0x2ff, my - 0x22d) < 0x19)
            hover = 4;
        if (hover == prev) {
            if (++frames > 0x1e && hover == 4) {
                strcpy(g_printBuffer, "Cancel");
                RenderToolbarTooltip(0x2ff, 0x22d);
            }
        } else {
            frames = 0;
        }
        g_simScreen.RenderFullSurface(0);
        WaitWithTimeout(2, 0);

        if (g_mouseButton == 0) {
            if (!PollInputState())
                continue;
            if (g_mouseButton == 0) {
                hover = -1;
                break;
            }
        }
        if (hover == 4)
            hover = -1;
        break;
    }
    return hover;
}

int ShowPickAProScreen()
{
    int firstRow;
    int sel;
    int count, maxTop;
    int cx, cy;
    int i, y;
    int hover, prevHover, frames, selShown;
    char *entry;
    char savedPro[0x80];

    int ax[5];
    int ay[5];
    ax[0] = 0x258;
    ax[1] = 0x2e4;
    ax[2] = 0x30a;
    ax[3] = 0x30a;
    ax[4] = 0x29e;
    ay[0] = 0x208;
    ay[1] = 0x212;
    ay[2] = 0x64;
    ay[3] = 0x17c;
    ay[4] = 0x208;
    strcpy(savedPro, g_activeThemeDir);
    strcpy(g_activeThemeDir, "Championship");

    SpriteObject icons[3][5];

    sel = -1;
    firstRow = 0;

    AssetSurface sheet;

    sheet.load_pcx("interface\\Title_LoadGame_MO", 0, 0, PALETTE_COLORS, 2);
    icons[1][0].extract(&sheet, 0x258, 0x208, 0x46, 0x46, 1, 1);
    icons[1][1].extract(&sheet, 0x2e4, 0x212, 0x3c, 0x3c, 1, 1);
    icons[1][2].extract(&sheet, 0x30a, 0x64, 0x16, 0x28, 1, 1);
    icons[1][3].extract(&sheet, 0x30a, 0x17c, 0x16, 0x28, 1, 1);
    icons[1][4].extract(&sheet, 0x29e, 0x208, 0x46, 0x46, 1, 1);
    icons[2][0].extract(&sheet, 0x258, 0x1b3, 0x46, 0x46, 1, 1);
    icons[2][4].extract(&sheet, 0x29e, 0x1b3, 0x46, 0x46, 1, 1);

    sheet.load_pcx("interface\\Title_PickAPro", 0, 0, PALETTE_COLORS, 2);
    icons[0][0].extract(&sheet, 0x258, 0x208, 0x46, 0x46, 1, 1);
    icons[0][1].extract(&sheet, 0x2e4, 0x212, 0x3c, 0x3c, 1, 1);
    icons[0][2].extract(&sheet, 0x30a, 0x64, 0x16, 0x28, 1, 1);
    icons[0][3].extract(&sheet, 0x30a, 0x17c, 0x16, 0x28, 1, 1);
    icons[0][4].extract(&sheet, 0x29e, 0x208, 0x46, 0x46, 1, 1);

    g_mouseButton = 0;

    for (;;) {
        g_pMenu->SetFontsForStyles(&g_kleptoBigFont, 0, 0, 0);
        sheet.copy(&g_simScreen, 0, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

        if (sel == -1) {
            icons[2][0].BlitScaled(g_pMenu, 0x258, 0x208, 0);
            icons[2][4].BlitScaled(g_pMenu, 0x29e, 0x208, 0);
        }

        DrawTextCenteredAtY("Pick A Pro", 0x1f8, 0x2a, 0x80000000);
        strcpy(g_printBuffer, "Themes\\Championship\\*.pro");
        count = FindFilesWithFilter(g_printBuffer, g_empty, g_empty);

        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);

        y = 0x74;
        maxTop = count - 16;
        if (firstRow > maxTop)
            firstRow = maxTop;
        if (firstRow < 0)
            firstRow = 0;

        for (i = firstRow; i < firstRow + 16 && i < count; i++) {
            strcpy(g_printBuffer, g_fileList[i]);
            if (sel == i)
                g_simScreen.fill(0x136, y - 1, 0x1c8, 0xf, 0x80001284);
            DrawTextWithColor(g_printBuffer, 0x140, y, sel == i ? 0x80007fff : 0x80000000);
            y += 0x10;
        }

        if (sel != -1) {
            SpriteObject *face = &g_playerHeadBmpArray[(HEAD_BANK(0) * 72 + g_golferTypeArray[0].head) * 3 + 1];
            face->Draw(g_pMenu, 0x45, 0x3d, 1, 1, 1, 0);

            strcpy(g_printBuffer, g_golferTypeArray[0].name);
            strcat(g_printBuffer, "'s skills");
            DrawTextWithColor(g_printBuffer, 0x38, 0xff, 0x80000000);

            strcpy(g_printBuffer, "'");
            g_golferPool[0].typeIdx = 0;
            BuildGolferDialogue(0x3e, 0, 0, 0);
            strcat(g_printBuffer, "'");
            DrawTextClampedToScreen(g_printBuffer, 0x24, 0x232, 0x80007fff);

            DrawTextWithColor("Signature saying:", 0x38, 0x220, 0x80000000);

            for (i = 0, y = 0x117; i < 10; i++, y += 0x18) {
                strcpy(g_printBuffer, g_skillNames[i]);
                DrawTextWithColor(g_printBuffer, 0x5e, y, 0x80000210);
                strcpy(g_printBuffer, "+");
                strcat(g_printBuffer, itoa(g_golferSkillGrades[i] * 10, g_numText, 10));
                strcat(g_printBuffer, "%");
                DrawTextCenteredAtY(g_printBuffer, 0x3b, y, 0x80000210);
            }
        }

        g_simScreen.RenderFullSurface(0);

        selShown = sel;
        hover = -1;
        frames = 0;

        for (;;) {
            WaitWithTimeout(2, 0);
            ProcessMouseMessages();
            g_simScreen.get_mouse_pos(&cx, &cy);

            prevHover = hover;
            hover = -1;
            if (ApproxDistance(cx - 0x279, cy - 0x22b) < 0x1a)
                hover = 0;
            if (ApproxDistance(cx - 0x301, cy - 0x22c) < 0x18)
                hover = 1;
            if (ApproxDistance(cx - 0x310, cy - 0x78) < 0x14)
                hover = 2;
            if (ApproxDistance(cx - 0x310, cy - 0x190) < 0x14)
                hover = 3;
            if (ApproxDistance(cx - 0x2c0, cy - 0x22b) < 0x17)
                hover = 4;

            if (hover != prevHover) {
                if (prevHover != -1)
                    icons[0][prevHover].BlitScaled(g_pMenu, ax[prevHover], ay[prevHover], 0);
                if (hover != -1)
                    icons[1][hover].BlitScaled(g_pMenu, ax[hover], ay[hover], 0);
                if (selShown == -1) {
                    icons[2][0].BlitScaled(g_pMenu, 0x258, 0x208, 0);
                    icons[2][4].BlitScaled(g_pMenu, 0x29e, 0x208, 0);
                }
                g_simScreen.RenderFullSurface(0);
                frames = 0;
            } else if (++frames > 0x1e && hover != -1) {
                g_printBuffer[0] = 0;
                switch (hover) {
                case 0: strcpy(g_printBuffer, selShown == -1 ? g_empty : "Load"); break;
                case 1: strcpy(g_printBuffer, "Cancel"); break;
                case 4: strcpy(g_printBuffer, "Delete"); break;
                }
                DrawTextCenteredWithShadow(g_printBuffer, ax[hover] + 0x1e, ay[hover] + 0x10, COLOR_WHITE);
                g_simScreen.RenderFullSurface(0);
                selShown = sel;
            }

            if (g_mouseButton != 0)
                break;
            if (PollInputState())
                break;
        }
        if (g_mouseButton == 0 && TakePendingChar() == 0x1b) {
            hover = 1;
        }
        if (hover != -1) {
            g_mouseButton = 0;
            if ((unsigned)hover > 4)
                continue;

            switch (hover) {
            case 2: firstRow = Clamp(firstRow - 4, 0, maxTop); continue;

            case 3: firstRow = Clamp(firstRow + 4, 0, maxTop); continue;

            case 4:
                strcpy(g_printBuffer, "Are you sure you want to delete\n");
                entry = g_fileList[sel];
                strcat(g_printBuffer, entry);
                strcat(g_printBuffer, "\n Yes, delete this file.\n No, never mind.\n");
                if (!ShowActionDialog(0x190, 0x64, 1, 1, 0)) {
                    strcpy(g_printBuffer, "Themes\\Championship\\");
                    strcat(g_printBuffer, entry);
                    remove(g_printBuffer);
                }
                continue;

            case 0:
                if (sel >= 0 && sel < count) {
                    g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
                    DrawTextWithColor("Loading...", 0x184, 0x1d8, COLOR_WHITE);
                    g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
                    g_simScreen.RenderFullSurface(0);
                    LoadGolferFromTheme(g_fileList[sel], 0, 1);
                    flush_input();
                    strcpy(g_activeThemeDir, savedPro);
                    return 1;
                }
                flush_input();
                strcpy(g_activeThemeDir, savedPro);
                return 0;

            case 1: flush_input(); return 0;
            }
        }
        if (g_clickX <= 0x140)
            continue;
        if (g_clickY < 0x74)
            continue;
        sel = firstRow + (g_clickY - 0x74) / 16;
        if (sel < count) {
            LoadGolferFromTheme(g_fileList[sel], 0, 1);
            continue;
        }
        sel = -1;
        icons[2][0].BlitScaled(g_pMenu, 0x258, 0x208, 0);
        icons[2][4].BlitScaled(g_pMenu, 0x29e, 0x208, 0);
    }
}

int ShowLoadGameScreen(int browsing, int courseMode)
{
    int top;
    int sel;
    int i;
    int count;
    int m[2];
    int maxTop;
    int saved[2];
    int bx[5];
    int by[5];
    char savedName[0x80];
    char autosaveLabel[0x3e8];
    int hot, prev, hold, sel2, key, n;
    char *entry;
    int y, h, off, j, v;
    int k;

    if (browsing == 0 && courseMode == 0)
        SaveGame("While_Browsing");

    saved[0] = g_currentThemeId;
    saved[1] = g_runModeFlags;
    strcpy(savedName, g_activeThemeDir);

    bx[0] = 0x258;
    bx[1] = 0x2e4;
    bx[2] = 0x30a;
    bx[3] = 0x30a;
    bx[4] = 0x29e;
    by[0] = 0x208;
    by[1] = 0x212;
    by[2] = 0x64;
    by[3] = 0x17c;
    by[4] = 0x208;

    SpriteObject btn[15];

    sel = -1;
    top = 0;

    AssetSurface sheet;

    sheet.load_pcx("interface\\Title_LoadGame_MO", 0, 0, PALETTE_COLORS, 2);
    btn[5].extract(&sheet, 0x258, 0x208, 0x46, 0x46, 1, 1);
    btn[6].extract(&sheet, 0x2e4, 0x212, 0x3c, 0x3c, 1, 1);
    btn[7].extract(&sheet, 0x30a, 0x64, 0x16, 0x28, 1, 1);
    btn[8].extract(&sheet, 0x30a, 0x17c, 0x16, 0x28, 1, 1);
    btn[9].extract(&sheet, 0x29e, 0x208, 0x46, 0x46, 1, 1);
    btn[10].extract(&sheet, 0x258, 0x1b3, 0x46, 0x46, 1, 1);
    btn[14].extract(&sheet, 0x29e, 0x1b3, 0x46, 0x46, 1, 1);

    sheet.load_pcx("interface\\Title_LoadGame", 0, 0, PALETTE_COLORS, 2);
    btn[0].extract(&sheet, 0x258, 0x208, 0x46, 0x46, 1, 1);
    btn[1].extract(&sheet, 0x2e4, 0x212, 0x3c, 0x3c, 1, 1);
    btn[2].extract(&sheet, 0x30a, 0x64, 0x16, 0x28, 1, 1);
    btn[3].extract(&sheet, 0x30a, 0x17c, 0x16, 0x28, 1, 1);
    btn[4].extract(&sheet, 0x29e, 0x208, 0x46, 0x46, 1, 1);

    g_mouseButton = 0;

    for (;;) {
        g_pMenu->SetFontsForStyles(&g_kleptoBigFont, 0, 0, 0);
        sheet.copy(&g_simScreen, 0, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

        if (sel == -1) {
            btn[10].BlitScaled(g_pMenu, 0x258, 0x208, 0);
            btn[14].BlitScaled(g_pMenu, 0x29e, 0x208, 0);
        }

        if (courseMode) {
            DrawTextCenteredAtY("Select Championship Course", 0x1f8, 0x2a, 0x80000000);
            strcpy(g_printBuffer, "Themes\\Championship\\*.cse");
            count = FindFilesWithFilter(g_printBuffer, g_empty, g_empty);
        } else {
            DrawTextCenteredAtY("Load Previous Game", 0x1f8, 0x2a, 0x80000000);
            strcpy(g_printBuffer, "Saved Games\\*.sve");
            if (browsing == 0)
                count = FindFilesWithFilter(g_printBuffer, g_empty, "While_Browsing");
            else
                count = FindFilesWithFilter(g_printBuffer, g_empty, "While_Browsing");
        }

        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);

        y = 0x74;
        maxTop = count - 0x10;
        if (top > maxTop)
            top = maxTop;
        if (top < 0)
            top = 0;

        if (count) {
            off = 228 * top / count;
            h = Clamp(228 * Clamp(count - top, 0, 0x10) / count, 0, 0xe4 - off);
            g_simScreen.fill(0x30d, off + 0x93, 6, h, COLOR_WHITE);
        }

        for (i = top; i < top + 0x10; i++) {
            if (i >= count)
                break;
            strcpy(g_printBuffer, g_fileList[i]);
            if (g_fileList[i][0] == '&') {
                strcpy(g_printBuffer, "autosave: (");
                strcpy(autosaveLabel, g_printBuffer);
                LoadSavedGame(g_fileList[i], 0, 1, 0);
                strcat(autosaveLabel, g_printBuffer);
                strcpy(g_printBuffer, autosaveLabel);
                strcat(g_printBuffer, ")");
            }
            if (sel == i)
                g_simScreen.fill(0x136, y - 1, 0x1c8, 0xf, 0x80007b20);
            DrawTextWithColor(g_printBuffer, 0x140, y, 0x80000000);
            y += 0x10;
        }

        if (count > top + 0x10)
            DrawTextWithColor("(more...)", 0x140, y, 0x80000000);

        if (sel != -1) {
            DrawTextCenteredAtY("Holes", 0x42, 0x7c, COLOR_GREY);
            DrawTextCenteredAtY("Par", 0x7d, 0x7c, COLOR_GREY);
            DrawTextCenteredAtY("Yards", 0xb8, 0x7c, COLOR_GREY);
            g_printBuffer[0] = 0;
            strcat(g_printBuffer, itoa(g_nextOpenHoleIndex - 1, g_numText, 10));
            DrawTextCenteredAtY(g_printBuffer, 0x42, 0x92, 0x80000000);
            g_printBuffer[0] = 0;
            strcat(g_printBuffer, itoa(g_gameCourseTotalPar, g_numText, 10));
            DrawTextCenteredAtY(g_printBuffer, 0x7d, 0x92, 0x80000000);
            g_printBuffer[0] = 0;
            strcat(g_printBuffer, itoa(g_simTotalLengthYards, g_numText, 10));
            DrawTextCenteredAtY(g_printBuffer, 0xb8, 0x92, 0x80000000);
            DrawTextCenteredAtY("Cash", 0x4b, 0xa9, COLOR_DARK_GREY);
            g_printBuffer[0] = 0;
            FormatNumberWithCommas(g_gameCashHundreds * 100);
            DrawTextCenteredAtY(g_printBuffer, 0xa1, 0xa9, 0x80000000);
            DrawTextWithColor("Fun Rating", 0x30, 0xc0, 0x80002108);
            DrawTextWithColor("Length Skill", 0x30, 0xd2, 0x80002108);
            DrawTextWithColor("Accuracy Skill", 0x30, 0xe4, 0x80002108);
            DrawTextWithColor("Imagination", 0x30, 0xf6, 0x80002108);
            g_printBuffer[0] = 0;
            strcat(g_printBuffer, itoa(g_courseTotalFunRate, g_numText, 10));
            DrawTextCenteredAtY(g_printBuffer, 0xb8, 0xc0, 0x80001284);
            g_printBuffer[0] = 0;
            AppendMoney(g_lengthAbilitySum);
            DrawTextCenteredAtY(g_printBuffer, 0xb8, 0xd2, 0x80000210);
            g_printBuffer[0] = 0;
            AppendMoney(g_gameCourseAccurPointsTotal);
            DrawTextCenteredAtY(g_printBuffer, 0xb8, 0xe4, 0x80000210);
            g_printBuffer[0] = 0;
            AppendMoney(g_totalImagination);
            DrawTextCenteredAtY(g_printBuffer, 0xb8, 0xf6, 0x80000210);
            DrawTextCenteredAtY("Hole", 0x42, 0x10e, COLOR_GREY);
            DrawTextCenteredAtY("Par", 0x7d, 0x10e, COLOR_GREY);
            DrawTextCenteredAtY("Yards", 0xb8, 0x10e, COLOR_GREY);

            for (k = 1; k < g_nextOpenHoleIndex; k++) {
                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(k, g_numText, 10));
                DrawTextCenteredAtY(g_printBuffer, 0x42, k * 0x11 + 0x10d, 0x80000000);
                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(g_allHolesArray[k].par, g_numText, 10));
                DrawTextCenteredAtY(g_printBuffer, 0x7d, k * 0x11 + 0x10d, 0x80000000);
                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(g_allHolesArray[k].yards, g_numText, 10));
                DrawTextCenteredAtY(g_printBuffer, 0xb8, k * 0x11 + 0x10d, 0x80000000);
            }

            g_printBuffer[0] = 0;
            AppendCourseName(0);
            strcat(g_printBuffer, "  (Theme: ");
            strcat(g_printBuffer, g_activeThemeDir);
            strcat(g_printBuffer, ")");
            DrawTextWithColor(g_printBuffer, 0x184, 0x1ba, COLOR_WHITE);
            strcpy(g_printBuffer, "Designed by ");
            strcat(g_printBuffer, g_golferTypeArray[0].name);
            DrawTextWithColor(g_printBuffer, 0x184, 0x1ca, COLOR_WHITE);

            if (g_bestTenScoresTable[0]) {
                strcpy(g_printBuffer, "Course Record: ");
                strcat(g_printBuffer, itoa(g_bestTenScoresTable[0], g_numText, 10));
                strcat(g_printBuffer, ", by ");
                AppendPooledString(0x14);
                DrawTextWithColor(g_printBuffer, 0x184, 0x1da, COLOR_WHITE);
            }

            v = g_mapCourseList[g_myWorldCourseIdx].site;
            j = g_tourSites[v].id;
            g_siteMapSprs[j].draw_antialiased(&g_regionAlphaFrames[j], g_pMenu, 0x54, 0x20, 0);
        }

        g_simScreen.RenderFullSurface(0);

        sel2 = sel;
        hot = -1;
        hold = 0;

        for (;;) {
            WaitWithTimeout(2, 0);
            ProcessMouseMessages();
            g_simScreen.get_mouse_pos(&m[1], &m[0]);
            prev = hot;
            hot = -1;
            if (ApproxDistance(m[1] - 0x279, m[0] - 0x22b) < 0x1a)
                hot = 0;
            if (ApproxDistance(m[1] - 0x301, m[0] - 0x22c) < 0x18)
                hot = 1;
            if (ApproxDistance(m[1] - 0x310, m[0] - 0x78) < 0x14)
                hot = 2;
            if (ApproxDistance(m[1] - 0x310, m[0] - 0x190) < 0x14)
                hot = 3;
            if (ApproxDistance(m[1] - 0x2c0, m[0] - 0x22b) < 0x17)
                hot = 4;

            if (hot != prev) {
                if (prev != -1)
                    btn[prev].BlitScaled(g_pMenu, bx[prev], by[prev], 0);
                if (hot != -1)
                    btn[hot + 5].BlitScaled(g_pMenu, bx[hot], by[hot], 0);
                if (sel2 == -1) {
                    btn[10].BlitScaled(g_pMenu, 0x258, 0x208, 0);
                    btn[14].BlitScaled(g_pMenu, 0x29e, 0x208, 0);
                }
                g_simScreen.RenderFullSurface(0);
                hold = 0;
            } else {
                hold++;
                if (hold > 0x1e && hot != -1) {
                    g_printBuffer[0] = 0;
                    switch (hot) {
                    case 0: strcpy(g_printBuffer, sel2 == -1 ? g_empty : "Load"); break;
                    case 1: strcpy(g_printBuffer, "Cancel"); break;
                    case 4: strcpy(g_printBuffer, "Delete"); break;
                    }
                    if (hold == 0x1f)
                        TilePatternToArea(bx[hot] + 6, by[hot] + 0xe, 0x30, 0x10, 0);
                    DrawTextCenteredWithShadow(g_printBuffer, bx[hot] + 0x1e, by[hot] + 0x10, COLOR_WHITE);
                    g_simScreen.RenderFullSurface(0);
                    sel2 = sel;
                }
            }

            if (g_mouseButton != 0)
                break;
            if (PollInputState() != 0)
                break;
        }
        int toList = 0;
        if (g_mouseButton == 0) {
            key = TakePendingChar();
            if (key != 0x1b)
                toList = 1;
            else
                hot = 1;
        }
        if (!toList && hot != -1) {
            g_mouseButton = 0;
            if ((unsigned)hot > 4)
                continue;
            switch (hot) {
            case 0:
                n = sel;
                g_currentThemeId = (signed char)saved[0];
                g_runModeFlags = saved[1];
                if (n >= 0 && n < count) {
                    g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
                    DrawTextWithColor("Loading...", 0x226, 0x1c8, 0x80007b20);
                    g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
                    g_simScreen.RenderFullSurface(0);
                    if (courseMode) {
                        strcpy(g_printBuffer, "Themes\\Championship\\");
                        strcat(g_printBuffer, g_fileList[n]);
                        LoadCourseFromFile(0);
                    } else {
                        LoadSavedGame(g_fileList[n], browsing == 0, 0, 0);
                    }
                    if (strcmp(g_activeThemeDir, savedName))
                        LoadThemeFiles(1);
                    flush_input();
                    return 1;
                }
                if (browsing == 0 && courseMode == 0)
                    LoadSavedGame("While_Browsing.sve", 1, 0, 0);
                flush_input();
                return 0;
            case 1:
                g_currentThemeId = (signed char)saved[0];
                g_runModeFlags = saved[1];
                if (browsing == 0 && courseMode == 0)
                    LoadSavedGame("While_Browsing.sve", 1, 0, 0);
                flush_input();
                return 0;
            case 2: top = Clamp(top - 4, 0, maxTop); continue;
            case 3: top = Clamp(top + 4, 0, maxTop); continue;
            case 4:
                strcpy(g_printBuffer, "Are you sure you want to delete\n");
                if (courseMode)
                    strcat(g_printBuffer, "Themes\\Championship\\");
                entry = g_fileList[sel];
                strcat(g_printBuffer, entry);
                strcat(g_printBuffer, "\n Yes, delete this file.\n No, never mind.\n");
                if (ShowActionDialog(0x190, 0x64, 1, 1, 0))
                    continue;
                if (courseMode)
                    strcpy(g_printBuffer, "Themes\\Championship\\");
                else
                    strcpy(g_printBuffer, "Saved Games\\");
                strcat(g_printBuffer, entry);
                remove(g_printBuffer);
                continue;
            }
        }
        if (g_clickX <= 0x140)
            continue;
        if (g_clickY < 0x74)
            continue;
        n = top + (g_clickY - 0x74) / 0x10;
        sel = n;
        if (n >= top + 0x10) {
            top = Clamp(top + 4, 0, maxTop);
            continue;
        }
        if (n < count) {
            if (courseMode) {
                strcpy(g_printBuffer, "Themes\\Championship\\");
                strcat(g_printBuffer, g_fileList[n]);
                LoadCourseFromFile(1);
            } else {
                LoadSavedGame(g_fileList[n], 0, 0, 0);
            }
        } else {
            sel = -1;
            btn[10].BlitScaled(g_pMenu, 0x258, 0x208, 0);
            btn[14].BlitScaled(g_pMenu, 0x29e, 0x208, 0);
        }
    }
}
