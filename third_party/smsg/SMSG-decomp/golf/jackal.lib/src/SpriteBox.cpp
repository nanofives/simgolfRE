#include "jgldefs.h"
#include "SpriteBox.h"
#include "Sprite.h"

SpriteBox::SpriteBox()
{
    m_selectionBevel = 0;
    m_scaleNum = 1;
    m_scaleDen = 1;
    m_cellMetrics[0] = 0;
    m_cellMetrics[1] = 0;
    m_cellMetrics[2] = 0;
    m_cellMetrics[3] = 0;
    m_onDrawCell = 0;
    m_onLeftClick = 0;
    m_onLeftDown = 0;
    m_onLeftUp = 0;
    m_onRightClick = 0;
    m_onRightDown = 0;
    m_onRightUp = 0;
    m_onLeftDoubleClick = 0;
    m_onRightDoubleClick = 0;
    m_selectionColor = g_spriteBoxSelectionColor;
    set_text_color(g_panelTextColor[0], g_panelShadowColor[0], g_panelShadowDx[0], g_panelShadowDy[0]);
    set_text_color2(g_panelTextColor[1], g_panelShadowColor[1], g_panelShadowDx[1], g_panelShadowDy[1]);
    set_text_color3(g_panelTextColor[2], g_panelShadowColor[2], g_panelShadowDx[2], g_panelShadowDy[2]);
    SetFontsForStyles(g_panelFonts[0], g_panelFonts[1], g_panelFonts[2], 0);
}


SpriteBox::~SpriteBox()
{
    close();
}

void SpriteBox::close()
{
    GraphicWin::close();
    Dialog::close();
    m_selectionBevel = 0;
    m_scaleNum = 1;
    m_scaleDen = 1;
    m_cellMetrics[0] = 0;
    m_cellMetrics[1] = 0;
    m_cellMetrics[2] = 0;
    m_cellMetrics[3] = 0;
    m_onDrawCell = 0;
    m_onLeftClick = 0;
    m_onLeftDown = 0;
    m_onLeftUp = 0;
    m_onRightClick = 0;
    m_onRightDown = 0;
    m_onRightUp = 0;
    m_onLeftDoubleClick = 0;
    m_onRightDoubleClick = 0;
    m_cells.Clear();
    m_cellHotspots.shutdown();
    m_selectionColor = g_spriteBoxSelectionColor;
    set_text_color(g_panelTextColor[0], g_panelShadowColor[0], g_panelShadowDx[0], g_panelShadowDy[0]);
    set_text_color2(g_panelTextColor[1], g_panelShadowColor[1], g_panelShadowDx[1], g_panelShadowDy[1]);
    set_text_color3(g_panelTextColor[2], g_panelShadowColor[2], g_panelShadowDx[2], g_panelShadowDy[2]);
    SetFontsForStyles(0, 0, 0, 0);
}



int SpriteBox::init(int id)
{
    close();
    set_text_color(g_panelTextColor[0], g_panelShadowColor[0], g_panelShadowDx[0], g_panelShadowDy[0]);
    set_text_color2(g_panelTextColor[1], g_panelShadowColor[1], g_panelShadowDx[1], g_panelShadowDy[1]);
    set_text_color3(g_panelTextColor[2], g_panelShadowColor[2], g_panelShadowDx[2], g_panelShadowDy[2]);
    SetFontsForStyles(g_panelFonts[0], g_panelFonts[1], g_panelFonts[2], 0);
    return Dialog::InitOwnArena(id);
}


int SpriteBox::ResetAndInitAlt(Heap *arena)
{
    close();
    return Dialog::InitSharedArena(arena);
}



int SpriteBox::attach(WidgetBase *owner, int x, int y, int flags)
{
    int rc;
    if (owner == 0)
        return 3;
    calculate_dimensions();
    m_originX = x;
    m_originY = y;
    if ((owner->m_flags & 0x1000) == 0)
        flags |= 0x100000;
    int h = m_height;
    int w = m_width;
    GraphicWin *ro = GraphicWin::is_graphicwin();
    rc = ro->init(x, y, w, h, 0, flags, owner, 0, 0);
    if (rc != 0)
        return rc;
    m_cellHotspots.init(m_cells.m_count);
    Rect r;
    r.SetRect(0, abs(m_rowHeight - m_cellMetrics[1]) >> 1, m_cellMetrics[0], m_rowHeight);
    m_cells.SetPos(-1);
    for (int col = 0; col < m_layoutColumns; ++col) {
        for (int row = 0; row < m_columnStride; ++row) {
            int key = m_cells.NextKey();
            m_cellHotspots.add(key, 0, r.left, r.top, r.right - r.left, r.bottom - r.top, 0);
            r.top += m_rowHeight;
            r.bottom += m_rowHeight;
            if (m_cells.m_curIndex == m_cells.m_count - 1)
                break;
        }
        if (m_cells.m_curIndex == m_cells.m_count - 1)
            break;
        r.bottom = m_cellMetrics[1];
        r.top = abs(m_rowHeight - r.bottom) >> 1;
        r.left += m_inset + m_contentWidth;
        r.right += m_inset + m_contentWidth;
    }
    set_text_color(m_textColor[0], m_textShadowColor[0], m_textShadowDx[0], m_textShadowDy[0]);
    set_text_color2(m_textColor[1], m_textShadowColor[1], m_textShadowDx[1], m_textShadowDy[1]);
    set_text_color3(m_textColor[2], m_textShadowColor[2], m_textShadowDx[2], m_textShadowDy[2]);
    SetFontsForStyles(m_rowFonts[0], m_rowFonts[1], m_rowFonts[2], 0);
    return 0;
}

int SpriteBox::sprite(SpriteObject *icon, const char *text, int id)
{
    if (icon == 0)
        return 3;
    if (m_cells.Push(icon, 0, 0, id) != 0)
        return 4;
    if (text != 0 && m_items.Push(text, 0, 0, id) != 0)
        return 4;
    return 0;
}



int SpriteBox::calculate_dimensions()
{
    if (m_cells.m_count == 0)
        return 7;
    set_text_color(m_textColor[0], m_textShadowColor[0], m_textShadowDx[0], m_textShadowDy[0]);
    set_text_color2(m_textColor[1], m_textShadowColor[1], m_textShadowDx[1], m_textShadowDy[1]);
    set_text_color3(m_textColor[2], m_textShadowColor[2], m_textShadowDx[2], m_textShadowDy[2]);
    SetFontsForStyles(m_rowFonts[0], m_rowFonts[1], m_rowFonts[2], 0);
    m_cellMetrics[2] = 0;
    m_cellMetrics[0] = 0;
    m_cellMetrics[1] = 0;
    SpriteList *cells = &m_cells;
    for (int i = cells->m_count; i > 0; i--) {
        SpriteObject *icon = cells->next_sprite();
        const char *caption = m_items.NextText();
        m_cellMetrics[0] = max(m_cellMetrics[0], icon->m_width * m_scaleNum / m_scaleDen);
        m_cellMetrics[1] = max(m_cellMetrics[1], icon->m_height * m_scaleNum / m_scaleDen);
        int w = text_width(caption);
        m_cellMetrics[2] = max(m_cellMetrics[2], w);
    }
    m_cellMetrics[3] = text_line_height();
    m_rowHeight = m_inset + max(m_cellMetrics[3], m_cellMetrics[1]);
    m_contentWidth = m_cellMetrics[0] + 2 * m_padding + m_cellMetrics[2];
    int rows = m_maxRows;
    m_columnStride = min(m_cells.m_count, rows);
    if ((m_sizeFlags & 1) && m_fixedHeight != 0) {
        m_columnStride = m_fixedHeight / m_rowHeight;
        m_height = m_fixedHeight;
    } else {
        m_height = m_rowHeight * m_columnStride + m_inset;
    }
    if ((m_sizeFlags & 2) && m_fixedWidth != 0) {
        m_layoutColumns = m_fixedWidth / m_contentWidth;
        m_width = m_fixedWidth;
    } else {
        m_layoutColumns = (m_columnStride + m_cells.m_count - 1) / m_columnStride;
        m_width = max(m_fixedWidth, (m_contentWidth + m_inset) * m_layoutColumns - m_inset);
    }
    return 0;
}



void SpriteBox::draw_item(int idx)
{
    Rect r;
    GraphicWin *owner = parent;
    if (owner == 0)
        return;
    int scale = m_scaleNum;
    if (graphsys != 0)
        graphsys->set_sprite_scale(scale, scale, m_scaleDen);
    int col = idx / m_columnStride;
    int x = (m_contentWidth + m_inset) * col;
    int y = (idx - m_columnStride * col) * m_rowHeight + (m_inset >> 1);
    if (m_cells.SetPos(idx) == 0)
        return;
    if (m_items.SetPos(idx) == 0)
        return;
    r.SetRect(x, y, m_contentWidth, m_rowHeight);
    r.left += m_cellMetrics[0] + 4;
    owner->copy(this, m_originX + x, m_originY + y, x, y, m_contentWidth, m_rowHeight);
    y += (m_rowHeight - m_cells.SpriteAtCursor()->m_height * m_scaleNum / m_scaleDen) / 2;
    x += (m_cellMetrics[0] - m_cells.SpriteAtCursor()->m_width * m_scaleNum / m_scaleDen) / 2;
    if (m_onDrawCell == 0) {
        m_cells.SpriteAtCursor()->BlitScaled(this, x, y, 0);
    } else {
        m_onDrawCell(m_cells.SpriteAtCursor(), this, m_cells.KeyAtCursor(), x, y);
        set_text_color(m_textColor[0], m_textShadowColor[0], m_textShadowDx[0], m_textShadowDy[0]);
        set_text_color2(m_textColor[1], m_textShadowColor[1], m_textShadowDx[1], m_textShadowDy[1]);
        set_text_color3(m_textColor[2], m_textShadowColor[2], m_textShadowDx[2], m_textShadowDy[2]);
        SetFontsForStyles(m_rowFonts[0], m_rowFonts[1], m_rowFonts[2], 0);
    }
    if (m_selected == idx && m_selectionBevel != 0) {
        box(x, ++y, x + m_cells.SpriteAtCursor()->m_width * m_scaleNum / m_scaleDen - 1, y + m_rowHeight - 3,
            m_selectionColor, m_selectionColor);
    }
    const char *s = m_items.TextAtCursor();
    if (s)
        RenderTextVerticallyCentered(s, &r, strlen(s));
}

void SpriteBox::on_mouse_move(int, int)
{
}

void SpriteBox::on_mouse_leave(int, int)
{
}



void SpriteBox::on_left_click(int x, int y)
{
    if (m_cellHotspots.check(x, y, &y, 0) == -1)
        return;
    if (m_onLeftClick)
        m_onLeftClick(y);
    int old = m_selected;
    m_selected = id_to_pos(y);
    draw_item(old);
    draw_item(m_selected);
    update();
}

void SpriteBox::on_left_down(int x, int y)
{
    if (m_cellHotspots.check(x, y, &y, 0) == -1)
        return;
    if (m_onLeftDown)
        m_onLeftDown(y);
}

void SpriteBox::on_left_up(int x, int y)
{
    if (m_cellHotspots.check(x, y, &y, 0) == -1)
        return;
    if (m_onLeftUp)
        m_onLeftUp(y);
}

void SpriteBox::on_right_click(int x, int y)
{
    if (m_cellHotspots.check(x, y, &y, 0) == -1)
        return;
    if (m_onRightClick)
        m_onRightClick(y);
}

void SpriteBox::on_right_down(int x, int y)
{
    if (m_cellHotspots.check(x, y, &y, 0) == -1)
        return;
    if (m_onRightDown)
        m_onRightDown(y);
}

void SpriteBox::on_right_up(int x, int y)
{
    if (m_cellHotspots.check(x, y, &y, 0) == -1)
        return;
    if (m_onRightUp)
        m_onRightUp(y);
}

void SpriteBox::on_left_double_click(int x, int y)
{
    if (m_cellHotspots.check(x, y, &y, 0) == -1)
        return;
    if (m_onLeftDoubleClick)
        m_onLeftDoubleClick(y);
}

void SpriteBox::on_right_double_click(int x, int y)
{
    if (m_cellHotspots.check(x, y, &y, 0) == -1)
        return;
    if (m_onRightDoubleClick)
        m_onRightDoubleClick(y);
}



void SpriteBox::on_redraw()
{
    GraphicWin *owner = parent;
    if (!owner)
        return;
    AssetSurface *dst = this;
    owner->copy(dst, m_originX, m_originY, 0, 0, GetSurfaceWidth(), GetSurfaceHeight());
    for (int i = 0; i < m_cells.m_count; ++i)
        draw_item(i);
}

int SpriteBox::on_key_down(int vk)
{
    int old;
    switch (vk) {
    case 0x26:
    case 0x68:
        if (m_selected == 0)
            return 1;
        m_selected = m_selected - 1;
        draw_item(m_selected + 1);
        break;
    case 0x28:
    case 0x62:
        if (m_selected == m_cells.m_count - 1)
            return 1;
        m_selected = m_selected + 1;
        draw_item(m_selected - 1);
        break;
    case 0x25:
    case 0x64:
        if (m_layoutColumns == 1)
            return 1;
        old = m_selected;
        if (old < m_columnStride)
            return 1;
        m_selected = old - m_columnStride;
        draw_item(old);
        break;
    case 0x27:
    case 0x66:
        if (m_layoutColumns == 1)
            return 1;
        old = m_selected;
        if (old >= (m_layoutColumns - 1) * m_columnStride)
            return 1;
        m_selected = old + m_columnStride;
        if (m_selected > m_cells.m_count - 1) {
            m_selected = old;
            return 1;
        }
        draw_item(old);
        break;
    default: return 0;
    }
    draw_item(m_selected);
    update();
    return 1;
}

void SpriteBox::on_dialog_focus(int)
{
    redraw();
}


int SpriteBox::id_to_pos(int id)
{
    if (m_cells.m_head) {
        m_cells.m_curIndex = 0;
        m_cells.m_cur = m_cells.m_head;
        for (int i = 0; i < m_cells.m_count; i++) {
            if (m_cells.m_cur->key == id)
                break;
            m_cells.m_curIndex++;
            m_cells.m_cur = m_cells.m_cur->next;
        }
    }
    return m_cells.m_curIndex;
}

int SpriteBox::pos_to_id(int idx)
{
    m_cells.SetPos(idx);
    if (m_cells.m_head)
        return m_cells.m_cur->key;
    return 0;
}



int g_spriteBoxSelectionColor;

int g_spriteBoxSpare[36] = {0};
