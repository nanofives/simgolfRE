#include "jgldefs.h"
#include "GameMain.h"
#include "EngineInit.h"

int g_cursorWidth = 0;
int g_cursorHeight = 0;

MouseCursor::MouseCursor()
{
    m_hCursor = 0;
    m_reserved1 = 0;
    m_reserved2 = 0;
    m_image = 0;
}

void MouseCursor::Cleanup()
{
    if (m_hCursor) {
        DestroyCursor(m_hCursor);
        m_hCursor = 0;
    }
    if (m_image) {
        delete m_image;
        m_image = 0;
    }
    m_reserved1 = 0;
    m_reserved2 = 0;
}

int init_cursor_class()
{
    g_cursorWidth = GetSystemMetrics(SM_CXCURSOR);
    g_cursorHeight = GetSystemMetrics(SM_CYCURSOR);
    return 0;
}

void GetMousePosition(HWND hwnd, int *x, int *y)
{
    POINT pt;

    GetCursorPos(&pt);
    ScreenToClient(hwnd, &pt);
    *x = pt.x;
    *y = pt.y;
}

void SetMousePosition(HWND hwnd, int x, int y)
{
    POINT pt;

    pt.x = x;
    pt.y = y;
    ClientToScreen(hwnd, &pt);
    SetCursorPos(pt.x, pt.y);
}

#include "MouseCursorDtor.h"

int g_cursorSpare = 0;
