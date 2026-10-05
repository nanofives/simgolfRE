// FLAGS golf_clean.exe: /O2
// golf_clean.exe batch 01, hand-matched (release /O2). Class and function names are chosen here, not recovered.
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <mmsystem.h>
#pragma warning(disable: 4715)

struct C49ee90 {
    int test(int);          // 0x49ef80
    void set(int, int);     // 0x49eef0
    void toggle(int);
};
// MATCH: golf_clean.exe 0x0049ee90 ?toggle@C49ee90@@QAEXH@Z
void C49ee90::toggle(int a)
{
    if (test(a))
        set(a, 0);
    else
        set(a, 1);
}

struct C449860 { void shutdown(); ~C449860(); };   // 0x4a4f64, dtor 0x4a4f5e
extern C449860* g_820ed0;
// MATCH: golf_clean.exe 0x00449860 ?free449860@@YAXXZ
void free449860()
{
    g_820ed0->shutdown();
    delete g_820ed0;
    g_820ed0 = 0;
}

struct I484550 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual int v5(); };
struct C484550 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32();
    char pad[0x40 - 4];
    I484550* m_40;
    int m_44;
    int stop();
};
// MATCH: golf_clean.exe 0x00484550 ?stop@C484550@@QAEHXZ
int C484550::stop()
{
    int r = 0;
    if (m_40)
        r = m_40->v5();
    v32();
    m_40 = 0;
    m_44 &= ~1;
    return r;
}

struct C484f00 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32();
    char pad[0x40 - 4];
    I484550* m_40;
    int m_44;
    char pad2[0x58 - 0x48];
    unsigned char m_58;
    int stop();
};
// MATCH: golf_clean.exe 0x00484f00 ?stop@C484f00@@QAEHXZ
int C484f00::stop()
{
    int r = 0;
    if (m_40)
        r = m_40->v5();
    m_40 = 0;
    if (!(m_58 & 2))
        v32();
    m_44 &= ~1;
    return r;
}

struct I4838f0 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual int write(const char*, int); };
struct C4838f0 { int m_0; I4838f0* m_4; int puts(const char*); };
// MATCH: golf_clean.exe 0x004838f0 ?puts@C4838f0@@QAEHPBD@Z
int C4838f0::puts(const char* s)
{
    if (m_4 && s)
        return m_4->write(s, strlen(s));
    return 0;
}

struct M486b30 { void on(); void off(); };   // 0x486dc0, 0x486ec0
struct C486b30 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual void v54(); virtual void v55(); virtual void v56(); virtual void v57(); virtual void v58(); virtual void v59(); virtual void v60(); virtual void v61(); virtual void v62(); virtual void v63(); virtual void v64(); virtual void v65(); virtual void v66(); virtual void v67(); virtual void v68(); virtual void v69(); virtual void v70(); virtual void v71(); virtual void v72();
    char pad[0x5a8 - 4];
    int m_5a8;
    M486b30 m_5ac;
    void setOn(int);
};
// MATCH: golf_clean.exe 0x00486b30 ?setOn@C486b30@@QAEXH@Z
void C486b30::setOn(int v)
{
    if (v)
        m_5ac.on();
    else
        m_5ac.off();
    m_5a8 = v;
    v72();
}

// MATCH: golf_clean.exe 0x004935b0 ?findBack@@YAPADPAD0D@Z
char* findBack(char* lo, char* hi, char c)
{
    char* r;
    __asm {
        mov ebx, lo
        and ebx, ebx
        je L35d6
        mov ecx, hi
        and ecx, ecx
        je L35d6
        mov eax, ecx
        sub ecx, ebx
        and ecx, ecx
        je L35d6
        mov bl, c
    L35ce:
        cmp byte ptr [eax], bl
        je L35d8
        dec eax
        dec ecx
        jne L35ce
    L35d6:
        xor eax, eax
    L35d8:
        mov r, eax
    }
    return r;
}

extern unsigned char g_4bba88, g_4bba89;
// MATCH: golf_clean.exe 0x004935f0 ?findRange@@YAPADPAD@Z
char* findRange(char* s)
{
    char* r;
    __asm {
        mov eax, s
        and eax, eax
        je L361c
        mov dl, g_4bba88
        mov dh, g_4bba89
        xor ebx, ebx
        dec eax
    L360b:
        inc eax
        mov bl, byte ptr [eax]
        and bl, bl
        je L361c
        cmp bl, dl
        jl L360b
        cmp bl, dh
        jg L360b
        jmp L361e
    L361c:
        xor eax, eax
    L361e:
        mov r, eax
    }
    return r;
}

extern int g_83ad4c;
int poll483c70();
int finish497b00();
// MATCH: golf_clean.exe 0x00483c90 ?drain483c90@@YAHXZ
int drain483c90()
{
    g_83ad4c = 2;
    while (poll483c70())
        g_83ad4c = 2;
    g_83ad4c = 0;
    return finish497b00();
}
int poll483cd0();
// MATCH: golf_clean.exe 0x00483cf0 ?drain483cf0@@YAHXZ
int drain483cf0()
{
    g_83ad4c = 8;
    while (poll483cd0())
        g_83ad4c = 8;
    g_83ad4c = 0;
    return finish497b00();
}

struct Slot49aa30 { int m_id; char pad[0x15 - 4]; char m_name[0x58 - 0x15]; };
struct C49aa30 { char pad[0x170]; Slot49aa30 m_slots[16]; char* nameOf(int); };
// MATCH: golf_clean.exe 0x0049aa30 ?nameOf@C49aa30@@QAEPADH@Z
char* C49aa30::nameOf(int id)
{
    int i;
    for (i = 0; i < 16; i++)
        if (m_slots[i].m_id == id)
            break;
    if (i == 16)
        return 0;
    return m_slots[i].m_name;
}

struct I492470 {
    virtual long __stdcall v0(); virtual long __stdcall v1(); virtual unsigned long __stdcall Release();
    virtual long __stdcall v3(); virtual long __stdcall v4(); virtual long __stdcall v5(); virtual long __stdcall v6(); virtual long __stdcall v7(); virtual long __stdcall v8(); virtual long __stdcall v9(); virtual long __stdcall v10(); virtual long __stdcall v11(); virtual long __stdcall v12(); virtual long __stdcall v13(); virtual long __stdcall v14(); virtual long __stdcall v15(); virtual long __stdcall v16(); virtual long __stdcall v17(); virtual long __stdcall v18(); virtual long __stdcall v19();
};
extern I492470* g_83c328;
extern int g_83c32c;
void pre4924b0();
void post4a0890();
// MATCH: golf_clean.exe 0x00492470 ?close492470@@YAXXZ
void close492470()
{
    pre4924b0();
    if (g_83c328) {
        g_83c328->v19();
        g_83c328->Release();
        g_83c328 = 0;
    }
    g_83c32c = 0;
    post4a0890();
}

struct C475b20 { int rect(RECT*); int box(int, int, int, int); };   // 0x475b00
// MATCH: golf_clean.exe 0x00475b20 ?box@C475b20@@QAEHHHHH@Z
int C475b20::box(int x, int y, int w, int h)
{
    RECT r;
    r.left = x;
    r.top = y;
    r.right = x + w;
    r.bottom = y + h;
    return rect(&r);
}

struct C477e60 { int draw(int, RECT*, int); int drawBox(int, int, int, int, int, int); };   // 0x477eb0
// MATCH: golf_clean.exe 0x00477e60 ?drawBox@C477e60@@QAEHHHHHHH@Z
int C477e60::drawBox(int a, int x, int y, int w, int h, int b)
{
    RECT r;
    r.left = x;
    r.top = y;
    r.right = x + w;
    r.bottom = y + h;
    return draw(a, &r, b);
}

struct H475b60 { int m_0; int m_4; };
struct D475b60 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32();
    virtual int f84(int, int, int);
    virtual void v34(); virtual void v35();
    virtual int f90(int, int, int, int);
    virtual void v37(); virtual void v38(); virtual void v39();
    virtual int fa0(int, int, int, int);
    virtual int fa4(int, int, int, int);
};
struct C475b60 {
    int m_0;
    D475b60* m_4;
    int call84(H475b60*, int, int);
    int call90(H475b60*, int, int, int);
    int calla4(H475b60*, int, int, int);
    int calla0(H475b60*, int, int, int);
};
// MATCH: golf_clean.exe 0x00475b60 ?call84@C475b60@@QAEHPAUH475b60@@HH@Z
int C475b60::call84(H475b60* h, int a, int b)
{
    if (!m_4)
        return 7;
    if (!h)
        return 0x10;
    return m_4->f84(h->m_4, a, b);
}
// MATCH: golf_clean.exe 0x00475ba0 ?call90@C475b60@@QAEHPAUH475b60@@HHH@Z
int C475b60::call90(H475b60* h, int a, int b, int c)
{
    if (!m_4)
        return 7;
    if (!h)
        return 0x10;
    return m_4->f90(h->m_4, a, b, c);
}
// MATCH: golf_clean.exe 0x00475be0 ?calla4@C475b60@@QAEHPAUH475b60@@HHH@Z
int C475b60::calla4(H475b60* h, int a, int b, int c)
{
    if (!m_4)
        return 7;
    if (!h)
        return 0x10;
    return m_4->fa4(h->m_4, a, b, c);
}
// MATCH: golf_clean.exe 0x00475c20 ?calla0@C475b60@@QAEHPAUH475b60@@HHH@Z
int C475b60::calla0(H475b60* h, int a, int b, int c)
{
    if (!m_4)
        return 7;
    if (!h)
        return 0x10;
    return m_4->fa0(h->m_4, a, b, c);
}

extern int g_822b80;
// MATCH: golf_clean.exe 0x00456b70 ?toScreen456b70@@YAXHHPAH0@Z
void toScreen456b70(int a, int b, int* x, int* y)
{
    *x = (a + b) * 6 + 0x6a;
    *y = (b - a) * 3 - (g_822b80 ? 1 : -1) + 0x1b8;
}

// MATCH: golf_clean.exe 0x0040e5b0 ?rank40e5b0@@YAHH@Z
int rank40e5b0(int v)
{
    int n = v / 200;
    if (n == 0)
        return 0;
    if (n <= 2)
        return 1;
    if (n <= 5)
        return 2;
    if (n <= 9)
        return 3;
    return 4;
}

// MATCH: golf_clean.exe 0x0042e7a0 ?dir42e7a0@@YAHHH@Z
int dir42e7a0(int dx, int dy)
{
    if (abs(dx) > abs(dy))
        return dx > 0 ? 2 : 6;
    return dy > 0 ? 4 : 0;
}


struct P482fd0 { virtual void destroy(int); };
struct Slot482fd0 { int m_id; P482fd0* m_obj; char m_a, m_b; char pad[2]; void* m_buf; };
struct C482fd0 {
    virtual void v0();
    int m_4;
    Slot482fd0 m_slots[5];
    C482fd0();
};
// MATCH: golf_clean.exe 0x00482fd0 ??0C482fd0@@QAE@XZ
C482fd0::C482fd0() : m_4(0)
{
    int i;
    for (i = 0; i < 5; i++) {
        m_slots[i].m_id = -1;
        m_slots[i].m_obj = 0;
        m_slots[i].m_a = 0;
        m_slots[i].m_b = 0;
        m_slots[i].m_buf = 0;
    }
    m_4 = 0;
}

// MATCH: golf_clean.exe 0x00476d80 ?skipToken@@YAPADPADPAH@Z
char* skipToken(char* p, int* n)
{
    __asm {
        mov eax, p
        mov ebx, n
        mov edx, dword ptr [ebx]
        and edx, edx
        je done
    L6d90:
        mov cl, byte ptr [eax]
        cmp cl, 0x7b
        je L6db4
        cmp cl, 0x7d
        je L6db4
        cmp cl, 0x5b
        je L6db4
        cmp cl, 0x5d
        je L6db4
        cmp cl, 0x24
        je L6db4
        cmp cl, 0x3d
        je L6db4
        inc eax
        dec edx
        jne L6d90
    L6db4:
        mov dword ptr [ebx], edx
        mov p, eax
        jmp done
    done:
    }
    return p;
}

struct M489890 { char pad[0x1c]; int m_1c; int m_20; int m_24; int start(int); };   // 0x401d10
struct C489890 { char pad[0xc0]; M489890 m_c0; int begin(int, int); };
// MATCH: golf_clean.exe 0x00489890 ?begin@C489890@@QAEHHH@Z
int C489890::begin(int a, int b)
{
    M489890* m;
    if (!a)
        return 3;
    m = &m_c0;
    m->m_1c = a;
    m->m_20 = 0;
    m->m_24 = 1;
    return m->start(b);
}

struct I487c00 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual int open(int, int); };
struct C487c00 {
    virtual void v0(); virtual int check(int); virtual void done();
    char pad[0x14 - 4];
    I487c00* m_14;
    int load(int, int);
    int load2(int, int);
};
#pragma warning(disable: 4715)
// MATCH: golf_clean.exe 0x00487c00 ?load@C487c00@@QAEHHH@Z
int C487c00::load(int a, int b)
{
    int r;
    if (!check(b)) {
        r = m_14->open(a, b);
        if (r) {
            done();
            return r;
        }
        return 0;
    }
}
// MATCH: golf_clean.exe 0x00487a70 ?load2@C487c00@@QAEHHH@Z
int C487c00::load2(int a, int b)
{
    int r;
    if (!check(b)) {
        r = m_14->open(a, b);
        if (r) {
            done();
            m_14 = 0;
            return r;
        }
        return 0;
    }
}

struct I4890e0 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53();
    virtual void notify(int, int);
};
struct C4890e0 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual void v54(); virtual void v55(); virtual void v56(); virtual void v57(); virtual void v58(); virtual void v59(); virtual void v60(); virtual void v61(); virtual void v62(); virtual void v63(); virtual void v64(); virtual void v65(); virtual void v66(); virtual void v67(); virtual void v68(); virtual void v69(); virtual void v70(); virtual void v71(); virtual void v72();
    char pad[0x130 - 4];
    I4890e0* m_130;
    char pad2[0x578 - 0x134];
    int m_578;
    char pad3[0x5e8 - 0x57c];
    int m_5e8;
    void setMode(int);
};
// MATCH: golf_clean.exe 0x004890e0 ?setMode@C4890e0@@QAEXH@Z
void C4890e0::setMode(int v)
{
    if (v != m_578) {
        m_578 = v;
        v72();
        if (m_130)
            m_130->notify(m_5e8, v);
    }
}

struct E45de30 { int m[0x40]; };
extern E45de30 g_5794b8[];
extern E45de30 g_582cb8;
// MATCH: golf_clean.exe 0x0045de30 ?swap45de30@@YAXHH@Z
void swap45de30(int a, int b)
{
    g_582cb8 = g_5794b8[a];
    g_5794b8[a] = g_5794b8[b];
    g_5794b8[b] = g_582cb8;
}

struct M497cc0 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); };
struct C497cc0 {
    char pad[0x64];
    M497cc0 m_64;
    char pad2[0xe4 - 0x68];
    int m_e4;
    unsigned int m_e8;
    void post(int, int, int, int, int);   // 0x499140
    void flush();
};
// MATCH: golf_clean.exe 0x00497cc0 ?flush@C497cc0@@QAEXXZ
void C497cc0::flush()
{
    if (m_e8 & 0x20000000) {
        m_64.v8();
        m_e8 &= ~0x20000000;
        post(0, 0, m_e4, 0x200, 0);
    }
}

struct C47d020 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void resize(int, int, int, int);
    char pad[0xa0 - 4];
    int m_a0;
    char pad2[0x184 - 0xa4];
    int m_184;
    char pad3[0x1bc - 0x188];
    RECT m_rc;
    void setValue(int);
};
// MATCH: golf_clean.exe 0x0047d020 ?setValue@C47d020@@QAEXH@Z
void C47d020::setValue(int v)
{
    if (m_a0 & 2) {
        m_184 = v;
        resize(m_rc.right - m_rc.left, m_rc.bottom - m_rc.top, 0, 0);
    }
}

struct C49eff0b { char pad[0xf0]; unsigned int m_bits; };
struct C49eff0a { int m_0; };
struct C49eff0 : virtual C49eff0a, virtual C49eff0b {
    void setBit(int, int);
};
// MATCH: golf_clean.exe 0x0049eff0 ?setBit@C49eff0@@QAEXHH@Z
void C49eff0::setBit(int bit, int on)
{
    if (on)
        m_bits |= 1 << bit;
    else
        m_bits &= ~(1 << bit);
}

typedef void (*FP4a4fc4)();
extern FP4a4fc4 g_4e4b00[6];
void f4a78d7();
void f4a7581();
void f4a75e7();
void f4a7527();
void f4a75cf();
// MATCH: golf_clean.exe 0x004a4fc4 ?init4a4fc4@@YAXXZ
void init4a4fc4()
{
    g_4e4b00[1] = f4a7581;
    g_4e4b00[0] = f4a78d7;
    g_4e4b00[2] = f4a75e7;
    g_4e4b00[3] = f4a7527;
    g_4e4b00[4] = f4a75cf;
    g_4e4b00[5] = f4a78d7;
}
// MATCH: golf_clean.exe 0x00493630 ?fill16b@@YAXPAGGH@Z
void fill16b(unsigned short* dst, unsigned short v, int n)
{
    int i;
    unsigned int w = ((unsigned int)v << 16) | v;
    unsigned int* p;
    if (n & 1) {
        *dst = v;
        p = (unsigned int*)(dst + 1);
    } else
        p = (unsigned int*)dst;
    n /= 2;
    for (i = 0; i < n; i++)
        *p++ = w;
}

struct C49eec0 {
    int test(int);          // 0x49f030
    void set(int, int);     // 0x49eff0
    void toggle(int);
};
// MATCH: golf_clean.exe 0x0049eec0 ?toggle@C49eec0@@QAEXH@Z
void C49eec0::toggle(int a)
{
    if (test(a))
        set(a, 0);
    else
        set(a, 1);
}

struct C484b30 {
    char pad[0x40];
    void* m_40;
    char pad2[0x58 - 0x44];
    unsigned int m_58;
    char pad3[0x64 - 0x5c];
    DWORD m_64;
    DWORD m_68;
    int busy2();     // 0x4845d0
    int busy();
};
// MATCH: golf_clean.exe 0x00484b30 ?busy@C484b30@@QAEHXZ
int C484b30::busy()
{
    if (m_40 == 0 && (char)(m_58 >> 4) & 1) {
        if (m_68 && m_64 > timeGetTime() - m_68)
            return 1;
        return 0;
    }
    return busy2();
}

extern const char s_004c3ee0[];
int cmp4a5800(const char*, const char*);
// MATCH: golf_clean.exe 0x00405ac0 ?trimSpaces@@YAHPAD@Z
int trimSpaces(char* s)
{
    int len;
    if (cmp4a5800(s, s_004c3ee0))
        return 0;
    do {
        len = strlen(s);
        if (len && s[len - 1] == ' ')
            s[len - 1] = 0;
        else
            break;
    } while (len > 0);
    return len > 0;
}

#pragma vtordisp(off)
extern int g_839650;
struct V482ae0 {
    V482ae0() { m_4 = g_839650; g_839650 = 0; }
    virtual ~V482ae0();
    int m_4;
};
struct D482ae0 : virtual V482ae0 {
    D482ae0();
    virtual ~D482ae0();
    int m_4[4];
};
// MATCH: golf_clean.exe 0x00482ae0 ??0D482ae0@@QAE@XZ
D482ae0::D482ae0()
{
}
struct D482b20 : virtual V482ae0 {
    D482b20();
    int m_4;
    virtual ~D482b20();
};
// MATCH: golf_clean.exe 0x00482b20 ??0D482b20@@QAE@XZ
D482b20::D482b20()
{
}
