#include "jgldefs.h"
#include <windows.h>
#include <ddraw.h>

#include "Display.h"

static DisplayFormat d_pixformat;
static IDirectDraw *d_directdraw = 0;
static IDirectDrawSurface *d_surface = 0;
static void *d_data = 0;

DisplayFormat *display_format()
{
    return &d_pixformat;
}

int display_depth()
{
    int bpp = d_pixformat.bpp;
    if (!bpp)
        bpp = 8;
    return bpp;
}

void display_close()
{
    display_unlock();
    if (d_directdraw) {
        d_directdraw->RestoreDisplayMode();
        d_directdraw->Release();
        d_directdraw = 0;
    }
    d_surface = 0;
    OnDisplayShutdown();
}

void display_unlock()
{
    if (d_surface && d_data) {
        d_surface->Unlock(d_data);
        d_data = 0;
    }
}

void OnDisplayShutdown()
{
}

int GetDisplayBitDepth(HDC dc)
{
    return GetDeviceCaps(dc, BITSPIXEL);
}

int g_displaySpare[3] = {0};
