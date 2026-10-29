#ifndef PULLDOWN_H
#define PULLDOWN_H

#include "WidgetBase.h"

struct ChoiceEntry {
    char *text;
    char *shortcut;
    int id;
    int flags;
    char *hotkey;
    SpriteObject *icon;
};

struct PullDown : GraphicWin {
    int m_popupFlags;
    ChoiceEntry m_entries[256];
    int m_reserved;
    int (*m_pfnIdle)();
    int (*m_pfnKeepOpen)(int id, PullDown *table);
    int m_count;
    int m_maxWidth;
    int m_minRowHeight;
    int m_hot;
    int m_shownCount;
    GraphicWin *m_menuBar;
    char m_dirty;
    int m_iconScaleX;
    int m_iconScaleY;
    int m_iconScaleDen;
    void (*m_pfnDrawEntryBackground)(PullDown *table, int hot, Rect *rect);
    void (*m_pfnDrawFrame)(PullDown *table);

    int add_separator(int id);
    int hide_pos(int index);
    int show_pos(int index);
    void delete_all();
    virtual void hide();
    virtual void on_right_down(int x, int y);
    virtual void on_right_up(int x, int y);
    int id_to_index(int id);
    int get_selected();
};

struct ChoiceSlot {
    PullDown *table;
    int selId;
};
extern ChoiceSlot g_parse_dropdowns[10];

#endif
