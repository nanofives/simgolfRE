#ifndef EDITBOX_H
#define EDITBOX_H

#include "WidgetBase.h"
#include "Timer.h"

extern int g_editBoxStyle[8];
extern int g_editBoxColors[3];

struct EditBox : GraphicWin {
    char *m_text;
    int m_maxLength;
    int m_firstLine;
    int m_entryFlags;
    int m_backColor;
    int m_selTextColor;
    int m_selBackColor;
    int m_bevelHiColor;
    int m_bevelLoColor;
    int m_border;
    int m_inset;
    int m_selAnchor;
    int m_caret;
    int m_caretVisible;
    Time m_caretTimer;

    EditBox();
    virtual ~EditBox() { close(); }
    void close();
    int Show(int x, int y, int cx, int cy, WidgetBase *parent, int unused);
    int Show(const Rect *rc, WidgetBase *parent, int unused)
    {
        if (!rc)
            return 3;
        return Show(rc->left, rc->top, RectWidth(rc), RectHeight(rc), parent, unused);
    }
    void CaretToEnd()
    {
        m_selAnchor = 0;
        m_caret = strlen(m_text);
        redraw();
    }
    static void caret_callback(int self);

    void ClearBuffer();
    void set_text(const char *s);
    void set_text_limit(int n);
    int find_y_position(int off, char **lineStarts, int n);
    void pos_to_pixel(char **lineStarts, int n, int off, int *px, int *py);
    void calc_line_breaks(char **lineStarts, int *n);
    int pixel_to_pos(int x, int y, char **lineStarts, int n);
    int IsFocused();

    virtual void on_redraw();
    virtual int on_key_click(int ch, int vk);
    virtual int on_key_down(int vk);
    virtual void on_scrolled(int bar, int pos);
    virtual void on_scrolling(int bar, int pos);
    virtual void on_dialog_focus(int on);
    virtual void pass_dialog_focus();
    virtual void on_left_down(int x, int y);
    virtual void on_left_up(int x, int y);
    virtual void on_mouse_move(int x, int y);
};

#endif
