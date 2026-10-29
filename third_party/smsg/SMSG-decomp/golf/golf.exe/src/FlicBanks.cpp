#include "jgldefs.h"
#include "FlicBanks.h"
#include "AssetSurface.h"
#include "BuildingSearch.h"
#include "CourseGrid.h"
#include "Employee.h"
#include "FLCPalette.h"
#include "FloatingText.h"
#include "GameSetup.h"
#include "Geometry.h"
#include "Golfer.h"
#include "ImageSet.h"
#include "InputPoll.h"
#include "IntroVideo.h"
#include "LoadingScreen.h"
#include "MapTile.h"
#include "Plot.h"
#include "RandomSource.h"
#include "SaveGame.h"
#include "Scorecard.h"
#include "Screen.h"
#include "SetupScreens.h"
#include "SoundDll.h"
#include "SoundEffects.h"
#include "Sprite.h"
#include "SpriteAssets.h"
#include "StringPool.h"
#include "Surface.h"
#include "TerrainGrid.h"
#include "TerrainInfluence.h"
#include "ToolbarSelect.h"
#include "Wildlife.h"
#include <windows.h>
#include <string.h>

FLCPalette g_palettes[190];
FLCPalette g_introPalette;
BinkPlayer g_binkPlayer;

void PlayBinkVideoWithTimeout(const char *file, int timeoutMS)
{
    if (!FileExists(file))
        return;
    if (timeoutMS == 0)
        g_skipWaitUntil = timeGetTime() + 7000;
    else
        g_skipWaitUntil = 0;
    g_binkPlayer.SetWindow(graphsys->get_window_handle());
    do_all_tasks();
    g_simScreen.EraseWholeBackground(0x80007fff);
    g_binkPlayer.Play(file, 0, 0, (unsigned long)wave_device.get_ds());
    g_binkPlayer.Close();
    g_skipWaitUntil = 0;
}

void PlayIntroVideo()
{
    int theme;

    PlayBinkVideoWithTimeout("./flics/SMSG_introfinal.bik", 0);
    AssetSurface sheet;
    sheet.Create(0x400, 0x320, 0, 1, 0, 0);

    sheet.load_pcx("interface\\Pop_upOK.pcx", 0, 0, PALETTE_COLORS, 2);
    for (theme = 0; theme < 4; theme++) {
        int y = 0;
        switch (theme) {
        case THEME_DESERT: y = 200; break;
        case THEME_TROPICAL: y = 100; break;
        case THEME_PARK:
        case THEME_LINKS: y = 0; break;
        }
        g_themeFrame[theme][1].extract(&sheet, 0, y, 16, 16, 1, 0);
        g_themeFrame[theme][2].extract(&sheet, 0, y + 17, 16, 16, 1, 0);
        g_themeFrame[theme][3].extract(&sheet, 0, y + 34, 16, 26, 1, 0);
        g_themeFrame[theme][4].extract(&sheet, 17, y, 16, 16, 1, 0);
        g_themeFrame[theme][5].extract(&sheet, 34, y, 26, 16, 1, 0);
        g_themeFrame[theme][6].extract(&sheet, 34, y + 17, 26, 16, 1, 0);
        g_themeFrame[theme][7].extract(&sheet, 34, y + 34, 26, 26, 1, 0);
        g_themeFrame[theme][0].extract(&sheet, 17, y + 34, 16, 26, 1, 0);
    }
    sheet.load_pcx("interface\\Pop_upOK_A.pcx", 0, 0, PALETTE_COLORS, 2);
    for (theme = 0; theme < 4; theme++) {
        int y = 0;
        switch (theme) {
        case THEME_DESERT: y = 200; break;
        case THEME_TROPICAL: y = 100; break;
        case THEME_PARK:
        case THEME_LINKS: y = 0; break;
        }
        g_themeAlphaFrames[theme][1].extract(&sheet, 0, y, 16, 16, 1, 0);
        g_themeAlphaFrames[theme][2].extract(&sheet, 0, y + 17, 16, 16, 1, 0);
        g_themeAlphaFrames[theme][3].extract(&sheet, 0, y + 34, 16, 26, 1, 0);
        g_themeAlphaFrames[theme][4].extract(&sheet, 17, y, 16, 16, 1, 0);
        g_themeAlphaFrames[theme][5].extract(&sheet, 34, y, 26, 16, 1, 0);
        g_themeAlphaFrames[theme][6].extract(&sheet, 34, y + 17, 26, 16, 1, 0);
        g_themeAlphaFrames[theme][7].extract(&sheet, 34, y + 34, 26, 26, 1, 0);
        g_themeAlphaFrames[theme][0].extract(&sheet, 17, y + 34, 16, 26, 1, 0);
    }

    sheet.load_pcx("interface\\TitleUnSel", 0, 0, PALETTE_COLORS, 2);
    g_simTitlePageGraphics[0].extract(&sheet, 42, 21, 324, 121, 1, 1);
    g_simTitlePageGraphics[2].extract(&sheet, 416, 31, 363, 141, 1, 1);
    g_simTitlePageGraphics[4].extract(&sheet, 510, 359, 271, 111, 1, 1);
    g_simTitlePageGraphics[6].extract(&sheet, 287, 474, 288, 115, 1, 1);
    g_simTitlePageGraphics[8].extract(&sheet, 13, 382, 287, 115, 1, 1);
    g_simTitlePageGraphics[12].extract(&sheet, 168, 198, 472, 176, 1, 1);
    g_simTitlePageGraphics[10].extract(&sheet, 721, 528, 58, 57, 1, 1);
    sheet.load_pcx("interface\\TitleMO", 0, 0, PALETTE_COLORS, 2);
    g_simTitlePageGraphics[1].extract(&sheet, 42, 21, 324, 121, 1, 1);
    g_simTitlePageGraphics[3].extract(&sheet, 416, 31, 363, 141, 1, 1);
    g_simTitlePageGraphics[5].extract(&sheet, 510, 359, 271, 111, 1, 1);
    g_simTitlePageGraphics[7].extract(&sheet, 287, 474, 288, 115, 1, 1);
    g_simTitlePageGraphics[9].extract(&sheet, 13, 382, 287, 115, 1, 1);
    g_simTitlePageGraphics[TITLE_LOGO_LIT_SPRITE].extract(&sheet, 168, 198, 472, 176, 1, 1);
    g_simTitlePageGraphics[11].extract(&sheet, 721, 528, 58, 57, 1, 1);

    {
        int i, n = 0;
        for (i = 0; i < 20; i++)
            if (g_facilityTypes[i].cost)
                n++;
    }
}

int FindFilesWithFilter(const char *pattern, const char *mustHave, const char *exclude)
{
    WIN32_FIND_DATAA fd;
    char name[100];
    HANDLE hFind;
    int count;

    memset(g_fileList, 0, sizeof(g_fileList));
    count = 0;
    hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return 0;
    do {
        int keep = 1;
        strcpy(name, fd.cFileName);
        if (*mustHave) {
            if (!strstr(name, mustHave))
                keep = 0;
        }
        if (*exclude) {
            if (strstr(name, exclude))
                keep = 0;
        }
        if (strstr(name, "Shadow"))
            keep = 0;
        if (strstr(name, "shadow"))
            keep = 0;
        if (keep) {
            strcat(g_printBuffer, name);
            strcat(g_printBuffer, " \n");
            strcpy(g_fileList[count++], name);
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
    return count;
}


SpriteObject g_flicSprites[0xdd40], g_flicReserve;
SpriteObject g_backupSprite;

int g_memBases[100];
int g_memSizes[100];
int g_poolReservedWords[3];

void MemPool_Free(int p, int sz)
{
    int i, j;

    for (i = 0; i < 100; i++) {
        if (g_memBases[i] >= 0 && p == g_memBases[i] + g_memSizes[i]) {
            g_memSizes[i] += sz;
            for (j = 0; j < 100; j++) {
                if (g_memBases[j] == g_memBases[i] + g_memSizes[i]) {
                    g_memBases[j] = -1;
                    g_memSizes[i] += g_memSizes[j];
                    g_memSizes[j] = 0;
                }
            }
            return;
        }
    }
    for (i = 0; 100 > i; i++) {
        if (g_memBases[i] < 0) {
            g_memBases[i] = p;
            g_memSizes[i] = sz;
            return;
        }
    }
}

int MemPool_Alloc(int size)
{
    for (int i = 0; i < 100; i++) {
        if (g_memBases[i] >= 0 && g_memSizes[i] >= size) {
            int base = g_memBases[i];
            if (g_memSizes[i] > size) {
                g_memBases[i] = base + size;
                g_memSizes[i] -= size;
            } else {
                g_memSizes[i] = 0;
                g_memBases[i] = -1;
            }
            return base;
        }
    }
    return -1;
}

AssetSurface g_flicSurface;

void FreeSpritePoolByIndex(int bank)
{
    int i;

    if (g_flicStartFrame[bank] == -1)
        return;
    MemPool_Free(g_flicStartFrame[bank], g_flicBankFrameLens[bank] * g_gameAnimNumAngles[bank]);
    for (i = 0; i < g_flicBankFrameLens[bank] * g_gameAnimNumAngles[bank]; i++)
        g_flicSprites[g_flicStartFrame[bank] + i].close();
    g_flicBankFrameLens[bank] = 0;
    g_flicStartFrame[bank] = -1;
}

SpriteObject *GetFlicBankSprite(int bank, int frame, int facing)
{
    if (g_flicStartFrame[bank] == -1)
        return g_homePlotGroundPics;
    if (frame >= g_flicBankFrameLens[bank])
        return g_homePlotGroundPics;
    return &g_flicSprites[g_flicBankFrameLens[bank] * facing + g_flicStartFrame[bank] + frame];
}

void LoadFLCAnimation(int slot, const char *name, int facings, int mode, int wantShadow)
{
    int saved;
    int i, j;
    int first;

    g_flicNumLoads++;
    if (g_flicNumLoads % 100 == 0) {
        if (g_level == -1) {
            ShowLoadingScreen(g_flicNumLoads / 4);
            g_simScreen.RenderFullSurface(0);
        }
        WaitWithTimeout(10, 0);
    }
    do_all_tasks();

    if (mode == -1)
        facings = 1;
    g_flicSurface.fill(0, 0, 480, 480, 255);
    slot += g_bankSlotOffset;
    strcpy(g_flcFileNames[slot], name);

    if (wantShadow) {
        strcpy(g_printBuffer, "flics\\");
        strcat(g_printBuffer, name);
        strcat(g_printBuffer, "Shadow.flc");
        if (FileExists(g_printBuffer)) {
            g_currentFlic = FLC_LoadAndGetSurface(g_printBuffer, 590, &g_flicBankFrameLens[slot]);
            g_currentFlic->BlitScaled(&g_flicSurface, 0, 0, 0);
        } else {
            wantShadow = 0;
        }
    }
    strcpy(g_printBuffer, "flics\\");
    strcat(g_printBuffer, name);
    strcat(g_printBuffer, ".flc");
    g_currentFlic = FLC_LoadAndGetSurface(g_printBuffer, slot, &g_flicBankFrameLens[slot]);
    g_currentFlic->BlitScaled(&g_flicSurface, 0, 0, 0);

    g_gameAnimNumAngles[slot] = facings;
    saved = g_flicNextFree;
    if (g_memBases[0] != -2)
        g_flicNextFree = MemPool_Alloc(facings * g_flicBankFrameLens[slot]);
    else
        saved = -1;
    if (g_flicNextFree == -1) {
        g_flicNextFree = saved;
        saved = -1;
    }
    if (g_flicBankFrameLens[slot] * facings + g_flicNextFree >= 0xdd40) {
        ShowListDialogWithParams("Too many flics!\n", 100, 100);
    } else {
        first = 1;
        g_flicStartFrame[slot] = g_flicNextFree;
        for (i = 0; i < facings; i++) {
            for (j = 0; j < g_flicBankFrameLens[slot]; j++) {
                if (!first) {
                    if (mode)
                        g_flicSurface.fill(0, 0, 480, 480, 255);
                    else
                        g_flicSurface.fill(210, 200, 60, 60, 255);
                    if (wantShadow) {
                        g_currentFlic = FLC_GetNextFrameSurface(590);
                        g_currentFlic->BlitScaled(&g_flicSurface, 0, 0, 0);
                    }
                    g_currentFlic = FLC_GetNextFrameSurface(slot);
                    g_currentFlic->BlitScaled(&g_flicSurface, 0, 0, 0);
                }
                if (mode == 0)
                    g_flicSprites[g_flicNextFree++].extract(&g_flicSurface, 210, 200, 60, 60, 1, 1);
                else if (mode == 1 || mode == -1)
                    g_flicSprites[g_flicNextFree++].extract(&g_flicSurface, 100, 100, 280, 240, 1, 1);
                else if (mode != 240)
                    g_flicSprites[g_flicNextFree++].extract(&g_flicSurface, 240 - mode, 240 - mode, mode * 2,
                        mode * 3 / 2, 1, 1);
                else
                    g_flicSprites[g_flicNextFree++].extract(&g_flicSurface, 0, 0, 480, 480, 1, 1);
                first = 0;
                if (mode == -1)
                    break;
            }
            if (mode == -1)
                break;
        }
    }
    FLC_ResetAndCleanup(slot);
    FLC_ResetAndCleanup(590);
    if (saved != -1)
        g_flicNextFree = saved;
}

void LoadThemeAssets()
{
    AssetSurface ld;
    int i;
    int t;
    ld.Create(0x400, 0x320, 0, 1, 0, 0);
    t = g_currentThemeId;
    switch (t) {
    case THEME_DESERT: {
        ld.load_pcx("flics\\bldgs\\desert\\dessignPal", &g_palettes[15], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\desert\\des_drpal", &g_palettes[26], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Desert\\DESSnackPal", &g_palettes[16], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Desert\\DESCartPal", &g_palettes[23], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Desert\\DESHotelPal", &g_palettes[17], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Desert\\DEScasPal", &g_palettes[18], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Desert\\heliPal", &g_palettes[27], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Desert\\dproPal", &g_palettes[14], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Desert\\DESclubPal", &g_palettes[25], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Desert\\spaPal", &g_palettes[28], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\desert\\dputtPal", &g_palettes[20], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\scenic\\Desert_watersprayPal", &g_palettes[29], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\scenic\\SailboatDesert_Palette", &g_palettes[83], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\scenic\\DolphinDesert_Palette", &g_palettes[90], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\desert\\DESSnackL2Pal", &g_palettes[151], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\flowers\\DesertFlowersIckyPal", &g_palettes[187], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\desert\\DesProL2pal", &g_palettes[152], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\desert\\DESputt_platL2pal", &g_palettes[154], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\desert\\DesCartL2pal", &g_palettes[155], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\desert\\SpaL2pal", &g_palettes[153], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\desert\\HelL2pal", &g_palettes[156], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\desert\\DesHoL2pal", &g_palettes[157], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\desert\\DesCasL2pal", &g_palettes[158], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\desert\\DesClubL2pal", &g_palettes[159], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\flowers\\OilSlickPal", &g_palettes[77], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\tees\\flag_desertpal", &g_palettes[99], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\homes\\regular\\desert\\A\\DesHouseA_palette", &g_palettes[96], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\desert\\dhouseconpal", &g_palettes[19], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\desert\\house_Apal", &g_palettes[93], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\water\\DesWater Palette", &g_palettes[178], 0, PALETTE_COLORS, 0);
        break;
    }
    case THEME_TROPICAL: {
        ld.load_pcx("flics\\bldgs\\Tropical\\TropHouseSign_palette", &g_palettes[15], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Tropical\\TropDrivingR1", &g_palettes[26], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Tropical\\TropSnackL1 palette", &g_palettes[16], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Tropical\\TropCartL1Pal", &g_palettes[23], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Tropical\\TropHotelL1Pal", &g_palettes[17], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Tropical\\TROPthemeL1_palette", &g_palettes[18], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Tropical\\TropMarL1_palette", &g_palettes[27], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Tropical\\TropProShopL1_palette", &g_palettes[14], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Tropical\\TropclubL1Pal", &g_palettes[25], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Tropical\\TropSwimL1Pal", &g_palettes[28], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Tropical\\TropPuttingG1", &g_palettes[20], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\scenic\\Trop_watersprayPal", &g_palettes[29], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\scenic\\sailboatTropical_Palette", &g_palettes[83], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\scenic\\DolphinTropical_Palette", &g_palettes[90], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\tropical\\SnackL2_palette", &g_palettes[151], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\flowers\\TropicalFlowers_IckyPal", &g_palettes[187], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\tropical\\TropProShopL2_palette", &g_palettes[152], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Tropical\\TropSwimL2_palette", &g_palettes[153], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\tropical\\TropDRL2_palette", &g_palettes[154], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\tropical\\TropCartL2_palette", &g_palettes[155], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Tropical\\TropMarL2_palette", &g_palettes[156], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Tropical\\TropHotelL2_palette", &g_palettes[157], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Tropical\\TROPthemeL2_palette", &g_palettes[158], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\Tropical\\TropClubL2_Palette", &g_palettes[159], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\flowers\\DryGrassPal", &g_palettes[77], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\tees\\flag_troppal", &g_palettes[99], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\tropical\\TropHouseCon_palette", &g_palettes[19], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\homes\\regular\\tropical\\B\\TropHouseB_palette", &g_palettes[96], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\water\\TropWater Palette", &g_palettes[178], 0, PALETTE_COLORS, 0);
        break;
    }
    case THEME_LINKS: {
        ld.load_pcx("flics\\bldgs\\links\\sign_links_Palette", &g_palettes[15], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\driving_rangeL1_palette", &g_palettes[26], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\PubPal", &g_palettes[16], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\HotelPal", &g_palettes[17], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\CastlePal", &g_palettes[18], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\ClubL1_Palette", &g_palettes[25], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\ChurchPal", &g_palettes[27], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\Pro_shopPal", &g_palettes[14], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\Cart_GaragePal", &g_palettes[23], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\StableL1_palette", &g_palettes[28], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\PG_L1 and L2 Palette", &g_palettes[20], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\scenic\\Links_watersprayPal", &g_palettes[29], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\scenic\\SailboatLinks_Palette", &g_palettes[83], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\scenic\\DolphinLinks_Palette", &g_palettes[90], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\PubL2_Palette", &g_palettes[151], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\flowers\\FlowerbedA_IckyPal", &g_palettes[187], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\DRL2_palette", &g_palettes[154], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\ProShopL2_palette", &g_palettes[152], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\Cart_GarageL2_palette", &g_palettes[155], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\StableL2_palette", &g_palettes[153], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\ChurchL2_palette", &g_palettes[156], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\HotelL2_palette", &g_palettes[157], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\CastleL2_palette", &g_palettes[158], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\ClubL2_Palette", &g_palettes[159], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\flowers\\dandelionPal", &g_palettes[77], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\tees\\flag_linkspal", &g_palettes[99], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\homes\\regular\\links\\B\\LinksHouseB_palette", &g_palettes[96], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\housecon_palette", &g_palettes[19], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\links\\house_A_links_palette", &g_palettes[93], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\water\\LinksWater Palette", &g_palettes[178], 0, PALETTE_COLORS, 0);
        break;
    }
    default: {
        ld.load_pcx("flics\\bldgs\\park\\signPal", &g_palettes[15], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\DRL1 palette", &g_palettes[26], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\ParkSnackL1 palette", &g_palettes[16], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\HotelL1 palette", &g_palettes[17], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\AirL1 Palette", &g_palettes[18], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\ClubL1 palette", &g_palettes[25], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\MarL1 Palette", &g_palettes[27], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\ParkProL1 Palette", &g_palettes[14], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\CartL1 Palette", &g_palettes[23], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\TenL1 Palette", &g_palettes[28], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\PuttL1 Palette", &g_palettes[20], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\scenic\\watersprayPal", &g_palettes[29], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\scenic\\Sail_MovePal", &g_palettes[83], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\scenic\\DolphinPal", &g_palettes[90], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\ParkSnackL2 Palette", &g_palettes[151], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\flowers\\FlowerbedA_IckyPal", &g_palettes[187], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\ParkProL2 palette", &g_palettes[152], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\DRL2 palette", &g_palettes[154], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\CartL2 palette", &g_palettes[155], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\TenL2 palette", &g_palettes[153], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\MarL2 palette", &g_palettes[156], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\HotelL2 Palette", &g_palettes[157], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\AirL2 Palette", &g_palettes[158], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\clubL2 Palette", &g_palettes[159], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\flowers\\dandelionPal", &g_palettes[77], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\tees\\flag_parkpal", &g_palettes[99], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\homes\\regular\\parkland\\A\\ParkHouseA_palette", &g_palettes[96], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\houseconpal", &g_palettes[19], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\bldgs\\park\\houseBpal", &g_palettes[93], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\water\\ParkWater Palette", &g_palettes[178], 0, PALETTE_COLORS, 0);
        break;
    }
    }
    g_unusedFlag = 0;
    switch (g_currentThemeId) {
    case THEME_PARK: ld.load_pcx("interface\\ParklandTerrainButtons", 0, 0, PALETTE_COLORS, 0x2); break;
    case THEME_LINKS: ld.load_pcx("interface\\LinksTerrainButtons", 0, 0, PALETTE_COLORS, 0x2); break;
    case THEME_TROPICAL: ld.load_pcx("interface\\TropicalTerrainButtons", 0, 0, PALETTE_COLORS, 0x2); break;
    case THEME_DESERT: ld.load_pcx("interface\\DesertTerrainButtons", 0, 0, PALETTE_COLORS, 0x2); break;
    }
    for (i = 0; i < 13; i++) {
        int y = (i % 7) * 0x36;
        int x = (i < 7) ? 0 : 0xf8;
        int k;
        for (k = 0; k < 4; k++, x += 0x3e)
            g_themeTerrainButtonBmps[i * 4 + k].extract(&ld, x, y, 0x3e, 0x36, 1, 1);
    }
    switch (g_currentThemeId) {
    case THEME_PARK: {
        int k;
        int x;
        for (x = 0x208, k = 0; x < 0x2f8; x += 0x50, ++k) {
            g_themeTerrainButtonBmps[52 + k].extract(&ld, x, 0, 0x41, 0x54, 1, 1);
            g_themeTerrainButtonBmps[56 + k].extract(&ld, x, 0x96, 0x3e, 0x46, 1, 1);
            g_themeTerrainButtonBmps[60 + k].extract(&ld, x, 0x12c, 0x3e, 0x58, 1, 1);
        }
        break;
    }
    case THEME_LINKS: {
        int k;
        int x;
        for (x = 0x208, k = 0; x < 0x2f8; x += 0x50, ++k) {
            g_themeTerrainButtonBmps[52 + k].extract(&ld, x, 0, 0x43, 0x54, 1, 1);
            g_themeTerrainButtonBmps[56 + k].extract(&ld, x, 0x96, 0x3e, 0x58, 1, 1);
            g_themeTerrainButtonBmps[60 + k].extract(&ld, x, 0x12c, 0x43, 0x4c, 1, 1);
        }
        break;
    }
    case THEME_TROPICAL: {
        int k;
        int x;
        for (x = 0x208, k = 0; x < 0x2f8; x += 0x50, ++k) {
            g_themeTerrainButtonBmps[52 + k].extract(&ld, x, 0, 0x48, 0x4f, 1, 1);
            g_themeTerrainButtonBmps[56 + k].extract(&ld, x, 0x96, 0x3e, 0x51, 1, 1);
            g_themeTerrainButtonBmps[60 + k].extract(&ld, x, 0x12c, 0x41, 0x68, 1, 1);
        }
        break;
    }
    case THEME_DESERT: {
        int k;
        int x;
        for (x = 0x208, k = 0; x < 0x2f8; x += 0x50, ++k) {
            g_themeTerrainButtonBmps[52 + k].extract(&ld, x, 0, 0x4b, 0x4a, 1, 1);
            g_themeTerrainButtonBmps[56 + k].extract(&ld, x, 0x96, 0x3e, 0x43, 1, 1);
            g_themeTerrainButtonBmps[60 + k].extract(&ld, x, 0x12c, 0x41, 0x68, 1, 1);
        }
        break;
    }
    }
    switch (g_currentThemeId) {
    case THEME_PARK: ld.load_pcx("interface\\Parklands Layout", 0, 0x0, PALETTE_COLORS, 0x2); break;
    case THEME_LINKS: ld.load_pcx("interface\\Links Layout", 0, 0x0, PALETTE_COLORS, 0x2); break;
    case THEME_TROPICAL: ld.load_pcx("interface\\Trop Layout", 0, 0x0, PALETTE_COLORS, 0x2); break;
    case THEME_DESERT: ld.load_pcx("interface\\Desert Layout", 0, 0x0, PALETTE_COLORS, 0x2); break;
    }
    for (i = 0; i < 10; i++) {
        int x = i * 0x4b;
        g_bldgSprites[i][0].extract(&ld, x, 0, 0x4b, 0x64, 1, 1);
        g_bldgSprites[i][2].extract(&ld, x, 0x64, 0x4b, 0x64, 1, 1);
        g_bldgSprites[i][1].extract(&ld, x, 0xc8, 0x4b, 0x64, 1, 1);
        g_bldgSprites[i][3].extract(&ld, x, 0x12c, 0x4b, 0x64, 1, 1);
    }
    g_bankSlotOffset = 0;
    for (i = 0x1b6; i <= 0x207; i++) {
        if (g_flicStartFrame[i] != -1)
            FreeSpritePoolByIndex(i);
        g_flicStartFrame[i] = -1;
        g_flcFileNames[i][0] = 0;
    }
    for (i = 0x22e; i <= 0x23c; i++) {
        if (g_flicStartFrame[i] != -1)
            FreeSpritePoolByIndex(i);
        g_flicStartFrame[i] = -1;
        g_flcFileNames[i][0] = 0;
    }
    if (g_currentThemeId == THEME_TROPICAL) {
        LoadFLCAnimation(0x1c8, "bldgs\\tropical\\tropsign", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c9, "bldgs\\tropical\\TropHouseconst", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1ca, "Homes\\Regular\\Tropical\\B\\TropHouseB", 0x4, 0x8c, 0x1);
        LoadFLCAnimation(0x1ec, "bldgs\\Tropical\\TropSnackL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1ee, "bldgs\\Tropical\\TropSnackL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1ef, "bldgs\\Tropical\\TropSnackL2_ANIM", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1b6, "bldgs\\tropical\\TropDrivingRShopL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1b7, "bldgs\\tropical\\TropDrivingRShopL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1b8, "bldgs\\tropical\\TropDrivingRL1PlatTop", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1b9, "bldgs\\tropical\\TropDrivingRL1PlatBase", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1ba, "bldgs\\tropical\\TropDrivingR100L1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1bb, "bldgs\\tropical\\TropDrivingR200L1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1bc, "bldgs\\tropical\\TropDrivingR300L1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1bd, "bldgs\\tropical\\TropDrivingRflagL1", 0x1, 0x28, 0x1);
        LoadFLCAnimation(0x1be, "bldgs\\tropical\\TropDrivingRfenceL1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1bf, "bldgs\\tropical\\TROPDrivingRshopL2_ANIM", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c0, "bldgs\\tropical\\TropDrivingRL2PlatTop", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1c1, "bldgs\\tropical\\TropDrivingRL2PlatBase", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f0, "bldgs\\tropical\\TropPuttingGL1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f1, "bldgs\\tropical\\TropPuttingGL1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f2, "bldgs\\tropical\\TropPuttingGFlagL1", 0x1, 0x28, 0x1);
        LoadFLCAnimation(0x1f3, "bldgs\\tropical\\TropPuttingGL2", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f4, "bldgs\\tropical\\TropPuttingGL2_Anim", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f5, "bldgs\\tropical\\TropPuttingGL2_Scenic", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1c2, "bldgs\\tropical\\TropCartL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c5, "bldgs\\tropical\\TropCartL1Base", 0x4, 0x1, 0);
        LoadFLCAnimation(0x1c3, "bldgs\\tropical\\TropCartL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c6, "bldgs\\tropical\\TropCartL2_Base", 0x4, 0x1, 0);
        LoadFLCAnimation(0x1f6, "bldgs\\tropical\\TropclubL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1fa, "bldgs\\tropical\\TropclubL1anim", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1f7, "bldgs\\tropical\\TropclubL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1f8, "bldgs\\tropical\\TropclubL2_base", 0x4, 0x1, 0);
        LoadFLCAnimation(0x1fb, "bldgs\\tropical\\TropclubL2_ANIM", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1ce, "bldgs\\tropical\\TropMarL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d1, "bldgs\\tropical\\TropMarL1_underwater", 0x4, 0x1, 0);
        LoadFLCAnimation(0x1cf, "bldgs\\tropical\\TropMarinaL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d2, "bldgs\\tropical\\TropMarinaL2_underwater", 0x4, 0x1, 0);
        LoadFLCAnimation(0x1e0, "bldgs\\tropical\\TropProshopL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1e3, "bldgs\\tropical\\TropProshopL1_base", 0x4, 0x1, 0);
        LoadFLCAnimation(0x1e4, "bldgs\\tropical\\TropProshopL1_anim", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1e1, "bldgs\\tropical\\TropProshopL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d4, "bldgs\\tropical\\TropSwimL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d7, "bldgs\\tropical\\TropSwimL1anim", 0x4, 0x1, 0);
        LoadFLCAnimation(0x1d5, "bldgs\\tropical\\TropSwimL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d8, "bldgs\\tropical\\TropSwimL2_anim", 0x4, 0x1, 0);
        LoadFLCAnimation(0x1da, "bldgs\\tropical\\TROPthemeL1", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1dc, "bldgs\\tropical\\TROPthemeL1_BoatAnim", 0x4, 0xf0, 0);
        LoadFLCAnimation(0x1de, "bldgs\\tropical\\TROPthemeL1_CannonAnim", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1db, "bldgs\\tropical\\TROPthemeL2", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1dd, "bldgs\\tropical\\TROPthemeL2_LoopAnim", 0x4, 0xf0, 0);
        LoadFLCAnimation(0x1df, "bldgs\\tropical\\TROPthemeL2_SprayAnim", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1e6, "bldgs\\Tropical\\TropHotelL1", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1e9, "bldgs\\Tropical\\TropHotelL1base", 0x4, 0xf0, 0);
        LoadFLCAnimation(0x1e7, "bldgs\\Tropical\\TropHotelL2", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1ea, "bldgs\\Tropical\\TropHotelL2_base", 0x4, 0xf0, 0);
        LoadFLCAnimation(0x1eb, "bldgs\\Tropical\\TropHotelL2_ANIM", 0x4, 0xf0, 0x1);
    }
    if (g_currentThemeId == THEME_DESERT) {
        LoadFLCAnimation(0x1c8, "bldgs\\desert\\DESsign", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c9, "bldgs\\desert\\DESHouseconst", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1ca, "Homes\\Regular\\Desert\\A\\DesHouseA", 0x4, 0x8c, 0x1);
        LoadFLCAnimation(0x1ec, "bldgs\\Desert\\DESSnack", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1ed, "bldgs\\Desert\\DESSnackBase", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1ee, "bldgs\\Desert\\DESSnackL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1b6, "bldgs\\desert\\dbuildDRL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1b7, "bldgs\\desert\\desDRL2house", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1b8, "bldgs\\desert\\dplatDRL1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1b9, "bldgs\\desert\\dplatDRL1base", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1ba, "bldgs\\desert\\d100DRL1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1bb, "bldgs\\desert\\d200DRL1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1bc, "bldgs\\desert\\d300DRL1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1bd, "bldgs\\desert\\dflagDRL1", 0x1, 0x28, 0x1);
        LoadFLCAnimation(0x1be, "bldgs\\desert\\dwallDRL1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1c0, "bldgs\\desert\\desDRL2plattop", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1c1, "bldgs\\desert\\desDRL2plat", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f0, "bldgs\\desert\\dputtL1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f1, "bldgs\\desert\\dputtL1base", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f2, "bldgs\\desert\\dshortflagL1", 0x1, 0x28, 0x1);
        LoadFLCAnimation(0x1f3, "bldgs\\desert\\desputtL2", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f4, "bldgs\\desert\\desputtL2", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f5, "bldgs\\desert\\desputtL2scenic", 0x4, 0x78, 0x1);
        LoadFLCAnimation(0x1c2, "bldgs\\Desert\\DESCartL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c5, "bldgs\\Desert\\DESCartL1Base", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c3, "bldgs\\Desert\\DESCartL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c6, "bldgs\\Desert\\DESCartL2Base", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c7, "bldgs\\Desert\\DESCartL2Anim", 0x4, 0x1, 0);
        LoadFLCAnimation(0x1f6, "bldgs\\Desert\\DESclubL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1f9, "bldgs\\Desert\\DESclubL1base", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1f7, "bldgs\\Desert\\DESclubL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1ce, "bldgs\\Desert\\HeliL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d1, "bldgs\\Desert\\HeliL1_Base", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1cf, "bldgs\\Desert\\HelL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d2, "bldgs\\Desert\\HelL2Base", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d0, "bldgs\\Desert\\HelAnimLites", 0x4, 0x1, 0);
        LoadFLCAnimation(0x1d3, "bldgs\\Desert\\HelAnimElev", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1e0, "bldgs\\Desert\\dproL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1e3, "bldgs\\Desert\\dproL1Base", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1e1, "bldgs\\Desert\\desproL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1e4, "bldgs\\Desert\\desproL2Anim", 0x4, 0x1, 0);
        LoadFLCAnimation(0x1d4, "bldgs\\Desert\\spaL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d7, "bldgs\\Desert\\spaL1_Base", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d5, "bldgs\\Desert\\spaL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d9, "bldgs\\Desert\\DESCASL2Wheel", 0x4, 0xf0, 0);
        LoadFLCAnimation(0x1da, "bldgs\\Desert\\DESCASL1", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1dd, "bldgs\\Desert\\DESCASL1Base", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1dc, "bldgs\\Desert\\DESCASL1Anim", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1db, "bldgs\\Desert\\DESCASL2", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1de, "bldgs\\Desert\\DESCASL2Base", 0x4, 0xf0, 0);
        LoadFLCAnimation(0x1df, "bldgs\\Desert\\DESCASL2Status", 0x4, 0xf0, 0);
        LoadFLCAnimation(0x1e6, "bldgs\\Desert\\DESHotelL1", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1e9, "bldgs\\Desert\\DESHotelL1base", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1e8, "bldgs\\Desert\\DESHotelL1Anim", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1e7, "bldgs\\Desert\\DESHoL2", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1ea, "bldgs\\Desert\\DESHoL2Status", 0x4, 0xf0, 0);
        LoadFLCAnimation(0x1eb, "bldgs\\Desert\\DESHoL2elev", 0x4, 0xf0, 0x1);
    }
    if (g_currentThemeId == THEME_LINKS) {
        LoadFLCAnimation(0x1c8, "bldgs\\links\\sign_links", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c9, "bldgs\\links\\Houseconst_links", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1ca, "Homes\\Regular\\Links\\B\\LinksHouseB", 0x4, 0x8c, 0x1);
        LoadFLCAnimation(0x1ec, "bldgs\\links\\Pub_SQ", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1ed, "bldgs\\links\\Pub_SQ_dirt", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1ee, "bldgs\\links\\PubL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1ef, "bldgs\\links\\PubL2_dirt", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1b6, "bldgs\\links\\DR_shop", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1b7, "bldgs\\links\\DRL2_shop", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1b8, "bldgs\\links\\DR_platform", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1b9, "bldgs\\links\\DR_platform_dirt", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1ba, "bldgs\\links\\DR_100sign", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1bb, "bldgs\\links\\DR_200sign", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1bc, "bldgs\\links\\DR_300sign", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1bd, "bldgs\\links\\DR_flag", 0x1, 0x28, 0x1);
        LoadFLCAnimation(0x1be, "bldgs\\links\\rockwall01", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1bf, "bldgs\\links\\DRL2_shop_ANIM", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c0, "bldgs\\links\\DRL2_platform", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1c1, "bldgs\\links\\DRL2_platform_base", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f0, "bldgs\\links\\PG_hutL1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f1, "bldgs\\links\\PG_hutL1_dirt", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f2, "bldgs\\links\\PG_pflag", 0x1, 0x28, 0x1);
        LoadFLCAnimation(0x1f3, "bldgs\\links\\PG_hutL2", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f4, "bldgs\\links\\PG_hutL2_dirt", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f5, "bldgs\\links\\PG_scenicL2", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1c2, "bldgs\\links\\Cart_garageL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c5, "bldgs\\links\\Cart_garageL1_dirt", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c3, "bldgs\\links\\Cart_garageL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c6, "bldgs\\links\\Cart_garageL2_dirt", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1e0, "bldgs\\links\\Pro_shop", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1e3, "bldgs\\links\\Pro_shop_dirt", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1e2, "bldgs\\links\\Pro_shop_anim", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1e1, "bldgs\\links\\Links_ProshopL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1e4, "bldgs\\links\\Links_ProshopL2_dirt", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1e5, "bldgs\\links\\Links_ProshopL2_anim", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1f6, "bldgs\\links\\clubL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1f9, "bldgs\\links\\clubL1_dirt", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1fa, "bldgs\\links\\clubL1_ANIM", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1f7, "bldgs\\links\\clubL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1f8, "bldgs\\links\\clubL2_dirt", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1fb, "bldgs\\links\\clubL2_ANIM", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d4, "bldgs\\links\\StableL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d7, "bldgs\\links\\StableL1_dirt", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d5, "bldgs\\links\\StableL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d8, "bldgs\\links\\StableL2_dirt", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d9, "bldgs\\links\\StableL2_anim", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1ce, "bldgs\\links\\ChurchL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d1, "bldgs\\links\\ChurchL1_dirt", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1cf, "bldgs\\links\\ChurchL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d2, "bldgs\\links\\ChurchL2_dirt", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1e6, "bldgs\\links\\HotelL1", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1e9, "bldgs\\links\\HotelL1_dirt", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1e8, "bldgs\\links\\HotelL1_anim", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1e7, "bldgs\\links\\HotelL2", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1ea, "bldgs\\links\\HotelL2_dirt", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1eb, "bldgs\\links\\HotelL2_anim", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1da, "bldgs\\links\\CastleL1", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1dd, "bldgs\\links\\CastleL1_dirt", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1db, "bldgs\\links\\CastleL2", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1de, "bldgs\\links\\CastleL2_dirt", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1dc, "bldgs\\links\\CastleL2_ANIM", 0x4, 0xf0, 0x1);
    }
    if (g_currentThemeId == THEME_PARK) {
        LoadFLCAnimation(0x1c8, "bldgs\\park\\sign", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c9, "bldgs\\park\\Houseconst", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1ca, "Homes\\Regular\\Parkland\\A\\ParkHouseA", 0x4, 0x8c, 0x1);
        LoadFLCAnimation(0x1ec, "bldgs\\park\\parksnackL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1ed, "bldgs\\park\\parksnackL1_base", 0x4, 0x1, 0);
        LoadFLCAnimation(0x1d3, "bldgs\\park\\parksnackL1_ANIM", 0x4, 0x1, 0);
        LoadFLCAnimation(0x1ee, "bldgs\\park\\ParkSnackL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1ef, "bldgs\\park\\ParkSnackL2_base", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d0, "bldgs\\park\\parksnackL2_ANIM", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1f6, "bldgs\\park\\clubL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1f9, "bldgs\\park\\clubL1_base", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1fa, "bldgs\\park\\clubL1_ANIM", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1f7, "bldgs\\park\\clubL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1f8, "bldgs\\park\\clubL2_base", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1fb, "bldgs\\park\\clubL2_ANIM", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1b6, "bldgs\\park\\buildDRL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1b7, "bldgs\\park\\buildDRL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1b8, "bldgs\\park\\plat_topDRL1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1b9, "bldgs\\park\\plat_baseDRL1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1ba, "bldgs\\park\\100DRL1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1bb, "bldgs\\park\\200DRL1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1bc, "bldgs\\park\\300DRL1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1bd, "bldgs\\park\\flag_DRL1", 0x1, 0x28, 0x1);
        LoadFLCAnimation(0x1be, "bldgs\\park\\wall_DRL1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1bf, "bldgs\\park\\buildDRL2_ANIM", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c0, "bldgs\\park\\plat_topDRL2", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1c1, "bldgs\\park\\plat_baseDRL2", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f0, "bldgs\\park\\puttL1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f1, "bldgs\\park\\puttL1", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f2, "bldgs\\park\\shortflagL1", 0x1, 0x28, 0x1);
        LoadFLCAnimation(0x1f3, "bldgs\\park\\puttL2", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f4, "bldgs\\park\\puttL2_ANIM", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1f5, "bldgs\\park\\puttscenic", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1ce, "bldgs\\park\\marL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d1, "bldgs\\park\\marL1_waterANIM", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1cf, "bldgs\\park\\marL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d2, "bldgs\\park\\marL2_waterANIM", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1e6, "bldgs\\park\\HotelL1", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1e9, "bldgs\\park\\HotelL1_base", 0x4, 0xf0, 0);
        LoadFLCAnimation(0x1e8, "bldgs\\park\\HotelL1_ANIM", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1e7, "bldgs\\park\\HotelL2", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1ea, "bldgs\\park\\HotelL2_base", 0x4, 0xf0, 0);
        LoadFLCAnimation(0x1eb, "bldgs\\park\\HotelL2_ANIM", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1da, "bldgs\\park\\airL1", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1dd, "bldgs\\park\\airL1_base", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1dc, "bldgs\\park\\airL1_ANIM", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1db, "bldgs\\park\\airL2", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1de, "bldgs\\park\\airL2_base", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1df, "bldgs\\park\\airL2_ANIM", 0x4, 0xf0, 0x1);
        LoadFLCAnimation(0x1c2, "bldgs\\park\\CartL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c5, "bldgs\\park\\CartL1_base", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c4, "bldgs\\park\\CartL1_ANIM", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c3, "bldgs\\park\\CartL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c6, "bldgs\\park\\CartL2_base", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1c7, "bldgs\\park\\CartL2_ANIM", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1e0, "bldgs\\park\\ProsL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1e3, "bldgs\\park\\ProSL1_base", 0x4, 0x1, 0);
        LoadFLCAnimation(0x1e1, "bldgs\\park\\ProsL2", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1e2, "bldgs\\park\\ProsL2_ANIM01", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1e5, "bldgs\\park\\ProsL2_ANIM02", 0x4, 0x1, 0);
        LoadFLCAnimation(0x1d4, "bldgs\\park\\tenL1", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1d5, "bldgs\\park\\tenL2", 0x4, 0x1, 0x1);
    }
    switch (g_currentThemeId) {
    case THEME_TROPICAL:
        ld.load_pcx("flics\\Bridges\\TRopBridgePal", &g_palettes[78], 0, PALETTE_COLORS, 0);
        LoadFLCAnimation(0x1fe, "Bridges\\TROPBridgeTILE", 0x2, 0x3c, 0x1);
        LoadFLCAnimation(0x1ff, "Bridges\\TROPBridgeCap", 0x4, 0x3c, 0x1);
        LoadFLCAnimation(0x200, "Bridges\\TROPBridgeL", 0x4, 0x3c, 0x1);
        LoadFLCAnimation(0x201, "Bridges\\TROPBridgeT", 0x4, 0x3c, 0x1);
        LoadFLCAnimation(0x202, "Bridges\\TROPBridgeX", 0x1, 0x3c, 0x1);
        LoadFLCAnimation(0x204, "Bridges\\TROPBridgeReflect", 0x2, 0x3c, 0);
        LoadFLCAnimation(0x205, "Bridges\\TROPBridgeXtra", 0x4, 0x3c, 0x1);
        ld.load_pcx("flics\\Bridges\\DESScenicBridgePal", &g_palettes[79], 0, PALETTE_COLORS, 0);
        LoadFLCAnimation(0x203, "Bridges\\DESBridgeScenic", 0x2, 0x3c, 0x1);
        ld.load_pcx("flics\\scenic\\Pelican_Palette", &g_palettes[60], 0, PALETTE_COLORS, 0);
        LoadFLCAnimation(0x206, "Scenic\\Pelican_SQ", 0x4, 0x8c, 0x1);
        LoadFLCAnimation(0x207, "Scenic\\Pelican_Fidget", 0x4, 0x8c, 0x1);
        LoadFLCAnimation(0x22e, "Water\\ASTropSandA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x22f, "Water\\ASTropSandB", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x230, "Water\\UWTropCoralA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x231, "Water\\UWTropCoralB", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x232, "Water\\UWTropCoralC", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x233, "Water\\WaveTropA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x234, "Water\\WaveTropA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x235, "Water\\WaveTropA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x237, "Water\\TropWaterFallShortA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x238, "Water\\TropWaterFallTallA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x23b, "Water\\TropSprayShortA", 0x2, 0x3c, 0);
        LoadFLCAnimation(0x23c, "Water\\TropSprayTallA", 0x2, 0x3c, 0);
        LoadFLCAnimation(0x239, "Water\\TropWaterfallTallTopA", 0x2, 0x3c, 0);
        break;
    case THEME_PARK:
        ld.load_pcx("flics\\Bridges\\ParkBridgePal", &g_palettes[78], 0, PALETTE_COLORS, 0);
        LoadFLCAnimation(0x1fe, "Bridges\\BridgeTILE", 0x2, 0x3c, 0x1);
        LoadFLCAnimation(0x1ff, "Bridges\\BridgeCap", 0x4, 0x3c, 0x1);
        LoadFLCAnimation(0x200, "Bridges\\BridgeL", 0x4, 0x3c, 0x1);
        LoadFLCAnimation(0x201, "Bridges\\BridgeT", 0x4, 0x3c, 0x1);
        LoadFLCAnimation(0x202, "Bridges\\BridgeX", 0x1, 0x3c, 0x1);
        LoadFLCAnimation(0x204, "Bridges\\BridgeReflect", 0x2, 0x3c, 0);
        LoadFLCAnimation(0x205, "Bridges\\BridgeXtra", 0x4, 0x3c, 0x1);
        ld.load_pcx("flics\\Bridges\\DESScenicBridgePal", &g_palettes[79], 0, PALETTE_COLORS, 0);
        LoadFLCAnimation(0x203, "Bridges\\DESBridgeScenic", 0x2, 0x3c, 0x1);
        ld.load_pcx("flics\\scenic\\EaglePalette", &g_palettes[60], 0, PALETTE_COLORS, 0);
        LoadFLCAnimation(0x206, "Scenic\\Eagle_SQ", 0x4, 0x8c, 0x1);
        LoadFLCAnimation(0x207, "Scenic\\Eagle_Fidget", 0x4, 0x8c, 0x1);
        LoadFLCAnimation(0x22e, "Water\\ASParkRockA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x22f, "Water\\ASParkRockB", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x230, "Water\\UWParkRockA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x231, "Water\\UWParkRockB", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x232, "Water\\UWParkRockC", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x233, "Water\\RippleParkA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x234, "Water\\RippleParkB", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x235, "Water\\RippleParkC", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x237, "Water\\WaterFallShortA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x238, "Water\\WaterFallTallA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x23b, "Water\\SprayShortA", 0x2, 0x3c, 0);
        LoadFLCAnimation(0x23c, "Water\\SprayTallA", 0x2, 0x3c, 0);
        LoadFLCAnimation(0x239, "Water\\ParkWaterfallTallTopA", 0x2, 0x3c, 0);
        break;
    case THEME_DESERT:
        ld.load_pcx("flics\\Bridges\\DesBridgePal", &g_palettes[78], 0, PALETTE_COLORS, 0);
        LoadFLCAnimation(0x1fe, "Bridges\\DESBridgeTILE", 0x2, 0x3c, 0x1);
        LoadFLCAnimation(0x1ff, "Bridges\\DESBridgeCap", 0x4, 0x3c, 0x1);
        LoadFLCAnimation(0x200, "Bridges\\DESBridgeL", 0x4, 0x3c, 0x1);
        LoadFLCAnimation(0x201, "Bridges\\DESBridgeT", 0x4, 0x3c, 0x1);
        LoadFLCAnimation(0x202, "Bridges\\DESBridgeX", 0x1, 0x3c, 0x1);
        LoadFLCAnimation(0x204, "Bridges\\DESBridgeReflect", 0x2, 0x3c, 0);
        LoadFLCAnimation(0x205, "Bridges\\DESBridgeXtra", 0x4, 0x3c, 0x1);
        ld.load_pcx("flics\\Bridges\\DESScenicBridgePal", &g_palettes[79], 0, PALETTE_COLORS, 0);
        LoadFLCAnimation(0x203, "Bridges\\DESBridgeScenic", 0x2, 0x3c, 0x1);
        ld.load_pcx("flics\\scenic\\Vulture_Palette", &g_palettes[60], 0, PALETTE_COLORS, 0);
        LoadFLCAnimation(0x206, "Scenic\\Vulture_SQ", 0x4, 0x8c, 0x1);
        LoadFLCAnimation(0x207, "Scenic\\Vulture_Fidget", 0x4, 0x8c, 0x1);
        LoadFLCAnimation(0x22e, "Water\\ASDesRockA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x22f, "Water\\ASDesRockB", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x230, "Water\\UWDesRockA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x231, "Water\\UWDesRockB", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x232, "Water\\UWDesRockC", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x233, "Water\\RippleDesA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x234, "Water\\RippleDesB", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x235, "Water\\RippleDesC", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x237, "Water\\DesWaterFallShortA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x238, "Water\\DesWaterFallTallA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x23b, "Water\\DesSprayShortA", 0x2, 0x3c, 0);
        LoadFLCAnimation(0x23c, "Water\\DesSprayTallA", 0x2, 0x3c, 0);
        LoadFLCAnimation(0x239, "Water\\DesWaterfallTallTopA", 0x2, 0x3c, 0);
        break;
    case THEME_LINKS:
        ld.load_pcx("flics\\Bridges\\LinksBridgePalette", &g_palettes[78], 0, PALETTE_COLORS, 0);
        LoadFLCAnimation(0x1fe, "Bridges\\LinksBridgeTILE", 0x2, 0x3c, 0x1);
        LoadFLCAnimation(0x1ff, "Bridges\\LinksBridgeCap", 0x4, 0x3c, 0x1);
        LoadFLCAnimation(0x200, "Bridges\\LinksBridgeL", 0x4, 0x3c, 0x1);
        LoadFLCAnimation(0x201, "Bridges\\LinksBridgeT", 0x4, 0x3c, 0x1);
        LoadFLCAnimation(0x202, "Bridges\\LinksBridgeX", 0x1, 0x3c, 0x1);
        LoadFLCAnimation(0x204, "Bridges\\LinksBridgeReflect", 0x2, 0x3c, 0);
        LoadFLCAnimation(0x205, "Bridges\\LinksBridgeXtra", 0x4, 0x3c, 0x1);
        ld.load_pcx("flics\\Bridges\\DESScenicBridgePal", &g_palettes[79], 0, PALETTE_COLORS, 0);
        LoadFLCAnimation(0x203, "Bridges\\DESBridgeScenic", 0x2, 0x3c, 0x1);
        ld.load_pcx("flics\\scenic\\Hawk_Palette", &g_palettes[60], 0, PALETTE_COLORS, 0);
        LoadFLCAnimation(0x206, "Scenic\\Hawk_SQ", 0x4, 0x8c, 0x1);
        LoadFLCAnimation(0x207, "Scenic\\Hawk_Fidget", 0x4, 0x8c, 0x1);
        LoadFLCAnimation(0x22e, "Water\\ASLinksRockA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x22f, "Water\\ASLinksRockB", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x230, "Water\\UWLinksRockA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x231, "Water\\UWLinksRockB", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x232, "Water\\UWLinksRockC", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x233, "Water\\WaveLinksA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x234, "Water\\WaveLinksA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x235, "Water\\WaveLinksA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x237, "Water\\LinksWaterFallShortA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x238, "Water\\LinksWaterFallTallA", 0x4, 0x3c, 0);
        LoadFLCAnimation(0x23b, "Water\\LinksSprayShortA", 0x2, 0x3c, 0);
        LoadFLCAnimation(0x23c, "Water\\LinksSprayTallA", 0x2, 0x3c, 0);
        LoadFLCAnimation(0x239, "Water\\LinksWaterfallTallTopA", 0x2, 0x3c, 0);
        break;
    }
    if (g_currentThemeId == THEME_DESERT) {
        ld.load_pcx("flics\\trees\\Desert\\JoshuaTreePal", &g_palettes[37], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\Desert\\JoshuaTreePal", &g_palettes[38], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\Desert\\JoshuaTreePal", &g_palettes[39], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\Desert\\JoshuaTreePal", &g_palettes[40], 0, PALETTE_COLORS, 0);
    } else if (g_currentThemeId == THEME_TROPICAL) {
        ld.load_pcx("flics\\trees\\Tropic\\TropBrushPal", &g_palettes[37], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\Tropic\\TropBrushPal2", &g_palettes[38], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\Tropic\\TropBrushPal3", &g_palettes[39], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\Tropic\\TropBrushPal", &g_palettes[40], 0, PALETTE_COLORS, 0);
    } else {
        ld.load_pcx("flics\\trees\\PalGreenMaple", &g_palettes[37], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\PalRedMaple", &g_palettes[38], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\PalBrownMaple", &g_palettes[39], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\PalBlueMaple", &g_palettes[40], 0, PALETTE_COLORS, 0);
    }
    if (g_currentThemeId == THEME_TROPICAL) {
        ld.load_pcx("flics\\trees\\Tropic\\Tree Cerc\\PalCerc", &g_palettes[41], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\Tropic\\Tree Drac\\PalDrac", &g_palettes[42], 0, PALETTE_COLORS, 0);
    }
    if (g_currentThemeId == THEME_PARK) {
        ld.load_pcx("flics\\trees\\PalPineBlue", &g_palettes[41], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\PalPineDarkGreen", &g_palettes[42], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\PalPineOriginal", &g_palettes[43], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\PalPineYellow", &g_palettes[44], 0, PALETTE_COLORS, 0);
    }
    if (g_currentThemeId == THEME_DESERT) {
        ld.load_pcx("flics\\trees\\Desert\\CactusAPal", &g_palettes[41], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\Desert\\CactusCPal", &g_palettes[42], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\flowers\\DesertFlowersRedPal", &g_palettes[45], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\flowers\\DesertFlowersOrgPal", &g_palettes[46], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\flowers\\DesertFlowersPurpPal", &g_palettes[47], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\flowers\\DesertFlowersBluePal", &g_palettes[48], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\flowers\\DesertFlowersPinkPal", &g_palettes[49], 0, PALETTE_COLORS, 0);
    } else if (g_currentThemeId == THEME_TROPICAL) {
        ld.load_pcx("flics\\flowers\\TropicalFlowers_AquaPal", &g_palettes[45], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\flowers\\TropicalFlowers_OrgPal", &g_palettes[46], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\flowers\\TropicalFlowers_PurpPal", &g_palettes[47], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\flowers\\TropicalFlowers_BluePal", &g_palettes[48], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\flowers\\TropicalFlowers_RedPal", &g_palettes[49], 0, PALETTE_COLORS, 0);
    } else {
        ld.load_pcx("flics\\flowers\\FlowerBedA_YelPal", &g_palettes[45], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\flowers\\FlowerBedA_OrgPal", &g_palettes[46], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\flowers\\FlowerBedA_PurpPal", &g_palettes[47], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\flowers\\FlowerBedA_WhitePal", &g_palettes[48], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\flowers\\FlowerBedA_RedPal", &g_palettes[49], 0, PALETTE_COLORS, 0);
    }
    if (g_currentThemeId == THEME_LINKS) {
        ld.load_pcx("flics\\trees\\Links\\LinksPinePal", &g_palettes[41], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\Links\\LinksPinePal2", &g_palettes[42], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\Links\\LinksPinePal3", &g_palettes[43], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\Links\\LinksPinePal4", &g_palettes[44], 0, PALETTE_COLORS, 0);
    }
    if (g_currentThemeId != THEME_LINKS) {
        if (g_currentThemeId == THEME_DESERT) {
            ld.load_pcx("flics\\trees\\desert\\tallpalmdesertpal", &g_palettes[50], 0, PALETTE_COLORS, 0);
            ld.load_pcx("flics\\trees\\desert\\tallpalmdesertpal", &g_palettes[51], 0, PALETTE_COLORS, 0);
            ld.load_pcx("flics\\trees\\desert\\tallpalmdesertpal", &g_palettes[52], 0, PALETTE_COLORS, 0);
            ld.load_pcx("flics\\trees\\desert\\tallpalmdesertpal", &g_palettes[53], 0, PALETTE_COLORS, 0);
        } else if (g_currentThemeId == THEME_TROPICAL) {
            ld.load_pcx("flics\\trees\\tropic\\tree tall palm\\paltallpalm", &g_palettes[50], 0, PALETTE_COLORS, 0);
            ld.load_pcx("flics\\trees\\tropic\\tree tall palm\\paltallpalm", &g_palettes[51], 0, PALETTE_COLORS, 0);
            ld.load_pcx("flics\\trees\\tropic\\tree tall palm\\paltallpalm", &g_palettes[52], 0, PALETTE_COLORS, 0);
            ld.load_pcx("flics\\trees\\tropic\\tree tall palm\\paltallpalm", &g_palettes[53], 0, PALETTE_COLORS, 0);
        } else {
            ld.load_pcx("flics\\trees\\PalOrangePalm", &g_palettes[50], 0, PALETTE_COLORS, 0);
            ld.load_pcx("flics\\trees\\PalOriginalPalm", &g_palettes[51], 0, PALETTE_COLORS, 0);
            ld.load_pcx("flics\\trees\\PalPlumPalm", &g_palettes[52], 0, PALETTE_COLORS, 0);
            ld.load_pcx("flics\\trees\\PalGreenPalm", &g_palettes[53], 0, PALETTE_COLORS, 0);
        }
    } else {
        ld.load_pcx("flics\\trees\\Links\\ScotsPinePal", &g_palettes[50], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\Links\\ScotsPinePal2", &g_palettes[51], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\Links\\ScotsPinePal3", &g_palettes[52], 0, PALETTE_COLORS, 0);
        ld.load_pcx("flics\\trees\\Links\\ScotsPinePal4", &g_palettes[53], 0, PALETTE_COLORS, 0);
    }
    switch (g_currentThemeId) {
    case THEME_DESERT:
        memcpy(g_squareTypeTable, g_terrainTypeDefaults, 1104);
        strcpy(g_squareTypeTable[TERRAIN_ROUGH].name, "desert");
        strcpy(g_squareTypeTable[TERRAIN_DEEP_ROUGH].name, "rough");
        strcpy(g_squareTypeTable[TERRAIN_BRUSH].name, "ravine");
        strcpy(g_squareTypeTable[TERRAIN_RAVINE].name, "brush");
        strcpy(g_squareTypeTable[TERRAIN_PINE_TREE].name, "cactus");
        strcpy(&g_squareTypeTable[TERRAIN_PINE_TREE].name[16], "cacti");
        strcpy(g_squareTypeTable[TERRAIN_TREE].name, "joshua tree");
        strcpy(&g_squareTypeTable[TERRAIN_TREE].name[16], "joshua trees");
        strcpy(g_squareTypeTable[TERRAIN_PALM_TREE].name, "palm tree");
        strcpy(&g_squareTypeTable[TERRAIN_PALM_TREE].name[16], "palm trees");
        strcpy(g_squareTypeTable[TERRAIN_WETLANDS].name, "canyon");
        g_squareTypeTable[TERRAIN_ROUGH].lieGrade = 1;
        g_squareTypeTable[TERRAIN_DEEP_ROUGH].category = 5;
        g_squareTypeTable[TERRAIN_WETLANDS].height[0] = 0;
        g_squareTypeTable[TERRAIN_WETLANDS].flags = 0;
        strcpy(g_facilityTypes[SCENERY_SNACK_BAR].name, "Snack Bar");
        strcpy(g_facilityTypes[SCENERY_AIRSTRIP].name, "Casino");
        strcpy(g_facilityTypes[SCENERY_SWIM_CLUB].name, "Spa");
        strcpy(g_facilityTypes[SCENERY_MARINA].name, "Helipad");
        break;
    case THEME_PARK:
        memcpy(g_squareTypeTable, g_terrainTypeDefaults, 1104);
        strcpy(g_squareTypeTable[TERRAIN_RAVINE].name, "stream");
        strcpy(g_squareTypeTable[TERRAIN_BRUSH].name, "brush");
        strcpy(g_squareTypeTable[TERRAIN_WETLANDS].name, "wetlands");
        strcpy(g_facilityTypes[SCENERY_SNACK_BAR].name, "Snack Bar");
        strcpy(g_facilityTypes[SCENERY_AIRSTRIP].name, "Airstrip");
        strcpy(g_facilityTypes[SCENERY_SWIM_CLUB].name, "Tennis Court");
        strcpy(g_facilityTypes[SCENERY_MARINA].name, "Marina");
        break;
    case THEME_TROPICAL:
        memcpy(g_squareTypeTable, g_terrainTypeDefaults, 1104);
        strcpy(g_squareTypeTable[TERRAIN_RAVINE].name, "stream");
        strcpy(g_squareTypeTable[TERRAIN_BRUSH].name, "brush");
        strcpy(g_squareTypeTable[TERRAIN_TREE].name, "tropical bush");
        strcpy(&g_squareTypeTable[TERRAIN_TREE].name[16], "tropical bushes");
        strcpy(g_squareTypeTable[TERRAIN_PINE_TREE].name, "tropical tree");
        strcpy(&g_squareTypeTable[TERRAIN_PINE_TREE].name[16], "tropical trees");
        strcpy(g_squareTypeTable[TERRAIN_PALM_TREE].name, "palm tree");
        strcpy(&g_squareTypeTable[TERRAIN_PALM_TREE].name[16], "palm trees");
        strcpy(g_squareTypeTable[TERRAIN_WETLANDS].name, "wetlands");
        strcpy(g_facilityTypes[SCENERY_SNACK_BAR].name, "Snack Bar");
        strcpy(g_facilityTypes[SCENERY_AIRSTRIP].name, "Theme Park");
        strcpy(g_facilityTypes[SCENERY_SWIM_CLUB].name, "Swim Club");
        strcpy(g_facilityTypes[SCENERY_MARINA].name, "Marina");
        break;
    case THEME_LINKS:
        memcpy(g_squareTypeTable, g_terrainTypeDefaults, 1104);
        strcpy(g_squareTypeTable[TERRAIN_RAVINE].name, "burn");
        strcpy(g_squareTypeTable[TERRAIN_BRUSH].name, "gorse");
        strcpy(g_squareTypeTable[TERRAIN_PINE_TREE].name, "pine");
        strcpy(&g_squareTypeTable[TERRAIN_PINE_TREE].name[16], "pines");
        strcpy(g_squareTypeTable[TERRAIN_TREE].name, "maple tree");
        strcpy(&g_squareTypeTable[TERRAIN_TREE].name[16], "maple trees");
        strcpy(g_squareTypeTable[TERRAIN_PALM_TREE].name, "scots pine");
        strcpy(&g_squareTypeTable[TERRAIN_PALM_TREE].name[16], "scots pines");
        strcpy(g_facilityTypes[SCENERY_SNACK_BAR].name, "Pub");
        strcpy(g_facilityTypes[SCENERY_SWIM_CLUB].name, "Spa");
        strcpy(g_facilityTypes[SCENERY_MARINA].name, "Helipad");
        strcpy(g_facilityTypes[SCENERY_AIRSTRIP].name, "Castle");
        strcpy(g_facilityTypes[SCENERY_SWIM_CLUB].name, "Stable");
        strcpy(g_facilityTypes[SCENERY_MARINA].name, "Church");
        break;
    }
#define TERRAIN_PRICE_SCALE 1
    for (i = 0; i < 23; i++)
        g_squareTypeTable[i].price *= TERRAIN_PRICE_SCALE;
    for (i = 0x190; i <= 0x1b3; i++) {
        if (g_flicStartFrame[i] != -1)
            FreeSpritePoolByIndex(i);
        g_flicStartFrame[i] = -1;
        g_flcFileNames[i][0] = 0;
    }
    if (g_currentThemeId == THEME_DESERT) {
        LoadFLCAnimation(0x191, "bldgs\\desert\\dputt_whiteflag", 0x1, 0, 0x1);
        LoadFLCAnimation(0x190, "flowers\\OilSlick", 0x4, 0, 0x1);
        LoadFLCAnimation(0x1a1, "Scenic\\SailboatDesert", 0x8, 0x50, 0x1);
        LoadFLCAnimation(0x19f, "Scenic\\DolphinDesert", 0x4, 0x3c, 0x1);
        LoadFLCAnimation(0x1a0, "Scenic\\Desert_waterspray", 0x1, 0x28, 0x1);
        LoadFLCAnimation(0x19b, "trees\\Desert\\JoshuaTree_Sm", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x19c, "trees\\Desert\\JoshuaTree_Md", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x19d, "trees\\Desert\\JoshuaTree_Lg", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1a2, "flowers\\DesFlowers_Single", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a3, "flowers\\DesFlowers_1Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a4, "flowers\\DesFlowers_2Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a5, "flowers\\DesFlowers_3Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a6, "flowers\\DesFlowers_4Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a7, "flowers\\DesFlowers_Corner", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a8, "flowers\\DesSin_Single", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a9, "flowers\\DesSin_1Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1aa, "flowers\\DesSin_2Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1ab, "flowers\\DesSin_3Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1ac, "flowers\\DesSin_4Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1ad, "flowers\\DesSin_Corner", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1ae, "flowers\\DesFlowers_Single_Wall", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1af, "flowers\\DesFlowers_1Side_Wall", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1b0, "flowers\\DesFlowers_2Side_Wall", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1b1, "flowers\\DesFlowers_3Side_Wall", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1b2, "flowers\\DesFlowers_4Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1b3, "flowers\\DesFlowers_Corner_Wall", 0x4, 0x28, 0x1);
    } else if (g_currentThemeId == THEME_TROPICAL) {
        LoadFLCAnimation(0x190, "flowers\\DryGrass", 0x4, 0, 0x1);
        LoadFLCAnimation(0x191, "bldgs\\tropical\\TROPPuttingG_whiteflag", 0x1, 0, 0x1);
        LoadFLCAnimation(0x1a1, "Scenic\\SailboatTropical", 0x8, 0x50, 0x1);
        LoadFLCAnimation(0x19f, "Scenic\\DolphinTropical", 0x4, 0x3c, 0x1);
        LoadFLCAnimation(0x1a0, "Scenic\\Trop_waterspray", 0x1, 0x28, 0x1);
        LoadFLCAnimation(0x19b, "trees\\Tropic\\TropicalBrush_Sm", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x19c, "trees\\Tropic\\TropicalBrush_Med", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x19d, "trees\\Tropic\\TropicalBrush_Lg", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1a2, "flowers\\TropFlowers_Single", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a3, "flowers\\TropFlowers_1Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a4, "flowers\\TropFlowers_2Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a5, "flowers\\TropFlowers_3Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a6, "flowers\\TropFlowers_4Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a7, "flowers\\TropFlowers_Corner", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a8, "flowers\\SinTrop_Single", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a9, "flowers\\SinTrop_1Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1aa, "flowers\\SinTrop_2Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1ab, "flowers\\SinTrop_3Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1ac, "flowers\\SinTrop_4Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1ad, "flowers\\SinTrop_Corner", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1ae, "flowers\\TropFlowers_Single_Wall", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1af, "flowers\\TropFlowers_1Side_Wall", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1b0, "flowers\\TropFlowers_2Side_Wall", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1b1, "flowers\\TropFlowers_3Side_Wall", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1b2, "flowers\\TropFlowers_4Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1b3, "flowers\\TropFlowers_Corner_Wall", 0x4, 0x28, 0x1);
    } else {
        if (g_currentThemeId == THEME_PARK) {
            LoadFLCAnimation(0x191, "bldgs\\park\\putt_whiteflag", 0x1, 0, 0x1);
            LoadFLCAnimation(0x19f, "Scenic\\Dolphin", 0x4, 0x3c, 0x1);
            LoadFLCAnimation(0x1a0, "Scenic\\waterspray", 0x1, 0x28, 0x1);
            LoadFLCAnimation(0x1a1, "Scenic\\Sailboat_Move", 0x8, 0x50, 0x1);
        } else {
            LoadFLCAnimation(0x191, "bldgs\\links\\pg_whiteflag", 0x1, 0, 0x1);
            LoadFLCAnimation(0x19f, "Scenic\\DolphinLinks", 0x4, 0x3c, 0x1);
            LoadFLCAnimation(0x1a0, "Scenic\\Links_waterspray", 0x1, 0x28, 0x1);
            LoadFLCAnimation(0x1a1, "Scenic\\SailboatLinks", 0x8, 0x50, 0x1);
        }
        LoadFLCAnimation(0x190, "flowers\\dandelion_01", 0x4, 0, 0x1);
        LoadFLCAnimation(0x19b, "trees\\TreeMapleSmall", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x19c, "trees\\TreeMapleMedium", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x19d, "trees\\TreeMapleLarge", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x1a2, "flowers\\Flowers_Single", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a3, "flowers\\Flowers_1Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a4, "flowers\\Flowers_2Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a5, "flowers\\Flowers_3Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a6, "flowers\\Flowers_4Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a7, "flowers\\Flowers_Corner", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a8, "flowers\\SinFlowers_Single", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1a9, "flowers\\SinFlowers_1Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1aa, "flowers\\SinFlowers_2Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1ab, "flowers\\SinFlowers_3Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1ac, "flowers\\SinFlowers_4Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1ad, "flowers\\SinFlowers_Corner", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1ae, "flowers\\WalledFlowers_Single", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1af, "flowers\\WalledFlowers_1Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1b0, "flowers\\WalledFlowers_2Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1b1, "flowers\\WalledFlowers_3Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1b2, "flowers\\Flowers_4Side", 0x4, 0x28, 0x1);
        LoadFLCAnimation(0x1b3, "flowers\\WalledFlowers_Corner", 0x4, 0x28, 0x1);
    }
    if (g_currentThemeId == THEME_TROPICAL) {
        LoadFLCAnimation(0x192, "trees\\Tropic\\Tree Cerc\\Cerc_Small", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x193, "trees\\Tropic\\Tree Cerc\\Cerc_Med", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x194, "trees\\Tropic\\Tree Cerc\\Cerc_Large", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x198, "trees\\Tropic\\Tree Drac\\Drac_Small", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x199, "trees\\Tropic\\Tree Drac\\Drac_Med", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x19a, "trees\\Tropic\\Tree Drac\\Drac_Large", 0x4, 0x1, 0x1);
    }
    if (g_currentThemeId == THEME_PARK) {
        LoadFLCAnimation(0x192, "trees\\TreePineSpruceSm", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x193, "trees\\TreePineSpruceMed", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x194, "trees\\TreePineSpruceLg", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x198, "trees\\TreePineFirSm", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x199, "trees\\TreePineFirMed", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x19a, "trees\\TreePineFirLg", 0x4, 0x1, 0x1);
    }
    if (g_currentThemeId == THEME_DESERT) {
        LoadFLCAnimation(0x192, "trees\\Desert\\CactusA_Sm", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x193, "trees\\Desert\\CactusA_Md", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x194, "trees\\Desert\\CactusA_Lg", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x198, "trees\\Desert\\CactusC_Sm", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x199, "trees\\Desert\\CactusC_Md", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x19a, "trees\\Desert\\CactusC_Lg", 0x4, 0x1, 0x1);
    }
    if (g_currentThemeId == THEME_LINKS) {
        LoadFLCAnimation(0x192, "trees\\Links\\LinksPine_Small", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x193, "trees\\Links\\LinksPine_Med", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x194, "trees\\Links\\LinksPine_Tall", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x198, "trees\\Links\\LinksPine_Small", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x199, "trees\\Links\\LinksPine_Med", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x19a, "trees\\Links\\LinksPine_Tall", 0x4, 0x1, 0x1);
    }
    if (g_currentThemeId != THEME_LINKS) {
        if (g_currentThemeId == THEME_DESERT || g_currentThemeId == THEME_TROPICAL) {
            LoadFLCAnimation(0x195, "trees\\desert\\TallPalm_small", 0x4, 0x1, 0x1);
            LoadFLCAnimation(0x196, "trees\\desert\\TallPalm_med", 0x4, 0x1, 0x1);
            LoadFLCAnimation(0x197, "trees\\desert\\TallPalm_large", 0x4, 0x1, 0x1);
        } else {
            LoadFLCAnimation(0x195, "trees\\TreePalmSm", 0x4, 0x1, 0x1);
            LoadFLCAnimation(0x196, "trees\\TreePalmMed", 0x4, 0x1, 0x1);
            LoadFLCAnimation(0x197, "trees\\TreePalmLg", 0x4, 0x1, 0x1);
        }
    } else {
        LoadFLCAnimation(0x195, "trees\\Links\\ScotsPine_Small", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x196, "trees\\Links\\ScotsPine_Med", 0x4, 0x1, 0x1);
        LoadFLCAnimation(0x197, "trees\\Links\\ScotsPine_Lg", 0x4, 0x1, 0x1);
    }
}

int g_bankSlotOffset;
char g_flcFileNames[590][50];

void ResetAllGameState(int startFresh)
{
    int i, j, k, m;
    int nfiles;

    GenerateRandomTerrainNoise();
    ClearStringPool();

    memset(g_allHolesArray, 0, 10400);
    memset(g_golferPool, 0, 39936);
    memset(g_staffSlots, 0, 4864);
    memset(g_monthlySkillScore, 0, 1000);
    memset(g_moneyLine, 0, 1000);
    memset(g_funRatingHistPoints, 0, 1000);
    memset(g_gameMembershipTrendLine, 0, 1000);
    memset(g_monthEventKindHistory, 0, 1000);
    memset(g_talentKindCount, 0, 128);
    memset(g_annualStatsRecords, 0, 2000);
    memset(g_skillClassStatRows, 0, 5888);
    memset(g_top10CoursePoints, 0, 240);
    g_nextOpenHoleIndex = 1;
    memset(g_gameFairwayTargetPoints, 0xff, 456);
    memset(g_tileExtra, 0, 5000);
    memset(g_ballStopCountMap, 0, 2500);
    memset(g_pointElevationMap, 0, 2601);

    for (i = 0; i < 8; i++)
        g_brushPicks[i] = i;

    for (i = 0; i < 256; i++)
        g_buildingTable[i].kind = -1;

    g_proMatchStakeLvl = 0;
    for (i = 0; i < 9; i++) {
        int r;
        do {
            r = g_rng.NextInt(9);
        } while (g_top10CoursePoints[r][TOP_COURSE_TOTAL] != 0);
        for (j = 0; j <= i; j++) {
            g_top10CoursePoints[r][g_rng.NextInt(4)]++;
            g_top10CoursePoints[r][TOP_COURSE_TOTAL]++;
        }
    }

    if (startFresh == 0) {
        LoadThemeAssets();
        return;
    }

    g_curNextSpawnPos = 0;
    memset(g_phrasesPerGolferType, 0, 105000);
    memset(g_flicStartFrame, 0xff, 2360);
    memset(g_flicBankFrameLens, 0, 2360);
    memset(g_flcFileNames, 0, 29500);
    memset(g_memBases, 0xff, 400);
    {
        g_memBases[0] = -2;
        AssetSurface surf;
        short pts[8];

        surf.Create(1024, 800, 0, 1, 0, 0);
        surf.load_pcx("Bodies\\MaleSwap01", &g_palettes[0], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\MaleSwap01", &g_palettes[1], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\MaleSwap02", &g_palettes[2], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\MaleSwap03", &g_palettes[3], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\MaleSwap04", &g_palettes[4], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\MaleSwap05", &g_palettes[5], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\MaleSwap06", &g_palettes[6], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\MaleSwap07", &g_palettes[7], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\MaleSwap08", &g_palettes[8], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\MaleSwap09", &g_palettes[9], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\MaleSwap10", &g_palettes[10], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\CartPal", &g_palettes[30], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\trees\\WillowWhite", &g_palettes[54], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\trees\\WillowLemon", &g_palettes[55], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\trees\\WillowPink", &g_palettes[56], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\trees\\WillowGreen", &g_palettes[57], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\BlimpFXSPal", &g_palettes[58], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\BlimpEAPal", &g_palettes[59], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\bldgs\\upgrade bwasherPal", &g_palettes[61], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Homes\\Sum_homePal", &g_palettes[65], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Homes\\political_homePal", &g_palettes[66], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Homes\\ArtDeco_HomePal", &g_palettes[67], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Homes\\DesertCelebHouse_Palette", &g_palettes[68], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Homes\\1stSummer_Palette", &g_palettes[69], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Homes\\2ndSummer_Palette", &g_palettes[70], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\flowers\\CrabgrassPal", &g_palettes[76], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Scenic\\ScenicBridgePalette", &g_palettes[80], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Scenic\\wave_palette", &g_palettes[82], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\HotAirPal", &g_palettes[84], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\Scenic_OgrassPal", &g_palettes[85], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\VineyardPal", &g_palettes[86], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\CactusGroupAPal", &g_palettes[87], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\RhodyPal", &g_palettes[88], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\ScenicFlowersPal", &g_palettes[89], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\flowers\\RoseBushPal", &g_palettes[91], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\Rock FormPal", &g_palettes[92], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\tees\\TeeMarkerPal", &g_palettes[94], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\Scenic_TreePal", &g_palettes[95], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\wmillAPal", &g_palettes[104], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\LighthouseBPal", &g_palettes[113], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\LighthouseCPal", &g_palettes[107], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\BarnPal", &g_palettes[101], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\WindmillPal", &g_palettes[109], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\StonetwoPal", &g_palettes[103], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\equestrianPal", &g_palettes[110], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\EasterPal", &g_palettes[111], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\BuddhaPal", &g_palettes[108], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\PagodaPal", &g_palettes[112], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\SundialPal", &g_palettes[100], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\ChineseHousePal", &g_palettes[114], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\CivilWarCannonPal", &g_palettes[102], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\CivilWarStatuePal", &g_palettes[106], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\TarpitPal", &g_palettes[115], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\ParklandRockPal", &g_palettes[105], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\wtow2Pal", &g_palettes[116], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\Radio TowerPal", &g_palettes[117], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Landmarks\\Red Oil Pump Pal", &g_palettes[118], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\animals\\ELK_Palette", &g_palettes[120], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\animals\\CranePal", &g_palettes[121], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\animals\\FlamingoPal", &g_palettes[122], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\animals\\Sheep_Palette", &g_palettes[123], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\animals\\Croc_Palette", &g_palettes[124], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\animals\\Snake_Palette", &g_palettes[125], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\animals\\Gila_Palette", &g_palettes[126], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\animals\\RR_Palette", &g_palettes[127], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\animals\\Duck_Palette", &g_palettes[128], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\landmarks\\goldPal", &g_palettes[129], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\employee\\GreeterPal", &g_palettes[130], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\employee\\RangerPal", &g_palettes[131], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\employee\\GKPal", &g_palettes[132], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\employee\\TrayGirlPal", &g_palettes[133], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\employee\\GolfCelebPal", &g_palettes[134], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\employee\\MarshallPal", &g_palettes[135], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\employee\\LawnTechPal", &g_palettes[136], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\employee\\SodaVendorPal", &g_palettes[137], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\FemaleSwap01", &g_palettes[FEMALE_FIGURE], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\FemaleSwap02", &g_palettes[141], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\FemaleSwap03", &g_palettes[142], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\FemaleSwap04", &g_palettes[143], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\FemaleSwap05", &g_palettes[144], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\FemaleSwap06", &g_palettes[145], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\FemaleSwap07", &g_palettes[146], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\FemaleSwap08", &g_palettes[147], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\FemaleSwap09", &g_palettes[148], 0, PALETTE_COLORS, 0);
        surf.load_pcx("Bodies\\FemaleSwap10", &g_palettes[149], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Flowers\\GazeboPal", &g_palettes[150], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Flowers\\TopiaryPal", &g_palettes[188], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\CowSkullPal", &g_palettes[160], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\DeadGolferPal", &g_palettes[161], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\Desert Water RockPal", &g_palettes[162], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\grousepal", &g_palettes[163], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\naturalbridgePal", &g_palettes[164], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\DeadSwampTreesPal", &g_palettes[165], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\ScenicElmPal", &g_palettes[166], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\Tropical WildflowersPal", &g_palettes[167], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\ScenicLinksRocksPal", &g_palettes[168], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\bridges\\ScenicGenPal", &g_palettes[169], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\Scenic\\benWPal", &g_palettes[170], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\flowers\\box benchpal", &g_palettes[171], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\flowers\\redbenchpal", &g_palettes[172], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\flowers\\round wood benchpal", &g_palettes[173], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\flowers\\backless benchpal", &g_palettes[174], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\flowers\\LoversBenchpal", &g_palettes[175], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\flowers\\Swingpal", &g_palettes[176], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\bldgs\\DrivPuttPal", &g_palettes[177], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\tees\\TVpal", &g_palettes[179], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\PlumPal", &g_palettes[180], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\DogwoodPal", &g_palettes[181], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\ScenicElmPal", &g_palettes[182], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\JapaneseMaple_OrgPal", &g_palettes[183], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\CypressPal", &g_palettes[184], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\Scenic_TreePal", &g_palettes[185], 0, PALETTE_COLORS, 0);
        surf.load_pcx("flics\\scenic\\PeachTreePal", &g_palettes[186], 0, PALETTE_COLORS, 0);
        g_flicSurface.Create(480, 480, 8, 1, 0, 0);
        g_bankSlotOffset = 0;
        LoadFLCAnimation(373, "Landmarks\\LighthouseB", 4, 1, 1);
        LoadFLCAnimation(367, "Landmarks\\LighthouseC", 4, 1, 1);
        LoadFLCAnimation(364, "Landmarks\\wmillA", 4, 1, 1);
        LoadFLCAnimation(361, "Landmarks\\Barn", 4, 1, 1);
        LoadFLCAnimation(369, "Landmarks\\Windmill", 4, 1, 1);
        LoadFLCAnimation(363, "Landmarks\\Stone Two", 8, 1, 1);
        LoadFLCAnimation(370, "Landmarks\\equestrian", 4, 1, 1);
        LoadFLCAnimation(371, "Landmarks\\Easter", 4, 1, 1);
        LoadFLCAnimation(368, "Landmarks\\Buddha", 4, 1, 1);
        LoadFLCAnimation(372, "Landmarks\\Pagoda", 4, 1, 1);
        LoadFLCAnimation(360, "Landmarks\\Sundial", 4, 1, 1);
        LoadFLCAnimation(374, "Landmarks\\ChineseHouse", 4, 1, 1);
        LoadFLCAnimation(362, "Landmarks\\civilwarCannon", 4, 1, 1);
        LoadFLCAnimation(366, "Landmarks\\civilwarStatue", 4, 1, 1);
        LoadFLCAnimation(375, "Landmarks\\Tarpit", 4, 1, 1);
        LoadFLCAnimation(365, "Landmarks\\ParklandRock", 4, 1, 1);
        LoadFLCAnimation(376, "Landmarks\\Wtow2", 4, 1, 1);
        LoadFLCAnimation(377, "Landmarks\\Radio Tower", 4, 1, 1);
        LoadFLCAnimation(378, "Landmarks\\Red Oil Pump", 4, 1, 1);
        LoadFLCAnimation(380, "Bldgs\\1Putt", 4, 1, 1);
        LoadFLCAnimation(381, "Bldgs\\2Putt", 4, 1, 1);
        LoadFLCAnimation(382, "Bldgs\\1Drive", 4, 1, 1);
        LoadFLCAnimation(383, "Bldgs\\2Drive", 4, 1, 1);
        LoadFLCAnimation(248, "Scenic\\splash", 1, 40, 1);
        LoadFLCAnimation(295, "Scenic\\CowSkull", 4, 40, 1);
        LoadFLCAnimation(296, "Scenic\\DeadGolfer", 4, 40, 1);
        LoadFLCAnimation(297, "Scenic\\Desert Water Rock", 4, 40, 1);
        LoadFLCAnimation(298, "Scenic\\grouse", 4, 40, 1);
        LoadFLCAnimation(299, "Scenic\\Natural Bridge", 4, 40, 1);
        LoadFLCAnimation(300, "Scenic\\DeadSwampTrees", 4, 40, 1);
        LoadFLCAnimation(301, "Scenic\\Tropical Wildflowers", 4, 40, 1);
        LoadFLCAnimation(302, "Scenic\\ScenicLinksRocks", 4, 40, 1);
        LoadFLCAnimation(303, "Scenic\\Plum", 4, 100, 1);
        LoadFLCAnimation(304, "Scenic\\Dogwood", 4, 100, 1);
        LoadFLCAnimation(305, "Scenic\\ScenicElm", 4, 100, 1);
        LoadFLCAnimation(306, "Scenic\\JapaneseMaple", 4, 100, 1);
        LoadFLCAnimation(307, "Scenic\\Cypress", 4, 100, 1);
        LoadFLCAnimation(308, "Scenic\\Scenic_Tree", 4, 100, 1);
        LoadFLCAnimation(309, "Scenic\\PeachTree", 4, 100, 1);
        LoadFLCAnimation(240, "Scenic\\hotair01", 4, 1, 0);
        LoadFLCAnimation(241, "Scenic\\hotair01Shadow", 4, 1, 0);
        LoadFLCAnimation(242, "Scenic\\blimpfxs", 4, 240, 0);
        LoadFLCAnimation(243, "Scenic\\blimpfxsshadow", 4, 240, 0);
        LoadFLCAnimation(244, "Scenic\\blimpEA", 4, 240, 0);
        LoadFLCAnimation(245, "Scenic\\blimpEAshadow", 4, 240, 0);
        LoadFLCAnimation(250, "Scenic\\CactusGroupA", 4, 60, 1);
        LoadFLCAnimation(252, "Scenic\\Scenic_Flowers", 4, 40, 1);
        LoadFLCAnimation(251, "Scenic\\Rhody", 4, 40, 1);
        LoadFLCAnimation(253, "Scenic\\Scenic_Tree", 4, 100, 1);
        LoadFLCAnimation(254, "Scenic\\Scenic_OGrass", 4, 60, 1);
        LoadFLCAnimation(255, "Scenic\\Vineyard", 4, 60, 1);
        LoadFLCAnimation(256, "Landmarks\\Gold", 4, 60, 1);
        LoadFLCAnimation(257, "flowers\\RoseBush", 4, 60, 1);
        LoadFLCAnimation(258, "Scenic\\Rock Form", 4, 60, 1);
        LoadFLCAnimation(550, "Bridges\\SCENICgen03", 2, 40, 1);
        LoadFLCAnimation(551, "Bridges\\ScenicGen04", 2, 40, 1);
        LoadFLCAnimation(552, "Bridges\\ScenicGen05", 2, 40, 1);
        LoadFLCAnimation(553, "Bridges\\ScenicGen06", 2, 40, 1);
        LoadFLCAnimation(554, "Bridges\\ScenicGen07", 2, 40, 1);
        LoadFLCAnimation(555, "Bridges\\ScenicGen08", 2, 40, 1);
        LoadFLCAnimation(556, "Bridges\\ScenicGen09", 2, 40, 1);
        LoadFLCAnimation(557, "Bridges\\ScenicGen10", 2, 40, 1);
        LoadFLCAnimation(582, "Tees\\TVTowerStatic", 4, 80, 1);
        LoadFLCAnimation(583, "Tees\\TVTowerANIM", 4, 80, 1);
        LoadFLCAnimation(584, "Tees\\TVCommentStatic", 4, 60, 1);
        LoadFLCAnimation(585, "Tees\\TVCommentANIM", 4, 60, 1);
        LoadFLCAnimation(586, "Tees\\TVScoreCenter", 4, 60, 1);
        LoadFLCAnimation(520, "Scenic\\benW01", 4, 40, 1);
        LoadFLCAnimation(521, "Flowers\\box bench", 4, 40, 1);
        LoadFLCAnimation(522, "Flowers\\red bench", 4, 40, 1);
        LoadFLCAnimation(523, "Flowers\\round wood bench", 4, 40, 1);
        LoadFLCAnimation(524, "Flowers\\backless bench", 4, 40, 1);
        LoadFLCAnimation(525, "Flowers\\lovers bench", 4, 40, 1);
        LoadFLCAnimation(389, "tees\\flagpark_pop", 1, 40, 1);
        LoadFLCAnimation(393, "tees\\flagpark_open", 4, 40, 1);
        LoadFLCAnimation(390, "tees\\flagdesert_pop", 1, 40, 1);
        LoadFLCAnimation(394, "tees\\flagdesert_open", 4, 40, 1);
        LoadFLCAnimation(391, "tees\\flagtrop_pop", 1, 40, 1);
        LoadFLCAnimation(395, "tees\\flagtrop_open", 4, 40, 1);
        LoadFLCAnimation(392, "tees\\flaglinks_pop", 1, 40, 1);
        LoadFLCAnimation(396, "tees\\flaglinks_open", 4, 40, 1);
        LoadFLCAnimation(397, "tees\\TeeMarkerRed_pop", 1, 40, 1);
        LoadFLCAnimation(398, "tees\\TeeMarkerWhite_pop", 1, 40, 1);
        LoadFLCAnimation(399, "tees\\TeeMarkerBlue_pop", 1, 40, 1);
        LoadFLCAnimation(249, "trees\\WillowTree", 4, 1, 1);
        LoadFLCAnimation(388, "flowers\\crabgrass", 4, 0, 1);
        LoadFLCAnimation(387, "bldgs\\upgrade bwasher", 4, 0, 1);
        LoadFLCAnimation(436, "flowers\\Gazebo", 4, 80, 1);
        LoadFLCAnimation(437, "flowers\\Topiary", 4, 80, 1);
        g_bankSlotOffset = 0;
        LoadFLCAnimation(0, "Male\\MalePLS_NormalWalk", 8, 0, 1);
        LoadFLCAnimation(40, "Male\\MalePLS_PerfectSwing", 8, 0, 1);
        LoadFLCAnimation(50, "Male\\MalePLS_NoAccSwing", 8, 0, 1);
        LoadFLCAnimation(55, "Male\\MalePLS_NoimagSwing", 8, 0, 1);
        LoadFLCAnimation(70, "Male\\MalePLS_SitSq", 4, 0, 1);
        LoadFLCAnimation(60, "Male\\MalePLS_Sitting", 4, 0, 1);
        LoadFLCAnimation(80, "Male\\MalePLS_Pitch", 8, 0, 1);
        LoadFLCAnimation(90, "Male\\MalePLS_Putt", 8, 0, 1);
        LoadFLCAnimation(30, "Male\\MalePLS_Happy", 8, 0, 1);
        LoadFLCAnimation(180, "Male\\MalePLS_SuccessA", 8, 0, 1);
        LoadFLCAnimation(190, "Male\\MalePLS_Sad", 8, 0, 1);
        LoadFLCAnimation(200, "Male\\MalePLS_FailureA", 8, 0, 1);
        LoadFLCAnimation(10, "Male\\MalePLS_sq", 8, 0, 1);
        LoadFLCAnimation(100, "Male\\MalePLS_LineUpPutt", 8, 0, 1);
        LoadFLCAnimation(110, "Male\\MalePLS_PointAt", 8, 0, 1);
        LoadFLCAnimation(20, "Male\\MalePLS_Fidget", 8, 0, 1);
        LoadFLCAnimation(210, "Male\\MalePLS_TiredWalk", 8, 0, 1);
        LoadFLCAnimation(120, "Male\\MalePLS_NormalAddress", 8, 0, 1);
        LoadFLCAnimation(140, "Male\\MalePLS_PuttAddress", 8, 0, 1);
        LoadFLCAnimation(130, "Male\\MalePLS_PitchAddress", 8, 0, 1);
        LoadFLCAnimation(160, "Male\\MalePLS_LeanLeft", 8, 0, 1);
        LoadFLCAnimation(170, "Male\\MalePLS_LookUp", 8, 0, 1);
        LoadFLCAnimation(150, "Male\\MalePLS_LeanRight", 8, 0, 1);
        LoadFLCAnimation(220, "Male\\MalePLS_HandShake", 8, 0, 1);
        LoadFLCAnimation(230, "Male\\MalePLS_Cart", 8, 0, 1);
        g_bankSlotOffset = 1;
        LoadFLCAnimation(0, "Male\\MaleKLS_NormalWalk", 8, 0, 1);
        LoadFLCAnimation(40, "Male\\MaleKLS_PerfectSwing", 8, 0, 1);
        LoadFLCAnimation(50, "Male\\MaleKLS_NoAccSwing", 8, 0, 1);
        LoadFLCAnimation(55, "Male\\MaleKLS_NoimagSwing", 8, 0, 1);
        LoadFLCAnimation(70, "Male\\MaleKLS_SitSq", 4, 0, 1);
        LoadFLCAnimation(60, "Male\\MaleKLS_Sitting", 4, 0, 1);
        LoadFLCAnimation(80, "Male\\MaleKLS_Pitch", 8, 0, 1);
        LoadFLCAnimation(90, "Male\\MaleKLS_Putt", 8, 0, 1);
        LoadFLCAnimation(30, "Male\\MaleKLS_Happy", 8, 0, 1);
        LoadFLCAnimation(180, "Male\\MaleKLS_SuccessA", 8, 0, 1);
        LoadFLCAnimation(190, "Male\\MaleKLS_Sad", 8, 0, 1);
        LoadFLCAnimation(200, "Male\\MaleKLS_FailureA", 8, 0, 1);
        LoadFLCAnimation(10, "Male\\MaleKLS_sq", 8, 0, 1);
        LoadFLCAnimation(100, "Male\\MaleKLS_LineUpPutt", 8, 0, 1);
        LoadFLCAnimation(110, "Male\\MaleKLS_PointAt", 8, 0, 1);
        LoadFLCAnimation(20, "Male\\MaleKLS_Fidget", 8, 0, 1);
        LoadFLCAnimation(210, "Male\\MaleKLS_TiredWalk", 8, 0, 1);
        LoadFLCAnimation(120, "Male\\MaleKLS_NormalAddress", 8, 0, 1);
        LoadFLCAnimation(140, "Male\\MaleKLS_PuttAddress", 8, 0, 1);
        LoadFLCAnimation(130, "Male\\MaleKLS_PitchAddress", 8, 0, 1);
        LoadFLCAnimation(160, "Male\\MaleKLS_LeanLeft", 8, 0, 1);
        LoadFLCAnimation(170, "Male\\MaleKLS_LookUp", 8, 0, 1);
        LoadFLCAnimation(150, "Male\\MaleKLS_LeanRight", 8, 0, 1);
        LoadFLCAnimation(220, "Male\\MaleKLS_HandShake", 8, 0, 1);
        LoadFLCAnimation(230, "Male\\MaleKLS_Cart", 8, 0, 1);
        g_bankSlotOffset = 2;
        LoadFLCAnimation(0, "Male\\MalePSS_NormalWalk", 8, 0, 1);
        LoadFLCAnimation(40, "Male\\MalePSS_PerfectSwing", 8, 0, 1);
        LoadFLCAnimation(50, "Male\\MalePSS_NoAccSwing", 8, 0, 1);
        LoadFLCAnimation(55, "Male\\MalePSS_NoimagSwing", 8, 0, 1);
        LoadFLCAnimation(70, "Male\\MalePSS_SitSq", 4, 0, 1);
        LoadFLCAnimation(60, "Male\\MalePSS_Sitting", 4, 0, 1);
        LoadFLCAnimation(80, "Male\\MalePSS_Pitch", 8, 0, 1);
        LoadFLCAnimation(90, "Male\\MalePSS_Putt", 8, 0, 1);
        LoadFLCAnimation(30, "Male\\MalePSS_Happy", 8, 0, 1);
        LoadFLCAnimation(180, "Male\\MalePSS_SuccessA", 8, 0, 1);
        LoadFLCAnimation(190, "Male\\MalePSS_Sad", 8, 0, 1);
        LoadFLCAnimation(200, "Male\\MalePSS_FailureA", 8, 0, 1);
        LoadFLCAnimation(10, "Male\\MalePSS_sq", 8, 0, 1);
        LoadFLCAnimation(100, "Male\\MalePSS_LineUpPutt", 8, 0, 1);
        LoadFLCAnimation(110, "Male\\MalePSS_PointAt", 8, 0, 1);
        LoadFLCAnimation(20, "Male\\MalePSS_Fidget", 8, 0, 1);
        LoadFLCAnimation(210, "Male\\MalePSS_TiredWalk", 8, 0, 1);
        LoadFLCAnimation(120, "Male\\MalePSS_NormalAddress", 8, 0, 1);
        LoadFLCAnimation(140, "Male\\MalePSS_PuttAddress", 8, 0, 1);
        LoadFLCAnimation(130, "Male\\MalePSS_PitchAddress", 8, 0, 1);
        LoadFLCAnimation(160, "Male\\MalePSS_LeanLeft", 8, 0, 1);
        LoadFLCAnimation(170, "Male\\MalePSS_LookUp", 8, 0, 1);
        LoadFLCAnimation(150, "Male\\MalePSS_LeanRight", 8, 0, 1);
        LoadFLCAnimation(220, "Male\\MalePSS_HandShake", 8, 0, 1);
        LoadFLCAnimation(230, "Male\\MalePSS_Cart", 8, 0, 1);
        g_bankSlotOffset = 3;
        LoadFLCAnimation(0, "Male\\MaleSSS_NormalWalk", 8, 0, 1);
        LoadFLCAnimation(40, "Male\\MaleSSS_PerfectSwing", 8, 0, 1);
        LoadFLCAnimation(50, "Male\\MaleSSS_NoAccSwing", 8, 0, 1);
        LoadFLCAnimation(55, "Male\\MaleSSS_NoimagSwing", 8, 0, 1);
        LoadFLCAnimation(70, "Male\\MaleSSS_SitSq", 4, 0, 1);
        LoadFLCAnimation(60, "Male\\MaleSSS_Sitting", 4, 0, 1);
        LoadFLCAnimation(80, "Male\\MaleSSS_Pitch", 8, 0, 1);
        LoadFLCAnimation(90, "Male\\MaleSSS_Putt", 8, 0, 1);
        LoadFLCAnimation(30, "Male\\MaleSSS_Happy", 8, 0, 1);
        LoadFLCAnimation(180, "Male\\MaleSSS_SuccessA", 8, 0, 1);
        LoadFLCAnimation(190, "Male\\MaleSSS_Sad", 8, 0, 1);
        LoadFLCAnimation(200, "Male\\MaleSSS_FailureA", 8, 0, 1);
        LoadFLCAnimation(10, "Male\\MaleSSS_sq", 8, 0, 1);
        LoadFLCAnimation(100, "Male\\MaleSSS_LineUpPutt", 8, 0, 1);
        LoadFLCAnimation(110, "Male\\MaleSSS_PointAt", 8, 0, 1);
        LoadFLCAnimation(20, "Male\\MaleSSS_Fidget", 8, 0, 1);
        LoadFLCAnimation(210, "Male\\MaleSSS_TiredWalk", 8, 0, 1);
        LoadFLCAnimation(120, "Male\\MaleSSS_NormalAddress", 8, 0, 1);
        LoadFLCAnimation(140, "Male\\MaleSSS_PuttAddress", 8, 0, 1);
        LoadFLCAnimation(130, "Male\\MaleSSS_PitchAddress", 8, 0, 1);
        LoadFLCAnimation(160, "Male\\MaleSSS_LeanLeft", 8, 0, 1);
        LoadFLCAnimation(170, "Male\\MaleSSS_LookUp", 8, 0, 1);
        LoadFLCAnimation(150, "Male\\MaleSSS_LeanRight", 8, 0, 1);
        LoadFLCAnimation(220, "Male\\MaleSSS_HandShake", 8, 0, 1);
        LoadFLCAnimation(230, "Male\\MaleSSS_Cart", 8, 0, 1);
        g_bankSlotOffset = 5;
        LoadFLCAnimation(0, "Female\\FemalePLS_NormalWalk", 8, 0, 1);
        LoadFLCAnimation(40, "Female\\FemalePLS_NormalSwing", 8, 0, 1);
        LoadFLCAnimation(70, "Female\\FemalePLS_SitSq", 4, 0, 1);
        LoadFLCAnimation(60, "Female\\FemalePLS_Sitting", 4, 0, 1);
        LoadFLCAnimation(80, "Female\\FemalePLS_Pitch", 8, 0, 1);
        LoadFLCAnimation(90, "Female\\FemalePLS_Putt", 8, 0, 1);
        LoadFLCAnimation(30, "Female\\FemalePLS_Happy", 8, 0, 1);
        LoadFLCAnimation(180, "Female\\FemalePLS_SuccessA", 8, 0, 1);
        LoadFLCAnimation(190, "Female\\FemalePLS_Sad", 8, 0, 1);
        LoadFLCAnimation(200, "Female\\FemalePLS_FailureA", 8, 0, 1);
        LoadFLCAnimation(10, "Female\\FemalePLS_sq", 8, 0, 1);
        LoadFLCAnimation(100, "Female\\FemalePLS_LineUpPutt", 8, 0, 1);
        LoadFLCAnimation(110, "Female\\FemalePLS_PointAt", 8, 0, 1);
        LoadFLCAnimation(20, "Female\\FemalePLS_Fidget", 8, 0, 1);
        LoadFLCAnimation(210, "Female\\FemalePLS_TiredWalk", 8, 0, 1);
        LoadFLCAnimation(120, "Female\\FemalePLS_NormalAddress", 8, 0, 1);
        LoadFLCAnimation(140, "Female\\FemalePLS_PuttAddress", 8, 0, 1);
        LoadFLCAnimation(130, "Female\\FemalePLS_PitchAddress", 8, 0, 1);
        LoadFLCAnimation(160, "Female\\FemalePLS_LeanLeft", 8, 0, 1);
        LoadFLCAnimation(170, "Female\\FemalePLS_LookUp", 8, 0, 1);
        LoadFLCAnimation(150, "Female\\FemalePLS_LeanRight", 8, 0, 1);
        LoadFLCAnimation(220, "Female\\FemalePLS_HandShake", 8, 0, 1);
        LoadFLCAnimation(230, "Female\\FemalePLS_Cart", 8, 0, 1);
        g_bankSlotOffset = 6;
        LoadFLCAnimation(0, "Female\\FemaleSSS_NormalWalk", 8, 0, 1);
        LoadFLCAnimation(40, "Female\\FemaleSSS_NormalSwing", 8, 0, 1);
        LoadFLCAnimation(70, "Female\\FemaleSSS_SitSq", 4, 0, 1);
        LoadFLCAnimation(60, "Female\\FemaleSSS_Sitting", 4, 0, 1);
        LoadFLCAnimation(80, "Female\\FemaleSSS_Pitch", 8, 0, 1);
        LoadFLCAnimation(90, "Female\\FemaleSSS_Putt", 8, 0, 1);
        LoadFLCAnimation(30, "Female\\FemaleSSS_Happy", 8, 0, 1);
        LoadFLCAnimation(180, "Female\\FemaleSSS_SuccessA", 8, 0, 1);
        LoadFLCAnimation(190, "Female\\FemaleSSS_Sad", 8, 0, 1);
        LoadFLCAnimation(200, "Female\\FemaleSSS_FailureA", 8, 0, 1);
        LoadFLCAnimation(10, "Female\\FemaleSSS_sq", 8, 0, 1);
        LoadFLCAnimation(100, "Female\\FemaleSSS_LineUpPutt", 8, 0, 1);
        LoadFLCAnimation(110, "Female\\FemaleSSS_PointAt", 8, 0, 1);
        LoadFLCAnimation(20, "Female\\FemaleSSS_Fidget", 8, 0, 1);
        LoadFLCAnimation(210, "Female\\FemaleSSS_TiredWalk", 8, 0, 1);
        LoadFLCAnimation(120, "Female\\FemaleSSS_NormalAddress", 8, 0, 1);
        LoadFLCAnimation(140, "Female\\FemaleSSS_PuttAddress", 8, 0, 1);
        LoadFLCAnimation(130, "Female\\FemaleSSS_PitchAddress", 8, 0, 1);
        LoadFLCAnimation(160, "Female\\FemaleSSS_LeanLeft", 8, 0, 1);
        LoadFLCAnimation(170, "Female\\FemaleSSS_LookUp", 8, 0, 1);
        LoadFLCAnimation(150, "Female\\FemaleSSS_LeanRight", 8, 0, 1);
        LoadFLCAnimation(220, "Female\\FemaleSSS_HandShake", 8, 0, 1);
        LoadFLCAnimation(230, "Female\\FemaleSSS_Cart", 8, 0, 1);
        g_bankSlotOffset = 7;
        LoadFLCAnimation(0, "Female\\FemalePSS_NormalWalk", 8, 0, 1);
        LoadFLCAnimation(40, "Female\\FemalePSS_NormalSwing", 8, 0, 1);
        LoadFLCAnimation(70, "Female\\FemalePSS_SitSq", 4, 0, 1);
        LoadFLCAnimation(60, "Female\\FemalePSS_Sitting", 4, 0, 1);
        LoadFLCAnimation(80, "Female\\FemalePSS_Pitch", 8, 0, 1);
        LoadFLCAnimation(90, "Female\\FemalePSS_Putt", 8, 0, 1);
        LoadFLCAnimation(30, "Female\\FemalePSS_Happy", 8, 0, 1);
        LoadFLCAnimation(180, "Female\\FemalePSS_SuccessA", 8, 0, 1);
        LoadFLCAnimation(190, "Female\\FemalePSS_Sad", 8, 0, 1);
        LoadFLCAnimation(200, "Female\\FemalePSS_FailureA", 8, 0, 1);
        LoadFLCAnimation(10, "Female\\FemalePSS_sq", 8, 0, 1);
        LoadFLCAnimation(100, "Female\\FemalePSS_LineUpPutt", 8, 0, 1);
        LoadFLCAnimation(110, "Female\\FemalePSS_PointAt", 8, 0, 1);
        LoadFLCAnimation(20, "Female\\FemalePSS_Fidget", 8, 0, 1);
        LoadFLCAnimation(210, "Female\\FemalePSS_TiredWalk", 8, 0, 1);
        LoadFLCAnimation(120, "Female\\FemalePSS_NormalAddress", 8, 0, 1);
        LoadFLCAnimation(140, "Female\\FemalePSS_PuttAddress", 8, 0, 1);
        LoadFLCAnimation(130, "Female\\FemalePSS_PitchAddress", 8, 0, 1);
        LoadFLCAnimation(160, "Female\\FemalePSS_LeanLeft", 8, 0, 1);
        LoadFLCAnimation(170, "Female\\FemalePSS_LookUp", 8, 0, 1);
        LoadFLCAnimation(150, "Female\\FemalePSS_LeanRight", 8, 0, 1);
        LoadFLCAnimation(220, "Female\\FemalePSS_HandShake", 8, 0, 1);
        LoadFLCAnimation(230, "Female\\FemalePSS_Cart", 8, 0, 1);
        g_bankSlotOffset = 8;
        LoadFLCAnimation(0, "Female\\FemaleSkTT_NormalWalk", 8, 0, 1);
        LoadFLCAnimation(40, "Female\\FemaleSkTT_NormalSwing", 8, 0, 1);
        LoadFLCAnimation(70, "Female\\FemaleSkTT_SitSq", 4, 0, 1);
        LoadFLCAnimation(60, "Female\\FemaleSkTT_Sitting", 4, 0, 1);
        LoadFLCAnimation(80, "Female\\FemaleSkTT_Pitch", 8, 0, 1);
        LoadFLCAnimation(90, "Female\\FemaleSkTT_Putt", 8, 0, 1);
        LoadFLCAnimation(30, "Female\\FemaleSkTT_Happy", 8, 0, 1);
        LoadFLCAnimation(180, "Female\\FemaleSkTT_SuccessA", 8, 0, 1);
        LoadFLCAnimation(190, "Female\\FemaleSkTT_Sad", 8, 0, 1);
        LoadFLCAnimation(200, "Female\\FemaleSkTT_FailureA", 8, 0, 1);
        LoadFLCAnimation(10, "Female\\FemaleSkTT_sq", 8, 0, 1);
        LoadFLCAnimation(100, "Female\\FemaleSkTT_LineUpPutt", 8, 0, 1);
        LoadFLCAnimation(110, "Female\\FemaleSkTT_PointAt", 8, 0, 1);
        LoadFLCAnimation(20, "Female\\FemaleSkTT_Fidget", 8, 0, 1);
        LoadFLCAnimation(210, "Female\\FemaleSkTT_TiredWalk", 8, 0, 1);
        LoadFLCAnimation(120, "Female\\FemaleSkTT_NormalAddress", 8, 0, 1);
        LoadFLCAnimation(140, "Female\\FemaleSkTT_PuttAddress", 8, 0, 1);
        LoadFLCAnimation(130, "Female\\FemaleSkTT_PitchAddress", 8, 0, 1);
        LoadFLCAnimation(160, "Female\\FemaleSkTT_LeanLeft", 8, 0, 1);
        LoadFLCAnimation(170, "Female\\FemaleSkTT_LookUp", 8, 0, 1);
        LoadFLCAnimation(150, "Female\\FemaleSkTT_LeanRight", 8, 0, 1);
        LoadFLCAnimation(220, "Female\\FemaleSkTT_HandShake", 8, 0, 1);
        LoadFLCAnimation(230, "Female\\FemaleSkTT_Cart", 8, 0, 1);
        g_bankSlotOffset = 0;
        LoadFLCAnimation(526, "employee\\GreeterWalk", 8, 0, 1);
        LoadFLCAnimation(534, "employee\\GreeterSQ", 8, 0, 1);
        LoadFLCAnimation(542, "employee\\GreeterAction2", 8, 0, 1);
        g_bankSlotOffset = 1;
        LoadFLCAnimation(526, "employee\\RangerWalk", 8, 0, 1);
        LoadFLCAnimation(534, "employee\\RangerSQ", 8, 0, 1);
        LoadFLCAnimation(542, "employee\\RangerAction", 8, 0, 1);
        g_bankSlotOffset = 2;
        LoadFLCAnimation(526, "employee\\GKWalk", 8, 0, 1);
        LoadFLCAnimation(534, "employee\\GKSq", 8, 0, 1);
        LoadFLCAnimation(542, "employee\\GKAction", 8, 0, 1);
        g_bankSlotOffset = 3;
        LoadFLCAnimation(526, "employee\\TrayGirl_Walk", 8, 0, 1);
        LoadFLCAnimation(534, "employee\\TrayGirl_SQ", 8, 0, 1);
        LoadFLCAnimation(542, "employee\\TrayGirl_Action", 8, 0, 1);
        g_bankSlotOffset = 4;
        LoadFLCAnimation(526, "employee\\GolfCeleb_Walk", 8, 0, 1);
        LoadFLCAnimation(534, "employee\\GolfCeleb_SQ", 8, 0, 1);
        LoadFLCAnimation(542, "employee\\GolfCeleb_Action", 8, 0, 1);
        g_bankSlotOffset = 5;
        LoadFLCAnimation(526, "employee\\Marshall_Walk", 8, 0, 1);
        LoadFLCAnimation(534, "employee\\Marshall_SQ", 8, 0, 1);
        LoadFLCAnimation(542, "employee\\Marshall_Action", 8, 0, 1);
        g_bankSlotOffset = 6;
        LoadFLCAnimation(526, "employee\\LawnTech_Walk", 8, 0, 1);
        LoadFLCAnimation(534, "employee\\LawnTech_Sq", 8, 0, 1);
        LoadFLCAnimation(542, "employee\\LawnTech_Action", 8, 0, 1);
        g_bankSlotOffset = 7;
        LoadFLCAnimation(526, "employee\\SodaVendorWalk", 8, 0, 1);
        LoadFLCAnimation(534, "employee\\SodaVendorSQ", 8, 0, 1);
        LoadFLCAnimation(542, "employee\\SodaVendorAction", 8, 0, 1);
        g_bankSlotOffset = 0;
        LoadFLCAnimation(259, "animals\\ELK_run", 8, 0, 1);
        LoadFLCAnimation(268, "animals\\ELK_SQ", 4, 0, 1);
        LoadFLCAnimation(277, "animals\\ELK_EatDown", 4, 0, 1);
        LoadFLCAnimation(286, "animals\\ELK_EatLoop", 4, 0, 1);
        g_bankSlotOffset = 1;
        LoadFLCAnimation(259, "animals\\Crane_Walk", 8, 0, 1);
        LoadFLCAnimation(268, "animals\\Crane_SQ", 4, 0, 1);
        LoadFLCAnimation(277, "animals\\Crane_DrinkDown", 4, 0, 1);
        LoadFLCAnimation(286, "animals\\Crane_DrinkLoop", 4, 0, 1);
        g_bankSlotOffset = 2;
        LoadFLCAnimation(259, "animals\\Flamingo_Walk", 8, 0, 1);
        LoadFLCAnimation(268, "animals\\Flamingo_SQ", 4, 0, 1);
        LoadFLCAnimation(277, "animals\\Flamingo_DrinkDown", 4, 0, 1);
        LoadFLCAnimation(286, "animals\\Flamingo_DrinkLoop", 4, 0, 1);
        g_bankSlotOffset = 3;
        LoadFLCAnimation(259, "animals\\Sheep_Walk", 8, 0, 1);
        LoadFLCAnimation(268, "animals\\Sheep_SQ", 4, 0, 1);
        LoadFLCAnimation(277, "animals\\Sheep_EatDown", 4, 0, 1);
        LoadFLCAnimation(286, "animals\\Sheep_EatLoop", 4, 0, 1);
        g_bankSlotOffset = 4;
        LoadFLCAnimation(259, "animals\\Croc_Walk", 8, 0, 1);
        LoadFLCAnimation(268, "animals\\Croc_SQ", 8, 0, 1);
        LoadFLCAnimation(277, "animals\\Croc_Turnover", 8, 0, 1);
        LoadFLCAnimation(286, "animals\\Croc_BackScratch", 8, 0, 1);
        g_bankSlotOffset = 5;
        LoadFLCAnimation(259, "animals\\Snake_Walk", 8, 0, 1);
        LoadFLCAnimation(268, "animals\\Snake_SQ", 4, 0, 1);
        LoadFLCAnimation(277, "animals\\Snake_SQ", 4, 0, 1);
        LoadFLCAnimation(286, "animals\\Snake_Fidget", 4, 0, 1);
        g_bankSlotOffset = 6;
        LoadFLCAnimation(259, "animals\\Gila_Walk", 8, 0, 1);
        LoadFLCAnimation(268, "animals\\Gila_SQ", 4, 0, 1);
        LoadFLCAnimation(277, "animals\\Gila_SQ", 4, 0, 1);
        LoadFLCAnimation(286, "animals\\Gila_Fidget", 4, 0, 1);
        g_bankSlotOffset = 7;
        LoadFLCAnimation(259, "animals\\RR_Walk", 8, 0, 1);
        LoadFLCAnimation(268, "animals\\RR_SQ", 4, 0, 1);
        LoadFLCAnimation(277, "animals\\RR_DrinkDown", 4, 0, 1);
        LoadFLCAnimation(286, "animals\\RR_DrinkLoop", 4, 0, 1);
        g_bankSlotOffset = 8;
        LoadFLCAnimation(259, "animals\\Duck_Walk", 8, 0, 1);
        LoadFLCAnimation(268, "animals\\Duck_SQ", 4, 0, 1);
        LoadFLCAnimation(277, "animals\\Duck_DrinkDown", 4, 0, 1);
        LoadFLCAnimation(286, "animals\\Duck_DrinkLoop", 4, 0, 1);
        g_bankSlotOffset = 0;
        LoadFLCAnimation(320, "celebs\\ActionStar_SQ", 8, 0, 1);
        LoadFLCAnimation(346, "celebs\\ActionStar_Walk", 8, 0, 1);
        LoadFLCAnimation(333, "celebs\\ActionStar_Char", 8, 0, 1);
        g_bankSlotOffset = 1;
        LoadFLCAnimation(320, "celebs\\Female_PopSinger_SQ", 8, 0, 1);
        LoadFLCAnimation(346, "celebs\\Female_PopSinger_Walk", 8, 0, 1);
        LoadFLCAnimation(333, "celebs\\Female_PopSinger_Char", 8, 0, 1);
        g_bankSlotOffset = 2;
        LoadFLCAnimation(320, "celebs\\Politician_SQ", 8, 0, 1);
        LoadFLCAnimation(346, "celebs\\Politician_Walk", 8, 0, 1);
        LoadFLCAnimation(333, "celebs\\Politician_Char", 8, 0, 1);
        g_bankSlotOffset = 3;
        LoadFLCAnimation(320, "celebs\\Comedian_SQ", 8, 0, 1);
        LoadFLCAnimation(346, "celebs\\Comedian_Walk", 8, 0, 1);
        LoadFLCAnimation(333, "celebs\\Comedian_Char", 8, 0, 1);
        g_bankSlotOffset = 4;
        LoadFLCAnimation(320, "celebs\\Supermodel_SQ", 8, 0, 1);
        LoadFLCAnimation(346, "celebs\\Supermodel_Walk", 8, 0, 1);
        LoadFLCAnimation(333, "celebs\\Supermodel_Char", 8, 0, 1);
        g_bankSlotOffset = 5;
        LoadFLCAnimation(320, "celebs\\FitnessFem_SQ", 8, 0, 1);
        LoadFLCAnimation(346, "celebs\\FitnessFem_Walk", 8, 0, 1);
        LoadFLCAnimation(333, "celebs\\FitnessFem_Char", 8, 0, 1);
        g_bankSlotOffset = 6;
        LoadFLCAnimation(320, "celebs\\FemComic_SQ", 8, 0, 1);
        LoadFLCAnimation(346, "celebs\\FemComic_Walk", 8, 0, 1);
        LoadFLCAnimation(333, "celebs\\FemComic_Char", 8, 0, 1);
        g_bankSlotOffset = 7;
        LoadFLCAnimation(320, "celebs\\GenMale_SQ", 8, 0, 1);
        LoadFLCAnimation(346, "celebs\\GenMale_Walk", 8, 0, 1);
        LoadFLCAnimation(333, "celebs\\GenMale_Char", 8, 0, 1);
        g_bankSlotOffset = 8;
        LoadFLCAnimation(320, "celebs\\MoviePrincess_SQ", 8, 0, 1);
        LoadFLCAnimation(346, "celebs\\MoviePrincess_Walk", 8, 0, 1);
        LoadFLCAnimation(333, "celebs\\MoviePrincess_Char", 8, 0, 1);
        g_bankSlotOffset = 9;
        LoadFLCAnimation(320, "celebs\\RockStar_SQ", 8, 0, 1);
        LoadFLCAnimation(346, "celebs\\RockStar_Walk", 8, 0, 1);
        LoadFLCAnimation(333, "celebs\\RockStar_Char", 8, 0, 1);
        g_bankSlotOffset = 10;
        LoadFLCAnimation(320, "celebs\\Basketball_SQ", 8, 0, 1);
        LoadFLCAnimation(346, "celebs\\Basketball_Walk", 8, 0, 1);
        LoadFLCAnimation(333, "celebs\\Basketball_Char", 8, 0, 1);
        g_bankSlotOffset = 11;
        LoadFLCAnimation(320, "celebs\\AgingStar_SQ", 8, 0, 1);
        LoadFLCAnimation(346, "celebs\\AgingStar_Walk", 8, 0, 1);
        LoadFLCAnimation(333, "celebs\\AgingStar_Char", 8, 0, 1);
        g_bankSlotOffset = 0;
        LoadFLCAnimation(310, "homes\\Sum_home", 4, 1, 1);
        LoadFLCAnimation(316, "homes\\Sum_home_dirt", 4, 1, 1);
        g_bankSlotOffset = 1;
        LoadFLCAnimation(310, "homes\\political_home", 4, 1, 1);
        LoadFLCAnimation(316, "homes\\political_home_dirt", 4, 1, 1);
        g_bankSlotOffset = 2;
        LoadFLCAnimation(310, "homes\\ArtDeco_Home", 4, 1, 1);
        LoadFLCAnimation(316, "homes\\ArtDeco_Home_dirt", 4, 1, 1);
        g_bankSlotOffset = 3;
        LoadFLCAnimation(310, "homes\\DesertCelebHouse", 4, 1, 1);
        g_bankSlotOffset = 4;
        LoadFLCAnimation(310, "homes\\Summer1stCelebHouse", 4, 1, 1);
        g_bankSlotOffset = 5;
        LoadFLCAnimation(310, "homes\\Summer2ndCelebHouse", 4, 1, 1);
        surf.load_pcx("interface\\golfballhalopage_male .pcx", 0, 0, PALETTE_COLORS, 2);

        for (k = 0; k < 0x13; k++) {
            g_playerHeadBmpArray[k * 3 + 216].extract(&surf, ((k) / 2) * 0x8c, ((k) & 1) * 0x1a4, 0x8c, 0x8c, 1, 1);
            g_playerHeadBmpArray[k * 3 + 217].extract(&surf, ((k) / 2) * 0x8c, ((k) & 1) * 0x1a4 + 0x8c, 0x8c, 0x8c, 1,
                1);
            g_playerHeadBmpArray[k * 3 + 218].extract(&surf, ((k) / 2) * 0x8c, ((k) & 1) * 0x1a4 + 0x118, 0x8c, 0x8c, 1,
                1);
        }
        surf.load_pcx("heads\\golfballhalopage_female.pcx", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 0x13; k++) {
            g_playerHeadBmpArray[k * 3].extract(&surf, ((k) / 2) * 0x8c, ((k) & 1) * 0x1a4, 0x8c, 0x8c, 1, 1);
            g_playerHeadBmpArray[k * 3 + 1].extract(&surf, ((k) / 2) * 0x8c, ((k) & 1) * 0x1a4 + 0x8c, 0x8c, 0x8c, 1,
                1);
            g_playerHeadBmpArray[k * 3 + 2].extract(&surf, ((k) / 2) * 0x8c, ((k) & 1) * 0x1a4 + 0x118, 0x8c, 0x8c, 1,
                1);
        }
        surf.load_pcx("heads\\sim_female_all_expressionsflat", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 19; k++) {
            int y = (k & 1) * 0x174;
            int x = (k / 2) * 0x64;
            g_allExpressionHeads[k * 3].extract(&surf, x, y + 4, 0x5a, 0x78, 1, 1);
            g_allExpressionHeads[k * 3 + 1].extract(&surf, x, y + 0x80, 0x5a, 0x78, 1, 1);
            g_allExpressionHeads[k * 3 + 2].extract(&surf, x, y + 0xfc, 0x5a, 0x78, 1, 1);
        }
        surf.load_pcx("heads\\sim_male_all_expressionsflat", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 19; k++) {
            int y = (k & 1) * 0x174;
            int x = (k / 2) * 0x64;
            g_allExpressionHeads[k * 3 + 60].extract(&surf, x, y + 4, 0x5a, 0x78, 1, 1);
            g_allExpressionHeads[k * 3 + 61].extract(&surf, x, y + 0x80, 0x5a, 0x78, 1, 1);
            g_allExpressionHeads[k * 3 + 62].extract(&surf, x, y + 0xfc, 0x5a, 0x78, 1, 1);
        }
        nfiles = FindFilesWithFilter("heads\\*.pcx", g_empty, g_empty);
        g_numHeads[0] = 0x13;
        for (i = 0; i < nfiles; i++) {
            if (g_numHeads[0] >= 0x48)
                break;
            if (g_fileList[i][0] == 'F' || g_fileList[i][0] == 'f') {
                strcpy(g_printBuffer, "heads\\");
                strcat(g_printBuffer, g_fileList[i]);
                surf.load_pcx(g_printBuffer, 0, 0, PALETTE_COLORS, 2);
                g_playerHeadBmpArray[g_numHeads[0] * 3].extract(&surf, 0, 0, 0x8c, 0x8c, 1, 1);
                g_playerHeadBmpArray[g_numHeads[0] * 3 + 1].extract(&surf, 0, 0x8c, 0x8c, 0x8c, 1, 1);
                g_playerHeadBmpArray[g_numHeads[0] * 3 + 2].extract(&surf, 0, 0x118, 0x8c, 0x8c, 1, 1);
                g_numHeads[0]++;
            }
        }
        nfiles = FindFilesWithFilter("heads\\*.pcx", g_empty, g_empty);
        g_numHeads[1] = 0x13;
        for (i = 0; i < nfiles; i++) {
            if (g_numHeads[1] >= 0x48)
                break;
            if (g_fileList[i][0] == 'M' || g_fileList[i][0] == 'm') {
                strcpy(g_printBuffer, "heads\\");
                strcat(g_printBuffer, g_fileList[i]);
                surf.load_pcx(g_printBuffer, 0, 0, PALETTE_COLORS, 2);
                g_playerHeadBmpArray[g_numHeads[1] * 3 + 216].extract(&surf, 0, 0, 0x8c, 0x8c, 1, 1);
                g_playerHeadBmpArray[g_numHeads[1] * 3 + 217].extract(&surf, 0, 0x8c, 0x8c, 0x8c, 1, 1);
                g_playerHeadBmpArray[g_numHeads[1] * 3 + 218].extract(&surf, 0, 0x118, 0x8c, 0x8c, 1, 1);
                g_numHeads[1]++;
            }
        }
        surf.load_pcx("flics\\homes\\stone_patio_tile.pcx", 0, 0, PALETTE_COLORS, 2);
        g_homePlotGroundPics[0].extract(&surf, 224, 229, 32, 22, 1, 1);
        surf.load_pcx("flics\\homes\\concrete_tile.pcx", 0, 0, PALETTE_COLORS, 2);
        g_homePlotGroundPics[1].extract(&surf, 224, 229, 32, 22, 1, 1);
        surf.load_pcx("flics\\homes\\dirt_tile.pcx", 0, 0, PALETTE_COLORS, 2);
        g_homePlotGroundPics[2].extract(&surf, 224, 229, 32, 22, 1, 1);
        surf.load_pcx("flics\\homes\\concrete_woodborder_tile.pcx", 0, 0, PALETTE_COLORS, 2);
        g_homePlotGroundPics[3].extract(&surf, 224, 229, 32, 22, 1, 1);
        surf.load_pcx("flics\\homes\\fence.pcx", 0, 0, PALETTE_COLORS, 2);
        g_homePlotGroundPics[4].extract(&surf, 0, 0, 32, 32, 1, 1);
        g_homePlotGroundPics[5].extract(&surf, 32, 0, 32, 32, 1, 1);
        surf.load_pcx("gbubbles.pcx", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 0x28; k++)
            g_bubblePieces[k].extract(&surf, k * 0x10, 0x144, 0x10, 0x10, 1, 1);
        surf.load_pcx("interface\\PopUpIcons.pcx", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 5; k++)
            g_messageIconPics[k].extract(&surf, k * 0x5a, 0, 0x5a, 0x50, 1, 0);
        surf.load_pcx("interface\\PopUpIcons_A.pcx", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 5; k++)
            g_speakerIconsAlpha[k].extract(&surf, k * 0x5a, 0, 0x5a, 0x50, 1, 0);
        for (k = 0; k < 8; k++) {
            switch (k) {
            case 0: surf.load_pcx("bodies\\FemalePLS.pcx", 0, 0, PALETTE_COLORS, 2); break;
            case 1: surf.load_pcx("bodies\\FemaleSSS.pcx", 0, 0, PALETTE_COLORS, 2); break;
            case 2: surf.load_pcx("bodies\\FemalePSS.pcx", 0, 0, PALETTE_COLORS, 2); break;
            case 3: surf.load_pcx("bodies\\FemaleSkTT.pcx", 0, 0, PALETTE_COLORS, 2); break;
            case 4: surf.load_pcx("bodies\\FemalePLS_sm.pcx", 0, 0, PALETTE_COLORS, 2); break;
            case 5: surf.load_pcx("bodies\\FemaleSSS_sm.pcx", 0, 0, PALETTE_COLORS, 2); break;
            case 6: surf.load_pcx("bodies\\FemalePSS_sm.pcx", 0, 0, PALETTE_COLORS, 2); break;
            case 7: surf.load_pcx("bodies\\FemaleSkTT_sm.pcx", 0, 0, PALETTE_COLORS, 2); break;
            }
            g_gameGolferBodyGfx[k].extract(&surf, 0, 0, 0x3c, 0x78, 1, 1);
        }
        for (k = 0; k < 8; k++) {
            switch (k) {
            case 0: surf.load_pcx("bodies\\MalePLS.pcx", 0, 0, PALETTE_COLORS, 2); break;
            case 1: surf.load_pcx("bodies\\MaleKLS.pcx", 0, 0, PALETTE_COLORS, 2); break;
            case 2: surf.load_pcx("bodies\\MalePSS.pcx", 0, 0, PALETTE_COLORS, 2); break;
            case 3: surf.load_pcx("bodies\\MaleSSS.pcx", 0, 0, PALETTE_COLORS, 2); break;
            case 4: surf.load_pcx("bodies\\MalePLS_sm.pcx", 0, 0, PALETTE_COLORS, 2); break;
            case 5: surf.load_pcx("bodies\\MaleKLS_sm.pcx", 0, 0, PALETTE_COLORS, 2); break;
            case 6: surf.load_pcx("bodies\\MalePSS_sm.pcx", 0, 0, PALETTE_COLORS, 2); break;
            case 7: surf.load_pcx("bodies\\MaleSSS_sm.pcx", 0, 0, PALETTE_COLORS, 2); break;
            }
            g_gameGolferBodyGfx[k + 8].extract(&surf, 0, 0, 0x3c, 0x78, 1, 1);
        }
        surf.load_pcx("bodies\\barrel.pcx", 0, 0, PALETTE_COLORS, 2);
        g_juniorsBarrelArt.extract(&surf, 0, 0, 60, 120, 1, 1);
        surf.load_pcx("bldg.pcx", 0, 0, PALETTE_COLORS, 2);
        g_tinyBallPicture.extract(&surf, GOLF_BALL_PIC_X, 0, GOLF_BALL_PIC_SIZE, GOLF_BALL_PIC_SIZE, 1, 1);
        g_groundShadowPixel.extract(&surf, 192, 2, 2, 2, 1, 1);
        g_bigGolfBallDot.extract(&surf, 196, 0, 4, 4, 1, 1);
        pts[0] = 1;
        pts[1] = 0x13;
        pts[2] = 0x2c;
        pts[3] = 0x52;
        pts[4] = 0x71;
        pts[5] = 0x8e;
        pts[6] = 0xa1;
        pts[7] = 0xae;
        surf.load_pcx("cliffs01.pcx", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 4; k++) {
            int krow = k * 0x3c;
            for (i = 1; i < 8; i++) {
                int xbase = 0x21a;
                int row = 0xc;
                if (!(i & 1))
                    row = 0x105;
                if (i > 1)
                    xbase = 0x21a - (i / 2) * 179;
                row += krow;
                for (m = 0; m < 7; m++)
                    g_courseCliffArt[k * 56 + i * 7 + m].extract(&surf, xbase + pts[m], row + 1,
                        pts[m + 1] - pts[m] - 1, 0x2e, 1, 1);
            }
        }
        for (k = 0; k < 4; k++) {
            switch (k) {
            case THEME_PARK: surf.load_pcx("data\\parkland.pcx", 0, 0, PALETTE_COLORS, 2); break;
            case THEME_DESERT: surf.load_pcx("data\\desert.pcx", 0, 0, PALETTE_COLORS, 2); break;
            case THEME_LINKS: surf.load_pcx("data\\links.pcx", 0, 0, PALETTE_COLORS, 2); break;
            case THEME_TROPICAL: surf.load_pcx("data\\tropical.pcx", 0, 0, PALETTE_COLORS, 2); break;
            }
            for (i = 0; i < 0x14; i++)
                g_themePaintImgs[k * 20 + i].extract(&surf, (i & 7) * 0x39, (i / 8) * 0x28, 0x39, 0x28, 1, 1);
        }
        surf.load_pcx("course1.pcx", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 0xc; k++)
            g_courseBigSprs[k].extract(&surf, k * 0x40, 0x140, 0x40, 0x50, 1, 1);
        for (k = 0; k < 0x12; k++)
            g_simHoleMarkArt[k + 54].extract(&surf, k * 0x10, 0x1bb, 0x10, 0x1e, 1, 1);
        g_speechStripTile.extract(&surf, 512, 128, 32, 10, 1, 1);
        g_talkBubblePointSpr.extract(&surf, 512, 140, 8, 8, 1, 1);
        g_selectHaloSprite.extract(&surf, 549, 215, 43, 21, 1, 1);
        surf.load_pcx("interface\\TrophyParts", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 0xb; k++)
            g_trophyPartCells[k].extract(&surf, 9, 0xdd + k * 0x23, 0xf8, 0x1d, 1, 0);
        for (k = 0; k < 0xb; k++)
            g_trophyPartCells[k + 11].extract(&surf, 0x223, 0xdd + k * 0x23, 0xf8, 0x1d, 1, 0);
        g_trophyPartCells[22].extract(&surf, 283, 357, 248, 146, 1, 0);
        g_trophyPartCells[23].extract(&surf, 276, 292, 257, 47, 1, 0);
        g_trophyPartCells[24].extract(&surf, 317, 490, 183, 52, 1, 0);
        g_trophyPartCells[25].extract(&surf, 9, 186, 248, 19, 1, 0);
        g_trophyPartCells[26].extract(&surf, 9, 116, 248, 30, 1, 0);
        g_trophyPartCells[27].extract(&surf, 510, 13, 132, 162, 1, 0);
        g_trophyPartCells[28].extract(&surf, 649, 13, 130, 163, 1, 0);
        for (k = 0; k < 3; k++)
            g_trophyPartCells[k + 29].extract(&surf, 0x119, 0xba + k * 0x23, 0xf8, 0x1e, 1, 0);
        surf.load_pcx("interface\\tacs&tees", 0, 0, PALETTE_COLORS, 2);
        g_trophyPartCells[32].extract(&surf, 668, 511, 53, 52, 1, 0);
        g_trophyPartCells[33].extract(&surf, 63, 251, 229, 209, 1, 0);
        g_trophyPartCells[34].extract(&surf, 58, 49, 207, 178, 1, 0);
        g_trophyPartCells[35].extract(&surf, 51, 482, 184, 80, 1, 0);
        g_trophyPartCells[36].extract(&surf, 732, 511, 53, 52, 1, 0);
        for (k = 0; k < 0x16; k++)
            g_trophyPartCells[k + 38].extract(&surf, g_trophyPartPos[k][0], g_trophyPartPos[k][1],
                g_trophyPartSize[k][0], g_trophyPartSize[k][1], 1, 0);
        for (k = 0; k < 3; k++)
            g_trophyPartCells[k + 67].extract(&surf, g_trophyPartPos[k + 22][0], g_trophyPartPos[k + 22][1],
                g_trophyPartSize[k + 22][0], g_trophyPartSize[k + 22][1], 1, 0);
        surf.load_pcx("interface\\TrophyParts_A", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 0xb; k++)
            g_trophiesMasksBmps[k].extract(&surf, 9, 0xdd + k * 0x23, 0xf8, 0x1d, 1, 0);
        for (k = 0; k < 0xb; k++)
            g_trophiesMasksBmps[k + 11].extract(&surf, 0x223, 0xdd + k * 0x23, 0xf8, 0x1d, 1, 0);
        g_trophiesMasksBmps[22].extract(&surf, 283, 357, 248, 146, 1, 0);
        g_trophiesMasksBmps[23].extract(&surf, 276, 292, 257, 47, 1, 0);
        g_trophiesMasksBmps[24].extract(&surf, 317, 490, 183, 52, 1, 0);
        g_trophiesMasksBmps[25].extract(&surf, 9, 186, 248, 19, 1, 0);
        g_trophiesMasksBmps[26].extract(&surf, 9, 116, 248, 30, 1, 0);
        g_trophiesMasksBmps[27].extract(&surf, 510, 13, 132, 162, 1, 0);
        g_trophiesMasksBmps[28].extract(&surf, 649, 13, 130, 163, 1, 0);
        for (k = 0; k < 3; k++)
            g_trophiesMasksBmps[k + 29].extract(&surf, 0x11a, 0xba + k * 0x23, 0xf8, 0x1e, 1, 0);
        surf.load_pcx("interface\\tacs&tees_A", 0, 0, PALETTE_COLORS, 2);
        g_trophiesMasksBmps[36].extract(&surf, 732, 511, 53, 52, 1, 0);
        g_trophiesMasksBmps[32].extract(&surf, 668, 511, 53, 52, 1, 0);
        g_trophiesMasksBmps[33].extract(&surf, 63, 251, 229, 209, 1, 0);
        g_trophiesMasksBmps[34].extract(&surf, 58, 49, 207, 178, 1, 0);
        g_trophiesMasksBmps[35].extract(&surf, 51, 482, 184, 80, 1, 0);
        for (k = 0; k < 0x16; k++)
            g_trophiesMasksBmps[k + 38].extract(&surf, g_trophyPartPos[k][0], g_trophyPartPos[k][1],
                g_trophyPartSize[k][0], g_trophyPartSize[k][1], 1, 0);
        for (k = 0; k < 3; k++)
            g_trophiesMasksBmps[k + 67].extract(&surf, g_trophyPartPos[k + 22][0], g_trophyPartPos[k + 22][1],
                g_trophyPartSize[k + 22][0], g_trophyPartSize[k + 22][1], 1, 0);
        surf.load_pcx("interface\\InfoButtons", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 0xa; k++)
            for (j = 0; j < 3; j++)
                g_infoBtnImages[k * 3 + j].extract(&surf, j * 0x32, k * 0x32, 0x2e, 0x28, 1, 0);
        for (k = 0; k < 3; k++)
            for (j = 0; j < 3; j++)
                g_infoBtnImages[k * 3 + j + 30].extract(&surf, 0xc8 + j * 0x11, k * 0x11, 0x10, 0x10, 1, 0);
        for (k = 0; k < 3; k++)
            g_checkBoxFrames[k].extract(&surf, 0x12c + k * 0x32, 0, 0x1a, 0x1a, 1, 1);
        surf.load_pcx("interface\\InfoButtons_A", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 0xa; k++)
            for (j = 0; j < 3; j++)
                g_infoButtonBitmapMask[k * 3 + j].extract(&surf, j * 0x32, k * 0x32, 0x2e, 0x28, 1, 0);
        for (k = 0; k < 3; k++)
            for (j = 0; j < 3; j++)
                g_infoButtonBitmapMask[k * 3 + j + 30].extract(&surf, 0xc8 + j * 0x11, k * 0x11, 0x10, 0x10, 1, 0);
        surf.load_pcx("interface\\s_TransPopups", 0, 0, PALETTE_COLORS, 2);
        g_popupEdgeBmps[9].extract(&surf, 317, 0, 16, 16, 1, 1);
        for (k = 0; k < 3; k++)
            for (j = 0; j < 3; j++)
                g_s_transPopUpSprites[k * 3 + j].extract(&surf, j * 0x11, k * 0x11, 0x10, 0x10, 1, 1);
        surf.load_pcx("interface\\TransPopups", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 3; k++)
            for (j = 0; j < 3; j++)
                g_popupEdgeBmps[k * 3 + j].extract(&surf, j * 0x11, k * 0x11, 0x10, 0x10, 1, 0);
        for (k = 0; k < 3; k++) {
            g_transPopUpButtonBmps[k + 4].extract(&surf, 0x12c + k * 0x32, 0x8c, 0x32, 0x32, 1, 0);
            g_transPopUpButtonBmps[k].extract(&surf, 0x12c + k * 0x32, 0x12c, 0x1e, 0x1e, 1, 0);
        }
        g_popupEdgeBmps[30].extract(&surf, 0, 150, 40, 24, 1, 0);
        g_popupEdgeBmps[31].extract(&surf, 0, 200, 35, 12, 1, 0);
        g_popupEdgeBmps[32].extract(&surf, 0, 220, 35, 12, 1, 0);
        for (k = 0; k < 3; k++)
            g_popupEdgeBmps[k + 12].extract(&surf, 0x12c + k * 0x11, 0x22, 0x10, 0x10, 1, 0);
        for (k = 0; k < 3; k++)
            g_popupEdgeBmps[k + 15].extract(&surf, 0x168 + k * 0x28, 0x22, 0x1e, 0x2a, 1, 0);
        for (k = 0; k < 3; k++)
            g_popupEdgeBmps[k + 18].extract(&surf, k * 0x11, 0x3c, 0x10, 0x13, 1, 0);
        for (k = 0; k < 3; k++)
            g_popupEdgeBmps[k + 21].extract(&surf, 0x12c + k * 0x11, 0x22, 0x10, 0x10, 1, 0);
        g_popupEdgeBmps[26].extract(&surf, 32, 100, 191, 24, 1, 0);
        g_popupEdgeBmps[PANEL_PORTRAIT_FRAME].extract(&surf, 0, 300, 140, 140, 1, 0);
        for (k = 0; k < 2; k++)
            for (i = 0; i < 2; i++)
                g_popupEdgeBmps[k * 3 + i + 24].extract(&surf, 0, 0xc8 + k * 0x32 + i * 0x14, 0x23, 0xc, 1, 0);
        surf.load_pcx("interface\\TransPopups_A", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 3; k++)
            for (j = 0; j < 3; j++)
                g_transPopupCellsAlpha[k * 3 + j].extract(&surf, j * 0x11, k * 0x11, 0x10, 0x10, 1, 0);
        for (k = 0; k < 3; k++) {
            g_transPopUpMaskSprites[k + 4].extract(&surf, 0x12c + k * 0x32, 0x8c, 0x32, 0x32, 1, 0);
            g_transPopUpMaskSprites[k].extract(&surf, 0x12c + k * 0x32, 0x12c, 0x1e, 0x1e, 1, 0);
        }
        g_transPopupCellsAlpha[30].extract(&surf, 0, 150, 40, 24, 1, 0);
        g_transPopupCellsAlpha[31].extract(&surf, 0, 200, 35, 12, 1, 0);
        g_transPopupCellsAlpha[32].extract(&surf, 0, 220, 35, 12, 1, 0);
        for (k = 0; k < 3; k++)
            g_transPopupCellsAlpha[k + 12].extract(&surf, 0x12c + k * 0x11, 0x22, 0x10, 0x10, 1, 0);
        for (k = 0; k < 3; k++)
            g_transPopupCellsAlpha[k + 15].extract(&surf, 0x168 + k * 0x28, 0x22, 0x1e, 0x2a, 1, 0);
        for (k = 0; k < 3; k++)
            g_transPopupCellsAlpha[k + 18].extract(&surf, k * 0x11, 0x3c, 0x10, 0x13, 1, 0);
        for (k = 0; k < 3; k++)
            g_transPopupCellsAlpha[k + 21].extract(&surf, 0x12c + k * 0x11, 0x22, 0x10, 0x10, 1, 0);
        g_transPopupCellsAlpha[26].extract(&surf, 32, 100, 191, 24, 1, 0);
        g_transPopupCellsAlpha[PANEL_PORTRAIT_FRAME].extract(&surf, 0, 300, 140, 140, 1, 0);
        for (k = 0; k < 2; k++)
            for (i = 0; i < 2; i++)
                g_transPopupCellsAlpha[k * 3 + i + 24].extract(&surf, 0, 0xc8 + k * 0x32 + i * 0x14, 0x23, 0xc, 1, 0);
        surf.load_pcx("interface\\tropdesert", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 8; k++)
            g_siteMapSprs[k].extract(&surf, 3 + k * 0x58, k >= 4 ? 0x1e0 : 0x1e1, 0x50, 0x50, 1, 0);
        surf.load_pcx("interface\\tropdesert_A", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 8; k++)
            g_regionAlphaFrames[k].extract(&surf, 3 + k * 0x58, k >= 4 ? 0x1e0 : 0x1e1, 0x50, 0x50, 1, 0);
        surf.load_pcx("interface\\parklink", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 8; k++)
            g_siteMapSprs[k + 8].extract(&surf, 3 + k * 0x58, 0x18b, 0x50, 0x50, 1, 0);
        surf.load_pcx("interface\\parklink_A", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 8; k++)
            g_regionAlphaFrames[k + 8].extract(&surf, 3 + k * 0x58, 0x18b, 0x50, 0x50, 1, 0);
        surf.load_pcx("interface\\courseinfo", 0, 0, PALETTE_COLORS, 2);
        g_simTopBoxes[0].extract(&surf, 48, 6, 183, 58, 1, 0);
        g_simTopBoxes[1].extract(&surf, 647, 13, 138, 36, 1, 0);
        g_simTopBoxes[2].extract(&surf, 675, 55, 110, 37, 1, 0);
        g_simTopBoxes[3].extract(&surf, 697, 99, 88, 36, 1, 0);
        surf.load_pcx("interface\\courseinfo_A", 0, 0, PALETTE_COLORS, 2);
        g_courseInfoBoxPicsAlphaMask[0].extract(&surf, 48, 6, 183, 58, 1, 0);
        g_courseInfoBoxPicsAlphaMask[1].extract(&surf, 647, 13, 138, 36, 1, 0);
        g_courseInfoBoxPicsAlphaMask[2].extract(&surf, 675, 55, 110, 37, 1, 0);
        g_courseInfoBoxPicsAlphaMask[3].extract(&surf, 697, 99, 88, 36, 1, 0);
        surf.load_pcx("interface\\s_courseinfo", 0, 0, PALETTE_COLORS, 2);
        g_s_courseInfo[0].extract(&surf, 48, 6, 183, 58, 1, 1);
        g_s_courseInfo[1].extract(&surf, 647, 13, 138, 36, 1, 1);
        g_s_courseInfo[2].extract(&surf, 675, 55, 110, 37, 1, 1);
        g_s_courseInfo[3].extract(&surf, 697, 99, 88, 36, 1, 1);
        surf.load_pcx("interface\\CGbuttons", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 3; k++)
            for (j = 0; j < 2; j++)
                g_cgScreenSprites[k * 2 + j].extract(&surf, j * 0x96, 0x12c + k * 0x32, 0x70, 0x23, 1, 1);
        for (k = 0; k < 5; k++)
            for (j = 0; j < 2; j++)
                g_cgScreenSprites[k * 2 + j + 6].extract(&surf, j * 0x96, 0x32 + k * 0x32, 0x70, 0x1c, 1, 1);
        for (k = 0; k < 2; k++)
            for (j = 0; j < 2; j++)
                g_cgScreenSprites[k * 2 + j + 16].extract(&surf, k * 0x64 + j * 0x32, 0, 0x23, 0x25, 1, 1);
        for (k = 0; k < 8; k++)
            for (j = 0; j < 2; j++)
                g_cgScreenSprites[k * 2 + j + 20].extract(&surf, 0x12c + j * 0x32, k * 0x32, 0x32, 0x32, 1, 1);
        for (k = 0; k < 2; k++)
            for (j = 0; j < 2; j++)
                g_cgScreenSprites[k * 2 + j + 36].extract(&surf, 0x1f4 + j * 0x64, k * 0x64, 0x3c, 0x3d, 1, 1);
        g_cgScreenSprites[40].extract(&surf, 300, 500, 57, 38, 1, 1);
        surf.load_pcx("interface\\HeadSelect", 0, 0, PALETTE_COLORS, 2);
        g_cgScreenSprites[41].extract(&surf, 0, 258, 800, 342, 1, 1);
        g_cgScreenSprites[42].extract(&surf, 432, 105, 87, 152, 1, 1);
        g_cgScreenSprites[43].extract(&surf, 519, 105, 87, 152, 1, 1);
        g_cgScreenSprites[44].extract(&surf, 614, 0, 93, 152, 1, 1);
        g_cgScreenSprites[45].extract(&surf, 707, 0, 93, 152, 1, 1);
        g_cgScreenSprites[46].extract(&surf, 0, 0, 133, 137, 1, 1);
        g_cgScreenSprites[47].extract(&surf, 150, 0, 133, 137, 1, 1);
        surf.load_pcx("interface\\GolferStats_A", 0, 0, PALETTE_COLORS, 2);
        g_toolbarPanelBackings_A[9].extract(&surf, 64, 33, 402, 244, 1, 0);
        g_toolbarPanelBackings_A[10].extract(&surf, 64, 300, 402, 138, 1, 0);
        for (k = 0; k < 0xa; k++)
            for (j = 0; j < 3; j++)
                g_golferStatsBarCellsMask[k * 3 + j].extract(&surf, 0x1f4 + j * 0x64, k * 0x32, 0x39, 0x32, 1, 0);
        surf.load_pcx("interface\\HeadBodyBck", 0, 0, PALETTE_COLORS, 2);
        g_headBodyBgPic.extract(&surf, 0, 0, 115, 229, 1, 1);
        surf.load_pcx("interface\\s_GolferStats", 0, 0, PALETTE_COLORS, 2);
        g_toolPanelBgImages[11].extract(&surf, 64, 33, 402, 244, 1, 1);
        surf.load_pcx("interface\\GolferStats", 0, 0, PALETTE_COLORS, 2);
        g_toolPanelBgImages[9].extract(&surf, 64, 33, 402, 244, 1, 0);
        g_toolPanelBgImages[10].extract(&surf, 64, 300, 402, 138, 1, 0);
        for (k = 0; k < 0xa; k++)
            for (j = 0; j < 3; j++)
                g_gameGolferStatsBtnSprs[k * 3 + j].extract(&surf, 0x1f4 + j * 0x64, k * 0x32, 0x39, 0x32, 1, 0);
        surf.load_pcx("interface\\3mainLowerLeft_A", 0, 0, PALETTE_COLORS, 2);
        g_toolbarPanelBackings_A[0].extract(&surf, 0, 435, 215, 165, 1, 0);
        for (k = 0; k < 3; k++) {
            g_lowerLeftBarAlphaSprites[k * 2].extract(&surf, 0, k * 0x64, 0x4e, 0x51, 1, 0);
            g_lowerLeftBarAlphaSprites[k * 2 + 1].extract(&surf, 0x64, k * 0x64, 0x4e, 0x51, 1, 0);
        }
        for (k = 1; k < 10; k++)
            g_lowerLeftBarAlphaSprites[k * 2 + 6].extract(&surf, 0x258, k * 0x32, 0x24, 0x26, 1, 1);
        surf.load_pcx("interface\\3mainLowerLeft", 0, 0, PALETTE_COLORS, 2);
        g_toolPanelBgImages[0].extract(&surf, 0, 435, 215, 165, 1, 0);
        for (k = 0; k < 3; k++)
            for (j = 0; j < 3; j++)
                g_lowerLeftBarParts[k * 3 + j].extract(&surf, j * 0x64, k * 0x64, 0x4e, 0x51, 1, 0);
        for (k = 0; k < 3; k++)
            g_lowerLeftBarParts[k + 9].extract(&surf, 0x12c, k * 0x64, 0xd6, 0x5b, 1, 1);
        for (k = 0; k < 9; k++)
            for (j = 0; j < 3; j++)
                g_lowerLeftBarParts[k * 3 + j + 12].extract(&surf, 0x258 + j * 0x32, 0x32 + k * 0x32, 0x24, 0x26, 1, 1);
        surf.load_pcx("interface\\BaseTerrainPanel_A", 0, 0, PALETTE_COLORS, 2);
        g_toolbarPanelBackings_A[1].extract(&surf, 216, 482, 584, 118, 1, 0);
        surf.load_pcx("interface\\BaseTerrainPanel", 0, 0, PALETTE_COLORS, 2);
        g_toolPanelBgImages[1].extract(&surf, 216, 482, 584, 118, 1, 0);
        g_themeTerrainButtonBmps[84].extract(&surf, 40, 550, 34, 40, 1, 1);
        g_themeTerrainButtonBmps[88].extract(&surf, 150, 480, 34, 34, 1, 1);
        surf.load_pcx("interface\\ElevationPanel_A", 0, 0, PALETTE_COLORS, 2);
        g_toolbarPanelBackings_A[8].extract(&surf, 215, 482, 585, 118, 1, 0);
        surf.load_pcx("interface\\ElevationPanel", 0, 0, PALETTE_COLORS, 2);
        g_toolPanelBgImages[8].extract(&surf, 215, 482, 585, 118, 1, 0);
        g_gameGlowingPanelDiscs[6].extract(&surf, 0, 400, 41, 44, 1, 1);
        for (k = 0; k < 3; k++)
            for (j = 0; j < 3; j++)
                g_simElevationPanelButtonFrames[k * 3 + j].extract(&surf, j * 0x64, k * 0x64, 0x58, 0x63, 1, 1);
        for (k = 0; k < 3; k++)
            g_simElevationPanelButtonFrames[k + 9].extract(&surf, 0x190 + k * 0x64, 0, 0x48, 0x63, 1, 1);
        for (k = 0; k < 3; k++)
            g_simElevationPanelButtonFrames[k + 12].extract(&surf, k * 0x32, 0x1c2, 0x22, 0x28, 1, 1);
        surf.load_pcx("interface\\BuildingPanel_A", 0, 0, PALETTE_COLORS, 2);
        g_toolbarPanelBackings_A[2].extract(&surf, 216, 482, 584, 118, 1, 0);
        surf.load_pcx("interface\\BuildingPanel", 0, 0, PALETTE_COLORS, 2);
        g_toolPanelBgImages[2].extract(&surf, 216, 482, 584, 118, 1, 0);
        g_gameGlowingPanelDiscs[4].extract(&surf, 150, 480, 34, 34, 1, 1);
        g_buildingPnlFrames[0].extract(&surf, 0, 0, 74, 64, 1, 1);
        g_buildingPnlFrames[1].extract(&surf, 100, 0, 77, 64, 1, 1);
        g_buildingPnlFrames[2].extract(&surf, 200, 0, 77, 64, 1, 1);
        g_buildingPnlFrames[4].extract(&surf, 0, 100, 77, 58, 1, 1);
        g_buildingPnlFrames[5].extract(&surf, 100, 100, 77, 58, 1, 1);
        g_buildingPnlFrames[6].extract(&surf, 200, 100, 77, 58, 1, 1);
        g_buildingPnlFrames[8].extract(&surf, 40, 550, 34, 40, 1, 1);
        g_buildingPnlFrames[9].extract(&surf, 80, 550, 34, 40, 1, 1);
        g_buildingPnlFrames[10].extract(&surf, 120, 550, 34, 40, 1, 1);
        surf.load_pcx("interface\\AmenitiesPanel_A", 0, 0, PALETTE_COLORS, 2);
        g_toolbarPanelBackings_A[3].extract(&surf, 214, 482, 586, 118, 1, 0);
        g_amenitiesMaskSprites[0].extract(&surf, 380, 250, 68, 79, 1, 0);
        g_amenitiesMaskSprites[1].extract(&surf, 450, 250, 47, 79, 1, 0);
        g_amenitiesMaskSprites[2].extract(&surf, 600, 250, 67, 79, 1, 0);
        g_amenitiesMaskSprites[3].extract(&surf, 500, 250, 67, 79, 1, 0);
        surf.load_pcx("interface\\AmenitiesPanel", 0, 0, PALETTE_COLORS, 2);
        g_toolPanelBgImages[3].extract(&surf, 214, 482, 586, 118, 1, 0);
        g_gameGlowingPanelDiscs[5].extract(&surf, 0, 400, 34, 34, 1, 1);
        for (k = 0; k < 3; k++) {
            g_amenitiesIcons[k].extract(&surf, 0x12c + k * 0x64 - 0x12c, 0, 0x3c, 0x38, 1, 1);
            g_amenitiesIcons[k + 3].extract(&surf, 0x12c + k * 0x64 - 0x12c, 0x64, 0x40, 0x32, 1, 1);
            g_amenitiesIcons[k + 6].extract(&surf, 0x12c + k * 0x64 - 0x12c, 0xc8, 0x3e, 0x32, 1, 1);
            g_amenitiesIcons[k + 9].extract(&surf, 0x12c + k * 0x64, 0, 0x33, 0x2f, 1, 1);
            g_amenitiesIcons[k + 27].extract(&surf, 0x12c + k * 0x64, 0x32, 0x33, 0x2d, 1, 1);
            g_amenitiesIcons[k + 12].extract(&surf, 0x12c + k * 0x64, 0x64, 0x33, 0x2d, 1, 1);
            g_amenitiesIcons[k + 15].extract(&surf, 0x12c + k * 0x64, 0x96, 0x33, 0x2f, 1, 1);
            g_amenitiesIcons[k + 18].extract(&surf, 0x12c + k * 0x64, 0xc8, 0x33, 0x2d, 1, 1);
            g_amenitiesIcons[k + 21].extract(&surf, k * 0x32, 0x1c2, 0x24, 0x24, 1, 1);
            g_amenitiesIcons[k + 24].extract(&surf, k * 0x32, 0x1f4, 0x26, 0x24, 1, 1);
        }
        g_bulldozeDimCursor.extract(&surf, 50, 550, 38, 36, 1, 1);
        g_bulldozeLitCursor.extract(&surf, 0, 550, 38, 36, 1, 1);
        g_amenityPanelParts[0].extract(&surf, 380, 250, 68, 79, 1, 0);
        g_amenityPanelParts[1].extract(&surf, 450, 250, 47, 79, 1, 0);
        g_amenityPanelParts[2].extract(&surf, 600, 250, 67, 79, 1, 0);
        g_amenityPanelParts[3].extract(&surf, 500, 250, 67, 79, 1, 0);
        for (k = 0; k < 4; k++) {
            g_amenityPanelParts[k + 4].extract(&surf, 0x12c + k * 0x64, 0x15e, 0x2f, 0x28, 1, 1);
            g_amenityPanelParts[k + 8].extract(&surf, 0x12c + k * 0x64, 0x190, 0x2f, 0x29, 1, 1);
        }
        surf.load_pcx("interface\\MemberPanel_A", 0, 0, PALETTE_COLORS, 2);
        g_toolbarPanelBackings_A[4].extract(&surf, 215, 474, 585, 126, 1, 0);
        surf.load_pcx("interface\\MemberPanel", 0, 0, PALETTE_COLORS, 2);
        g_toolPanelBgImages[4].extract(&surf, 215, 474, 585, 126, 1, 0);
        g_gameGlowingPanelDiscs[0].extract(&surf, 0, 400, 33, 33, 1, 1);
        g_gameGlowingPanelDiscs[1].extract(&surf, 0, 450, 33, 33, 1, 1);
        g_memberWindowSprite[0].extract(&surf, 0, 0, 121, 44, 1, 1);
        for (k = 0; k < 0xa; k++)
            g_memberWindowSprite[k + 1].extract(&surf, 0x252 + k * -0x10, 0x64, 0x10, 0x10, 1, 1);
        g_memberWindowSprite[13].extract(&surf, 450, 220, 7, 7, 1, 1);
        g_memberWindowSprite[16].extract(&surf, 450, 230, 7, 7, 1, 1);
        g_memberWindowSprite[11].extract(&surf, 0, 150, 36, 13, 1, 1);
        g_memberWindowSprite[12].extract(&surf, 0, 250, 36, 13, 1, 1);
        g_memberWindowSprite[14].extract(&surf, 0, 300, 42, 42, 1, 1);
        g_memberWindowSprite[15].extract(&surf, 50, 300, 42, 42, 1, 1);
        surf.load_pcx("interface\\JoeCoolPanel_A", 0, 0, PALETTE_COLORS, 2);
        g_toolbarPanelBackings_A[5].extract(&surf, 214, 474, 586, 126, 1, 0);
        g_joeCoolToolbarBmpsAlpha[0].extract(&surf, 0, 0, 80, 50, 1, 1);
        g_joeCoolToolbarBmpsAlpha[1].extract(&surf, 0, 100, 80, 50, 1, 1);
        surf.load_pcx("interface\\JoeCoolPanel", 0, 0, PALETTE_COLORS, 2);
        g_toolPanelBgImages[5].extract(&surf, 214, 474, 586, 126, 1, 0);
        g_toolPanelBgImages[6].extract(&surf, 297, 300, 503, 122, 1, 1);
        for (k = 0; k < 4; k++) {
            int x = k * 0x32;
            int wideX = k * 0x64;
            g_peopleButtonBmps[k].extract(&surf, x, 0x12c, 0x2a, 0x2c, 1, 1);
            g_peopleButtonBmps[k + 4].extract(&surf, x, 0x1f4, 0x26, 0x20, 1, 1);
            g_peopleButtonBmps[k + 8].extract(&surf, x, 0x217, 0x26, 0x1e, 1, 1);
            g_peopleButtonBmps[k + 12].extract(&surf, x, 0x235, 0x26, 0x20, 1, 1);
            int y = (k == 0);
            g_peopleButtonBmps[k + 16].extract(&surf, wideX, y, 0x50, 0x32, 1, 1);
            g_peopleButtonBmps[k + 20].extract(&surf, wideX, y + 0x32, 0x50, 0x32, 1, 1);
            g_peopleButtonBmps[k + 24].extract(&surf, wideX, y + 0x64, 0x50, 0x32, 1, 1);
            g_peopleButtonBmps[k + 28].extract(&surf, wideX, y + 0x96, 0x50, 0x32, 1, 1);
            g_peopleButtonBmps[k + 32].extract(&surf, wideX, y + 0xc8, 0x50, 0x32, 1, 1);
        }
        g_peopleButtonBmps[36].extract(&surf, 0, 400, 33, 33, 1, 1);
        g_peopleButtonBmps[40].extract(&surf, 0, 450, 33, 33, 1, 1);
        surf.load_pcx("interface\\EmployeePanel_A", 0, 0, PALETTE_COLORS, 2);
        g_toolbarPanelBackings_A[7].extract(&surf, 215, 474, 585, 126, 1, 0);
        surf.load_pcx("interface\\EmployeePanel", 0, 0, PALETTE_COLORS, 2);
        g_toolPanelBgImages[7].extract(&surf, 215, 474, 585, 126, 1, 0);
        g_gameGlowingPanelDiscs[2].extract(&surf, 0, 400, 33, 33, 1, 1);
        g_gameGlowingPanelDiscs[3].extract(&surf, 0, 450, 33, 33, 1, 1);
        for (k = 0; k < 2; k++)
            for (j = 0; j < 3; j++)
                g_staffPanelSpr[k * 4 + j].extract(&surf, j * 0x32, 0x1f4 + k * 0x32, 0x2a, 0x2a, 1, 1);
        for (k = 0; k < 2; k++)
            for (j = 0; j < 3; j++)
                g_staffPanelSpr[k * 4 + j + 8].extract(&surf, j * 0x32, 0xc8 + k * 0x64, 0x10, 0x3e, 1, 1);
        for (k = 0; k < 4; k++)
            for (j = 0; j < 4; j++)
                g_staffPanelSpr[k * 4 + j + 16].extract(&surf, 0x190 + j * 0x64, k * 0x32, 0x34, 0x30, 1, 1);
        for (k = 0; k < 2; k++)
            for (j = 0; j < 3; j++)
                g_staffPanelSpr[k * 4 + j + 32].extract(&surf, j * 0x64, k * 0x32, 0x3f, 0x2b, 1, 1);
        surf.load_pcx("interface\\ChooseAlphaButtons_A", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 4; k++)
            g_mapMarkerButtonAlphas[k].extract(&surf, 2 + k * 0xc8, 0, k < 2 ? 0x3c : 0xb6, k < 2 ? 0x32 : 0x4a, 1, 1);
        surf.load_pcx("interface\\chooseParklandButtons", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 4; k++)
            g_mapSiteMarkerPics[k].extract(&surf, 2 + k * 0xc8, 0, k < 2 ? 0x3b : 0xb6, k < 2 ? 0x31 : 0x49, 1, 1);
        surf.load_pcx("interface\\chooseDesertButtons", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 4; k++)
            g_mapSiteMarkerPics[k + 4].extract(&surf, 2 + k * 0xc8, 0, k < 2 ? 0x3b : 0xb6, k < 2 ? 0x31 : 0x49, 1, 1);
        surf.load_pcx("interface\\chooseTropicalButtons", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 4; k++)
            g_mapSiteMarkerPics[k + 8].extract(&surf, 2 + k * 0xc8, 0, k < 2 ? 0x3b : 0xb6, k < 2 ? 0x31 : 0x49, 1, 1);
        surf.load_pcx("interface\\chooseLinksButtons", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 4; k++)
            g_mapSiteMarkerPics[k + 12].extract(&surf, 2 + k * 0xc8, 0, k < 2 ? 0x3b : 0xb6, k < 2 ? 0x31 : 0x49, 1, 1);
        surf.load_pcx("interface\\TacksandArrow", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 4; k++)
            for (j = 0; j < 4; j++)
                g_mapSiteMarkerPics[k * 4 + j + 16].extract(&surf, k * 0x32, j * 0x32, 0x14, 0x18, 1, 1);
        surf.load_pcx("interface\\TacksandArrow_A", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 4; k++)
            for (j = 0; j < 4; j++)
                g_mapMarkerButtonAlphas[k * 4 + j + 16].extract(&surf, k * 0x32, j * 0x32, 0x14, 0x18, 1, 1);
        surf.load_pcx("flics\\bldgs\\holemark.pcx", 0, 0, PALETTE_COLORS, 2);
        for (k = 0; k < 18; k++) {
            int a = k / 9;
            int b = k % 9;
            g_simHoleMarkArt[k].extract(&surf, b * 0x11 + 3, a * 0xe + 4, 0x11, 0xe, 1, 1);
            g_simHoleMarkArt[k + 18].extract(&surf, b * 0x12 + 4, a * 0x11 + 0x24, 0x12, 0x11, 1, 1);
            g_simHoleMarkArt[k + 36].extract(&surf, b * 0x13 + 4, a * 0x15 + 0x49, 0x13, 0x15, 1, 1);
        }
    }
}

unsigned long g_skipWaitUntil;
char g_fileList[100][100];
int g_flicSpareIndex;
int g_flicNextFree;
int g_flicReservedDword;
SpriteObject *g_currentFlic;
int g_flicNumLoads;

int g_unusedFlag = 1;


Wave g_sounds[300];

void ConfigureAndPlaySound(int index, int volume, int pan, int pitch, int option)
{
    if (index == -1)
        return;
    g_sounds[index].set_pitch(pitch);
    g_sounds[index].set_volume(volume);
    g_sounds[index].set_pan(pan);
    g_sounds[index].set_delay(option);
    g_sounds[index].play();
}

void FadeOutSound(int index)
{
    g_sounds[index].fade(1000);
}

void LoadSoundEffects()
{
    g_sounds[188].load("sounds\\Golf sfx\\Tiger Bounce 3.wav", 4);
    g_sounds[189].load("sounds\\Golf sfx\\Tiger Swing 1.wav", 4);
    g_sounds[SOUND_BASS_UP].load("sounds\\interface\\bass up 2.wav", 4);
    g_sounds[SOUND_BASS_DOWN].load("sounds\\interface\\bass down 2.wav", 4);
    g_sounds[190].load("sounds\\Golf sfx\\Golf Swing Perfect M.wav", 4);
    g_sounds[191].load("sounds\\Golf sfx\\Golf Swing NoAcc.wav", 4);
    g_sounds[192].load("sounds\\Golf sfx\\Golf Swing NoImag M.wav", 4);
    g_sounds[193].load("sounds\\Golf sfx\\Golf Swing Perfect FM.wav", 4);
    g_sounds[0].load("sounds\\Golf sfx\\Drive With Ball.wav", 4);
    g_sounds[1].load("sounds\\Golf sfx\\Iron.wav", 4);
    g_sounds[2].load("sounds\\Golf sfx\\Chip.wav", 4);
    g_sounds[3].load("sounds\\Golf sfx\\Putt.wav", 4);
    g_sounds[4].load("sounds\\Golf sfx\\Ball In Hole.wav", 4);
    g_sounds[5].load("sounds\\Golf sfx\\Ball Water.wav", 4);
    g_sounds[6].load("sounds\\Golf sfx\\Ball Tree.wav", 4);
    g_sounds[7].load("sounds\\Golf sfx\\Ball Tree2.wav", 4);
    g_sounds[8].load("sounds\\Golf sfx\\Ball Tree Leaves.wav", 4);
    g_sounds[54].load("sounds\\Golf sfx\\Ball Drop Fairway.wav", 4);
    g_sounds[55].load("sounds\\Golf sfx\\Ball Drop Sand.wav", 4);
    g_sounds[11].load("simsfx\\Male\\mHAPPY0.wav", 4);
    g_sounds[13].load("simsfx\\Male\\mHAPPY1.wav", 4);
    g_sounds[17].load("simsfx\\Male\\mHARD0.wav", 4);
    g_sounds[19].load("simsfx\\Male\\mEASY0.wav", 4);
    g_sounds[21].load("simsfx\\Male\\mTRICKY0.wav", 4);
    g_sounds[23].load("simsfx\\Male\\mBLIND.wav", 4);
    g_sounds[61].load("simsfx\\Male\\mCOKE0.wav", 4);
    g_sounds[63].load("simsfx\\Male\\mBAD0.wav", 4);
    g_sounds[65].load("simsfx\\Male\\mSAD2.wav", 4);
    g_sounds[67].load("simsfx\\Male\\mCRAB0.wav", 4);
    g_sounds[69].load("simsfx\\Male\\mWAITING0.wav", 4);
    g_sounds[71].load("simsfx\\Male\\mMAD0.wav", 4);
    g_sounds[73].load("simsfx\\Male\\mBENCH0.wav", 4);
    g_sounds[75].load("simsfx\\Male\\mSENIC0.wav", 4);
    g_sounds[77].load("simsfx\\Male\\mLOVELY0.wav", 4);
    g_sounds[79].load("simsfx\\Male\\mCELEB0.wav", 4);
    g_sounds[81].load("simsfx\\Male\\mWET0.wav", 4);
    g_sounds[83].load("simsfx\\Male\\mOOPS0.wav", 4);
    g_sounds[9].load("simsfx\\Male\\mWATER0.wav", 4);
    g_sounds[87].load("simsfx\\Male\\mUGLY0.wav", 4);
    g_sounds[89].load("simsfx\\Male\\mSTORY0.wav", 4);
    g_sounds[91].load("simsfx\\Male\\mSTORYNO.wav", 4);
    g_sounds[165].load("simsfx\\Male\\mSTORYOK.wav", 4);
    g_sounds[167].load("simsfx\\Male\\mSTORYYES.wav", 4);
    g_sounds[14].load("sounds\\Female\\Good Shot FM2.wav", 4);
    g_sounds[15].load("sounds\\Male\\Good Shot M2.wav", 4);
    g_sounds[10].load("simsfx\\Female\\fHAPPY0.wav", 4);
    g_sounds[12].load("simsfx\\Female\\fHAPPY1.wav", 4);
    g_sounds[16].load("simsfx\\Female\\fHARD0.wav", 4);
    g_sounds[18].load("simsfx\\Female\\fEASY0.wav", 4);
    g_sounds[20].load("simsfx\\Female\\fTRICKY0.wav", 4);
    g_sounds[22].load("simsfx\\Female\\fBLIND.wav", 4);
    g_sounds[60].load("simsfx\\Female\\fCOKE0.wav", 4);
    g_sounds[62].load("simsfx\\Female\\fBAD0.wav", 4);
    g_sounds[64].load("simsfx\\Female\\fSAD2.wav", 4);
    g_sounds[66].load("simsfx\\Female\\fCRAB0.wav", 4);
    g_sounds[68].load("simsfx\\Female\\fWAITING0.wav", 4);
    g_sounds[70].load("simsfx\\Female\\fMAD0.wav", 4);
    g_sounds[72].load("simsfx\\Female\\fBENCH0.wav", 4);
    g_sounds[74].load("simsfx\\Female\\fSENIC0.wav", 4);
    g_sounds[76].load("simsfx\\Female\\fLOVELY0.wav", 4);
    g_sounds[78].load("simsfx\\Female\\fCELEB0.wav", 4);
    g_sounds[80].load("simsfx\\Female\\fWET0.wav", 4);
    g_sounds[82].load("simsfx\\Female\\fOOPS0.wav", 4);
    g_sounds[8].load("simsfx\\Female\\fWATER0.wav", 4);
    g_sounds[86].load("simsfx\\Female\\fUGLY0.wav", 4);
    g_sounds[88].load("simsfx\\Female\\fSTORY0.wav", 4);
    g_sounds[90].load("simsfx\\Female\\fSTORYNO.wav", 4);
    g_sounds[164].load("simsfx\\Female\\fSTORYOK.wav", 4);
    g_sounds[166].load("simsfx\\Female\\fSTORYYES.wav", 4);
    g_sounds[151].load("simsfx\\Male\\mVARIETY.wav", 4);
    g_sounds[153].load("simsfx\\Male\\mSAME.wav", 4);
    g_sounds[155].load("simsfx\\Male\\mBADHOLE.wav", 4);
    g_sounds[157].load("simsfx\\Male\\mHUNGRY.wav", 4);
    g_sounds[159].load("simsfx\\Male\\mTHIRSTY.wav", 4);
    g_sounds[161].load("simsfx\\Male\\mTIRED.wav", 4);
    g_sounds[163].load("simsfx\\Male\\mOPTIONS.wav", 4);
    g_sounds[150].load("simsfx\\female\\fVARIETY.wav", 4);
    g_sounds[152].load("simsfx\\female\\fSAME.wav", 4);
    g_sounds[154].load("simsfx\\female\\fBADHOLE.wav", 4);
    g_sounds[156].load("simsfx\\female\\fHUNGRY.wav", 4);
    g_sounds[158].load("simsfx\\female\\fTHIRSTY.wav", 4);
    g_sounds[160].load("simsfx\\female\\fTIRED.wav", 4);
    g_sounds[162].load("simsfx\\female\\fOPTIONS.wav", 4);
    g_sounds[92].load("simsfx\\Male\\mGREETING.wav", 4);
    g_sounds[93].load("sounds\\Celebs\\Ranger.wav", 4);
    g_sounds[94].load("simsfx\\Male\\mCRABGRASS2.wav", 4);
    g_sounds[95].load("sounds\\Celebs\\Tray girl.wav", 4);
    g_sounds[96].load("sound\\Celebs\\Politician Like.wav", 4);
    g_sounds[97].load("sounds\\Celebs\\Marshal Male.wav", 4);
    g_sounds[98].load("sounds\\Celebs\\Lawn Technician.wav", 4);
    g_sounds[99].load("simsfx\\Female\\fHAVEADRINK.wav", 4);
    g_sounds[100].load("sounds\\Celebs\\mGREETING.wav", 4);
    g_sounds[101].load("sounds\\Celebs\\Politician Dislike.wav", 4);
    g_sounds[102].load("sounds\\Celebs\\Action Star Dislike.wav", 4);
    g_sounds[103].load("sounds\\Celebs\\Politician Like.wav", 4);
    g_sounds[104].load("sounds\\Celebs\\Spears Like.wav", 4);
    g_sounds[105].load("sounds\\Celebs\\Spears Dislike.wav", 4);
    g_sounds[106].load("sounds\\Celebs\\Fitness Female.wav", 4);
    g_sounds[107].load("sounds\\Celebs\\Tray Girl.wav", 4);
    g_sounds[39].load("sounds\\effects\\winter.wav", 4);
    g_sounds[SOUND_AMBIENT_SPRING].load("sounds\\effects\\spring.wav", 4);
    g_sounds[42].load("sounds\\effects\\tada.wav", 4);
    g_sounds[43].load("sounds\\effects\\tada 2.wav", 4);
    g_sounds[41].load("sounds\\effects\\belch.wav", 4);
    g_sounds[SOUND_REFUSED].load("sounds\\effects\\boing.wav", 4);
    g_sounds[25].load("sounds\\effects\\cash.wav", 4);
    g_sounds[SOUND_AMBIENT_LAKE].load("sounds\\effects\\lake 1.wav", 4);
    g_sounds[SOUND_AMBIENT_WIND].load("sounds\\effects\\wind 1.wav", 4);
    g_sounds[SOUND_AMBIENT_DESERT].load("sounds\\effects\\desert3.wav", 4);
    g_sounds[SOUND_AMBIENT_FOREST].load("sounds\\effects\\forest2.wav", 4);
    g_sounds[SOUND_AMBIENT_GRASS].load("sounds\\effects\\grass3.wav", 4);
    g_sounds[SOUND_FLIES].load("sounds\\effects\\fly.wav", 4);
    g_sounds[SOUND_WIN].load("sounds\\effects\\taunt009.wav", 4);
    g_sounds[SOUND_LOSE].load("sounds\\effects\\unrest 2.wav", 4);
    g_sounds[37].load("sounds\\effects\\twinkle.wav", 4);
    g_sounds[SOUND_TIME_TUNNEL].load("sounds\\effects\\TimeTunnelClip2.wav", 4);
    g_sounds[143].load("sounds\\interface\\violin down 2.wav", 4);
    g_sounds[SOUND_CONSTRUCTION].load("sounds\\effects\\club 2 drop no wtr.wav", 4);
    g_sounds[SOUND_TADA].load("sounds\\effects\\tada 3.wav", 4);
    g_sounds[57].load("sounds\\effects\\Hot Air Long.wav", 4);
    g_sounds[58].load("sounds\\effects\\Seriama Amazon.wav", 4);
    g_sounds[59].load("sounds\\effects\\Moose 2.wav", 4);
    g_sounds[195].load("sounds\\effects\\Sheep 1.wav", 4);
    g_sounds[196].load("sounds\\effects\\Alligator.wav", 4);
    g_sounds[197].load("sounds\\effects\\gila monster.wav", 4);
    g_sounds[198].load("sounds\\effects\\snake.wav", 4);
    g_sounds[209].load("sounds\\Sail Boat.wav", 4);
    g_sounds[44].load("sounds\\buy2short.wav", 20);
    g_sounds[50].load("sounds\\buy1short.wav", 20);
    g_sounds[SOUND_PURCHASE].load("sounds\\buy3short.wav", 20);
    g_sounds[52].load("sounds\\buy4short.wav", 20);
    g_sounds[53].load("sounds\\Romeo and Juliet Edit.wav", 20);
    g_sounds[125].load("sounds\\New Age.wav", 20);
    g_sounds[126].load("sounds\\Golf Harp 7.wav", 20);
    g_sounds[127].load("sounds\\Golf Harp 6.wav", 20);
    g_sounds[SOUND_TITLE_MUSIC].load("sounds\\Music Idea.wav", 20);
    g_sounds[SOUND_CREDITS_MUSIC].load("sounds\\music\\credit.wav", 4);
    g_sounds[SOUND_COURSE_AMBIENCE].load("sounds\\GolfAmbience122.wav", 4);
    g_sounds[SOUND_APPLAUSE].load("sounds\\Applause.wav", 4);
    g_sounds[SOUND_APPLAUSE_GOOD].load("sounds\\ApplauseGood.wav", 4);
    g_sounds[SOUND_APPLAUSE_BAD].load("sounds\\ApplauseBad.wav", 4);
    g_sounds[49].load("sounds\\golf sfx\\Cart Away.wav", 4);
    g_sounds[SOUND_TREE_SPROUT].load("sounds\\effects\\tree sprout 3.wav", 4);
    g_sounds[SOUND_PALM].load("sounds\\effects\\palm.wav", 4);
    g_sounds[SOUND_FLOWER_BED].load("sounds\\effects\\flower bed.wav", 4);
    g_sounds[SOUND_MAPLE].load("sounds\\effects\\maple.wav", 4);
    g_sounds[SOUND_CAMERA].load("sounds\\effects\\camera.wav", 4);
    g_sounds[194].load("sounds\\shotgun.wav", 4);
    g_sounds[120].load("sounds\\world\\Spanish Guitar wFlute 2.wav", 20);
    g_sounds[121].load("sounds\\music\\Desert Music\\Spanish Guitar 1.wav", 20);
    g_sounds[122].load("sounds\\music\\Desert Music\\Spanish Guitar 3.wav", 20);
    g_sounds[110].load("sounds\\world\\Steel Drums 2.wav", 20);
    g_sounds[111].load("sounds\\music\\Tropical Music\\Steel Drum 1a.wav", 20);
    g_sounds[112].load("sounds\\music\\Tropical Music\\Steel Drums 3.wav", 20);
    g_sounds[115].load("sounds\\world\\Bagpipe.wav", 20);
    g_sounds[116].load("sounds\\music\\Links Music\\Harpiscord 1.wav", 20);
    g_sounds[117].load("sounds\\music\\Links Music\\Bagpipe2.wav", 20);
    g_sounds[130].load("sounds\\celebs\\I'll Be Back.wav", 4);
    g_sounds[131].load("sounds\\celebs\\Spears Char.wav", 4);
    g_sounds[132].load("sounds\\celebs\\Politician Char.wav", 4);
    g_sounds[133].load("sounds\\celebs\\Comedian Char.wav", 4);
    g_sounds[134].load("sounds\\celebs\\Super Model.wav", 4);
    g_sounds[135].load("sounds\\celebs\\Fitness Female.wav", 4);
    g_sounds[136].load("sounds\\celebs\\Female Comic.wav", 4);
    g_sounds[137].load("simsfx\\male\\Vox_clear_throat_M3", 4);
    g_sounds[138].load("sounds\\celebs\\Movie Princess.wav", 4);
    g_sounds[139].load("sounds\\celebs\\Rock Star.wav", 4);
    g_sounds[140].load("sounds\\celebs\\Basketball.wav", 4);
    g_sounds[141].load("sounds\\celebs\\Aging Star.wav", 4);
    g_sounds[210].load("sounds\\emotion\\PLS Happy mix.wav", 4);
    g_sounds[220].load("sounds\\emotion\\PLS Sid mix.wav", 4);
    g_sounds[230].load("sounds\\emotion\\PLS Success mix.wav", 4);
    g_sounds[16].load("sounds\\emotion\\PLS Failure mix.wav", 4);
    g_sounds[211].load("sounds\\emotion\\MKLS Happy Mix.wav", 4);
    g_sounds[221].load("sounds\\emotion\\MKLS Sad mix.wav", 4);
    g_sounds[231].load("sounds\\emotion\\MKLS Success mix.wav", 4);
    g_sounds[17].load("sounds\\emotion\\MKLS Failure mix.wav", 4);
    g_sounds[212].load("sounds\\emotion\\MalePSSHappy.wav", 4);
    g_sounds[222].load("sounds\\emotion\\MalePSSSad.wav", 4);
    g_sounds[232].load("sounds\\emotion\\MalePSSSuccess.wav", 4);
    g_sounds[18].load("sounds\\emotion\\MalePSSFailure.wav", 4);
    g_sounds[213].load("sounds\\emotion\\SSS Happy mix.wav", 4);
    g_sounds[223].load("sounds\\emotion\\SSS Sad mix.wav", 4);
    g_sounds[233].load("sounds\\emotion\\SSS Success mix.wav", 4);
    g_sounds[19].load("sounds\\emotion\\SSS Failure mix.wav", 4);
    g_sounds[215].load("sounds\\emotion\\FemalePLSHappy.wav", 4);
    g_sounds[225].load("sounds\\emotion\\FemalePLSSad.wav", 4);
    g_sounds[235].load("sounds\\emotion\\FemalePLSSuccess.wav", 4);
    g_sounds[21].load("sounds\\emotion\\FemalePLSFailure.wav", 4);
    g_sounds[216].load("sounds\\emotion\\FemaleSSSHappy.wav", 4);
    g_sounds[226].load("sounds\\emotion\\FemaleSSSSad.wav", 4);
    g_sounds[236].load("sounds\\emotion\\FemaleSSSSuccess.wav", 4);
    g_sounds[22].load("sounds\\emotion\\FemaleSSSFailure.wav", 4);
    g_sounds[217].load("sounds\\emotion\\FemalePSSHappy.wav", 4);
    g_sounds[227].load("sounds\\emotion\\FemalePSSSad.wav", 4);
    g_sounds[237].load("sounds\\emotion\\FemalePSSSuccess.wav", 4);
    g_sounds[23].load("sounds\\emotion\\FemalePSSFailure.wav", 4);
    g_sounds[218].load("sounds\\emotion\\FemaleSkTTHappy.wav", 4);
    g_sounds[228].load("sounds\\emotion\\FemaleSkTTSad.wav", 4);
    g_sounds[238].load("sounds\\emotion\\FemaleSkTTSuccess.wav", 4);
    g_sounds[SOUND_REFUSED].load("sounds\\emotion\\FemaleSkTTFailure.wav", 4);
    g_sounds[250].load("sounds\\interface\\button1.wav", 4);
    g_sounds[251].load("sounds\\interface\\button2.wav", 4);
    g_sounds[252].load("sounds\\interface\\button3.wav", 4);
    g_sounds[SOUND_BUILDING].load("sounds\\interface\\building.wav", 4);
    g_sounds[SOUND_PAINT_ROUGH].load("sounds\\interface\\test 9.wav", 4);
    g_sounds[SOUND_PAINT_FAIRWAY].load("sounds\\interface\\test 7.wav", 4);
    g_sounds[SOUND_PLACE_ROCKS].load("sounds\\interface\\place rocks 1.wav", 4);
    g_sounds[SOUND_PLACE_WATER].load("sounds\\interface\\place water.wav", 4);
    g_sounds[SOUND_PAINT_BUNKER].load("sounds\\interface\\test 10.wav", 4);
    g_sounds[SOUND_PAINT_TEE_GREEN].load("sounds\\interface\\test 2.wav", 4);
    g_sounds[SOUND_PLACE_BENCH].load("sounds\\interface\\bench.wav", 4);
    g_sounds[SOUND_PLACE_BRIDGE].load("sounds\\interface\\bridge.wav", 4);
    g_sounds[SOUND_PLACE_PATH].load("sounds\\interface\\path.wav", 4);
    g_sounds[265].load("sounds\\interface\\check box.wav", 4);
    g_sounds[266].load("sounds\\interface\\text box.wav", 4);
    g_sounds[267].load("sounds\\interface\\slide pop up.wav", 4);
    g_sounds[268].load("sounds\\interface\\unavailable.wav", 4);
    g_sounds[269].load("sounds\\interface\\wrong.wav", 4);
}

void CleanupAllSoundObjects()
{
    for (int i = 0; i < 300; ++i)
        g_sounds[i].unload();
    release_sound();
}
