// FLAGS golf_clean.exe: /O2 /GX
// golf_clean.exe batch r1 written by hand (release /O2). Names are chosen here, not recovered.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list>
#pragma warning(disable: 4715)
extern int g_4e4a24, g_4e4a28;
struct R1V1_49ece0 { virtual void a(); void f480610(); char pad[0x570]; };
struct R1V2_49ece0 { virtual void b(); void reset(); };
struct R1C49ece0 : virtual R1V1_49ece0, virtual R1V2_49ece0 {
    void cleanup();
    virtual void a();
    virtual void b();
    int m_4, m_8, m_c;
    int m_10, m_14;
};
// MATCH: golf_clean.exe 0x0049ece0 ?cleanup@R1C49ece0@@QAEXXZ
void R1C49ece0::cleanup()
{
    m_4 = 0;
    m_8 = 0;
    m_c = 0;
    m_14 = g_4e4a28;
    m_10 = g_4e4a24;
    R1V2_49ece0::reset();
    R1V1_49ece0::f480610();
}
extern const char s_004c1434[];
extern const char s_004c1458[];
struct R1H4823c0 { int f474860(size_t); };
extern R1H4823c0* g_839650;
// MATCH: golf_clean.exe 0x004823c0 ?setHeap4823c0@@YAHIPAUR1H4823c0@@@Z
int setHeap4823c0(size_t n, R1H4823c0* h)
{
    if (h) {
        int r = h->f474860(n);
        if (r) {
            g_839650 = h;
            return r;
        }
        MessageBoxA(0, s_004c1434, s_004c1458, 0);
        _exit(3);
    }
    void* p = malloc(n);
    if (!p)
        return 0;
    g_839650 = 0;
    return (int)p;
}
#pragma vtordisp(off)
struct R1A479 { virtual ~R1A479(); char pad[0x270]; };
struct R1B474 { virtual void b(); };
// Base with ctor 0x4804a0 and dtor 0x4805a0.
struct R1Base4804a0 : R1A479, R1B474 {
    R1Base4804a0();
    virtual ~R1Base4804a0();
    char pad2[0x574 - 0x278];
};
struct R1M49c574 { R1M49c574(); ~R1M49c574(); char pad[0x20]; };   // ctor 0x4837f0
struct R1M49c598 { R1M49c598(); int m_0; };                          // ctor 0x486c90
struct R1C49c020 : R1Base4804a0 {
    R1C49c020();
    virtual ~R1C49c020();
    virtual void b();
    R1M49c574 m_574;
    int m_594;
    R1M49c598 m_598;
};
// MATCH: golf_clean.exe 0x0049c020 ??0R1C49c020@@QAE@XZ
R1C49c020::R1C49c020()
{
    m_594 = 0;
}
#pragma vtordisp(off)
struct R1V482f60 {
    R1V482f60() { m_4 = g_839650; g_839650 = 0; }
    virtual ~R1V482f60();
    R1H4823c0* m_4;
};
struct R1VB482f60 { virtual ~R1VB482f60(); };
struct R1Data482f60 : virtual R1VB482f60 { };
struct R1Node482f60 : virtual R1VB482f60 { int m_4; R1Data482f60* m_data; R1Node482f60* m_next; };
// List with the heap holder as virtual base; the clear loop is inline in both destructors.
struct R1B482f60 : virtual R1V482f60 {
    R1B482f60() { m_head = 0; m_cur = 0; m_count = 0; m_14 = 0; m_18 = 0; }
    virtual void f();
    virtual void onRemove(R1Data482f60*);
    virtual ~R1B482f60() { reset(); }
    void removeAll()
    {
        int i;
        R1Data482f60* d;
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
    }
    void reset() { removeAll(); m_14 = 0; }
    R1Node482f60* m_head;
    R1Node482f60* m_cur;
    int m_count, m_14, m_18;
};
struct R1C482f60 : R1B482f60 {
    R1C482f60();
    virtual void f();
    virtual ~R1C482f60();
    int m_1c;
};
// MATCH: golf_clean.exe 0x00482f60 ??0R1C482f60@@QAE@XZ
R1C482f60::R1C482f60()
{
}
// MATCH: golf_clean.exe 0x004835f0 ??1R1C482f60@@UAE@XZ
R1C482f60::~R1C482f60()
{
    reset();
}
#define R1MIN(a, b) (((a) < (b)) ? (a) : (b))
struct R1F477cd0 { int m_0; int m_4; };
struct R1C477cd0 {
    char pad[0x5c];
    R1F477cd0* m_5c;
    int height();                                         // 0x477560
    int draw(const char* s, int x, int y, int n);         // 0x4775b0
    int drawCentered(const char* s, RECT* r, int n);
};
// MATCH: golf_clean.exe 0x00477cd0 ?drawCentered@R1C477cd0@@QAEHPBDPAUtagRECT@@H@Z
int R1C477cd0::drawCentered(const char* s, RECT* r, int n)
{
    if (s && r && m_5c && m_5c->m_4) {
        if (R1MIN((int)strlen(s), n) >= 0) {
            n = R1MIN((int)strlen(s), n);
            if (n) {
                RECT rc = *r;
                int y = (rc.bottom - rc.top - height()) / 2 + rc.top;
                return draw(s, rc.left, y, n);
            }
        }
    }
    return 0;
}
struct R1H482490 { char pad[0x60]; unsigned short m_60; unsigned short m_62; char pad2[0x70 - 0x64]; unsigned int m_70; };
struct R1S482490 { char pad[8]; int m_8; char pad2[0x38 - 0xc]; int* m_38; int m_3c; int m_40; };
struct R1C482490 {
    char pad[0x48];
    int** m_48;
    char pad2[0x58 - 0x4c];
    R1H482490* m_58;
    void load(int* p, R1S482490* s, int d);   // 0x482570
    void step(R1S482490* s, int f);           // 0x482420
    int* seek(int a, int b, R1S482490* s, int d);
};
// MATCH: golf_clean.exe 0x00482490 ?seek@R1C482490@@QAEPAHHHPAUR1S482490@@H@Z
int* R1C482490::seek(int a, int b, R1S482490* s, int d)
{
    R1H482490* h = m_58;
    int n, i, pos;
    if (!h)
        return 0;
    if (b == -1)
        b = 0;
    else if (h->m_70) {
        if (!(h->m_70 & (1 << a)))
            return 0;
        n = 0;
        for (i = 0; i < a; i++)
            if (h->m_70 & (1 << i))
                n++;
        a = n;
    } else
        a = a % h->m_60;
    pos = b % h->m_62;
    if (s->m_40 != a) {
        s->m_40 = a;
        s->m_3c = 0;
        s->m_38 = m_48[a];
        load(s->m_38, s, d);
        s->m_38 = (int*)((char*)s->m_38 + *s->m_38);
    }
    while (s->m_3c != pos)
        step(s, 0);
    return &s->m_8;
}
struct R1Q486110 { void setQuad(int, int, int, int); };              // 0x476310
struct R1T486110 { void start(void (*fn)(void*), void* arg, int ms, int n); };   // 0x486cf0
struct R1W486110 { char pad[0x9c]; unsigned int m_9c; };
void r1cb486b70(void*);
extern int g_83afe0, g_4e43fc, g_4e4400, g_4e4404;
struct R1C486110 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual void v54(); virtual void v55(); virtual void v56(); virtual void v57(); virtual void v58(); virtual void v59(); virtual void v60(); virtual void v61(); virtual void v62(); virtual void v63(); virtual void v64(); virtual void v65(); virtual void v66(); virtual void v67(); virtual void v68(); virtual void v69(); virtual void v70(); virtual void v71(); virtual void refresh();
    char pad0[0x274 - 4];
    R1Q486110 m_274;
    char pad1[0x2b4 - 0x275];
    int m_2b4;
    char pad2[0x574 - 0x2b8];
    char* m_574;
    char pad3[0x5a0 - 0x578];
    int m_5a0;
    int m_5a4;
    char pad4[0x5ac - 0x5a8];
    R1T486110 m_5ac;
    void reset();                    // 0x4860d0
    void setMode(int);               // 0x486250
    int createWindow(int a, int b, int c, int d, const char* title, int style, R1W486110* parent, int* e, int* f);   // 0x4806c0
    void setCursor(int id);          // 0x47ab00
    int create(int a, int b, int c, int d, R1W486110* parent, int unused);
};
// MATCH: golf_clean.exe 0x00486110 ?create@R1C486110@@QAEHHHHHPAUR1W486110@@H@Z
int R1C486110::create(int a, int b, int c, int d, R1W486110* parent, int unused)
{
    int r, style;
    reset();
    if (!parent)
        return 3;
    setMode(0x40);
    style = 0x2020;
    if (!(parent->m_9c & 0x1000))
        style = 0x102020;
    r = createWindow(a, b, c, d, 0, style, parent, 0, 0);
    if (r)
        return r;
    m_2b4 = 0;
    m_274.setQuad(g_83afe0, g_4e43fc, g_4e4400, g_4e4404);
    setCursor(0x7f01);
    m_5a0 = 0;
    m_5a4 = strlen(m_574);
    refresh();
    m_5ac.start(r1cb486b70, this, 500, 5);
    refresh();
    return 0;
}
struct R1Scr473e60 { virtual void s0(); virtual int open(void* a, int b, void* c); virtual void s2(); virtual void s3(); virtual void s4(); virtual void s5(); virtual void s6(); virtual void s7(); virtual void s8(); virtual void s9(); virtual void s10(); virtual void s11(); virtual void s12(); virtual void s13(); virtual void s14(); virtual void s15(); virtual void s16(); virtual void s17(); virtual void s18(); virtual void s19(); virtual void s20(); virtual void s21(); virtual void s22(); virtual void s23(); virtual void s24(); virtual void s25(); virtual void s26(); virtual void s27(); virtual void s28(); virtual void s29(); virtual void s30(); virtual void s31(); virtual void s32(); virtual void s33(); virtual void s34(); virtual void s35(); virtual void s36(); virtual void s37(); virtual void s38(); virtual void s39(); virtual void scale(int*, int*, int*); };
extern R1Scr473e60* g_83ad50;
struct R1Surf473e60 {
    int load(const char* name, void* pal, int a, int b, int c);   // 0x475840
    int blit(int a, int b, int c);                                  // 0x475b60
    void unload();                                                  // 0x474cb0
};
extern R1Surf473e60 g_839348;
struct R1Dev473e60 { virtual void d0(); virtual void d1(); virtual void d2(); virtual void d3(); virtual void d4(); virtual void d5(); virtual void d6(); virtual void d7(); virtual void d8(); virtual void d9(); virtual void d10(); virtual void d11(); virtual void d12(); virtual void d13(); virtual void d14(); virtual void d15(); virtual void d16(); virtual int blt(void* src, int x, int y, void* key); };
struct R1Img473e60 { int m_0; void* m_4; };
struct R1C473e60 {
    int m_0;
    R1Dev473e60* m_4;
    const char* m_8;
    char pad[0x20 - 0xc];
    int m_20, m_24;
    int draw(R1Img473e60* img, int x, int y, R1Img473e60* key);
};
// MATCH: golf_clean.exe 0x00473e60 ?draw@R1C473e60@@QAEHPAUR1Img473e60@@HH0@Z
int R1C473e60::draw(R1Img473e60* img, int x, int y, R1Img473e60* key)
{
    int r;
    int sx, d, sy;
    if (m_8) {
        r = g_839348.load(m_8, 0, 10, 0xec, 0);
        if (r)
            return r;
        r = g_839348.blit((int)img, x, y);
        g_839348.unload();
        return r;
    }
    if (!img)
        return 0x10;
    if (m_4 && img->m_4) {
        g_83ad50->scale(&sx, &sy, &d);
        int px = m_20 * sx / d;
        int py = m_24 * sy / d;
        return m_4->blt(img->m_4, px + x, py + y, key ? key->m_4 : 0);
    }
    return 7;
}
extern int g_83ff10;
struct R1I47cb10 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual void v54(); virtual void v55(); virtual void v56(); virtual void v57(); virtual void v58(); virtual void v59(); virtual void v60(); virtual void v61(); virtual void v62(); virtual void v63(); virtual void v64(); virtual void v65(); virtual void v66(); virtual void v67(); virtual void v68(); virtual void v69(); virtual void v70(); virtual void v71(); virtual void v72(); virtual void v73(); virtual void v74(); virtual void v75(); virtual void v76(); virtual void v77(); virtual void v78(); virtual void v79(); virtual void v80(); virtual void v81(); virtual void v82(); virtual void v83(); virtual void v84(); virtual void v85(); virtual void v86(); virtual void v87(); virtual void v88(); virtual void v89(); virtual void v90(); virtual void v91(); virtual int extent(); };
struct R1C47cb10 {
    char pad0[0x9c];
    unsigned int m_9c;
    char pad1[0x15c - 0xa0];
    R1I47cb10* m_15c;
    char pad2[0x180 - 0x160];
    int m_180, m_184, m_188;
    void inner(int* w, int* h);
};
// MATCH: golf_clean.exe 0x0047cb10 ?inner@R1C47cb10@@QAEXPAH0@Z
void R1C47cb10::inner(int* w, int* h)
{
    if (!w || !h)
        return;
    if (m_9c & 4)
        *h -= g_83ff10;
    if (m_9c & 8)
        *w -= g_83ff10;
    if (m_9c & 0x400) {
        *w += -m_184 * 2;
        *h += -m_184 * 2;
        if (m_188 != -1)
            *h += m_184 - m_188;
    } else if (m_9c & 0x11) {
        *w += -m_184 * 2;
        *h += -m_184 * 2;
        if (m_188 != -1)
            *h += m_184 - m_188;
    }
    if (m_9c & 0x10)
        *h += m_184 - m_180;
    if (m_15c && !(m_9c & 0x20000000))
        *h -= m_15c->extent();
}
struct R1P481760 { char pad[0x18]; int m_18; int m_1c; };
struct R1C481760 {
    char pad0[0x9c];
    unsigned int m_9c;
    char pad1[0x1a4 - 0xa0];
    int m_1a4, m_1a8;
    char pad2[0x52c - 0x1ac];
    R1P481760* m_52c;
    R1P481760* m_530;
    R1P481760* m_534;
    R1P481760* m_538;
    char pad3[0x54c - 0x53c];
    R1P481760* m_54c;
    R1P481760* m_550;
    void calcSize();
};
// MATCH: golf_clean.exe 0x00481760 ?calcSize@R1C481760@@QAEXXZ
void R1C481760::calcSize()
{
    if (m_9c & 0x10) {
        if (m_52c && m_534 && m_530 && m_538) {
            m_1a4 = m_52c->m_18 + m_534->m_18;
            m_1a4 = max(m_1a4, m_538->m_18 + m_530->m_18);
            m_1a8 = m_52c->m_1c + m_530->m_1c;
            m_1a8 = max(m_1a8, m_538->m_1c + m_534->m_1c);
        }
    } else {
        if (m_54c && m_550 && m_530 && m_538) {
            m_1a4 = m_54c->m_18 + m_550->m_18;
            m_1a4 = max(m_1a4, m_538->m_18 + m_530->m_18);
            m_1a8 = m_54c->m_1c + m_530->m_1c;
            m_1a8 = max(m_1a8, m_538->m_1c + m_550->m_1c);
        }
    }
}
#pragma vtordisp(off)
struct R1A479k { virtual ~R1A479k(); char pad[0x270]; };
struct R1B474k { virtual void b(); };
struct R1Base488500 : R1A479k, R1B474k {
    R1Base488500();
    virtual ~R1Base488500();
    char pad2[0x574 - 0x278];
};
struct R1M488500 { R1M488500(); ~R1M488500(); char pad[0x30]; };   // ctor 0x486c90, dtor 0x486ce0
extern int g_4e4480, g_4e4484, g_83b604, g_4e443c, g_4e4440, g_4e4444, g_4e4448, g_83b608, g_4e444c;
struct R1C488500 : R1Base488500 {
    R1C488500();
    virtual ~R1C488500();
    virtual void b();
    void cleanup();      // 0x4886d0
    int m_574;
    char pad3[0x57c - 0x578];
    R1M488500 m_57c;
    int m_5ac, m_5b0;
    R1M488500 m_5b4;
    int m_5e4, m_5e8, m_5ec, m_5f0, m_5f4, m_5f8, m_5fc, m_600, m_604, m_608, m_60c, m_610, m_614, m_618, m_61c, m_620, m_624, m_628, m_62c;
};
// MATCH: golf_clean.exe 0x00488500 ??0R1C488500@@QAE@XZ
R1C488500::R1C488500()
{
    m_5ec = g_4e4480;
    m_5f0 = g_4e4484;
    m_574 = 0;
    m_5e4 = 0;
    m_5ac = -1;
    m_5b0 = -1;
    m_5e8 = 0;
    m_614 = 0;
    m_5f4 = 0;
    m_5f8 = 0;
    m_620 = 0;
    m_624 = 0;
    m_628 = 0;
    m_62c = 0;
    m_60c = g_83b604;
    m_5fc = g_4e443c;
    m_600 = g_4e4440;
    m_604 = g_4e4444;
    m_608 = g_4e4448;
    m_610 = g_83b608;
    m_61c = 0;
    m_618 = g_4e444c;
}
// MATCH: golf_clean.exe 0x00488650 ??1R1C488500@@UAE@XZ
R1C488500::~R1C488500()
{
    cleanup();
}
struct R1Tile44a5b0 { char pad[0x248]; };
struct R1Ter44a5b0 {
    char pad0[0x14];
    int m_width, m_height;
    char pad1[0x3a4 - 0x1c];
    R1Tile44a5b0 m_tiles[1];
    R1Tile44a5b0* tileAt(int x, int y)
    {
        if (x >= m_width || x < 0 || y >= m_height || y < 0)
            return 0;
        return &m_tiles[x + y * m_width];
    }
    bool hasPath(R1Tile44a5b0* t);                 // 0x4a4fa0 (import thunk)
    void updatePath(int x, int y, int on);         // 0x4a4f9a
    bool hasConnectedPath(int x, int y);           // 0x4a4f94
    void layPath(R1Tile44a5b0* t, int on, int dir);   // 0x4a4f3a
};
extern R1Ter44a5b0* g_820ed0;
extern int g_4c2844;
extern std::list<R1Tile44a5b0*> g_820f18;
int blocked40bf60(int, int);
char test449f00(R1Tile44a5b0*, int, int);
char test449fa0(R1Tile44a5b0*, int, int, int, int);
char test44a380(R1Tile44a5b0*, int, int);
char test44a410(R1Tile44a5b0*, int, int);
// MATCH: golf_clean.exe 0x0044a5b0 ?collect44a5b0@@YAXHH@Z
void collect44a5b0(int cx, int cy)
{
    int x, y;
    for (x = cx - 4 / g_4c2844 * 13; x < 50; x++) {
        if (x < 0)
            x = 0;
        for (y = cy - 4 / g_4c2844 * 13; y < 50; y++) {
            if (y < 0)
                y = 0;
            if (!blocked40bf60(x, y)) {
                R1Tile44a5b0* t = g_820ed0->tileAt(x, y);
                char a = test449f00(t, x, y);
                a |= test449fa0(t, x, y, cx, cy);
                a |= test44a380(t, x, y);
                if (test44a410(t, x, y) | a)
                    g_820f18.push_front(t);
            }
        }
    }
}
#pragma vtordisp(off)
struct R1H4823c0;
extern R1H4823c0* g_839650;
struct R1V489370 {
    virtual ~R1V489370() { g_839650 = m_4; }
    R1H4823c0* m_4;
};
struct R1VB489370 { virtual ~R1VB489370(); };
struct R1Data489370 : virtual R1VB489370 { };
struct R1Node489370 : virtual R1VB489370 { int m_4; R1Data489370* m_data; R1Node489370* m_next; };
struct R1B489370 : virtual R1V489370 {
    virtual void v0();
    virtual void onRemove(R1Data489370*);
    R1Node489370* m_head;
    R1Node489370* m_cur;
    int m_count;
    int m_14;
    ~R1B489370() { reset(); }
    void reset() { removeAll(); m_14 = 0; }
    void removeAll()
    {
        int i;
        R1Data489370* d;
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
    }
};
struct R1D489370 : R1B489370 {
    ~R1D489370() { reset(); }
    void reset() { clear(); m_14 = 0; }
    void clear();      // 0x4026a0
    int m_18[4];
};
struct R1File489370 { ~R1File489370(); char pad[0xbc]; };   // dtor 0x474810
struct R1C489370 {
    virtual ~R1C489370();
    void reset();      // 0x4894b0
    R1File489370 m_4;
    R1D489370 m_list;
};
// MATCH: golf_clean.exe 0x00489370 ??1R1C489370@@UAE@XZ
R1C489370::~R1C489370()
{
    reset();
}
struct R1V4a3480 {
    virtual ~R1V4a3480() { g_839650 = m_4; }
    R1H4823c0* m_4;
};
struct R1VB4a3480 { virtual ~R1VB4a3480(); };
struct R1Data4a3480 : virtual R1VB4a3480 { };
struct R1Node4a3480 : virtual R1VB4a3480 { int m_4; R1Data4a3480* m_data; R1Node4a3480* m_next; };
struct R1L4a3480 : virtual R1V4a3480 {
    virtual void v0();
    virtual void onRemove(R1Data4a3480*);
    ~R1L4a3480() { reset(); }
    void removeAll()
    {
        int i;
        R1Data4a3480* d;
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
    }
    void reset() { removeAll(); m_14 = 0; }
    R1Node4a3480* m_head;
    R1Node4a3480* m_cur;
    int m_count, m_14;
    int m_18[4];
};
struct R1M4928d0 { ~R1M4928d0(); char pad[0x68]; };   // dtor 0x4928d0
struct R1V2_4a3480 { virtual void c(); };
#pragma vtordisp(on)
struct R1X4a3480 : virtual R1Base488500, virtual R1V2_4a3480 {
    virtual ~R1X4a3480();
    virtual void b();
    virtual void c();
    void cleanup();      // 0x4a35e0
    char pad0[0xc];
    R1M4928d0 m_10;
    R1L4a3480 m_78;
    char pad1[0xdc - 0xa8];
};
#pragma vtordisp(off)
// MATCH: golf_clean.exe 0x004a3480 ??1R1X4a3480@@UAE@XZ
R1X4a3480::~R1X4a3480()
{
    cleanup();
}
struct R1Q4887c0 {
    void setA(int, int, int, int);   // 0x476310
    void setB(int, int, int, int);   // 0x476340
    void setC(int, int, int, int);   // 0x476370
    void setD(int, int, int, int);   // 0x4762d0
};
struct R1W4887c0;
extern int g_4e4450, g_4e4454, g_4e4458, g_4e445c, g_4e4460, g_4e4464, g_4e4468, g_4e446c, g_4e4470, g_4e4474, g_4e4478, g_4e447c;
extern int g_83b60c, g_83b610, g_83b614;
struct R1C4887c0 {
    virtual void v0(); virtual void show(int); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual void v54(); virtual void v55(); virtual void v56(); virtual void v57(); virtual void v58(); virtual void v59(); virtual void v60(); virtual void v61(); virtual void v62(); virtual void v63(); virtual void v64(); virtual void v65(); virtual void v66(); virtual void v67(); virtual void v68(); virtual void v69(); virtual void v70(); virtual void v71(); virtual void v72(); virtual void v73(); virtual void v74(); virtual void v75(); virtual void v76(); virtual void v77(); virtual void v78(); virtual void v79(); virtual void v80(); virtual void v81(); virtual void v82(); virtual void v83(); virtual void v84(); virtual void v85(); virtual void v86(); virtual void v87(); virtual void v88(); virtual void v89(); virtual void v90(); virtual void reset();
    char pad0[0x274 - 4];
    R1Q4887c0 m_274;
    char pad1[0x5e8 - 0x275];
    int m_5e8;
    char pad2[0x5f4 - 0x5ec];
    char* m_5f4;
    int createWindow(int a, int b, int c, int d, const char* title, int style, R1W4887c0* parent, int* e, int* f);   // 0x4806c0
    int create(const char* text, int id, int a, int b, int c, int d, R1W4887c0* parent, int flag);
};
// MATCH: golf_clean.exe 0x004887c0 ?create@R1C4887c0@@QAEHPBDHHHHHPAUR1W4887c0@@H@Z
int R1C4887c0::create(const char* text, int id, int a, int b, int c, int d, R1W4887c0* parent, int flag)
{
    int r;
    reset();
    if (!parent)
        return 3;
    if (text) {
        m_5f4 = (char*)malloc(strlen(text) + 1);
        if (!m_5f4)
            return 4;
        *m_5f4 = 0;
        strcat(m_5f4, text);
    }
    r = createWindow(a, b, c, d, 0, (flag ? 0x8000 : 0) | 0x1000220, parent, 0, 0);
    if (r)
        return r;
    m_5e8 = id;
    m_274.setA(g_4e4450, g_4e445c, g_4e4468, g_4e4474);
    m_274.setB(g_4e4454, g_4e4460, g_4e446c, g_4e4478);
    m_274.setC(g_4e4458, g_4e4464, g_4e4470, g_4e447c);
    m_274.setD(g_83b60c, g_83b610, g_83b614, 0);
    show(0);
    return 0;
}
struct R1Rec422530 {
    char pad0[0xce - 0xc0];
    unsigned short m_ce;
    char m_d0;
    unsigned char m_d1;
    char pad1[0xda - 0xd2];
    char m_da;
    char pad2[0xee - 0xdb];
    signed char m_ee;
    char pad3[0x172 - 0xef];
    signed char m_172;
    char pad4[0x18c - 0x173];
    int m_18c;
    int m_190;
    char pad5[0x1a8 - 0x194];
    unsigned char m_1a8;
    unsigned char m_1a9;
    char pad6[0x1c0 - 0x1aa];
};
struct R1K422530 { char pad[2]; signed char m_2; char pad2[0x2d]; };
extern R1Rec422530 g_5794c0[];
extern R1K422530 g_578370[];
extern int g_822c88, g_543cc8;
int f40bfa0(int, int);
int clamp467130(int, int, int);
// MATCH: golf_clean.exe 0x00422530 ?rate422530@@YAHH@Z
int rate422530(int i)
{
    int v = ((g_5794c0[i].m_d1 & 1) ? 50 : 0) + 150;
    int k;
    if (g_822c88 >= 1)
        v += g_5794c0[i].m_172 * 50 / 3;
    else
        v += (g_5794c0[i].m_d1 & 1) ? 40 : 25;
    k = f40bfa0(g_5794c0[i].m_18c, g_5794c0[i].m_190);
    if (i >= 0x98)
        k = g_5794c0[i].m_da ? 2 : 0;
    if (g_5794c0[i].m_d0) {
        if (g_5794c0[i].m_ce & 1)
            v += g_5794c0[i].m_1a8 * 4 - 20;
        if ((g_5794c0[i].m_ce & 2) && !k)
            v += (g_5794c0[i].m_1a9 - 5) * 6;
    }
    if (g_5794c0[i].m_ee > 0)
        v += clamp467130(g_5794c0[i].m_ee, 0, 3) * v / 24;
    if (g_5794c0[i].m_d1 & 1)
        v += g_543cc8 * 15;
    if (g_578370[k].m_2 > 0)
        v -= clamp467130(g_578370[k].m_2, 0, 3) * v / 8;
    if (k)
        v += v / -5;
    return v > 330 ? 330 : v;
}
extern unsigned int g_83afd0;
extern int g_83a4f8, g_83a7b8;
struct R1Q4855b0 { void f483030(); void f483060(); };
extern R1Q4855b0 g_83acb0;
extern R1Q4855b0* g_83ad0c;
struct R1R4855b0 { int f49d090(int); };
extern R1R4855b0 g_8406e8;
extern int g_83ad20, g_83ab68, g_83ad44;
R1Scr473e60* create4a00a0(const char*);
void f485740();
int f47cfd0(int, int);
void f483320(int);
int f4b04b0();
int f49fe50();
int f49e9d0();
int f483ac0(int*);
void f4889f0(int*, int, int);
int f490bf0();
int f49d3b0();
int f49d1b0();
void f491c10();
int f4884e0();
int f486fe0();
// MATCH: golf_clean.exe 0x004855b0 ?init4855b0@@YAHPBDHI@Z
int init4855b0(const char* name, int win, unsigned int flags)
{
    int r;
    g_83afd0 = flags;
    g_83ad50 = create4a00a0(name);
    if (!g_83ad50)
        return 0x19;
    if (flags & 0x8000) {
        r = g_83ad50->open(&g_83a4f8, win, &g_83a7b8);
        if (r) {
            f485740();
            return r;
        }
    }
    if ((flags & 1) && (r = f47cfd0(win, 1)) != 0)
        return r;
    if (flags & 2) {
        f483320(0);
        g_83ad0c = &g_83acb0;
        g_83acb0.f483030();
        g_83acb0.f483060();
    }
    if ((flags & 4) && (r = f4b04b0()) != 0)
        return r;
    if ((flags & 8) && (r = f49fe50()) != 0)
        return r;
    if ((flags & 0x10) && (r = f4b04b0()) != 0)
        return r;
    if ((flags & 0x20) && (r = f49e9d0()) != 0)
        return r;
    if (flags & 0x80) {
        if ((r = f483ac0(&g_83ad20)) != 0)
            return r;
        f4889f0(&g_83ad20, 0, 0);
        g_83ab68 = g_83ad44;
    }
    if ((flags & 0x100) && (r = f490bf0()) != 0)
        return r;
    if ((flags & 0x200) && (r = f49d3b0()) != 0)
        return r;
    if ((flags & 0x400) && (r = f49d1b0()) != 0)
        return r;
    if (flags & 0x4000)
        f491c10();
    if ((flags & 0x800) && (r = f4884e0()) != 0)
        return r;
    if ((flags & 0x2000) && g_8406e8.f49d090(0x8000))
        return 1;
    if ((flags & 0x1000) && (r = f486fe0()) != 0)
        return r;
    return 0;
}
struct R1W47e140;
struct R1M47e140 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual RECT* clipRect(); };
extern R1M47e140* g_83a7bc;
inline void r1Offset47e140(RECT* r, int dx, int dy)
{
    r->left += dx;
    r->right += dx;
    r->top += dy;
    r->bottom += dy;
}
struct R1W47e140 {
    char pad0[0x9c];
    unsigned int m_9c;
    char pad1[0x1ac - 0xa0];
    RECT m_1ac;
    RECT m_1bc;
    char pad2[0x1dc - 0x1cc];
    RECT m_1dc;
    char pad3[0x224 - 0x1ec];
    R1W47e140** m_224;
    int m_228;
    int m_22c;
    void f47b0d0(RECT*);
    void f47b120(RECT*);
    int visible();     // 0x4801f0
};
// MATCH: golf_clean.exe 0x0047e140 ?clip47e140@@YAXPAUR1W47e140@@PAUtagRECT@@@Z
void clip47e140(R1W47e140* w, RECT* clip)
{
    RECT a, b;
    int i;
    a = w->m_1bc;
    r1Offset47e140(&a, -a.left, -a.top);
    w->f47b120(&a);
    b = w->m_1ac;
    r1Offset47e140(&b, -b.left, -b.top);
    w->f47b0d0(&b);
    IntersectRect(&b, &b, clip);
    IntersectRect(&a, &a, clip);
    for (i = 0; i < w->m_22c; i++) {
        if (w->m_224[i]->visible()) {
            R1W47e140* c = w->m_224[i];
            clip47e140(c, (c->m_9c & 0x8000) ? &a : &b);
        }
    }
    if (w->visible()) {
        if (w->m_9c & 0x20) {
            w->m_1dc = *clip;
            return;
        }
        if (g_83a7bc && g_83a7bc->clipRect())
            w->m_1dc = *(g_83a7bc ? g_83a7bc->clipRect() : 0);
    }
}
extern unsigned short g_53caf0[2500];
extern char g_820f2b;
int isPath4493b0(int x, int y);
inline void r1Refresh44a410()
{
    int i, j;
    for (i = 0; i < 50; i++)
        for (j = 0; j < 50; j++)
            if (isPath4493b0(i, j))
                g_820ed0->updatePath(i, j, g_53caf0[i * 50 + j] & 0x40);
    g_820f2b = 1;
}
// MATCH: golf_clean.exe 0x0044a410 ?togglePath44a410@@YA_NPAUR1Tile44a5b0@@HH@Z
bool togglePath44a410(R1Tile44a5b0* t, int x, int y)
{
    bool r = false;
    if (isPath4493b0(x, y) && !g_820ed0->hasPath(t)) {
        if (g_53caf0[x * 50 + y] & 0x40) {
            r1Refresh44a410();
        }
        g_820ed0->layPath(t, isPath4493b0(x, y), g_53caf0[x * 50 + y] & 0x40);
        r = true;
    } else if (!isPath4493b0(x, y) && g_820ed0->hasPath(t)) {
        if (g_820ed0->hasConnectedPath(x, y)) {
            r1Refresh44a410();
        }
        g_820ed0->layPath(t, isPath4493b0(x, y), g_53caf0[x * 50 + y] & 0x40);
        r = true;
    }
    return r;
}
struct R1Score4732d0 {
    char name[0x40];
    char text[0x40];
    int fun;
    int skill;
    int cash;
    char pad[0x96 - 0x8c];
    short level;
    int id;
};
extern R1Score4732d0 g_541ce0[10];
extern int g_571fd4, g_541cd8, g_59ae78, g_822c88, g_822c78, g_8392a8;
extern char g_51a068[];
extern const char s_004d6098[];
void f40daa0(int);
// MATCH: golf_clean.exe 0x004732d0 ?addScore4732d0@@YAHXZ
int addScore4732d0()
{
    int i, j, slot;
    int score = (g_571fd4 / 10 + g_541cd8 + g_59ae78) * (g_822c88 + 1);
    g_8392a8 = -1;
    for (i = 0; i < 10; i++) {
        if (score >= (g_541ce0[i].cash / 10 + g_541ce0[i].fun + g_541ce0[i].skill) * (g_541ce0[i].level + 1))
            goto found;
        if (g_541ce0[i].id == g_822c78)
            return -1;
    }
    return -1;
found:
    slot = 9;
    for (j = 9; j >= i; j--)
        if (g_541ce0[j].id == g_822c78)
            slot = j;
    for (j = slot; j > i; j--)
        g_541ce0[j] = g_541ce0[j - 1];
    g_51a068[0] = 0;
    g_541ce0[i].cash = g_571fd4;
    g_541ce0[i].fun = g_59ae78;
    g_541ce0[i].skill = g_541cd8;
    g_541ce0[i].level = g_822c88;
    g_541ce0[i].id = g_822c78;
    strcpy(g_541ce0[i].name, s_004d6098);
    f40daa0(0);
    g_8392a8 = i;
    strcpy(g_541ce0[i].text, g_51a068);
    return i;
}
struct R1Surf431d20 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual unsigned short* bits(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual int width(); virtual int height(); virtual int pitch(); virtual int* format(); };
struct R1Win431d20 {
    int m_0;
    R1Surf431d20* m_4;
    int width() { return m_4 ? m_4->width() : 0; }
    int height() { return m_4 ? m_4->height() : 0; }
    int pitch() { return m_4 ? m_4->pitch() : 0; }
    int* format() { return m_4 ? m_4->format() : 0; }
    unsigned short* bits() { if (!m_4) return 0; return m_4->bits(); }
};
extern int g_56125c, g_4c3074;
extern unsigned short* g_543cf0;
extern void* g_56a510;
extern const char s_004c7828[];
void r1cb431be0();
extern "C" int CreateJPG(FILE* f, int w, int h, void (*cb)());
// MATCH: golf_clean.exe 0x00431d20 ?saveJpg431d20@@YAXPBDPAUR1Win431d20@@HHHHH@Z
void saveJpg431d20(const char* path, R1Win431d20* w, int x, int y, int ww, int hh, int scale)
{
    int W, H;
    int* fmt;
    FILE* f;
    if (path && w && ww && hh && scale && x >= 0 && y >= 0 && ww >= 0 && hh >= 0 && scale >= 0 && scale <= 20) {
        W = w->width();
        H = w->height();
        int p = w->pitch();
        g_56125c = p;
        fmt = w->format();
        if (fmt[0] == 16 && fmt[1] == 0 && x < W && y < H) {
            if (x + ww > W)
                ww = W - x;
            if (y + hh > H)
                hh = H - y;
            f = fopen(path, s_004c7828);
            if (f) {
                g_4c3074 = scale;
                g_543cf0 = w->bits() + (g_56125c * y + x);
                g_56a510 = malloc(ww * 3 / scale);
                CreateJPG(f, ww / scale, hh / scale, r1cb431be0);
                if (g_56a510)
                    free(g_56a510);
                fclose(f);
            }
        }
    }
}
struct R1Node491710;
struct R1Data491710;
struct R1B2_491710 : virtual R1V489370 {
    virtual void v0();
    virtual void onRemove(R1Data491710*);
    ~R1B2_491710();          // 0x401c70
    R1Node491710* m_head;
    R1Node491710* m_cur;
    int m_count;
    int m_14;
    int m_18;
};
struct R1L491710 : R1B2_491710 {
    ~R1L491710() { reset(); }
    void reset() { clear(); m_14 = 0; }
    void clear();            // 0x4026a0
    int m_1c[3];
};
struct R1B473_491710 { virtual ~R1B473_491710(); char pad[0x3c]; };   // dtor 0x473ae0
struct R1M473_491710 : R1B473_491710 { virtual ~R1M473_491710() {} };
struct R1Mf491710 { ~R1Mf491710(); char pad[0x38]; };                  // dtor 0x474810
struct R1Mw491710 : R1C488500 {
    ~R1Mw491710() { shutdown(); }
    void shutdown();         // 0x4a14c0
    char pad4[0x6d0 - 0x630];
};
struct R1Sh491710 : R1Base488500 { void shutdown() throw(); char pad5[4]; };   // shutdown 0x491500
// The original sets no EH state around the three calls of this member's destructor; declaring shutdown and the
// 0x489370 destructor throw() reproduces that (the source spelling is not determined by this).
struct R1M489370nt { virtual ~R1M489370nt() throw(); char pad[0xec]; };   // dtor 0x489370
struct R1M3_491710 {
    ~R1M3_491710() { m_1fc.shutdown(); }
    char pad[0x1fc];
    R1Sh491710 m_1fc;
    R1M489370nt m_774;
};
struct R1X491710 : R1Base488500 {
    virtual ~R1X491710();
    virtual void b();
    void cleanup();          // 0x48d480
    char pad0[0x5a4 - 0x574];
    R1Mf491710 m_5a4;
    R1Mw491710 m_5dc;
    R1Mw491710 m_cac;
    char pad1[0x13a0 - 0x137c];
    R1M473_491710 m_13a0;
    R1L491710 m_13e0;
    R1L491710 m_1410;
    char pad2[0x1460 - 0x1440];
    R1M3_491710 m_1460;
    char pad3[0x1f04 - 0x1cc4];
    R1M4928d0 m_1f04;
};
// MATCH: golf_clean.exe 0x00491710 ??1R1X491710@@UAE@XZ
R1X491710::~R1X491710()
{
    cleanup();
}
