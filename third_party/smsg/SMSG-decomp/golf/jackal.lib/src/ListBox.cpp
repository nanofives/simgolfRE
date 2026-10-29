#include "jgldefs.h"
#include <windows.h>
#include <ctype.h>
#include "ListBox.h"
#include "ClassListPush.h"
#include "GameMain.h"

ListBox::ListBox()
{
    m_lastMouseX = -1;
    m_lastMouseY = -1;
    m_theme = 0;
    m_multiSelect = 0;
    m_forceNotify = 0;
    m_selTextColor = g_panelTheme[2];
    m_focusColor = g_panelTheme[3];
    m_firstVisible = 0;
    m_visibleCells = 0;
    m_totalLines = 0;
    m_bevelHi = g_panelTheme[0];
    m_bevelLo = g_panelDefault;
    m_bevelWidth = g_panelTheme[1];
    m_rowHook = 0;
    m_cellHook = 0;
    m_backgroundHook = 0;
    m_onChange = 0;
    m_onActivate = 0;
    m_itemStyle[2] = 1;
    m_scrollbarsNeeded = 0;
    memset(&m_options, 0, sizeof m_options);
}

ListBox::~ListBox()
{
    close();
}

void ListBox::close()
{
    GraphicWin::close();
    Dialog::close();
    m_lastMouseX = -1;
    m_lastMouseY = -1;
    m_multiSelect = 0;
    m_forceNotify = 0;
    m_firstVisible = 0;
    m_visibleCells = 0;
    m_totalLines = 0;
    m_bevelHi = g_panelTheme[0];
    m_bevelLo = g_panelDefault;
    m_bevelWidth = g_panelTheme[1];
    m_selTextColor = g_panelTheme[2];
    m_focusColor = g_panelTheme[3];
    m_itemStyle[2] = 1;
    m_rowHook = 0;
    m_cellHook = 0;
    m_backgroundHook = 0;
    m_onChange = 0;
    m_onActivate = 0;
    m_scrollbarsNeeded = 0;
}

void ListBox::set_selected_id(int key)
{
    StringList *list = &m_items;
    if (list->m_head) {
        int n = list->m_count;
        list->m_curIndex = 0;
        list->m_cur = list->m_head;
        for (int i = 0; i < n; i++) {
            if (list->m_cur->key == key) {
                set_selected_pos(m_items.m_curIndex, 1);
                return;
            }
            list->m_curIndex++;
            list->m_cur = list->m_cur->next;
        }
    }
    set_selected_pos(-1, 1);
}

void ListBox::set_selected_pos(int row, int notify)
{
    int moved = 0;
    if (row == m_selected && m_forceNotify == 0)
        return;
    m_selected = row;
    if (row >= 0) {
        if (m_selected < m_firstVisible) {
            moved = 1;
            do {
                int step = (m_layoutColumns > 1) ? m_columnStride : 1;
                m_firstVisible -= step;
                if (m_firstVisible < 0)
                    m_firstVisible = 0;
            } while (m_selected < m_firstVisible);
        }
        if (m_items.m_count <= m_visibleCells) {
            m_firstVisible = 0;
        } else if (m_selected >= m_firstVisible + m_visibleCells) {
            moved = 1;
            do {
                int step = (m_layoutColumns > 1) ? m_columnStride : 1;
                m_firstVisible += step;
            } while (m_selected >= m_firstVisible + m_visibleCells);
        }
        if (moved && m_items.m_count > m_visibleCells) {
            if (m_layoutColumns > 1)
                set_horz_pos(m_firstVisible);
            else
                set_vert_pos(m_firstVisible);
        }
        if (m_onChange)
            m_onChange(get_selected_id());
        if (notify) {
            GraphicWin *p = parent;
            if (p)
                p->on_selected(pos_to_id(row));
            p = parent;
            if (p)
                p->OnItemSelectedEx(pos_to_id(row), listbox_right_click);
            p = parent;
            if (p) {
                WidgetObserver *o = p->m_onSelectionChanged;
                if (o)
                    o->play();
            }
        }
    }
    redraw();
}

int ListBox::calculate_dimensions()
{
    if (m_items.m_count == 0)
        return 7;

    m_scrollbarsNeeded = 0;

    set_text_color(m_textColor[0], m_textShadowColor[0], m_textShadowDx[0], m_textShadowDy[0]);
    set_text_color2(m_textColor[1], m_textShadowColor[1], m_textShadowDx[1], m_textShadowDy[1]);
    set_text_color3(m_textColor[2], m_textShadowColor[2], m_textShadowDx[2], m_textShadowDy[2]);
    SetFontsForStyles(m_rowFonts[0], m_rowFonts[1], m_rowFonts[2], 0);

    m_columnStride = (m_items.m_count < m_maxRows) ? m_items.m_count : m_maxRows;

    int widest = 0;
    for (int i = 0; i < m_items.m_count; i++) {
        int w = text_width(m_items.NextText());
        if (w > widest)
            widest = w;
    }
    if (widest == 0)
        widest = m_fixedWidth;

    m_contentWidth = m_inset + m_bevelWidth * 2 + 2 + m_indent + widest;

    if (m_fixedRowHeight == -1)
        m_rowHeight = text_line_height() + m_inset;
    else
        m_rowHeight = m_fixedRowHeight;

    if ((m_sizeFlags & 2) && m_fixedWidth != 0) {
        m_width = m_fixedWidth;
        m_layoutColumns = m_columnCount;
        if (m_layoutColumns == 1)
            m_contentWidth = m_width - m_padding * 2;
        else
            m_contentWidth = m_contentWidth - m_inset;
    } else {
        if (m_columnCount == 1) {
            m_width = (m_fixedWidth > m_contentWidth + m_padding * 2) ? m_fixedWidth : m_contentWidth + m_padding * 2;
            m_contentWidth = m_width - m_padding * 2;
        } else {
            m_contentWidth = (m_width - m_padding * 2 + m_inset) / m_columnCount - m_inset;
            int w = (m_contentWidth + m_inset) * m_columnCount + m_padding * 2 - m_inset;
            m_width = (m_fixedWidth > w) ? m_fixedWidth : w;
        }
        m_layoutColumns = m_columnCount;
    }

    if ((m_sizeFlags & 1) && m_fixedHeight != 0) {
        m_height = m_fixedHeight;
        m_columnStride = (m_height - m_bottomPadding - m_topPadding + m_inset) / m_rowHeight;
        if (!(m_flags & 0x40)) {
            if (m_columnStride * m_layoutColumns < m_totalLines) {
                if (m_layoutColumns > 1) {
                    m_height = m_fixedHeight - g_scrollbarThickness;
                    m_columnStride = m_height / m_rowHeight;
                } else {
                    m_width = m_fixedWidth - g_scrollbarThickness;
                    m_layoutColumns = 1;
                }
            }
        }
    } else {
        m_height = m_rowHeight * m_columnStride - m_inset + m_bottomPadding + m_topPadding;
    }

    m_rowHeight = m_rowHeight - m_inset;
    if (m_items.m_count <= (m_height - m_bottomPadding - m_topPadding + m_inset) / (m_rowHeight + m_inset)) {
        if (m_sizeFlags & 2)
            m_contentWidth = (m_width - m_padding * 2 + m_inset) / m_layoutColumns - m_inset;
        else
            m_contentWidth = m_width - m_padding * 2;
    }

    m_visibleCells = m_columnStride * m_layoutColumns;
    if (m_totalLines > m_visibleCells) {
        if (m_layoutColumns > 1) {
            if (!(m_flags & 0x40))
                m_height += g_scrollbarThickness;
            m_scrollbarsNeeded |= 1;
        } else {
            if (!(m_flags & 0x40))
                m_width += g_scrollbarThickness;
            if (m_sizeFlags & 2)
                m_contentWidth -= g_scrollbarThickness / m_layoutColumns;
            m_scrollbarsNeeded |= 2;
        }
    }
    return 0;
}

int ListBox::attach(WidgetBase *owner, int x, int y, int flags)
{
    int rc;
    if (owner == 0)
        return 3;
    flags = (flags & ~0x0c) | 0x20;
    m_flags = flags;
    rc = calculate_dimensions();
    if (rc != 0) {
        if (rc != 7)
            return rc;
        m_width = m_fixedWidth;
        m_height = m_fixedHeight;
        m_layoutColumns = 1;
        m_columnStride = 1;
    }
    m_originX = x;
    m_originY = y;
    if (m_totalLines > m_visibleCells) {
        if (m_layoutColumns > 1)
            flags |= 4;
        else
            flags |= 8;
    }
    if ((owner->m_flags & 0x1000) == 0)
        flags |= 0x100000;
    if (flags & 0x40)
        flags |= 0x800;
    int h = m_height;
    int w = m_width;
    GraphicWin *ro = GraphicWin::is_graphicwin();
    rc = ro->init(x, y, w, h, 0, flags, owner, 0, 0);
    if (rc != 0)
        return rc;
    if (flags & 8) {
        set_vert_range(0, m_items.m_count - m_columnStride);
        set_vert_paging(m_columnStride - 1);
    } else if (flags & 4) {
        set_horz_range(0, m_items.m_count / m_columnStride - 1);
        set_horz_paging(m_layoutColumns);
    }
    if (m_items.m_count == 0) {
        m_firstVisible = 0;
    } else {
        while (m_selected >= m_firstVisible + m_visibleCells)
            m_firstVisible += (m_layoutColumns > 1) ? m_columnStride : 1;
    }
    if (m_totalLines > m_visibleCells) {
        if (m_layoutColumns > 1)
            set_horz_pos(m_firstVisible);
        else
            set_vert_pos(m_firstVisible);
    }
    set_text_color(m_textColor[0], m_textShadowColor[0], m_textShadowDx[0], m_textShadowDy[0]);
    set_text_color2(m_textColor[1], m_textShadowColor[1], m_textShadowDx[1], m_textShadowDy[1]);
    set_text_color3(m_textColor[2], m_textShadowColor[2], m_textShadowDx[2], m_textShadowDy[2]);
    SetFontsForStyles(m_rowFonts[0], m_rowFonts[1], m_rowFonts[2], 0);
    set_vert_pos(m_firstVisible);
    set_horz_pos(m_firstVisible / m_layoutColumns);
    return 0;
}

void ListBox::on_right_down(int x, int y)
{
    listbox_right_click = 1;
    on_left_down(x, y);
    listbox_right_click = 0;
}

void ListBox::on_left_down(int x, int y)
{
    g_renderingWidget = parent;
    if (m_items.m_count == 0)
        return;
    m_lastMouseX = x;
    m_lastMouseY = y;
    y -= m_topPadding;
    if (y < 0)
        return;
    int idx;
    if (m_layoutColumns != 1) {
        idx = (x - m_padding) / (m_contentWidth + m_inset) * m_columnStride + y / (m_rowHeight + m_inset);
    } else {
        m_items.SetPos(m_firstVisible - 1);
        idx = 0;
        for (int k = m_items.m_count; k > 0; k--) {
            if (y < 0)
                break;
            ++idx;
            y -= (m_rowHeight + m_inset) * m_items.NextSpan();
        }
        if (idx == 0)
            return;
        --idx;
    }
    if (idx >= m_visibleCells)
        return;
    x = m_firstVisible + idx;
    if (x >= m_items.m_count)
        return;
    if (m_multiSelect != 0 && x != m_selected) {
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0) {
            m_items.SetPos(-1);
            for (int k = m_items.m_count; k > 0; k--) {
                m_items.next_entry()->attr = 0;
            }
        }
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
            if (x > m_selected) {
                m_items.SetPos(m_selected - 1);
                for (int k = m_selected; k <= x; k++) {
                    m_items.next_entry()->attr = 1;
                }
            } else {
                m_items.SetPos(x - 1);
                for (int k = x; k <= m_selected; k++) {
                    m_items.next_entry()->attr = 1;
                }
            }
        } else {
            m_items.SetPos(x);
            if (m_items.current_entry()->attr)
                m_items.current_entry()->attr = 0;
            else
                m_items.current_entry()->attr = 1;
        }
    }
    set_selected_pos(x, 1);
    if (m_sizeFlags & 4) {
        GraphicWin *p = parent;
        if (p)
            p->on_button_clicked(-1);
    }
}

int ListBox::on_key_down(int vk)
{
    int count = m_items.m_count;
    g_renderingWidget = parent;
    m_lastMouseX = -1;
    m_lastMouseY = -1;
    int rows = count / m_columnStride;
    if (m_columnStride * rows < count)
        rows++;
    int lastShown = m_visibleCells + m_firstVisible - 1;
    if (m_selected == -1)
        m_selected = 0;
    int first = m_firstVisible;
    int cur = m_selected;

    switch (vk) {
    case VK_UP:
    case VK_NUMPAD8:
        if (m_selected == 0)
            return 1;
        m_selected--;
        if (m_visibleCells < count && m_selected < m_firstVisible) {
            if (m_layoutColumns == 1)
                m_firstVisible = m_selected;
            else
                m_firstVisible -= m_columnStride;
        }
        break;
    case VK_DOWN:
    case VK_NUMPAD2:
        if (m_visibleCells < count) {
            if (m_layoutColumns == 1) {
                if ((m_selected + 1 > m_columnStride + m_firstVisible - 1 || m_selected == count - 1)
                    && m_firstVisible < count - 1)
                    m_firstVisible++;
            } else {
                if (m_selected + 1 > lastShown && m_firstVisible + m_columnStride <= count - 1)
                    m_firstVisible += m_columnStride;
                if (m_selected == count - 1 && m_firstVisible < (rows - 1) * m_columnStride)
                    m_firstVisible += m_columnStride;
            }
        }
        if (m_selected < count - 1)
            m_selected++;
        break;
    case VK_LEFT:
    case VK_NUMPAD4:
        if (m_layoutColumns == 1)
            return 0;
        if (m_selected >= m_columnStride)
            m_selected -= m_columnStride;
        if (m_visibleCells < count && m_selected < m_firstVisible)
            m_firstVisible -= m_columnStride;
        break;
    case VK_RIGHT:
    case VK_NUMPAD6:
        if (m_layoutColumns == 1)
            return 0;
        if (m_visibleCells < count) {
            if (m_selected >= (m_layoutColumns - 1) * m_columnStride + m_firstVisible) {
                if (m_columnStride + m_firstVisible <= count - 1)
                    m_firstVisible += m_columnStride;
            } else {
                int lastColumn = (rows - 1) * m_columnStride;
                if (m_selected >= m_firstVisible + lastColumn && m_firstVisible < lastColumn)
                    m_firstVisible += m_columnStride;
            }
        }
        if (m_selected < (rows - 1) * m_columnStride)
            m_selected += m_columnStride;
        if (m_selected > count - 1)
            m_selected = count - 1;
        break;
    default: {
        if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
            return 0;
        int ch = MapVirtualKeyA(vk & 0xfff8ffff, 2) & 0xffff;
        if ((char)ch == 0)
            return 0;
        m_items.SetPos(m_selected);
        for (int i = 0; i < m_items.m_count; i++) {
            char c = *m_items.NextText();
            if (c == (char)ch || c == tolower((char)ch)) {
                set_selected_pos(m_items.m_curIndex, 1);
                return 1;
            }
        }
        return 0;
    }
    }

    if (m_selected < m_firstVisible || m_selected >= m_visibleCells + m_firstVisible)
        m_firstVisible = m_selected;
    set_vert_pos(m_firstVisible);
    set_horz_pos(m_firstVisible / m_layoutColumns);
    if (m_onChange)
        m_onChange(get_selected_id());
    GraphicWin *p = parent;
    if (p)
        p->on_selected(get_selected_id());
    p = parent;
    if (p) {
        WidgetObserver *o = p->m_onSelectionChanged;
        if (o)
            o->play();
    }
    if (m_firstVisible != first) {
        redraw();
        return 1;
    }
    if (cur != m_selected) {
        listbox_count = cur - m_firstVisible;
        draw_item(cur);
        listbox_count = m_selected - m_firstVisible;
        draw_item(m_selected);
    }
    soft_update();
    return 1;
}

void ListBox::on_dialog_focus(int on)
{
}

void ListBox::draw_item(int row)
{
    Rect r;
    Rect frame;
    if (row < 0)
        return;
    r.SetRect(0, 0, m_contentWidth, m_rowHeight);
    {
        int mx = m_topPadding;
        int my = m_padding;
        int sel;
        int res;
        r.Offset(m_padding, m_topPadding);
        r.right -= m_padding * 2;
        if (m_columnCount == 1) {
            int sum = 0;
            m_items.SetPos(m_firstVisible - 1);
            {
                int reps = row - m_firstVisible;
                while (reps > 0) {
                    sum += m_items.NextSpan();
                    reps--;
                }
            }
            {
                int lh = m_rowHeight + m_inset;
                r.top += lh * sum;
                r.bottom += lh * sum;
            }
            {
                int span = m_items.NextSpan();
                r.bottom = (m_rowHeight + m_inset) * span - m_inset + r.top;
            }
        } else {
            int off = row - m_firstVisible;
            int step = (m_contentWidth + m_inset) * (off / m_columnStride);
            int step2;
            r.left = my + step;
            r.right += step;
            step2 = (m_rowHeight + m_inset) * (off % m_columnStride);
            r.top = step2 + mx;
            r.bottom += step2;
        }
        if (row >= m_items.m_count)
            return;
        if (m_multiSelect == 0) {
            sel = (m_selected == row);
        } else {
            m_items.SeekForward(row);
            sel = m_items.current_entry()->attr;
        }
        set_clip(&r);
        res = 0;
        if (m_backgroundHook) {
            res = m_backgroundHook(this, pos_to_id(row), m_firstVisible + listbox_count, &r);
        }
        set_text_color(m_textColor[0], m_textShadowColor[0], m_textShadowDx[0], m_textShadowDy[0]);
        set_text_color2(m_textColor[1], m_textShadowColor[1], m_textShadowDx[1], m_textShadowDy[1]);
        set_text_color3(m_textColor[2], m_textShadowColor[2], m_textShadowDx[2], m_textShadowDy[2]);
        SetFontsForStyles(m_rowFonts[0], m_rowFonts[1], m_rowFonts[2], 0);
        if (sel == 0) {
            set_text_color(m_textColor[0], m_textShadowColor[0], m_textShadowDx[0], m_textShadowDy[0]);
            set_text_color2(m_textColor[1], m_textShadowColor[1], m_textShadowDx[1], m_textShadowDy[1]);
            set_text_color3(m_textColor[2], m_textShadowColor[2], m_textShadowDx[2], m_textShadowDy[2]);
        } else {
            set_clip(&r);
            if (m_selTextColor != -1) {
                set_text_color(m_selTextColor, m_textShadowColor[0], m_textShadowDx[0], m_textShadowDy[0]);
                set_text_color2(m_selTextColor, m_textShadowColor[1], m_textShadowDx[1], m_textShadowDy[1]);
                set_text_color3(m_selTextColor, m_textShadowColor[2], m_textShadowDx[2], m_textShadowDy[2]);
            } else {
                set_text_color(m_backColor, m_textShadowColor[0], m_textShadowDx[0], m_textShadowDy[0]);
                set_text_color2(m_backColor, m_textShadowColor[1], m_textShadowDx[1], m_textShadowDy[1]);
                set_text_color3(m_backColor, m_textShadowColor[2], m_textShadowDx[2], m_textShadowDy[2]);
            }
        }
        {
            int item = pos_to_id(row);
            if (res == 0 && parent) {
                int mode;
                if (sel)
                    mode = 2;
                else
                    mode = (m_hotIndex == row);
                res = parent->on_dialog_item_back_draw(this, item, mode, &r);
                if (res == 0 && m_theme == 0) {
                    if ((m_flags & 0x80000) && parent) {
                        AssetSurface *dst = this;
                        int hh = GetSurfaceHeight();
                        int ww = GetSurfaceWidth();
                        parent->copy(dst, m_originX, m_originY, 0, 0, ww, hh);
                    } else {
                        EraseWholeBackground(m_backColor);
                    }
                }
            }
            if (res == 0) {
                GetSurfaceBounds(&frame);
                int i = 0;
                while (i < m_itemStyle[2]) {
                    DrawBevelFrameInRect(&frame, m_itemStyle[0], m_itemStyle[1]);
                    frame.left++;
                    frame.right--;
                    frame.top++;
                    frame.bottom--;
                    i++;
                }
            }
            m_items.SeekForward(row);
            if (m_rowHook) {
                current_listbox = this;
                m_rowHook(this, item, m_firstVisible + listbox_count, r.left, r.top, r.bottom - r.top);
            }
            if (m_cellHook) {
                if (m_cellHook(this, pos_to_id(row), row, &r, m_items.TextAtCursor()))
                    return;
            }
            if (sel == 0) {
                InsetForText(&r);
                DrawRowText(&r);
                OutsetFromText(&r);
                return;
            }
            if (res == 0 && is_dialog_focus()) {
                set_clip(&r);
                if (m_focusColor != -1)
                    EraseBackground(&r, m_focusColor);
            }
            InsetForText(&r);
            DrawRowText(&r);
            OutsetFromText(&r);
            if (res)
                return;
            {
                int i = 0;
                while (i < m_bevelWidth) {
                    DrawBevelFrameInRect(&r, m_bevelHi, m_bevelLo);
                    r.left++;
                    r.right--;
                    r.top++;
                    r.bottom--;
                    i++;
                }
            }
        }
    }
}

void ListBox::on_redraw()
{
    Rect r;
    GraphicWin *dst = parent;
    if (dst == 0)
        return;
    g_renderingWidget = dst;
    ResetSurfaceClip();
    if (m_backgroundHook == 0 || m_backgroundHook(this, 0, 0, 0) == 0) {
        if (dst->on_dialog_back_draw(this) == 0) {
            if (m_theme) {
                box_sprite(GetRect(), m_theme);
            } else if ((m_flags & 0x80000) && parent) {
                AssetSurface *src = this;
                int hh = GetSurfaceHeight();
                int ww = GetSurfaceWidth();
                parent->copy(src, m_originX, m_originY, 0, 0, ww, hh);
            } else {
                EraseWholeBackground(m_backColor);
            }
        }
    }
    GetSurfaceBounds(&r);
    for (int i = 0; i < m_itemStyle[2]; i++) {
        DrawBevelFrameInRect(&r, m_itemStyle[0], m_itemStyle[1]);
        r.left++;
        r.right--;
        r.top++;
        r.bottom--;
    }
    if (m_items.m_count == 0)
        return;
    m_items.SetPos(m_firstVisible - 1);
    listbox_count = 0;
    for (int col = 0; col < m_layoutColumns; col++) {
        for (int row = 0; row < m_columnStride; row++) {
            draw_item(m_firstVisible + listbox_count);
            listbox_count++;
            if (listbox_count == m_items.m_count - m_firstVisible)
                break;
        }
    }
}

void ListBox::on_scrolling(int bar, int pos)
{
    g_renderingWidget = parent;
    int oldFirst = m_firstVisible;
    int oldSel = m_selected;
    switch (bar) {
    case -2: m_firstVisible = m_columnStride * pos; break;
    case -1: m_firstVisible = pos; break;
    }
    if (scroll_from_key)
        set_selected_pos(m_firstVisible, 1);
    if (m_firstVisible != oldFirst) {
        redraw();
        return;
    }
    if (oldSel != m_selected) {
        draw_item(oldSel);
        draw_item(m_selected);
    }
    soft_update();
}

void ListBox::on_scrolled(int bar, int pos)
{
    on_scrolling(bar, pos);
}

void ListBox::on_right_double_click(int x, int y)
{
    listbox_right_click = 1;
    on_left_double_click(x, y);
    listbox_right_click = 0;
}

void ListBox::on_left_double_click(int x, int y)
{
    g_renderingWidget = parent;
    if (m_onActivate)
        m_onActivate(get_selected_id());
    GraphicWin *p = parent;
    if (p == 0)
        return;
    p->on_double_clicked(get_selected_id());
    p = parent;
    p->OnItemActivatedEx(get_selected_id(), listbox_right_click);
    WidgetObserver *o = parent->m_onItemActivated;
    if (o)
        o->play();
}

void ListBox::on_left_click(int x, int y)
{
    GraphicWin *p = parent;
    if (p)
        p->on_left_click(x, y);
}

int ListBox::find_entry(const char *text)
{
    if (m_items.m_count == 0)
        return -1;
    if (!text)
        return -1;
    int saved = m_items.m_curIndex;
    int found = -1;
    for (int i = 0; i < m_items.m_count; i++) {
        if (stricmp(m_items.NextText(), text) == 0) {
            found = m_items.m_curIndex;
            break;
        }
    }
    m_items.SetPos(saved);
    return found;
}

int ListBox::item(char *text, int idx, char single)
{
    if (text == 0)
        return 3;
    int avail = m_fixedWidth - g_scrollbarThickness;
    if (avail > 0)
        wrap_height(text, avail, 0, 0);
    int lines;
    if (avail != 0)
        lines = m_lineCount;
    else
        lines = 1;
    m_totalLines += lines;
    if (single)
        lines = 1;
    return m_items.Push(text, 0, lines, idx);
}

int ListBox::update_changes()
{
    int shown = is_visible();
    get_selected_id();
    unsigned oldBars = m_scrollbarsNeeded;
    int oldSel = m_selected;
    calculate_dimensions();
    if (oldBars != m_scrollbarsNeeded) {
        int flags = m_flags;
        GraphicWin *owner = parent;
        int rc = ListBox::attach(owner, m_originX, m_originY, flags);
        if (rc != 0)
            return rc;
        if (oldSel == -1)
            set_selected_pos(-1, 1);
        else
            set_selected_pos(0, 1);
        ReleaseCapture();
        if (shown)
            WidgetBase::show(0);
    } else {
        set_selected_pos(oldSel, 1);
    }
    if (m_flags & 8) {
        set_vert_range(0, m_items.m_count - 1);
        set_vert_paging(m_columnStride - 1);
    } else if (m_flags & 4) {
        set_horz_range(0, m_items.m_count / m_columnStride - 1);
        set_horz_paging(m_layoutColumns);
    }
    if (m_onChange)
        m_onChange(get_selected_id());
    GraphicWin *p = parent;
    if (p)
        p->on_selected(get_selected_id());
    p = parent;
    if (p) {
        WidgetObserver *o = p->m_onSelectionChanged;
        if (o)
            o->play();
    }
    if (shown)
        redraw();
    return 0;
}

void ListBox::on_mouse_move(int x, int y)
{
    if (m_items.m_count == 0)
        return;
    int oldHot = m_hotIndex;
    m_lastMouseX = x;
    m_lastMouseY = y;
    y -= m_topPadding;
    if (y < 0)
        return;
    int idx;
    if (m_layoutColumns != 1) {
        idx = (x - m_padding) / (m_contentWidth + m_inset) * m_columnStride + y / (m_rowHeight + m_inset);
    } else {
        m_items.SetPos(m_firstVisible - 1);
        idx = 0;
        for (int k = m_items.m_count; k > 0; k--) {
            if (y < 0)
                break;
            ++idx;
            y -= (m_rowHeight + m_inset) * m_items.NextSpan();
        }
        if (idx == 0)
            return;
        --idx;
    }
    idx += m_firstVisible;
    if (idx >= m_items.m_count)
        return;
    if (idx == m_selected) {
        if (m_hotIndex == -1)
            return;
        m_hotIndex = -1;
        listbox_count = oldHot - m_firstVisible;
        draw_item(oldHot);
    } else {
        m_hotIndex = idx;
        if (m_options & 1)
            set_selected_pos(idx, 1);
        if (m_hotIndex != oldHot) {
            if (oldHot != -1) {
                listbox_count = oldHot - m_firstVisible;
                draw_item(oldHot);
            }
            listbox_count = m_hotIndex - m_firstVisible;
            draw_item(m_hotIndex);
            soft_update();
        }
        return;
    }
    soft_update();
}

void ListBox::on_mouse_leave(int x, int y)
{
    m_hotIndex = -1;
    redraw();
}

void ListBox::on_mouse_wheel(int delta, int x, int y)
{
    if (m_flags & 8) {
        if (delta > 0) {
            if (m_firstVisible > 0) {
                set_vert_pos(--m_firstVisible);
                redraw();
            }
        } else if (m_firstVisible < m_items.m_count - m_columnStride) {
            set_vert_pos(++m_firstVisible);
            redraw();
        }
    }
}

void CaptureListDrag(HWND hwnd)
{
    SetCapture(hwnd);
}

int g_panelDefault;
ListBox *current_listbox;
int listbox_count = 0;
int listbox_right_click = 0;

int g_panelTheme[4] = {247, 2, -1, -1};
