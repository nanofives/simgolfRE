#ifndef CHECKBOXCTL_H
#define CHECKBOXCTL_H

#include "WidgetBase.h"
#include "ListBoxCtl.h"
#include "Sprite.h"

struct CheckBoxCtl : virtual GraphicWin, virtual Dialog {
    int m_focus;
    void(__cdecl *m_onChange)(int mask);
    void(__cdecl *m_onActivate)(int mask);
    void SetOnActivate(void(__cdecl *fn)(int mask)) { m_onActivate = fn; }
    SpriteObject *m_uncheckedSprite;
    SpriteObject *m_checkedSprite;

    CheckBoxCtl();
    virtual ~CheckBoxCtl();
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

    virtual int attach(WidgetBase *owner, int x, int y, int flags);
    int item(const char *text, int key, int checked);

    void flip_state_id(int key);
    void flip_state_pos(int i);
    void set_state_id(int key, int on);
    int get_state_id(int key);
    void set_state_pos(int i, int on);
    int get_state_pos(int i);
    void set_state_flag(int mask);
    int GetCheckMask() { return m_selected; }

    int calculate_dimensions();
    void draw_item(int idx, int checked, int focused);
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

    virtual void on_left_down(int x, int y);
    virtual int on_key_down(int vk);
    virtual void on_left_double_click(int x, int y);
    virtual void on_redraw();
    virtual void on_dialog_focus(int on);
    virtual void on_mouse_move(int x, int y);
    virtual void on_mouse_leave(int x, int y);

    static SpriteObject s_checkBoxEmpty;
    static SpriteObject s_checkBoxTicked;
    static SpriteObject *s_defaultSprites[2];
    static int init_class();
    static void close_class();
};

#endif
