#include "jgldefs.h"
#include <windows.h>

#include "SysMessageHandler.h"

void SysMessageHandler::OnSize(int w, int h)
{
    if (g_rootWidget != 0)
        g_rootWidget->resize(w, h, 0, 0);
}

void SysMessageHandler::OnPaint(RECT *r)
{
    Rect *clip = (Rect *)r;
    RenderWidgetTree(clip, 0);
    WidgetBase::flip(clip);
}

long SysMessageHandler::OnNCHitTest(int x, int y)
{
    if (g_rootWidget != 0 && g_resizingWidget == 0 && (g_rootWidget->m_status & 2) != 0) {
        Rect r;
        graphsys->get_window_rect(&r);
        x -= r.left;
        y -= r.top;
        WidgetBase *w = g_rootWidget;
        int capH = w->m_borderWidth;
        int inset = capH / 2;
        if (w->m_flags & 0x10)
            capH = w->m_captionHeight;
        if ((w->m_flags & 0x400) == 0)
            return (y >= capH) ? 1 : 2;
        if (x < inset) {
            if (y < inset)
                return 13;
            return (y > w->m_screen.bottom - w->m_screen.top - inset - 1) ? 16 : 10;
        }
        if (x > w->m_screen.right - w->m_screen.left - inset - 1) {
            if (y < inset)
                return 14;
            return (y > w->m_screen.bottom - w->m_screen.top - inset - 1) ? 17 : 11;
        }
        if (y < inset)
            return 12;
        if (y > w->m_screen.bottom - w->m_screen.top - inset - 1)
            return 15;
        return (y < capH) ? 2 : 0;
    }
    return 1;
}

void SysMessageHandler::OnKey(unsigned int vk, int down)
{
    if (GetKeyState(VK_SHIFT) & 0x8000)
        vk |= 0x10000;
    if (GetKeyState(VK_CONTROL) & 0x8000)
        vk |= 0x20000;
    if (GetKeyState(VK_MENU) & 0x8000)
        vk |= 0x40000;
    WidgetBase *w = GetActiveWidget();
    if (w)
        w->on_key(vk, down);
    if (g_keyDownHook && down)
        g_keyDownHook(vk);
    if (g_afterInputHook)
        g_afterInputHook();
}

void SysMessageHandler::OnChar(char ch)
{
    WidgetBase *w = GetActiveWidget();
    if (w)
        w->on_char(ch);
}

void SysMessageHandler::OnMouseMove(int x, int y)
{
    if (g_resizingWidget) {
        g_resizingWidget->do_tracking(x, y);
        return;
    }
    WidgetBase *w = FindWidgetAtPoint(&x, &y);
    UpdateCursor(w, 1, -1, -1);
    if (!w)
        return;
    if (g_hoverWidget != 0 && w != g_hoverWidget) {
        if (g_dragWidget == 0 || g_dragWidget == g_hoverWidget) {
            g_hoverWidget->on_mouse_leave(x, y);
            g_hoverWidget->m_hotspots.on_leave();
        }
    }
    g_hoverWidget = w;
    w->OnMouseMoveInternal(x, y, g_pointInFrame);
}

void SysMessageHandler::OnMouseWheel(int delta, int x, int y)
{
    WidgetBase *w = FindWidgetAtPoint(&x, &y);
    UpdateCursor(w, 1, -1, -1);
    if (w != 0 && g_pointInFrame == 0)
        w->on_mouse_wheel(delta, x, y);
}

void SysMessageHandler::OnLButtonDown(int dbl, int x, int y)
{
    WidgetBase *w = FindWidgetAtPoint(&x, &y);
    if (w) {
        if (!(w == g_rootWidget && g_pointInFrame != 0)) {
            if (g_pointInFrame != 0 || (w->m_flags & 0x4000000)) {
                if (x >= 0 && y >= 0) {
                    int hit = w->on_nc_hittest(x, y);
                    if (hit) {
                        w->bring_to_top();
                        if (hit == 2 || (hit > 9 && hit <= 17)) {
                            g_resizeHit = hit;
                            g_resizingWidget = w;
                            g_resizeGrabX = x;
                            g_resizeGrabY = y;
                            return;
                        }
                    }
                }
            }
            w->on_l_button_down(dbl, x, y, g_pointInFrame);
        }
    }
    if (g_lButtonDownHook)
        g_lButtonDownHook(x, y);
    if (g_afterInputHook)
        g_afterInputHook();
}

void SysMessageHandler::OnLButtonUp(int x, int y)
{
    g_resizingWidget = 0;
    WidgetBase *w = FindWidgetAtPoint(&x, &y);
    if (w)
        w->on_l_button_up(x, y, g_pointInFrame);
}

void SysMessageHandler::OnRButtonDown(int dbl, int x, int y)
{
    WidgetBase *w = FindWidgetAtPoint(&x, &y);
    if (w != 0)
        w->on_r_button_down(dbl, x, y, g_pointInFrame);
    if (g_rButtonDownHook != 0)
        g_rButtonDownHook(x, y);
    if (g_afterInputHook != 0)
        g_afterInputHook();
}

void SysMessageHandler::OnRButtonUp(int x, int y)
{
    g_resizingWidget = 0;
    WidgetBase *w = FindWidgetAtPoint(&x, &y);
    if (w)
        w->on_r_button_up(x, y, g_pointInFrame);
}

void SysMessageHandler::OnActivate(int active, int minimized)
{
    if (active == 0) {
        g_mouseDownWidget = 0;
        g_mouseDownWidgetAlt = 0;
        if (g_cursorClipWidget != 0)
            ClipCursor(0);
    } else {
        if (g_cursorClipWidget != 0)
            g_cursorClipWidget->set_mouse_confines(&g_cursorConfineRect);
    }
}

void SysMessageHandler::OnJackalTimer(unsigned int wParam)
{
    Time *t = (Time *)wParam;
    t->m_posted = 0;
    if (task_flags)
        return;
    unsigned long flags = t->m_flags;
    if (flags & 1) {
        if (flags & 2) {
            t->stop();
            return;
        }
        t->m_flags = flags | 2;
    }
    if (g_activeTimer == 0 || t == g_activeTimer) {
        if (t->m_onTick)
            t->m_onTick(t->m_arg);
        if (t->m_callback)
            t->m_callback(t->m_arg, t->m_context);
    }
}

void SysMessageHandler::OnNetMessage()
{
    if (g_pNetwork)
        g_pNetwork->handle_sys_msg();
}
