// FLAGS jgld.dll: /Od /ZI /GZ /GX
// jgld.dll drawing surface (debug build, C++ EH on): GDI clip rectangle/region, a memory DC with its DIB section,
// a critical section, and an owner back pointer. Class, member and slot names are chosen here; offsets are the
// instructions'. Base9690 derives from Tracked (jgld_list.cpp); its ctor/dtor are in the raw files. The pixel loops
// of fill8/fill16 are inline __asm in the original (register-only loops in a /Od build) and are reproduced as such.
// Most methods dispatch on the bits per pixel (format(), slot 57) and return 0x17 for an unsupported depth.
// line8 (0x1000fb70) exits through `goto done;`: VC6 /Od compiles each forward goto as a jump to its own `jmp`
// stub, and the stubs pile up in reverse order after the function's last statement, which is how they are told
// apart from nested if/else exits (those jump straight to the join).
#include <windows.h>
#include <string.h>
#include <stdlib.h>
class Base9690 {
public:
    Base9690();
    virtual ~Base9690();
    virtual void v1();
    virtual void v2();
    virtual void* lockAt(int x, int y);  // slot 3 (+0x0c)
    virtual void* lockBits();
    virtual void* lockAt8(int x, int y);     // slot 5 (+0x14)
    virtual char* bits();                    // slot 6 (+0x18)
    virtual void* lockAt16(int x, int y);    // slot 7 (+0x1c)
    virtual void v8();
    virtual void unlock(int flag);           // slot 9 (+0x24)
    virtual HDC getDC();                 // slot 10 (+0x28)
    virtual void releaseDC(int flag);    // slot 11 (+0x2c)
    virtual void v12();
    virtual void op13(RECT* r);          // slot 13 (+0x34)
    virtual void v14();
    virtual void v15();
    virtual void v16();
    virtual void fillRectC(RECT* r, int c);    // slot 17 (+0x44)
    virtual void v18();
    virtual void v19();
    virtual void v20();
    virtual void v21();
    virtual void v22();
    virtual void v23();
    virtual void v24();
    virtual void v25();
    virtual void v26();
    virtual void v27();
    virtual void v28();
    virtual void v29();
    virtual void v30();
    virtual void v31();
    virtual void v32();
    virtual void v33();
    virtual void v34();
    virtual void v35();
    virtual void v36();
    virtual void v37();
    virtual void v38();
    virtual void v39();
    virtual void v40();
    virtual void v41();
    virtual void v42();
    virtual void v43();
    virtual void v44();
    virtual int is565();                 // slot 45 (+0xb4): 1 = RGB565, else RGB555
    virtual void v46();
    virtual void v47();
    virtual void v48();
    virtual void v49();
    virtual void getClipRect(RECT* r);       // slot 50 (+0xc8)
    virtual RECT* clipRect();                // slot 51 (+0xcc): returns this+0x44 (add eax,0x44 at 0x1000a9b0)
    virtual void v52();
    virtual RECT* boundsRect();              // slot 53 (+0xd4): returns this+0x54 (add eax,0x54 at 0x1000aa50)
    virtual int width();                 // slot 54 (+0xd8)
    virtual int height();                // slot 55 (+0xdc)
    virtual int pitch();                     // slot 56 (+0xe0)
    virtual int* format();               // slot 57 (+0xe4): first int = bits per pixel
    int m_4;
};
struct Sub92e0 { Sub92e0(); int bpp; char pad[0x10]; };
int makeColor(int r, int g, int b);      // 0x1000a660
extern class Base9690* g_display;       // 0x10128420
extern int g_skip;                      // 0x1012847c (row skip shared by the 8-bit fills)
extern int g_101284b0; extern int g_101284b8; extern int g_101284bc;   // blit8from8 loop state (0x101284b8 dwords per row, 0x101284bc / 0x101284b0 row skips)
void setRect(RECT* r, int x, int y, int w, int h);     // 0x10008360
void setRectLTRB(RECT* r, int l, int t, int rr, int b);  // 0x10010390
int rectWidth(const RECT* r);            // 0x10009120
int rectHeight(const RECT* r);           // 0x10009160
struct Owner { int m_0; class Surface* m_surface; };
class Surface : public Base9690 {
public:
    Surface();
    Surface(Owner* owner);
    virtual ~Surface();
    void release();
    int setClip(const RECT* r);
    void* pixelAddr(int x, int y);
    void setPalette(struct Palette* p);
    int blit(Surface* src, int a, int b, int c, int d, int e, int f, int flags);
    int blit8from8(Surface* src, int a, int b, int c, int d, int e, int f, int flags);    // 0x10013800
    int blit8from16(Surface* src, int a, int b, int c, int d, int e, int f, int flags);   // 0x10012900
    int stretchTo(Surface* dst, const RECT* sr, const RECT* dr);
    void hline(int x1, int x2, int y, int c);
    void line8(int x1, int y1, int x2, int y2, int c);
    void clear8(int c);
    void dashedLine8(int x1, int y1, int x2, int y2, int c1, int c2, int len1, int len2, int phase);
    void dashedVLine8(int x, int y1, int y2, int c1, int c2, int len1, int len2, int phase);   // 0x10011880
    void dashedHLine8(int x1, int x2, int y, int c1, int c2, int len1, int len2, int phase);   // 0x100114e0
    void vline(int x, int y1, int y2, int c);
    int blitT(Surface* src, int a, int b, int c, int d, int e, int f);
    int blitT8from8(Surface* src, int a, int b, int c, int d, int e, int f);     // 0x10013c50
    int blitT8from16(Surface* src, int a, int b, int c, int d, int e, int f);    // 0x10012db0
    int blitT16(Surface* src, int a, int b, int c, int d, int e, int f);         // 0x1000ed70
    int fillRect(const RECT* r, int c1, int c2);
    int fill8(int x, int y, int w, int h, int c1, int c2);       // 0x1000f880
    int fill16(int x, int y, int w, int h, int c1, int c2);      // 0x1000afe0
    int fill2_8(int l, int t, int r, int b, int c);              // 0x1000f9e0
    int fill2(const RECT* r, int c);
    int op5(int a, int b, int c, int d, unsigned e, int f);
    int op5_8(int a, int b, int c, int d, unsigned e);                           // 0x1000fb70
    int op5_16(int a, int b, int c, int d, unsigned e);                          // 0x1000b140
    int op9(int a, int b, int c, int d, unsigned e, unsigned f, int g, int h, int i);
    int op9_8(int a, int b, int c, int d, unsigned e, unsigned f, int g, int h, int i);    // 0x10010870
    int op9_16(int a, int b, int c, int d, unsigned e, unsigned f, int g, int h, int i);   // 0x1000c140
    int op2(int* p, unsigned c);
    int op2_8(int* p, unsigned c);                                                  // 0x10011dd0
    int op2_16(int* p, unsigned c);                                              // 0x1000dcf0
    int blit16(RECT* r, Surface* src, int a, int b);
    int blit16from16(RECT* r, Surface* src, int a, int b);                       // 0x1000f6a0
    int op3a(int* p, unsigned c, int d);
    int op3a_16(int* p, unsigned c, int d);                                      // 0x1000dfc0
    int op2b(int* p, unsigned c);
    int op2b_8(int* p, unsigned c);                                              // 0x10011f80
    int op2b_16(int* p, unsigned c);                                             // 0x1000e580
    int op3b(int* p, unsigned c, int d);
    int op3b_16(int* p, unsigned c, int d);                                      // 0x1000e8b0
    int op1(unsigned* p);
    int op1_8(unsigned* p);                                                      // 0x10012140
    void call3a(int a, int b);
    void call3b(int a, int b);
    void callBounds();
    void getClip(RECT* out);
    void getBounds(RECT* out);
    int m_8;
    CRITICAL_SECTION m_cs;               // +0x0c
    Sub92e0 m_24;
    int m_38, m_3c, m_40;
    RECT m_clip;                         // +0x44
    RECT m_bounds;                       // +0x54
    int m_64, m_68, m_6c, m_70;
    char m_74; char pad75[3];
    int m_78;
    struct Palette* m_palette;           // +0x7c
    Owner* m_owner;                      // +0x80
    int m_84, m_88, m_8c;
    short m_90, m_92;
    int m_94, m_98, m_9c, m_a0, m_a4, m_a8;
    char pad_ac[0x4ac - 0xac];
    HRGN m_clipRgn;                      // +0x4ac
    HGDIOBJ m_oldBitmap;                 // +0x4b0
    HBITMAP m_bitmap;                    // +0x4b4
    HDC m_dc;                            // +0x4b8 (DC handed out by getDC)
    HDC m_memDC;                         // +0x4bc
    int m_4c0, m_4c4, m_4c8, m_4cc, m_4d0;
};
// MATCH: jgld.dll 0x10008590 ?intersect@@YAHPAUtagRECT@@PBU1@1@Z
BOOL intersect(RECT* dst, const RECT* a, const RECT* b)
{
    return IntersectRect(dst, a, b);
}
// MATCH: jgld.dll 0x100085f0 ?equal@@YAHPBUtagRECT@@0@Z
BOOL equal(const RECT* a, const RECT* b)
{
    return EqualRect(a, b);
}
// MATCH: jgld.dll 0x10007660 ??0Surface@@QAE@XZ
Surface::Surface()
{
    m_8 = 0;
    m_owner = 0;
    m_38 = 0;
    m_3c = 0;
    m_40 = 0;
    m_74 = 0;
    m_palette = 0;
    m_clipRgn = 0;
    m_oldBitmap = 0;
    m_bitmap = 0;
    m_dc = 0;
    m_memDC = 0;
    m_4c0 = 0;
    m_4c4 = 0;
    m_4c8 = 0;
    m_84 = 0x28;
    m_88 = 0;
    m_8c = 0;
    m_90 = 1;
    m_92 = 8;
    m_94 = 0;
    m_98 = 0;
    m_9c = 0;
    m_a0 = 0;
    m_a4 = 0x100;
    m_a8 = 0;
    memset(&m_bounds, 0, 0x10);
    InitializeCriticalSection(&m_cs);
    if (g_display)
        m_4d0 = makeColor(0xff, 0, 0xff);
    else
        m_4d0 = 0x80007c1f;
}
// MATCH: jgld.dll 0x10007970 ??0Surface@@QAE@PAUOwner@@@Z
Surface::Surface(Owner* owner)
{
    m_owner = owner;
    m_8 = 0;
    m_38 = 0;
    m_3c = 0;
    m_40 = 0;
    m_74 = 0;
    m_palette = 0;
    m_clipRgn = 0;
    m_oldBitmap = 0;
    m_bitmap = 0;
    m_dc = 0;
    m_memDC = 0;
    m_4c0 = 0;
    m_4c4 = 0;
    m_4c8 = 0;
    m_84 = 0x28;
    m_88 = 0;
    m_8c = 0;
    m_90 = 1;
    m_92 = 8;
    m_94 = 0;
    m_98 = 0;
    m_9c = 0;
    m_a0 = 0;
    m_a4 = 0x100;
    m_a8 = 0;
    memset(&m_bounds, 0, 0x10);
    InitializeCriticalSection(&m_cs);
    m_4d0 = makeColor(0xff, 0, 0xff);
}
// MATCH: jgld.dll 0x10007bf0 ??1Surface@@UAE@XZ
Surface::~Surface()
{
    if (m_owner)
        m_owner->m_surface = 0;
    DeleteCriticalSection(&m_cs);
    release();
}
// MATCH: jgld.dll 0x10007900 ??_GSurface@@UAEPAXI@Z
// MATCH: jgld.dll 0x10007cc0 ?release@Surface@@QAEXXZ
void Surface::release()
{
    if (m_memDC) {
        m_4c4 = 0;
        m_4c8 = 0;
        if (m_oldBitmap) {
            SelectObject(m_memDC, m_oldBitmap);
            m_oldBitmap = 0;
        }
        DeleteDC(m_memDC);
        m_dc = 0;
        m_memDC = 0;
    }
    if (m_bitmap) {
        DeleteObject(m_bitmap);
        m_bitmap = 0;
    }
    if (m_clipRgn) {
        DeleteObject(m_clipRgn);
        m_clipRgn = 0;
    }
}
// MATCH: jgld.dll 0x100083c0 ?setClip@Surface@@QAEHPBUtagRECT@@@Z
int Surface::setClip(const RECT* r)
{
    RECT rc;
    HDC dc;
    if (r == 0)
        return 3;
    rc = *r;
    if (!intersect(&m_clip, &m_bounds, &rc))
        return 1;
    dc = getDC();
    if (dc) {
        if (m_clipRgn) {
            DeleteObject(m_clipRgn);
            m_clipRgn = 0;
        }
        if (equal(&m_clip, &m_bounds))
            SelectClipRgn(dc, 0);
        else {
            m_clipRgn = CreateRectRgnIndirect(&m_clip);
            if (m_clipRgn == 0)
                return 1;
            SelectClipRgn(dc, m_clipRgn);
        }
    }
    releaseDC(1);
    return 0;
}
// MATCH: jgld.dll 0x10008830 ?pixelAddr@Surface@@QAEPAXHH@Z
void* Surface::pixelAddr(int x, int y)
{
    char* bits;
    if (x >= width() || y >= height())
        return 0;
    bits = (char*)lockBits();
    if (bits == 0)
        return 0;
    switch (m_24.bpp) {
    case 8:
        return bits + x + y * m_40;
    case 16:
        return (short*)bits + x + y * m_40;
    case 24:
        return bits + x * 3 + y * m_40 * 3;
    case 32:
        return (int*)bits + x + y * m_40;
    }
    return 0;
}
struct Palette {
    int id();                            // 0x1000aef0
    void getColors(RGBQUAD* out);        // 0x1006ae50
};
// MATCH: jgld.dll 0x100089d0 ?setPalette@Surface@@QAEXPAUPalette@@@Z
void Surface::setPalette(Palette* p)
{
    Palette* pal = p;
    RGBQUAD colors[256];
    if (pal == 0)
        return;
    m_palette = pal;
    if (m_78 != pal->id()) {
        m_78 = pal->id();
        pal->getColors(colors);
        if (getDC()) {
            SetDIBColorTable(m_dc, 0, 0x100, colors);
            releaseDC(1);
        }
    }
}
// MATCH: jgld.dll 0x10008e50 ?blit@Surface@@QAEHPAV1@HHHHHHH@Z
int Surface::blit(Surface* src, int a, int b, int c, int d, int e, int f, int flags)
{
    switch (*format()) {
    case 8:
        switch (*src->format()) {
        case 8:
            return blit8from8(src, a, b, c, d, e, f, flags);
        case 0x10:
            return blit8from16(src, a, b, c, d, e, f, flags);
        }
        return 0x17;
    }
    return 0x17;
}
// MATCH: jgld.dll 0x10008f70 ?stretchTo@Surface@@QAEHPAV1@PBUtagRECT@@1@Z
int Surface::stretchTo(Surface* dst, const RECT* sr, const RECT* dr)
{
    HDC ddc;
    HDC sdc;
    Surface* d = dst;
    if (d == 0 || sr == 0 || dr == 0)
        return 0x10;
    ddc = d->getDC();
    if (ddc == 0)
        return 7;
    sdc = getDC();
    if (sdc == 0) {
        d->releaseDC(1);
        return 7;
    }
    StretchBlt(ddc, dr->left, dr->top, rectWidth(dr), rectHeight(dr), sdc, sr->left, sr->top, rectWidth(sr), rectHeight(sr), 0xcc0020);
    d->releaseDC(1);
    releaseDC(1);
    return 0;
}
// MATCH: jgld.dll 0x100091a0 ?blitT@Surface@@QAEHPAV1@HHHHHH@Z
int Surface::blitT(Surface* src, int a, int b, int c, int d, int e, int f)
{
    switch (m_24.bpp) {
    case 8:
        switch (*src->format()) {
        case 8:
            return blitT8from8(src, a, b, c, d, e, f);
        case 0x10:
            return blitT8from16(src, a, b, c, d, e, f);
        }
        return 0x17;
    case 0x10:
        return blitT16(src, a, b, c, d, e, f);
    }
    return 0x17;
}
// MATCH: jgld.dll 0x10009320 ?fillRect@Surface@@QAEHPBUtagRECT@@HH@Z
int Surface::fillRect(const RECT* r, int c1, int c2)
{
    if (r == 0)
        return 0x10;
    switch (*format()) {
    case 8:
        return fill8(r->left, r->top, rectWidth(r), rectHeight(r), c1, c2);
    case 0x10:
        return fill16(r->left, r->top, rectWidth(r), rectHeight(r), c1, c2);
    }
    return 0x17;
}
// MATCH: jgld.dll 0x10009440 ?fill2@Surface@@QAEHPBUtagRECT@@H@Z
int Surface::fill2(const RECT* r, int c)
{
    switch (*format()) {
    case 8:
        return fill2_8(r->left, r->top, r->right, r->bottom, c);
    }
    return 0;
}
// MATCH: jgld.dll 0x100094e0 ?op5@Surface@@QAEHHHHHIH@Z
int Surface::op5(int a, int b, int c, int d, unsigned e, int f)
{
    switch (*format()) {
    case 8:
        op5_8(a, b, c, d, e);
        break;
    case 0x10:
        op5_16(a, b, c, d, e);
        break;
    }
    return 0;
}
// MATCH: jgld.dll 0x100095a0 ?op9@Surface@@QAEHHHHHIIHHH@Z
int Surface::op9(int a, int b, int c, int d, unsigned e, unsigned f, int g, int h, int i)
{
    switch (*format()) {
    case 8:
        op9_8(a, b, c, d, e, f, g, h, i);
        break;
    case 0x10:
        op9_16(a, b, c, d, e, f, g, h, i);
        break;
    }
    return 0;
}
// MATCH: jgld.dll 0x10009770 ?op2@Surface@@QAEHPAHI@Z
int Surface::op2(int* p, unsigned c)
{
    switch (*format()) {
    case 8:
        op2_8(p, c);
        break;
    case 0x10:
        op2_16(p, c);
        break;
    }
    return 0;
}
// MATCH: jgld.dll 0x100098f0 ?blit16@Surface@@QAEHPAUtagRECT@@PAV1@HH@Z
int Surface::blit16(RECT* r, Surface* src, int a, int b)
{
    switch (*format()) {
    case 0x10:
        switch (*src->format()) {
        case 0x10:
            return blit16from16(r, src, a, b);
        }
        return 0x17;
    }
    return 0x17;
}
// MATCH: jgld.dll 0x10009ac0 ?op3a@Surface@@QAEHPAHIH@Z
int Surface::op3a(int* p, unsigned c, int d)
{
    switch (*format()) {
    case 0x10:
        return op3a_16(p, c, d);
    }
    return 0x17;
}
// MATCH: jgld.dll 0x10009c50 ?op2b@Surface@@QAEHPAHI@Z
int Surface::op2b(int* p, unsigned c)
{
    switch (*format()) {
    case 8:
        return op2b_8(p, c);
    case 0x10:
        return op2b_16(p, c);
    }
    return 0x17;
}
// MATCH: jgld.dll 0x10009e40 ?op3b@Surface@@QAEHPAHIH@Z
int Surface::op3b(int* p, unsigned c, int d)
{
    switch (*format()) {
    case 0x10:
        return op3b_16(p, c, d);
    }
    return 0x17;
}
// MATCH: jgld.dll 0x1000a550 ?op1@Surface@@QAEHPAI@Z
int Surface::op1(unsigned* p)
{
    switch (*format()) {
    case 8:
        return op1_8(p);
    }
    return 0x17;
}
// MATCH: jgld.dll 0x1000a660 ?makeColor@@YAHHHH@Z
int makeColor(int r, int g, int b)
{
    if (g_display->is565() == 1)
        return (b & 0xff) >> 3 | (unsigned short)((g & 0xff) >> 2) << 5 | ((r & 0xff) >> 3) << 11 | 0x80000000;
    return (b & 0xff) >> 3 | (unsigned short)((g & 0xff) >> 3) << 5 | ((r & 0xff) >> 3) << 10 | 0x80000000;
}
// MATCH: jgld.dll 0x1000a810 ?call3a@Surface@@QAEXHH@Z   // 0x1000a870 compiles identically
void Surface::call3a(int a, int b)
{
    lockAt(a, b);
}
// MATCH: jgld.dll 0x1000a870 ?call3b@Surface@@QAEXHH@Z   // same body as 0x1000a810: name not determined
void Surface::call3b(int a, int b)
{
    lockAt(a, b);
}
// MATCH: jgld.dll 0x1000a8d0 ?callBounds@Surface@@QAEXXZ
void Surface::callBounds()
{
    op13(&m_bounds);
}
// MATCH: jgld.dll 0x1000a930 ?getClip@Surface@@QAEXPAUtagRECT@@@Z
void Surface::getClip(RECT* out)
{
    *out = m_clip;
}
// MATCH: jgld.dll 0x1000a9d0 ?getBounds@Surface@@QAEXPAUtagRECT@@@Z
void Surface::getBounds(RECT* out)
{
    *out = m_bounds;
}
// MATCH: jgld.dll 0x1000f880 ?fill8@Surface@@QAEHHHHHHH@Z
int Surface::fill8(int x, int y, int w, int h, int c1, int c2)
{
    RECT r;
    char* bits;
    int skip;
    setRect(&r, x, y, w, h);
    if (!intersect(&r, clipRect(), &r))
        return 0;
    bits = (char*)lockAt8(r.left, r.top);
    if (bits == 0)
        return 7;
    w = rectWidth(&r);
    h = rectHeight(&r);
    skip = m_40 - w;
    __asm {
        push edi
        push esi
        mov ah, byte ptr c1
        mov al, byte ptr c2
        mov edi, bits
        mov ebx, w
        mov ecx, h
        mov edx, skip
    row8:
        mov esi, ebx
    px8:
        cmp [edi], ah
        jne next8
        mov [edi], al
    next8:
        inc edi
        dec esi
        jnz px8
        add edi, edx
        dec ecx
        jnz row8
        pop esi
        pop edi
    }
    unlock(1);
    return 0;
}
// MATCH: jgld.dll 0x1000afe0 ?fill16@Surface@@QAEHHHHHHH@Z
int Surface::fill16(int x, int y, int w, int h, int c1, int c2)
{
    RECT r;
    short* bits;
    int skip;
    setRect(&r, x, y, w, h);
    if (!intersect(&r, clipRect(), &r))
        return 0;
    bits = (short*)lockAt16(r.left, r.top);
    if (bits == 0)
        return 7;
    w = rectWidth(&r);
    h = rectHeight(&r);
    skip = (m_40 - w) * 2;
    __asm {
        push edi
        push esi
        mov ax, word ptr c1
        mov bx, word ptr c2
        mov edi, bits
        mov ecx, h
        mov edx, skip
    row16:
        mov esi, w
    px16:
        cmp [edi], ax
        jne next16
        mov [edi], bx
    next16:
        add edi, 2
        dec esi
        jnz px16
        add edi, edx
        dec ecx
        jnz row16
        pop esi
        pop edi
    }
    unlock(1);
    return 0;
}
// MATCH: jgld.dll 0x1000f9e0 ?fill2_8@Surface@@QAEHHHHHH@Z
int Surface::fill2_8(int l, int t, int r, int b, int lut)
{
    RECT rc;
    unsigned char* bits;
    int w;
    int h;
    int skip;
    if (lut == 0)
        return 0x10;
    setRectLTRB(&rc, l, t, r, b);
    if (!intersect(&rc, &rc, clipRect()))
        return 0;
    w = rectWidth(&rc);
    h = rectHeight(&rc);
    skip = pitch() - w;
    bits = (unsigned char*)lockAt(rc.left, rc.top);
    if (bits == 0)
        return 7;
    __asm {
        push esi
        push edi
        mov edi, bits
        mov esi, lut
        mov ebx, skip
        mov edx, h
        mov eax, w
        push ebp
        mov ebp, eax
        xor eax, eax
    rowL:
        mov ecx, ebp
    pxL:
        mov al, [edi]
        mov al, [esi + eax]
        mov [edi], al
        inc edi
        dec ecx
        jnz pxL
        add edi, ebx
        dec edx
        jnz rowL
        pop ebp
        pop edi
        pop esi
    }
    unlock(1);
    return 0;
}
// MATCH: jgld.dll 0x1000f6a0 ?blit16from16@Surface@@QAEHPAUtagRECT@@PAV1@HH@Z
int Surface::blit16from16(RECT* r, Surface* src, int alpha, int table)
{
    unsigned short* d;
    unsigned short* s;
    int skip;
    int w;
    short h;
    int shade;
    int tbl;
    shade = (15 - (int)(alpha / 100.0f * 15.0f)) * 0x8000;
    tbl = table;
    if (!IntersectRect(r, r, boundsRect()))
        return 3;
    d = (unsigned short*)lockAt16(r->left, r->top);
    s = (unsigned short*)src->lockAt16(r->left, r->top);
    w = r->right - r->left;
    h = (short)(r->bottom - r->top);
    skip = (pitch() - w) * 2;
    __asm {
        push esi
        push edi
        mov esi, dword ptr [ebp - 0xc]
        mov edi, dword ptr [ebp - 8]
        mov edx, dword ptr [ebp - 0x14]
        shl edx, 0x10
        mov dx, word ptr [ebp - 0x18]
        mov ebx, dword ptr [ebp - 0x1c]
        push ebp
        mov ebp, dword ptr [ebp - 0x20]
    Lf7bf:
        mov ecx, edx
        shr ecx, 0x10
    Lf7c4:
        mov eax, ebx
        cmp word ptr [edi], 0x7c1f
        jne Lf7da
        or ax, word ptr [esi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
        jmp Lf7e5
    Lf7da:
        or ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    Lf7e5:
        add esi, 2
        add edi, 2
        dec cx
        jne Lf7c4
        pop ebp
        add esi, dword ptr [ebp - 0x10]
        add edi, dword ptr [ebp - 0x10]
        push ebp
        mov ebp, dword ptr [ebp - 0x20]
        dec dx
        jne Lf7bf
        pop ebp
        pop edi
        pop esi
    }
    return 0;
}
// MATCH: jgld.dll 0x100103f0 ?hline@Surface@@QAEXHHHH@Z
void Surface::hline(int x1, int x2, int y, int c)
{
    void* p;
    if (y < clipRect()->top || y >= clipRect()->bottom)
        return;
    if (x1 == x2)
        return;
    if (x1 > x2) {
        x1 ^= x2;
        x2 ^= x1;
        x1 ^= x2;
    }
    if (x1 >= clipRect()->right || x2 < clipRect()->left)
        return;
    if (x1 < clipRect()->left)
        x1 = clipRect()->left;
    if (x2 >= clipRect()->right)
        x2 = clipRect()->right - 1;
    p = lockAt8(x1, y);
    if (p == 0)
        return;
    memset(p, c, x2 - x1 + 1);
    unlock(1);
}
// MATCH: jgld.dll 0x10010620 ?vline@Surface@@QAEXHHHH@Z
void Surface::vline(int x, int y1, int y2, int c)
{
    char* p;
    int step;
    if (x < clipRect()->left || x >= clipRect()->right)
        return;
    if (y1 == y2)
        return;
    if (y1 > y2) {
        y1 ^= y2;
        y2 ^= y1;
        y1 ^= y2;
    }
    if (y1 >= clipRect()->bottom || y2 < clipRect()->top)
        return;
    if (y1 < clipRect()->top)
        y1 = clipRect()->top;
    if (y2 >= clipRect()->bottom)
        y2 = clipRect()->bottom - 1;
    p = (char*)lockAt8(x, y1);
    if (p == 0)
        return;
    step = pitch();
    __asm {
        push edi
        mov ecx, dword ptr [ebp + 0x10]
        sub ecx, dword ptr [ebp + 0xc]
        inc ecx
        mov ah, byte ptr [ebp + 0x14]
        mov ebx, dword ptr [ebp - 0xc]
        mov edi, dword ptr [ebp - 8]
    L07bf:
        mov byte ptr [edi], ah
        add edi, ebx
        loop L07bf
        pop edi
    }
    unlock(1);
}
// MATCH: jgld.dll 0x1000fb70 ?line8@Surface@@QAEXHHHHH@Z
void Surface::line8(int x1, int y1, int x2, int y2, int c)
{
    RECT rc;
    int d;
    int dx;
    int dy;
    int incE;
    int incNE;
    int incNE2;
    char* p;
    if (x1 == x2) {
        vline(x1, y1, y2, c);
        return;
    }
    if (y1 == y2) {
        hline(x1, x2, y1, c);
        return;
    }
    if (x2 < x1) {
    __asm {
        mov eax, dword ptr [ebp + 8]
        xchg dword ptr [ebp + 0x10], eax
        mov dword ptr [ebp + 8], eax
        mov eax, dword ptr [ebp + 0xc]
        xchg dword ptr [ebp + 0x14], eax
        mov dword ptr [ebp + 0xc], eax
    }
    }
    if (lockBits() == 0)
        return;
    getClipRect(&rc);
    dx = x2 - x1;
    dy = y2 - y1;
    if (y1 < y2) {
        if (x1 > rc.right || y1 > rc.bottom || x2 < rc.left || y2 < rc.top)
            goto done;
        {
            if (dx >= dy) {
                d = dy * 2 - dx;
                incE = dy * 2;
                incNE = (dy - dx) * 2;
                while (x1 < rc.left || y1 < rc.top) {
                    if (d <= 0)
                        d += incE;
                    else {
                        d += incNE;
                        y1++;
                    }
                    x1++;
                }
                if (x1 >= rc.right || y1 >= rc.bottom)
                    goto done;
                {
                    p = bits() + x1 + y1 * pitch();
                    *p = (char)c;
                    if (x2 >= rc.right)
                        x2 = rc.right - 1;
                    while (x1 < x2) {
                        if (d <= 0)
                            d += incE;
                        else {
                            y1++;
                            if (y1 >= rc.bottom)
                                goto done;
                            d += incNE;
                            p += pitch();
                        }
                        p++;
                        x1++;
                        *p = (char)c;
                    }
                    goto done;
                }
            } else {
                d = dx * 2 - dy;
                incE = dx * 2;
                incNE = (dx - dy) * 2;
                while (x1 < rc.left || y1 < rc.top) {
                    if (d <= 0)
                        d += incE;
                    else {
                        d += incNE;
                        x1++;
                    }
                    y1++;
                }
                if (x1 >= rc.right || y1 >= rc.bottom)
                    goto done;
                {
                    p = bits() + x1 + y1 * pitch();
                    *p = (char)c;
                    if (y2 >= rc.bottom)
                        y2 = rc.bottom - 1;
                    while (y1 < y2) {
                        if (d <= 0)
                            d += incE;
                        else {
                            x1++;
                            if (x1 >= rc.right)
                                goto done;
                            d += incNE;
                            p++;
                        }
                        y1++;
                        p += pitch();
                        *p = (char)c;
                    }
                    goto done;
                }
            }
        }
    } else {
        if (x1 > rc.right || y1 < rc.top || x2 < rc.left || y2 > rc.bottom)
            goto done;
        {
            dy = abs(dy);
            if (dx >= dy) {
                d = dy * 2 - dx;
                incE = dy * 2;
                incNE2 = (dy - dx) * 2;
                while (x1 < rc.left || y1 >= rc.bottom) {
                    if (d <= 0)
                        d += incE;
                    else {
                        d += incNE2;
                        y1--;
                    }
                    x1++;
                }
                if (x1 >= rc.right || y1 < rc.top)
                    goto done;
                {
                    p = bits() + x1 + y1 * pitch();
                    *p = (char)c;
                    if (x2 >= rc.right)
                        x2 = rc.right - 1;
                    while (x1 < x2) {
                        if (d <= 0)
                            d += incE;
                        else {
                            y1--;
                            if (y1 < rc.top)
                                goto done;
                            d += incNE2;
                            p -= pitch();
                        }
                        x1++;
                        p++;
                        *p = (char)c;
                    }
                    goto done;
                }
            } else {
                d = dx * 2 - dy;
                incE = dx * 2;
                incNE2 = (dx - dy) * 2;
                while (x1 < rc.left || y1 >= rc.bottom) {
                    if (d <= 0)
                        d += incE;
                    else {
                        d += incNE2;
                        x1++;
                    }
                    y1--;
                }
                if (x1 >= rc.right || y1 < rc.top)
                    goto done;
                {
                    p = bits() + x1 + y1 * pitch();
                    *p = (char)c;
                    if (y2 < rc.top)
                        y2 = rc.top;
                    while (y1 > y2) {
                        if (d <= 0)
                            d += incE;
                        else {
                            x1++;
                            if (x1 >= rc.right)
                                goto done;
                            d += incNE2;
                            p++;
                        }
                        y1--;
                        p -= pitch();
                        *p = (char)c;
                    }
                    goto done;
                }
            }
        }
    }
done:
    unlock(1);
}
#define PLOT                                    if (phase < 0) {                                if (c1 != -1)                                   *p = (char)c1;                          phase++;                                    if (phase == 0)                                 phase = len2;                       } else {                                        if (c2 != -1)                                   *p = (char)c2;                          phase--;                                    if (phase == 0)                                 phase = len1;                       }
// MATCH: jgld.dll 0x10010870 ?dashedLine8@Surface@@QAEXHHHHHHHHH@Z
void Surface::dashedLine8(int x1, int y1, int x2, int y2, int c1, int c2, int len1, int len2, int phase)
{
    RECT rc;
    int d;
    int dx;
    int dy;
    int incE;
    int incNE;
    int incNE2;
    char* p;
    if (x1 == x2) {
        dashedVLine8(x1, y1, y2, c1, c2, len1, len2, phase);
        return;
    }
    if (y1 == y2) {
        dashedHLine8(x1, x2, y1, c1, c2, len1, len2, phase);
        return;
    }
    if (x2 < x1) {
    __asm {
        mov eax, dword ptr [ebp + 8]
        xchg dword ptr [ebp + 0x10], eax
        mov dword ptr [ebp + 8], eax
        mov eax, dword ptr [ebp + 0xc]
        xchg dword ptr [ebp + 0x14], eax
        mov dword ptr [ebp + 0xc], eax
    }
        len1 ^= len2;
        len2 ^= len1;
        len1 ^= len2;
        c1 ^= c2;
        c2 ^= c1;
        c1 ^= c2;
        phase = len1 + len2 - phase;
    }
    if (lockBits() == 0)
        return;
    getClipRect(&rc);
    phase %= len1 + len2;
    if (phase >= len1) {
        phase -= len1;
        phase = len2 - phase;
    } else {
        phase = len1 - phase;
        phase = -phase;
    }
    len1 = -len1;
    dx = x2 - x1;
    dy = y2 - y1;
    if (y1 < y2) {
        if (x1 > rc.right || y1 > rc.bottom || x2 < rc.left || y2 < rc.top)
            goto done;
        if (dx >= dy) {
            d = dy * 2 - dx;
            incE = dy * 2;
            incNE = (dy - dx) * 2;
            while (x1 < rc.left || y1 < rc.top) {
                if (d <= 0) {
                    d += incE;
                    x1++;
                } else {
                    d += incNE;
                    x1++;
                    y1++;
                }
            }
            if (x1 >= rc.right || y1 >= rc.bottom)
                goto done;
            p = bits() + x1 + y1 * pitch();
            PLOT
            if (x2 >= rc.right)
                x2 = rc.right - 1;
            while (x1 < x2) {
                if (d <= 0) {
                    d += incE;
                    x1++;
                    p++;
                } else {
                    y1++;
                    if (y1 >= rc.bottom)
                        goto done;
                    d += incNE;
                    x1++;
                    p = p + pitch() + 1;
                }
                PLOT
            }
            goto done;
        } else {
            d = dx * 2 - dy;
            incE = dx * 2;
            incNE = (dx - dy) * 2;
            while (x1 < rc.left || y1 < rc.top) {
                if (d <= 0) {
                    d += incE;
                    y1++;
                } else {
                    d += incNE;
                    y1++;
                    x1++;
                }
            }
            if (x1 >= rc.right || y1 >= rc.bottom)
                goto done;
            p = bits() + x1 + y1 * pitch();
            PLOT
            if (y2 >= rc.bottom)
                y2 = rc.bottom - 1;
            while (y1 < y2) {
                if (d <= 0) {
                    d += incE;
                    y1++;
                    p += pitch();
                } else {
                    x1++;
                    if (x1 >= rc.right)
                        goto done;
                    d += incNE;
                    y1++;
                    p = p + pitch() + 1;
                }
                PLOT
            }
            goto done;
        }
    } else {
        if (x1 > rc.right || y1 < rc.top || x2 < rc.left || y2 > rc.bottom)
            goto done;
        dy = abs(dy);
        if (dx >= dy) {
            d = dy * 2 - dx;
            incE = dy * 2;
            incNE2 = (dy - dx) * 2;
            while (x1 < rc.left || y1 >= rc.bottom) {
                if (d <= 0) {
                    d += incE;
                    x1++;
                } else {
                    d += incNE2;
                    x1++;
                    y1--;
                }
            }
            if (x1 >= rc.right || y1 < rc.top)
                goto done;
            p = bits() + x1 + y1 * pitch();
            PLOT
            if (x2 >= rc.right)
                x2 = rc.right - 1;
            while (x1 < x2) {
                if (d <= 0) {
                    d += incE;
                    x1++;
                    p++;
                } else {
                    y1--;
                    if (y1 < rc.top)
                        goto done;
                    d += incNE2;
                    x1++;
                    p = p - pitch() + 1;
                }
                PLOT
            }
            goto done;
        } else {
            d = dx * 2 - dy;
            incE = dx * 2;
            incNE2 = (dx - dy) * 2;
            while (x1 < rc.left || y1 >= rc.bottom) {
                if (d <= 0) {
                    d += incE;
                    y1--;
                } else {
                    d += incNE2;
                    y1--;
                    x1++;
                }
            }
            if (x1 >= rc.right || y1 < rc.top)
                goto done;
            p = bits() + x1 + y1 * pitch();
            PLOT
            if (y2 < rc.top)
                y2 = rc.top;
            while (y1 > y2) {
                if (d <= 0) {
                    d += incE;
                    y1--;
                    p -= pitch();
                } else {
                    x1++;
                    if (x1 >= rc.right)
                        goto done;
                    d += incNE2;
                    y1--;
                    p = p - pitch() + 1;
                }
                PLOT
            }
            goto done;
        }
    }
done:
    unlock(1);
}
#undef PLOT
// MATCH: jgld.dll 0x100114e0 ?dashedHLine8@Surface@@QAEXHHHHHHHH@Z
void Surface::dashedHLine8(int x1, int x2, int y, int c1, int c2, int len1, int len2, int phase)
{
    char* p;
    if (y < clipRect()->top || y >= clipRect()->bottom)
        return;
    if (x1 == x2)
        return;
    if (c1 == -1 && c2 == -1)
        return;
    if (x1 > x2) {
        x1 ^= x2;
        x2 ^= x1;
        x1 ^= x2;
        len1 ^= len2;
        len2 ^= len1;
        len1 ^= len2;
        c1 ^= c2;
        c2 ^= c1;
        c1 ^= c2;
        phase = len1 + len2 - phase;
    }
    if (x1 >= clipRect()->right || x2 < clipRect()->left)
        return;
    if (x1 < clipRect()->left)
        x1 = clipRect()->left;
    if (x2 >= clipRect()->right)
        x2 = clipRect()->right - 1;
    p = (char*)lockAt8(x1, y);
    if (p == 0)
        return;
    phase %= len1 + len2;
    if (phase >= len1) {
        phase -= len1;
        phase = len2 - phase;
    } else {
        phase = len1 - phase;
        phase = -phase;
    }
    len1 = -len1;
    if (c1 == -1) {
    __asm {
        push edi
        push esi
        mov ecx, dword ptr [ebp + 0xc]
        sub ecx, dword ptr [ebp + 8]
        inc ecx
        mov ah, byte ptr [ebp + 0x14]
        mov al, byte ptr [ebp + 0x18]
        mov ebx, dword ptr [ebp + 0x1c]
        mov esi, dword ptr [ebp + 0x20]
        mov edi, dword ptr [ebp - 8]
        mov edx, dword ptr [ebp + 0x24]
    L170d:
        and edx, edx
        js L171a
        mov byte ptr [edi], al
        dec edx
        jne L171f
        mov edx, ebx
        jmp L171f
    L171a:
        inc edx
        jne L171f
        mov edx, esi
    L171f:
        inc edi
        dec ecx
        jne L170d
        pop esi
        pop edi
    }
    } else if (c2 == -1) {
    __asm {
        push esi
        push edi
        mov ecx, dword ptr [ebp + 0xc]
        sub ecx, dword ptr [ebp + 8]
        inc ecx
        mov ah, byte ptr [ebp + 0x14]
        mov al, byte ptr [ebp + 0x18]
        mov ebx, dword ptr [ebp + 0x1c]
        mov esi, dword ptr [ebp + 0x20]
        mov edi, dword ptr [ebp - 8]
        mov edx, dword ptr [ebp + 0x24]
    L1748:
        and edx, edx
        js L1753
        dec edx
        jne L175a
        mov edx, ebx
        jmp L175a
    L1753:
        mov byte ptr [edi], ah
        inc edx
        jne L175a
        mov edx, esi
    L175a:
        inc edi
        dec ecx
        jne L1748
        pop edi
        pop esi
    }
    } else {
    __asm {
        push esi
        push edi
        mov ecx, dword ptr [ebp + 0xc]
        sub ecx, dword ptr [ebp + 8]
        inc ecx
        mov ah, byte ptr [ebp + 0x14]
        mov al, byte ptr [ebp + 0x18]
        mov ebx, dword ptr [ebp + 0x1c]
        mov esi, dword ptr [ebp + 0x20]
        mov edi, dword ptr [ebp - 8]
        mov edx, dword ptr [ebp + 0x24]
    L177d:
        and edx, edx
        js L178a
        mov byte ptr [edi], al
        dec edx
        jne L1791
        mov edx, ebx
        jmp L1791
    L178a:
        mov byte ptr [edi], ah
        inc edx
        jne L1791
        mov edx, esi
    L1791:
        inc edi
        dec ecx
        jne L177d
        pop edi
        pop esi
    }
    }
    unlock(1);
}
// MATCH: jgld.dll 0x10011880 ?dashedVLine8@Surface@@QAEXHHHHHHHH@Z
void Surface::dashedVLine8(int x, int y1, int y2, int c1, int c2, int len1, int len2, int phase)
{
    char* p;
    int step;
    if (x < clipRect()->left || x >= clipRect()->right)
        return;
    if (y1 == y2)
        return;
    if (c1 == -1 && c2 == -1)
        return;
    if (y1 > y2) {
        y1 ^= y2;
        y2 ^= y1;
        y1 ^= y2;
        len1 ^= len2;
        len2 ^= len1;
        len1 ^= len2;
        c1 ^= c2;
        c2 ^= c1;
        c1 ^= c2;
        phase = len1 + len2 - phase;
    }
    if (y1 >= clipRect()->bottom || y2 < clipRect()->top)
        return;
    if (y1 < clipRect()->top)
        y1 = clipRect()->top;
    if (y2 >= clipRect()->bottom)
        y2 = clipRect()->bottom - 1;
    p = (char*)lockAt8(x, y1);
    if (p == 0)
        return;
    step = pitch();
    phase %= len1 + len2;
    if (phase >= len1) {
        phase -= len1;
        phase = len2 - phase;
    } else {
        phase = len1 - phase;
        phase = -phase;
    }
    len1 = -len1;
    if (c1 == -1) {
    __asm {
        push esi
        push edi
        mov ecx, dword ptr [ebp + 0x10]
        sub ecx, dword ptr [ebp + 0xc]
        inc ecx
        mov ah, byte ptr [ebp + 0x14]
        mov al, byte ptr [ebp + 0x18]
        mov ebx, dword ptr [ebp - 0xc]
        mov edi, dword ptr [ebp - 8]
        mov edx, dword ptr [ebp + 0x24]
    L1ac6:
        and edx, edx
        js L1ad4
        mov byte ptr [edi], al
        dec edx
        jne L1ada
        mov edx, dword ptr [ebp + 0x1c]
        jmp L1ada
    L1ad4:
        inc edx
        jne L1ada
        mov edx, dword ptr [ebp + 0x20]
    L1ada:
        add edi, ebx
        dec ecx
        jne L1ac6
        pop edi
        pop esi
    }
    } else if (c2 == -1) {
    __asm {
        push edi
        push esi
        mov ecx, dword ptr [ebp + 0x10]
        sub ecx, dword ptr [ebp + 0xc]
        inc ecx
        mov ah, byte ptr [ebp + 0x14]
        mov al, byte ptr [ebp + 0x18]
        mov ebx, dword ptr [ebp - 0xc]
        mov edi, dword ptr [ebp - 8]
        mov edx, dword ptr [ebp + 0x24]
    L1b01:
        and edx, edx
        js L1b0d
        dec edx
        jne L1b15
        mov edx, dword ptr [ebp + 0x1c]
        jmp L1b15
    L1b0d:
        mov byte ptr [edi], ah
        inc edx
        jne L1b15
        mov edx, dword ptr [ebp + 0x20]
    L1b15:
        add edi, ebx
        dec ecx
        jne L1b01
        pop esi
        pop edi
    }
    } else {
    __asm {
        push edi
        push esi
        mov ecx, dword ptr [ebp + 0x10]
        sub ecx, dword ptr [ebp + 0xc]
        inc ecx
        mov ah, byte ptr [ebp + 0x14]
        mov al, byte ptr [ebp + 0x18]
        mov ebx, dword ptr [ebp - 0xc]
        mov edi, dword ptr [ebp - 8]
        mov edx, dword ptr [ebp + 0x24]
    L1b36:
        and edx, edx
        js L1b44
        mov byte ptr [edi], al
        dec edx
        jne L1b4c
        mov edx, dword ptr [ebp + 0x1c]
        jmp L1b4c
    L1b44:
        mov byte ptr [edi], ah
        inc edx
        jne L1b4c
        mov edx, dword ptr [ebp + 0x20]
    L1b4c:
        add edi, ebx
        dec ecx
        jne L1b36
        pop esi
        pop edi
    }
    }
    unlock(1);
}
// MATCH: jgld.dll 0x10011c40 ?clear8@Surface@@QAEXH@Z
void Surface::clear8(int c)
{
    int h;
    unsigned int w;
    void* p;
    if (!equal(clipRect(), boundsRect()))
        fillRectC(clipRect(), c);
    else {
        p = bits();
        h = boundsRect()->bottom;
        w = boundsRect()->right;
    __asm {
        pushf
        push edi
        push esi
        cld
        mov edi, dword ptr [ebp - 0x10]
        mov eax, dword ptr [ebp - 0xc]
        mov ebx, eax
        xor ecx, ecx
        and ebx, 3
        setne cl
        shr eax, 2
        add eax, ecx
        mov ecx, dword ptr [ebp - 8]
        mul ecx
        mov ecx, eax
        mov dl, byte ptr [ebp + 8]
        mov al, dl
        mov ah, dl
        shl eax, 0x10
        mov al, dl
        mov ah, dl
        rep stosd
        pop esi
        pop edi
        popf
    }
        unlock(1);
    }
}
// MATCH: jgld.dll 0x10011dd0 ?op2_8@Surface@@QAEHPAHI@Z
int Surface::op2_8(int* rp, unsigned c)
{
    int h;
    int w;
    RECT rc;
    char* p;
    p = 0;
    if (rp == 0) {
        clear8(c);
        return 0;
    }
    rc = *(RECT*)rp;
    if (!intersect(&rc, &rc, clipRect()))
        return 0;
    p = (char*)lockAt8(rc.left, rc.top);
    if (p == 0)
        return 0;
    h = rc.bottom - rc.top;
    w = rc.right - rc.left;
    g_skip = pitch() - w;
    __asm {
        pushf
        push edi
        push esi
        cld
        mov edi, dword ptr [ebp - 0x20]
        mov edx, dword ptr [ebp - 0xc]
        mov ebx, edx
        and ebx, 3
        shr edx, 2
        mov cl, byte ptr [ebp + 0xc]
        mov al, cl
        mov ah, cl
        shl eax, 0x10
        mov al, cl
        mov ah, cl
        mov ecx, dword ptr [ebp - 8]
        push ebp
        mov ebp, ecx
    L1ede:
        mov ecx, edx
        rep stosd
        mov ecx, ebx
        rep stosb
        add edi, g_skip
        dec ebp
        jne L1ede
        pop ebp
        pop esi
        pop edi
        popf
    }
    unlock(1);
    return 0;
}
// MATCH: jgld.dll 0x10011f80 ?op2b_8@Surface@@QAEHPAHI@Z
int Surface::op2b_8(int* rp, unsigned c)
{
    char* p;
    int skip;
    int h;
    int w;
    RECT rc;
    p = 0;
    if (rp == 0)
        rp = (int*)boundsRect();
    rc = *(RECT*)rp;
    if (intersect(&rc, &rc, clipRect())) {
        p = (char*)lockAt8(rc.left, rc.top);
        if (p) {
            h = rc.bottom - rc.top;
            w = rc.right - rc.left;
            skip = pitch() - w;
    __asm {
        pushf
        push edi
        mov ah, byte ptr [ebp + 0xc]
        mov edi, dword ptr [ebp - 8]
        mov edx, dword ptr [ebp - 0x10]
        mov ecx, dword ptr [ebp - 0x14]
        mov ebx, ecx
        and ebx, 1
    L207a:
        test edx, 1
        je L2083
        inc edi
    L2083:
        mov ecx, dword ptr [ebp - 0x14]
        shr ecx, 1
        cmp ecx, 0
        je L2094
    L208d:
        mov byte ptr [edi], ah
        add edi, 2
        loop L208d
    L2094:
        test edx, 1
        je L20a7
        test ebx, 1
        jne L20b2
        dec edi
        jmp L20b2
    L20a7:
        test ebx, 1
        je L20b2
        mov byte ptr [edi], ah
        inc edi
    L20b2:
        add edi, dword ptr [ebp - 0xc]
        dec edx
        jne L207a
        pop edi
        popf
    }
            unlock(1);
        }
    }
    return 0;
}
// MATCH: jgld.dll 0x10013800 ?blit8from8@Surface@@QAEHPAV1@HHHHHHH@Z
int Surface::blit8from8(Surface* src, int x, int y, int sx, int sy, int w, int h, int key)
{
    char* a;
    char* b;
    int apitch;
    int bpitch;
    RECT rc;
    int rows;
    int cols;
    if (src == 0)
        return 3;
    if (x < 0) {
        w += x;
        sx -= x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        sy -= y;
        y = 0;
    }
    if (x > width())
        return 0;
    if (y > height())
        return 0;
    if (x + w > width())
        w = width() - x;
    if (y + h > height())
        h = height() - y;
    setRect(&rc, sx, sy, w, h);
    if (!intersect(&rc, &rc, src->clipRect()))
        return 0;
    x += rc.left - sx;
    y += rc.top - sy;
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    a = (char*)src->lockAt8(rc.left, rc.top);
    if (a == 0)
        return 3;
    b = (char*)lockAt8(x, y);
    if (b == 0)
        return 3;
    apitch = src->pitch();
    bpitch = pitch();
    rows = rc.bottom - rc.top;
    cols = rc.right - rc.left;
    g_101284b8 = cols >> 2;
    g_101284bc = bpitch - cols;
    g_101284b0 = apitch - cols;
    __asm {
        push esi
        push edi
        mov esi, dword ptr [ebp - 0xc]
        mov edi, dword ptr [ebp - 8]
        mov ecx, dword ptr g_101284b8
        mov ebx, dword ptr [ebp - 0x28]
        mov al, byte ptr [ebp + 0x24]
        mov edx, dword ptr [ebp - 0x2c]
        and edx, 3
        je L3a96
        dec edx
        je L3a9e
        dec edx
        je L3aad
        dec edx
        je L3abc
    L3a96:
        lea edx, L3afa
        jmp L3afa
    L3a9e:
        lea edx, L3b0c
        inc ecx
        mov dword ptr g_101284b8, ecx
        jmp L3b0c
    L3aad:
        lea edx, L3b06
        inc ecx
        mov dword ptr g_101284b8, ecx
        jmp L3b06
    L3abc:
        lea edx, L3b00
        inc ecx
        mov dword ptr g_101284b8, ecx
        jmp L3b00
    L3acb:
        jmp edx
    L3acd:
        cmp byte ptr [edi], al
        jne L3afe
    L3ad1:
        mov ah, byte ptr [esi]
        mov byte ptr [edi], ah
        inc esi
        inc edi
        cmp byte ptr [edi], al
        jne L3b04
    L3adb:
        mov ah, byte ptr [esi]
        mov byte ptr [edi], ah
        inc esi
        inc edi
        cmp byte ptr [edi], al
        jne L3b0a
    L3ae5:
        mov ah, byte ptr [esi]
        mov byte ptr [edi], ah
        inc esi
        inc edi
        cmp byte ptr [edi], al
        jne L3b10
    L3aef:
        mov ah, byte ptr [esi]
        mov byte ptr [edi], ah
        inc esi
        inc edi
        dec ecx
        jne L3acd
        jmp L3b15
    L3afa:
        cmp byte ptr [edi], al
        je L3ad1
    L3afe:
        inc esi
        inc edi
    L3b00:
        cmp byte ptr [edi], al
        je L3adb
    L3b04:
        inc esi
        inc edi
    L3b06:
        cmp byte ptr [edi], al
        je L3ae5
    L3b0a:
        inc esi
        inc edi
    L3b0c:
        cmp byte ptr [edi], al
        je L3aef
    L3b10:
        inc esi
        inc edi
        dec ecx
        jne L3afa
    L3b15:
        mov ecx, dword ptr g_101284b8
        add esi, dword ptr g_101284bc
        add edi, dword ptr g_101284b0
        dec ebx
        jne L3acb
        pop edi
        pop esi
    }
    unlock(1);
    src->unlock(1);
    return 0;
}
