// FLAGS golf_clean.exe: /O2 /GX
// golf_clean.exe batch r2 written by hand (release /O2; /GX for EH frames). Names are chosen here.
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#define R2V4(a) virtual void a##_0(); virtual void a##_1(); virtual void a##_2(); virtual void a##_3();
#define R2V8(a) R2V4(a##x) R2V4(a##y)
#define R2V16(a) R2V8(a##x) R2V8(a##y)
#define R2V32(a) R2V16(a##x) R2V16(a##y)
#include <mmsystem.h>
#include <stdio.h>
#pragma warning(disable: 4700)
#pragma warning(disable: 4715)
struct R2P0_491 { virtual ~R2P0_491(); char pad[0x270]; };
struct R2P1_491 { virtual void p1(); };
struct R2V1_491 : R2P0_491, R2P1_491 { char pad[0x574 - 0x278]; void f480610(); };
struct R2V2_491 { virtual void v2(); void detach(); };
extern int g_4e4a10, g_4e4a14, g_4e4a18;
struct R2C491680 : virtual R2V1_491, virtual R2V2_491 {
    void cleanup();
    virtual void p1();
    virtual void v2();
    int m_4, m_8, m_c, m_10, m_14, m_18, m_1c;
};
// MATCH: golf_clean.exe 0x0049d690 ?cleanup@R2C491680@@QAEXXZ
void R2C491680::cleanup()
{
    m_10 = 0;
    m_14 = 0;
    m_8 = g_4e4a14;
    m_c = g_4e4a18;
    m_4 = g_4e4a10;
    detach();
    f480610();
}
struct R2M44b { R2M44b(); ~R2M44b(); char pad[0x2c]; };
struct R2C44b450 {
    R2M44b m_0;
    R2M44b m_2c;
    R2M44b m_58[0x12];
    R2C44b450();
};
// MATCH: golf_clean.exe 0x0044b450 ??0R2C44b450@@QAE@XZ
R2C44b450::R2C44b450()
{
}
struct R2P0_480 { virtual ~R2P0_480(); char pad[0x270]; };
struct R2P1_480 { virtual void p1(); ~R2P1_480(); char pad[0x2f8]; };
struct R2C4805a0 : R2P0_480, R2P1_480 {
    ~R2C4805a0();
    virtual void p1();
    int m_570;
};
// MATCH: golf_clean.exe 0x004805a0 ??1R2C4805a0@@UAE@XZ
R2C4805a0::~R2C4805a0()
{
    m_570 = 0;
}
struct R2M8_404 { virtual void f(); ~R2M8_404() { clear(); } void clear(); char pad[0x28]; };
struct R2B_404 {
    virtual void g();
    ~R2B_404() { cleanup(); }
    void cleanup();
    int m_4;
    R2M8_404 m_8;
};
struct R2C404280 : R2B_404 {
    virtual void g();
    ~R2C404280();
    void f481ba0();
};
// MATCH: golf_clean.exe 0x00404280 ??1R2C404280@@QAE@XZ
R2C404280::~R2C404280()
{
    f481ba0();
}
struct R2N489 { int m_0; int m_key; int m_8; R2N489* m_next; };
struct R2C4898d0 {
    char pad[0xc8];
    R2N489* m_head;
    R2N489* m_cur;
    int m_count;
    int m_index;
    char pad2[0xf0 - 0xd8];
    int m_sel;
    void select(int key);
};
// MATCH: golf_clean.exe 0x004898d0 ?select@R2C4898d0@@QAEXH@Z
void R2C4898d0::select(int key)
{

    int i = 0;
    if (m_head) {
        m_index = 0;
        m_cur = m_head;
        for (;;) {
            if (i >= m_count) break;
            if (m_cur->m_key == key) break;
            m_index++;
            m_cur = m_cur->m_next;
            i++;
        }
    }
    m_sel = m_index;

}
struct R2Font477 { int m_0; int m_4; };
struct R2C477da0 {
    char pad[0x5c];
    R2Font477* m_5c;
    int textWidth(const char* s, int n);
    int draw(const char* s, int x, int y, int n);
    int drawCentered(const char* s, int x, int y, int w, int n);
};
// MATCH: golf_clean.exe 0x00477da0 ?drawCentered@R2C477da0@@QAEHPBDHHHH@Z
int R2C477da0::drawCentered(const char* s, int x, int y, int w, int n)
{
    if (!s)
        return x;
    if (!m_5c || !m_5c->m_4)
        return 3;
    if (((int)strlen(s) < n ? (int)strlen(s) : n) >= 0) {
        n = ((int)strlen(s) < n ? (int)strlen(s) : n);
        if (!n)
            return x;
        x += (w - textWidth(s, n)) / 2;
        return draw(s, x, y, n);
    }
    return x;
}
struct R2V0_490 { int m_0; };
struct R2V_490 { char pad[0xd0]; int m_count; char pad2[0xf0 - 0xd4]; int m_f0; int getSel(); };   // 0x489950
struct R2Mode1460 { int active(); int m_0; };   // 0x4a1370
struct R2P0_490 {
    virtual void v0(); virtual void v1(); virtual void post(); R2V32(a) R2V32(b) virtual void c0(); virtual void redraw();
    char pad[0x590 - 4];
    R2P0_490* m_590;
    char pad1[0x5c0 - 0x594];
    int m_5c0;
    char pad2[0x1460 - 0x5c4];
    R2Mode1460 m_1460;
};
struct R2P1464 : virtual R2V0_490, virtual R2V_490 { char pad[0x70]; };
struct R2P14d8 : virtual R2V0_490, virtual R2V_490 {
    int flags() { return m_f0; }
    char pad[0x1654 - 0x14dc];
};
struct R2C490960 : R2P0_490, R2P1464, R2P14d8 {
    int m_1654;
    char pad3[0x1f68 - 0x1658];
    int m_1f68;
    char pad4[0x1fc0 - 0x1f6c];
    int m_1fc0;
    void select(int v);
};
// MATCH: golf_clean.exe 0x00490960 ?select@R2C490960@@QAEXH@Z
void R2C490960::select(int v)
{
    m_5c0 = 0;
    if (m_1460.active()) {
        if (m_1654 == 1)
            m_5c0 = flags();
        else
            m_5c0 = R2P1464::getSel();
    }
    switch (v) {
    case -2:
        if (m_1f68 & 0x40)
            m_5c0 = -1;
        else {
            if (m_1654 != -1)
                return;
            m_5c0 = 0;
        }
        m_1fc0 = 0;
        break;
    case -1:
        m_1fc0 = 0;
        break;
    default:
        m_1fc0 = v;
    }

    if (!(m_1f68 & 0x100)) {
        if (m_590)
            m_590->redraw();
        else
            redraw();
    }
    if (m_1f68 & 0x4000)
        post();
}
struct R2Stop492 { void stop(); int m_0; };     // 0x486ec0
struct R2Win492 {
    void toScreen(long*, long*);          // 0x47b170
    void show(char*, RECT*);            // 0x480220
};
struct R2Ent492 { int m_0; RECT m_rc; int m_14, m_18; char* m_text; };
extern int g_83aac8, g_83ad44, g_83aad8, g_4e42c4;
struct R2C492bd0 {
    char pad[0xc];
    R2Win492* m_c;
    int m_10, m_14, m_18, m_1c;
    R2Stop492 m_20;
    char pad2[0x50 - 0x24];
    R2Ent492* m_50;
    void showTip(int i);
};
// MATCH: golf_clean.exe 0x00492bd0 ?showTip@R2C492bd0@@QAEXH@Z
void R2C492bd0::showTip(int i)
{
    m_20.stop();
    if (m_c && m_50[i].m_text) {
        R2Ent492* e = &m_50[i];
        RECT r = e->m_rc;
        POINT p = { 0, 0 };
        m_c->toScreen(&p.x, &p.y);
        r.left += p.x;
        r.right += p.x;
        r.top += p.y;
        r.bottom += p.y;
        if (!m_10)
            g_83aac8 = g_83ad44;
        else
            g_83aac8 = m_10;
        g_83aad8 = m_18;
        g_4e42c4 = m_1c;
        m_c->show(e->m_text, &r);
    }

}
extern int g_83ff10;
struct R2Ctl47c { R2V32(a) R2V32(b) R2V16(c) R2V8(d) R2V4(e) virtual int extra(); };
struct R2C47ca10 {
    char pad[0x9c];
    unsigned int m_flags;
    char pad2[0x15c - 0xa0];
    R2Ctl47c* m_15c;
    char pad3[0x180 - 0x160];
    int m_180, m_184, m_188;
    void adjustSize(int* w, int* h);
};
// MATCH: golf_clean.exe 0x0047ca10 ?adjustSize@R2C47ca10@@QAEXPAH0@Z
void R2C47ca10::adjustSize(int* w, int* h)
{

    if (!w || !h)
        return;
    if (m_flags & 4)
        *h += g_83ff10;
    if (m_flags & 8)
        *w += g_83ff10;
    if (m_flags & 0x400) {
        *w += m_184 * 2;
        *h += m_184 * 2;
        if (m_188 != -1)
            *h = m_188 - m_184 + *h;
    } else if (m_flags & 0x11) {
        *w += m_184 * 2;
        *h += m_184 * 2;
        if (m_188 != -1)
            *h = m_188 - m_184 + *h;
    }
    if (m_flags & 0x10)
        *h += m_180 - m_184;
    if (m_15c && !(m_flags & 0x20000000))
        *h += m_15c->extra();

}
struct R2Img49e { R2Img49e(); ~R2Img49e(); int load(const char*, void*, int, int, int); char pad[0x2b8]; };
struct R2Cur49e { void set(R2Img49e*, int, int, int, int, int, int); };
extern R2Cur49e g_840830, g_840890, g_840860;
extern const char s_004e49fc[];
// MATCH: golf_clean.exe 0x0049e9d0 ?loadCursors49e9d0@@YAHXZ
int loadCursors49e9d0()
{
    R2Img49e img;
    int err = img.load(s_004e49fc, 0, 0, 0x100, 2);
    if (err)
        return err;
    g_840830.set(&img, 1, 0x23, 0x20, 0x20, 1, 1);
    g_840890.set(&img, 1, 0x23, 0x20, 0x20, 1, 1);
    g_840860.set(&img, 0x22, 0x23, 0x20, 0x20, 1, 1);
    return 0;
}
struct R2Rec568 { short m_a, m_b, m_idx, m_pad; };
extern R2Rec568 g_5689e8[100];
struct R2Gol4d6 { char m_name[0x230]; };
extern R2Gol4d6 g_4d6098[];
extern char g_51a068[];
extern const char s_004c4e38[], s_004c4e34[];
void __cdecl f407700(int, int, int);
// MATCH: golf_clean.exe 0x00407b60 ?lookup407b60@@YAHHHH@Z
int lookup407b60(int a, int b, int text)
{
    for (int i = 0; i < 100; i++) {
        if (g_5689e8[i].m_a == a && g_5689e8[i].m_b == b) {
            if (!text)
                return 1;
            strcat(g_51a068, g_4d6098[g_5689e8[i].m_idx].m_name);
            strcat(g_51a068, s_004c4e34);
            f407700(a, b, -1);
            return 1;
        }
    }
    if (text)
        strcat(g_51a068, s_004c4e38);
    return 0;
}
int __cdecl f433c60(int, int);
void __cdecl f40d320(int, int, int, int);
int __cdecl f45c0c0(int);
struct R2Obj519 { void f480c80(int); };
extern R2Obj519 g_519a60;
extern int g_55e928, g_567afc, g_4c2e08, g_542f20, g_4c2848;
extern unsigned int g_59e7b8;
extern char g_51a068[];
extern const char s_004c7ec4[], s_004c7e7c[];
// MATCH: golf_clean.exe 0x00433d30 ?click433d30@@YAHHH@Z
int click433d30(int a, int b)
{
    int r = f433c60(a, b);

    if (r == -2) {
        g_55e928 = 0;
        g_567afc = 1;
        g_59e7b8 &= ~2;
        return -1;
    } else if (r == -3) {
        g_4c2e08 = -22;
        strcpy(g_51a068, s_004c7ec4);
        strcat(g_51a068, s_004c7e7c);
        f40d320(200, 0x154, 0x80007fff, -2);
        g_55e928 = 0;
        g_567afc = 1;
        g_59e7b8 &= ~2;
        g_519a60.f480c80(0);
        f45c0c0(0);
    } else if (r >= 0) {
        g_542f20 = r;
        g_4c2848 = -1;
        g_59e7b8 |= 2;
    }

}
struct R2Disp83a { R2V8(a) R2V4(a2) virtual void update(int); R2V16(d) R2V8(e) R2V4(f) virtual void f4(); virtual void f5(); virtual void f6(); virtual void c4(); virtual void redraw(); };
extern R2Disp83a* g_83ad50;
extern int g_83b9c0;
struct R2Ctl293c { R2V32(a) R2V16(b) R2V4(c) virtual void c4(); virtual RECT* rect(); };
struct R2Txt23b0 {
    void f476310(int, int, int, int);
    void f4762d0(int, int, int, int);
    void f477c30(const char*, int, int, int);
    void f477e60(const char*, int, int, int, int, int);
    char pad[4];
};
struct R2M213c { void f480ce0(); char pad[4]; };
struct R2C493a60 {
    int m_0;
    unsigned char m_4;
    char pad[0x1c - 5];
    int m_1c;
    char pad1[0x213c - 0x20];
    R2M213c m_213c;
    char pad2[0x22e8 - 0x2140];
    RECT m_22e8;
    char pad3[0x23b0 - 0x22f8];
    R2Txt23b0 m_23b0;
    char pad4[0x293c - 0x23b4];
    R2Ctl293c* m_293c;
    char* text();      // 0x4941e0
    int f4942a0();
    void f493f50();
    void refresh();
};
// MATCH: golf_clean.exe 0x00493a60 ?refresh@R2C493a60@@QAEXXZ
void R2C493a60::refresh()
{
    char* s = text();
    m_1c = f4942a0();
    f493f50();
    if (m_4 & 1) {
        g_83ad50->redraw();
        m_23b0.f476310(0x8000001f, -1, 2, 2);
        m_23b0.f4762d0(g_83b9c0, 0, 0, 0);
    }
    RECT r = m_22e8;
    RECT* p = m_293c ? m_293c->rect() : 0;
    r.right += p->left - p->right;
    if (m_4 & 8) {
        if (s)
            m_23b0.f477c30(s, 4, 4, strlen(s));
    } else if (s)
        m_23b0.f477e60(s, 0, 0, r.right - r.left, r.bottom - r.top, strlen(s));
    m_213c.f480ce0();
}
void __cdecl FUN_004a0320(const char*, const char*, DWORD, int, int);
extern const char s_004e498c[], s_004e49b4[], s_004e499c[], s_004e4974[];
struct R2Chunk668 { int read(char** buf, int size, int frame, int flag); };   // 0x4a4db0
struct R2C49cb90 {
    char pad[0xb8];
    unsigned int m_b8;
    HMMIO m_bc;
    char pad1[0xd8 - 0xc0];
    DWORD m_d8;
    char pad2[0x668 - 0xdc];
    R2Chunk668 m_668;
    char pad3[0x674 - 0x669];
    int m_674;
    char pad4[0x940 - 0x678];
    int m_940;
    void f49c940();
    int readFrame();
};
// MATCH: golf_clean.exe 0x0049cb90 ?readFrame@R2C49cb90@@QAEHXZ
int R2C49cb90::readFrame()
{
    char* buf;

    if (!m_d8) {
        if (m_668.read(&buf, 0, m_940, 1))
            return 0;
        FUN_004a0320(s_004e498c, s_004e49b4, m_940, m_674, 0);
    } else {
        if (m_668.read(&buf, m_d8, m_940, 0) == 0x12)
            return 0;
        if (!buf) {
            FUN_004a0320(s_004e498c, s_004e499c, timeGetTime(), 0, 0);
            return 0;
        }
        for (DWORD n = m_d8; n; ) {
            LONG got = mmioRead(m_bc, buf, n > 0x10000 ? 0x10000 : n);
            if (!got) {
                m_b8 &= ~0x8000;
                return 0;
            }
            n -= got;
            buf += got;
        }
        FUN_004a0320(s_004e498c, s_004e4974, m_940, m_674, 0);
    }

    f49c940();
    return 1;
}
extern char g_56fcb0[0x1002];
extern short g_59d81c[128];
extern short g_5a46b8[128];
extern char g_51a068[];
// MATCH: golf_clean.exe 0x0045b8b0 ?store45b8b0@@YAHH@Z
int store45b8b0(int id)
{
    if (id != -1 && g_59d81c[id] != -1) {
        int off = g_59d81c[id];
        int len = strlen(g_56fcb0 + off) + 1;
        memcpy(g_56fcb0 + off, g_56fcb0 + off + len, 0x1002 - len - off);
        g_59d81c[id] = -1;
        for (int i = 0; i < 128; i++)
            if (g_59d81c[i] > off)
                g_59d81c[i] -= len;
    }
    int end = 0, slot = -1;
    for (int i = 0; i < 128; i++) {
        if (g_59d81c[i] != -1) {
            if (g_5a46b8[i] + g_59d81c[i] > end)
                end = g_5a46b8[i] + g_59d81c[i];
        }
        if (g_59d81c[i] == -1 && slot == -1 && i > 0x20)
            slot = i;
    }
    if (id != -1)
        slot = id;
    int n = strlen(g_51a068) + 1;
    if (n + end < 0x1002) {
        strcpy(g_56fcb0 + end, g_51a068);
        g_59d81c[slot] = end;
        g_5a46b8[slot] = n;
        return slot;
    }
    return -1;
}


struct R2VB489 { virtual ~R2VB489(); };
struct R2Data489 : virtual R2VB489 { };
struct R2Node489 : virtual R2VB489 { int m_4; R2Data489* m_data; R2Node489* m_next; };
struct R2ListB489 : virtual R2VB489 {
    virtual void v0();
    virtual void onRemove(R2Data489*);
    R2Node489* m_head;
    R2Node489* m_cur;
    int m_count;
    int m_14;
    void clear()
    {
        int i;
        R2Data489* d;
        if (m_head) {
            for (i = 0; i < m_count; i++) {
                m_cur = m_head->m_next;
                d = m_head->m_data;
                onRemove(d);
                delete d;
                m_head->m_data = 0;
                delete m_head;
                m_head = m_cur;
            }
            m_head = 0;
            m_14 = 0;
            m_count = 0;
        }
        m_14 = 0;
    }
    ~R2ListB489()
    {
        clear();
    }
};
struct R2List489 : R2ListB489 {
    virtual void onRemove(R2Data489*);
    ~R2List489();
    int m_18, m_1c, m_20, m_24;
};
// MATCH: golf_clean.exe 0x00489b30 ??1R2List489@@UAE@XZ
R2List489::~R2List489()
{
    clear();
}
struct R2Img {
    R2V8(x) virtual void x8(); virtual void release(int);
    R2V32(y) R2V8(z) R2V4(w) virtual int width(); virtual int height(); virtual void q2(); virtual int* format();
};
struct R2B274_480 {
    virtual void b0();
    R2Img* m_4;
    int width() { return m_4 ? m_4->width() : 0; }
    int height() { return m_4 ? m_4->height() : 0; }
    void blit(R2B274_480* src, int x, int y, int sx, int sy, int w, int h);   // 0x475c60
    int fill(int x, int y, int x2, int y2, int c);                           // 0x475da0
    void paint(int a);                                                        // 0x478b30
};
struct R2C4808c0;
struct R2P0_4808 {
    R2V32(a) R2V32(b) R2V4(c) virtual void c4(); virtual void c5(); virtual void c6(); virtual int visible();
    char pad[0x9c - 4];
    unsigned int m_9c;
    char pad1[0x130 - 0xa0];
    R2C4808c0* m_130;
    char pad2[0x1ac - 0x134];
    RECT m_1ac;
    RECT m_1bc;
    char pad3[0x274 - 0x1cc];
};
extern int g_83abe0;
struct R2C4808c0 : R2P0_4808, R2B274_480 {
    void paint(int a);
};
// MATCH: golf_clean.exe 0x004808c0 ?paint@R2C4808c0@@QAEXH@Z
void R2C4808c0::paint(int a)
{
    if ((m_9c & 0x80000) && m_130 && m_130->visible()) {
        RECT o = m_1bc;
        RECT r = m_1ac;
        int x = o.left + r.left;
        int y = r.top + o.top;
        R2C4808c0* t = m_130;
        t->blit(this, x, y, 0, 0, width(), height());
        if (g_83abe0)
            fill(0, 0, width() - 1, height() - 1, g_83abe0);
    } else
        R2B274_480::paint(a);
}
int __cdecl dist467170(int, int);
extern int g_567afc;
// MATCH: golf_clean.exe 0x00435f00 ?hit435f00@@YAHHH@Z
int hit435f00(int x, int y)
{
    int r = -1;
    if (dist467170(x - 0x111, y - 0x22f) < 20)
        r = 0;
    if (dist467170(x - 0x139, y - 0x207) < 15)
        r = 1;
    if (dist467170(x - 0x139, y - 0x224) < 15)
        r = 2;
    if (dist467170(x - 0x139, y - 0x244) < 15)
        r = 3;
    if (g_567afc == 3) {
        if (dist467170(x - 0x19e, y - 0x1f9) < 20)
            r = 4;
        if (dist467170(x - 0x1ed, y - 0x1f9) < 20)
            r = 5;
        if (dist467170(x - 0x23c, y - 0x1f9) < 20)
            r = 6;
        if (dist467170(x - 0x28b, y - 0x1f9) < 20)
            r = 7;
        if (dist467170(x - 0x2da, y - 0x1f9) < 20)
            r = 8;
    }
    if (dist467170(x - 0x11e, y - 0x1ea) < 15)
        r = 9;
    if (dist467170(x - 0xe7, y - 0x21c) < 15)
        r = 10;
    return r;
}
extern const char s_004c84f0[];
// MATCH: golf_clean.exe 0x00474ee0 ?loadBmp474ee0@@YGPAEPBDPAPAUtagBITMAPINFO@@@Z
unsigned char* __stdcall loadBmp474ee0(const char* filename, BITMAPINFO** info)
{
    FILE* fp;
    unsigned char* bits;
    int bitsize;
    int infosize;
    BITMAPFILEHEADER header;

    if ((fp = fopen(filename, s_004c84f0)) == NULL)
        return NULL;
    if (fread(&header, sizeof(BITMAPFILEHEADER), 1, fp) < 1) {
        fclose(fp);
        return NULL;
    }
    if (header.bfType != 'MB') {
        fclose(fp);
        return NULL;
    }
    infosize = header.bfOffBits - sizeof(BITMAPFILEHEADER);
    if ((*info = (BITMAPINFO*)malloc(infosize)) == NULL) {
        fclose(fp);
        return NULL;
    }
    if (fread(*info, 1, infosize, fp) < infosize) {
        free(*info);
        fclose(fp);
        return NULL;
    }
    if ((bitsize = (*info)->bmiHeader.biSizeImage) == 0)
        bitsize = abs((*info)->bmiHeader.biHeight) * (*info)->bmiHeader.biBitCount * (*info)->bmiHeader.biWidth;
    if ((bits = (unsigned char*)malloc(bitsize)) == NULL) {
        free(*info);
        fclose(fp);
        return NULL;
    }
    if (fread(bits, 1, bitsize, fp) == 0) {
        free(*info);
        free(bits);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    return bits;
}
extern int g_83fe78[];
struct R2C495d30 {
    virtual void v0();
    int m_4, m_8, m_c, m_10;
    int m_14[0x22];
    R2C495d30();
};
// MATCH: golf_clean.exe 0x00495d30 ??0R2C495d30@@QAE@XZ
R2C495d30::R2C495d30()
{
    m_4 = g_83fe78[0];
    m_c = g_83fe78[1];
    m_10 = g_83fe78[2];
    m_8 = g_83fe78[3];
    m_14[0] = g_83fe78[4];
    m_14[1] = g_83fe78[5];
    m_14[2] = g_83fe78[6];
    m_14[3] = g_83fe78[7];
    m_14[4] = g_83fe78[8];
    m_14[5] = g_83fe78[9];
    m_14[6] = g_83fe78[10];
    m_14[7] = g_83fe78[11];
    m_14[8] = g_83fe78[12];
    m_14[9] = g_83fe78[13];
    m_14[10] = g_83fe78[14];
    m_14[11] = g_83fe78[15];
    m_14[12] = g_83fe78[16];
    m_14[13] = g_83fe78[17];
    m_14[14] = g_83fe78[18];
    m_14[15] = g_83fe78[19];
    m_14[16] = g_83fe78[20];
    m_14[17] = g_83fe78[21];
    m_14[18] = g_83fe78[22];
    m_14[19] = g_83fe78[23];
    m_14[20] = g_83fe78[24];
    m_14[21] = g_83fe78[25];
    m_14[22] = g_83fe78[26];
    m_14[23] = g_83fe78[27];
    m_14[24] = g_83fe78[28];
    m_14[25] = g_83fe78[29];
    m_14[26] = g_83fe78[30];
    m_14[27] = g_83fe78[31];
    m_14[28] = g_83fe78[32];
    m_14[29] = g_83fe78[33];
    m_14[30] = g_83fe78[34];
    m_14[31] = g_83fe78[35];
    m_14[32] = g_83fe78[36];
    m_14[33] = g_83fe78[37];
}
struct R2Rc47b {
    int l, t, r, b;
    void set(int x, int y, int w, int h) { l = x; t = y; r = x + w; b = y + h; }
    void offset(int dx, int dy) { l += dx; r += dx; t += dy; b += dy; }
};
void __cdecl f47e140(struct R2C47b4e0*, R2Rc47b*);
struct R2C47b4e0 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void moved(int, int, int, int);
    R2V16(a) virtual void v20(); virtual void clampSize(int*, int*, int*); virtual void sized();
    R2V32(b) R2V8(c) R2V4(d) virtual void d4(); virtual void d5(); virtual void d6(); virtual void d7(); virtual int isLocked();
    virtual void e0(); virtual void e1(); virtual void e2(); virtual void e3(); virtual void layoutChanged();
    char pad[0xa0 - 4];
    unsigned int m_a0;
    char pad1[0xb0 - 0xa4];
    R2C47b4e0* m_b0;
    char pad2[0x1ac - 0xb4];
    R2Rc47b m_1ac;
    R2Rc47b m_1bc;
    char pad3[0x1dc - 0x1cc];
    R2Rc47b m_1dc;
    void adjustSize(int* w, int* h);    // 0x47ca10
    void adjust(R2Rc47b* r);            // 0x47cce0
    void f47b9a0();
    void f47bc60(int w, int h);
    int setSize(int w, int h, int mode, int unused);
};
// MATCH: golf_clean.exe 0x0047b4e0 ?setSize@R2C47b4e0@@QAEHHHHH@Z
int R2C47b4e0::setSize(int w, int h, int mode, int unused)
{
    if (!isLocked())
        clampSize(&w, &h, &mode);
    if (mode) {
        if (!(m_a0 & 2)) {
            m_1ac.set(m_1ac.l, m_1ac.t, w, h);
            goto done;
        }
        adjustSize(&w, &h);
        mode = 0;
    }
    m_1bc.set(m_1bc.l, m_1bc.t, w, h);
    m_1ac = m_1bc;
    m_1ac.offset(-m_1ac.l, -m_1ac.t);
    adjust(&m_1ac);
done:
    layoutChanged();
    f47b9a0();
    f47e140(this, &m_1dc);
    if (m_b0 != this)
        m_b0->moved(w, h, 0, 0);
    f47bc60(m_1ac.r - m_1ac.l, m_1ac.b - m_1ac.t);
    if (g_83ad50)
        g_83ad50->update(0);
    sized();
    return 0;
}
struct R2V1_4a4 { virtual int onKey(int); R2V32(a) R2V32(b) R2V8(c) virtual void redraw(); };
struct R2V2_4a4 { char pad[0x58]; int m_58, m_5c; char pad2[0xf0 - 0x60]; int m_f0; };
struct R2C4a4680 : virtual R2V1_4a4, virtual R2V2_4a4 {
    char pad[0x88 - 4];
    int m_88;
    char pad2[0xe0 - 0x8c];
    void select(int i);     // 0x4a3f10
    virtual int onKey(int key);
};
// MATCH: golf_clean.exe 0x004a4680 ?onKey@R2C4a4680@@UAEHH@Z
int R2C4a4680::onKey(int key)
{
    int prev;
    switch (key) {
    case 0x26:
    case 0x68:
        if (m_f0 == 0)
            return 1;
        m_f0--;
        select(m_f0 + 1);
        break;
    case 0x28:
    case 0x62:
        if (m_f0 == m_88 - 1)
            return 1;
        m_f0++;
        select(m_f0 - 1);
        break;
    case 0x25:
    case 0x64:
        if (m_58 == 1)
            return 1;
        prev = m_f0;
        if (prev < m_5c)
            return 1;
        m_f0 = prev - m_5c;
        select(prev);
        break;
    case 0x27:
    case 0x66:
        if (m_58 == 1)
            return 1;
        prev = m_f0;
        if (prev >= (m_58 - 1) * m_5c)
            return 1;
        m_f0 = prev + m_5c;
        if (m_f0 > m_88 - 1) {
            m_f0 = prev;
            return 1;
        }
        select(prev);
        break;
    default:
        return 0;
    }
    select(m_f0);
    redraw();
    return 1;
}
struct R2P0_496 { virtual ~R2P0_496(); char pad[0x270]; };
struct R2P1_496 { virtual void p1(); char pad[0x2f8]; };
struct R2Base496 : R2P0_496, R2P1_496 { R2Base496(); ~R2Base496(); int m_570; };   // 0x4804a0 / 0x4805a0
struct R2M496 { R2M496(); ~R2M496(); char pad[0x6d0]; };                               // 0x4a13f0
extern int g_83ff1c, g_83ff20, g_83ff58, g_83ff5c;
extern int g_4e4764, g_4e4768, g_4e476c, g_4e4770, g_4e4774, g_4e4778, g_4e477c, g_4e4780, g_4e4784, g_4e4788, g_4e478c;
extern int g_83ff24[3], g_83ff30[3], g_83ff3c[3], g_83ff48[3];
struct R2C496030 : R2Base496 {
    int m_574, m_578, m_57c, m_580, m_584, m_588, m_58c, m_590, m_594, m_598, m_59c;
    int m_5a0, m_5a4, m_5a8, m_5ac, m_5b0, m_5b4, m_5b8, m_5bc, m_5c0, m_5c4, m_5c8, m_5cc, m_5d0, m_5d4, m_5d8;
    int m_5dc[3], m_5e8[3], m_5f4[3], m_600[3];
    R2M496 m_60c;
    R2M496 m_cdc;
    int m_13ac, m_13b0;
    R2C496030();
    virtual void p1();
};
// MATCH: golf_clean.exe 0x00496030 ??0R2C496030@@QAE@XZ
R2C496030::R2C496030()
{
    m_574 = g_83ff1c;
    m_57c = g_4e4770;
    m_588 = 0;
    m_580 = g_83ff20;
    m_584 = g_4e4774;
    m_58c = g_83ff20;
    m_590 = g_4e476c;
    m_59c = -1;
    m_598 = 0;
    m_594 = g_4e4768;
    m_5a0 = g_4e4764;
    m_5a4 = 0;
    m_5ac = 0;
    m_5b0 = 0;
    m_5b4 = 0;
    m_5b8 = 0;
    m_5a8 = g_4e4778;
    m_5bc = g_4e477c;
    m_5c4 = g_4e4780;
    m_5c8 = g_4e4784;
    m_5cc = g_4e4788;
    m_5d0 = g_4e478c;
    for (int i = 0; i < 3; i++) {
        m_5dc[i] = g_83ff24[i];
        m_5e8[i] = g_83ff30[i];
        m_5f4[i] = g_83ff3c[i];
        m_600[i] = g_83ff48[i];
    }
    m_5d4 = g_83ff58;
    m_5d8 = g_83ff5c;
    m_13ac = 0;
    m_13b0 = 0;
}
struct R2Img4806 { R2Img4806(); ~R2Img4806(); char pad[0x2b8]; };      // ctor 0x474ae0
struct R2P0_4806 { virtual void p0(); char pad[0x9c - 4]; unsigned int m_9c; char pad2[0x274 - 0xa0]; };
struct R2P1_4806 {
    virtual void p1();
    char pad[0x52c - 0x278];
    int m_52c, m_530, m_534, m_538, m_53c, m_540, m_544, m_548, m_54c, m_550, m_554;
    char pad2[0x568 - 0x558];
    R2Img4806* m_568;
    int m_56c;
    int open(int x, int y, int bits, int a, int b, int c);   // 0x474dd0
    void fail(int code);                                        // 0x4789f0
};
extern int g_83abe4, g_83abe8, g_83abec, g_83abf0, g_83abf4, g_83abf8, g_83abfc, g_83ac00, g_83ac04, g_83ac08, g_83ac0c;
extern int g_83ff10, g_83ad10;
struct R2C4806c0 : R2P0_4806, R2P1_4806 {
    int m_570;
    void reset();                                                                            // 0x480610
    int base(int a1, int a2, int x, int y, const char* s, unsigned int flags, int a7, int* a8, int* a9);   // 0x47a4c0
    void f481760();
    void adjustSize(int* w, int* h);                                                         // 0x47cb10
    int create(int a1, int a2, int x, int y, const char* s, unsigned int flags, int a7, int* a8, int* a9);
};
// MATCH: golf_clean.exe 0x004806c0 ?create@R2C4806c0@@QAEHHHHHPBDIHPAH1@Z
int R2C4806c0::create(int a1, int a2, int x, int y, const char* s, unsigned int flags, int a7, int* a8, int* a9)
{
    reset();
    if (flags & 0x30000000) {
        m_52c = g_83abe4;
        m_534 = g_83abec;
        m_530 = g_83abe8;
        m_538 = g_83abf0;
        m_548 = g_83abf4;
        m_544 = g_83abf8;
        m_53c = g_83abfc;
        m_540 = g_83ac00;
        m_54c = g_83ac08;
        m_550 = g_83ac04;
        m_554 = g_83ac0c;
    }
    if (flags & 0x40000000)
        m_568 = new R2Img4806;
    int r = base(a1, a2, x, y, s, flags, a7, a8, a9);
    if (!r) {
        f481760();
        if (!(flags & 0x800))
            adjustSize(&x, &y);
        else {
            if (m_9c & 8)
                x += g_83ff10;
            if (m_9c & 4)
                y += g_83ff10;
        }
        r = open(x, y, (flags >> 28) & 8, 1, 0, 0);
        if (!r) {
            fail(g_83ad10);
            r = 0;
        }
    }
    return r;
}
extern char g_80b130[100][100];
extern char g_51a068[];
extern const char s_004c8a34[], s_004c8a2c[], s_004c8a28[];
// MATCH: golf_clean.exe 0x0043d2a0 ?listFiles43d2a0@@YAHPBD00@Z
int listFiles43d2a0(const char* pattern, const char* incl, const char* excl)
{
    int n;
    HANDLE h;
    char name[100];
    WIN32_FIND_DATAA fd;
    memset(g_80b130, 0, sizeof(g_80b130));
    n = 0;
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    do {
        int ok = 1;
        strcpy(name, fd.cFileName);
        if (*incl && !strstr(name, incl))
            ok = 0;
        if (*excl && strstr(name, excl))
            ok = 0;
        if (strstr(name, s_004c8a34))
            ok = 0;
        if (!strstr(name, s_004c8a2c) && ok) {
            strcat(g_51a068, name);
            strcat(g_51a068, s_004c8a28);
            strcpy(g_80b130[n++], name);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return n;
}
struct R2Rng822 { unsigned short next(int n); };   // 0x45c1e0
extern R2Rng822 g_822d9c;
extern short g_53caf0[][50];
extern signed char g_5a34e0;
// MATCH: golf_clean.exe 0x004070b0 ?inRange4070b0@@YAHHHHH@Z
int inRange4070b0(int type, int v, int row, int col)
{
    int hi, lo;     // uninitialized when type is not 0x15/0x16 and no case below assigns them (the original reads the type slot there)
    if (type == 0x16 || type == 0x15) {
        hi = 200;
        lo = 0;
        if ((g_53caf0[row][col] & 0x1f) - 1 <= 3)
            hi = 0;
    }
    switch (g_5a34e0) {
    case 0:
        if (type == 0xd) {
            hi = g_822d9c.next(100) + 400;
            lo = 50;
        } else if (type == 0xe) {
            hi = g_822d9c.next(100) + 100;
            lo = 20;
        } else if (type == 0xf) {
            hi = g_822d9c.next(100) + 100;
            lo = 50;
        } else if (type == 0x10) {
            hi = g_822d9c.next(200) + 400;
            lo = 75;
        }
        break;
    case 1:
        if (type == 0xd) {
            hi = g_822d9c.next(100) + 400;
            lo = 50;
        } else if (type == 0xe) {
            hi = g_822d9c.next(100) + 100;
            lo = 20;
        } else if (type == 0xf) {
            hi = g_822d9c.next(100) + 300;
            lo = 100;
        } else if (type == 0x10) {
            hi = g_822d9c.next(200) + 400;
            lo = 75;
        }
        break;
    case 2:
        if (type == 0xd) {
            hi = g_822d9c.next(100) + 100;
            lo = 20;
        } else if (type == 0xe) {
            hi = g_822d9c.next(100) + 400;
            lo = 50;
        } else if (type == 0xf) {
            hi = g_822d9c.next(100) + 100;
            lo = 50;
        } else if (type == 0x10) {
            hi = g_822d9c.next(200) + 400;
            lo = 75;
        }
        break;
    case 3:
        if (type == 0xd || type == 0xe || type == 0xf || type == 0x10) {
            hi = g_822d9c.next(100) + 400;
            lo = 20;
        }
        break;
    }
    return v < hi && v > lo;
}
struct R2Pool474 { void* alloc(int n); };      // 0x474860
struct R2Node47a { int m_0; void* m_owner; void* m_data; R2Node47a* m_next; R2Node47a* m_prev; int m_14, m_18; };
struct R2C47abe0 {
    char pad[0x9c];
    unsigned int m_9c;
    unsigned char m_a0;
    char pad1[0x130 - 0xa1];
    R2C47abe0* m_130;
    char pad2[0x138 - 0x134];
    R2Node47a* m_head;
    R2Node47a* m_cur;
    int m_count;
    int m_index;
    R2Pool474* m_pool;
    void* alloc(int n) { return m_pool ? m_pool->alloc(n) : malloc(n); }
    void attach(R2C47abe0* p);
};
// MATCH: golf_clean.exe 0x0047abe0 ?attach@R2C47abe0@@QAEXPAU1@@Z
void R2C47abe0::attach(R2C47abe0* p)
{
    if (m_9c & 0x200000)
        return;
    if (m_a0 & 8)
        return;
    if ((m_9c & 0x1000) && !p)
        return;
    if (!(~p->m_9c & 0x1000))
        return;
    if (p->m_count == 0) {
        p->m_head = (R2Node47a*)p->alloc(0x1c);
        if (!p->m_head)
            goto out;
        p->m_head->m_owner = this;
        p->m_head->m_next = p->m_head;
        p->m_head->m_prev = p->m_head;
        if (p->m_pool)
            p->m_head->m_data = p->m_pool->alloc(4);
        else
            p->m_head->m_data = malloc(4);
        if (!p->m_head->m_data)
            goto out;
        p->m_cur = p->m_head;
    } else {
        p->m_head->m_prev->m_next = (R2Node47a*)p->alloc(0x1c);
        if (!p->m_head->m_prev->m_next)
            goto out;
        p->m_head->m_prev->m_next->m_prev = p->m_head->m_prev;
        p->m_head->m_prev->m_next->m_next = p->m_head;
        p->m_head->m_prev = p->m_head->m_prev->m_next;
        p->m_cur = p->m_head->m_prev;
        p->m_cur->m_owner = this;
        if (p->m_pool)
            p->m_cur->m_data = p->m_pool->alloc(4);
        else
            p->m_cur->m_data = malloc(4);
        if (!p->m_cur->m_data)
            goto out;
    }
    p->m_index = p->m_count;
    p->m_count++;
out:
    if (p->m_count - 1 >= 0) {
        p->m_index = 0;
        p->m_cur = p->m_head;
    }
    m_130 = p;
}
