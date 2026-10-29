#include "jgldefs.h"
#include "ListBoxCtl.h"
#include "Sprite.h"
#include "ImageSet.h"



SpriteObject ListBoxCtl::s_normalCell;
SpriteObject ListBoxCtl::s_glowCell;
SpriteObject ListBoxCtl::s_highlightCell;
SpriteObject *ListBoxCtl::s_defaultSprites[3] = {&s_normalCell, &s_highlightCell, &s_glowCell};

ListBoxCtl::ListBoxCtl()
{
    onSelect = 0;
    onActivate = 0;
    m_selectedSprite = s_defaultSprites[1];
    m_hotSprite = s_defaultSprites[2];
    m_normalSprite = s_defaultSprites[0];
    m_hotTextColor = m_textColor[0];
    m_selectedTextColor = m_textColor[0];
}

void ListBoxCtl::close()
{
    onSelect = 0;
    onActivate = 0;
    m_selectedSprite = s_defaultSprites[1];
    m_hotSprite = s_defaultSprites[2];
    m_normalSprite = s_defaultSprites[0];
    Dialog::close();
    GraphicWin::close();
}




int ListBoxCtl::attach(WidgetBase *owner, int x, int y, int flags)
{
    int rc;
    if (owner == 0)
        return 3;
    rc = calculate_dimensions();
    if (rc != 0)
        return rc;
    m_originX = x;
    m_originY = y;
    if ((owner->m_flags & 0x1000) == 0)
        flags |= 0x100000;
    int h = m_height;
    int w = m_width;
    GraphicWin *ro = GraphicWin::is_graphicwin();
    return ro->init(x, y, w, h, 0, flags, owner, 0, 0);
}














static inline int WidestOf(int a, int b, const SpriteObject *sprite)
{
    int m;
    if ((m = sprite->m_width) <= max(a, b)) {
        if (a > b)
            b = a;
        m = b;
    }
    return m;
}

static inline int TallestOf(int a, int b, const SpriteObject *sprite)
{
    int m;
    if ((m = sprite->m_height) <= max(a, b)) {
        if (a > b)
            b = a;
        m = b;
    }
    return m;
}

int ListBoxCtl::calculate_dimensions()
{
    int widest;
    int cell;
    int i;
    if (m_items.m_count == 0)
        return 7;
    if (m_indent == 0 && m_normalSprite != 0 && m_selectedSprite != 0 && m_hotSprite != 0) {
        m_indent = WidestOf(m_normalSprite->m_width, m_selectedSprite->m_width, m_hotSprite);
    }
    if (m_fixedRowHeight == -1) {
        if (m_normalSprite != 0 && m_selectedSprite != 0 && m_hotSprite != 0) {
            m_rowHeight = TallestOf(m_normalSprite->m_height, m_selectedSprite->m_height, m_hotSprite);
        } else
            m_rowHeight = 0;
    } else {
        m_rowHeight = m_fixedRowHeight;
    }
    m_columnStride = min(m_items.m_count, m_maxRows);
    if (m_columnStride < m_items.m_count) {
        m_columnCount = m_items.m_count / m_columnStride + 1;
        m_columnStride = m_items.m_count / m_columnCount;
        if (m_columnCount * m_columnStride < m_items.m_count)
            m_columnStride = m_columnStride + 1;
    }
    if ((m_sizeFlags & 2) && m_fixedWidth != 0)
        cell = m_fixedWidth / m_columnCount - m_padding * 2 - m_indent;
    else
        cell = 100000;
    widest = 0;
    for (i = 0; i < m_items.m_count; i++) {
        int w = text_width(m_items.NextText());
        if (w > widest)
            widest = w;
        m_items.current_entry()->attr = w / cell + 1;
    }
    m_contentWidth = m_inset + m_indent + widest;
    m_rowHeight = max(m_rowHeight, text_height());
    if ((m_sizeFlags & 2) && m_fixedWidth != 0) {
        m_width = m_fixedWidth;
        m_contentWidth = (m_fixedWidth - m_padding * 2) / m_columnCount - m_inset;
    } else {
        m_width = max(m_padding * 2 + m_fixedWidth, m_contentWidth * m_columnCount + m_padding * 2);
        m_contentWidth = (m_width - m_padding * 2) / m_columnCount - m_inset;
        m_width = m_width - m_inset;
    }
    m_layoutColumns = m_columnCount;
    if ((m_sizeFlags & 1) && m_fixedHeight != 0) {
        m_height = m_fixedHeight;
        m_columnStride = m_fixedHeight / m_rowHeight;
    } else {
        m_height = (m_rowHeight + m_inset) * m_columnStride;
    }
    return 0;
}

void ListBoxCtl::draw_item(int idx, int selected)
{
    Rect r;
    int y;
    int px;
    int rowH;
    int state;
    int k;
    char *bar;
    GraphicWin *dst = parent;
    if (dst == 0)
        return;
    if (graphsys != 0)
        graphsys->set_sprite_scale(1, 1, 1);
    set_text_color(m_textColor[0], m_textShadowColor[0], m_textShadowDx[0], m_textShadowDy[0]);
    set_text_color2(m_textColor[1], m_textShadowColor[1], m_textShadowDx[1], m_textShadowDy[1]);
    set_text_color3(m_textColor[2], m_textShadowColor[2], m_textShadowDx[2], m_textShadowDy[2]);
    SetFontsForStyles(m_rowFonts[0], m_rowFonts[1], m_rowFonts[2], 0);
    y = 0;
    m_items.SetPos(-1);
    for (k = idx; k > 0; k--) {
        if (m_items.NextAttr() > 1)
            y += max(m_rowHeight, text_line_height() * m_items.AttrAtCursor()) - m_rowHeight;
    }
    px = (m_contentWidth + m_inset) * (idx / m_columnStride) + m_padding;
    int ty = y + (idx - (idx / m_columnStride) * m_columnStride) * (m_rowHeight + m_inset) + (m_inset >> 1);
    if (m_items.SetPos(idx) == 0)
        return;
    rowH = max(m_rowHeight, text_line_height() * m_items.AttrAtCursor());
    r.SetRect(px, ty, m_contentWidth, rowH);
    if (selected != 0)
        state = 2;
    else
        state = (m_hotIndex == idx);
    if (dst->on_dialog_item_back_draw(this, pos_to_id(idx), state, &r) == 0) {
        dst->copy(this, m_originX + px, m_originY + ty, px, ty, m_contentWidth, rowH);
        if (m_selectedSprite != 0 && m_normalSprite != 0) {
            if (selected != 0) {
                m_selectedSprite->BlitScaled(this, px, ((r.bottom - m_selectedSprite->m_height - r.top) >> 1) + ty, 0);
                set_text_color(m_selectedTextColor, -1, 2, 2);
            } else if (m_hotIndex != idx) {
                m_normalSprite->BlitScaled(this, px, ((r.bottom - m_normalSprite->m_height - r.top) >> 1) + ty, 0);
            } else {
                m_hotSprite->BlitScaled(this, px, ((r.bottom - m_hotSprite->m_height - r.top) >> 1) + ty, 0);
                set_text_color(m_hotTextColor, -1, 2, 2);
            }
        }
    }
    r.left += m_indent;
    if (m_items.AttrAtCursor() == 1) {
        bar = strchr(m_items.TextAtCursor(), '|');
        if (bar == 0) {
            DrawLeftAligned(m_items.TextAtCursor(), &r);
        } else {
            *bar = 0;
            DrawLeftAligned(m_items.TextAtCursor(), &r);
            *bar = '|';
            bar++;
            DrawRightAligned(bar, &r);
        }
    } else {
        bar = strchr(m_items.TextAtCursor(), '|');
        if (bar == 0) {
            RenderMultilineTextInRect((char *)m_items.TextAtCursor(), &r, 0, 0);
        } else {
            *bar = 0;
            RenderMultilineTextInRect((char *)m_items.TextAtCursor(), &r, 0, 0);
            *bar = '|';
            bar++;
            DrawRightAligned(bar, &r);
        }
    }
    if (m_itemStyle[3] != -1 && selected != 0) {
        r.left -= 2;
        dotted_box(&r, m_itemStyle[3]);
    }
}




void ListBoxCtl::on_left_down(int x, int y)
{
    int first = 1;
    g_renderingWidget = parent;
    m_items.SetPos(-1);
    while (y > 0) {
        if (first == 0 && m_items.m_curIndex == m_items.m_count - 1)
            return;
        first = 0;
        int lines = m_items.NextAttr();
        int step = text_line_height() * lines;
        y -= max(m_rowHeight + m_inset, step);
    }
    int idx = x / (m_contentWidth + m_inset) * m_columnStride + m_items.m_curIndex;
    if (m_selected == idx) {
        if (onActivate != 0)
            onActivate(get_selected_id());
        if ((m_sizeFlags & 4) && parent != 0)
            parent->on_button_clicked(-1);
    } else {
        draw_item(m_selected, 0);
        m_selected = idx;
        if (onSelect != 0)
            onSelect(m_selected);
        draw_item(idx, 1);
        m_hotIndex = -1;
        m_notifiedHotIndex = -1;
        soft_update();
        if ((m_sizeFlags & 4) && parent != 0)
            parent->on_button_clicked(-1);
    }
}

























int ListBoxCtl::on_key_down(int vk)
{
    g_renderingWidget = parent;

    int old4 = m_hotIndex;
    int oldSel = m_selected;

    switch (vk) {
    case 0x26:
    case 0x68:
        m_selected = m_selected - 1;
        m_selected = m_selected > 0 ? m_selected : 0;
        break;
    case 0x28:
    case 0x62:
        m_selected = m_selected + 1;
        m_selected = m_selected < m_items.m_count - 1 ? m_selected : m_items.m_count - 1;
        break;
    case 0x25:
    case 0x64:
        m_selected += m_columnStride;
        if (m_selected >= m_items.m_count)
            m_selected -= m_columnStride;
        break;
    case 0x27:
    case 0x66:
        m_selected -= m_columnStride;
        if (m_selected < 0)
            m_selected += m_columnStride;
        break;
    default: return 0;
    }

    m_hotIndex = -1;
    m_notifiedHotIndex = -1;
    if (old4 != -1)
        draw_item(old4, 0);
    draw_item(oldSel, 0);
    if (onSelect != 0)
        onSelect(m_selected);
    draw_item(m_selected, 1);
    soft_update();
    return 1;
}

void ListBoxCtl::on_left_double_click(int x, int y)
{
    g_renderingWidget = parent;
    if (onActivate)
        onActivate(get_selected_id());
}



void ListBoxCtl::on_redraw()
{
    GraphicWin *owner = parent;
    g_renderingWidget = owner;
    if (owner == 0)
        return;
    if (owner->on_dialog_back_draw(this) == 0) {
        AssetSurface *dst = this;
        owner->copy(dst, m_originX, m_originY, 0, 0, GetSurfaceWidth(), GetSurfaceHeight());
    }
    for (int i = 0; i < m_items.m_count; i++) {
        if (m_selected == i)
            draw_item(i, 1);
        else
            draw_item(i, 0);
    }
    soft_update();
}

void ListBoxCtl::on_dialog_focus(int on)
{
    redraw();
}

void ListBoxCtl::on_mouse_move(int x, int y)
{
    int first = 1;
    m_items.SetPos(-1);
    while (y > 0) {
        if (first == 0 && m_items.m_curIndex == m_items.m_count - 1)
            return;
        first = 0;
        int lines = m_items.NextAttr();
        int step = text_line_height() * lines;
        y -= max(m_rowHeight + m_inset, step);
    }
    int idx = x / (m_contentWidth + m_inset) * m_columnStride + m_items.m_curIndex;
    if (idx == m_selected) {
        if (m_hotIndex == -1)
            return;
        m_hotIndex = -1;
        redraw();
    } else {
        m_hotIndex = idx;
        if (m_hotIndex != m_notifiedHotIndex) {
            if (m_onHoverChanged != 0)
                m_onHoverChanged->play();
            m_notifiedHotIndex = m_hotIndex;
        }
        redraw();
    }
}

void ListBoxCtl::on_mouse_leave(int x, int y)
{
    m_hotIndex = -1;
    m_notifiedHotIndex = -1;
    redraw();
}


int ListBoxCtl::init_class()
{
    AssetSurface sheet;
    int rc = sheet.load_pcx("jackal.pcx", 0, 0, 0x100, 2);
    if (rc)
        return rc;
    s_normalCell.extract(&sheet, 1, 0x23, 0x20, 0x20, 1, 1);
    s_glowCell.extract(&sheet, 1, 0x23, 0x20, 0x20, 1, 1);
    s_highlightCell.extract(&sheet, 0x22, 0x23, 0x20, 0x20, 1, 1);
    return 0;
}

void ListBoxCtl::close_class()
{
    s_highlightCell.close();
    s_glowCell.close();
    s_normalCell.close();
}
