// FLAGS golf_clean.exe: /O2
// golf_clean.exe batch 00 written by hand (release /O2): small ctors, accessors, wrappers and a virtual-base class. Names are chosen here.
#include <stdlib.h>
#include <string.h>
struct C49cb80 { int run(int); };
// MATCH: golf_clean.exe 0x0049cb80 ?run@C49cb80@@QAEHH@Z
int C49cb80::run(int)
{
    return 1;
}
extern int g_83c310;
// MATCH: golf_clean.exe 0x00492460 ?get492460@@YAHXZ
int get492460()
{
    return g_83c310 ? g_83c310 : 8;
}
// m_4 is stored before the vptr: that store comes from the inline base ctor.
struct B4837f0 { B4837f0() { m_4 = 0; } virtual void v(); int m_4; };
struct C4837f0 : B4837f0 { C4837f0(); virtual void v(); };
// MATCH: golf_clean.exe 0x004837f0 ??0C4837f0@@QAE@XZ
C4837f0::C4837f0()
{
}
extern int g_83afc0;
extern int (*g_83af94)();
// MATCH: golf_clean.exe 0x004840c0 ?call4840c0@@YAHXZ
int call4840c0()
{
    if (!g_83afc0)
        return 0;
    return g_83af94();
}
struct V4845e0 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual int v8(); };
struct C4845e0 { char pad[0x40]; V4845e0* m_40; int get(); };
// MATCH: golf_clean.exe 0x004845e0 ?get@C4845e0@@QAEHXZ
int C4845e0::get()
{
    int r = 0x14;
    if (m_40)
        r = m_40->v8();
    return r;
}
struct C488490 { C488490(); virtual void v(); int m_4, m_8, m_c, m_10; };
// MATCH: golf_clean.exe 0x00488490 ??0C488490@@QAE@XZ
C488490::C488490()
{
    m_4 = 0;
    m_c = 0;
    m_10 = 0;
    m_8 = 0;
}
struct C492d80 { C492d80(); virtual void v(); int m_4, m_8, m_c; };
// MATCH: golf_clean.exe 0x00492d80 ??0C492d80@@QAE@XZ
C492d80::C492d80()
{
    m_4 = 0;
    m_8 = -1;
    m_c = 0;
}
// The member at +0x28 has vtable 0x4ba278; its inline dtor writes m_4 to 0x839650.
extern int g_839650;
struct M489c90 { virtual void v(); void shutdown(); void shutdown2(); ~M489c90() { g_839650 = m_4; } int m_4; };
struct C489c90 { ~C489c90(); char pad[0x28]; M489c90 m_28; };
// MATCH: golf_clean.exe 0x00489c90 ??1C489c90@@QAE@XZ
C489c90::~C489c90()
{
    m_28.shutdown();     // 0x489b30
}
struct C4a3370 { ~C4a3370(); char pad[0x28]; M489c90 m_28; };
// MATCH: golf_clean.exe 0x004a3370 ??1C4a3370@@QAE@XZ
C4a3370::~C4a3370()
{
    m_28.shutdown2();    // 0x4a33e0
}
// MATCH: golf_clean.exe 0x004925f0 ?addNewline@@YAXPAD@Z
void addNewline(char* s)
{
    char* p = s + strlen(s);
    p[0] = '\n';
    p[1] = 0;
}
struct T477560 { char pad[0x10]; int m_10; };
extern T477560* g_83ad44;
struct C477560 { char pad[0x5c]; T477560* m_5c; int get(); };
// MATCH: golf_clean.exe 0x00477560 ?get@C477560@@QAEHXZ
int C477560::get()
{
    if (!m_5c)
        m_5c = g_83ad44;
    return m_5c->m_10;
}
struct C487000 { C487000(); virtual void v(); int m_4, m_8, m_c, m_10, m_14, m_18; };
// MATCH: golf_clean.exe 0x00487000 ??0C487000@@QAE@XZ
C487000::C487000()
{
    m_c = 0;
    m_4 = 0;
    m_8 = 0;
    m_10 = 0;
    m_14 = 0;
    m_18 = 0;
}
extern int g_83afc4;
extern int (*g_83af70)(int);
// MATCH: golf_clean.exe 0x00484110 ?call484110@@YAHH@Z
int call484110(int a)
{
    if (!g_83afc4)
        return 1;
    return g_83af70(a);
}
struct E487770 { char m_0, m_1; char pad[0x1a]; };
struct C487770 { char pad[0x24]; E487770 m_e[16]; bool isEmpty(unsigned i); bool isSet(unsigned i); };
// MATCH: golf_clean.exe 0x00487770 ?isEmpty@C487770@@QAE_NI@Z
bool C487770::isEmpty(unsigned i)
{
    if (i > 15)
        return true;
    return m_e[i].m_0 ? false : true;
}
// MATCH: golf_clean.exe 0x004877a0 ?isSet@C487770@@QAE_NI@Z
bool C487770::isSet(unsigned i)
{
    if (i > 15)
        return true;
    return m_e[i].m_1 ? true : false;
}
struct B473ab0 { B473ab0() { m_4 = 0; } virtual void v(); int m_4; };
struct C473ab0 : B473ab0 { C473ab0(); virtual void v(); int m_8, m_c, m_10, m_14, m_18, m_1c, m_20, m_24; };
// MATCH: golf_clean.exe 0x00473ab0 ??0C473ab0@@QAE@XZ
C473ab0::C473ab0()
{
    m_8 = 0;
    m_10 = 0;
    m_14 = 0;
    m_18 = 0;
    m_1c = 0;
    m_20 = 0;
    m_24 = 0;
    m_c = 0;
}
struct C421b60 { void draw(int, int, int, int, int, int); void draw7(int, int, int, int, int, int, int); };
// MATCH: golf_clean.exe 0x00421b60 ?draw@C421b60@@QAEXHHHHHH@Z
void C421b60::draw(int a, int b, int c, int d, int e, int f)
{
    draw7(a, b, c, d, d, e, f);     // 0x473cb0
}
extern void (*g_83af80)();
extern void (*g_83af88)();
extern void (*g_83af90)();
struct C487430 { char pad[0x14]; int m_14; int close1(); int close2(); int close3(); };
// MATCH: golf_clean.exe 0x00487430 ?close1@C487430@@QAEHXZ
int C487430::close1()
{
    if (m_14 && g_83af80) {
        g_83af80();
        m_14 = 0;
        return 0;
    }
    return 0x14;
}
// MATCH: golf_clean.exe 0x004879f0 ?close2@C487430@@QAEHXZ
int C487430::close2()
{
    if (m_14 && g_83af88) {
        g_83af88();
        m_14 = 0;
        return 0;
    }
    return 0x14;
}
// MATCH: golf_clean.exe 0x00487bd0 ?close3@C487430@@QAEHXZ
int C487430::close3()
{
    if (m_14 && g_83af90) {
        g_83af90();
        m_14 = 0;
        return 0;
    }
    return 0x14;
}
// MATCH: golf_clean.exe 0x00493580 ?swap493580@@YAXPAH0@Z
void swap493580(int* a, int* b)
{
    if (a != b) {
        *a ^= *b;
        *b ^= *a;
        *a ^= *b;
    }
}
struct I4924b0 { virtual void __stdcall v0(); virtual void __stdcall v1(); virtual void __stdcall v2(); virtual void __stdcall v3(); virtual void __stdcall v4(); virtual void __stdcall v5(); virtual void __stdcall v6(); virtual void __stdcall v7(); virtual void __stdcall v8(); virtual void __stdcall v9(); virtual void __stdcall v10(); virtual void __stdcall v11(); virtual void __stdcall v12(); virtual void __stdcall v13(); virtual void __stdcall v14(); virtual void __stdcall v15(); virtual void __stdcall v16(); virtual void __stdcall v17(); virtual void __stdcall v18(); virtual void __stdcall v19(); virtual void __stdcall v20(); virtual void __stdcall v21(); virtual void __stdcall v22(); virtual void __stdcall v23(); virtual void __stdcall v24(); virtual void __stdcall v25(); virtual void __stdcall v26(); virtual void __stdcall v27(); virtual void __stdcall v28(); virtual void __stdcall v29(); virtual void __stdcall v30(); virtual void __stdcall v31(); virtual void __stdcall release(int); };
extern I4924b0* g_83c32c;
extern int g_83c330;
// MATCH: golf_clean.exe 0x004924b0 ?release4924b0@@YAXXZ
void release4924b0()
{
    if (g_83c32c && g_83c330) {
        g_83c32c->release(g_83c330);
        g_83c330 = 0;
    }
}
// The redundant inner NULL test is kept by VC6 when it comes from an inline helper.
inline void safeFree4a05d0(void* p) { if (p) free(p); }
struct S4a05d0 { int m_0; void* m_4; };
struct C4a05d0 { char pad[0x18]; int m_18; void freeBuf(S4a05d0* s); };
// MATCH: golf_clean.exe 0x004a05d0 ?freeBuf@C4a05d0@@QAEXPAUS4a05d0@@@Z
void C4a05d0::freeBuf(S4a05d0* s)
{
    if (s->m_4 && !m_18) {
        safeFree4a05d0(s->m_4);
        s->m_4 = 0;
    }
}
struct C486330 { char pad[0x574]; unsigned m_574; int find(int off, unsigned* tab, int n); };
// MATCH: golf_clean.exe 0x00486330 ?find@C486330@@QAEHHPAIH@Z
int C486330::find(int off, unsigned* tab, int n)
{
    unsigned v = m_574 + off;
    int i;
    for (i = 0; i < n; i++, tab++) {
        if (v < *tab)
            break;
    }
    return i - 1;
}
struct C486d60 { void init(int, int, int, int, int); void a(); void b(); void initA(int, int, int, int, int); void initB(int, int, int, int, int); };
// MATCH: golf_clean.exe 0x00486d60 ?initA@C486d60@@QAEXHHHHH@Z
void C486d60::initA(int p1, int p2, int p3, int p4, int p5)
{
    init(p1, p2, p3, p4, p5);   // 0x486d20
    a();                        // 0x486dc0
}
// MATCH: golf_clean.exe 0x00486d90 ?initB@C486d60@@QAEXHHHHH@Z
void C486d60::initB(int p1, int p2, int p3, int p4, int p5)
{
    init(p1, p2, p3, p4, p5);   // 0x486d20
    b();                        // 0x486e40
}
struct C486c90 { C486c90(); virtual void v(); int m_4, m_8, m_c, m_10, m_14, m_18, m_1c, m_20, m_24, m_28, m_2c; };
// MATCH: golf_clean.exe 0x00486c90 ??0C486c90@@QAE@XZ
C486c90::C486c90()
{
    m_c = 0;
    m_10 = 0;
    m_14 = 0;
    m_18 = 0;
    m_1c = 0;
    m_20 = 0;
    m_24 = 0;
    m_28 = 5;
    m_2c = 0;
    m_8 = 0;
    m_4 = 0;
}
// Virtual inheritance: vbptr at +0; vbtable entry +4 is the first virtual base, +8 the second.
// 0x49fc30 overrides a virtual of the first virtual base, so `this` arrives pointing at that base (+0x1c).
struct V1_49fc30 { virtual void v0(); virtual void f(int, int); char pad[0x130 - 4]; int m_130; };
struct V2_49fc30 { char pad[0xf0]; unsigned m_f0; };
extern int g_83ab2c;
struct D49fc30 : virtual V1_49fc30, virtual V2_49fc30 {
    int m_4, m_8;
    void (*m_c)(int);
    int m_10, m_14, m_18;
    virtual void f(int, int);
    unsigned test(int bit);
};
// MATCH: golf_clean.exe 0x0049fc30 ?f@D49fc30@@UAEXHH@Z
void D49fc30::f(int, int)
{
    g_83ab2c = m_130;
    if (m_c)
        m_c(m_f0);
}
// MATCH: golf_clean.exe 0x0049f030 ?test@D49fc30@@QAEIH@Z
unsigned D49fc30::test(int bit)
{
    return m_f0 & (1 << bit);
}
