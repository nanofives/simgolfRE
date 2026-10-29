#include "jgldefs.h"
#include "jglwin.h"
#include "jgl.h"
#include "jglglobals.h"
void JGLSys::wait_task(void)
{
    if (do_task())
        return;
    WaitMessage();
}
int JGLSys::do_task(void)
{
    MSG msg;
    if (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_USER + 2) {
            if (g_config)
                g_config->OnNetMessage();
        } else {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        return 1;
    }
    return 0;
}
int JGLSys::do_non_input(void)
{
    MSG msg[3];
    int i;
    int best;
    for (i = 0; i < 3; i += 1) {
        msg[i].time = 0xffffffff;
    }
    if (PeekMessageA(&msg[0], 0, 0, WM_KEYFIRST - 1, PM_NOREMOVE) == 0
        && PeekMessageA(&msg[1], 0, WM_KEYLAST + 1, WM_MOUSEFIRST - 1, PM_NOREMOVE) == 0
        && PeekMessageA(&msg[2], 0, WM_MOUSEWHEEL + 1, 0xffff, PM_NOREMOVE) == 0) {
        return 0;
    }
    best = 0;
    for (i = 1; i < 3; i += 1) {
        if (msg[i].time < msg[best].time) {
            best = i;
        }
    }
    PeekMessageA(&msg[0], msg[best].hwnd, msg[best].message, msg[best].message, PM_REMOVE);
    TranslateMessage(&msg[0]);
    DispatchMessageA(&msg[0]);
    return 1;
}
int JGLSys::do_draw(void)
{
    MSG msg;
    if (!PeekMessageA(&msg, 0, WM_PAINT, WM_PAINT, PM_REMOVE))
        return 0;
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
    return 1;
}
int JGLSys::do_key(void)
{
    MSG msg;
    if (!PeekMessageA(&msg, 0, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
        return 0;
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
    return 1;
}
int JGLSys::do_mouse_move(void)
{
    MSG msg;
    if (PeekMessageA(&msg, 0, WM_MOUSEMOVE, WM_MOUSEMOVE, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
        return 1;
    }
    return 0;
}
int JGLSys::do_mouse_stroke(void)
{
    MSG msg;
    if (PeekMessageA(&msg, 0, WM_MOUSEFIRST, WM_MOUSEWHEEL, PM_REMOVE) == 0) {
        return 0;
    }
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
    if (msg.message == WM_LBUTTONDOWN) {
        if (PeekMessageA(&msg, msg.hwnd, WM_LBUTTONUP, WM_LBUTTONUP, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
            return 1;
        }
    }
    if (msg.message == WM_RBUTTONDOWN) {
        if (PeekMessageA(&msg, msg.hwnd, WM_RBUTTONUP, WM_RBUTTONUP, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
            return 1;
        }
    }
    return 1;
}
int JGLSys::do_char(void)
{
    MSG msg;
    if (!PeekMessageA(&msg, 0, WM_CHAR, WM_CHAR, PM_REMOVE))
        return 0;
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
    return 1;
}
int JGLSys::do_mouse(void)
{
    MSG msg;
    if (!PeekMessageA(&msg, 0, WM_MOUSEFIRST, WM_MOUSEWHEEL, PM_REMOVE))
        return 0;
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
    return 1;
}
void JGLSys::flush_keyboard(void)
{
    MSG msg;
    while (PeekMessageA(&msg, 0, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
        ;
}
void JGLSys::flush_mouse(void)
{
    MSG msg;
    while (PeekMessageA(&msg, 0, WM_MOUSEFIRST, WM_MOUSEWHEEL, PM_REMOVE))
        ;
}
int JGLSys::do_timer(void)
{
    MSG msg;
    if (!PeekMessageA(&msg, 0, WM_USER + 1, WM_USER + 1, PM_REMOVE))
        return 0;
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
    return 1;
}
void JGLSys::flush_timer(void)
{
    MSG msg;
    while (PeekMessageA(&msg, 0, WM_USER + 1, WM_USER + 1, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}
