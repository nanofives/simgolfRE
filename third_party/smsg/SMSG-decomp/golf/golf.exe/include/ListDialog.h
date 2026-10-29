#ifndef LISTDIALOG_H
#define LISTDIALOG_H

int ShowListDialogWithParams(const char *menu, int left, int top);
int ShowListSelectionDialog(const char *menu, int mode);
int RenderMenuWithSelection(char *s, int sel);
void DrawRectOutline(int x, int y, int w, int h, int color);

extern unsigned g_menuAccentColor;
extern char g_itemHotkeys[32];
extern unsigned g_highlightItems;
extern int g_hideSelection;
extern int g_firstMenuDraw;
extern int g_lineOffsets[32];
extern int g_menuJustDraw;
extern unsigned g_headingColor;
extern int g_nMenuLines;
extern int g_nMenuItems;
extern int g_menuFrameDrawn;
extern int g_menuMaxX;
extern int g_menuMaxY;
extern int g_widestLine;
extern unsigned g_itemColor;
extern unsigned g_selectedColor;
extern int g_menuLeft;
extern int g_menuTop;
extern int g_menuShowOK;
extern int g_menuTrackHover;
extern int g_firstItemLine;
extern unsigned g_disabledItemMask;
extern int g_menuRightClick;
extern int g_menuTallFrame;
extern int g_menuMouseDown;
extern int g_menuInitialSel;
extern int g_menuLastSel;
extern int g_menuRowHeight;

#endif
