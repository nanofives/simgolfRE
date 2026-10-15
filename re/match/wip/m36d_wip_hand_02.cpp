// FLAGS golf_clean.exe: /O2
// m36d near misses (round 36): copy of re/match/m36d_hand_02.cpp whose MATCH lines are only the functions below
// 100%. Kept in wip/ so the 100% test and the evidence run never see them; what was tried is in
// log/c3/m36d_notes.txt.
// m36d: golf_clean.exe functions written by hand, batch 2 (release /O2). Names are placeholders; offsets, constants
// and callees are the original's. Callee conventions from their `ret N` (log/c3/m36d_notes.txt).
#include <windows.h>
#include <string.h>
#pragma vtordisp(off)
#define V4(a, b, c, d) virtual void v##a(); virtual void v##b(); virtual void v##c(); virtual void v##d();
#define V8(x) V4(x##0, x##1, x##2, x##3) V4(x##4, x##5, x##6, x##7)

// The Surface image (golf_hand_r0.cpp R0I4796a0): slot 9 unlock, 52 rect, 54 width, 55 height, 57 format.
struct H2Image {
    V8(a) virtual void b0(); virtual void unlock(int);
    V4(b2, b3, b4, b5) V8(c) V8(d) V8(e) V8(f) V4(g0, g1, g2, g3)
    virtual void g4(); virtual void g5();
    virtual void rect(RECT*);
    virtual void g7();
    virtual int width(); virtual int height(); virtual int pitch(); virtual int* format();
};
struct H2Surface {
    int m_0;
    H2Image* m_4;
    int width() { return m_4 ? m_4->width() : 0; }
    int height() { return m_4 ? m_4->height() : 0; }
    int* format() { return m_4 ? m_4->format() : 0; }
    void unlock() { if (m_4) m_4->unlock(1); }
    char* pixel(int x, int y);      // 0x004796a0
    char pixelBlue(int x, int y, char def);
    unsigned readPixel(int x, int y);
    int writePixel(int x, int y, int v);
};
// MATCH: golf_clean.exe 0x00479950 ?pixelBlue@H2Surface@@QAEDHHD@Z
char H2Surface::pixelBlue(int x, int y, char def)
{
    unsigned char* p = (unsigned char*)pixel(x, y);
    if (!p)
        return def;
    char c;
    switch (*format()) {
    default:
        c = def;
        break;
    case 24:
    case 32:
        c = *p;
        break;
    case 16:
        c = *p << 3;
        break;
    }
    unlock();
    return c;
}
// MATCH: golf_clean.exe 0x00478df0 ?readPixel@H2Surface@@QAEIHH@Z
unsigned H2Surface::readPixel(int x, int y)
{
    if (!m_4 || x < 0 || y < 0 || x >= width() || y >= height())
        return 0;
    char* p = pixel(x, y);
    unlock();
    if (!p)
        return 0;
    switch (*format()) {
    case 8:
        return *(unsigned char*)p;
    case 16:
        return *(unsigned short*)p;
    }
    return *(unsigned*)p;
}
// MATCH: golf_clean.exe 0x00478ea0 ?writePixel@H2Surface@@QAEHHHH@Z
int H2Surface::writePixel(int x, int y, int v)
{
    if (!m_4)
        return 7;
    if (x < 0 || y < 0 || x >= width() || y >= height())
        return 0;
    char* p = pixel(x, y);
    if (p) {
        switch (*format()) {
        case 8:
            *p = (char)v;
            break;
        case 16:
            *(short*)p = (short)v;
            break;
        default:
            return 0x17;
        }
    }
    unlock();
    return 0;
}

// ---- 0x00484a40 Snd::configure: flag word to the state bits at +0x58, device at +0x40
struct H2Dev { V8(a) V8(b) V8(c) virtual void d0(); virtual void d1(); virtual void d2(); virtual void setFlags(int); };   // slot 27 (0x6c)
struct H2Snd {
    V8(a) V8(b) virtual void c0(); virtual void c1(); virtual void play(int);     // slot 18 (0x48)
    char pad[0x40 - 4];
    H2Dev* m_40;
    char pad44[0x58 - 0x44];
    struct { unsigned b0 : 1, b1 : 1, b2 : 1, b3 : 1, b4 : 1, b5 : 1, b6 : 1; } m_58;   // state bits
    void prepare(int);      // 0x00484b80
    void openDevice(int);   // 0x00484c20
    void configure(int, int);
};
// MATCHED-IN re/match/m36d_hand_02.cpp: golf_clean.exe 0x00484a40 ?configure@H2Snd@@QAEXHH@Z
void H2Snd::configure(int a, int flags)
{
    int dev = flags & 4;
    if (dev && (flags & 0x80))
        return;
    prepare(a);
    memset(&m_58, 0, sizeof(m_58));
    if (dev) {
        m_58.b1 = 1;
        if (!m_40)
            openDevice(a);
        if (m_40)
            m_40->setFlags(flags);
    }
    if (flags & 1)
        m_58.b0 = 1;
    if ((flags & 0x10) && !dev)
        m_58.b2 = 1;
    if (flags & 2)
        play(1);
    if (flags & 0x40)
        m_58.b3 = 1;
    if (flags & 0x80)
        m_58.b4 = 1;
    if (flags & 0x100)
        m_58.b5 = 1;
    if (flags & 0x400)
        m_58.b6 = 1;
}

// ---- 0x00478530 drawCenteredTextLines: draws wrapped lines and returns the next y
extern int g_839aa8;
struct H2Text {
    char pad[0x1c];
    int m_1c;                                           // line counter
    const char* wrap(const char*, int*, int);           // 0x00476ef0
    void drawLine(const char*, int, int, int, int);     // 0x00477da0
    int lineHeight();                                   // 0x00477580
    int drawCenteredTextLines(const char*, int, int, int);
};
// MATCH: golf_clean.exe 0x00478530 ?drawCenteredTextLines@H2Text@@QAEHPBDHHH@Z
int H2Text::drawCenteredTextLines(const char* text, int x, int y, int w)
{
    if (!text)
        return y;
    g_839aa8 = 0;
    m_1c = 0;
    const char* next;
    do {
        int ww = w;
        if (!text) {
            next = 0;
        } else {
            g_839aa8 = 0;
            next = wrap(text, &ww, strlen(text));
            drawLine(text, x, y, w, next ? next - text : strlen(text));
        }
        m_1c++;
        y += lineHeight();
        text = next;
    } while (next);
    return y;
}

// ---- sound library: 0x00483fc0 soundVersionCheck, 0x00483e90 initSound
extern HMODULE g_83afc0;
extern FARPROC g_83af6c[11];
extern int g_83afc4;
extern char s_4e43bc[], s_4babd0[], s_4bac50[];
void freeSoundLib();                    // 0x00484060
int callSoundHook94();                  // 0x004840c0
void showMessageBox(const char*, const char*);  // 0x00485590
void callSoundHook74(int, int);         // 0x00484090
struct H2Res { int loadB(int, int); /* 0x00487a70 */ int load(int, int); /* 0x00487c00 */ };
extern H2Res g_83ad58, g_83ad80, g_83af98;
// MATCHED-IN re/match/m36d_hand_02.cpp: golf_clean.exe 0x00483fc0 ?soundVersionCheck@@YAHXZ
int soundVersionCheck()
{
    if (!g_83afc0) {
        g_83afc0 = LoadLibraryA(s_4e43bc);
        if (!g_83afc0)
            return 1;
        for (int i = 0; i < 11; i++) {
            g_83af6c[i] = GetProcAddress(g_83afc0, MAKEINTRESOURCEA(i + 1));
            if (!g_83af6c[i]) {
                freeSoundLib();
                g_83afc4 = 0;
                return 1;
            }
        }
        if ((callSoundHook94() & 0xff00) != 0xc00)
            showMessageBox(s_4babd0, s_4bac50);
    }
    return 0;
}
// MATCHED-IN re/match/m36d_hand_02.cpp: golf_clean.exe 0x00483e90 ?initSound@@YAHHH@Z
int initSound(int wnd, int flags)
{
    int mode = 0;
    int r = soundVersionCheck();
    if (r)
        return r;
    callSoundHook74(0, 0);
    if (flags & 2) {
        if (flags & 0x100)
            mode = 2;
        g_83ad58.loadB(wnd, mode);
    }
    r = g_83ad80.load(wnd, flags);
    if (r) {
        freeSoundLib();
        return r;
    }
    if (flags & 8)
        g_83af98.load(wnd, flags);
    g_83afc4 = 1;
    return 0;
}

// ---- 0x0047d570 Window::layoutScrollbars: three child bars at +0x150 / +0x154 / +0x158
struct H2Win;
struct H2WinSurf { int m_0; H2Image* m_4; int width() { return m_4 ? m_4->width() : 0; } };
struct H2WinFirst { char pad[0x274]; };
struct H2Win : H2WinFirst, H2WinSurf {
    char pad27c[0x150 - 0x27c + 0x274];
};
struct H2Bar : H2WinFirst, H2WinSurf { void moveTo(int, int); /* 0x0047b420 */ };
struct H2Layout {
    char pad[0x150];
    H2Bar* m_150;
    H2Bar* m_154;
    H2Bar* m_158;
    char pad15c[0x180 - 0x15c];
    int m_180, m_184;
    char pad188[0x194 - 0x188];
    int m_194;
    char pad198[0x1bc - 0x198];
    int m_1bc;
    char pad1c0[0x1c4 - 0x1c0];
    int m_1c4;
    void layoutScrollbars();
};
// MATCH: golf_clean.exe 0x0047d570 ?layoutScrollbars@H2Layout@@QAEXXZ
void H2Layout::layoutScrollbars()
{
    int y = m_194;
    int x = m_1c4 - 2 * y - m_1bc - m_184 - m_180;
    x -= 2;
    if (m_158) {
        m_158->moveTo(x, y);
        x -= m_158->width() + 3;
    }
    if (m_154)
        m_154->moveTo(x, y);
    if (m_150)
        m_150->moveTo(x, y);
}

// ---- 0x00491270 TextView::drawPanelWithImage
struct H2Img { int m_0; H2Image* m_4; void rect(RECT* r) { if (m_4) m_4->rect(r); } };
struct H2Drawer { int draw(H2Img*, int, int, int); /* 0x00473e60 */ };
struct H2TextView {
    char pad[0x11c];
    H2Img* m_11c;
    char pad120[0x194 - 0x120];
    int m_194;
    char pad198[0x13cc - 0x198];
    H2Drawer* m_13cc;
    void drawPanel(int);    // 0x004810d0
    void drawPanelWithImage(int);
};
// MATCH: golf_clean.exe 0x00491270 ?drawPanelWithImage@H2TextView@@QAEXH@Z
void H2TextView::drawPanelWithImage(int part)
{
    drawPanel(part);
    if (m_11c && (part == -1 || part == 0) && m_13cc) {
        RECT r;
        m_11c->rect(&r);
        r.right -= m_194;
        r.top += m_194;
        r.left += m_194;
        r.bottom -= m_194;
        m_13cc->draw(m_11c, r.left, r.top, 0);
    }
}

// ---- 0x004340a0 hitBuildingTool: the two round buttons, then nine slots in two rows
int approxDistance(int, int);   // 0x00467170
// MATCHED-IN re/match/m36d_hand_02.cpp: golf_clean.exe 0x004340a0 ?hitBuildingTool@@YAHHH@Z
int hitBuildingTool(int x, int y)
{
    if (approxDistance(x - 0xec, y - 0x20e) < 0x10)
        return -2;
    if (approxDistance(x - 0x115, y - 0x237) < 0xc)
        return -3;
    int i = 0;
    int dx = x - 0x17f;
    int dy = 2 * y - 0x426;
    for (; i < 9; i++) {
        if (approxDistance(dx, dy) < 0x28)
            return i;
        if (i != 4) {
            dx -= 0x50;
        } else {
            dy -= 0x50;
            dx += 0x118;
        }
    }
    return -1;
}
