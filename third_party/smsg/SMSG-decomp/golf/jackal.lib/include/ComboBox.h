#ifndef COMBOBOX_H
#define COMBOBOX_H

#include "BasePop.h"
#include "ListBox.h"

struct Font;
struct ComboBox;

struct CBTopBar : GraphicWin {
    CBTopBar();
    virtual ~CBTopBar();
    virtual void OnMouseMoveInternal(int x, int y, int alt);
    virtual void on_button_clicked(int id);

    ComboBox *m_owner;
    Rect m_dropRect;
};

struct CBListBox : ListBox {
    CBListBox() { m_owner = 0; }
    virtual ~CBListBox() { m_owner = 0; }
    virtual void on_left_double_click(int x, int y);
    virtual void on_mouse_move(int x, int y);
    virtual void on_left_click(int x, int y);

    ComboBox *m_owner;
};

struct CBListPop : BasePop {
    CBListPop() { m_owner = 0; }
    virtual ~CBListPop() { m_owner = 0; }
    virtual void on_left_click(int x, int y);
    virtual void on_mouse_move(int x, int y) {}
    virtual void on_mouse_leave(int x, int y) {}

    ComboBox *m_owner;
};

struct ComboBox {
    ComboBox();
    ~ComboBox();
    int init(int x, int y, int w, int h, WidgetBase *owner, unsigned flags);
    void show();
    void updateTopBarText();
    int updateListBoxPositionAndSize(int w, int h);
    void setListOverlap(WidgetBase *overlap);
    void hide();
    void close();
    int width();
    void hideListBox();
    virtual void showListBox();
    void setSelectedID(int id);
    void setTopBarSprites(SpriteObject **sprites);
    void drawTopBarSprites();
    void setButtonSprites(SpriteObject **sprites);
    static void setDefaultTopBarSprites(SpriteObject **sprites);
    static void deleteDefaultTopBarSprites();
    static void setDefaultButtonSprites(SpriteObject **sprites);
    static void deleteDefaultButtonSprites();
    const char *getItemString(int key);
    const char *getCurrentString();
    int getCurrentID();
    int addItem(char *text, int id);
    virtual void onLinkClicked(int id) {}
    virtual void onSelect() {}

    void SetDropRect(Rect *r)
    {
        if (r)
            m_frame.m_dropRect = *r;
    }

    unsigned char m_flags;
    int m_x;
    int m_y;
    int m_buttonW;
    int m_dropH;
    int m_buttonH;
    int m_selId;
    int f0020;
    CBListPop m_dialog;
    CBTopBar m_frame;
    ImageButton m_button;
    CBListBox m_scrollList;
    SpriteObject *m_trackSprites[3];
    SpriteObject *m_buttonSprites[3];
    WidgetBase *m_owner;
    WidgetBase *m_listOverlap;
    Font *m_rowFont;
    int m_reserved;
    static SpriteObject *m_defaultSprButton[3];
    static SpriteObject *m_defaultSprTopBar[3];
};


#endif
