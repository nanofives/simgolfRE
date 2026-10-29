#include "jgldefs.h"
#include <iostream.h>
#include "jglmodearrays.h"
#include "jglwin.h"
#include "jgl.h"
#include "jgltypes.h"
#include <string.h>
struct Point {
    long x, y;
};
#include "gsobject.h"
#include "jglfont.h"
#include "jglpalettetypes.h"
#include "gspalette.h"
#include "jglpalettemgr.h"
#include "jglpalette.h"
#include "jglglobals.h"
#include "jglbuffer.h"
#include "jglpixelformat.h"
#include "gsbuffer.h"
#include "gssprite.h"
#include "bufferbase.h"
DisplayDeviceArray       device_array;
DisplayModeArray         mode_array;
DevModeArray             dev_array;
CRITICAL_SECTION         g_critSec;
extern "C" int __stdcall DllMain(void *hinstDLL, unsigned long fdwReason, void *lpvReserved)
{
    switch (fdwReason) {
    case 1:
        InitializeCriticalSection(&g_critSec);
        break;
    case 0:
        DeleteCriticalSection(&g_critSec);
        break;
    }
    return 1;
}
extern "C" {
JGLSys *sys = 0;
}
extern "C" JGLSys *get_graphsy_object_ptr()
{
    sys = new JGLSys;
    graphsys = sys;
    return sys;
}
struct WndClassA {
    unsigned int style;
    long(__stdcall *lpfnWndProc)(void *, unsigned int, unsigned int, long);
    int         cbClsExtra;
    int         cbWndExtra;
    void       *hInstance;
    void       *hIcon;
    void       *hCursor;
    void       *hbrBackground;
    const char *lpszMenuName;
    const char *lpszClassName;
};
void DestroyAllSprites();
void JGLSys::cleanup_system()
{
    DestroyAllSprites();
    delete sys;
}
JGLSys::JGLSys()
{
    m_width = 0;
    m_height = 0;
    m_bpp = 0;
    m_windowDC = 0;
    m_hInstance = 0;
    m_hwnd = 0;
    m_provider = 0;
}
#ifdef _USRDLL
inline GraphSys::GraphSys() {}
#endif
JGLSys::~JGLSys() {}
void JGLSys::close_app()
{
    ChangeDisplaySettingsA((LPDEVMODEA)&m_dmCurrent, 0);
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = 0;
    }
    if (m_provider) {
        m_provider->close_buffer();
        m_provider = 0;
    }
    UnregisterClassA("JackalClass", (HINSTANCE)m_hInstance);
    m_hInstance = 0;
}
int JGLSys::init_app(void *config, const char *title, BufferBase *provider)
{
    WndClassA wc;
    m_hInstance = GetModuleHandleA(0);
    g_config = (SysMessage *)config;
    m_provider = provider;
    wc.style = 0x2b;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = m_hInstance;
    wc.hIcon = LoadIconA(0, IDI_APPLICATION);
    wc.hCursor = 0;
    wc.hbrBackground = GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = "JackalClass";
    if (RegisterClassA((const WNDCLASSA *)&wc) == 0) {
        return 1;
    }
    unsigned long style = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    m_hwnd = CreateWindowExA(WS_EX_APPWINDOW, "JackalClass", title, style, 0, 0, GetSystemMetrics(SM_CXSCREEN),
                             GetSystemMetrics(SM_CYSCREEN), 0, 0, (HINSTANCE)m_hInstance, 0);
    if (m_hwnd == 0) {
        return 1;
    }
    m_width = GetSystemMetrics(SM_CXSCREEN);
    m_height = GetSystemMetrics(SM_CYSCREEN);
    m_bpp = 8;
    m_windowDC = GetDC(m_hwnd);
    SetBkMode(m_windowDC, 1);
    provider->init_buffer(m_width, m_height, m_bpp, 1);
    return 0;
}
DisplayModeArray *JGLSys::get_display_modes()
{
    DevModeA    dm;
    DisplayMode mode;
    int         i;
    dm.dmSize = sizeof(DevModeA);
    dm.dmFields = 0;
    dm.dmDriverExtra = 0;
    mode_array.Clear();
    dev_array.Clear();
    i = 0;
    while (EnumDisplaySettingsA(0, i++, (LPDEVMODEA)&dm)) {
        mode.bpp = dm.dmBitsPerPel;
        mode.width = dm.dmPelsWidth;
        mode.height = dm.dmPelsHeight;
        mode.virtualWidth = dm.dmPelsWidth;
        mode.freq = dm.dmDisplayFrequency;
        mode_array.add(mode);
        dev_array.add(dm);
    }
    return &mode_array;
}
void *JGLSys::get_hdc(void)
{
    return this->m_windowDC;
}
int JGLSys::init_display_quick_GL(int w, int h, int bpp)
{
    int               n = 0;
    Rect              rc;
    DisplayModeArray *modes;
    int               i;
    rc.left = 0;
    rc.right = w;
    rc.top = 0;
    rc.bottom = h;
    int best = -1;
    int bestFreq = -1;
    modes = get_display_modes();
    for (i = 0; i < modes->GetCount(); i++) {
        if (modes->m_pData[i].width == w && modes->m_pData[i].height == h && modes->m_pData[i].bpp == bpp
            && modes->m_pData[i].freq > bestFreq) {
            bestFreq = modes->m_pData[i].freq;
            best = i;
        }
    }
    init_display(best);
    return 0;
}
int JGLSys::init_display_quick(int w, int h, int bpp, bool bStore)
{
    if (!bStore)
        return SetDisplayMode(w, h, m_bpp);
    m_width = w;
    m_height = h;
    m_bpp = bpp;
    return m_provider->init_buffer(m_width, m_height, m_bpp, 1);
}
int JGLSys::SetDisplayMode(int w, int h, int bpp)
{
    DisplayModeArray *modes;
    int               i;
    int               best = -1;
    int               bestFreq = -1;
    modes = get_display_modes();
    for (i = 0; i < modes->GetCount(); i++) {
        if (modes->m_pData[i].width == w && modes->m_pData[i].height == h && modes->m_pData[i].bpp == bpp
            && modes->m_pData[i].freq > bestFreq) {
            if (modes->m_pData[i].freq > 75 && best > -1)
                return init_display(best);
            bestFreq = modes->m_pData[i].freq;
            best = i;
        }
    }
    if (best >= 0)
        return init_display(best);
    return 1;
}
int JGLSys::SetDisplayModeWithRefresh(int w, int h, int bpp, bool bDefer, int freq)
{
    DisplayModeArray *modes;
    int               i;
    int               best = -1;
    int               bestFreq = -1;
    if (bDefer)
        return init_display_quick(w, h, bpp, bDefer);
    modes = get_display_modes();
    for (i = 0; i < modes->GetCount(); i++) {
        if (modes->m_pData[i].width == w && modes->m_pData[i].height == h && modes->m_pData[i].bpp == bpp) {
            if (modes->m_pData[i].freq == freq) {
                best = i;
                break;
            }
            if (modes->m_pData[i].freq > bestFreq) {
                bestFreq = modes->m_pData[i].freq;
                best = i;
            }
        }
    }
    if (best >= 0)
        return init_display(best);
    return 1;
}
int JGLSys::SetDisplayModeToDesktop(int flag)
{
    int cx = GetSystemMetrics(SM_CXSCREEN);
    int cy = GetSystemMetrics(SM_CYSCREEN);
    return SetDisplayMode(cx, cy, flag);
}
int JGLSys::init_display(int nMode)
{
    DevModeA dm;
    ulong    dwFields = 0x1c0000;
    EnumDisplaySettingsA(0, (ulong)-1, (LPDEVMODEA)&m_dmCurrent);
    if (m_dmCurrent.dmPelsWidth == 640) {
        MessageBoxA(
            (HWND)graphsys->get_window_handle(),
            "Sid Meier's SimGolf must be played in 800x600 or higher resolution.  Please change your Windows desktop resolution to 800x600 or higher and restart the game.",
            "Display Request", MB_OK);
        return 2;
    }
    if (nMode >= dev_array.GetCount())
        return 3;
    dm = dev_array.m_pData[nMode];
    dm.dmSize = sizeof(DevModeA);
    dm.dmFields = 0x5c0000;
    if (ChangeDisplaySettingsA((LPDEVMODEA)&dm, 4))
        return 2;
    m_width = dm.dmPelsWidth;
    m_height = dm.dmPelsHeight;
    m_bpp = dm.dmBitsPerPel;
    return m_provider->init_buffer(m_width, m_height, m_bpp, 1);
}
int JGLSys::flip(Rect *rect)
{
    HDC        hdc;
    JGLBuffer *surf;
    Rect       r;
    surf = m_provider->m_surface;
    hdc = surf->get_hdc();
    surf->set_current_palette(def_palette->pal);
    if (rect == 0) {
        BitBlt(m_windowDC, 0, 0, GetRectWidth(surf->get_rect()), GetRectHeight(surf->get_rect()), hdc, 0, 0, 0xcc0020);
    } else {
        if (jgl_IntersectRect(&r, rect, surf->get_rect())) {
            BitBlt(m_windowDC, r.left, r.top, GetRectWidth(&r), GetRectHeight(&r), hdc, r.left, r.top, 0xcc0020);
        }
    }
    surf->release_hdc(1);
    return 0;
}
void JGLSys::get_window_rect(Rect *r)
{
    ::GetWindowRect(this->m_hwnd, (LPRECT)r);
}
void JGLSys::get_screen_dimensions(int *pw, int *ph)
{
    *pw = m_width;
    *ph = m_height;
}
void JGLSys::invalidate_rect(const Rect *r)
{
    InvalidateRect(this->m_hwnd, (const RECT *)r, 0);
}
void JGLSys::get_cursor_pos(int *px, int *py)
{
    Point pt;
    GetCursorPos((LPPOINT)&pt);
    ScreenToClient(this->m_hwnd, (LPPOINT)&pt);
    *px = pt.x;
    *py = pt.y;
}
void JGLSys::set_window_pos(int x, int y, int cx, int cy)
{
    SetWindowPos(this->m_hwnd, 0, x, y, cx, cy, 0);
}
int JGLSys::setTemporaryResolution(int width, int height)
{
    DevModeA dm;
    EnumDisplaySettingsA(0, (ulong)-1, (LPDEVMODEA)&m_dmRequested);
    memcpy(&dm, &m_dmRequested, sizeof(DevModeA));
    dm.dmPelsWidth = width;
    dm.dmPelsHeight = height;
    dm.dmFields = 0x1c0000;
    if (ChangeDisplaySettingsA((LPDEVMODEA)&dm, 4))
        return 2;
    return 0;
}
int JGLSys::resetTemporaryResolution(void)
{
    if (ChangeDisplaySettingsA((LPDEVMODEA) & this->m_dmRequested, 4))
        return 2;
    return 0;
}
DisplayDeviceArray::~DisplayDeviceArray()
{
    Reset();
}
DisplayModeArray::~DisplayModeArray()
{
    Reset();
}
int DisplayModeArray::GetCount()
{
    return m_count;
}
void DisplayModeArray::Clear()
{
    m_count = 0;
}
DevModeArray::~DevModeArray()
{
    Reset();
}
int DevModeArray::GetCount()
{
    return m_count;
}
void DevModeArray::Clear()
{
    m_count = 0;
}
inline int ModeArrayPad(int a, int b)
{
    int c = a * b;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c > 3)
        c -= a;
    c += b;
    return c + b;
}
GraphSys::~GraphSys() {}
void *GraphSys::get_window_handle()
{
    return 0;
}
void *GraphSys::get_app_instance()
{
    return 0;
}
void GraphSys::set_window_pos(int x, int y, int cx, int cy) {}
int  GraphSys::init_device(int)
{
    return 0x18;
}
int GraphSys::Unimplemented16(char *)
{
    return 0x18;
}
int GraphSys::get_devices(void)
{
    return 0;
}
void GraphSys::close_device(void) {}
int  GraphSys::get_light_data(int)
{
    return 0;
}
void GraphSys::delete_light_data(int) {}
int  GraphSys::clear_buffer()
{
    return 0x18;
}
int GraphSys::begin_scene()
{
    return 0x18;
}
int GraphSys::end_scene()
{
    return 0x18;
}
int GraphSys::create_verts(void **, ulong)
{
    return 0x18;
}
int GraphSys::set_material(int)
{
    return 0x18;
}
int GraphSys::set_world_transform(int)
{
    return 0x18;
}
int GraphSys::set_camera_transform(int)
{
    return 0x18;
}
int GraphSys::set_projection(float, float, float)
{
    return 0x18;
}
int GraphSys::load_texture(char *)
{
    return 0x18;
}
int GraphSys::set_texture(int, char *)
{
    return 0x18;
}
int GraphSys::zbuffer_enable(int)
{
    return 0x18;
}
int GraphSys::zwrite_enable(int)
{
    return 0x18;
}
int GraphSys::src_blend_enable(int)
{
    return 0x18;
}
int GraphSys::specular_enable(int)
{
    return 0x18;
}
int GraphSys::read_x_file(char *, int, int, int, int, int)
{
    return 0x18;
}
int GraphSys::copy_verts(void *, int, int, int)
{
    return 0x18;
}
int GraphSys::render_verts(void *, int)
{
    return 0x18;
}
int GraphSys::set_src_blend(int)
{
    return 0x18;
}
int GraphSys::set_dst_blend(int)
{
    return 0x18;
}
int GraphSys::set_cull_mode(int)
{
    return 0x18;
}
int GraphSys::set_fill_mode(int)
{
    return 0x18;
}
int GraphSys::set_shade_mode(int)
{
    return 0x18;
}
void  GraphSys::setCopyDataCallBack(int (*p)(unsigned int, long)) {}
void *JGLSys::get_window_handle(void)
{
    return this->m_hwnd;
}
void *JGLSys::get_app_instance(void)
{
    return this->m_hInstance;
}
void JGLSys::show_app(void)
{
    ShowWindow(this->m_hwnd, 1);
}
void JGLSys::hide_app(void)
{
    ShowWindow(this->m_hwnd, 0);
}
void JGLSys::minimize_app(void)
{
    ShowWindow(this->m_hwnd, 6);
}
void JGLSys::maximize_app(void)
{
    ShowWindow(this->m_hwnd, 3);
}
int JGLSys::screen_width(void)
{
    return this->m_width;
}
int JGLSys::screen_height(void)
{
    return this->m_height;
}
int JGLSys::screen_bpp(void)
{
    return this->m_bpp;
}
int JGLSys::screen_mask(void)
{
    return 0;
}
JGLFont *JGLSys::get_font_data(FontBase *metrics)
{
    return new JGLFont(metrics);
}
JGLPalette *JGLSys::get_palette_data(PaletteBase *client)
{
    return new JGLPalette(client);
}
JGLBuffer *JGLSys::get_buffer_data(BufferBase *owner, int memType)
{
    return new JGLBuffer(owner);
}
JGLSprite *JGLSys::get_sprite_data(JGLSprite **ppOwner, int memType)
{
    return new JGLSprite(ppOwner);
}
void JGLSys::delete_font_data(GSFont *p)
{
    delete p;
}
void JGLSys::delete_buffer_data(JGLBuffer *p)
{
    delete p;
}
void JGLSys::delete_palette_data(JGLPalette *p)
{
    delete p;
}
void JGLSys::delete_sprite_data(GSSprite *p)
{
    delete p;
}
void JGLSys::set_sprite_scale(int nX, int nY, int nDenom)
{
    g_scaleX = nX;
    g_scaleY = nY;
    g_scaleDenom = nDenom;
}
void JGLSys::get_sprite_scale(int *pa, int *pb, int *pc)
{
    *pa = g_scaleX;
    *pb = g_scaleY;
    *pc = g_scaleDenom;
}
int JGLSys::init_font_system(void)
{
    return JGLFont::init_class();
}
void JGLSys::close_font_system(void)
{
    JGLFont::close_class();
}
void JGLSys::init_palette_system(PaletteBase *pBase, PaletteBase *pAlt)
{
    JGLPalette::init_class(pBase, pAlt);
}
void JGLSys::close_palette_system(void)
{
    JGLPalette::close_class();
}
void JGLSys::setCopyDataCallBack(int (*p)(unsigned int, long))
{
    m_dmCurrent.SetGlobalDevmodeSink(p);
}
void MessageHandler::SetGlobalDevmodeSink(int (*p)(unsigned int, long))
{
    g_pfnCopyData = p;
}
#include "jglmodearrays.h"
DisplayDeviceArray::DisplayDeviceArray()
{
    m_data = 0;
    Init();
}
void DisplayDeviceArray::Reset()
{
    if (m_data)
        delete m_data;
    m_data = 0;
    m_capacity = 0;
    m_count = 0;
    m_grow = 1;
}
DisplayModeArray::DisplayModeArray()
{
    m_pData = 0;
    Init();
}
void DisplayModeArray::Reset()
{
    if (m_pData)
        delete m_pData;
    m_pData = 0;
    m_capacity = 0;
    m_count = 0;
    m_growBy = 1;
}
int DisplayModeArray::add(DisplayMode value)
{
    DisplayMode *newData;
    if (m_count < m_capacity) {
        m_pData[m_count] = value;
        m_count = m_count + 1;
    } else {
        newData = new DisplayMode[m_capacity + m_growBy];
        m_capacity = m_capacity + m_growBy;
        if (m_pData != 0) {
            memcpy(newData, m_pData, m_count * sizeof(DisplayMode));
        }
        newData[m_count] = value;
        m_count = m_count + 1;
        delete m_pData;
        m_pData = newData;
    }
    return m_count - 1;
}
DevModeArray::DevModeArray()
{
    m_pData = 0;
    Init();
}
void DevModeArray::Reset()
{
    if (m_pData)
        delete m_pData;
    m_pData = 0;
    m_capacity = 0;
    m_count = 0;
    m_growBy = 1;
}
int DevModeArray::add(DevModeA value)
{
    DevModeA *newData;
    if (m_count < m_capacity) {
        m_pData[m_count] = value;
        m_count = m_count + 1;
    } else {
        newData = new DevModeA[m_capacity + m_growBy];
        m_capacity = m_capacity + m_growBy;
        if (m_pData != 0) {
            memcpy(newData, m_pData, m_count * sizeof(DevModeA));
        }
        newData[m_count] = value;
        m_count = m_count + 1;
        delete m_pData;
        m_pData = newData;
    }
    return m_count - 1;
}
int DisplayDeviceArray::Init()
{
    Reset();
    return 0;
}
int DisplayModeArray::Init()
{
    Reset();
    return 0;
}
int DevModeArray::Init()
{
    Reset();
    return 0;
}
