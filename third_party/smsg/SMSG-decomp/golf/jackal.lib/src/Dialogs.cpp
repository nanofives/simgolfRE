#include "jgldefs.h"
#include "Dialogs.h"

Dialogs::Dialogs()
{
}

void Dialogs::close()
{
}

int Dialogs::attach(int type, WidgetBase *owner, int x, int y, int flags)
{
    m_type = type & 0x1f;
    switch (m_type) {
    case MW_LISTBOX: return ListBoxCtl::attach(owner, x, y, flags);
    case MW_SCROLLLIST: return ListBox::attach(owner, x, y, flags);
    case MW_CHECKBOX: return CheckBoxCtl::attach(owner, x, y, flags);
    case MW_SPRITEBOX: return SpriteBox::attach(owner, x, y, flags);
    case MW_TEXTINPUT: return EditGroup::attach(owner, x, y, flags);
    }
    return 3;
}

int Dialogs::item(char *text, int idx)
{
    switch (m_type) {
    case MW_SCROLLLIST: return ListBox::item(text, idx, 0);
    case MW_CHECKBOX:
    case MW_TEXTINPUT:
    case MW_SPRITEBOX:
    case MW_LISTBOX: return Dialog::item(text, idx);
    }
    return 0;
}

void Dialogs::on_mouse_leave(int x, int y)
{
    switch (m_type) {
    case MW_LISTBOX: ListBoxCtl::on_mouse_leave(x, y); return;
    case MW_SCROLLLIST: ListBox::on_mouse_leave(x, y); return;
    case MW_CHECKBOX: CheckBoxCtl::on_mouse_leave(x, y); return;
    case MW_SPRITEBOX: SpriteBox::on_mouse_leave(x, y); return;
    case MW_TEXTINPUT: return;
    }
}

void Dialogs::on_mouse_move(int x, int y)
{
    switch (m_type) {
    case MW_LISTBOX: ListBoxCtl::on_mouse_move(x, y); return;
    case MW_SCROLLLIST: ListBox::on_mouse_move(x, y); return;
    case MW_CHECKBOX: CheckBoxCtl::on_mouse_move(x, y); return;
    case MW_SPRITEBOX: SpriteBox::on_mouse_move(x, y); return;
    }
}

void Dialogs::on_left_down(int x, int y)
{
    switch (m_type) {
    case MW_LISTBOX: ListBoxCtl::on_left_down(x, y); return;
    case MW_SCROLLLIST: ListBox::on_left_down(x, y); return;
    case MW_CHECKBOX: CheckBoxCtl::on_left_down(x, y); return;
    case MW_SPRITEBOX: SpriteBox::on_left_down(x, y); return;
    case MW_TEXTINPUT: return;
    }
}

int Dialogs::on_key_down(int vk)
{
    switch (m_type) {
    case MW_LISTBOX: return ListBoxCtl::on_key_down(vk);
    case MW_SCROLLLIST: return ListBox::on_key_down(vk);
    case MW_CHECKBOX: return CheckBoxCtl::on_key_down(vk);
    case MW_SPRITEBOX: return SpriteBox::on_key_down(vk);
    case MW_TEXTINPUT: return 0;
    }
    return 0;
}

void Dialogs::on_left_double_click(int x, int y)
{
    switch (m_type) {
    case MW_LISTBOX: ListBoxCtl::on_left_double_click(x, y); return;
    case MW_SCROLLLIST: ListBox::on_left_double_click(x, y); return;
    case MW_CHECKBOX: CheckBoxCtl::on_left_double_click(x, y); return;
    case MW_SPRITEBOX: SpriteBox::on_left_double_click(x, y); return;
    }
}

void Dialogs::on_redraw()
{
    switch (m_type) {
    case MW_LISTBOX: ListBoxCtl::on_redraw(); return;
    case MW_SCROLLLIST: ListBox::on_redraw(); return;
    case MW_CHECKBOX: CheckBoxCtl::on_redraw(); return;
    case MW_SPRITEBOX: SpriteBox::on_redraw(); return;
    case MW_TEXTINPUT: EditGroup::on_redraw(); return;
    }
}

void Dialogs::on_left_click(int x, int y)
{
    switch (m_type) {
    case MW_LISTBOX: return;
    case MW_SCROLLLIST: ListBox::on_left_click(x, y); return;
    case MW_CHECKBOX: return;
    case MW_SPRITEBOX: SpriteBox::on_left_click(x, y); return;
    case MW_TEXTINPUT: return;
    }
}

void Dialogs::on_right_down(int x, int y)
{
    if (m_type == MW_SPRITEBOX)
        SpriteBox::on_right_down(x, y);
}

void Dialogs::on_right_double_click(int x, int y)
{
    if (m_type == MW_SPRITEBOX)
        SpriteBox::on_right_double_click(x, y);
}

void Dialogs::on_left_up(int x, int y)
{
    switch (m_type) {
    case MW_TEXTINPUT: return;
    case MW_SPRITEBOX: SpriteBox::on_left_up(x, y); return;
    }
}

void Dialogs::on_right_up(int x, int y)
{
    if (m_type == MW_SPRITEBOX)
        SpriteBox::on_right_up(x, y);
}

void Dialogs::on_right_click(int x, int y)
{
    if (m_type == MW_SPRITEBOX)
        SpriteBox::on_right_click(x, y);
}

void Dialogs::on_scrolled(int bar, int pos)
{
    switch (m_type) {
    case MW_SCROLLLIST: ListBox::on_scrolling(bar, pos); return;
    }
}

void Dialogs::on_scrolling(int bar, int pos)
{
    switch (m_type) {
    case MW_SCROLLLIST: ListBox::on_scrolling(bar, pos); return;
    }
}

int Dialogs::attach(WidgetBase *owner, int x, int y, int flags)
{
    switch (m_type) {
    case MW_LISTBOX: return ListBoxCtl::attach(owner, x, y, flags);
    case MW_SCROLLLIST: return ListBox::attach(owner, x, y, flags);
    case MW_CHECKBOX: return CheckBoxCtl::attach(owner, x, y, flags);
    case MW_SPRITEBOX: return SpriteBox::attach(owner, x, y, flags);
    case MW_TEXTINPUT: return EditGroup::attach(owner, x, y, flags);
    }
    return 7;
}

void Dialogs::on_dialog_focus(int on)
{
    switch (m_type) {
    case MW_LISTBOX: ListBoxCtl::on_dialog_focus(on); return;
    case MW_SCROLLLIST: ListBox::on_dialog_focus(on); return;
    case MW_CHECKBOX: CheckBoxCtl::on_dialog_focus(on); return;
    case MW_SPRITEBOX: SpriteBox::on_dialog_focus(on); return;
    case MW_TEXTINPUT: EditGroup::on_dialog_focus(on); return;
    }
}

void Dialogs::pass_dialog_focus()
{
    switch (m_type) {
    case MW_LISTBOX: WidgetBase::pass_dialog_focus(); return;
    case MW_SCROLLLIST: WidgetBase::pass_dialog_focus(); return;
    case MW_CHECKBOX: WidgetBase::pass_dialog_focus(); return;
    case MW_SPRITEBOX: WidgetBase::pass_dialog_focus(); return;
    case MW_TEXTINPUT: EditGroup::pass_dialog_focus(); return;
    }
}

int Dialogs::on_scroll_create()
{
    switch (m_type) {
    case MW_SCROLLLIST: return ListBox::on_scroll_create();
    }
    return 0;
}

int Dialogs::on_scroll_delete(int bar)
{
    switch (m_type) {
    case MW_SCROLLLIST: return ListBox::on_scroll_delete(bar);
    }
    return bar;
}

int Dialogs::calculate_dimensions()
{
    switch (m_type) {
    case MW_LISTBOX: return ListBoxCtl::calculate_dimensions();
    case MW_SCROLLLIST: return ListBox::calculate_dimensions();
    case MW_CHECKBOX: return CheckBoxCtl::calculate_dimensions();
    case MW_SPRITEBOX: return SpriteBox::calculate_dimensions();
    case MW_TEXTINPUT: return EditGroup::calculate_dimensions();
    }
    m_width = 0;
    m_height = 0;
    return 0;
}

int Dialogs::init(int bytes)
{
    int r;
    if ((r = ListBoxCtl::ResetAndInit(bytes)) != 0)
        return r;
    if ((r = ListBox::ResetAndInit(bytes)) != 0)
        return r;
    if ((r = CheckBoxCtl::ResetAndInit(bytes)) != 0)
        return r;
    if ((r = SpriteBox::init(bytes)) != 0)
        return r;
    return EditGroup::ResetAndInit(bytes);
}

int Dialogs::init(Heap *arena)
{
    int r;
    if ((r = ListBoxCtl::ResetAndInitAlt(arena)) != 0)
        return r;
    if ((r = ListBox::ResetAndInitAlt(arena)) != 0)
        return r;
    if ((r = CheckBoxCtl::ResetAndInitAlt(arena)) != 0)
        return r;
    if ((r = SpriteBox::ResetAndInitAlt(arena)) != 0)
        return r;
    return EditGroup::ResetAndInitAlt(arena);
}

int Dialogs::get_num_items()
{
    switch (m_type) {
    case MW_CHECKBOX:
    case MW_SCROLLLIST:
    case MW_TEXTINPUT:
    case MW_LISTBOX: return Dialog::m_items.m_count;
    case MW_SPRITEBOX: return m_cells.m_count;
    }
    return 0;
}

void Dialogs::on_mouse_wheel(int delta, int x, int y)
{
    switch (m_type) {
    case MW_SCROLLLIST: ListBox::on_mouse_wheel(delta, x, y); return;
    }
}
