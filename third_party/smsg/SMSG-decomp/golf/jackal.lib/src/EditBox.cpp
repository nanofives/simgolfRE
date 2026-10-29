#include "jgldefs.h"
#include "EditBox.h"
#include "crt.h"
#include "JackalUtil.h"

int g_editBoxStyle[8] = {0xff, 1, 4, -1, 1, 1, -1, -1};
int g_editBoxColors[3] = {0};

void EditBox::ClearBuffer()
{
    m_text = 0;
    m_entryFlags = 0;
    m_firstLine = 0;
    m_maxLength = 0;
    m_caretVisible = 0;
    m_caret = 0;
    m_backColor = g_editBoxStyle[0];
    m_selTextColor = g_editBoxStyle[7];
    m_selBackColor = g_editBoxStyle[6];
    m_inset = g_editBoxStyle[2];
    m_bevelHiColor = g_editBoxColors[0];
    m_bevelLoColor = g_editBoxColors[1];
    m_border = g_editBoxStyle[1];
}

EditBox::EditBox()
{
    ClearBuffer();
}

void EditBox::close()
{
    if (m_text)
        FreeIfNotNull(m_text);
    ClearBuffer();
    m_caretTimer.close();
    GraphicWin::close();
}

int EditBox::Show(int x, int y, int cx, int cy, WidgetBase *parent, int unused)
{
    close();
    if (!parent)
        return 3;
    set_text_limit(0x40);
    int style = 0x2020;
    if ((parent->m_flags & 0x1000) == 0)
        style |= 0x100000;
    int rc = init(x, y, cx, cy, 0, style, parent, 0, 0);
    if (rc)
        return rc;
    m_formatOn = 0;
    set_text_color(g_editBoxColors[2], g_editBoxStyle[3], g_editBoxStyle[4], g_editBoxStyle[5]);
    set_cursor(0x7f01);
    CaretToEnd();
    m_caretTimer.init(caret_callback, (int)this, 500, 5);
    redraw();
    return 0;
}

void EditBox::set_text(const char *s)
{
    char *buf = m_text;
    if (buf) {
        if (!s)
            buf[0] = 0;
        else
            strncpy(buf, s, m_maxLength);
        m_caret = strlen(m_text);
        redraw();
    }
}

void EditBox::set_text_limit(int n)
{
    if (n == m_maxLength)
        return;
    char *p = (char *)AllocateBytes(n + 1);
    p[n] = 0;
    ClearString(p);
    if (m_text) {
        strncpy(p, m_text, n);
        if (m_text)
            FreeIfNotNull(m_text);
    }
    m_text = p;
    m_maxLength = n;
}

void EditBox::calc_line_breaks(char **lineStarts, int *n)
{
    *n = 1;
    lineStarts[0] = m_text;
    int margin = m_inset + m_border;
    wrap_height(m_text, GetSurfaceWidth() - margin * 2, lineStarts, n);
    lineStarts[*n] = m_text + strlen(m_text);
}

int EditBox::find_y_position(int off, char **lineStarts, int n)
{
    char *pos = m_text + off;
    int i;
    for (i = 0; i < n; i++) {
        if (pos < lineStarts[i])
            break;
    }
    return i - 1;
}

void EditBox::pos_to_pixel(char **lineStarts, int n, int off, int *px, int *py)
{
    int line = find_y_position(off, lineStarts, n);
    *px = CalculateFormattedTextWidth(lineStarts[line], m_text - lineStarts[line] + off);
    *py = text_line_height() * (line - m_firstLine);
    int margin = m_inset + m_border;
    *px += margin;
    *py += margin;
}

int EditBox::pixel_to_pos(int x, int y, char **lineStarts, int n)
{
    char *own[32];
    int margin = m_inset + m_border;
    x -= margin;
    y -= margin;
    int maxX = GetSurfaceWidth() - margin * 2 - 1;
    int maxY = GetSurfaceHeight() - margin * 2 - 1;
    if (x < 0)
        x = 0;
    if (x > maxX)
        x = maxX;
    if (!lineStarts) {
        lineStarts = own;
        calc_line_breaks(own, &n);
    }
    int line = y / text_line_height() + m_firstLine;
    if (line < 0)
        line = 0;
    if (line >= n)
        line = n - 1;
    char *p = lineStarts[line];
    char *end = lineStarts[line + 1];
    while (x > 0 && p < end)
        x -= CalculateFormattedTextWidth(p++, 1);
    return p - m_text;
}

static __inline void InflateRect(Rect *r, int dx, int dy)
{
    r->left -= dx;
    r->right += dx;
    r->top -= dy;
    r->bottom += dy;
}

void EditBox::on_redraw()
{
    Rect r;
    Rect *pr = &r;
    char *lines[32];
    int nLines;

    GetSurfaceBounds(pr);
    ResetSurfaceClip();
    EraseWholeBackground(m_backColor);
    for (int i = 0; i < m_border; i++) {
        DrawBevelFrameInRect(pr, m_bevelHiColor, m_bevelLoColor);
        InflateRect(&r, -1, -1);
    }
    InflateRect(&r, -m_inset, -m_inset);
    set_clip(pr);
    calc_line_breaks(lines, &nLines);
    int line = find_y_position(m_caret, lines, nLines);
    int extra = text_line_height();
    extra -= text_height();
    int visible = (pr->bottom - pr->top + extra) / text_line_height();
    int last = m_firstLine + visible - 1;
    if (line < m_firstLine)
        m_firstLine = line;
    if (line > last)
        m_firstLine = line - visible + 1;
    RenderMultilineTextInRect(lines[m_firstLine], pr, 0, 0);
    if (m_caretVisible) {
        int width = CalculateFormattedTextWidth(lines[line], m_text - lines[line] + m_caret);
        int y = text_line_height() * (line - m_firstLine);
        int margin = m_inset + m_border;
        int color = m_textColors[0][0];
        y += margin - extra / 2;
        vline(margin + width, y, y + text_line_height(), color);
    }
}

int EditBox::on_key_click(int ch, int vk)
{
    if (ch >= 0) {
        if (ch < 0x20)
            return 0;
        if (ch == 0 && vk >= 0x60 && vk <= 0x69)
            ch = vk - 0x30;
    }
    if (strlen(m_text) == m_maxLength) {
        MessageBeep(0);
        return 1;
    }
    if (m_text[m_caret] == 0)
        m_text[m_caret + 1] = 0;
    else
        memmove(&m_text[m_caret + 1], &m_text[m_caret], strlen(&m_text[m_caret]) + 1);
    m_text[m_caret] = (char)ch;
    ++m_caret;
    redraw();
    return 1;
}

int EditBox::on_key_down(int vk)
{
    char *lineStarts[32];
    int x, y, n;
    int pos;
    switch (vk & 0xffff) {
    case VK_UP:
        calc_line_breaks(lineStarts, &n);
        pos_to_pixel(lineStarts, n, m_caret, &x, &y);
        pos = pixel_to_pos(x, y - text_line_height(), lineStarts, n);
        if (pos == m_caret)
            MessageBeep(0);
        else
            m_caret = pos;
        break;
    case VK_DOWN:
        calc_line_breaks(lineStarts, &n);
        pos_to_pixel(lineStarts, n, m_caret, &x, &y);
        pos = pixel_to_pos(x, y + text_line_height(), lineStarts, n);
        if (pos == m_caret)
            MessageBeep(0);
        else
            m_caret = pos;
        break;
    case VK_LEFT:
        if (m_caret == 0)
            MessageBeep(0);
        else
            m_caret--;
        break;
    case VK_RIGHT:
        if (m_text[m_caret] == 0)
            MessageBeep(0);
        else
            m_caret++;
        break;
    case VK_BACK:
        if (m_caret == 0) {
            MessageBeep(0);
        } else {
            strcpy(&m_text[m_caret - 1], &m_text[m_caret]);
            m_caret--;
        }
        break;
    case VK_DELETE:
        if (m_text[m_caret] == 0)
            MessageBeep(0);
        else
            strcpy(&m_text[m_caret], &m_text[m_caret + 1]);
        break;
    case VK_HOME: m_caret = 0; break;
    case VK_END: m_caret = strlen(m_text); break;
    default: return 0;
    }
    redraw();
    return 1;
}

void EditBox::on_scrolled(int bar, int pos)
{
    redraw();
}

void EditBox::on_scrolling(int bar, int pos)
{
    on_scrolled(bar, pos);
}

void EditBox::pass_dialog_focus()
{
    if (parent)
        WidgetBase::pass_dialog_focus();
}

void EditBox::on_dialog_focus(int on)
{
    Time *t = &m_caretTimer;
    if (on)
        t->start();
    else
        t->stop();
    m_caretVisible = on;
    redraw();
}

void EditBox::caret_callback(int self)
{
    EditBox *te = (EditBox *)self;
    te->m_caretVisible = !te->m_caretVisible;
    te->redraw();
}

void EditBox::on_left_down(int x, int y)
{
    if (g_dragWidget == 0) {
        g_dragWidget = this;
        g_dragTargetWidget = 0;
    }
    m_caret = pixel_to_pos(x, y, 0, 1);
    redraw();
}

void EditBox::on_left_up(int x, int y)
{
    if (g_dragWidget == this) {
        g_dragWidget = 0;
        on_lose_mouse_capture();
    }
    if (g_dragTargetWidget == this)
        g_dragTargetWidget = 0;
    m_caret = pixel_to_pos(x, y, 0, 1);
    redraw();
}

void EditBox::on_mouse_move(int x, int y)
{
    if (g_dragWidget == this || (g_dragTargetWidget == this && g_dragWidget != 0 && g_dragTargetWidget != 0)) {
        m_caret = pixel_to_pos(x, y, 0, 1);
        redraw();
    }
}

int EditBox::IsFocused()
{
    return is_dialog_focus() != 0;
}

int CopyEditBoxToClipboard(EditBox *entry)
{
    HGLOBAL mem;
    int len;

    if (!OpenClipboard((HWND)graphsys->get_window_handle()))
        return 0;
    EmptyClipboard();
    len = strlen(entry->m_text) + 1;
    mem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, len);
    if (mem != 0) {
        memcpy(GlobalLock(mem), entry->m_text, len);
        GlobalUnlock(mem);
        SetClipboardData(CF_TEXT, mem);
    }
    CloseClipboard();
    return mem != 0;
}

int PasteClipboardIntoEditBox(EditBox *entry)
{
    HANDLE mem;
    const char *p;

    if (!IsClipboardFormatAvailable(CF_TEXT))
        return 0;
    if (!OpenClipboard((HWND)graphsys->get_window_handle()))
        return 0;
    mem = GetClipboardData(CF_TEXT);
    if (mem != 0) {
        for (p = (const char *)GlobalLock(mem); *p != 0; p++) {
            if (strlen(entry->m_text) == entry->m_maxLength)
                break;
            entry->on_key_click(*p, 0);
        }
        GlobalUnlock(mem);
    }
    CloseClipboard();
    return mem != 0;
}

int g_editBoxSpare[2] = {0};
