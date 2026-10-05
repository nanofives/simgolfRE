// FLAGS golf_clean.exe: /O2 /GX
// golf_clean.exe batch r0 written by hand (release /O2). Names are chosen here, not recovered.
#include <windows.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#define V8(p) virtual void p##0(); virtual void p##1(); virtual void p##2(); virtual void p##3(); virtual void p##4(); virtual void p##5(); virtual void p##6(); virtual void p##7();
struct R0I484c20 { V8(a) V8(b) V8(c) virtual void d0(); virtual void d1(); virtual void d2(); virtual void set(int); };
struct R0C484c20 {
    V8(a) V8(b) V8(c) virtual void d0(); virtual void d1(); virtual void d2(); virtual void d3(); virtual int get(); virtual void d5(); virtual void d6(); virtual void refresh();
    char pad[0x40 - 4];
    R0I484c20* m_40;
    int open(int a);
};
int __cdecl r0_create4840e0(R0I484c20** pp, int a, int b);
// MATCH: golf_clean.exe 0x00484c20 ?open@R0C484c20@@QAEHH@Z
int R0C484c20::open(int a)
{
    int r;
    R0I484c20** pp = &m_40;
    if (*pp == 0) {
        r = r0_create4840e0(pp, a, 1);
        if (r == 0) {
            (*pp)->set(get());
            refresh();
        }
    } else
        r = 12;
    return r;
}




struct R0M4928d0 { ~R0M4928d0(); char pad[0x10]; };   // dtor 0x4928d0
struct R0C474c40 {
    virtual ~R0C474c40();
    char pad0[0xb4 - 4];
    R0M4928d0 m_b4;
    char pad1[0x254 - 0xb4 - sizeof(R0M4928d0)];
    R0M4928d0 m_254;
    void cleanup();   // 0x474cb0
};
// MATCH: golf_clean.exe 0x00474c40 ??1R0C474c40@@UAE@XZ
R0C474c40::~R0C474c40()
{
    cleanup();
}

struct R0B4805a0 { virtual ~R0B4805a0(); char pad[0x270]; };   // dtor 0x4805a0
struct R0B43cb50 { virtual void v(); };
struct R0M486ce0 { ~R0M486ce0(); int m_0; };                   // dtor 0x486ce0
struct R0C43cb50 : R0B4805a0, R0B43cb50 {
    virtual ~R0C43cb50();
    void cleanup();   // 0x4860d0
    char pad[0x5ac - 0x278];
    R0M486ce0 m_5ac;
};
// MATCH: golf_clean.exe 0x0043cb50 ??1R0C43cb50@@UAE@XZ
R0C43cb50::~R0C43cb50()
{
    cleanup();
}

struct R0X44b3e0 { virtual ~R0X44b3e0() { cleanup(); } void cleanup(); char pad[0x28]; };   // cleanup 0x473ae0
struct R0Y44b3e0 { ~R0Y44b3e0(); char pad[0x2c]; };                                    // dtor 0x4041f0
struct R0C44b3e0 {
    R0X44b3e0 m_0;
    R0X44b3e0 m_2c;
    R0Y44b3e0 m_58[0x12];
    ~R0C44b3e0();
};
// MATCH: golf_clean.exe 0x0044b3e0 ??1R0C44b3e0@@QAE@XZ
R0C44b3e0::~R0C44b3e0()
{
}

struct R0S494020 { char pad[0x18]; int m_18; int m_1c; };
struct R0T494020 { R0S494020* a[3]; };
struct R0I494020 { V8(a) V8(b) V8(c) V8(d) V8(e) V8(f) virtual void h0(); virtual void h1(); virtual void h2(); virtual void h3(); virtual void h4(); virtual void h5(); virtual int width(); virtual int height(); };
struct R0W494020 {
    V8(a) V8(b) V8(c) V8(d) V8(e) V8(f) V8(g) V8(h) V8(i) virtual void update();
    char pad[0x650 - 4];
    R0T494020 m_650;
    void move(int x, int y);   // 0x47b420
    void set(R0S494020* a, R0S494020* b, R0S494020* c) { m_650.a[0] = a; m_650.a[1] = b; m_650.a[2] = c; }
};
struct R0C494020 {
    char pad0[0x23b4];
    R0I494020* m_23b4;
    char pad1[0x26c4 - 0x23b8];
    R0W494020 m_26c4;
    char pad2[0x3474 - 0x26c4 - sizeof(R0W494020)];
    R0T494020 m_3474;
    int width() { return m_23b4 ? m_23b4->width() : 0; }
    int height() { return m_23b4 ? m_23b4->height() : 0; }
    void setItems(R0S494020** p);
};
// MATCH: golf_clean.exe 0x00494020 ?setItems@R0C494020@@QAEXPAPAUR0S494020@@@Z
void R0C494020::setItems(R0S494020** p)
{
    for (int i = 0; i < 3; i++)
        m_3474.a[i] = p[i];
    m_26c4.set(m_3474.a[0], m_3474.a[1], m_3474.a[2]);
    int h = height();
    int t = m_3474.a[0]->m_1c;
    int w = width();
    m_26c4.move(w - m_3474.a[0]->m_18 - 4, (h - t) / 2);

    m_26c4.update();
}

extern int g_839aa8;
struct R0C476e20 {
    char pad[0x14];
    int m_14;
    int m_18;
    int m_1c;
    char* fit(char* s, int* w, int len);   // 0x476ef0
    int lineHeight();                      // 0x477580
    int wrap(char* s, int w, char** lines, int* count);
};
// MATCH: golf_clean.exe 0x00476e20 ?wrap@R0C476e20@@QAEHPADHPAPADPAH@Z
int R0C476e20::wrap(char* s, int w, char** lines, int* count)
{
    if (!s)
        return 0;
    m_1c = 0;
    g_839aa8 = 0;
    m_18 = m_14;
    int total = 0;
    int n = 0;
    char** out = lines;
    int left = w;
    do {
        if (lines) {
            n++;
            *out++ = s;
        }
        int len = strlen(s);
        w = left;
        char* next = fit(s, &w, len);

        g_839aa8 = 0;
        total += lineHeight();
        s = next;
        m_1c++;
        if (m_18) {
            left -= m_18;
            m_18 = 0;
        }
    } while (s);

    if (count)
        *count = n;
    return total;
}

struct R0R4929b0 { int l, t, r, b; void set(int x, int y, int w, int h) { l = x; t = y; r = x + w; b = y + h; } };
struct R0E4929b0 { int m_0; R0R4929b0 rc; int m_14; int m_18; char* text; void init(); };   // init 0x492660
struct R0C4929b0 {
    char pad[0x50];
    R0E4929b0* m_50;
    int m_54;
    int m_58;
    void grow();   // 0x492690
    int add(int a, int b, int x, int y, int w, int h, const char* s);
};
// MATCH: golf_clean.exe 0x004929b0 ?add@R0C4929b0@@QAEHHHHHHHPBD@Z
int R0C4929b0::add(int a, int b, int x, int y, int w, int h, const char* s)
{
    if (m_58 >= m_54)
        grow();
    int n = m_58++;
    m_50[n].init();
    m_50[n].rc.set(x, y, w, h);
    m_50[n].m_18 = a;
    m_50[n].m_14 = b;

    if (s) {
        m_50[n].text = (char*)malloc(strlen(s) + 1);
        if (!m_50[n].text)
            return 4;
        *m_50[n].text = 0;
        strcat(m_50[n].text, s);
    }
    return n;
}

extern char g_51a068[];
extern const char s_004d395c[];
extern const char s_004d3954[];
void __cdecl r0_f4676e0(int side, int b);
void __cdecl r0_f45b7c0(const char* key, const char* value);
// MATCH: golf_clean.exe 0x0045c460 ?r0_names45c460@@YAXH@Z
void r0_names45c460(int side)
{
    char save[1024];
    char a[64];
    char b[64];
    strcpy(save, g_51a068);
    g_51a068[0] = 0;
    r0_f4676e0(side, 0);
    strcpy(a, g_51a068);
    g_51a068[0] = 0;
    r0_f4676e0(side ^ 1, 0);
    strcpy(b, g_51a068);
    strcpy(g_51a068, save);
    r0_f45b7c0(s_004d395c, a);
    r0_f45b7c0(s_004d3954, b);
}

extern unsigned char g_5619a0[][0x32];
extern int g_4c2878[8];
extern int g_4c2898[8];
void __cdecl r0_f42f4b0(int x, int y, int* a, int* b);
int __cdecl r0_f40bf60(int x, int y);
int __cdecl r0_f40bfe0(int x, int y, int d, int f);
// MATCH: golf_clean.exe 0x0042f530 ?r0_edges42f530@@YAXHH@Z
void r0_edges42f530(int x, int y)
{
    int a, b;
    r0_f42f4b0(x, y, &a, &b);
    unsigned char* p = &g_5619a0[x][y];
    *p = 0;
    for (int k = 0; k < 8; k += 2) {
        int nx = g_4c2878[k] + x;
        int ny = g_4c2898[k] + y;
        if (!r0_f40bf60(nx, ny)) {
            if (r0_f40bfe0(x, y, k - 1, 0) < r0_f40bfe0(nx, ny, k + 5, 0))
                *p |= 1 << k;
            if (r0_f40bfe0(x, y, k + 1, 0) < r0_f40bfe0(nx, ny, k + 3, 0))
                *p |= 1 << k;

        }
    }
}

struct R0N47a3c0 { int m_0; int m_4; void* data; R0N47a3c0* next; };
struct R0L47a3c0 {
    virtual void v0();
    R0N47a3c0* head;
    R0N47a3c0* tmp;
    int count;
    int m_144;
    int shared;
    void clear() {
        if (head) {
            if (!shared) {
                for (int i = 0; i < count; i++) {
                    tmp = head->next;
                    if (head->data)
                        free(head->data);
                    head->data = 0;
                    if (head)
                        free(head);
                    head = tmp;
                }
            }
            head = 0;
            m_144 = 0;
            count = 0;
        }
    }
    virtual ~R0L47a3c0() { clear(); m_144 = 0; }
};
struct R0B47a3c0 { virtual ~R0B47a3c0() { cleanup(); } void cleanup(); };   // cleanup 0x495eb0
struct R0C47a3c0 : R0B47a3c0 {
    virtual ~R0C47a3c0();
    void release();   // 0x479f30
    char pad0[0xbc - 4];
    R0M4928d0 m_bc;
    char pad1[0x134 - 0xbc - sizeof(R0M4928d0)];
    R0L47a3c0 m_134;
};
// MATCH: golf_clean.exe 0x0047a3c0 ??1R0C47a3c0@@UAE@XZ
R0C47a3c0::~R0C47a3c0()
{
    release();
}

struct R0W47e330 {
    char pad0[0x9c];
    unsigned int m_9c;
    char pad1[0xb0 - 0xa0];
    R0W47e330* m_b0;
    char pad2[0x224 - 0xb4];
    R0W47e330** m_224;
    int m_228;
    int m_22c;
    int visible();   // 0x4801f0
};
extern R0W47e330* g_83aae0;
extern int g_83ab90;
extern R0W47e330* g_839ac0[];
// MATCH: golf_clean.exe 0x0047e330 ?r0_collect47e330@@YAXPAUR0W47e330@@@Z
void r0_collect47e330(R0W47e330* w)
{
    int found = 0;
    int i;
    for (i = 0; i < w->m_22c; i++) {
        if (!(w->m_224[i]->m_9c & 0x20)) {
            if (g_83aae0 && g_83aae0 == w->m_224[i]) {
                g_83ab90 = 0;
                found = 1;
            }
            if (w->m_224[i]->visible())
                r0_collect47e330(w->m_224[i]);
            if (g_83aae0 && found)
                break;
        }
    }
    if (!g_83aae0 || !found) {
        for (i = 0; i < w->m_22c; i++) {
            if (g_83aae0 && g_83aae0 == w->m_224[i]) {
                g_83ab90 = 0;
                found = 1;
            }
            if (w->m_b0 == w && (w->m_224[i]->m_9c & 0x20) && w->m_224[i]->visible())
                r0_collect47e330(w->m_224[i]);
            if (g_83aae0 && found)
                break;
        }
    }
    if (w->visible())
        g_839ac0[g_83ab90++] = w;
}

extern int g_4e44d0, g_4e44d4, g_4e44d8, g_4e44dc, g_83b648;
struct R0P1489cb0 { virtual ~R0P1489cb0(); virtual void p1(); char pad[0x270]; };
struct R0P2489cb0 { virtual void q0(); virtual void q1(); };
struct R0V1489cb0 : R0P1489cb0, R0P2489cb0 { R0V1489cb0(); char pad[0x574 - 0x278]; };   // ctor 0x4804a0
struct R0V2489cb0 { R0V2489cb0(); virtual ~R0V2489cb0(); virtual void r0(); virtual void r1(); char pad[0xb8 - 4]; int m_b8; };   // ctor 0x489150
struct R0C489cb0 : virtual R0V1489cb0, virtual R0V2489cb0 {
    R0C489cb0();
    ~R0C489cb0();
    virtual void c0();
    virtual void p1();
    virtual void q1();
    virtual void r1();
    char m_8[4];
    int m_c, m_10, m_14, m_18, m_1c, m_20, m_24, m_28, m_2c, m_30, m_34, m_38, m_3c, m_40, m_44, m_48, m_4c, m_50, m_54;
};
// MATCH: golf_clean.exe 0x00489cb0 ??0R0C489cb0@@QAE@XZ
R0C489cb0::R0C489cb0()
{
    m_20 = -1;
    m_24 = -1;
    m_40 = 0;
    m_c = 0;
    m_10 = 0;
    m_18 = g_4e44d8;
    m_1c = g_4e44dc;
    m_28 = 0;
    m_2c = 0;
    m_30 = 0;
    m_34 = g_4e44d0;
    m_38 = g_83b648;
    m_3c = g_4e44d4;
    m_44 = 0;
    m_48 = 0;
    m_4c = 0;
    m_50 = 0;
    m_54 = 0;
    m_b8 = 1;
    m_14 = 0;
    memset(m_8, 0, 1);

}

struct R0F480220 {
    int m_0, m_4, m_8, m_c, m_10;
    int textWidth(const char* s);   // 0x4838f0
    int lineHeight() { return m_8 >= 0 ? m_10 + m_8 : m_c; }
};
struct R0Scr480220 { V8(a) V8(b) V8(c) V8(d) V8(e) virtual void f0(); virtual void f1(); virtual int width(); };
extern R0F480220* g_83aac8;
extern R0Scr480220* g_83ad50;
extern void* g_83aac0;
extern char* g_83aac4;
extern RECT g_83a2c8;
extern const char s_004c3f70[];
void __cdecl r0_hide480360();
void __cdecl r0_f47cdb0(RECT* r);
inline int r0_scrW480220() { return g_83ad50 == 0 ? 0 : g_83ad50->width(); }
struct R0C480220 {
    void showTip(char* s, RECT* r);
};
// MATCH: golf_clean.exe 0x00480220 ?showTip@R0C480220@@QAEXPADPAUtagRECT@@@Z
void R0C480220::showTip(char* s, RECT* r)
{
    if (s == 0 || r == 0)
        return;
    {
        r0_hide480360();
        g_83aac0 = this;
        int w = 0;
        int n = 0;

        char* p = s;
        char* c;
        do {
            c = strchr(p, '^');
            if (c)
                *c = 0;
            int lw = g_83aac8->textWidth(p) + g_83aac8->textWidth(s_004c3f70) * 2;

            if (lw > w)
                w = lw;
            if (c)
                *c = '^';
            n++;
            p = c + 1;
        } while (c);
        int h = g_83aac8->lineHeight() * n;
        int y = r->top - h;
        if (y < 0)
            y = r->bottom;
        int x = (r->right - w + r->left) / 2;
        if (x < 0)
            x = 0;
        else if (x + w > r0_scrW480220())
            x = r0_scrW480220() - w;
        g_83a2c8.left = x;
        g_83a2c8.top = y;
        g_83a2c8.right = x + w;
        g_83a2c8.bottom = y + h;
        g_83aac4 = s;
        r0_f47cdb0(&g_83a2c8);
    }
}

struct R0B1_4a0740 : virtual R0V1489cb0, virtual R0V2489cb0 { virtual void p1(); virtual void r1(); R0B1_4a0740(); ~R0B1_4a0740() { cleanup(); } void cleanup(); char pad[0x20 - 4]; };   // ctor 0x49d5a0, cleanup 0x49d690
struct R0B2_4a0740 : virtual R0V1489cb0, virtual R0V2489cb0 { virtual void p1(); R0B2_4a0740(); ~R0B2_4a0740(); char pad[0x18 - 4]; };   // ctor 0x49ebb0
struct R0B3_4a0740 : virtual R0V1489cb0, virtual R0V2489cb0 { virtual void p1(); R0B3_4a0740(); ~R0B3_4a0740(); char pad[0xdc - 4]; };   // ctor 0x4a3110
struct R0B4_4a0740 : virtual R0V1489cb0, virtual R0V2489cb0 { R0B4_4a0740(); ~R0B4_4a0740(); char pad[0x8c - 4]; };   // ctor 0x4a2250
struct R0D4a0740 : R0C489cb0, R0B1_4a0740, R0B2_4a0740, R0B3_4a0740, R0B4_4a0740 {
    R0D4a0740();
    ~R0D4a0740();
    void cleanup();   // 0x4a0890
    virtual void c0();
    virtual void p1();
    virtual void q1();
    virtual void r1();
};
// MATCH: golf_clean.exe 0x004a0740 ??0R0D4a0740@@QAE@XZ
R0D4a0740::R0D4a0740()
{
}
// MATCH: golf_clean.exe 0x00491500 ??1R0D4a0740@@UAE@XZ
R0D4a0740::~R0D4a0740()
{
    cleanup();
}

struct R0I4796a0 { virtual void a0(); virtual void a1(); virtual void a2(); virtual void a3(); virtual char* bits(); virtual void a5(); virtual void a6(); virtual void a7(); V8(b) V8(c) V8(d) V8(e) V8(f) virtual void g0(); virtual void g1(); virtual void g2(); virtual void g3(); virtual void g4(); virtual void g5(); virtual int width(); virtual int height(); virtual int pitch(); virtual int* format(); };
struct R0C4796a0 {
    int m_0;
    R0I4796a0* m_4;
    int width() { return m_4 ? m_4->width() : 0; }
    int height() { return m_4 ? m_4->height() : 0; }
    int pitch() { return m_4 ? m_4->pitch() : 0; }
    int* format() { return m_4 ? m_4->format() : 0; }
    char* pixel(int x, int y);
};
// MATCH: golf_clean.exe 0x004796a0 ?pixel@R0C4796a0@@QAEPADHH@Z
char* R0C4796a0::pixel(int x, int y)
{
    if (x < width() && y < height()) {
        char* p = m_4->bits();
        if (p) {
            switch (*format()) {
            case 8:
                return p + y * pitch() + x;
            case 16:
                return p + (y * pitch() + x) * 2;
            case 24:
                return p + (y * pitch() + x) * 3;
            case 32:
                return p + (y * pitch() + x) * 4;
            }
        }
    }
    return 0;
}

extern int g_822c8c, g_4c2844, g_838200;
extern int g_830164[], g_831164[], g_82c164[], g_82915c[], g_82b160[], g_82415c[], g_82a15c[], g_82815c[];
void __cdecl r0_f462be0(int i);
// MATCH: golf_clean.exe 0x004628d0 ?r0_add4628d0@@YAXHHHHHHHH@Z
void r0_add4628d0(int x, int y, int a, int dx, int dy, int b, int t, int flags)
{
    int d = 16;
    if (g_822c8c > 1000)
        d = 13;
    if (g_822c8c >= 1200)
        d = 10;
    if (flags & 0x100)
        d = 16;
    if (t == 0) {
        g_830164[g_838200] = x - g_4c2844 * dx / 4;
        g_831164[g_838200] = y - g_4c2844 * dy / 4;
    } else if (t > 0) {
        g_830164[g_838200] = x - g_4c2844 * dx * t / d;
        g_831164[g_838200] = y - g_4c2844 * dy * t / d;
    } else {
        g_830164[g_838200] = g_4c2844 * dx * t / (d * 2) + x;
        g_831164[g_838200] = g_4c2844 * dy * t / (d * 2) + y;
    }
    g_82c164[g_838200] = y;
    g_82915c[g_838200] = t;
    g_82b160[g_838200] = x;
    g_82415c[g_838200] = a;
    g_82a15c[g_838200] = b;
    g_82815c[g_838200] = flags;
    r0_f462be0(g_838200);
}

struct R0I485e80 {
    virtual void a0(); virtual void a1(); virtual void a2(); virtual void a3(); virtual void a4(); virtual char* lock(int x, int y); virtual void a6(); virtual void a7();
    virtual void b0(); virtual void unlock(int); virtual void b2(); virtual void b3(); virtual void b4(); virtual void b5(); virtual void b6(); virtual void b7();
    V8(c) V8(d) V8(e) V8(f) virtual void g0(); virtual void g1(); virtual void g2(); virtual RECT* clip(); virtual void g4(); virtual void g5(); virtual void g6(); virtual void g7(); virtual int pitch();
};
struct R0C485e80 {
    int m_0;
    R0I485e80* m_4;
    RECT* clip() { return m_4 ? m_4->clip() : 0; }
    int pitch() { return m_4 ? m_4->pitch() : 0; }
    void vline(int x, int y1, int y2, char c);
};
// MATCH: golf_clean.exe 0x00485e80 ?vline@R0C485e80@@QAEXHHHD@Z
void R0C485e80::vline(int x, int y1, int y2, char c)
{
    char* p;
    if (m_4 && x >= m_4->clip()->left && x < clip()->right && y1 != y2) {
        if (y1 > y2) {
            y1 ^= y2;
            y2 ^= y1;
            y1 ^= y2;
        }
        if (y1 < clip()->bottom && y2 >= clip()->top) {
            if (y1 < clip()->top)
                y1 = clip()->top;
            if (y2 >= clip()->bottom)
                y2 = clip()->bottom - 1;
            if (m_4) {
                p = m_4->lock(x, y1);
                if (p) {
                    x = pitch();
                    __asm {
                        push edi
                        mov ecx, y2
                        sub ecx, y1
                        inc ecx
                        mov ah, c
                        mov ebx, x
                        mov edi, p
                        shr ecx, 1
                        shl ebx, 1
                    L5fc4:
                        mov byte ptr [edi], ah
                        add edi, ebx
                        loop L5fc4
                        pop edi
                    }
                    if (m_4)
                        m_4->unlock(1);
                }
            }
        }
    }
}

extern int g_568d08, g_4c2e08, g_567afc, g_4c2854, g_4c2848;
extern const char s_004c52f4[];
struct R0O40b840 { void f480c80(int); };
extern R0O40b840 g_519a60;
void __cdecl r0_f40d320(int, int, int, int);
void __cdecl r0_f442180(int);
void __cdecl r0_f4315e0();
void __cdecl r0_f40bbf0(int);
void __cdecl r0_f42f7a0();
// MATCH: golf_clean.exe 0x0040b840 ?r0_load40b840@@YAXH@Z
void r0_load40b840(int mode)
{
    char hdr[16];
    char name[100];
    strcpy(name, g_51a068);
    g_568d08 = _open(name, _O_BINARY);
    g_51a068[0] = 0;
    _read(g_568d08, g_51a068, 100);
    if (mode == 2) {
        _close(g_568d08);
        return;
    }
    if (mode == 0) {
        strcpy(g_51a068, s_004c52f4);
        g_4c2e08 = -1;
        r0_f40d320(200, 200, 0x80001284, -2);
        g_519a60.f480c80(mode);
        r0_f442180(mode);
        r0_f4315e0();
    }
    _read(g_568d08, g_51a068, 100);
    if (mode == 3) {
        _close(g_568d08);
        return;
    }
    r0_f40bbf0(1);
    _read(g_568d08, hdr, 8);
    _tell(g_568d08);
    _close(g_568d08);
    if (mode == 0) {
        g_567afc = mode;
        g_4c2854 = -1;
        g_4c2848 = -1;
        r0_f42f7a0();
    }
}

struct R0I496e30 { V8(a) V8(b) V8(c) V8(d) V8(e) V8(f) virtual void g0(); virtual void g1(); virtual void g2(); virtual void g3(); virtual void g4(); virtual void g5(); virtual int width(); virtual int height(); };
struct R0N496e30 { virtual void a0(); virtual void a1(); virtual void a2(); virtual void a3(); virtual void a4(); virtual void a5(); virtual void a6(); virtual void notify(); };
struct R0P496e30 { V8(a) V8(b) V8(c) V8(d) V8(e) V8(f) V8(g) virtual void h0(); virtual void setPos(int id, int pos); char pad[0x84 - 4]; R0N496e30* m_84; };
extern int g_83ff18;
extern R0P496e30* g_83ab2c;
struct R0C496e30 {
    V8(a) V8(b) V8(c) V8(d) V8(e) V8(f) V8(g) V8(h) V8(i) virtual void redraw();
    char pad0[0x130 - 4];
    R0P496e30* m_130;
    char pad1[0x278 - 0x134];
    R0I496e30* m_278;
    char pad2[0x578 - 0x27c];
    int m_578;
    int m_57c;
    int m_580;
    int m_584;
    int m_588;
    int m_58c;
    char pad3[0x59c - 0x590];
    int m_59c;
    int m_5a0;
    char pad4[0x13ac - 0x5a4];
    void (*m_13ac)(int, int);
    int width() { return m_278 ? m_278->width() : 0; }
    int height() { return m_278 ? m_278->height() : 0; }
    int key(int a, int k);
};
// MATCH: golf_clean.exe 0x00496e30 ?key@R0C496e30@@QAEHHH@Z
int R0C496e30::key(int a, int k)
{
    int r = 0;
    g_83ff18 = 1;
    if (width() > height()) {
        switch (k) {
        case 0x23:
            m_58c = m_584;
            r = 1;
            break;
        case 0x24:
            m_58c = m_580;
            r = 1;
            break;
        }
    } else {
        switch (k) {
        case 0x21:
            m_58c -= m_5a0;
            if (m_58c < m_580)
                m_58c = m_580;
            r = 1;
            break;
        case 0x22:
            m_58c += m_5a0;
            if (m_58c > m_584)
                m_58c = m_584;
            r = 1;
            break;
        }
    }
    if (r) {
        g_83ab2c = m_130;
        if (!m_588) {
            m_130->setPos(m_578, m_58c);
            if (m_13ac)
                m_13ac(m_578, m_58c);
        } else {
            m_130->setPos(m_578, m_584 - m_58c + m_580);
            if (m_13ac)
                m_13ac(m_578, m_584 - m_58c + m_580);
        }
        if (m_130->m_84)
            m_130->m_84->notify();
        m_59c = -1;
        redraw();
    }
    return r;
}

struct R0T42f7a0 { char pad0[7]; char m_7; char pad1[4]; unsigned int flags; char pad2[0x30 - 0x10]; };
struct R0E42f7a0 { char c[8]; };
extern R0T42f7a0 g_578370[];
extern char g_5722e8[][50];
extern char g_543018[][50];
extern R0E42f7a0 g_51b770[][50];
extern unsigned char g_5619a0[][0x32];
extern int g_4c2e04;
extern unsigned int g_59e7b8;
int __cdecl r0_f40bfe0(int x, int y, int d, int f);
void __cdecl r0_f42f4b0(int x, int y, int* a, int* b);
int __cdecl r0_f42f630(int x, int y);
int __cdecl r0_f42f6e0(int x, int y, int w);
void r0_edges42f530(int x, int y);
// MATCH: golf_clean.exe 0x0042f7a0 ?r0_rebuild42f7a0@@YAXXZ
void r0_rebuild42f7a0()
{
    int x, y, n;
    int a, b;
    g_4c2e04 = -1;
    memset(g_543018, 0, sizeof(g_543018[0]) * 50);
    memset(g_51b770, 0, sizeof(g_51b770[0]) * 50);
    for (x = 0; x < 50; x++) {
        for (y = 0; y < 50; y++) {
            g_51b770[x][y].c[1] = r0_f40bfe0(x, y, 1, 0);
            g_51b770[x][y].c[3] = r0_f40bfe0(x, y, 3, 0);
            g_51b770[x][y].c[5] = r0_f40bfe0(x, y, 5, 0);
            g_51b770[x][y].c[7] = r0_f40bfe0(x, y, 7, 0);
            if (g_578370[g_5722e8[x][y]].flags & 6) {
                r0_f42f4b0(x, y, &a, &b);
                if (g_578370[g_5722e8[x][y]].flags & 2)
                    g_543018[x][y] = b;
                if (g_578370[g_5722e8[x][y]].flags & 4)
                    g_543018[x][y] = a;
            }
        }
    }
    do {
        n = 0;
        for (x = 0; x < 50; x++) {
            for (y = 0; y < 50; y++) {
                if (g_578370[g_5722e8[x][y]].flags & 1) {
                    if (g_578370[g_5722e8[x][y]].flags & 2)
                        n += r0_f42f630(x, y);
                    if (g_578370[g_5722e8[x][y]].flags & 4)
                        n += r0_f42f6e0(x, y, g_578370[g_5722e8[x][y]].m_7 == 0x10);
                }
            }
        }
    } while (n);
    for (x = 0; x < 50; x++)
        for (y = 0; y < 50; y++)
            r0_edges42f530(x, y);
    g_5619a0[0][0] = g_5619a0[0][0] & ~2 | 8;
    g_59e7b8 &= ~0x40000;
}

struct R0VB4a35e0 { virtual ~R0VB4a35e0(); };
struct R0Item4a35e0 : virtual R0VB4a35e0 { int m_4; };
struct R0Node4a35e0 : virtual R0VB4a35e0 { int m_4; R0Item4a35e0* data; R0Node4a35e0* next; };
struct R0List4a35e0 {
    virtual void v0();
    virtual void onRemove(R0Item4a35e0* d);
    int m_4;
    R0Node4a35e0* head;
    R0Node4a35e0* tmp;
    int count;
    int m_14;
    void clear() {
        if (head) {
            for (int i = 0; i < count; i++) {
                tmp = head->next;
                R0Item4a35e0* d = head->data;
                onRemove(d);
                delete d;
                head->data = 0;
                delete head;
                head = tmp;
            }
            head = 0;
            m_14 = 0;
            count = 0;
        }
        m_14 = 0;
    }
};
struct R0M4a35e0 { void reset(); int m_0; };   // 0x492830
struct R0Q4a35e0 {
    virtual void q0();
    void setA(int, int, int, int);    // 0x476310
    void setB(int, int, int, int);    // 0x476340
    void setC(int, int, int, int);    // 0x476370
    void setD(int, int, int, int);    // 0x4762d0
};
struct R0P4a35e0 { virtual ~R0P4a35e0(); char pad[0x270]; };
struct R0V1_4a35e0 : R0P4a35e0, R0Q4a35e0 { void reset(); char pad[0x574 - 0x278]; };   // reset 0x480610
struct R0V2_4a35e0 { virtual void r0(); void reset(); char pad[0x100]; };              // reset 0x4894b0
extern int g_8409cc;
extern int g_4e449c[4][3];
struct R0C4a35e0 : virtual R0V1_4a35e0, virtual R0V2_4a35e0 {
    int m_4;
    int m_8;
    int m_c;
    R0M4a35e0 m_10;
    char pad0[0x70 - 0x14];
    int m_70;
    int m_74;
    R0List4a35e0 m_78;
    char pad1[0xa8 - 0x90];
    int m_a8[13];
    void reset();
};
// MATCH: golf_clean.exe 0x004a35e0 ?reset@R0C4a35e0@@QAEXXZ
void R0C4a35e0::reset()
{
    R0V1_4a35e0::reset();
    R0V2_4a35e0::reset();
    m_4 = 0;
    m_70 = 1;
    m_74 = 1;
    m_a8[0] = 0;
    m_a8[1] = 0;
    m_a8[2] = 0;
    m_a8[3] = 0;
    m_a8[4] = 0;
    m_a8[5] = 0;
    m_a8[6] = 0;
    m_a8[7] = 0;
    m_a8[8] = 0;
    m_a8[9] = 0;
    m_a8[10] = 0;
    m_a8[11] = 0;
    m_a8[12] = 0;
    m_78.clear();
    m_10.reset();
    m_8 = g_8409cc;
    setA(g_4e449c[0][0], g_4e449c[1][0], g_4e449c[2][0], g_4e449c[3][0]);
    setB(g_4e449c[0][1], g_4e449c[1][1], g_4e449c[2][1], g_4e449c[3][1]);
    setC(g_4e449c[0][2], g_4e449c[1][2], g_4e449c[2][2], g_4e449c[3][2]);
    setD(0, 0, 0, 0);
}

struct R0Scr496fc0 { V8(a) V8(b) V8(c) V8(d) virtual void e0(); virtual void e1(); virtual void e2(); virtual void e3(); virtual void e4(); virtual void e5(); virtual void e6(); virtual void lockMode(int, int, int); };
extern R0Scr496fc0* g_83ad50_r0b;   // 0x83ad50
struct R0I496fc0 { V8(a) V8(b) V8(c) V8(d) V8(e) V8(f) virtual void g0(); virtual void g1(); virtual void g2(); virtual void g3(); virtual void g4(); virtual RECT* bounds(); virtual int width(); virtual int height(); };
struct R0S496fc0;
struct R0Img496fc0 { char pad[0x18]; int m_18; int m_1c; void draw(R0S496fc0* dst, int x, int y, int f); };   // 0x473e60
struct R0S496fc0 {
    int m_0;
    R0I496fc0* m_4;
    RECT* bounds() { return m_4 ? m_4->bounds() : 0; }
    int width() { return m_4 ? m_4->width() : 0; }
    int height() { return m_4 ? m_4->height() : 0; }
    void frame(RECT* r, int c1, int c2);                                         // 0x4795d0
    int tile(R0Img496fc0* img, int ox, int oy, int x, int y, int w, int h);     // 0x476140
};
struct R0W496fc0 { V8(a) V8(b) V8(c) V8(d) V8(e) V8(f) V8(g) V8(h) V8(i) virtual void redraw(); };
struct R0B496fc0 { virtual void b0(); char pad[0x270]; void f4808c0(int); int paint(RECT* r, int a); };   // 0x4808c0, 0x480a10
struct R0C496fc0 : R0B496fc0, R0S496fc0 {
    char pad0[0x57c - 0x27c];
    int m_57c;
    char pad1[0x5c4 - 0x580];
    int m_5c4;
    int m_5c8;
    int m_5cc;
    int m_5d0;
    R0Img496fc0* m_5d4;
    R0Img496fc0* m_5d8;
    char pad2[0x60c - 0x5dc];
    R0W496fc0 m_60c;
    char pad3[0xcdc - 0x610];
    R0W496fc0 m_cdc;
    void getRect(RECT* r);   // 0x4974d0
    void draw();
};
// MATCH: golf_clean.exe 0x00496fc0 ?draw@R0C496fc0@@QAEXXZ
void R0C496fc0::draw()
{
    RECT rc;
    if (g_83ad50_r0b)
        g_83ad50_r0b->lockMode(1, 1, 1);
    m_60c.redraw();
    m_cdc.redraw();
    f4808c0(m_5c4);
    if (m_57c != -1)
        frame(bounds(), (unsigned char)m_57c, (unsigned char)m_57c);
    getRect(&rc);
    paint(&rc, m_5c4);
    if (m_5d8)
        tile(m_5d8, 0, 0, 0, 0, width(), height());
    if (m_5d4)
        m_5d4->draw(this, (rc.right - m_5d4->m_18 - rc.left) / 2 + rc.left, (rc.bottom - m_5d4->m_1c - rc.top) / 2 + rc.top, 0);
    for (int i = 0; i < m_5c8; i++) {
        frame(&rc, m_5cc, m_5d0);
        rc.left++;
        rc.right--;
        rc.top++;
        rc.bottom--;
    }
}

struct R0G4722c0 { int x; int y; char pad[0x100 - 8]; };
struct R0T4722c0 { char pad0[6]; char m_6; char pad1[0x30 - 7]; };
struct R0Rng4722c0 { unsigned short next(int n); };   // 0x45c1e0
extern R0G4722c0 g_5794b8[];
extern R0T4722c0 g_578370_r0b[];
extern char g_5722e8[][50];
extern unsigned short g_53caf0[][50];
extern char g_53a454[][50];
extern R0Rng4722c0 g_822d9c;
extern unsigned int g_822c70, g_543cfc;
extern char g_51a068[];
extern const char s_004e409c[], s_004e4070[];
int __cdecl r0_f40bf60(int x, int y);
void __cdecl r0_f4074a0(int k, int a);
void __cdecl r0_f40cb00(int a, int b, int c);
// MATCH: golf_clean.exe 0x004722c0 ?r0_landmark4722c0@@YAXHH@Z
void r0_landmark4722c0(int g, int who)
{
    int x = g_5794b8[g].x >> 10;
    int y = g_5794b8[g].y >> 10;
    int id;
    while (g_578370_r0b[g_5722e8[x][y]].m_6 != 4 || g_5722e8[x][y] == 0x16 || (short)(g_53caf0[x][y] & 0x320)
) {
        x += g_822d9c.next(3) - 1;
        y += g_822d9c.next(3) - 1;
        if (r0_f40bf60(x, y))
            return;
    }
    switch (g_53a454[who][0]) {
    case 'C':
        id = 0x168;
        break;
    case 'F':
    case 'R':
        id = 0x170;
        break;
    case 'G':
        id = 0x16f;
        break;
    case 'H':
        id = 0x16d;
        break;
    case 'L':
        id = 0x16c;
        break;
    case 'P':
        id = 0x169;
        break;
    case 'X':
        id = 0x171;
        break;
    case 'M':
        id = 0x16b;
        break;
    case 'S':
        id = 0x173;
        break;
    case 'A':
        id = 0x16a;
        break;
    default:
        id = g_822d9c.next(10) + 0x168;
        break;
    }
    g_822c70 |= 1 << (id - 0x168);
    g_543cfc |= 1 << (id - 0x168);
    strcat(g_51a068, s_004e409c);
    r0_f4074a0(id - 0x168, 1);
    strcat(g_51a068, s_004e4070);
    r0_f40cb00(0x80000210, 0, -1);
}

struct R0DP49b7b0 { virtual void a0(); virtual void a1(); virtual void a2(); virtual void a3(); virtual void a4(); virtual void a5(); virtual void a6(); virtual void a7(); virtual void a8(); virtual void a9(); virtual void a10(); virtual void a11(); virtual void a12(); virtual void a13(); virtual void a14(); virtual void a15(); virtual void a16(); virtual void a17(); virtual void a18(); virtual void a19(); virtual void a20(); virtual long __stdcall getName(DWORD id, void* buf, DWORD* size); };
extern R0DP49b7b0* g_8400b0;
extern char g_83ff9c[];
extern char* g_83ffa8;
struct R0Rec49b7b0 { DWORD id; char name[0x20]; DWORD b; };
struct R0Msg49b7b0 { DWORD type; DWORD a; DWORD n; R0Rec49b7b0 recs[1]; };
struct R0Q49b7b0 { int recv(DWORD* msg, int* from, int a, int b); };   // 0x4a4c70
struct R0P49b7b0 { DWORD id; char pad[0x14 - 4]; char m_14; char pad1[0x58 - 0x15]; };
struct R0C49b7b0 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void onData(DWORD a, DWORD b); virtual void onClose();
    DWORD* m_4;
    char pad0[0x148 - 8];
    R0Q49b7b0 m_148;
    char pad1[0x170 - 0x14c];
    R0P49b7b0 m_170[16];
    char pad2[0x77c - 0x170 - 16 * 0x58];
    int m_77c;
    int m_780;
    void drop(DWORD id, int a);                        // 0x49acf0
    void addPlayer(char* name, DWORD id, DWORD b);     // 0x49b690
    void f497d10();                                    // 0x497d10
    void poll();
};
// MATCH: golf_clean.exe 0x0049b7b0 ?poll@R0C49b7b0@@QAEXXZ
void R0C49b7b0::poll()
{
    int from;
    DWORD size;
    if (g_8400b0 && m_4) {
        DWORD* msg = m_4;
        while (m_148.recv(msg, &from, 0, 0)) {
            if (from) {
                if (*(unsigned short*)msg & 0x80)
                    drop(msg[2], 1);
                else if (*(unsigned short*)msg & 0x10) {
                    for (int i = 0; i < *(unsigned char*)(msg + 2); i++)
                        addPlayer(((R0Msg49b7b0*)msg)->recs[i].name, ((R0Msg49b7b0*)msg)->recs[i].id, ((R0Msg49b7b0*)msg)->recs[i].b);

                }
            } else {
                switch (msg[0]) {
                case 5:
                    if (msg[1] == 1)
                        drop(msg[2], 1);
                    break;
                case 3:
                    if (m_77c == m_780 && msg[1] == 1) {
                        size = 0x100;
                        g_8400b0->getName(msg[2], g_83ff9c, &size);
                        addPlayer(g_83ffa8, msg[2], msg[4] ? *(unsigned char*)msg[4] : 0);
                    }
                    break;
                case 0x31:
                    onClose();
                    f497d10();
                    break;
                case 0x103:
                    onData(msg[6], msg[2]);
                    break;
                case 0x102: {
                    int i;
                    for (i = 0; i < 16; i++)
                        if (m_170[i].id == msg[2])
                            break;
                    if (i != 16 && msg[3])
                        m_170[i].m_14 = *(char*)msg[3];
                    break;
                }
                }
            }
        }
    }
}
