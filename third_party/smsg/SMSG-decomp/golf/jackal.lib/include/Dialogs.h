#ifndef DIALOGS_H
#define DIALOGS_H

#include "ListBox.h"
#include "ListBoxCtl.h"
#include "CheckBoxCtl.h"
#include "SpriteBox.h"
#include "EditGroup.h"

enum {
    MW_CHECKBOX = 1,
    MW_SCROLLLIST = 2,
    MW_TEXTINPUT = 4,
    MW_SPRITEBOX = 8,
    MW_LISTBOX = 16
};

struct Dialogs : ListBox, ListBoxCtl, CheckBoxCtl, SpriteBox, EditGroup {
    int m_type;

    Dialogs();
    void close();
    ~Dialogs();

    int attach(int type, WidgetBase *owner, int x, int y, int flags);
    int item(char *text, int idx);

    virtual void on_mouse_leave(int x, int y);
    virtual void on_mouse_move(int x, int y);
    virtual void on_left_down(int x, int y);
    virtual int on_key_down(int vk);
    virtual void on_left_double_click(int x, int y);
    virtual void on_redraw();
    virtual void on_left_click(int x, int y);
    virtual void on_right_down(int x, int y);
    virtual void on_right_double_click(int x, int y);
    virtual void on_left_up(int x, int y);
    virtual void on_right_up(int x, int y);
    virtual void on_right_click(int x, int y);
    virtual void on_scrolled(int bar, int pos);
    virtual void on_scrolling(int bar, int pos);
    virtual int attach(WidgetBase *owner, int x, int y, int flags);
    virtual void on_dialog_focus(int on);
    virtual void pass_dialog_focus();
    virtual int on_scroll_create();
    virtual int on_scroll_delete(int bar);
    int calculate_dimensions();
    int init(int bytes);
    int init(Heap *arena);
    int get_num_items();
    virtual void on_mouse_wheel(int delta, int x, int y);
};

#endif
