#ifndef GAMEMAIN_H
#define GAMEMAIN_H

#include "Surface.h"

class CursorImage {
public:
    virtual ~CursorImage();
};

class MouseCursor {
public:
    MouseCursor();
    virtual ~MouseCursor();
    void Cleanup();

    HCURSOR m_hCursor;
    CursorImage *m_image;
    int m_reserved1;
    int m_reserved2;
};

extern MouseCursor g_mouseCursor;

extern char *g_manualFontFace;

int InitFadeTable(int steps);
void FreeFadeTable();
int Render_InitColorTable();
int SetDefaultOkCaption(const char *s);
int SetDefaultCancelCaption(const char *s);
struct Font;
int Dialog_SetGlobalTextFonts(Font *font, Font *braceFont, Font *bracketFont, Font *linkFont);
void Dialog_SetGlobalTextColors(int color, int shadowColor, int shadowDx, int shadowDy);
int Dialog_SetGlobalButtonFonts(Font *font, Font *braceFont, Font *bracketFont);
void Dialog_SetGlobalButtonColors(int color, int shadowColor, int shadowDx, int shadowDy);

struct WidgetBase;
extern WidgetBase *g_dialogRoot;
extern int g_dlgButtonFrameColor;
extern int g_dlgDrawParams[3];
extern unsigned char g_dlgButtonColor;
extern int g_dlgButtonBevel[3];
extern int g_startupCleared;
extern int g_defFrameColor;
extern int g_defTitleShadowColor;
extern int g_defTitleShadowDx;
extern int g_defTitleShadowDy;
extern int g_defBorderThickness;
extern int g_defBevelColorA;
extern int g_themeTitleGlyphColor;
extern int g_themeBevelColorB;
extern int g_buttonDefaults[];
extern int g_buttonFrameColors[2];
extern Font *g_buttonFonts[4];
extern int g_panelTheme[];

extern Font g_labelFont;
extern Font g_comicFont;
extern Font g_timesBoldFont;

struct AssetSurface;
extern AssetSurface g_terrainSurfaceObject;

#endif
