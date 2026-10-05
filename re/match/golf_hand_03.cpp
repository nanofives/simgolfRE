// FLAGS golf_clean.exe: /O2
// golf_clean.exe batch 03, matched by hand (release /O2). Class and member names are chosen here.
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <mmsystem.h>

struct Obj47e { char pad[0x98]; unsigned int m_flags; virtual void v0(); virtual void v1(); virtual void release(); };
struct C47e700 {
    char pad[0x224];
    Obj47e** m_data;
    int m_cap;
    int m_count;
    void refresh();            // 0x47e450
    void shrink();             // 0x479bb0
    void bringToFront(Obj47e* p);
    void remove(Obj47e* p);
};
// MATCH: golf_clean.exe 0x0047e700 ?bringToFront@C47e700@@QAEXPAUObj47e@@@Z
void C47e700::bringToFront(Obj47e* p)
{
    int i;
    if (p && !(p->m_flags & 0x2000000)) {
        for (i = 0; i < m_count; i++)
            if (m_data[i] == p)
                break;
        if (i < m_count) {
            for (; i > 0; i--)
                m_data[i] = m_data[i - 1];
            m_data[0] = p;
        }
        refresh();
    }
}
// MATCH: golf_clean.exe 0x0047e680 ?remove@C47e700@@QAEXPAUObj47e@@@Z
void C47e700::remove(Obj47e* p)
{
    int i;
    if (p) {
        for (i = 0; i < m_count; i++)
            if (m_data[i] == p)
                break;
        if (i < m_count) {
            m_count--;
            p->release();
            for (; i < m_count; i++)
                m_data[i] = m_data[i + 1];
        }
        if (m_count <= m_cap - 10)
            shrink();
    }
}

struct T495 {
    int m_0, m_4;
    int m_8;
    int m_c;
    void (__cdecl* m_10)(int);
    void (__cdecl* m_14)(int, int);
    int m_18;
    int m_1c;
    int m_20;
    int m_24;
    void stop();               // 0x486ec0
};
extern int g_83ad4c;
extern T495* g_83aff0;
// MATCH: golf_clean.exe 0x00495cb0 ?f495cb0@@YGXPAUT495@@@Z
void __stdcall f495cb0(T495* t)
{
    t->m_24 = 0;
    if (g_83ad4c == 0) {
        if (t->m_8 & 1) {
            if (t->m_8 & 2) {
                t->stop();
                return;
            }
            t->m_8 |= 2;
        }
        if (g_83aff0 == 0 || t == g_83aff0) {
            if (t->m_10)
                t->m_10(t->m_1c);
            if (t->m_14)
                t->m_14(t->m_1c, t->m_18);
        }
    }
}

struct Fmt { int m_bpp; };
struct DSurf {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8();
    virtual void unlock(int);
    virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15();
    virtual int fill(DSurf*, RECT*, RECT*);
    virtual void v17(); virtual void v18(); virtual void v19();
    virtual int blt3(DSurf*, DSurf*, DSurf*, int, int, DSurf*);
    virtual void v21(); virtual void v22();
    virtual int blt(DSurf*, int, int, DSurf*);
    virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53(); virtual void v54(); virtual void v55(); virtual void v56();
    virtual Fmt* format();
};
struct Surf473 {
    int m_0;
    DSurf* m_4;
    int m_8;
    char pad[0x20 - 0xc];
    int m_20;
    int m_24;
    int blitTo(Surf473* s, int a2, int a3, int a4, int a5, int a6, int a7);   // 0x492000
    int f473bf0(Surf473* s, int a2, int a3, int a4, int a5, int a6, int a7);
    int f473df0(Surf473* s, int x, int y, Surf473* s2);
    int f473fc0(Surf473* a, Surf473* b, Surf473* c, int d, int e, Surf473* g);
    int f475c90(Surf473* s, int x, int y, int w, int h);
};
// MATCH: golf_clean.exe 0x00473bf0 ?f473bf0@Surf473@@QAEHPAU1@HHHHHH@Z
int Surf473::f473bf0(Surf473* s, int a2, int a3, int a4, int a5, int a6, int a7)
{
    if (!s)
        return 0x10;
    if (!s->m_4)
        return 7;
    if (s->m_4->format()->m_bpp != 8)
        return 0;
    return blitTo(s, a2, a3, a4, a5, a6, a7);
}
// MATCH: golf_clean.exe 0x00473df0 ?f473df0@Surf473@@QAEHPAU1@HH0@Z
int Surf473::f473df0(Surf473* s, int x, int y, Surf473* s2)
{
    if (m_8)
        return 0x18;
    if (!s)
        return 0x10;
    if (m_4 && s->m_4)
        return m_4->blt(s->m_4, m_20 + x, m_24 + y, s2 ? s2->m_4 : 0);
    return 7;
}
// MATCH: golf_clean.exe 0x00473fc0 ?f473fc0@Surf473@@QAEHPAU1@00HH0@Z
int Surf473::f473fc0(Surf473* a, Surf473* b, Surf473* c, int d, int e, Surf473* g)
{
    if (!b || !a)
        return 0x10;
    if (m_4 && b->m_4 && a->m_4 && c->m_4)
        return m_4->blt3(a->m_4, b->m_4, c->m_4, d, e, g ? g->m_4 : 0);
    return 7;
}
// MATCH: golf_clean.exe 0x00475c90 ?f475c90@Surf473@@QAEHPAU1@HHHH@Z
int Surf473::f475c90(Surf473* s, int x, int y, int w, int h)
{
    RECT r;
    if (!s)
        return 0x10;
    if (m_4 && s->m_4) {
        r.left = x;
        r.top = y;
        r.right = x + w;
        r.bottom = y + h;
        return m_4->fill(s->m_4, &r, &r);
    }
    return 7;
}

struct Painter474 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void draw(int, int, int, int); };
extern int g_839ab0;
struct C474e70 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3();
    virtual Painter474* painter();
    char pad[0x20 - 4];
    int m_20;
    void drawSelf(int, int, int, int, int, int);   // 0x474dd0
    void paint(int a, int b, int c);
};
// MATCH: golf_clean.exe 0x00474e70 ?paint@C474e70@@QAEXHHH@Z
void C474e70::paint(int a, int b, int c)
{
    g_839ab0 = 1;
    if (painter()) {
        painter()->draw(a, b, 1, c);
        g_839ab0 = 0;
        return;
    }
    drawSelf(a, b, c, 0, m_20, 0);
    g_839ab0 = 0;
}

struct DD474 {
    virtual void v0();
    virtual int create(int, int, int, int, int, int);
    virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5();
    virtual void setAlpha(int);
};
struct Mgr {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7();
    virtual HWND hwnd();
    virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31();
    virtual DD474* newSurface(void*, int);
};
extern Mgr* g_83ad50;
extern int g_83acb4;
struct IA474 { virtual void v0(); };
struct IB474 { DD474* m_surf; };
struct C474550 : IA474, IB474 {
    int m_8, m_c;
    int m_10, m_14, m_18, m_1c, m_20, m_24;
    void release();     // 0x473ae0
    int create(int w, int h);
};
// MATCH: golf_clean.exe 0x00474550 ?create@C474550@@QAEHHH@Z
int C474550::create(int w, int h)
{
    int r;
    release();
    m_surf = g_83ad50->newSurface((IB474*)this, 1);
    m_surf->setAlpha(0xff);
    r = m_surf->create(0, w, h, 8, 1, g_83acb4);
    if (r == 0) {
        m_10 = w;
        m_14 = h;
        m_18 = w;
        m_1c = h;
        m_20 = 0;
        m_24 = 0;
    }
    return r;
}

void CALLBACK cb486f90(UINT, UINT, DWORD, DWORD, DWORD);
void CALLBACK cb486f40(HWND, UINT, UINT, DWORD);
void __cdecl f483da0();
struct Timer486 {
    int m_0, m_4;
    int m_8;
    int m_c;
    int m_10;
    int m_14;
    int m_18, m_1c;
    unsigned int m_20;
    int m_24;
    int m_28;
    int start();
    int resume();
};
// MATCH: golf_clean.exe 0x00486dc0 ?start@Timer486@@QAEHXZ
int Timer486::start()
{
    if (!m_10 && !m_14)
        return 7;
    m_8 &= ~1;
    if (m_20 < 50) {
        m_c = timeSetEvent(m_20, m_28, (LPTIMECALLBACK)cb486f90, (DWORD)this, TIME_PERIODIC);
        return m_c ? 0 : 2;
    }
    m_c = SetTimer(g_83ad50->hwnd(), (UINT)this, m_20, (TIMERPROC)cb486f40);
    return m_c ? 0 : 2;
}
// MATCH: golf_clean.exe 0x00486e40 ?resume@Timer486@@QAEHXZ
int Timer486::resume()
{
    if (!m_10 && !m_14)
        return 7;
    if (m_20 < 50) {
        m_c = timeSetEvent(m_20, m_28, (LPTIMECALLBACK)cb486f90, (DWORD)this, TIME_ONESHOT);
        return m_c ? 0 : 2;
    }
    m_8 = (m_8 & ~2) | 1;
    m_c = SetTimer(g_83ad50->hwnd(), (UINT)this, m_20, (TIMERPROC)cb486f40);
    return m_c ? 0 : 2;
}

struct Ent492 { char pad[0x14]; int m_14; int m_18; int m_1c; };
struct Timer486b {
    void setup(void (__cdecl*)(int, void*), int, void*, int, int);   // 0x486d20
    int resume();                                                    // 0x486e40
    char pad[0x2c];
};
void __cdecl cb492bb0(int, void*);
struct C492cc0 {
    int m_0;
    int m_4;
    int m_8;
    int m_c;
    char pad10[0x10];
    Timer486b m_20;
    char pad4c[4];
    Ent492* m_50;
    int m_54;
    int m_58;
    int m_5c;
    void select(int a, int b, int c);
};
// MATCH: golf_clean.exe 0x00492cc0 ?select@C492cc0@@QAEXHHH@Z
void C492cc0::select(int a, int b, int c)
{
    int i;
    m_c = c;
    for (i = 0; i < m_58; i++)
        if (m_50[i].m_18 == a && m_50[i].m_14 == b)
            break;
    if (i != m_58) {
        if (m_50[i].m_1c) {
            m_20.setup(cb492bb0, i, this, m_5c, 5);
            m_20.resume();
        }
        m_4 = a;
        m_8 = b;
    }
}

struct Node480 { int m_0, m_4; void* m_data; Node480* m_next; };
struct List480 {
    virtual ~List480();
    Node480* m_head;
    Node480* m_cur;
    int m_count;
    int m_10;
    int m_shared;
    void clear()
    {
        int i;
        if (m_head) {
            if (!m_shared) {
                for (i = 0; i < m_count; i++) {
                    m_cur = m_head->m_next;
                    if (m_head->m_data)
                        free(m_head->m_data);
                    m_head->m_data = 0;
                    if (m_head)
                        free(m_head);
                    m_head = m_cur;
                }
            }
            m_10 = 0;
            m_head = 0;
            m_count = 0;
        }
    }
};
// MATCH: golf_clean.exe 0x00480390 ??1List480@@UAE@XZ
List480::~List480()
{
    clear();
    m_10 = 0;
}

struct VB402 { virtual ~VB402(); };
struct Data402 : virtual VB402 { };
struct Node402 : virtual VB402 { int m_4; Data402* m_data; Node402* m_next; };
struct List402 {
    virtual void v0();
    virtual void onRemove(Data402*);
    int m_4;
    Node402* m_head;
    Node402* m_cur;
    int m_count;
    int m_14;
    void clear();
};
// MATCH: golf_clean.exe 0x004026a0 ?clear@List402@@QAEXXZ
void List402::clear()
{
    int i;
    Data402* d;
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

extern char g_51a068[];
extern const char s_004d3174[];
extern const char s_004d316c[];
extern const char s_004d3164[];
extern const char s_004d3154[];
extern const char s_004d3144[];
// MATCH: golf_clean.exe 0x004532a0 ?f4532a0@@YAXH@Z
void f4532a0(int n)
{
    const char* s;
    if (n < 0)
        s = s_004d3174;
    else {
        switch (n / 25) {
        case 0:
            s = s_004d316c;
            break;
        case 1:
            s = s_004d3164;
            break;
        case 2:
        case 3:
            s = s_004d3154;
            break;
        default:
            s = s_004d3144;
        }
    }
    strcat(g_51a068, s);
}

extern int g_83e8b8[10];
extern int g_83d3c8[10];
extern char g_83e8e0[10][256];
extern int g_83f35c;
extern int g_83f360;
// MATCH: golf_clean.exe 0x00494cb0 ?f494cb0@@YAHHPBDHH@Z
int f494cb0(int idx, const char* s, int a, int b)
{
    if (!s || idx > 9)
        return 3;
    if (a < 0)
        a = g_83f35c;
    g_83e8b8[idx] = a;
    if (b < 0)
        b = g_83f360;
    g_83d3c8[idx] = b;
    g_83e8e0[idx][0] = 0;
    strcat(g_83e8e0[idx], s);
    return 0;
}

extern const char* g_4e4584[22];
int __cdecl f4ad580(const char*, const char*, size_t);
void __cdecl f4925b0(const char*);
// MATCH: golf_clean.exe 0x0048cd80 ?f48cd80@@YAHPAPAD@Z
int f48cd80(char** pp)
{
    int result = -1;
    int i;
    if (pp) {
        for (i = 0; result == -1 && i < 22; i++) {
            if (f4ad580(*pp, g_4e4584[i], strlen(g_4e4584[i])) == 0) {
                *pp += strlen(g_4e4584[i]);
                result = i;
                f4925b0(*pp);
            }
        }
    }
    return result;
}
struct Node49e { int m_0; int m_id; int m_8; Node49e* m_next; };
struct List49e {
    int m_0, m_4;
    Node49e* m_head;
    Node49e* m_cur;
    int m_count;
    int m_idx;
    int find(int id)
    {
        int i;
        if (m_head) {
            m_idx = 0;
            m_cur = m_head;
            for (i = 0; i < m_count; i++) {
                if (m_cur->m_id == id)
                    return 1;
                m_idx++;
                m_cur = m_cur->m_next;
            }
        }
        return 0;
    }
};
struct VA49e { int m_a; };
struct VB49e {
    char pad[0xc0];
    List49e m_list;
    char pad2[0xf0 - 0xd8];
    int m_mask;
};
struct C49ef80 : virtual VA49e, virtual VB49e {
    int isSet(int id);
};
// MATCH: golf_clean.exe 0x0049ef80 ?isSet@C49ef80@@QAEHH@Z
int C49ef80::isSet(int id)
{
    m_list.find(id);
    return (1 << m_list.m_idx) & m_mask;
}
struct VB489 {
    char pad[0xc0];
    List49e m_list;
};
struct C489ed0 : virtual VA49e, virtual VB489 {
    virtual void v0();
    void select(int, int);   // 0x489f50
    void selectId(int id);
};
// MATCH: golf_clean.exe 0x00489ed0 ?selectId@C489ed0@@QAEXH@Z
void C489ed0::selectId(int id)
{
    if (m_list.find(id))
        select(m_list.m_idx, 1);
    else
        select(-1, 1);
}
struct Poly4a2 { virtual ~Poly4a2(); };
struct P1a { virtual ~P1a(); char pad[0x270]; };
struct P2a { virtual void h(); char pad[0x2fc]; };
struct VB1a : P1a, P2a { };
struct VB2a { virtual void k(); void detach(); };   // detach 0x4894b0
extern int g_8409c4;
struct C4a23a0 : virtual VB1a, virtual VB2a {
    virtual ~C4a23a0();
    virtual void h();
    virtual void k();
    void reset();
    Poly4a2* m_obj[10];
    void* m_buf[10];
    int m_cap[10];
    int m_7c, m_80;
    int m_84;
};
// MATCH: golf_clean.exe 0x004a23a0 ?reset@C4a23a0@@QAEXXZ
void C4a23a0::reset()
{
    int i;
    for (i = 0; i < 10; i++) {
        if (m_obj[i]) {
            delete m_obj[i];
            m_obj[i] = 0;
            m_cap[i] = 10;
        }
        if (m_buf[i]) {
            free(m_buf[i]);
            m_buf[i] = 0;
        }
    }
    m_84 = g_8409c4;
    detach();
}
// MATCH: golf_clean.exe 0x004a2330 ??1C4a23a0@@UAE@XZ
C4a23a0::~C4a23a0()
{
    reset();
}
extern "C" int g_83d35c;
extern "C" int g_83d38c;
extern "C" int g_83d384;
extern "C" int g_83d388;
extern "C" int g_83d37c;
extern "C" int g_83d380;
extern "C" int g_83d34c;
// MATCH: golf_clean.exe 0x00493000 ?f493000@@YAXHH@Z
__declspec(naked) void f493000(int, int)
{
    __asm {
        push ebp
        mov ebp, esp
        push ebx
        push edi
        push edi
        mov ebx, dword ptr [ebp + 0xc]
        cmp ebx, g_83d35c
        jle done
        mov ecx, dword ptr [ebp + 8]
        cmp ecx, g_83d38c
        jge done
        sub ebx, ecx
        jle done
        mov ecx, dword ptr [ebp + 0xc]
        cmp ecx, g_83d38c
        jl rightok
        mov ecx, g_83d38c
        mov dword ptr [ebp + 0xc], ecx
rightok:
        mov edi, dword ptr [ebp + 8]
        cmp edi, g_83d35c
        jge leftok
        mov edi, g_83d35c
        mov dword ptr [ebp + 8], edi
leftok:
        sub ecx, edi
        jle done
        mov eax, g_83d384
        imul g_83d388
        add eax, dword ptr [ebp + 8]
        add eax, dword ptr [ebp + 8]
        add eax, g_83d37c
        mov edi, eax
        mov ax, word ptr g_83d34c
fill:
        mov word ptr [edi], ax
        add edi, 2
        dec ecx
        jne fill
done:
        pop edi
        pop edi
        pop ebx
        pop ebp
        ret
    }
}
// MATCH: golf_clean.exe 0x00493080 ?f493080@@YAXHH@Z
__declspec(naked) void f493080(int, int)
{
    __asm {
        push ebp
        mov ebp, esp
        push ebx
        push edi
        push edi
        mov ebx, dword ptr [ebp + 0xc]
        cmp ebx, g_83d35c
        jle done
        mov ecx, dword ptr [ebp + 8]
        cmp ecx, g_83d38c
        jge done
        sub ebx, ecx
        jle done
        mov ecx, dword ptr [ebp + 0xc]
        cmp ecx, g_83d38c
        jl rightok
        mov ecx, g_83d38c
        mov dword ptr [ebp + 0xc], ecx
rightok:
        mov edi, dword ptr [ebp + 8]
        cmp edi, g_83d35c
        jge leftok
        mov edi, g_83d35c
        mov dword ptr [ebp + 8], edi
leftok:
        sub ecx, edi
        jle done
        mov eax, g_83d384
        imul g_83d388
        add eax, dword ptr [ebp + 8]
        add eax, g_83d380
        mov edi, eax
        mov ah, byte ptr g_83d34c
fill:
        mov byte ptr [edi], ah
        inc edi
        dec ecx
        jne fill
done:
        pop edi
        pop edi
        pop ebx
        pop ebp
        ret
    }
}
struct Src493 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39(); virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49(); virtual void v50(); virtual void v51(); virtual void v52(); virtual void v53();
    virtual int handle();
};
struct Sink493 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void open(int, int, int, int); };
struct Item493 { char pad[0x1c]; int m_1c; };
struct C493ef0 {
    char pad[0x213c];
    Sink493& sink() { return *(Sink493*)(pad + 0x213c); }
    char pad2[0x23b4 - 0x213c];
    Src493* m_src;
    char pad3[0x3468 - 0x23b8];
    Item493* m_item[3];
    void update();       // 0x493f50
    void setItems(Item493** p);
};
// MATCH: golf_clean.exe 0x00493ef0 ?setItems@C493ef0@@QAEXPAPAUItem493@@@Z
void C493ef0::setItems(Item493** p)
{
    int i;
    int id;
    for (i = 0; i < 3; i++)
        m_item[i] = p[i];
    id = m_item[0]->m_1c;
    sink().open(m_src ? m_src->handle() : 0, id, 0, 0);
    update();
}
