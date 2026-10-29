#include "jgldefs.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "Spot.h"
#include "StringUtil.h"
#include "TextRenderer.h"
#include "WidgetBase.h"
#include "JackalUtil.h"

int HotSpot::s_keepText;

int in_box(int x, int y, int left, int top, int w, int h)
{
    if (x < left)
        return 0;
    if (x >= left + w)
        return 0;
    if (y < top)
        return 0;
    return y < top + h;
}

int in_box(int x, int y, Rect *r)
{
    if (x < r->left)
        return 0;
    if (x >= r->right)
        return 0;
    if (y < r->top)
        return 0;
    return y < r->bottom;
}

HotSpot::HotSpot()
{
    m_text = 0;
}

void HotSpot::close()
{
    if (s_keepText)
        return;
    if (m_text) {
        FreeIfNotNull(m_text);
        m_text = 0;
    }
}

void Spot::increase_spots()
{
    HotSpot *grown;
    int i;

    m_capacity += 10;
    grown = new HotSpot[m_capacity];
    for (i = 0; i < m_count; i++) {
        HotSpot *s = &m_items[i];
        grown[i].m_rect = s->m_rect;
        grown[i].m_param = s->m_param;
        grown[i].m_id = s->m_id;
        grown[i].m_text = s->m_text;
    }
    HotSpot::s_keepText = 1;
    delete[] m_items;
    HotSpot::s_keepText = 0;
    m_items = grown;
}

void Spot::clear()
{
    m_capacity = 0;
    m_count = 0;
    m_items = 0;
    m_owner = 0;
    m_font = 0;
    m_reserved = 0;
    m_fillColor = -1;
    m_borderColor = 0;
    m_hoverId = -1;
    m_hoverParam = -1;
    m_timer.close();
}

void Spot::shutdown()
{
    if (m_items) {
        delete[] m_items;
        m_items = 0;
    }
    clear();
}

Spot::Spot()
{
    m_delayMs = 1000;
    clear();
}

Spot::~Spot()
{
    shutdown();
}

void Spot::init(int capacity)
{
    shutdown();
    HotSpot *p = new HotSpot[capacity];
    m_items = p;
    if (m_items) {
        m_capacity = capacity;
        m_count = 0;
    }
}

int Spot::add(int id, int param, int x, int y, int w, int h, char *text)
{
    int i;

    if (m_count >= m_capacity)
        increase_spots();
    i = m_count;
    m_count = i + 1;
    m_items[i].close();
    m_items[i].m_rect.SetRect(x, y, w, h);
    m_items[i].m_id = id;
    m_items[i].m_param = param;
    if (text) {
        m_items[i].m_text = (char *)AllocateBytes(strlen(text) + 1);
        if (m_items[i].m_text == 0)
            return 4;
        ClearString(m_items[i].m_text);
        AppendString(m_items[i].m_text, text);
    }
    return i;
}

int Spot::check(int x, int y, int *pId, int *pParam)
{
    int i;

    for (i = m_count - 1; i >= 0; --i) {
        if (in_box(x, y, &m_items[i].m_rect)) {
            if (pId)
                *pId = m_items[i].m_id;
            if (pParam)
                *pParam = m_items[i].m_param;
            return i;
        }
    }
    return -1;
}

int Spot::check(int x, int y, int *pId, int *pParam, Rect *pRect)
{
    int i;

    for (i = m_count - 1; i >= 0; --i) {
        if (in_box(x, y, &m_items[i].m_rect)) {
            if (pId)
                *pId = m_items[i].m_id;
            if (pParam)
                *pParam = m_items[i].m_param;
            if (pRect)
                *pRect = m_items[i].m_rect;
            return i;
        }
    }
    return -1;
}

void __cdecl Spot::timer_callback_daemon(int index, void *list)
{
    if (list)
        ((Spot *)list)->timer_callback(index);
}

void Spot::timer_callback(int index)
{
    HotSpot *e;
    Rect r;
    int ox, oy;

    m_timer.stop();
    if (m_owner == 0)
        return;
    if (m_items[index].m_text == 0)
        return;
    e = &m_items[index];
    r = e->m_rect;
    ox = 0;
    oy = 0;
    m_owner->client_to_screen(&ox, &oy);
    r.Offset(ox, oy);
    if (m_font == 0)
        g_uiFont = g_defaultFont;
    else
        g_uiFont = m_font;
    g_tooltipFillColor = m_fillColor;
    g_tooltipBorderColor = m_borderColor;
    m_owner->set_bubble_text(e->m_text, &r);
}

void Spot::on_move(int id, int param, WidgetBase *owner)
{
    int n;
    int i;

    m_owner = owner;
    n = m_count;
    for (i = 0; i < n; i++) {
        HotSpot *e = &m_items[i];
        if (e->m_id == id && e->m_param == param)
            break;
    }
    if (i != n) {
        if (m_items[i].m_text) {
            m_timer.SetCallback(&timer_callback_daemon, i, this, m_delayMs, 5);
            m_timer.pulse();
        }
        m_hoverId = id;
        m_hoverParam = param;
    }
}

void Spot::on_leave()
{
    if (m_hoverId != -1 || m_hoverParam != -1) {
        m_timer.stop();
        WidgetBase::clear_bubble_text();
        m_hoverId = -1;
        m_hoverParam = -1;
    }
}
