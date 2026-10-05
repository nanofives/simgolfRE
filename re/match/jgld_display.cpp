// FLAGS jgld.dll: /Od /ZI /GZ /GX
// jgld.dll entry point and the display object (debug build). Names are chosen here.
#include <windows.h>
extern CRITICAL_SECTION g_cs;    // 0x101286b0
struct ModeInfo;
template <class T> class Array;
struct DisplayBase {
    DisplayBase();               // 0x10065630
    virtual ~DisplayBase();
    virtual void b1(); virtual void b2(); virtual void b3(); virtual void b4(); virtual void b5(); virtual void b6(); virtual void b7(); virtual HWND getHwnd(); virtual void b9(); virtual void b10(); virtual void b11(); virtual void b12(); virtual void b13(); virtual void b14(); virtual void b15(); virtual void b16(); virtual void b17(); virtual void b18(); virtual void b19(); virtual void b20(); virtual void b21();
    virtual int setModeDirect(int w, int h, int bpp);   // slot 22 (+0x58)
    virtual void b23();
    virtual int setModeFull(int w, int h, int bpp, bool apply);   // slot 24 (+0x60)
    virtual int setModeIndex(int i);                    // slot 25 (+0x64)
    virtual Array<ModeInfo>* enumModes();              // slot 26 (+0x68)
};
struct DisplayMember { DisplayMember(); DEVMODEA m_cur; DEVMODEA m_orig; }; // +4, ctor 0x10069270
struct Listener;
struct ModeInfo { DWORD w, h, w2, bpp, freq; };
template <class T> class Array {             // jgld_font.cpp
public:
    int count();                             // 0x10066a80
    void clear();
    virtual ~Array();
    T* m_data;
    int m_cap;
    int m_count;
    int m_grow;
    int add(T v);
};
extern Array<ModeInfo> g_modes;              // 0x101286c8
extern Array<DEVMODEA> g_devmodes;           // 0x101286f8
struct Surf {                                // jgld_surface.cpp Surface, slots used here
    virtual void s0(); virtual void s1(); virtual void s2(); virtual void s3(); virtual void s4(); virtual void s5(); virtual void s6(); virtual void s7(); virtual void s8(); virtual void s9();
    virtual HDC getDC();                     // +0x28
    virtual void releaseDC(int);             // +0x2c
    virtual void t12(); virtual void t13(); virtual void t14(); virtual void t15(); virtual void t16(); virtual void t17(); virtual void t18(); virtual void t19(); virtual void t20(); virtual void t21(); virtual void t22(); virtual void t23(); virtual void t24(); virtual void t25(); virtual void t26(); virtual void t27(); virtual void t28(); virtual void t29(); virtual void t30(); virtual void t31(); virtual void t32(); virtual void t33(); virtual void t34(); virtual void t35(); virtual void t36(); virtual void t37(); virtual void t38(); virtual void t39(); virtual void t40(); virtual void t41(); virtual void t42(); virtual void t43(); virtual void t44(); virtual void t45(); virtual void t46(); virtual void t47(); virtual void t48(); virtual void t49(); virtual void t50(); virtual void t51(); virtual void t52();
    virtual RECT* bounds();                  // +0xd4
    virtual void u54(); virtual void u55(); virtual void u56(); virtual void u57(); virtual void u58();
    virtual void selectPalette(HPALETTE p);  // +0xec
};
struct PalHolder { int m_0; HPALETTE m_pal; };
extern PalHolder* g_palHolder;               // 0x1012873c
int rectWidth(const RECT* r);                // 0x10009120
int rectHeight(const RECT* r);               // 0x10009160
int intersect(RECT* out, const RECT* a, const RECT* b);   // 0x10008590
extern const char s_needRes[];               // 0x1011d988 (message text, placeholder)
extern const char s_needResTitle[];          // 0x1011da48 (caption, placeholder)
struct Mode { virtual int set(int w, int h, int bpp, int full); Surf* m_surf; };
class Display : public DisplayBase {
public:
    Display();
    virtual ~Display();
    int createWindow(Listener* l, const char* title, Mode* mode);
    Array<ModeInfo>* enumModes();
    int findMode(int w, int h, int bpp);
    void setMode(int w, int h, int bpp, bool apply);
    int pickMode(int w, int h, int bpp);
    int pickModeFreq(int w, int h, int bpp, bool apply, int freq);
    void setDesktopMode(int bpp);
    int setModeIndex(int i);
    int present(RECT* r);
    void getCursor(LONG* x, LONG* y);
    int setResolution(DWORD w, DWORD h);
    int restore();
    DisplayMember m_4;
    int m_width;                 // +0x12c
    int m_height;                // +0x130
    int m_bpp;                   // +0x134
    HDC m_dc;                    // +0x138
    int m_13c;
    HINSTANCE m_hinst;           // +0x140
    HWND m_hwnd;                 // +0x144
    Mode* m_mode;                // +0x148
};
extern Listener* g_listener;     // 0x1012872c
LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);   // 0x10001302 (thunk)
extern const char s_className[]; // 0x1011d... window class name (placeholder)
extern Display* g_display;       // 0x10128420
extern Display* g_main;          // 0x1012870c
struct Tracked { static void deleteAll(); };
// MATCH: jgld.dll 0x10065300 _DllMain@12
extern "C" BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved)
{
    switch (reason) {
    case 1:
        InitializeCriticalSection(&g_cs);
        break;
    case 0:
        DeleteCriticalSection(&g_cs);
        break;
    }
    return TRUE;
}
// MATCH: jgld.dll 0x100654b0 ??0Display@@QAE@XZ
Display::Display()
{
    m_width = 0;
    m_height = 0;
    m_bpp = 0;
    m_dc = 0;
    m_hinst = 0;
    m_hwnd = 0;
    m_mode = 0;
}
// MATCH: jgld.dll 0x100657e0 ?createWindow@Display@@QAEHPAUListener@@PBDPAUMode@@@Z
int Display::createWindow(Listener* l, const char* title, Mode* mode)
{
    WNDCLASSA wc;
    DWORD style;
    m_hinst = GetModuleHandleA(0);
    g_listener = l;
    m_mode = mode;
    wc.style = 0x2b;
    wc.lpfnWndProc = wndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = m_hinst;
    wc.hIcon = LoadIconA(0, IDI_APPLICATION);
    wc.hCursor = 0;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = s_className;
    if (!RegisterClassA(&wc))
        return 1;
    style = 0x86000000;
    m_hwnd = CreateWindowExA(WS_EX_APPWINDOW, s_className, title, style, 0, 0, GetSystemMetrics(SM_CXSCREEN),
                             GetSystemMetrics(SM_CYSCREEN), 0, 0, m_hinst, 0);
    if (m_hwnd == 0)
        return 1;
    m_width = GetSystemMetrics(SM_CXSCREEN);
    m_height = GetSystemMetrics(SM_CYSCREEN);
    m_bpp = 8;
    m_dc = GetDC(m_hwnd);
    SetBkMode(m_dc, TRANSPARENT);
    mode->set(m_width, m_height, m_bpp, 1);
    return 0;
}
// MATCH: jgld.dll 0x10065a90 ?enumModes@Display@@UAEPAV?$Array@UModeInfo@@@@XZ
Array<ModeInfo>* Display::enumModes()
{
    DEVMODEA dm;
    ModeInfo r;
    DWORD i;
    dm.dmSize = sizeof(dm);
    dm.dmFields = 0;
    dm.dmDriverExtra = 0;
    g_modes.clear();
    g_devmodes.clear();
    i = 0;
    while (EnumDisplaySettingsA(0, i++, &dm)) {
        r.bpp = dm.dmBitsPerPel;
        r.w = dm.dmPelsWidth;
        r.h = dm.dmPelsHeight;
        r.w2 = dm.dmPelsWidth;
        r.freq = dm.dmDisplayFrequency;
        g_modes.add(r);
        g_devmodes.add(dm);
    }
    return &g_modes;
}
// MATCH: jgld.dll 0x10065c30 ?findMode@Display@@QAEHHHH@Z
int Display::findMode(int w, int h, int bpp)
{
    int unused;
    RECT rc;
    Array<ModeInfo>* modes;
    int i;
    int best;
    int bestFreq;
    unused = 0;
    rc.left = 0;
    rc.right = w;
    rc.top = 0;
    rc.bottom = h;
    best = -1;
    bestFreq = -1;
    modes = enumModes();
    for (i = 0; i < modes->count(); i++) {
        if (modes->m_data[i].w == w && modes->m_data[i].h == h && modes->m_data[i].bpp == bpp
                && (int)modes->m_data[i].freq > bestFreq) {
            bestFreq = modes->m_data[i].freq;
            best = i;
        }
    }
    setModeIndex(best);
    return 0;
}
// MATCH: jgld.dll 0x10065da0 ?setMode@Display@@QAEXHHH_N@Z
void Display::setMode(int w, int h, int bpp, bool apply)
{
    if (!apply)
        setModeDirect(w, h, m_bpp);
    else {
    m_width = w;
    m_height = h;
    m_bpp = bpp;
    m_mode->set(m_width, m_height, m_bpp, 1);
    }
}
// MATCH: jgld.dll 0x10065ea0 ?pickMode@Display@@QAEHHHH@Z
int Display::pickMode(int w, int h, int bpp)
{
    Array<ModeInfo>* modes;
    int i;
    int best;
    int bestFreq;
    best = -1;
    bestFreq = -1;
    modes = enumModes();
    for (i = 0; i < modes->count(); i++) {
        if (modes->m_data[i].w == w && modes->m_data[i].h == h && modes->m_data[i].bpp == bpp
                && (int)modes->m_data[i].freq > bestFreq) {
            if ((int)modes->m_data[i].freq > 75 && best > -1) {
                return setModeIndex(best);
            }
            bestFreq = modes->m_data[i].freq;
            best = i;
        }
    }
    if (best >= 0)
        return setModeIndex(best);
    return 1;
}
// MATCH: jgld.dll 0x10066040 ?pickModeFreq@Display@@QAEHHHH_NH@Z
int Display::pickModeFreq(int w, int h, int bpp, bool apply, int freq)
{
    Array<ModeInfo>* modes;
    int i;
    int best;
    int bestFreq;
    best = -1;
    bestFreq = -1;
    if (apply)
        return setModeFull(w, h, bpp, apply);
    {
        modes = enumModes();
        for (i = 0; i < modes->count(); i++) {
            if (modes->m_data[i].w == w && modes->m_data[i].h == h && modes->m_data[i].bpp == bpp) {
                if ((int)modes->m_data[i].freq == freq) {
                    best = i;
                    break;
                }
                if ((int)modes->m_data[i].freq > bestFreq) {
                    bestFreq = modes->m_data[i].freq;
                    best = i;
                }
            }
        }
        if (best >= 0)
            return setModeIndex(best);
    }
    return 1;
}
// MATCH: jgld.dll 0x10066200 ?setDesktopMode@Display@@QAEXH@Z
void Display::setDesktopMode(int bpp)
{
    int w;
    int h;
    w = GetSystemMetrics(SM_CXSCREEN);
    h = GetSystemMetrics(SM_CYSCREEN);
    setModeDirect(w, h, bpp);
}
// MATCH: jgld.dll 0x100662a0 ?setModeIndex@Display@@UAEHH@Z
int Display::setModeIndex(int i)
{
    DEVMODEA dm;
    DWORD fields;
    fields = 0x1c0000;
    EnumDisplaySettingsA(0, ENUM_CURRENT_SETTINGS, &m_4.m_cur);
    if (m_4.m_cur.dmPelsWidth == 640) {
        MessageBoxA(g_display->getHwnd(), s_needRes, s_needResTitle, 0);
        return 2;
    }
    if (i >= g_devmodes.count())
        return 3;
    dm = g_devmodes.m_data[i];
    dm.dmSize = sizeof(dm);
    dm.dmFields = 0x5c0000;
    if (ChangeDisplaySettingsA(&dm, CDS_FULLSCREEN))
        return 2;
    m_width = dm.dmPelsWidth;
    m_height = dm.dmPelsHeight;
    m_bpp = dm.dmBitsPerPel;
    return m_mode->set(m_width, m_height, m_bpp, 1);
}
// MATCH: jgld.dll 0x10066470 ?present@Display@@QAEHPAUtagRECT@@@Z
int Display::present(RECT* r)
{
    HDC dc;
    Surf* s;
    RECT rc;
    s = m_mode->m_surf;
    dc = s->getDC();
    s->selectPalette(g_palHolder->m_pal);
    if (r == 0) {
        BitBlt(m_dc, 0, 0, rectWidth(s->bounds()), rectHeight(s->bounds()), dc, 0, 0, SRCCOPY);
    } else {
        if (intersect(&rc, r, s->bounds()))
            BitBlt(m_dc, rc.left, rc.top, rectWidth(&rc), rectHeight(&rc), dc, rc.left, rc.top, SRCCOPY);
    }
    s->releaseDC(1);
    return 0;
}
// MATCH: jgld.dll 0x10066780 ?getCursor@Display@@QAEXPAJ0@Z
void Display::getCursor(LONG* x, LONG* y)
{
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(m_hwnd, &pt);
    *x = pt.x;
    *y = pt.y;
}
// MATCH: jgld.dll 0x10066890 ?setResolution@Display@@QAEHKK@Z
int Display::setResolution(DWORD w, DWORD h)
{
    DEVMODEA dm;
    EnumDisplaySettingsA(0, ENUM_CURRENT_SETTINGS, &m_4.m_orig);
    memcpy(&dm, &m_4.m_orig, sizeof(dm));
    dm.dmPelsWidth = w;
    dm.dmPelsHeight = h;
    dm.dmFields = 0x1c0000;
    if (ChangeDisplaySettingsA(&dm, CDS_FULLSCREEN))
        return 2;
    return 0;
}
// MATCH: jgld.dll 0x10066970 ?restore@Display@@QAEHXZ
int Display::restore()
{
    if (ChangeDisplaySettingsA(&m_4.m_orig, CDS_FULLSCREEN))
        return 2;
    return 0;
}
