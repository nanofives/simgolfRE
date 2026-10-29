#include "jgldefs.h"
#include "crt.h"
#include "AssetSurface.h"
#include "Screen.h"
#include "ScreenText.h"
#include "InputPoll.h"
#include "TextWrap.h"
#include "DialogFrame.h"
#include "Geometry.h"
#include "ListDialog.h"

unsigned g_menuAccentColor;
char g_itemHotkeys[32];
unsigned g_highlightItems;
int g_hideSelection;
int g_firstMenuDraw;
int g_lineOffsets[32];
int g_menuJustDraw;
unsigned g_headingColor;
int g_nMenuLines;
int g_nMenuItems;
int g_menuFrameDrawn;
int g_menuMaxX;
int g_menuMaxY;
int g_menuHeight;
int g_menuTextHeight;
int g_widestLine;

AssetSurface g_menuOffscreen;

unsigned g_itemColor = 0;
unsigned g_disabledColor = 0;
unsigned g_shadowColor = 0;
unsigned g_selectedColor = 0;
int g_menuLeft = 0;
int g_menuTop = 0;
int g_menuTopItem = 0;
int g_menuScrollPos = 0;
int g_menuPageSize = 0;
int g_menuShowOK = 0;
int g_menuTrackHover = 0;
int g_firstItemLine = 0;
unsigned g_disabledItemMask = 0;
int g_menuLeftClick = 0;
int g_menuDoubleClick = 0;
int g_menuRightClick = 0;
int g_menuTallFrame = 0;
int g_menuMouseDown = 0;

int g_menuInitialSel = -1;
int g_menuLastSel = -1;
int g_menuRowHeight = 8;

int ShowListDialogWithParams(const char *menu, int left, int top)
{
    g_menuLeft = left;
    g_menuTop = top;
    g_menuTrackHover = top & 1;
    g_menuTrackHover = 1;
    return ShowListSelectionDialog(menu, 1);
}

int ShowListSelectionDialog(const char *menu, int mode)
{
    char buf[1024];
    int keyboardIdle;
    int lastSel;
    int my;
    int done;
    int mx;
    int sel;
    int i;
    int key;

    strcpy(buf, menu);
    g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
    sel = lastSel = -1;
    g_menuRightClick = g_menuMouseDown = g_menuFrameDrawn = 0;
    sel = 0;
    if (g_menuInitialSel != -1)
        sel = g_menuInitialSel;
    g_firstItemLine = -1;
    g_firstMenuDraw = 1;
    g_menuRowHeight = g_pMenu->text_line_height();
    g_menuRowHeight = 7;
    if (g_menuRowHeight >= 10)
        g_menuRowHeight = 10;
    g_selectedColor = 0x80000000;
    g_itemColor = 0x80004210;
    g_menuAccentColor = 0x800003ff;
    g_headingColor = 0x80000018;
    if (g_menuTop & 1) {
        g_selectedColor = 0x80007fff;
        g_menuAccentColor = g_headingColor = 0x800003ff;
        g_itemColor = 0x80006318;
    }
    done = 0;
    keyboardIdle = 1;
    RenderMenuWithSelection(buf, g_menuInitialSel);
    if (g_menuJustDraw != 0) {
        g_menuJustDraw = 0;
        return -1;
    }
    WaitWithTimeout(30, 0);
    flush_input();
    g_mouseButton = 0;
    do {
        g_firstMenuDraw = 0;
        ProcessMouseMessages();
        if (g_mouseButton != 0 || keyboardIdle != 0) {
            keyboardIdle = 1;
            if (g_mouseButton == 0) {
                g_simScreen.get_mouse_pos(&mx, &my);
                mx = mx * 4 / ScaleXCoordinate(4);
                my = my * 4 / ScaleXCoordinate(4);
            } else {
                g_menuMouseDown = 1;
                mx = g_mouseX;
                my = g_mouseY;
            }
            if (g_mouseButton == 2)
                g_menuRightClick = 1;
            sel = (my - g_menuTop - 4) / g_menuRowHeight - g_firstItemLine;
            if (sel < 0)
                sel = -1;
            if (g_disabledItemMask & (1 << sel))
                sel = lastSel;
            if (mx > g_menuMaxX || mx < g_menuLeft)
                sel = -1;
            if (my > g_menuMaxY || my < g_menuTop)
                sel = -1;
            if (g_menuMouseDown != 0 && g_mouseButton == 0)
                done = 1;
            if (PollInputState())
                goto keys;
        } else if (PollInputState()) {
keys:
            keyboardIdle = 0;
            key = TakePendingKey();
            switch (key) {
            case 40:
                if (sel < g_nMenuItems - 1)
                    sel++;
                break;
            case 38:
                if (sel > 0)
                    sel--;
                break;
            case 13:
            case 32:
                if (!(g_disabledItemMask & (1 << sel)))
                    done = 1;
                break;
            case 27:
                sel = -1;
                done = 1;
                break;
            default:
                for (i = sel + 1; i < 32; i++) {
                    if (g_itemHotkeys[i] != -1 && (key & 0x1f) == (g_itemHotkeys[i] & 0x1f)) {
                        sel = i;
                        break;
                    }
                }
                break;
            }
        }
        if (sel < 0 || sel >= g_nMenuItems || g_menuShowOK != 0 || g_hideSelection != 0)
            sel = -1;
        if (sel != lastSel && g_menuTrackHover != 0) {
            RenderMenuWithSelection(buf, sel);
            lastSel = sel;
        }
    } while (done == 0 && quit == 0);
    if (sel != -1)
        WaitWithTimeout(20, 0);
    else
        g_menuRightClick = 0;
    g_menuInitialSel = g_menuLastSel = -1;
    g_highlightItems = g_disabledItemMask = g_hideSelection = g_menuTallFrame = 0;
    ClearPendingInput();
    return sel;
}

int RenderMenuWithSelection(char *s, int sel)
{
    int col;
    char cbuf[2];

    memset(g_itemHotkeys, -1, 32);

    g_lineOffsets[0] = 0;
    g_nMenuLines = 0;
    g_widestLine = 0;
    cbuf[1] = 0;
    col = 0;

    {
        int w = 0;
        unsigned int i;
        for (i = 0; i < strlen(s); i++) {
            if (s[i] == '\n') {
                if (g_widestLine < w)
                    g_widestLine = w;
                w = 0;
                g_lineOffsets[++g_nMenuLines] = i + 1;
            } else {
                if (w == 0 && (s[i] == ' ' || s[i] == '_')) {
                    if (col < 32)
                        g_itemHotkeys[col] = s[i + 1];
                    if (g_firstItemLine == -1)
                        g_firstItemLine = g_nMenuLines;
                    col++;
                }
                cbuf[0] = s[i];
                w += g_pMenu->text_width(cbuf);
                if (s[i] == '|')
                    w += g_pMenu->text_width("X");
            }
        }
    }

    g_nMenuLines = Clamp(g_nMenuLines, 0, (0xe8 - g_menuTop) / g_menuRowHeight);

    if (g_menuLeft == -1)
        g_menuLeft = 0xa0 - (g_widestLine + 8) / 2;

    g_menuMaxX = g_menuLeft + g_widestLine * 2 / ScaleXCoordinate(2) + 8;
    g_menuMaxY = g_menuTop + g_nMenuLines * g_menuRowHeight + 6;
    if (g_menuTallFrame != 0)
        g_menuMaxY += 2;

    if (s[strlen(s) - 1] != '\n')
        col--;
    g_nMenuItems = col;

    if ((g_menuTop & 1) == 0) {
        RenderUIFrameWithCorners(ScaleXCoordinate(g_menuLeft - 8), ScaleXCoordinate(g_menuTop - 6),
            ScaleXCoordinate(g_menuMaxX - g_menuLeft + 0x10), ScaleXCoordinate(g_menuMaxY - g_menuTop + 0xc), 1);
    } else {
        DrawRectOutline(g_menuLeft, g_menuTop, g_menuMaxX - g_menuLeft, g_menuMaxY - g_menuTop + 4, 0x80007fff);
        g_menuFrameDrawn = 1;
    }

    if (g_menuShowOK != 0) {
        DrawTextWithColor("OK", g_menuMaxX - 0x11, g_menuMaxY - 8, 0x800003ffu);
        DrawRectOutline(g_menuMaxX - 0x14, g_menuMaxY - 0xa, 0x14, 0xa, 0x800003ff);
    }

    col = (s[0] == ' ' || s[0] == '_') ? 0 : -1;

    int *p = g_lineOffsets;
    for (int k = 0; k < g_nMenuLines; k++, p++) {
        char *bar;
        int xpos;

        s[p[1] - 1] = 0;
        bar = strchr(s + p[0], '|');
        if (bar != 0) {
            *bar = 0;
            xpos = g_menuMaxX - 5 - g_pMenu->text_width(bar + 1) * 2 / ScaleXCoordinate(2);
        }

        DrawTextScaledAtPosition(s + p[0], g_menuLeft + 5, g_menuTop + k * g_menuRowHeight + 5,
            (col < 0) ? g_headingColor : ((g_highlightItems & (1 << col)) ? 0x800003ffu : g_itemColor));
        if (bar != 0)
            DrawTextScaledAtPosition(bar + 1, xpos, g_menuTop + k * g_menuRowHeight + 5,
                (col < 0) ? g_headingColor : ((g_highlightItems & (1 << col)) ? 0x800003ffu : g_itemColor));

        if (g_menuTrackHover != 0 && col >= 0 && col == sel) {
            DrawTextScaledAtPosition(s + p[0], g_menuLeft + 5, g_menuTop + k * g_menuRowHeight + 5, g_selectedColor);
            if (bar != 0)
                DrawTextScaledAtPosition(bar + 1, xpos, g_menuTop + k * g_menuRowHeight + 5, g_selectedColor);
        }

        if (bar != 0)
            *bar = '|';
        s[p[1] - 1] = '\n';
        if (s[p[1]] == ' ' || s[p[1]] == '_')
            col++;
    }

    if (g_menuJustDraw == 0)
        g_simScreen.RenderFullSurface(0);

    return sel;
}

void DrawRectOutline(int x, int y, int w, int h, int color)
{
    int sx = ScaleXCoordinate(x);
    int sy = ScaleXCoordinate(y);
    int sw = ScaleXCoordinate(w);
    int sh = ScaleXCoordinate(h);

    DrawSurfaceLine(g_pMenu, sx, sy, sx + sw, sy, color);
    DrawSurfaceLine(g_pMenu, sx, sy + sh, sx + sw, sy + sh, color);
    DrawSurfaceLine(g_pMenu, sx + sw, sy, sx + sw, sy + sh, color);
    DrawSurfaceLine(g_pMenu, sx, sy, sx, sy + sh, color);
}

int g_menuSpare[3] = {0};
