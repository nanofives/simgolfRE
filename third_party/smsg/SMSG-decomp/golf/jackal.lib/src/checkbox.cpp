#include "jgldefs.h"
#include "CheckBoxCtl.h"
#include "ImageSet.h"

SpriteObject *CheckBoxCtl::s_defaultSprites[2] = {&s_checkBoxEmpty, &s_checkBoxTicked};

CheckBoxCtl::CheckBoxCtl()
{
    m_focus = 0;
    m_onChange = 0;
    m_onActivate = 0;
    m_checkedSprite = s_defaultSprites[1];
    m_uncheckedSprite = s_defaultSprites[0];
}

CheckBoxCtl::~CheckBoxCtl()
{
    close();
}

void CheckBoxCtl::close()
{
    m_focus = 0;
    m_onChange = 0;
    m_onActivate = 0;
    m_checkedSprite = s_defaultSprites[1];
    m_uncheckedSprite = s_defaultSprites[0];
    Dialog::close();
    GraphicWin::close();
}

int CheckBoxCtl::attach(WidgetBase *owner, int x, int y, int flags)
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
    set_text_color(m_textColor[0], m_textShadowColor[0], m_textShadowDx[0], m_textShadowDy[0]);
    set_text_color2(m_textColor[1], m_textShadowColor[1], m_textShadowDx[1], m_textShadowDy[1]);
    set_text_color3(m_textColor[2], m_textShadowColor[2], m_textShadowDx[2], m_textShadowDy[2]);
    SetFontsForStyles(m_rowFonts[0], m_rowFonts[1], m_rowFonts[2], 0);
    return 0;
}

int CheckBoxCtl::item(const char *text, int key, int checked)
{
    if (text == 0)
        return 3;
    set_state_pos(m_items.m_count, checked);
    if (m_items.Push(text, 0, 0, key) != 0)
        return 4;
    return 0;
}

void CheckBoxCtl::flip_state_id(int key)
{
    if (get_state_id(key))
        set_state_id(key, 0);
    else
        set_state_id(key, 1);
}

void CheckBoxCtl::flip_state_pos(int i)
{
    if (get_state_pos(i))
        set_state_pos(i, 0);
    else
        set_state_pos(i, 1);
}

void CheckBoxCtl::set_state_id(int key, int on)
{
    StringList *list = &m_items;
    if (list->m_head) {
        int n = list->m_count;
        list->m_curIndex = 0;
        list->m_cur = list->m_head;
        for (int i = 0; i < n; i++) {
            if (list->m_cur->key == key)
                break;
            list->m_curIndex++;
            list->m_cur = list->m_cur->next;
        }
    }
    if (on)
        m_selected |= 1 << m_items.m_curIndex;
    else
        m_selected &= ~(1 << m_items.m_curIndex);
}

int CheckBoxCtl::get_state_id(int key)
{
    StringList *list = &m_items;
    if (list->m_head) {
        int n = list->m_count;
        list->m_curIndex = 0;
        list->m_cur = list->m_head;
        for (int i = 0; i < n; i++) {
            if (list->m_cur->key == key)
                break;
            list->m_curIndex++;
            list->m_cur = list->m_cur->next;
        }
    }
    return (1 << m_items.m_curIndex) & m_selected;
}

void CheckBoxCtl::set_state_pos(int i, int on)
{
    if (on)
        m_selected |= 1 << i;
    else
        m_selected &= ~(1 << i);
}

int CheckBoxCtl::get_state_pos(int i)
{
    return (1 << i) & m_selected;
}

void CheckBoxCtl::set_state_flag(int mask)
{
    m_selected = mask;
    redraw();
}

int CheckBoxCtl::calculate_dimensions()
{
    if (m_items.m_count == 0)
        return 7;
    set_text_color(m_textColor[0], m_textShadowColor[0], m_textShadowDx[0], m_textShadowDy[0]);
    set_text_color2(m_textColor[1], m_textShadowColor[1], m_textShadowDx[1], m_textShadowDy[1]);
    set_text_color3(m_textColor[2], m_textShadowColor[2], m_textShadowDx[2], m_textShadowDy[2]);
    SetFontsForStyles(m_rowFonts[0], m_rowFonts[1], m_rowFonts[2], 0);
    if (m_indent == 0 && m_checkedSprite != 0 && m_uncheckedSprite != 0) {
        int w = m_checkedSprite->m_width;
        if (m_uncheckedSprite->m_width > m_checkedSprite->m_width)
            w = m_uncheckedSprite->m_width;
        m_indent = w;
    }
    if (m_fixedRowHeight == -1) {
        if (m_uncheckedSprite != 0 && m_checkedSprite != 0) {
            int h = m_checkedSprite->m_height;
            if (m_uncheckedSprite->m_height > m_checkedSprite->m_height)
                h = m_uncheckedSprite->m_height;
            m_rowHeight = h;
        } else
            m_rowHeight = 0;
    } else {
        m_rowHeight = m_fixedRowHeight;
    }
    m_columnStride = m_items.m_count;
    int maxw = 0;
    for (int i = 0; i < m_items.m_count; i++) {
        int w = text_width(m_items.NextText());
        if (w > maxw)
            maxw = w;
    }
    m_contentWidth = m_inset + m_indent + maxw;
    m_rowHeight = max(m_rowHeight, text_height());
    if ((m_sizeFlags & 2) && m_fixedWidth != 0) {
        m_width = m_fixedWidth;
        m_contentWidth = (m_fixedWidth - 2 * m_padding) / m_columnCount - m_inset;
    } else {
        m_width = max(m_fixedWidth + 2 * m_padding, m_contentWidth * m_columnCount + 2 * m_padding);
        m_contentWidth = (m_width - 2 * m_padding) / m_columnCount - m_inset;
        m_width = m_width - m_inset;
    }
    m_layoutColumns = m_columnCount;
    if ((m_sizeFlags & 1) && m_fixedHeight != 0) {
        m_height = m_fixedHeight;
        m_columnStride = m_fixedHeight / m_rowHeight;
        return 0;
    }
    m_height = (m_rowHeight + m_inset) * m_columnStride;
    return 0;
}

void CheckBoxCtl::draw_item(int idx, int checked, int focused)
{
    Rect r;
    int y;
    int px;
    int rowH;
    int state;
    int k;
    char *bar;
    GraphicWin *owner = parent;
    if (owner == 0)
        return;
    if (graphsys != 0)
        graphsys->set_sprite_scale(1, 1, 1);
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
    if (checked != 0)
        state = 2;
    else
        state = (m_hotIndex == idx);
    if (owner->on_dialog_item_back_draw(this, pos_to_id(idx), state, &r) == 0) {
        owner->copy(this, m_originX + px, m_originY + ty, px, ty, m_contentWidth, rowH);
        if (m_checkedSprite != 0 && m_uncheckedSprite != 0) {
            if (checked != 0)
                m_checkedSprite->BlitScaled(this, px, ((r.bottom - m_checkedSprite->m_height - r.top) >> 1) + ty, 0);
            else
                m_uncheckedSprite->BlitScaled(this, px, ((r.bottom - m_uncheckedSprite->m_height - r.top) >> 1) + ty,
                    0);
        }
    }
    r.top += m_fonts[0]->m_descent;
    r.left += m_indent;
    if (m_items.AttrAtCursor() == 1) {
        bar = strchr(m_items.TextAtCursor(), '|');
        if (bar == 0) {
            DrawLeftAligned(m_items.TextAtCursor(), &r);
            return;
        }
        *bar = 0;
        DrawLeftAligned(m_items.TextAtCursor(), &r);
        *bar = '|';
        bar++;
        DrawRightAligned(bar, &r);
        return;
    }
    bar = strchr(m_items.TextAtCursor(), '|');
    if (bar == 0) {
        RenderMultilineTextInRect((char *)m_items.TextAtCursor(), &r, 0, 0);
        return;
    }
    *bar = 0;
    RenderMultilineTextInRect((char *)m_items.TextAtCursor(), &r, 0, 0);
    *bar = '|';
    bar++;
    DrawRightAligned(bar, &r);
}

void CheckBoxCtl::on_left_down(int x, int y)
{
    int item;
    int cell;
    g_renderingWidget = parent;
    cell = x / (m_contentWidth + m_inset) * m_columnStride;
    cell += y / (m_rowHeight + m_inset);
    item = pos_to_id(cell);
    flip_state_id(item);
    if (pos_to_id(m_focus) != item) {
        draw_item(m_focus, get_state_pos(m_focus), 0);
        m_focus = id_to_pos(item);
    }
    if (m_onChange != 0)
        m_onChange(GetCheckMask());
    draw_item(id_to_pos(item), get_state_id(item), 1);
    soft_update();
}

int CheckBoxCtl::on_key_down(int vk)
{
    g_renderingWidget = parent;
    draw_item(m_focus, get_state_pos(m_focus), 0);
    switch (vk) {
    case 0x26:
    case 0x68:
        m_focus = m_focus - 1;
        m_focus = max(m_focus, 0);
        break;
    case 0x28:
    case 0x62:
        m_focus = m_focus + 1;
        m_focus = min(m_focus, m_items.m_count - 1);
        break;
    case 0x25:
    case 0x64:
        m_focus += m_columnStride;
        if (m_focus >= m_items.m_count)
            m_focus -= m_columnStride;
        break;
    case 0x27:
    case 0x66:
        m_focus -= m_columnStride;
        if (m_focus < 0)
            m_focus += m_columnStride;
        break;
    case 0x20: flip_state_pos(m_focus); break;
    default: return 0;
    }
    if (m_onChange != 0)
        m_onChange(GetCheckMask());
    draw_item(m_focus, get_state_pos(m_focus), 1);
    soft_update();
    return 1;
}

void CheckBoxCtl::on_left_double_click(int x, int y)
{
    g_renderingWidget = parent;
    if (m_onActivate != 0)
        m_onActivate(GetCheckMask());
}

void CheckBoxCtl::on_redraw()
{
    GraphicWin *owner = parent;
    if (owner == 0)
        return;
    g_renderingWidget = owner;
    if (owner->on_dialog_back_draw(this) == 0) {
        AssetSurface *dst = this;
        owner->copy(dst, m_originX, m_originY, 0, 0, GetSurfaceWidth(), GetSurfaceHeight());
    }
    for (int i = 0; i < m_items.m_count; i++)
        draw_item(i, get_state_pos(i), m_focus == i);
}

void CheckBoxCtl::on_dialog_focus(int on)
{
    redraw();
}

void CheckBoxCtl::on_mouse_move(int x, int y)
{
    int col = x / (m_contentWidth + m_inset);
    int row = y / (m_rowHeight + m_inset);
    int cell = col * m_columnStride + row;
    if (m_selected & (1 << cell)) {
        if (m_hotIndex != -1) {
            m_hotIndex = -1;
            redraw();
        }
    } else {
        m_hotIndex = cell;
        redraw();
    }
}

void CheckBoxCtl::on_mouse_leave(int x, int y)
{
    m_hotIndex = -1;
    redraw();
}

int CheckBoxCtl::init_class()
{
    AssetSurface sheet;
    int rc = sheet.load_pcx("jackal.pcx", 0, 0, 0x100, 2);
    if (rc)
        return rc;
    s_checkBoxEmpty.extract(&sheet, 1, 0x23, 0x20, 0x20, 1, 1);
    s_checkBoxTicked.extract(&sheet, 0x22, 0x23, 0x20, 0x20, 1, 1);
    return 0;
}

void CheckBoxCtl::close_class()
{
    s_checkBoxEmpty.close();
    s_checkBoxTicked.close();
}
