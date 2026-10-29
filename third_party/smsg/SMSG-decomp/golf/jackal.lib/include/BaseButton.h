#ifndef BASEBUTTON_H
#define BASEBUTTON_H

#include "WidgetBase.h"
#include "Timer.h"

struct ButtonGroup;

struct ButtonState {
    int m_colors[4];
    int m_frameColor;
    int m_frameColorAlt;
    int m_pressed;
    unsigned long m_minPressMs;

    void Set(int which, int v)
    {
        if (which < 4)
            m_colors[which] = v;
        else if (which == 4)
            m_frameColor = v;
        else
            m_frameColorAlt = v;
    }
};

struct BaseButton : public GraphicWin {
    BaseButton();
    virtual ~BaseButton();
    virtual void close();

    int init(char *text, int id, int x, int y, int w, int h, WidgetBase *parent, int flag);
    void set_text_color(int color, int shadowColor, int shadowDx, int shadowDy);
    void set_text_color2(int color, int shadowColor, int shadowDx, int shadowDy);
    void set_text_color3(int color, int shadowColor, int shadowDx, int shadowDy);
    int set_name(const char *text);
    int set_bubble_text(const char *text);

    virtual void on_left_down(int x, int y);
    virtual void on_left_up(int x, int y);
    virtual int on_key_click(int ch, int vk);
    virtual void on_mouse_leave(int x, int y);
    virtual void on_mouse_move(int x, int y);
    virtual void on_right_click(int x, int y);
    virtual void on_left_click(int x, int y);
    virtual void on_dialog_focus(int on);
    static void __cdecl timer_callback_daemon(int id, void *ctx);
    void timer_callback(int id);
    void set(int selected);
    virtual void hide();

    int m_reserved;
    int m_selected;
    Time m_tooltipTimer;
    int m_repeatDelay;
    int m_repeatPeriod;
    Time m_holdTimer;
    int m_hover;
    int m_id;
    int m_pressOffsetX;
    int m_pressOffsetY;
    char *m_text;
    char *m_tooltip;
    ButtonState m_state;
    unsigned long m_pressTime;
    ButtonGroup *m_group;
    int m_disabled;
    void(__cdecl *m_onClick)(int id);
    void(__cdecl *m_onRepeat)(int id);
};

extern void (*g_buttonHoverHook)();
extern void (*g_buttonIdMinus1Hook)();
extern void (*g_buttonIdMinus2Hook)();

#endif
