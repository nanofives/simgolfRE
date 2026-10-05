// FLAGS jgld.dll: /Od /ZI /GZ /GX
// jgld.dll application message pump helpers (debug build): each peeks one message class (paint, keyboard, mouse
// move, char, mouse, the private 0x401) and dispatches it, or drains the queue of one class. Names are chosen here.
#include <windows.h>
class App {
public:
    int pumpPaint();
    int pumpKeys();
    int pumpMouseMove();
    int pumpChars();
    int pumpMouse();
    int pumpPrivate();
    void flushKeys();
    void drainPrivate();
    int pumpOldest();
    int pumpClick();
    void flushMouse();
    int pumpAll();
    void idle();
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual void v54(); virtual void v55(); virtual void v56(); virtual void v57(); virtual void v58(); virtual void v59(); virtual void v60(); virtual void v61(); virtual void v62(); virtual void v63(); virtual void v64(); virtual void v65(); virtual void v66(); virtual void v67(); virtual void v68(); virtual void v69(); virtual void v70(); virtual void v71(); virtual void v72(); virtual void v73();
    virtual int busy();          // slot 74 (+0x128)
};
class Listener {
public:
    virtual void l0();
    virtual void paint(RECT* r);             // slot 1 (+0x04)
    virtual void l2();
    virtual void key(unsigned vk, int down); // slot 3 (+0x0c)
    virtual void onChar(char c);             // slot 4 (+0x10)
    virtual void l5(); virtual void l6(); virtual void l7(); virtual void l8(); virtual void l9(); virtual void l10();
    virtual void activate(unsigned state, int minimized);   // slot 11 (+0x2c)
    virtual void l12();
    virtual void onPrivate();                // slot 13 (+0x34)
};
class Display {
public:
    virtual void d0(); virtual void d1(); virtual void d2(); virtual void d3(); virtual void d4(); virtual void d5(); virtual void d6(); virtual void d7(); virtual void d8(); virtual void d9(); virtual void d10(); virtual void d11(); virtual void d12(); virtual void d13(); virtual void d14(); virtual void d15(); virtual void d16(); virtual void d17(); virtual void d18(); virtual void d19(); virtual void d20(); virtual void d21(); virtual void d22(); virtual void d23(); virtual void d24(); virtual void d25(); virtual void d26(); virtual void d27(); virtual void d28(); virtual void d29(); virtual void d30(); virtual void d31(); virtual void d32(); virtual void d33(); virtual void d34(); virtual void d35(); virtual void d36(); virtual void d37(); virtual void d38(); virtual void d39(); virtual void d40(); virtual void d41(); virtual void d42(); virtual void d43(); virtual void d44(); virtual void d45(); virtual void d46(); virtual void d47(); virtual void d48(); virtual void d49(); virtual void d50(); virtual void d51(); virtual void d52(); virtual void d53(); virtual void d54(); virtual void d55(); virtual void d56(); virtual void d57(); virtual void d58(); virtual void d59(); virtual void d60(); virtual void d61(); virtual void d62(); virtual void d63(); virtual void d64(); virtual void d65(); virtual void d66(); virtual void d67(); virtual void d68(); virtual void d69(); virtual void d70(); virtual void d71(); virtual void d72(); virtual void d73(); virtual void d74(); virtual void d75(); virtual void d76(); virtual void d77();
    virtual int pumpOne();                   // slot 78 (+0x138)
};
extern Display* g_display;                   // 0x10128420
extern int (*g_command)(int, int);           // 0x10128728
extern Listener* g_listener;     // 0x1012872c
extern HPALETTE g_hpalette;      // 0x10128738
extern RGBQUAD g_sysColorsLow[10];    // 0x1011dab0
extern RGBQUAD g_sysColorsHigh[10];   // 0x1011dad8
class Random {                   // jgld_random.cpp
public:
    Random();
    void seed(unsigned s);       // 0x1006afc0
    int range(int n);
    unsigned seed_;
};
class PaletteBase {
public:
    virtual ~PaletteBase();      // 0x1006af00
};
struct PaletteOwner { int m_0; class Palette* m_palette; };
class Palette : public PaletteBase {
public:
    virtual ~Palette();
    void animate();
    int setRGB(const unsigned char* rgb, int first, int n);
    void update();
    void apply();                // 0x1006a4a0
    void release();              // 0x1006a380
    int setDefault();
    virtual void v1();
    virtual void clear();        // slot 2 (+0x08)
    int m_4, m_8;
    PALETTEENTRY m_e[256];       // +0x0c
    unsigned short m_555[256];   // +0x40c: m_e as RGB555
    unsigned short m_565[256];   // +0x60c: m_e as RGB565
    PaletteOwner* m_owner;       // +0x80c
    int m_id;                    // +0x810 (random non-zero, changes on every update)
    int m_814;
    LARGE_INTEGER m_qpc;         // +0x818
};
// MATCH: jgld.dll 0x1006b3a0 ?pumpPaint@App@@QAEHXZ
int App::pumpPaint()
{
    MSG msg;
    if (!PeekMessageA(&msg, 0, 0xf, 0xf, PM_REMOVE))
        return 0;
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
    return 1;
}
// MATCH: jgld.dll 0x1006b440 ?pumpKeys@App@@QAEHXZ
int App::pumpKeys()
{
    MSG msg;
    if (!PeekMessageA(&msg, 0, 0x100, 0x108, PM_REMOVE))
        return 0;
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
    return 1;
}
// MATCH: jgld.dll 0x1006b4f0 ?pumpMouseMove@App@@QAEHXZ
int App::pumpMouseMove()
{
    MSG msg;
    if (PeekMessageA(&msg, 0, 0x200, 0x200, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
        return 1;
    }
    return 0;
}
// MATCH: jgld.dll 0x1006b730 ?pumpChars@App@@QAEHXZ
int App::pumpChars()
{
    MSG msg;
    if (!PeekMessageA(&msg, 0, 0x102, 0x102, PM_REMOVE))
        return 0;
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
    return 1;
}
// MATCH: jgld.dll 0x1006b7e0 ?pumpMouse@App@@QAEHXZ
int App::pumpMouse()
{
    MSG msg;
    if (!PeekMessageA(&msg, 0, 0x200, 0x20a, PM_REMOVE))
        return 0;
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
    return 1;
}
// MATCH: jgld.dll 0x1006b970 ?pumpPrivate@App@@QAEHXZ
int App::pumpPrivate()
{
    MSG msg;
    if (!PeekMessageA(&msg, 0, 0x401, 0x401, PM_REMOVE))
        return 0;
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
    return 1;
}
// MATCH: jgld.dll 0x1006b890 ?flushKeys@App@@QAEXXZ
void App::flushKeys()
{
    MSG msg;
    while (PeekMessageA(&msg, 0, 0x100, 0x108, PM_REMOVE)) {
    }
}
// MATCH: jgld.dll 0x1006b900 ?flushMouse@App@@QAEXXZ
void App::flushMouse()
{
    MSG msg;
    while (PeekMessageA(&msg, 0, 0x200, 0x20a, PM_REMOVE)) {
    }
}
// MATCH: jgld.dll 0x1006b080 ?idle@App@@QAEXXZ
void App::idle()
{
    if (busy())
        return;
    WaitMessage();
}
// MATCH: jgld.dll 0x1006b0f0 ?pumpAll@App@@QAEHXZ
int App::pumpAll()
{
    MSG msg;
    if (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
        if (msg.message == 0x402) {
            if (g_listener)
                g_listener->onPrivate();
        } else {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        return 1;
    }
    return 0;
}
// MATCH: jgld.dll 0x1006a860 ?animate@Palette@@QAEXXZ
void Palette::animate()
{
    if (g_hpalette == 0)
        return;
    AnimatePalette(g_hpalette, 10, 0xec, &m_e[10]);
}
// MATCH: jgld.dll 0x1006a9a0 ?setRGB@Palette@@QAEHPBEHH@Z
int Palette::setRGB(const unsigned char* rgb, int first, int n)
{
    for (int i = 0; i < n; i++) {
        m_e[i + first].peRed = rgb[i * 3];
        m_e[i + first].peGreen = rgb[i * 3 + 1];
        m_e[i + first].peBlue = rgb[i * 3 + 2];
    }
    update();
    return 0;
}
// MATCH: jgld.dll 0x10069ac0 ?onPaint@@YAXPAUHWND__@@@Z
void onPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc;
    RECT rc;
    dc = BeginPaint(hwnd, &ps);
    if (dc == 0)
        return;
    rc = ps.rcPaint;
    g_listener->paint(&rc);
    EndPaint(hwnd, &ps);
}
// MATCH: jgld.dll 0x10069c70 ?onKey@@YAXPAUHWND__@@IH@Z
void onKey(HWND hwnd, unsigned vk, int down)
{
    MSG msg;
    g_listener->key(vk, down);
    if (down) {
        if (vk < 0x60 || vk > 0x6f) {
            while (g_display->pumpOne()) {
            }
        } else {
            while (PeekMessageA(&msg, hwnd, WM_CHAR, WM_CHAR, PM_REMOVE)) {
            }
        }
    }
}
// MATCH: jgld.dll 0x10069d50 ?onChar@@YAXPAUHWND__@@D@Z
void onChar(HWND hwnd, char c)
{
    g_listener->onChar(c);
}
// MATCH: jgld.dll 0x1006a0e0 ?onCommand@@YAHPAUHWND__@@HH@Z
int onCommand(HWND hwnd, int id, int code)
{
    if (g_command)
        return g_command(id, code);
    return 0;
}
// MATCH: jgld.dll 0x1006a3d0 ?update@Palette@@QAEXXZ
void Palette::update()
{
    Random rng;
    m_id = 0;
    QueryPerformanceCounter(&m_qpc);
    rng.seed(m_qpc.LowPart);
    while (m_id == 0)
        m_id = rng.range(0xffff) & 0xffff;
    apply();
}
// MATCH: jgld.dll 0x1006a2c0 ??1Palette@@UAE@XZ
Palette::~Palette()
{
    if (m_owner)
        m_owner->m_palette = 0;
    release();
}
// MATCH: jgld.dll 0x1006a4a0 ?apply@Palette@@QAEXXZ
void Palette::apply()
{
    if (m_814 == m_id)
        return;
    m_814 = m_id;
    for (int i = 0; i < 0x100; i++) {
        m_555[i] = (m_e[i].peBlue >> 3) & 0x1f | (m_e[i].peGreen << 2) & 0x3e0 | (m_e[i].peRed << 7) & 0xfc00;
        m_565[i] = (m_e[i].peBlue >> 3) & 0x1f | (m_e[i].peGreen << 3) & 0x7e0 | (m_e[i].peRed << 8) & 0xf800;
    }
}
// MATCH: jgld.dll 0x1006a600 ?setDefault@Palette@@QAEHXZ
int Palette::setDefault()
{
    int i;
    clear();
    for (i = 0; i < 10; i++) {
        m_e[i].peRed = g_sysColorsLow[i].rgbRed;
        m_e[i].peGreen = g_sysColorsLow[i].rgbGreen;
        m_e[i].peBlue = g_sysColorsLow[i].rgbBlue;
        m_e[i].peFlags = 4;
        m_e[i + 0xf6].peRed = g_sysColorsHigh[i].rgbRed;
        m_e[i + 0xf6].peGreen = g_sysColorsHigh[i].rgbGreen;
        m_e[i + 0xf6].peBlue = g_sysColorsHigh[i].rgbBlue;
        m_e[i + 0xf6].peFlags = 4;
    }
    for (i = 10; i < 0xf6; i++) {
        m_e[i].peRed = i;
        m_e[i].peGreen = i;
        m_e[i].peBlue = i;
        m_e[i].peFlags = 5;
    }
    for (i = 0; i < 2; i++) {
        m_e[i + 8].peRed = i + 8;
        m_e[i + 8].peGreen = 0;
        m_e[i + 8].peBlue = 0;
        m_e[i + 8].peFlags = 2;
        m_e[i + 0xf6].peRed = i + 0xf5;
        m_e[i + 0xf6].peGreen = 0;
        m_e[i + 0xf6].peBlue = 0;
        m_e[i + 0xf6].peFlags = 2;
    }
    update();
    return 0;
}
// MATCH: jgld.dll 0x100699c0 ?realizePalette@@YAHPAUHWND__@@@Z
int realizePalette(HWND hwnd)
{
    HDC dc;
    dc = GetDC(hwnd);
    SelectPalette(dc, g_hpalette, FALSE);
    RealizePalette(dc);
    ReleaseDC(hwnd, dc);
    return 1;
}
// MATCH: jgld.dll 0x10069b90 ?onActivate@@YAXPAUHWND__@@I0H@Z
void onActivate(HWND hwnd, unsigned state, HWND other, int minimized)
{
    InvalidateRect(hwnd, 0, FALSE);
    if (state && !minimized)
        realizePalette(hwnd);
    g_listener->activate(state, minimized);
    DefWindowProcA(hwnd, WM_ACTIVATE, MAKEWPARAM(state, minimized), (LPARAM)other);
}
// MATCH: jgld.dll 0x1006ba20 ?drainPrivate@App@@QAEXXZ
void App::drainPrivate()
{
    MSG msg;
    while (PeekMessageA(&msg, 0, 0x401, 0x401, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}
// MATCH: jgld.dll 0x1006b1d0 ?pumpOldest@App@@QAEHXZ
int App::pumpOldest()
{
    MSG msg[3];
    int i;
    int best;
    for (i = 0; i < 3; i++)
        msg[i].time = 0xffffffff;
    if (!PeekMessageA(&msg[0], 0, 0, 0xff, PM_NOREMOVE) && !PeekMessageA(&msg[1], 0, 0x109, 0x1ff, PM_NOREMOVE)
            && !PeekMessageA(&msg[2], 0, 0x20b, 0xffff, PM_NOREMOVE))
        return 0;
    best = 0;
    for (i = 1; i < 3; i++)
        if (msg[i].time < msg[best].time)
            best = i;
    PeekMessageA(&msg[0], msg[best].hwnd, msg[best].message, msg[best].message, PM_REMOVE);
    TranslateMessage(&msg[0]);
    DispatchMessageA(&msg[0]);
    return 1;
}
// MATCH: jgld.dll 0x1006b5a0 ?pumpClick@App@@QAEHXZ
int App::pumpClick()
{
    MSG msg;
    if (!PeekMessageA(&msg, 0, 0x200, 0x20a, PM_REMOVE))
        return 0;
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
    if (msg.message == WM_LBUTTONDOWN && PeekMessageA(&msg, msg.hwnd, WM_LBUTTONUP, WM_LBUTTONUP, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
        return 1;
    }
    if (msg.message == WM_RBUTTONDOWN && PeekMessageA(&msg, msg.hwnd, WM_RBUTTONUP, WM_RBUTTONUP, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
        return 1;
    }
    return 1;
}
struct PalClient { virtual void attach(); };   // slot 0
extern PalClient* g_palClient1;     // 0x1012873c
extern PalClient* g_palClient2;     // 0x10128740
void* jalloc(size_t n);             // 0x10014a40
int jfree(void* p);                 // 0x10014a90
void freePalette();                 // 0x1006ad60
// MATCH: jgld.dll 0x1006aa70 ?createPalette@@YAHPAUPalClient@@0@Z
int createPalette(PalClient* a, PalClient* b)
{
    int i;
    LOGPALETTE* lp;
    freePalette();
    lp = (LOGPALETTE*)jalloc(0x404);
    if (lp == 0)
        return 4;
    lp->palVersion = 0x300;
    lp->palNumEntries = 0x100;
    for (i = 0; i < 10; i++) {
        lp->palPalEntry[i].peRed = g_sysColorsLow[i].rgbRed;
        lp->palPalEntry[i].peGreen = g_sysColorsLow[i].rgbGreen;
        lp->palPalEntry[i].peBlue = g_sysColorsLow[i].rgbBlue;
        lp->palPalEntry[i].peFlags = 0;
        lp->palPalEntry[i + 246].peRed = g_sysColorsHigh[i].rgbRed;
        lp->palPalEntry[i + 246].peGreen = g_sysColorsHigh[i].rgbGreen;
        lp->palPalEntry[i + 246].peBlue = g_sysColorsHigh[i].rgbBlue;
        lp->palPalEntry[i + 246].peFlags = 0;
    }
    for (i = 10; i < 246; i++)
        lp->palPalEntry[i].peFlags = PC_NOCOLLAPSE | PC_RESERVED;
    for (i = 0; i < 2; i++) {
        lp->palPalEntry[i + 8].peRed = (BYTE)(i + 8);
        lp->palPalEntry[i + 8].peGreen = 0;
        lp->palPalEntry[i + 8].peBlue = 0;
        lp->palPalEntry[i + 8].peFlags = PC_EXPLICIT;
        lp->palPalEntry[i + 246].peRed = (BYTE)(i + 245);
        lp->palPalEntry[i + 246].peGreen = 0;
        lp->palPalEntry[i + 246].peBlue = 0;
        lp->palPalEntry[i + 246].peFlags = PC_EXPLICIT;
    }
    g_hpalette = CreatePalette(lp);
    if (g_hpalette == 0) {
        jfree(lp);
        lp = 0;
        return 1;
    }
    jfree(lp);
    lp = 0;
    a->attach();
    b->attach();
    g_palClient1 = a;
    g_palClient2 = b;
    return 0;
}
