#ifndef SPRITEBOX_H
#define SPRITEBOX_H

#include "WidgetBase.h"
#include "ListBoxCtl.h"
#include "Spot.h"
#include "ClassList.h"

extern int g_spriteBoxSelectionColor;

struct SpriteObject;

struct SpriteStruct : virtual HeapNew {
    SpriteObject *sprite;
    char *text;
    int extra;
};

struct SpriteList : ClassList<SpriteStruct> {
    SpriteObject *m_stageSprite;
    char *m_stageText;
    int m_stageExtra;

    virtual int fill_entry(SpriteStruct *elem);
    virtual void kill_entry(SpriteStruct *elem);

    int Push(SpriteObject *sprite, char *text, int extra, int key)
    {
        m_stageSprite = sprite;
        m_stageText = text;
        m_stageExtra = extra;
        return add(key);
    }
    SpriteObject *CurrentSprite()
    {
        if (m_cur != 0)
            return current_entry()->sprite;
        return 0;
    }
    SpriteObject *next_sprite()
    {
        Advance();
        return CurrentSprite();
    }
    SpriteObject *SpriteAtCursor()
    {
        if (m_head != 0)
            return current_entry()->sprite;
        return 0;
    }
    int KeyAtCursor()
    {
        if (m_head != 0)
            return m_cur->key;
        return 0;
    }
    int NextKey()
    {
        if (m_head == 0)
            return 0;
        Advance();
        return m_cur->key;
    }
};

struct SpriteBox : virtual GraphicWin, virtual Dialog {
    int m_selectionBevel;
    int m_selectionColor;
    int f0c;
    Spot m_cellHotspots;
    int m_scaleNum;
    int m_scaleDen;
    SpriteList m_cells;
    int m_cellMetrics[4];
    void(__cdecl *m_onDrawCell)(SpriteObject *icon, WidgetBase *grid, int id, int x, int y);
    void(__cdecl *m_onLeftClick)(int id);
    void(__cdecl *m_onLeftDown)(int id);
    void(__cdecl *m_onLeftUp)(int id);
    void(__cdecl *m_onRightClick)(int id);
    void(__cdecl *m_onRightDown)(int id);
    void(__cdecl *m_onRightUp)(int id);
    void(__cdecl *m_onLeftDoubleClick)(int id);
    void(__cdecl *m_onRightDoubleClick)(int id);
    void SetOnLeftClick(void(__cdecl *fn)(int id)) { m_onLeftClick = fn; }
    void SetOnLeftDoubleClick(void(__cdecl *fn)(int id)) { m_onLeftDoubleClick = fn; }
    void SetOnDrawCell(void(__cdecl *fn)(SpriteObject *icon, WidgetBase *grid, int id, int x, int y))
    {
        m_onDrawCell = fn;
    }
    void SetSelectionBevel(int on) { m_selectionBevel = on; }

    SpriteBox();
    ~SpriteBox();

    int init(int id);
    int ResetAndInitAlt(Heap *arena);
    void close();
    void draw_item(int idx);
    int calculate_dimensions();

    virtual int attach(WidgetBase *owner, int x, int y, int flags);
    int sprite(SpriteObject *icon, const char *text, int id);

    virtual void on_redraw();
    virtual void on_mouse_move(int x, int y);
    virtual void on_mouse_leave(int x, int y);
    virtual void on_left_click(int x, int y);
    virtual void on_left_down(int x, int y);
    virtual void on_left_up(int x, int y);
    virtual void on_right_click(int x, int y);
    virtual void on_right_down(int x, int y);
    virtual void on_right_up(int x, int y);
    virtual void on_left_double_click(int x, int y);
    virtual void on_right_double_click(int x, int y);
    virtual int on_key_down(int vk);
    virtual void on_dialog_focus(int on);

    int id_to_pos(int id);
    int pos_to_id(int idx);
};

#endif
