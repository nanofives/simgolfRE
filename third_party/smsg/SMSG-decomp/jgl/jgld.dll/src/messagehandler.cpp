#include "jgldefs.h"
#include "jgl.h"
#include "jglwin.h"
#include "jglbuffer.h"
typedef char GraphsySizeCheck[(sizeof(JGLSys) == 0x14c) ? 1 : -1];
#include "jglglobals.h"
extern "C" {
int (*g_pfnCopyData)(unsigned int, long);
}
extern "C" {
SysMessage *g_config;
}
extern "C" long OnCopyData(void *, unsigned int, long);
extern "C" int  OnQueryNewPalette(void *);
extern "C" void OnPaletteChanged(void *, unsigned int);
extern "C" void OnPaint(void *);
extern "C" void OnActivate(void *, int, long, int);
extern "C" void OnSize(void *, unsigned int, int, int);
extern "C" long OnNcHitTest(void *, int, int);
extern "C" void OnKey(void *, unsigned int, int, int, int);
extern "C" void OnChar(void *, char, int);
extern "C" void OnMouseWheel(void *, int, int, int, int);
extern "C" void OnMouseMove(void *, int, int, unsigned int);
extern "C" void OnLButtonDown(void *, int, int, int, unsigned int);
extern "C" void OnLButtonUp(void *, int, int, unsigned int);
extern "C" void OnRButtonDown(void *, int, int, int, unsigned int);
extern "C" void OnRButtonUp(void *, int, int, unsigned int);
MessageHandler::MessageHandler()
{
    g_pfnCopyData = 0;
}
extern "C" long __stdcall WndProc(void *hwnd, unsigned int uMsg, unsigned int wParam, long lParam)
{
    switch (uMsg) {
    case WM_USER + 1:
        g_config->OnJackalTimer(wParam);
        break;
    case WM_COPYDATA:
        return OnCopyData(hwnd, wParam, lParam);
    case WM_POWERBROADCAST:
        switch (wParam) {
        case PBT_APMQUERYSUSPEND:
            return 0x424d5144;
        }
    case WM_SYSCOMMAND:
        switch (wParam) {
        case SC_SCREENSAVE:
        case SC_MONITORPOWER:
            return 0;
        }
        break;
    case WM_QUERYNEWPALETTE:
        return (unsigned short)OnQueryNewPalette(hwnd);
    case WM_PALETTECHANGED:
        OnPaletteChanged(hwnd, wParam);
        return 0;
    case WM_PAINT:
        OnPaint(hwnd);
        return 0;
    case WM_ACTIVATE:
        OnActivate(hwnd, LOWORD(wParam), lParam, HIWORD(wParam));
        return 0;
    case WM_SIZE:
        OnSize(hwnd, wParam, (short)LOWORD(lParam), (short)HIWORD(lParam));
        return 0;
    case WM_NCHITTEST:
        return OnNcHitTest(hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam));
    case WM_KEYDOWN:
        OnKey(hwnd, wParam, 1, (short)LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_KEYUP:
        OnKey(hwnd, wParam, 0, (short)LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_CHAR:
        OnChar(hwnd, wParam, (short)LOWORD(lParam));
        return 0;
    case WM_SYSKEYDOWN:
        OnKey(hwnd, wParam, 1, (short)LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_SYSKEYUP:
        OnKey(hwnd, wParam, 0, (short)LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_MOUSEWHEEL:
        OnMouseWheel(hwnd, (short)HIWORD(wParam), (short)LOWORD(lParam), (short)HIWORD(lParam), LOWORD(wParam));
        break;
    case WM_MOUSEMOVE:
        OnMouseMove(hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam), wParam);
        return 0;
    case WM_LBUTTONDBLCLK:
        OnLButtonDown(hwnd, 1, (short)LOWORD(lParam), (short)HIWORD(lParam), wParam);
        return 0;
    case WM_LBUTTONDOWN:
        OnLButtonDown(hwnd, 0, (short)LOWORD(lParam), (short)HIWORD(lParam), wParam);
        return 0;
    case WM_LBUTTONUP:
        OnLButtonUp(hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam), wParam);
        return 0;
    case WM_RBUTTONDBLCLK:
        OnRButtonDown(hwnd, 1, (short)LOWORD(lParam), (short)HIWORD(lParam), wParam);
        return 0;
    case WM_RBUTTONDOWN:
        OnRButtonDown(hwnd, 0, (short)LOWORD(lParam), (short)HIWORD(lParam), wParam);
        return 0;
    case WM_RBUTTONUP:
        OnRButtonUp(hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam), wParam);
        return 0;
    }
    return DefWindowProcA((HWND)hwnd, uMsg, wParam, lParam);
}
extern "C" int OnQueryNewPalette(void *hWnd)
{
    void *hdc = GetDC((HWND)hWnd);
    SelectPalette((HDC)hdc, (HPALETTE)g_hPalette, 0);
    RealizePalette((HDC)hdc);
    ReleaseDC((HWND)hWnd, (HDC)hdc);
    return 1;
}
extern "C" void OnPaletteChanged(void *hWnd, unsigned int wParam)
{
    if (hWnd != (void *)wParam)
        OnQueryNewPalette(hWnd);
}
extern "C" void OnPaint(void *hWnd)
{
    PAINTSTRUCT ps;
    void       *hdc;
    RECT        r;
    hdc = BeginPaint((HWND)hWnd, &ps);
    if (hdc == 0)
        return;
    r.left = ps.rcPaint.left;
    r.top = ps.rcPaint.top;
    r.right = ps.rcPaint.right;
    r.bottom = ps.rcPaint.bottom;
    g_config->OnPaint(&r);
    EndPaint((HWND)hWnd, &ps);
}
extern "C" void OnActivate(void *hWnd, int active, long lParam, int minimized)
{
    InvalidateRect((HWND)hWnd, 0, 0);
    if (active != 0 && minimized == 0)
        OnQueryNewPalette(hWnd);
    g_config->OnActivate(active, minimized);
    DefWindowProcA((HWND)hWnd, 6, MAKELONG(active, minimized), lParam);
}
extern "C" void OnKey(void *hWnd, unsigned int vk, int down, int repeat, int flags)
{
    MSG msg;
    g_config->OnKey(vk, down);
    if (down != 0) {
        if (vk < 0x60 || vk > 0x6f) {
            while (graphsys->do_char())
                ;
        } else {
            while (PeekMessageA(&msg, (HWND)hWnd, 0x102, 0x102, 1))
                ;
        }
    }
}
extern "C" void OnChar(void *hWnd, char ch, int repeat)
{
    g_config->OnChar(ch);
}
extern "C" void OnSize(void *hWnd, unsigned int flag, int w, int h)
{
    g_config->OnSize(w, h);
}
extern "C" long OnNcHitTest(void *hWnd, int x, int y)
{
    return g_config->OnNCHitTest(x, y);
}
extern "C" void OnMouseWheel(void *hWnd, int delta, int x, int y, int keys)
{
    g_config->OnMouseWheel(delta, x, y);
}
extern "C" void OnMouseMove(void *hWnd, int x, int y, unsigned int keys)
{
    g_config->OnMouseMove(x, y);
}
extern "C" void OnLButtonDown(void *hWnd, int dbl, int x, int y, unsigned int keys)
{
    g_config->OnLButtonDown(dbl, x, y);
}
extern "C" void OnLButtonUp(void *hWnd, int x, int y, unsigned int keys)
{
    g_config->OnLButtonUp(x, y);
}
extern "C" void OnRButtonDown(void *hWnd, int dbl, int x, int y, unsigned int keys)
{
    g_config->OnRButtonDown(dbl, x, y);
}
extern "C" void OnRButtonUp(void *hWnd, int x, int y, unsigned int keys)
{
    g_config->OnRButtonUp(x, y);
}
extern "C" long OnCopyData(void *hwnd, unsigned int wParam, long lParam)
{
    if (g_pfnCopyData != 0)
        return g_pfnCopyData(wParam, lParam);
    return 0;
}
