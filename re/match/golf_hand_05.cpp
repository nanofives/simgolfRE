// FLAGS golf_clean.exe: /O2 /GX
// golf_clean.exe batch 05 written by hand (release /O2). /GX is needed for the EH frames of 0x4852e0, 0x44b5a0, 0x44b4f0 and 0x44afb0; the other functions compile identically without it. Names are chosen here, not recovered.
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <mmsystem.h>
#pragma warning(disable: 4715)
#pragma vtordisp(off)
struct C47c970;
extern C47c970* g_83ab2c;
struct I47c970 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); };
struct L47c970 { int m_0; C47c970* m_4; };
struct C47c970 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual int v37(int);
    char pad0[0x40 - 4];
    I47c970* m_40;
    char pad1[0x9c - 0x44];
    unsigned int m_9c;
    unsigned int m_a0;
    char pad2[0x138 - 0xa4];
    int m_138;
    L47c970* m_13c;
    int m_140;
    char pad3[0x264 - 0x144];
    int (*m_264)(int);
    C47c970* first() { return m_138 ? m_13c->m_4 : 0; }
    int dispatch(int a);
};
// MATCH: golf_clean.exe 0x0047c970 ?dispatch@C47c970@@QAEHH@Z
int C47c970::dispatch(int a)
{
    int r = 0;
    if ((m_9c & 0x200000) || (m_a0 & 8))
        return 0;
    if (m_140) {
        r = first()->dispatch(a);
        if (r)
            return 1;
    }
    g_83ab2c = this;
    if (m_264)
        r = m_264(a);
    r += v37(a);
    if (m_40)
        m_40->v7();
    return r;
}

struct I404ad0 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual void v54(); virtual int height(); };
struct C404ad0 {
    char pad[4];
    I404ad0* m_4;
    void setQuad(int, int, int, int);          // 0x476310
    int f477580();                             // 0x477580
    void text(const char*, void*, int, int);   // 0x477c30
    void text(const char*, int, int, int);
    int height() { return m_4 ? m_4->height() : 0; }
};
extern C404ad0* g_4c1570;
// MATCH: golf_clean.exe 0x00404ad0 ?text404ad0@@YAXPBDPAXHH@Z
void text404ad0(const char* s, void* p, int y, int q)
{
    g_4c1570->setQuad(q, 1, 0, 1);
    if (g_4c1570->f477580() + y >= g_4c1570->height())
        y = g_4c1570->height() - g_4c1570->f477580() - 1;
    if (s)
        g_4c1570->text(s, p, y, strlen(s));
}

// Global display object at 0x83ad50.
struct Screen { virtual void s0(); virtual void s1(); virtual void s2(); virtual void s3(); virtual void s4(); virtual void s5(); virtual void s6(); virtual void s7(); virtual HWND hwnd(); virtual void s9(); virtual void s10(); virtual void s11(); virtual void s12(); virtual void s13(); virtual void s14(); virtual void s15(); virtual void s16(); virtual void s17(); virtual void s18(); virtual void s19(); virtual void s20(); virtual void s21(); virtual void s22(); virtual void s23(); virtual void s24(); virtual void s25(); virtual void s26(); virtual void s27(); virtual void s28(); virtual void s29(); virtual void s30(); virtual void s31(); virtual void s32(); virtual void s33(); virtual void s34(); virtual void s35(); virtual void s36(); virtual void s37(); virtual void s38(); virtual void setScale(int, int, int); virtual void scale(int*, int*, int*); virtual void s41(); virtual void s42(); virtual void s43(); virtual void s44(); virtual void s45(); virtual void s46(); virtual void s47(); virtual void s48(); virtual void s49(); };
extern Screen* g_83ad50;
extern RECT g_83aa78;
struct C479a80;
extern C479a80* g_83aa98;
struct C479a80 {
    char pad[0x1ac];
    RECT m_1ac;
    void adjust(RECT*);   // 0x47b0d0
    int f4801f0();        // 0x4801f0
    void clip(RECT* r);
};
// MATCH: golf_clean.exe 0x00479a80 ?clip@C479a80@@QAEXPAUtagRECT@@@Z
void C479a80::clip(RECT* r)
{
    if (r)
        g_83aa78 = *r;
    else {
        g_83aa78 = m_1ac;
        adjust(&g_83aa78);
    }
    g_83aa98 = this;
    if (f4801f0()) {
        HWND h = g_83ad50->hwnd();
        if (GetForegroundWindow() == h)
            ClipCursor(&g_83aa78);
    }
}

extern int g_840940;
extern const char s_004e4a70[];
extern const char s_004e4a60[];
extern const char s_004e4a50[];
FILE* open4a00f0(const char*, const char*);
struct C4a0280 {
    int m_0;
    const char* m_4;
    int m_8;
    void log(int a, int b, int c, int d, int e);
};
// MATCH: golf_clean.exe 0x004a0280 ?log@C4a0280@@QAEXHHHHH@Z
void C4a0280::log(int a, int b, int c, int d, int e)
{
    if (m_4 && !m_8 && !g_840940) {
        FILE* f = open4a00f0(m_4, s_004e4a70);
        if (f) {
            if (b)
                fprintf(f, s_004e4a60, a, b, c, d, e);
            else
                fprintf(f, s_004e4a50, a, c, d, e);
            fclose(f);
        }
    }
}

// Second virtual base: vbtable entry +8; first: +4.
extern int g_83ff10;
struct T476e20 { void put(char*, int, int*, int*); };
struct O401d10 {
    char pad[0x1c];
    char* m_1c;
    int m_20;
    int m_24;
    int run(int);   // 0x401d10
    int go(char* s, int p, int k) { m_1c = s; m_20 = 0; m_24 = k; return run(p); }
};
struct V1_48c640 { virtual void a0(); char pad[0x274 - 4]; T476e20 m_274; char pad2[0x290 - 0x275]; int m_290; };
struct V2_48c640 { virtual void b0(); char pad[0x30 - 4]; int m_vb30; char pad2[0xc0 - 0x34]; O401d10 m_c0; };
struct D48c640 : virtual V1_48c640, virtual V2_48c640 {
    virtual void d0();
    char pad[0x30 - 8];
    int m_30;
    int add(char* s, int p, char flag);
};
// MATCH: golf_clean.exe 0x0048c640 ?add@D48c640@@QAEHPADHD@Z
int D48c640::add(char* s, int p, char flag)
{
    if (!s)
        return 3;
    int n = m_vb30 - g_83ff10;
    if (n > 0)
        m_274.put(s, n, 0, 0);
    int k = n ? m_290 : 1;
    m_30 += k;
    if (flag)
        k = 1;
    return m_c0.go(s, p, k);
}

struct B4852e0 {
    B4852e0();               // 0x484150
    virtual ~B4852e0();
    void setKind(int);       // 0x484260
    char pad[0x50 - 4];
    char* m_50;
};
// The ctor copies a 9-byte local char array (2 dwords + 1 byte) from 0x4e43c8; a struct copy reproduces that code without the literal.
struct S9_4852e0 { char c[9]; };
extern const S9_4852e0 s_004e43c8;
struct C4852e0 : B4852e0 {
    C4852e0();
    virtual ~C4852e0();
};
// MATCH: golf_clean.exe 0x004852e0 ??0C4852e0@@QAE@XZ
C4852e0::C4852e0()
{
    S9_4852e0 name = s_004e43c8;
    setKind(6);
    m_50 = new char[10];
    strcpy(m_50, name.c);
}

struct O48df20 {
    char pad[0x1c];
    char* m_1c;
    int m_20;
    int m_24;
    int run(int);                          // 0x401d10
    int post(char*, int, int, int);        // 0x4a0600
    int go(char* s, int lvl) { m_1c = s; m_20 = lvl; m_24 = 0; return run(0); }
};
struct C48df20 {
    char pad[0x1410];
    O48df20 m_1410;
    int say(char* s, char now);
};
// MATCH: golf_clean.exe 0x0048df20 ?say@C48df20@@QAEHPADD@Z
int C48df20::say(char* s, char now)
{
    int lvl = 0;
    if (!s)
        return 3;
    if (*s == '^') {
        if (s[1] != 0) {
            if (s[1] != '^') {
                lvl = 1;
                s++;
            } else if (s[2] == '^') {
                lvl = 2;
                s += 3;
            } else {
                lvl = 2;
                s += 2;
            }
        } else {
            lvl = 3;
            s++;
        }
    }

    int r = 0;
    if (now) {
        if (m_1410.post(s, 0, lvl, 0))
            r = 1;
    } else {
        if (m_1410.go(s, lvl))
            r = 1;
    }
    return r;
}

struct T484940 { bool a(unsigned); bool b(unsigned); void f(); };   // 0x487770, 0x4877a0, 0x487460
extern T484940 g_83ad80;
struct I484940 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual int v7(); };
struct C484940 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(int);
    virtual void w17(); virtual void w18(); virtual void w19(); virtual void w20(); virtual void w21(); virtual void w22(); virtual void w23(); virtual void w24(); virtual void w25(); virtual void w26(); virtual void w27(); virtual void w28(); virtual void w29(); virtual void w30(); virtual void w31(); virtual void v32();
    int m_4;
    char pad[0x3c - 8];
    unsigned m_3c;
    I484940* m_40;
    char pad2[0x58 - 0x44];
    unsigned m_58;
    char pad3[0x68 - 0x5c];
    DWORD m_68;
    void f484d60();
    int play();
};
// MATCH: golf_clean.exe 0x00484940 ?play@C484940@@QAEHXZ
int C484940::play()
{
    int r = 0;
    if (m_3c < 0x10) {
        if (g_83ad80.a(m_3c) || g_83ad80.b(m_3c))
            return 0x14;
        if (m_58 & 0x10)
            v16(m_4);
    }
    if (m_40)
        r = m_40->v7();
    else if (m_58 & 0x10) {
        f484d60();
        if (m_40)
            r = m_40->v7();
    } else
        r = 0x14;
    if (m_58 & 0x10) {
        m_68 = timeGetTime();
        v32();
        m_40 = 0;
    }
    return r;
}

struct VB4897f0 { virtual ~VB4897f0(); };
struct Item4897f0 : virtual VB4897f0 { };
struct Node4897f0 : virtual VB4897f0 { int m_4; Item4897f0* m_item; Node4897f0* m_next; };
struct List4897f0 {
    virtual void v0();
    virtual void onRemove(Item4897f0*);
    int m_4;
    Node4897f0* m_head;
    Node4897f0* m_next;
    int m_count;
    int m_14;
    int m_18;
    void clear()
    {
        if (m_head) {
            for (int i = 0; i < m_count; i++) {
                m_next = m_head->m_next;
                Item4897f0* it = m_head->m_item;
                onRemove(it);
                delete it;
                m_head->m_item = 0;
                delete m_head;
                m_head = m_next;
            }
            m_head = 0;
            m_14 = 0;
            m_count = 0;
        }
    }
    void reset(int v) { clear(); m_14 = 0; m_18 = v; }
};
struct C4897f0 {
    char pad[0x1c];
    int m_1c;
    int m_20;
    char pad2[0xc0 - 0x24];
    List4897f0 m_list;
    void f4894b0();
    int reset(int v);
};
// MATCH: golf_clean.exe 0x004897f0 ?reset@C4897f0@@QAEHH@Z
int C4897f0::reset(int v)
{
    f4894b0();
    m_20 = -1;
    m_1c = v;
    m_list.reset(v);
    return 0;
}

struct T44b5a0 { T44b5a0(); ~T44b5a0(); char pad[0x2c]; };   // 0x473ab0, 0x4041f0
struct C44b5a0 {
    T44b5a0 m_0;
    T44b5a0 m_2c[4];
    T44b5a0 m_dc[12];
    T44b5a0 m_2ec;
    T44b5a0 m_318[21];
    C44b5a0();
};
// MATCH: golf_clean.exe 0x0044b5a0 ??0C44b5a0@@QAE@XZ
C44b5a0::C44b5a0()
{
}

struct H474650 { void* alloc(unsigned); };   // 0x474860
struct O474650 { int m_0; int m_4; char* m_8; int m_c; };
struct C474650 {
    char pad[0x18];
    H474650* m_18;
    int m_1c;
    char* m_20;
    int m_24;
    int get(O474650* o);
};
// MATCH: golf_clean.exe 0x00474650 ?get@C474650@@QAEHPAUO474650@@@Z
int C474650::get(O474650* o)
{
    o->m_4 = m_1c;
    if (!m_20) {
        o->m_8 = 0;
    } else {
        if (m_18)
            o->m_8 = (char*)m_18->alloc(strlen(m_20) + 1);
        else
            o->m_8 = (char*)malloc(strlen(m_20) + 1);
        if (!o->m_8)
            return 4;
        *o->m_8 = 0;
        strcat(o->m_8, m_20);
    }
    o->m_c = m_24;
    return 0;

}

struct C47b8f0 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual void v54(); virtual void v55(); virtual void v56(); virtual void v57(); virtual void v58(); virtual void v59(); virtual void v60(); virtual void v61(); virtual void v62(); virtual void v63(); virtual void v64(); virtual void v65(); virtual void v66(); virtual void v67(); virtual void v68(); virtual void v69(); virtual void v70(); virtual void v71(); virtual void v72(); virtual void v73(); virtual void v74(); virtual void v75(); virtual void v76(); virtual void v77(int); virtual void v78(); virtual void v79(int, int);
    char pad[0x14c - 4];
    char* m_14c;
    int f4801f0();   // 0x4801f0
    void setText(const char* s);
};
// MATCH: golf_clean.exe 0x0047b8f0 ?setText@C47b8f0@@QAEXPBD@Z
void C47b8f0::setText(const char* s)
{
    if (m_14c) {
        free(m_14c);
        m_14c = 0;
    }
    if (s) {
        m_14c = (char*)malloc(strlen(s) + 1);
        if (!m_14c)
            return;
        *m_14c = 0;
        strcat(m_14c, s);
    }
    if (f4801f0()) {
        v77(0);
        v79(0, 0);
    }
}

void free4a4ffc(void*);
struct O4876c0 { char pad[0x3c]; unsigned m_3c; };
struct N4876c0 { N4876c0* m_prev; N4876c0* m_next; O4876c0* m_data; };
struct L4876c0 { int m_0; N4876c0* m_head; N4876c0* m_tail; N4876c0* m_cur; int m_count; char pad[0x1c - 0x14]; };
struct C4876c0 {
    char pad[0x2c];
    L4876c0 m_lists[16];
    int remove(O4876c0* o);
};
// MATCH: golf_clean.exe 0x004876c0 ?remove@C4876c0@@QAEHPAUO4876c0@@@Z
int C4876c0::remove(O4876c0* o)
{
    if (!o || o->m_3c >= 0x10)
        return 10;
    L4876c0* l = &m_lists[o->m_3c];
    N4876c0* n;
    for (n = l->m_head; n; n = n->m_next)
        if (n->m_data == o)
            break;
    if (n) {
        if (n->m_prev)
            n->m_prev->m_next = n->m_next;
        else
            l->m_head = n->m_next;
        N4876c0* next = n->m_next;
        if (next) {
            next->m_prev = n->m_prev;
            l->m_cur = next;
        } else {
            l->m_tail = n->m_prev;
            l->m_cur = 0;
        }
        free4a4ffc(n);
        l->m_count--;
    }
    o->m_3c = 0x10;
    return 0;
}

struct O483f10 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32();
    char pad[0x40 - 4];
    void* m_40;
    unsigned m_44;
    char pad2[0x50 - 0x48];
    void* m_50;
};
struct T487ac0 { void f(); };
struct T487c40 { void f(); };
extern int g_83afc4;
extern void (*g_83af78)();
extern O483f10* g_83af68;
extern T487ac0 g_83ad58;
extern T487c40 g_83af98;
void f484110(void*);
void f4843e0(O483f10*);
void f484060();
// MATCH: golf_clean.exe 0x00483f10 ?shutdown483f10@@YAHXZ
int shutdown483f10()
{
    if (!g_83afc4)
        return 1;
    g_83af78();
    while (g_83af68) {
        O483f10* p = g_83af68;
        if (p->m_40) {
            f484110(p->m_40);
            p->m_40 = 0;
        }
        if (p->m_50) {
            free4a4ffc(p->m_50);
            p->m_50 = 0;
        }
        if ((p->m_44 & 2) && (p->m_44 & 0x200)) {
            f4843e0(p);
            f484110(p);
        } else
            p->v32();
    }
    g_83ad58.f();
    g_83ad80.f();
    g_83af98.f();
    f484060();
    return 0;
}

struct T44b4f0 { T44b4f0(); ~T44b4f0() { release(); } virtual void v0(); void release(); char pad[0x28]; };   // release 0x473ae0
struct C44b4f0 {
    T44b4f0 m_0;
    T44b4f0 m_2c[4];
    T44b4f0 m_dc[12];
    T44b4f0 m_2ec;
    T44b4f0 m_318[21];
    ~C44b4f0();
};
// MATCH: golf_clean.exe 0x0044b4f0 ??1C44b4f0@@QAE@XZ
C44b4f0::~C44b4f0()
{
}

struct VB49bec0 { virtual ~VB49bec0(); };
struct I49bec0 { virtual ~I49bec0(); };
struct N49bec0 : virtual VB49bec0 { int m_key; I49bec0* m_item; N49bec0* m_next; N49bec0* m_prev; };
struct L49bec0 {
    virtual void v0();
    virtual void onRemove(I49bec0*);
    int m_4;
    N49bec0* m_head;
    N49bec0* m_cur;
    int m_count;
    int m_14;
    void remove(int key);
};
// MATCH: golf_clean.exe 0x0049bec0 ?remove@L49bec0@@QAEXH@Z
void L49bec0::remove(int key)
{
    if (m_head) {
        for (int i = 0; i < m_count; i++) {
            if (m_cur->m_key == key) {
                m_cur->m_next->m_prev = m_cur->m_prev;
                m_cur->m_prev->m_next = m_cur->m_next;
                N49bec0* n = m_cur;
                if (n == m_head)
                    m_head = n->m_next;
                m_cur = n->m_next;
                I49bec0* it = n->m_item;
                onRemove(it);
                delete it;
                n->m_item = 0;
                delete n;
                m_count--;
                break;
            }
            m_cur = m_cur->m_next;
        }
        if (m_count == 0)
            m_head = 0;
        m_14 = m_count - 1;
    }
}

extern List4897f0 g_83ac88;
// MATCH: golf_clean.exe 0x00483340 ?clear483340@@YAXXZ
void clear483340()
{
    g_83ac88.clear();
    if (g_83ad50)
        g_83ad50->s49();
}


struct D4741b0 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual int blit(void*, int, int, int, void*); };
struct S4741b0 { int m_0; void* m_4; };
struct C4741b0 {
    int m_0;
    D4741b0* m_4;
    char pad[0x20 - 8];
    int m_20;
    int m_24;
    int draw(S4741b0* s, int x, int y, int flags, S4741b0* mask);
};
// MATCH: golf_clean.exe 0x004741b0 ?draw@C4741b0@@QAEHPAUS4741b0@@HHH0@Z
int C4741b0::draw(S4741b0* s, int x, int y, int flags, S4741b0* mask)
{
    if (!s)
        return 0x10;
    if (!m_4 || !s->m_4)
        return 7;
    int a, b, c;
    g_83ad50->scale(&a, &b, &c);
    int px = m_20 * a / c;
    int py = m_24 * b / c;
    return m_4->blit(s->m_4, px + x, py + y, flags, mask ? mask->m_4 : 0);
}

struct P4830e0 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual int setEntries(unsigned char*, int, int); };
struct C4830e0 {
    virtual void create();
    P4830e0* m_4;
    int setPalette(BITMAPINFO* bi, int first, int count);
};
// MATCH: golf_clean.exe 0x004830e0 ?setPalette@C4830e0@@QAEHPAUtagBITMAPINFO@@HH@Z
int C4830e0::setPalette(BITMAPINFO* bi, int first, int count)
{
    unsigned char buf[0x300];
    if (!bi)
        return 3;
    if (!m_4)
        create();
    for (int i = first; i < first + count; i++) {
        buf[(i - first) * 3] = bi->bmiColors[i].rgbRed;
        buf[(i - first) * 3 + 1] = bi->bmiColors[i].rgbGreen;
        buf[(i - first) * 3 + 2] = bi->bmiColors[i].rgbBlue;
    }
    return m_4->setEntries(buf, first, count);
}

extern int g_83b654;
struct I48c560 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); };
struct O48c560 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual void v54(); virtual void v55(); virtual void v56(); virtual void v57(); virtual void v58(); virtual void v59(); virtual void v60(); virtual void v61(); virtual void v62(); virtual void v63(); virtual void v64(); virtual void v65(int, int); virtual void v66(int); char pad[0x98 - 4]; I48c560* m_98; };
struct V1_48c560 { virtual void a0(); virtual void f(int, int); char pad[0x130 - 4]; O48c560* m_130; };
struct V2_48c560 { int get(); };   // 0x489950
struct D48c560 : virtual V1_48c560, virtual V2_48c560 {
    char pad[0x50 - 4];
    void (*m_50)(int);
    int m_54;
    virtual void f(int, int);
};
// MATCH: golf_clean.exe 0x0048c560 ?f@D48c560@@UAEXHH@Z
void D48c560::f(int, int)
{
    g_83ab2c = (C47c970*)m_130;
    if (m_50)
        m_50(get());
    O48c560* o = m_130;
    if (o) {
        o->v66(get());
        o = m_130;
        o->v65(get(), g_83b654);
        I48c560* m = m_130->m_98;
        if (m)
            m->v7();
    }
}

extern signed char g_543018[][50];
extern char g_5722e8[][50];
extern int g_4c2878[8];
extern int g_4c2898[8];
int blocked40bf60(int, int);
// MATCH: golf_clean.exe 0x0042f630 ?relax42f630@@YAHHH@Z
int relax42f630(int x, int y)
{
    int changed = 0;
    int best = g_543018[x][y];
    for (int i = 0; i < 8; i += 2) {
        int nx = x + g_4c2878[i];
        int ny = y + g_4c2898[i];
        if (!blocked40bf60(nx, ny) && g_5722e8[nx][ny] == g_5722e8[x][y] && g_543018[nx][ny] < best) {
            best = g_543018[nx][ny];
            g_543018[x][y] = best;
            changed = 1;
        }
    }
    return changed;
}

struct Tile449470 { char pad[0x24]; int m_type; char pad2[0x248 - 0x28]; };
struct Terrain449470 {
    char pad[0x14];
    int m_width;
    int m_height;
    char pad2[0x3a4 - 0x1c];
    Tile449470 m_tiles[1];
    Tile449470* tileAt(int x, int y)
    {
        if (x >= m_width || x < 0 || y >= m_height || y < 0)
            return 0;
        return &m_tiles[x + y * m_width];
    }
    int getVariation(Tile449470*);           // Terrain.dll import
    void setType(Tile449470*, int, int);     // Terrain.dll import
};
extern Terrain449470* g_820ed0;
extern char g_820f2a;
void f483bd0();
int type4492d0(int, int);
int var4492f0(int, int);
// MATCH: golf_clean.exe 0x00449470 ?syncTiles449470@@YAXXZ
void syncTiles449470()
{
    for (int x = 49; x >= 0; x--) {
        f483bd0();
        for (int y = 49; y >= 0; y--) {
            Tile449470* t = g_820ed0->tileAt(x, y);
            int type = t->m_type;
            if (type4492d0(x, y) != type || var4492f0(x, y) != g_820ed0->getVariation(t))
                g_820ed0->setType(t, type4492d0(x, y), var4492f0(x, y));
        }
    }
    g_820f2a = 1;
}

struct T44afb0 { ~T44afb0() { release(); } virtual void v0(); void release(); char pad[0x28]; };   // release 0x473ae0
struct C44afb0 {
    T44afb0 m_0, m_2c, m_58, m_84, m_b0, m_dc;
    ~C44afb0();
};
// MATCH: golf_clean.exe 0x0044afb0 ??1C44afb0@@QAE@XZ
C44afb0::~C44afb0()
{
}

struct E49cf50 { int m_key; unsigned char m_flags; char pad[0x10 - 5]; };
struct C49cf50 {
    char pad[0xbc];
    int m_bc;
    char pad2[0x1a4 - 0xc0];
    int m_key;
    char pad3[0x938 - 0x1a8];
    int m_count;
    char pad4[0x948 - 0x93c];
    E49cf50* m_list;
    E49cf50* m_sel;
    int select(int n);
};
// MATCH: golf_clean.exe 0x0049cf50 ?select@C49cf50@@QAEHH@Z
int C49cf50::select(int n)
{
    if (m_bc && m_list && n < m_count && n >= 0) {
        int key = m_key;
        E49cf50* e = m_list;
        while (e->m_key != key)
            e++;
        for (int i = n; i > 0; i--) {
            e++;
            while (e->m_key != key)
                e++;
        }
        while (n < m_count - 1 && !(e->m_flags & 0x10)) {
            e++;
            while (e->m_key != key)
                e++;
            n++;
        }
        if (n == m_count - 1) {
            if (e->m_flags & 0x10) {
                m_sel = e;
                return n;
            }
        } else if (n != m_count) {
            m_sel = e;
            return n;
        }
    }
    return -1;
}

int scale404970(int);
// MATCH: golf_clean.exe 0x00404a20 ?text404a20@@YAXPBDHHH@Z
void text404a20(const char* s, int x, int y, int q)
{
    g_4c1570->setQuad(q, -1, 2, 2);
    if (g_4c1570->f477580() + y >= g_4c1570->height())
        y = g_4c1570->height() - g_4c1570->f477580() - 1;
    int sy = scale404970(y);
    int sx = scale404970(x);
    if (s)
        g_4c1570->text(s, sx, sy, strlen(s));
}

extern char g_51a068[];
extern const char s_004c4714[];
extern const char s_004c46bc[];
extern const char s_004d6098[];
struct E4065c0 { int m_0; char pad[0x30 - 4]; };
extern E4065c0 g_4c15a0[22];
extern int g_4c2e08;
extern short g_4c2c9c;
void f40d320(int, int, int, int);
int f45f0f0(const char*, int, int, int, int);
// MATCH: golf_clean.exe 0x004065c0 ?show4065c0@@YAXXZ
void show4065c0()
{
    strcpy(g_51a068, s_004c4714);
    strcat(g_51a068, s_004c46bc);
    int h = 10;
    for (int i = 0; i < 22; i++)
        if (g_4c15a0[i].m_0)
            h += 3;
    g_4c2e08 = -1;
    f40d320(200, 0x172, 0x80007fff, -2);
    g_4c2c9c = f45f0f0(s_004d6098, 0, h, -1, 200);
}

extern const char s_004c7924[];
FILE* open4a00f0(const char*, const char*);
struct C4a01d0 {
    int m_0;
    char* m_4;
    int setPath(const char* s);
};
// MATCH: golf_clean.exe 0x004a01d0 ?setPath@C4a01d0@@QAEHPBD@Z
int C4a01d0::setPath(const char* s)
{
    if (!s)
        return 0x10;
    if (m_4) {
        free(m_4);
        m_4 = 0;
    }
    m_4 = (char*)malloc(strlen(s) + 1);
    if (!m_4)
        return 4;
    *m_4 = 0;
    strcat(m_4, s);
    FILE* f = open4a00f0(s, s_004c7924);
    if (!f)
        return 6;
    fclose(f);
    return 0;
}

extern const char s_004c7c34[];
extern const char s_004c7c30[];
extern const char* g_4c2ca8[6];
extern int g_5a5a00;
int f46d6e0(int, int, int, int, int);
// MATCH: golf_clean.exe 0x00432560 ?show432560@@YAXXZ
void show432560()
{
    strcpy(g_51a068, s_004c7c34);
    for (int i = 0; i < 6; i++) {
        strcat(g_51a068, g_4c2ca8[i]);
        strcat(g_51a068, s_004c7c30);
    }
    g_5a5a00 = f46d6e0(400, 200, g_5a5a00, 0, 0);
}

#define MIN478080(a, b) ((a) < (b) ? (a) : (b))
struct F478080 { int m_0; int m_4; };
struct C478080 {
    char pad[0x5c];
    F478080* m_5c;
    int width(const char*, int);                  // 0x477280
    int draw(const char*, int, int, int);         // 0x4775b0
    int drawRight(const char* s, int x, int y, int w, int len);
};
// MATCH: golf_clean.exe 0x00478080 ?drawRight@C478080@@QAEHPBDHHHH@Z
int C478080::drawRight(const char* s, int x, int y, int w, int len)
{
    if (s == 0)
        return x;
    if (m_5c == 0 || m_5c->m_4 == 0)
        return 3;
    int n = MIN478080((int)strlen(s), len);
    if (n >= 0) {
        len = MIN478080((int)strlen(s), len);
        if (len == 0)
            return x;
        x += w - width(s, len);
        return draw(s, x, y, len);
    }
    return x;

}

struct C474030 {
    char pad[0x20];
    int m_20;
    int m_24;
    int draw(int, int, int, int, int);   // 0x4740f0
    int drawScaled(int a, int b, int c, int d, int sx, int sy, int div, int e);
};
// MATCH: golf_clean.exe 0x00474030 ?drawScaled@C474030@@QAEHHHHHHHHH@Z
int C474030::drawScaled(int a, int b, int c, int d, int sx, int sy, int div, int e)
{
    int nx, ny, nd;
    g_83ad50->scale(&nx, &ny, &nd);
    g_83ad50->setScale(sx, sy, div);
    int o20 = m_20;
    int o24 = m_24;
    m_20 = o20 * sx / div;
    m_24 = o24 * sy / div;
    int r = draw(a, b, c, d, e);
    m_20 = o20;
    m_24 = o24;
    g_83ad50->setScale(nx, ny, nd);
    return r;
}


struct TT42ee80 { char m_0, m_1; signed char m_2, m_3; char m_4, m_5, m_6; char pad[0x30 - 7]; };
extern TT42ee80 g_578370[];
extern char g_5a34e0;
// MATCH: golf_clean.exe 0x0042ee80 ?cost42ee80@@YAHHH@Z
int cost42ee80(int x, int y)
{
    if (blocked40bf60(x, y))
        return 0;
    int t = g_5722e8[x][y];
    if (t == 4)
        return 12;
    if (t == 0x14)
        return 0;
    if (t == 0x15)
        return -16;
    if (g_578370[t].m_2 <= 0)
        return -8;
    if (t == 0x11)
        return 32;
    if (g_5a34e0 == 1 && t == 0x12)
        return 16;
    if (g_578370[t].m_6 == 13)
        return g_578370[t].m_3 * 5 / 2;
    return g_578370[t].m_3 * g_578370[t].m_2 / 2;
}

struct It4941e0 { int m_0; int m_4; };
struct N4941e0 { int m_0; int m_key; It4941e0* m_item; N4941e0* m_next; };
struct L4941e0 {
    virtual void v0();
    int m_4;
    N4941e0* m_head;
    N4941e0* m_cur;
    int m_count;
    int m_index;
    int curKey() { return m_head ? m_cur->m_key : 0; }
    It4941e0* item() { return m_head ? m_cur->m_item : 0; }
    void find(int key)
    {
        if (m_head) {
            m_index = 0;
            m_cur = m_head;
            for (int i = 0; i < m_count; i++) {
                if (m_cur->m_key == key)
                    break;
                m_index++;
                m_cur = m_cur->m_next;
            }
        }
    }
};
struct VA4941e0 { int m_0; };
struct VB4941e0 { char pad[0xc0]; L4941e0 m_list; };
struct M4941e0 : virtual VA4941e0, virtual VB4941e0 { };
struct C4941e0 {
    int m_0;
    unsigned m_4;
    char pad[0x1488 - 8];
    M4941e0 m_1488;
    char pad2[0x2d98 - 0x1488 - sizeof(M4941e0)];
    M4941e0 m_2d98;
    int current();
};
// MATCH: golf_clean.exe 0x004941e0 ?current@C4941e0@@QAEHXZ
int C4941e0::current()
{
    if (m_4 & 4) {
        m_1488.m_list.find(m_1488.m_list.curKey());
        return m_1488.m_list.item()->m_4;
    }
    return m_2d98.m_list.item()->m_4;
}

extern int g_840948[3];
extern int g_840954[3];
extern int g_840960[3];
extern int g_84096c[3];
extern int g_840978[3];
extern int g_840984[3];
extern int g_840990[3];
extern int g_84099c[3];
extern int g_8409a8[3];
extern int g_8409b4[3];
struct M4a14c0 { void reset(); };   // 0x486f10
struct C4a14c0 {
    char pad[0x574];
    int m_574;
    int m_578;
    M4a14c0 m_57c;
    char pad2[0x630 - 0x57d];
    int m_630;
    int m_634;
    int m_638[3];
    int m_644[3];
    int m_650[3];
    char pad3[0x664 - 0x65c];
    int m_664[9][3];
    void f4886d0();
    void reset();
};
// MATCH: golf_clean.exe 0x004a14c0 ?reset@C4a14c0@@QAEXXZ
void C4a14c0::reset()
{
    m_630 = 1000;
    m_57c.reset();
    m_578 = 0;
    m_574 = 0;
    m_634 = -1;
    f4886d0();
    for (int i = 0; i < 3; i++) {
        m_644[i] = g_840948[i];
        m_638[i] = -1;
        m_664[0][i] = g_840954[i];
        m_664[1][i] = g_840960[i];
        m_664[2][i] = g_84096c[i];
        m_664[3][i] = g_840978[i];
        m_664[4][i] = g_840984[i];
        m_664[5][i] = g_840990[i];
        m_664[6][i] = g_84099c[i];
        m_664[7][i] = g_8409a8[i];
        m_664[8][i] = g_8409b4[i];
        m_650[i] = 0;
    }
}
