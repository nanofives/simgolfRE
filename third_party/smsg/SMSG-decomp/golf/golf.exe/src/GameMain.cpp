#include "jgldefs.h"
#include "MainLoop.h"
#include "GameMain.h"
#include "EngineInit.h"
#include "../res/resource.h"

MouseCursor g_mouseCursor;
SpriteObject g_unusedSprite3;

char *g_manualFontFace = "Manual SSi Bold";
int g_unusedData = 0;
GUID g_appGuid = {0x46a4a1e1, 0x1a2d, 0x11d1, {0x98, 0x0b, 0x00, 0xa0, 0x24, 0x29, 0x32, 0x3c}};

static int s_spareWords[30] = {0};
int g_mouseX = 0;
int g_mouseY = 0;
int g_clickX = 0;
int g_clickY = 0;
int g_mouseButton = 0;
int g_shiftDown = 0;
int g_ctrlDown = 0;
int g_mouseEvent = 0;
int g_mouseDoubleClick = 0;
int g_waitKey = 0;
static int s_spareWord0 = 0;
static int s_spareWord1 = 0;
static int s_spareWord2 = 0;
int g_mouseDownX = 0;
int g_mouseDownY = 0;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    g_screenWidth = 800;
    g_screenHeight = 600;

    if (jackal_init("jgld.dll", "Sid Meier's SimGolf", -1) == 0
        && graphsys->SetDisplayMode(g_screenWidth, g_screenHeight, 16) == 0) {

        HANDLE hIconBig =
            LoadImageA((HINSTANCE)graphsys->get_app_instance(), MAKEINTRESOURCE(IDI_GOLF), IMAGE_ICON, 32, 32, 0);
        HANDLE hIconSmall =
            LoadImageA((HINSTANCE)graphsys->get_app_instance(), MAKEINTRESOURCE(IDI_GOLF), IMAGE_ICON, 16, 16, 0);
        PostMessageA((HWND)graphsys->get_window_handle(), WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
        PostMessageA((HWND)graphsys->get_window_handle(), WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

        InitFadeTable(8);
        graphsys->show_app();

        int h = ScaleXCoordinate(240);
        int w = ScaleXCoordinate(320);
        if (g_simScreen.init(0, 0, w, h, "Sid Meier's SimGolf", 0x400000, 0, 0, 0) != 0) {
            jackal_close();
            return 1;
        }

        WidgetBase::set_def_focus(&g_simScreen);
        g_simScreen.EraseWholeBackground(0);
        g_simScreen.show(0);
        do_all_tasks();

        g_dialogRoot = &g_simScreen;
        SetDefaultOkCaption("x");
        SetDefaultCancelCaption("x");
        g_dlgDrawParams[2] = 7;
        Dialog_SetGlobalTextFonts(&g_dialogFont, 0, 0, 0);
        Dialog_SetGlobalTextColors(0, -1, 1, 1);
        g_dlgButtonColor = 0xf8;
        Dialog_SetGlobalButtonColors(0, -1, 1, 1);
        Dialog_SetGlobalButtonFonts(&g_dialogFont, 0, 0);

        g_dlgButtonBevel[0] = 7;
        g_dlgButtonBevel[1] = 0xf8;
        g_dlgButtonFrameColor = 0;
        g_defFrameColor = 1;
        g_defBevelColorA = 0xf8;
        g_themeBevelColorB = 0;
        g_defBorderThickness = 8;
        g_themeTitleGlyphColor = 0xff;
        g_dlgButtonBevel[2] = 2;
        g_buttonDefaults[3] = 2;
        g_defTitleShadowColor = 0;
        g_defTitleShadowDx = 1;
        g_defTitleShadowDy = 1;
        g_panelTheme[1] = 0;
        g_panelTheme[2] = 0xff;
        g_panelLayout[4] = 7;
        g_editBoxStyle[1] = 1;
        g_editBoxColors[0] = 0;
        g_editBoxColors[1] = 0;
        g_editBoxColors[2] = 0;
        g_editBoxStyle[3] = -1;
        g_editBoxStyle[4] = 1;
        g_editBoxStyle[5] = 1;
        g_editBoxStyle[0] = 7;
        g_buttonDefaults[0] = 0xf8;
        g_buttonDefaults[1] = 0xff;
        g_buttonDefaults[2] = 0xf8;
        g_buttonFrameColors[0] = 0;
        g_buttonFrameColors[1] = 0;

        init_sound((HWND)graphsys->get_window_handle(), 0x8a);
        InitTimer();

        g_labelFont.CreateFace("Arial Bold", 9, 0);
        g_boldArialFont.CreateFace("Arial Bold", 10, 0);
        g_smallManualFont.init("manu3_.ttf", g_manualFontFace, 15, 0);
        g_dialogFont.init("manu3_.ttf", g_manualFontFace, 20, 0);
        g_smallKleptoFont.init("klepto__.ttf", "Klepto ITC TT", 18, 0);
        g_kleptoBigFont.init("klepto__.ttf", "Klepto ITC TT", 24, 0);
        g_hudFont.init("manu3_.ttf", g_manualFontFace, 24, 0);
        g_comicFont.CreateFace("Comic Sans MS Bold", ScaleXCoordinate(40), 2);
        g_timesBoldFont.CreateFace("Times New Roman Bold", ScaleXCoordinate(7), 0);

        {
            Font *pf = &g_dialogFont;
            if (pf)
                g_defaultFont = pf;
        }
        g_startupCleared = 0;

        h = ScaleXCoordinate(240);
        w = ScaleXCoordinate(320);
        g_terrainSurfaceObject.Create(w, h, 0, 1, 0, 0);
        g_terrainSurfaceObject.sync_to_palette(&g_palette);
        g_simScreen.center();

        AssetSurface surf;

        g_palette.set();

        g_colorLut = (unsigned short *)malloc(0x20000);
        if (g_colorLut) {
            for (int i = 0; i < 0x10000; ++i)
                g_colorLut[i] =
                    (unsigned short)((((((i >> 10) & 0x1e) << 5) | ((i >> 5) & 0x1e)) << 4) | ((i >> 1) & 0xf));
        }

        Render_InitColorTable();
        PlayIntroVideo();
        g_palette.set();
        LoadSoundEffects();
        g_palette.init_cycle(0, 0xe3, 8, 50);
        g_palette.start_cycle(0);
        g_startupCleared = 0;
        MainLoop();
        CleanupAllSoundObjects();
        FreeFadeTable();
        ShutdownTerrain();
        CleanupAllFontsAndSprites();
        jackal_close();
    }
    return 0;
}

void FreeWindowIcons()
{
    HWND hwnd = (HWND)graphsys->get_window_handle();
    HICON bigIcon = (HICON)SendMessageA(hwnd, WM_SETICON, ICON_BIG, 0);
    HICON smallIcon = (HICON)SendMessageA(hwnd, WM_SETICON, ICON_SMALL, 0);

    if (bigIcon != 0)
        DestroyIcon(bigIcon);
    if (smallIcon != 0)
        DestroyIcon(smallIcon);
}

int WaitWithTimeout(int ticks, int pump)
{
    DWORD start;
    if (ticks == 0)
        return 0;
    ticks = ticks * 1000 / 100;
    start = timeGetTime();
    while (timeGetTime() - start < ticks) {
        do_all_non_input();
        if (pump) {
            ProcessMouseMessages();
            if (ProcessInputAndCheckIdle() == 0 || g_mouseButton != 0) {
                DrainPendingInput();
                return 1;
            }
        }
    }
    return 0;
}

#include "MouseCursorDtor.h"
