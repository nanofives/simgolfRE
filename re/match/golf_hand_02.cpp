// FLAGS golf_clean.exe: /O2 /GX
// golf_clean.exe batch 02 written by hand (release /O2; /GX for the EH frames of ctors/dtors). Names are chosen here.
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <mmsystem.h>
struct Q475 { int l, t, r, b; };
struct V475 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual int fill(Q475* r, int c); };
struct C475da0 {
    int m_0;
    V475* m_4;
    int fill(int x, int y, int x2, int y2, int c);
};
// MATCH: golf_clean.exe 0x00475da0 ?fill@C475da0@@QAEHHHHHH@Z
int C475da0::fill(int x, int y, int x2, int y2, int c)
{
    Q475 r;
    if (!m_4)
        return 7;
    r.l = x;
    r.t = y;
    r.r = x2;
    r.b = y;
    return m_4->fill(&r, c);
}
// MATCH: golf_clean.exe 0x00476dd0 ?scanToken476dd0@@YAPADPADPAH@Z
char* __cdecl scanToken476dd0(char* p, int* n)
{
    __asm {
        mov eax, p
        mov ebx, n
        mov edx, [ebx]
        and edx, edx
        je done
    lp:
        mov cl, [eax]
        cmp cl, 0x7b
        je stop
        cmp cl, 0x7d
        je stop
        cmp cl, 0x5b
        je stop
        cmp cl, 0x5d
        je stop
        cmp cl, 0x24
        je stop
        cmp cl, 0x3d
        je stop
        cmp cl, 0x5e
        je stop
        inc eax
        dec edx
        jne lp
    stop:
        mov [ebx], edx
        mov p, eax
        jmp done
    done:
    }
    return p;
}
struct V4a2 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual void v54(); virtual void v55(); virtual void v56(); virtual void v57(); virtual void v58(); virtual void v59(); virtual void show(int); };
struct Outer4a2 { V4a2* m_items[10]; };
struct C4a2fa0 { void show(int v); Outer4a2* outer() { return (Outer4a2*)((char*)this - 0x88); } };
// MATCH: golf_clean.exe 0x004a2fa0 ?show@C4a2fa0@@QAEXH@Z
void C4a2fa0::show(int v)
{
    if (v) {
        if (outer()->m_items[0])
            outer()->m_items[0]->show(v);
    } else {
        for (int i = 0; i < 10; i++)
            if (outer()->m_items[i])
                outer()->m_items[i]->show(0);
    }
}
struct V484 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual int flags(); };
struct C484ff0 {
    char pad[0x30];
    int m_30;
    char pad2[0xc];
    V484* m_40;
    char pad3[0x14];
    unsigned m_58;
    int flags();
};
// MATCH: golf_clean.exe 0x00484ff0 ?flags@C484ff0@@QAEHXZ
int C484ff0::flags()
{
    int r = 0;
    if (m_40)
        r = m_40->flags();
    if (m_30)
        r |= 2;
    if (m_58 & 1)
        r |= 1;
    if (m_58 & 8)
        r |= 0x40;
    if (m_58 & 2)
        r |= 4;
    if (m_58 & 4)
        r |= 0x10;
    if (m_58 & 0x10)
        r |= 0x80;
    if (m_58 & 0x20)
        r |= 0x100;
    return r;
}
struct S48d { char pad[0x1c]; int m_1c, m_20, m_24; int f401d10(int); int begin(int v) { m_1c = v; m_20 = 0; m_24 = 0; return f401d10(0); } };
struct C48dfc0 {
    char pad[0x137c];
    int m_137c;
    char pad2[0x13e0 - 0x1380];
    S48d m_13e0;
    int start(int v);
};
// MATCH: golf_clean.exe 0x0048dfc0 ?start@C48dfc0@@QAEHH@Z
int C48dfc0::start(int v)
{
    if (!v)
        return 3;
    if (m_13e0.begin(v))
        return 1;
    m_137c++;
    return 0;
}
struct V482 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void* lock(); virtual void unlock(int); virtual void v10(); virtual void v11(); virtual int height(); virtual int pitch(); };
struct A482 { int m_0; V482* m_4; };
// MATCH: golf_clean.exe 0x00482940 ?clear482940@@YGHHHPAUA482@@@Z
int __stdcall clear482940(int a, int b, A482* s)
{
    void* p = s->m_4->lock();
    V482* o = s->m_4;
    memset(p, 0, o->pitch() * o->height());
    s->m_4->unlock(1);
    return 0;
}
struct T476 { int a, b, c; };
struct P476 { int m_0; int m_4; };
struct C476650 {
    char pad[0x5c];
    T476 m_5c;
    void draw(P476* p, int a, int b, int c, int d);
    void f477c30(int a, int b, int c, int d);
};
// MATCH: golf_clean.exe 0x00476650 ?draw@C476650@@QAEXPAUP476@@HHHH@Z
void C476650::draw(P476* p, int a, int b, int c, int d)
{
    T476 saved = m_5c;
    if (p && p->m_4) {
        m_5c.a = (int)p;
        m_5c.b = 0;
        m_5c.c = 0;
    }
    f477c30(a, b, c, d);
    m_5c = saved;
}
struct V482b { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void* lock(); virtual void unlock(int); virtual void v10(); virtual void v11(); virtual int height(); virtual int pitch(); };
struct A482b { int m_0; V482b* m_4; };
// MATCH: golf_clean.exe 0x00482a80 ?copy482a80@@YGHPADHPAUA482b@@@Z
int __stdcall copy482a80(char* src, int b, A482b* s)
{
    void* p = s->m_4->lock();
    V482b* o = s->m_4;
    memcpy(p, src + 6, o->pitch() * o->height());
    s->m_4->unlock(1);
    return 0;
}
struct B44b6 { B44b6(); ~B44b6(); char pad[0x2c]; };
struct C44b6f0 : B44b6 {
    C44b6f0();
    B44b6 m_2c[4];
};
// MATCH: golf_clean.exe 0x0044b6f0 ??0C44b6f0@@QAE@XZ
C44b6f0::C44b6f0()
{
}
struct A492 { int m_x[7]; };
struct B486 { B486(); ~B486(); char pad[0x3c]; };
struct C492850 {
    C492850();
    ~C492850();
    virtual void v0();
    void init();       // 0x492800
    void release();    // 0x492830
    A492 m_4;
    B486 m_20;
    int m_5c;
};
// MATCH: golf_clean.exe 0x00492850 ??0C492850@@QAE@XZ
C492850::C492850()
{
    m_5c = 1000;
    init();
}
// MATCH: golf_clean.exe 0x004928d0 ??1C492850@@QAE@XZ
C492850::~C492850()
{
    release();
}
struct Node4a4d { int m_0, m_4; void* m_buf; int m_c; Node4a4d* m_next; };
struct List4a4d {
    int m_0;
    Node4a4d* m_head;
    int m_8;
    int m_c;
    void clear();
};
// MATCH: golf_clean.exe 0x004a4d50 ?clear@List4a4d@@QAEXXZ
void List4a4d::clear()
{
    while (m_head) {
        Node4a4d* next = m_head->m_next;
        if (m_head->m_buf) {
            free(m_head->m_buf);
            m_head->m_buf = 0;
        }
        if (m_head)
            free(m_head);
        m_head = next;
    }
    m_8 = 0;
    m_c = 0;
}
struct T4766 { int a, b, c; };
struct P4766 { int m_0; int m_4; };
struct C4766a0 {
    char pad[0x5c];
    T4766 m_5c;
    void draw(P4766* p, int a, int b, int c, int d, int e);
    void f477da0(int a, int b, int c, int d, int e);
};
// MATCH: golf_clean.exe 0x004766a0 ?draw@C4766a0@@QAEXPAUP4766@@HHHHH@Z
void C4766a0::draw(P4766* p, int a, int b, int c, int d, int e)
{
    T4766 saved = m_5c;
    if (p && p->m_4) {
        m_5c.a = (int)p;
        m_5c.b = 0;
        m_5c.c = 0;
    }
    f477da0(a, b, c, d, e);
    m_5c = saved;
}
struct V486 { virtual void v0(); };
struct C486200 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual void v54(); virtual void v55(); virtual void v56(); virtual void v57(); virtual void v58(); virtual void v59(); virtual void v60(); virtual void v61(); virtual void v62(); virtual void v63(); virtual void v64(); virtual void v65(); virtual void v66(); virtual void v67(); virtual void v68(); virtual void v69(); virtual void v70(); virtual void v71(); virtual void changed();
    char pad[0x574 - 4];
    char* m_text;
    int m_max;
    char pad2[0x5a4 - 0x57c];
    int m_len;
    void setText(const char* s);
};
// MATCH: golf_clean.exe 0x00486200 ?setText@C486200@@QAEXPBD@Z
void C486200::setText(const char* s)
{
    if (m_text) {
        if (!s)
            *m_text = 0;
        else
            strncpy(m_text, s, m_max);
        m_len = strlen(m_text);
        changed();
    }
}
struct V1_4a0 { int m_v1; };
struct V2_4a0 { int f489890(const char* a, int b); };
struct C4a09a0 : virtual V1_4a0, virtual V2_4a0 {
    virtual void v0();
    char pad[0x1f4 - 8];
    int m_mode;
    int f48c640(const char* a, int b, char c);
    int send(const char* a, int b);
};
// MATCH: golf_clean.exe 0x004a09a0 ?send@C4a09a0@@QAEHPBDH@Z
int C4a09a0::send(const char* a, int b)
{
    switch (m_mode) {
    case 2:
        return f48c640(a, b, 0);
    case 1:
    case 4:
    case 8:
    case 0x10:
        return f489890(a, b);
    }
    return 0;
}
struct S494 { int m_0, m_4, m_8; struct P494 { int m_0, m_4; }* m_c; };
struct V2_494 { char pad[0xc0]; S494 m_s; };
struct V1_494 { int m_x; };
struct M494 : virtual V1_494, virtual V2_494 { int m_y; };
struct C4942a0 {
    int m_0;
    unsigned char m_4;
    char pad[0x1488 - 5];
    M494 m_1488;
    char pad2[0x2d98 - 0x1488 - sizeof(M494)];
    M494 m_2d98;
    int current();
};
// MATCH: golf_clean.exe 0x004942a0 ?current@C4942a0@@QAEHXZ
int C4942a0::current()
{
    S494* s;
    if (m_4 & 4) {
        s = &m_1488.m_s;
        if (s->m_8)
            return s->m_c->m_4;
    } else {
        s = &m_2d98.m_s;
        if (s->m_8)
            return s->m_c->m_4;
    }
    return 0;
}
struct B473 { B473(); virtual ~B473(); char pad[0x28]; };
struct X473 : B473 { X473(); virtual ~X473() {} };
struct C404200 {
    virtual ~C404200();
    void cleanup();          // 0x482dd0
    int m_4;
    X473 m_8;
};
// MATCH: golf_clean.exe 0x00404200 ??1C404200@@UAE@XZ
C404200::~C404200()
{
    cleanup();
}
struct C44ae00 {
    ~C44ae00();
    X473 m_0;
    X473 m_2c;
};
// MATCH: golf_clean.exe 0x0044ae00 ??1C44ae00@@QAE@XZ
C44ae00::~C44ae00()
{
}
struct V47e { virtual void v0(); virtual void v1(); virtual void release(); };
extern V47e* g_list47e[];
extern int g_count47e;
// MATCH: golf_clean.exe 0x0047e520 ?remove47e520@@YAXPAUV47e@@@Z
void __cdecl remove47e520(V47e* o)
{
    int i;
    if (!o)
        return;
    for (i = 0; i < g_count47e; i++)
        if (g_list47e[i] == o)
            break;
    if (i < g_count47e) {
        g_count47e--;
        o->release();
        for (; i < g_count47e; i++)
            g_list47e[i] = g_list47e[i + 1];
    }
}
void __cdecl FUN_004a0320(const char*, const char*, DWORD, int, int);
extern const char s_004e4960[];
extern const char s_004e4954[];
struct C49cb20 {
    char pad[0xb8];
    unsigned char m_flags;
    void run();
    int next();          // 0x49ccc0
    int step(int);       // 0x49cb80
    void finish();       // 0x49cb90
};
// MATCH: golf_clean.exe 0x0049cb20 ?run@C49cb20@@QAEXXZ
void C49cb20::run()
{
    FUN_004a0320(s_004e4960, s_004e4954, timeGetTime(), 0, 0);
    if (!(m_flags & 0x10)) {
        while (next() && step(0))
            ;
    }
    finish();
}
struct Line492 { int m_0, m_count, m_8, m_pos, m_step, m_step2, m_err, m_inc, m_dec; };
int __cdecl FUN_00492ed0(Line492*, int);
// MATCH: golf_clean.exe 0x00492fa0 ?step492fa0@@YAHPAULine492@@@Z
int __cdecl step492fa0(Line492* p)
{
    if (--p->m_count == 0)
        return FUN_00492ed0(p, p->m_8) != 0;
    p->m_pos += p->m_step;
    p->m_err += p->m_inc;
    if (p->m_err > 0) {
        p->m_pos += p->m_step2;
        p->m_err -= p->m_dec;
    }
    return 1;
}
struct C4979a0 {
    char pad[0x57c];
    int m_57c;
    char pad2[0x5ac - 0x580];
    struct R { int l, t, r, b; void set(int a, int b2, int c, int d) { l = a; t = b2; r = c; b = d; } void inset(int d) { l += d; r -= d; t += d; b -= d; } } m_5ac;
    char pad3[4];
    int m_5c0;
    void reset();
};
// MATCH: golf_clean.exe 0x004979a0 ?reset@C4979a0@@QAEXXZ
void C4979a0::reset()
{
    m_5ac.set(0, 0, m_5c0, m_5c0);
    if (m_57c != -1)
        m_5ac.inset(1);
}
struct P0_491 { virtual void a(); char pad[0x270]; };
struct P1_491 { virtual void b(); };
struct B488650 : P0_491, P1_491 { ~B488650(); };
struct C491410 : B488650 {
    ~C491410();
    virtual void a();
    void shutdown();     // 0x4a14c0
};
// MATCH: golf_clean.exe 0x00491410 ??1C491410@@QAE@XZ
C491410::~C491410()
{
    shutdown();
}
extern const char s_004e1b78[];
extern const char s_004e1b70[];
extern const char s_004e1b68[];
extern const char s_004e1b5c[];
extern char g_buf51a068[];
// MATCH: golf_clean.exe 0x00467560 ?append467560@@YAXI@Z
void __cdecl append467560(unsigned int v)
{
    switch (v & 3) {
    case 0:
        strcat(g_buf51a068, s_004e1b78);
        break;
    case 1:
        strcat(g_buf51a068, s_004e1b70);
        break;
    case 2:
        strcat(g_buf51a068, s_004e1b68);
        break;
    case 3:
        strcat(g_buf51a068, s_004e1b5c);
        break;
    }
}
struct C48ce00 { C48ce00(); char pad[0x2114]; };
// MATCH: golf_clean.exe 0x00491310 ?create491310@@YAPAUC48ce00@@XZ
C48ce00* __cdecl create491310()
{
    return new C48ce00;
}
struct N489 { int m_0; int m_key; int m_8; N489* m_next; };
struct C4899d0 {
    char pad[0xc8];
    N489* m_head;
    N489* m_cur;
    int m_count;
    int m_index;
    int find(int key);
};
// MATCH: golf_clean.exe 0x004899d0 ?find@C4899d0@@QAEHH@Z
int C4899d0::find(int key)
{
    int i = 0;
    if (m_head) {
        m_index = 0;
        m_cur = m_head;
        for (; i < m_count; i++) {
            if (m_cur->m_key == key)
                break;
            m_index++;
            m_cur = m_cur->m_next;
        }
    }
    return m_index;
}
struct B44b { B44b(); virtual ~B44b(); char pad[0x28]; };
struct X44b : B44b { X44b(); virtual ~X44b() {} };
struct C44b690 {
    ~C44b690();
    X44b m_0;
    X44b m_2c[4];
};
// MATCH: golf_clean.exe 0x0044b690 ??1C44b690@@QAE@XZ
C44b690::~C44b690()
{
}
struct E46e { int m_0; int m_4; char pad[0x28]; };
extern unsigned g_flags59e7b8;
extern E46e g_tbl4c15a0[];
extern int g_4e3db8, g_4e3dbc, g_8392a4, g_839338, g_834170, g_59bf90;
// MATCH: golf_clean.exe 0x0046e7b0 ?post46e7b0@@YAXHHH@Z
void __cdecl post46e7b0(int i, int a, int b)
{
    if (!(g_flags59e7b8 & 0x4000000) && g_tbl4c15a0[i].m_0 == 0) {
        g_tbl4c15a0[i].m_0 = g_834170;
        g_839338 = 0;
        g_tbl4c15a0[i].m_4 = g_59bf90;
        g_4e3db8 = i;
        g_4e3dbc = a;
        g_8392a4 = b;
    }
}
struct C4844e0;
extern C4844e0* g_head83af68;
extern C4844e0* g_tail83af64;
struct C4844e0 {
    char pad[0x44];
    unsigned m_flags;
    C4844e0* m_prev;
    C4844e0* m_next;
    int unlink();
};
// MATCH: golf_clean.exe 0x004844e0 ?unlink@C4844e0@@QAEHXZ
int C4844e0::unlink()
{
    if (!(m_flags & 2))
        return 0;
    if (m_prev)
        m_prev->m_next = m_next;
    else
        g_head83af68 = m_next;
    if (m_next)
        m_next->m_prev = m_prev;
    else
        g_tail83af64 = m_prev;
    m_next = 0;
    m_flags &= ~2;
    m_prev = 0;
    return 0;
}
struct B487 { virtual ~B487(); };
struct E487 { ~E487(); char pad[0x1c]; };
struct C487390 : B487 {
    ~C487390();
    char pad[0x20];
    E487 m_24[16];
};
// MATCH: golf_clean.exe 0x00487390 ??1C487390@@UAE@XZ
C487390::~C487390()
{
}
struct V1_4a1 { int m_v1; };
struct V2_4a1 { int f489890(const char* a, int b); char pad[0x60]; int m_60, m_64; };
struct S49d { void f49d770(); };
struct S49f { void f49f050(); };
struct S4a3 { void f4a3be0(); };
struct S4a2 { void f4a2a60(); };
struct C4a11c0 : virtual V1_4a1, virtual V2_4a1 {
    virtual void v0();
    char pad[0x58 - 8];
    S49d m_58;
    char pad2[0x78 - 0x59];
    S49f m_78;
    char pad3[0x90 - 0x79];
    S4a3 m_90;
    char pad4[0x16c - 0x91];
    S4a2 m_16c;
    char pad5[0x1f4 - 0x16d];
    int m_mode;
    void f48a120();
    void stop();
};
// MATCH: golf_clean.exe 0x004a11c0 ?stop@C4a11c0@@QAEXXZ
void C4a11c0::stop()
{
    switch (m_mode) {
    case 0x10:
        m_58.f49d770();
        return;
    case 2:
        f48a120();
        return;
    case 1:
        m_78.f49f050();
        return;
    case 8:
        m_90.f4a3be0();
        return;
    case 4:
        m_16c.f4a2a60();
        return;
    }
    m_60 = 0;
    m_64 = 0;
}
struct P0_486 { virtual void a(); char pad[0x270]; };
struct P1_486 { virtual void b(); };
struct B480 : P0_486, P1_486 { B480(); ~B480(); char pad[0x574 - 0x278]; };
struct M486 { M486(); int m_0; };
struct C486070 : B480 {
    C486070();
    virtual void a();
    void reset();       // 0x485ff0
    void* m_574;
    char pad[0x5ac - 0x578];
    M486 m_5ac;
};
// MATCH: golf_clean.exe 0x00486070 ??0C486070@@QAE@XZ
C486070::C486070()
{
    reset();
}
struct P0_v491 { virtual ~P0_v491(); char pad[0x270]; };
struct P1_v491 { virtual void p1(); };
struct V1_v491 : P0_v491, P1_v491 { char pad[0x574 - 0x278]; };
struct V2_v491 { virtual void v2(); };
struct C491680 : virtual V1_v491, virtual V2_v491 {
    ~C491680();
    void cleanup();     // 0x49d690
    virtual void p1();
    virtual void v2();
    char m[0x1c];
};
// MATCH: golf_clean.exe 0x00491680 ??1C491680@@UAE@XZ
C491680::~C491680()
{
    cleanup();
}
struct C49ec80 : virtual V1_v491, virtual V2_v491 {
    ~C49ec80();
    void cleanup();     // 0x49ece0
    virtual void p1();
    virtual void v2();
    char m[0x14];
};
// MATCH: golf_clean.exe 0x0049ec80 ??1C49ec80@@UAE@XZ
C49ec80::~C49ec80()
{
    cleanup();
}
struct C489de0 : virtual V1_v491, virtual V2_v491 {
    ~C489de0();
    virtual void own();
    void cleanup();     // 0x489e40
    virtual void p1();
    virtual void v2();
    char m[0x50];
};
// MATCH: golf_clean.exe 0x00489de0 ??1C489de0@@UAE@XZ
C489de0::~C489de0()
{
    cleanup();
}
struct C4a4890 {
    char pad[0x80];
    N489* m_head;
    N489* m_cur;
    int m_count;
    int m_index;
    int find(int key);
};
// MATCH: golf_clean.exe 0x004a4890 ?find@C4a4890@@QAEHH@Z
int C4a4890::find(int key)
{
    int i = 0;
    if (m_head) {
        m_index = 0;
        m_cur = m_head;
        for (; i < m_count; i++) {
            if (m_cur->m_key == key)
                break;
            m_index++;
            m_cur = m_cur->m_next;
        }
    }
    return m_index;
}
