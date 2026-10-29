#include "jgldefs.h"
#include "EditGroup.h"
#include "ClassListPush.h"
#include "crt.h"
#include "FrameStyle.h"
#include "JackalUtil.h"











int g_editGroupInitValue = 0;





EditGroup::EditGroup()
{
    for (int i = 0; i < 10; ++i) {
        m_items[i] = 0;
        m_defaultTexts[i] = 0;
        m_maxLens[i] = 10;
    }
    m_initValue = g_editGroupInitValue;
}

EditGroup::~EditGroup()
{
    close();
}













void EditGroup::close()
{
    for (int i = 0; i < 10; ++i) {
        if (m_items[i]) {
            delete m_items[i];
            m_items[i] = 0;
            m_maxLens[i] = 10;
        }
        if (m_defaultTexts[i]) {
            FreeIfNotNull(m_defaultTexts[i]);
            m_defaultTexts[i] = 0;
        }
    }
    m_initValue = g_editGroupInitValue;
    Dialog::close();
}






int EditGroup::attach(WidgetBase *owner, int x, int y, int flags)
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

    int idx = 0;
    Rect cell;
    cell.SetRect(m_indent + m_inset * 2 + m_maxLabelWidth, m_inset >> 1, m_contentWidth - m_inset * 2 - m_maxLabelWidth,
        m_rowHeight - m_inset);
    for (int col = 0; col < m_layoutColumns; col++) {
        Rect r = cell;
        r.Offset(m_contentWidth * col, 0);
        for (int row = 0; row < m_columnStride; row++) {
            if (m_items[idx])
                delete m_items[idx];
            m_items[idx] = new EditBox;
            if (!m_items[idx])
                return 4;
            rc = m_items[idx]->Show(&r, this, 1);
            if (rc != 0)
                return rc;
            if (m_sizeFlags & 0x1000)
                m_items[idx]->m_entryFlags |= 2;
            m_items[idx]->set_text_color(m_textColor[0], m_textShadowColor[0], m_textShadowDx[0], m_textShadowDy[0]);
            m_items[idx]->SetFontsForStyles(m_rowFonts[0], m_rowFonts[1], m_rowFonts[2], 0);
            m_items[idx]->set_text_limit(m_maxLens[idx]);
            m_items[idx]->set_text(m_defaultTexts[idx]);
            m_items[idx]->m_backColor = m_backColor;
            m_items[idx]->m_bevelHiColor = m_itemStyle[0];
            m_items[idx]->m_bevelLoColor = m_itemStyle[1];
            m_items[idx]->m_border = m_itemStyle[2];
            m_items[idx]->m_inset = m_padding;
            m_items[idx]->CaretToEnd();
            m_items[idx]->show(0);
            r.Offset(0, m_rowHeight);
            idx++;
        }
    }
    return 0;
}











int EditGroup::edit(const char *label, const char *defaultText, int maxLen)
{
    int i = Dialog::m_items.m_count;
    if (i == 10)
        return 1;
    if (defaultText) {
        if (m_defaultTexts[i])
            FreeIfNotNull(m_defaultTexts[i]);
        m_defaultTexts[i] = static_cast<char *>(AllocateBytes(strlen(defaultText) + 1));
        if (!m_defaultTexts[i])
            return 4;
        ClearString(m_defaultTexts[i]);
        AppendString(m_defaultTexts[i], defaultText);
    }
    if (Dialog::m_items.Push(label, 0, 0, i))
        return 4;
    m_maxLens[i] = maxLen;
    return 0;
}


const char *EditGroup::get_text(int i)
{
    if (m_items[i])
        return m_items[i]->m_text;
    return 0;
}


void EditGroup::set_text_limits(int n)
{
    for (int i = 0; i < 10; i++)
        m_maxLens[i] = n;
}
















int EditGroup::set_default(const char *s, int idx)
{
    if (!s) {
        char *p = m_defaultTexts[idx];
        if (!p)
            return 0;
        ClearString(p);
        return 0;
    }
    if (m_defaultTexts[idx])
        FreeIfNotNull(m_defaultTexts[idx]);
    m_defaultTexts[idx] = static_cast<char *>(AllocateBytes(strlen(s) + 1));
    if (!m_defaultTexts[idx])
        return 4;
    ClearString(m_defaultTexts[idx]);
    AppendString(m_defaultTexts[idx], s);
    return 0;
}

























int EditGroup::calculate_dimensions()
{
    if (Dialog::m_items.m_count == 0)
        return 7;

    set_text_color(m_textColor[0], m_textShadowColor[0], m_textShadowDx[0], m_textShadowDy[0]);
    set_text_color2(m_textColor[1], m_textShadowColor[1], m_textShadowDx[1], m_textShadowDy[1]);
    set_text_color3(m_textColor[2], m_textShadowColor[2], m_textShadowDx[2], m_textShadowDy[2]);
    SetFontsForStyles(m_rowFonts[0], m_rowFonts[1], m_rowFonts[2], 0);

    m_columnStride = (Dialog::m_items.m_count < m_maxRows) ? Dialog::m_items.m_count : m_maxRows;

    Dialog::m_items.SetPos(-1);

    m_maxLabelWidth = 0;
    m_maxEntryWidth = 0;
    int n = Dialog::m_items.m_count;
    for (int i = 0; i < n; i++) {
        int w = text_width(Dialog::m_items.NextText());
        if (w > m_maxLabelWidth)
            m_maxLabelWidth = w;
        int v = text_width(m_defaultTexts[i]);
        if (v > m_maxEntryWidth)
            m_maxEntryWidth = v + 2 * m_padding;
    }

    m_maxEntryWidth = (m_maxEntryWidth > 10) ? m_maxEntryWidth : 10;
    m_contentWidth = m_indent + (m_inset + m_padding) * 2 + m_maxEntryWidth + m_maxLabelWidth;

    m_rowHeight = text_line_height() + m_inset;

    if ((m_sizeFlags & 2) && m_fixedWidth != 0) {
        m_width = m_fixedWidth;
        m_layoutColumns = m_fixedWidth / m_contentWidth;
    } else {
        m_width = (m_fixedWidth > m_contentWidth * m_columnCount) ? m_fixedWidth : m_contentWidth * m_columnCount;
        m_contentWidth = m_width / m_columnCount;
        m_layoutColumns = m_columnCount;
    }

    if ((m_sizeFlags & 1) && m_fixedHeight != 0) {
        m_height = m_fixedHeight;
        m_columnStride = m_fixedHeight / m_rowHeight;
        return 0;
    }
    m_height = m_rowHeight * m_columnStride + m_inset;
    return 0;
}




void EditGroup::on_redraw()
{
    GraphicWin *owner = parent;
    if (!owner)
        return;
    g_renderingWidget = owner;
    AssetSurface *dst = this;
    owner->copy(dst, m_originX, m_originY, 0, 0, GetSurfaceWidth(), GetSurfaceHeight());

    Rect cell;
    cell.SetRect(m_indent + m_padding, m_inset >> 1, m_contentWidth, m_rowHeight - m_inset);
    Dialog::m_items.SetPos(-1);
    for (int col = 0; col < m_layoutColumns; col++) {
        Rect r = cell;
        r.Offset(m_contentWidth * col, 0);
        for (int row = 0; row < m_columnStride; row++) {
            const char *s = Dialog::m_items.NextText();
            if (s)
                RenderTextVerticallyCentered(s, &r, strlen(s));
            r.Offset(0, m_rowHeight);
        }
    }
}











void EditGroup::on_dialog_focus(int on)
{
    if (on) {
        if (m_items[0])
            m_items[0]->on_dialog_focus(on);
    } else {
        for (int i = 0; i < 10; ++i) {
            if (m_items[i])
                m_items[i]->on_dialog_focus(0);
        }
    }
}



void EditGroup::pass_dialog_focus()
{
    if (m_childList.count && m_childList.head) {
        WidgetBase *child = m_childList.cur->data;
        if (child) {
            child->pass_dialog_focus();
            return;
        }
    }
    WidgetBase::pass_dialog_focus();
}














const int *GridFrameStyle(int flat)
{
    return flat ? g_flatFrameStyle : g_raisedFrameStyle;
}

int g_inputGridSpare = 0;
