#include "jgldefs.h"
#include "ListBoxCtl.h"
#include "ClassListPush.h"

Dialog::Dialog()
{
    m_sizeFlags = g_panelSizeFlags;
    m_originX = 0;
    m_originY = 0;
    m_fixedWidth = g_panelLayout[0];
    m_fixedHeight = g_panelSizing[0];
    m_selected = 0;
    m_hotIndex = -1;
    m_topPadding = 0;
    m_bottomPadding = 0;
    m_padding = g_panelSizing[2];
    m_inset = g_panelLayout[1];
    m_indent = g_panelSizing[1];
    m_fixedRowHeight = -1;
    m_columnCount = g_panelLayout[2];
    m_maxRows = g_panelLayout[3];
    m_layoutColumns = 0;
    m_columnStride = 0;
    m_width = 0;
    m_height = 0;
    m_contentWidth = 0;
    m_rowHeight = 0;
    m_backColor = g_panelLayout[4];
    m_itemStyle[0] = g_panelItemStyle[0];
    m_itemStyle[1] = g_panelItemStyle[1];
    m_itemStyle[2] = g_panelItemStyle[2];
    m_itemStyle[3] = g_panelItemStyle[3];
    m_rowFonts[0] = g_panelFonts[0];
    m_textColor[0] = g_panelTextColor[0];
    m_textShadowColor[0] = g_panelShadowColor[0];
    m_textShadowDx[0] = g_panelShadowDx[0];
    m_textShadowDy[0] = g_panelShadowDy[0];
    m_rowFonts[1] = g_panelFonts[1];
    m_textColor[1] = g_panelTextColor[1];
    m_textShadowColor[1] = g_panelShadowColor[1];
    m_textShadowDx[1] = g_panelShadowDx[1];
    m_textShadowDy[1] = g_panelShadowDy[1];
    m_rowFonts[2] = g_panelFonts[2];
    m_textColor[2] = g_panelTextColor[2];
    m_textShadowColor[2] = g_panelShadowColor[2];
    m_textShadowDx[2] = g_panelShadowDx[2];
    m_textShadowDy[2] = g_panelShadowDy[2];
    m_arenaSize = -1;
}

Dialog::~Dialog()
{
    close();
}

void Dialog::close()
{
    m_items.Clear();
    m_arena.shutdown();
    m_sharedArena = 0;
    m_sizeFlags = g_panelSizeFlags;
    m_originX = 0;
    m_originY = 0;
    m_fixedWidth = g_panelLayout[0];
    m_fixedHeight = g_panelSizing[0];
    m_selected = 0;
    m_hotIndex = -1;
    m_topPadding = 0;
    m_bottomPadding = 0;
    m_padding = g_panelSizing[2];
    m_inset = g_panelLayout[1];
    m_fixedRowHeight = -1;
    m_indent = g_panelSizing[1];
    m_columnCount = g_panelLayout[2];
    m_maxRows = g_panelLayout[3];
    m_layoutColumns = 0;
    m_columnStride = 0;
    m_width = 0;
    m_height = 0;
    m_contentWidth = 0;
    m_rowHeight = 0;
    m_backColor = g_panelLayout[4];
    m_itemStyle[0] = g_panelItemStyle[0];
    m_itemStyle[1] = g_panelItemStyle[1];
    m_itemStyle[2] = g_panelItemStyle[2];
    m_itemStyle[3] = g_panelItemStyle[3];
    m_rowFonts[0] = g_panelFonts[0];
    m_textColor[0] = g_panelTextColor[0];
    m_textShadowColor[0] = g_panelShadowColor[0];
    m_textShadowDx[0] = g_panelShadowDx[0];
    m_textShadowDy[0] = g_panelShadowDy[0];
    m_rowFonts[1] = g_panelFonts[1];
    m_textColor[1] = g_panelTextColor[1];
    m_textShadowColor[1] = g_panelShadowColor[1];
    m_textShadowDx[1] = g_panelShadowDx[1];
    m_textShadowDy[1] = g_panelShadowDy[1];
    m_rowFonts[2] = g_panelFonts[2];
    m_textColor[2] = g_panelTextColor[2];
    m_textShadowColor[2] = g_panelShadowColor[2];
    m_textShadowDx[2] = g_panelShadowDx[2];
    m_textShadowDy[2] = g_panelShadowDy[2];
    m_arenaSize = -1;
}

int Dialog::clear_strings()
{
    if (m_arenaSize < 0)
        return 1;
    m_items.Clear();
    if (m_arenaSize) {
        if (m_arena.init(m_arenaSize))
            return 4;
        m_items.Reset(&m_arena);
        return 0;
    }
    m_items.Reset(0);
    return 0;
}

int Dialog::InitOwnArenaAt(int x, int y, int width, int height, int bytes)
{
    close();
    m_originX = x;
    m_originY = y;
    m_fixedWidth = width;
    m_fixedHeight = height;
    PinSize();
    m_arenaSize = bytes;
    if (bytes) {
        if (m_arena.init(bytes))
            return 4;
        m_items.Reset(&m_arena);
        return 0;
    }
    m_items.Reset(0);
    return 0;
}

int Dialog::InitOwnArenaAt(Rect *rect, int bytes)
{
    if (!rect)
        return 3;
    return InitOwnArenaAt(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, bytes);
}

int Dialog::InitOwnArena(int bytes)
{
    close();
    m_arenaSize = bytes;
    if (bytes) {
        if (m_arena.init(bytes))
            return 4;
        m_items.Reset(&m_arena);
        return 0;
    }
    m_items.Reset(0);
    return 0;
}

int Dialog::InitSharedArena(Heap *arena)
{
    close();
    m_arenaSize = -1;
    m_sharedArena = arena;
    m_items.Reset(arena);
    return 0;
}

int Dialog::item(const char *text, int idx)
{
    if (!text)
        return 3;
    return m_items.Push(text, 0, 1, idx);
}

void Dialog::set_selected_id(int key)
{
    int i = 0;
    if (m_items.m_head != 0) {
        int n = m_items.m_count;
        m_items.m_curIndex = 0;
        m_items.m_cur = m_items.m_head;
        for (;;) {
            if (i >= n)
                break;
            if (m_items.m_cur->key == key)
                break;
            m_items.m_curIndex = m_items.m_curIndex + 1;
            i++;
            m_items.m_cur = m_items.m_cur->next;
        }
    }
    m_selected = m_items.m_curIndex;
}

int Dialog::get_selected_id()
{
    m_items.SetPos(m_selected);
    if (m_items.m_head)
        return m_items.m_cur->key;
    return 0;
}

int Dialog::id_to_pos(int key)
{
    m_items.SeekKey(key);
    return m_items.m_curIndex;
}

int Dialog::pos_to_id(int idx)
{
    m_items.SetPos(idx);
    return m_items.KeyAtCursor();
}

int Dialog::set_dialog_font(Font *font, Font *braceFont, Font *bracketFont)
{
    if (font == 0)
        return 3;
    if (font->m_owner)
        m_rowFonts[0] = font;
    m_rowFonts[1] = braceFont;
    m_rowFonts[2] = bracketFont;
    return 0;
}

void Dialog::set_dialog_text_color(int color, int shadowColor, int shadowDx, int shadowDy)
{
    m_textColor[0] = color;
    m_textShadowColor[0] = shadowColor;
    m_textShadowDx[0] = shadowDx;
    m_textShadowDy[0] = shadowDy;
}

void Dialog::set_dialog_text_color2(int color, int shadowColor, int shadowDx, int shadowDy)
{
    m_textColor[1] = color;
    m_textShadowColor[1] = shadowColor;
    m_textShadowDx[1] = shadowDx;
    m_textShadowDy[1] = shadowDy;
}

void Dialog::set_dialog_text_color3(int color, int shadowColor, int shadowDx, int shadowDy)
{
    m_textColor[2] = color;
    m_textShadowColor[2] = shadowColor;
    m_textShadowDx[2] = shadowDx;
    m_textShadowDy[2] = shadowDy;
}

int Dialog::set_def_dialog_font(Font *font, Font *braceFont, Font *bracketFont)
{
    if (font == 0)
        return 3;
    if (font->m_owner)
        g_panelFonts[0] = font;
    g_panelFonts[1] = braceFont;
    g_panelFonts[2] = bracketFont;
    return 0;
}

void Dialog::set_def_dialog_text_color(int color, int shadowColor, int shadowDx, int shadowDy, int row)
{
    if (row < 0 || row > 3)
        row = 0;
    g_panelTextColor[row] = color;
    g_panelShadowColor[row] = shadowColor;
    g_panelShadowDx[row] = shadowDx;
    g_panelShadowDy[row] = shadowDy;
}

int g_panelSizing[3];
Font *g_panelFonts[3];
int g_panelItemStyle[4] = {0};

int g_panelLayout[5] = {320, 6, 1, 12, 255};
int g_panelTextColor[3] = {0, -1, -1};
int g_panelShadowColor[3] = {-1, -1, -1};
int g_panelShadowDx[3] = {1, 1, 1};
int g_panelShadowDy[3] = {1, 1, 1};
int g_panelSizeFlags = 3;

int g_panelSpare = 0;
