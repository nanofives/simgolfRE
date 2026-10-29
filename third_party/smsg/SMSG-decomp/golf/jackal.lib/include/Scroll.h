#ifndef SCROLL_H
#define SCROLL_H

#include "jglrect.h"
#include "WidgetBase.h"
#include "ImageButton.h"

extern int g_scrollDefaultFlagsLow[2];
extern SpriteObject *g_scrollHorzNear[3];
extern SpriteObject *g_scrollVertNear[3];
extern SpriteObject *g_scrollHorzFar[3];
extern SpriteObject *g_scrollVertFar[3];
extern SpriteObject *g_scrollThumbTrack[2];
extern int g_scrollTheme[11];
extern "C" int g_scrollbarNonClient;
extern int scroll_from_key;

extern "C" __declspec(dllimport) unsigned long __stdcall timeGetTime(void);
extern "C" __declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);

struct Scroll : public GraphicWin {
    int m_flags;
    int m_id;
    int m_bevelColor;
    int m_rangeLo;
    int m_rangeHi;
    int m_inverted;
    int m_scrollPos;
    int m_repeatDelay;
    unsigned m_rate;
    int m_firstStepTime;
    int m_drag;
    int m_pageStep;
    int m_pageDir;
    int f5A8;
    Rect m_thumb;
    int m_arrowColor;
    int m_extent;
    int m_backColor;
    int m_bevelDepth;
    int m_bevelHiColor;
    int m_bevelLoColor;
    SpriteObject *m_thumbSprite;
    SpriteObject *m_trackSprite;
    SpriteObject *m_imgHorzNear[3];
    SpriteObject *m_imgVertNear[3];
    SpriteObject *m_imgHorzFar[3];
    SpriteObject *m_imgVertFar[3];
    ImageButton m_arrowNear;
    ImageButton m_arrowFar;
    void (*m_onStep)(int id, int pos);
    void (*m_onRepeat)(int id, int pos);

    Scroll();
    virtual ~Scroll() { close(); }
    void close();
    int init(int x, int y, int w, int h, WidgetBase *parent, int id, int flags);
    int init(Rect *rect, WidgetBase *parent, int id, int flags);
    int init_vert(int x, int y, int height, WidgetBase *parent, int id);
    int init_horz(int x, int y, int width, WidgetBase *parent, int id);
    int init_vert_nc(int x, int y, int height, WidgetBase *parent, int id);
    int init_horz_nc(int x, int y, int width, WidgetBase *parent, int id);
    void set_range(int lo, int hi);
    void set_bar_thickness(int extent);
    void set_pos(int pos);
    void compute_thumb_rect(Rect *out);
    void set_thumb_rect();

    virtual void on_redraw();
    virtual void on_resize(int w, int h);
    virtual void on_mouse_move(int x, int y);
    virtual void on_left_click(int x, int y);
    virtual void on_left_down(int x, int y);
    virtual void on_left_up(int x, int y);
    virtual int on_key_click(int ch, int vk);
    virtual int on_button_pressed(int id);
    virtual void on_button_clicked(int id);
};

#endif
