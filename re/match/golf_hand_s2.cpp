// FLAGS golf_clean.exe: /O2 /GX
// golf_clean.exe batch s2 written by hand (release /O2). Names are chosen here, not recovered.
#include <string.h>
#include <windows.h>
#pragma warning(disable: 4715)
#pragma vtordisp(off)

struct S2Obj { char pad[0x2c]; void reset(); /* 0x473ae0 */ };
struct S2Big { char pad[0x2c]; void reset(); /* 0x4838b0 */ };
extern S2Big g_821ee8, g_821f28, g_821020;
extern S2Obj g_821040, g_820f40[3], g_8210c8, g_8210f4, g_821308, g_821360, g_8213b8, g_821410;
struct S2Grp821468 { S2Obj a[3]; S2Obj b[5]; S2Obj x[12][3]; };
extern S2Grp821468 g_821468;
extern S2Obj g_821334, g_82138c, g_8213e4, g_82143c, g_821c60, g_821c8c, g_821200, g_821258, g_8212b0, g_82122c, g_821284, g_8212dc;
extern S2Obj g_8223c0, g_822418, g_822470, g_8223ec, g_822444, g_82249c, g_821f48, g_821f74, g_821fa0[18];
extern S2Obj g_8222b8, g_822310, g_822368, g_8222e4, g_82233c, g_822394, g_821bf8, g_821c24;
struct S2Grp8224f4 { S2Obj d[4]; S2Obj e[12]; S2Obj one; S2Obj c[21]; };
extern S2Grp8224f4 g_8224f4;
extern S2Obj g_8224c8, g_821070, g_82109c, g_821120, g_82114c, g_821178, g_8211a4, g_8211d0;
extern S2Obj g_821cb8, g_821d10, g_821d68, g_821dc0, g_821e18, g_821e70, g_821ce4, g_821d3c, g_821d94, g_821dec, g_821e44, g_821e9c;
// MATCH: golf_clean.exe 0x0044cce0 ?S2_resetAll44cce0@@YAXXZ
void S2_resetAll44cce0()
{
    int i, j;
    g_821ee8.reset();
    g_821f28.reset();
    g_821020.reset();
    g_821040.reset();
    for (i = 0; i < 3; i++)
        g_820f40[i].reset();
    g_8210c8.reset();
    g_8210f4.reset();
    g_821308.reset();
    g_821360.reset();
    g_8213b8.reset();
    g_821410.reset();
    for (i = 0; i < 5; i++) {
        if (i < 3) {
            for (j = 0; j < 12; j++)
                g_821468.x[j][i].reset();
            g_821468.a[i].reset();
        }
        g_821468.b[i].reset();
    }
    g_821334.reset();
    g_82138c.reset();
    g_8213e4.reset();
    g_82143c.reset();
    g_821c60.reset();
    g_821c8c.reset();
    g_821200.reset();
    g_821258.reset();
    g_8212b0.reset();
    g_82122c.reset();
    g_821284.reset();
    g_8212dc.reset();
    g_8223c0.reset();
    g_822418.reset();
    g_822470.reset();
    g_8223ec.reset();
    g_822444.reset();
    g_82249c.reset();
    g_821f48.reset();
    g_821f74.reset();
    for (i = 0; i < 18; i++)
        g_821fa0[i].reset();
    g_8222b8.reset();
    g_822310.reset();
    g_822368.reset();
    g_8222e4.reset();
    g_82233c.reset();
    g_822394.reset();
    g_821bf8.reset();
    g_821c24.reset();
    g_8224f4.one.reset();
    for (i = 0; i < 21; i++) {
        g_8224f4.c[i].reset();
        if (i < 4)
            g_8224f4.d[i].reset();
        if (i < 12)
            g_8224f4.e[i].reset();
    }
    g_8224f4.one.reset();
    g_8224c8.reset();
    g_821070.reset();
    g_82109c.reset();
    g_821120.reset();
    g_82114c.reset();
    g_821178.reset();
    g_8211a4.reset();
    g_8211d0.reset();
    g_821cb8.reset();
    g_821d10.reset();
    g_821d68.reset();
    g_821dc0.reset();
    g_821e18.reset();
    g_821e70.reset();
    g_821ce4.reset();
    g_821d3c.reset();
    g_821d94.reset();
    g_821dec.reset();
    g_821e44.reset();
    g_821e9c.reset();
}

struct S2Screen { virtual void s0(); virtual void s1(); virtual void s2(); virtual void s3(); virtual void s4(); virtual void s5(); virtual void s6(); virtual void s7(); virtual void s8(); virtual void s9(); virtual void s10(); virtual void s11(); virtual void s12(); virtual void s13(); virtual void s14(); virtual void s15(); virtual void s16(); virtual void s17(); virtual void s18(); virtual void s19(); virtual void s20(); virtual void s21(); virtual void s22(); virtual void s23(); virtual void s24(); virtual void s25(); virtual void s26(); virtual void s27(); virtual void s28(); virtual void s29(); virtual void s30(); virtual void s31(); virtual void s32(); virtual void s33(); virtual void s34(); virtual void s35(); virtual void s36(); virtual void s37(); virtual void s38(); virtual void setScale(int, int, int); };
extern S2Screen* g_83ad50;
struct S2Img { char pad[0x2c]; };
struct S2Sprite { char pad[0x2c]; void draw(S2Img*, void*, int, int, int); /* 0x473f60 */ };
extern S2Sprite g_562368[][8];
extern S2Img g_59d920[][8];
extern char g_5a34e0;
extern void* g_4c1570;
struct S2Fill { void fill(int, int, int, int, unsigned int); /* 0x480b00 */ };
extern S2Fill g_519a60;
// MATCH: golf_clean.exe 0x0040d0b0 ?S2_frame40d0b0@@YAXHHHHH@Z
void S2_frame40d0b0(int x, int y, int w, int h, int fill)
{
    int i, r;
    if (g_83ad50)
        g_83ad50->setScale(1, 1, 1);
    r = w & 15;
    if (r) {
        w += 15 - r;
        x -= (15 - r) / 2;
    }
    r = h & 15;
    if (r) {
        h += 15 - r;
        y -= (15 - r) / 2;
    }
    if (fill)
        g_519a60.fill(x + 2, y + 2, w - 4, h - 4, 0x80007fdc);
    g_562368[g_5a34e0][1].draw(&g_59d920[g_5a34e0][1], g_4c1570, x, y, 0);
    for (i = 16; i < w - 16; i += 16) {
        g_562368[g_5a34e0][4].draw(&g_59d920[g_5a34e0][4], g_4c1570, x + i, y, 0);
        g_562368[g_5a34e0][0].draw(&g_59d920[g_5a34e0][0], g_4c1570, x + i, y + h - 16, 0);
    }
    for (i = 16; i < h - 16; i += 16) {
        g_562368[g_5a34e0][2].draw(&g_59d920[g_5a34e0][2], g_4c1570, x, y + i, 0);
        g_562368[g_5a34e0][6].draw(&g_59d920[g_5a34e0][6], g_4c1570, x + w - 16, y + i, 0);
    }
    g_562368[g_5a34e0][5].draw(&g_59d920[g_5a34e0][5], g_4c1570, x + w - 16, y, 0);
    g_562368[g_5a34e0][7].draw(&g_59d920[g_5a34e0][7], g_4c1570, x + w - 16, y + h - 16, 0);
    g_562368[g_5a34e0][3].draw(&g_59d920[g_5a34e0][3], g_4c1570, x, y + h - 16, 0);
}

#include <string.h>
#include <stdlib.h>
#include <windows.h>
#pragma vtordisp(off)
struct S2DSBuf { virtual HRESULT __stdcall q0(); virtual ULONG __stdcall q1(); virtual ULONG __stdcall Release(); virtual HRESULT __stdcall q3(); virtual HRESULT __stdcall Stop(); };
extern S2DSBuf* g_8400b0;
struct S2Item497 { virtual ~S2Item497(); };
struct S2NodeBase497 { virtual ~S2NodeBase497(); };
struct S2Node497 : virtual S2NodeBase497 { int m_4; S2Item497* data; S2Node497* next; };
struct S2List497 {
    virtual void v0(); virtual void remove(S2Item497*);
    int m_4; S2Node497* head; S2Node497* cur; int count; int m_14;
    void clear() {
        if (head) {
            for (int i = 0; i < count; i++) {
                cur = head->next;
                S2Item497* d = head->data;
                remove(d);
                if (d)
                    delete d;
                head->data = 0;
                delete head;
                head = cur;
            }
            head = 0;
            m_14 = 0;
            count = 0;
        }
    }
};
struct S2Sub497 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void reset(); };
struct S2Res497 { char pad[0x28]; void reset(); /* 0x4a4b00 */ };
struct S2Voice497 { int m_0; int m_4; int m_8; int m_c; int m_10; char m_14; char pad[0x58 - 0x15]; };
struct S2Snd497 {
    int m_0; void* m_4; int m_8; int m_c; int m_10;
    HANDLE m_14; HANDLE m_18; HANDLE m_1c; HANDLE m_20; HANDLE m_24;
    int m_28; int m_2c; int m_30; int m_34; CRITICAL_SECTION m_38;
    int m_50; int m_54; int m_58; int m_5c; int m_60;
    S2Sub497 m_64; char pad64[0xc0 - 0x68];
    S2List497 m_c0; char padd8[0xe4 - 0xd8];
    int m_e4; int m_e8; int m_ec; int m_f0; int m_f4;
    S2Res497 m_f8; S2Res497 m_120; S2Res497 m_148;
    S2Voice497 m_170[16];
    char pad6f0[0x6f8 - 0x6f0];
    int m_6f8; void* m_6fc; char pad700[0x708 - 0x700];
    int m_708[4]; int m_718; int m_71c; void* m_720; void* m_724; char pad728[0x77c - 0x728];
    int m_77c; int m_780;
    void shutdown();
};
// MATCH: golf_clean.exe 0x00497d10 ?shutdown@S2Snd497@@QAEXXZ
void S2Snd497::shutdown()
{
    int i;
    m_64.reset();
    m_f0 = 20000;
    m_77c = 0;
    m_780 = 0;
    m_ec = 0;
    m_6f8 = 0;
    if (g_8400b0) {
        EnterCriticalSection(&m_38);
        SetEvent(m_20);
        g_8400b0->Stop();
        g_8400b0->Release();
        g_8400b0 = 0;
        m_28 = 0;
        LeaveCriticalSection(&m_38);
        Sleep(0);
        EnterCriticalSection(&m_38);
        LeaveCriticalSection(&m_38);
        m_2c = 0;
        m_30 = 0;
        m_c = 0;
    }
    if (m_34) {
        m_34 = 0;
        DeleteCriticalSection(&m_38);
    }
    if (m_14) { CloseHandle(m_14); m_14 = 0; }
    if (m_18) { CloseHandle(m_18); m_18 = 0; }
    if (m_1c) { CloseHandle(m_1c); m_1c = 0; }
    if (m_20) { CloseHandle(m_20); m_20 = 0; }
    if (m_24) { CloseHandle(m_24); m_24 = 0; }
    if (m_720) { free(m_720); m_720 = 0; }
    if (m_724) { free(m_724); m_724 = 0; }
    if (m_6fc) { free(m_6fc); m_6fc = 0; }
    if (m_4) { free(m_4); m_4 = 0; }
    m_8 = 0;
    m_718 = 200;
    m_e8 = 0;
    for (i = 0; i < 16; i++) {
        m_170[i].m_0 = 0;
        m_170[i].m_4 = 0;
        m_170[i].m_8 = 0;
        m_170[i].m_14 = 0;
    }
    m_c0.clear();
    m_e4 = 0;
    m_e8 = 0;
    m_f8.reset();
    m_120.reset();
    m_148.reset();
    memset(m_708, 0, 16);
    m_f4 = 1;
    m_54 = 0;
    m_58 = 0;
    m_5c = 0;
    m_60 = 0;
}

extern S2Sprite g_561810[9];
extern S2Img g_58d1e0[9];
// MATCH: golf_clean.exe 0x0040cc00 ?S2_panel40cc00@@YAXHHHH@Z
void S2_panel40cc00(int x, int y, int w, int h)
{
    int i, r;
    if (g_83ad50)
        g_83ad50->setScale(1, 1, 1);
    r = w & 15;
    if (r) {
        w += 15 - r;
        x -= (15 - r) / 2;
    }
    r = h & 15;
    if (r) {
        h += 15 - r;
        y -= (15 - r) / 2;
    }
    g_519a60.fill(x + 4, y + 4, w - 8, h - 8, 0x80004e79);
    g_561810[0].draw(&g_58d1e0[0], g_4c1570, x, y, 0);
    for (i = 16; i < w - 16; i += 16) {
        g_561810[1].draw(&g_58d1e0[1], g_4c1570, x + i, y, 0);
        g_561810[7].draw(&g_58d1e0[7], g_4c1570, x + i, y + h - 16, 0);
    }
    for (i = 16; i < h - 16; i += 16) {
        g_561810[3].draw(&g_58d1e0[3], g_4c1570, x, y + i, 0);
        g_561810[5].draw(&g_58d1e0[5], g_4c1570, x + w - 16, y + i, 0);
    }
    g_561810[2].draw(&g_58d1e0[2], g_4c1570, x + w - 16, y, 0);
    g_561810[8].draw(&g_58d1e0[8], g_4c1570, x + w - 16, y + h - 16, 0);
    g_561810[6].draw(&g_58d1e0[6], g_4c1570, x, y + h - 16, 0);
}

#pragma vtordisp(off)
struct S2E489 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); };
struct S2D489 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual void v54(); virtual void v55(); virtual void v56(); virtual void v57(); virtual void v58(); virtual void v59(); virtual void v60(); virtual void v61(); virtual void v62();
    virtual void setText(int, int); virtual void setValue(int);
    char pad[0x94 - 4]; S2E489* m_94;
};
extern int g_83b654;
struct S2A489 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual void v54(); virtual void v55(); virtual void v56(); virtual void v57(); virtual void v58(); virtual void v59(); virtual void v60(); virtual void v61(); virtual void v62(); virtual void v63(); virtual void v64(); virtual void v65(); virtual void v66(); virtual void v67(); virtual void v68(); virtual void v69(); virtual void v70(); virtual void v71(); virtual void redraw();
    char pad[0x130 - 4]; S2D489* m_130;
    void scrollTo(int);     // 0x47b9f0
    void scrollToRow(int);  // 0x47ba10
};
struct S2B489 {
    char pad0[0x58]; int m_58; int m_5c; char pad1[0xd0 - 0x60]; int m_d0; char pad2[0xf0 - 0xd4]; int m_f0;
    int current();       // 0x489950
    int itemAt(int);     // 0x489a30
    int step() { return m_58 > 1 ? m_5c : 1; }
};
struct S2C489 : virtual S2A489, virtual S2B489 {
    virtual void c0();
    char pad8[0x10 - 8]; int m_10; char pad14[0x28 - 0x14]; int m_28; int m_2c; char pad30[0x50 - 0x30]; void (*m_50)(int);
    void select(int v, int notify);
};
// MATCH: golf_clean.exe 0x00489f50 ?select@S2C489@@QAEXHH@Z
void S2C489::select(int v, int notify)
{
    int moved = 0;
    if (v == m_f0 && !m_10)
        return;
    m_f0 = v;
    if (v >= 0) {
        while (m_f0 < m_28) {
            moved = 1;
            m_28 -= step();
            if (m_28 < 0)
                m_28 = 0;
        }
        if (m_d0 <= m_2c)
            m_28 = 0;
        else {
            while (m_f0 >= m_2c + m_28) {
                moved = 1;
                m_28 += step();
            }
        }
        if (moved && m_d0 > m_2c) {
            if (m_58 > 1)
                scrollToRow(m_28);
            else
                scrollTo(m_28);
        }
        if (m_50)
            m_50(current());
        if (notify) {
            S2D489* d = m_130;
            if (d)
                d->setValue(itemAt(v));
            d = m_130;
            if (d)
                d->setText(itemAt(v), g_83b654);
            if (m_130 && m_130->m_94)
                m_130->m_94->v7();
        }
    }
    redraw();
}

struct S2Img47b { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual int width(); virtual int height(); };
struct S2Skin47b { int m_0; S2Img47b* m_4; int width() { return m_4 ? m_4->width() : 0; } int height() { return m_4 ? m_4->height() : 0; } };
struct S2Win47b;
extern S2Win47b* g_83ab2c;
struct S2Win47b {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void move(int, int, int, int); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void onSize(int, int);
    char pad0[0x9c - 4]; unsigned int m_9c;
    char pad1[0x1ac - 0xa0]; int m_1ac; int m_1b0; int m_1b4; int m_1b8;
    char pad2[0x230 - 0x1bc]; void (*m_230)(int, int);
    char pad3[0x26c - 0x234]; S2Win47b* m_26c; S2Win47b* m_270; S2Skin47b m_274;
    void setPos(int, int);   // 0x47b420
    void f47d570();          // 0x47d570
    void resized(int a, int b);
};
// MATCH: golf_clean.exe 0x0047bc60 ?resized@S2Win47b@@QAEXHH@Z
void S2Win47b::resized(int a, int b)
{
    g_83ab2c = this;
    if (!(m_9c & 0x40)) {
        if (m_26c) {
            m_26c->setPos(m_1b4 - m_1ac, 0);
            if (m_270)
                m_26c->move(m_26c->m_274.width(), m_1b8 - m_1b0 - m_270->m_274.height(), 0, 0);
            else
                m_26c->move(m_26c->m_274.width(), m_1b8 - m_1b0, 0, 0);
        }
        if (m_270) {
            m_270->setPos(0, m_1b8 - m_1b0);
            if (m_26c)
                m_270->move(m_1b4 - m_1ac - m_26c->m_274.width(), m_270->m_274.height(), 0, 0);
            else
                m_270->move(m_1b4 - m_1ac, m_270->m_274.height(), 0, 0);
        }
    }
    if (m_230)
        m_230(a, b);
    onSize(a, b);
    f47d570();
}

#pragma warning(disable: 4700) // 0x4a0350: oldIdx/newIdx are read only under their flags, never initialized in the original
struct S2Lis4a0 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual void v54(); virtual void changed3(int, int, int); virtual void changed(int, int); };
struct S2Btn4a0 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual void v54(); virtual void v55(); virtual void v56(); virtual void v57(); virtual void v58(); virtual void v59(); virtual void v60(); virtual void v61(); virtual void v62(); virtual void v63(); virtual void v64(); virtual void v65(); virtual void v66(); virtual void v67(); virtual void v68(); virtual void v69(); virtual void v70(); virtual void v71(); virtual void redraw();
    char pad0[0x130 - 4]; S2Lis4a0* m_130;
    char pad1[0x5e4 - 0x134]; int m_5e4; int m_5e8;
    void setChecked(int);   // 0x4890e0
};
struct S2Radio4a0 {
    int m_0; S2Btn4a0* m_items[32]; int m_84; int m_88; unsigned int m_8c; int m_90; int m_94;
    int select(int id);
};
// MATCH: golf_clean.exe 0x004a0350 ?select@S2Radio4a0@@QAEHH@Z
int S2Radio4a0::select(int id)
{
    int i;
    int oldIdx, newIdx;
    int hasOld = 0, hasNew = 0;
    for (i = 0; i < m_84; i++) {
        if (m_items[i]->m_5e8 == m_88) {
            oldIdx = i;
            hasOld = 1;
        }
        if (m_items[i]->m_5e8 == id) {
            hasNew = 1;
            newIdx = i;
        }
    }
    if ((m_8c & 2) && m_88 == id) {
        m_88 = -1;
        hasNew = 0;
    }
    if (m_88 != id || (m_8c & 1)) {
        if (hasOld) {
            m_items[oldIdx]->setChecked(0);
            m_items[oldIdx]->redraw();
        }
        if (hasNew) {
            m_items[newIdx]->setChecked(1);
            m_items[newIdx]->redraw();
            m_88 = id;
        } else
            m_88 = -1;
        if (!m_94 && m_84) {
            m_items[0]->m_130->changed(hasOld ? m_items[oldIdx]->m_5e8 : -1, hasNew ? m_items[newIdx]->m_5e8 : -1);
            if (m_84)
                m_items[0]->m_130->changed3(m_90, hasOld ? m_items[oldIdx]->m_5e8 : -1, hasNew ? m_items[newIdx]->m_5e8 : -1);
        }
    }
    for (i = 0; i < m_84; i++) {
        if (m_items[i]->m_5e4) {
            m_items[i]->m_5e4 = 0;
            m_items[i]->redraw();
        }
    }
    return 0;
}

#include <string.h>
extern int g_4c2854, g_56d1b0, g_5a9f50, g_5a9f60, g_5a9f64, g_55e924, g_567afc, g_4c2e08;
extern char g_51a068[];
extern const char s_4c7ec4[];
extern const char s_4c7e7c[];
int S2_hit434980(int, int);                  // 0x434980
void S2_message40d320(int, int, int, int);   // 0x40d320
void S2_f45c0c0(int);                        // 0x45c0c0
struct S2Pick434 { int pick(int); /* 0x45c1e0 */ };
extern S2Pick434 g_822d9c;
struct S2Gfx434 { void flush(int); /* 0x480c80 */ };
extern S2Gfx434 g_519a60_434;
// MATCH: golf_clean.exe 0x00434ac0 ?S2_click434ac0@@YAHHH@Z
int S2_click434ac0(int x, int y)
{
    int r = S2_hit434980(x, y);
    switch (r) {
    case 0:
        g_4c2854 = 3;
        return -1;
    case 1:
        g_4c2854 = 0;
        return -1;
    case 2:
        g_4c2854 = g_56d1b0 > 0 ? 5 : -1;
        return -1;
    case 3:
        g_5a9f60 = 0;
        g_5a9f50 = 0;
        g_4c2854 = 1;
        return 0;
    case 4:
        g_4c2854 = 4;
        g_5a9f60 = -1;
        g_5a9f50 = -1;
        return -1;
    case 5:
        g_4c2854 = 2;
        return g_5a9f50 = g_5a9f60 = (unsigned short)g_822d9c.pick(4);
    case 6:
        g_4c2854 = 16;
        return g_5a9f50 = g_5a9f60 = (unsigned short)g_822d9c.pick(7);
    case 7:
    case 8:
        strcpy(g_51a068, s_4c7ec4);
        g_4c2e08 = -22;
        strcat(g_51a068, s_4c7e7c);
        S2_message40d320(200, 0x154, 0x80007fff, -2);
        g_519a60_434.flush(0);
        S2_f45c0c0(0);
        return g_5a9f64;
    case 9:
        g_4c2854 = 19;
        return g_5a9f50 = g_5a9f60 = (unsigned short)g_822d9c.pick(8);
    default:
        if (r == -2) {
            g_55e924 = 0;
            g_567afc = 0;
        }
        if ((g_4c2854 == 4 || g_4c2854 == 2 || g_4c2854 == 1 || g_4c2854 == 16 || g_4c2854 == 19) && (g_5a9f50 != -1 || g_4c2854 == 4))
            return g_5a9f60 = g_5a9f50;
        return 0;
    }
}

// ---- 0x00491da0 ----
#include <string.h>
#include <windows.h>
extern char g_83c004[];
extern char g_83bf04[];
extern char g_83c104[];
extern char g_83be04[];
// MATCH: golf_clean.exe 0x00491da0 ?S2_resolvePath491da0@@YAPADPAD@Z
char* S2_resolvePath491da0(char* param_1)
{
    WIN32_FIND_DATAA fd;
    if (param_1) {
        if (param_1 == g_83c004)
            return g_83c004;
        if (param_1[1] == ':') {
            HANDLE h;
            g_83c004[0] = 0;
            strcat(g_83c004, param_1);
            h = FindFirstFileA(g_83c004, &fd);
            FindClose(h);
            return (char*)(h != INVALID_HANDLE_VALUE ? (int)g_83c004 : 0);
        }
        if (g_83bf04[0]) {
            HANDLE h;
            g_83c004[0] = 0;
            strcat(g_83c004, g_83bf04);
            strcat(g_83c004, param_1);
            h = FindFirstFileA(g_83c004, &fd);
            if (h != INVALID_HANDLE_VALUE) {
                FindClose(h);
                return g_83c004;
            }
        }
        g_83c004[0] = 0;
        strcat(g_83c004, g_83c104);
        strcat(g_83c004, param_1);
        {
            HANDLE h = FindFirstFileA(g_83c004, &fd);
            if (h != INVALID_HANDLE_VALUE) {
                FindClose(h);
                return g_83c004;
            }
        }
        if (g_83be04[0]) {
            HANDLE h;
            g_83c004[0] = 0;
            strcat(g_83c004, g_83be04);
            strcat(g_83c004, param_1);
            h = FindFirstFileA(g_83c004, &fd);
            if (h != INVALID_HANDLE_VALUE) {
                FindClose(h);
                return g_83c004;
            }
        }
    }
    return 0;
}
