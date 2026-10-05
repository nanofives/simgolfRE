// FLAGS golf_clean.exe: /O2
// golf_clean.exe classes written by hand (release /O2). Names are chosen here. A function that ends in `call; ret`
// instead of a tail `jmp` is written here as a destructor (VC6 never tail-calls implicit member and base destructors); the
// same code also comes from a body call followed by an inlined member destructor, so 0x482dd0 and 0x492830, which other
// destructors call as plain methods (golf_hand_02.cpp), are not determined to be destructors.
#include <stdlib.h>
#include <windows.h>
#include <mmsystem.h>
struct Poly { virtual ~Poly(); };
struct Member473ae0 { ~Member473ae0(); char pad[0x2c]; };   // dtor 0x473ae0
struct C482dd0 {
    ~C482dd0();
    int m_0;
    int m_4;
    Member473ae0 m_8;
    Poly* m_34;
    int m_38;
    int m_3c;
    int m_40;
};
// MATCH: golf_clean.exe 0x00482dd0 ??1C482dd0@@QAE@XZ
C482dd0::~C482dd0()
{
    m_4 = 0;
    m_38 = 0;
    m_3c = 0;
    m_40 = -1;
    if (m_34) {
        delete[] m_34;
        m_34 = 0;
    }
}
struct Base480610 { ~Base480610(); char pad[0x574]; };      // dtor 0x480610
struct Member486f10 { ~Member486f10(); int m_0; };          // dtor 0x486f10
struct C4860d0 : Base480610 {
    ~C4860d0();
    void cleanup();                                          // 0x485ff0
    void* m_574;
    char pad[0x5ac - 0x578];
    Member486f10 m_5ac;
};
// MATCH: golf_clean.exe 0x004860d0 ??1C4860d0@@QAE@XZ
C4860d0::~C4860d0()
{
    if (m_574)
        free(m_574);
    cleanup();
}
void __cdecl FUN_004a0320(const char*, const char*, DWORD, int, int);
extern const char s_004e493c[];
extern const char s_004e4954[];
struct C49cae0 {
    char pad[0xb8];
    unsigned char m_flags;
    int start();
    void prepare();          // 0x49ccc0
    int run(int);            // 0x49cb80
};
// MATCH: golf_clean.exe 0x0049cae0 ?start@C49cae0@@QAEHXZ
int C49cae0::start()
{
    FUN_004a0320(s_004e493c, s_004e4954, timeGetTime(), 0, 0);
    if (m_flags & 0x10)
        return 0;
    prepare();
    return run(0);
}
struct Base474780 { ~Base474780(); int m_0; int m_4; };     // dtor 0x474780
struct C4747e0 : Base474780 {
    ~C4747e0();
    void* m_8;
};
// MATCH: golf_clean.exe 0x004747e0 ??1C4747e0@@QAE@XZ
C4747e0::~C4747e0()
{
    if (m_8) {
        free(m_8);
        m_8 = 0;
    }
}
struct A4914d0 { char pad[0x1fc]; };
struct B4805a0 { ~B4805a0(); void shutdown(); char pad[0x774 - 0x1fc]; };   // dtor 0x4805a0, shutdown 0x491500
struct M489370 { ~M489370(); int m_0; };                                   // dtor 0x489370
struct C4914d0 {
    ~C4914d0();
    char pad[0x1fc];
    B4805a0 m_1fc;
    M489370 m_774;
};
// MATCH: golf_clean.exe 0x004914d0 ??1C4914d0@@QAE@XZ
C4914d0::~C4914d0()
{
    m_1fc.shutdown();
}
struct Base492800 { ~Base492800(); char pad[0x50]; };                       // dtor 0x492800
struct C492830 : Base492800 {
    ~C492830();
    Poly* m_50;
};
// MATCH: golf_clean.exe 0x00492830 ??1C492830@@QAE@XZ
C492830::~C492830()
{
    if (m_50) {
        delete[] m_50;
        m_50 = 0;
    }
}
struct Node4a4 { int m_0, m_4; void* m_buf; int m_c; Node4a4* m_next; };
struct List4a4 {
    int m_0;
    Node4a4* m_head;
    int m_8;
    int m_count;
    void removeHead();
};
// MATCH: golf_clean.exe 0x004a4ed0 ?removeHead@List4a4@@QAEXXZ
void List4a4::removeHead()
{
    Node4a4* n;
    if (m_head) {
        n = m_head;
        if (n->m_buf) {
            free(n->m_buf);
            n->m_buf = 0;
        }
        m_head = m_head->m_next;
        if (n) {
            free(n);
            n = 0;
        }
        m_count--;
    }
}
struct Vec479 {
    char pad[0x224];
    int* m_data;
    int m_cap;
    int m_count;
    void shrink();
};
// MATCH: golf_clean.exe 0x00479bb0 ?shrink@Vec479@@QAEXXZ
void Vec479::shrink()
{
    int i;
    int* p;
    if (m_cap == 0 || m_cap >= 10) {
        m_cap -= 10;
        if (m_cap == 0) {
            if (m_data)
                free(m_data);
            m_data = 0;
            return;
        }
        p = (int*)malloc(m_cap * 4);
        for (i = 0; i < m_count; i++)
            p[i] = m_data[i];
        if (m_data)
            free(m_data);
        m_data = p;
    }
}
struct Poly2 { virtual void destroy(int); };
struct Mgr83ad50 { virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7(); virtual void v8(); virtual void v9(); virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19(); virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29(); virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void release(int); };
extern Mgr83ad50* g_mgr;   // 0x83ad50
struct Slot483 { int m_id; Poly2* m_obj; char m_a, m_b; char pad[2]; void* m_buf; };
struct C483070 {
    int m_0;
    int m_4;
    Slot483 m_slots[5];
    void reset();
};
// MATCH: golf_clean.exe 0x00483070 ?reset@C483070@@QAEXXZ
void C483070::reset()
{
    int i;
    if (m_4 && g_mgr) {
        g_mgr->release(m_4);
        m_4 = 0;
    }
    for (i = 0; i < 5; i++) {
        m_slots[i].m_id = -1;
        if (m_slots[i].m_obj) {
            m_slots[i].m_obj->destroy(1);
            m_slots[i].m_obj = 0;
        }
        if (m_slots[i].m_buf) {
            free(m_slots[i].m_buf);
            m_slots[i].m_buf = 0;
        }
        m_slots[i].m_a = 0;
        m_slots[i].m_b = 0;
    }
}
struct Node4a4b { int m_0, m_4, m_8; void* m_buf; int m_10; Node4a4b* m_next; };
struct List4a4b {
    int m_0;
    Node4a4b* m_head;
    int m_8;
    int m_c;
    CRITICAL_SECTION m_cs;
    void clear();
};
// MATCH: golf_clean.exe 0x004a4b00 ?clear@List4a4b@@QAEXXZ
void List4a4b::clear()
{
    Node4a4b* next;
    EnterCriticalSection(&m_cs);
    while (m_head) {
        next = m_head->m_next;
        if (m_head->m_buf)
            free(m_head->m_buf);
        m_head->m_buf = 0;
        if (m_head)
            free(m_head);
        m_head = next;
    }
    m_8 = 0;
    m_c = 0;
    LeaveCriticalSection(&m_cs);
}
