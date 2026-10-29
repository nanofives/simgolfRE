







#ifndef LISTBOX_H
#define LISTBOX_H

#include "WidgetBase.h"
#include "ListBoxCtl.h"


extern int g_panelDefault;


extern int listbox_count;



extern int listbox_right_click;

extern int scroll_from_key;

struct ListBox : virtual GraphicWin, virtual Dialog {

    virtual void draw_item(int row);

    ListBox();
    virtual ~ListBox();

    void close();

    int ResetAndInit(int bytes)
    {
        close();
        return InitOwnArena(bytes);
    }
    int ResetAndInitAlt(Heap *arena)
    {
        close();
        return InitSharedArena(arena);
    }

    void set_selected_id(int key);



    void set_selected_pos(int row, int notify);


    int calculate_dimensions();


    virtual int attach(WidgetBase *owner, int x, int y, int flags);


    virtual void on_right_down(int x, int y);

    virtual void on_left_down(int x, int y);

    virtual int on_key_down(int vk);

    virtual void on_redraw();

    virtual void on_scrolling(int bar, int pos);

    virtual void on_scrolled(int bar, int pos);

    virtual void on_right_double_click(int x, int y);


    virtual void on_left_double_click(int x, int y);

    virtual void on_left_click(int x, int y);


    int find_entry(const char *text);
    int item(char *text, int idx, char single);


    int update_changes();



    virtual void on_mouse_move(int x, int y);

    virtual void on_mouse_leave(int x, int y);

    virtual void on_mouse_wheel(int delta, int x, int y);



    virtual int on_scroll_create()
    {
        if (m_flags & 0x40) {
            if (parent)
                return parent->on_listbox_scroll_create();
        }
        return 0;
    }

    virtual int on_scroll_delete(int bar)
    {
        if (m_flags & 0x40) {
            if (parent)
                return parent->on_listbox_scroll_delete(bar);
        }
        return bar;
    }

    virtual void on_dialog_focus(int on);


    void DrawLeftAligned(const char *s, Rect *r)
    {
        if (s)
            RenderTextVerticallyCentered(s, r, strlen(s));
    }
    void DrawRightAligned(const char *s, Rect *r)
    {
        if (s)
            RenderTextRightAlignedInRectVCentered(s, r, strlen(s));
    }



    void InsetForText(Rect *r)
    {
        int k = m_bevelWidth + 1;
        r->left += m_indent + k;
        r->right -= k;
        set_clip(r);
    }
    void OutsetFromText(Rect *r)
    {
        int k = m_bevelWidth + 1;
        r->left -= k;
        r->right += k;
        r->left -= m_indent;
        set_clip(r);
    }

    void SetBevelColors(int hi, int lo)
    {
        m_bevelLo = lo;
        m_bevelHi = hi;
    }
    void DrawRowText(Rect *r)
    {
        char *bar = strchr(m_items.TextAtCursor(), '|');
        if (bar == 0) {
            if (m_items.SpanAtCursor() > 1)
                RenderMultilineTextInRect((char *)m_items.TextAtCursor(), r, 0, 0);
            else
                DrawLeftAligned(m_items.TextAtCursor(), r);
        } else {
            *bar = 0;
            if (m_items.SpanAtCursor() > 1)
                RenderMultilineTextInRect((char *)m_items.TextAtCursor(), r, 0, 0);
            else
                DrawLeftAligned(m_items.TextAtCursor(), r);
            *bar = '|';
            bar++;
            DrawRightAligned(bar, r);
        }
    }



    char m_options;

    int m_multiSelect;

    int m_forceNotify;


    unsigned m_scrollbarsNeeded;

    int m_selTextColor;

    int m_focusColor;

    int m_lastMouseX;
    int m_lastMouseY;

    int m_firstVisible;


    int m_visibleCells;
    int m_totalLines;


    int m_bevelHi;
    int m_bevelLo;
    int m_bevelWidth;

    BoxSpriteParams *m_theme;



    void(__cdecl *m_rowHook)(GraphicWin *self, int key, int row, int x, int y, int h);
    int(__cdecl *m_cellHook)(GraphicWin *self, int key, int row, Rect *r, const char *text);
    int(__cdecl *m_backgroundHook)(GraphicWin *self, int key, int row, Rect *r);


    void(__cdecl *m_onChange)(int cur);
    void(__cdecl *m_onActivate)(int cur);
    void SetOnActivate(void(__cdecl *fn)(int cur)) { m_onActivate = fn; }
};



extern ListBox *current_listbox;

#endif
