#ifndef EDITGROUP_H
#define EDITGROUP_H

#include "WidgetBase.h"
#include "ListBoxCtl.h"
#include "EditBox.h"

extern int g_editGroupInitValue;

struct EditGroup : virtual GraphicWin, virtual Dialog {
    EditBox *m_items[10];
    char *m_defaultTexts[10];
    int m_maxLens[10];
    int m_maxLabelWidth;
    int m_maxEntryWidth;
    int m_initValue;

    EditGroup();
    virtual ~EditGroup();
    virtual int attach(WidgetBase *owner, int x, int y, int flags);

    const char *get_text(int i);
    void set_text_limits(int n);

    virtual void on_dialog_focus(int on);
    virtual void pass_dialog_focus();

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

    int edit(const char *label, const char *defaultText, int maxLen);

    int calculate_dimensions();

    int set_default(const char *s, int idx);

    virtual void on_redraw();
};

#endif
