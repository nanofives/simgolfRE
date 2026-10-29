#ifndef SPOT_H
#define SPOT_H

#include "jglrect.h"
#include "Timer.h"

struct Font;
struct WidgetBase;

class HotSpot {
public:
    HotSpot();
    virtual ~HotSpot() { close(); }
    void close();

    Rect m_rect;
    int m_param;
    int m_id;
    char *m_text;

    static int s_keepText;
};

class Spot {
public:
    Spot();
    virtual ~Spot();

    void increase_spots();
    void clear();
    void shutdown();
    void init(int capacity);
    int add(int id, int param, int x, int y, int w, int h, char *text);
    int check(int x, int y, int *pId, int *pParam);
    int check(int x, int y, int *pId, int *pParam, Rect *pRect);
    static void __cdecl timer_callback_daemon(int index, void *list);
    void timer_callback(int index);
    void on_move(int id, int param, WidgetBase *owner);
    void on_leave();

    int m_hoverId;
    int m_hoverParam;
    WidgetBase *m_owner;
    Font *m_font;
    int m_reserved;
    int m_fillColor;
    int m_borderColor;
    Time m_timer;
    HotSpot *m_items;
    int m_capacity;
    int m_count;
    int m_delayMs;
};

#endif
